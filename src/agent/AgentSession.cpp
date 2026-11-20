#include "agent/AgentSession.h"

#include "agent/ToolRunner.h"
#include "agent/Toolchain.h"
#include "core/LlamaEngine.h"

#include <QDateTime>
#include <QDir>

namespace lulu {

namespace {

QString clip(const QString& text, int maxChars)
{
    if (text.size() <= maxChars) {
        return text;
    }
    return text.left(maxChars) + QStringLiteral("\n... [result truncated to fit the context window]");
}

QString sanitizeToolOutput(QString text)
{
    text.replace(QStringLiteral("</tool_result>"), QStringLiteral("[/tool_result]"));
    return text;
}

const QString& protocolHelp()
{
    static const QString help = QString::fromUtf8(R"HELP(
HOW TO CALL TOOLS
When you need a tool, reply with one or more fenced blocks in exactly this format:
```lulu-tool
{"tool": "read_file", "args": {"path": "src/main.cpp"}}
```
Rules for tool calls:
- The JSON object must contain "tool" and "args".
- One JSON object per block. Several blocks in one reply are executed in order.
- Do not mix prose and tool blocks in the same reply.
- Tool results come back as <tool_result> messages. Read them, then continue.
- When the task is done, answer the user in plain text with no tool blocks.
- Never guess a file's contents: read it first. Never repeat a call that just failed with the same arguments.
)HELP");
    return help;
}

} // namespace

AgentSession::AgentSession(QObject* parent)
    : QObject(parent)
{
}

void AgentSession::attachEngine(LlamaEngine* engine)
{
    connect(this, &AgentSession::generationRequested, engine, &LlamaEngine::generate,
            Qt::QueuedConnection);
    connect(this, &AgentSession::cancelGenerationRequested, engine,
            [engine] { engine->cancel(); }, Qt::DirectConnection);
    connect(engine, &LlamaEngine::token, this, &AgentSession::onEngineToken, Qt::QueuedConnection);
    connect(engine, &LlamaEngine::finished, this, &AgentSession::onEngineFinished, Qt::QueuedConnection);
    connect(engine, &LlamaEngine::failed, this, &AgentSession::onEngineFailed, Qt::QueuedConnection);
}

void AgentSession::setToolRunner(ToolRunner* runner)
{
    tools_ = runner;
}

void AgentSession::setWorkspace(const QString& root)
{
    workspace_ = root.isEmpty() ? QString() : QDir(root).absolutePath();
    project_ = ProjectScanner::scan(workspace_);
}

void AgentSession::setModelReady(bool ready)
{
    modelReady_ = ready;
    if (!ready && state_ != State::Idle) {
        cancel();
    }
}

void AgentSession::setState(State state, const QString& detail)
{
    state_ = state;
    emit stateChanged(state_, detail);
}

void AgentSession::reset()
{
    cancel();
    history_.clear();
    pendingCalls_.clear();
    streamText_.clear();
    assistantId_.clear();
    step_ = 0;
    cancelRequested_ = false;
    setState(State::Idle, QStringLiteral("Ready"));
}

void AgentSession::send(const QString& text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    if (!modelReady_) {
        emit errorOccurred(QStringLiteral("No model is loaded. Download or load a model first."));
        return;
    }
    if (state_ != State::Idle) {
        emit notice(QStringLiteral("Lulu Code is still working. Stop the current run first."));
        return;
    }

    history_.append(ChatMessage::user(trimmed));
    emit userMessage(trimmed);

    step_ = 0;
    cancelRequested_ = false;
    startGeneration();
}

void AgentSession::cancel()
{
    if (state_ == State::Generating) {
        cancelRequested_ = true;
        emit cancelGenerationRequested();
    } else if (state_ == State::RunningTool) {
        cancelRequested_ = true;
        if (tools_) {
            tools_->cancelActive();
        }
    }
}

void AgentSession::startGeneration()
{
    if (state_ != State::Idle) {
        return;
    }
    if (!modelReady_) {
        emit errorOccurred(QStringLiteral("No model is loaded."));
        return;
    }

    cancelRequested_ = false;
    streamText_.clear();
    assistantId_ = QStringLiteral("assistant-%1").arg(++messageCounter_);
    emit assistantStarted(assistantId_);
    setState(State::Generating, QStringLiteral("Thinking"));

    QVector<ChatMessage> messages;
    messages.append(ChatMessage::system(buildSystemPrompt()));
    messages += history_;

    emit generationRequested(messages, params_);
}

void AgentSession::onEngineToken(const QString& text)
{
    if (state_ != State::Generating) {
        return;
    }
    streamText_ += text;
    emit assistantDelta(assistantId_, text);
}

void AgentSession::onEngineFinished(const QString& text, const TokenStats& stats, bool cancelled)
{
    if (state_ != State::Generating) {
        return;
    }

    const QString raw = text.isEmpty() ? streamText_ : text;
    streamText_.clear();
    emit statsUpdated(stats);

    const ToolCallParser::Parsed parsed = ToolCallParser::parse(raw);
    const bool hasCalls = !parsed.calls.isEmpty() && !cancelled;

    if (!raw.trimmed().isEmpty()) {
        history_.append(ChatMessage::assistant(raw));
    }

    if (cancelled) {
        cancelRequested_ = false;
        emit assistantFinished(assistantId_, raw, raw, false);
        emit notice(QStringLiteral("Generation stopped."));
        finishTurn();
        return;
    }

    QString display = parsed.cleanText;
    if (display.isEmpty() && !hasCalls) {
        display = QStringLiteral("*(empty response)*");
        emit notice(QStringLiteral("The model returned an empty answer. Try again, or switch to a stronger model."));
    }

    emit assistantFinished(assistantId_, display, raw, hasCalls);

    if (!hasCalls) {
        finishTurn();
        return;
    }

    ++step_;
    if (step_ > maxSteps_) {
        emit notice(QStringLiteral("Stopped after %1 tool steps. Ask for a smaller next step.").arg(maxSteps_));
        finishTurn();
        return;
    }

    pendingCalls_ = parsed.calls;
    for (qsizetype i = 0; i < pendingCalls_.size(); ++i) {
        pendingCalls_[i].id = QStringLiteral("%1-tool-%2").arg(assistantId_).arg(i + 1);
    }
    runNextTool();
}

void AgentSession::onEngineFailed(const QString& error)
{
    if (state_ != State::Generating) {
        return;
    }
    streamText_.clear();
    emit assistantFinished(assistantId_, QStringLiteral("**Error:** %1").arg(error), {}, false);
    emit errorOccurred(error);
    finishTurn();
}

void AgentSession::runNextTool()
{
    if (cancelRequested_) {
        emit notice(QStringLiteral("Generation stopped."));
        finishTurn();
        return;
    }

    if (pendingCalls_.isEmpty()) {
        // All tool results are in. Ask the model to continue.
        setState(State::Idle, QStringLiteral("Thinking"));
        startGeneration();
        return;
    }

    if (!tools_) {
        emit errorOccurred(QStringLiteral("Tool runner is not available."));
        finishTurn();
        return;
    }

    const ToolCall call = pendingCalls_.takeFirst();
    emit toolStarted(call.id, call.name, call.args);
    setState(State::RunningTool, call.name);

    tools_->execute(call, [this, call](const ToolResult& result) {
        const QString output = result.ok ? result.output : result.error;
        emit toolFinished(call.id, result.ok, output);

        QString body = sanitizeToolOutput(clip(output.isEmpty() ? QStringLiteral("(no output)") : output, 24000));
        history_.append(ChatMessage::user(
            QStringLiteral("<tool_result tool=\"%1\" ok=\"%2\">\n%3\n</tool_result>")
                .arg(call.name, result.ok ? QStringLiteral("true") : QStringLiteral("false"), body)));

        // Keep the transcript bounded; the engine also trims by tokens.
        while (history_.size() > 80) {
            history_.removeFirst();
        }

        if (cancelRequested_) {
            emit notice(QStringLiteral("Generation stopped."));
            finishTurn();
            return;
        }
        runNextTool();
    });
}

void AgentSession::finishTurn()
{
    pendingCalls_.clear();
    setState(State::Idle, QStringLiteral("Ready"));
    cancelRequested_ = false;
}

QString AgentSession::buildSystemPrompt() const
{
    QString prompt;
    prompt += QString::fromUtf8(
        "You are Lulu Code, a local AI coding agent. You run entirely on the user's machine "
        "through llama.cpp and you work directly inside their project.\n\n"
        "You are not a chat toy: you inspect real files, make real edits, run real tools and "
        "verify results. Be precise, concise and honest about what you changed.\n\n");

    prompt += QStringLiteral("WORKSPACE\n");
    prompt += project_.valid ? project_.summary() : QStringLiteral("No workspace folder is open.");
    prompt += QStringLiteral("\n\n");

    prompt += QStringLiteral("TOOLCHAIN\n%1\n\n").arg(Toolchain::summary(workspace_));

    prompt += QStringLiteral("TOOLS\n%1\n").arg(ToolRunner::promptGuide());
    prompt += protocolHelp();

    prompt += QString::fromUtf8(R"RULES(
RULES
- Inspect before editing: read the relevant files, then change only what is needed.
- Keep edits surgical. Do not rewrite whole files unless creating them or the change is truly global.
- Match the existing code style, naming and formatting of the project.
- Use run_tests (or run_command) after non-trivial changes when a test suite exists.
- Use `git status` / `git diff` to review your work. Never commit, push or reset unless the user asks.
- Never read or modify secrets (.env, keys, credentials), .git internals or files outside the workspace.
- Explain failures instead of hiding them. If a tool fails, adapt your approach.
- In your final answer, name the files you touched and summarise the change in a few short lines.
)RULES");

    if (!extraSystemPrompt_.trimmed().isEmpty()) {
        prompt += QStringLiteral("\nADDITIONAL USER INSTRUCTIONS\n");
        prompt += extraSystemPrompt_.trimmed();
        prompt += QLatin1Char('\n');
    }

    return prompt;
}

} // namespace lulu
