#pragma once

#include <QHash>
#include <QJsonObject>
#include <QWidget>

class QLabel;
class QScrollArea;
class QVBoxLayout;
class QFrame;

namespace lulu {

class MessageBubble;
class ToolCard;
class WelcomeView;
class Composer;

class ChatPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ChatPanel(QWidget* parent = nullptr);

    void setProjectName(const QString& name, const QString& fullPath);
    void setModelName(const QString& name);
    void setBusy(bool busy);
    void setStatusText(const QString& text);
    void setReady(bool ready);
    void clearConversation();

    Composer* composer() const { return composer_; }

signals:
    void sendRequested(const QString& text);
    void stopRequested();
    void newChatRequested();
    void chooseModelRequested();
    void openFolderRequested();
    void toggleWorkspaceRequested();

public slots:
    void addUserMessage(const QString& text);
    void beginAssistant(const QString& id);
    void appendAssistant(const QString& id, const QString& delta);
    void finishAssistant(const QString& id, const QString& displayText,
                         const QString& rawText, bool hadToolCalls);
    void addToolCard(const QString& id, const QString& name, const QJsonObject& args);
    void finishToolCard(const QString& id, bool ok, const QString& output);
    void addNotice(const QString& text, bool error = false);

private:
    void appendWidget(QWidget* widget);
    void insertBeforeStretch(QWidget* widget);
    void removeWidget(QWidget* widget);
    void scrollToBottom(bool force = false);

    QLabel* projectChip_ = nullptr;
    QLabel* modelChip_ = nullptr;
    QLabel* statusChip_ = nullptr;
    QScrollArea* scroll_ = nullptr;
    QWidget* canvas_ = nullptr;
    QVBoxLayout* canvasLayout_ = nullptr;
    WelcomeView* welcome_ = nullptr;
    Composer* composer_ = nullptr;
    QFrame* header_ = nullptr;

    QHash<QString, MessageBubble*> bubbles_;
    QHash<QString, ToolCard*> toolCards_;
    bool stickToBottom_ = true;
};

} // namespace lulu
