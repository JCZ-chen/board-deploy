// Modbus-66 扩展协议: CRC-16 (初值 0xFFFF, 多项式 0xA001, 标准 Modbus)
#ifndef MODBUS66_CRC16_H
#define MODBUS66_CRC16_H

#include <cstdint>
#include <cstddef>

namespace mb66 {

// 计算整段数据的 CRC-16
uint16_t crc16(const uint8_t* data, size_t len);

} // namespace mb66

#endif
