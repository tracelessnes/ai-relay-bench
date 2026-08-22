#include "ClientFingerprint.h"
#include "protocol/SseDecoder.h"
namespace airb {
static QByteArray low(QByteArray b) { return b.toLower(); }
CaptureClient identifyCaptureClient(const CaptureHeaders& h, const QByteArray& target, const QByteArray& body) {
    const auto ua = low(headerValue(h, "user-agent"));
    const auto t = low(target); const auto b = low(body);
    if (hasHeader(h, "originator") || hasHeader(h, "x-codex-beta-features") || hasHeader(h, "x-openai-internal-codex-responses-lite") || ua.contains("codex_exec") || ua.contains("codex_cli")) return CaptureClient::Codex;
    if (ua.contains("claude-cli") || ua.contains("claude code")) return CaptureClient::ClaudeCode;
    if (ua.contains("cherry")) return CaptureClient::CherryStudio;
    if (ua.contains("chatbox")) return CaptureClient::ChatBox;
    if (hasHeader(h, "anthropic-version") || hasHeader(h, "x-api-key") || t.contains("/messages") || b.contains("anthropic")) return CaptureClient::Claude;
    if (t.contains("/chat/completions") || t.contains("/responses") || hasHeader(h, "authorization")) return CaptureClient::OpenAI;
    return CaptureClient::Unknown;
}
QList<SseCaptureEvent> parseCapturedSse(const CaptureHeaders& h, const QByteArray& body) {
    if (!low(headerValue(h, "content-type")).contains("text/event-stream") && !body.contains("data:") && !body.contains("event:")) return {};
    SseDecoder decoder; QList<SseCaptureEvent> out;
    auto append = [&out](const QList<SseEvent>& events) { for (const auto& e : events) out.append({e.event, e.data, e.id}); };
    append(decoder.feed(body)); append(decoder.finish()); return out;
}
QString summarizeCaptureBody(const QByteArray& body, qsizetype maxChars) { auto text = QString::fromUtf8(body); if (text.size() > maxChars) text = text.left(maxChars) + QStringLiteral("…"); return text; }
}
