#include <QtTest>
#include <algorithm>
#include "domain/Types.h"
#include "domain/Statistics.h"
#include "protocol/SseDecoder.h"
#include "security/RedactionService.h"
#include "services/ScanService.h"
using namespace airb;
class CoreTests:public QObject{
Q_OBJECT
private slots:
void context(){QCOMPARE(parseContext("128K"),qint64(128000));QCOMPARE(parseContext("1M"),qint64(1000000));QCOMPARE(formatContext(128000),QString("128K"));}
void output(){QCOMPARE(sanitizeOutput(QString::fromUtf8("  ‘OK’  ")),QString("OK"));QCOMPARE(estimateTokens(QString::fromUtf8("你好")),qint64(2));}
void stats(){QVector<double>v{10,20,30,40,50};QCOMPARE(percentile(v,.5),30.0);QCOMPARE(percentile(v,.95),48.0);}
void scanEndpoints(){
    const auto candidates=endpointCandidates(QUrl(QStringLiteral("https://relay.example.com")), QStringLiteral("models"));
    QCOMPARE(candidates.size(), 2);
    QCOMPARE(candidates.at(0).url.path(), QStringLiteral("/v1/models"));
    QCOMPARE(candidates.at(1).url.path(), QStringLiteral("/models"));
    const auto versioned=endpointCandidates(QUrl(QStringLiteral("https://relay.example.com/v1/")), QStringLiteral("models"));
    QCOMPARE(versioned.size(), 2);
    QVERIFY(std::all_of(versioned.cbegin(), versioned.cend(), [](const EndpointCandidate& item){ return !item.url.path().contains(QStringLiteral("/v1/v1/")); }));

    ScanResult result;
    result.checks.append({QStringLiteral("models"), QStringLiteral("Models"), QStringLiteral("pass"), QStringLiteral("18 models"), 200, true});
    result.checks.append({QStringLiteral("openai"), QStringLiteral("OpenAI"), QStringLiteral("warn"), QStringLiteral("stream_options unsupported"), 400, false});
    QCOMPARE(result.score(), 50);
}
void timingSerialization(){
    TestResult result;
    result.metrics.timing.requestMs=12.5;
    result.metrics.timing.firstByteMs=123.0;
    result.metrics.timing.firstTextMs=240.0;
    result.metrics.timing.generationMs=800.0;
    result.metrics.timing.totalMs=1040.0;
    const auto restored=testResultFromJson(toJson(result));
    QCOMPARE(restored.metrics.timing.dnsMs,-1.0);
    QCOMPARE(restored.metrics.timing.requestMs,12.5);
    QCOMPARE(restored.metrics.timing.firstByteMs,123.0);
    QCOMPARE(restored.metrics.timing.firstTextMs,240.0);
    QCOMPARE(restored.metrics.timing.generationMs,800.0);
    QCOMPARE(restored.metrics.timing.totalMs,1040.0);
}
void redaction(){
    const QString json=QStringLiteral(R"({"authorization":"Bearer sk-super-secret-value","nested":{"x-api-key":"short","safe":"visible"},"items":[{"password":"hunter2"}]})");
    const QString clean=RedactionService::text(json);
    QVERIFY(!clean.contains(QStringLiteral("super-secret")));
    QVERIFY(!clean.contains(QStringLiteral("hunter2")));
    QVERIFY(clean.contains(QStringLiteral("visible")));
    QVERIFY(clean.contains(QStringLiteral("****")));

    const QString sse=QStringLiteral("event: message\ndata: {\"token\":\"abcdef123456\",\"text\":\"OK\"}\n\nAuthorization: Bearer secret-token-value\n");
    const QString cleanSse=RedactionService::text(sse);
    QVERIFY(!cleanSse.contains(QStringLiteral("abcdef123456")));
    QVERIFY(!cleanSse.contains(QStringLiteral("secret-token-value")));
    QVERIFY(cleanSse.contains(QStringLiteral("OK")));

    const QList<QPair<QByteArray,QByteArray>> headers{{"Content-Type","application/json"},{"X-Api-Key","abcdefghijklmnop"}};
    const auto cleanHeaders=RedactionService::headers(headers);
    QCOMPARE(cleanHeaders.at(0).second,QByteArray("application/json"));
    QVERIFY(cleanHeaders.at(1).second!=QByteArray("abcdefghijklmnop"));
}
void sse(){SseDecoder d;auto a=d.feed("event: message\r\ndata: {\"x\":");QVERIFY(a.isEmpty());auto b=d.feed("1}\r\n\r\n");QCOMPARE(b.size(),1);QCOMPARE(b[0].event,QByteArray("message"));QCOMPARE(b[0].data,QByteArray("{\"x\":1}"));}
};
QTEST_MAIN(CoreTests)
#include "tst_core.moc"
