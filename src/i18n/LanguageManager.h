#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <QVariantMap>

class QWidget;

namespace airb {

class LanguageManager final : public QObject {
    Q_OBJECT
public:
    static LanguageManager* instance();

    QString currentLanguage() const;
    QString currentLanguageName() const;
    QString systemLanguage() const;
    bool followSystem() const;
    QStringList availableLanguages() const;
    QString languageName(const QString& id) const;
    QString languageNativeName(const QString& id) const;
    QString languageSource(const QString& id) const;

    QString trText(const QString& source, const QVariantMap& args = {}) const;
    QString trKey(const QString& key, const QString& fallback = {}) const;

    bool setLanguage(const QString& id, bool persist = true);
    void setFollowSystem(bool enabled, bool persist = true);
    bool installLanguagePack(const QString& filePath, QString* error = nullptr,
                             QString* installedId = nullptr);
    QString languageDirectory() const;
    void refresh();
    void retranslate(QWidget* root);

signals:
    void languageChanged();

private:
    explicit LanguageManager(QObject* parent = nullptr);

    struct Pack {
        QString id;
        QString name;
        QString nativeName;
        int version = 1;
        QJsonObject strings;
        QString source;
    };

    void loadSettings();
    void loadPack(const QString& id, const QString& source, const QByteArray& data);
    void loadBuiltinPacks();
    void loadUserPacks();
    QString normalizeLocale(const QString& locale) const;
    QString chooseSystemLanguage() const;
    QString valueFor(const QString& source) const;
    QString valueForKey(const QString& key) const;

    QHash<QString, Pack> packs_;
    QString currentId_;
    QString requestedId_;
    bool followSystem_ = true;
};

} // namespace airb
