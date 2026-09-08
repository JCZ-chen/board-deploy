// 采集调度器: 单线程, 内部 QTimer 驱动 (无 Q_OBJECT / 无信号槽, 规避 moc 依赖)
// 轮询/读写/命令全部在本对象所在线程同步执行; 通过 std::function 回调通知上层
#ifndef POLLMANAGER_H
#define POLLMANAGER_H

#include <QString>
#include <QTimer>
#include <QElapsedTimer>
#include <QVector>
#include <functional>
#include <vector>
#include "comms/serialport.h"
#include "comms/metersnapshot.h"
#include "protocol/frame.h"

class PollManager {
public:
    enum State { ST_UNKNOWN, ST_ONLINE, ST_OFFLINE };

    using DataCallback   = std::function<void(const MeterSnapshot&)>;
    using LogCallback    = std::function<void(const QString&)>;
    using WriteCb        = std::function<void(uint8_t, uint16_t, bool, const QString&)>;
    using StateCb        = std::function<void(uint8_t, int)>;

    PollManager();
    ~PollManager();

    void setCallbacks(DataCallback data, LogCallback log, WriteCb wcb, StateCb scb);

    void setDevice(const QString& dev, int baud);
    void setAddresses(const QVector<uint8_t>& addrs);
    void setPollPeriodMs(int ms);
    void setDeGpio(int gpioNum) { deGpio_ = gpioNum; }   // RS485 DE 方向脚(sysfs编号)

    void start();   // 打开串口 + 启动 QTimer
    void stop();
    bool isRunning() const { return running_; }

    void writeThreshold(uint8_t addr, uint16_t oi, float value);

    const MeterSnapshot& lastSnapshot(uint8_t addr) const;
    State stateOf(uint8_t addr) const;

private:
    void tick();
    bool transact(SerialPort& sp, const std::vector<uint8_t>& req,
                  mb66::ParsedFrame& resp, int timeoutMs);
    float decodeFloatResp(const mb66::ParsedFrame&);
    void pollOne(uint8_t addr);
    void doWrites();
    void log(const QString& m) { if (logcb_) logcb_(m); }

    struct WriteCmd { uint8_t addr; uint16_t oi; float val; };

    DataCallback datacb_;
    LogCallback logcb_;
    WriteCb writecb_;
    StateCb statecb_;

    QVector<WriteCmd> pendingWrites_;

    QString dev_;
    int baud_ = 9600;
    int deGpio_ = 22;   // 默认 485-1 (GPIO1_IO22) 的 DE 脚
    QVector<uint8_t> addrs_;
    int periodMs_ = 5000;
    bool running_ = false;

    SerialPort sp_;             // 常驻串口: start() 打开, stop() 关闭
    QTimer* timer_ = nullptr;
    QElapsedTimer sinceTimeSync_;
    mb66::FrameParser parser_;
    std::vector<MeterSnapshot> snaps_;
    QVector<State> states_;
    QVector<int> failCount_;
};

#endif
