// 主窗口: 连接栏 + 三个标签页, 编排 PollManager(工作线程) / HistoryStore / UI
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QThread>
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
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;
    void configure(const QString& dev, int baud, const QString& addrs, int deGpio = 22);  // 来自命令行参数

private slots:
    void onPollData(const MeterSnapshot& s);
    void onStartStop();

private:
    void buildPages();
    void applyStateToPages(const QVector<uint8_t>& addrs);
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

    QThread worker_;          // PollManager 的工作线程
    PollManager* poll_;       // moveToThread(worker_)
    HistoryStore* store_;
    QVector<uint8_t> addrs_;
};

#endif
