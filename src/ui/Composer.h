#pragma once

#include <QPlainTextEdit>
#include <QWidget>

class QLabel;
class QPushButton;

namespace lulu {

class ComposerEdit : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit ComposerEdit(QWidget* parent = nullptr);

signals:
    void submitRequested();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
};

/// The bottom input bar: auto-growing editor, send/stop button and hints.
class Composer : public QWidget
{
    Q_OBJECT

public:
    explicit Composer(QWidget* parent = nullptr);

    void setBusy(bool busy);
    void setReady(bool ready);
    void focusInput();

signals:
    void sendRequested(const QString& text);
    void stopRequested();

private:
    void submit();
    void updateEditorHeight();

    ComposerEdit* edit_ = nullptr;
    QPushButton* send_ = nullptr;
    QPushButton* stop_ = nullptr;
    QLabel* hint_ = nullptr;
    bool busy_ = false;
    bool ready_ = false;
};

} // namespace lulu
