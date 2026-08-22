#include "ProtocolAdapter.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonValue>

namespace airb {
namespace {
bool parseObject(const QByteArray& data,QJsonObject* out){QJsonParseError e;const auto d=QJsonDocument::fromJson(data,&e);if(e.error!=QJsonParseError::NoError||!d.isObject())return false;*out=d.object();return true;}
qint64 tokenValue(const QJsonObject&o,const QString&key){const auto v=o.value(key);return v.isDouble()?qRound64(v.toDouble()):parseContext(v);}
qint64 contextOf(const QJsonObject& o){
 const QStringList keys={"context_length","context_window","max_context_length","input_token_limit","max_input_tokens"};
 for(const auto&k:keys){auto n=parseContext(o.value(k));if(n>0)return n;}
 for(const auto&container:QStringList{"architecture","capabilities","limit","limits"}){const auto nested=o.value(container).toObject();for(const auto&k:keys){auto n=parseContext(nested.value(k));if(n>0)return n;}for(const auto&k:QStringList{"context","input"}){auto n=parseContext(nested.value(k));if(n>0)return n;}}
 return -1;
}
QString textFromContent(const QJsonValue& v){if(v.isString())return v.toString();QString out;if(v.isArray())for(const auto x:v.toArray()){const auto o=x.toObject();const auto type=o.value("type").toString();if(type=="text"||type=="output_text"||o.contains("text"))out+=o.value("text").toString();}return out;}
AdapterEvent malformed(AdapterState&s,const QString&dialect){++s.malformedEvents;AdapterEvent e;e.error=QString("Malformed %1 SSE JSON event #%2").arg(dialect).arg(s.malformedEvents);e.recoverable=true;return e;}
QString apiError(const QJsonObject&o){auto e=o.value("error");if(e.isString())return e.toString();const auto x=e.toObject();QString m=x.value("message").toString();if(m.isEmpty())m=o.value("message").toString();return m;}

class OpenAIAdapter final: public ProtocolAdapter {
public:
 QList<QUrl> modelUrls(const Profile& p)const override{return {endpoint(p.baseUrl,"v1/models"),endpoint(p.baseUrl,"models")};}
 QList<CompletionAttempt> completionAttempts(const Profile&p,const RequestConfig&c)const override{
  QList<CompletionAttempt> out;QList<QUrl> urls;for(const auto&u:QList<QUrl>{endpoint(p.baseUrl,"v1/chat/completions"),endpoint(p.baseUrl,"chat/completions")})if(!urls.contains(u))urls.push_back(u);
  for(const auto&u:urls)for(int mode=0;mode<3;++mode){QJsonObject body{{"model",c.model},{"messages",QJsonArray{QJsonObject{{"role","user"},{"content",c.prompt}}}},{"max_tokens",c.maxTokens},{"temperature",c.temperature},{"stream",mode<2}};if(mode==0&&c.includeUsage)body["stream_options"]=QJsonObject{{"include_usage",true}};out.push_back({u,QJsonDocument(body).toJson(QJsonDocument::Compact),commonHeaders(p,mode<2),mode<2,mode==0?"stream + usage":mode==1?"stream fallback":"non-stream fallback",mode});}
  return out;
 }
 AdapterEvent parseSse(const SseEvent&e,AdapterState&s)const override{
  if(e.data.trimmed()=="[DONE]"){AdapterEvent r;r.completed=true;return r;}QJsonObject o;if(!parseObject(e.data,&o))return malformed(s,"OpenAI");
  AdapterEvent r;const auto em=apiError(o);if(!em.isEmpty()){r.error=em;return r;}const auto choices=o.value("choices").toArray();
  if(!choices.isEmpty()){const auto c=choices.first().toObject();const auto d=c.value("delta").toObject();r.delta=textFromContent(d.value("content"));if(r.delta.isEmpty()&&!s.gotDelta)r.delta=textFromContent(c.value("message").toObject().value("content"));if(!r.delta.isEmpty())s.gotDelta=true;}
  if(o.value("usage").isObject()){s.usage=openAIUsage(o.value("usage").toObject());r.usage=s.usage;r.usageChanged=s.usage.promptTokens>=0||s.usage.completionTokens>=0;}
  return r;
 }
 AdapterEvent parseNonStream(const QByteArray&b,AdapterState&s)const override{
  QJsonObject o;AdapterEvent r;if(!parseObject(b,&o)){r.error="Response is not a valid OpenAI JSON object";return r;}const auto em=apiError(o);if(!em.isEmpty()){r.error=em;return r;}
  const auto choices=o.value("choices").toArray();if(!choices.isEmpty()){const auto c=choices.first().toObject();r.delta=textFromContent(c.value("message").toObject().value("content"));if(r.delta.isEmpty())r.delta=c.value("text").toString();}
  if(o.value("usage").isObject()){s.usage=openAIUsage(o.value("usage").toObject());r.usage=s.usage;r.usageChanged=true;}r.completed=true;return r;
 }
};

class ClaudeAdapter final: public ProtocolAdapter {
public:
 QList<QUrl> modelUrls(const Profile&p)const override{return {endpoint(p.baseUrl,"models"),endpoint(p.baseUrl,"v1/models")};}
 QList<CompletionAttempt> completionAttempts(const Profile&p,const RequestConfig&c)const override{
  auto h=commonHeaders(p,true);h.push_back({"x-api-key",p.apiKey.toUtf8()});h.push_back({"anthropic-version","2023-06-01"});if(p.context1M)h.push_back({"anthropic-beta","context-1m-2025-08-07"});addCustomHeaders(h,p.customHeaders);
  const QJsonObject b{{"model",c.model},{"max_tokens",c.maxTokens},{"messages",QJsonArray{QJsonObject{{"role","user"},{"content",c.prompt}}}},{"stream",true}};const auto bytes=QJsonDocument(b).toJson(QJsonDocument::Compact);
  QList<CompletionAttempt> out; for (const auto& u : QList<QUrl>{endpoint(p.baseUrl,"v1/messages"), endpoint(p.baseUrl,"messages")}) { if (!out.isEmpty() && out.constLast().url == u) continue; out.push_back({u,bytes,h,true,"Claude stream",0}); } return out;
 }
 AdapterEvent parseSse(const SseEvent&e,AdapterState&s)const override{
  QJsonObject o;if(!parseObject(e.data,&o))return malformed(s,"Claude");AdapterEvent r;const QString type=o.value("type").toString(QString::fromUtf8(e.event));
  if(type=="error"){r.error=apiError(o);if(r.error.isEmpty())r.error="Claude streaming error";return r;}
  if(type=="message_start"){const auto m=o.value("message").toObject();s.model=m.value("model").toString();const auto u=m.value("usage").toObject();s.usage.promptTokens=tokenValue(u,"input_tokens");s.usage.exact=s.usage.promptTokens>=0;s.usage.source="server";r.usage=s.usage;r.usageChanged=true;}
  else if(type=="content_block_delta"){const auto d=o.value("delta").toObject();if(d.value("type").toString()=="text_delta"||d.contains("text")){r.delta=d.value("text").toString();if(!r.delta.isEmpty())s.gotDelta=true;}}
  else if(type=="message_delta"){const auto u=o.value("usage").toObject();const auto n=tokenValue(u,"output_tokens");if(n>=0)s.usage.completionTokens=n;if(s.usage.promptTokens>=0&&s.usage.completionTokens>=0)s.usage.totalTokens=s.usage.promptTokens+s.usage.completionTokens;s.usage.exact=s.usage.totalTokens>=0;s.usage.source="server";r.usage=s.usage;r.usageChanged=true;}
  else if(type=="message_stop"){if(s.usage.totalTokens<0&&s.usage.promptTokens>=0&&s.usage.completionTokens>=0)s.usage.totalTokens=s.usage.promptTokens+s.usage.completionTokens;r.usage=s.usage;r.usageChanged=s.usage.promptTokens>=0;r.completed=true;}
  return r;
 }
 AdapterEvent parseNonStream(const QByteArray&b,AdapterState&s)const override{
  QJsonObject o;AdapterEvent r;if(!parseObject(b,&o)){r.error="Response is not a valid Claude JSON object";return r;}const auto em=apiError(o);if(!em.isEmpty()){r.error=em;return r;}r.delta=textFromContent(o.value("content"));const auto u=o.value("usage").toObject();s.usage.promptTokens=tokenValue(u,"input_tokens");s.usage.completionTokens=tokenValue(u,"output_tokens");if(s.usage.promptTokens>=0&&s.usage.completionTokens>=0)s.usage.totalTokens=s.usage.promptTokens+s.usage.completionTokens;s.usage.exact=s.usage.totalTokens>=0;s.usage.source="server";r.usage=s.usage;r.usageChanged=s.usage.promptTokens>=0;r.completed=true;return r;
 }
};

class CodexAdapter final: public ProtocolAdapter {
public:
 QList<QUrl> modelUrls(const Profile&p)const override{return {endpoint(p.baseUrl,"v1/models"),endpoint(p.baseUrl,"models")};}
 QList<CompletionAttempt> completionAttempts(const Profile&p,const RequestConfig&c)const override{
  auto h=commonHeaders(p,true);const auto sid=QUuid::createUuid().toString(QUuid::WithoutBraces);const auto tid=QUuid::createUuid().toString(QUuid::WithoutBraces);const auto rid=QUuid::createUuid().toString(QUuid::WithoutBraces);
  if(c.codexHeaders){h.append(qMakePair(QByteArray("originator"),QByteArray("codex_exec")));h.append(qMakePair(QByteArray("user-agent"),QByteArray("codex_exec/0.147.0 (Windows 10.0.26200; x86_64) unknown (codex_exec; 0.147.0)")));h.append(qMakePair(QByteArray("x-codex-beta-features"),QByteArray("remote_compaction_v2")));h.append(qMakePair(QByteArray("x-openai-internal-codex-responses-lite"),QByteArray("true")));h.append(qMakePair(QByteArray("session-id"),sid.toUtf8()));h.append(qMakePair(QByteArray("thread-id"),tid.toUtf8()));h.append(qMakePair(QByteArray("x-client-request-id"),rid.toUtf8()));h.append(qMakePair(QByteArray("x-codex-window-id"),(sid+":0").toUtf8()));const QJsonObject meta{{"session_id",sid},{"thread_id",tid},{"turn_id",rid},{"client","codex_exec"},{"client_version","0.147.0"},{"platform","windows"}};h.append({"x-codex-turn-metadata",QJsonDocument(meta).toJson(QJsonDocument::Compact)});}
  addCustomHeaders(h,p.customHeaders);const QJsonObject b{{"model",c.model},{"input",c.prompt},{"max_output_tokens",c.maxTokens},{"stream",true}};return {{endpoint(p.baseUrl,"v1/responses"),QJsonDocument(b).toJson(QJsonDocument::Compact),h,true,"Responses stream",0}};
 }
 AdapterEvent parseSse(const SseEvent&e,AdapterState&s)const override{
  QJsonObject o;if(!parseObject(e.data,&o))return malformed(s,"Responses");AdapterEvent r;const QString type=o.value("type").toString(QString::fromUtf8(e.event));
  if(type=="response.output_text.delta"){r.delta=o.value("delta").toString();if(!r.delta.isEmpty())s.gotDelta=true;}
  else if(type=="response.output_item.done"&&!s.gotDelta){const auto item=o.value("item").toObject();for(const auto v:item.value("content").toArray())r.delta+=v.toObject().value("text").toString();if(!r.delta.isEmpty())s.gotDelta=true;}
  else if(type=="response.completed"){const auto response=o.value("response").toObject();const auto u=response.value("usage").toObject();s.usage.promptTokens=tokenValue(u,"input_tokens");s.usage.completionTokens=tokenValue(u,"output_tokens");s.usage.totalTokens=tokenValue(u,"total_tokens");if(s.usage.totalTokens<0&&s.usage.promptTokens>=0&&s.usage.completionTokens>=0)s.usage.totalTokens=s.usage.promptTokens+s.usage.completionTokens;s.usage.exact=s.usage.totalTokens>=0;s.usage.source="server";r.usage=s.usage;r.usageChanged=true;r.completed=true;}
  else if(type=="error"||type=="response.failed"){r.error=apiError(o);if(r.error.isEmpty())r.error="Responses API error";}
  return r;
 }
 AdapterEvent parseNonStream(const QByteArray&b,AdapterState&s)const override{
  QJsonObject o;AdapterEvent r;if(!parseObject(b,&o)){r.error="Response is not a valid Responses API JSON object";return r;}const auto em=apiError(o);if(!em.isEmpty()){r.error=em;return r;}r.delta=o.value("output_text").toString();if(r.delta.isEmpty())for(const auto item:o.value("output").toArray())for(const auto c:item.toObject().value("content").toArray())r.delta+=c.toObject().value("text").toString();const auto u=o.value("usage").toObject();s.usage.promptTokens=tokenValue(u,"input_tokens");s.usage.completionTokens=tokenValue(u,"output_tokens");s.usage.totalTokens=tokenValue(u,"total_tokens");if(s.usage.totalTokens<0&&s.usage.promptTokens>=0&&s.usage.completionTokens>=0)s.usage.totalTokens=s.usage.promptTokens+s.usage.completionTokens;s.usage.exact=s.usage.totalTokens>=0;s.usage.source="server";r.usage=s.usage;r.usageChanged=s.usage.totalTokens>=0;r.completed=true;return r;
 }
};
}

QUrl ProtocolAdapter::endpoint(const QUrl& base,const QString& suffix){QUrl u=base;QString path=u.path();while(path.endsWith('/'))path.chop(1);QString s=suffix;while(s.startsWith('/'))s.remove(0,1);if(path.endsWith("/v1")&&s.startsWith("v1/"))s=s.mid(3);u.setPath(path+"/"+s);u.setQuery(QString());u.setFragment(QString());return u;}
QList<QPair<QByteArray,QByteArray>> ProtocolAdapter::commonHeaders(const Profile&p,bool sse){QList<QPair<QByteArray,QByteArray>> h{{"Content-Type","application/json"},{"Accept",sse?"text/event-stream":"application/json"},{"Authorization",("Bearer "+p.apiKey).toUtf8()}};addCustomHeaders(h,p.customHeaders);return h;}
void ProtocolAdapter::addCustomHeaders(QList<QPair<QByteArray,QByteArray>>&h,const QJsonObject&o){for(auto it=o.begin();it!=o.end();++it){const QByteArray k=it.key().toUtf8(),v=it.value().toVariant().toString().toUtf8();for(auto i=h.begin();i!=h.end();){if(i->first.compare(k,Qt::CaseInsensitive)==0)i=h.erase(i);else++i;}h.append({k,v});}}
Usage ProtocolAdapter::openAIUsage(const QJsonObject&o){Usage u;u.promptTokens=tokenValue(o,"prompt_tokens");u.completionTokens=tokenValue(o,"completion_tokens");u.totalTokens=tokenValue(o,"total_tokens");if(u.totalTokens<0&&u.promptTokens>=0&&u.completionTokens>=0)u.totalTokens=u.promptTokens+u.completionTokens;u.exact=u.totalTokens>=0;u.source="server";return u;}
QList<ModelInfo> ProtocolAdapter::parseModels(const QByteArray&b,QString*error)const{QJsonParseError e;const auto d=QJsonDocument::fromJson(b,&e);if(e.error!=QJsonParseError::NoError){if(error)*error=e.errorString();return{};}QJsonArray a;if(d.isArray())a=d.array();else{const auto o=d.object();a=o.value("data").toArray();if(a.isEmpty())a=o.value("models").toArray();}QList<ModelInfo> out;QSet<QString> seen;for(const auto v:a){ModelInfo m;if(v.isString()){m.id=v.toString();m.displayName=m.id;}else{const auto o=v.toObject();m.id=o.value("id").toString(o.value("name").toString());m.displayName=o.value("display_name").toString(m.id);m.ownedBy=o.value("owned_by").toString();m.contextLength=contextOf(o);m.contextSource=m.contextLength>0?"API":"";}if(!m.id.isEmpty()&&!seen.contains(m.id)){seen.insert(m.id);out.push_back(m);}}if(out.isEmpty()&&error)*error="No model array found";return out;}
QList<QPair<QByteArray,QByteArray>> ProtocolAdapter::modelHeaders(const Profile& profile) const {
    RequestConfig config;
    const auto attempts = completionAttempts(profile, config);
    return attempts.isEmpty() ? QList<QPair<QByteArray,QByteArray>>() : attempts.first().headers;
}

bool ProtocolAdapter::shouldRetry(const CompletionAttempt&a,int status,const QByteArray&body,bool networkError)const{if(status==401||status==403||status==429)return false;if(networkError)return true;const auto l=body.toLower();if(status==404||status==405)return true;if(a.mode==0&&(status==400||status==422)&&l.contains("stream_options"))return true;if(a.streaming&&(status==400||status==406||status==415||status==422)&&(l.contains("stream")||l.contains("sse")))return true;return false;}
std::unique_ptr<ProtocolAdapter> ProtocolAdapter::create(Protocol p){if(p==Protocol::Claude)return std::make_unique<ClaudeAdapter>();if(p==Protocol::Codex)return std::make_unique<CodexAdapter>();return std::make_unique<OpenAIAdapter>();}
}
