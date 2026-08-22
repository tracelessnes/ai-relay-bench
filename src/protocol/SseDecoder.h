#pragma once
#include <QByteArray>
#include <QList>
#include <QString>

namespace airb {
struct SseEvent { QByteArray event; QByteArray data; QByteArray id; };

class SseDecoder {
public:
    QList<SseEvent> feed(const QByteArray& bytes);
    QList<SseEvent> finish();
    void reset();
    qint64 totalBytes() const { return totalBytes_; }
    QString lastWarning() const { return warning_; }
private:
    QList<SseEvent> consume(bool final);
    void processLine(QByteArray line, QList<SseEvent>& out);
    void dispatch(QList<SseEvent>& out);
    QByteArray buffer_, event_, data_, id_;
    qint64 totalBytes_ = 0;
    QString warning_;
    static constexpr qsizetype MaxBuffer = 8 * 1024 * 1024;
};
}
