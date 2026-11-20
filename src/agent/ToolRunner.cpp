#include "agent/ToolRunner.h"

#include "agent/Toolchain.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QTimer>
#include <QtDebug>

#include <algorithm>

namespace lulu {

namespace {

constexpr qsizetype kMaxProcessOutput = 64 * 1024;
constexpr qsizetype kMaxReadBytes = 400 * 1024;
constexpr qsizetype kMaxSearchFileBytes = 2 * 1024 * 1024;

bool looksBinary(const QByteArray& data)
{
    return data.contains('\0');
}

QString humanBytes(qint64 bytes)
{
    if (bytes >= 1024 * 1024) {
        return QStringLiteral("%1 MB").arg(QString::number(static_cast<double>(bytes) / (1024.0 * 1024.0), 'f', 2));
    }
    if (bytes >= 1024) {
        return QStringLiteral("%1 KB").arg(QString::number(static_cast<double>(bytes) / 1024.0, 'f', 1));
    }
    return QStringLiteral("%1 B").arg(bytes);
}

QString truncateMiddle(const QString& text, int maxChars)
{
    if (text.size() <= maxChars) {
        return text;
    }
    const int head = maxChars * 2 / 3;
    const int tail = maxChars - head;
    return text.left(head) + QStringLiteral("\n... [truncated] ...\n") + text.right(tail);
}

QString pickString(const QJsonObject& args, std::initializer_list<const char*> keys)
{
    for (const char* key : keys) {
        const QJsonValue value = args.value(QLatin1String(key));
        if (value.isString() && !value.toString().isEmpty()) {
            return value.toString();
        }
    }
    return {};
}

} // namespace

ToolRunner::ToolRunner(QObject* parent)
    : QObject(parent)
{
}

void ToolRunner::setWorkspace(const QString& root)
{
    root_ = root.isEmpty() ? QString() : QDir(root).absolutePath();
}

void ToolRunner::setApprovalHandler(ApprovalHandler handler)
{
    approval_ = std::move(handler);
}

bool ToolRunner::approve(const ApprovalRequest& request)
{
    if (!approval_) {
        return false;
    }
    return approval_(request);
}

bool ToolRunner::isIgnored(const QString& name) const
{
    static const QSet<QString> ignored = {
        QStringLiteral(".git"),      QStringLiteral(".hg"),         QStringLiteral(".svn"),
        QStringLiteral("node_modules"), QStringLiteral(".venv"),    QStringLiteral("venv"),
        QStringLiteral("__pycache__"), QStringLiteral(".pytest_cache"),
        QStringLiteral(".mypy_cache"), QStringLiteral(".ruff_cache"),
        QStringLiteral("build"),     QStringLiteral("dist"),       QStringLiteral("out"),
        QStringLiteral("target"),    QStringLiteral(".idea"),      QStringLiteral(".vscode"),
        QStringLiteral(".gradle"),   QStringLiteral(".next"),      QStringLiteral(".nuxt"),
        QStringLiteral("coverage"),  QStringLiteral(".cache"),     QStringLiteral("bin"),
        QStringLiteral("obj"),       QStringLiteral(".dart_tool"), QStringLiteral(".terraform"),
    };
    return ignored.contains(name);
}

QString ToolRunner::relative(const QString& absolute) const
{
    const QString rel = QDir(root_).relativeFilePath(absolute);
    return QDir::fromNativeSeparators(rel);
}

QString ToolRunner::resolvePath(const QString& candidate, bool mustExist, QString* error) const
{
    if (root_.isEmpty()) {
        *error = QStringLiteral("No workspace folder is open. Ask the user to open a project folder first.");
        return {};
    }

    QString input = candidate.trimmed();
    if (input.isEmpty()) {
        input = QStringLiteral(".");
    }
    input = QDir::fromNativeSeparators(input);

    const QFileInfo rootInfo(root_);
    const QString rootAbs = QDir::cleanPath(rootInfo.absoluteFilePath());
    const QString rootCanonical = rootInfo.canonicalFilePath();
    const QString absolute = QDir::cleanPath(
        QDir::isAbsolutePath(input) ? input : rootAbs + QLatin1Char('/') + input);

    auto inside = [](const QString& base, const QString& path) {
#ifdef Q_OS_WIN
        const Qt::CaseSensitivity cs = Qt::CaseInsensitive;
#else
        const Qt::CaseSensitivity cs = Qt::CaseSensitive;
#endif
        return path.compare(base, cs) == 0 || path.startsWith(base + QLatin1Char('/'), cs);
    };

    if (!inside(rootAbs, absolute)) {
        *error = QStringLiteral("Refusing to access a path outside the workspace: %1").arg(candidate);
        return {};
    }

    const QFileInfo info(absolute);
    if (info.exists()) {
        const QString canonical = info.canonicalFilePath();
        if (!canonical.isEmpty() && !rootCanonical.isEmpty() && !inside(rootCanonical, canonical)) {
            *error = QStringLiteral("Refusing to follow a symlink outside the workspace: %1").arg(candidate);
            return {};
        }
    } else if (mustExist) {
        *error = QStringLiteral("Path does not exist: %1").arg(candidate);
        return {};
    }

    return absolute;
}

QVector<ToolSpec> ToolRunner::specs()
{
    return {
        {QStringLiteral("list_files"),
         QStringLiteral("path=\".\", depth=2, max_entries=400"),
         QStringLiteral("List files and folders in the workspace (ignores .git, node_modules, build, ...).")},
        {QStringLiteral("read_file"),
         QStringLiteral("path, start_line?, end_line?"),
         QStringLiteral("Read a UTF-8 text file with line numbers. Prefer reading with a line range for big files.")},
        {QStringLiteral("write_file"),
         QStringLiteral("path, content"),
         QStringLiteral("Create a file or replace its whole contents. Creates parent folders.")},
        {QStringLiteral("edit_file"),
         QStringLiteral("path, old_text, new_text, replace_all=false"),
         QStringLiteral("Replace an exact text fragment in a file. old_text must match exactly once unless replace_all=true.")},
        {QStringLiteral("search_files"),
         QStringLiteral("query, path=\".\", glob=\"*\", regex=false, max_results=60"),
         QStringLiteral("Search text across workspace files. Returns file:line: text matches.")},
        {QStringLiteral("delete_file"),
         QStringLiteral("path"),
         QStringLiteral("Delete a single file from the workspace.")},
        {QStringLiteral("run_command"),
         QStringLiteral("command, cwd=\".\", timeout_seconds=120"),
         QStringLiteral("Run a shell command in the workspace and return its combined output and exit code.")},
        {QStringLiteral("run_tests"),
         QStringLiteral("framework=\"auto\""),
         QStringLiteral("Detect and run the project's tests (pytest, Playwright, npm, ctest, cargo, go).")},
        {QStringLiteral("git"),
         QStringLiteral("action, args=[], message=\"\""),
         QStringLiteral("Run a git command: status, diff, log, show, add, commit, restore, branch, checkout, switch, stash, ls-files.")},
        {QStringLiteral("detect_toolchain"),
         QStringLiteral(""),
         QStringLiteral("Report which developer tools are available (git, python, node, npm, pytest, Playwright).")},
    };
}

QString ToolRunner::promptGuide()
{
    QString guide;
    for (const ToolSpec& spec : specs()) {
        guide += QStringLiteral("- %1(%2): %3\n").arg(spec.name, spec.signature, spec.description);
    }
    return guide;
}

void ToolRunner::cancelActive()
{
    if (!activeProcess_) {
        return;
    }
    activeCancelled_ = true;
    activeProcess_->kill();
}

void ToolRunner::runProcess(const QString& program, const QStringList& args, const QString& cwd,
                            int timeoutMs, std::function<void(const ProcessOutcome&)> done)
{
    auto* process = new QProcess(this);
    process->setWorkingDirectory(cwd);
    process->setProcessChannelMode(QProcess::MergedChannels);

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("NO_COLOR"), QStringLiteral("1"));
    environment.insert(QStringLiteral("GIT_PAGER"), QStringLiteral("cat"));
    environment.insert(QStringLiteral("PAGER"), QStringLiteral("cat"));
    environment.insert(QStringLiteral("PYTHONIOENCODING"), QStringLiteral("utf-8"));
    environment.insert(QStringLiteral("PYTHONUNBUFFERED"), QStringLiteral("1"));
    process->setProcessEnvironment(environment);

