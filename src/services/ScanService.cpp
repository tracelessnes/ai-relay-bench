#include "ScanService.h"
#include "protocol/ProtocolAdapter.h"
#include "security/RedactionService.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkProxy>
#include <QRegularExpression>

namespace airb {
namespace {
QUrl makeEndpoint(const QUrl& base, const QString& suffix) {
    QUrl url = base;
    QString path = url.path();
    while (path.endsWith('/')) path.chop(1);
    QString normalized = suffix;
    while (normalized.startsWith('/')) normalized.remove(0, 1);
    if (path.endsWith(QStringLiteral("/v1"), Qt::CaseInsensitive)
        && normalized.startsWith(QStringLiteral("v1/"), Qt::CaseInsensitive)) {
        normalized = normalized.mid(3);
    }
    url.setPath(path + QStringLiteral("/") + normalized);
    url.setQuery(QString());
    url.setFragment({});
    return url;
}
}

QList<EndpointCandidate> endpointCandidates(const QUrl& base, const QString& suffix) {
    QList<EndpointCandidate> result;
    const auto versioned = makeEndpoint(base, QStringLiteral("v1/") + suffix);
    QUrl plainBase = base;
    QString basePath = plainBase.path();
    while (basePath.endsWith('/')) basePath.chop(1);
    if (basePath.endsWith(QStringLiteral("/v1"), Qt::CaseInsensitive)) {
        basePath.chop(3);
        if (basePath.isEmpty()) basePath = QStringLiteral("/");
        plainBase.setPath(basePath);
    }
    const auto plain = makeEndpoint(plainBase, suffix);
    result.append({versioned, QStringLiteral("/v1/") + suffix});
    if (plain != versioned) result.append({plain, QStringLiteral("/") + suffix});
    return result;
}

QJsonObject toJson(const ScanCheck& check) {
    return QJsonObject{{"id", check.id}, {"label", check.label}, {"status", check.status},
                       {"detail", RedactionService::text(check.detail)},
                       {"httpStatus", check.httpStatus}, {"passed", check.passed},
                       {"category", check.category}};
}

ScanCheck scanCheckFromJson(const QJsonObject& object) {
    ScanCheck check;
    check.id = object.value(QStringLiteral("id")).toString();
    check.label = object.value(QStringLiteral("label")).toString();
    check.status = object.value(QStringLiteral("status")).toString();
    check.detail = object.value(QStringLiteral("detail")).toString();
    check.httpStatus = object.value(QStringLiteral("httpStatus")).toInt();
    check.passed = object.value(QStringLiteral("passed")).toBool();
    check.category = object.value(QStringLiteral("category")).toString();
    return check;
}

QJsonObject toJson(const ScanResult& result) {
    QJsonArray checks;
    for (const auto& check : result.checks) checks.append(toJson(check));
    QJsonArray models;
    for (const auto& model : result.discoveredModels) models.append(model);
    return QJsonObject{{"id", result.id}, {"timestamp", result.timestamp.toString(Qt::ISODateWithMs)},
                       {"profileName", result.profileName}, {"model", result.model},
                       {"protocol", protocolName(result.protocol)}, {"checks", checks},
                       {"diagnostic", RedactionService::text(result.diagnostic)},
                       {"discoveredModels", models}, {"maxContextLength", result.maxContextLength},
                       {"modelsSupported", result.modelsSupported}, {"streamSupported", result.streamSupported},
                       {"usageSupported", result.usageSupported},
                       {"streamOptionsSupported", result.streamOptionsSupported},
                       {"htmlIntercepted", result.htmlIntercepted}, {"score", result.score()}};
}

ScanResult scanResultFromJson(const QJsonObject& object) {
    ScanResult result;
    result.id = object.value(QStringLiteral("id")).toString(result.id);
    const auto timestamp = QDateTime::fromString(object.value(QStringLiteral("timestamp")).toString(), Qt::ISODateWithMs);
    if (timestamp.isValid()) result.timestamp = timestamp;
    result.profileName = object.value(QStringLiteral("profileName")).toString();
    result.model = object.value(QStringLiteral("model")).toString();
    result.protocol = protocolFromString(object.value(QStringLiteral("protocol")).toString());
    result.diagnostic = object.value(QStringLiteral("diagnostic")).toString();
    result.maxContextLength = object.value(QStringLiteral("maxContextLength")).toInteger(-1);
    result.modelsSupported = object.value(QStringLiteral("modelsSupported")).toBool();
    result.streamSupported = object.value(QStringLiteral("streamSupported")).toBool();
    result.usageSupported = object.value(QStringLiteral("usageSupported")).toBool();
    result.streamOptionsSupported = object.value(QStringLiteral("streamOptionsSupported")).toBool();
    result.htmlIntercepted = object.value(QStringLiteral("htmlIntercepted")).toBool();
    for (const auto& value : object.value(QStringLiteral("discoveredModels")).toArray())
        if (value.isString()) result.discoveredModels.append(value.toString());
    for (const auto& value : object.value(QStringLiteral("checks")).toArray())
        if (value.isObject()) result.checks.append(scanCheckFromJson(value.toObject()));
    return result;
}

int ScanResult::score() const {
    if (checks.isEmpty()) return 0;
    int passed = 0;
    int counted = 0;
    for (const auto& check : checks) {
        if (check.status == QStringLiteral("skipped")) continue;
        ++counted;
        if (check.passed) ++passed;
    }
    return counted == 0 ? 0 : qRound(100.0 * passed / counted);
}

ScanService::ScanService(QObject* parent) : QObject(parent) {
    timeout_.setSingleShot(true);
    connect(&timeout_, &QTimer::timeout, this, &ScanService::onTimeout);
}

void ScanService::scan(const Profile& profile, const QString& model) {
    cancel();
    profile_ = profile;
    model_ = model.trimmed();
    result_ = {};
    result_.profileName = profile.name;
    result_.model = model_;
    result_.protocol = profile.protocol;
    plans_.clear();
    planIndex_ = -1;
    buildPlans();
    running_ = !plans_.isEmpty();
    emit progress(running_ ? QStringLiteral("正在扫描 %1").arg(profile.name)
                           : QStringLiteral("扫描未生成检查项"));
    if (running_) sendNext();
}

void ScanService::cancel() {
    ++generation_;
    timeout_.stop();
    if (reply_) {
        disconnect(reply_, nullptr, this, nullptr);
        reply_->abort();
        reply_->deleteLater();
        reply_ = nullptr;
    }
    running_ = false;
    plans_.clear();
    planIndex_ = -1;
    body_.clear();
}

void ScanService::configureProxy(QNetworkAccessManager& manager, const Profile& profile) {
    if (profile.proxy.mode == ProxyMode::None) manager.setProxy(QNetworkProxy::NoProxy);
    else if (profile.proxy.mode == ProxyMode::System) manager.setProxy(QNetworkProxy::DefaultProxy);
    else manager.setProxy(QNetworkProxy(profile.proxy.mode == ProxyMode::Http ? QNetworkProxy::HttpProxy : QNetworkProxy::Socks5Proxy,
                                        profile.proxy.host, profile.proxy.port, profile.proxy.username, profile.proxy.password));
}

void ScanService::buildPlans() {
    configureProxy(manager_, profile_);
    const auto adapter = ProtocolAdapter::create(profile_.protocol);
    int modelIndex = 0;
    for (const auto& url : adapter->modelUrls(profile_)) {
        Plan plan;
        plan.id = QStringLiteral("models-%1").arg(modelIndex++);
        plan.label = QStringLiteral("模型接口 %1").arg(url.path());
        plan.request = QNetworkRequest(url);
        plan.request.setRawHeader("Accept", "application/json");
        plan.request.setTransferTimeout(qMax(1000, profile_.timeoutSeconds * 1000));
        for (const auto& header : adapter->modelHeaders(profile_)) plan.request.setRawHeader(header.first, header.second);
        plans_.append(plan);
    }
    if (model_.isEmpty()) return;
    RequestConfig config;
    config.model = model_;
    config.prompt = QStringLiteral("请只回复 OK");
    config.maxTokens = 8;
    const auto attempts = adapter->completionAttempts(profile_, config);
    if (attempts.isEmpty()) return;
    const auto& attempt = attempts.first();
    Plan plan;
    plan.id = QStringLiteral("protocol");
    plan.label = QStringLiteral("%1 流式接口").arg(protocolName(profile_.protocol));
    plan.request = QNetworkRequest(attempt.url);
    for (const auto& header : attempt.headers) plan.request.setRawHeader(header.first, header.second);
    plan.body = attempt.body;
    plan.stream = attempt.streaming;
    plan.streamOptions = plan.body.contains("stream_options");
    plan.request.setTransferTimeout(qMax(1000, profile_.timeoutSeconds * 1000));
    plans_.append(plan);
}

void ScanService::sendNext() {
    if (!running_) return;
    ++planIndex_;
    if (planIndex_ >= plans_.size()) {
        running_ = false;
        emit progress(QStringLiteral("扫描完成，评分 %1/100").arg(result_.score()));
        emit finished(result_);
        return;
    }
    const auto& plan = plans_.at(planIndex_);
    emit progress(plan.label);
    body_.clear();
    reply_ = plan.body.isEmpty() ? manager_.get(plan.request) : manager_.post(plan.request, plan.body);
    connect(reply_, &QNetworkReply::readyRead, this, &ScanService::onReadyRead);
    connect(reply_, &QNetworkReply::finished, this, &ScanService::onFinished);
    timeout_.start(qMax(1000, profile_.timeoutSeconds * 1000));
}

void ScanService::onReadyRead() {
    if (!reply_) return;
    body_ += reply_->readAll();
    if (body_.size() > 8 * 1024 * 1024) {
        body_.truncate(8 * 1024 * 1024);
        reply_->abort();
    }
}

void ScanService::onFinished() {
    if (!reply_) return;
    timeout_.stop();
    body_ += reply_->readAll();
    const int status = reply_->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const auto reason = reply_->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
    const auto transportError = reply_->error() == QNetworkReply::NoError ? QString() : reply_->errorString();
    const auto check = classifyCurrent(status, body_, reason, transportError);
    reply_->deleteLater();
    reply_ = nullptr;
    finishCurrent(check);
}

void ScanService::finishCurrent(const ScanCheck& check) {
    if (!running_) return;
    result_.checks.append(check);
    emit checkReady(check);
    const quint64 generation = generation_;
    QTimer::singleShot(0, this, [this, generation] {
        if (running_ && generation_ == generation) sendNext();
    });
}

ScanCheck ScanService::classifyCurrent(int status, const QByteArray& body, const QString& reason,
                                       const QString& transportError) {
    const auto& plan = plans_.at(planIndex_);
    ScanCheck check;
    check.id = plan.id;
    check.label = plan.label;
    check.httpStatus = status;
    check.category = plan.id.startsWith(QStringLiteral("models")) ? QStringLiteral("models") : QStringLiteral("protocol");
    if (status >= 200 && status < 300 && !isHtml(body)) {
        check.status = QStringLiteral("pass");
        check.passed = true;
        if (check.category == QStringLiteral("models")) {
            QString error;
            const auto adapter = ProtocolAdapter::create(profile_.protocol);
            const auto models = adapter->parseModels(body, &error);
            check.detail = models.isEmpty() ? QStringLiteral("模型解析失败：%1").arg(error)
                                             : QStringLiteral("发现 %1 个模型").arg(models.size());
            if (models.isEmpty()) {
                check.status = QStringLiteral("warn");
                check.passed = false;
            } else {
                result_.modelsSupported = true;
                for (const auto& model : models) {
                    if (!result_.discoveredModels.contains(model.id)) result_.discoveredModels.append(model.id);
                    result_.maxContextLength = qMax(result_.maxContextLength, model.contextLength);
                }
            }
        } else {
            result_.streamSupported = plan.stream && (body.contains("data:") || body.contains("output_text") || body.contains("message_start"));
            result_.usageSupported = body.contains("usage") || body.contains("input_tokens") || body.contains("output_tokens");
            result_.streamOptionsSupported = plan.streamOptions;
            check.detail = result_.streamSupported
                ? QStringLiteral("\u6d41\u5f0f\u54cd\u5e94\u53ef\u7528")
                : QStringLiteral("HTTP %1\uff0c\u4f46\u672a\u68c0\u6d4b\u5230 SSE \u6d41\u5f0f\u4e8b\u4ef6").arg(status);
            if (!result_.streamSupported) check.status = QStringLiteral("warn");
        }
    } else if (isHtml(body)) {
        check.status = QStringLiteral("fail");
        check.category = QStringLiteral("html-waf");
        check.detail = QStringLiteral("HTTP %1 %2\uff1a\u68c0\u6d4b\u5230 HTML/WAF \u62e6\u622a\u9875\uff1a%3").arg(status).arg(reason, bodySummary(body));
        check.passed = false;
        result_.htmlIntercepted = true;
    } else if (status == 0) {
        check.status = QStringLiteral("fail");
        check.category = QStringLiteral("network");
        check.detail = QStringLiteral("\u7f51\u7edc\u9519\u8bef\uff1a%1").arg(transportError.isEmpty() ? bodySummary(body) : transportError);
    } else {
        check.status = status == 404 || status == 405 ? QStringLiteral("warn") : QStringLiteral("fail");
        check.category = status == 401 || status == 403 ? QStringLiteral("auth")
                         : status == 429 ? QStringLiteral("rate-limit")
                         : status >= 500 ? QStringLiteral("upstream") : QStringLiteral("http");
        check.passed = false;
        const auto structured = structuredError(body);
        check.detail = QStringLiteral("HTTP %1 %2\uff1a%3").arg(status).arg(reason, structured.isEmpty() ? bodySummary(body) : structured);
    }
    return check;
}

void ScanService::onTimeout() {
    if (!reply_ || !running_) return;
    reply_->abort();
    const auto& plan = plans_.at(planIndex_);
    const ScanCheck check{plan.id, plan.label, QStringLiteral("fail"), QStringLiteral("请求超时"), 0, false, QStringLiteral("timeout")};
    reply_->deleteLater();
    reply_ = nullptr;
    finishCurrent(check);
}

QString ScanService::bodySummary(const QByteArray& body) {
    const auto trimmed = body.trimmed();
    if (trimmed.isEmpty()) return QStringLiteral("空响应");
    const auto summary = QString::fromUtf8(trimmed.left(512));
    return trimmed.size() > 512 ? summary + QStringLiteral("…") : summary;
}

QString ScanService::structuredError(const QByteArray& body) {
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(body, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return {};
    const auto root = document.object();
    const auto value = root.value(QStringLiteral("error"));
    if (value.isObject()) {
        const auto object = value.toObject();
        const auto type = object.value(QStringLiteral("type")).toString();
        const auto code = object.value(QStringLiteral("code")).toVariant().toString();
        const auto message = object.value(QStringLiteral("message")).toString();
        return QStringLiteral("%1%2%3").arg(type.isEmpty() ? QString() : type + QStringLiteral(" / "))
            .arg(code.isEmpty() ? QString() : code + QStringLiteral(" / "))
            .arg(message.isEmpty() ? bodySummary(body) : message);
    }
    return root.value(QStringLiteral("message")).toString(root.value(QStringLiteral("detail")).toString());
}

bool ScanService::isHtml(const QByteArray& body) {
    const auto lower = body.left(2048).trimmed().toLower();
    return lower.startsWith("<!doctype html") || lower.startsWith("<html")
        || lower.contains("<title>") && (lower.contains("cloudflare") || lower.contains("access denied") || lower.contains("forbidden"))
        || lower.contains("cf-ray") || lower.contains("attention required");
}

} // namespace airb
