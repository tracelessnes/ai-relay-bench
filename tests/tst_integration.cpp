#include <QtTest>
#include <QtNetwork>
#include <QEventLoop>
#include <QTimer>
#include <QSignalSpy>
#include "services/ScanService.h"
#include <algorithm>
#include "domain/Types.h"
#include "protocol/SseDecoder.h"
#include "protocol/ProtocolAdapter.h"
#include "network/BenchmarkJob.h"

using namespace airb;

class MockHttpServer final : public QObject {
    Q_OBJECT
public:
    enum class FaultMode {
        Normal,
        HtmlWaf,
        Unauthorized,
        Forbidden,
        RateLimited,
        ServerError,
        ServiceUnavailable,
        Delay,
        Disconnect,
        DuplicateSse,
        MissingUsage,
        InvalidJson,
        SplitSse,
    };

    explicit MockHttpServer(QObject* parent=nullptr):QObject(parent) {
        connect(&server_, &QTcpServer::newConnection, this, &MockHttpServer::accept);
    }
    bool start() { return server_.listen(QHostAddress::LocalHost, 0); }
    quint16 port() const { return server_.serverPort(); }
    int requestCount() const { return requestCount_; }
    QByteArray requestBody(int i) const { return i >= 0 && i < requests_.size() ? requests_[i].body : QByteArray{}; }
    void setFaultMode(FaultMode mode) { mode_ = mode; }
    void setDelayMs(int delayMs) { delayMs_ = delayMs; }
private:
    struct Request { QByteArray path; QByteArray body; };
    struct ClientState { QByteArray buffer; bool responded=false; };
    QTcpServer server_;
    QHash<QTcpSocket*, ClientState> clients_;
    QList<Request> requests_;
    int requestCount_=0;
    FaultMode mode_=FaultMode::Normal;
    int delayMs_=1500;

    void accept() {
        while (server_.hasPendingConnections()) {
            auto* socket=server_.nextPendingConnection();
            clients_.insert(socket, {});
            connect(socket, &QTcpSocket::readyRead, this, [this,socket]{read(socket);});
            connect(socket, &QTcpSocket::disconnected, this, [this,socket]{clients_.remove(socket);socket->deleteLater();});
        }
    }
    static int contentLength(const QByteArray& headers) {
        for (const auto& line: headers.split('\n')) {
            if (line.toLower().startsWith("content-length:")) return line.mid(line.indexOf(':')+1).trimmed().toInt();
        }
        return 0;
    }
    void read(QTcpSocket* socket) {
        if (!clients_.contains(socket)) return;
        auto& state=clients_[socket]; state.buffer += socket->readAll();
        if (state.responded) return;
        const int split=state.buffer.indexOf("\r\n\r\n");
        if (split<0) return;
        const auto headers=state.buffer.left(split);
        const int len=contentLength(headers);
        if (state.buffer.size()<split+4+len) return;
        state.responded=true;
        const auto firstLine=headers.left(headers.indexOf('\n')).trimmed();
        const auto parts=firstLine.split(' ');
        const QByteArray path=parts.size()>1?parts[1]:QByteArray{};
        const QByteArray body=state.buffer.mid(split+4,len);
        requests_.push_back({path,body});
        ++requestCount_;
        if (mode_ == FaultMode::Normal && requestCount_==1 && body.contains("stream_options")) {
            send(socket, 400, "application/json", {"{\"error\":{\"type\":\"invalid_request_error\",\"code\":\"stream_options\",\"message\":\"stream_options unsupported\"}}"});
            return;
        }
        respond(socket);
    }

    static QByteArray statusReason(int status) {
        switch (status) {
        case 401: return " Unauthorized";
        case 403: return " Forbidden";
        case 429: return " Too Many Requests";
        case 500: return " Internal Server Error";
        case 503: return " Service Unavailable";
        default: return status == 200 ? " OK" : " Bad Request";
        }
    }

    int faultStatus() const {
        switch (mode_) {
        case FaultMode::Unauthorized: return 401;
        case FaultMode::Forbidden: return 403;
        case FaultMode::RateLimited: return 429;
        case FaultMode::ServerError: return 500;
        case FaultMode::ServiceUnavailable: return 503;
        default: return 200;
        }
    }

