#include "BenchmarkController.h"
#include "network/BenchmarkJob.h"
#include "persistence/ResultRepository.h"
namespace airb {
BenchmarkController::BenchmarkController(QObject*p):QObject(p),repository_(new ResultRepository(this)){
    if(repository_->open(&historyError_))results_=repository_->loadRecent(500,&historyError_);
}
void BenchmarkController::runSingle(const Profile&p,const RequestConfig&c){runBatch(p,c,{c.model},1,1);}
void BenchmarkController::runBatch(const Profile&p,const RequestConfig&base,const QStringList&models,int concurrency,int rounds){
    cancelAll();queue_.clear();done_=0;total_=models.size()*qMax(1,rounds);concurrency_=qBound(1,concurrency,8);cancelling_=false;
    for(int r=0;r<qMax(1,rounds);++r)for(const auto&m:models){auto c=base;c.model=m;queue_.enqueue({p,c});}pump();
}
void BenchmarkController::pump(){
    while(!cancelling_&&active_.size()<concurrency_&&!queue_.isEmpty()){
        auto i=queue_.dequeue();auto*j=new BenchmarkJob(i.p,i.c,this);active_.insert(j->id(),j);
        connect(j,&BenchmarkJob::started,this,&BenchmarkController::jobStarted);connect(j,&BenchmarkJob::delta,this,&BenchmarkController::jobDelta);connect(j,&BenchmarkJob::progress,this,&BenchmarkController::jobProgress);
        connect(j,&BenchmarkJob::finished,this,[this,j](const TestResult&r){active_.remove(j->id());results_.push_back(r);if(results_.size()>5000)results_.removeFirst();QString e;if(repository_&&!repository_->append(r,&e))emit historyWarning(e);done_++;emit resultReady(r);emit batchProgress(done_,total_);pump();if(active_.isEmpty()&&queue_.isEmpty())emit allFinished();});j->start();
    }
    if(active_.isEmpty()&&queue_.isEmpty()&&total_==0)emit allFinished();
}
void BenchmarkController::cancelAll(){cancelling_=true;queue_.clear();const auto jobs=active_.values();for(auto*j:jobs)j->cancel();}
void BenchmarkController::clear(){results_.clear();QString e;if(repository_&&!repository_->clear(&e))emit historyWarning(e);}
SessionStats BenchmarkController::stats()const{SessionStats s;s.total=results_.size();double tt=0,lat=0,sp=0;int nt=0,nl=0,ns=0;for(const auto&r:results_){if(r.passed)s.passed++;else if(r.status!=TestStatus::Cancelled)s.failed++;if(r.status==TestStatus::Timeout)s.timeout++;if(r.metrics.ttftMs>=0){tt+=r.metrics.ttftMs;nt++;}if(r.metrics.totalLatencyMs>=0){lat+=r.metrics.totalLatencyMs;nl++;}if(r.metrics.tokensPerSecond>=0){sp+=r.metrics.tokensPerSecond;ns++;}if(r.metrics.usage.totalTokens>0)s.totalTokens+=r.metrics.usage.totalTokens;}s.passRate=s.total?100.0*s.passed/s.total:0;s.avgTtft=nt?tt/nt:0;s.avgLatency=nl?lat/nl:0;s.avgSpeed=ns?sp/ns:0;return s;}
}
