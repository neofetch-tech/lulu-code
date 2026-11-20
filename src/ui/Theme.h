#pragma once

#include <QFont>
#include <QString>

class QApplication;
class QTextDocument;

namespace lulu::Theme {

/// Applies the Fusion style, the dark palette, the fonts and theme.qss.
void apply(QApplication& app);

QFont bodyFont(int pointSize = 10, int weight = QFont::Normal);
QFont displayFont(int pointSize = 18, int weight = QFont::Bold);
QFont monoFont(int pointSize = 9);

QString bodyFamily();
QString monoFamily();

/// CSS used inside QTextBrowser bubbles (markdown rendering).
QString documentStyleSheet();

} // namespace lulu::Theme
