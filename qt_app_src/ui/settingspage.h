// 参数设置页: 写阈值(2506 超高 / 2507 超低)
#ifndef SETTINGSPAGE_H
#define SETTINGSPAGE_H

#include <QWidget>
#include <QVector>
#include <cstdint>

class QLineEdit;
class QLabel;
class QComboBox;

class SettingsPage : public QWidget {
    Q_OBJECT
public:
    explicit SettingsPage(QWidget* parent = nullptr);

    void setAddresses(const QVector<uint8_t>& addrs);
    void showWriteResult(uint8_t addr, uint16_t oi, bool ok, const QString& msg);
    void refreshThresholds(uint8_t addr, float hi, float lo);

signals:
    void writeRequested(uint8_t addr, uint16_t oi, float value);   // -> PollManager::writeThreshold

private slots:
    void send(uint16_t oi, QLineEdit* edit);

private:
    void refreshFromCurrent();

    QComboBox* addrCombo_;
    QLineEdit* hiEdit_;
    QLineEdit* loEdit_;
    QLabel* resultLabel_;
    QVector<uint8_t> addrs_;
};

#endif
