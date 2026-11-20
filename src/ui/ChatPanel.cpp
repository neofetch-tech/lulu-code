#include "ui/ChatPanel.h"

#include "ui/Composer.h"
#include "ui/MessageBubble.h"
#include "ui/ToolCard.h"
#include "ui/WelcomeView.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>

namespace lulu {

ChatPanel::ChatPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("ChatPanel"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    header_ = new QFrame(this);
    header_->setObjectName(QStringLiteral("ChatHeader"));
    header_->setFixedHeight(52);
    auto* headerLayout = new QHBoxLayout(header_);
    headerLayout->setContentsMargins(20, 8, 14, 8);
    headerLayout->setSpacing(8);

    projectChip_ = new QLabel(QStringLiteral("◈ NO FOLDER"), header_);
    projectChip_->setObjectName(QStringLiteral("Chip"));
    projectChip_->setMaximumWidth(300);
    headerLayout->addWidget(projectChip_);

    modelChip_ = new QLabel(QStringLiteral("NO MODEL"), header_);
    modelChip_->setObjectName(QStringLiteral("Chip"));
    modelChip_->setMaximumWidth(280);
    headerLayout->addWidget(modelChip_);

    headerLayout->addStretch();

    statusChip_ = new QLabel(QStringLiteral("READY"), header_);
    statusChip_->setObjectName(QStringLiteral("Chip"));
    headerLayout->addWidget(statusChip_);

    auto* newChat = new QPushButton(QStringLiteral("＋"), header_);
    newChat->setProperty("variant", QStringLiteral("icon"));
    newChat->setToolTip(QStringLiteral("New chat (Ctrl+N)"));
    newChat->setCursor(Qt::PointingHandCursor);
    headerLayout->addWidget(newChat);

    auto* panelButton = new QPushButton(QStringLiteral("▤"), header_);
    panelButton->setProperty("variant", QStringLiteral("icon"));
    panelButton->setToolTip(QStringLiteral("Toggle workspace panel (Ctrl+B)"));
    panelButton->setCursor(Qt::PointingHandCursor);
    headerLayout->addWidget(panelButton);

    layout->addWidget(header_);

    scroll_ = new QScrollArea(this);
    scroll_->setObjectName(QStringLiteral("ChatScroll"));
    scroll_->setWidgetResizable(true);
    scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    canvas_ = new QWidget(scroll_);
    canvas_->setObjectName(QStringLiteral("ChatCanvas"));
    canvasLayout_ = new QVBoxLayout(canvas_);
    canvasLayout_->setContentsMargins(30, 22, 30, 26);
    canvasLayout_->setSpacing(16);

    welcome_ = new WelcomeView(canvas_);
    canvasLayout_->addWidget(welcome_);
    canvasLayout_->addStretch(1);

    scroll_->setWidget(canvas_);
    layout->addWidget(scroll_, 1);

    composer_ = new Composer(this);
    layout->addWidget(composer_);

    connect(composer_, &Composer::sendRequested, this, &ChatPanel::sendRequested);
    connect(composer_, &Composer::stopRequested, this, &ChatPanel::stopRequested);
    connect(newChat, &QPushButton::clicked, this, &ChatPanel::newChatRequested);
    connect(panelButton, &QPushButton::clicked, this, &ChatPanel::toggleWorkspaceRequested);
    connect(welcome_, &WelcomeView::promptChosen, this, [this](const QString& prompt) {
        composer_->setReady(true);
        composer_->focusInput();
        emit sendRequested(prompt);
    });
    connect(welcome_, &WelcomeView::chooseModelRequested, this, &ChatPanel::chooseModelRequested);
    connect(welcome_, &WelcomeView::openFolderRequested, this, &ChatPanel::openFolderRequested);

    connect(scroll_->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int value) {
        const QScrollBar* bar = scroll_->verticalScrollBar();
        stickToBottom_ = value >= bar->maximum() - 60;
    });
    connect(scroll_->verticalScrollBar(), &QScrollBar::rangeChanged, this, [this](int, int max) {
        if (stickToBottom_) {
            scroll_->verticalScrollBar()->setValue(max);
        }
    });
}

void ChatPanel::setProjectName(const QString& name, const QString& fullPath)
{
    if (name.isEmpty()) {
        projectChip_->setText(QStringLiteral("◈ NO FOLDER"));
        projectChip_->setToolTip(QString());
    } else {
        projectChip_->setText(QStringLiteral("◈ %1").arg(name));
        projectChip_->setToolTip(fullPath);
    }
    welcome_->setWorkspaceName(name);
}

