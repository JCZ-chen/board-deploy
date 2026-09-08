// Modbus-66 扩展协议: 帧编码 / 解码 / 分帧状态机
#ifndef MODBUS66_FRAME_H
#define MODBUS66_FRAME_H

#include <cstdint>
#include <cstddef>
#include <vector>
#include "tlv.h"

namespace mb66 {

constexpr uint8_t FUN_EXT    = 0x66;          // 扩展功能码
constexpr uint8_t FUN_ERROR  = 0xE6;          // 异常响应 (FUN 最高位置 1)

// SFUN 动作 (D5~D0)
enum Sfun : uint8_t {
    SFUN_READ            = 0x01, // 读取请求
    SFUN_WRITE           = 0x02, // 写入请求
    SFUN_BROADCAST_TIME  = 0x33, // 广播对时 (从机不应答)
    SFUN_READ_NEXT       = 0x41, // 读后续帧请求
    SFUN_READ_RESP       = 0x81, // 读取应答
    SFUN_WRITE_RESP      = 0x82, // 写入应答
    SFUN_READ_CTD_RESP   = 0xC1, // 读取有后续应答
};

// SFUN 标志位
constexpr uint8_t SF_D7_DIR   = 0x80; // 1=响应帧
constexpr uint8_t SF_D6_NEXT  = 0x40; // 1=还有后续帧

// 解析后的帧 (去掉 ADDR/FUN/CRC, LEN 已拆)
struct ParsedFrame {
    uint8_t addr;      // 从机地址
    uint8_t fun;       // 功能码 (0x66 或 0xE6 异常)
    uint8_t sfun;      // 子功能码 (含 D7/D6 标志)
    uint16_t oi;       // 第 1 个对象标识 (小端解码)
    std::vector<Tlv>  tlvs;    // 若带数据体, 解析出的 TLV 列表
    uint8_t errorCode = 0;     // 异常响应时的错误码 (无异常=0)

    bool isError() const { return fun == FUN_ERROR; }
    bool hasMore() const { return (sfun & SF_D6_NEXT) != 0; } // 分帧标志
    bool isResponse() const { return (sfun & SF_D7_DIR) != 0; }
};

// ---- 请求帧编码 ----
// 读单个对象: [ADDR][66][3][01][OI_lo][OI_hi][CRC]
std::vector<uint8_t> buildReadRequest(uint8_t addr, uint16_t oi);
// 读多个对象: 多个 OI 连发; LEN = 1(SFUN)+2*count
std::vector<uint8_t> buildReadMultiRequest(uint8_t addr, const std::vector<uint16_t>& oiList);
// 写单个对象: SFUN=02 + OI + TLV 数据体
std::vector<uint8_t> buildWriteRequest(uint8_t addr, uint16_t oi, const Tlv& data);
// 广播对时: 地址 0, SFUN=33, OI=2004(日期时间), TLV[DataTime]
std::vector<uint8_t> buildTimeBroadcast(const std::vector<uint8_t>& datetime7);
// 读后续帧请求: SFUN=41 (D6 置 1)
std::vector<uint8_t> buildReadNextRequest(uint8_t addr);

// ---- 帧解析 / 分帧状态机 ----
// 流式解析器: 处理半包/粘包/CRC 错误, 从字节流中取出完整合法帧
class FrameParser {
public:
    // 喂入新收到的字节, 每成功解析出一帧就追加进 out (自动重同步, 处理粘包)
    void feed(const uint8_t* data, size_t len, std::vector<ParsedFrame>& out);
    void reset();

private:
    std::vector<uint8_t> buf_;  // 接收缓冲
};

// 把一帧字节 (含 ADDR..CRC) 解析成 ParsedFrame; 成功返回 true
bool parseFrameBytes(const std::vector<uint8_t>& frame, ParsedFrame& out);

} // namespace mb66

#endif
