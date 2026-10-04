#ifndef FINDER_ICON_LOGGING_H
#define FINDER_ICON_LOGGING_H

#include "System71StdLib.h"

#if defined(__aarch64__) || defined(__arm64__)
#define FINDER_ICON_LOG_DEBUG(...) ((void)0)
#else
#define FINDER_ICON_LOG_DEBUG(fmt, ...) \
    serial_logf(kLogModuleFinder, kLogLevelDebug, fmt, ##__VA_ARGS__)
#endif

#endif
