#pragma once
#include "domain/Types.h"
#include <QObject>
namespace airb {
class ProfileStore final : public QObject {
    Q_OBJECT
public:
    explicit ProfileStore(QObject* parent = nullptr);
    QList<Profile> load() const;
    void save(const QList<Profile>& profiles) const;
    static bool exportFile(const QString& path,const QList<Profile>& profiles,QString* error=nullptr);
    static QList<Profile> importFile(const QString& path,QString* error=nullptr);
};
}
