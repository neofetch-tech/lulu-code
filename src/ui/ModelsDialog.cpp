#include "ui/ModelsDialog.h"

#include "app/AppPaths.h"
#include "ui/ElidedLabel.h"

#include <QDir>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QHash>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QVBoxLayout>

namespace lulu {


namespace {

QPixmap providerLogo(const QString& path)
{
    static QHash<QString, QPixmap> cache;
    const auto found = cache.constFind(path);
    if (found != cache.cend()) {
        return found.value();
    }

    QSvgRenderer renderer(path);
    if (!renderer.isValid()) {
        return {};
    }
    QImage image(48, 48, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    renderer.render(&painter);
    QPixmap pixmap = QPixmap::fromImage(image);
    pixmap.setDevicePixelRatio(2.0);
    cache.insert(path, pixmap);
    return pixmap;
}

QLabel* badge(const QString& text, bool active, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setObjectName(active ? QStringLiteral("BadgeActive") : QStringLiteral("Badge"));
    label->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    return label;
}

QString percentText(qint64 received, qint64 total)
{
    if (total <= 0) {
        return QStringLiteral("Downloading…");
    }
    return QStringLiteral("Downloading · %1%").arg(static_cast<int>((received * 100) / total));
}

} // namespace

ModelsDialog::ModelsDialog(ModelStore* store, const QString& activeModelId, QWidget* parent)
    : QDialog(parent)
    , store_(store)
    , activeId_(activeModelId)
{
    setWindowTitle(QStringLiteral("Models"));
    setModal(true);
    setMinimumSize(760, 600);
    resize(800, 720);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 16);
    layout->setSpacing(10);

    auto* title = new QLabel(QStringLiteral("Models"), this);
    title->setObjectName(QStringLiteral("DialogTitle"));
    layout->addWidget(title);

    auto* subtitle = new QLabel(
        QStringLiteral("GGUF models for llama.cpp. Everything is downloaded once, then runs offline."),
        this);
    subtitle->setObjectName(QStringLiteral("DialogSubtitle"));
    layout->addWidget(subtitle);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setObjectName(QStringLiteral("ChatScroll"));

    auto* content = new QWidget(scroll);
    contentLayout_ = new QVBoxLayout(content);
    contentLayout_->setContentsMargins(0, 6, 4, 6);
    contentLayout_->setSpacing(12);
    contentLayout_->addStretch(1);
    scroll->setWidget(content);
    layout->addWidget(scroll, 1);

    auto* footer = new QHBoxLayout;
    auto* storage = new QLabel(QStringLiteral("Stored in %1").arg(AppPaths::displayDataDir()), this);
    storage->setObjectName(QStringLiteral("ComposerHint"));
    storage->setToolTip(QDir::toNativeSeparators(AppPaths::modelsDir()));
    footer->addWidget(storage);
    footer->addStretch();

    auto* addLocal = new QPushButton(QStringLiteral("Add local GGUF…"), this);
    addLocal->setCursor(Qt::PointingHandCursor);
    footer->addWidget(addLocal);

    auto* close = new QPushButton(QStringLiteral("Close"), this);
    close->setProperty("variant", QStringLiteral("primary"));
    close->setCursor(Qt::PointingHandCursor);
    footer->addWidget(close);
    layout->addLayout(footer);

    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    connect(addLocal, &QPushButton::clicked, this, &ModelsDialog::addLocalFile);

    connect(store_, &ModelStore::changed, this, &ModelsDialog::rebuild);
    connect(store_, &ModelStore::downloadProgress, this, [this](const QString& id, qint64 received, qint64 total) {
        const auto it = cards_.constFind(id);
        if (it == cards_.cend() || !it->progress) {
            return;
        }
        it->progress->setVisible(true);
        it->progress->setMaximum(100);
        it->progress->setValue(total > 0 ? static_cast<int>((received * 100) / total) : 0);
        if (it->status) {
            it->status->setText(percentText(received, total));
        }
    });
    connect(store_, &ModelStore::downloadFailed, this, [this](const QString& id, const QString& error) {
        QMessageBox::warning(this, QStringLiteral("Download failed"), error);
        rebuild();
    });
    connect(store_, &ModelStore::downloadFinished, this, [this](const QString& id) {
        Q_UNUSED(id);
        rebuild();
    });

