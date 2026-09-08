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

    poll_ = new PollManager;          // 无 parent: 将 moveToThread
    poll_->moveToThread(&worker_);    // 串口轮询整体进工作线程
    worker_.start();
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
    connect(startBtn_, &QPushButton::clicked, this, &MainWindow::onStartStop);
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
    // 先让工作线程安全收尾: 退出事件循环再结束线程, 避免析构竞态
    QMetaObject::invokeMethod(poll_, "stop", Qt::BlockingQueuedConnection);
    worker_.quit();
    worker_.wait(2000);
    store_->flush();
    delete poll_;   // 线程已结束, 安全
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

    // PollManager(工作线程) -> UI: 跨线程 queued 信号, UI 不再被串口阻塞
    connect(poll_, &PollManager::dataReady, this, &MainWindow::onPollData, Qt::QueuedConnection);
    connect(poll_, &PollManager::logMsg, this, &MainWindow::onLog, Qt::QueuedConnection);
    connect(poll_, &PollManager::writeResult, this,
        [this](uint8_t a, uint16_t oi, bool ok, const QString& msg){
            settings_->showWriteResult(a, oi, ok, msg);
        }, Qt::QueuedConnection);
    connect(poll_, &PollManager::stateChanged, this,
        [this](uint8_t a, int st){
            QString s = st == (int)PollManager::ST_ONLINE ? "上线"
                      : st == (int)PollManager::ST_OFFLINE ? "离线" : "未知";
            statusBar()->showMessage(QString("从机 %1 %2").arg(a).arg(s), 5000);
        }, Qt::QueuedConnection);

    // UI -> PollManager(工作线程): 写阈值请求走 queued 信号
    connect(settings_, &SettingsPage::writeRequested, poll_, &PollManager::writeThreshold, Qt::QueuedConnection);

    history_->setStore(store_);
}

void MainWindow::onPollData(const MeterSnapshot& s)
{
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
    if (startBtn_->text() == "启动") {   // 用按钮文字判断状态, 避免跨线程读 poll_ 内部
        addrs_.clear();
        const QStringList parts = addrEdit_->text().split(',', QString::SkipEmptyParts);
        for (const QString& p : parts) {
            bool ok = false;
            int v = p.trimmed().toInt(&ok);
            if (ok && v >= 1 && v <= 247) addrs_.append((uint8_t)v);
        }
        if (addrs_.isEmpty()) addrs_ = { 1 };

        // setter 直接调用: 这些只改成员变量, 未启动状态下无并发风险
        poll_->setDevice(portEdit_->text(), baudCombo_->currentData().toInt());
        poll_->setAddresses(addrs_);
        poll_->setPollPeriodMs(500);
        applyStateToPages(addrs_);

        QString dbPath = "/home/oilmeter_history.db";
        QString err;
        if (!store_->open(dbPath, &err)) {
            onLog(QString("数据库打开失败: %1").arg(err));
        } else {
            onLog(QString("数据库已打开: %1").arg(dbPath));
        }

        // start 里要打开串口(可能阻塞几十ms), 投递到工作线程执行
        QMetaObject::invokeMethod(poll_, "start", Qt::QueuedConnection);
        startBtn_->setText("停止");
        statusBar()->showMessage(QString("运行中: %1 从机%2").arg(portEdit_->text()).arg(addrs_.size()));
    } else {
        QMetaObject::invokeMethod(poll_, "stop", Qt::BlockingQueuedConnection);
        store_->flush();
        startBtn_->setText("启动");
        statusBar()->showMessage("已停止");
    }
}

void MainWindow::onLog(const QString& msg)
{
    log_->append(QDateTime::currentDateTime().toString("HH:mm:ss ") + msg);
}
