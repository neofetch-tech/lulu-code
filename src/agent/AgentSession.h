#pragma once

#include "agent/ProjectScanner.h"
#include "agent/ToolCall.h"
#include "core/ChatMessage.h"

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVector>

namespace lulu {

class LlamaEngine;
class ToolRunner;

/// Drives one conversation: builds prompts, streams tokens, parses tool calls,
/// executes them and loops until the model stops asking for tools.
class AgentSession : public QObject
{
    Q_OBJECT

public:
    enum class State {
        Idle,
        Generating,
        RunningTool,
    };
    Q_ENUM(State)

    explicit AgentSession(QObject* parent = nullptr);

    void attachEngine(LlamaEngine* engine);
    void setToolRunner(ToolRunner* runner);
    void setWorkspace(const QString& root);
    void setModelReady(bool ready);
    void setGenerationParams(const GenerationParams& params) { params_ = params; }
    void setExtraSystemPrompt(const QString& prompt) { extraSystemPrompt_ = prompt; }
    void setMaxSteps(int steps) { maxSteps_ = qBound(2, steps, 40); }

    State state() const { return state_; }
    bool busy() const { return state_ != State::Idle; }
    const ProjectInfo& project() const { return project_; }

    void reset();

public slots:
    void send(const QString& text);
    void cancel();

signals:
    void generationRequested(const QVector<lulu::ChatMessage>& messages,
                             const lulu::GenerationParams& params);
    void cancelGenerationRequested();

    void userMessage(const QString& text);
    void assistantStarted(const QString& id);
    void assistantDelta(const QString& id, const QString& delta);
    void assistantFinished(const QString& id, const QString& displayText,
                           const QString& rawText, bool hadToolCalls);
    void toolStarted(const QString& id, const QString& name, const QJsonObject& args);
    void toolFinished(const QString& id, bool ok, const QString& output);
    void stateChanged(lulu::AgentSession::State state, const QString& detail);
    void statsUpdated(const lulu::TokenStats& stats);
    void contextUsed(int promptTokens, int contextSize);
    void errorOccurred(const QString& message);
    void notice(const QString& message);

private slots:
    void onEngineToken(const QString& text);
    void onEngineFinished(const QString& text, const lulu::TokenStats& stats, bool cancelled);
    void onEngineFailed(const QString& error);

private:
    void startGeneration();
    void runNextTool();
    void finishTurn();
    void setState(State state, const QString& detail);
    QString buildSystemPrompt() const;

    QVector<ChatMessage> history_;
    QVector<ToolCall> pendingCalls_;
    QString streamText_;
    QString assistantId_;
    QString workspace_;
    QString extraSystemPrompt_;
    ProjectInfo project_;

    GenerationParams params_;
    int maxSteps_ = 14;
    int step_ = 0;
    int messageCounter_ = 0;

    bool modelReady_ = false;
    bool cancelRequested_ = false;
    State state_ = State::Idle;

    ToolRunner* tools_ = nullptr;
};

} // namespace lulu
