#include "HarExporter.h"
#include "security/RedactionService.h"
#include <QJsonArray>
#include <QJsonDocument>
namespace airb {
static CaptureHeaders safeHeaders(const CaptureHeaders& headers) {
    QList<QPair<QByteArray, QByteArray>> pairs; for (const auto& h : headers) pairs.append({h.name, h.value});
    const auto safe = RedactionService::headers(pairs, false); CaptureHeaders out;
    for (const auto& h : safe) out.append({h.first, h.second == "****" ? QByteArray("[REDACTED]") : h.second});
    return out;
}
static QByteArray safeBody(const QByteArray& body) { auto value = RedactionService::text(QString::fromUtf8(body), false).toUtf8(); value.replace("****", "[REDACTED]"); return value; }
static QJsonArray harHeaders(const CaptureHeaders& headers) { QJsonArray out; for (const auto& h : safeHeaders(headers)) out.append(QJsonObject{{"name", QString::fromUtf8(h.name)}, {"value", QString::fromUtf8(h.value)}}); return out; }
QByteArray HarExporter::toJson(const QList<CaptureExchange>& exchanges, bool includeBody) {
    QJsonArray entries;
    for (const auto& x : exchanges) {
        QJsonObject request{{"method", x.request.method}, {"url", x.request.url.isValid() ? x.request.url.toString() : x.request.target}, {"httpVersion", "HTTP/1.1"}, {"headers", harHeaders(x.request.headers)}, {"queryString", QJsonArray{}}, {"headersSize", -1}, {"bodySize", x.request.body.size()}};
        QJsonObject response{{"status", x.response.statusCode}, {"statusText", QString::fromUtf8(x.response.reason)}, {"httpVersion", "HTTP/1.1"}, {"headers", harHeaders(x.response.headers)}, {"cookies", QJsonArray{}}, {"redirectURL", ""}, {"headersSize", -1}, {"bodySize", x.response.body.size()}};
        if (includeBody && !x.request.body.isEmpty()) request["postData"] = QJsonObject{{"mimeType", QString::fromUtf8(headerValue(x.request.headers, "content-type"))}, {"text", QString::fromUtf8(safeBody(x.request.body))}};
        if (includeBody) response["content"] = QJsonObject{{"size", x.response.body.size()}, {"mimeType", QString::fromUtf8(headerValue(x.response.headers, "content-type"))}, {"text", QString::fromUtf8(safeBody(x.response.body))}};
        entries.append(QJsonObject{{"startedDateTime", x.timestamp.toUTC().toString(Qt::ISODateWithMs)}, {"time", x.totalMs}, {"request", request}, {"response", response}, {"cache", QJsonObject{}}, {"timings", QJsonObject{{"send", 0}, {"wait", x.totalMs}, {"receive", 0}}}, {"comment", captureClientName(x.request.client)}});
    }
    return QJsonDocument(QJsonObject{{"log", QJsonObject{{"version", "1.2"}, {"creator", QJsonObject{{"name", "AI Relay Bench"}, {"version", "1.3"}}}, {"entries", entries}}}}).toJson(QJsonDocument::Indented);
}
QString HarExporter::toMarkdown(const QList<CaptureExchange>& exchanges) {
    QString out = QStringLiteral("| ID | Time | Method | URL | Status | Client | Duration ms | Bytes |\n|---|---|---|---|---:|---|---:|---:|\n");
    for (const auto& x : exchanges) out += QStringLiteral("| %1 | %2 | %3 | %4 | %5 | %6 | %7 | %8 |\n").arg(x.id, x.timestamp.toString(Qt::ISODate), x.request.method, QString(x.request.target).replace('|', QStringLiteral("\\|")), QString::number(x.response.statusCode), captureClientName(x.request.client), QString::number(x.totalMs, 'f', 1), QString::number(x.responseBytes));
    return out;
}
}
