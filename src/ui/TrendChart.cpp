#include "TrendChart.h"
#include "i18n/LanguageManager.h"
#include "ui/ThemeManager.h"
#include <QPainter>
#include <QPainterPath>
#include <algorithm>

namespace airb {
TrendChart::TrendChart(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(240);
    retranslate();
}

void TrendChart::retranslate() {
    tooltip_ = LanguageManager::instance()->trText(QStringLiteral("最近 60 次测试趋势：TTFT、总延迟、Tokens/s"));
    setToolTip(tooltip_);
    update();
}

void TrendChart::setResults(const QList<TestResult>& results) {
    results_ = results.mid(qMax(0, results.size() - 60));
    update();
}

void TrendChart::paintEvent(QPaintEvent*) {
    const bool dark = ThemeManager::instance()->effectiveTheme() == QStringLiteral("dark");
    const QColor background = dark ? QColor("#17191e") : QColor("#ffffff");
    const QColor grid = dark ? QColor("#343944") : QColor("#dbe2ee");
    const QColor text = dark ? QColor("#aeb6c4") : QColor("#64748b");
    const QColor legendText = dark ? QColor("#cbd5e1") : QColor("#475569");
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), background);
    const QRectF plot = rect().adjusted(58, 22, -24, -42);
    p.setPen(grid);
    for (int i = 0; i <= 4; ++i) {
        const qreal y = plot.top() + plot.height() * i / 4.0;
        p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
    }
    auto* language = LanguageManager::instance();
    p.setPen(text);
    p.drawText(QRectF(8, 4, 130, 18), language->trText(QStringLiteral("延迟（ms）")));
    p.drawText(QRectF(width() - 150, 4, 130, 18), Qt::AlignRight,
               language->trText(QStringLiteral("速度（Token/s）")));
    if (results_.isEmpty()) {
        p.setPen(text);
        p.drawText(plot, Qt::AlignCenter, language->trText(QStringLiteral("暂无趋势数据")));
        return;
    }
    double maxLatency = 1, maxSpeed = 1;
    for (const auto& r : results_) {
        maxLatency = qMax(maxLatency, qMax(r.metrics.totalLatencyMs, r.metrics.ttftMs));
        maxSpeed = qMax(maxSpeed, r.metrics.tokensPerSecond);
    }
    auto pathFor = [&](auto getter, double max) {
        QPainterPath path;
        bool started = false;
        for (int i = 0; i < results_.size(); ++i) {
            const double v = getter(results_[i]);
            if (v < 0) continue;
            const qreal x = results_.size() == 1 ? plot.center().x()
                : plot.left() + plot.width() * i / (results_.size() - 1.0);
            const qreal y = plot.bottom() - plot.height() * qBound(0.0, v / max, 1.0);
            if (!started) { path.moveTo(x, y); started = true; }
            else path.lineTo(x, y);
        }
        return path;
    };
    const auto ttft = pathFor([](const TestResult& r) { return r.metrics.ttftMs; }, maxLatency);
    const auto total = pathFor([](const TestResult& r) { return r.metrics.totalLatencyMs; }, maxLatency);
    const auto speed = pathFor([](const TestResult& r) { return r.metrics.tokensPerSecond; }, maxSpeed);
    p.setPen(QPen(QColor("#60a5fa"), 2)); p.drawPath(ttft);
    p.setPen(QPen(QColor("#f59e0b"), 2)); p.drawPath(total);
    p.setPen(QPen(QColor("#34d399"), 2)); p.drawPath(speed);
    p.setPen(text);
    p.drawText(QRectF(8, plot.top() - 4, 44, 18), Qt::AlignRight, QString::number(maxLatency, 'f', 0));
    p.drawText(QRectF(8, plot.bottom() - 12, 44, 18), Qt::AlignRight, QStringLiteral("0"));
    p.drawText(QRectF(plot.right() + 4, plot.top() - 4, 50, 18), QString::number(maxSpeed, 'f', 0));
    qreal x = plot.left();
    auto legend = [&](const QColor& color, const QString& label) {
        p.setPen(QPen(color, 3));
        p.drawLine(QPointF(x, rect().bottom() - 20), QPointF(x + 18, rect().bottom() - 20));
        p.setPen(legendText);
        p.drawText(QRectF(x + 24, rect().bottom() - 30, 110, 20), label);
        x += 130;
    };
    legend(QColor("#60a5fa"), language->trText(QStringLiteral("TTFT")));
    legend(QColor("#f59e0b"), language->trText(QStringLiteral("总延迟")));
    legend(QColor("#34d399"), language->trText(QStringLiteral("Tokens/s")));
}
}
