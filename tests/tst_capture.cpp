#include <QtTest>
#include "capture/CaptureTypes.h"
#include "capture/HttpParser.h"
#include "capture/ClientFingerprint.h"
#include "capture/HarExporter.h"
#include "capture/CaptureProxyServer.h"

#include <QTcpServer>
#include <QTcpSocket>

using namespace airb;

class CaptureTests final : public QObject {
    Q_OBJECT

private slots:
    void parsesSplitContentLengthRequest() {
        HttpParser parser(HttpParser::Mode::Request);
        QVERIFY(!parser.feed("POST http://relay.test/v1/chat/completions HTTP/1.1\r\n"
                             "Host: relay.test\r\nContent-Length: 5\r\n\r\n{"));
        QVERIFY(parser.feed("\"x\"}"));
        QVERIFY(parser.isComplete());
        QCOMPARE(parser.message().startLine,
                 QByteArray("POST http://relay.test/v1/chat/completions HTTP/1.1"));
        QCOMPARE(parser.message().body, QByteArray("{\"x\"}"));
        QCOMPARE(headerValue(parser.message().headers, "host"), QByteArray("relay.test"));
    }

    void parsesChunkedBody() {
        HttpMessage message;
        QVERIFY(HttpParser::parse("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"
                                  "4\r\nWiki\r\n5\r\npedia\r\n0\r\n\r\n",
                                  HttpParser::Mode::Response, &message));
        QCOMPARE(message.body, QByteArray("Wikipedia"));
        QVERIFY(message.chunked);
    }

    void parsesCloseDelimitedResponse() {
        HttpParser parser(HttpParser::Mode::Response);
        QVERIFY(!parser.feed("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nOK"));
        QVERIFY(parser.finish());
        QCOMPARE(parser.message().body, QByteArray("OK"));
    }

    void rejectsMalformedAndOversizedInput() {
        HttpMessage message;
        QVERIFY(!HttpParser::parse("GET / HTTP/1.1\r\nBrokenHeader\r\n\r\n",
                                  HttpParser::Mode::Request, &message));
        QVERIFY(message.malformed);
        HttpParser parser(HttpParser::Mode::Request, 16, 128);
        QVERIFY(!parser.feed("GET / HTTP/1.1\r\nX: this-is-too-large\r\n"));
        QVERIFY(parser.hasError());
    }

    void identifiesAiClients() {
        CaptureHeaders codex{{"originator", "codex_exec"},
                             {"x-codex-beta-features", "remote_compaction_v2"},
                             {"x-openai-internal-codex-responses-lite", "true"}};
        QCOMPARE(identifyCaptureClient(codex, "/v1/responses"), CaptureClient::Codex);

        CaptureHeaders claude{{"x-api-key", "synthetic"},
                              {"anthropic-version", "2023-06-01"},
                              {"user-agent", "claude-cli/1.0.117 (external, cli)"}};
        QCOMPARE(identifyCaptureClient(claude, "/v1/messages"), CaptureClient::ClaudeCode);

        CaptureHeaders openai{{"authorization", "Bearer synthetic"}};
        QCOMPARE(identifyCaptureClient(openai, "/v1/chat/completions"), CaptureClient::OpenAI);
    }

    void parsesSseAndExportsRedactedHar() {
        CaptureHeaders headers{{"content-type", "text/event-stream"}};
        const auto events = parseCapturedSse(
            headers, "event: content_block_delta\ndata: {\"delta\":{\"text\":\"OK\"}}\n\n");
        QCOMPARE(events.size(), 1);
        QCOMPARE(events.first().event, QByteArray("content_block_delta"));

        CaptureExchange exchange;
        exchange.id = "one";
        exchange.timestamp = QDateTime::currentDateTimeUtc();
        exchange.request.method = "POST";
        exchange.request.target = "/v1/messages";
        exchange.request.headers = {{"x-api-key", "sk-test-synthetic"}};
        exchange.request.body = "{\"token\":\"sk-test-synthetic\"}";
        exchange.response.statusCode = 200;
        exchange.response.headers = headers;
        exchange.response.body = "data: [DONE]\n\n";

        const auto json = HarExporter::toJson({exchange});
        QVERIFY(json.contains("[REDACTED]"));
        QVERIFY(!json.contains("sk-test-synthetic"));
        QVERIFY(json.contains("\"log\""));
        QVERIFY(HarExporter::toMarkdown({exchange}).contains("| ID |"));
    }

    void rejectsHttpsAbsoluteUrl() {
        CaptureProxyServer proxy;
        QVERIFY(proxy.start(QHostAddress::LocalHost, 0));
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, proxy.port());
        QVERIFY(client.waitForConnected());
        client.write("GET https://relay.test/v1/models HTTP/1.1\r\nHost: relay.test\r\n\r\n");
        client.flush();
        QTRY_VERIFY_WITH_TIMEOUT(client.bytesAvailable() > 0, 3000);
        const auto response = client.readAll();
        QVERIFY(response.startsWith("HTTP/1.1 501 Not Implemented"));
    }

    void forwardsHttpAndCapturesExchange() {
        qRegisterMetaType<CaptureExchange>();
        QTcpServer upstream;
        QVERIFY(upstream.listen(QHostAddress::LocalHost, 0));
        QByteArray upstreamRequest;
        connect(&upstream, &QTcpServer::newConnection, &upstream, [&] {
            auto* socket = upstream.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [socket, &upstreamRequest] {
                upstreamRequest += socket->readAll();
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\n"
                              "Content-Length: 14\r\nConnection: close\r\n\r\n"
                              "data: [DONE]\n\n");
                socket->disconnectFromHost();
            });
        });

        CaptureProxyServer proxy;
        QVERIFY(proxy.start(QHostAddress::LocalHost, 0));
        QSignalSpy spy(&proxy, &CaptureProxyServer::exchangeReady);
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, proxy.port());
        QVERIFY(client.waitForConnected());
        const auto request = QByteArray("GET http://127.0.0.1:")
            + QByteArray::number(upstream.serverPort())
            + "/v1/responses HTTP/1.1\r\nHost: 127.0.0.1\r\n"
              "Authorization: Bearer synthetic-token\r\n"
              "Proxy-Connection: keep-alive\r\nConnection: keep-alive\r\n\r\n";
        client.write(request);
        client.flush();

        QTRY_VERIFY_WITH_TIMEOUT(client.bytesAvailable() > 0, 3000);
        QVERIFY(client.readAll().contains("200 OK"));
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 3000);

        const auto exchange = qvariant_cast<CaptureExchange>(spy.first().first());
        QCOMPARE(exchange.response.statusCode, 200);
        QCOMPARE(exchange.request.client, CaptureClient::OpenAI);
        QVERIFY(!exchange.request.headers.first().value.contains("synthetic-token"));
        QCOMPARE(exchange.response.sseEvents.size(), 1);
        QVERIFY(exchange.error.isEmpty());
        QVERIFY(upstreamRequest.contains("Connection: close\r\n"));
        QVERIFY(!upstreamRequest.contains("Proxy-Connection:"));
        QVERIFY(!upstreamRequest.contains("Connection: keep-alive"));
    }
};

QTEST_MAIN(CaptureTests)
#include "tst_capture.moc"
