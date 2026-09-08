// 历史查询页: 按时间+地址查 SQLite, 表格显示 (无 Q_OBJECT/moc)
#ifndef HISTORYPAGE_H
#define HISTORYPAGE_H

#include <QWidget>
#include <QVector>
#include <cstdint>
#include "storage/historystore.h"

class QTableView;
class QDateTimeEdit;
class QComboBox;
class QLabel;

class HistoryPage : public QWidget {
public:
    explicit HistoryPage(QWidget* parent = nullptr);
    void setAddresses(const QVector<uint8_t>& addrs);
    void setStore(HistoryStore* store) { store_ = store; }
    void query();   // 由查询按钮触发

private:
    QTableView* table_;
    QDateTimeEdit* fromEdit_;
    QDateTimeEdit* toEdit_;
    QComboBox* addrCombo_;
    QLabel* countLabel_;
    HistoryStore* store_ = nullptr;
};

#endif
