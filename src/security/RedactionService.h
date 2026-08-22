#pragma once

#include <QJsonValue>
#include <QList>
#include <QPair>
#include <QString>

namespace airb {

class RedactionService final {
public:
    static QString text(const QString& input, bool preserveShape = true);
    static QJsonValue jsonValue(const QJsonValue& value, bool preserveShape = true);
    static QList<QPair<QByteArray, QByteArray>> headers(const QList<QPair<QByteArray, QByteArray>>& input,
                                                         bool preserveShape = true);

private:
    static bool isSensitiveKey(const QString& key);
    static QString mask(const QString& value, bool preserveShape);
};

} // namespace airb
