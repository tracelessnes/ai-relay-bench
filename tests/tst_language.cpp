#include <QtTest>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include "ui/CaptureDialog.h"
#include "ui/SettingsDialog.h"
#include "i18n/LanguageManager.h"

class LanguageTests final : public QObject {
    Q_OBJECT
private slots:
    void builtInSimplifiedChinesePackIsValid() {
        QFile file(QStringLiteral(":/lang/zh-CN.json"));
        QVERIFY2(file.open(QIODevice::ReadOnly), "built-in zh-CN language pack is missing");
        QJsonParseError parseError{};
        const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
        QCOMPARE(parseError.error, QJsonParseError::NoError);
        QVERIFY(document.isObject());

        const auto root = document.object();
        QCOMPARE(root.value(QStringLiteral("language")).toString(), QStringLiteral("zh-CN"));
        QCOMPARE(root.value(QStringLiteral("name")).toString(),
                 QStringLiteral("\u7B80\u4F53\u4E2D\u6587"));

        const auto strings = root.value(QStringLiteral("strings")).toObject();
        QCOMPARE(strings.value(QStringLiteral("AI \u4E2D\u8F6C\u7AD9\u6D4B\u901F\u5DE5\u5177")).toString(),
                 QStringLiteral("AI \u4E2D\u8F6C\u7AD9\u6D4B\u901F\u5DE5\u5177"));
        QCOMPARE(strings.value(QStringLiteral("capture.analysis")).toString(),
                 QStringLiteral("\u6293\u5305\u5206\u6790"));
        QCOMPARE(strings.value(QStringLiteral("capture.start")).toString(),
                 QStringLiteral("\u542F\u52A8\u6293\u5305"));
        QVERIFY(strings.value(QStringLiteral("capture.instructions")).toString().contains(
            QStringLiteral("HTTP_PROXY")));
        QVERIFY(strings.value(QStringLiteral("capture.httpsWarning")).toString().contains(
            QStringLiteral("HTTPS CONNECT")));

        auto* manager = airb::LanguageManager::instance();
        QCOMPARE(manager->currentLanguage(), QStringLiteral("zh-CN"));
        QCOMPARE(manager->trKey(QStringLiteral("capture.analysis"), QStringLiteral("Capture Analysis")),
                 QStringLiteral("\u6293\u5305\u5206\u6790"));
        QCOMPARE(manager->trKey(QStringLiteral("capture.instructions"), QStringLiteral("fallback")),
                 strings.value(QStringLiteral("capture.instructions")).toString());
    }

    void captureDialogUsesSimplifiedChineseAtRuntime() {
        auto* manager = airb::LanguageManager::instance();
        QVERIFY(manager->setLanguage(QStringLiteral("zh-CN"), false));

        airb::CaptureDialog dialog;
        QCOMPARE(dialog.windowTitle(), QStringLiteral("\u6293\u5305\u5206\u6790"));
        QCOMPARE(dialog.findChild<QLabel*>(QStringLiteral("contentTitle"))->text(),
                 QStringLiteral("\u6293\u5305\u5206\u6790"));
        QCOMPARE(dialog.findChild<QPushButton*>(QStringLiteral("primaryButton"))->text(),
                 QStringLiteral("\u542F\u52A8\u6293\u5305"));
        QCOMPARE(dialog.findChild<QLabel*>(QStringLiteral("captureInstructions"))->text().contains(
                     QStringLiteral("HTTP_PROXY")), true);
        QCOMPARE(dialog.findChild<QTabWidget*>(QStringLiteral("captureDetails"))->tabText(0),
                 QStringLiteral("\u8BF7\u6C42\u5934"));

        const auto closeButtons = dialog.findChildren<QPushButton*>(QStringLiteral("titleBarClose"));
        QCOMPARE(closeButtons.size(), 1);
        QCOMPARE(closeButtons.first()->text(), QStringLiteral("\u00D7"));
    }

    void settingsDialogUsesSelfDrawnChineseTitleBar() {
        auto* manager = airb::LanguageManager::instance();
        QVERIFY(manager->setLanguage(QStringLiteral("zh-CN"), false));

        airb::SettingsDialog dialog;
        QCOMPARE(dialog.windowTitle(), QStringLiteral("\u7CFB\u7EDF\u8BBE\u7F6E"));
        auto* title = dialog.findChild<QLabel*>(QStringLiteral("windowTitleLabel"));
        QVERIFY(title);
        QCOMPARE(title->text(), QStringLiteral("\u7CFB\u7EDF\u8BBE\u7F6E"));

        const auto closeButtons = dialog.findChildren<QPushButton*>(QStringLiteral("titleBarClose"));
        QCOMPARE(closeButtons.size(), 1);
        QCOMPARE(closeButtons.first()->text(), QStringLiteral("\u00D7"));

        const auto primaryButtons = dialog.findChildren<QPushButton*>(QStringLiteral("primaryButton"));
        QVERIFY(!primaryButtons.isEmpty());
        QCOMPARE(primaryButtons.first()->text(), QStringLiteral("\u5173\u95ED"));
    }
};

#include "tst_language.moc"
QTEST_MAIN(LanguageTests)
