#include "ui/WorkspacePanel.h"

#include "ui/ElidedLabel.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonValue>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>

namespace lulu {

namespace {

class IgnoreFilterModel : public QSortFilterProxyModel
{
public:
    explicit IgnoreFilterModel(QObject* parent = nullptr)
        : QSortFilterProxyModel(parent)
    {
        setRecursiveFilteringEnabled(true);
    }

protected:
    bool filterAcceptsRow(int row, const QModelIndex& parent) const override
    {
        const QModelIndex index = sourceModel()->index(row, 0, parent);
        if (!index.isValid()) {
            return true;
        }
        auto* fs = qobject_cast<QFileSystemModel*>(sourceModel());
        if (!fs) {
            return true;
        }
        const QFileInfo info = fs->fileInfo(index);
        static const QStringList ignored = {
            QStringLiteral(".git"),   QStringLiteral("node_modules"), QStringLiteral(".venv"),
            QStringLiteral("venv"),   QStringLiteral("__pycache__"),  QStringLiteral("build"),
            QStringLiteral("dist"),   QStringLiteral("out"),          QStringLiteral(".idea"),
            QStringLiteral("target"), QStringLiteral(".next"),        QStringLiteral("obj"),
            QStringLiteral(".cache"), QStringLiteral(".gradle"),
        };
        if (info.isDir() && ignored.contains(info.fileName())) {
            return false;
        }
        return true;
    }
};

QString activityDetail(const QString& name, const QJsonObject& args)
{
    auto value = [&args](const QString& key) { return args.value(key).toString(); };
    if (name == QStringLiteral("run_command")) {
        return value(QStringLiteral("command"));
    }
    if (name == QStringLiteral("git")) {
        return QStringLiteral("git ") + value(QStringLiteral("action"));
    }
    if (name == QStringLiteral("search_files")) {
        return value(QStringLiteral("query"));
    }
    if (args.contains(QStringLiteral("path"))) {
        return value(QStringLiteral("path"));
    }
    return {};
}

} // namespace

WorkspacePanel::WorkspacePanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("WorkspacePanel"));
    setMinimumWidth(250);
    setMaximumWidth(380);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 14, 14, 12);
    layout->setSpacing(8);

    auto* header = new QHBoxLayout;
    header->setSpacing(4);
    auto* title = new QLabel(QStringLiteral("EXPLORER"), this);
    title->setObjectName(QStringLiteral("SectionLabel"));
    header->addWidget(title);
    header->addStretch();

    auto* refresh = new QPushButton(QStringLiteral("↻"), this);
    refresh->setProperty("variant", QStringLiteral("icon"));
    refresh->setToolTip(QStringLiteral("Refresh"));
    refresh->setCursor(Qt::PointingHandCursor);
    header->addWidget(refresh);

    auto* open = new QPushButton(QStringLiteral("＋"), this);
    open->setProperty("variant", QStringLiteral("icon"));
    open->setToolTip(QStringLiteral("Open folder"));
    open->setCursor(Qt::PointingHandCursor);
    header->addWidget(open);

    auto* close = new QPushButton(QStringLiteral("✕"), this);
    close->setProperty("variant", QStringLiteral("icon"));
    close->setToolTip(QStringLiteral("Hide panel (Ctrl+B)"));
    close->setCursor(Qt::PointingHandCursor);
    header->addWidget(close);
    layout->addLayout(header);

    pathLabel_ = new ElidedLabel(this);
    pathLabel_->setObjectName(QStringLiteral("PanelPath"));
    pathLabel_->setText(QStringLiteral("No folder open"));
    layout->addWidget(pathLabel_);

    emptyLabel_ = new QLabel(QStringLiteral("Open a project folder to let the agent explore your code."), this);
    emptyLabel_->setObjectName(QStringLiteral("CardMeta"));
    emptyLabel_->setWordWrap(true);
    emptyLabel_->setAlignment(Qt::AlignCenter);
    layout->addWidget(emptyLabel_);

    auto* openBig = new QPushButton(QStringLiteral("Open folder"), this);
    openBig->setProperty("variant", QStringLiteral("primary"));
    openBig->setCursor(Qt::PointingHandCursor);
    layout->addWidget(openBig);

    fileModel_ = new QFileSystemModel(this);
    fileModel_->setReadOnly(true);
    fileModel_->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);
    filterModel_ = new IgnoreFilterModel(this);
    filterModel_->setSourceModel(fileModel_);

    tree_ = new QTreeView(this);
    tree_->setModel(filterModel_);
    tree_->setHeaderHidden(true);
    tree_->setAnimated(true);
    tree_->setIndentation(12);
    tree_->setUniformRowHeights(true);
    tree_->setFrameShape(QFrame::NoFrame);
    tree_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tree_->setSelectionMode(QAbstractItemView::SingleSelection);
    for (int column = 1; column < fileModel_->columnCount(); ++column) {
        tree_->hideColumn(column);
    }
    layout->addWidget(tree_, 1);

    auto* activityLabel = new QLabel(QStringLiteral("AGENT ACTIVITY"), this);
    activityLabel->setObjectName(QStringLiteral("SectionLabel"));
    layout->addWidget(activityLabel);

    activity_ = new QListWidget(this);
    activity_->setMaximumHeight(170);
    activity_->setMinimumHeight(90);
    activity_->setFrameShape(QFrame::NoFrame);
    layout->addWidget(activity_);

    activityEmpty_ = new QLabel(QStringLiteral("No tool calls yet."), this);
    activityEmpty_->setObjectName(QStringLiteral("ComposerHint"));
    layout->addWidget(activityEmpty_);

    connect(open, &QPushButton::clicked, this, &WorkspacePanel::openFolderRequested);
    connect(openBig, &QPushButton::clicked, this, &WorkspacePanel::openFolderRequested);
    connect(close, &QPushButton::clicked, this, &WorkspacePanel::hideRequested);
    connect(refresh, &QPushButton::clicked, this, [this] {
        if (!root_.isEmpty()) {
            setRoot(root_);
        }
    });
    connect(tree_, &QTreeView::doubleClicked, this, [this](const QModelIndex& index) {
        const QModelIndex source = filterModel_->mapToSource(index);
        if (!fileModel_->isDir(source)) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(fileModel_->filePath(source)));
        }
    });

    updateEmptyState();
}

