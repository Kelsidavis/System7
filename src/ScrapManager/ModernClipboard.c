#include "MemoryMgr/MemoryManager.h"
// #include "CompatibilityFix.h" // Removed
#include "ErrorCodes.h"
#include <stdlib.h>
#include <string.h>
/*
 * ModernClipboard.c - Modern Clipboard Integration
 * System 7.1 Portable - Scrap Manager Component
 *
 * Implements integration with modern platform clipboard systems including
 * Windows Clipboard, macOS Pasteboard, and X11 selections for seamless
 * cross-platform clipboard operation.
 */


#include "ScrapManager/ScrapManager.h"
#include "ErrorCodes.h"
#include "ScrapManager/ScrapTypes.h"
#include "ErrorCodes.h"
#include "ScrapManager/ScrapFormats.h"
#include "ErrorCodes.h"
#include "SystemTypes.h"
#include "ErrorCodes.h"
#include "System71StdLib.h"
#include "ErrorCodes.h"
#include "ScrapManager/ScrapLogging.h"

/* Platform detection */
#ifdef PLATFORM_REMOVED_WIN32
    #define PLATFORM_WINDOWS 1
    #include <windows.h>
#elif defined(__APPLE__)
    #define PLATFORM_MACOS 1
    #include <CoreFoundation/CoreFoundation.h>
    #include <ApplicationServices/ApplicationServices.h>
#elif defined(__linux__) || defined(__unix__)
    #define PLATFORM_X11 1
    #include <X11/Xlib.h>
    #include <X11/Xatom.h>

#endif

/* Platform format mapping */
typedef struct {
    ResType         macType;
    UInt32        platformFormat;
    char            formatName[64];
    Boolean         isRegistered;
} FormatMapping;

/* Modern clipboard state */
typedef struct {
    Boolean             isInitialized;
    Boolean             useNativeClipboard;
    ModernClipboardContext context;
    FormatMapping       formatMappings[64];
    SInt16             mappingCount;
    UInt32            lastChangeSequence;
    time_t              lastSyncTime;
    Boolean             autoSync;
    void                *platformSpecific;
} ModernClipboardState;

static ModernClipboardState gModernState = {0};

/* Internal function prototypes */
static OSErr InitializePlatformClipboard(void);
static void CleanupPlatformClipboard(void);
static OSErr RegisterDefaultFormatMappings(void);
static FormatMapping *FindFormatMapping(ResType macType);
static FormatMapping *FindFormatMappingByPlatform(UInt32 platformFormat);
static OSErr SyncMacToNative(void);
static OSErr SyncNativeToMac(void);
static UInt32 GetNativeChangeSequence(void);
static OSErr PutNativeData(UInt32 format, const void *data, size_t size);
static OSErr GetNativeData(UInt32 format, void **data, size_t *size);

/* Platform-specific implementations */
#ifdef PLATFORM_WINDOWS
static OSErr InitializeWindowsClipboard(void);
static void CleanupWindowsClipboard(void);
static OSErr PutWindowsData(UInt32 format, const void *data, size_t size);
static OSErr GetWindowsData(UInt32 format, void **data, size_t *size);
static UInt32 GetWindowsChangeSequence(void);
#endif

#ifdef PLATFORM_MACOS
static OSErr InitializeMacOSPasteboard(void);
static void CleanupMacOSPasteboard(void);
static OSErr PutMacOSData(UInt32 format, const void *data, size_t size);
static OSErr GetMacOSData(UInt32 format, void **data, size_t *size);
static UInt32 GetMacOSChangeSequence(void);
#endif

#ifdef PLATFORM_X11
static OSErr InitializeX11Clipboard(void);
static void CleanupX11Clipboard(void);
static OSErr PutX11Data(UInt32 format, const void *data, size_t size);
static OSErr GetX11Data(UInt32 format, void **data, size_t *size);
static UInt32 GetX11ChangeSequence(void);
#endif

/*
 * Modern Clipboard Integration Functions
 */

