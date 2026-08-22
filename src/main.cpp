#include "ui/MainWindow.h"
#include "i18n/LanguageManager.h"
#include "ui/ThemeManager.h"
#include <QApplication>
#include <QNetworkProxyFactory>
int main(int argc,char**argv){
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication app(argc,argv);
    QApplication::setOrganizationName("AIRelayBench");
    QApplication::setApplicationName("AI 中转站测速工具");
    QApplication::setApplicationVersion(APP_VERSION);
    QNetworkProxyFactory::setUseSystemConfiguration(true);
    airb::LanguageManager::instance();
    airb::ThemeManager::instance()->apply(app);
    airb::MainWindow window;
    window.show();
    return app.exec();
}
