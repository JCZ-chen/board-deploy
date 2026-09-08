# 变压器油位计 Qt 上位机 — 部署说明（板子）

## 文件（本目录位于 NFS: 板子 /mnt/nfs/qt_app/deploy/）
- `oilmeter`      —— ARM 可执行（主程序，参数: [串口] [波特率] [地址列表]）
- `libQt5Sql.so.5.11.3` —— QtSql 运行库（若板子 runtime-deps 已装 Qt5Sql 则可不放）
- `sqldrivers/libqsqlite.so` —— **SQLite 驱动插件（关键，默认缺失）**
- `modbus66_slave` —— 仿真从机（无真表时联调用）

## 上板步骤

### 1) 放 SQLite 插件（必做，否则历史存储报 "database not open"）
```bash
# 板子执行
sudo cp /mnt/nfs/qt_app/deploy/sqldrivers/libqsqlite.so \
     /usr/lib/arm-linux-gnueabihf/qt5/plugins/sqldrivers/
```
若无 ./plugins/sqldrivers/ 目录则先 `sudo mkdir -p .../sqldrivers`。
运行前确认驱动可用（程序日志会打印 QSQLITE 是否存在）。

### 2) （可选）补 QtSql 主库
```bash
# 若启动报 libQt5Sql.so.5 找不到:
sudo cp /mnt/nfs/qt_app/deploy/libQt5Sql.so.5.11.3 /usr/lib/arm-linux-gnueabihf/
sudo ldconfig
```

### 3) 无真表联调（用仿真从机 + PTY）
```bash
# 板子上(或 PC 上分别开两个终端)
# 终端A: 仿真从机监听一个 pty
python3 -c "import pty,os; m,s=pty.openpty(); print('master',os.ttyname(m)); print('slave',os.ttyname(s)); import time; time.sleep(60)" &
# 把 slave 端传给 modbus66_slave
# 终端B: 运行上位机连 master 端
sudo ./oilmeter /dev/pts/N 9600 1 -platform linuxfb
```

### 4) 真 RS485
- 启用 imx-fire-485r1 (→/dev/ttymxc1) 或 485r2 (→/dev/ttymxc2) overlay（uEnv.txt）
- **先卸 CAN overlay**（RS485 与 CAN 共用 UART2/3 CTS/RTS 引脚，互斥）
- 板子波特率按表计协议配 9600，8N1

## 运行
```bash
sudo ./oilmeter -platform linuxfb        # 默认 /dev/ttymxc1 @ 9600, 从机1
# 或指定参数: ./oilmeter <串口> <波特率> <地址,逗号分隔>
# 界面: 实时监控(数值+状态灯+曲线) / 参数设置(写超高/超低阈值) / 历史查询(SQLite)
```
历史库存于 `/home/oilmeter_history.db`。

## 已知限制（工具链）
- 编译期不含 QThread（工具链 libstdc++/glibc 不兼容 std::mutex），轮询为主线程 QTimer 驱动，
  单线程设计。UI 不会因串口读卡死（read 带短超时）。
