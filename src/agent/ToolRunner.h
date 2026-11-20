#pragma once

#include "agent/ToolCall.h"

#include <QObject>
#include <QString>
#include <QVector>

#include <functional>

class QProcess;

namespace lulu {

enum class ApprovalKind {
    WriteFile,
    EditFile,
    DeleteFile,
    Command,
};

struct ApprovalRequest {
    ApprovalKind kind = ApprovalKind::Command;
    QString tool;
    QString title;
    QString detail;
};

using ApprovalHandler = std::function<bool(const ApprovalRequest&)>;

/// Executes the built-in tool set against one workspace folder.
///
/// Every mutating operation goes through an approval callback first. Shell
/// commands are asynchronous, so the UI never freezes while a test suite runs.
class ToolRunner : public QObject
{
    Q_OBJECT

public:
    explicit ToolRunner(QObject* parent = nullptr);

    void setWorkspace(const QString& root);
    QString workspace() const { return root_; }

    void setApprovalHandler(ApprovalHandler handler);
    void setAutoApproveWrites(bool enabled) { autoWrites_ = enabled; }
    void setAutoApproveCommands(bool enabled) { autoCommands_ = enabled; }

    bool busy() const { return activeProcess_ != nullptr; }
    void cancelActive();

    static QVector<ToolSpec> specs();
    static QString promptGuide();

    void execute(const ToolCall& call, std::function<void(const ToolResult&)> done);

signals:
    void workspaceChanged();

private:
    struct ProcessOutcome {
        int exitCode = -1;
        bool timedOut = false;
        bool truncated = false;
        bool cancelled = false;
        QString output;
        QString error;
    };

    void handleTool(const ToolCall& call, const std::function<void(const ToolResult&)>& done);
    void runProcess(const QString& program, const QStringList& args, const QString& cwd,
                    int timeoutMs, std::function<void(const ProcessOutcome&)> done);

    ToolResult listFiles(const QJsonObject& args);
    ToolResult readFile(const QJsonObject& args);
    ToolResult writeFile(const QJsonObject& args);
    ToolResult editFile(const QJsonObject& args);
    ToolResult searchFiles(const QJsonObject& args);
    ToolResult deleteFile(const QJsonObject& args);
    ToolResult detectToolchain(const QJsonObject& args);

    void runCommand(const QJsonObject& args, const std::function<void(const ToolResult&)>& done);
    void runTests(const QJsonObject& args, const std::function<void(const ToolResult&)>& done);
    void runGit(const QJsonObject& args, const std::function<void(const ToolResult&)>& done);

    QString resolvePath(const QString& candidate, bool mustExist, QString* error) const;
    bool approve(const ApprovalRequest& request);
    bool isIgnored(const QString& name) const;
    QString relative(const QString& absolute) const;

    QString root_;
    ApprovalHandler approval_;
    bool autoWrites_ = false;
    bool autoCommands_ = false;

    QProcess* activeProcess_ = nullptr;
    bool activeCancelled_ = false;
};

} // namespace lulu
