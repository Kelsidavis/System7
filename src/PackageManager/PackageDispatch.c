/*
 * PackageDispatch.c
 * System 7.1 Portable Package Manager Dispatch System
 *
 * Central dispatch system for routing package calls to appropriate implementations.
 * Handles trap instruction decoding and parameter marshalling for all Mac OS packages.
 */

#include "PackageManager/PackageManager.h"
#include "PackageManager/PackageTypes.h"
#include "PackageManager/SANEPackage.h"
#include "PackageManager/StringPackage.h"
#include "PackageManager/StandardFilePackage.h"
#include "PackageManager/ListManagerPackage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Package dispatch table */
typedef struct {
    int16_t         packID;
    const char     *name;
    uint16_t        trapBase;
    PackageFunction dispatcher;
} PackageDispatchEntry;

static PackageDispatchEntry g_dispatchTable[] = {
    {listMgr,   "List Manager",         0xA9E7, ListManagerDispatch},
    {stdFile,   "Standard File",        0xA9EA, StandardFileDispatch},
    {flPoint,   "SANE Floating Point",  0xA9EB, SANEDispatch},
    {trFunc,    "Transcendental Funcs", 0xA9EC, SANEDispatch},
    {intUtil,   "International Utils",  0xA9ED, StringDispatch},
    {bdConv,    "Binary/Decimal Conv",  0xA9EE, SANEDispatch},
    {dskInit,   "Disk Initialization",  0xA9E9, NULL}, /* Not implemented */
    {editionMgr, "Edition Manager",     0xA9EF, NULL}, /* Not implemented */
    {-1, NULL, 0, NULL} /* Terminator */
};

/* Forward declarations */
static PackageDispatchEntry *FindPackageByTrap(uint16_t trapWord);
static PackageDispatchEntry *FindPackageByID(int16_t packID);
static void LogPackageCall(int16_t packID, int16_t selector, const char *function);

/**
 * Main package dispatch entry point
 * This is called by the trap handler for all package traps
 */
int32_t PackageDispatch(uint16_t trapWord, void *params)
{
    /* Find package entry */
    PackageDispatchEntry *entry = FindPackageByTrap(trapWord);
    if (!entry || !entry->dispatcher) {
        /* Unknown or unimplemented trap 0x%04X */
        return PACKAGE_INVALID_SELECTOR;
    }

    /* Extract selector from parameters */
    int16_t selector = 0;
    if (params) {
        /* For most packages, selector is first parameter */
        selector = *(int16_t*)params;
    }

    LogPackageCall(entry->packID, selector, entry->name);

    /* Dispatch to package implementation */
    return entry->dispatcher(selector, params);
}

/**
 * Call package by ID and selector
 */
int32_t CallPackageByID(int16_t packID, int16_t selector, void *params)
{
    PackageDispatchEntry *entry = FindPackageByID(packID);
    if (!entry || !entry->dispatcher) {
        printf("CallPackageByID: Unknown or unimplemented package %d\n", packID);
        return PACKAGE_INVALID_ID;
    }

    LogPackageCall(packID, selector, entry->name);

    return entry->dispatcher(selector, params);
}

/**
 * Trap instruction handlers for specific packages
 */

/* List Manager trap handler (0xA9E7) */
int32_t Trap_ListManager(void *params)
{
    return PackageDispatch(0xA9E7, params);
}

/* Standard File trap handler (0xA9EA) */
int32_t Trap_StandardFile(void *params)
{
    return PackageDispatch(0xA9EA, params);
}

/* SANE Floating Point trap handler (0xA9EB) */
int32_t Trap_FloatingPoint(void *params)
{
    return PackageDispatch(0xA9EB, params);
}

/* Transcendental Functions trap handler (0xA9EC) */
int32_t Trap_Transcendental(void *params)
{
    return PackageDispatch(0xA9EC, params);
}

/* International Utilities trap handler (0xA9ED) */
int32_t Trap_International(void *params)
{
    return PackageDispatch(0xA9ED, params);
}

/* Binary/Decimal Conversion trap handler (0xA9EE) */
int32_t Trap_BinaryDecimal(void *params)
{
    return PackageDispatch(0xA9EE, params);
}

/**
 * High-level package function wrappers
 * These provide convenient C interfaces to package functions
 */

/* SANE wrapper functions */
double Package_Add(double x, double y)
{
    double args[2] = {x, y};
    CallPackageByID(flPoint, SANE_ADD, args);
    return args[0]; /* Result stored in first argument */
}

double Package_Multiply(double x, double y)
{
    double args[2] = {x, y};
    CallPackageByID(flPoint, SANE_MUL, args);
    return args[0];
}

double Package_Sqrt(double x)
{
    double args[1] = {x};
    CallPackageByID(flPoint, SANE_SQRT, args);
    return args[0];
}

