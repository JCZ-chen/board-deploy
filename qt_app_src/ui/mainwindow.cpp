#include "mainwindow.h"
#include "monitorpage.h"
#include "settingspage.h"
#include "historypage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QTabWidget>
#include <QTextEdit>
#include <QStatusBar>
#include <QDateTime>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle("变压器油位计上位机 (Modbus-66 / RS485)");
    setFixedSize(900, 600);   // 固定窗口, 防止子页面/表格把窗口撑大(linuxfb无窗口管理器)

    poll_ = new PollManager;
    store_ = new HistoryStore(this);

    auto* central = new QWidget;
    auto* outer = new QVBoxLayout(central);

    auto* conn = new QHBoxLayout;
    conn->addWidget(new QLabel("串口:"));
    portEdit_ = new QLineEdit("/dev/ttymxc1");
    conn->addWidget(portEdit_);
    conn->addWidget(new QLabel("波特率:"));
    baudCombo_ = new QComboBox;
    for (int b : { 2400, 4800, 9600, 19200, 38400, 57600, 115200 })
        baudCombo_->addItem(QString::number(b), b);
    baudCombo_->setCurrentText("9600");
    conn->addWidget(baudCombo_);
    conn->addWidget(new QLabel("从机地址:"));
    addrEdit_ = new QLineEdit("1");
    conn->addWidget(addrEdit_);
    startBtn_ = new QPushButton("启动");
    QObject::connect(startBtn_, &QPushButton::clicked, [this]{ onStartStop(); });
    conn->addWidget(startBtn_);
    conn->addStretch();
    outer->addLayout(conn);

    tabs_ = new QTabWidget;
    buildPages();
    outer->addWidget(tabs_);

    outer->addWidget(new QLabel("日志:"));
    log_ = new QTextEdit;
    log_->setReadOnly(true);
    log_->setMaximumHeight(90);
    outer->addWidget(log_);

    setCentralWidget(central);
    statusBar()->showMessage("未启动");
}

MainWindow::~MainWindow()
{
    poll_->stop();
    store_->flush();
    delete poll_;
}

void MainWindow::configure(const QString& dev, int baud, const QString& addrs, int deGpio)
{
    if (!dev.isEmpty()) portEdit_->setText(dev);
    if (baud > 0) { int i = baudCombo_->findData(baud); if (i >= 0) baudCombo_->setCurrentIndex(i); }
    if (!addrs.isEmpty()) addrEdit_->setText(addrs);
    poll_->setDeGpio(deGpio);
}

void MainWindow::buildPages()
{
    monitor_ = new MonitorPage;
    settings_ = new SettingsPage;
    history_ = new HistoryPage;

    tabs_->addTab(monitor_, "实时监控");
    tabs_->addTab(settings_, "参数设置");
    tabs_->addTab(history_, "历史查询");

    // PollManager 回调 -> UI (单线程同步调用)
    poll_->setCallbacks(
        [this](const MeterSnapshot& s){
            monitor_->updateMeter(s);
            if (!s.valid) return;
            HistoryRow r;
            r.ts = QDateTime::currentDateTime().toSecsSinceEpoch();
            r.addr = s.addr;
            r.hasLevel = s.hasLevel; r.levelPct = s.levelPct;
            r.hasMm = s.hasMm; r.levelMm = s.levelMm;
            r.hasStatus = s.hasStatus; r.status = s.status;
            r.hasThr = s.hasThr; r.hiThr = s.hiThr; r.loThr = s.loThr;
            store_->append(r);
        },
        [this](const QString& m){ onLog(m); },
        [this](uint8_t a, uint16_t oi, bool ok, const QString& msg){
            settings_->showWriteResult(a, oi, ok, msg);
        },
        [this](uint8_t a, int st){
            QString s = st == (int)PollManager::ST_ONLINE ? "上线"
                      : st == (int)PollManager::ST_OFFLINE ? "离线" : "未知";
            statusBar()->showMessage(QString("从机 %1 %2").arg(a).arg(s), 5000);
        });

    // 参数设置写阈值回调, 接到 PollManager
    settings_->setWriteHandler([this](uint8_t a, uint16_t oi, float v){
        poll_->writeThreshold(a, oi, v);
    });

    history_->setStore(store_);
}

void MainWindow::applyStateToPages(const QVector<uint8_t>& addrs)
{
    monitor_->removeAll();
    for (uint8_t a : addrs) monitor_->addMeter(a);
    settings_->setAddresses(addrs);
    history_->setAddresses(addrs);
}

void MainWindow::onStartStop()
{
    if (!poll_->isRunning()) {
        addrs_.clear();
        const QStringList parts = addrEdit_->text().split(',', QString::SkipEmptyParts);
        for (const QString& p : parts) {
            bool ok = false;
            int v = p.trimmed().toInt(&ok);
            if (ok && v >= 1 && v <= 247) addrs_.append((uint8_t)v);
        }
        if (addrs_.isEmpty()) addrs_ = { 1 };

        poll_->setDevice(portEdit_->text(), baudCombo_->currentData().toInt());
        poll_->setAddresses(addrs_);
        // 轮询周期默认 500ms(原 5000ms 刷新太慢);
        // 若设备应答正常 tick 仅耗 ~30ms, 不会卡 UI。
        // 想更平滑可改小到 200ms(需设备应答<50ms); 再小需把串口挪出主线程。
        int period = 500;
        poll_->setPollPeriodMs(period);
        applyStateToPages(addrs_);

        QString dbPath = "/home/oilmeter_history.db";
        QString err;
        if (!store_->open(dbPath, &err)) {
            onLog(QString("数据库打开失败: %1").arg(err));
        } else {
            onLog(QString("数据库已打开: %1").arg(dbPath));
        }

        poll_->start();
        startBtn_->setText("停止");
        statusBar()->showMessage(QString("运行中: %1 从机%2").arg(portEdit_->text()).arg(addrs_.size()));
    } else {
        poll_->stop();
        store_->flush();
        startBtn_->setText("启动");
        statusBar()->showMessage("已停止");
    }
}

void MainWindow::onLog(const QString& msg)
{
    log_->append(QDateTime::currentDateTime().toString("HH:mm:ss ") + msg);
}
