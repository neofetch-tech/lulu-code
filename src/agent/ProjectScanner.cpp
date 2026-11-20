#include "agent/ProjectScanner.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>

#include <algorithm>
#include <functional>

namespace lulu {

namespace {

const QSet<QString>& ignoredDirs()
{
    static const QSet<QString> ignored = {
        QStringLiteral(".git"),        QStringLiteral(".hg"),        QStringLiteral(".svn"),
        QStringLiteral("node_modules"), QStringLiteral(".venv"),     QStringLiteral("venv"),
        QStringLiteral("__pycache__"),  QStringLiteral("build"),      QStringLiteral("dist"),
        QStringLiteral("out"),          QStringLiteral("target"),     QStringLiteral(".idea"),
        QStringLiteral(".vscode"),      QStringLiteral("coverage"),   QStringLiteral(".next"),
        QStringLiteral(".cache"),       QStringLiteral(".gradle"),    QStringLiteral("obj"),
        QStringLiteral("bin"),          QStringLiteral(".dart_tool"), QStringLiteral(".terraform"),
    };
    return ignored;
}

QString readSmallFile(const QString& path, int maxBytes = 64 * 1024)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromUtf8(file.read(maxBytes));
}

QString detectGitBranch(const QString& root)
{
    const QString headPath = root + QStringLiteral("/.git/HEAD");
    const QString head = readSmallFile(headPath, 1024).trimmed();
    if (head.startsWith(QStringLiteral("ref: refs/heads/"))) {
        return head.mid(static_cast<int>(QStringLiteral("ref: refs/heads/").size()));
    }
    if (!head.isEmpty() && !head.startsWith(QStringLiteral("ref:"))) {
        return head.left(8); // detached HEAD
    }
    return {};
}

} // namespace

QString ProjectInfo::summary() const
{
    if (!valid) {
        return QStringLiteral("No workspace folder is open.");
    }

    QString text;
    text += QStringLiteral("Project: %1\n").arg(name);
    text += QStringLiteral("Root: %1\n").arg(QDir::toNativeSeparators(root));
    if (!stacks.isEmpty()) {
        text += QStringLiteral("Stack: %1\n").arg(stacks.join(QStringLiteral(", ")));
    }
    if (!gitBranch.isEmpty()) {
        text += QStringLiteral("Git branch: %1\n").arg(gitBranch);
    }

    QStringList languages;
    QList<QPair<int, QString>> sorted;
    for (auto it = extensions.cbegin(); it != extensions.cend(); ++it) {
        sorted.append({it.value(), it.key()});
    }
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
        return a.first > b.first;
    });
    for (int i = 0; i < sorted.size() && i < 8; ++i) {
        languages.append(QStringLiteral("%1 %2").arg(sorted.at(i).second).arg(sorted.at(i).first));
    }
    text += QStringLiteral("%1 files").arg(fileCount);
    if (!languages.isEmpty()) {
        text += QStringLiteral(" (%1)").arg(languages.join(QStringLiteral(", ")));
    }
    text += QLatin1Char('\n');

    if (!topLevel.isEmpty()) {
        text += QStringLiteral("Top level: %1\n").arg(topLevel.join(QStringLiteral(", ")));
    }
    if (!testFolders.isEmpty()) {
        text += QStringLiteral("Test folders: %1\n").arg(testFolders.join(QStringLiteral(", ")));
    }
    return text.trimmed();
}

