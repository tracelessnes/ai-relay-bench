#include "BenchmarkJob.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkProxy>
#include <QRegularExpression>

#include "security/RedactionService.h"

namespace airb {
namespace {
void configureProxy(QNetworkAccessManager& manager, const Profile& profile) {
    if (profile.proxy.mode == ProxyMode::None) {
        manager.setProxy(QNetworkProxy::NoProxy);
    } else if (profile.proxy.mode == ProxyMode::System) {
        manager.setProxy(QNetworkProxy::DefaultProxy);
    } else {
        manager.setProxy(QNetworkProxy(profile.proxy.mode == ProxyMode::Http
                                          ? QNetworkProxy::HttpProxy
                                          : QNetworkProxy::Socks5Proxy,
                                      profile.proxy.host, profile.proxy.port,
                                      profile.proxy.username, profile.proxy.password));
    }
}

bool looksHtml(const QByteArray& body) {
    const auto lower = body.left(1024).trimmed().toLower();
    return lower.startsWith("<!doctype html") || lower.startsWith("<html")
        || lower.contains("<title>cloudflare") || lower.contains("cf-ray");
}
} // namespace

BenchmarkJob::BenchmarkJob(Profile profile, RequestConfig config, QObject* parent)
    : QObject(parent), profile_(std::move(profile)), config_(std::move(config)),
      id_(QUuid::createUuid().toString(QUuid::WithoutBraces)),
      adapter_(ProtocolAdapter::create(profile_.protocol)) {
    timeoutTimer_.setSingleShot(true);
    connect(&timeoutTimer_, &QTimer::timeout, this, &BenchmarkJob::onTimeout);
    configureProxy(manager_, profile_);
}

void BenchmarkJob::start() {
    attempts_ = adapter_->completionAttempts(profile_, config_);
    overallClock_.start();
    if (attempts_.isEmpty()) {
        auto result = makeResult();
        result.error.message = QStringLiteral("\u672a\u751f\u6210\u53ef\u7528\u7684\u8bf7\u6c42\u65b9\u6848");
        complete(result);
        return;
    }
    emit started(id_, attempts_.first().label);
    QTimer::singleShot(0, this, &BenchmarkJob::sendAttempt);
}

void BenchmarkJob::cancel() {
    if (finished_) return;
    if (reply_) {
        disconnect(reply_, nullptr, this, nullptr);
        reply_->abort();
        reply_->deleteLater();
        reply_ = nullptr;
    }
    timeoutTimer_.stop();
    appendAttemptRaw();
    complete(makeResult(TestStatus::Cancelled));
}

void BenchmarkJob::sendAttempt() {
    if (finished_) return;
    if (attemptIndex_ >= attempts_.size()) {
        auto result = makeResult();
        result.error.message = QStringLiteral("\u6ca1\u6709\u6210\u529f\u7684\u517c\u5bb9\u63a5\u53e3\u6216\u8bf7\u6c42\u65b9\u6848");
        complete(result);
        return;
    }
    beginAttempt();
}

void BenchmarkJob::beginAttempt() {
    const auto& attempt = attempts_[attemptIndex_];
    emit progress(id_, QStringLiteral("\u8fde\u63a5\u4e2d \u00b7 %1 \u00b7 %2")
                          .arg(attempt.label, attempt.url.toString()));

    QNetworkRequest request(attempt.url);
    for (const auto& header : attempt.headers) request.setRawHeader(header.first, header.second);
    request.setTransferTimeout(qMax(1000, profile_.timeoutSeconds * 1000));

    reply_ = manager_.post(request, attempt.body);
    attemptRaw_.clear();
    output_.clear();
    decoder_.reset();
    parserState_ = {};
    firstByteNs_ = -1;
    firstTextNs_ = -1;
    finishedAttemptNs_ = -1;
    streamCompleted_ = false;
    seenEventSignatures_.clear();
    attemptClock_.restart();
    timeoutTimer_.start(qMax(1000, profile_.timeoutSeconds * 1000));

    connect(reply_, &QNetworkReply::readyRead, this, &BenchmarkJob::onReadyRead);
    connect(reply_, &QNetworkReply::finished, this, &BenchmarkJob::onFinished);
}

void BenchmarkJob::onReadyRead() {
    if (finished_ || !reply_) return;
    const auto bytes = reply_->readAll();
    if (bytes.isEmpty()) return;
    if (firstByteNs_ < 0) firstByteNs_ = attemptClock_.nsecsElapsed();
    attemptRaw_ += bytes;
    transferredBytes_ += bytes.size();
    if (attempts_.value(attemptIndex_).streaming && !looksHtml(attemptRaw_)) {
        if (!handleEvents(decoder_.feed(bytes))) return;
    }
}

bool BenchmarkJob::handleEvents(const QList<SseEvent>& events) {
    for (const auto& event : events) {
        // SSE id is the only transport-level identity we can trust for replay deduplication.
        // A repeated payload without an SSE id is valid model output and must be preserved.
        if (!event.id.isEmpty()) {
            const QByteArray signature = event.event + '\n' + event.id + '\n' + event.data;
            if (seenEventSignatures_.contains(signature)) continue;
            seenEventSignatures_.insert(signature);
        }

        const auto parsed = adapter_->parseSse(event, parserState_);
        if (!parsed.error.isEmpty()) {
            if (parsed.recoverable) {
                warnings_.push_back(parsed.error);
                continue;
            }
            auto result = makeResult(TestStatus::Error);
            result.error.message = parsed.error;
            result.error.rawSummary = QString::fromUtf8(attemptRaw_.left(4096));
            if (reply_) {
                disconnect(reply_, nullptr, this, nullptr);
                reply_->abort();
                reply_->deleteLater();
                reply_ = nullptr;
            }
            timeoutTimer_.stop();
            appendAttemptRaw();
            complete(result);
            return false;
        }

        if (!parsed.delta.isEmpty()) {
            if (firstTextNs_ < 0) {
                firstTextNs_ = attemptClock_.nsecsElapsed();
                emit progress(id_, QStringLiteral("\u6d41\u5f0f\u751f\u6210"));
            }
            output_ += parsed.delta;
            emit delta(id_, parsed.delta);
        }
        if (parsed.usageChanged) parserState_.usage = parsed.usage;
        if (parsed.completed) streamCompleted_ = true;
    }
    return true;
}

void BenchmarkJob::appendAttemptRaw() {
    if (attemptRaw_.isEmpty()) return;
    const auto& attempt = attempts_.value(attemptIndex_);
    combinedRaw_ += "===== Attempt " + QByteArray::number(attemptIndex_ + 1)
        + " / " + attempt.label.toUtf8() + " / " + attempt.url.toString().toUtf8()
        + " =====\n" + attemptRaw_ + "\n\n";
    attemptRaw_.clear();
}

int BenchmarkJob::nextRetryIndex(int status, bool networkError) const {
    int next = attemptIndex_ + 1;
    if (status == 404 || status == 405 || networkError) {
        const auto failedUrl = attempts_.value(attemptIndex_).url;
        while (next < attempts_.size() && attempts_[next].url == failedUrl) ++next;
    }
    return next;
}

void BenchmarkJob::onFinished() {
    if (finished_ || !reply_) return;
    timeoutTimer_.stop();
    const auto tail = reply_->readAll();
    attemptRaw_ += tail;
    transferredBytes_ += tail.size();
    finishedAttemptNs_ = attemptClock_.nsecsElapsed();

    const int status = reply_->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const auto networkError = reply_->error();
    const bool transportFailed = status == 0 && networkError != QNetworkReply::NoError;
    const QString reason = reply_->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
    const auto contentType = reply_->header(QNetworkRequest::ContentTypeHeader).toString().toLower();
    const auto body = attemptRaw_;
    const auto attempt = attempts_.value(attemptIndex_);
    reply_->deleteLater();
    reply_ = nullptr;

    if (transportFailed || status >= 400 || status == 0) {
        const bool retry = adapter_->shouldRetry(attempt, status, body, transportFailed);
        const int next = nextRetryIndex(status, transportFailed);
        if (retry && next < attempts_.size()) {
            warnings_ << QStringLiteral("\u7b2c %1 \u6b21\u5c1d\u8bd5\u5931\u8d25\uff08%2%3\uff09\uff0c\u56de\u9000\u5230 %4")
                             .arg(attemptIndex_ + 1)
                             .arg(status)
                             .arg(transportFailed ? QStringLiteral("\uff0c\u7f51\u7edc\u9519\u8bef") : QString())
                             .arg(attempts_[next].label);
            appendAttemptRaw();
            attemptIndex_ = next;
            QTimer::singleShot(0, this, &BenchmarkJob::sendAttempt);
            return;
        }
        auto result = makeResult(networkError == QNetworkReply::TimeoutError
                                     ? TestStatus::Timeout : TestStatus::Error);
        result.error = parseError(status, body, reason, transportFailed);
        appendAttemptRaw();
        complete(result);
        return;
    }

    if (looksHtml(body) || contentType.contains(QStringLiteral("text/html"))) {
        auto result = makeResult(TestStatus::Error);
        result.error = parseError(status, body, reason, false);
        result.error.html = true;
        result.error.message = QStringLiteral("\u63a5\u53e3\u8fd4\u56de\u4e86 HTML \u9875\u9762\uff0c\u800c\u4e0d\u662f API \u54cd\u5e94");
        appendAttemptRaw();
        complete(result);
        return;
    }

    if (attempt.streaming) {
        if (!handleEvents(decoder_.finish())) return;
        bool parsedNonStream = false;
        if (output_.isEmpty() && !body.trimmed().isEmpty()) {
            const auto parsed = adapter_->parseNonStream(body, parserState_);
            if (!parsed.error.isEmpty()) {
                auto result = makeResult(TestStatus::Error);
                result.error.message = parsed.error;
                result.error.rawSummary = QString::fromUtf8(body.left(4096));
                appendAttemptRaw();
                complete(result);
                return;
            }
            if (!parsed.delta.isEmpty()) {
                if (firstTextNs_ < 0) firstTextNs_ = attemptClock_.nsecsElapsed();
                output_ += parsed.delta;
                emit delta(id_, parsed.delta);
                parsedNonStream = true;
            }
            if (parsed.usageChanged) parserState_.usage = parsed.usage;
            if (parsedNonStream) warnings_ << QStringLiteral("\u670d\u52a1\u5668\u8fd4\u56de\u666e\u901a JSON/\u975e SSE \u5185\u5bb9\uff0c\u5df2\u6309\u975e\u6d41\u5f0f\u54cd\u5e94\u89e3\u6790");
        }
        if (!parsedNonStream && !streamCompleted_) {
            auto result = makeResult(TestStatus::Error);
            result.error.network = true;
            result.error.message = QStringLiteral("\u6d41\u5f0f\u54cd\u5e94\u5728\u5b8c\u6210\u4e8b\u4ef6\u524d\u7ed3\u675f\uff0c\u54cd\u5e94\u53ef\u80fd\u5df2\u622a\u65ad");
            result.error.rawSummary = QString::fromUtf8(body.left(4096));
            appendAttemptRaw();
            complete(result);
            return;
        }
    } else {
        const auto parsed = adapter_->parseNonStream(body, parserState_);
        if (!parsed.error.isEmpty()) {
            auto result = makeResult(TestStatus::Error);
            result.error.message = parsed.error;
            result.error.rawSummary = QString::fromUtf8(body.left(4096));
            appendAttemptRaw();
            complete(result);
            return;
        }
        if (!parsed.delta.isEmpty()) {
            if (firstTextNs_ < 0) firstTextNs_ = attemptClock_.nsecsElapsed();
            output_ += parsed.delta;
            emit delta(id_, parsed.delta);
        }
        if (parsed.usageChanged) parserState_.usage = parsed.usage;
        warnings_ << QStringLiteral("\u670d\u52a1\u5668\u4e0d\u652f\u6301\u6d41\u5f0f\uff0c\u5df2\u56de\u9000\u4e3a\u975e\u6d41\u5f0f\u8bf7\u6c42");
    }

    if (parserState_.usage.promptTokens < 0) parserState_.usage.promptTokens = estimateTokens(config_.prompt);
    if (parserState_.usage.completionTokens < 0) parserState_.usage.completionTokens = estimateTokens(output_);
    if (parserState_.usage.totalTokens < 0) parserState_.usage.totalTokens = parserState_.usage.promptTokens + parserState_.usage.completionTokens;
    if (!parserState_.usage.exact) parserState_.usage.source = QStringLiteral("estimated");

    auto result = makeResult(matches(output_) ? TestStatus::Passed : TestStatus::Failed);
    result.passed = matches(output_);
    result.metrics.usage = parserState_.usage;
    result.estimated = !parserState_.usage.exact;
    appendAttemptRaw();
    complete(result);
}

void BenchmarkJob::onTimeout() {
    if (finished_) return;
    timeoutTimer_.stop();
    if (reply_) {
        const auto tail = reply_->readAll();
        attemptRaw_ += tail;
        transferredBytes_ += tail.size();
        disconnect(reply_, nullptr, this, nullptr);
        reply_->abort();
        reply_->deleteLater();
        reply_ = nullptr;
    }
    finishedAttemptNs_ = attemptClock_.isValid() ? attemptClock_.nsecsElapsed() : -1;
    auto result = makeResult(TestStatus::Timeout);
    result.error.timeout = true;
    result.error.network = true;
    result.error.message = QStringLiteral("\u8bf7\u6c42\u5728 %1 \u79d2\u540e\u8d85\u65f6").arg(profile_.timeoutSeconds);
    appendAttemptRaw();
    complete(result);
}

void BenchmarkJob::complete(TestResult result) {
    if (finished_) return;
    finished_ = true;
    timeoutTimer_.stop();
    if (!warnings_.isEmpty()) result.note = warnings_.join(QStringLiteral(" \u00b7 "));
    result.rawResponse = RedactionService::text(QString::fromUtf8(combinedRaw_.left(4 * 1024 * 1024)));
    if (!attemptRaw_.isEmpty()) {
        QByteArray raw = combinedRaw_;
        const auto& attempt = attempts_.value(attemptIndex_);
        raw += "===== Attempt " + QByteArray::number(attemptIndex_ + 1)
            + " / " + attempt.label.toUtf8() + " / " + attempt.url.toString().toUtf8()
            + " =====\n" + attemptRaw_;
        result.rawResponse = RedactionService::text(QString::fromUtf8(raw.left(4 * 1024 * 1024)));
    }
    emit finished(result);
    deleteLater();
}

ErrorInfo BenchmarkJob::parseError(int status, const QByteArray& body, const QString& reason, bool network) const {
    ErrorInfo error;
    error.httpStatus = status;
    error.reason = reason;
    error.network = network;
    error.rawSummary = RedactionService::text(QString::fromUtf8(body.left(4096))).trimmed();
    error.html = looksHtml(body);

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error == QJsonParseError::NoError && document.isObject()) {
        const auto object = document.object();
        const auto value = object.value(QStringLiteral("error"));
        if (value.isObject()) {
            const auto detail = value.toObject();
            error.message = detail.value(QStringLiteral("message")).toString();
            error.type = detail.value(QStringLiteral("type")).toString();
            error.code = detail.value(QStringLiteral("code")).toVariant().toString();
        } else if (value.isString()) {
            error.message = value.toString();
        }
        if (error.message.isEmpty()) error.message = object.value(QStringLiteral("message")).toString(object.value(QStringLiteral("detail")).toString());
    }
    if (error.message.isEmpty()) {
        error.message = error.html
            ? QStringLiteral("HTML/WAF \u62e6\u622a\u9875\uff1a") + error.rawSummary.left(512)
            : (network ? QStringLiteral("\u7f51\u7edc\u9519\u8bef")
                       : (error.rawSummary.isEmpty() ? QStringLiteral("HTTP %1").arg(status) : error.rawSummary.left(512)));
    }
    return error;
}