OSErr InitModernClipboard(void)
{
    OSErr err = noErr;

    if (gModernState.isInitialized) {
        return noErr;
    }

    memset(&gModernState, 0, sizeof(gModernState));

    /* Initialize platform-specific clipboard */
    err = InitializePlatformClipboard();
    if (err != noErr) {
        return err;
    }

    /* Register default format mappings */
    err = RegisterDefaultFormatMappings();
    if (err != noErr) {
        CleanupPlatformClipboard();
        return err;
    }

    gModernState.isInitialized = true;
    gModernState.useNativeClipboard = true;
    gModernState.autoSync = true;
    gModernState.lastChangeSequence = GetNativeChangeSequence();
    gModernState.lastSyncTime = time(NULL);

    return noErr;
}

void CleanupModernClipboard(void)
{
    if (!gModernState.isInitialized) {
        return;
    }

    CleanupPlatformClipboard();
    memset(&gModernState, 0, sizeof(gModernState));
}

OSErr SyncWithNativeClipboard(Boolean toNative)
{
    OSErr err = noErr;

    if (!gModernState.isInitialized) {
        return scrapNoError;
    }

    if (!gModernState.useNativeClipboard) {
        return noErr;
    }

    if (toNative) {
        err = SyncMacToNative();
    } else {
        err = SyncNativeToMac();
    }

    if (err == noErr) {
        gModernState.lastSyncTime = time(NULL);
        gModernState.lastChangeSequence = GetNativeChangeSequence();
    }

    return err;
}

OSErr RegisterPlatformFormat(ResType macType, UInt32 platformFormat, const char *formatName)
{
    FormatMapping *mapping;

    if (!gModernState.isInitialized) {
        InitModernClipboard();
    }

    if (gModernState.mappingCount >= 64) {
        return scrapTooManyFormats;
    }

    /* Check if already registered */
    mapping = FindFormatMapping(macType);
    if (mapping) {
        mapping->platformFormat = platformFormat;
        if (formatName) {
            strncpy(mapping->formatName, formatName, sizeof(mapping->formatName) - 1);
            mapping->formatName[sizeof(mapping->formatName) - 1] = '\0';
        }
        return noErr;
    }

    /* Add new mapping */
    mapping = &gModernState.formatMappings[gModernState.mappingCount];
    mapping->macType = macType;
    mapping->platformFormat = platformFormat;
    mapping->isRegistered = true;

    if (formatName) {
        strncpy(mapping->formatName, formatName, sizeof(mapping->formatName) - 1);
        mapping->formatName[sizeof(mapping->formatName) - 1] = '\0';
    } else {
        mapping->formatName[0] = '\0';
    }

    gModernState.mappingCount++;
    return noErr;
}

UInt32 MacToPlatformFormat(ResType macType)
{
    FormatMapping *mapping;

    if (!gModernState.isInitialized) {
        return 0;
    }

    mapping = FindFormatMapping(macType);
    return mapping ? mapping->platformFormat : 0;
}

ResType PlatformToMacFormat(UInt32 platformFormat)
{
    FormatMapping *mapping;

    if (!gModernState.isInitialized) {
        return 0;
    }

    mapping = FindFormatMappingByPlatform(platformFormat);
    return mapping ? mapping->macType : 0;
}

Boolean HasNativeClipboardChanged(void)
{
    UInt32 currentSequence;

    if (!gModernState.isInitialized || !gModernState.useNativeClipboard) {
        return false;
    }

    currentSequence = GetNativeChangeSequence();
    return (currentSequence != gModernState.lastChangeSequence);
}

