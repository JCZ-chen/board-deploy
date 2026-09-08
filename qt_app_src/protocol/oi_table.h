// 2500 系列(变压器数字化油位计) 对象映射表
#ifndef MODBUS66_OITABLE_H
#define MODBUS66_OITABLE_H

#include <cstdint>
#include <string>
#include <vector>
#include "tlv.h"

namespace mb66 {

// 字段读写权限
enum Access { A_READ = 1, A_WRITE = 2, A_RW = 3 };

struct OiField {
    uint16_t oi;
    std::string name;
    TlTag tag;          // 期望值类型
    uint8_t len;        // 期望字节数
    std::string unit;
    uint8_t access;     // Access 位
};

// 2500 系列字段表 (定义在 oi_table.cpp)
extern const std::vector<OiField> kOiTable2500;

// 查找字段描述; 找不到返回 nullptr
const OiField* findOi(uint16_t oi);

// 该 OI 是否可写
bool isWritable(uint16_t oi);

// 判断一个 TLV 与期望字段是否匹配(类型/长度)
bool validateTlv(const OiField& f, const Tlv& tlv);

} // namespace mb66

#endif
