#include "ScanDialog.h"
#include "services/ScanService.h"
#include "i18n/LanguageManager.h"
#include <QtWidgets>

namespace airb {
ScanDialog::ScanDialog(const Profile& profile, const QString& model, QWidget* parent)
    : QDialog(parent), profile_(profile), model_(model.trimmed()), service_(new ScanService(this)) {
    setObjectName(QStringLiteral("scanDialog"));
    setWindowTitle(LanguageManager::instance()->trText(QStringLiteral("站点协议扫描")));
    resize(860, 560);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 16, 18, 14);
    layout->setSpacing(10);

    auto* heading = new QHBoxLayout;
    title_ = new QLabel(LanguageManager::instance()->trText(QStringLiteral("站点协议扫描")));
    title_->setObjectName(QStringLiteral("contentTitle"));
    profileLabel_ = new QLabel;
    profileLabel_->setObjectName(QStringLiteral("secondaryText"));
    scoreLabel_ = new QLabel(QStringLiteral("—"));
    scoreLabel_->setObjectName(QStringLiteral("scanScore"));
    heading->addWidget(title_);
    heading->addSpacing(14);
    heading->addWidget(profileLabel_, 1);
    heading->addWidget(scoreLabel_);
    layout->addLayout(heading);

    progressLabel_ = new QLabel;
    progressLabel_->setObjectName(QStringLiteral("secondaryText"));
    layout->addWidget(progressLabel_);
    progress_ = new QProgressBar;
    progress_->setRange(0, 0);
    progress_->setTextVisible(false);
    progress_->setFixedHeight(5);
    layout->addWidget(progress_);

    checks_ = new QTableWidget(0, 4);
    checks_->setObjectName(QStringLiteral("scanChecks"));
    checks_->setHorizontalHeaderLabels({
        LanguageManager::instance()->trText(QStringLiteral("检查项")),
        LanguageManager::instance()->trText(QStringLiteral("状态")),
        LanguageManager::instance()->trText(QStringLiteral("HTTP")),
        LanguageManager::instance()->trText(QStringLiteral("诊断详情"))});
    checks_->setSelectionMode(QAbstractItemView::NoSelection);
    checks_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    checks_->setAlternatingRowColors(true);
    checks_->verticalHeader()->setVisible(false);
    checks_->horizontalHeader()->setStretchLastSection(true);
    checks_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    checks_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    checks_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    layout->addWidget(checks_, 1);

    auto* footer = new QHBoxLayout;
    footer->addStretch();
    cancel_ = new QPushButton(LanguageManager::instance()->trText(QStringLiteral("取消")));
    cancel_->setObjectName(QStringLiteral("dangerButton"));
    footer->addWidget(cancel_);
    layout->addLayout(footer);

    profileLabel_->setText(QStringLiteral("%1 · %2 · %3")
                               .arg(profile_.name, protocolName(profile_.protocol),
                                    model_.isEmpty() ? LanguageManager::instance()->trText(QStringLiteral("未指定模型")) : model_));
    connect(cancel_, &QPushButton::clicked, this, &ScanDialog::cancelScan);
    connect(service_, &ScanService::progress, this, &ScanDialog::onProgress);
    connect(service_, &ScanService::checkReady, this, &ScanDialog::onCheck);
    connect(service_, &ScanService::finished, this, &ScanDialog::onFinished);
    service_->scan(profile_, model_);
}

void ScanDialog::onProgress(const QString& message) {
    progressLabel_->setText(message);
}

void ScanDialog::addCheck(const ScanCheck& check) {
    const int row = checks_->rowCount();
    checks_->insertRow(row);
    auto* label = new QTableWidgetItem(check.label);
    auto* status = new QTableWidgetItem(check.status == QStringLiteral("pass") ? QStringLiteral("✓ 通过")
                                         : check.status == QStringLiteral("warn") ? QStringLiteral("! 警告")
                                                                                   : QStringLiteral("× 失败"));
    status->setData(Qt::UserRole, check.status);
    auto* http = new QTableWidgetItem(check.httpStatus > 0 ? QString::number(check.httpStatus) : QStringLiteral("—"));
    auto* detail = new QTableWidgetItem(check.detail);
    for (auto* item : {label, status, http, detail}) item->setToolTip(item->text());
    checks_->setItem(row, 0, label);
    checks_->setItem(row, 1, status);
    checks_->setItem(row, 2, http);
    checks_->setItem(row, 3, detail);
    const auto color = check.status == QStringLiteral("pass") ? QColor(QStringLiteral("#22c55e"))
                       : check.status == QStringLiteral("warn") ? QColor(QStringLiteral("#f59e0b"))
                                                                  : QColor(QStringLiteral("#ef4444"));
    status->setForeground(color);
}

void ScanDialog::onCheck(const ScanCheck& check) {
    addCheck(check);
    progress_->setRange(0, qMax(1, checks_->rowCount() + 1));
    progress_->setValue(checks_->rowCount());
}

void ScanDialog::updateScore(int score) {
    scoreLabel_->setText(LanguageManager::instance()->trText(QStringLiteral("评分 %1/100")).arg(score));
}

void ScanDialog::onFinished(const ScanResult& result) {
    finished_ = true;
    updateScore(result.score());
    progress_->setRange(0, 1);
    progress_->setValue(1);
    progressLabel_->setText(LanguageManager::instance()->trText(QStringLiteral("扫描完成")));
    cancel_->setText(LanguageManager::instance()->trText(QStringLiteral("关闭")));
}

void ScanDialog::cancelScan() {
    if (!finished_) service_->cancel();
    reject();
}
}
