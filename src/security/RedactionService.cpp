#include "RedactionService.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

namespace airb {
namespace {
QString normalizedKey(QString key) {
    key = key.trimmed().toLower();
    key.remove('-');
    key.remove('_');
    key.remove(' ');
    return key;
}

bool looksLikeJson(const QString& text, QJsonDocument* document) {
    QJsonParseError error;
    const auto parsed = QJsonDocument::fromJson(text.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || parsed.isNull()) return false;
    if (document) *document = parsed;
    return true;
}

QString maskValue(const QString& value, bool preserveShape) {
    Q_UNUSED(preserveShape);
    if (value.size() < 8) return QStringLiteral("****");
    const int prefix = qMin(4, value.size() / 4);
    const int suffix = qMin(4, value.size() / 4);
    return value.left(prefix) + QStringLiteral("****") + value.right(suffix);
}

QString redactHeaderText(QString line, bool preserveShape) {
    static const QRegularExpression bearer(
        QStringLiteral(R"((?i)(\bBearer\s+)([^\s,;\r\n]+))"));
    line.replace(bearer, QStringLiteral("\\1") + maskValue(QStringLiteral("sensitive-token"), preserveShape));

    static const QRegularExpression keyValue(
        QStringLiteral(R"((?i)(\b(?:authorization|x-api-key|api-key|apikey|proxy-password|password|secret|token|cookie|set-cookie)\s*[:=]\s*)([^\s,;\r\n]+))"));
    line.replace(keyValue, QStringLiteral("\\1") + maskValue(QStringLiteral("sensitive-value"), preserveShape));
    return line;
}
} // namespace

bool RedactionService::isSensitiveKey(const QString& key) {
    const auto normalized = normalizedKey(key);
    static const QSet<QString> keys{
        QStringLiteral("authorization"), QStringLiteral("xapikey"), QStringLiteral("apikey"),
        QStringLiteral("api_key"), QStringLiteral("apisecret"), QStringLiteral("key"),
        QStringLiteral("cookie"), QStringLiteral("setcookie"), QStringLiteral("password"),
        QStringLiteral("secret"), QStringLiteral("token"), QStringLiteral("accesstoken"),
        QStringLiteral("refreshtoken"), QStringLiteral("proxyusername"), QStringLiteral("proxypassword")
    };
    if (keys.contains(normalized)) return true;
    return normalized.contains(QStringLiteral("apikey")) || normalized.contains(QStringLiteral("password")) ||
           normalized.contains(QStringLiteral("secret")) || normalized.endsWith(QStringLiteral("token")) ||
           normalized.contains(QStringLiteral("authorization"));
}

QString RedactionService::mask(const QString& value, bool preserveShape) {
    return maskValue(value, preserveShape);
}

QJsonValue RedactionService::jsonValue(const QJsonValue& value, bool preserveShape) {
    if (value.isObject()) {
        QJsonObject result;
        const auto object = value.toObject();
        for (auto it = object.begin(); it != object.end(); ++it) {
            if (isSensitiveKey(it.key()) && (it.value().isString() || it.value().isDouble() || it.value().isBool())) {
                result.insert(it.key(), mask(it.value().toVariant().toString(), preserveShape));
            } else {
                result.insert(it.key(), jsonValue(it.value(), preserveShape));
            }
        }
        return result;
    }
    if (value.isArray()) {
        QJsonArray result;
        for (const auto item : value.toArray()) result.append(jsonValue(item, preserveShape));
        return result;
    }
    return value;
}

QList<QPair<QByteArray, QByteArray>> RedactionService::headers(
    const QList<QPair<QByteArray, QByteArray>>& input, bool preserveShape) {
    QList<QPair<QByteArray, QByteArray>> result;
    for (const auto& header : input) {
        const auto key = QString::fromUtf8(header.first);
        const auto lower = normalizedKey(key);
        auto value = QString::fromUtf8(header.second);
        if (isSensitiveKey(key) || lower == QStringLiteral("proxyauthorization")) value = mask(value, preserveShape);
        else if (lower == QStringLiteral("authorization")) value = mask(value, preserveShape);
        result.append({header.first, value.toUtf8()});
    }
    return result;
}

QString RedactionService::text(const QString& input, bool preserveShape) {
    QJsonDocument document;
    if (looksLikeJson(input.trimmed(), &document)) {
        const auto redacted = jsonValue(document.isArray() ? QJsonValue(document.array()) : QJsonValue(document.object()), preserveShape);
        return QJsonDocument(redacted.isArray() ? QJsonDocument(redacted.toArray()) : QJsonDocument(redacted.toObject()))
            .toJson(QJsonDocument::Indented);
    }

    QStringList lines = input.split(QRegularExpression(QStringLiteral("\\r\\n|\\n|\\r")), Qt::KeepEmptyParts);
    for (auto& line : lines) {
        const auto trimmed = line.trimmed();
        if (trimmed.startsWith(QStringLiteral("data:"), Qt::CaseInsensitive)) {
            const auto prefixLength = line.indexOf(trimmed);
            const auto payload = trimmed.mid(5).trimmed();
            QJsonDocument eventDocument;
            if (looksLikeJson(payload, &eventDocument)) {
                const auto redacted = jsonValue(eventDocument.isArray() ? QJsonValue(eventDocument.array()) : QJsonValue(eventDocument.object()), preserveShape);
                const auto serialized = QJsonDocument(redacted.isArray() ? QJsonDocument(redacted.toArray()) : QJsonDocument(redacted.toObject())).toJson(QJsonDocument::Compact);
                line = line.left(prefixLength) + QStringLiteral("data: ") + QString::fromUtf8(serialized).trimmed();
                continue;
            }
        }
        line = redactHeaderText(line, preserveShape);
    }
    return lines.join(QStringLiteral("\n"));
}

} // namespace airb
