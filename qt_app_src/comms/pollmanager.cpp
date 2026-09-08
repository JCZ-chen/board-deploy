#include "pollmanager.h"
#include <QDateTime>
#include <QCoreApplication>
#include <cstring>

PollManager::PollManager()
{
    timer_ = new QTimer(nullptr);
    timer_->setSingleShot(false);
    QObject::connect(timer_, &QTimer::timeout, [this]{ tick(); });
}

PollManager::~PollManager()
{
    stop();
    delete timer_;
}

void PollManager::setCallbacks(DataCallback data, LogCallback log,
                               WriteCb wcb, StateCb scb)
{
    datacb_ = data;
    logcb_ = log;
    writecb_ = wcb;
    statecb_ = scb;
}

void PollManager::setDevice(const QString& dev, int baud) { dev_ = dev; baud_ = baud; }

void PollManager::setAddresses(const QVector<uint8_t>& addrs)
{
    addrs_ = addrs;
    snaps_.clear(); states_.clear(); failCount_.clear();
    for (int i = 0; i < addrs_.size(); ++i) {
        MeterSnapshot s; s.addr = addrs_[i];
        snaps_.push_back(s);
        states_.push_back(ST_UNKNOWN);
        failCount_.push_back(0);
    }
}

void PollManager::setPollPeriodMs(int ms) { periodMs_ = ms; }

void PollManager::writeThreshold(uint8_t addr, uint16_t oi, float value)
{
    pendingWrites_.append(WriteCmd{ addr, oi, value });
}

const MeterSnapshot& PollManager::lastSnapshot(uint8_t addr) const
{
    for (int i = 0; i < addrs_.size(); ++i)
        if (addrs_[i] == addr) return snaps_[i];
    static MeterSnapshot empty;
    return empty;
}

PollManager::State PollManager::stateOf(uint8_t addr) const
{
    for (int i = 0; i < addrs_.size(); ++i)
        if (addrs_[i] == addr) return states_[i];
    return ST_UNKNOWN;
}

void PollManager::start()
{
    if (running_) return;
    sp_.setDeGpio(deGpio_);
    if (!sp_.open(dev_.toStdString(), baud_)) {
        log(QString("无法打开串口 %1").arg(dev_));
        return;
    }
    running_ = true;
    sinceTimeSync_.start();
    log(QString("已启动: %1 @ %2").arg(dev_).arg(baud_));
    timer_->start(periodMs_);
}

void PollManager::stop()
{
    if (!running_) { sp_.close(); return; }
    running_ = false;
    timer_->stop();
    sp_.close();
    log("已停止");
}

bool PollManager::transact(SerialPort& sp, const std::vector<uint8_t>& req,
                           mb66::ParsedFrame& resp, int timeoutMs)
{
    if (!sp.writeAll(req.data(), req.size())) return false;
    uint8_t buf[512];
    QElapsedTimer t; t.start();
    parser_.reset();
    while (t.elapsed() < timeoutMs) {
        int left = timeoutMs - (int)t.elapsed();
        if (left <= 0) break;
        size_t n = sp.readSome(buf, sizeof(buf), left);
        if (n > 0) {
            std::vector<mb66::ParsedFrame> out;
            parser_.feed(buf, n, out);
            if (!out.empty()) { resp = out.front(); return true; }
        }
    }
    return false;
}

float PollManager::decodeFloatResp(const mb66::ParsedFrame& f)
{
    if (f.tlvs.empty()) return 0;
    float v = 0;
    if (!mb66::decodeFloat(f.tlvs[0], v)) return 0;
    return v;
}

