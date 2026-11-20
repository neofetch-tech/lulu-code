#pragma once

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QString>
#include <QVector>

#include <optional>

class QNetworkAccessManager;
class QNetworkReply;
class QFile;

namespace lulu {

struct InstalledModel {
    QString id;
    QString name;
    QString family;
    QString path;
    QString repoId;
    qint64 sizeBytes = 0;
    QDateTime addedAt;

    bool valid() const;
    QString sizeLabel() const;
};

/// Downloads GGUF files from Hugging Face and remembers what is installed.
class ModelStore : public QObject
{
    Q_OBJECT

public:
    explicit ModelStore(QObject* parent = nullptr);
    ~ModelStore() override;

    QVector<InstalledModel> installed() const;
    std::optional<InstalledModel> find(const QString& id) const;
    bool isInstalled(const QString& id) const;
    QString expectedPathFor(const QString& id) const;

    /// Registers a .gguf that already lives somewhere on disk (no copy).
    bool registerLocalFile(const QString& filePath, QString* error = nullptr);

    void download(const QString& id);
    void cancelDownload(const QString& id);
    bool isDownloading(const QString& id) const;
    void remove(const QString& id, bool deleteFiles);

signals:
    void changed();
    void downloadStarted(const QString& id);
    void downloadProgress(const QString& id, qint64 received, qint64 total);
    void downloadFinished(const QString& id);
    void downloadFailed(const QString& id, const QString& error);

private:
    struct Job {
        QNetworkReply* reply = nullptr;
        QFile* file = nullptr;
        QString id;
        QString partialPath;
        QString finalPath;
        QDateTime startedAt;
    };

    void loadManifest();
    void saveManifest() const;
    QString modelDirFor(const QString& id) const;
    void finishJob(Job* job, const QString& error);

    QNetworkAccessManager* network_ = nullptr;
    QVector<InstalledModel> installed_;
    QHash<QString, Job*> jobs_;
};

} // namespace lulu
