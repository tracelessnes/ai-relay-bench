#pragma once
#include <QByteArray>
#include <QString>
namespace airb { class SecureStorage { public: static QByteArray protect(const QString& plain); static QString unprotect(const QByteArray& cipher); }; }
