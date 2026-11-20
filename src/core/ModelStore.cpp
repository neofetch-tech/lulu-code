#include "core/ModelStore.h"

#include "app/AppPaths.h"
#include "core/ModelCatalog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>

namespace lulu {

namespace {
constexpr qint64 kGB = 1000LL * 1000LL * 1000LL;

QString humanSize(qint64 bytes)
{
    if (bytes >= kGB) {
        return QStringLiteral("%1 GB").arg(QString::number(static_cast<double>(bytes) / static_cast<double>(kGB), 'f', 1));
    }
    return QStringLiteral("%1 MB").arg(QString::number(static_cast<double>(bytes) / (1024.0 * 1024.0), 'f', 1));
}
} // namespace

bool InstalledModel::valid() const
{
    return !path.isEmpty() && QFileInfo::exists(path);
}

QString InstalledModel::sizeLabel() const
{
    return humanSize(sizeBytes);
}

ModelStore::ModelStore(QObject* parent)
    : QObject(parent)
    , network_(new QNetworkAccessManager(this))
{
    AppPaths::ensure();
    loadManifest();
}

ModelStore::~ModelStore()
{
    for (Job* job : std::as_const(jobs_)) {
        if (job->reply) {
            job->reply->abort();
        }
    }
    qDeleteAll(jobs_);
    jobs_.clear();
}

QString ModelStore::modelDirFor(const QString& id) const
{
    return AppPaths::modelsDir() + QLatin1Char('/') + id;
}

QString ModelStore::expectedPathFor(const QString& id) const
{
    if (const auto info = ModelCatalog::find(id)) {
        return modelDirFor(id) + QLatin1Char('/') + info->filename;
    }
    if (const auto local = find(id)) {
        return local->path;
    }
    return {};
}

void ModelStore::loadManifest()
{
    installed_.clear();

    QFile file(AppPaths::manifestFile());
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    const QJsonArray array = doc.object().value(QStringLiteral("models")).toArray();
    for (const QJsonValue& value : array) {
        const QJsonObject obj = value.toObject();
        InstalledModel model;
        model.id = obj.value(QStringLiteral("id")).toString();
        model.name = obj.value(QStringLiteral("name")).toString();
        model.family = obj.value(QStringLiteral("family")).toString();
        model.path = obj.value(QStringLiteral("path")).toString();
        model.repoId = obj.value(QStringLiteral("repoId")).toString();
        model.sizeBytes = static_cast<qint64>(obj.value(QStringLiteral("size")).toDouble());
        model.addedAt = QDateTime::fromString(obj.value(QStringLiteral("addedAt")).toString(), Qt::ISODate);

        if (model.id.isEmpty() || !model.valid()) {
            if (!model.path.isEmpty()) {
                qWarning() << "Lulu Code: dropping missing model" << model.id << model.path;
            }
            continue;
        }
        if (model.name.isEmpty()) {
            model.name = QFileInfo(model.path).completeBaseName();
        }
        installed_.append(model);
    }
}

void ModelStore::saveManifest() const
{
    QJsonArray array;
    for (const InstalledModel& model : installed_) {
        QJsonObject obj;
        obj.insert(QStringLiteral("id"), model.id);
        obj.insert(QStringLiteral("name"), model.name);
        obj.insert(QStringLiteral("family"), model.family);
        obj.insert(QStringLiteral("path"), model.path);
        obj.insert(QStringLiteral("repoId"), model.repoId);
        obj.insert(QStringLiteral("size"), static_cast<double>(model.sizeBytes));
        obj.insert(QStringLiteral("addedAt"), model.addedAt.toString(Qt::ISODate));
        array.append(obj);
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("models"), array);

    QDir().mkpath(AppPaths::dataDir());
    QFile file(AppPaths::manifestFile());
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    } else {
        qWarning() << "Lulu Code: cannot write model manifest" << AppPaths::manifestFile();
    }
}

QVector<InstalledModel> ModelStore::installed() const
{
    return installed_;
}

std::optional<InstalledModel> ModelStore::find(const QString& id) const
{
    for (const InstalledModel& model : installed_) {
        if (model.id == id) {
            return model;
        }
    }
    return std::nullopt;
}

bool ModelStore::isInstalled(const QString& id) const
{
    return find(id).has_value();
}

bool ModelStore::isDownloading(const QString& id) const
{
    return jobs_.contains(id);
}

bool ModelStore::registerLocalFile(const QString& filePath, QString* error)
{
    const QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) {
        if (error) {
            *error = QStringLiteral("File does not exist: %1").arg(filePath);
        }
        return false;
    }
    if (info.suffix().compare(QStringLiteral("gguf"), Qt::CaseInsensitive) != 0) {
        if (error) {
            *error = QStringLiteral("Only .gguf files can be added.");
        }
        return false;
    }

    QString id = info.completeBaseName().toLower();
    id.replace(QRegularExpression(QStringLiteral("[^a-z0-9._-]+")), QStringLiteral("-"));
    if (id.isEmpty()) {
        id = QStringLiteral("local-model");
    }
    QString unique = id;
    int suffix = 2;
    while (find(unique).has_value()) {
        unique = QStringLiteral("%1-%2").arg(id).arg(suffix++);
    }

    InstalledModel model;
    model.id = unique;
    model.name = info.completeBaseName();
    model.family = ModelCatalog::guessFamily(info.fileName());
    model.path = info.absoluteFilePath();
    model.sizeBytes = info.size();
    model.addedAt = QDateTime::currentDateTime();

    installed_.append(model);
    saveManifest();
    emit changed();
    return true;
}

