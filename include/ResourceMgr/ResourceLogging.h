/*
 * ResourceLogging.h - Logging macros for Resource Manager subsystem
 *
 * Provides module-specific logging macros that route through the
 * hierarchical logging system with [RM] tag prefix.
 */

#ifndef RESOURCE_LOGGING_H
#define RESOURCE_LOGGING_H

#include "System71StdLib.h"

/* CRITICAL FIX: Disable RM logging on ARM64 - serial_logf uses va_list which
 * causes hangs on bare-metal ARM64 due to ABI differences */
#if defined(__aarch64__) || defined(__arm64__)
#define RM_LOG_TRACE(fmt, ...) ((void)0)
#define RM_LOG_DEBUG(fmt, ...) ((void)0)
#define RM_LOG_INFO(fmt, ...)  ((void)0)
#define RM_LOG_WARN(fmt, ...)  ((void)0)
#define RM_LOG_ERROR(fmt, ...) ((void)0)
#else
#define RM_LOG_TRACE(fmt, ...) serial_logf(kLogModuleResource, kLogLevelTrace, "[RM] " fmt, ##__VA_ARGS__)
#define RM_LOG_DEBUG(fmt, ...) serial_logf(kLogModuleResource, kLogLevelDebug, "[RM] " fmt, ##__VA_ARGS__)
#define RM_LOG_INFO(fmt, ...)  serial_logf(kLogModuleResource, kLogLevelInfo,  "[RM] " fmt, ##__VA_ARGS__)
#define RM_LOG_WARN(fmt, ...)  serial_logf(kLogModuleResource, kLogLevelWarn,  "[RM] " fmt, ##__VA_ARGS__)
#define RM_LOG_ERROR(fmt, ...) serial_logf(kLogModuleResource, kLogLevelError, "[RM] " fmt, ##__VA_ARGS__)
#endif

#endif /* RESOURCE_LOGGING_H */
