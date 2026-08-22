#pragma once
#include "domain/Types.h"
#include "SseDecoder.h"
#include <QNetworkRequest>
#include <memory>

namespace airb {
struct CompletionAttempt {
    QUrl url;
    QByteArray body;
    QList<QPair<QByteArray,QByteArray>> headers;
    bool streaming=true;
    QString label;
    int mode=0;
};
struct AdapterState { bool gotDelta=false; Usage usage; QString model; int malformedEvents=0; };
struct AdapterEvent {
    QString delta;
    Usage usage;
    bool usageChanged=false;
    bool completed=false;
    QString error;
    bool recoverable=false;
};

class ProtocolAdapter {
public:
    virtual ~ProtocolAdapter()=default;
    virtual QList<QUrl> modelUrls(const Profile&) const=0;
    virtual QList<CompletionAttempt> completionAttempts(const Profile&,const RequestConfig&) const=0;
    virtual AdapterEvent parseSse(const SseEvent&,AdapterState&) const=0;
    virtual AdapterEvent parseNonStream(const QByteArray&,AdapterState&) const=0;
    virtual QList<ModelInfo> parseModels(const QByteArray&,QString* error=nullptr) const;
    virtual bool shouldRetry(const CompletionAttempt&,int,const QByteArray&,bool networkError) const;
    static std::unique_ptr<ProtocolAdapter> create(Protocol);
protected:
    static QUrl endpoint(const QUrl& base,const QString& suffix);
    static QList<QPair<QByteArray,QByteArray>> commonHeaders(const Profile&,bool sse=true);
    static void addCustomHeaders(QList<QPair<QByteArray,QByteArray>>&,const QJsonObject&);
    static Usage openAIUsage(const QJsonObject&);
};
}
