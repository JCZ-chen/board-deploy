/*
 * 仿真从机: 模拟 Modbus-66 扩展协议的 2500 系列(变压器油位计) 应答
 * 纯 C, 无 Qt。用于在无真表时联调上位机。
 *
 * 用法:
 *   ./modbus66_slave <串口设备> [选项]
 *   选项:
 *     -a <addr>      从机地址 (默认 1)
 *     -l <level>     初始油位 % (float, 默认 60.0)
 *     -hi <val>      超高阈值 % (默认 90.0)
 *     -lo <val>      超低阈值 % (默认 10.0)
 *     -st <hex>      状态字 (默认 0)
 *     -v             打印收到的请求
 *  测试注入(模拟现场异常, 便于联调):
 *     -drop          丢弃前 2 次请求(模拟对时未应答/抓包)
 *     -stataddr      地址不匹配时静默(符合协议: 不响应, 主站超时)
 *     -e <hex>       对指定 OI 注入异常应答(返回错误码) 未实现, 用 -errOI <hex>
 *
 * 示例: socat -d -d pty,raw,echo=0,link=/tmp/slave pty,raw,echo=0,link=/tmp/master &
 *       ./modbus66_slave /tmp/slave -a 1 -l 62.5 -v
 *       (Qt 上位机打开 /tmp/master; 或直接对板子串口用真 RS485)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <stdint.h>

/* ---- 协议常量 ---- */
#define FUN_EXT       0x66
#define FUN_ERROR     0xE6
#define SFUN_READ     0x01
#define SFUN_WRITE    0x02
#define SFUN_READ_NEXT 0x41
#define SFUN_READ_RESP  0x81
#define SFUN_WRITE_RESP 0x82
#define SFUN_READ_CTD_RESP 0xC1
#define SFUN_D6_NEXT   0x40
#define SFUN_D7_DIR    0x80

#define TAG_UTINY  0x20
#define TAG_SHORT  0x21
#define TAG_UINT   0x35
#define TAG_FLOAT  0x26
#define TAG_OCTET  0x04
#define TAG_STRING 0x05
#define TAG_DATETIME 0x40
#define TAG_STRUCT 0x41

/* ---- CRC16 (初值 FFFF, 多项式 A001), 与上位机一致 ---- */
static uint16_t crc16(const uint8_t* d, size_t n)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < n; ++i) {
        crc ^= d[i];
        for (int b = 0; b < 8; ++b)
            crc = (crc & 1) ? ((crc >> 1) ^ 0xA001) : (crc >> 1);
    }
    return crc;
}
static void append_crc(uint8_t* f, size_t n)
{
    uint16_t c = crc16(f, n);
    f[n] = (uint8_t)(c & 0xFF);
    f[n+1] = (uint8_t)(c >> 8);
}

/* ---- 假表状态 (2500 系列字段) ---- */
static int g_addr = 1;
static float g_level = 60.0f;     /* 2502 油位 % */
static float g_level_mm = 520.0f; /* 2503 绝对 mm */
static float g_hi = 90.0f;        /* 2506 超高阈值 */
static float g_lo = 10.0f;        /* 2507 超低阈值 */
static uint16_t g_status = 0;     /* 2501 状态字 */
static int g_drop_left = 0;
static int g_verbose = 0;

/* ---- 串口打开(原始模式) ---- */
static int open_serial(const char* dev)
{
    int fd = open(dev, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) { perror("open"); return -1; }
    struct termios t;
    memset(&t, 0, sizeof(t));
    cfsetispeed(&t, B9600);
    cfsetospeed(&t, B9600);
    t.c_cflag = CS8 | CREAD | CLOCAL;      /* 8N1, 无校验 */
    t.c_iflag = 0; t.c_oflag = 0; t.c_lflag = 0;
    t.c_cc[VMIN] = 1; t.c_cc[VTIME] = 1;   /* 100ms */
    tcsetattr(fd, TCSANOW, &t);
    return fd;
}

/* 编码一个 float 到小端 */
static void put_f32(uint8_t* out, float v)
{
    memcpy(out, &v, 4);   /* x86/ARM 均为小端 */
}

