#pragma once

#include "capture/CaptureTypes.h"
#include <QDialog>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QTabWidget;

namespace airb { class DialogTitleBar; }

namespace airb {
class CaptureProxyServer;

class CaptureDialog final : public QDialog {
    Q_OBJECT
public:
    explicit CaptureDialog(QWidget* parent = nullptr);

private slots:
    void startCapture();
    void stopCapture();
    void onExchange(const CaptureExchange& exchange);
    void selectExchange(int row, int column);
    void clearCaptures();
    void exportCapture();
    void retranslate();

private:
    void updateState(bool running, const QString& message = {});
    void showExchange(int row);
    void setHeaders(QPlainTextEdit* editor, const CaptureHeaders& headers);
    static QString bodyText(const QByteArray& body);
    static QString sseText(const QList<SseCaptureEvent>& events);
    static QString headerText(const CaptureHeaders& headers);

    CaptureProxyServer* proxy_ = nullptr;
    QList<CaptureExchange> exchanges_;
    QLineEdit* address_ = nullptr;
    QSpinBox* port_ = nullptr;
    QLabel* state_ = nullptr;
    QLabel* count_ = nullptr;
    QLabel* instructions_ = nullptr;
    QLabel* environment_ = nullptr;
    QPushButton* start_ = nullptr;
    QPushButton* stop_ = nullptr;
    QPushButton* clear_ = nullptr;
    QPushButton* export_ = nullptr;
    QTableWidget* table_ = nullptr;
    QTabWidget* detailTabs_ = nullptr;
    QPlainTextEdit* requestHeaders_ = nullptr;
    QPlainTextEdit* requestBody_ = nullptr;
    QPlainTextEdit* responseHeaders_ = nullptr;
    QPlainTextEdit* responseBody_ = nullptr;
    QPlainTextEdit* sseEvents_ = nullptr;
    DialogTitleBar* titleBar_ = nullptr;
    QLabel* title_ = nullptr;
    QLabel* detailTitle_ = nullptr;
};
} // namespace airb