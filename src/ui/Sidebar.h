#pragma once

#include "agent/Toolchain.h"

#include <QWidget>

class QLabel;
class QProgressBar;
class QPushButton;

namespace lulu {

class Sidebar : public QWidget
{
    Q_OBJECT

public:
    explicit Sidebar(QWidget* parent = nullptr);

    void setModelInfo(bool loaded, const QString& name, const QString& description, int contextSize);
    void setModelLoading(const QString& name);
    void setWorkspace(const QString& path);
    void setDownloadProgress(const QString& name, qint64 received, qint64 total);
    void clearDownload();
    void setToolchain(const QVector<ToolchainEntry>& entries);

signals:
    void newChatRequested();
    void manageModelsRequested();
    void openFolderRequested();
    void settingsRequested();
    void reloadModelRequested();
    void ejectModelRequested();
    void githubRequested();

private:
    void updateStatus(bool loaded);

    QLabel* modelTitle_ = nullptr;
    QLabel* modelDescription_ = nullptr;
    QLabel* modelMeta_ = nullptr;
    QLabel* modelStatus_ = nullptr;
    QWidget* statusDot_ = nullptr;
    QPushButton* chooseModel_ = nullptr;
    QPushButton* changeModel_ = nullptr;
    QPushButton* reloadModel_ = nullptr;
    QPushButton* ejectModel_ = nullptr;

    QWidget* downloadBox_ = nullptr;
    QLabel* downloadLabel_ = nullptr;
    QProgressBar* downloadBar_ = nullptr;

    QLabel* workspaceName_ = nullptr;
    QLabel* workspacePath_ = nullptr;

    QVector<QLabel*> toolchainDots_;
    QVector<QLabel*> toolchainNames_;
};

} // namespace lulu
