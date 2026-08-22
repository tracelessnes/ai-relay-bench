#pragma once
#include "domain/Types.h"
namespace airb { class ExportService {public:static bool json(const QString&,const QList<TestResult>&,QString*error=nullptr);static bool csv(const QString&,const QList<TestResult>&,QString*error=nullptr);static bool markdown(const QString&,const QList<TestResult>&,QString*error=nullptr);}; }
