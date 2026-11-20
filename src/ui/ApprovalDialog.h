#pragma once

#include "agent/ToolRunner.h"

#include <QDialog>

class QCheckBox;
class QLabel;
class QPlainTextEdit;

namespace lulu {

/// Modal confirmation for a mutating agent action.
class ApprovalDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ApprovalDialog(const ApprovalRequest& request, QWidget* parent = nullptr);

    bool remember() const;

private:
    QCheckBox* remember_ = nullptr;
};

} // namespace lulu
