#pragma once
#include <QWidget>
class QLabel; class QPushButton; class QEvent; class QMouseEvent;
namespace airb {
class TitleBar final : public QWidget {
    Q_OBJECT
public:
    explicit TitleBar(QWidget* window, QWidget* parent = nullptr);
    void retranslate();
    void updateWindowState();
signals:
    void settingsRequested();
    void themeToggleRequested();
protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
private:
    QWidget* window_ = nullptr;
    QLabel* title_ = nullptr;
    QPushButton* theme_ = nullptr;
    QPushButton* settings_ = nullptr;
    QPushButton* minimize_ = nullptr;
    QPushButton* maximize_ = nullptr;
    QPushButton* close_ = nullptr;
};
}