void PollManager::pollOne(uint8_t addr)
{
    int idx = addrs_.indexOf(addr);
    if (idx < 0) return;
    MeterSnapshot& snap = snaps_[idx];

    // 读单个对象 2502(油位) —— 与已验证的小串口助手一致的请求/应答方式
    std::vector<uint8_t> req = mb66::buildReadRequest(addr, 0x2502);
    mb66::ParsedFrame resp;
    if (!transact(sp_, req, resp, 200)) {
        failCount_[idx]++;
        if (states_[idx] != ST_OFFLINE && failCount_[idx] >= 3) {
            states_[idx] = ST_OFFLINE;
            if (statecb_) statecb_(addr, (int)ST_OFFLINE);
            log(QString("从机 %1 离线").arg(addr));
        }
        return;
    }
    if (resp.isError()) {
        log(QString("从机 %1 异常应答 code=%2").arg(addr).arg(resp.errorCode));
        return;
    }
    if (resp.tlvs.empty()) return;

    {   float v = 0;
        if (mb66::decodeFloat(resp.tlvs[0], v)) { snap.levelPct = v; snap.hasLevel = true; }
        else snap.hasLevel = false;
    }
    snap.valid = true;
    failCount_[idx] = 0;
    if (states_[idx] != ST_ONLINE) {
        states_[idx] = ST_ONLINE;
        if (statecb_) statecb_(addr, (int)ST_ONLINE);
        log(QString("从机 %1 上线").arg(addr));
    }
    if (datacb_) datacb_(snap);
}

void PollManager::doWrites()
{
    if (pendingWrites_.isEmpty()) return;
    for (const WriteCmd& wc : pendingWrites_) {
        mb66::Tlv t = mb66::makeFloatTlv(wc.val);
        std::vector<uint8_t> req = mb66::buildWriteRequest(wc.addr, wc.oi, t);
        mb66::ParsedFrame resp;
        if (!transact(sp_, req, resp, 200)) { if (writecb_) writecb_(wc.addr, wc.oi, false, "写超时/无应答"); continue; }
        if (resp.isError()) { if (writecb_) writecb_(wc.addr, wc.oi, false, QString("写失败, 异常码 %1").arg(resp.errorCode)); continue; }
        if ((resp.sfun & 0x3F) != mb66::SFUN_WRITE_RESP) { if (writecb_) writecb_(wc.addr, wc.oi, false, "应答类型异常"); continue; }

        std::vector<uint8_t> rreq = mb66::buildReadRequest(wc.addr, wc.oi);
        mb66::ParsedFrame rr;
        if (!transact(sp_, rreq, rr, 200)) { if (writecb_) writecb_(wc.addr, wc.oi, false, "回读超时"); continue; }
        float back = decodeFloatResp(rr);
        if (back >= wc.val - 0.01 && back <= wc.val + 0.01) {
            if (writecb_) writecb_(wc.addr, wc.oi, true, "写入成功");
            for (int i = 0; i < addrs_.size(); ++i)
                if (addrs_[i] == wc.addr) {
                    if (wc.oi == 0x2506) { snaps_[i].hiThr = back; snaps_[i].hasThr = true; }
                    else if (wc.oi == 0x2507) { snaps_[i].loThr = back; snaps_[i].hasThr = true; }
                    break;
                }
        } else {
            if (writecb_) writecb_(wc.addr, wc.oi, false, QString("回读不一致: %1").arg(back));
        }
    }
    pendingWrites_.clear();
}

void PollManager::tick()
{
    if (!running_ || !sp_.isOpen()) return;
    for (uint8_t addr : addrs_) pollOne(addr);
    doWrites();

    if (sinceTimeSync_.elapsed() >= 60000) {
        QDateTime now = QDateTime::currentDateTime();
        int y = now.date().year();
        uint8_t mo = now.date().month(), d = now.date().day();
        uint8_t h = now.time().hour(), mi = now.time().minute(), s = now.time().second();
        std::vector<uint8_t> dt = { (uint8_t)(y >> 8), (uint8_t)(y & 0xFF), mo, d, h, mi, s };
        std::vector<uint8_t> t = mb66::buildTimeBroadcast(dt);
        sp_.writeAll(t.data(), t.size());
        sinceTimeSync_.restart();
        log("已广播对时");
    }
}
