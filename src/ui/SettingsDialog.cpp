#include "ui/SettingsDialog.h"

#include "ui/Theme.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

namespace lulu {

namespace {

QLabel* sectionLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setProperty("role", QStringLiteral("section"));
    return label;
}

QLabel* hintLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setProperty("role", QStringLiteral("hint"));
    label->setWordWrap(true);
    return label;
}

QWidget* sliderRow(QSlider* slider, QLabel* value, QWidget* parent)
{
    auto* widget = new QWidget(parent);
    auto* layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    layout->addWidget(slider, 1);
    value->setMinimumWidth(42);
    value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    value->setProperty("role", QStringLiteral("value"));
    layout->addWidget(value);
    return widget;
}

} // namespace

SettingsDialog::SettingsDialog(const AppSettings& current, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Settings"));
    setModal(true);
    setMinimumWidth(640);
    resize(680, 720);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(26, 22, 26, 20);
    layout->setSpacing(14);

    auto* title = new QLabel(QStringLiteral("Settings"), this);
    title->setObjectName(QStringLiteral("DialogTitle"));
    layout->addWidget(title);

    auto* subtitle = new QLabel(QStringLiteral("Tune how the model and the agent behave on this machine."), this);
    subtitle->setObjectName(QStringLiteral("DialogSubtitle"));
    layout->addWidget(subtitle);

    auto* scrollBody = new QWidget(this);
    auto* bodyLayout = new QVBoxLayout(scrollBody);
    bodyLayout->setContentsMargins(0, 6, 0, 0);
    bodyLayout->setSpacing(18);
    bodyLayout->addWidget(buildGenerationSection());
    bodyLayout->addWidget(buildModelSection());
    bodyLayout->addWidget(buildAgentSection());
    bodyLayout->addStretch(1);
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setObjectName(QStringLiteral("ChatScroll"));

    scrollArea->setWidget(scrollBody);
    layout->addWidget(scrollArea, 1);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    auto* cancel = new QPushButton(QStringLiteral("Cancel"), this);
    cancel->setCursor(Qt::PointingHandCursor);
    buttons->addWidget(cancel);
    auto* save = new QPushButton(QStringLiteral("Save settings"), this);
    save->setProperty("variant", QStringLiteral("primary"));
    save->setCursor(Qt::PointingHandCursor);
    buttons->addWidget(save);
    layout->addLayout(buttons);

    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(save, &QPushButton::clicked, this, &QDialog::accept);

    // Load values.
    temperature_->setValue(static_cast<int>(current.temperature * 100));
    topP_->setValue(static_cast<int>(current.topP * 100));
    maxTokens_->setValue(current.maxTokens);
    repeatPenalty_->setValue(current.repeatPenalty);
    contextSize_->setValue(current.contextSize);
    gpuLayers_->setValue(current.gpuLayers);
    threads_->setValue(current.threads);
    maxSteps_->setValue(current.maxSteps);
    autoWrites_->setChecked(current.autoApproveWrites);
    autoCommands_->setChecked(current.autoApproveCommands);
    systemPrompt_->setPlainText(current.systemPrompt);
    temperatureValue_->setText(QString::number(current.temperature, 'f', 2));
    topPValue_->setText(QString::number(current.topP, 'f', 2));
}

QWidget* SettingsDialog::buildGenerationSection()
{
    auto* section = new QWidget(this);
    auto* layout = new QVBoxLayout(section);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(9);

    layout->addWidget(sectionLabel(QStringLiteral("GENERATION"), section));

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(10);

    temperature_ = new QSlider(Qt::Horizontal, section);
    temperature_->setRange(0, 200);
    temperature_->setSingleStep(5);
    temperatureValue_ = new QLabel(QStringLiteral("0.70"), section);
    grid->addWidget(new QLabel(QStringLiteral("Temperature"), section), 0, 0);
    grid->addWidget(sliderRow(temperature_, temperatureValue_, section), 0, 1);

    topP_ = new QSlider(Qt::Horizontal, section);
    topP_->setRange(10, 100);
    topPValue_ = new QLabel(QStringLiteral("0.95"), section);
    grid->addWidget(new QLabel(QStringLiteral("Top-p"), section), 1, 0);
    grid->addWidget(sliderRow(topP_, topPValue_, section), 1, 1);

    maxTokens_ = new QSpinBox(section);
    maxTokens_->setRange(64, 16384);
    maxTokens_->setSingleStep(64);
    grid->addWidget(new QLabel(QStringLiteral("Max new tokens"), section), 2, 0);
    grid->addWidget(maxTokens_, 2, 1);

    repeatPenalty_ = new QDoubleSpinBox(section);
    repeatPenalty_->setRange(1.0, 1.6);
    repeatPenalty_->setSingleStep(0.01);
    repeatPenalty_->setDecimals(2);
    grid->addWidget(new QLabel(QStringLiteral("Repeat penalty"), section), 3, 0);
    grid->addWidget(repeatPenalty_, 3, 1);

    grid->setColumnStretch(1, 1);
    layout->addLayout(grid);

    connect(temperature_, &QSlider::valueChanged, this, [this](int value) {
        temperatureValue_->setText(QString::number(value / 100.0, 'f', 2));
    });
    connect(topP_, &QSlider::valueChanged, this, [this](int value) {
        topPValue_->setText(QString::number(value / 100.0, 'f', 2));
    });

    return section;
}

