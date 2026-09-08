// 采集调度器 v2: 串口轮询工作线程化
// PollManager 本体(moveToThread 到工作线程), 内部 QTimer 在工作线程触发,
// 串口同步阻塞只发生在工作线程 —— UI 主线程不再被串口卡住。
// 与 UI 的数据交换全部走 Qt 信号槽(跨线程自动 queued):
//   工作线程 -> UI: dataReady / logMsg / writeResult / stateChanged
//   UI -> 工作线程: writeRequested (queued)
#ifndef POLLMANAGER_H
#define POLLMANAGER_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QElapsedTimer>
#include <QVector>
#include <vector>
#include "comms/serialport.h"
#include "comms/metersnapshot.h"
#include "protocol/frame.h"

class PollManager : public QObject {
    Q_OBJECT
public:
    enum State { ST_UNKNOWN, ST_ONLINE, ST_OFFLINE };

    explicit PollManager(QObject* parent = nullptr);
    ~PollManager() override;

    // 这些 setter 必须在 start() 之前、且在对象尚未 moveToThread 时调用(主线程)
    void setDevice(const QString& dev, int baud);
    void setAddresses(const QVector<uint8_t>& addrs);
    void setPollPeriodMs(int ms);
    void setDeGpio(int gpioNum) { deGpio_ = gpioNum; }

public slots:
    void start();   // 在工作线程执行: 打开串口 + 启动 QTimer
    void stop();
    void writeThreshold(uint8_t addr, uint16_t oi, float value);  // UI 经信号投递

signals:
    void dataReady(const MeterSnapshot& snap);
    void logMsg(const QString& msg);
    void writeResult(uint8_t addr, uint16_t oi, bool ok, const QString& msg);
    void stateChanged(uint8_t addr, int st);

private:
    void tick();
    bool transact(SerialPort& sp, const std::vector<uint8_t>& req,
                  mb66::ParsedFrame& resp, int timeoutMs);
    float decodeFloatResp(const mb66::ParsedFrame&);
    void pollOne(uint8_t addr);
    void doWrites();
    void log(const QString& m) { emit logMsg(m); }

    struct WriteCmd { uint8_t addr; uint16_t oi; float val; };

    QVector<WriteCmd> pendingWrites_;

    QString dev_;
    int baud_ = 9600;
    int deGpio_ = 22;   // 默认 485-1 (GPIO1_IO22) 的 DE 脚
    QVector<uint8_t> addrs_;
    int periodMs_ = 500;
    bool running_ = false;

    SerialPort sp_;             // 常驻串口: start() 打开, stop() 关闭(工作线程内)
    QTimer* timer_ = nullptr;
    QElapsedTimer sinceTimeSync_;
    mb66::FrameParser parser_;
    std::vector<MeterSnapshot> snaps_;
    QVector<State> states_;
    QVector<int> failCount_;
};

#endif