/* 拼一个 TLV 并写入 resp 缓冲, 返回写入长度(0=失败) */
static int put_tlv(uint8_t* out, uint8_t oi_hi, uint8_t oi_lo,
                   uint8_t tag, const uint8_t* val, uint8_t vlen)
{
    out[0] = oi_hi; out[1] = oi_lo;
    out[2] = tag;   out[3] = vlen;
    memcpy(out + 4, val, vlen);
    return 4 + vlen;
}

/* ---- 处理一帧请求, 写响应到 buf, 返回响应长度; 0=不应答 ---- */
static int handle_frame(const uint8_t* in, size_t n, uint8_t* out)
{
    if (n < 6) return 0;
    uint8_t addr = in[0];
    uint8_t fun  = in[1];
    uint8_t len  = in[2];
    if (n != 3 + len + 2) return 0;              /* 长度不符, 忽略 */
    if (fun != FUN_EXT) return 0;                /* 非本协议 */
    uint16_t crc = crc16(in, n - 2);
    uint16_t recv = in[n-2] | ((uint16_t)in[n-1] << 8);
    if (crc != recv) { if (g_verbose) printf("[slave] CRC bad, drop\n"); return 0; }

    uint8_t sfun = in[3];
    uint8_t req_dir = sfun & SFUN_D7_DIR;   /* 0=请求 */

    /* 只处理请求帧(从机不应答自己的响应) */
    if (req_dir) return 0;

    /* 广播对时: 地址 0, 从机不应答 */
    if (addr == 0) { if (g_verbose) printf("[slave] broadcast(time) ignored\n"); return 0; }

    /* 地址过滤: 只应答自己的地址 */
    if (addr != g_addr) {
        if (g_verbose) printf("[slave] addr %u != mine %d, silent\n", addr, g_addr);
        return 0;
    }

    /* 注入: 丢弃若干请求(模拟超时/抓包丢失) */
    if (g_drop_left > 0) { g_drop_left--; if (g_verbose) printf("[slave] drop frame (left %d)\n", g_drop_left+1); return 0; }

    if (g_verbose) {
        printf("[slave] RX:"); for (size_t i=0;i<n;i++) printf(" %02X", in[i]); printf("\n");
    }

    /* 读取后续帧: 本仿真不真正分帧, 统一按"无后续"的 81/82 响应 */
    if ((sfun & 0x3F) == SFUN_READ_NEXT) {
        /* 正常不会走到: 分帧未启用 */
        return 0;
    }

    uint8_t action = sfun & 0x3F;   /* D5~D0 */

    if (action == SFUN_READ) {
        /* OI 从 in[4], 大端(高字节在前) */
        if (n < 6) return 0;
        uint16_t oi = ((uint16_t)in[4] << 8) | in[5];

        /* 我们支持一次请求多个 OI: in[4..] 每 2 字节一个 */
        uint8_t resp[300];
        int rp = 0;
        uint8_t first_sfun = SFUN_READ_RESP;
        int count = 0;
        size_t p = 4;
        while (p + 2 <= 3 + len) {
            uint16_t cur = ((uint16_t)in[p] << 8) | in[p+1];
            p += 2;
            count++;
            /* 组每字段应答 */
            switch (cur) {
            case 0x2501: { /* 状态字 OcterString 2B */
                uint8_t v[2] = { (uint8_t)(g_status & 0xFF), (uint8_t)(g_status >> 8) };
                rp += put_tlv(resp + rp, 0x25, 0x01, TAG_OCTET, v, 2);
                break; }
            case 0x2502: { uint8_t v[4]; put_f32(v, g_level); rp += put_tlv(resp+rp, 0x25,0x02, TAG_FLOAT, v, 4); break; }
            case 0x2503: { uint8_t v[4]; put_f32(v, g_level_mm); rp += put_tlv(resp+rp, 0x25,0x03, TAG_FLOAT, v, 4); break; }
            case 0x2506: { uint8_t v[4]; put_f32(v, g_hi); rp += put_tlv(resp+rp, 0x25,0x06, TAG_FLOAT, v, 4); break; }
            case 0x2507: { uint8_t v[4]; put_f32(v, g_lo); rp += put_tlv(resp+rp, 0x25,0x07, TAG_FLOAT, v, 4); break; }
            case 0x2500: case 0x2504: case 0x2505: default:
                /* 未实现字段: 返回错误码 03(非法数据值) */
                out[0] = (uint8_t)g_addr;
                out[1] = FUN_ERROR;
                out[2] = 2;                 /* 数据长度 = SFUN + 错误码 */
                out[3] = SFUN_READ_RESP;    /* 错误时也用读取响应的 SFUN 方向位+1 */
                out[4] = 0x03;              /* 错误码: 非法数据值 */
                append_crc(out, 5);
                return 7;
            }
            if (rp > 250) break;   /* 防溢出, 本仿真字段少不会到 */
        }

        /* 组装响应帧: ADDR FUN LEN SFUN ... */
        out[0] = (uint8_t)g_addr;
        out[1] = FUN_EXT;
        out[2] = (uint8_t)(1 + rp);      /* LEN = SFUN + 数据体 */
        out[3] = first_sfun;             /* 81 */
        memcpy(out + 4, resp, rp);
        uint8_t total = 3 + out[2] + 2;
        append_crc(out, total - 2);
        if (g_verbose) { printf("[slave] TX:"); for (int i=0;i<total;i++) printf(" %02X",out[i]); printf("\n"); }
        return total;
    }

    if (action == SFUN_WRITE) {
        /* in: ADDR 66 LEN 02 OI_hi OI_lo TAG LEN VAL... */
        if (n < 9) return 0;
        uint16_t oi = ((uint16_t)in[4] << 8) | in[5];
        uint8_t tag = in[6];
        uint8_t vlen = in[7];
        float val;
        if (tag == TAG_FLOAT && vlen == 4) {
            memcpy(&val, in + 8, 4);
        } else { return 0; }

        int ok = 1;
        if (oi == 0x2506) g_hi = val;
        else if (oi == 0x2507) g_lo = val;
        else ok = 0;

        if (ok) {
            if (g_verbose) printf("[slave] WRITE OI=%u val=%.2f\n", oi, val);
            /* 写应答 82: 原样回带同结构; LEN = 1(sfun)+2(oi)+1(tag)+1(len)+vlen */
            out[0] = g_addr; out[1] = FUN_EXT; out[2] = (uint8_t)(5 + vlen);
            out[3] = SFUN_WRITE_RESP;          /* 82 */
            out[4] = in[4]; out[5] = in[5];     /* OI */
            out[6] = tag; out[7] = vlen;
            memcpy(out + 8, in + 8, vlen);
            uint8_t total = 3 + out[2] + 2;
            append_crc(out, total - 2);
            return total;
        } else {
            /* 非法 OI: 异常 03 */
            out[0] = g_addr; out[1] = FUN_ERROR; out[2] = 2;
            out[3] = SFUN_WRITE_RESP; out[4] = 0x03;
            append_crc(out, 5);
            return 7;
        }
    }

    return 0;
}

