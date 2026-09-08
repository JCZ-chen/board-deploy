// 参数设置页: 写阈值(2506 超高 / 2507 超低) (无 Q_OBJECT/moc)
#ifndef SETTINGSPAGE_H
#define SETTINGSPAGE_H

#include <QWidget>
#include <QVector>
#include <functional>
#include <cstdint>

class QLineEdit;
class QLabel;
class QComboBox;

class SettingsPage : public QWidget {
public:
    using WriteThresholdFn = std::function<void(uint8_t, uint16_t, float)>;

    explicit SettingsPage(QWidget* parent = nullptr);

    void setAddresses(const QVector<uint8_t>& addrs);
    void setWriteHandler(WriteThresholdFn fn) { writeFn_ = fn; }
    void showWriteResult(uint8_t addr, uint16_t oi, bool ok, const QString& msg);
    void refreshThresholds(uint8_t addr, float hi, float lo);

private:
    void send(uint16_t oi, QLineEdit* edit);
    void refreshFromCurrent();

    QComboBox* addrCombo_;
    QLineEdit* hiEdit_;
    QLineEdit* loEdit_;
    QLabel* resultLabel_;
    WriteThresholdFn writeFn_;
    QVector<uint8_t> addrs_;
};

#endif
