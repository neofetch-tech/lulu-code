#pragma once

#include "core/ChatMessage.h"

#include <QFrame>
#include <QString>

class QLabel;
class QPushButton;
class QTextBrowser;
class QTimer;

namespace lulu {

/// One chat message. Markdown is rendered with QTextDocument, the height
/// follows the content, and streaming deltas are coalesced into ~30 fps.
class MessageBubble : public QFrame
{
    Q_OBJECT

public:
    explicit MessageBubble(Role role, const QString& text = {}, QWidget* parent = nullptr);

    void setText(const QString& text);
    void appendDelta(const QString& delta);
    void setThinking(bool thinking);

    QString rawText() const { return text_; }
    Role role() const { return role_; }

signals:
    void contentChanged();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void render();
    void updateHeight();
    void updateThinkingLabel();

    Role role_;
    QString text_;
    bool streaming_ = false;

    QTextBrowser* body_ = nullptr;
    QLabel* thinking_ = nullptr;
    QPushButton* copy_ = nullptr;
    QTimer* renderTimer_ = nullptr;
    QTimer* thinkingTimer_ = nullptr;
    int thinkingStep_ = 0;
    int lastWidth_ = 0;
    bool updatingHeight_ = false;
};

} // namespace lulu