OSErr GetNativeClipboardData(UInt32 platformFormat, Handle *data)
{
    OSErr err = noErr;
    void *nativeData = NULL;
    size_t dataSize = 0;

    if (!data) {
        return paramErr;
    }

    if (!gModernState.isInitialized) {
        return scrapNoError;
    }

    err = GetNativeData(platformFormat, &nativeData, &dataSize);
    if (err != noErr || !nativeData) {
        return err;
    }

    *data = NewHandle(dataSize);
    if (!*data) {
        DisposePtr((Ptr)nativeData);
        return memFullErr;
    }

    HLock(*data);
    if (!**data) {
        HUnlock(*data);
        DisposeHandle(*data);
        *data = NULL;
        DisposePtr((Ptr)nativeData);
        return memFullErr;
    }
    memcpy(**data, nativeData, dataSize);
    HUnlock(*data);

    DisposePtr((Ptr)nativeData);
    return noErr;
}

OSErr PutNativeClipboardData(UInt32 platformFormat, Handle data)
{
    OSErr err = noErr;

    if (!data) {
        return paramErr;
    }

    if (!gModernState.isInitialized) {
        return scrapNoError;
    }

    HLock(data);
    err = PutNativeData(platformFormat, *data, GetHandleSize(data));
    HUnlock(data);

    if (err == noErr) {
        gModernState.lastChangeSequence = GetNativeChangeSequence();
    }

    return err;
}

/*
 * Automatic Synchronization Functions
 */

OSErr EnableAutoSync(Boolean enable)
{
    if (!gModernState.isInitialized) {
        InitModernClipboard();
    }

    gModernState.autoSync = enable;
    return noErr;
}

OSErr CheckAndSyncClipboard(void)
{
    if (!gModernState.isInitialized || !gModernState.autoSync) {
        return noErr;
    }

    if (HasNativeClipboardChanged()) {
        return SyncWithNativeClipboard(false); /* Sync from native to Mac */
    }

    return noErr;
}

/*
 * Internal Helper Functions
 */

static OSErr InitializePlatformClipboard(void)
{
#ifdef PLATFORM_WINDOWS
    return InitializeWindowsClipboard();
#elif defined(PLATFORM_MACOS)
    return InitializeMacOSPasteboard();
#elif defined(PLATFORM_X11)
    return InitializeX11Clipboard();
#else
    /* No native clipboard support */
    gModernState.useNativeClipboard = false;
    return noErr;
#endif
}

static void CleanupPlatformClipboard(void)
{
#ifdef PLATFORM_WINDOWS
    CleanupWindowsClipboard();
#elif defined(PLATFORM_MACOS)
    CleanupMacOSPasteboard();
#elif defined(PLATFORM_X11)
    CleanupX11Clipboard();
#endif
}

static OSErr RegisterDefaultFormatMappings(void)
{
    OSErr err = noErr;

#ifdef PLATFORM_WINDOWS
    /* Windows clipboard format mappings */
    RegisterPlatformFormat(SCRAP_TYPE_TEXT, CF_TEXT, "CF_TEXT");
    RegisterPlatformFormat(SCRAP_TYPE_UTF8, CF_UNICODETEXT, "CF_UNICODETEXT");
    RegisterPlatformFormat(SCRAP_TYPE_PICT, CF_DIB, "CF_DIB");
    RegisterPlatformFormat(SCRAP_TYPE_PNG, RegisterClipboardFormat("PNG"), "PNG");
    RegisterPlatformFormat(SCRAP_TYPE_HTML, RegisterClipboardFormat("HTML Format"), "HTML Format");
    RegisterPlatformFormat(SCRAP_TYPE_RTF, RegisterClipboardFormat("Rich Text Format"), "Rich Text Format");

#elif defined(PLATFORM_MACOS)
    /* macOS pasteboard format mappings */
    RegisterPlatformFormat(SCRAP_TYPE_TEXT, 'TEXT', "com.apple.traditional-mac-plain-text");
    RegisterPlatformFormat(SCRAP_TYPE_UTF8, 'utf8', "public.utf8-plain-text");
    RegisterPlatformFormat(SCRAP_TYPE_RTF, 'RTF ', "public.rtf");
    RegisterPlatformFormat(SCRAP_TYPE_HTML, 'HTML', "public.html");
    RegisterPlatformFormat(SCRAP_TYPE_PNG, 'PNG ', "public.png");
    RegisterPlatformFormat(SCRAP_TYPE_JPEG, 'JPEG', "public.jpeg");
    RegisterPlatformFormat(SCRAP_TYPE_PICT, 'PICT', "com.apple.pict");

#elif defined(PLATFORM_X11)
    /* X11 selection format mappings */
    RegisterPlatformFormat(SCRAP_TYPE_TEXT, XA_STRING, "STRING");
    RegisterPlatformFormat(SCRAP_TYPE_UTF8, 0, "UTF8_STRING"); /* Would get atom */
    RegisterPlatformFormat(SCRAP_TYPE_HTML, 0, "text/html");
    RegisterPlatformFormat(SCRAP_TYPE_PNG, 0, "image/png");
    RegisterPlatformFormat(SCRAP_TYPE_JPEG, 0, "image/jpeg");

#endif

    return err;
}

