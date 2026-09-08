// 实时趋势曲线 (QPainter 自绘, 不依赖 QtCharts)
#ifndef TRENDWIDGET_H
#define TRENDWIDGET_H

#include <QWidget>
#include <QVector>
#include <QPointF>

class TrendWidget : public QWidget {
public:
    explicit TrendWidget(QWidget* parent = nullptr);
    void pushPoint(float y);            // 追加一个点 (自动平移)
    void setRange(float lo, float hi);  // Y 轴范围
    void clear();
    int pointCount() const { return pts_.size(); }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QVector<float> pts_;
    float lo_ = 0, hi_ = 100;
    QColor lineColor_ = QColor(0, 140, 220);
};

#endif
