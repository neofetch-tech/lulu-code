#include "app/Logger.h"

#include "app/AppPaths.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QTextStream>

#include <cstdio>

namespace lulu {

namespace {

QMutex g_mutex;
QtMessageHandler g_previous = nullptr;

constexpr qint64 kMaxLogBytes = 2 * 1024 * 1024;

QString levelTag(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return QStringLiteral("DBG");
    case QtInfoMsg:
        return QStringLiteral("INF");
    case QtWarningMsg:
        return QStringLiteral("WRN");
    case QtCriticalMsg:
        return QStringLiteral("ERR");
    case QtFatalMsg:
        return QStringLiteral("FTL");
    }
    return QStringLiteral("???");
}

void rotateIfNeeded(const QString& path)
{
    QFileInfo info(path);
    if (!info.exists() || info.size() < kMaxLogBytes) {
        return;
    }
    const QString old = path + QStringLiteral(".1");
    QFile::remove(old);
    QFile::rename(path, old);
}

void handler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    const QString line = QStringLiteral("%1 [%2] %3")
                             .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")),
                                  levelTag(type),
                                  message);

    {
        QMutexLocker lock(&g_mutex);
        const QString path = AppPaths::logsDir() + QStringLiteral("/lulu-code.log");
        rotateIfNeeded(path);
        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            QTextStream stream(&file);
            stream << line << '\n';
        }
    }

    std::fprintf(stderr, "%s\n", line.toLocal8Bit().constData());

    if (type == QtFatalMsg) {
        std::abort();
    }
    Q_UNUSED(context);
}

} // namespace

void Logger::install()
{
    AppPaths::ensure();
    g_previous = qInstallMessageHandler(handler);
}

} // namespace lulu