    rebuild();
}

void ModelsDialog::setActiveModel(const QString& id)
{
    activeId_ = id;
    rebuild();
}

void ModelsDialog::rebuild()
{
    while (contentLayout_->count() > 1) {
        QLayoutItem* item = contentLayout_->takeAt(0);
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
    cards_.clear();

    for (const ModelInfo& info : ModelCatalog::all()) {
        contentLayout_->insertWidget(contentLayout_->count() - 1, buildCatalogCard(info));
    }

    QVector<InstalledModel> custom;
    for (const InstalledModel& model : store_->installed()) {
        if (!ModelCatalog::find(model.id).has_value()) {
            custom.append(model);
        }
    }

    if (!custom.isEmpty()) {
        auto* label = new QLabel(QStringLiteral("YOUR GGUF FILES"), this);
        label->setProperty("role", QStringLiteral("section"));
        contentLayout_->insertWidget(contentLayout_->count() - 1, label);
        for (const InstalledModel& model : std::as_const(custom)) {
            contentLayout_->insertWidget(contentLayout_->count() - 1, buildLocalCard(model));
        }
    }
}

QWidget* ModelsDialog::buildCatalogCard(const ModelInfo& info)
{
    const bool installed = store_->isInstalled(info.id);
    const bool active = installed && info.id == activeId_;
    const bool downloading = store_->isDownloading(info.id);

    auto* card = new QFrame(this);
    card->setObjectName(QStringLiteral("ModelCard"));
    card->setProperty("active", active);
    card->setProperty("installed", installed);

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 13, 16, 13);
    layout->setSpacing(6);

    auto* top = new QHBoxLayout;
    top->setSpacing(7);

    auto* name = new QLabel(info.name, card);
    name->setObjectName(QStringLiteral("ModelName"));

    // Show the logo that belongs to this model family.
    auto* modelLogo = new QLabel(card);
    QString logoPath;
    if (info.family.contains(QStringLiteral("qwen"), Qt::CaseInsensitive)) {
        logoPath = QStringLiteral(":/icons/qwen.svg");
    } else if (info.family.contains(QStringLiteral("deepseek"), Qt::CaseInsensitive)) {
        logoPath = QStringLiteral(":/icons/deepseek.svg");
    } else if (info.family.contains(QStringLiteral("llama"), Qt::CaseInsensitive)) {
        logoPath = QStringLiteral(":/icons/llama.svg");
    } else if (info.family.contains(QStringLiteral("glm"), Qt::CaseInsensitive)) {
        logoPath = QStringLiteral(":/icons/glm.svg");
    }
    const QPixmap logoPixmap = logoPath.isEmpty() ? QPixmap() : providerLogo(logoPath);
    modelLogo->setFixedSize(22, 22);
    modelLogo->setAlignment(Qt::AlignCenter);
    if (logoPixmap.isNull()) {
        modelLogo->hide();
    } else {
        modelLogo->setPixmap(logoPixmap);
    }

    top->addWidget(modelLogo);
    top->addWidget(name);

    top->addWidget(badge(info.family, false, card));
    if (info.recommended) {
        top->addWidget(badge(QStringLiteral("RECOMMENDED"), true, card));
    }
    if (active) {
        top->addWidget(badge(QStringLiteral("ACTIVE"), true, card));
    }
    top->addStretch();
    layout->addLayout(top);

    auto* description = new QLabel(info.description, card);
    description->setObjectName(QStringLiteral("ModelDesc"));
    description->setWordWrap(true);
    layout->addWidget(description);

    auto* meta = new QLabel(
        QStringLiteral("%1 · %2 · %3k context · %4")
            .arg(info.quant, info.sizeLabel())
            .arg(info.trainedContext / 1024)
            .arg(info.license),
        card);
    meta->setObjectName(QStringLiteral("ToolMeta"));
    layout->addWidget(meta);

    auto* bottom = new QHBoxLayout;
    bottom->setSpacing(7);

    auto* status = new QLabel(downloading ? QStringLiteral("Downloading…")
                                          : installed ? QStringLiteral("Installed")
                                                      : info.repoId,
                              card);
    status->setObjectName(QStringLiteral("MonoSmall"));
    status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    bottom->addWidget(status);

    auto* progress = new QProgressBar(card);
    progress->setRange(0, 100);
    progress->setValue(0);
    progress->setTextVisible(false);
    progress->setFixedWidth(150);
    progress->setVisible(downloading);
    bottom->addWidget(progress);
    bottom->addStretch();

    CardWidgets widgets;
    widgets.progress = progress;
    widgets.status = status;

    auto* primary = new QPushButton(card);
    primary->setCursor(Qt::PointingHandCursor);
    if (downloading) {
        primary->setText(QStringLiteral("Cancel"));
        connect(primary, &QPushButton::clicked, this, [this, id = info.id] {
            store_->cancelDownload(id);
            rebuild();
        });
    } else if (installed) {
        primary->setText(active ? QStringLiteral("✓ Active") : QStringLiteral("Use"));
        primary->setProperty("variant", QStringLiteral("primary"));
        primary->setEnabled(!active);
        connect(primary, &QPushButton::clicked, this, [this, id = info.id] {
            emit loadRequested(id);
            accept();
        });
    } else {
        primary->setText(QStringLiteral("Download"));
        primary->setProperty("variant", QStringLiteral("primary"));
        connect(primary, &QPushButton::clicked, this, [this, id = info.id, status] {
            status->setText(percentText(0, 1));
            store_->download(id);
        });
    }
    bottom->addWidget(primary);
    widgets.primary = primary;

    if (installed && !active) {
        auto* remove = new QPushButton(QStringLiteral("Remove"), card);
        remove->setProperty("variant", QStringLiteral("ghost"));
        remove->setCursor(Qt::PointingHandCursor);
        connect(remove, &QPushButton::clicked, this, [this, id = info.id, name = info.name] {
            const auto answer = QMessageBox::question(
                this, QStringLiteral("Remove model"),
                QStringLiteral("Delete %1 from disk?").arg(name),
                QMessageBox::No | QMessageBox::Yes, QMessageBox::No);
            if (answer == QMessageBox::Yes) {
                store_->remove(id, true);
            }
        });
        bottom->addWidget(remove);
        widgets.remove = remove;
    }

    layout->addLayout(bottom);
    cards_.insert(info.id, widgets);
    return card;
}

QWidget* ModelsDialog::buildLocalCard(const InstalledModel& model)
{
    auto* card = new QFrame(this);
    card->setObjectName(QStringLiteral("ModelCard"));
    card->setProperty("installed", true);

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 13, 16, 13);
    layout->setSpacing(6);

