#include "ui/MainWindow.h"

#include "agent/AgentSession.h"
#include "agent/Toolchain.h"
#include "app/AppPaths.h"
#include "core/LlamaEngine.h"
#include "core/ModelCatalog.h"
#include "core/ModelStore.h"
#include "ui/ApprovalDialog.h"
#include "ui/ChatPanel.h"
#include "ui/Composer.h"
#include "ui/ModelsDialog.h"
#include "ui/Sidebar.h"
#include "ui/WorkspacePanel.h"

#include <QAction>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QList>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPixmap>
#include <QSettings>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <QUrl>
#include <QtGlobal>

namespace lulu {

namespace {

const QString& githubUrl()
{
    static const QString url = QStringLiteral("https://github.com/neofetch-tech/lulu-code");
    return url;
}

QString modelName(const QString& id, ModelStore* store)
{
    if (const auto info = ModelCatalog::find(id)) {
        return info->name;
    }
    if (const auto local = store->find(id)) {
        return local->name;
    }
    return id;
}

QString modelDescription(const QString& id)
{
    if (const auto info = ModelCatalog::find(id)) {
        return info->description;
    }
    return QStringLiteral("A local GGUF model driven by llama.cpp.");
}

QLabel* addStatusItem(QStatusBar* bar, const QString& key)
{
    auto* keyLabel = new QLabel(key, bar);
    keyLabel->setObjectName(QStringLiteral("StatusKey"));
    keyLabel->setContentsMargins(12, 0, 0, 0);
    bar->addPermanentWidget(keyLabel);

    auto* value = new QLabel(QStringLiteral("—"), bar);
    value->setObjectName(QStringLiteral("StatusValue"));
    value->setContentsMargins(6, 0, 10, 0);
    bar->addPermanentWidget(value);
    return value;
}

QString shortCount(int value)
{
    if (value >= 1000) {
        return QStringLiteral("%1k").arg(QString::number(value / 1000.0, 'f', 1));
    }
    return QString::number(value);
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Lulu Code"));
    setWindowIcon(QIcon(QStringLiteral(":/icons/logo.png")));
    setMinimumSize(1080, 680);
    resize(1400, 900);

    readSettings();

    engine_ = new LlamaEngine;
    engine_->moveToThread(&engineThread_);
    connect(&engineThread_, &QThread::finished, engine_, &QObject::deleteLater);

    store_ = new ModelStore(this);
    tools_ = new ToolRunner(this);
    agent_ = new AgentSession(this);
    agent_->attachEngine(engine_);
    agent_->setToolRunner(tools_);
    tools_->setApprovalHandler([this](const ApprovalRequest& request) {
        return askApproval(request);
    });

    buildUi();
    buildMenus();
    buildStatusBar();
    wireSignals();

    applySettings(settings_, false);

    engineThread_.start();
    restoreState();

    updateToolchain();

    const QString lastModel = QSettings().value(QStringLiteral("model/last")).toString();
    if (!lastModel.isEmpty() && store_->isInstalled(lastModel)) {
        QTimer::singleShot(200, this, [this, lastModel] { loadModelById(lastModel); });
    }

    chat_->composer()->focusInput();
}

MainWindow::~MainWindow()
{
    persistState();

    if (agent_) {
        agent_->setModelReady(false);
    }
    if (engine_) {
        engine_->cancel();
    }

    engineThread_.quit();
    if (!engineThread_.wait(5000)) {
        engineThread_.terminate();
        engineThread_.wait(1000);
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    persistState();
    QMainWindow::closeEvent(event);
}

void MainWindow::buildUi()
{
    auto* root = new QWidget(this);
    root->setObjectName(QStringLiteral("Root"));

    auto* layout = new QHBoxLayout(root);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    sidebar_ = new Sidebar(root);
    layout->addWidget(sidebar_);

    splitter_ = new QSplitter(Qt::Horizontal, root);
    splitter_->setObjectName(QStringLiteral("MainSplitter"));
    splitter_->setChildrenCollapsible(false);
    splitter_->setHandleWidth(1);

    chat_ = new ChatPanel(splitter_);
    chat_->setMinimumWidth(560);
    workspacePanel_ = new WorkspacePanel(splitter_);

    splitter_->addWidget(chat_);
    splitter_->addWidget(workspacePanel_);
    splitter_->setStretchFactor(0, 1);
    splitter_->setStretchFactor(1, 0);
    splitter_->setSizes(QList<int>{980, 300});

    layout->addWidget(splitter_, 1);
    setCentralWidget(root);
}

void MainWindow::buildMenus()
{
    auto* fileMenu = menuBar()->addMenu(QStringLiteral("&File"));

    auto* newChatAction = fileMenu->addAction(QStringLiteral("New chat"));
    newChatAction->setShortcut(QKeySequence::New);
    connect(newChatAction, &QAction::triggered, this, &MainWindow::newChat);

    auto* openFolderAction = fileMenu->addAction(QStringLiteral("Open folder…"));
    openFolderAction->setShortcut(QKeySequence::Open);
    connect(openFolderAction, &QAction::triggered, this, &MainWindow::chooseWorkspace);

    auto* modelsAction = fileMenu->addAction(QStringLiteral("Manage models…"));
    modelsAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+M")));
    connect(modelsAction, &QAction::triggered, this, &MainWindow::openModelsDialog);

    auto* settingsAction = fileMenu->addAction(QStringLiteral("Settings…"));
    settingsAction->setShortcut(QKeySequence::Preferences);
    connect(settingsAction, &QAction::triggered, this, &MainWindow::openSettingsDialog);

    fileMenu->addSeparator();

    auto* quitAction = fileMenu->addAction(QStringLiteral("Quit"));
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    auto* viewMenu = menuBar()->addMenu(QStringLiteral("&View"));

    toggleExplorer_ = viewMenu->addAction(QStringLiteral("Explorer panel"));
    toggleExplorer_->setCheckable(true);
    toggleExplorer_->setChecked(true);
    toggleExplorer_->setShortcut(QKeySequence(QStringLiteral("Ctrl+B")));
    connect(toggleExplorer_, &QAction::toggled, this, [this](bool visible) {
        workspacePanel_->setVisible(visible);
    });

    viewMenu->addSeparator();

    auto* reloadAction = viewMenu->addAction(QStringLiteral("Reload model"));
    reloadAction->setShortcut(QKeySequence(QStringLiteral("F5")));
    connect(reloadAction, &QAction::triggered, this, &MainWindow::reloadModel);

    auto* unloadAction = viewMenu->addAction(QStringLiteral("Unload model"));
    connect(unloadAction, &QAction::triggered, this, &MainWindow::unloadModel);

    auto* helpMenu = menuBar()->addMenu(QStringLiteral("&Help"));

    auto* githubAction = helpMenu->addAction(QStringLiteral("View on GitHub"));
    connect(githubAction, &QAction::triggered, this, [this] {
        QDesktopServices::openUrl(QUrl(githubUrl()));
    });

    auto* dataAction = helpMenu->addAction(QStringLiteral("Open data folder"));
    connect(dataAction, &QAction::triggered, this, [] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(AppPaths::dataDir()));
    });

