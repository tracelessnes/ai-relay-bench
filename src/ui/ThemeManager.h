#pragma once
#include <QObject>
#include <QString>

class QApplication;
namespace airb {
enum class ThemeMode { System, Dark, Light };
class ThemeManager final : public QObject {
    Q_OBJECT
public:
    static ThemeManager* instance();
    ThemeMode mode() const;
    QString modeId() const;
    bool setMode(ThemeMode mode, bool persist = true);
    bool setModeId(const QString& id, bool persist = true);
    void apply(QApplication& app);
    QString effectiveTheme() const;
signals:
    void themeChanged();
private:
    explicit ThemeManager(QObject* parent = nullptr);
    ThemeMode mode_ = ThemeMode::System;
    QString stylesheetPath() const;
    static ThemeMode modeFromId(const QString& id);
};
}