    auto output = std::make_shared<QByteArray>();
    auto truncated = std::make_shared<bool>(false);
    auto timedOut = std::make_shared<bool>(false);
    auto finished = std::make_shared<bool>(false);

    activeProcess_ = process;
    activeCancelled_ = false;

    auto appendChunk = [output, truncated](const QByteArray& chunk) {
        if (output->size() + chunk.size() > kMaxProcessOutput) {
            const qsizetype room = kMaxProcessOutput - output->size();
            if (room > 0) {
                output->append(chunk.left(room));
            }
            *truncated = true;
        } else {
            output->append(chunk);
        }
    };

    connect(process, &QProcess::readyRead, this, [process, appendChunk] {
        appendChunk(process->readAll());
    });

    auto* timer = new QTimer(process);
    timer->setSingleShot(true);

    auto complete = [this, process, timer, output, truncated, timedOut, finished, appendChunk,
                     done](int exitCode, const QString& startError) {
        if (*finished) {
            return;
        }
        *finished = true;
        timer->stop();
        appendChunk(process->readAllStandardOutput());
        appendChunk(process->readAllStandardError());

        ProcessOutcome outcome;
        outcome.exitCode = exitCode;
        outcome.timedOut = *timedOut;
        outcome.truncated = *truncated;
        outcome.cancelled = activeCancelled_;
        outcome.output = QString::fromUtf8(*output);
        outcome.error = startError;
        if (outcome.timedOut) {
            outcome.error = QStringLiteral("Command timed out.");
        } else if (outcome.cancelled) {
            outcome.error = QStringLiteral("Cancelled by user.");
        }

        if (activeProcess_ == process) {
            activeProcess_ = nullptr;
        }
        process->deleteLater();
        done(outcome);
    };

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [complete](int exitCode, QProcess::ExitStatus) { complete(exitCode, {}); });
    connect(process, &QProcess::errorOccurred, this, [complete, process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            complete(-1, QStringLiteral("Could not start '%1': %2").arg(process->program(), process->errorString()));
        }
    });
    connect(timer, &QTimer::timeout, this, [process, timedOut] {
        *timedOut = true;
        process->kill();
    });

    if (timeoutMs > 0) {
        timer->start(timeoutMs);
    }
    process->start(program, args);
    if (!process->waitForStarted(3000)) {
        // errorOccurred/FailedToStart usually fires as well; guard with complete().
        complete(-1, QStringLiteral("Could not start '%1': %2").arg(program, process->errorString()));
    }
}

