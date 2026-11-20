#pragma once

#include <QWidget>

class QLabel;
class QPushButton;

namespace lulu {

/// The empty state shown on a fresh conversation.
class WelcomeView : public QWidget
{
    Q_OBJECT

public:
    explicit WelcomeView(QWidget* parent = nullptr);

    void setModelReady(bool ready, const QString& modelName);
    void setWorkspaceName(const QString& name);
    void setBusy(bool busy);

signals:
    void promptChosen(const QString& prompt);
    void chooseModelRequested();
    void openFolderRequested();

private:
    QLabel* modelChip_ = nullptr;
    QLabel* folderChip_ = nullptr;
    QPushButton* modelButton_ = nullptr;
    QPushButton* folderButton_ = nullptr;
    QWidget* examples_ = nullptr;
};

} // namespace lulu
