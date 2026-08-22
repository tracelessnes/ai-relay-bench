#include "ResultRepository.h"
#include <QCoreApplication>
#include <QDir>
#include <QJsonDocument>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <algorithm>

namespace airb {
ResultRepository::ResultRepository(QObject* parent):QObject(parent),connectionName_("results-"+QUuid::createUuid().toString(QUuid::WithoutBraces)){}
ResultRepository::~ResultRepository(){if(db_.isValid()){const auto name=db_.connectionName();db_.close();db_=QSqlDatabase();QSqlDatabase::removeDatabase(name);}}
bool ResultRepository::open(QString* error){
    auto dir=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if(dir.isEmpty())dir=QCoreApplication::applicationDirPath()+"/data";
    if(!QDir().mkpath(dir)){if(error)*error="Cannot create application data directory: "+dir;return false;}
    path_=QDir(dir).filePath("benchmark-history.sqlite"); db_=QSqlDatabase::addDatabase("QSQLITE",connectionName_); db_.setDatabaseName(path_);
    if(!db_.open()){if(error)*error=db_.lastError().text();return false;} return ensureSchema(error);
}
bool ResultRepository::ensureSchema(QString* error){
    QSqlQuery q(db_); if(!q.exec("PRAGMA journal_mode=WAL")){} if(!q.exec("PRAGMA synchronous=NORMAL")){}
    const char* sql="CREATE TABLE IF NOT EXISTS results (id TEXT PRIMARY KEY, timestamp_ms INTEGER NOT NULL, profile TEXT, model TEXT, protocol TEXT, status TEXT, passed INTEGER, json BLOB NOT NULL)";
    if(!q.exec(sql)){if(error)*error=q.lastError().text();return false;}
    if(!q.exec("CREATE INDEX IF NOT EXISTS idx_results_timestamp ON results(timestamp_ms DESC)")){if(error)*error=q.lastError().text();return false;}
    return true;
}
bool ResultRepository::append(const TestResult& result,QString* error){
    if(!db_.isOpen()&&!open(error))return false; QSqlQuery q(db_);
    q.prepare("INSERT OR REPLACE INTO results(id,timestamp_ms,profile,model,protocol,status,passed,json) VALUES(?,?,?,?,?,?,?,?)");
    q.addBindValue(result.id);q.addBindValue(result.timestamp.toMSecsSinceEpoch());q.addBindValue(result.profileName);q.addBindValue(result.model);
    q.addBindValue(protocolName(result.protocol));q.addBindValue(statusName(result.status));q.addBindValue(result.passed?1:0);
    q.addBindValue(QJsonDocument(toJson(result,true)).toJson(QJsonDocument::Compact));
    if(!q.exec()){if(error)*error=q.lastError().text();return false;}return true;
}
QList<TestResult> ResultRepository::loadRecent(int limit,QString* error)const{
    QList<TestResult> out;if(!db_.isOpen())return out;QSqlQuery q(db_);q.prepare("SELECT json FROM results ORDER BY timestamp_ms DESC LIMIT ?");q.addBindValue(qBound(1,limit,10000));
    if(!q.exec()){if(error)*error=q.lastError().text();return out;}while(q.next()){QJsonParseError pe;auto d=QJsonDocument::fromJson(q.value(0).toByteArray(),&pe);if(pe.error==QJsonParseError::NoError&&d.isObject())out.push_back(testResultFromJson(d.object()));}std::reverse(out.begin(),out.end());return out;
}
bool ResultRepository::clear(QString* error){if(!db_.isOpen())return true;QSqlQuery q(db_);if(!q.exec("DELETE FROM results")){if(error)*error=q.lastError().text();return false;}return true;}
}