QWidget* SettingsDialog::buildModelSection()
{
    auto* section = new QWidget(this);
    auto* layout = new QVBoxLayout(section);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(9);

    layout->addWidget(sectionLabel(QStringLiteral("MODEL LOADING"), section));

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(10);

    contextSize_ = new QSpinBox(section);
    contextSize_->setRange(1024, 262144);
    contextSize_->setSingleStep(2048);
    contextSize_->setSuffix(QStringLiteral(" tokens"));
    grid->addWidget(new QLabel(QStringLiteral("Context size"), section), 0, 0);
    grid->addWidget(contextSize_, 0, 1);

    gpuLayers_ = new QSpinBox(section);
    gpuLayers_->setRange(0, 400);
    gpuLayers_->setToolTip(QStringLiteral("Layers offloaded to the GPU. 0 = pure CPU. 999 = all layers."));
    grid->addWidget(new QLabel(QStringLiteral("GPU layers"), section), 1, 0);
    grid->addWidget(gpuLayers_, 1, 1);

    threads_ = new QSpinBox(section);
    threads_->setRange(0, 128);
    threads_->setSpecialValueText(QStringLiteral("Auto"));
    grid->addWidget(new QLabel(QStringLiteral("CPU threads"), section), 2, 0);
    grid->addWidget(threads_, 2, 1);

    grid->setColumnStretch(1, 1);
    layout->addLayout(grid);
    layout->addWidget(hintLabel(
        QStringLiteral("Larger context uses more RAM. GPU layers need a llama.cpp build with CUDA, Vulkan or Metal. "
                       "Reload the model after changing these values."),
        section));

    return section;
}

QWidget* SettingsDialog::buildAgentSection()
{
    auto* section = new QWidget(this);
    auto* layout = new QVBoxLayout(section);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(9);

    layout->addWidget(sectionLabel(QStringLiteral("AGENT"), section));

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(10);
    maxSteps_ = new QSpinBox(section);
    maxSteps_->setRange(2, 40);
    grid->addWidget(new QLabel(QStringLiteral("Max tool steps per turn"), section), 0, 0);
    grid->addWidget(maxSteps_, 0, 1);
    grid->setColumnStretch(1, 1);
    layout->addLayout(grid);

    autoWrites_ = new QCheckBox(QStringLiteral("Auto-approve file writes and edits"), section);
    layout->addWidget(autoWrites_);
    autoCommands_ = new QCheckBox(QStringLiteral("Auto-approve shell commands, git and tests"), section);
    layout->addWidget(autoCommands_);
    layout->addWidget(hintLabel(
        QStringLiteral("With approvals on, Lulu Code asks before every change and runs commands only after you agree. "
                       "Auto-approving is faster but gives the model a free hand inside the workspace."),
        section));

    auto* promptLabel = new QLabel(QStringLiteral("Extra system prompt"), section);
    promptLabel->setProperty("role", QStringLiteral("value"));
    layout->addWidget(promptLabel);
    systemPrompt_ = new QPlainTextEdit(section);
    systemPrompt_->setPlaceholderText(QStringLiteral("Optional. Example: Prefer C++20, never use exceptions, always add tests."));
    systemPrompt_->setFixedHeight(84);
    layout->addWidget(systemPrompt_);

    return section;
}

AppSettings SettingsDialog::settings() const
{
    AppSettings result;
    result.temperature = temperature_->value() / 100.0;
    result.topP = topP_->value() / 100.0;
    result.maxTokens = maxTokens_->value();
    result.repeatPenalty = repeatPenalty_->value();
    result.contextSize = contextSize_->value();
    result.gpuLayers = gpuLayers_->value();
    result.threads = threads_->value();
    result.maxSteps = maxSteps_->value();
    result.autoApproveWrites = autoWrites_->isChecked();
    result.autoApproveCommands = autoCommands_->isChecked();
    result.systemPrompt = systemPrompt_->toPlainText().trimmed();
    return result;
}

} // namespace lulu
