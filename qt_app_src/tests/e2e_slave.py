#!/usr/bin/env python3
# 端到端联调: 用 pty 把 仿真从机 和 本脚本(模拟主站) 连接, 验证协议链路
# 不依赖 Qt, 在主机跑: python3 tests/e2e_slave.py
import os, pty, sys, time, subprocess, struct, binascii, select

def crc16(data):
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if (crc & 1) else (crc >> 1)
    return crc

def frame(addr, sfun, oi_hi, oi_lo, payload=b""):
    apdu = bytes([sfun, oi_hi, oi_lo]) + payload
    body = bytes([addr, 0x66, len(apdu)]) + apdu
    c = crc16(body)
    return body + bytes([c & 0xFF, c >> 8])

def wframe(addr, oi_hi, oi_lo, tag, val_bytes):
    apdu = bytes([0x02, oi_hi, oi_lo, tag, len(val_bytes)]) + val_bytes
    body = bytes([addr, 0x66, len(apdu)]) + apdu
    c = crc16(body)
    return body + bytes([c & 0xFF, c >> 8])

def read_frame(f, timeout=2.0):
    end = time.time() + timeout
    data = b""
    while time.time() < end:
        r, _, _ = select.select([f], [], [], 0.02)
        if r:
            try:
                chunk = os.read(f, 512)
            except OSError:
                chunk = b""
            if chunk:
                data += chunk
                return data
    return data

def make_float(vle, v):  # 小端 float
    return vle + struct.pack('<f', v)

master_fd, slave_fd = pty.openpty()

sim = subprocess.Popen(["./simulator/modbus66_slave", os.ttyname(slave_fd), "-a", "1", "-l", "62.5", "-v"])
time.sleep(0.5)
os.close(slave_fd)

def mwrite(fd, data):
    os.write(fd, data)

fails = 0
def chk(ok, name):
    global fails
    print(("  PASS " if ok else "  FAIL ") + name)
    if not ok: fails += 1

# 1) 读单对象 2502 (油位)
print("== read 2502")
req = frame(1, 0x01, 0x25, 0x02)
mwrite(master_fd, req)
resp = read_frame(master_fd)
print("   req :", req.hex())
print("   resp:", resp.hex())
chk(len(resp) >= 9, "got response")
if len(resp) >= 9:
    # 01 66 09 81 25 02 26 04 <f32> crc  (Float 4B → LEN=0x09)
    chk(resp[0]==1 and resp[1]==0x66 and resp[2]==0x09 and resp[3]==0x81, "resp header/sfun=81")
    chk(resp[4]==0x25 and resp[5]==0x02, "resp OI=2502")
    chk(resp[6]==0x26 and resp[7]==0x04, "resp TLV Float/4")
    val = struct.unpack('<f', resp[8:12])[0]
    chk(abs(val-62.5) < 0.01, f"resp oil level={val}")
    # CRC
    c = crc16(resp[:-2]); recv = resp[-2] | (resp[-1]<<8)
    chk(c == recv, "resp CRC ok")

# 2) 多对象读取 2502+2503
print("== read multi 2502,2503")
req = bytes([1,0x66,0x05,0x01, 0x25,0x02, 0x25,0x03])
c = crc16(req); req += bytes([c&0xFF, c>>8])
mwrite(master_fd, req)
resp = read_frame(master_fd)
print("   req :", req.hex())
print("   resp:", resp.hex())
chk(resp[:2]==bytes([1,0x66]) and resp[3]==0x81, "multi resp header")
chk(resp[4:6]==bytes([0x25,0x02]) and resp[6]==0x26, "multi 1st OI=2502 float")

# 3) 写阈值 2506 = 85.5
print("== write 2506")
newv = 85.5
req = wframe(1, 0x25, 0x06, 0x26, struct.pack('<f', newv))   # Float 4B
mwrite(master_fd, req)
resp = read_frame(master_fd)
print("   req :", req.hex())
print("   resp:", resp.hex())
chk(resp[3]==0x82, "write resp sfun=82")
if len(resp)>=12:
    val = struct.unpack('<f', resp[8:12])[0]
    chk(abs(val-newv)<0.01, f"write ack value={val}")

# 4) 重新读 2506 确认已写入
print("== read back 2506")
req = frame(1, 0x01, 0x25, 0x06)
mwrite(master_fd, req)
resp = read_frame(master_fd)
val = struct.unpack('<f', resp[8:12])[0]
chk(abs(val-newv)<0.01, f"read-back hi={val}")

# 5) 读未实现字段 2504 -> 应返回异常 03
print("== read unimplemented 2504 (expect error 03)")
req = frame(1, 0x01, 0x25, 0x04)
mwrite(master_fd, req)
resp = read_frame(master_fd)
print("   resp:", resp.hex())
chk(resp[1]==0xE6, "error FUN=0xE6")
chk(resp[4]==0x03, "error code=03")

# 6) 地址不匹配 -> 静默
print("== wrong addr 2 (expect silence)")
req = frame(2, 0x01, 0x25, 0x02)
mwrite(master_fd, req)
resp = read_frame(master_fd, timeout=1.0)
chk(resp == b"", "silent for wrong addr")

# 7) 广播对时 addr=0 -> 静默
print("== broadcast time (expect silence)")
req = bytes([0x00,0x66,0x0C,0x33, 0x20,0x04,0x40,0x07, 0xE6,0x07,0x01,0x02,0x03,0x04,0x05])
c = crc16(req); req += bytes([c&0xFF, c>>8])
mwrite(master_fd, req)
resp = read_frame(master_fd, timeout=1.0)
chk(resp == b"", "silent for broadcast")

sim.terminate()
print()
print("== e2e: %s ==" % ("ALL PASS" if fails==0 else f"{fails} FAILURES"))
sys.exit(0 if fails==0 else 1)
