#include "frame.h"
#include "crc16.h"

namespace mb66 {

static void appendCrc(std::vector<uint8_t>& f)
{
    uint16_t crc = crc16(f.data(), f.size());
    f.push_back(static_cast<uint8_t>(crc & 0xFF));       // CRC 低字节在前
    f.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
}

static std::vector<uint8_t> baseRequest(uint8_t addr, uint8_t sfun)
{
    std::vector<uint8_t> f;
    f.push_back(addr);
    f.push_back(FUN_EXT);
    return f;
}

// 在 "ADDR FUN" 之后追加 LEN 并回填头, 再附加 CRC
static std::vector<uint8_t> finalize(std::vector<uint8_t> f, const std::vector<uint8_t>& apdu)
{
    // LEN = APDU 长度 ({SFUN+OI+DATA})
    if (apdu.size() > 255) return {};  // 超长需分帧, 本工具暂不自动分帧
    f.push_back(static_cast<uint8_t>(apdu.size()));
    f.insert(f.end(), apdu.begin(), apdu.end());
    appendCrc(f);
    return f;
}

std::vector<uint8_t> buildReadRequest(uint8_t addr, uint16_t oi)
{
    std::vector<uint8_t> apdu;
    apdu.push_back(SFUN_READ);
    apdu.push_back(static_cast<uint8_t>((oi >> 8) & 0xFF));  // OI 高字节在前(大端)
    apdu.push_back(static_cast<uint8_t>(oi & 0xFF));
    return finalize(baseRequest(addr, SFUN_READ), apdu);
}

std::vector<uint8_t> buildReadMultiRequest(uint8_t addr, const std::vector<uint16_t>& oiList)
{
    if (oiList.empty()) return {};
    std::vector<uint8_t> apdu;
    apdu.push_back(SFUN_READ);
    for (uint16_t oi : oiList) {
        apdu.push_back(static_cast<uint8_t>((oi >> 8) & 0xFF));  // 高在前
        apdu.push_back(static_cast<uint8_t>(oi & 0xFF));
    }
    return finalize(baseRequest(addr, SFUN_READ), apdu);
}

std::vector<uint8_t> buildWriteRequest(uint8_t addr, uint16_t oi, const Tlv& data)
{
    std::vector<uint8_t> apdu;
    apdu.push_back(SFUN_WRITE);
    apdu.push_back(static_cast<uint8_t>((oi >> 8) & 0xFF));  // 高在前
    apdu.push_back(static_cast<uint8_t>(oi & 0xFF));
    apdu.push_back(data.tag);
    apdu.push_back(static_cast<uint8_t>(data.value.size()));
    apdu.insert(apdu.end(), data.value.begin(), data.value.end());
    return finalize(baseRequest(addr, SFUN_WRITE), apdu);
}

std::vector<uint8_t> buildTimeBroadcast(const std::vector<uint8_t>& datetime7)
{
    if (datetime7.size() != 7) return {};
    std::vector<uint8_t> apdu;
    apdu.push_back(SFUN_BROADCAST_TIME);   // 0x33; D7=0,D6=0
    apdu.push_back(0x20);                  // OI 高字节 (2004)
    apdu.push_back(0x04);                  // OI 低字节
    apdu.push_back(TAG_DATETIME);          // Tag 64
    apdu.push_back(0x07);                  // Len 7
    apdu.insert(apdu.end(), datetime7.begin(), datetime7.end());
    return finalize(baseRequest(0, SFUN_BROADCAST_TIME), apdu);
}

std::vector<uint8_t> buildReadNextRequest(uint8_t addr)
{
    std::vector<uint8_t> apdu;
    apdu.push_back(SFUN_READ_NEXT);   // 0x41 (D6=1)
    return finalize(baseRequest(addr, SFUN_READ_NEXT), apdu);
}

bool parseFrameBytes(const std::vector<uint8_t>& f, ParsedFrame& out)
{
    // f: [ADDR][FUN][LEN][APDU...][CRCL][CRCH]
    const size_t n = f.size();
    if (n < 7) return false;  // 至少: addr+fun+len+sfun+crc(2) => 6, 但 OI 也需 >=7; 保守 7
    uint8_t len = f[2];
    if (n != 3 + len + 2) return false;   // 长度不符

    // 校验 CRC: 对 [0 .. n-3) 重算, 与末 2 字节比
    uint16_t crcCalc = crc16(f.data(), n - 2);
    uint16_t crcRecv = static_cast<uint16_t>(f[n-2]) | (static_cast<uint16_t>(f[n-1]) << 8);
    if (crcCalc != crcRecv) return false;

    out.addr = f[0];
    out.fun  = f[1];
    out.sfun = f[3];  // APDU[0] = SFUN

    // 数据区从 f[4] 开始; OI 在线上为高字节在前(大端)
    size_t p = 4;
    const size_t end = 3 + len;  // 数据区结束(不含 2 CRC)
    if (p + 2 <= end) {
        out.oi = static_cast<uint16_t>(f[p] << 8) | f[p+1];
        p += 2;
    }

    if (out.isError()) {
        // 异常响应: 数据区第 1 字节(apdu[1])是错误码
        if (p < end) out.errorCode = f[p];
        return true;
    }

    // 解析 TLV 序列
    while (p + 2 <= end) {
        uint8_t tag = f[p];
        uint8_t tlen = f[p+1];
        p += 2;
        if (p + tlen > end) return false;  // TLV 越界
        Tlv t;
        t.tag = tag;
        t.value.assign(f.begin() + p, f.begin() + p + tlen);
        out.tlvs.push_back(t);
        p += tlen;
    }
    return p == end;  // 必须恰好消费完
}

void FrameParser::feed(const uint8_t* data, size_t len, std::vector<ParsedFrame>& out)
{
    buf_.insert(buf_.end(), data, data + len);
    size_t p = 0;
    while (buf_.size() - p >= 3) {
        // 只接受 FUN 合法(0x66 或 0xE6)的帧头, 否则立即丢弃首字节重同步,
        // 避免把垃圾字节当成 LEN 无限等待
        uint8_t fun = buf_[p + 1];
        if (fun != FUN_EXT && fun != FUN_ERROR) {
            p += 1;
            continue;
        }
        uint8_t lenField = buf_[p + 2];
        size_t total = 3 + lenField + 2;  // addr+fun+len + apdu + 2 crc
        if (buf_.size() - p < total) {
            // 半包: 收满为止
            break;
        }
        std::vector<uint8_t> frame(buf_.begin() + p, buf_.begin() + p + total);
        ParsedFrame pf;
        if (parseFrameBytes(frame, pf)) {
            out.push_back(pf);
            p += total;
        } else {
            // CRC 错或伪帧头: 丢弃首字节, 逐字节重同步
            p += 1;
        }
    }
    // 丢弃已消费部分; 保留未满的一帧
    if (p > 0) buf_.erase(buf_.begin(), buf_.begin() + p);
}

void FrameParser::reset() { buf_.clear(); }

} // namespace mb66
