#pragma once
#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QWidget;

namespace airb {
class DialogTitleBar;
class SettingsDialog final : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

private slots:
    void importLanguage();
    void refreshLanguageList();
    void applyLanguage(int index);
    void applyTheme(int index);
    void retranslate();

private:
    DialogTitleBar* titleBar_ = nullptr;
    QComboBox* language_ = nullptr;
    QComboBox* theme_ = nullptr;
    QCheckBox* followSystem_ = nullptr;
    QLabel* systemLanguage_ = nullptr;
    QLabel* source_ = nullptr;
    QPushButton* import_ = nullptr;
    QPushButton* openDir_ = nullptr;
    QPushButton* reset_ = nullptr;
    QLabel* languageLabel_ = nullptr;
    QLabel* themeLabel_ = nullptr;
    QLabel* systemLabel_ = nullptr;
    QLabel* sourceLabel_ = nullptr;
    QLabel* hint_ = nullptr;
    QPushButton* close_ = nullptr;
};
}
