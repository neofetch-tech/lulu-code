#include "ui/Theme.h"

#include <QApplication>
#include <QFile>
#include <QFontDatabase>
#include <QLoggingCategory>
#include <QPalette>
#include <QStyleFactory>

namespace lulu::Theme {

namespace {

QString pickFamily(const QStringList& candidates, const QString& fallback)
{
    const QStringList installed = QFontDatabase::families();
    for (const QString& candidate : candidates) {
        for (const QString& family : installed) {
            if (family.compare(candidate, Qt::CaseInsensitive) == 0) {
                return family;
            }
        }
    }
    return fallback;
}

QString g_body;
QString g_mono;

void resolveFonts()
{
    if (!g_body.isEmpty()) {
        return;
    }
    g_body = pickFamily({QStringLiteral("Inter"), QStringLiteral("SF Pro Text"),
                         QStringLiteral("Segoe UI Variable Text"), QStringLiteral("Segoe UI"),
                         QStringLiteral("Noto Sans"), QStringLiteral("DejaVu Sans")},
                        QStringLiteral("Sans Serif"));
    g_mono = pickFamily({QStringLiteral("JetBrains Mono"), QStringLiteral("Cascadia Mono"),
                         QStringLiteral("Cascadia Code"), QStringLiteral("SF Mono"),
                         QStringLiteral("Consolas"), QStringLiteral("DejaVu Sans Mono"),
                         QStringLiteral("Liberation Mono")},
                        QStringLiteral("Monospace"));
}

} // namespace

QString bodyFamily()
{
    resolveFonts();
    return g_body;
}

QString monoFamily()
{
    resolveFonts();
    return g_mono;
}

QFont bodyFont(int pointSize, int weight)
{
    QFont font(bodyFamily(), pointSize);
    font.setWeight(static_cast<QFont::Weight>(weight));
    font.setStyleStrategy(QFont::PreferAntialias);
    return font;
}

QFont displayFont(int pointSize, int weight)
{
    return bodyFont(pointSize, weight);
}

QFont monoFont(int pointSize)
{
    QFont font(monoFamily(), pointSize);
    font.setStyleStrategy(QFont::PreferAntialias);
    return font;
}

QString documentStyleSheet()
{
    resolveFonts();
    return QStringLiteral(
               "body { color: #e8e8ee; font-family: '%1'; font-size: 10.5pt; line-height: 1.5; }"
               "p { margin-top: 0; margin-bottom: 7px; }"
               "a { color: #ffffff; text-decoration: underline; }"
               "code { font-family: '%2'; font-size: 9.5pt; color: #f0f0f5; background-color: #15151a; }"
               "pre { font-family: '%2'; font-size: 9pt; background-color: #0c0c0f; color: #e9e9ef; }"
               "h1, h2, h3, h4 { color: #ffffff; font-family: '%1'; }"
               "blockquote { color: #a2a2ad; border-left: 2px solid #33333d; margin-left: 4px; padding-left: 10px; }"
               "li { margin-bottom: 3px; }"
               "hr { border: none; border-top: 1px solid #2a2a32; }")
        .arg(bodyFamily(), monoFamily());
}

void apply(QApplication& app)
{
    resolveFonts();

    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    app.setFont(bodyFont(10));

    QPalette palette;
    palette.setColor(QPalette::Window, QColor(0x0a, 0x0a, 0x0c));
    palette.setColor(QPalette::WindowText, QColor(0xe7, 0xe7, 0xec));
    palette.setColor(QPalette::Base, QColor(0x10, 0x10, 0x13));
    palette.setColor(QPalette::AlternateBase, QColor(0x16, 0x16, 0x1a));
    palette.setColor(QPalette::ToolTipBase, QColor(0x1c, 0x1c, 0x21));
    palette.setColor(QPalette::ToolTipText, QColor(0xf2, 0xf2, 0xf5));
    palette.setColor(QPalette::Text, QColor(0xe7, 0xe7, 0xec));
    palette.setColor(QPalette::Button, QColor(0x13, 0x13, 0x17));
    palette.setColor(QPalette::ButtonText, QColor(0xe7, 0xe7, 0xec));
    palette.setColor(QPalette::BrightText, QColor(0xffffff));
    palette.setColor(QPalette::Link, QColor(0xffffff));
    palette.setColor(QPalette::Highlight, QColor(0x33, 0x33, 0x3d));
    palette.setColor(QPalette::HighlightedText, QColor(0xffffff));
    palette.setColor(QPalette::PlaceholderText, QColor(0x5e, 0x5e, 0x68));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(0x5c, 0x5c, 0x66));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x5c, 0x5c, 0x66));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x5c, 0x5c, 0x66));
    app.setPalette(palette);

    QFile styleFile(QStringLiteral(":/theme.qss"));
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        app.setStyleSheet(QString::fromUtf8(styleFile.readAll()));
    } else {
        qWarning("Lulu Code: theme.qss is missing from the resources.");
    }
}

} // namespace lulu::Theme
