#pragma once

#include <QString>
#include <QVector>

namespace lulu {

struct ToolchainEntry {
    QString name;
    QString purpose;
    QString path;
    QString hint;
    bool found = false;
};

/// Detects the local developer tools Lulu Code can drive as its "exoskeleton".
class Toolchain
{
public:
    static QVector<ToolchainEntry> probe(const QString& workspaceRoot = {});
    static QString summary(const QString& workspaceRoot = {});
};

} // namespace lulu
