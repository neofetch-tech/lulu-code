#include "ui/Sidebar.h"

#include "ui/ElidedLabel.h"
#include "ui/Theme.h"

#include <QDir>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

#include <algorithm>

namespace lulu {

namespace {

QPushButton* iconButton(const QString& text, const QString& tip, QWidget* parent)
{
    auto* button = new QPushButton(text, parent);
    button->setProperty("variant", QStringLiteral("icon"));
    button->setToolTip(tip);
    button->setCursor(Qt::PointingHandCursor);
    button->setFixedSize(28, 28);
    return button;
}

QFrame* sectionCard(QWidget* parent)
{
    auto* card = new QFrame(parent);
    card->setObjectName(QStringLiteral("SidebarCard"));
    return card;
}

QLabel* sectionLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("SectionLabel"));
    return label;
}

} // namespace

Sidebar::Sidebar(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("Sidebar"));
    setFixedWidth(262);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 18, 16, 14);
    layout->setSpacing(12);

    // -- brand --------------------------------------------------------------
    auto* brand = new QHBoxLayout;
    brand->setSpacing(10);
    auto* logo = new QLabel(this);
    QPixmap pixmap(QStringLiteral(":/icons/logo.png"));
    logo->setPixmap(pixmap.scaled(34, 34, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    brand->addWidget(logo);

    auto* brandText = new QVBoxLayout;
    brandText->setSpacing(0);
    auto* wordmark = new QLabel(QStringLiteral("Lulu Code"), this);
    wordmark->setObjectName(QStringLiteral("Wordmark"));
    brandText->addWidget(wordmark);
    auto* tagline = new QLabel(QStringLiteral("LOCAL AI CODER"), this);
    tagline->setObjectName(QStringLiteral("Tagline"));
    brandText->addWidget(tagline);
    brand->addLayout(brandText);
    brand->addStretch();
    layout->addLayout(brand);

    auto* newChat = new QPushButton(QStringLiteral("＋   New chat"), this);
    newChat->setProperty("variant", QStringLiteral("primary"));
    newChat->setCursor(Qt::PointingHandCursor);
    newChat->setMinimumHeight(36);
    layout->addWidget(newChat);

    layout->addSpacing(2);
    layout->addWidget(sectionLabel(QStringLiteral("MODEL"), this));

    // -- model card ---------------------------------------------------------
    auto* modelCard = sectionCard(this);
    auto* modelLayout = new QVBoxLayout(modelCard);
    modelLayout->setContentsMargins(12, 11, 12, 11);
    modelLayout->setSpacing(6);

    auto* modelTop = new QHBoxLayout;
    modelTop->setSpacing(6);
    modelTitle_ = new QLabel(QStringLiteral("No model loaded"), modelCard);
    modelTitle_->setObjectName(QStringLiteral("CardTitle"));
    modelTop->addWidget(modelTitle_, 1);
    statusDot_ = new QWidget(modelCard);
    statusDot_->setObjectName(QStringLiteral("StatusDot"));
    statusDot_->setProperty("active", false);
    modelTop->addWidget(statusDot_, 0, Qt::AlignVCenter);
    modelLayout->addLayout(modelTop);

    modelDescription_ = new QLabel(QStringLiteral("Download a GGUF model to start working locally."), modelCard);
    modelDescription_->setObjectName(QStringLiteral("CardMeta"));
    modelDescription_->setWordWrap(true);
    modelDescription_->setMaximumHeight(46);
    modelLayout->addWidget(modelDescription_);

    modelMeta_ = new QLabel(QStringLiteral("llama.cpp · 100% local"), modelCard);
    modelMeta_->setObjectName(QStringLiteral("MonoSmall"));
    modelLayout->addWidget(modelMeta_);

    modelStatus_ = new QLabel(QStringLiteral("IDLE"), modelCard);
    modelStatus_->setObjectName(QStringLiteral("SectionLabel"));
    modelLayout->addWidget(modelStatus_);

    chooseModel_ = new QPushButton(QStringLiteral("Choose model"), modelCard);
    chooseModel_->setProperty("variant", QStringLiteral("primary"));
    chooseModel_->setCursor(Qt::PointingHandCursor);
    modelLayout->addWidget(chooseModel_);

    auto* modelButtons = new QHBoxLayout;
    modelButtons->setSpacing(5);
    changeModel_ = new QPushButton(QStringLiteral("Change"), modelCard);
    changeModel_->setCursor(Qt::PointingHandCursor);
    modelButtons->addWidget(changeModel_, 1);
    reloadModel_ = iconButton(QStringLiteral("↻"), QStringLiteral("Reload model with current settings"), modelCard);
    ejectModel_ = iconButton(QStringLiteral("⏏"), QStringLiteral("Unload model"), modelCard);
    modelButtons->addWidget(reloadModel_);
    modelButtons->addWidget(ejectModel_);
    modelLayout->addLayout(modelButtons);

    downloadBox_ = new QWidget(modelCard);
    auto* downloadLayout = new QVBoxLayout(downloadBox_);
    downloadLayout->setContentsMargins(0, 4, 0, 0);
    downloadLayout->setSpacing(4);
    downloadLabel_ = new QLabel(QStringLiteral("Downloading…"), downloadBox_);
    downloadLabel_->setObjectName(QStringLiteral("CardMeta"));
    downloadLayout->addWidget(downloadLabel_);
    downloadBar_ = new QProgressBar(downloadBox_);
    downloadBar_->setRange(0, 100);
    downloadBar_->setValue(0);
    downloadBar_->setTextVisible(false);
    downloadBar_->setFixedHeight(5);
    downloadLayout->addWidget(downloadBar_);
    downloadBox_->setVisible(false);
    modelLayout->addWidget(downloadBox_);

    layout->addWidget(modelCard);

    chooseModel_->setVisible(true);
    changeModel_->setVisible(false);
    reloadModel_->setVisible(false);
    ejectModel_->setVisible(false);

    // -- workspace ----------------------------------------------------------
    layout->addWidget(sectionLabel(QStringLiteral("WORKSPACE"), this));
    auto* workspaceCard = sectionCard(this);
    auto* workspaceLayout = new QVBoxLayout(workspaceCard);
    workspaceLayout->setContentsMargins(12, 11, 12, 11);
    workspaceLayout->setSpacing(4);

    workspaceName_ = new QLabel(QStringLiteral("No folder open"), workspaceCard);
    workspaceName_->setObjectName(QStringLiteral("CardTitle"));
    workspaceLayout->addWidget(workspaceName_);

    workspacePath_ = new ElidedLabel(workspaceCard);
    workspacePath_->setObjectName(QStringLiteral("MonoSmall"));
    workspacePath_->setText(QStringLiteral("Open a project folder to give the agent hands."));
    workspaceLayout->addWidget(workspacePath_);

    auto* openFolder = new QPushButton(QStringLiteral("Open folder"), workspaceCard);
    openFolder->setCursor(Qt::PointingHandCursor);
    workspaceLayout->addWidget(openFolder);
    layout->addWidget(workspaceCard);

    // -- exoskeleton --------------------------------------------------------
    layout->addWidget(sectionLabel(QStringLiteral("EXOSKELETON"), this));
    auto* toolCard = new QFrame(this);
    toolCard->setObjectName(QStringLiteral("SidebarCardMuted"));
    auto* toolLayout = new QVBoxLayout(toolCard);
    toolLayout->setContentsMargins(12, 10, 12, 10);
    toolLayout->setSpacing(5);

    const QStringList names = {
        QStringLiteral("git"), QStringLiteral("python"), QStringLiteral("node"),
        QStringLiteral("pytest"), QStringLiteral("playwright"),
    };
    for (const QString& name : names) {
        auto* row = new QHBoxLayout;
        row->setSpacing(7);
        auto* dot = new QLabel(QStringLiteral("•"), toolCard);
        dot->setStyleSheet(QStringLiteral("color: #3a3a42; font-size: 14px;"));
        auto* label = new QLabel(name, toolCard);
        label->setObjectName(QStringLiteral("ToolchainRow"));
        row->addWidget(dot);
        row->addWidget(label, 1);
        auto* state = new QLabel(QStringLiteral("—"), toolCard);
        state->setObjectName(QStringLiteral("MonoSmall"));
        row->addWidget(state);
        toolLayout->addLayout(row);
        toolchainDots_.append(dot);
        toolchainNames_.append(state);
    }
    layout->addWidget(toolCard);

    layout->addStretch(1);

    // -- footer -------------------------------------------------------------
    auto* github = new QPushButton(QStringLiteral("View on GitHub  ↗"), this);
    github->setProperty("variant", QStringLiteral("ghost"));
    github->setCursor(Qt::PointingHandCursor);
    layout->addWidget(github);

    auto* settings = new QPushButton(QStringLiteral("⚙   Settings"), this);
    settings->setProperty("variant", QStringLiteral("ghost"));
    settings->setCursor(Qt::PointingHandCursor);
    layout->addWidget(settings);

    auto* footer = new QLabel(QStringLiteral("v") + QStringLiteral(LULU_VERSION) + QStringLiteral("  ·  llama.cpp · Qt 6"), this);
    footer->setObjectName(QStringLiteral("SidebarFooter"));
    footer->setAlignment(Qt::AlignCenter);
    layout->addWidget(footer);

    connect(newChat, &QPushButton::clicked, this, &Sidebar::newChatRequested);
    connect(chooseModel_, &QPushButton::clicked, this, &Sidebar::manageModelsRequested);
    connect(changeModel_, &QPushButton::clicked, this, &Sidebar::manageModelsRequested);
    connect(reloadModel_, &QPushButton::clicked, this, &Sidebar::reloadModelRequested);
    connect(ejectModel_, &QPushButton::clicked, this, &Sidebar::ejectModelRequested);
    connect(openFolder, &QPushButton::clicked, this, &Sidebar::openFolderRequested);
    connect(settings, &QPushButton::clicked, this, &Sidebar::settingsRequested);
    connect(github, &QPushButton::clicked, this, &Sidebar::githubRequested);
}

void Sidebar::setModelInfo(bool loaded, const QString& name, const QString& description, int contextSize)
{
    if (loaded) {
        modelTitle_->setText(name);
        modelDescription_->setText(description);
        modelMeta_->setText(QStringLiteral("%1 tokens context · llama.cpp").arg(contextSize));
        modelStatus_->setText(QStringLiteral("READY"));
    } else {
        modelTitle_->setText(QStringLiteral("No model loaded"));
        modelDescription_->setText(QStringLiteral("Download a GGUF model to start working locally."));
        modelMeta_->setText(QStringLiteral("llama.cpp · 100% local"));
        modelStatus_->setText(QStringLiteral("IDLE"));
    }

    chooseModel_->setVisible(!loaded);
    changeModel_->setVisible(loaded);
    reloadModel_->setVisible(loaded);
    ejectModel_->setVisible(loaded);
    updateStatus(loaded);
    downloadBox_->setVisible(false);
}

void Sidebar::setModelLoading(const QString& name)
{
    modelTitle_->setText(name.isEmpty() ? QStringLiteral("Loading model…") : QStringLiteral("Loading %1…").arg(name));
    modelDescription_->setText(QStringLiteral("Reading weights into memory. Big models can take a while on CPU."));
    modelMeta_->setText(QStringLiteral("llama.cpp · warming up"));
    modelStatus_->setText(QStringLiteral("LOADING"));
    updateStatus(false);
    chooseModel_->setVisible(false);
    changeModel_->setVisible(false);
    reloadModel_->setVisible(false);
    ejectModel_->setVisible(false);
}

void Sidebar::updateStatus(bool loaded)
{
    statusDot_->setProperty("active", loaded);
    statusDot_->style()->unpolish(statusDot_);
    statusDot_->style()->polish(statusDot_);
}

void Sidebar::setWorkspace(const QString& path)
{
    if (path.isEmpty()) {
        workspaceName_->setText(QStringLiteral("No folder open"));
        workspacePath_->setText(QStringLiteral("Open a project folder to give the agent hands."));
        return;
    }
    const QDir dir(path);
    workspaceName_->setText(dir.dirName().isEmpty() ? path : dir.dirName());
    workspacePath_->setText(QDir::toNativeSeparators(path));
    workspacePath_->setToolTip(QDir::toNativeSeparators(path));
}

void Sidebar::setDownloadProgress(const QString& name, qint64 received, qint64 total)
{
    downloadBox_->setVisible(true);
    const int percent = total > 0 ? static_cast<int>((received * 100) / total) : 0;
    downloadBar_->setValue(std::clamp(percent, 0, 100));
    downloadLabel_->setText(QStringLiteral("Downloading %1 · %2%")
                                .arg(name)
                                .arg(percent));
    modelStatus_->setText(QStringLiteral("DOWNLOADING"));
}

void Sidebar::clearDownload()
{
    downloadBox_->setVisible(false);
    downloadBar_->setValue(0);
}

void Sidebar::setToolchain(const QVector<ToolchainEntry>& entries)
{
    // Rows are created in a fixed order: git, python, node, pytest, playwright.
    const QStringList order = {
        QStringLiteral("git"), QStringLiteral("python"), QStringLiteral("node"),
        QStringLiteral("pytest"), QStringLiteral("playwright"),
    };
    for (qsizetype i = 0; i < order.size() && i < toolchainNames_.size(); ++i) {
        const QString& name = order.at(i);
        const auto it = std::find_if(entries.cbegin(), entries.cend(),
                                     [&name](const ToolchainEntry& entry) { return entry.name == name; });
        const bool found = it != entries.cend() && it->found;
        toolchainNames_.at(i)->setText(found ? QStringLiteral("ok") : QStringLiteral("—"));
        toolchainNames_.at(i)->setStyleSheet(found ? QStringLiteral("color: #9fd6a0; font-size: 10px;")
                                                   : QStringLiteral("color: #4a4a52; font-size: 10px;"));
        toolchainDots_.at(i)->setStyleSheet(found ? QStringLiteral("color: #9fd6a0; font-size: 14px;")
                                                  : QStringLiteral("color: #33333a; font-size: 14px;"));
        toolchainDots_.at(i)->setToolTip(found ? it->path : (it != entries.cend() ? it->hint : QString()));
    }
}

} // namespace lulu
