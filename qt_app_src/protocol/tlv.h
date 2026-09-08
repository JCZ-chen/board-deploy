// Modbus-66: TLV 数据体 / 值类型 (Tag 定义见协议附录 E)
#ifndef MODBUS66_TLV_H
#define MODBUS66_TLV_H

#include <cstdint>
#include <vector>

namespace mb66 {

// 值类型 Tag
enum TlTag : uint8_t {
    TAG_BOOL       = 0x01, // 1:true 0:false
    TAG_TINY       = 0x2B, // 43  -128..127
    TAG_UTINY      = 0x20, // 32  0..255
    TAG_SHORT      = 0x21, // 33  -32768..32767
    TAG_USHORT     = 0x2D, // 45  0..65535
    TAG_INT        = 0x02, // 4 字节有符号
    TAG_UINT       = 0x23, // 35  4 字节无符号
    TAG_UINT4      = 0x02, // 别名(文档 Int 显示为 2)
    TAG_FLOAT      = 0x26, // 38  4 字节单精度
    TAG_DOUBLE     = 0x27, // 39  8 字节
    TAG_OCTET      = 0x04, // 4   可变
    TAG_STRING     = 0x05, // 5   可变(ASCII, \0 结尾)
    TAG_DATETIME   = 0x40, // 64  7 字节
    TAG_STRUCT     = 0x41, // 65  可变
};

// 单条 TLV
struct Tlv {
    uint8_t tag = 0;
    std::vector<uint8_t> value;
};

// 把一个 float 编码成线上字节序 (低字节在前), 返回 Tag+Len+Val 的 TLV 三元组
Tlv makeFloatTlv(float f);
// 把 TLV 的 value 按小端解码成 float; 非 Float 标记返回 false
bool decodeFloat(const Tlv& tlv, float& out);
// 把 USHORT/UTINY 等整型 value 解码 (小端)
bool decodeUint(const Tlv& tlv, uint64_t& out);

// 把一个 TLV 序列序列化为线上的 [Tag][Len][value...] 字节流
std::vector<uint8_t> tlvSerialize(const std::vector<Tlv>& tlvs);

} // namespace mb66

#endif
