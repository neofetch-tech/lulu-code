#pragma once

#include <QHash>
#include <QJsonObject>
#include <QWidget>

class QFileSystemModel;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QSortFilterProxyModel;
class QTreeView;

namespace lulu {

class WorkspacePanel : public QWidget
{
    Q_OBJECT

public:
    explicit WorkspacePanel(QWidget* parent = nullptr);

    void setRoot(const QString& path);
    void clearActivity();

signals:
    void openFolderRequested();
    void hideRequested();

public slots:
    void activityStarted(const QString& id, const QString& name, const QJsonObject& args);
    void activityFinished(const QString& id, bool ok);

private:
    void updateEmptyState();

    QFileSystemModel* fileModel_ = nullptr;
    QSortFilterProxyModel* filterModel_ = nullptr;
    QTreeView* tree_ = nullptr;
    QLabel* pathLabel_ = nullptr;
    QLabel* emptyLabel_ = nullptr;
    QLabel* activityEmpty_ = nullptr;
    QListWidget* activity_ = nullptr;
    QHash<QString, QListWidgetItem*> activityItems_;
    QString root_;
};

} // namespace lulu
