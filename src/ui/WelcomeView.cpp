#include "ui/WelcomeView.h"

#include "ui/Theme.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

namespace lulu {

namespace {

QPushButton* exampleButton(const QString& text, QWidget* parent)
{
    auto* button = new QPushButton(text, parent);
    button->setObjectName(QStringLiteral("ExampleButton"));
    button->setCursor(Qt::PointingHandCursor);
    button->setMinimumHeight(52);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    return button;
}

} // namespace

WelcomeView::WelcomeView(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("ChatCanvas"));

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(40, 24, 40, 24);
    outer->addStretch(1);

    auto* card = new QWidget(this);
    card->setMaximumWidth(660);
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);

    auto* logo = new QLabel(card);
    QPixmap pixmap(QStringLiteral(":/icons/logo.png"));
    logo->setPixmap(pixmap.scaled(72, 72, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logo->setAlignment(Qt::AlignCenter);
    layout->addWidget(logo, 0, Qt::AlignHCenter);

    auto* title = new QLabel(QStringLiteral("Lulu Code"), card);
    title->setObjectName(QStringLiteral("WelcomeTitle"));
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    auto* subtitle = new QLabel(
        QStringLiteral("A local coding agent that reads your files, edits them and runs your tools."),
        card);
    subtitle->setObjectName(QStringLiteral("WelcomeSubtitle"));
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setWordWrap(true);
    layout->addWidget(subtitle);

    auto* chips = new QHBoxLayout;
    chips->setSpacing(8);
    chips->addStretch();
    modelChip_ = new QLabel(QStringLiteral("MODEL · NONE"), card);
    modelChip_->setObjectName(QStringLiteral("WelcomeChip"));
    chips->addWidget(modelChip_);
    folderChip_ = new QLabel(QStringLiteral("FOLDER · NOT SET"), card);
    folderChip_->setObjectName(QStringLiteral("WelcomeChip"));
    chips->addWidget(folderChip_);
    chips->addStretch();
    layout->addLayout(chips);

    auto* actions = new QHBoxLayout;
    actions->setSpacing(9);
    actions->addStretch();
    modelButton_ = new QPushButton(QStringLiteral("Choose a model"), card);
    modelButton_->setProperty("variant", QStringLiteral("primary"));
    modelButton_->setCursor(Qt::PointingHandCursor);
    actions->addWidget(modelButton_);
    folderButton_ = new QPushButton(QStringLiteral("Open a folder"), card);
    folderButton_->setCursor(Qt::PointingHandCursor);
    actions->addWidget(folderButton_);
    actions->addStretch();
    layout->addLayout(actions);

    layout->addSpacing(10);

    examples_ = new QWidget(card);
    auto* grid = new QGridLayout(examples_);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(10);

    const QVector<QPair<QString, QString>> examples = {
        {QStringLiteral("Explain this codebase"),
         QStringLiteral("Map the architecture, the entry points and the main modules of this project.")},
        {QStringLiteral("Fix the failing tests"),
         QStringLiteral("Run the test suite, find the failure, fix the cause and re-run the tests.")},
        {QStringLiteral("Add tests for a module"),
         QStringLiteral("Pick the most important untested module and add focused unit tests for it.")},
        {QStringLiteral("Refactor the roughest file"),
         QStringLiteral("Find the largest, messiest source file and split it into clean pieces without changing behaviour.")},
    };

    int index = 0;
    for (const auto& example : examples) {
        auto* button = exampleButton(example.first + QStringLiteral("  →"), examples_);
        button->setToolTip(example.second);
        const QString prompt = example.second;
        connect(button, &QPushButton::clicked, this, [this, prompt] { emit promptChosen(prompt); });
        grid->addWidget(button, index / 2, index % 2);
        ++index;
    }
    layout->addWidget(examples_);

    auto* footnote = new QLabel(QStringLiteral("100% local · llama.cpp · no cloud, no telemetry"), card);
    footnote->setObjectName(QStringLiteral("ComposerHint"));
    footnote->setAlignment(Qt::AlignCenter);
    layout->addWidget(footnote);

    outer->addWidget(card, 0, Qt::AlignHCenter);
    outer->addStretch(2);

    connect(modelButton_, &QPushButton::clicked, this, &WelcomeView::chooseModelRequested);
    connect(folderButton_, &QPushButton::clicked, this, &WelcomeView::openFolderRequested);

    setModelReady(false, {});
    setWorkspaceName({});
}

void WelcomeView::setModelReady(bool ready, const QString& modelName)
{
    if (ready) {
        modelChip_->setText(QStringLiteral("MODEL · %1").arg(modelName.toUpper()));
        modelChip_->setStyleSheet(QStringLiteral("background:#f5f5f7;color:#0a0a0c;border-color:#f5f5f7;font-weight:700;"));
        modelButton_->setVisible(false);
    } else {
        modelChip_->setText(QStringLiteral("MODEL · NONE"));
        modelChip_->setStyleSheet({});
        modelButton_->setVisible(true);
    }
}

void WelcomeView::setWorkspaceName(const QString& name)
{
    if (name.isEmpty()) {
        folderChip_->setText(QStringLiteral("FOLDER · NOT SET"));
        folderButton_->setText(QStringLiteral("Open a folder"));
    } else {
        folderChip_->setText(QStringLiteral("FOLDER · %1").arg(name.toUpper()));
        folderButton_->setText(QStringLiteral("Change folder"));
    }
}

void WelcomeView::setBusy(bool busy)
{
    examples_->setEnabled(!busy);
    modelButton_->setEnabled(!busy);
    folderButton_->setEnabled(!busy);
}

} // namespace lulu
