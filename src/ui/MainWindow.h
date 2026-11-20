#pragma once

#include "agent/ToolRunner.h"
#include "ui/SettingsDialog.h"

#include <QMainWindow>
#include <QThread>

class QAction;
class QLabel;
class QSplitter;

namespace lulu {

class AgentSession;
class ChatPanel;
class LlamaEngine;
class ModelStore;
class Sidebar;
class ToolRunner;
class WorkspacePanel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void buildUi();
    void buildMenus();
    void buildStatusBar();
    void wireSignals();
    void restoreState();
    void persistState();
    void readSettings();
    void saveSettings();
    void applySettings(const AppSettings& settings, bool persist);
    void applyGenerationSettings();

    void openWorkspace(const QString& path);
    void chooseWorkspace();
    void openModelsDialog();
    void openSettingsDialog();
    void loadModelById(const QString& id);
    void reloadModel();
    void unloadModel();
    void newChat();
    void updateToolchain();
    void refreshModelUi();
    bool askApproval(const ApprovalRequest& request);

    QThread engineThread_;
    LlamaEngine* engine_ = nullptr;
    ModelStore* store_ = nullptr;
    ToolRunner* tools_ = nullptr;
    AgentSession* agent_ = nullptr;

    Sidebar* sidebar_ = nullptr;
    ChatPanel* chat_ = nullptr;
    WorkspacePanel* workspacePanel_ = nullptr;
    QSplitter* splitter_ = nullptr;
    QAction* toggleExplorer_ = nullptr;

    QLabel* statusState_ = nullptr;
    QLabel* statusContext_ = nullptr;
    QLabel* statusSpeed_ = nullptr;
    QLabel* statusBackend_ = nullptr;

    AppSettings settings_;
    QString workspacePath_;
    QString loadedModelId_;
    QString pendingModelId_;
    bool modelReady_ = false;
};

} // namespace lulu