ToolResult ToolRunner::listFiles(const QJsonObject& args)
{
    QString error;
    const QString path = resolvePath(pickString(args, {"path"}), true, &error);
    if (path.isEmpty()) {
        return ToolResult::failure(error);
    }
    if (!QFileInfo(path).isDir()) {
        return ToolResult::failure(QStringLiteral("Not a directory: %1").arg(relative(path)));
    }

    const int depth = std::clamp(args.value(QStringLiteral("depth")).toInt(2), 1, 8);
    const int maxEntries = std::clamp(args.value(QStringLiteral("max_entries")).toInt(400), 10, 2000);

    QStringList lines;
    bool cut = false;

    std::function<void(const QString&, int)> walk = [&](const QString& directory, int level) {
        if (lines.size() >= maxEntries) {
            cut = true;
            return;
        }
        QDir dir(directory);
        const QFileInfoList entries = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot,
                                                       QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
        for (const QFileInfo& entry : entries) {
            if (lines.size() >= maxEntries) {
                cut = true;
                return;
            }
            if (entry.isDir() && isIgnored(entry.fileName())) {
                continue;
            }
            const QString indent(static_cast<qsizetype>(level * 2), QLatin1Char(' '));
            const QString rel = relative(entry.absoluteFilePath());
            if (entry.isDir()) {
                lines.append(indent + rel + QLatin1Char('/'));
                if (level < depth) {
                    walk(entry.absoluteFilePath(), level + 1);
                }
            } else {
                lines.append(indent + rel + QStringLiteral("  (") + humanBytes(entry.size()) + QLatin1Char(')'));
            }
        }
    };

    walk(path, 0);
    if (lines.isEmpty()) {
        return ToolResult::success(QStringLiteral("The folder is empty."));
    }

    QString text = QStringLiteral("Workspace: %1\n").arg(relative(path));
    text += lines.join(QLatin1Char('\n'));
    if (cut) {
        text += QStringLiteral("\n... truncated at %1 entries").arg(maxEntries);
    }
    return ToolResult::success(text);
}

