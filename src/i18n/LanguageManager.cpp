#include "LanguageManager.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QGroupBox>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTableWidget>
#include <QWidget>
#include <algorithm>

namespace airb {
namespace {
LanguageManager* g_manager = nullptr;
constexpr qint64 kMaxPackBytes = 2 * 1024 * 1024;
constexpr int kMaxPackStrings = 20000;
}

LanguageManager* LanguageManager::instance() {
    if (!g_manager) g_manager = new LanguageManager(qApp);
    return g_manager;
}

LanguageManager::LanguageManager(QObject* parent) : QObject(parent) { refresh(); }

QString LanguageManager::languageDirectory() const {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + QStringLiteral("/languages");
}

QString LanguageManager::normalizeLocale(const QString& locale) const {
    QString value = locale.trimmed();
    value.replace(QLatin1Char('_'), QLatin1Char('-'));
    const auto parts = value.split(QLatin1Char('-'), Qt::SkipEmptyParts);
    if (parts.isEmpty() || parts.first().compare(QStringLiteral("C"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("en-US");

    QStringList normalized;
    normalized << parts.first().toLower();
    for (int i = 1; i < parts.size(); ++i) {
        const QString part = parts.at(i);
        if (part.size() == 4 && part.front().isLetter())
            normalized << (part.left(1).toUpper() + part.mid(1).toLower());
        else if ((part.size() == 2 && part.front().isLetter())
                 || (part.size() == 3 && part.front().isDigit()))
            normalized << part.toUpper();
        else
            normalized << part.toLower();
    }
    return normalized.join(QLatin1Char('-'));
}

QString LanguageManager::systemLanguage() const { return chooseSystemLanguage(); }

QString LanguageManager::chooseSystemLanguage() const {
    QStringList candidates = QLocale::system().uiLanguages();
    candidates << QLocale::system().name();
    for (const auto& candidate : candidates) {
        const QString locale = normalizeLocale(candidate);
        if (packs_.contains(locale)) return locale;
    }
    for (const auto& candidate : candidates) {
        const QString base = normalizeLocale(candidate).section(QLatin1Char('-'), 0, 0);
        auto ids = packs_.keys();
        std::sort(ids.begin(), ids.end());
        for (const auto& id : ids)
            if (id.section(QLatin1Char('-'), 0, 0).compare(base, Qt::CaseInsensitive) == 0)
                return id;
    }
    if (packs_.contains(QStringLiteral("en-US"))) return QStringLiteral("en-US");
    return packs_.isEmpty() ? QStringLiteral("en-US") : packs_.cbegin().key();
}

void LanguageManager::loadPack(const QString& id, const QString& source,
                               const QByteArray& data) {
    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) return;
    const auto root = doc.object();
    const QString packId = normalizeLocale(root.value(QStringLiteral("language")).toString(id));
    const auto strings = root.value(QStringLiteral("strings")).toObject();
    if (packId.isEmpty() || strings.isEmpty()) return;
    Pack pack;
    pack.id = packId;
    pack.name = root.value(QStringLiteral("name")).toString(packId);
    pack.nativeName = root.value(QStringLiteral("nativeName")).toString(pack.name);
    pack.version = root.value(QStringLiteral("version")).toInt(1);
    pack.strings = strings;
    pack.source = source;
    packs_.insert(packId, pack);
}

void LanguageManager::loadBuiltinPacks() {
    const QStringList ids = {QStringLiteral("zh-CN"), QStringLiteral("en-US")};
    for (const auto& id : ids) {
        QFile file(QStringLiteral(":/lang/") + id + QStringLiteral(".json"));
        if (file.open(QIODevice::ReadOnly))
            loadPack(id, QStringLiteral("builtin"), file.readAll());
    }
}

void LanguageManager::loadUserPacks() {
    QDir dir(languageDirectory());
    if (!dir.exists()) return;
    const auto files = dir.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    for (const auto& fileName : files) {
        QFile file(dir.filePath(fileName));
        if (file.open(QIODevice::ReadOnly))
            loadPack(fileName, QStringLiteral("user"), file.readAll());
    }
}

void LanguageManager::loadSettings() {
    QSettings settings;
    const QString stored = settings.value(QStringLiteral("ui/language"), QString()).toString();
    requestedId_ = stored.isEmpty() ? QString() : normalizeLocale(stored);
    followSystem_ = settings.value(QStringLiteral("ui/languageAutoDetect"), true).toBool();
    if (followSystem_ || requestedId_.isEmpty() || !packs_.contains(requestedId_))
        currentId_ = chooseSystemLanguage();
    else
        currentId_ = requestedId_;
}

void LanguageManager::refresh() {
    packs_.clear();
    loadBuiltinPacks();
    loadUserPacks();
    loadSettings();
}

QString LanguageManager::currentLanguage() const { return currentId_; }
QString LanguageManager::currentLanguageName() const { return languageName(currentId_); }
QString LanguageManager::languageName(const QString& id) const {
    const QString normalized = normalizeLocale(id);
    return packs_.contains(normalized) ? packs_.value(normalized).name : id;
}
QString LanguageManager::languageNativeName(const QString& id) const {
    const QString normalized = normalizeLocale(id);
    return packs_.contains(normalized) ? packs_.value(normalized).nativeName : id;
}
QString LanguageManager::languageSource(const QString& id) const {
    return packs_.value(normalizeLocale(id)).source;
}
QStringList LanguageManager::availableLanguages() const {
    auto ids = packs_.keys();
    std::sort(ids.begin(), ids.end());
    return ids;
}
bool LanguageManager::followSystem() const { return followSystem_; }

QString LanguageManager::valueFor(const QString& source) const {
    const auto direct = packs_.value(currentId_).strings.value(source);
    if (direct.isString()) return direct.toString();
    const auto prefixed = packs_.value(currentId_).strings.value(QStringLiteral("source:") + source);
    if (prefixed.isString()) return prefixed.toString();
    if (currentId_ != QStringLiteral("en-US")) {
        const auto fallback = packs_.value(QStringLiteral("en-US")).strings.value(source);
        if (fallback.isString()) return fallback.toString();
    }
    return source;
}

QString LanguageManager::valueForKey(const QString& key) const {
    const auto value = packs_.value(currentId_).strings.value(key);
    if (value.isString()) return value.toString();
    if (currentId_ != QStringLiteral("en-US")) {
        const auto fallback = packs_.value(QStringLiteral("en-US")).strings.value(key);
        if (fallback.isString()) return fallback.toString();
    }
    return key;
}

QString LanguageManager::trText(const QString& source, const QVariantMap& args) const {
    QString result = valueFor(source);
    for (auto it = args.cbegin(); it != args.cend(); ++it)
        result.replace(QStringLiteral("{%1}").arg(it.key()), it.value().toString());
    return result;
}

QString LanguageManager::trKey(const QString& key, const QString& fallback) const {
    const QString value = valueForKey(key);
    return value == key ? fallback : value;
}

bool LanguageManager::setLanguage(const QString& id, bool persist) {
    const QString normalized = normalizeLocale(id);
    if (!packs_.contains(normalized)) return false;
    const bool changed = followSystem_ || currentId_ != normalized;
    followSystem_ = false;
    currentId_ = normalized;
    requestedId_ = normalized;
    if (persist) {
        QSettings settings;
        settings.setValue(QStringLiteral("ui/language"), normalized);
        settings.setValue(QStringLiteral("ui/languageAutoDetect"), false);
    }
    if (changed) emit languageChanged();
    return true;
}

void LanguageManager::setFollowSystem(bool enabled, bool persist) {
    const QString oldLanguage = currentId_;
    const bool oldFollow = followSystem_;
    followSystem_ = enabled;
    if (enabled) currentId_ = chooseSystemLanguage();
    if (persist) {
        QSettings settings;
        settings.setValue(QStringLiteral("ui/languageAutoDetect"), enabled);
        if (enabled) settings.remove(QStringLiteral("ui/language"));
    }
    if (oldFollow != followSystem_ || oldLanguage != currentId_) emit languageChanged();
}

bool LanguageManager::installLanguagePack(const QString& filePath, QString* error,
                                          QString* installedId) {
    QFile input(filePath);
    if (!input.open(QIODevice::ReadOnly)) {
        if (error) *error = input.errorString();
        return false;
    }
    const QByteArray bytes = input.read(kMaxPackBytes + 1);
    if (bytes.size() > kMaxPackBytes) {
        if (error) *error = QStringLiteral("语言包大小不能超过 2 MB");
        return false;
    }
    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error) *error = QStringLiteral("JSON 解析失败：%1").arg(parseError.errorString());
        return false;
    }
    const auto object = doc.object();
    const QString rawId = object.value(QStringLiteral("language")).toString();
    const auto strings = object.value(QStringLiteral("strings")).toObject();
    if (rawId.isEmpty() || strings.isEmpty()) {
        if (error) *error = QStringLiteral("语言包必须包含 language 与 strings");
        return false;
    }
    if (object.value(QStringLiteral("version")).toInt(1) != 1) {
        if (error) *error = QStringLiteral("不支持的语言包版本");
        return false;
    }
    if (strings.size() > kMaxPackStrings) {
        if (error) *error = QStringLiteral("语言包条目过多");
        return false;
    }
    for (auto it = strings.constBegin(); it != strings.constEnd(); ++it) {
        if (!it.value().isString()) {
            if (error) *error = QStringLiteral("语言包 strings 中的所有值都必须是字符串");
            return false;
        }
        if (it.key().size() > 1024 || it.value().toString().size() > 65536) {
            if (error) *error = QStringLiteral("语言包包含过长的键或文本");
            return false;
        }
    }
    const QString id = normalizeLocale(rawId);
    const QRegularExpression pattern(QStringLiteral("^[A-Za-z]{2,3}(?:-[A-Za-z0-9]{2,8})*$"));
    if (!pattern.match(id).hasMatch()) {
        if (error) *error = QStringLiteral("language 标识不合法");
        return false;
    }
    if (!QDir().mkpath(languageDirectory())) {
        if (error) *error = QStringLiteral("无法创建语言包目录");
        return false;
    }
    QSaveFile output(QDir(languageDirectory()).filePath(id + QStringLiteral(".json")));
    if (!output.open(QIODevice::WriteOnly)) {
        if (error) *error = output.errorString();
        return false;
    }
    if (output.write(bytes) != bytes.size() || !output.commit()) {
        if (error) *error = output.errorString();
        return false;
    }
    refresh();
    if (installedId) *installedId = id;
    emit languageChanged();
    return true;
}

void LanguageManager::retranslate(QWidget* root) {
    if (!root) return;
    const auto widgets = root->findChildren<QWidget*>(QString(), Qt::FindChildrenRecursively);
    for (QWidget* widget : widgets) {
        if (widget->property("i18nIgnore").toBool()) continue;
        QString source = widget->property("i18nSource").toString();
        if (source.isEmpty()) {
            if (auto* label = qobject_cast<QLabel*>(widget)) source = label->text();
            else if (auto* button = qobject_cast<QPushButton*>(widget)) source = button->text();
            else if (auto* check = qobject_cast<QCheckBox*>(widget)) source = check->text();
            else if (auto* group = qobject_cast<QGroupBox*>(widget)) source = group->title();
            if (!source.isEmpty()) widget->setProperty("i18nSource", source);
        }
        if (source.isEmpty()) continue;
        const QString translated = trText(source);
        if (auto* label = qobject_cast<QLabel*>(widget)) label->setText(translated);
        else if (auto* button = qobject_cast<QPushButton*>(widget)) button->setText(translated);
        else if (auto* check = qobject_cast<QCheckBox*>(widget)) check->setText(translated);
        else if (auto* group = qobject_cast<QGroupBox*>(widget)) group->setTitle(translated);
    }
    for (auto* combo : root->findChildren<QComboBox*>()) {
        if (!combo->property("i18nTranslateItems").toBool()) continue;
        QStringList sources = combo->property("i18nItems").toStringList();
        if (sources.isEmpty()) {
            for (int i = 0; i < combo->count(); ++i) sources << combo->itemText(i);
            combo->setProperty("i18nItems", sources);
        }
        for (int i = 0; i < sources.size() && i < combo->count(); ++i)
            combo->setItemText(i, trText(sources.at(i)));
    }
    for (auto* tab : root->findChildren<QTabWidget*>()) {
        QStringList sources = tab->property("i18nTabs").toStringList();
        if (sources.isEmpty()) {
            for (int i = 0; i < tab->count(); ++i) sources << tab->tabText(i);
            tab->setProperty("i18nTabs", sources);
        }
        for (int i = 0; i < sources.size() && i < tab->count(); ++i)
            tab->setTabText(i, trText(sources.at(i)));
    }
    for (auto* table : root->findChildren<QTableWidget*>()) {
        QStringList sources = table->property("i18nHeaders").toStringList();
        if (sources.isEmpty()) {
            for (int i = 0; i < table->columnCount(); ++i)
                sources << (table->horizontalHeaderItem(i) ? table->horizontalHeaderItem(i)->text() : QString());
            table->setProperty("i18nHeaders", sources);
        }
        for (int i = 0; i < sources.size() && i < table->columnCount(); ++i)
            if (table->horizontalHeaderItem(i))
                table->horizontalHeaderItem(i)->setText(trText(sources.at(i)));
    }
}

} // namespace airb
