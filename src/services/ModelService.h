#pragma once
#include "domain/Types.h"
#include "protocol/ProtocolAdapter.h"
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QElapsedTimer>
#include <QQueue>
#include <QSet>
#include <memory>

namespace airb {
class ModelService final : public QObject {
    Q_OBJECT
public:
    explicit ModelService(QObject* parent = nullptr);
    void fetch(const Profile& profile);
    void cancel();
signals:
    void modelsReady(const QList<ModelInfo>& models, const QString& message);
    void status(const QString& message);
private:
    struct DetailTask { int modelIndex=-1; int urlIndex=0; QList<QUrl> urls; };
    void tryNext();
    void startDetailLookup(QList<ModelInfo> models, const QUrl& modelsUrl, const QString& sourceMessage);
    void pumpDetails();
    void finishDetails();
    void enrichFromModelsDev(QList<ModelInfo> models, const QString& sourceMessage);
    static QList<QUrl> detailUrls(const QUrl& modelsUrl,const QString& modelId);
    static void mergeCatalog(QList<ModelInfo>& models, const QByteArray& catalog);
    static qint64 detailContext(const QByteArray& body);
    Profile profile_;
    std::unique_ptr<ProtocolAdapter> adapter_;
    QList<QUrl> urls_;
    int index_ = 0;
    QNetworkAccessManager manager_;
    QPointer<QNetworkReply> reply_;
    QSet<QNetworkReply*> detailReplies_;
    QQueue<DetailTask> detailQueue_;
    QList<ModelInfo> detailModels_;
    QString detailMessage_;
    int activeDetails_ = 0;
    QElapsedTimer budget_;
    bool detailsFinishing_=false;
    quint64 generation_=0;
};
}
