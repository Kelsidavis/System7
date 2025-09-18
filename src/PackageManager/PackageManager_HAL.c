/*
 * PackageManager_HAL.c - Hardware Abstraction Layer Implementation
 *
 * Provides platform-specific implementations for package operations.
 *
 * Copyright (c) 2024 System7.1-Portable Project
 * MIT License
 */

#include "PackageManager/PackageManager_HAL.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

/* Platform-specific includes */
#ifdef __APPLE__
    #include <CoreFoundation/CoreFoundation.h>
    #include <AppKit/AppKit.h>
#elif defined(_WIN32)
    #include <windows.h>
    #include <commdlg.h>
#elif defined(__linux__)
    #include <gtk/gtk.h>
#endif

/* Package resource path */
static char gPackagePath[256] = "/System/Library/Packages/";

/* Initialize HAL */
OSErr PackageManager_HAL_Init(void) {
#ifdef __linux__
    /* Initialize GTK if needed for dialogs */
    if (!gtk_init_check(NULL, NULL)) {
        /* GTK not available, but not critical */
    }
#endif
    return noErr;
}

/* Cleanup HAL */
void PackageManager_HAL_Cleanup(void) {
    /* Platform-specific cleanup */
}

/* Load package from resources */
OSErr PackageManager_HAL_LoadPackage(short packID, Handle* packHandle) {
    /* Try to load from resource file first */
    OSErr err = PackageManager_HAL_LoadFromResource(packID, packHandle);

    if (err != noErr) {
        /* Try loading from file */
        char filename[256];
        sprintf(filename, "%sPACK_%d.rsrc", gPackagePath, packID);
        err = PackageManager_HAL_LoadFromFile(packID, filename, packHandle);
    }

    return err;
}

/* Load from resource */
OSErr PackageManager_HAL_LoadFromResource(short packID, Handle* packHandle) {
    /* In real implementation, this would load from resource fork */
    /* For now, return not found to use built-in implementations */
    *packHandle = NULL;
    return resNotFound;
}

/* Load from file */
OSErr PackageManager_HAL_LoadFromFile(short packID, const char* path,
                                      Handle* packHandle) {
    FILE* file = fopen(path, "rb");
    if (!file) {
        *packHandle = NULL;
        return fnfErr;
    }

    /* Get file size */
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    /* Allocate handle */
    *packHandle = NewHandle(size);
    if (!*packHandle) {
        fclose(file);
        return memFullErr;
    }

    /* Read package data */
    size_t read = fread(**packHandle, 1, size, file);
    fclose(file);

    if (read != size) {
        DisposeHandle(*packHandle);
        *packHandle = NULL;
        return ioErr;
    }

    return noErr;
}

/* Call package entry point */
OSErr PackageManager_HAL_CallPackage(short packID, short selector,
                                     Ptr entryPoint, void* params) {
    /* In real implementation, this would execute package code */
    /* For now, return selector error */
    return packSelectorErr;
}

/* Standard File - Get File */
OSErr PackageManager_HAL_StandardGetFile(void* params) {
#ifdef __APPLE__
    @autoreleasepool {
        NSOpenPanel* openPanel = [NSOpenPanel openPanel];
        [openPanel setCanChooseFiles:YES];
        [openPanel setCanChooseDirectories:NO];
        [openPanel setAllowsMultipleSelection:NO];

        if ([openPanel runModal] == NSModalResponseOK) {
            NSURL* url = [[openPanel URLs] objectAtIndex:0];
            /* Store result in params */
            return noErr;
        }
        return userCanceledErr;
    }

#elif defined(_WIN32)
    OPENFILENAME ofn;
    char szFile[260] = {0};

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn)) {
        /* Store result in params */
        return noErr;
    }
    return userCanceledErr;

#elif defined(__linux__)
    /* Use GTK file chooser if available */
    return noErr;

#else
    return unimpErr;
#endif
}

/* Standard File - Put File */
OSErr PackageManager_HAL_StandardPutFile(void* params) {
#ifdef __APPLE__
    @autoreleasepool {
        NSSavePanel* savePanel = [NSSavePanel savePanel];

        if ([savePanel runModal] == NSModalResponseOK) {
            NSURL* url = [savePanel URL];
            /* Store result in params */
            return noErr;
        }
        return userCanceledErr;
    }

#elif defined(_WIN32)
    OPENFILENAME ofn;
    char szFile[260] = {0};

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

    if (GetSaveFileName(&ofn)) {
        /* Store result in params */
        return noErr;
    }
    return userCanceledErr;

#elif defined(__linux__)
    /* Use GTK file chooser if available */
    return noErr;

#else
    return unimpErr;
#endif
}

/* SANE Floating Point Operations */
OSErr PackageManager_HAL_SANEOperation(short selector, void* params) {
    /* SANE operations map to standard math functions */
    /* Selector determines which operation */

    /* This would dispatch to specific math operations */
    return noErr;
}

/* Floating point operations */
double PackageManager_HAL_FloatAdd(double a, double b) {
    return a + b;
}

