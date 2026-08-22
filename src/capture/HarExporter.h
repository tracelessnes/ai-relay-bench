#pragma once
#include "CaptureTypes.h"
namespace airb { class HarExporter final { public: static QByteArray toJson(const QList<CaptureExchange>& exchanges,bool includeBody=true); static QString toMarkdown(const QList<CaptureExchange>& exchanges); }; }
