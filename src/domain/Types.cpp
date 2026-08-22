#include "Types.h"
#include <QRegularExpression>
#include <QJsonDocument>

namespace airb {
QString protocolName(Protocol p) {
    switch (p) { case Protocol::OpenAI: return "OpenAI"; case Protocol::Claude: return "Claude"; case Protocol::Codex: return "Codex"; }
    return "OpenAI";
}
Protocol protocolFromString(const QString& s) {
    const auto x = s.trimmed().toLower();
    if (x == "claude") return Protocol::Claude;
    if (x == "codex") return Protocol::Codex;
    return Protocol::OpenAI;
}
QString statusName(TestStatus s) {
    switch (s) {
    case TestStatus::Passed: return "Passed"; case TestStatus::Failed: return "Failed";
    case TestStatus::Timeout: return "Timeout"; case TestStatus::Cancelled: return "Cancelled";
    case TestStatus::Connecting: return "Connecting"; case TestStatus::WaitingFirstToken: return "Waiting first token";
    case TestStatus::Streaming: return "Streaming"; case TestStatus::Queued: return "Queued";
    default: return "Error";
    }
}
TestStatus statusFromString(const QString& s) {
    const auto x=s.trimmed().toLower();
    if(x=="passed")return TestStatus::Passed; if(x=="failed")return TestStatus::Failed;
    if(x=="timeout")return TestStatus::Timeout; if(x=="cancelled")return TestStatus::Cancelled;
    if(x=="connecting")return TestStatus::Connecting; if(x=="waiting first token")return TestStatus::WaitingFirstToken;
    if(x=="streaming")return TestStatus::Streaming; if(x=="queued")return TestStatus::Queued;
    return TestStatus::Error;
}

QJsonObject toJson(const Profile& p) {
    QJsonObject o{{"id",p.id},{"name",p.name},{"baseUrl",p.baseUrl.toString()},{"apiKey",p.apiKey},
                  {"protocol",protocolName(p.protocol)},{"defaultModel",p.defaultModel},{"customHeaders",p.customHeaders},
                  {"timeoutSeconds",p.timeoutSeconds},{"context1M",p.context1M}};
    o["proxy"] = QJsonObject{{"mode",int(p.proxy.mode)},{"host",p.proxy.host},{"port",int(p.proxy.port)},
                              {"username",p.proxy.username},{"password",p.proxy.password}};
    return o;
}
Profile profileFromJson(const QJsonObject& o) {
    Profile p;
    p.id=o.value("id").toString(p.id); p.name=o.value("name").toString(p.name);
    p.baseUrl=QUrl(o.value("baseUrl").toString()); p.apiKey=o.value("apiKey").toString();
    p.protocol=protocolFromString(o.value("protocol").toString()); p.defaultModel=o.value("defaultModel").toString();
    p.customHeaders=o.value("customHeaders").toObject(); p.timeoutSeconds=qBound(5,o.value("timeoutSeconds").toInt(15),60);
    p.context1M=o.value("context1M").toBool(false);
    const auto x=o.value("proxy").toObject(); const int mode=x.value("mode").toInt(int(ProxyMode::System));
    p.proxy.mode=ProxyMode(qBound(0,mode,3)); p.proxy.host=x.value("host").toString();
    p.proxy.port=quint16(qBound(0,x.value("port").toInt(),65535)); p.proxy.username=x.value("username").toString();
    p.proxy.password=x.value("password").toString(); return p;
}
QJsonObject toJson(const TestResult& r, bool includeRaw) {
    QJsonObject usage{{"promptTokens",double(r.metrics.usage.promptTokens)},
                      {"completionTokens",double(r.metrics.usage.completionTokens)},
                      {"totalTokens",double(r.metrics.usage.totalTokens)},
                      {"exact",r.metrics.usage.exact},{"source",r.metrics.usage.source}};
    const QJsonObject timing{{"dnsMs", r.metrics.timing.dnsMs}, {"tcpMs", r.metrics.timing.tcpMs},
                             {"tlsMs", r.metrics.timing.tlsMs}, {"requestMs", r.metrics.timing.requestMs},
                             {"firstByteMs", r.metrics.timing.firstByteMs}, {"firstTextMs", r.metrics.timing.firstTextMs},
                             {"generationMs", r.metrics.timing.generationMs}, {"totalMs", r.metrics.timing.totalMs}};
    QJsonObject metrics{{"ttftMs",r.metrics.ttftMs},{"firstByteMs",r.metrics.firstByteMs},
                        {"generationMs",r.metrics.generationMs},{"totalLatencyMs",r.metrics.totalLatencyMs},
                        {"tokensPerSecond",r.metrics.tokensPerSecond},{"responseBytes",double(r.metrics.responseBytes)},
                        {"timing", timing}, {"usage",usage}};
    QJsonObject error{{"httpStatus",r.error.httpStatus},{"reason",r.error.reason},{"type",r.error.type},
                      {"code",r.error.code},{"message",r.error.message},{"rawSummary",r.error.rawSummary},
                      {"html",r.error.html},{"timeout",r.error.timeout},{"network",r.error.network}};
    QJsonObject out{{"id",r.id},{"timestamp",r.timestamp.toString(Qt::ISODateWithMs)},
                    {"profile",r.profileName},{"model",r.model},{"protocol",protocolName(r.protocol)},
                    {"status",statusName(r.status)},{"passed",r.passed},{"output",r.output},
                    {"endpoint",r.endpoint},{"note",r.note},{"estimated",r.estimated},
                    {"metrics",metrics},{"error",error}};
    if(includeRaw)out["rawResponse"]=r.rawResponse;
    return out;
}
TestResult testResultFromJson(const QJsonObject& o) {
    TestResult r;
    r.id=o.value("id").toString(r.id); r.timestamp=QDateTime::fromString(o.value("timestamp").toString(),Qt::ISODateWithMs);
    if(!r.timestamp.isValid())r.timestamp=QDateTime::fromString(o.value("timestamp").toString(),Qt::ISODate);
    if(!r.timestamp.isValid())r.timestamp=QDateTime::currentDateTime();
    r.profileName=o.value("profile").toString(); r.model=o.value("model").toString();
    r.protocol=protocolFromString(o.value("protocol").toString()); r.status=statusFromString(o.value("status").toString());
    r.passed=o.value("passed").toBool(); r.output=o.value("output").toString(); r.endpoint=o.value("endpoint").toString();
    r.rawResponse=o.value("rawResponse").toString(); r.note=o.value("note").toString(); r.estimated=o.value("estimated").toBool();
    const auto m=o.value("metrics").toObject(); r.metrics.ttftMs=m.value("ttftMs").toDouble(-1);
    r.metrics.firstByteMs=m.value("firstByteMs").toDouble(-1); r.metrics.generationMs=m.value("generationMs").toDouble(-1);
    r.metrics.totalLatencyMs=m.value("totalLatencyMs").toDouble(-1); r.metrics.tokensPerSecond=m.value("tokensPerSecond").toDouble(-1);
    r.metrics.responseBytes=qRound64(m.value("responseBytes").toDouble());
    const auto t=m.value("timing").toObject();
    r.metrics.timing.dnsMs=t.value("dnsMs").toDouble(-1);
    r.metrics.timing.tcpMs=t.value("tcpMs").toDouble(-1);
    r.metrics.timing.tlsMs=t.value("tlsMs").toDouble(-1);
    r.metrics.timing.requestMs=t.value("requestMs").toDouble(-1);
    r.metrics.timing.firstByteMs=t.value("firstByteMs").toDouble(r.metrics.firstByteMs);
    r.metrics.timing.firstTextMs=t.value("firstTextMs").toDouble(r.metrics.ttftMs);
    r.metrics.timing.generationMs=t.value("generationMs").toDouble(r.metrics.generationMs);
    r.metrics.timing.totalMs=t.value("totalMs").toDouble(r.metrics.totalLatencyMs);
    const auto u=m.value("usage").toObject();
    r.metrics.usage.promptTokens=qRound64(u.value("promptTokens").toDouble(-1));
    r.metrics.usage.completionTokens=qRound64(u.value("completionTokens").toDouble(-1));
    r.metrics.usage.totalTokens=qRound64(u.value("totalTokens").toDouble(-1));
    r.metrics.usage.exact=u.value("exact").toBool(); r.metrics.usage.source=u.value("source").toString();
    const auto e=o.value("error").toObject(); r.error.httpStatus=e.value("httpStatus").toInt(); r.error.reason=e.value("reason").toString();
    r.error.type=e.value("type").toString(); r.error.code=e.value("code").toVariant().toString(); r.error.message=e.value("message").toString();
    r.error.rawSummary=e.value("rawSummary").toString(); r.error.html=e.value("html").toBool();
    r.error.timeout=e.value("timeout").toBool(); r.error.network=e.value("network").toBool(); return r;
}
QString formatContext(qint64 t) {
    if(t<0)return "Unknown"; if(t>=1000000)return QString::number(t/1000000.0,'g',3)+"M";
    if(t>=1000)return QString::number(t/1000.0,'g',3)+"K"; return QString::number(t);
}
qint64 parseContext(const QJsonValue& v) {
    if(v.isDouble())return qRound64(v.toDouble()); QString s=v.toString().trimmed().toUpper(); if(s.isEmpty())return -1;
    s.remove(','); bool ok=false; double n=s.toDouble(&ok); if(ok)return qRound64(n); double mul=1;
    if(s.endsWith('K')){mul=1000;s.chop(1);}else if(s.endsWith('M')){mul=1000000;s.chop(1);}else if(s.endsWith('B')){mul=1000000000;s.chop(1);}
    n=s.toDouble(&ok);return ok?qRound64(n*mul):-1;
}
QString sanitizeOutput(QString text) {
    text=text.trimmed(); if(text.size()>=2){const QChar a=text.front(),b=text.back();
        if((a=='"'&&b=='"')||(a=='\''&&b=='\'')||(a==QChar(0x201c)&&b==QChar(0x201d))||
           (a==QChar(0x2018)&&b==QChar(0x2019))||(a==QChar(0x300c)&&b==QChar(0x300d))||
           (a==QChar(0x300e)&&b==QChar(0x300f)))text=text.mid(1,text.size()-2).trimmed();}
    return text;
}
qint64 estimateTokens(const QString& text) {
    if(text.trimmed().isEmpty())return 0; qint64 n=0,ascii=0;
    for(const QChar c:text){const bool a=c.unicode()<128&&!c.isSpace(); if(a){if(++ascii==4){++n;ascii=0;}}
        else{if(ascii){++n;ascii=0;}if(!c.isSpace())++n;}}
    if(ascii)++n;return n;
}
}
