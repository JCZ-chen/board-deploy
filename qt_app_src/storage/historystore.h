// 历史数据存储 (QtSql + SQLite): 建表/批量插入/按时间查询
#ifndef HISTORYSTORE_H
#define HISTORYSTORE_H

#include <QObject>
#include <QSqlDatabase>
#include <QVector>
#include <QDateTime>
#include "comms/metersnapshot.h"

struct HistoryRow {
    qint64 ts;            // unix 秒
    uint8_t addr;
    bool hasLevel; float levelPct;
    bool hasMm; float levelMm;
    uint16_t status; bool hasStatus;
    bool hasThr; float hiThr; float loThr;
};

class HistoryStore : public QObject {
public:
    explicit HistoryStore(QObject* parent = nullptr);
    ~HistoryStore();

    // 打开/创建数据库文件, 建表; 返回是否成功
    bool open(const QString& dbPath, QString* err = nullptr);
    void close();

    // 攒批缓存, 达到阈值或 flush() 时写库 (事务批量, 避免每条 fsync)
    void append(const HistoryRow& row);
    void flush();

    // 查询 [from,to] 时间范围内某地址的记录
    QVector<HistoryRow> queryRange(uint8_t addr, qint64 from, qint64 to);

    QString dbPath() const { return dbPath_; }

private:
    void ensureTable();
    QString dbPath_;
    QString connName_;
    bool opened_ = false;
    QVector<HistoryRow> pending_;
};

#endif
