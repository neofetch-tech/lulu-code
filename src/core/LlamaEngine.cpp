#include "core/LlamaEngine.h"

#include <QByteArray>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QScopeGuard>
#include <QThread>
#include <QtDebug>

#include <algorithm>
#include <mutex>

#if LULU_WITH_LLAMA
#include "llama.h"
#endif

namespace lulu {

namespace {

const QStringList& defaultStopSequences()
{
    static const QStringList stops = {
        QStringLiteral("<|eot_id|>"),
        QStringLiteral("<|im_end|>"),
        QStringLiteral("<|endoftext|>"),
        QStringLiteral("<|eot|>"),
        QStringLiteral("<|end|>"),
        QStringLiteral("</s>"),
    };
    return stops;
}

int autoThreadCount()
{
    const int ideal = QThread::idealThreadCount();
    return std::clamp(ideal > 0 ? ideal - 1 : 4, 1, 16);
}

void removeEmptyStrings(QStringList& list)
{
    list.removeIf([](const QString& value) { return value.isEmpty(); });
}

#if LULU_WITH_LLAMA

void ensureBackend()
{
    static std::once_flag once;
    std::call_once(once, [] {
        llama_backend_init();
        llama_log_set(
            [](ggml_log_level level, const char* text, void*) {
                if (level >= GGML_LOG_LEVEL_ERROR) {
                    qWarning().noquote() << QString::fromUtf8(text).trimmed();
                }
            },
            nullptr);
    });
}

QString pieceForToken(const llama_vocab* vocab, int token)
{
    char stackBuffer[256];
    int size = llama_token_to_piece(vocab, token, stackBuffer, sizeof(stackBuffer), 0, true);
    if (size >= 0) {
        return QString::fromUtf8(stackBuffer, size);
    }
    QByteArray buffer(-size, '\0');
    const int written = llama_token_to_piece(vocab, token, buffer.data(), buffer.size(), 0, true);
    if (written <= 0) {
        return {};
    }
    return QString::fromUtf8(buffer.constData(), written);
}

#endif // LULU_WITH_LLAMA

QString chatMlFallback(const QVector<ChatMessage>& messages)
{
    QString prompt;
    for (const ChatMessage& message : messages) {
        prompt += QStringLiteral("<|im_start|>") + roleName(message.role) + QLatin1Char('\n');
        prompt += message.content;
        prompt += QStringLiteral("<|im_end|>\n");
    }
    prompt += QStringLiteral("<|im_start|>assistant\n");
    return prompt;
}

} // namespace

LlamaEngine::LlamaEngine(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<QVector<lulu::ChatMessage>>("QVector<lulu::ChatMessage>");
    qRegisterMetaType<lulu::GenerationParams>("lulu::GenerationParams");
    qRegisterMetaType<lulu::TokenStats>("lulu::TokenStats");
}

LlamaEngine::~LlamaEngine()
{
    releaseModel();
}

QString LlamaEngine::modelDescription() const
{
    return description_;
}

int LlamaEngine::contextSize() const
{
    return contextSize_;
}

void LlamaEngine::cancel()
{
    cancelRequested_.store(true, std::memory_order_release);
}

void LlamaEngine::releaseModel()
{
#if LULU_WITH_LLAMA
    if (context_) {
        llama_free(context_);
        context_ = nullptr;
    }
    if (model_) {
        llama_model_free(model_);
        model_ = nullptr;
    }
#endif
    vocab_ = nullptr;
    cachedTokens_.clear();
    description_.clear();
    chatTemplate_.clear();
    contextSize_ = 0;
    loaded_.store(false, std::memory_order_release);
}

void LlamaEngine::loadModel(const QString& path, int nCtx, int nGpuLayers, int nThreads)
{
    emit loadingStarted();
    cancelRequested_.store(true, std::memory_order_release);
    releaseModel();

#if !LULU_WITH_LLAMA
    Q_UNUSED(path);
    Q_UNUSED(nCtx);
    Q_UNUSED(nGpuLayers);
    Q_UNUSED(nThreads);
    emit loadFailed(QStringLiteral(
        "This build of Lulu Code was compiled without llama.cpp.\n"
        "Reconfigure with -DLULU_WITH_LLAMA=ON to enable local inference."));
    return;
#else
    ensureBackend();

    const QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        emit loadFailed(QStringLiteral("Model file not found:\n%1").arg(QDir::toNativeSeparators(path)));
        return;
    }

