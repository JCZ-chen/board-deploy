#include "settingspage.h"
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QGroupBox>

SettingsPage::SettingsPage(QWidget* parent) : QWidget(parent)
{
    auto* outer = new QVBoxLayout(this);
    auto* box = new QGroupBox("阈值设置 (2506 超高 / 2507 超低)");
    auto* form = new QFormLayout(box);

    addrCombo_ = new QComboBox;
    form->addRow("从机地址:", addrCombo_);

    hiEdit_ = new QLineEdit("90.0");
    loEdit_ = new QLineEdit("10.0");
    form->addRow("超高阈值 (%):", hiEdit_);
    form->addRow("超低阈值 (%):", loEdit_);

    auto* btnRow = new QHBoxLayout;
    auto* btnHi = new QPushButton("写入超高阈值");
    auto* btnLo = new QPushButton("写入超低阈值");
    connect(btnHi, &QPushButton::clicked, [this]{ send(0x2506, hiEdit_); });
    connect(btnLo, &QPushButton::clicked, [this]{ send(0x2507, loEdit_); });
    btnRow->addWidget(btnHi);
    btnRow->addWidget(btnLo);
    form->addRow(btnRow);

    resultLabel_ = new QLabel("就绪");
    resultLabel_->setWordWrap(true);
    form->addRow("结果:", resultLabel_);

    outer->addWidget(box);
    outer->addStretch();
}

void SettingsPage::setAddresses(const QVector<uint8_t>& addrs)
{
    addrs_ = addrs;
    addrCombo_->clear();
    for (uint8_t a : addrs)
        addrCombo_->addItem(QString("%1").arg(a), a);
}

void SettingsPage::send(uint16_t oi, QLineEdit* edit)
{
    if (!writeFn_) return;
    bool ok = false;
    float v = edit->text().toFloat(&ok);
    if (!ok) { resultLabel_->setText("阈值格式错误(需数字)"); return; }
    uint8_t addr = addrCombo_->currentData().toUInt();
    writeFn_(addr, oi, v);
    resultLabel_->setText(QString("已发送写入: 从机 %1, %2=%3")
                          .arg(addr).arg(oi == 0x2506 ? "超高" : "超低").arg(v));
}

void SettingsPage::showWriteResult(uint8_t addr, uint16_t oi, bool ok, const QString& msg)
{
    QString which = (oi == 0x2506) ? "超高" : (oi == 0x2507) ? "超低" : QString("OI %1").arg(oi, 4, 16);
    resultLabel_->setStyleSheet(ok ? "color:green;" : "color:red;");
    resultLabel_->setText(ok
        ? QString("OK  从机 %1 %2阈值写入成功").arg(addr).arg(which)
        : QString("FAIL 从机 %1 %2阈值: %3").arg(addr).arg(which, msg));
}

void SettingsPage::refreshThresholds(uint8_t addr, float hi, float lo)
{
    // 在监控页展示时同步当前的阈值输入框(可作下一条轮询读回优化, 此处简单跳过)
    (void)addr; (void)hi; (void)lo;
}