ToolResult ToolRunner::readFile(const QJsonObject& args)
{
    QString error;
    const QString path = resolvePath(pickString(args, {"path"}), true, &error);
    if (path.isEmpty()) {
        return ToolResult::failure(error);
    }
    QFileInfo info(path);
    if (info.isDir()) {
        return ToolResult::failure(QStringLiteral("%1 is a directory, not a file.").arg(relative(path)));
    }
    if (info.size() > kMaxReadBytes * 4 && !args.contains(QStringLiteral("end_line"))) {
        return ToolResult::failure(QStringLiteral("File is %1. Read it in ranges with start_line/end_line.")
                                       .arg(humanBytes(info.size())));
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return ToolResult::failure(QStringLiteral("Cannot read %1: %2").arg(relative(path), file.errorString()));
    }
    const QByteArray data = file.read(kMaxReadBytes + 1);
    if (looksBinary(data)) {
        return ToolResult::failure(QStringLiteral("%1 looks like a binary file.").arg(relative(path)));
    }

    QStringList allLines = QString::fromUtf8(data).split(QLatin1Char('\n'));
    if (data.size() > kMaxReadBytes) {
        allLines.removeLast();
        allLines.append(QStringLiteral("... [file continues]"));
    }

    const int total = static_cast<int>(allLines.size());
    int start = std::max(1, args.value(QStringLiteral("start_line")).toInt(1));
    int end = args.value(QStringLiteral("end_line")).toInt(0);
    if (end <= 0 || end > total) {
        end = total;
    }
    if (start > end) {
        return ToolResult::failure(QStringLiteral("start_line is greater than end_line."));
    }
    const int maxLines = 2500;
    bool truncated = false;
    if (end - start + 1 > maxLines) {
        end = start + maxLines - 1;
        truncated = true;
    }

    const int width = QString::number(end).size();
    QString text = QStringLiteral("%1  (lines %2-%3 of %4)\n").arg(relative(path)).arg(start).arg(end).arg(total);
    for (int line = start; line <= end; ++line) {
        text += QStringLiteral("%1 | %2\n")
                    .arg(QString::number(line).rightJustified(width),
                         allLines.at(line - 1));
    }
    if (truncated) {
        text += QStringLiteral("... [use start_line/end_line to read more]\n");
    }
    return ToolResult::success(text);
}

ToolResult ToolRunner::writeFile(const QJsonObject& args)
{
    QString error;
    const QString path = resolvePath(pickString(args, {"path"}), false, &error);
    if (path.isEmpty()) {
        return ToolResult::failure(error);
    }
    if (QFileInfo(path).isDir()) {
        return ToolResult::failure(QStringLiteral("%1 is a directory.").arg(relative(path)));
    }

    const QString content = args.value(QStringLiteral("content")).toString();
    const QFileInfo info(path);
    if (info.exists() && info.size() < kMaxReadBytes) {
        QFile existing(path);
        if (existing.open(QIODevice::ReadOnly) && existing.readAll() == content.toUtf8()) {
            return ToolResult::success(QStringLiteral("No changes: %1 already has this content.").arg(relative(path)));
        }
    }

    if (!autoWrites_ && !approve(ApprovalRequest{ApprovalKind::WriteFile,
                                                 QStringLiteral("write_file"),
                                                 QStringLiteral("Write %1").arg(relative(path)),
                                                 QStringLiteral("%1 lines, %2\n\n%3")
                                                     .arg(content.count(QLatin1Char('\n')) + 1)
                                                     .arg(humanBytes(content.toUtf8().size()))
                                                     .arg(truncateMiddle(content, 1200))})) {
        return ToolResult::failure(QStringLiteral("The user rejected this file write."));
    }

    QDir().mkpath(info.absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return ToolResult::failure(QStringLiteral("Cannot write %1: %2").arg(relative(path), file.errorString()));
    }
    file.write(content.toUtf8());
    if (!file.commit()) {
        return ToolResult::failure(QStringLiteral("Failed to save %1: %2").arg(relative(path), file.errorString()));
    }

    emit workspaceChanged();
    return ToolResult::success(QStringLiteral("Wrote %1 (%2).").arg(relative(path), humanBytes(content.toUtf8().size())));
}