double PackageManager_HAL_FloatSub(double a, double b) {
    return a - b;
}

double PackageManager_HAL_FloatMul(double a, double b) {
    return a * b;
}

double PackageManager_HAL_FloatDiv(double a, double b) {
    if (b == 0.0) {
        /* Handle division by zero */
        return NAN;
    }
    return a / b;
}

double PackageManager_HAL_FloatSqrt(double x) {
    return sqrt(x);
}

double PackageManager_HAL_FloatSin(double x) {
    return sin(x);
}

double PackageManager_HAL_FloatCos(double x) {
    return cos(x);
}

double PackageManager_HAL_FloatTan(double x) {
    return tan(x);
}

double PackageManager_HAL_FloatLog(double x) {
    return log(x);
}

double PackageManager_HAL_FloatExp(double x) {
    return exp(x);
}

/* International Utilities */
OSErr PackageManager_HAL_IntlOperation(short selector, void* params) {
    /* Handle international operations */
    return noErr;
}

/* Get international resource */
OSErr PackageManager_HAL_GetIntlResource(short id, Handle* resource) {
    /* Load localized resource */
    *resource = NULL;
    return resNotFound;
}

/* Compare strings according to locale */
OSErr PackageManager_HAL_CompareString(const char* s1, const char* s2,
                                       short* result) {
    *result = strcmp(s1, s2);
    return noErr;
}

/* Convert to uppercase */
OSErr PackageManager_HAL_UppercaseText(char* text, short length) {
    int i;
    for (i = 0; i < length; i++) {
        text[i] = toupper(text[i]);
    }
    return noErr;
}

/* Convert to lowercase */
OSErr PackageManager_HAL_LowercaseText(char* text, short length) {
    int i;
    for (i = 0; i < length; i++) {
        text[i] = tolower(text[i]);
    }
    return noErr;
}

/* Binary to decimal conversion */
OSErr PackageManager_HAL_BinaryToDecimal(long binary, char* decimal) {
    sprintf(decimal, "%ld", binary);
    return noErr;
}

/* Decimal to binary conversion */
OSErr PackageManager_HAL_DecimalToBinary(const char* decimal, long* binary) {
    char* endptr;
    *binary = strtol(decimal, &endptr, 10);

    if (*endptr != '\0') {
        return paramErr;
    }
    return noErr;
}

/* Color Picker */
OSErr PackageManager_HAL_ColorPicker(short selector, void* params) {
    RGBColor* color = (RGBColor*)params;
    Boolean ok = false;

    return PackageManager_HAL_ShowColorDialog(color, &ok);
}

/* Show color picker dialog */
OSErr PackageManager_HAL_ShowColorDialog(RGBColor* color, Boolean* ok) {
#ifdef __APPLE__
    @autoreleasepool {
        NSColorPanel* colorPanel = [NSColorPanel sharedColorPanel];

        /* Set initial color */
        CGFloat r = color->red / 65535.0;
        CGFloat g = color->green / 65535.0;
        CGFloat b = color->blue / 65535.0;
        [colorPanel setColor:[NSColor colorWithRed:r green:g blue:b alpha:1.0]];

        /* Show panel (would need to be modal in real implementation) */
        [colorPanel makeKeyAndOrderFront:nil];

        /* Get selected color */
        NSColor* selected = [colorPanel color];
        color->red = [selected redComponent] * 65535;
        color->green = [selected greenComponent] * 65535;
        color->blue = [selected blueComponent] * 65535;

        *ok = true;
        return noErr;
    }

#elif defined(_WIN32)
    CHOOSECOLOR cc;
    static COLORREF customColors[16];

    ZeroMemory(&cc, sizeof(cc));
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = NULL;
    cc.lpCustColors = customColors;
    cc.rgbResult = RGB(color->red >> 8, color->green >> 8, color->blue >> 8);
    cc.Flags = CC_FULLOPEN | CC_RGBINIT;

    if (ChooseColor(&cc)) {
        color->red = GetRValue(cc.rgbResult) << 8;
        color->green = GetGValue(cc.rgbResult) << 8;
        color->blue = GetBValue(cc.rgbResult) << 8;
        *ok = true;
        return noErr;
    }

    *ok = false;
    return userCanceledErr;

#elif defined(__linux__)
    /* Use GTK color chooser if available */
    *ok = false;
    return noErr;

#else
    *ok = false;
    return unimpErr;
#endif
}

/* Get resource */
Handle PackageManager_HAL_GetResource(ResType type, short id) {
    /* This would load from resource file */
    return NULL;
}

/* Release resource */
void PackageManager_HAL_ReleaseResource(Handle resource) {
    if (resource) {
        DisposeHandle(resource);
    }
}

/* Get package path */
const char* PackageManager_HAL_GetPackagePath(void) {
    return gPackagePath;
}

/* Set package path */
void PackageManager_HAL_SetPackagePath(const char* path) {
    if (path) {
        strncpy(gPackagePath, path, sizeof(gPackagePath) - 1);
        gPackagePath[sizeof(gPackagePath) - 1] = '\0';
    }
}