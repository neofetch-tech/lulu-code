#pragma once

#include <QLabel>
#include <QPainter>

namespace lulu {

/// Single-line label that elides in the middle instead of widening layouts.
class ElidedLabel : public QLabel
{
public:
    explicit ElidedLabel(QWidget* parent = nullptr)
        : QLabel(parent)
    {
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        setMinimumHeight(14);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setPen(palette().color(foregroundRole()));
        painter.setFont(font());
        const QString elided = fontMetrics().elidedText(text(), Qt::ElideMiddle, width());
        painter.drawText(rect(), static_cast<int>(alignment() | Qt::AlignVCenter), elided);
    }
};

} // namespace lulu
