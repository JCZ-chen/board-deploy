// 最小 485 测试程序: 打开 ttymxc1, 用 DE GPIO 半双工方向控制,
// 发一个 Modbus-66 读请求(读 OI=2502), 然后读应答并打印 hex。
// 逻辑与官方 libmodbus 例子的 RTS 控制一致: 发前 DE=1(高), 发完 DE=0(低), 再读。
// 用法: 485test <串口> <DE_GPIO编号>  例: 485test /dev/ttymxc1 22
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>

static void gpio_write(int num, int on)
{
    char path[64]; int fd;
    fd = open("/sys/class/gpio/export", O_WRONLY);
    if (fd >= 0) { char b[8]; snprintf(b, sizeof(b), "%d\n", num); write(fd, b, strlen(b)); close(fd); }
    usleep(50000);
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", num);
    fd = open(path, O_WRONLY);
    if (fd >= 0) { write(fd, "out", 3); close(fd); }
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", num);
    fd = open(path, O_WRONLY);
    if (fd >= 0) { write(fd, on ? "1" : "0", 1); close(fd); }
}

static int open_serial(const char* dev, int baud)
{
    int fd = open(dev, O_RDWR | O_NOCTTY);
    if (fd < 0) { perror("open"); return -1; }
    struct termios t; memset(&t, 0, sizeof(t));
    cfsetispeed(&t, B9600); cfsetospeed(&t, B9600);
    t.c_cflag = CS8 | CREAD | CLOCAL;
    t.c_cflag &= ~(PARENB | CSTOPB | CSIZE); t.c_cflag |= CS8;
    t.c_iflag = 0; t.c_oflag = 0; t.c_lflag = 0;
    t.c_cc[VMIN] = 0; t.c_cc[VTIME] = 10;   /* 1 秒超时 */
    tcsetattr(fd, TCSANOW, &t);
    return fd;
}

int main(int argc, char** argv)
{
    if (argc < 2) { printf("usage: %s <dev> [de_gpio]\n", argv[0]); return 1; }
    const char* dev = argv[1];
    int de = argc >= 3 ? atoi(argv[2]) : 22;

    // 请求: 读 从机1 OI=0x2502
    unsigned char req[] = { 0x01, 0x66, 0x03, 0x01, 0x25, 0x02, 0xC3, 0x17 };
    unsigned char buf[256];
    int fd = open_serial(dev, 9600);
    if (fd < 0) return 1;

    printf("dev=%s de_gpio=%d\n", dev, de);
    gpio_write(de, 1);            /* DE=高: 发送方向 */
    usleep(50000);
    int wr = write(fd, req, sizeof(req));
    tcdrain(fd);                  /* 等发送完 */
    printf("write %d bytes\n", wr);
    usleep(50000);
    gpio_write(de, 0);            /* DE=低: 接收方向 */
    usleep(50000);

    /* 读应答, 最多 1 秒 */
    int total = 0, t0 = 0;
    for (int i = 0; i < 20; ++i) {
        int r = read(fd, buf + total, sizeof(buf) - total);
        if (r > 0) total += r;
        if (total > 0) break;
    }
    printf("recv %d bytes:", total);
    for (int i = 0; i < total; ++i) printf(" %02X", buf[i]);
    printf("\n");
    close(fd);
    return 0;
}
