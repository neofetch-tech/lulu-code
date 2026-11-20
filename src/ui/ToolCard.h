#pragma once

#include <QFrame>
#include <QJsonObject>
#include <QString>

class QLabel;
class QTextBrowser;
class QToolButton;

namespace lulu {

/// Compact timeline entry for a tool invocation. Collapsed by default, opens
/// automatically when a tool fails.
class ToolCard : public QFrame
{
    Q_OBJECT

public:
    ToolCard(const QString& id, const QString& name, const QJsonObject& args,
             QWidget* parent = nullptr);

    QString id() const { return id_; }
    void setResult(bool ok, const QString& output);

private:
    void setExpanded(bool expanded);
    void applyState(const QString& state);
    static QString summarize(const QString& name, const QJsonObject& args);

    QString id_;
    QToolButton* toggle_ = nullptr;
    QLabel* name_ = nullptr;
    QLabel* meta_ = nullptr;
    QLabel* status_ = nullptr;
    QTextBrowser* output_ = nullptr;
    bool expanded_ = false;
};

} // namespace lulu