ToolResult ToolRunner::editFile(const QJsonObject& args)
{
    QString error;
    const QString path = resolvePath(pickString(args, {"path"}), true, &error);
    if (path.isEmpty()) {
        return ToolResult::failure(error);
    }

    const QString oldText = args.value(QStringLiteral("old_text")).toString();
    const QString newText = args.value(QStringLiteral("new_text")).toString();
    const bool replaceAll = args.value(QStringLiteral("replace_all")).toBool(false);

    if (oldText.isEmpty()) {
        return ToolResult::failure(QStringLiteral("old_text must not be empty. Use write_file to create a file."));
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return ToolResult::failure(QStringLiteral("Cannot read %1: %2").arg(relative(path), file.errorString()));
    }
    QString content = QString::fromUtf8(file.readAll());
    file.close();

    const qsizetype occurrences = content.count(oldText);
    if (occurrences == 0) {
        return ToolResult::failure(QStringLiteral("old_text was not found in %1. Read the file again and copy the exact text, "
                                                  "including indentation.")
                                       .arg(relative(path)));
    }
    if (occurrences > 1 && !replaceAll) {
        return ToolResult::failure(QStringLiteral("old_text appears %1 times in %2. Add more surrounding context, "
                                                  "or pass replace_all=true.")
                                       .arg(occurrences)
                                       .arg(relative(path)));
    }

    if (!autoWrites_ && !approve(ApprovalRequest{ApprovalKind::EditFile,
                                                 QStringLiteral("edit_file"),
                                                 QStringLiteral("Edit %1").arg(relative(path)),
                                                 QStringLiteral("Replace %1 occurrence(s).\n\n--- before ---\n%2\n\n--- after ---\n%3")
                                                     .arg(occurrences)
                                                     .arg(truncateMiddle(oldText, 800), truncateMiddle(newText, 800))})) {
        return ToolResult::failure(QStringLiteral("The user rejected this edit."));
    }

    content.replace(oldText, newText);
    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly)) {
        return ToolResult::failure(QStringLiteral("Cannot write %1: %2").arg(relative(path), out.errorString()));
    }
    out.write(content.toUtf8());
    if (!out.commit()) {
        return ToolResult::failure(QStringLiteral("Failed to save %1: %2").arg(relative(path), out.errorString()));
    }

    emit workspaceChanged();
    return ToolResult::success(QStringLiteral("Edited %1 (%2 replacement(s)).").arg(relative(path)).arg(occurrences));
}

ToolResult ToolRunner::searchFiles(const QJsonObject& args)
{
    QString query = args.value(QStringLiteral("query")).toString();
    if (query.isEmpty()) {
        return ToolResult::failure(QStringLiteral("query must not be empty."));
    }

    QString error;
    const QString path = resolvePath(pickString(args, {"path"}), true, &error);
    if (path.isEmpty()) {
        return ToolResult::failure(error);
    }

    const bool useRegex = args.value(QStringLiteral("regex")).toBool(false);
    const bool caseSensitive = args.value(QStringLiteral("case_sensitive")).toBool(false);
    const int maxResults = std::clamp(args.value(QStringLiteral("max_results")).toInt(60), 1, 300);
    QString glob = args.value(QStringLiteral("glob")).toString();
    if (glob.isEmpty()) {
        glob = QStringLiteral("*");
    }

    QRegularExpression expression(useRegex ? query : QRegularExpression::escape(query));
    if (!useRegex) {
        expression.setPatternOptions(caseSensitive ? QRegularExpression::NoPatternOption
                                                   : QRegularExpression::CaseInsensitiveOption);
    } else if (!caseSensitive) {
        expression.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
    }
    if (!expression.isValid()) {
        return ToolResult::failure(QStringLiteral("Invalid regular expression: %1").arg(expression.errorString()));
    }

    QStringList results;
    bool truncated = false;
    int scanned = 0;

    std::function<void(const QString&)> walk = [&](const QString& directory) {
        if (results.size() >= maxResults) {
            truncated = true;
            return;
        }
        QDir dir(directory);
        const QFileInfoList entries = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot,
                                                       QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
        for (const QFileInfo& entry : entries) {
            if (results.size() >= maxResults) {
                truncated = true;
                return;
            }
            if (entry.isDir()) {
                if (!isIgnored(entry.fileName())) {
                    walk(entry.absoluteFilePath());
                }
                continue;
            }
            if (entry.size() > kMaxSearchFileBytes) {
                continue;
            }
            if (!QDir::match(glob, entry.fileName())) {
                continue;
            }
            QFile file(entry.absoluteFilePath());
            if (!file.open(QIODevice::ReadOnly)) {
                continue;
            }
            const QByteArray data = file.read(kMaxSearchFileBytes);
            if (looksBinary(data)) {
                continue;
            }
            ++scanned;
            const QStringList lines = QString::fromUtf8(data).split(QLatin1Char('\n'));
            for (int i = 0; i < lines.size(); ++i) {
                if (!expression.match(lines.at(i)).hasMatch()) {
                    continue;
                }
                QString line = lines.at(i).trimmed();
                if (line.size() > 240) {
                    line = line.left(240) + QStringLiteral("...");
                }
                results.append(QStringLiteral("%1:%2: %3").arg(relative(entry.absoluteFilePath())).arg(i + 1).arg(line));
                if (results.size() >= maxResults) {
                    truncated = true;
                    return;
                }
            }
        }
    };
    walk(path);

    if (results.isEmpty()) {
        return ToolResult::success(QStringLiteral("No matches for '%1' (scanned %2 files).").arg(query).arg(scanned));
    }
    QString text = QStringLiteral("Found %1 match(es) in %2 file(s):\n%3")
                       .arg(results.size())
                       .arg(scanned)
                       .arg(results.join(QLatin1Char('\n')));
    if (truncated) {
        text += QStringLiteral("\n... [more results available]");
    }
    return ToolResult::success(text);
}

