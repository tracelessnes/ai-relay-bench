#pragma once

#include "domain/Types.h"
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QPointer>
#include <QTimer>

namespace airb {

struct EndpointCandidate {
    QUrl url;
    QString label;
};

QList<EndpointCandidate> endpointCandidates(const QUrl& base, const QString& suffix);

struct ScanCheck {
    QString id;
    QString label;
    QString status; // pass / warn / fail / skipped
    QString detail;
    int httpStatus = 0;
    bool passed = false;
    QString category;
};

struct ScanResult {
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QDateTime timestamp = QDateTime::currentDateTime();
    QString profileName;
    QString model;
    Protocol protocol = Protocol::OpenAI;
    QList<ScanCheck> checks;
    QString diagnostic;
    QStringList discoveredModels;
    qint64 maxContextLength = -1;
    bool modelsSupported = false;
    bool streamSupported = false;
    bool usageSupported = false;
    bool streamOptionsSupported = false;
    bool htmlIntercepted = false;
    int score() const;
};

QJsonObject toJson(const ScanCheck& check);
ScanCheck scanCheckFromJson(const QJsonObject& object);
QJsonObject toJson(const ScanResult& result);
ScanResult scanResultFromJson(const QJsonObject& object);

class ScanService final : public QObject {
    Q_OBJECT
public:
    explicit ScanService(QObject* parent = nullptr);
    void scan(const Profile& profile, const QString& model);
    void cancel();
    bool running() const { return running_; }

signals:
    void progress(const QString& message);
    void checkReady(const airb::ScanCheck& check);
    void finished(const airb::ScanResult& result);

private slots:
    void onReadyRead();
    void onFinished();
    void onTimeout();

private:
    struct Plan {
        QString id;
        QString label;
        QNetworkRequest request;
        QByteArray body;
        bool stream = false;
        bool streamOptions = false;
    };
    void buildPlans();
    void sendNext();
    void finishCurrent(const ScanCheck& check);
    ScanCheck classifyCurrent(int status, const QByteArray& body, const QString& reason,
                              const QString& transportError);
    static bool isHtml(const QByteArray& body);
    static QString bodySummary(const QByteArray& body);
    static QString structuredError(const QByteArray& body);
    static void configureProxy(QNetworkAccessManager& manager, const Profile& profile);

    Profile profile_;
    QString model_;
    ScanResult result_;
    QList<Plan> plans_;
    int planIndex_ = -1;
    QNetworkAccessManager manager_;
    QPointer<QNetworkReply> reply_;
    QByteArray body_;
    QTimer timeout_;
    bool running_ = false;
    quint64 generation_ = 0;
};

} // namespace airb

Q_DECLARE_METATYPE(airb::ScanCheck)
Q_DECLARE_METATYPE(airb::ScanResult)