    void respond(QTcpSocket* socket) {
        if (!socket) return;
        if (mode_ == FaultMode::Delay) {
            QPointer<QTcpSocket> guarded(socket);
            QTimer::singleShot(delayMs_, this, [this, guarded] {
                if (guarded && clients_.contains(guarded)) sendNormal(guarded);
            });
            return;
        }
        if (mode_ == FaultMode::Disconnect) {
            sendPartial(socket, 200, "text/event-stream", "data: {\"id\":\"x\",\"choices\":[{\"delta\":{\"content\":\"O\"}}]}");
            return;
        }
        if (mode_ == FaultMode::HtmlWaf) {
            send(socket, 503, "text/html", {"<!doctype html><html><title>Cloudflare</title><body>cf-ray challenge</body></html>"});
            return;
        }
        const int status = faultStatus();
        if (status != 200) {
            send(socket, status, "application/json", {"{\"error\":{\"message\":\"mock failure\",\"type\":\"mock_error\"}}"});
            return;
        }
        sendNormal(socket);
    }

    void sendNormal(QTcpSocket* socket) {
        if (!socket) return;
        if (mode_ == FaultMode::DuplicateSse) {
            send(socket, 200, "text/event-stream", {
                "data: {\"id\":\"dup\",\"choices\":[{\"delta\":{\"content\":\"O\"}}]}\n\n",
                "data: {\"id\":\"dup\",\"choices\":[{\"delta\":{\"content\":\"O\"}}]}\n\n",
                "data: {\"id\":\"dup\",\"choices\":[{\"delta\":{\"content\":\"K\"}}]}\n\n",
                "data: {\"id\":\"dup\",\"choices\":[{\"delta\":{\"content\":\"K\"}}]}\n\n",
                "data: {\"choices\":[],\"usage\":{\"prompt_tokens\":2,\"completion_tokens\":2,\"total_tokens\":4}}\n\n",
                "data: [DONE]\n\n"
            });
            return;
        }
        if (mode_ == FaultMode::MissingUsage) {
            send(socket, 200, "text/event-stream", {
                "data: {\"id\":\"x\",\"choices\":[{\"delta\":{\"content\":\"O\"}}]}\n\n",
                "data: {\"id\":\"x\",\"choices\":[{\"delta\":{\"content\":\"K\"}}]}\n\n",
                "data: [DONE]\n\n"
            });
            return;
        }
        if (mode_ == FaultMode::InvalidJson) {
            send(socket, 200, "text/event-stream", {
                "data: {not-json}\n\n",
                "data: {\"id\":\"x\",\"choices\":[{\"delta\":{\"content\":\"O\"}}]}\n\n",
                "data: {\"id\":\"x\",\"choices\":[{\"delta\":{\"content\":\"K\"}}]}\n\n",
                "data: {\"choices\":[],\"usage\":{\"prompt_tokens\":2,\"completion_tokens\":2,\"total_tokens\":4}}\n\n",
                "data: [DONE]\n\n"
            });
            return;
        }
        if (mode_ == FaultMode::SplitSse) {
            send(socket, 200, "text/event-stream", {
                "data: {\"id\":\"x\",\"choices\":[{\"delta\":{\"content\":\"O\"}}]}\r",
                "\n",
                "\r",
                "\n",
                "data: {\"id\":\"x\",\"choices\":[{\"delta\":{\"content\":\"K\"}}]}\n",
                "\n",
                "data: {\"choices\":[],\"usage\":{\"prompt_tokens\":2,\"completion_tokens\":2,\"total_tokens\":4}}\r\n\r\n",
                "data: [DONE]\n\n"
            });
            return;
        }
        send(socket, 200, "text/event-stream", {
            "data: {\"id\":\"x\",\"choices\":[{\"delta\":{\"content\":\"O\"}}]}\n\n",
            "data: {\"id\":\"x\",\"choices\":[{\"delta\":{\"content\":\"K\"}}]}\n\n",
            "data: {\"choices\":[],\"usage\":{\"prompt_tokens\":2,\"completion_tokens\":2,\"total_tokens\":4}}\n\n",
            "data: [DONE]\n\n"
        });
    }

    void send(QTcpSocket* socket,int status,const QByteArray& contentType,const QList<QByteArray>& chunks) {
        if (!socket) return;
        QByteArray total; for (const auto& c:chunks) total += c;
        const QByteArray header=QByteArray("HTTP/1.1 ")+QByteArray::number(status)+statusReason(status)+
            "\r\nContent-Type: "+contentType+"\r\nContent-Length: "+QByteArray::number(total.size())+"\r\nConnection: close\r\n\r\n";
        socket->write(header);
        for (const auto& chunk:chunks) socket->write(chunk);
        socket->flush();
        socket->disconnectFromHost();
    }