    auto* top = new QHBoxLayout;
    top->setSpacing(7);
    auto* name = new QLabel(model.name, card);
    name->setObjectName(QStringLiteral("ModelName"));
    top->addWidget(name);
    top->addWidget(badge(model.family, false, card));
    if (model.id == activeId_) {
        top->addWidget(badge(QStringLiteral("ACTIVE"), true, card));
    }
    top->addStretch();
    layout->addLayout(top);

    auto* path = new ElidedLabel(card);
    path->setObjectName(QStringLiteral("MonoSmall"));
    path->setText(QDir::toNativeSeparators(model.path));
    path->setToolTip(QDir::toNativeSeparators(model.path));
    layout->addWidget(path);

    auto* bottom = new QHBoxLayout;
    bottom->setSpacing(7);
    auto* meta = new QLabel(model.sizeLabel(), card);
    meta->setObjectName(QStringLiteral("ToolMeta"));
    bottom->addWidget(meta);
    bottom->addStretch();

    auto* use = new QPushButton(model.id == activeId_ ? QStringLiteral("✓ Active") : QStringLiteral("Use"), card);
    use->setProperty("variant", QStringLiteral("primary"));
    use->setEnabled(model.id != activeId_);
    use->setCursor(Qt::PointingHandCursor);
    connect(use, &QPushButton::clicked, this, [this, id = model.id] {
        emit loadRequested(id);
        accept();
    });
    bottom->addWidget(use);

    auto* remove = new QPushButton(QStringLiteral("Remove"), card);
    remove->setProperty("variant", QStringLiteral("ghost"));
    remove->setCursor(Qt::PointingHandCursor);
    connect(remove, &QPushButton::clicked, this, [this, id = model.id, name = model.name] {
        const auto answer = QMessageBox::question(
            this, QStringLiteral("Remove model"),
            QStringLiteral("Forget %1? The file itself is kept because it lives outside Lulu's model folder.").arg(name),
            QMessageBox::No | QMessageBox::Yes, QMessageBox::No);
        if (answer == QMessageBox::Yes) {
            store_->remove(id, false);
        }
    });
    bottom->addWidget(remove);
    layout->addLayout(bottom);
    return card;
}

void ModelsDialog::addLocalFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Add a GGUF model"), QDir::homePath(),
        QStringLiteral("GGUF models (*.gguf)"));
    if (path.isEmpty()) {
        return;
    }
    QString error;
    if (!store_->registerLocalFile(path, &error)) {
        QMessageBox::warning(this, QStringLiteral("Could not add model"), error);
    }
}

} // namespace lulu