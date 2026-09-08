QT += widgets sql
CONFIG += c++11
TARGET = oilmeter
TEMPLATE = app

# 板端原生编译时用 linuxfb 插件, 需要显式说明 gui 目标
CONFIG += console_hide

SOURCES += \
    app/main.cpp \
    app/glibc_stub.cpp \
    comms/pollmanager.cpp \
    comms/serialport.cpp \
    protocol/crc16.cpp \
    protocol/frame.cpp \
    protocol/oi_table.cpp \
    protocol/tlv.cpp \
    storage/historystore.cpp \
    ui/mainwindow.cpp \
    ui/monitorpage.cpp \
    ui/historypage.cpp \
    ui/trendwidget.cpp

HEADERS += \
    comms/metersnapshot.h \
    comms/pollmanager.h \
    comms/serialport.h \
    protocol/crc16.h \
    protocol/frame.h \
    protocol/oi_table.h \
    protocol/tlv.h \
    storage/historystore.h \
    ui/mainwindow.h \
    ui/monitorpage.h \
    ui/historypage.h \
    ui/trendwidget.h

INCLUDEPATH += app comms protocol storage ui