    helpMenu->addSeparator();

    auto* aboutAction = helpMenu->addAction(QStringLiteral("About Lulu Code"));
    connect(aboutAction, &QAction::triggered, this, [this] {
        QMessageBox box(this);
        box.setWindowTitle(QStringLiteral("About Lulu Code"));
        box.setIconPixmap(QPixmap(QStringLiteral(":/icons/logo.png"))
                              .scaled(72, 72, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        box.setTextFormat(Qt::RichText);
        box.setText(QStringLiteral(
            "<b>Lulu Code</b> %1<br><br>"
            "A local-first AI coding agent. Your models, your files, your machine.<br><br>"
            "<span style='color:#8d8d97'>llama.cpp · Qt %2 · C++23</span>")
                        .arg(QStringLiteral(LULU_VERSION), QStringLiteral(QT_VERSION_STR)));
        box.setStandardButtons(QMessageBox::Ok);
        box.exec();
    });

    auto* stopAction = new QAction(this);
    stopAction->setShortcut(QKeySequence(Qt::Key_Escape));
    stopAction->setShortcutContext(Qt::WindowShortcut);
    connect(stopAction, &QAction::triggered, agent_, &AgentSession::cancel);
    addAction(stopAction);
}

void MainWindow::buildStatusBar()
{
    auto* bar = statusBar();
    bar->setSizeGripEnabled(false);
    bar->setFixedHeight(27);

    statusState_ = new QLabel(QStringLiteral("IDLE"), bar);
    statusState_->setObjectName(QStringLiteral("StatusValue"));
    statusState_->setContentsMargins(12, 0, 12, 0);
    bar->addWidget(statusState_);

    statusContext_ = addStatusItem(bar, QStringLiteral("CONTEXT"));
    statusSpeed_ = addStatusItem(bar, QStringLiteral("SPEED"));
    statusBackend_ = addStatusItem(bar, QStringLiteral("BACKEND"));
    statusBackend_->setText(QStringLiteral("llama.cpp"));
}

void MainWindow::wireSignals()
{
    connect(sidebar_, &Sidebar::newChatRequested, this, &MainWindow::newChat);
    connect(sidebar_, &Sidebar::manageModelsRequested, this, &MainWindow::openModelsDialog);
    connect(sidebar_, &Sidebar::openFolderRequested, this, &MainWindow::chooseWorkspace);
    connect(sidebar_, &Sidebar::settingsRequested, this, &MainWindow::openSettingsDialog);
    connect(sidebar_, &Sidebar::reloadModelRequested, this, &MainWindow::reloadModel);
    connect(sidebar_, &Sidebar::ejectModelRequested, this, &MainWindow::unloadModel);
    connect(sidebar_, &Sidebar::githubRequested, this, [] {
        QDesktopServices::openUrl(QUrl(githubUrl()));
    });

    connect(chat_, &ChatPanel::sendRequested, agent_, &AgentSession::send);
    connect(chat_, &ChatPanel::stopRequested, agent_, &AgentSession::cancel);
    connect(chat_, &ChatPanel::newChatRequested, this, &MainWindow::newChat);
    connect(chat_, &ChatPanel::chooseModelRequested, this, &MainWindow::openModelsDialog);
    connect(chat_, &ChatPanel::openFolderRequested, this, &MainWindow::chooseWorkspace);
    connect(chat_, &ChatPanel::toggleWorkspaceRequested, this, [this] {
        const bool visible = !workspacePanel_->isVisible();
        workspacePanel_->setVisible(visible);
        if (toggleExplorer_) {
            const QSignalBlocker blocker(toggleExplorer_);
            toggleExplorer_->setChecked(visible);
        }
    });

    connect(workspacePanel_, &WorkspacePanel::openFolderRequested, this, &MainWindow::chooseWorkspace);
    connect(workspacePanel_, &WorkspacePanel::hideRequested, this, [this] {
        workspacePanel_->hide();
        if (toggleExplorer_) {
            const QSignalBlocker blocker(toggleExplorer_);
            toggleExplorer_->setChecked(false);
        }
    });

    connect(agent_, &AgentSession::userMessage, chat_, &ChatPanel::addUserMessage);
    connect(agent_, &AgentSession::assistantStarted, chat_, &ChatPanel::beginAssistant);
    connect(agent_, &AgentSession::assistantDelta, chat_, &ChatPanel::appendAssistant);
    connect(agent_, &AgentSession::assistantFinished, chat_, &ChatPanel::finishAssistant);

    connect(agent_, &AgentSession::toolStarted, chat_, &ChatPanel::addToolCard);
    connect(agent_, &AgentSession::toolFinished, chat_, &ChatPanel::finishToolCard);
    connect(agent_, &AgentSession::toolStarted, workspacePanel_, &WorkspacePanel::activityStarted);
    connect(agent_, &AgentSession::toolFinished, workspacePanel_, &WorkspacePanel::activityFinished);

    connect(agent_, &AgentSession::notice, this, [this](const QString& message) {
        chat_->addNotice(message, false);
    });
    connect(agent_, &AgentSession::errorOccurred, this, [this](const QString& message) {
        chat_->addNotice(message, true);
    });

    connect(agent_, &AgentSession::stateChanged, this,
            [this](AgentSession::State state, const QString& detail) {
                chat_->setBusy(state != AgentSession::State::Idle);
                switch (state) {
                case AgentSession::State::Generating:
                    statusState_->setText(QStringLiteral("THINKING"));
                    chat_->setStatusText(QStringLiteral("GENERATING"));
                    break;
                case AgentSession::State::RunningTool:
                    statusState_->setText(detail.isEmpty() ? QStringLiteral("TOOL")
                                                          : detail.toUpper());
                    chat_->setStatusText(QStringLiteral("TOOL · %1")
                                             .arg(detail.isEmpty() ? QStringLiteral("RUNNING")
                                                                   : detail.toUpper()));
                    break;
                case AgentSession::State::Idle:
                    statusState_->setText(modelReady_ ? QStringLiteral("READY")
                                                      : QStringLiteral("IDLE"));
                    chat_->setStatusText(QStringLiteral("READY"));
                    break;
                }
            });

    connect(agent_, &AgentSession::statsUpdated, this, [this](const TokenStats& stats) {
        if (stats.tokensPerSecond > 0.0) {
            statusSpeed_->setText(QStringLiteral("%1 tok/s")
                                      .arg(QString::number(stats.tokensPerSecond, 'f', 1)));
        }
    });

    connect(agent_, &AgentSession::contextUsed, this, [this](int promptTokens, int contextSize) {
        statusContext_->setText(QStringLiteral("%1 / %2")
                                    .arg(shortCount(promptTokens), shortCount(contextSize)));
    });

    connect(engine_, &LlamaEngine::loadingStarted, this, [this] {
        modelReady_ = false;
        agent_->setModelReady(false);
        sidebar_->setModelLoading(modelName(pendingModelId_, store_));
        statusState_->setText(QStringLiteral("LOADING"));
        chat_->setStatusText(QStringLiteral("LOADING MODEL"));
    });

    connect(engine_, &LlamaEngine::loaded, this,
            [this](const QString& description, int contextSize, const QString& chatTemplate) {
                Q_UNUSED(chatTemplate);
                loadedModelId_ = pendingModelId_;
                pendingModelId_.clear();
                modelReady_ = true;
                agent_->setModelReady(true);

                const QString name = modelName(loadedModelId_, store_);
                sidebar_->setModelInfo(true, name, description, contextSize);
                sidebar_->clearDownload();
                chat_->setModelName(name);
                chat_->setReady(true);
                chat_->setStatusText(QStringLiteral("READY"));
                statusState_->setText(QStringLiteral("READY"));
                statusContext_->setText(QStringLiteral("0 / %1").arg(shortCount(contextSize)));
                statusSpeed_->setText(QStringLiteral("—"));
                saveSettings();
            });

    connect(engine_, &LlamaEngine::loadFailed, this, [this](const QString& error) {
        const QString failed = pendingModelId_;
        pendingModelId_.clear();
        loadedModelId_.clear();
        modelReady_ = false;
        agent_->setModelReady(false);

        sidebar_->setModelInfo(false, {}, {}, 0);
        chat_->setModelName({});
        chat_->setReady(false);
        statusState_->setText(QStringLiteral("ERROR"));
        chat_->setStatusText(QStringLiteral("MODEL ERROR"));
        chat_->addNotice(QStringLiteral("Could not load %1: %2")
                             .arg(failed.isEmpty() ? QStringLiteral("the model") : failed, error),
                         true);
    });

    connect(engine_, &LlamaEngine::unloaded, this, [this] {
        loadedModelId_.clear();
        pendingModelId_.clear();
        modelReady_ = false;
        agent_->setModelReady(false);

        sidebar_->setModelInfo(false, {}, {}, 0);
        chat_->setModelName({});
        chat_->setReady(false);
        chat_->setStatusText(QStringLiteral("READY"));
        statusState_->setText(QStringLiteral("IDLE"));
        statusContext_->setText(QStringLiteral("—"));
        statusSpeed_->setText(QStringLiteral("—"));
        saveSettings();
    });

    connect(store_, &ModelStore::changed, this, &MainWindow::refreshModelUi);

    connect(store_, &ModelStore::downloadStarted, this, [this](const QString& id) {
        sidebar_->setDownloadProgress(modelName(id, store_), 0, 0);
    });
    connect(store_, &ModelStore::downloadProgress, this,
            [this](const QString& id, qint64 received, qint64 total) {
                sidebar_->setDownloadProgress(modelName(id, store_), received, total);
            });
    connect(store_, &ModelStore::downloadFinished, this, [this](const QString& id) {
        sidebar_->clearDownload();
        refreshModelUi();
        chat_->addNotice(QStringLiteral("%1 is ready to use.").arg(modelName(id, store_)), false);
    });
    connect(store_, &ModelStore::downloadFailed, this, [this](const QString& id, const QString& error) {
        sidebar_->clearDownload();
        chat_->addNotice(QStringLiteral("Download of %1 failed: %2").arg(modelName(id, store_), error),
                         true);
    });

    connect(tools_, &ToolRunner::workspaceChanged, this, &MainWindow::updateToolchain);
}

void MainWindow::restoreState()
{
    QSettings settings;
    restoreGeometry(settings.value(QStringLiteral("ui/geometry")).toByteArray());
    QMainWindow::restoreState(settings.value(QStringLiteral("ui/windowState")).toByteArray());
    splitter_->restoreState(settings.value(QStringLiteral("ui/splitter")).toByteArray());

    const bool explorerVisible = settings.value(QStringLiteral("ui/explorer"), true).toBool();
    workspacePanel_->setVisible(explorerVisible);
    if (toggleExplorer_) {
        const QSignalBlocker blocker(toggleExplorer_);
        toggleExplorer_->setChecked(explorerVisible);
    }

    const QString workspace = settings.value(QStringLiteral("workspace/path")).toString();
    if (!workspace.isEmpty() && QFileInfo(workspace).isDir()) {
        openWorkspace(workspace);
    }
}

void MainWindow::persistState()
{
    QSettings settings;
    settings.setValue(QStringLiteral("ui/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("ui/windowState"), saveState());
    settings.setValue(QStringLiteral("ui/splitter"), splitter_->saveState());
    settings.setValue(QStringLiteral("ui/explorer"), workspacePanel_->isVisible());
    settings.setValue(QStringLiteral("workspace/path"), workspacePath_);
    saveSettings();
}

void MainWindow::readSettings()
{
    QSettings settings;
    settings_.maxTokens = settings.value(QStringLiteral("generation/maxTokens"), 768).toInt();
    settings_.temperature = settings.value(QStringLiteral("generation/temperature"), 0.7).toDouble();
    settings_.topP = settings.value(QStringLiteral("generation/topP"), 0.95).toDouble();
    settings_.topK = settings.value(QStringLiteral("generation/topK"), 40).toInt();
    settings_.repeatPenalty = settings.value(QStringLiteral("generation/repeatPenalty"), 1.08).toDouble();

    settings_.contextSize = settings.value(QStringLiteral("model/contextSize"), 8192).toInt();
    settings_.gpuLayers = settings.value(QStringLiteral("model/gpuLayers"), 0).toInt();
    settings_.threads = settings.value(QStringLiteral("model/threads"), 0).toInt();

    settings_.maxSteps = settings.value(QStringLiteral("agent/maxSteps"), 14).toInt();
    settings_.autoApproveWrites = settings.value(QStringLiteral("agent/autoApproveWrites"), false).toBool();
    settings_.autoApproveCommands =
        settings.value(QStringLiteral("agent/autoApproveCommands"), false).toBool();
    settings_.systemPrompt = settings.value(QStringLiteral("agent/systemPrompt")).toString();
}

void MainWindow::saveSettings()
{
    QSettings settings;
    settings.setValue(QStringLiteral("generation/maxTokens"), settings_.maxTokens);
    settings.setValue(QStringLiteral("generation/temperature"), settings_.temperature);
    settings.setValue(QStringLiteral("generation/topP"), settings_.topP);
    settings.setValue(QStringLiteral("generation/topK"), settings_.topK);
    settings.setValue(QStringLiteral("generation/repeatPenalty"), settings_.repeatPenalty);

    settings.setValue(QStringLiteral("model/contextSize"), settings_.contextSize);
    settings.setValue(QStringLiteral("model/gpuLayers"), settings_.gpuLayers);
    settings.setValue(QStringLiteral("model/threads"), settings_.threads);
    settings.setValue(QStringLiteral("model/last"), loadedModelId_);

    settings.setValue(QStringLiteral("agent/maxSteps"), settings_.maxSteps);
    settings.setValue(QStringLiteral("agent/autoApproveWrites"), settings_.autoApproveWrites);
    settings.setValue(QStringLiteral("agent/autoApproveCommands"), settings_.autoApproveCommands);
    settings.setValue(QStringLiteral("agent/systemPrompt"), settings_.systemPrompt);

    settings.setValue(QStringLiteral("workspace/path"), workspacePath_);
}

void MainWindow::applySettings(const AppSettings& settings, bool persist)
{
    settings_ = settings;
    applyGenerationSettings();

    if (!persist) {
        return;
    }

    saveSettings();

    const QString target = loadedModelId_.isEmpty() ? pendingModelId_ : loadedModelId_;
    if (!target.isEmpty()) {
        chat_->addNotice(QStringLiteral("Settings saved. Reloading the model to apply them."), false);
        loadModelById(target);
    } else {
        chat_->addNotice(QStringLiteral("Settings saved."), false);
    }
}

void MainWindow::applyGenerationSettings()
{
    GenerationParams params;
    params.maxTokens = settings_.maxTokens;
    params.temperature = settings_.temperature;
    params.topP = settings_.topP;
    params.topK = settings_.topK;
    params.repeatPenalty = settings_.repeatPenalty;
    agent_->setGenerationParams(params);

    agent_->setExtraSystemPrompt(settings_.systemPrompt);
    agent_->setMaxSteps(settings_.maxSteps);

    tools_->setAutoApproveWrites(settings_.autoApproveWrites);
    tools_->setAutoApproveCommands(settings_.autoApproveCommands);
}

void MainWindow::openWorkspace(const QString& path)
{
    const QFileInfo info(path);
    if (path.isEmpty() || !info.exists() || !info.isDir()) {
        chat_->addNotice(QStringLiteral("That folder is not available:\n%1")
                             .arg(QDir::toNativeSeparators(path)),
                         true);
        return;
    }

    workspacePath_ = info.absoluteFilePath();
    const QDir dir(workspacePath_);

    sidebar_->setWorkspace(workspacePath_);
    chat_->setProjectName(dir.dirName().isEmpty() ? workspacePath_ : dir.dirName(), workspacePath_);
    workspacePanel_->setRoot(workspacePath_);
    workspacePanel_->clearActivity();
    tools_->setWorkspace(workspacePath_);
    agent_->setWorkspace(workspacePath_);
    updateToolchain();

    QSettings().setValue(QStringLiteral("workspace/path"), workspacePath_);
}

void MainWindow::chooseWorkspace()
{
    const QString start = workspacePath_.isEmpty() ? QDir::homePath() : workspacePath_;
    const QString path = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Open a project folder"), start,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!path.isEmpty()) {
        openWorkspace(path);
    }
}

void MainWindow::openModelsDialog()
{
    ModelsDialog dialog(store_, loadedModelId_, this);
    connect(&dialog, &ModelsDialog::loadRequested, this, &MainWindow::loadModelById);
    dialog.exec();
    refreshModelUi();
}

void MainWindow::openSettingsDialog()
{
    SettingsDialog dialog(settings_, this);
    if (dialog.exec() == QDialog::Accepted) {
        applySettings(dialog.settings(), true);
    }
}

void MainWindow::loadModelById(const QString& id)
{
    if (id.isEmpty()) {
        return;
    }

    QString path;
    if (const auto model = store_->find(id)) {
        path = model->path;
    } else {
        path = store_->expectedPathFor(id);
    }
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        chat_->addNotice(QStringLiteral("The model file for %1 is missing. Open Models to download it again.")
                             .arg(modelName(id, store_)),
                         true);
        return;
    }

    if (agent_->busy()) {
        agent_->cancel();
    }

    pendingModelId_ = id;
    loadedModelId_.clear();
    modelReady_ = false;
    agent_->setModelReady(false);
    chat_->setReady(false);

    sidebar_->setModelLoading(modelName(id, store_));
    chat_->setModelName(modelName(id, store_));
    statusState_->setText(QStringLiteral("LOADING"));

    const int contextSize = settings_.contextSize;
    const int gpuLayers = settings_.gpuLayers;
    const int threads = settings_.threads;

    QMetaObject::invokeMethod(engine_, "loadModel", Qt::QueuedConnection,
                              Q_ARG(QString, path),
                              Q_ARG(int, contextSize),
                              Q_ARG(int, gpuLayers),
                              Q_ARG(int, threads));
}

void MainWindow::reloadModel()
{
    const QString target = loadedModelId_.isEmpty() ? pendingModelId_ : loadedModelId_;
    if (target.isEmpty()) {
        chat_->addNotice(QStringLiteral("No model is loaded yet. Choose one from Models."), false);
        openModelsDialog();
        return;
    }
    loadModelById(target);
}

void MainWindow::unloadModel()
{
    if (loadedModelId_.isEmpty() && pendingModelId_.isEmpty()) {
        return;
    }
    statusState_->setText(QStringLiteral("UNLOADING"));
    QMetaObject::invokeMethod(engine_, "unloadModel", Qt::QueuedConnection);
}

void MainWindow::newChat()
{
    agent_->reset();
    chat_->clearConversation();
    workspacePanel_->clearActivity();
    statusState_->setText(modelReady_ ? QStringLiteral("READY") : QStringLiteral("IDLE"));
    statusContext_->setText(QStringLiteral("—"));
    statusSpeed_->setText(QStringLiteral("—"));
    chat_->composer()->focusInput();
}

void MainWindow::updateToolchain()
{
    sidebar_->setToolchain(Toolchain::probe(workspacePath_));
}

void MainWindow::refreshModelUi()
{
    if (!pendingModelId_.isEmpty()) {
        return;
    }

    if (modelReady_ && !loadedModelId_.isEmpty()) {
        const QString name = modelName(loadedModelId_, store_);
        sidebar_->setModelInfo(true, name, modelDescription(loadedModelId_), engine_->contextSize());
        chat_->setModelName(name);
        chat_->setReady(true);
        statusState_->setText(QStringLiteral("READY"));
    } else {
        sidebar_->setModelInfo(false, {}, {}, 0);
        chat_->setModelName({});
        chat_->setReady(false);
        statusState_->setText(QStringLiteral("IDLE"));
    }
}

bool MainWindow::askApproval(const ApprovalRequest& request)
{
    ApprovalDialog dialog(request, this);
    const bool allowed = dialog.exec() == QDialog::Accepted;

    if (allowed && dialog.remember()) {
        if (request.kind == ApprovalKind::Command) {
            settings_.autoApproveCommands = true;
            tools_->setAutoApproveCommands(true);
        } else {
            settings_.autoApproveWrites = true;
            tools_->setAutoApproveWrites(true);
        }
    }

    return allowed;
}

} // namespace lulu
