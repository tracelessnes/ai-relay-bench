#include "ExportService.h"
#include "security/RedactionService.h"

#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTextStream>

namespace airb {
namespace {
bool writeFile(const QString& path, const QByteArray& bytes, QString* error) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    if (file.write(bytes) != bytes.size()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

QString csvEscape(QString value) {
    value = RedactionService::text(value);
    value.replace('"', QStringLiteral("\"\""));
    return '"' + value + '"';
}

QString markdownEscape(QString value) {
    value = RedactionService::text(value);
    value.replace('|', QStringLiteral("\\|"));
    value.replace('\n', QStringLiteral("<br>"));
    value.replace('\r', QString());
    return value;
}

TestResult redactedResult(TestResult result) {
    result.output = RedactionService::text(result.output);
    result.rawResponse = RedactionService::text(result.rawResponse);
    result.error.message = RedactionService::text(result.error.message);
    result.error.rawSummary = RedactionService::text(result.error.rawSummary);
    result.note = RedactionService::text(result.note);
    return result;
}
} // namespace

bool ExportService::json(const QString& path, const QList<TestResult>& results, QString* error) {
    QJsonArray array;
    for (const auto& result : results) array.push_back(toJson(redactedResult(result)));
    const QJsonObject root{{"application", "AI Relay Station Benchmark & Tester"},
                           {"version", APP_VERSION},
                           {"exportedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
                           {"results", array}};
    return writeFile(path, QJsonDocument(root).toJson(QJsonDocument::Indented), error);
}

bool ExportService::csv(const QString& path, const QList<TestResult>& results, QString* error) {
    QString text = QStringLiteral("Timestamp,Profile,Protocol,Model,Status,TTFT_ms,FirstByte_ms,FirstText_ms,Generation_ms,Total_ms,Tokens_per_s,DNS_ms,TCP_ms,TLS_ms,Request_ms,Prompt_tokens,Completion_tokens,Total_tokens,Bytes,Output,Error\r\n");
    for (const auto& original : results) {
        const auto result = redactedResult(original);
        text += QStringList{csvEscape(result.timestamp.toString(Qt::ISODate)), csvEscape(result.profileName),
                            csvEscape(protocolName(result.protocol)), csvEscape(result.model), csvEscape(statusName(result.status)),
                            QString::number(result.metrics.ttftMs, 'f', 2), QString::number(result.metrics.timing.firstByteMs, 'f', 2),
                            QString::number(result.metrics.timing.firstTextMs, 'f', 2), QString::number(result.metrics.generationMs, 'f', 2),
                            QString::number(result.metrics.totalLatencyMs, 'f', 2), QString::number(result.metrics.tokensPerSecond, 'f', 2),
                            QString::number(result.metrics.timing.dnsMs, 'f', 2), QString::number(result.metrics.timing.tcpMs, 'f', 2),
                            QString::number(result.metrics.timing.tlsMs, 'f', 2), QString::number(result.metrics.timing.requestMs, 'f', 2),
                            QString::number(result.metrics.usage.promptTokens), QString::number(result.metrics.usage.completionTokens),
                            QString::number(result.metrics.usage.totalTokens), QString::number(result.metrics.responseBytes),
                            csvEscape(result.output), csvEscape(result.error.message)}.join(',') + QStringLiteral("\r\n");
    }
    return writeFile(path, QByteArray("\xEF\xBB\xBF") + text.toUtf8(), error);
}

bool ExportService::markdown(const QString& path, const QList<TestResult>& results, QString* error) {
    QString text = QStringLiteral("| Time | Profile | Protocol | Model | Status | TTFT ms | First byte ms | Generation ms | Total ms | Tokens/s | Usage P/C/T |\n|---|---|---|---|---:|---:|---:|---:|---|\n");
    for (const auto& original : results) {
        const auto result = redactedResult(original);
        text += QStringLiteral("| %1 | %2 | %3 | %4 | %5 | %6 | %7 | %8 | %9 | %10 | %11 | %12/%13/%14 |\n")
                    .arg(markdownEscape(result.timestamp.toString("HH:mm:ss")), markdownEscape(result.profileName),
                         markdownEscape(protocolName(result.protocol)), markdownEscape(result.model), markdownEscape(statusName(result.status)))
                    .arg(result.metrics.ttftMs, 0, 'f', 1)
                    .arg(result.metrics.timing.firstByteMs, 0, 'f', 1)
                    .arg(result.metrics.generationMs, 0, 'f', 1)
                    .arg(result.metrics.totalLatencyMs, 0, 'f', 1)
                    .arg(result.metrics.tokensPerSecond, 0, 'f', 1)
                    .arg(result.metrics.usage.promptTokens)
                    .arg(result.metrics.usage.completionTokens)
                    .arg(result.metrics.usage.totalTokens);
    }
    return writeFile(path, text.toUtf8(), error);
}

} // namespace airb