    void sendPartial(QTcpSocket* socket, int status, const QByteArray& contentType, const QByteArray& body) {
        if (!socket) return;
        const QByteArray header=QByteArray("HTTP/1.1 ")+QByteArray::number(status)+statusReason(status)+
            "\r\nContent-Type: "+contentType+"\r\nContent-Length: "+QByteArray::number(body.size()+32)+"\r\nConnection: close\r\n\r\n";
        socket->write(header);
        socket->write(body);
        socket->flush();
        socket->disconnectFromHost();
    }
};

class IntegrationTests: public QObject {
    Q_OBJECT
private slots:
    void openAiFallbackAndStreaming();
    void claudeEventsNormalizeUsage();
    void codexDeltaDeduplication();
    void malformedAndSplitSseAreBounded();
    void baseUrlWithoutV1IsAutoDetected();
    void scanServiceFindsModelsAndProtocol();
    void modelHeadersAreProtocolAware();
    void htmlWafResponseIsClassified();
    void scanHtmlWafDetailIsReadable();
    void httpStatusFailures_data();
    void httpStatusFailures();
    void delayedResponseTimesOut();
    void truncatedStreamIsNotAccepted();
    void duplicateSseEventsAreDeduplicated();
    void missingUsageIsEstimated();
    void invalidJsonEventIsRecoverable();
    void splitSseBoundariesAreDecoded();
    void scanCancelStopsFinishedAndFollowupRequests();
};

void IntegrationTests::openAiFallbackAndStreaming() {
    MockHttpServer server; QVERIFY(server.start());
    Profile profile; profile.name="mock"; profile.baseUrl=QUrl(QString("http://127.0.0.1:%1/v1").arg(server.port())); profile.apiKey="test"; profile.protocol=Protocol::OpenAI; profile.proxy.mode=ProxyMode::None;
    RequestConfig config; config.model="mock-model";
    QEventLoop loop; TestResult result; bool got=false;
    auto* job=new BenchmarkJob(profile,config);
    connect(job,&BenchmarkJob::finished,&loop,[&](const TestResult&r){result=r;got=true;loop.quit();});
    job->start(); QTimer::singleShot(5000,&loop,&QEventLoop::quit); loop.exec();
    QVERIFY2(got,"benchmark did not finish within 5 seconds");    QCOMPARE(server.requestCount(),2);
    QVERIFY(server.requestBody(0).contains("stream_options"));
    QVERIFY(!server.requestBody(1).contains("stream_options"));
    QCOMPARE(result.output,QString("OK")); QVERIFY(result.passed); QCOMPARE(result.status,TestStatus::Passed);
    QCOMPARE(result.metrics.usage.promptTokens,qint64(2)); QCOMPARE(result.metrics.usage.completionTokens,qint64(2));
    QCOMPARE(result.metrics.usage.totalTokens,qint64(4)); QVERIFY(result.metrics.ttftMs>=0); QVERIFY(result.metrics.totalLatencyMs>=result.metrics.ttftMs);
    QVERIFY(result.note.contains("fallback"));
}

void IntegrationTests::claudeEventsNormalizeUsage() {
    auto adapter=ProtocolAdapter::create(Protocol::Claude); AdapterState state;
    auto e=adapter->parseSse({"message", "{\"type\":\"message_start\",\"message\":{\"model\":\"claude-x\",\"usage\":{\"input_tokens\":11}}}"},state);
    QVERIFY(e.usageChanged); QCOMPARE(state.model,QString("claude-x")); QCOMPARE(state.usage.promptTokens,qint64(11));
    e=adapter->parseSse({"content_block_delta", "{\"type\":\"content_block_delta\",\"delta\":{\"type\":\"text_delta\",\"text\":\"O\"}}"},state); QCOMPARE(e.delta,QString("O"));
    e=adapter->parseSse({"message_delta", "{\"type\":\"message_delta\",\"usage\":{\"output_tokens\":2}}"},state); QVERIFY(e.usageChanged);
    e=adapter->parseSse({"message_stop", "{\"type\":\"message_stop\"}"},state); QVERIFY(e.completed); QCOMPARE(e.usage.totalTokens,qint64(13));
}

