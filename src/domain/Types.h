#pragma once
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QUuid>
#include <QStringList>

namespace airb {

enum class Protocol { OpenAI, Claude, Codex };
enum class TestStatus { Queued, Connecting, WaitingFirstToken, Streaming, Passed, Failed, Timeout, Cancelled, Error };
enum class ProxyMode { None, System, Http, Socks5 };

QString protocolName(Protocol p);
Protocol protocolFromString(const QString& s);
QString statusName(TestStatus s);
TestStatus statusFromString(const QString& s);

struct ProxyConfig {
    ProxyMode mode = ProxyMode::System;
    QString host;
    quint16 port = 0;
    QString username;
    QString password;
};

struct Profile {
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString name = "Default";
    QUrl baseUrl;
    QString apiKey;
    Protocol protocol = Protocol::OpenAI;
    QString defaultModel;
    QJsonObject customHeaders;
    ProxyConfig proxy;
    int timeoutSeconds = 15;
    bool context1M = false;
};

struct ModelInfo {
    QString id;
    QString displayName;
    QString ownedBy;
    qint64 contextLength = -1;
    qint64 maxOutputTokens = -1;
    bool streaming = true;
    QString contextSource;
};

struct Usage {
    qint64 promptTokens = -1;
    qint64 completionTokens = -1;
    qint64 totalTokens = -1;
    bool exact = false;
    QString source;
};

struct RequestConfig {
    QString model;
    QString prompt = QString::fromUtf8("请只回复 OK 两个字母，不要输出任何其他内容。");
    int maxTokens = 32;
    double temperature = 0.0;
    bool stream = true;
    bool includeUsage = true;
    bool codexHeaders = true;
    QString matchRegex;
    bool caseSensitive = true;
};

struct ErrorInfo {
    int httpStatus = 0;
    QString reason;
    QString type;
    QString code;
    QString message;
    QString rawSummary;
    bool html = false;
    bool timeout = false;
    bool network = false;
};

struct LinkTiming {
    double dnsMs = -1;
    double tcpMs = -1;
    double tlsMs = -1;
    double requestMs = -1;
    double firstByteMs = -1;
    double firstTextMs = -1;
    double generationMs = -1;
    double totalMs = -1;
};

struct Metrics {
    LinkTiming timing;
    double ttftMs = -1;
    double firstByteMs = -1;
    double generationMs = -1;
    double totalLatencyMs = -1;
    double tokensPerSecond = -1;
    qint64 responseBytes = 0;
    Usage usage;
};

struct TestResult {
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QDateTime timestamp = QDateTime::currentDateTime();
    QString profileName;
    QString model;
    Protocol protocol = Protocol::OpenAI;
    TestStatus status = TestStatus::Error;
    QString output;
    QString endpoint;
    QString rawResponse;
    ErrorInfo error;
    Metrics metrics;
    bool passed = false;
    bool estimated = false;
    QString note;
};

struct SessionStats {
    int total = 0;
    int passed = 0;
    int failed = 0;
    int timeout = 0;
    double passRate = 0;
    double avgTtft = 0;
    double avgLatency = 0;
    double avgSpeed = 0;
    qint64 totalTokens = 0;
};

QJsonObject toJson(const Profile& p);
Profile profileFromJson(const QJsonObject& o);
QJsonObject toJson(const TestResult& r, bool includeRaw = true);
TestResult testResultFromJson(const QJsonObject& o);
QString formatContext(qint64 tokens);
qint64 parseContext(const QJsonValue& v);
QString sanitizeOutput(QString text);
qint64 estimateTokens(const QString& text);

} // namespace airb
