/*
 * System 7.1 Error Codes
 * Consolidated error definitions for all managers
 */

#ifndef ERROR_CODES_H
#define ERROR_CODES_H

#include "MacTypes.h"
#include "Errors/ErrorCodes.h"

/* ScrapManager Error Codes */
#define scrapNoError            0       /* No error */
#define scrapNoTypeError        -102    /* No such scrap type */
#define scrapCorruptError       -103    /* Scrap data corrupt */
#define scrapConversionError    -104    /* Conversion failed */
#define scrapMemoryError        -108    /* Not enough memory for scrap */

/* Standard QD Error Codes */
#define qdNoError               0       /* No error */
#define qdMemoryError           -108    /* Not enough memory */
#define qdRegionTooBigError     -147    /* Region too complex */
#define qdPictureDataError      -148    /* Bad picture data */

/* Zone Operation Results */
#define zoneOK                  0       /* Zone operation successful */
#define zoneError               -111    /* Generic zone error */

#endif /* ERROR_CODES_H */
