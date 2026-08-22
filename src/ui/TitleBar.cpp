#include "TitleBar.h"
#include "ThemeManager.h"
#include "i18n/LanguageManager.h"
#include <QApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QWindow>

namespace airb {
TitleBar::TitleBar(QWidget* window, QWidget* parent) : QWidget(parent), window_(window) {
    setObjectName(QStringLiteral("titleBar")); setFixedHeight(42); setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* layout = new QHBoxLayout(this); layout->setContentsMargins(12, 0, 6, 0); layout->setSpacing(2);
    auto* mark = new QLabel(QStringLiteral("AI")); mark->setAlignment(Qt::AlignCenter); mark->setFixedSize(28, 28); mark->setStyleSheet(QStringLiteral("background:#2563eb;color:white;border-radius:7px;font-weight:800;"));
    title_ = new QLabel; title_->setObjectName(QStringLiteral("windowTitleLabel"));
    theme_ = new QPushButton; settings_ = new QPushButton; minimize_ = new QPushButton(QStringLiteral("—")); maximize_ = new QPushButton; close_ = new QPushButton(QStringLiteral("×"));
    for (auto* b : {theme_, settings_, minimize_, maximize_, close_}) { b->setObjectName(QStringLiteral("titleBarButton")); b->setFlat(true); b->setCursor(Qt::PointingHandCursor); b->setFixedHeight(32); }
    close_->setObjectName(QStringLiteral("titleBarClose")); theme_->setFixedWidth(42); settings_->setFixedWidth(42); minimize_->setFixedWidth(42); maximize_->setFixedWidth(42); close_->setFixedWidth(46);
    layout->addWidget(mark); layout->addSpacing(8); layout->addWidget(title_); layout->addStretch(); layout->addWidget(theme_); layout->addWidget(settings_); layout->addWidget(minimize_); layout->addWidget(maximize_); layout->addWidget(close_);
    connect(theme_, &QPushButton::clicked, this, &TitleBar::themeToggleRequested); connect(settings_, &QPushButton::clicked, this, &TitleBar::settingsRequested);
    connect(minimize_, &QPushButton::clicked, window_, &QWidget::showMinimized); connect(maximize_, &QPushButton::clicked, this, [this]{ window_->isMaximized() ? window_->showNormal() : window_->showMaximized(); updateWindowState(); }); connect(close_, &QPushButton::clicked, window_, &QWidget::close);
    if (window_) window_->installEventFilter(this); retranslate(); updateWindowState();
}
void TitleBar::retranslate() {
    auto* l = LanguageManager::instance(); title_->setText(l->trText(QStringLiteral("AI 中转站测速工具")) + QStringLiteral("  v") + qApp->applicationVersion());
    theme_->setToolTip(l->trText(QStringLiteral("切换黑夜/白天主题"))); settings_->setToolTip(l->trText(QStringLiteral("系统设置"))); minimize_->setToolTip(l->trText(QStringLiteral("最小化"))); maximize_->setToolTip(window_ && window_->isMaximized() ? l->trText(QStringLiteral("还原")) : l->trText(QStringLiteral("最大化"))); close_->setToolTip(l->trText(QStringLiteral("关闭")));
    theme_->setText(ThemeManager::instance()->effectiveTheme() == QStringLiteral("dark") ? QStringLiteral("☀") : QStringLiteral("☾")); settings_->setText(QStringLiteral("⚙"));
}
void TitleBar::updateWindowState() { maximize_->setText(window_ && window_->isMaximized() ? QStringLiteral("❐") : QStringLiteral("□")); retranslate(); }
void TitleBar::mousePressEvent(QMouseEvent* event) { if (event->button() == Qt::LeftButton && window_ && window_->windowHandle()) { window_->windowHandle()->startSystemMove(); event->accept(); return; } QWidget::mousePressEvent(event); }
void TitleBar::mouseDoubleClickEvent(QMouseEvent* event) { if (event->button() == Qt::LeftButton && window_) { window_->isMaximized() ? window_->showNormal() : window_->showMaximized(); updateWindowState(); event->accept(); return; } QWidget::mouseDoubleClickEvent(event); }
bool TitleBar::eventFilter(QObject* watched, QEvent* event) { if (watched == window_ && event->type() == QEvent::WindowStateChange) updateWindowState(); return QWidget::eventFilter(watched, event); }
}