double Package_Sin(double x)
{
    double args[1] = {x};
    CallPackageByID(flPoint, SANE_SIN, args);
    return args[0];
}

double Package_Log(double x)
{
    double args[1] = {x};
    CallPackageByID(flPoint, SANE_LOG, args);
    return args[0];
}

/* String utilities wrapper functions */
int16_t Package_CompareStrings(const char *str1, const char *str2)
{
    void *args[3] = {(void*)str1, (void*)str2, NULL};
    int16_t result = 0;
    args[2] = &result;
    CallPackageByID(intUtil, iuSelMagString, args);
    return result;
}

void Package_NumToString(int32_t num, char *str)
{
    void *args[2] = {&num, str};
    CallPackageByID(intUtil, 100, args); /* NumToString selector */
}

/* Standard File wrapper functions */
Boolean Package_GetFile(const char *prompt, char *filename, int maxLen)
{
    StandardFileReply reply;
    void *args[4] = {NULL, NULL, NULL, &reply};

    CallPackageByID(stdFile, sfSelStandardGet, args);

    if (reply.sfGood && filename) {
        /* Convert FSSpec name to C string */
        int nameLen = reply.sfFile.name[0];
        if (nameLen > maxLen - 1) nameLen = maxLen - 1;
        memcpy(filename, reply.sfFile.name + 1, nameLen);
        filename[nameLen] = '\0';
    }

    return reply.sfGood;
}

Boolean Package_PutFile(const char *prompt, const char *defaultName, char *filename, int maxLen)
{
    Str255 pPrompt, pDefaultName;
    StandardFileReply reply;

    /* Convert C strings to Pascal strings */
    if (prompt) {
        int len = strlen(prompt);
        if (len > 255) len = 255;
        pPrompt[0] = len;
        memcpy(pPrompt + 1, prompt, len);
    } else {
        pPrompt[0] = 0;
    }

    if (defaultName) {
        int len = strlen(defaultName);
        if (len > 255) len = 255;
        pDefaultName[0] = len;
        memcpy(pDefaultName + 1, defaultName, len);
    } else {
        pDefaultName[0] = 0;
    }

    void *args[3] = {pPrompt, pDefaultName, &reply};
    CallPackageByID(stdFile, sfSelStandardPut, args);

    if (reply.sfGood && filename) {
        /* Convert FSSpec name to C string */
        int nameLen = reply.sfFile.name[0];
        if (nameLen > maxLen - 1) nameLen = maxLen - 1;
        memcpy(filename, reply.sfFile.name + 1, nameLen);
        filename[nameLen] = '\0';
    }

    return reply.sfGood;
}

/* List Manager wrapper functions */
ListHandle Package_CreateList(const Rect *bounds, Point cellSize, Boolean hasScroll)
{
    Rect dataBounds = {0, 0, 100, 1}; /* Default 100 rows, 1 column */
    void *args[10];
    ListHandle result = NULL;

    args[0] = (void*)bounds;
    args[1] = &dataBounds;
    args[2] = &cellSize;
    args[3] = &(int16_t){0}; /* Default list proc */
    args[4] = NULL; /* Window */
    args[5] = &(Boolean){true}; /* Draw it */
    args[6] = &(Boolean){false}; /* Has grow */
    args[7] = &(Boolean){false}; /* Scroll horizontal */
    args[8] = &hasScroll; /* Scroll vertical */
    args[9] = &result;

    CallPackageByID(listMgr, lSelNew, args);
    return result;
}

void Package_SetListCell(ListHandle list, Cell cell, const char *text)
{
    if (!list || !text) return;

    int16_t textLen = strlen(text);
    void *args[4] = {(void*)text, &textLen, &cell, &list};
    CallPackageByID(listMgr, lSelSetCell, args);
}

void Package_GetListCell(ListHandle list, Cell cell, char *text, int maxLen)
{
    if (!list || !text) return;

    int16_t textLen = maxLen - 1;
    void *args[4] = {text, &textLen, &cell, &list};
    CallPackageByID(listMgr, lSelGetCell, args);

    if (textLen >= 0 && textLen < maxLen) {
        text[textLen] = '\0';
    } else {
        text[0] = '\0';
    }
}

/**
 * Package system utilities
 */

/* Get package information */
const char *GetPackageName(int16_t packID)
{
    PackageDispatchEntry *entry = FindPackageByID(packID);
    return entry ? entry->name : "Unknown Package";
}

Boolean IsPackageAvailable(int16_t packID)
{
    PackageDispatchEntry *entry = FindPackageByID(packID);
    return (entry && entry->dispatcher) ? true : false;
}

uint16_t GetPackageTrapWord(int16_t packID)
{
    PackageDispatchEntry *entry = FindPackageByID(packID);
    return entry ? entry->trapBase : 0;
}

