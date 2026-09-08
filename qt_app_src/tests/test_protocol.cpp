// 协议引擎单元测试: 用协议文档附录 C.3 的官方示例帧逐字节断言
// 编译(主机): g++ -std=c++11 -I. tests/test_protocol.cpp protocol/*.cpp -o /tmp/test_protocol && /tmp/test_protocol
#include <cstdio>
#include <cstdint>
#include <vector>
#include "protocol/crc16.h"
#include "protocol/frame.h"
#include "protocol/tlv.h"
#include "protocol/oi_table.h"

using namespace mb66;

static int failures = 0, checks = 0;

static void check(bool cond, const char* what)
{
    checks++;
    if (!cond) { failures++; printf("  [FAIL] #%d %s\n", checks, what); }
}

// 带进度的字节对数组打印 (用于肉眼核对)
static void dump(const std::vector<uint8_t>& v)
{
    for (uint8_t b : v) printf("%02X ", b);
    printf("\n");
}

int main()
{
    // ---------- CRC16: 用已知 Modbus 样例验证基础正确性 ----------
    // 标准 Modbus 校验 "01 03 00 00 00 0A" -> CRC=0xCDC5, 低字节在前发送 C5 CD
    {
        uint8_t d[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
        uint16_t c = crc16(d, sizeof(d));
        check(c == 0xCDC5, "CRC16 known vector 0xCDC5");
        // 发送时低字节在前 => appendCrc 推 [C5][CD]
        std::vector<uint8_t> f(d, d + sizeof(d));
        uint16_t crc = crc16(f.data(), f.size());
        f.push_back(uint8_t(crc & 0xFF));      // C5
        f.push_back(uint8_t(crc >> 8));        // CD
        check(f[6]==0xC5 && f[7]==0xCD, "CRC sent low-byte-first");
    }

    // ---------- 文档 C.3.1.1: 读取单个对象请求报文 ----------
    // 下行: 01 66 03 01 [20 00] CRCL CRCH  (OI=2000, 高字节在前)
    {
        auto f = buildReadRequest(0x01, 0x2000);
        check(f.size() == 8, "read-request length = 8");
        check(f[0]==0x01 && f[1]==0x66 && f[2]==0x03 && f[3]==0x01 && f[4]==0x20 && f[5]==0x00,
              "read OI=2000 header bytes (OI big-endian)");
        if (f.size() >= 8) {
            uint16_t c = crc16(f.data(), 6);
            check(f[6]==uint8_t(c&0xFF) && f[7]==uint8_t(c>>8), "read-request CRC low-first");
        }
        printf("  read OI=2000 req: "); dump(f);
    }

    // ---------- 文档 C.3.1.2: 读取单个对象应答报文 ----------
    // 上行: 01 66 07 81 20 XX 21 02 2C 01 CRCL CRCH
    //       = OI=0xXX20... 取 OI=0x2001 -> body "20 01", TLV[Short, val=300 小端 2C 01]
    {
        uint8_t body[] = {0x01,0x66,0x07,0x81, 0x20,0x01, 0x21,0x02, 0x2C,0x01};
        std::vector<uint8_t> frame(body, body+10);
        uint16_t c = crc16(frame.data(), frame.size());
        frame.push_back(uint8_t(c&0xFF)); frame.push_back(uint8_t(c>>8));

        ParsedFrame pf;
        bool pr = parseFrameBytes(frame, pf);
        check(pr, "parse C.3.1.2 response");
        if (pr) {
            check(pf.addr==0x01 && pf.fun==0x66 && pf.sfun==0x81, "resp addr/fun/sfun");
            check(pf.oi==0x2001, "resp OI decoded (0x2001, big-endian)");
            check(pf.tlvs.size()==1, "resp 1 tlv");
            if (pf.tlvs.size()==1) {
                check(pf.tlvs[0].tag==0x21 && pf.tlvs[0].value.size()==2, "resp tlv tag/len");
                uint64_t u=0;
                if (decodeUint(pf.tlvs[0], u)) check(u==300, "resp tlv value=300 (little-endian)");
            }
        }
        printf("  C.3.1.2 resp: "); dump(frame);
    }

    // ---------- 文档 C.3.2.1: 写入单个对象请求报文 ----------
    // 下行: 01 66 07 02 20 XX 21 02 2C 01  (OI 高字节在前)
    {
        Tlv t; t.tag = 0x21; t.value = {0x2C, 0x01};  // 300 小端 (2字节)
        auto f = buildWriteRequest(0x01, 0x2003, t);
        // LEN = 1(sfun)+2(oi)+4(tlv:tag+len+2B val) = 7; 总长 = 3+7+2 = 12
        check(f.size()==12, "write-request length = 12");
        check(f[0]==0x01 && f[1]==0x66 && f[2]==0x07 && f[3]==0x02, "write header");
        check(f[4]==0x20 && f[5]==0x03, "write OI 0x2003 (big-endian)");
        check(f[6]==0x21 && f[7]==0x02 && f[8]==0x2C && f[9]==0x01, "write TLV payload");
        printf("  write OI=2003 req: "); dump(f);
    }

    // ---------- 读多个对象 (C.3.1.3) ----------
    {
        auto f = buildReadMultiRequest(0x01, {0x2201,0x2202,0x2203});
        // LEN = 1+3*2 = 7; 总长 = 3+7+2 = 12
        check(f.size()==12, "read-multi length");
        check(f[0]==0x01 && f[1]==0x66 && f[2]==0x07 && f[3]==0x01, "read-multi header");
        check(f[4]==0x22 && f[5]==0x01 && f[6]==0x22 && f[7]==0x02 && f[8]==0x22 && f[9]==0x03,
              "read-multi 3 OI (big-endian)");
        printf("  read-multi 3 OI: "); dump(f);
    }

    // ---------- 广播对时 (C.3.3) ----------
    // 下行: 00 66 0C 33 20 04 40 07 E6 07 01 02 03 04 05
    {
        std::vector<uint8_t> dt = {0xE6,0x07,0x01,0x02,0x03,0x04,0x05}; // 2022-01-02 03:04:05
        auto f = buildTimeBroadcast(dt);
        check(f.size()==17, "time-broadcast length");
        check(f[0]==0x00 && f[1]==0x66 && f[2]==0x0C && f[3]==0x33, "time-broadcast header");
        check(f[4]==0x20 && f[5]==0x04, "time-broadcast OI=2004 (big-endian)");
        check(f[6]==0x40 && f[7]==0x07, "time-broadcast TLV DataTime/7");
        check(f[8]==0xE6 && f[9]==0x07 && f[10]==0x01, "time-broadcast value");
        printf("  time-broadcast: "); dump(f);
    }

    // ---------- 粘包: 两帧连发 (FrameParser 应拆出 2 帧) ----------
    {
        auto f1 = buildReadRequest(0x01, 0x2502);
        auto f2 = buildReadRequest(0x01, 0x2503);
        std::vector<uint8_t> stream;
        stream.insert(stream.end(), f1.begin(), f1.end());
        stream.insert(stream.end(), f2.begin(), f2.end());

        FrameParser fp;
        std::vector<ParsedFrame> out;
        fp.feed(stream.data(), stream.size(), out);
        check(out.size()==2, "frame parser splits 2 concatenated frames");
        if (out.size()==2) {
            check(out[0].oi==0x2502 && out[1].oi==0x2503, "parser keeps frame order/OI");
        }
    }

    // ---------- 半包 + 分批 feed ----------
    {
        auto f = buildReadRequest(0x01, 0x2506);
        FrameParser fp;
        std::vector<ParsedFrame> out;
        fp.feed(f.data(), 3, out);   // 只给头
        check(out.empty(), "parser waits on partial frame");
        fp.feed(f.data()+3, f.size()-3, out);
        check(out.size()==1, "parser completes after second feed");
    }

    // ---------- 畸形帧重同步: 帧头混入一个坏字节 ----------
    {
        auto f = buildReadRequest(0x01, 0x2502);
        std::vector<uint8_t> stream;
        stream.push_back(0xFF);   // 垃圾
        stream.insert(stream.end(), f.begin(), f.end());
        FrameParser fp;
        std::vector<ParsedFrame> out;
        fp.feed(stream.data(), stream.size(), out);
        check(out.size()==1, "parser resyncs after garbage byte");
    }

    // ---------- 写请求与 2500 表: 2506 可写, 2502 只读 (OI 用十进制, 与表一致) ----------
    {
        check(isWritable(2506) && isWritable(2507), "2506/2507 writable");
        check(!isWritable(2502), "2502 read-only");
        const OiField* f = findOi(2502);
        check(f && f->unit == "%", "2502 unit=%");
    }

    // ---------- float TLV 编解码 ----------
    {
        Tlv t = makeFloatTlv(55.5f);
        float back = 0;
        check(decodeFloat(t, back) && back >= 55.49f && back <= 55.51f, "float TLV roundtrip");
    }

    printf("\n== %d checks, %d failures ==\n", checks, failures);
    return failures ? 1 : 0;
}
