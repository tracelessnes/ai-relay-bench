#include "ThemeManager.h"
#include <QApplication>
#include <QFile>
#include <QPalette>
#include <QSettings>
#include <QStyleHints>

namespace airb {
namespace { ThemeManager* g_theme = nullptr; }
ThemeManager* ThemeManager::instance() { if (!g_theme) g_theme = new ThemeManager(qApp); return g_theme; }
ThemeManager::ThemeManager(QObject* parent) : QObject(parent) {
    mode_ = modeFromId(QSettings().value(QStringLiteral("ui/theme"), QStringLiteral("system")).toString());
    if (qApp && qApp->styleHints()) {
        connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged, this,
                [this](Qt::ColorScheme) {
                    if (mode_ != ThemeMode::System || !qApp) return;
                    apply(*qApp);
                    emit themeChanged();
                });
    }
}
ThemeMode ThemeManager::modeFromId(const QString& id) { if (id.compare(QStringLiteral("dark"), Qt::CaseInsensitive) == 0) return ThemeMode::Dark; if (id.compare(QStringLiteral("light"), Qt::CaseInsensitive) == 0) return ThemeMode::Light; return ThemeMode::System; }
ThemeMode ThemeManager::mode() const { return mode_; }
QString ThemeManager::modeId() const { return mode_ == ThemeMode::Dark ? QStringLiteral("dark") : mode_ == ThemeMode::Light ? QStringLiteral("light") : QStringLiteral("system"); }
QString ThemeManager::effectiveTheme() const {
    if (mode_ != ThemeMode::System) return mode_ == ThemeMode::Dark ? QStringLiteral("dark") : QStringLiteral("light");
    // Qt 6 exposes the operating-system color scheme.  It is more reliable than
    // inspecting the application palette, which is still the default palette at
    // startup (before our QSS has been applied).
    if (qApp && qApp->styleHints()) {
        const auto scheme = qApp->styleHints()->colorScheme();
        if (scheme == Qt::ColorScheme::Dark) return QStringLiteral("dark");
        if (scheme == Qt::ColorScheme::Light) return QStringLiteral("light");
    }
    const auto p = qApp ? qApp->palette() : QPalette();
    return p.color(QPalette::Window).lightness() < 128 ? QStringLiteral("dark") : QStringLiteral("light");
}
QString ThemeManager::stylesheetPath() const { return effectiveTheme() == QStringLiteral("dark") ? QStringLiteral(":/dark.qss") : QStringLiteral(":/light.qss"); }
void ThemeManager::apply(QApplication& app) { QFile f(stylesheetPath()); if (f.open(QIODevice::ReadOnly)) app.setStyleSheet(QString::fromUtf8(f.readAll())); }
bool ThemeManager::setMode(ThemeMode mode, bool persist) { if (mode_ == mode) { apply(*qApp); return true; } mode_ = mode; if (persist) QSettings().setValue(QStringLiteral("ui/theme"), modeId()); apply(*qApp); emit themeChanged(); return true; }
bool ThemeManager::setModeId(const QString& id, bool persist) { return setMode(modeFromId(id), persist); }
}
