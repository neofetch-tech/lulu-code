#include "ui/MessageBubble.h"

#include "ui/Theme.h"

#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>

namespace lulu {

MessageBubble::MessageBubble(Role role, const QString& text, QWidget* parent)
    : QFrame(parent)
    , role_(role)
    , text_(text)
{
    setObjectName(role == Role::User ? QStringLiteral("UserBubble") : QStringLiteral("AssistantBubble"));
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    if (role == Role::User) {
        setMaximumWidth(760);
    }

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(role == Role::User ? 16 : 0, role == Role::User ? 12 : 0,
                               role == Role::User ? 16 : 0, role == Role::User ? 12 : 0);
    layout->setSpacing(5);

    auto* header = new QHBoxLayout;
    header->setContentsMargins(role == Role::User ? 0 : 2, 0, role == Role::User ? 0 : 2, 0);
    header->setSpacing(8);

    auto* roleLabel = new QLabel(role == Role::User ? QStringLiteral("YOU") : QStringLiteral("LULU CODE"), this);
    roleLabel->setObjectName(QStringLiteral("BubbleRole"));
    header->addWidget(roleLabel);

    if (role == Role::Assistant) {
        auto* logo = new QLabel(this);
        QPixmap pixmap(QStringLiteral(":/icons/logo.png"));
        logo->setPixmap(pixmap.scaled(14, 14, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        header->insertWidget(0, logo);
    }

    header->addStretch();

    copy_ = new QPushButton(QStringLiteral("Copy"), this);
    copy_->setProperty("variant", QStringLiteral("ghost"));
    copy_->setCursor(Qt::PointingHandCursor);
    copy_->setToolTip(QStringLiteral("Copy message"));
    copy_->setFixedHeight(22);
    copy_->setStyleSheet(QStringLiteral("font-size: 10px; padding: 2px 7px; color: #6f6f79;"));
    header->addWidget(copy_);
    layout->addLayout(header);

    body_ = new QTextBrowser(this);
    body_->setObjectName(QStringLiteral("BubbleBody"));
    body_->setFrameShape(QFrame::NoFrame);
    body_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    body_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    body_->setOpenExternalLinks(true);
    body_->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
    body_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    body_->document()->setDocumentMargin(0);
    body_->setContentsMargins(0, 0, 0, 0);
    body_->viewport()->setAutoFillBackground(false);
    body_->setContextMenuPolicy(Qt::DefaultContextMenu);
    if (role == Role::User) {
        body_->setStyleSheet(QStringLiteral("background: transparent; border: none; color: #f3f3f7;"));
    }
    layout->addWidget(body_);

    thinking_ = new QLabel(QStringLiteral("Thinking"), this);
    thinking_->setObjectName(QStringLiteral("BubbleRole"));
    thinking_->setVisible(text_.isEmpty());
    layout->addWidget(thinking_);

    renderTimer_ = new QTimer(this);
    renderTimer_->setSingleShot(true);
    renderTimer_->setInterval(30);
    connect(renderTimer_, &QTimer::timeout, this, &MessageBubble::render);

    thinkingTimer_ = new QTimer(this);
    thinkingTimer_->setInterval(420);
    connect(thinkingTimer_, &QTimer::timeout, this, &MessageBubble::updateThinkingLabel);
    if (text_.isEmpty()) {
        thinkingTimer_->start();
    }

    connect(copy_, &QPushButton::clicked, this, [this] {
        QApplication::clipboard()->setText(text_);
        copy_->setText(QStringLiteral("Copied"));
        QTimer::singleShot(1200, copy_, [this] { copy_->setText(QStringLiteral("Copy")); });
    });

    body_->document()->setDefaultStyleSheet(Theme::documentStyleSheet());
    render();
}

void MessageBubble::setText(const QString& text)
{
    text_ = text;
    streaming_ = false;
    renderTimer_->stop();
    render();
}

void MessageBubble::appendDelta(const QString& delta)
{
    text_ += delta;
    streaming_ = true;
    if (thinkingTimer_->isActive()) {
        thinkingTimer_->stop();
        thinking_->setVisible(false);
    }
    if (!renderTimer_->isActive()) {
        renderTimer_->start();
    }
}

void MessageBubble::setThinking(bool thinking)
{
    if (thinking && text_.isEmpty()) {
        thinking_->setVisible(true);
        thinkingTimer_->start();
    } else if (!thinking) {
        thinkingTimer_->stop();
        thinking_->setVisible(false);
    }
}

void MessageBubble::updateThinkingLabel()
{
    thinkingStep_ = (thinkingStep_ + 1) % 3;
    thinking_->setText(QStringLiteral("Thinking") + QString(thinkingStep_ + 1, QLatin1Char('.')));
}

void MessageBubble::render()
{
    if (thinkingTimer_->isActive() && !text_.isEmpty()) {
        thinkingTimer_->stop();
        thinking_->setVisible(false);
    }

    if (text_.isEmpty() && streaming_) {
        body_->clear();
        updateHeight();
        return;
    }

    if (role_ == Role::User) {
        // Preserve the user's line breaks instead of letting markdown eat them.
        QString escaped = text_.toHtmlEscaped();
        escaped.replace(QStringLiteral("\n"), QStringLiteral("<br>"));
        body_->setHtml(QStringLiteral("<div style=\"color:#f3f3f7;\">%1</div>").arg(escaped));
    } else {
        body_->setMarkdown(text_.isEmpty() ? QStringLiteral(" ") : text_);
    }

    updateHeight();
    emit contentChanged();
}

void MessageBubble::updateHeight()
{
    if (updatingHeight_) {
        return;
    }
    updatingHeight_ = true;

    QTextDocument* doc = body_->document();
    int width = body_->viewport()->width();
    if (width <= 0) {
        width = width > 0 ? width : 600;
    }
    doc->setTextWidth(width);
    const int height = static_cast<int>(std::ceil(doc->size().height())) + 2;
    body_->setFixedHeight(std::max(height, 18));

    lastWidth_ = width;
    updatingHeight_ = false;
}

void MessageBubble::resizeEvent(QResizeEvent* event)
{
    QFrame::resizeEvent(event);
    if (body_->viewport()->width() != lastWidth_) {
        updateHeight();
    }
}

void MessageBubble::showEvent(QShowEvent* event)
{
    QFrame::showEvent(event);
    QTimer::singleShot(0, this, &MessageBubble::updateHeight);
}

} // namespace lulu
