#include "app/AppPaths.h"

#include <QDir>
#include <QStandardPaths>

namespace lulu {

QString AppPaths::dataDir()
{
    // e.g. C:/Users/me/AppData/Roaming/Lulu/Lulu Code on Windows,
    //      ~/.local/share/Lulu/Lulu Code on Linux.
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty()) {
        dir = QDir::homePath() + QStringLiteral("/.lulu-code");
    }
    return dir;
}

QString AppPaths::modelsDir()
{
    return dataDir() + QStringLiteral("/models");
}

QString AppPaths::logsDir()
{
    return dataDir() + QStringLiteral("/logs");
}

QString AppPaths::manifestFile()
{
    return dataDir() + QStringLiteral("/models.json");
}

QString AppPaths::displayDataDir()
{
    return QDir::toNativeSeparators(dataDir());
}

void AppPaths::ensure()
{
    QDir().mkpath(modelsDir());
    QDir().mkpath(logsDir());
}

} // namespace lulu
