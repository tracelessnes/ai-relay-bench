#include "CaptureTypes.h"
#include <QJsonArray>
#include <QJsonDocument>
namespace airb {
QString captureClientName(CaptureClient c){ switch(c){case CaptureClient::OpenAI:return "OpenAI";case CaptureClient::Claude:return "Claude";case CaptureClient::ClaudeCode:return "Claude Code";case CaptureClient::Codex:return "Codex CLI";case CaptureClient::CherryStudio:return "Cherry Studio";case CaptureClient::ChatBox:return "ChatBox";default:return "Unknown";} }
QByteArray headerValue(const CaptureHeaders& h,const QByteArray& n){for(const auto& x:h)if(x.name.compare(n,Qt::CaseInsensitive)==0)return x.value;return {};}
bool hasHeader(const CaptureHeaders& h,const QByteArray& n){for(const auto& x:h)if(x.name.compare(n,Qt::CaseInsensitive)==0)return true;return false;}
static QJsonArray headersJson(const CaptureHeaders& h){QJsonArray a;for(const auto& x:h)a.append(QJsonObject{{"name",QString::fromUtf8(x.name)},{"value",QString::fromUtf8(x.value)}});return a;}
QJsonObject captureExchangeToJson(const CaptureExchange& x,bool body){QJsonObject req{{"method",x.request.method},{"target",x.request.target},{"url",x.request.url.toString()},{"client",captureClientName(x.request.client)},{"headers",headersJson(x.request.headers)}};QJsonObject res{{"status",x.response.statusCode},{"reason",QString::fromUtf8(x.response.reason)},{"headers",headersJson(x.response.headers)}};if(body){req["body"]=QString::fromUtf8(x.request.body);res["body"]=QString::fromUtf8(x.response.body);}return {{"id",x.id},{"timestamp",x.timestamp.toUTC().toString(Qt::ISODateWithMs)},{"request",req},{"response",res},{"requestBytes",x.requestBytes},{"responseBytes",x.responseBytes},{"totalMs",x.totalMs},{"success",x.success},{"error",x.error}};}
}
