#pragma once

#include <QMap>
#include <QString>
#include <QStringList>

namespace lulu {

struct ProjectInfo {
    bool valid = false;
    QString root;
    QString name;
    QString gitBranch;
    QStringList stacks;
    QStringList topLevel;
    QStringList testFolders;
    QMap<QString, int> extensions;
    int fileCount = 0;

    /// Compact, token-friendly description injected into the system prompt.
    QString summary() const;
};

/// Builds a cheap overview of a project: stack, size, language mix, git branch.
class ProjectScanner
{
public:
    static ProjectInfo scan(const QString& root);
};

} // namespace lulu
