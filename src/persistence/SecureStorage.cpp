#include "SecureStorage.h"
#ifdef Q_OS_WIN
#include <windows.h>
#include <wincrypt.h>
#endif
namespace airb {
QByteArray SecureStorage::protect(const QString&p){auto in=p.toUtf8();if(in.isEmpty())return{};
#ifdef Q_OS_WIN
 DATA_BLOB src{DWORD(in.size()),reinterpret_cast<BYTE*>(in.data())},dst{};if(CryptProtectData(&src,L"AI Relay Bench",nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&dst)){QByteArray out(reinterpret_cast<char*>(dst.pbData),int(dst.cbData));LocalFree(dst.pbData);return out;}
#endif
 return in.toBase64();}
QString SecureStorage::unprotect(const QByteArray&c){if(c.isEmpty())return{};
#ifdef Q_OS_WIN
 DATA_BLOB src{DWORD(c.size()),reinterpret_cast<BYTE*>(const_cast<char*>(c.data()))},dst{};if(CryptUnprotectData(&src,nullptr,nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&dst)){QString out=QString::fromUtf8(reinterpret_cast<char*>(dst.pbData),int(dst.cbData));LocalFree(dst.pbData);return out;}
#endif
 return QString::fromUtf8(QByteArray::fromBase64(c));}
}
