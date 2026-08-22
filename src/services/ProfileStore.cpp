#include "ProfileStore.h"
#include "persistence/SecureStorage.h"
#include <QSettings>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>

namespace airb {
ProfileStore::ProfileStore(QObject* parent):QObject(parent){}
QList<Profile> ProfileStore::load() const {
    const auto bytes=QSettings().value("profiles/json").toByteArray();QList<Profile> out;const auto array=QJsonDocument::fromJson(bytes).array();
    for(const auto& value:array){if(value.isString()){const auto encrypted=QByteArray::fromBase64(value.toString().toLatin1());const auto plain=SecureStorage::unprotect(encrypted).toUtf8();const auto object=QJsonDocument::fromJson(plain).object();if(!object.isEmpty())out.push_back(profileFromJson(object));continue;}auto object=value.toObject();const auto encrypted=QByteArray::fromBase64(object.take("apiKeyEncrypted").toString().toLatin1());auto profile=profileFromJson(object);profile.apiKey=SecureStorage::unprotect(encrypted);out.push_back(profile);}return out;
}
void ProfileStore::save(const QList<Profile>& profiles) const {QJsonArray array;for(const auto& profile:profiles){const QString json=QString::fromUtf8(QJsonDocument(toJson(profile)).toJson(QJsonDocument::Compact));array.push_back(QString::fromLatin1(SecureStorage::protect(json).toBase64()));}QSettings().setValue("profiles/json",QJsonDocument(array).toJson(QJsonDocument::Compact));}
bool ProfileStore::exportFile(const QString&path,const QList<Profile>&profiles,QString*error){QJsonArray a;for(auto p:profiles){p.apiKey.clear();p.proxy.password.clear();a.push_back(toJson(p));}QJsonObject root{{"format","ai-relay-bench-profiles"},{"version",1},{"warning","Secrets intentionally omitted"},{"profiles",a}};QFile f(path);if(!f.open(QIODevice::WriteOnly)){if(error)*error=f.errorString();return false;}if(f.write(QJsonDocument(root).toJson(QJsonDocument::Indented))<0){if(error)*error=f.errorString();return false;}return true;}
QList<Profile> ProfileStore::importFile(const QString&path,QString*error){QFile f(path);if(!f.open(QIODevice::ReadOnly)){if(error)*error=f.errorString();return{};}QJsonParseError pe;const auto d=QJsonDocument::fromJson(f.readAll(),&pe);if(pe.error!=QJsonParseError::NoError||!d.isObject()){if(error)*error=pe.errorString();return{};}QList<Profile> out;for(const auto&v:d.object().value("profiles").toArray()){auto p=profileFromJson(v.toObject());p.id=QUuid::createUuid().toString(QUuid::WithoutBraces);if(!p.name.isEmpty())out.push_back(p);}if(out.isEmpty()&&error)*error="No profiles found in file";return out;}
}
