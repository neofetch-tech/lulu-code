#pragma once

#include "core/ModelCatalog.h"
#include "core/ModelStore.h"

#include <QDialog>
#include <QHash>

class QLabel;
class QProgressBar;
class QPushButton;
class QVBoxLayout;

namespace lulu {

/// Model browser/downloader. Cards are rebuilt whenever the store changes.
class ModelsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ModelsDialog(ModelStore* store, const QString& activeModelId, QWidget* parent = nullptr);

    void setActiveModel(const QString& id);

signals:
    void loadRequested(const QString& id);

private:
    struct CardWidgets {
        QPushButton* primary = nullptr;
        QPushButton* remove = nullptr;
        QProgressBar* progress = nullptr;
        QLabel* status = nullptr;
    };

    void rebuild();
    QWidget* buildCatalogCard(const ModelInfo& info);
    QWidget* buildLocalCard(const InstalledModel& model);
    void addLocalFile();

    ModelStore* store_ = nullptr;
    QString activeId_;
    QVBoxLayout* contentLayout_ = nullptr;
    QHash<QString, CardWidgets> cards_;
};

} // namespace lulu
