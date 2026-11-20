#include "ui/ToolCard.h"

#include "ui/Theme.h"

#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonValue>
#include <QLabel>
#include <QPixmap>
#include <QStyle>
#include <QTextBrowser>
#include <QToolButton>
#include <QVBoxLayout>

namespace lulu {

ToolCard::ToolCard(const QString& id, const QString& name, const QJsonObject& args, QWidget* parent)
    : QFrame(parent)
    , id_(id)
{
    setObjectName(QStringLiteral("ToolCard"));
    setProperty("state", QStringLiteral("running"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 9, 12, 9);
    layout->setSpacing(7);

    auto* header = new QHBoxLayout;
    header->setSpacing(8);

    toggle_ = new QToolButton(this);
    toggle_->setText(QStringLiteral("▸"));
    toggle_->setCursor(Qt::PointingHandCursor);
    toggle_->setAutoRaise(true);
    header->addWidget(toggle_);

    auto* icon = new QLabel(this);
    icon->setText(QStringLiteral("⚙"));
    icon->setStyleSheet(QStringLiteral("color: #8d8d97; font-size: 13px;"));
    header->addWidget(icon);

    name_ = new QLabel(name, this);
    name_->setObjectName(QStringLiteral("ToolName"));
    header->addWidget(name_);

    meta_ = new QLabel(summarize(name, args), this);
    meta_->setObjectName(QStringLiteral("ToolMeta"));
    meta_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    header->addWidget(meta_, 1);

    status_ = new QLabel(QStringLiteral("running"), this);
    status_->setObjectName(QStringLiteral("ToolStatus"));
    status_->setProperty("state", QStringLiteral("running"));
    header->addWidget(status_);

    layout->addLayout(header);

    output_ = new QTextBrowser(this);
    output_->setFrameShape(QFrame::NoFrame);
    output_->setMaximumHeight(260);
    output_->setFont(Theme::monoFont(9));
    output_->setStyleSheet(QStringLiteral("background: #0a0a0c; border: 1px solid #1e1e24; border-radius: 7px; padding: 6px;"));
    output_->setVisible(false);
    layout->addWidget(output_);

    connect(toggle_, &QToolButton::clicked, this, [this] { setExpanded(!expanded_); });
}

QString ToolCard::summarize(const QString& name, const QJsonObject& args)
{
    auto text = [](const QJsonValue& value) { return value.toString().trimmed(); };
    QString summary;

    if (name == QStringLiteral("run_command")) {
        summary = QStringLiteral("$ ") + text(args.value(QStringLiteral("command")));
    } else if (name == QStringLiteral("git")) {
        summary = QStringLiteral("git ") + text(args.value(QStringLiteral("action")));
        const QJsonArray array = args.value(QStringLiteral("args")).toArray();
        for (const QJsonValue& value : array) {
            summary += QLatin1Char(' ') + value.toString();
        }
    } else if (name == QStringLiteral("search_files")) {
        summary = QStringLiteral("\"") + text(args.value(QStringLiteral("query"))) + QStringLiteral("\"");
    } else if (name == QStringLiteral("run_tests")) {
        summary = text(args.value(QStringLiteral("framework")));
        if (summary.isEmpty()) {
            summary = QStringLiteral("auto");
        }
    } else if (name == QStringLiteral("detect_toolchain")) {
        summary.clear();
    } else if (args.contains(QStringLiteral("path"))) {
        summary = text(args.value(QStringLiteral("path")));
    } else if (args.contains(QStringLiteral("command"))) {
        summary = text(args.value(QStringLiteral("command")));
    }

    summary.replace(QLatin1Char('\n'), QLatin1Char(' '));
    if (summary.size() > 110) {
        summary = summary.left(110) + QStringLiteral("…");
    }
    return summary;
}

void ToolCard::setResult(bool ok, const QString& output)
{
    applyState(ok ? QStringLiteral("ok") : QStringLiteral("failed"));
    status_->setProperty("state", ok ? QStringLiteral("ok") : QStringLiteral("failed"));
    status_->setText(ok ? QStringLiteral("done") : QStringLiteral("failed"));
    status_->style()->unpolish(status_);
    status_->style()->polish(status_);

    output_->setPlainText(output.isEmpty() ? QStringLiteral("(no output)") : output);
    toggle_->setEnabled(true);
    if (!ok) {
        setExpanded(true);
    } else if (expanded_) {
        setExpanded(true);
    }
}

void ToolCard::applyState(const QString& state)
{
    setProperty("state", state);
    style()->unpolish(this);
    style()->polish(this);
}

void ToolCard::setExpanded(bool expanded)
{
    expanded_ = expanded;
    output_->setVisible(expanded);
    toggle_->setText(expanded ? QStringLiteral("▾") : QStringLiteral("▸"));
}

} // namespace lulu
