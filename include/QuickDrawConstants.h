#ifndef QUICKDRAW_CONSTANTS_H
#define QUICKDRAW_CONSTANTS_H

/* QuickDraw transfer modes */
#ifndef srcCopy
enum QuickDrawPatterns {
    srcCopy = 0,
    srcOr = 1,
    srcXor = 2,
    srcBic = 3,
    notSrcCopy = 4,
    notSrcOr = 5,
    notSrcXor = 6,
    notSrcBic = 7,
    patCopy = 8,
    patOr = 9,
    patXor = 10,
    patBic = 11,
    notPatCopy = 12,
    notPatOr = 13,
    notPatXor = 14,
    notPatBic = 15,
    blend = 32,
    addPin = 33,
    addOver = 34,
    subPin = 35,
    addMax = 37,
    adMax = 37,
    subOver = 38,
    adMin = 39,
    ditherCopy = 64,
    transparent = 36
};
#endif

/* Region Manager error codes */
#ifndef rgnOverflowErr
#define rgnOverflowErr -147
#endif

/* Additional error codes */
#ifndef insufficientStackErr
#define insufficientStackErr -149
#endif

/* Standard colors */
#ifndef blackColor
#define blackColor 33
#define whiteColor 30
#define redColor 205
#define greenColor 341
#define blueColor 409
#define cyanColor 273
#define magentaColor 137
#define yellowColor 69
#endif

/* GDevice flags */
#ifndef screenActive
#define screenActive 15
#define mainScreen 11
#define screenDevice 0
#define noDriver 1
#define clutType 0
#define fixedType 1
#define directType 2
#endif

/* Shape drawing verbs */
#ifndef frame
#define frame 0
#define paint 1
#define erase 2
#define invert 3
#define fill 4
#endif

#endif /* QUICKDRAW_CONSTANTS_H */
