#include "ui/Composer.h"

#include "ui/Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QTextDocument>
#include <QVBoxLayout>

namespace lulu {

ComposerEdit::ComposerEdit(QWidget* parent)
    : QPlainTextEdit(parent)
{
    setObjectName(QStringLiteral("ComposerEdit"));
    setFrameShape(QFrame::NoFrame);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setProperty("variant", QStringLiteral("composer"));
    setFont(Theme::bodyFont(11));
    setTabChangesFocus(true);
}

void ComposerEdit::keyPressEvent(QKeyEvent* event)
{
    const bool isEnter = event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter;
    if (isEnter && !(event->modifiers() & Qt::ShiftModifier)) {
        emit submitRequested();
        event->accept();
        return;
    }
    QPlainTextEdit::keyPressEvent(event);
}

void ComposerEdit::resizeEvent(QResizeEvent* event)
{
    QPlainTextEdit::resizeEvent(event);
}

Composer::Composer(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("ComposerBar"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(26, 10, 26, 14);
    layout->setSpacing(7);

    auto* frame = new QFrame(this);
    frame->setObjectName(QStringLiteral("ComposerFrame"));
    auto* frameLayout = new QHBoxLayout(frame);
    frameLayout->setContentsMargins(14, 9, 9, 9);
    frameLayout->setSpacing(9);

    edit_ = new ComposerEdit(frame);
    edit_->setPlaceholderText(QStringLiteral("Load a model to start"));
    edit_->setMinimumHeight(38);
    edit_->setMaximumHeight(170);
    frameLayout->addWidget(edit_, 1);

    stop_ = new QPushButton(QStringLiteral("■"), frame);
    stop_->setObjectName(QStringLiteral("StopButton"));
    stop_->setCursor(Qt::PointingHandCursor);
    stop_->setToolTip(QStringLiteral("Stop (Esc)"));
    stop_->setVisible(false);
    frameLayout->addWidget(stop_, 0, Qt::AlignBottom);

    send_ = new QPushButton(QStringLiteral("↑"), frame);
    send_->setObjectName(QStringLiteral("SendButton"));
    send_->setCursor(Qt::PointingHandCursor);
    send_->setToolTip(QStringLiteral("Send (Enter)"));
    send_->setEnabled(false);
    frameLayout->addWidget(send_, 0, Qt::AlignBottom);

    layout->addWidget(frame);

    hint_ = new QLabel(QStringLiteral("Enter to send · Shift+Enter for a new line"), this);
    hint_->setObjectName(QStringLiteral("ComposerHint"));
    layout->addWidget(hint_);

    connect(edit_, &ComposerEdit::submitRequested, this, &Composer::submit);
    connect(send_, &QPushButton::clicked, this, &Composer::submit);
    connect(stop_, &QPushButton::clicked, this, &Composer::stopRequested);
    connect(edit_, &QPlainTextEdit::textChanged, this, [this] {
        updateEditorHeight();
        send_->setEnabled(ready_ && !busy_ && !edit_->toPlainText().trimmed().isEmpty());
    });

    setReady(false);
}

void Composer::updateEditorHeight()
{
    const int documentHeight = static_cast<int>(edit_->document()->size().height());
    const int target = qBound(38, documentHeight + 14, 160);
    if (edit_->height() != target) {
        edit_->setFixedHeight(target);
        updateGeometry();
    }
}

void Composer::submit()
{
    if (busy_ || !ready_) {
        return;
    }
    const QString text = edit_->toPlainText().trimmed();
    if (text.isEmpty()) {
        return;
    }
    edit_->clear();
    updateEditorHeight();
    send_->setEnabled(false);
    emit sendRequested(text);
}

void Composer::setBusy(bool busy)
{
    busy_ = busy;
    stop_->setVisible(busy);
    send_->setEnabled(ready_ && !busy_ && !edit_->toPlainText().trimmed().isEmpty());
    edit_->setReadOnly(busy);
    if (!busy) {
        edit_->setFocus();
    }
    if (busy) {
        hint_->setText(QStringLiteral("Lulu Code is working…"));
    } else if (!ready_) {
        hint_->setText(QStringLiteral("No model loaded"));
    } else {
        hint_->setText(QStringLiteral("Enter to send · Shift+Enter for a new line"));
    }
}

void Composer::setReady(bool ready)
{
    ready_ = ready;
    edit_->setPlaceholderText(ready ? QStringLiteral("Ask Lulu Code to build, fix or explain something…")
                                    : QStringLiteral("Load a model to start"));
    edit_->setEnabled(true);
    send_->setEnabled(ready && !busy_ && !edit_->toPlainText().trimmed().isEmpty());
    if (!busy_) {
        hint_->setText(ready ? QStringLiteral("Enter to send · Shift+Enter for a new line")
                             : QStringLiteral("No model loaded"));
    }
}

void Composer::focusInput()
{
    edit_->setFocus();
}

} // namespace lulu