    const QByteArray encodedPath = QDir::toNativeSeparators(info.absoluteFilePath()).toUtf8();

    llama_model_params modelParams = llama_model_default_params();
    modelParams.n_gpu_layers = nGpuLayers;
    modelParams.load_mode = LLAMA_LOAD_MODE_MMAP;

    qInfo().noquote() << QStringLiteral("Loading model %1 (ctx=%2, gpu_layers=%3)")
                             .arg(QDir::toNativeSeparators(path))
                             .arg(nCtx)
                             .arg(nGpuLayers);

    model_ = llama_model_load_from_file(encodedPath.constData(), modelParams);
    if (!model_) {
        emit loadFailed(QStringLiteral(
            "llama.cpp could not load this model.\n"
            "The file may be corrupt, incomplete, or use an unsupported GGUF version."));
        return;
    }

    const int threads = nThreads > 0 ? nThreads : autoThreadCount();

    llama_context_params contextParams = llama_context_default_params();
    contextParams.n_ctx = static_cast<uint32_t>(nCtx > 0 ? nCtx : 8192);
    contextParams.n_batch = std::min<uint32_t>(contextParams.n_ctx, 2048);
    contextParams.n_ubatch = std::min<uint32_t>(contextParams.n_batch, 512);
    contextParams.n_threads = threads;
    contextParams.n_threads_batch = threads;

    context_ = llama_init_from_model(model_, contextParams);
    if (!context_) {
        llama_model_free(model_);
        model_ = nullptr;
        emit loadFailed(QStringLiteral("llama.cpp could not allocate a context for this model. "
                                       "Try a smaller context size."));
        return;
    }

    vocab_ = llama_model_get_vocab(model_);
    contextSize_ = static_cast<int>(llama_n_ctx(context_));
    batchSize_ = static_cast<int>(contextParams.n_batch);

    char description[256] = {};
    llama_model_desc(model_, description, sizeof(description));
    description_ = QString::fromUtf8(description);

    if (const char* tmpl = llama_model_chat_template(model_, nullptr)) {
        chatTemplate_ = QString::fromUtf8(tmpl);
    }

    loaded_.store(true, std::memory_order_release);
    qInfo().noquote() << QStringLiteral("Model ready: %1 (ctx=%2)").arg(description_).arg(contextSize_);
    emit loaded(description_, contextSize_, chatTemplate_);
#endif
}

void LlamaEngine::unloadModel()
{
    cancelRequested_.store(true, std::memory_order_release);
    releaseModel();
    emit unloaded();
}

QVector<int> LlamaEngine::tokenize(const QString& text, bool addSpecial) const
{
#if !LULU_WITH_LLAMA
    Q_UNUSED(text);
    Q_UNUSED(addSpecial);
    return {};
#else
    if (!vocab_) {
        return {};
    }
    const QByteArray utf8 = text.toUtf8();
    int count = llama_tokenize(vocab_, utf8.constData(), static_cast<int>(utf8.size()),
                               nullptr, 0, addSpecial, true);
    if (count == 0) {
        return {};
    }
    if (count < 0) {
        count = -count;
    }
    QVector<int> tokens(count);
    const int written = llama_tokenize(vocab_, utf8.constData(), static_cast<int>(utf8.size()),
                                       reinterpret_cast<llama_token*>(tokens.data()), count,
                                       addSpecial, true);
    if (written < 0) {
        return {};
    }
    tokens.resize(written);
    return tokens;
#endif
}

QString LlamaEngine::pieceFor(int token) const
{
#if !LULU_WITH_LLAMA
    Q_UNUSED(token);
    return {};
#else
    return pieceForToken(vocab_, token);
#endif
}

