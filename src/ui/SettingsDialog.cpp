#include "SettingsDialog.h"
#include "i18n/LanguageManager.h"
#include "ThemeManager.h"
#include "ui/TitleBar.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QSignalBlocker>
#include <QUrl>
#include <QVBoxLayout>
#include <QWindow>

namespace airb {

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
    setObjectName(QStringLiteral("settingsDialog"));
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint);
    setModal(true);
    resize(620, 390);
    setMinimumSize(560, 350);

    auto* shell = new QVBoxLayout(this);
    shell->setContentsMargins(0, 0, 0, 0);
    shell->setSpacing(0);
    titleBar_ = new DialogTitleBar(this, this);
    shell->addWidget(titleBar_);

    auto* body = new QWidget(this);
    body->setObjectName(QStringLiteral("settingsBody"));
    auto* root = new QVBoxLayout(body);
    root->setContentsMargins(24, 22, 24, 20);
    root->setSpacing(16);
    shell->addWidget(body, 1);

    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setHorizontalSpacing(18);
    form->setVerticalSpacing(14);

    languageLabel_ = new QLabel;
    language_ = new QComboBox;
    language_->setMinimumWidth(310);
    followSystem_ = new QCheckBox;
    themeLabel_ = new QLabel;
    theme_ = new QComboBox;
    theme_->addItem(QString(), QStringLiteral("system"));
    theme_->addItem(QString(), QStringLiteral("dark"));
    theme_->addItem(QString(), QStringLiteral("light"));
    systemLabel_ = new QLabel;
    systemLanguage_ = new QLabel;
    sourceLabel_ = new QLabel;
    source_ = new QLabel;
    source_->setWordWrap(true);

    form->addRow(languageLabel_, language_);
    form->addRow(QString(), followSystem_);
    form->addRow(themeLabel_, theme_);
    form->addRow(systemLabel_, systemLanguage_);
    form->addRow(sourceLabel_, source_);
    root->addLayout(form);

    hint_ = new QLabel;
    hint_->setObjectName(QStringLiteral("secondaryText"));
    hint_->setWordWrap(true);
    root->addWidget(hint_);
    root->addStretch();

    auto* actions = new QHBoxLayout;
    import_ = new QPushButton;
    openDir_ = new QPushButton;
    reset_ = new QPushButton;
    close_ = new QPushButton;
    close_->setObjectName(QStringLiteral("primaryButton"));
    actions->addWidget(import_);
    actions->addWidget(openDir_);
    actions->addWidget(reset_);
    actions->addStretch();
    actions->addWidget(close_);
    root->addLayout(actions);

    connect(language_, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::applyLanguage);
    connect(followSystem_, &QCheckBox::toggled, this, [this](bool on) {
        LanguageManager::instance()->setFollowSystem(on);
        refreshLanguageList();
        retranslate();
    });
    connect(theme_, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::applyTheme);
    connect(import_, &QPushButton::clicked, this, &SettingsDialog::importLanguage);
    connect(openDir_, &QPushButton::clicked, this, [] {
        auto* language = LanguageManager::instance();
        QDir().mkpath(language->languageDirectory());
        QDesktopServices::openUrl(QUrl::fromLocalFile(language->languageDirectory()));
    });
    connect(reset_, &QPushButton::clicked, this, [this] {
        LanguageManager::instance()->setFollowSystem(true);
        refreshLanguageList();
        retranslate();
    });
    connect(close_, &QPushButton::clicked, this, &QDialog::accept);
    connect(LanguageManager::instance(), &LanguageManager::languageChanged,
            this, &SettingsDialog::retranslate);

    refreshLanguageList();
    theme_->setCurrentIndex(theme_->findData(ThemeManager::instance()->modeId()));
    retranslate();
}

