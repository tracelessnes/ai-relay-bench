#include "Statistics.h"
#include <algorithm>
#include <cmath>
#include <QMap>
namespace airb {
double percentile(QVector<double> v,double p){if(v.isEmpty())return -1;std::sort(v.begin(),v.end());if(v.size()==1)return v[0];double x=p*(v.size()-1);int i=int(std::floor(x));double f=x-i;return i+1<v.size()?v[i]*(1-f)+v[i+1]*f:v.back();}
double average(const QVector<double>& v){if(v.isEmpty())return -1;double s=0;for(auto x:v)s+=x;return s/v.size();}
double stddev(const QVector<double>& v){if(v.size()<2)return 0;double a=average(v),s=0;for(auto x:v)s+=(x-a)*(x-a);return std::sqrt(s/v.size());}
QList<CompareSummary> compareResults(const QList<TestResult>& results) {
    struct Bucket { int total=0; int passed=0; QVector<double> ttft; QVector<double> latency; QVector<double> speed; };
    QMap<QString, Bucket> buckets;
    for (const auto& result : results) {
        const QString label = result.profileName.isEmpty() ? QStringLiteral("未命名站点") : result.profileName;
        auto& bucket = buckets[label];
        ++bucket.total;
        if (result.passed) ++bucket.passed;
        if (result.metrics.ttftMs >= 0) bucket.ttft.append(result.metrics.ttftMs);
        if (result.metrics.totalLatencyMs >= 0) bucket.latency.append(result.metrics.totalLatencyMs);
        if (result.metrics.tokensPerSecond >= 0) bucket.speed.append(result.metrics.tokensPerSecond);
    }
    QList<CompareSummary> output;
    for (auto it = buckets.cbegin(); it != buckets.cend(); ++it) {
        CompareSummary summary;
        summary.label = it.key();
        summary.total = it.value().total;
        summary.passed = it.value().passed;
        summary.passRate = summary.total ? 100.0 * summary.passed / summary.total : 0;
        summary.avgTtft = average(it.value().ttft);
        summary.p50Ttft = percentile(it.value().ttft, 0.5);
        summary.p95Ttft = percentile(it.value().ttft, 0.95);
        summary.avgLatency = average(it.value().latency);
        summary.p50Latency = percentile(it.value().latency, 0.5);
        summary.p95Latency = percentile(it.value().latency, 0.95);
        summary.avgSpeed = average(it.value().speed);
        output.append(summary);
    }
    return output;
}
}