QString LlamaEngine::formatChat(const QVector<ChatMessage>& messages) const
{
    if (messages.isEmpty()) {
        return {};
    }
#if !LULU_WITH_LLAMA
    return chatMlFallback(messages);
#else
    if (chatTemplate_.isEmpty()) {
        return chatMlFallback(messages);
    }

    QVector<QByteArray> roles;
    QVector<QByteArray> contents;
    roles.reserve(messages.size());
    contents.reserve(messages.size());
    qsizetype totalChars = 0;
    for (const ChatMessage& message : messages) {
        roles.append(roleName(message.role).toUtf8());
        contents.append(message.content.toUtf8());
        totalChars += message.content.size() + 32;
    }

    QVector<llama_chat_message> chat;
    chat.reserve(messages.size());
    for (qsizetype i = 0; i < messages.size(); ++i) {
        chat.append(llama_chat_message{roles.at(i).constData(), contents.at(i).constData()});
    }

    const QByteArray tmpl = chatTemplate_.toUtf8();
    QByteArray buffer(std::max<qsizetype>(1024, totalChars * 2 + 512), '\0');
    int needed = llama_chat_apply_template(tmpl.constData(), chat.constData(),
                                           static_cast<size_t>(chat.size()), true,
                                           buffer.data(), static_cast<int32_t>(buffer.size()));
    if (needed < 0) {
        qWarning() << "Lulu Code: chat template failed, falling back to ChatML";
        return chatMlFallback(messages);
    }
    if (needed > buffer.size()) {
        buffer.resize(needed);
        needed = llama_chat_apply_template(tmpl.constData(), chat.constData(),
                                           static_cast<size_t>(chat.size()), true,
                                           buffer.data(), static_cast<int32_t>(buffer.size()));
    }
    if (needed < 0) {
        return chatMlFallback(messages);
    }
    return QString::fromUtf8(buffer.constData(), needed);
#endif
}

void LlamaEngine::decodeRange(QVector<int>& tokens, int begin, int end)
{
#if LULU_WITH_LLAMA
    if (!context_) {
        throw std::runtime_error("No context");
    }
    int position = begin;
    while (position < end) {
        const int count = std::min(batchSize_, end - position);
        llama_batch batch = llama_batch_get_one(
            reinterpret_cast<llama_token*>(tokens.data()) + position, count);
        const int result = llama_decode(context_, batch);
        if (result != 0) {
            throw std::runtime_error(result == 1
                                         ? "The context window is full. Start a new chat or raise the context size."
                                         : "llama_decode failed.");
        }
        position += count;
    }
#else
    Q_UNUSED(tokens);
    Q_UNUSED(begin);
    Q_UNUSED(end);
#endif
}

