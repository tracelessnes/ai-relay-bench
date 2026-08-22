#pragma once
#include "CaptureTypes.h"
namespace airb { CaptureClient identifyCaptureClient(const CaptureHeaders& headers,const QByteArray& target,const QByteArray& body={}); QList<SseCaptureEvent> parseCapturedSse(const CaptureHeaders& headers,const QByteArray& body); QString summarizeCaptureBody(const QByteArray& body,qsizetype maxChars=2048); }