void ModelStore::download(const QString& id)
{
    if (jobs_.contains(id)) {
        return;
    }
    const auto info = ModelCatalog::find(id);
    if (!info) {
        emit downloadFailed(id, QStringLiteral("Unknown model: %1").arg(id));
        return;
    }
    if (isInstalled(id)) {
        emit downloadFinished(id);
        return;
    }

    const QString dir = modelDirFor(id);
    if (!QDir().mkpath(dir)) {
        emit downloadFailed(id, QStringLiteral("Cannot create %1").arg(dir));
        return;
    }

    const QString finalPath = dir + QLatin1Char('/') + info->filename;
    const QString partialPath = finalPath + QStringLiteral(".part");

    auto* file = new QFile(partialPath);
    if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        const QString error = file->errorString();
        delete file;
        emit downloadFailed(id, QStringLiteral("Cannot write %1: %2").arg(partialPath, error));
        return;
    }

    QUrl url(QStringLiteral("https://huggingface.co/%1/resolve/main/%2")
                 .arg(info->repoId, info->filename));
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setRawHeader(QByteArrayLiteral("User-Agent"), QByteArrayLiteral("LuluCode/") + LULU_VERSION);
    request.setRawHeader(QByteArrayLiteral("Accept"), QByteArrayLiteral("*/*"));

    QNetworkReply* reply = network_->get(request);

    auto* job = new Job;
    job->id = id;
    job->reply = reply;
    job->file = file;
    job->partialPath = partialPath;
    job->finalPath = finalPath;
    job->startedAt = QDateTime::currentDateTime();
    jobs_.insert(id, job);

    connect(reply, &QNetworkReply::readyRead, this, [job]() {
        if (job->file && job->file->isOpen()) {
            job->file->write(job->reply->readAll());
        }
    });

    connect(reply, &QNetworkReply::downloadProgress, this,
            [this, id, estimate = info->sizeBytes](qint64 received, qint64 total) {
                emit downloadProgress(id, received, total > 0 ? total : estimate);
            });

    connect(reply, &QNetworkReply::finished, this, [this, job]() {
        const QNetworkReply::NetworkError error = job->reply->error();
        const int status = job->reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        QString message;
        if (error != QNetworkReply::NoError) {
            message = job->reply->errorString();
        } else if (status == 404) {
            message = QStringLiteral("Model file not found on Hugging Face (404).");
        } else if (status >= 400) {
            message = QStringLiteral("Hugging Face returned HTTP %1.").arg(status);
        }

        if (job->file && job->file->isOpen()) {
            job->file->write(job->reply->readAll());
        }
        finishJob(job, message);
    });

    emit downloadStarted(id);
}

void ModelStore::finishJob(Job* job, const QString& errorIn)
{
    QString error = errorIn;
    const QString id = job->id;
    jobs_.remove(id);

    if (job->file) {
        job->file->close();
        delete job->file;
        job->file = nullptr;
    }
    if (job->reply) {
        job->reply->deleteLater();
        job->reply = nullptr;
    }

    if (error.isEmpty()) {
        QFile::remove(job->finalPath);
        if (!QFile::rename(job->partialPath, job->finalPath)) {
            error = QStringLiteral("Could not move the downloaded file into place.");
        }
    }

    if (!error.isEmpty()) {
        QFile::remove(job->partialPath);
        delete job;
        emit downloadFailed(id, error);
        return;
    }

    const QFileInfo info(job->finalPath);
    InstalledModel model;
    model.id = id;
    if (const auto known = ModelCatalog::find(id)) {
        model.name = known->name;
        model.family = known->family;
        model.repoId = known->repoId;
    } else {
        model.name = info.completeBaseName();
        model.family = ModelCatalog::guessFamily(info.fileName());
    }
    model.path = job->finalPath;
    model.sizeBytes = info.size();
    model.addedAt = QDateTime::currentDateTime();

    installed_.removeIf([&](const InstalledModel& existing) { return existing.id == id; });
    installed_.append(model);
    saveManifest();
    delete job;

    emit changed();
    emit downloadFinished(id);
}

void ModelStore::cancelDownload(const QString& id)
{
    Job* job = jobs_.value(id, nullptr);
    if (!job || !job->reply) {
        return;
    }
    job->reply->abort();
}

void ModelStore::remove(const QString& id, bool deleteFiles)
{
    const auto model = find(id);
    if (!model) {
        return;
    }

    if (deleteFiles) {
        const QString modelsRoot = QDir::cleanPath(AppPaths::modelsDir());
        const QString filePath = QDir::cleanPath(model->path);
        if (filePath.startsWith(modelsRoot + QLatin1Char('/'), Qt::CaseInsensitive)) {
            QFile::remove(filePath);
            QDir().rmdir(QFileInfo(filePath).absolutePath());
        }
    }

    installed_.removeIf([&](const InstalledModel& existing) { return existing.id == id; });
    saveManifest();
    emit changed();
}

} // namespace lulu