void SettingsDialog::refreshLanguageList() {
    auto* manager = LanguageManager::instance();
    QSignalBlocker languageBlocker(language_);
    language_->clear();
    language_->addItem(manager->trText(QStringLiteral("跟随系统")), QStringLiteral("system"));
    for (const auto& id : manager->availableLanguages())
        language_->addItem(manager->languageNativeName(id) + QStringLiteral(" (") + id + QStringLiteral(")"), id);
    const QString id = manager->followSystem() ? QStringLiteral("system") : manager->currentLanguage();
    language_->setCurrentIndex(qMax(0, language_->findData(id)));
    QSignalBlocker followBlocker(followSystem_);
    followSystem_->setChecked(manager->followSystem());
}

void SettingsDialog::applyLanguage(int index) {
    if (index < 0) return;
    const QString id = language_->itemData(index).toString();
    if (id == QStringLiteral("system")) LanguageManager::instance()->setFollowSystem(true);
    else if (!id.isEmpty()) LanguageManager::instance()->setLanguage(id);
    refreshLanguageList();
    retranslate();
}

void SettingsDialog::applyTheme(int index) {
    if (index >= 0) ThemeManager::instance()->setModeId(theme_->itemData(index).toString());
    retranslate();
}

void SettingsDialog::importLanguage() {
    auto* language = LanguageManager::instance();
    const QString path = QFileDialog::getOpenFileName(
        this, language->trText(QStringLiteral("导入语言包")), {},
        language->trText(QStringLiteral("JSON (*.json)")));
    if (path.isEmpty()) return;
    QString error, installedId;
    if (!language->installLanguagePack(path, &error, &installedId)) {
        QMessageBox::warning(this, language->trText(QStringLiteral("导入失败")),
                             language->trText(error));
        return;
    }
    language->setLanguage(installedId);
    refreshLanguageList();
    QMessageBox::information(this, language->trText(QStringLiteral("导入成功")),
                             language->trText(QStringLiteral("语言包已安装并启用。")));
}

void SettingsDialog::retranslate() {
    auto* language = LanguageManager::instance();
    const QString title = language->trText(QStringLiteral("系统设置"));
    setWindowTitle(title);
    if (titleBar_) {
        titleBar_->setTitle(title);
        titleBar_->setCloseToolTip(language->trText(QStringLiteral("\u5173\u95ed")));
    }
    languageLabel_->setText(language->trText(QStringLiteral("界面语言")));
    followSystem_->setText(language->trText(QStringLiteral("自动跟随系统语言")));
    themeLabel_->setText(language->trText(QStringLiteral("情景皮肤")));
    systemLabel_->setText(language->trText(QStringLiteral("系统语言")));
    const QLocale system = QLocale::system();
    systemLanguage_->setText(system.nativeLanguageName() + QStringLiteral(" · ") + system.name());
    sourceLabel_->setText(language->trText(QStringLiteral("语言包来源")));
    source_->setText(language->languageName(language->currentLanguage()) + QStringLiteral(" · ")
                     + language->currentLanguage() + QStringLiteral(" · ")
                     + language->trText(language->languageSource(language->currentLanguage()) == QStringLiteral("user")
                                            ? QStringLiteral("用户语言包") : QStringLiteral("内置语言包")));
    hint_->setText(language->trText(QStringLiteral("支持导入 JSON 语言包；语言包保存在应用数据目录，用户可自行扩展。切换后界面会立即刷新。")));
    import_->setText(language->trText(QStringLiteral("导入语言包")));
    openDir_->setText(language->trText(QStringLiteral("打开语言包目录")));
    reset_->setText(language->trText(QStringLiteral("恢复跟随系统")));
    close_->setText(language->trText(QStringLiteral("关闭")));
    if (language_->count()) language_->setItemText(0, language->trText(QStringLiteral("跟随系统")));
    theme_->setItemText(0, language->trText(QStringLiteral("跟随系统")));
    theme_->setItemText(1, language->trText(QStringLiteral("黑夜")));
    theme_->setItemText(2, language->trText(QStringLiteral("白天")));
}
}
