#include "monitorpage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QLabel>
#include <QGroupBox>
#include <QGridLayout>
#include <QScrollArea>

MonitorPage::MonitorPage(QWidget* parent) : QWidget(parent)
{
    auto* outer = new QVBoxLayout(this);
    auto* listScroll = new QScrollArea;
    listScroll->setWidgetResizable(true);
    auto* listContainer = new QWidget;
    listLayout_ = new QVBoxLayout(listContainer);
    listLayout_->addStretch();
    listScroll->setWidget(listContainer);
    outer->addWidget(listScroll);
}

void MonitorPage::addMeter(uint8_t addr)
{
    MeterBox box;
    auto* card = new QGroupBox(QString("从机 %1").arg(addr));
    auto* grid = new QGridLayout(card);

    box.title = new QLabel(QString("油位计 #%1").arg(addr));
    box.levelPct = new QLabel("-- %");
    box.levelMm = new QLabel("-- mm");
    box.statusText = new QLabel("未连接");
    box.statusText->setStyleSheet("color:gray;");
    box.trend = new TrendWidget;
    box.led = new QLabel;
    box.led->setFixedSize(14, 14);

    grid->addWidget(box.title, 0, 0);
    grid->addWidget(box.led, 0, 1, Qt::AlignRight);
    grid->addWidget(new QLabel("油位:"), 1, 0);
    grid->addWidget(box.levelPct, 1, 1);
    grid->addWidget(new QLabel("绝对高度:"), 2, 0);
    grid->addWidget(box.levelMm, 2, 1);
    grid->addWidget(box.statusText, 3, 0, 1, 2);
    grid->addWidget(box.trend, 4, 0, 1, 2);

    // 插到 stretch 之前
    listLayout_->insertWidget(listLayout_->count() - 1, card);
    boxes_.insert(addr, box);
}

void MonitorPage::removeAll()
{
    // 清掉所有卡片 (简单起见重建)
    while (QLayoutItem* item = listLayout_->takeAt(0)) {
        if (QWidget* w = item->widget()) { w->deleteLater(); }
        delete item;
    }
    boxes_.clear();
    listLayout_->addStretch();
}

void MonitorPage::updateMeter(const MeterSnapshot& snap)
{
    if (!boxes_.contains(snap.addr)) return;
    MeterBox& box = boxes_[snap.addr];
    if (snap.hasLevel) box.levelPct->setText(QString::number(snap.levelPct, 'f', 1) + " %");
    else box.levelPct->setText("--");
    if (snap.hasMm) box.levelMm->setText(QString::number(snap.levelMm, 'f', 1) + " mm");
    else box.levelMm->setText("--");

    QString st;
    if (snap.hasStatus) {
        QStringList bits;
        if (snap.status & 0x02) bits << "超高报警";
        if (snap.status & 0x04) bits << "超低报警";
        if (snap.status & 0x08) bits << "保护动作";
        st = bits.isEmpty() ? "正常" : bits.join(",");
        box.statusText->setStyleSheet(st == "正常"
            ? "color:green;" : "color:red;font-weight:bold;");
    } else {
        st = "无状态";
    }
    box.statusText->setText(st);
    box.led->setStyleSheet(st == "正常"
        ? "background:green;border-radius:7px;"
        : st == "无状态" ? "background:gray;border-radius:7px;"
                         : "background:red;border-radius:7px;");

    if (snap.hasLevel) box.trend->pushPoint(snap.levelPct);
}
