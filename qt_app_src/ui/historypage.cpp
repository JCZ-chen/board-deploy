#include "historypage.h"
#include "storage/historystore.h"
#include <QTableView>
#include <QDateTimeEdit>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStandardItemModel>
#include <QHeaderView>
#include <QDateTime>

#include <QDebug>

HistoryPage::HistoryPage(QWidget* parent) : QWidget(parent), store_(nullptr)
{
    auto* outer = new QVBoxLayout(this);

    auto* bar = new QHBoxLayout;
    bar->addWidget(new QLabel("地址:"));
    addrCombo_ = new QComboBox;
    bar->addWidget(addrCombo_);

    bar->addWidget(new QLabel("从:"));
    fromEdit_ = new QDateTimeEdit(QDateTime::currentDateTime().addDays(-1));
    fromEdit_->setDisplayFormat("yyyy-MM-dd HH:mm:ss");
    bar->addWidget(fromEdit_);

    bar->addWidget(new QLabel("到:"));
    toEdit_ = new QDateTimeEdit(QDateTime::currentDateTime());
    toEdit_->setDisplayFormat("yyyy-MM-dd HH:mm:ss");
    bar->addWidget(toEdit_);

    auto* btn = new QPushButton("查询");
    QObject::connect(btn, &QPushButton::clicked, [this]{ query(); });
    bar->addWidget(btn);

    countLabel_ = new QLabel;
    bar->addWidget(countLabel_);
    bar->addStretch();
    outer->addLayout(bar);

    table_ = new QTableView;
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);  // 不按内容撑大
    table_->setMaximumHeight(360);                                    // 限制表格高度, 防撑大窗口
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    outer->addWidget(table_);
}

void HistoryPage::setAddresses(const QVector<uint8_t>& addrs)
{
    addrCombo_->clear();
    for (uint8_t a : addrs) addrCombo_->addItem(QString("0x%1").arg(a, 2, 16, QLatin1Char('0')));
}


void HistoryPage::query()
{
    if (!store_) { countLabel_->setText("数据库未就绪"); return; }
    HistoryStore* store = (HistoryStore*)store_;
    uint8_t addr = (uint8_t)addrCombo_->currentText().toInt(nullptr, 16);
    qint64 from = fromEdit_->dateTime().toSecsSinceEpoch();
    qint64 to = toEdit_->dateTime().toSecsSinceEpoch();
    QVector<HistoryRow> rows = store->queryRange(addr, from, to);

    auto* model = new QStandardItemModel(rows.size(), 5, this);
    model->setHeaderData(0, Qt::Horizontal, "时间");
    model->setHeaderData(1, Qt::Horizontal, "油位 %");
    model->setHeaderData(2, Qt::Horizontal, "绝对 mm");
    model->setHeaderData(3, Qt::Horizontal, "状态字");
    model->setHeaderData(4, Qt::Horizontal, "超高/超低");
    for (int i = 0; i < rows.size(); ++i) {
        const HistoryRow& r = rows[i];
        model->setItem(i, 0, new QStandardItem(
            QDateTime::fromSecsSinceEpoch(r.ts).toString("yyyy-MM-dd HH:mm:ss")));
        model->setItem(i, 1, new QStandardItem(r.hasLevel ? QString::number(r.levelPct,'f',1) : "-"));
        model->setItem(i, 2, new QStandardItem(r.hasMm ? QString::number(r.levelMm,'f',1) : "-"));
        model->setItem(i, 3, new QStandardItem(r.hasStatus ? QString("0x%1").arg(r.status,4,16,QLatin1Char('0')) : "-"));
        model->setItem(i, 4, new QStandardItem(r.hasThr ? QString("%1 / %2").arg(r.hiThr).arg(r.loThr) : "-"));
    }
    table_->setModel(model);
    table_->horizontalHeader()->setStretchLastSection(true);
    countLabel_->setText(QString("共 %1 条").arg(rows.size()));
}
