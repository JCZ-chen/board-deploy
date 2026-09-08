#include "trendwidget.h"
#include <QPainter>
#include <QPaintEvent>

TrendWidget::TrendWidget(QWidget* parent) : QWidget(parent)
{
    setMinimumHeight(160);
}

void TrendWidget::pushPoint(float y)
{
    pts_.append(y);
    const int cap = 240;              // 最多保留 240 点
    if (pts_.size() > cap) pts_.remove(0, pts_.size() - cap);
    update();   // Qt 会把多次 update() 合并成一次 paintEvent, 无需手工节流
}

void TrendWidget::setRange(float lo, float hi)
{
    lo_ = lo; hi_ = hi;
    update();
}

void TrendWidget::clear() { pts_.clear(); update(); }

void TrendWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), Qt::black);
    p.setPen(QColor(40, 40, 40));
    // 网格线
    for (int i = 0; i <= 4; ++i) {
        int y = height() * i / 4;
        p.drawLine(0, y, width(), y);
    }
    if (pts_.size() < 2) {
        p.setPen(Qt::gray);
        p.drawText(rect(), Qt::AlignCenter, "等待数据...");
        return;
    }
    float span = (hi_ - lo_);
    if (span <= 0) span = 1;
    QPainterPath path;
    int n = pts_.size();
    for (int i = 0; i < n; ++i) {
        float v = pts_[i];
        double x = (double)i / (n - 1) * width();
        double y = height() - (v - lo_) / span * height();
        if (i == 0) path.moveTo(x, y);
        else path.lineTo(x, y);
    }
    p.setPen(QPen(lineColor_, 2));
    p.drawPath(path);
}