void ChatPanel::setModelName(const QString& name)
{
    if (name.isEmpty()) {
        modelChip_->setText(QStringLiteral("NO MODEL"));
        modelChip_->setToolTip(QString());
        welcome_->setModelReady(false, {});
    } else {
        modelChip_->setText(name.toUpper());
        modelChip_->setToolTip(name);
        welcome_->setModelReady(true, name);
    }
}

void ChatPanel::setBusy(bool busy)
{
    composer_->setBusy(busy);
    welcome_->setBusy(busy);
    setStatusText(busy ? QStringLiteral("WORKING") : QStringLiteral("READY"));
}

void ChatPanel::setStatusText(const QString& text)
{
    statusChip_->setText(text);
}

void ChatPanel::setReady(bool ready)
{
    composer_->setReady(ready);
}

void ChatPanel::appendWidget(QWidget* widget)
{
    insertBeforeStretch(widget);
    welcome_->hide();
}

void ChatPanel::insertBeforeStretch(QWidget* widget)
{
    const int stretchIndex = canvasLayout_->count() - 1;
    canvasLayout_->insertWidget(stretchIndex, widget);
}

void ChatPanel::removeWidget(QWidget* widget)
{
    if (!widget) {
        return;
    }
    canvasLayout_->removeWidget(widget);
    widget->deleteLater();
}

void ChatPanel::scrollToBottom(bool force)
{
    if (!force && !stickToBottom_) {
        return;
    }
    stickToBottom_ = true;
    QTimer::singleShot(0, this, [this] {
        auto* bar = scroll_->verticalScrollBar();
        bar->setValue(bar->maximum());
    });
}

void ChatPanel::addUserMessage(const QString& text)
{
    auto* row = new QWidget(canvas_);
    row->setStyleSheet(QStringLiteral("background: transparent;"));
    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->addStretch(1);
    auto* bubble = new MessageBubble(Role::User, text, row);
    rowLayout->addWidget(bubble);
    appendWidget(row);
    scrollToBottom();
}

void ChatPanel::beginAssistant(const QString& id)
{
    auto* bubble = new MessageBubble(Role::Assistant, {}, canvas_);
    bubble->setThinking(true);
    appendWidget(bubble);
    bubbles_.insert(id, bubble);
    connect(bubble, &MessageBubble::contentChanged, this, [this] { scrollToBottom(); });
    scrollToBottom();
}

void ChatPanel::appendAssistant(const QString& id, const QString& delta)
{
    MessageBubble* bubble = bubbles_.value(id, nullptr);
    if (!bubble) {
        beginAssistant(id);
        bubble = bubbles_.value(id, nullptr);
    }
    if (bubble) {
        bubble->appendDelta(delta);
    }
}

void ChatPanel::finishAssistant(const QString& id, const QString& displayText,
                                const QString& rawText, bool hadToolCalls)
{
    MessageBubble* bubble = bubbles_.take(id);
    if (!bubble) {
        return;
    }

    if (displayText.trimmed().isEmpty()) {
        if (hadToolCalls) {
            removeWidget(bubble);
            return;
        }
        bubble->setText(QStringLiteral("*(no output)*"));
        scrollToBottom();
        return;
    }

    Q_UNUSED(rawText);
    bubble->setThinking(false);
    bubble->setText(displayText);
    scrollToBottom();
}

void ChatPanel::addToolCard(const QString& id, const QString& name, const QJsonObject& args)
{
    auto* card = new ToolCard(id, name, args, canvas_);
    appendWidget(card);
    toolCards_.insert(id, card);
    scrollToBottom();
}

void ChatPanel::finishToolCard(const QString& id, bool ok, const QString& output)
{
    if (ToolCard* card = toolCards_.value(id, nullptr)) {
        card->setResult(ok, output);
        scrollToBottom();
    }
}

void ChatPanel::addNotice(const QString& text, bool error)
{
    auto* label = new QLabel(text, canvas_);
    label->setObjectName(error ? QStringLiteral("ErrorNotice") : QStringLiteral("NoticeCard"));
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    appendWidget(label);
    scrollToBottom();
}

void ChatPanel::clearConversation()
{
    for (MessageBubble* bubble : std::as_const(bubbles_)) {
        bubble->deleteLater();
    }
    bubbles_.clear();
    for (ToolCard* card : std::as_const(toolCards_)) {
        card->deleteLater();
    }
    toolCards_.clear();

    while (canvasLayout_->count() > 2) {
        QLayoutItem* item = canvasLayout_->takeAt(1);
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    welcome_->show();
    setStatusText(QStringLiteral("READY"));
    stickToBottom_ = true;
    scrollToBottom(true);
}

} // namespace lulu
