#include "agent/Toolchain.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace lulu {

namespace {

ToolchainEntry make(const QString& name, const QString& purpose, const QString& hint)
{
    ToolchainEntry entry;
    entry.name = name;
    entry.purpose = purpose;
    entry.hint = hint;

    QString path = QStandardPaths::findExecutable(name);
    if (path.isEmpty() && name == QStringLiteral("python")) {
        path = QStandardPaths::findExecutable(QStringLiteral("python3"));
    }
    if (!path.isEmpty()) {
        entry.path = path;
        entry.found = true;
    }
    return entry;
}

} // namespace

QVector<ToolchainEntry> Toolchain::probe(const QString& workspaceRoot)
{
    QVector<ToolchainEntry> entries;
    entries.append(make(QStringLiteral("git"), QStringLiteral("version control"),
                        QStringLiteral("install git")));
    entries.append(make(QStringLiteral("python"), QStringLiteral("Python runtime"),
                        QStringLiteral("install Python 3")));
    entries.append(make(QStringLiteral("node"), QStringLiteral("JavaScript runtime"),
                        QStringLiteral("install Node.js")));
    entries.append(make(QStringLiteral("npm"), QStringLiteral("package manager"),
                        QStringLiteral("install Node.js")));
    entries.append(make(QStringLiteral("npx"), QStringLiteral("package runner"),
                        QStringLiteral("install Node.js")));
    entries.append(make(QStringLiteral("pytest"), QStringLiteral("Python tests"),
                        QStringLiteral("pip install pytest")));

    // Playwright is usually a project-local dependency, not a global binary.
    bool playwright = false;
    QString playwrightPath;
    if (!workspaceRoot.isEmpty()) {
        const QStringList candidates = {
            QStringLiteral("/node_modules/.bin/playwright"),
            QStringLiteral("/node_modules/@playwright/test"),
            QStringLiteral("/.venv/bin/playwright"),
        };
        for (const QString& candidate : candidates) {
            const QString full = QDir(workspaceRoot).absoluteFilePath(candidate);
            if (QFileInfo::exists(full)) {
                playwright = true;
                playwrightPath = full;
                break;
            }
        }
    }
    if (const QString global = QStandardPaths::findExecutable(QStringLiteral("playwright")); !global.isEmpty()) {
        playwright = true;
        playwrightPath = global;
    }

    ToolchainEntry pw;
    pw.name = QStringLiteral("playwright");
    pw.purpose = QStringLiteral("browser tests");
    pw.hint = QStringLiteral("npm i -D @playwright/test");
    pw.found = playwright;
    pw.path = playwrightPath;
    entries.append(pw);

    return entries;
}

QString Toolchain::summary(const QString& workspaceRoot)
{
    QStringList lines;
    for (const ToolchainEntry& entry : probe(workspaceRoot)) {
        if (entry.found) {
            lines.append(QStringLiteral("%1 (%2)").arg(entry.name, entry.purpose));
        }
    }
    if (lines.isEmpty()) {
        return QStringLiteral("No external developer tools detected.");
    }
    return QStringLiteral("Available: ") + lines.join(QStringLiteral(", "));
}

} // namespace lulu
