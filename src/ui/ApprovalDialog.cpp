#include "ui/ApprovalDialog.h"

#include "ui/Theme.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace lulu {

namespace {

QString titleFor(ApprovalKind kind)
{
    switch (kind) {
    case ApprovalKind::WriteFile:
        return QStringLiteral("Allow this file write?");
    case ApprovalKind::EditFile:
        return QStringLiteral("Allow this edit?");
    case ApprovalKind::DeleteFile:
        return QStringLiteral("Delete this file?");
    case ApprovalKind::Command:
        return QStringLiteral("Run this command?");
    }
    return QStringLiteral("Allow this action?");
}

QString checkboxTextFor(ApprovalKind kind)
{
    switch (kind) {
    case ApprovalKind::WriteFile:
    case ApprovalKind::EditFile:
    case ApprovalKind::DeleteFile:
        return QStringLiteral("Allow all file changes for the rest of this session");
    case ApprovalKind::Command:
        return QStringLiteral("Allow all commands for the rest of this session");
    }
    return QStringLiteral("Remember this choice for the session");
}

} // namespace

ApprovalDialog::ApprovalDialog(const ApprovalRequest& request, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Lulu Code · approval"));
    setModal(true);
    setMinimumWidth(600);
    resize(640, 460);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 22, 24, 18);
    layout->setSpacing(12);

    auto* title = new QLabel(titleFor(request.kind), this);
    title->setObjectName(QStringLiteral("DialogTitle"));
    layout->addWidget(title);

    auto* subtitle = new QLabel(request.title, this);
    subtitle->setObjectName(QStringLiteral("DialogSubtitle"));
    subtitle->setWordWrap(true);
    layout->addWidget(subtitle);

    auto* tool = new QLabel(request.tool.toUpper(), this);
    tool->setObjectName(QStringLiteral("Chip"));
    tool->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    layout->addWidget(tool);

    auto* detail = new QPlainTextEdit(this);
    detail->setReadOnly(true);
    detail->setPlainText(request.detail);
    detail->setFont(Theme::monoFont(9));
    layout->addWidget(detail, 1);

    remember_ = new QCheckBox(checkboxTextFor(request.kind), this);
    layout->addWidget(remember_);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch();

    auto* reject = new QPushButton(QStringLiteral("Reject"), this);
    reject->setCursor(Qt::PointingHandCursor);
    buttons->addWidget(reject);

    auto* accept = new QPushButton(request.kind == ApprovalKind::DeleteFile ? QStringLiteral("Delete")
                                                                          : QStringLiteral("Allow"),
                                   this);
    accept->setProperty("variant", request.kind == ApprovalKind::DeleteFile
                                       ? QStringLiteral("danger")
                                       : QStringLiteral("primary"));
    accept->setCursor(Qt::PointingHandCursor);
    accept->setDefault(true);
    buttons->addWidget(accept);
    layout->addLayout(buttons);

    connect(reject, &QPushButton::clicked, this, &QDialog::reject);
    connect(accept, &QPushButton::clicked, this, &QDialog::accept);
}

bool ApprovalDialog::remember() const
{
    return remember_->isChecked();
}

} // namespace lulu