/* List all available packages */
void ListAvailablePackages(void)
{
    printf("Available Package Manager packages:\n");
    printf("ID  Name                    Trap   Status\n");
    printf("--- ----------------------- ------ ----------\n");

    for (int i = 0; g_dispatchTable[i].packID >= 0; i++) {
        PackageDispatchEntry *entry = &g_dispatchTable[i];
        printf("%2d  %-22s 0x%04X %s\n",
               entry->packID,
               entry->name,
               entry->trapBase,
               entry->dispatcher ? "Available" : "Not Impl.");
    }
}

/* Package debugging and diagnostics */
void EnablePackageLogging(Boolean enabled)
{
    static Boolean loggingEnabled = false;
    loggingEnabled = enabled;
}

void DumpPackageState(int16_t packID)
{
    PackageDispatchEntry *entry = FindPackageByID(packID);
    if (!entry) {
        printf("Package %d not found\n", packID);
        return;
    }

    printf("Package %d (%s):\n", packID, entry->name);
    printf("  Trap Word: 0x%04X\n", entry->trapBase);
    printf("  Status: %s\n", entry->dispatcher ? "Implemented" : "Not implemented");
    printf("  Dispatcher: %p\n", (void*)entry->dispatcher);
}

/* Performance monitoring */
static struct {
    int32_t callCounts[16];
    double totalTime[16];
    double lastCallTime[16];
} g_packageStats;

void ResetPackageStats(void)
{
    memset(&g_packageStats, 0, sizeof(g_packageStats));
}

void GetPackageStats(int16_t packID, int32_t *callCount, double *avgTime)
{
    if (packID >= 0 && packID < 16) {
        if (callCount) *callCount = g_packageStats.callCounts[packID];
        if (avgTime) {
            *avgTime = (g_packageStats.callCounts[packID] > 0) ?
                      g_packageStats.totalTime[packID] / g_packageStats.callCounts[packID] : 0.0;
        }
    }
}

/**
 * Internal helper functions
 */
static PackageDispatchEntry *FindPackageByTrap(uint16_t trapWord)
{
    for (int i = 0; g_dispatchTable[i].packID >= 0; i++) {
        if (g_dispatchTable[i].trapBase == trapWord) {
            return &g_dispatchTable[i];
        }
    }
    return NULL;
}

static PackageDispatchEntry *FindPackageByID(int16_t packID)
{
    for (int i = 0; g_dispatchTable[i].packID >= 0; i++) {
        if (g_dispatchTable[i].packID == packID) {
            return &g_dispatchTable[i];
        }
    }
    return NULL;
}

static void LogPackageCall(int16_t packID, int16_t selector, const char *function)
{
    static Boolean loggingEnabled = false;

    if (loggingEnabled) {
        printf("Package call: %s (ID=%d, selector=0x%04X)\n",
               function, packID, selector);
    }

    /* Update statistics */
    if (packID >= 0 && packID < 16) {
        g_packageStats.callCounts[packID]++;
    }
}

/**
 * Emergency package system reset
 * Used when packages get into an inconsistent state
 */
void ResetPackageSystem(void)
{
    printf("Resetting Package Manager system...\n");

    /* Reinitialize all packages */
    InitPackageManager();
    InitAllPacks();

    /* Reset statistics */
    ResetPackageStats();

    printf("Package system reset complete\n");
}

/**
 * Package Manager self-test
 * Verifies that all packages are working correctly
 */
Boolean TestPackageSystem(void)
{
    Boolean allTestsPassed = true;

    printf("Running Package Manager self-test...\n");

    /* Test SANE package */
    printf("Testing SANE package... ");
    double testResult = Package_Add(2.5, 3.7);
    if (fabs(testResult - 6.2) < 0.001) {
        printf("PASSED\n");
    } else {
        printf("FAILED (got %f, expected 6.2)\n", testResult);
        allTestsPassed = false;
    }

    /* Test String package */
    printf("Testing String package... ");
    int16_t cmpResult = Package_CompareStrings("abc", "def");
    if (cmpResult < 0) {
        printf("PASSED\n");
    } else {
        printf("FAILED (string comparison)\n");
        allTestsPassed = false;
    }

    /* Test List Manager package */
    printf("Testing List Manager package... ");
    Rect listBounds = {0, 0, 200, 100};
    Point cellSize = {100, 16};
    ListHandle testList = Package_CreateList(&listBounds, cellSize, true);
    if (testList) {
        printf("PASSED\n");
        LDispose(testList); /* Clean up */
    } else {
        printf("FAILED (list creation)\n");
        allTestsPassed = false;
    }

    printf("Self-test %s\n", allTestsPassed ? "PASSED" : "FAILED");
    return allTestsPassed;
}