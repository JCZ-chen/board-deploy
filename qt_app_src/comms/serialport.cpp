#include "serialport.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <poll.h>

// ---- DE GPIO 控制 (高=发送, 低=接收) ----
// export/direction 只做一次; 之后只写 value, 避免每次 export 的 50ms 开销
static int     s_deGpio = 0;
static bool    s_deInit = false;
static int     s_deFd = -1;

static void degpioOpen(int num)
{
    char path[64]; int fd;
    fd = ::open("/sys/class/gpio/export", O_WRONLY);
    if (fd >= 0) { char b[8]; snprintf(b, sizeof(b), "%d\n", num); ::write(fd, b, strlen(b)); ::close(fd); }
    usleep(50000);
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", num);
    fd = ::open(path, O_WRONLY);
    if (fd >= 0) { ::write(fd, "out", 3); ::close(fd); }
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", num);
    if (s_deFd >= 0) ::close(s_deFd);
    s_deFd = ::open(path, O_WRONLY);
}

// 顶层的 DE 电平设置
static void degpioSet(int level)
{
    if (s_deGpio <= 0) return;
    if (!s_deInit) { degpioOpen(s_deGpio); s_deInit = true; }
    if (s_deFd >= 0) {
        ::lseek(s_deFd, 0, SEEK_SET);
        ::write(s_deFd, level ? "1" : "0", 1);
        fsync(s_deFd);
    }
}

SerialPort::SerialPort() {}
SerialPort::~SerialPort() { close(); }

void SerialPort::deSet(int level)
{
    degpioSet(level);   // 仅设电平; export/direction 已在 open() 初始化
}

bool SerialPort::open(const std::string& dev, int baud)
{
    s_deGpio = deGpio_;
    s_deInit = false;
    if (s_deFd >= 0) { ::close(s_deFd); s_deFd = -1; }

    // 与已验证的 485test 一致: 阻塞模式 O_RDWR|O_NOCTTY
    fd_ = ::open(dev.c_str(), O_RDWR | O_NOCTTY);
    if (fd_ < 0) { perror("serial open"); return false; }

    termios t;
    memset(&t, 0, sizeof(t));
    speed_t sp = B9600;
    switch (baud) {
        case 2400: sp = B2400; break;
        case 4800: sp = B4800; break;
        case 9600: sp = B9600; break;
        case 19200: sp = B19200; break;
        case 38400: sp = B38400; break;
        case 57600: sp = B57600; break;
        case 115200: sp = B115200; break;
        default: sp = B9600; break;
    }
    cfsetispeed(&t, sp);
    cfsetospeed(&t, sp);
    t.c_cflag = CS8 | CREAD | CLOCAL;
    t.c_cflag &= ~(PARENB | CSTOPB | CSIZE);
    t.c_cflag |= CS8;
    t.c_iflag = 0; t.c_oflag = 0; t.c_lflag = 0;
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 0;
    tcsetattr(fd_, TCSANOW, &t);
    tcflush(fd_, TCIOFLUSH);

    deSet(0);   // 默认接收方向
    return true;
}

void SerialPort::close()
{
    if (fd_ >= 0) {
        deSet(0);
        ::close(fd_);
        fd_ = -1;
    }
}

bool SerialPort::writeAll(const uint8_t* data, size_t len)
{
    if (fd_ < 0) return false;
    deSet(1);                       // 发送方向
    usleep(2000);                   // 2ms 等 DE 稳定(485收发器翻转典型<1ms, 10ms纯浪费)
    size_t off = 0;
    while (off < len) {
        ssize_t w = ::write(fd_, data + off, len - off);
        if (w < 0) { if (errno == EAGAIN || errno == EWOULDBLOCK) { usleep(1000); continue; } deSet(0); return false; }
        off += (size_t)w;
    }
    tcdrain(fd_);                   // 等发送完
    usleep(2000);
    deSet(0);                       // 切回接收
    usleep(2000);
    return true;
}

size_t SerialPort::readSome(uint8_t* data, size_t len, int timeoutMs)
{
    if (fd_ < 0) return 0;
    pollfd pfd{ fd_, POLLIN, 0 };
    int r = poll(&pfd, 1, timeoutMs);
    if (r <= 0 || !(pfd.revents & POLLIN)) return 0;
    ssize_t n = ::read(fd_, data, len);
    if (n <= 0) return 0;
    return (size_t)n;
}
