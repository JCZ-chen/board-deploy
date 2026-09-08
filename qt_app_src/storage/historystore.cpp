#include "historystore.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QUuid>

HistoryStore::HistoryStore(QObject* parent) : QObject(parent)
{
    connName_ = "hist_" + QUuid::createUuid().toString(QUuid::WithoutBraces);
}

HistoryStore::~HistoryStore()
{
    flush();
    close();
}

bool HistoryStore::open(const QString& dbPath, QString* err)
{
    dbPath_ = dbPath;
    if (QSqlDatabase::contains(connName_)) QSqlDatabase::removeDatabase(connName_);
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName_);
    db.setDatabaseName(dbPath_);
    if (!db.open()) {
        if (err) *err = db.lastError().text();
        return false;
    }
    opened_ = true;
    ensureTable();
    return true;
}

void HistoryStore::ensureTable()
{
    QSqlDatabase db = QSqlDatabase::database(connName_);
    QSqlQuery q(db);
    q.exec("CREATE TABLE IF NOT EXISTS history("
           "id INTEGER PRIMARY KEY AUTOINCREMENT,"
           "ts INTEGER NOT NULL,"
           "addr INTEGER NOT NULL,"
           "level_pct REAL, level_mm REAL, status INTEGER,"
           "hi_thr REAL, lo_thr REAL)");
    QSqlQuery idx(db);
    idx.exec("CREATE INDEX IF NOT EXISTS idx_hist_addr_ts ON history(addr, ts)");
}

void HistoryStore::close()
{
    if (!opened_) return;
    {
        QSqlDatabase db = QSqlDatabase::database(connName_);
        if (db.isOpen()) db.commit();
    }
    QSqlDatabase::removeDatabase(connName_);
    opened_ = false;
}

void HistoryStore::append(const HistoryRow& row)
{
    pending_.append(row);
    if (pending_.size() >= 20) flush();
}

void HistoryStore::flush()
{
    if (pending_.isEmpty() || !opened_) return;
    QSqlDatabase db = QSqlDatabase::database(connName_);
    QSqlQuery q(db);
    q.exec("BEGIN");
    for (const HistoryRow& r : pending_) {
        q.prepare("INSERT INTO history(ts,addr,level_pct,level_mm,status,hi_thr,lo_thr) "
                  "VALUES(?,?,?,?,?,?,?)");
        q.addBindValue(r.ts);
        q.addBindValue((int)r.addr);
        q.addBindValue(r.hasLevel ? (double)r.levelPct : QVariant());
        q.addBindValue(r.hasMm ? (double)r.levelMm : QVariant());
        q.addBindValue(r.hasStatus ? (int)r.status : QVariant());
        q.addBindValue(r.hasThr ? (double)r.hiThr : QVariant());
        q.addBindValue(r.hasThr ? (double)r.loThr : QVariant());
        q.exec();
    }
    q.exec("COMMIT");
    pending_.clear();
}

QVector<HistoryRow> HistoryStore::queryRange(uint8_t addr, qint64 from, qint64 to)
{
    QVector<HistoryRow> rows;
    if (!opened_) return rows;
    QSqlDatabase db = QSqlDatabase::database(connName_);
    QSqlQuery q(db);
    q.prepare("SELECT ts,addr,level_pct,level_mm,status,hi_thr,lo_thr "
              "FROM history WHERE addr=? AND ts BETWEEN ? AND ? ORDER BY ts");
    q.addBindValue((int)addr);
    q.addBindValue(from);
    q.addBindValue(to);
    if (!q.exec()) return rows;
    while (q.next()) {
        HistoryRow r;
        r.ts = q.value(0).toLongLong();
        r.addr = (uint8_t)q.value(1).toInt();
        r.hasLevel = !q.value(2).isNull(); r.levelPct = q.value(2).toFloat();
        r.hasMm = !q.value(3).isNull(); r.levelMm = q.value(3).toFloat();
        r.hasStatus = !q.value(4).isNull(); r.status = (uint16_t)q.value(4).toInt();
        r.hasThr = !q.value(5).isNull();
        r.hiThr = q.value(5).toFloat(); r.loThr = q.value(6).toFloat();
        rows.append(r);
    }
    return rows;
}
