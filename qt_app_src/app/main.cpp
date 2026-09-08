#include <QApplication>
#include "ui/mainwindow.h"

// 用法: oilmeter [串口] [波特率] [地址列表,逗号分隔] [DE_GPIO]
//   例: ./oilmeter /dev/ttymxc1 9600 1         (485-1, DE=gpio22)
//        ./oilmeter /dev/ttymxc1 9600 1 22     (等价)
//        ./oilmeter /dev/ttymxc2 9600 2 23     (485-2, DE=gpio23)
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    MainWindow w;
    w.resize(900, 600);
    int deGpio = argc >= 5 ? QString(argv[4]).toInt() : 22;
    if (argc >= 2)
        w.configure(QString::fromLocal8Bit(argv[1]),
                    argc >= 3 ? QString::fromLocal8Bit(argv[2]).toInt() : 9600,
                    argc >= 4 ? QString::fromLocal8Bit(argv[3]) : QString("1"),
                    deGpio);
    w.show();
    return app.exec();
}
