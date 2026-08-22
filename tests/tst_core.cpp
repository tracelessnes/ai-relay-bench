#include <QtTest>
#include "domain/Types.h"
#include "domain/Statistics.h"
#include "protocol/SseDecoder.h"
using namespace airb;
class CoreTests:public QObject{
Q_OBJECT
private slots:
void context(){QCOMPARE(parseContext("128K"),qint64(128000));QCOMPARE(parseContext("1M"),qint64(1000000));QCOMPARE(formatContext(128000),QString("128K"));}
void output(){QCOMPARE(sanitizeOutput(QString::fromUtf8("  ‘OK’  ")),QString("OK"));QCOMPARE(estimateTokens(QString::fromUtf8("你好")),qint64(2));}
void stats(){QVector<double>v{10,20,30,40,50};QCOMPARE(percentile(v,.5),30.0);QCOMPARE(percentile(v,.95),48.0);}
void sse(){SseDecoder d;auto a=d.feed("event: message\r\ndata: {\"x\":");QVERIFY(a.isEmpty());auto b=d.feed("1}\r\n\r\n");QCOMPARE(b.size(),1);QCOMPARE(b[0].event,QByteArray("message"));QCOMPARE(b[0].data,QByteArray("{\"x\":1}"));}
};
QTEST_MAIN(CoreTests)
#include "tst_core.moc"
