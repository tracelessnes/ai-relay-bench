#pragma once
#include "domain/Types.h"
#include <QMainWindow>
#include <QList>

class QComboBox; class QLineEdit; class QPlainTextEdit; class QSpinBox; class QCheckBox; class QLabel; class QPushButton; class QTableWidget; class QProgressBar;
namespace airb { class BenchmarkController; class ModelService; class ProfileStore; class TrendChart; class TitleBar;
class MainWindow final:public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* p=nullptr);
    ~MainWindow() override;
private slots:
    void loadProfile(int);
    void newProfile();
    void deleteProfile();
    void saveProfile();
    void importProfiles();
    void exportProfiles();
    void fetchModels();
    void validateHeaders();
    void applyPreset(const QString&);
    void runSingle();
    void runBatch();
    void runStability();
    void onResult(const TestResult&);
    void updateSummary();
    void exportResults();
    void showRaw(int,int);
    void filterResults(const QString&);
    void showSettings();
    void toggleTheme();
    void retranslateUi();
protected:
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
private:
    Profile formProfile() const;
    RequestConfig formRequest() const;
    bool validateForm(bool requireModel=true);
    void setBusy(bool);
    QWidget* metricCard(const QString&,QLabel**);
    void populateProfiles(int select=-1);
    void addResultRow(const TestResult&);
    void updateMetricCards(const TestResult&);
    void loadHistory();
    QList<Profile> profiles_; QList<ModelInfo> models_; QList<TestResult> rowResults_;
    BenchmarkController* controller_; ModelService* modelService_; ProfileStore* store_; TrendChart* trend_;
    TitleBar* titleBar_ = nullptr; QComboBox* profileBox_; QLineEdit* profileName_; QComboBox* protocolBox_; QLineEdit* baseUrl_; QLineEdit* apiKey_; QCheckBox* showKey_; QCheckBox* context1M_; QComboBox* modelBox_; QSpinBox* timeout_; QPlainTextEdit* headers_; QLabel* headerState_; QPlainTextEdit* prompt_; QLineEdit* regex_; QCheckBox* caseSensitive_; QSpinBox* rounds_; QSpinBox* concurrency_; QComboBox* proxyMode_; QLineEdit* proxyHost_; QSpinBox* proxyPort_; QLineEdit* proxyUser_; QLineEdit* proxyPassword_;
    QPushButton* singleButton_; QPushButton* batchButton_; QPushButton* stabilityButton_; QPushButton* stopButton_; QLabel* statusValue_; QLabel* ttftValue_; QLabel* generationValue_; QLabel* speedValue_; QLabel* usageValue_; QLabel* summary_; QPlainTextEdit* output_; QTableWidget* table_; QProgressBar* progress_; QLineEdit* resultFilter_;
}; }
