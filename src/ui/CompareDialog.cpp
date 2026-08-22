#include "CompareDialog.h"
#include "domain/Statistics.h"
#include "i18n/LanguageManager.h"
#include "ui/TitleBar.h"
#include <QtWidgets>
namespace airb {
CompareDialog::CompareDialog(const QList<TestResult>& results, QWidget* parent) : QDialog(parent) {
    auto* l = LanguageManager::instance();
    setWindowTitle(l->trText(QStringLiteral("多站点横向对比")));
    resize(1080, 520);
    auto* layout = new QVBoxLayout(this);
    auto* title = new QLabel(l->trText(QStringLiteral("多站点横向对比")));
    title->setObjectName(QStringLiteral("contentTitle"));
    layout->addWidget(title);
    auto* hint = new QLabel(l->trText(QStringLiteral("按配置名称汇总当前会话中的测试结果；P95 使用线性插值计算。")));
    hint->setObjectName(QStringLiteral("secondaryText"));
    layout->addWidget(hint);
    table_ = new QTableWidget(0, 10);
    table_->setHorizontalHeaderLabels({l->trText(QStringLiteral("站点")), l->trText(QStringLiteral("次数")), l->trText(QStringLiteral("通过率")),
                                       l->trText(QStringLiteral("平均 TTFT")), QStringLiteral("P50 TTFT"), QStringLiteral("P95 TTFT"),
                                       l->trText(QStringLiteral("平均总延迟")), QStringLiteral("P50 总延迟"), QStringLiteral("P95 总延迟"), QStringLiteral("平均速度")});
    table_->setAlternatingRowColors(true);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->verticalHeader()->setVisible(false);
    table_->horizontalHeader()->setStretchLastSection(true);
    layout->addWidget(table_, 1);
    const auto summaries = compareResults(results);
    for (const auto& summary : summaries) {
        const int row = table_->rowCount(); table_->insertRow(row);
        const QStringList values{summary.label, QString::number(summary.total), QStringLiteral("%1%").arg(summary.passRate, 0, 'f', 1),
                                 QString::number(summary.avgTtft, 'f', 1), QString::number(summary.p50Ttft, 'f', 1), QString::number(summary.p95Ttft, 'f', 1),
                                 QString::number(summary.avgLatency, 'f', 1), QString::number(summary.p50Latency, 'f', 1), QString::number(summary.p95Latency, 'f', 1),
                                 QString::number(summary.avgSpeed, 'f', 2)};
        for (int col = 0; col < values.size(); ++col) table_->setItem(row, col, new QTableWidgetItem(values.at(col)));
    }
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}
}
