#pragma once
#include <QVector>
namespace airb { double percentile(QVector<double> values, double p); double average(const QVector<double>& values); double stddev(const QVector<double>& values); }
