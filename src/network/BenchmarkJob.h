#pragma once
#include "domain/Types.h"
#include "protocol/ProtocolAdapter.h"
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QElapsedTimer>
#include <QTimer>
#include <QPointer>
#include <QSet>

namespace airb {
class BenchmarkJob final : public QObject {
    Q_OBJECT
public:
    BenchmarkJob(Profile profile, RequestConfig config, QObject* parent=nullptr);
    void start();
    void cancel();
    const QString& id() const { return id_; }
signals:
    void started(const QString& id, const QString& label);
    void delta(const QString& id, const QString& text);
    void progress(const QString& id, const QString& state);
    void finished(const TestResult& result);
private slots:
    void sendAttempt();
    void onReadyRead();
    void onFinished();
    void onTimeout();
private:
    void beginAttempt();
    bool handleEvents(const QList<SseEvent>& events);
    void appendAttemptRaw();
    void complete(TestResult result);
    int nextRetryIndex(int status,bool networkError) const;
    TestResult makeResult(TestStatus status=TestStatus::Error) const;
    ErrorInfo parseError(int status, const QByteArray& body, const QString& reason, bool network=false) const;
    bool matches(const QString& text) const;

    Profile profile_;
    RequestConfig config_;
    QString id_;
    std::unique_ptr<ProtocolAdapter> adapter_;
    QList<CompletionAttempt> attempts_;
    int attemptIndex_ = 0;
    QNetworkAccessManager manager_;
    QPointer<QNetworkReply> reply_;
    SseDecoder decoder_;
    AdapterState parserState_;
    QElapsedTimer overallClock_;
    QElapsedTimer attemptClock_;
    qint64 firstByteNs_ = -1;
    qint64 firstTextNs_ = -1;
    qint64 finishedAttemptNs_ = -1;
    qint64 transferredBytes_=0;
    QByteArray attemptRaw_;
    QByteArray combinedRaw_;
    QString output_;
    QStringList warnings_;
    QTimer timeoutTimer_;
    bool finished_ = false;
    bool streamCompleted_=false;
    QSet<QByteArray> seenEventSignatures_;
};
}