ToolResult ToolRunner::deleteFile(const QJsonObject& args)
{
    QString error;
    const QString path = resolvePath(pickString(args, {"path"}), true, &error);
    if (path.isEmpty()) {
        return ToolResult::failure(error);
    }
    if (QFileInfo(path).isDir()) {
        return ToolResult::failure(QStringLiteral("delete_file only removes files, not folders."));
    }
    if (!approve(ApprovalRequest{ApprovalKind::DeleteFile,
                                 QStringLiteral("delete_file"),
                                 QStringLiteral("Delete %1").arg(relative(path)),
                                 QStringLiteral("This cannot be undone (unless the file is in git).")})) {
        return ToolResult::failure(QStringLiteral("The user rejected this deletion."));
    }
    if (!QFile::remove(path)) {
        return ToolResult::failure(QStringLiteral("Could not delete %1.").arg(relative(path)));
    }
    emit workspaceChanged();
    return ToolResult::success(QStringLiteral("Deleted %1.").arg(relative(path)));
}

ToolResult ToolRunner::detectToolchain(const QJsonObject& args)
{
    Q_UNUSED(args);
    QStringList lines;
    for (const ToolchainEntry& entry : Toolchain::probe(root_)) {
        lines.append(QStringLiteral("%1 %2  %3")
                         .arg(entry.found ? QStringLiteral("[ok]  ") : QStringLiteral("[miss]"),
                              entry.name,
                              entry.found ? entry.path : entry.hint));
    }
    return ToolResult::success(lines.join(QLatin1Char('\n')));
}

void ToolRunner::runCommand(const QJsonObject& args, const std::function<void(const ToolResult&)>& done)
{
    const QString command = args.value(QStringLiteral("command")).toString().trimmed();
    if (command.isEmpty()) {
        done(ToolResult::failure(QStringLiteral("command must not be empty.")));
        return;
    }

    const QString lowered = command.toLower();
    static const QStringList blocked = {
        QStringLiteral("rm -rf /"), QStringLiteral("rm -rf ~"), QStringLiteral("mkfs"),
        QStringLiteral("format c:"), QStringLiteral("del /f /s /q c:\\"),
        QStringLiteral(":(){"), QStringLiteral("shutdown"), QStringLiteral("diskpart"),
    };
    for (const QString& pattern : blocked) {
        if (lowered.contains(pattern)) {
            done(ToolResult::failure(QStringLiteral("This command is blocked for safety: %1").arg(command)));
            return;
        }
    }

    QString error;
    QString cwd = root_;
    const QString cwdArg = pickString(args, {"cwd"});
    if (!cwdArg.isEmpty()) {
        cwd = resolvePath(cwdArg, true, &error);
        if (cwd.isEmpty()) {
            done(ToolResult::failure(error));
            return;
        }
    }
    if (!QFileInfo(cwd).isDir()) {
        done(ToolResult::failure(QStringLiteral("Working directory is not a folder: %1").arg(cwd)));
        return;
    }

    const int timeoutSeconds = std::clamp(args.value(QStringLiteral("timeout_seconds")).toInt(120), 5, 900);

    if (!autoCommands_ && !approve(ApprovalRequest{ApprovalKind::Command,
                                                   QStringLiteral("run_command"),
                                                   QStringLiteral("Run a command"),
                                                   QStringLiteral("$ %1\n\nin %2\n(timeout %3s)")
                                                       .arg(command, relative(cwd))
                                                       .arg(timeoutSeconds)})) {
        done(ToolResult::failure(QStringLiteral("The user rejected this command.")));
        return;
    }

    auto finish = [done, command](const ProcessOutcome& outcome) {
        if (outcome.cancelled) {
            done(ToolResult::failure(QStringLiteral("The command was cancelled.")));
            return;
        }
        if (!outcome.error.isEmpty()) {
            done(ToolResult::failure(outcome.error + QStringLiteral("\n\nOutput:\n") + outcome.output));
            return;
        }
        QString text = QStringLiteral("$ %1\nexit code: %2\n\n%3")
                           .arg(command)
                           .arg(outcome.exitCode)
                           .arg(outcome.output.isEmpty() ? QStringLiteral("(no output)") : outcome.output);
        if (outcome.truncated) {
            text += QStringLiteral("\n... [output truncated]");
        }
        if (outcome.exitCode == 0) {
            done(ToolResult::success(text));
        } else {
            done(ToolResult::failure(text));
        }
    };

#ifdef Q_OS_WIN
    const QString full = QStringLiteral("chcp 65001>nul & ") + command;
    runProcess(QStringLiteral("cmd.exe"),
               {QStringLiteral("/d"), QStringLiteral("/s"), QStringLiteral("/c"), full},
               cwd, timeoutSeconds * 1000, finish);
#else
    runProcess(QStringLiteral("/bin/sh"), {QStringLiteral("-c"), command}, cwd,
               timeoutSeconds * 1000, finish);
#endif
}

