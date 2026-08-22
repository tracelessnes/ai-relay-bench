#pragma once
#include "CaptureTypes.h"
namespace airb {
class HttpParser final {
public:
 enum class Mode { Request, Response };
 explicit HttpParser(Mode mode,qsizetype maxHeaderBytes=64*1024,qsizetype maxBodyBytes=16*1024*1024);
 bool feed(const QByteArray& bytes); bool finish(); bool isComplete()const{return message_.complete;} bool hasError()const{return message_.malformed;} QString error()const{return message_.error;} const HttpMessage& message()const{return message_;} QByteArray remaining()const{return remaining_;} void reset();
 static bool parse(const QByteArray& bytes,Mode mode,HttpMessage* message,qsizetype maxHeaderBytes=64*1024,qsizetype maxBodyBytes=16*1024*1024);
private: bool tryParse(bool final); bool fail(const QString& message); Mode mode_; qsizetype maxHeaderBytes_; qsizetype maxBodyBytes_; QByteArray buffer_,remaining_; HttpMessage message_;
}; }