static FormatMapping *FindFormatMapping(ResType macType)
{
    SInt16 i;

    for (i = 0; i < gModernState.mappingCount; i++) {
        if (gModernState.formatMappings[i].isRegistered &&
            gModernState.formatMappings[i].macType == macType) {
            return &gModernState.formatMappings[i];
        }
    }

    return NULL;
}

static FormatMapping *FindFormatMappingByPlatform(UInt32 platformFormat)
{
    SInt16 i;

    for (i = 0; i < gModernState.mappingCount; i++) {
        if (gModernState.formatMappings[i].isRegistered &&
            gModernState.formatMappings[i].platformFormat == platformFormat) {
            return &gModernState.formatMappings[i];
        }
    }

    return NULL;
}

static OSErr SyncMacToNative(void)
{
    OSErr err = noErr;
    ResType availableTypes[16];
    SInt16 typeCount;
    SInt16 i;

    /* Get available Mac scrap formats */
    err = GetScrapFormats(availableTypes, &typeCount, 16);
    if (err != noErr || typeCount == 0) {
        return err;
    }

    /* Convert and put each format to native clipboard */
    for (i = 0; i < typeCount; i++) {
        Handle macData = NewHandle(0);
        FormatMapping *mapping;
        SInt32 offset;

        if (!macData) continue;

        /* Get Mac data */
        err = GetScrap(macData, availableTypes[i], &offset);
        if (err < 0) {
            DisposeHandle(macData);
            continue;
        }

        /* Find platform mapping */
        mapping = FindFormatMapping(availableTypes[i]);
        if (!mapping) {
            DisposeHandle(macData);
            continue;
        }

        /* Put to native clipboard */
        HLock(macData);
        PutNativeData(mapping->platformFormat, *macData, GetHandleSize(macData));
        HUnlock(macData);

        DisposeHandle(macData);
    }

    return noErr;
}

static OSErr SyncNativeToMac(void)
{
    /* This would enumerate native clipboard formats and convert to Mac formats */
    /* For now, implement basic text synchronization */

#ifdef PLATFORM_WINDOWS
    if (IsClipboardFormatAvailable(CF_TEXT)) {
        void *textData = NULL;
        size_t textSize = 0;

        if (GetNativeData(CF_TEXT, &textData, &textSize) == noErr && textData) {
            ZeroScrap();
            PutScrap(textSize, SCRAP_TYPE_TEXT, textData);
            DisposePtr((Ptr)textData);
        }
    }
#endif

    return noErr;
}

static UInt32 GetNativeChangeSequence(void)
{
#ifdef PLATFORM_WINDOWS
    return GetWindowsChangeSequence();
#elif defined(PLATFORM_MACOS)
    return GetMacOSChangeSequence();
#elif defined(PLATFORM_X11)
    return GetX11ChangeSequence();
#else
    return 0;
#endif
}

static OSErr PutNativeData(UInt32 format, const void *data, size_t size)
{
#ifdef PLATFORM_WINDOWS
    return PutWindowsData(format, data, size);
#elif defined(PLATFORM_MACOS)
    return PutMacOSData(format, data, size);
#elif defined(PLATFORM_X11)
    return PutX11Data(format, data, size);
#else
    return scrapConversionError;
#endif
}