void IntegrationTests::codexDeltaDeduplication() {
    auto adapter=ProtocolAdapter::create(Protocol::Codex); AdapterState state;
    auto e=adapter->parseSse({"response.output_text.delta", "{\"type\":\"response.output_text.delta\",\"delta\":\"OK\"}"},state); QCOMPARE(e.delta,QString("OK"));
    e=adapter->parseSse({"response.output_item.done", "{\"type\":\"response.output_item.done\",\"item\":{\"content\":[{\"type\":\"output_text\",\"text\":\"OK\"}]}}"},state); QVERIFY(e.delta.isEmpty());
    e=adapter->parseSse({"response.completed", "{\"type\":\"response.completed\",\"response\":{\"usage\":{\"input_tokens\":3,\"output_tokens\":1,\"total_tokens\":4}}}"},state); QVERIFY(e.completed); QCOMPARE(e.usage.totalTokens,qint64(4));
}

void IntegrationTests::malformedAndSplitSseAreBounded() {
    SseDecoder decoder;
    QVERIFY(decoder.feed("data: {\"x\":1\r").isEmpty());
    auto events=decoder.feed("\ndata: {\"y\":2}\r\n\r\n");
    QCOMPARE(events.size(),1); QCOMPARE(events.first().data,QByteArray("{\"x\":1\n{\"y\":2}"));
    decoder.reset(); auto huge=QByteArray("data: ")+QByteArray(9*1024*1024,'x'); decoder.feed(huge); QVERIFY(decoder.totalBytes()==huge.size()); QVERIFY(!decoder.lastWarning().isEmpty());
    auto adapter=ProtocolAdapter::create(Protocol::OpenAI); AdapterState state; auto bad=adapter->parseSse({{},"{bad json"},state); QVERIFY(bad.recoverable); QVERIFY(!bad.error.isEmpty());
}


void IntegrationTests::modelHeadersAreProtocolAware() {
    Profile profile; profile.apiKey = QStringLiteral("secret");
    profile.protocol = Protocol::Claude;
    auto claude = ProtocolAdapter::create(profile.protocol);
    const auto claudeHeaders = claude->modelHeaders(profile);
    QVERIFY(std::any_of(claudeHeaders.cbegin(), claudeHeaders.cend(), [](const auto& h) { return h.first == "x-api-key"; }));
    profile.protocol = Protocol::Codex;
    auto codex = ProtocolAdapter::create(profile.protocol);
    const auto codexHeaders = codex->modelHeaders(profile);
    QVERIFY(std::any_of(codexHeaders.cbegin(), codexHeaders.cend(), [](const auto& h) { return h.first == "originator"; }));
}

void IntegrationTests::scanServiceFindsModelsAndProtocol() {
    MockHttpServer server; QVERIFY(server.start());
    Profile profile; profile.name = QStringLiteral("scan-mock"); profile.baseUrl = QUrl(QStringLiteral("http://127.0.0.1:%1").arg(server.port())); profile.apiKey = QStringLiteral("test-key"); profile.timeoutSeconds = 2;
    ScanService service;
    ScanResult result;
    QSignalSpy spy(&service, &ScanService::finished);
    service.scan(profile, QStringLiteral("demo"));
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() == 1, 3000);
    result = qvariant_cast<ScanResult>(spy.at(0).at(0));
    QVERIFY(result.checks.size() >= 1);
    QVERIFY(result.score() >= 0 && result.score() <= 100);
}
namespace {
TestResult runBenchmark(MockHttpServer& server, MockHttpServer::FaultMode mode, int timeoutSeconds = 2) {
    server.setFaultMode(mode);
    Profile profile;
    profile.name = QStringLiteral("fault-mock");
    profile.baseUrl = QUrl(QStringLiteral("http://127.0.0.1:%1/v1").arg(server.port()));
    profile.apiKey = QStringLiteral("test");
    profile.protocol = Protocol::OpenAI;
    profile.proxy.mode = ProxyMode::None;
    profile.timeoutSeconds = timeoutSeconds;
    RequestConfig config;
    config.model = QStringLiteral("mock-model");

    QEventLoop loop;
    TestResult result;
    bool got = false;
    auto* job = new BenchmarkJob(profile, config);
    QObject::connect(job, &BenchmarkJob::finished, &loop, [&](const TestResult& value) {
        result = value;
        got = true;
        loop.quit();
    });
    job->start();
    QTimer::singleShot((timeoutSeconds + 3) * 1000, &loop, &QEventLoop::quit);
    loop.exec();
    if (!got) result.status = TestStatus::Error;
    return result;
}
}

