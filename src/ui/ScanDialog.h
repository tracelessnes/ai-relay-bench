#pragma once

#include "domain/Types.h"
#include "services/ScanService.h"
#include <QDialog>

class QLabel;
class QProgressBar;
class QTableWidget;
class QPushButton;

namespace airb {
class ScanService;

class ScanDialog final : public QDialog {
    Q_OBJECT
public:
    explicit ScanDialog(const Profile& profile, const QString& model, QWidget* parent = nullptr);

private slots:
    void onProgress(const QString& message);
    void onCheck(const ScanCheck& check);
    void onFinished(const ScanResult& result);
    void cancelScan();

private:
    void addCheck(const ScanCheck& check);
    void updateScore(int score);

    Profile profile_;
    QString model_;
    ScanService* service_ = nullptr;
    QLabel* title_ = nullptr;
    QLabel* profileLabel_ = nullptr;
    QLabel* progressLabel_ = nullptr;
    QLabel* scoreLabel_ = nullptr;
    QProgressBar* progress_ = nullptr;
    QTableWidget* checks_ = nullptr;
    QPushButton* cancel_ = nullptr;
    bool finished_ = false;
};
}
