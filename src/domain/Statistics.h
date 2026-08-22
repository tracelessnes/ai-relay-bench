#pragma once
#include "Types.h"
#include <QVector>
namespace airb {
struct CompareSummary {
    QString label;
    int total = 0;
    int passed = 0;
    double passRate = 0;
    double avgTtft = -1;
    double p50Ttft = -1;
    double p95Ttft = -1;
    double avgLatency = -1;
    double p50Latency = -1;
    double p95Latency = -1;
    double avgSpeed = -1;
};
double percentile(QVector<double> values, double p);
double average(const QVector<double>& values);
double stddev(const QVector<double>& values);
QList<CompareSummary> compareResults(const QList<TestResult>& results);
}
