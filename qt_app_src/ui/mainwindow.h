// 主窗口: 连接栏 + 三个标签页, 编排 PollManager / HistoryStore / UI (无 Q_OBJECT/moc)
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVector>
#include <cstdint>
#include "comms/pollmanager.h"
#include "storage/historystore.h"

class MonitorPage;
class SettingsPage;
class HistoryPage;
class QLineEdit;
class QComboBox;
class QPushButton;
class QTabWidget;
class QTextEdit;

class MainWindow : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();
    void configure(const QString& dev, int baud, const QString& addrs, int deGpio = 22);  // 来自命令行参数

private:
    void buildPages();
    void applyStateToPages(const QVector<uint8_t>& addrs);
    void onStartStop();
    void onLog(const QString& msg);

    QTabWidget* tabs_;
    MonitorPage* monitor_;
    SettingsPage* settings_;
    HistoryPage* history_;
    QLineEdit* portEdit_;
    QLineEdit* addrEdit_;
    QComboBox* baudCombo_;
    QPushButton* startBtn_;
    QTextEdit* log_;

    PollManager* poll_;
    HistoryStore* store_;
    QVector<uint8_t> addrs_;
};

#endif
