#include "oi_table.h"

namespace mb66 {

const std::vector<OiField> kOiTable2500 = {
    { 2500, "数据结构体",      TAG_STRUCT,   122, "",       A_READ },
    { 2501, "传感器状态",      TAG_OCTET,    2,   "",       A_READ },
    { 2502, "油位",            TAG_FLOAT,    4,   "%",      A_READ },
    { 2503, "油位绝对值",      TAG_FLOAT,    4,   "mm",     A_READ },
    { 2504, "油位",            TAG_FLOAT,    4,   "无单位", A_READ },
    { 2505, "油位",            TAG_FLOAT,    4,   "自定义", A_READ },
    { 2506, "油位超高阈值",    TAG_FLOAT,    4,   "%",      A_RW },
    { 2507, "油位超低阈值",    TAG_FLOAT,    4,   "%",      A_RW },
};

const OiField* findOi(uint16_t oi)
{
    for (const OiField& f : kOiTable2500)
        if (f.oi == oi) return &f;
    return nullptr;
}

bool isWritable(uint16_t oi)
{
    const OiField* f = findOi(oi);
    return f && (f->access & A_WRITE);
}

bool validateTlv(const OiField& f, const Tlv& tlv)
{
    return tlv.tag == f.tag && tlv.value.size() == f.len;
}

} // namespace mb66
