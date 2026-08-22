#pragma once
#include "domain/Types.h"
#include <QObject>
#include <QQueue>
#include <QHash>

namespace airb {
class BenchmarkJob;
class ResultRepository;
class BenchmarkController final : public QObject {
    Q_OBJECT
public:
    explicit BenchmarkController(QObject* parent=nullptr);
    void runSingle(const Profile&,const RequestConfig&);
    void runBatch(const Profile&,const RequestConfig&,const QStringList& models,int concurrency=3,int rounds=1);
    void cancelAll();
    QList<TestResult> results() const{return results_;}
    SessionStats stats() const;
    void clear();
    QString historyError() const{return historyError_;}
signals:
    void jobStarted(const QString&,const QString&);
    void jobDelta(const QString&,const QString&);
    void jobProgress(const QString&,const QString&);
    void resultReady(const TestResult&);
    void batchProgress(int done,int total);
    void allFinished();
    void historyWarning(const QString&);
private:
    struct Item{Profile p;RequestConfig c;};
    void pump();
    QQueue<Item> queue_; QHash<QString,BenchmarkJob*> active_; QList<TestResult> results_;
    ResultRepository* repository_=nullptr; int concurrency_=1,total_=0,done_=0; bool cancelling_=false; QString historyError_;
};
}
