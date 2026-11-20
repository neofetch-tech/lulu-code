#pragma once

#include <QDialog>
#include <QString>

class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QPlainTextEdit;
class QSlider;
class QSpinBox;

namespace lulu {

struct AppSettings {
    int maxTokens = 768;
    double temperature = 0.7;
    double topP = 0.95;
    int topK = 40;
    double repeatPenalty = 1.08;
    int contextSize = 8192;
    int gpuLayers = 0;
    int threads = 0;
    int maxSteps = 14;
    bool autoApproveWrites = false;
    bool autoApproveCommands = false;
    QString systemPrompt;
};

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(const AppSettings& current, QWidget* parent = nullptr);

    AppSettings settings() const;

private:
    QWidget* buildGenerationSection();
    QWidget* buildModelSection();
    QWidget* buildAgentSection();

    QSlider* temperature_ = nullptr;
    QLabel* temperatureValue_ = nullptr;
    QSlider* topP_ = nullptr;
    QLabel* topPValue_ = nullptr;
    QSpinBox* maxTokens_ = nullptr;
    QDoubleSpinBox* repeatPenalty_ = nullptr;

    QSpinBox* contextSize_ = nullptr;
    QSpinBox* gpuLayers_ = nullptr;
    QSpinBox* threads_ = nullptr;

    QSpinBox* maxSteps_ = nullptr;
    QCheckBox* autoWrites_ = nullptr;
    QCheckBox* autoCommands_ = nullptr;
    QPlainTextEdit* systemPrompt_ = nullptr;
};

} // namespace lulu
