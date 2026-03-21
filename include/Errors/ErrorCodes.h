#ifndef ERROR_CODES_H
#define ERROR_CODES_H

#include "SystemTypes.h"

// System error codes
#define noErr 0
#define memFullErr -108
#define nilHandleErr -109
#define memWZErr -111
#define memPurErr -112

// IO error codes
#define dsIOCoreErr -1279
#define ioTimeout -1075
#define queueIsPaused -1076
#define queueOverflow -1077
#define tooManyRequests -1078

// File error codes
#define fnfErr -43
#define eofErr -39
#define posErr -40
#define wPrErr -44
#define fLckdErr -45
#define dirNFErr -120
#define nsvErr -35

#endif