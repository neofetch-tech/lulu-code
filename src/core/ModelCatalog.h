#pragma once

#include <QString>
#include <QVector>

#include <optional>

namespace lulu {

/// A model that Lulu Code knows how to fetch. GGUF files only.
struct ModelInfo {
    QString id;          ///< stable key, e.g. "qwen2.5-coder-7b"
    QString name;        ///< display name
    QString family;      ///< "Llama", "Qwen", "GLM", "DeepSeek"
    QString repoId;      ///< Hugging Face repo
    QString filename;    ///< GGUF file inside the repo
    QString quant;       ///< "Q4_K_M"
    QString description;
    QString license;
    qint64 sizeBytes = 0;
    int trainedContext = 0;
    bool recommended = false;

    QString sizeLabel() const;
};

class ModelCatalog
{
public:
    static const QVector<ModelInfo>& all();
    static std::optional<ModelInfo> find(const QString& id);
    /// Best-effort guess for hand-imported GGUF files.
    static QString guessFamily(const QString& fileName);
};

} // namespace lulu
