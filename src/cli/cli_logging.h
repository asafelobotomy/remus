// purpose:  Redirect Qt logging macros to the CLI logging category for all CLI translation units.
// when:     Include at the top of every CLI .cpp file (after Qt headers), before any qDebug/qInfo use.
// inputs:   None (no arguments — macro-only header)
// outputs:  Redefines qDebug/qInfo/qWarning/qCritical to use logCli category.
// risk:     safe
// source:   original
//
// NOTE: Include AFTER all Qt headers and AFTER logging_categories.h.
//       This is an internal implementation header; do NOT include from other headers.
#pragma once
#include "../core/logging_categories.h"

#undef qDebug
#undef qInfo
#undef qWarning
#undef qCritical
#define qDebug()    qCDebug(logCli)
#define qInfo()     qCInfo(logCli)
#define qWarning()  qCWarning(logCli)
#define qCritical() qCCritical(logCli)
