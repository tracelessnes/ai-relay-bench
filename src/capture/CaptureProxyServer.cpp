#include "CaptureProxyServer.h"
#include "HttpParser.h"
#include "ClientFingerprint.h"
#include "security/RedactionService.h"
#include <QElapsedTimer>
#include <QHostAddress>
#include <QRegularExpression>
#include <QSet>
#include <QTcpServer>
#include <QTcpSocket>

namespace airb {
namespace {
QByteArray redactedBody(const QByteArray& input) { auto v = RedactionService::text(QString::fromUtf8(input), false).toUtf8(); v.replace("****", "[REDACTED]"); return v; }
CaptureHeaders redactedHeaders(const CaptureHeaders& input) { QList<QPair<QByteArray,QByteArray>> pairs; for (const auto& h : input) pairs.append({h.name,h.value}); const auto safe = RedactionService::headers(pairs,false); CaptureHeaders result; for (const auto& h:safe) result.append({h.first,h.second == "****" ? QByteArray("[REDACTED]") : h.second}); return result; }
}
CaptureConnection::CaptureConnection(QTcpSocket* client, CaptureProxyServer* server, QObject* parent) : QObject(parent), server_(server), client_(client), requestParser_(new HttpParser(HttpParser::Mode::Request, 64*1024, 16*1024*1024)), responseParser_(new HttpParser(HttpParser::Mode::Response, 64*1024, 64*1024*1024)) {
    client_->setParent(this); connect(client_, &QTcpSocket::readyRead, this, &CaptureConnection::readClient); connect(client_, &QTcpSocket::disconnected, this, [this]{ if (!started_ && !finished_) finishExchange(QStringLiteral("客户端在请求完成前断开")); deleteLater(); });
}
CaptureHeaders CaptureConnection::safeHeaders(const CaptureHeaders& input) const { return redactedHeaders(input); }
QByteArray CaptureConnection::safeBody(const QByteArray& body) const { return redactedBody(body); }
void CaptureConnection::readClient() {
    if (started_ || rejected_) return;
    const auto bytes = client_->readAll(); requestRaw_ += bytes; requestParser_->feed(bytes);
    if (requestParser_->hasError()) { rejectConnect(requestParser_->error().toUtf8()); return; }
    if (requestParser_->isComplete()) startUpstream();
}
void CaptureConnection::rejectConnect(const QByteArray& reason) { rejected_=true; const QByteArray body = "HTTPS CONNECT is not enabled by default. " + reason; client_->write("HTTP/1.1 501 Not Implemented\r\nContent-Type: text/plain\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body); client_->disconnectFromHost(); }
void CaptureConnection::startUpstream() {
    if (started_) return; started_=true; timer_.start();
    const auto line = requestParser_->message().startLine; const auto parts = line.split(' '); if (parts.size()<2) { finishExchange(QStringLiteral("请求行缺少 URL")); return; }
    method_=QString::fromLatin1(parts.at(0)); target_=QString::fromUtf8(parts.at(1));
    if (method_.compare(QStringLiteral("CONNECT"),Qt::CaseInsensitive)==0) { rejectConnect("CONNECT/HTTPS MITM 尚未启用"); return; }
    url_=QUrl(target_); if (!url_.isValid() || url_.scheme().isEmpty()) { const auto host=headerValue(requestParser_->message().headers,"host"); url_=QUrl(QStringLiteral("http://") + QString::fromUtf8(host) + target_); }
    if (!url_.isValid() || url_.host().isEmpty()) { finishExchange(QStringLiteral("无法解析上游 URL")); return; }
    if (url_.scheme().compare(QStringLiteral("http"), Qt::CaseInsensitive) != 0) {
        rejectConnect("Only plain HTTP upstreams are supported");
        return;
    }
    upstream_=new QTcpSocket(this); connect(upstream_,&QTcpSocket::connected,this,&CaptureConnection::upstreamConnected); connect(upstream_,&QTcpSocket::readyRead,this,&CaptureConnection::readUpstream); connect(upstream_,&QTcpSocket::disconnected,this,&CaptureConnection::upstreamDisconnected); connect(upstream_,&QAbstractSocket::errorOccurred,this,&CaptureConnection::socketError); upstream_->connectToHost(url_.host(),quint16(url_.port(80)));
}
void CaptureConnection::upstreamConnected() {
    const auto& msg=requestParser_->message(); auto headers=msg.headers; QByteArray path=url_.path(QUrl::FullyEncoded).toUtf8(); if(path.isEmpty())path="/"; if(!url_.query(QUrl::FullyEncoded).isEmpty())path+="?"+url_.query(QUrl::FullyEncoded).toUtf8();
    QByteArray out=method_.toUtf8()+" "+path+" HTTP/1.1\r\n";
    const QSet<QByteArray> hopByHop = {"connection", "proxy-connection", "keep-alive", "proxy-authenticate", "proxy-authorization", "te", "trailer", "transfer-encoding", "upgrade"};
    for (const auto& h : headers) {
        if (hopByHop.contains(h.name.toLower())) continue;
        out += h.name + ": " + h.value + "\r\n";
    }
    out += "Connection: close\r\n\r\n" + msg.body;
    upstream_->write(out);
    upstream_->flush();
}
void CaptureConnection::readUpstream() { const auto b=upstream_->readAll(); responseRaw_+=b; client_->write(b); client_->flush(); responseParser_->feed(b); }
void CaptureConnection::socketError() {
    if (!upstream_ || finished_) return;
    if (upstream_->error() == QAbstractSocket::RemoteHostClosedError) return;
    finishExchange(upstream_->errorString());
}
void CaptureConnection::upstreamDisconnected() { if(finished_)return; responseParser_->finish(); finishExchange(responseParser_->hasError()?responseParser_->error():QString{}); client_->disconnectFromHost(); }
void CaptureConnection::finishExchange(const QString& error) {
    if(finished_)return; finished_=true; CaptureExchange x; x.id=QUuid::createUuid().toString(QUuid::WithoutBraces); x.timestamp=QDateTime::currentDateTime(); x.totalMs=timer_.isValid()?timer_.elapsed():-1; x.error=error; x.request.id=x.id; x.request.timestamp=x.timestamp; x.request.method=method_; x.request.target=target_; x.request.url=url_; x.request.headers=safeHeaders(requestParser_->message().headers); x.request.body=safeBody(requestParser_->message().body); x.request.client=identifyCaptureClient(x.request.headers,target_.toUtf8(),x.request.body); x.request.sseEvents=parseCapturedSse(x.request.headers,x.request.body); x.responseBytes=responseRaw_.size(); x.requestBytes=requestRaw_.size(); if(responseParser_->message().complete){const auto line=responseParser_->message().startLine;const auto p=line.split(' ');if(p.size()>1)x.response.statusCode=p.at(1).toInt();if(p.size()>2)x.response.reason=QByteArrayList(p.mid(2)).join(' ');x.response.headers=safeHeaders(responseParser_->message().headers);x.response.body=safeBody(responseParser_->message().body);x.response.sseEvents=parseCapturedSse(x.response.headers,x.response.body);x.success=x.response.statusCode>=200&&x.response.statusCode<400&&!responseParser_->hasError();}else{x.response.body=redactedBody(responseRaw_);x.error=x.error.isEmpty()?QStringLiteral("响应未完成"):x.error;} if(server_)emit server_->exchangeReady(x); deleteLater(); }
CaptureProxyServer::CaptureProxyServer(QObject* parent):QObject(parent),server_(new QTcpServer(this)){connect(server_,&QTcpServer::newConnection,this,&CaptureProxyServer::acceptConnection);}
CaptureProxyServer::~CaptureProxyServer(){stop();}
bool CaptureProxyServer::start(const QHostAddress& address,quint16 port){if(server_->isListening())return true;if(!server_->listen(address,port)){emit stateChanged(false,server_->errorString());return false;}emit stateChanged(true,QStringLiteral("监听 %1:%2").arg(server_->serverAddress().toString()).arg(server_->serverPort()));return true;}
void CaptureProxyServer::stop(){if(!server_->isListening())return;server_->close();for(auto* c:std::as_const(connections_))if(c)c->deleteLater();connections_.clear();emit stateChanged(false,QStringLiteral("已停止"));}
bool CaptureProxyServer::isListening()const{return server_->isListening();} quint16 CaptureProxyServer::port()const{return server_->serverPort();} QString CaptureProxyServer::errorString()const{return server_->errorString();}
void CaptureProxyServer::acceptConnection(){while(server_->hasPendingConnections()){auto* socket=server_->nextPendingConnection();auto* c=new CaptureConnection(socket,this,this);connections_.append(c);connect(c,&QObject::destroyed,this,[this,c]{connections_.removeAll(c);});}}
}