void ToolRunner::runTests(const QJsonObject& args, const std::function<void(const ToolResult&)>& done)
{
    const QString framework = args.value(QStringLiteral("framework")).toString(QStringLiteral("auto"));
    const QString root = root_;
    QString command;
    QString detected = framework;

    auto exists = [&root](const QString& name) { return QFileInfo::exists(root + QLatin1Char('/') + name); };
    auto readText = [&root](const QString& name) {
        QFile file(root + QLatin1Char('/') + name);
        if (!file.open(QIODevice::ReadOnly)) {
            return QString();
        }
        return QString::fromUtf8(file.read(256 * 1024));
    };

    if (framework == QStringLiteral("auto")) {
        const QString packageJson = readText(QStringLiteral("package.json"));
        const QString pyproject = readText(QStringLiteral("pyproject.toml"));
        if (!packageJson.isEmpty() && packageJson.contains(QStringLiteral("playwright"), Qt::CaseInsensitive)) {
            command = QStringLiteral("npx playwright test");
            detected = QStringLiteral("playwright");
        } else if (!packageJson.isEmpty()) {
            command = QStringLiteral("npm test");
            detected = QStringLiteral("npm");
        } else if (exists(QStringLiteral("Cargo.toml"))) {
            command = QStringLiteral("cargo test");
            detected = QStringLiteral("cargo");
        } else if (exists(QStringLiteral("go.mod"))) {
            command = QStringLiteral("go test ./...");
            detected = QStringLiteral("go");
        } else if (exists(QStringLiteral("pytest.ini")) || exists(QStringLiteral("conftest.py"))
                   || exists(QStringLiteral("tox.ini")) || exists(QStringLiteral("tests"))
                   || pyproject.contains(QStringLiteral("[tool.pytest"))) {
            command = QStringLiteral("python -m pytest -q");
            detected = QStringLiteral("pytest");
        } else if (exists(QStringLiteral("CMakeLists.txt"))) {
            if (exists(QStringLiteral("build/CTestTestfile.cmake"))) {
                command = QStringLiteral("ctest --test-dir build --output-on-failure");
            } else {
                command = QStringLiteral("cmake -S . -B build && cmake --build build -j && ctest --test-dir build --output-on-failure");
            }
            detected = QStringLiteral("ctest");
        }
    } else if (framework == QStringLiteral("pytest") || framework == QStringLiteral("selenium")) {
        command = QStringLiteral("python -m pytest -q");
        detected = QStringLiteral("pytest");
    } else if (framework == QStringLiteral("playwright")) {
        command = QStringLiteral("npx playwright test");
    } else if (framework == QStringLiteral("npm")) {
        command = QStringLiteral("npm test");
    } else if (framework == QStringLiteral("ctest")) {
        command = QStringLiteral("ctest --test-dir build --output-on-failure");
    } else if (framework == QStringLiteral("cargo")) {
        command = QStringLiteral("cargo test");
    } else if (framework == QStringLiteral("go")) {
        command = QStringLiteral("go test ./...");
    }

    if (command.isEmpty()) {
        done(ToolResult::failure(QStringLiteral(
            "Could not detect a test suite. Pass framework= one of: pytest, playwright, selenium, npm, ctest, cargo, go. "
            "Or ask the user how this project is tested.")));
        return;
    }

    QJsonObject synthetic;
    synthetic.insert(QStringLiteral("command"), command);
    synthetic.insert(QStringLiteral("timeout_seconds"), 600);

    runCommand(synthetic, [done, command, detected](const ToolResult& result) {
        ToolResult copy = result;
        const QString header = QStringLiteral("Test framework: %1\nCommand: %2\n\n").arg(detected, command);
        copy.output = header + copy.output;
        if (!copy.error.isEmpty()) {
            copy.error = header + copy.error;
        }
        done(copy);
    });
}