void IntegrationTests::htmlWafResponseIsClassified() {
    MockHttpServer server; QVERIFY(server.start());
    const auto result = runBenchmark(server, MockHttpServer::FaultMode::HtmlWaf);
    QCOMPARE(result.status, TestStatus::Error);
    QCOMPARE(result.error.httpStatus, 503);
    QVERIFY(result.error.html);
    QVERIFY(result.error.message.contains(QStringLiteral("HTML")));
    QCOMPARE(server.requestCount(), 1);
}

void IntegrationTests::scanHtmlWafDetailIsReadable() {
    MockHttpServer server; QVERIFY(server.start());
    server.setFaultMode(MockHttpServer::FaultMode::HtmlWaf);
    Profile profile;
    profile.name = QStringLiteral("scan-html-mock");
    profile.baseUrl = QUrl(QStringLiteral("http://127.0.0.1:%1").arg(server.port()));
    profile.apiKey = QStringLiteral("test-key");
    profile.timeoutSeconds = 2;
    ScanService service;
    QSignalSpy spy(&service, &ScanService::finished);
    service.scan(profile, QStringLiteral("demo"));
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() == 1, 3000);
    const auto result = qvariant_cast<ScanResult>(spy.at(0).at(0));
    QVERIFY(!result.checks.isEmpty());
    QVERIFY(std::any_of(result.checks.cbegin(), result.checks.cend(), [](const ScanCheck& check) {
        return check.category == QStringLiteral("html-waf")
            && check.detail.contains(QStringLiteral("HTML/WAF 拦截页"));
    }));
}

void IntegrationTests::httpStatusFailures_data() {
    QTest::addColumn<int>("status");
    QTest::newRow("401 unauthorized") << 401;
    QTest::newRow("403 forbidden") << 403;
    QTest::newRow("429 rate limited") << 429;
    QTest::newRow("500 server error") << 500;
    QTest::newRow("503 unavailable") << 503;
}

void IntegrationTests::httpStatusFailures() {
    QFETCH(int, status);
    MockHttpServer server; QVERIFY(server.start());
    const auto mode = status == 401 ? MockHttpServer::FaultMode::Unauthorized
                    : status == 403 ? MockHttpServer::FaultMode::Forbidden
                    : status == 429 ? MockHttpServer::FaultMode::RateLimited
                    : status == 500 ? MockHttpServer::FaultMode::ServerError
                                    : MockHttpServer::FaultMode::ServiceUnavailable;
    const auto result = runBenchmark(server, mode);
    QCOMPARE(result.status, TestStatus::Error);
    QCOMPARE(result.error.httpStatus, status);
    QVERIFY(!result.passed);
    QCOMPARE(server.requestCount(), 1);
}

void IntegrationTests::delayedResponseTimesOut() {
    MockHttpServer server; QVERIFY(server.start());
    server.setDelayMs(1500);
    const auto result = runBenchmark(server, MockHttpServer::FaultMode::Delay, 1);
    QCOMPARE(result.status, TestStatus::Timeout);
    QVERIFY(result.error.timeout);
    QVERIFY(result.error.network);
    QCOMPARE(server.requestCount(), 1);
}

void IntegrationTests::truncatedStreamIsNotAccepted() {
    MockHttpServer server; QVERIFY(server.start());
    const auto result = runBenchmark(server, MockHttpServer::FaultMode::Disconnect);
    QVERIFY(result.status == TestStatus::Error || result.status == TestStatus::Failed || result.status == TestStatus::Timeout);
    QVERIFY(!result.passed);
    QVERIFY(result.output != QStringLiteral("OK"));
}

void IntegrationTests::duplicateSseEventsAreDeduplicated() {
    MockHttpServer server; QVERIFY(server.start());
    const auto result = runBenchmark(server, MockHttpServer::FaultMode::DuplicateSse);
    QCOMPARE(result.status, TestStatus::Passed);
    QCOMPARE(result.output, QStringLiteral("OK"));
    QVERIFY(result.passed);
}

void IntegrationTests::missingUsageIsEstimated() {
    MockHttpServer server; QVERIFY(server.start());
    const auto result = runBenchmark(server, MockHttpServer::FaultMode::MissingUsage);
    QCOMPARE(result.status, TestStatus::Passed);
    QCOMPARE(result.output, QStringLiteral("OK"));
    QVERIFY(result.estimated);
    QVERIFY(result.metrics.usage.promptTokens >= 0);
    QVERIFY(result.metrics.usage.completionTokens >= 0);
    QVERIFY(result.metrics.usage.totalTokens >= 0);
    QCOMPARE(result.metrics.usage.source, QStringLiteral("estimated"));
}

