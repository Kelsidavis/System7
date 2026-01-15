/*
 * FinderLogging.h - Logging macros for Finder/Desktop subsystem
 *
 * Provides module-specific logging macros that route through the
 * hierarchical logging system with [FINDER] tag prefix.
 */

#ifndef FINDER_LOGGING_H
#define FINDER_LOGGING_H

#include "System71StdLib.h"

/* CRITICAL FIX: Disable logging on ARM64 - serial_logf uses va_list which
 * causes hangs on bare-metal ARM64 due to ABI differences */
#if defined(__aarch64__) || defined(__arm64__)
#define FINDER_LOG_TRACE(fmt, ...) ((void)0)
#define FINDER_LOG_DEBUG(fmt, ...) ((void)0)
#define FINDER_LOG_INFO(fmt, ...)  ((void)0)
#define FINDER_LOG_WARN(fmt, ...)  ((void)0)
#define FINDER_LOG_ERROR(fmt, ...) ((void)0)

#define DESKTOP_LOG_TRACE(fmt, ...) ((void)0)
#define DESKTOP_LOG_DEBUG(fmt, ...) ((void)0)
#define DESKTOP_LOG_INFO(fmt, ...)  ((void)0)
#define DESKTOP_LOG_WARN(fmt, ...)  ((void)0)
#define DESKTOP_LOG_ERROR(fmt, ...) ((void)0)
#else
#define FINDER_LOG_TRACE(fmt, ...) serial_logf(kLogModuleFinder, kLogLevelTrace, "[FINDER] " fmt, ##__VA_ARGS__)
#define FINDER_LOG_DEBUG(fmt, ...) serial_logf(kLogModuleFinder, kLogLevelDebug, "[FINDER] " fmt, ##__VA_ARGS__)
#define FINDER_LOG_INFO(fmt, ...)  serial_logf(kLogModuleFinder, kLogLevelInfo,  "[FINDER] " fmt, ##__VA_ARGS__)
#define FINDER_LOG_WARN(fmt, ...)  serial_logf(kLogModuleFinder, kLogLevelWarn,  "[FINDER] " fmt, ##__VA_ARGS__)
#define FINDER_LOG_ERROR(fmt, ...) serial_logf(kLogModuleFinder, kLogLevelError, "[FINDER] " fmt, ##__VA_ARGS__)

#define DESKTOP_LOG_TRACE(fmt, ...) serial_logf(kLogModuleDesktop, kLogLevelTrace, "[DESKTOP] " fmt, ##__VA_ARGS__)
#define DESKTOP_LOG_DEBUG(fmt, ...) serial_logf(kLogModuleDesktop, kLogLevelDebug, "[DESKTOP] " fmt, ##__VA_ARGS__)
#define DESKTOP_LOG_INFO(fmt, ...)  serial_logf(kLogModuleDesktop, kLogLevelInfo,  "[DESKTOP] " fmt, ##__VA_ARGS__)
#define DESKTOP_LOG_WARN(fmt, ...)  serial_logf(kLogModuleDesktop, kLogLevelWarn,  "[DESKTOP] " fmt, ##__VA_ARGS__)
#define DESKTOP_LOG_ERROR(fmt, ...) serial_logf(kLogModuleDesktop, kLogLevelError, "[DESKTOP] " fmt, ##__VA_ARGS__)
#endif

#endif /* FINDER_LOGGING_H */