void ToolRunner::runGit(const QJsonObject& args, const std::function<void(const ToolResult&)>& done)
{
    const QString action = args.value(QStringLiteral("action")).toString().trimmed();
    static const QSet<QString> allowed = {
        QStringLiteral("status"), QStringLiteral("diff"), QStringLiteral("log"), QStringLiteral("show"),
        QStringLiteral("add"),    QStringLiteral("commit"), QStringLiteral("restore"),
        QStringLiteral("branch"), QStringLiteral("checkout"), QStringLiteral("switch"),
        QStringLiteral("stash"),  QStringLiteral("ls-files"), QStringLiteral("blame"),
        QStringLiteral("rev-parse"), QStringLiteral("remote"), QStringLiteral("show-ref"),
    };
    if (!allowed.contains(action)) {
        return done(ToolResult::failure(
            QStringLiteral("Unsupported git action '%1'. Allowed: %2")
                .arg(action, QStringList(allowed.cbegin(), allowed.cend()).join(QStringLiteral(", ")))));
    }

    QStringList extra;
    const QJsonArray extraArray = args.value(QStringLiteral("args")).toArray();
    for (const QJsonValue& value : extraArray) {
        extra.append(value.toString());
    }

    QStringList gitArgs{action};
    if (action == QStringLiteral("status") && extra.isEmpty()) {
        gitArgs << QStringLiteral("--short") << QStringLiteral("--branch");
    } else if (action == QStringLiteral("log") && extra.isEmpty()) {
        gitArgs << QStringLiteral("--oneline") << QStringLiteral("-20");
    } else if (action == QStringLiteral("commit")) {
        const QString message = args.value(QStringLiteral("message")).toString().trimmed();
        if (message.isEmpty()) {
            return done(ToolResult::failure(QStringLiteral("commit requires a non-empty message.")));
        }
        gitArgs << QStringLiteral("-m") << message;
    }
    gitArgs << extra;

    const QString pretty = QStringLiteral("git ") + gitArgs.join(QLatin1Char(' '));
    if (!autoCommands_ && !approve(ApprovalRequest{ApprovalKind::Command,
                                                   QStringLiteral("git"),
                                                   QStringLiteral("Run git"),
                                                   pretty + QStringLiteral("\n\nin ") + QDir(root_).dirName()})) {
        return done(ToolResult::failure(QStringLiteral("The user rejected this git command.")));
    }

    runProcess(QStringLiteral("git"), gitArgs, root_, 300 * 1000,
               [done, pretty](const ProcessOutcome& outcome) {
                   if (!outcome.error.isEmpty() && outcome.output.isEmpty()) {
                       return done(ToolResult::failure(outcome.error));
                   }
                   QString text = QStringLiteral("$ %1\nexit code: %2\n\n%3")
                                      .arg(pretty)
                                      .arg(outcome.exitCode)
                                      .arg(outcome.output.isEmpty() ? QStringLiteral("(no output)") : outcome.output);
                   if (outcome.truncated) {
                       text += QStringLiteral("\n... [output truncated]");
                   }
                   done(outcome.exitCode == 0 ? ToolResult::success(text) : ToolResult::failure(text));
               });
}

void ToolRunner::handleTool(const ToolCall& call, const std::function<void(const ToolResult&)>& done)
{
    if (call.name == QStringLiteral("list_files")) {
        done(listFiles(call.args));
    } else if (call.name == QStringLiteral("read_file")) {
        done(readFile(call.args));
    } else if (call.name == QStringLiteral("write_file")) {
        done(writeFile(call.args));
    } else if (call.name == QStringLiteral("edit_file")) {
        done(editFile(call.args));
    } else if (call.name == QStringLiteral("search_files")) {
        done(searchFiles(call.args));
    } else if (call.name == QStringLiteral("delete_file")) {
        done(deleteFile(call.args));
    } else if (call.name == QStringLiteral("run_command")) {
        runCommand(call.args, done);
    } else if (call.name == QStringLiteral("run_tests")) {
        runTests(call.args, done);
    } else if (call.name == QStringLiteral("git")) {
        runGit(call.args, done);
    } else if (call.name == QStringLiteral("detect_toolchain")) {
        done(detectToolchain(call.args));
    } else {
        QStringList names;
        for (const ToolSpec& spec : specs()) {
            names.append(spec.name);
        }
        done(ToolResult::failure(QStringLiteral("Unknown tool '%1'. Available: %2")
                                     .arg(call.name, names.join(QStringLiteral(", ")))));
    }
}

void ToolRunner::execute(const ToolCall& call, std::function<void(const ToolResult&)> done)
{
    if (call.name.isEmpty()) {
        done(ToolResult::failure(QStringLiteral("Tool call has no name.")));
        return;
    }
    try {
        handleTool(call, done);
    } catch (const std::exception& error) {
        done(ToolResult::failure(QString::fromUtf8(error.what())));
    }
}

} // namespace lulu
