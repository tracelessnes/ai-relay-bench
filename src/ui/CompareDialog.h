#pragma once
#include "domain/Types.h"
#include <QDialog>
class QTableWidget;
namespace airb {
class CompareDialog final : public QDialog {
    Q_OBJECT
public:
    explicit CompareDialog(const QList<TestResult>& results, QWidget* parent = nullptr);
private:
    QTableWidget* table_ = nullptr;
};
}
