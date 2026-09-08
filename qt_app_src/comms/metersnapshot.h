// 仪表数据快照 (跨线程值对象, 按值传递, 无指针)
#ifndef METERSNAPSHOT_H
#define METERSNAPSHOT_H

#include <cstdint>
#include <vector>

struct MeterSnapshot {
    uint8_t addr;
    bool valid = false;        // 本次是否有有效数据
    float levelPct = 0;        // 2502 油位 %
    bool hasLevel = false;     // 2502 是否有值(如无=0xFFFFFFFF)
    float levelMm = 0;         // 2503 绝对 mm
    bool hasMm = false;
    uint16_t status = 0;       // 2501 状态字
    bool hasStatus = false;
    float hiThr = 0;           // 2506 超高阈值
    float loThr = 0;           // 2507 超低阈值
    bool hasThr = false;
};

#endif
