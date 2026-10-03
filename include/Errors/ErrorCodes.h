#ifndef SYSTEM7_ERRORS_ERROR_CODES_H
#define SYSTEM7_ERRORS_ERROR_CODES_H

#include "SystemTypes.h"

// Memory Manager errors
#define memFullErr -108
#define nilHandleErr -109
#define memWZErr -111
#define memPurErr -112

// I/O errors
#define ioErr -36
#define dsIOCoreErr -1279
#define ioTimeout -1075
#define queueIsPaused -1076
#define queueOverflow -1077
#define tooManyRequests -1078

// File Manager and HFS errors
#define vLckdErr -46
#define fBsyErr -47
#define opWrErr -49
#define volOffLinErr -53
#define permErr -54
#define nsvErr -35
#define fnfErr -43
#define fnOpnErr -38
#define eofErr -39
#define posErr -40
#define mFulErr -41
#define tmfoErr -42
#define wPrErr -44
#define fLckdErr -45
#define dskFulErr -34
#define dirNFErr -120
#define tmwdoErr -121
#define diffVolErr -1303
#define btNoErr 0
#define btRecNotFnd -1300

// Resource Manager errors
#define resNotFound -192

#endif /* SYSTEM7_ERRORS_ERROR_CODES_H */