TestResult BenchmarkJob::makeResult(TestStatus status) const {
    TestResult result;
    result.profileName = profile_.name;
    result.model = config_.model;
    result.protocol = profile_.protocol;
    result.status = status;
    result.output = output_;
    result.endpoint = attempts_.value(attemptIndex_).url.toString();
    result.metrics.responseBytes = transferredBytes_;
    result.metrics.usage = parserState_.usage;

    const qint64 overallEnd = overallClock_.isValid() ? overallClock_.nsecsElapsed() : 0;
    if (firstByteNs_ >= 0) result.metrics.firstByteMs = firstByteNs_ / 1e6;
    if (firstTextNs_ >= 0) {
        result.metrics.ttftMs = firstTextNs_ / 1e6;
        const qint64 end = finishedAttemptNs_ >= 0 ? finishedAttemptNs_ : attemptClock_.nsecsElapsed();
        result.metrics.generationMs = qMax<qint64>(0, end - firstTextNs_) / 1e6;
    }
    result.metrics.totalLatencyMs = overallEnd / 1e6;
    result.metrics.timing.firstByteMs = result.metrics.firstByteMs;
    result.metrics.timing.firstTextMs = result.metrics.ttftMs;
    result.metrics.timing.generationMs = result.metrics.generationMs;
    result.metrics.timing.totalMs = result.metrics.totalLatencyMs;
    result.metrics.timing.requestMs = finishedAttemptNs_ >= 0 ? finishedAttemptNs_ / 1e6 : -1;

    const auto completion = parserState_.usage.completionTokens >= 0
        ? parserState_.usage.completionTokens : estimateTokens(output_);
    if (result.metrics.generationMs > 0 && completion > 0)
        result.metrics.tokensPerSecond = completion / (result.metrics.generationMs / 1000.0);
    return result;
}

bool BenchmarkJob::matches(const QString& text) const {
    const QString sanitized = sanitizeOutput(text);
    if (config_.matchRegex.isEmpty()) return sanitized == QStringLiteral("OK");
    const auto options = config_.caseSensitive ? QRegularExpression::NoPatternOption : QRegularExpression::CaseInsensitiveOption;
    const QRegularExpression expression(config_.matchRegex, options);
    if (!expression.isValid()) return false;
    const auto match = expression.match(sanitized);
    return match.hasMatch() && match.capturedStart() == 0 && match.capturedLength() == sanitized.size();
}

} // namespace airb
