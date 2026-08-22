#include <QtTest>
#include <QtNetwork>
#include <QEventLoop>
#include <QTimer>
#include <algorithm>
#include "domain/Types.h"
#include "protocol/SseDecoder.h"
#include "protocol/ProtocolAdapter.h"
#include "network/BenchmarkJob.h"

using namespace airb;

class MockHttpServer final : public QObject {
    Q_OBJECT
public:
    explicit MockHttpServer(QObject* parent=nullptr):QObject(parent) {
        connect(&server_, &QTcpServer::newConnection, this, &MockHttpServer::accept);
    }
    bool start() { return server_.listen(QHostAddress::LocalHost, 0); }
    quint16 port() const { return server_.serverPort(); }
    int requestCount() const { return requestCount_; }
    QByteArray requestBody(int i) const { return i >= 0 && i < requests_.size() ? requests_[i].body : QByteArray{}; }
private:
    struct Request { QByteArray path; QByteArray body; };
    struct ClientState { QByteArray buffer; bool responded=false; };
    QTcpServer server_;
    QHash<QTcpSocket*, ClientState> clients_;
    QList<Request> requests_;
    int requestCount_=0;

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
        const auto request=state.buffer.left(split+4+len);
        const auto firstLine=headers.left(headers.indexOf('\n')).trimmed();
        const auto parts=firstLine.split(' ');
        const QByteArray path=parts.size()>1?parts[1]:QByteArray{};
        const QByteArray body=state.buffer.mid(split+4,len);
        requests_.push_back({path,body});
        ++requestCount_;
        if (requestCount_==1 && body.contains("stream_options")) {
            send(socket, 400, "application/json", {"{\"error\":{\"type\":\"invalid_request_error\",\"code\":\"stream_options\",\"message\":\"stream_options unsupported\"}}"});
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
        const QByteArray header=QByteArray("HTTP/1.1 ")+QByteArray::number(status)+(status==200?" OK":" Bad Request")+
            "\r\nContent-Type: "+contentType+"\r\nContent-Length: "+QByteArray::number(total.size())+"\r\nConnection: close\r\n\r\n";
        socket->write(header);
        for (const auto& chunk:chunks) socket->write(chunk);
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
