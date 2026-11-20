#pragma once

#include "core/ChatMessage.h"

#include <QObject>
#include <QString>
#include <QVector>

#include <atomic>

struct llama_model;
struct llama_context;
struct llama_vocab;

namespace lulu {

/// Owns one llama.cpp model + context. Lives in a worker thread: every public
/// slot blocks until it is done, so the UI calls them through queued
/// connections. cancel() is the only thread-safe method meant to be called
/// directly from the UI thread.
class LlamaEngine : public QObject
{
    Q_OBJECT

public:
    explicit LlamaEngine(QObject* parent = nullptr);
    ~LlamaEngine() override;

    bool isLoaded() const { return loaded_.load(std::memory_order_acquire); }
    QString modelDescription() const;
    int contextSize() const;

    /// Safe to call from any thread. Stops the current generation.
    void cancel();

public slots:
    void loadModel(const QString& path, int nCtx, int nGpuLayers, int nThreads);
    void unloadModel();
    void generate(const QVector<lulu::ChatMessage>& messages, const lulu::GenerationParams& params);

signals:
    void loadingStarted();
    void loaded(const QString& description, int contextSize, const QString& chatTemplate);
    void loadFailed(const QString& error);
    void unloaded();

    void generationStarted();
    void token(const QString& text);
    void finished(const QString& text, const lulu::TokenStats& stats, bool cancelled);
    void failed(const QString& error);
    void contextUsed(int promptTokens, int contextSize);

private:
    QString formatChat(const QVector<ChatMessage>& messages) const;
    QVector<int> tokenize(const QString& text, bool addSpecial) const;
    QString pieceFor(int token) const;
    void decodeRange(QVector<int>& tokens, int begin, int end);
    void releaseModel();

    llama_model* model_ = nullptr;
    llama_context* context_ = nullptr;
    const llama_vocab* vocab_ = nullptr;

    QString description_;
    QString chatTemplate_;
    int contextSize_ = 0;
    int batchSize_ = 512;

    QVector<int> cachedTokens_;
    std::atomic_bool loaded_{false};
    std::atomic_bool cancelRequested_{false};
};

} // namespace lulu
