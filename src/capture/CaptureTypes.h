#pragma once
#include <QByteArray>
#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QUrl>
namespace airb {
enum class CaptureClient { Unknown, OpenAI, Claude, ClaudeCode, Codex, CherryStudio, ChatBox };
QString captureClientName(CaptureClient client);
struct CaptureHeader { QByteArray name; QByteArray value; };
using CaptureHeaders = QList<CaptureHeader>;
struct HttpMessage { QByteArray startLine; CaptureHeaders headers; QByteArray body; bool chunked=false; bool complete=false; bool malformed=false; QString error; };
struct SseCaptureEvent { QByteArray event; QByteArray data; QByteArray id; };
struct CaptureRequest { QString id; QDateTime timestamp; QString method; QString target; QUrl url; CaptureHeaders headers; QByteArray body; CaptureClient client=CaptureClient::Unknown; QList<SseCaptureEvent> sseEvents; bool redacted=true; };
struct CaptureResponse { int statusCode=0; QByteArray reason; CaptureHeaders headers; QByteArray body; QList<SseCaptureEvent> sseEvents; bool redacted=true; };
struct CaptureExchange { QString id; QDateTime timestamp; CaptureRequest request; CaptureResponse response; qint64 requestBytes=0; qint64 responseBytes=0; double totalMs=-1; bool success=false; QString error; };
QByteArray headerValue(const CaptureHeaders& headers,const QByteArray& name);
bool hasHeader(const CaptureHeaders& headers,const QByteArray& name);
QJsonObject captureExchangeToJson(const CaptureExchange& exchange,bool includeBody=true);
}