static OSErr GetNativeData(UInt32 format, void **data, size_t *size)
{
#ifdef PLATFORM_WINDOWS
    return GetWindowsData(format, data, size);
#elif defined(PLATFORM_MACOS)
    return GetMacOSData(format, data, size);
#elif defined(PLATFORM_X11)
    return GetX11Data(format, data, size);
#else
    return scrapConversionError;
#endif
}

/*
 * Platform-Specific Implementations
 */

#ifdef PLATFORM_WINDOWS

static OSErr InitializeWindowsClipboard(void)
{
    /* Windows clipboard is always available */
    return noErr;
}

static void CleanupWindowsClipboard(void)
{
    /* No cleanup needed for Windows clipboard */
}

static OSErr PutWindowsData(UInt32 format, const void *data, size_t size)
{
    HGLOBAL hMem;
    void *pMem;

    if (!OpenClipboard(NULL)) {
        return scrapPermissionError;
    }

    EmptyClipboard();

    hMem = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!hMem) {
        CloseClipboard();
        return memFullErr;
    }

    pMem = GlobalLock(hMem);
    if (pMem) {
        memcpy(pMem, data, size);
        GlobalUnlock(hMem);
        SetClipboardData(format, hMem);
    }

    CloseClipboard();
    return noErr;
}

static OSErr GetWindowsData(UInt32 format, void **data, size_t *size)
{
    HGLOBAL hMem;
    void *pMem;
    size_t dataSize;

    *data = NULL;
    *size = 0;

    if (!OpenClipboard(NULL)) {
        return scrapPermissionError;
    }

    hMem = GetClipboardData(format);
    if (!hMem) {
        CloseClipboard();
        return scrapNoTypeError;
    }

    dataSize = GlobalSize(hMem);
    *data = NewPtr(dataSize);
    if (!*data) {
        CloseClipboard();
        return memFullErr;
    }

    pMem = GlobalLock(hMem);
    if (pMem) {
        memcpy(*data, pMem, dataSize);
        *size = dataSize;
        GlobalUnlock(hMem);
    }

    CloseClipboard();
    return noErr;
}

static UInt32 GetWindowsChangeSequence(void)
{
    return GetClipboardSequenceNumber();
}

#endif /* PLATFORM_WINDOWS */

#ifdef PLATFORM_MACOS

static OSErr InitializeMacOSPasteboard(void)
{
    /* macOS Pasteboard is always available */
    return noErr;
}

static void CleanupMacOSPasteboard(void)
{
    /* No cleanup needed for macOS Pasteboard */
}

static OSErr PutMacOSData(UInt32 format, const void *data, size_t size)
{
    /* This would use Pasteboard APIs */
    /* Simplified implementation */
    return noErr;
}

static OSErr GetMacOSData(UInt32 format, void **data, size_t *size)
{
    /* This would use Pasteboard APIs */
    /* Simplified implementation */
    *data = NULL;
    *size = 0;
    return scrapNoTypeError;
}

static UInt32 GetMacOSChangeSequence(void)
{
    /* This would get pasteboard change count */
    return 0;
}

#endif /* PLATFORM_MACOS */

#ifdef PLATFORM_X11

static OSErr InitializeX11Clipboard(void)
{
    /* This would initialize X11 connection and atoms */
    return noErr;
}

static void CleanupX11Clipboard(void)
{
    /* This would cleanup X11 resources */
}

static OSErr PutX11Data(UInt32 format, const void *data, size_t size)
{
    /* This would use X11 selection APIs */
    return noErr;
}

static OSErr GetX11Data(UInt32 format, void **data, size_t *size)
{
    /* This would use X11 selection APIs */
    *data = NULL;
    *size = 0;
    return scrapNoTypeError;
}

static UInt32 GetX11ChangeSequence(void)
{
    /* This would track X11 selection changes */
    return 0;
}

#endif /* PLATFORM_X11 */
