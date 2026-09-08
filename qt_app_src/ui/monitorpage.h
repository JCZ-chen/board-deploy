// 实时监控页: 每台表数值 + 状态灯 + 趋势曲线 (无 Q_OBJECT/moc)
#ifndef MONITORPAGE_H
#define MONITORPAGE_H

#include <QWidget>
#include <QMap>
#include "comms/metersnapshot.h"
#include "ui/trendwidget.h"

class QLabel;
class QVBoxLayout;

class MonitorPage : public QWidget {
public:
    explicit MonitorPage(QWidget* parent = nullptr);
    void addMeter(uint8_t addr);
    void removeAll();
    void updateMeter(const MeterSnapshot& snap);   // 由 MainWindow 调用

private:
    struct MeterBox {
        QLabel* title;
        QLabel* levelPct;
        QLabel* levelMm;
        QLabel* statusText;
        TrendWidget* trend;
        QLabel* led;
    };
    QMap<uint8_t, MeterBox> boxes_;
    QVBoxLayout* listLayout_;
};

#endif
