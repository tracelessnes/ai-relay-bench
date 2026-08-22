#include "Statistics.h"
#include <algorithm>
#include <cmath>
namespace airb { double percentile(QVector<double> v,double p){if(v.isEmpty())return -1;std::sort(v.begin(),v.end());if(v.size()==1)return v[0];double x=p*(v.size()-1);int i=int(std::floor(x));double f=x-i;return i+1<v.size()?v[i]*(1-f)+v[i+1]*f:v.back();} double average(const QVector<double>& v){if(v.isEmpty())return -1;double s=0;for(auto x:v)s+=x;return s/v.size();} double stddev(const QVector<double>& v){if(v.size()<2)return 0;double a=average(v),s=0;for(auto x:v)s+=(x-a)*(x-a);return std::sqrt(s/v.size());} }
