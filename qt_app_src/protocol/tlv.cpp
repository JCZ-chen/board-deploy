#include "tlv.h"
#include <cstring>
#include <cmath>

namespace mb66 {

Tlv makeFloatTlv(float f)
{
    Tlv t;
    t.tag = TAG_FLOAT;
    // 小端: 内存里 float 在 x86/ARM 都是小端, 直接拷 4 字节
    std::vector<uint8_t>& v = t.value;
    v.resize(4);
    std::memcpy(v.data(), &f, 4);
    return t;
}

bool decodeFloat(const Tlv& tlv, float& out)
{
    if (tlv.tag != TAG_FLOAT || tlv.value.size() != 4)
        return false;
    std::memcpy(&out, tlv.value.data(), 4);
    return true;
}

bool decodeUint(const Tlv& tlv, uint64_t& out)
{
    // 支持 UTINY(1)/USHORT(2)/UINT(4) 等小端整型
    if (tlv.value.empty() || tlv.value.size() > 8)
        return false;
    uint64_t v = 0;
    for (size_t i = 0; i < tlv.value.size(); ++i)
        v |= static_cast<uint64_t>(tlv.value[i]) << (8 * i);
    out = v;
    return true;
}

std::vector<uint8_t> tlvSerialize(const std::vector<Tlv>& tlvs)
{
    std::vector<uint8_t> out;
    for (const Tlv& t : tlvs) {
        out.push_back(t.tag);
        out.push_back(static_cast<uint8_t>(t.value.size()));
        out.insert(out.end(), t.value.begin(), t.value.end());
    }
    return out;
}

} // namespace mb66