int main(int argc, char** argv)
{
    const char* dev = NULL;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-a") && i+1<argc) g_addr = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-l") && i+1<argc) g_level = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "-hi") && i+1<argc) g_hi = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "-lo") && i+1<argc) g_lo = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "-st") && i+1<argc) g_status = (uint16_t)strtol(argv[++i], 0, 16);
        else if (!strcmp(argv[i], "-drop") && i+1<argc) g_drop_left = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-v")) g_verbose = 1;
        else dev = argv[i];   /* 第一个非选项视为设备 */
    }
    if (!dev) {
        fprintf(stderr, "usage: %s <dev> [-a addr] [-l level%%] [-hi x] [-lo x] [-st hex] [-drop n] [-v]\n", argv[0]);
        return 1;
    }
    int fd = open_serial(dev);
    if (fd < 0) return 1;

    printf("[slave] addr=%d device=%s level=%.1f%% hi=%.1f lo=%.1f status=0x%04X\n",
           g_addr, dev, g_level, g_hi, g_lo, g_status);

    uint8_t buf[512];
    while (1) {
        ssize_t r = read(fd, buf, sizeof(buf));
        if (r > 0) {
            /* 可能一读多帧或半帧; 简化解法: 每收到可解析的帧即响(仿真下对一帧足矣) */
            uint8_t resp[300];
            int rlen = handle_frame(buf, (size_t)r, resp);
            if (rlen > 0) {
                write(fd, resp, rlen);
                tcdrain(fd);   /* 等发完再回读/等下一帧 */
            }
        } else if (r < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            break;
        }
        usleep(50000);   /* 50ms 轮询, 仿真足够 */
    }
    close(fd);
    return 0;
}