void WorkspacePanel::setRoot(const QString& path)
{
    root_ = path;
    if (path.isEmpty()) {
        pathLabel_->setText(QStringLiteral("No folder open"));
        tree_->setModel(filterModel_);
        updateEmptyState();
        return;
    }

    root_ = QDir(path).absolutePath();
    pathLabel_->setText(QDir::toNativeSeparators(root_));
    pathLabel_->setToolTip(QDir::toNativeSeparators(root_));
    fileModel_->setRootPath(root_);
    tree_->setRootIndex(filterModel_->mapFromSource(fileModel_->index(root_)));
    updateEmptyState();
}

void WorkspacePanel::updateEmptyState()
{
    const bool hasRoot = !root_.isEmpty();
    emptyLabel_->setVisible(!hasRoot);
    tree_->setVisible(hasRoot);
    activity_->setVisible(hasRoot);
    activityEmpty_->setVisible(hasRoot && activity_->count() == 0);
}

void WorkspacePanel::activityStarted(const QString& id, const QString& name, const QJsonObject& args)
{
    if (root_.isEmpty()) {
        return;
    }
    const QString detail = activityDetail(name, args);
    QString text = QStringLiteral("› %1").arg(name);
    if (!detail.isEmpty()) {
        text += QStringLiteral("  ·  %1").arg(detail.size() > 40 ? detail.left(40) + QStringLiteral("…") : detail);
    }
    auto* item = new QListWidgetItem(text, activity_);
    item->setForeground(QColor(0x8d, 0x8d, 0x97));
    item->setToolTip(detail);
    activityItems_.insert(id, item);
    activity_->scrollToBottom();
    activityEmpty_->setVisible(false);
}

void WorkspacePanel::activityFinished(const QString& id, bool ok)
{
    QListWidgetItem* item = activityItems_.value(id, nullptr);
    if (!item) {
        return;
    }
    QString text = item->text();
    text.replace(0, 1, ok ? QStringLiteral("✓") : QStringLiteral("✗"));
    item->setText(text);
    item->setForeground(ok ? QColor(0x9f, 0xd6, 0xa0) : QColor(0xff, 0x8a, 0x8a));
}

void WorkspacePanel::clearActivity()
{
    activityItems_.clear();
    activity_->clear();
    updateEmptyState();
}

} // namespace lulu