void IntegrationTests::invalidJsonEventIsRecoverable() {
    MockHttpServer server; QVERIFY(server.start());
    const auto result = runBenchmark(server, MockHttpServer::FaultMode::InvalidJson);
    QCOMPARE(result.status, TestStatus::Passed);
    QCOMPARE(result.output, QStringLiteral("OK"));
    QVERIFY(result.note.contains(QStringLiteral("Malformed")));
}

void IntegrationTests::splitSseBoundariesAreDecoded() {
    MockHttpServer server; QVERIFY(server.start());
    const auto result = runBenchmark(server, MockHttpServer::FaultMode::SplitSse);
    QCOMPARE(result.status, TestStatus::Passed);
    QCOMPARE(result.output, QStringLiteral("OK"));
    QCOMPARE(result.metrics.usage.totalTokens, qint64(4));
}

void IntegrationTests::scanCancelStopsFinishedAndFollowupRequests() {
    MockHttpServer server; QVERIFY(server.start());
    server.setFaultMode(MockHttpServer::FaultMode::Delay);
    server.setDelayMs(2000);
    Profile profile;
    profile.name = QStringLiteral("cancel-mock");
    profile.baseUrl = QUrl(QStringLiteral("http://127.0.0.1:%1").arg(server.port()));
    profile.apiKey = QStringLiteral("test-key");
    profile.timeoutSeconds = 5;
    ScanService service;
    QSignalSpy finished(&service, &ScanService::finished);
    service.scan(profile, QStringLiteral("demo"));
    QTRY_VERIFY_WITH_TIMEOUT(server.requestCount() == 1, 1000);
    service.cancel();
    QTest::qWait(2500);
    QCOMPARE(finished.count(), 0);
    QCOMPARE(server.requestCount(), 1);
    QVERIFY(!service.running());
}

void IntegrationTests::baseUrlWithoutV1IsAutoDetected() {
    Profile profile;
    profile.baseUrl = QUrl(QStringLiteral("https://relay.example.com"));
    RequestConfig config;
    config.model = QStringLiteral("test-model");

    profile.protocol = Protocol::OpenAI;
    auto openAi = ProtocolAdapter::create(profile.protocol);
    const auto openAiAttempts = openAi->completionAttempts(profile, config);
    QVERIFY(!openAiAttempts.isEmpty());
    QCOMPARE(openAiAttempts.first().url.path(), QStringLiteral("/v1/chat/completions"));
    QVERIFY(std::any_of(openAiAttempts.cbegin(), openAiAttempts.cend(), [](const CompletionAttempt& attempt) {
        return attempt.url.path() == QStringLiteral("/chat/completions");
    }));
    const auto openAiModels = openAi->modelUrls(profile);
    QCOMPARE(openAiModels.first().path(), QStringLiteral("/v1/models"));
    QVERIFY(std::any_of(openAiModels.cbegin(), openAiModels.cend(), [](const QUrl& url) {
        return url.path() == QStringLiteral("/models");
    }));

    profile.protocol = Protocol::Claude;
    auto claude = ProtocolAdapter::create(profile.protocol);
    const auto claudeAttempts = claude->completionAttempts(profile, config);
    QCOMPARE(claudeAttempts.first().url.path(), QStringLiteral("/v1/messages"));
    QVERIFY(std::any_of(claudeAttempts.cbegin(), claudeAttempts.cend(), [](const CompletionAttempt& attempt) {
        return attempt.url.path() == QStringLiteral("/messages");
    }));

    profile.protocol = Protocol::Codex;
    auto codex = ProtocolAdapter::create(profile.protocol);
    QCOMPARE(codex->completionAttempts(profile, config).first().url.path(), QStringLiteral("/v1/responses"));

    profile.baseUrl = QUrl(QStringLiteral("https://relay.example.com/v1"));
    profile.protocol = Protocol::OpenAI;
    openAi = ProtocolAdapter::create(profile.protocol);
    const auto versionedAttempts = openAi->completionAttempts(profile, config);
    QVERIFY(std::all_of(versionedAttempts.cbegin(), versionedAttempts.cend(), [](const CompletionAttempt& attempt) {
        return !attempt.url.path().contains(QStringLiteral("/v1/v1/"));
    }));
}

QTEST_MAIN(IntegrationTests)
#include "tst_integration.moc"
