#include "core/ModelCatalog.h"

#include <QLocale>

namespace lulu {

namespace {

constexpr qint64 kGB = 1000LL * 1000LL * 1000LL;

ModelInfo make(const QString& id,
               const QString& name,
               const QString& family,
               const QString& repoId,
               const QString& filename,
               const QString& quant,
               const QString& description,
               const QString& license,
               qint64 sizeBytes,
               int trainedContext,
               bool recommended)
{
    return ModelInfo{id, name, family, repoId, filename, quant, description,
                     license, sizeBytes, trainedContext, recommended};
}

const QVector<ModelInfo> kModels = {
    make(QStringLiteral("qwen2.5-coder-7b"),
         QStringLiteral("Qwen2.5 Coder 7B"),
         QStringLiteral("Qwen"),
         QStringLiteral("bartowski/Qwen2.5-Coder-7B-Instruct-GGUF"),
         QStringLiteral("Qwen2.5-Coder-7B-Instruct-Q4_K_M.gguf"),
         QStringLiteral("Q4_K_M"),
         QStringLiteral("The sweet spot for local coding agents. Strong tool use, fast on 8 GB of VRAM."),
         QStringLiteral("Apache-2.0"),
         static_cast<qint64>(4.68 * kGB),
         32768,
         true),

    make(QStringLiteral("llama-3.1-8b"),
         QStringLiteral("Llama 3.1 8B Instruct"),
         QStringLiteral("Llama"),
         QStringLiteral("bartowski/Meta-Llama-3.1-8B-Instruct-GGUF"),
         QStringLiteral("Meta-Llama-3.1-8B-Instruct-Q4_K_M.gguf"),
         QStringLiteral("Q4_K_M"),
         QStringLiteral("Meta's general-purpose 8B model. Great writing, decent at code."),
         QStringLiteral("Llama 3.1 Community"),
         static_cast<qint64>(4.92 * kGB),
         131072,
         false),

    make(QStringLiteral("qwen2.5-coder-14b"),
         QStringLiteral("Qwen2.5 Coder 14B"),
         QStringLiteral("Qwen"),
         QStringLiteral("bartowski/Qwen2.5-Coder-14B-Instruct-GGUF"),
         QStringLiteral("Qwen2.5-Coder-14B-Instruct-Q4_K_M.gguf"),
         QStringLiteral("Q4_K_M"),
         QStringLiteral("Noticeably smarter than 7B for refactors. Needs ~10 GB of VRAM or fast RAM."),
         QStringLiteral("Apache-2.0"),
         static_cast<qint64>(8.99 * kGB),
         32768,
         false),

    make(QStringLiteral("glm-4-9b"),
         QStringLiteral("GLM-4 9B"),
         QStringLiteral("GLM"),
         QStringLiteral("bartowski/THUDM_GLM-4-9B-0414-GGUF"),
         QStringLiteral("THUDM_GLM-4-9B-0414-Q4_K_M.gguf"),
         QStringLiteral("Q4_K_M"),
         QStringLiteral("Zhipu AI's bilingual model. Strong reasoning and function calling."),
         QStringLiteral("MIT"),
         static_cast<qint64>(5.5 * kGB),
         131072,
         false),

    make(QStringLiteral("deepseek-coder-v2-lite"),
         QStringLiteral("DeepSeek Coder V2 Lite"),
         QStringLiteral("DeepSeek"),
         QStringLiteral("bartowski/DeepSeek-Coder-V2-Lite-Instruct-GGUF"),
         QStringLiteral("DeepSeek-Coder-V2-Lite-Instruct-Q4_K_M.gguf"),
         QStringLiteral("Q4_K_M"),
         QStringLiteral("16B MoE with only ~2.4B active parameters. Fast and code-focused."),
         QStringLiteral("DeepSeek License"),
         static_cast<qint64>(10.4 * kGB),
         163840,
         false),

    make(QStringLiteral("qwen2.5-coder-32b"),
         QStringLiteral("Qwen2.5 Coder 32B"),
         QStringLiteral("Qwen"),
         QStringLiteral("bartowski/Qwen2.5-Coder-32B-Instruct-GGUF"),
         QStringLiteral("Qwen2.5-Coder-32B-Instruct-Q4_K_M.gguf"),
         QStringLiteral("Q4_K_M"),
         QStringLiteral("Best local coder in this list, if you have ~24 GB of VRAM or 32 GB of RAM."),
         QStringLiteral("Apache-2.0"),
         static_cast<qint64>(19.9 * kGB),
         32768,
         false),

    make(QStringLiteral("qwen2.5-coder-1.5b"),
         QStringLiteral("Qwen2.5 Coder 1.5B"),
         QStringLiteral("Qwen"),
         QStringLiteral("bartowski/Qwen2.5-Coder-1.5B-Instruct-GGUF"),
         QStringLiteral("Qwen2.5-Coder-1.5B-Instruct-Q4_K_M.gguf"),
         QStringLiteral("Q4_K_M"),
         QStringLiteral("Tiny and instant. Good for autocomplete-sized tasks and low-end laptops."),
         QStringLiteral("Apache-2.0"),
         static_cast<qint64>(0.99 * kGB),
         32768,
         false),
};

} // namespace

QString ModelInfo::sizeLabel() const
{
    const double gb = static_cast<double>(sizeBytes) / static_cast<double>(kGB);
    return QStringLiteral("~%1 GB").arg(QLocale::c().toString(gb, 'f', 1));
}

const QVector<ModelInfo>& ModelCatalog::all()
{
    return kModels;
}

std::optional<ModelInfo> ModelCatalog::find(const QString& id)
{
    for (const ModelInfo& model : kModels) {
        if (model.id == id) {
            return model;
        }
    }
    return std::nullopt;
}

QString ModelCatalog::guessFamily(const QString& fileName)
{
    const QString lower = fileName.toLower();
    if (lower.contains(QStringLiteral("qwen"))) {
        return QStringLiteral("Qwen");
    }
    if (lower.contains(QStringLiteral("llama"))) {
        return QStringLiteral("Llama");
    }
    if (lower.contains(QStringLiteral("glm"))) {
        return QStringLiteral("GLM");
    }
    if (lower.contains(QStringLiteral("deepseek"))) {
        return QStringLiteral("DeepSeek");
    }
    if (lower.contains(QStringLiteral("mistral")) || lower.contains(QStringLiteral("magistral"))) {
        return QStringLiteral("Mistral");
    }
    if (lower.contains(QStringLiteral("gemma"))) {
        return QStringLiteral("Gemma");
    }
    if (lower.contains(QStringLiteral("phi"))) {
        return QStringLiteral("Phi");
    }
    return QStringLiteral("Custom");
}

} // namespace lulu
