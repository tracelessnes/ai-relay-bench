#pragma once
#include "domain/Types.h"
#include <QObject>
#include <QSqlDatabase>

namespace airb {
class ResultRepository final : public QObject {
    Q_OBJECT
public:
    explicit ResultRepository(QObject* parent=nullptr);
    ~ResultRepository() override;
    bool open(QString* error=nullptr);
    bool append(const TestResult& result, QString* error=nullptr);
    QList<TestResult> loadRecent(int limit=500, QString* error=nullptr) const;
    bool clear(QString* error=nullptr);
    QString databasePath() const { return path_; }
private:
    bool ensureSchema(QString* error);
    QString connectionName_;
    QString path_;
    QSqlDatabase db_;
};
}