void LlamaEngine::generate(const QVector<ChatMessage>& messagesIn, const GenerationParams& paramsIn)
{
    cancelRequested_.store(false, std::memory_order_release);

#if !LULU_WITH_LLAMA
    Q_UNUSED(messagesIn);
    Q_UNUSED(paramsIn);
    emit failed(QStringLiteral("This build of Lulu Code has no llama.cpp backend."));
    return;
#else
    if (!model_ || !context_ || !vocab_) {
        emit failed(QStringLiteral("No model is loaded."));
        return;
    }
    if (messagesIn.isEmpty()) {
        emit failed(QStringLiteral("Nothing to send."));
        return;
    }

    emit generationStarted();

    GenerationParams params = paramsIn;
    params.maxTokens = std::clamp(params.maxTokens, 16, std::max(16, contextSize_ - 64));

    QVector<ChatMessage> messages = messagesIn;
    QVector<int> tokens;
    QString prompt;

    // Make the conversation fit into the context window, dropping the oldest
    // non-system turns first. If a single message is still too large, keep the
    // tail of the prompt.
    for (int attempt = 0;; ++attempt) {
        prompt = formatChat(messages);
        tokens = tokenize(prompt, false);
        if (tokens.size() + params.maxTokens <= contextSize_) {
            break;
        }
        if (messages.size() <= 1 || attempt > messages.size() + 2) {
            const int keep = std::max(64, contextSize_ - params.maxTokens);
            if (tokens.size() > keep) {
                tokens = tokens.mid(tokens.size() - keep);
            }
            params.maxTokens = std::max(32, std::min(params.maxTokens, contextSize_ - static_cast<int>(tokens.size()) - 8));
            break;
        }
        const int dropIndex = (messages.first().role == Role::System) ? 1 : 0;
        messages.removeAt(dropIndex);
    }

    if (tokens.isEmpty()) {
        emit failed(QStringLiteral("The prompt is empty after tokenization."));
        return;
    }

    try {
        // Reuse the KV cache for the longest common prefix with the previous
        // turn. Agent loops repeat the same system prompt and history, so this
        // turns a full re-process into a few tokens in most cases.
        int common = 0;
        const int maxCommon = static_cast<int>(std::min<qsizetype>(cachedTokens_.size(), tokens.size()));
        while (common < maxCommon && cachedTokens_.at(common) == tokens.at(common)) {
            ++common;
        }
        if (common == tokens.size() && common > 0) {
            --common; // always recompute logits for the final prompt token
        }

        llama_memory_t memory = llama_get_memory(context_);
        if (common == 0) {
            llama_memory_clear(memory, true);
            cachedTokens_.clear();
        } else if (common < cachedTokens_.size()) {
            if (!llama_memory_seq_rm(memory, 0, common, -1)) {
                llama_memory_clear(memory, true);
                cachedTokens_.clear();
                common = 0;
            }
        }

        decodeRange(tokens, common, static_cast<int>(tokens.size()));
        cachedTokens_ = tokens;
        emit contextUsed(static_cast<int>(tokens.size()), contextSize_);

        QElapsedTimer decodeTimer;
        decodeTimer.start();

        llama_sampler_chain_params chainParams = llama_sampler_chain_default_params();
        llama_sampler* sampler = llama_sampler_chain_init(chainParams);
        const auto samplerGuard = qScopeGuard([sampler] { llama_sampler_free(sampler); });

        if (params.repeatPenalty > 1.0) {
            llama_sampler_chain_add(
                sampler, llama_sampler_init_penalties(llama_vocab_n_tokens(vocab_), 256,
                                                      static_cast<float>(params.repeatPenalty), 0.0f, 0.0f));
        }
        if (params.topK > 0) {
            llama_sampler_chain_add(sampler, llama_sampler_init_top_k(params.topK));
        }
        llama_sampler_chain_add(sampler, llama_sampler_init_top_p(static_cast<float>(params.topP), 1));
        if (params.temperature > 0.01) {
            llama_sampler_chain_add(sampler, llama_sampler_init_temp(static_cast<float>(params.temperature)));
            llama_sampler_chain_add(sampler, llama_sampler_init_dist(params.seed));
        } else {
            llama_sampler_chain_add(sampler, llama_sampler_init_greedy());
        }

        QStringList stops = params.stopSequences;
        if (stops.isEmpty()) {
            stops = defaultStopSequences();
        }
        removeEmptyStrings(stops);
        int maxStopLength = 0;
        for (const QString& stop : std::as_const(stops)) {
            maxStopLength = std::max(maxStopLength, static_cast<int>(stop.size()));
        }

        QString full;
        QString pending;
        int generated = 0;
        bool cancelled = false;
        bool stopped = false;

        for (int i = 0; i < params.maxTokens; ++i) {
            if (cancelRequested_.load(std::memory_order_acquire)) {
                cancelled = true;
                break;
            }

            const llama_token sampled = llama_sampler_sample(sampler, context_, -1);
            if (sampled < 0) {
                break;
            }
            if (llama_vocab_is_eog(vocab_, sampled)) {
                stopped = true;
                break;
            }

            llama_sampler_accept(sampler, sampled);

            const QString piece = pieceFor(static_cast<int>(sampled));
            if (!piece.isEmpty()) {
                pending += piece;
                ++generated;
            }

            int cutAt = -1;
            for (const QString& stop : std::as_const(stops)) {
                const qsizetype index = pending.indexOf(stop);
                if (index >= 0 && (cutAt < 0 || index < cutAt)) {
                    cutAt = static_cast<int>(index);
                }
            }
            if (cutAt >= 0) {
                const QString visible = pending.left(cutAt);
                if (!visible.isEmpty()) {
                    full += visible;
                    emit token(visible);
                }
                pending.clear();
                stopped = true;
                break;
            }

            const int safeLength = static_cast<int>(pending.size()) - (maxStopLength - 1);
            if (safeLength > 0) {
                const QString visible = pending.left(safeLength);
                pending.remove(0, safeLength);
                full += visible;
                emit token(visible);
            }

            if (static_cast<int>(cachedTokens_.size()) >= contextSize_) {
                stopped = true;
                break;
            }

            QVector<int> single{static_cast<int>(sampled)};
            decodeRange(single, 0, 1);
            cachedTokens_.append(static_cast<int>(sampled));
        }

        if (!pending.isEmpty()) {
            full += pending;
            emit token(pending);
        }

        Q_UNUSED(stopped);

        const double seconds = static_cast<double>(decodeTimer.elapsed()) / 1000.0;
        TokenStats stats;
        stats.promptTokens = static_cast<int>(tokens.size());
        stats.generatedTokens = generated;
        stats.tokensPerSecond = seconds > 0.001 ? static_cast<double>(generated) / seconds : 0.0;

        emit finished(full, stats, cancelled);
    } catch (const std::exception& error) {
        emit failed(QString::fromUtf8(error.what()));
    }
#endif
}

} // namespace lulu
