// 裸 termios 串口封装 (不依赖 QtSerialPort): 打开/读/写/RS485(硬件DE控制)
#ifndef SERIALPORT_H
#define SERIALPORT_H

#include <string>
#include <vector>
#include <cstdint>

class SerialPort {
public:
    SerialPort();
    ~SerialPort();
    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;

    // 打开串口; 波特率默认 9600, 8N1 无校验。成功返回 true
    bool open(const std::string& dev, int baud = 9600);
    void close();
    bool isOpen() const { return fd_ >= 0; }

    // 配置 RS485 半双工 DE 方向脚 (sysfs GPIO 编号; 高=发送, 低=接收)
    // 某些板子系统内核没自动管理 DE(无 rts-gpios), 需用户态翻转。gpio<=0 表示不用。
    void setDeGpio(int gpioNum) { deGpio_ = gpioNum; }

    bool writeAll(const uint8_t* data, size_t len);
    // 阻塞读取至多 len 字节, 最多等 timeoutMs; 返回实际读到的字节数(0=超时)
    size_t readSome(uint8_t* data, size_t len, int timeoutMs);

private:
    void deSet(int level);   // 高=发送方向, 低=接收方向

    int fd_ = -1;
    int deGpio_ = 0;         // 0=未配置DE
    bool rs485_ = false;
};

#endif
