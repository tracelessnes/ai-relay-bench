#pragma once
#include "CaptureTypes.h"
#include <QElapsedTimer>
#include <QHostAddress>
#include <QObject>

class QTcpSocket;
class QTcpServer;
namespace airb {
class CaptureProxyServer;
class HttpParser;
class CaptureConnection final : public QObject {
    Q_OBJECT
public:
    CaptureConnection(QTcpSocket* client, CaptureProxyServer* server, QObject* parent = nullptr);
private slots:
    void readClient(); void upstreamConnected(); void readUpstream(); void upstreamDisconnected(); void socketError();
private:
    void rejectConnect(const QByteArray& reason); void startUpstream(); void finishExchange(const QString& error = {});
    CaptureHeaders safeHeaders(const CaptureHeaders& input) const; QByteArray safeBody(const QByteArray& body) const;
    CaptureProxyServer* server_ = nullptr; QTcpSocket* client_ = nullptr; QTcpSocket* upstream_ = nullptr;
    HttpParser* requestParser_ = nullptr; HttpParser* responseParser_ = nullptr; QByteArray responseRaw_; QString method_; QString target_; QUrl url_; QByteArray requestRaw_; QElapsedTimer timer_; bool started_=false; bool finished_=false; bool rejected_=false;
};
class CaptureProxyServer final : public QObject {
    Q_OBJECT
public:
    explicit CaptureProxyServer(QObject* parent = nullptr); ~CaptureProxyServer() override;
    bool start(const QHostAddress& address = QHostAddress::LocalHost, quint16 port = 8765); void stop(); bool isListening() const; quint16 port() const; QString errorString() const;
signals: void exchangeReady(const CaptureExchange& exchange); void stateChanged(bool running, const QString& message);
private slots: void acceptConnection();
private: QTcpServer* server_ = nullptr; QList<CaptureConnection*> connections_;
};
} // namespace airb
