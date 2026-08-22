#pragma once
#include "domain/Types.h"
#include <QWidget>

namespace airb {
class TrendChart final : public QWidget {
    Q_OBJECT
public:
    explicit TrendChart(QWidget* parent = nullptr);
    void setResults(const QList<TestResult>& results);
    void retranslate();
    QSize minimumSizeHint() const override { return {500, 240}; }
protected:
    void paintEvent(QPaintEvent*) override;
private:
    QList<TestResult> results_;
    QString tooltip_;
};
}