ProjectInfo ProjectScanner::scan(const QString& rootPath)
{
    ProjectInfo info;
    const QFileInfo rootInfo(rootPath);
    if (rootPath.isEmpty() || !rootInfo.isDir()) {
        return info;
    }

    info.valid = true;
    info.root = QDir(rootPath).absolutePath();
    info.name = rootInfo.fileName();
    if (info.name.isEmpty()) {
        info.name = info.root;
    }
    info.gitBranch = detectGitBranch(info.root);

    auto has = [&info](const QString& name) {
        return QFileInfo::exists(info.root + QLatin1Char('/') + name);
    };

    if (has(QStringLiteral("package.json"))) {
        const QString package = readSmallFile(info.root + QStringLiteral("/package.json"));
        QStringList nodeStack{QStringLiteral("Node.js")};
        if (package.contains(QStringLiteral("\"typescript\""))) {
            nodeStack.append(QStringLiteral("TypeScript"));
        }
        if (package.contains(QStringLiteral("\"react\""))) {
            nodeStack.append(QStringLiteral("React"));
        }
        if (package.contains(QStringLiteral("\"next\""))) {
            nodeStack.append(QStringLiteral("Next.js"));
        }
        if (package.contains(QStringLiteral("\"vue\""))) {
            nodeStack.append(QStringLiteral("Vue"));
        }
        if (package.contains(QStringLiteral("playwright"))) {
            nodeStack.append(QStringLiteral("Playwright"));
        }
        info.stacks.append(nodeStack.join(QStringLiteral(" + ")));
    }
    if (has(QStringLiteral("CMakeLists.txt"))) {
        info.stacks.append(QStringLiteral("C/C++ + CMake"));
    }
    if (has(QStringLiteral("pyproject.toml")) || has(QStringLiteral("requirements.txt"))) {
        info.stacks.append(QStringLiteral("Python"));
    }
    if (has(QStringLiteral("pytest.ini")) || has(QStringLiteral("conftest.py"))) {
        info.stacks.append(QStringLiteral("pytest"));
    }
    if (has(QStringLiteral("Cargo.toml"))) {
        info.stacks.append(QStringLiteral("Rust"));
    }
    if (has(QStringLiteral("go.mod"))) {
        info.stacks.append(QStringLiteral("Go"));
    }
    if (has(QStringLiteral("pom.xml"))) {
        info.stacks.append(QStringLiteral("Java + Maven"));
    }
    if (has(QStringLiteral("build.gradle"))) {
        info.stacks.append(QStringLiteral("Gradle"));
    }
    if (has(QStringLiteral("composer.json"))) {
        info.stacks.append(QStringLiteral("PHP"));
    }
    if (has(QStringLiteral("Gemfile"))) {
        info.stacks.append(QStringLiteral("Ruby"));
    }
    if (has(QStringLiteral("pubspec.yaml"))) {
        info.stacks.append(QStringLiteral("Dart/Flutter"));
    }

    for (const QString& name : {QStringLiteral("tests"), QStringLiteral("test"), QStringLiteral("__tests__"),
                                QStringLiteral("e2e"), QStringLiteral("spec"), QStringLiteral("t")}) {
        if (QFileInfo(info.root + QLatin1Char('/') + name).isDir()) {
            info.testFolders.append(name + QLatin1Char('/'));
        }
    }

    bool truncated = false;
    std::function<void(const QString&, int)> walk = [&](const QString& directory, int depth) {
        if (info.fileCount >= 6000) {
            truncated = true;
            return;
        }
        QDir dir(directory);
        const QFileInfoList entries = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot,
                                                       QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
        for (const QFileInfo& entry : entries) {
            if (info.fileCount >= 6000) {
                truncated = true;
                return;
            }
            if (entry.isDir()) {
                if (depth < 4 && !ignoredDirs().contains(entry.fileName())) {
                    walk(entry.absoluteFilePath(), depth + 1);
                }
                continue;
            }
            if (entry.size() > 4 * 1024 * 1024) {
                continue;
            }
            ++info.fileCount;
            QString extension = entry.suffix().toLower();
            if (extension.isEmpty()) {
                extension = QStringLiteral("(none)");
            }
            info.extensions[extension] += 1;
            if (info.fileCount >= 6000) {
                truncated = true;
                return;
            }
        }
    };
    walk(info.root, 0);

    const QFileInfoList top = QDir(info.root).entryInfoList(
        QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
    for (int i = 0; i < top.size() && i < 14; ++i) {
        const QFileInfo& entry = top.at(i);
        if (entry.isDir() && ignoredDirs().contains(entry.fileName())) {
            continue;
        }
        info.topLevel.append(entry.isDir() ? entry.fileName() + QLatin1Char('/') : entry.fileName());
    }
    if (truncated) {
        info.topLevel.append(QStringLiteral("..."));
    }

    return info;
}

} // namespace lulu
