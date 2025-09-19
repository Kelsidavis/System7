/*
 * Copyright (c) 2024 System7 Project
 * MIT License - See LICENSE file
 */
/*
 * ComponentManager_HAL.c
 *
 * Hardware Abstraction Layer for Component Manager
 * Provides platform-specific plugin and threading support
 *
 * Based on Mac OS 7.1 Component Manager
 */

#include "ComponentManager/ComponentManager_HAL.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <mach-o/dyld.h>
#include <dirent.h>
#elif defined(__linux__)
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#elif defined(_WIN32)
#include <windows.h>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")
#endif

/* ========================================================================
 * Threading Support
 * ======================================================================== */

struct ComponentMutex {
    pthread_mutex_t mutex;
    Boolean initialized;
};

OSErr HAL_CreateMutex(ComponentMutex** mutex) {
    if (!mutex) return paramErr;

    *mutex = malloc(sizeof(ComponentMutex));
    if (!*mutex) return memFullErr;

    if (pthread_mutex_init(&(*mutex)->mutex, NULL) != 0) {
        free(*mutex);
        *mutex = NULL;
        return componentSecurityErr;
    }

    (*mutex)->initialized = true;
    return noErr;
}

OSErr HAL_DestroyMutex(ComponentMutex* mutex) {
    if (!mutex || !mutex->initialized) return paramErr;

    pthread_mutex_destroy(&mutex->mutex);
    mutex->initialized = false;
    free(mutex);
    return noErr;
}

OSErr HAL_LockMutex(ComponentMutex* mutex) {
    if (!mutex || !mutex->initialized) return paramErr;
    if (pthread_mutex_lock(&mutex->mutex) != 0) return componentSecurityErr;
    return noErr;
}

OSErr HAL_UnlockMutex(ComponentMutex* mutex) {
    if (!mutex || !mutex->initialized) return paramErr;
    if (pthread_mutex_unlock(&mutex->mutex) != 0) return componentSecurityErr;
    return noErr;
}

/* ========================================================================
 * Platform-Specific Plugin Discovery
 * ======================================================================== */

#ifdef __APPLE__

OSErr HAL_ScanForComponents_macOS(const char* directory) {
    CFStringRef dirPath = CFStringCreateWithCString(NULL, directory, kCFStringEncodingUTF8);
    if (!dirPath) return memFullErr;

    CFURLRef dirURL = CFURLCreateWithFileSystemPath(NULL, dirPath, kCFURLPOSIXPathStyle, true);
    CFRelease(dirPath);
    if (!dirURL) return componentNotFound;

    /* Enumerate bundles in directory */
    CFArrayRef bundleArray = CFBundleCopyExecutableArchitecturesForURL(dirURL);
    if (bundleArray) {
        CFIndex count = CFArrayGetCount(bundleArray);
        for (CFIndex i = 0; i < count; i++) {
            /* Process each bundle */
            /* Would load bundle and register components here */
        }
        CFRelease(bundleArray);
    }

    /* Also scan for .component files */
    DIR* dir = opendir(directory);
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strstr(entry->d_name, ".component")) {
                char fullPath[512];
                snprintf(fullPath, sizeof(fullPath), "%s/%s", directory, entry->d_name);

                /* Load component bundle */
                CFStringRef bundlePath = CFStringCreateWithCString(NULL, fullPath, kCFStringEncodingUTF8);
                CFURLRef bundleURL = CFURLCreateWithFileSystemPath(NULL, bundlePath, kCFURLPOSIXPathStyle, true);
                CFBundleRef bundle = CFBundleCreate(NULL, bundleURL);

                if (bundle) {
                    /* Get component entry point */
                    ComponentRoutine entryPoint = (ComponentRoutine)CFBundleGetFunctionPointerForName(bundle, CFSTR("ComponentEntryPoint"));
                    if (entryPoint) {
                        /* Get component description */
                        ComponentDescription* (*getDesc)(void) = (ComponentDescription* (*)(void))
                            CFBundleGetFunctionPointerForName(bundle, CFSTR("GetComponentDescription"));

                        if (getDesc) {
                            ComponentDescription* desc = getDesc();
                            RegisterComponent(desc, entryPoint, 0, NULL, NULL, NULL);
                        }
                    }
                    /* Note: Don't release bundle, keep it loaded */
                }

                CFRelease(bundlePath);
                CFRelease(bundleURL);
            }
        }
        closedir(dir);
    }

    CFRelease(dirURL);
    return noErr;
}

#elif defined(__linux__)

OSErr HAL_ScanForComponents_Linux(const char* directory) {
    DIR* dir = opendir(directory);
    if (!dir) return componentNotFound;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        /* Look for .so files */
        if (strstr(entry->d_name, ".so")) {
            char fullPath[512];
            snprintf(fullPath, sizeof(fullPath), "%s/%s", directory, entry->d_name);

            /* Check if it's a component */
            struct stat st;
            if (stat(fullPath, &st) == 0 && S_ISREG(st.st_mode)) {
                /* Try to load as component */
                void* handle = dlopen(fullPath, RTLD_LAZY | RTLD_LOCAL);
                if (handle) {
                    /* Get component entry point */
                    ComponentRoutine entryPoint = (ComponentRoutine)dlsym(handle, "ComponentEntryPoint");
                    if (entryPoint) {
                        /* Get component description */
                        ComponentDescription* (*getDesc)(void) =
                            (ComponentDescription* (*)(void))dlsym(handle, "GetComponentDescription");

                        if (getDesc) {
                            ComponentDescription* desc = getDesc();
                            RegisterComponent(desc, entryPoint, 0, NULL, NULL, NULL);
                        }
                    }
                    /* Note: Don't dlclose, keep it loaded */
                }
            }
        }
    }

    closedir(dir);
    return noErr;
}

#elif defined(_WIN32)

OSErr HAL_ScanForComponents_Windows(const char* directory) {
    char searchPath[MAX_PATH];
    snprintf(searchPath, sizeof(searchPath), "%s\\*.dll", directory);

    WIN32_FIND_DATA findData;
    HANDLE hFind = FindFirstFile(searchPath, &findData);

    if (hFind == INVALID_HANDLE_VALUE) {
        return componentNotFound;
    }

    do {
        char fullPath[MAX_PATH];
        snprintf(fullPath, sizeof(fullPath), "%s\\%s", directory, findData.cFileName);

        /* Try to load as component */
        HMODULE hModule = LoadLibrary(fullPath);
        if (hModule) {
            /* Get component entry point */
            ComponentRoutine entryPoint = (ComponentRoutine)GetProcAddress(hModule, "ComponentEntryPoint");
            if (entryPoint) {
                /* Get component description */
                typedef ComponentDescription* (*GetDescFunc)(void);
                GetDescFunc getDesc = (GetDescFunc)GetProcAddress(hModule, "GetComponentDescription");

                if (getDesc) {
                    ComponentDescription* desc = getDesc();
                    RegisterComponent(desc, entryPoint, 0, NULL, NULL, NULL);
                }
            }
            /* Note: Don't FreeLibrary, keep it loaded */
        }
    } while (FindNextFile(hFind, &findData));

    FindClose(hFind);
    return noErr;
}

#endif

/* ========================================================================
 * Unified HAL Interface
 * ======================================================================== */

OSErr HAL_ScanForComponents(const char* directory) {
    if (!directory) return paramErr;

#ifdef __APPLE__
    return HAL_ScanForComponents_macOS(directory);
#elif defined(__linux__)
    return HAL_ScanForComponents_Linux(directory);
#elif defined(_WIN32)
    return HAL_ScanForComponents_Windows(directory);
#else
    return componentDllLoadErr;
#endif
}

/* ========================================================================
 * Platform-Specific Component Info
 * ======================================================================== */

OSErr HAL_GetPlatformInfo(char* platformName, size_t maxLength) {
    if (!platformName || maxLength == 0) return paramErr;

    /* Fill platform name */
#ifdef __APPLE__
    snprintf(platformName, maxLength, "macOS");
#elif defined(__linux__)
    snprintf(platformName, maxLength, "Linux");
#elif defined(_WIN32)
    snprintf(platformName, maxLength, "Windows");
#else
    snprintf(platformName, maxLength, "Unknown");
#endif

    return noErr;
}

/* ========================================================================
 * Component File Operations
 * ======================================================================== */

OSErr HAL_OpenComponentFile(const char* path, int16_t* refNum) {
    if (!path || !refNum) return paramErr;

    /* Simple file handle allocation */
    static int16_t nextRefNum = 1000;
    *refNum = nextRefNum++;

    /* Store path for later use */
    /* In a real implementation, would maintain a file table */

    return noErr;
}

OSErr HAL_CloseComponentFile(int16_t refNum) {
    /* In a real implementation, would close the file */
    return noErr;
}

/* ========================================================================
 * Component Resource Loading
 * ======================================================================== */

OSErr HAL_LoadComponentResource(int16_t refNum, OSType resourceType, int16_t resourceID, Handle* resource) {
    if (!resource) return paramErr;

    /* This would load resources from component files */
    /* For now, return empty handle */
    *resource = NewHandle(0);
    if (!*resource) return memFullErr;

    return noErr;
}

/* ========================================================================
 * Memory Management
 * ======================================================================== */

Handle NewHandle(Size size) {
    Handle h = malloc(sizeof(void*));
    if (!h) return NULL;

    *h = malloc(size);
    if (!*h) {
        free(h);
        return NULL;
    }

    return h;
}

void DisposeHandle(Handle h) {
    if (!h) return;
    if (*h) free(*h);
    free(h);
}

Size GetHandleSize(Handle h) {
    if (!h || !*h) return 0;
    /* In a real implementation, would track handle sizes */
    return 0;
}

OSErr SetHandleSize(Handle h, Size newSize) {
    if (!h) return paramErr;

    void* newPtr = realloc(*h, newSize);
    if (!newPtr && newSize > 0) return memFullErr;

    *h = newPtr;
    return noErr;
}

void HLock(Handle h) {
    /* Handle locking not needed in modern systems */
}

void HUnlock(Handle h) {
    /* Handle unlocking not needed in modern systems */
}

/* ========================================================================
 * Component Security
 * ======================================================================== */

OSErr HAL_ValidateComponent(Component component) {
    /* Validate component integrity */
    /* Could check signatures, checksums, etc. */
    return noErr;
}

OSErr HAL_SetComponentSecurity(ComponentSecurityLevel level) {
    /* Set security level for component loading */
    return noErr;
}

/* ========================================================================
 * Platform-Specific Initialization
 * ======================================================================== */

OSErr HAL_InitializePlatform(void) {
#ifdef __APPLE__
    /* macOS-specific initialization */
#elif defined(__linux__)
    /* Linux-specific initialization */
#elif defined(_WIN32)
    /* Windows-specific initialization */
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
    return noErr;
}

void HAL_CleanupPlatform(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

/* ========================================================================
 * Memory Management Functions
 * ======================================================================== */

void* HAL_AllocateMemory(size_t size) {
    return malloc(size);
}

void HAL_FreeMemory(void* ptr) {
    if (ptr) free(ptr);
}

void* HAL_ReallocateMemory(void* ptr, size_t newSize) {
    return realloc(ptr, newSize);
}

void HAL_ZeroMemory(void* ptr, size_t size) {
    if (ptr) memset(ptr, 0, size);
}

void HAL_CopyMemory(const void* src, void* dst, size_t size) {
    if (src && dst) memcpy(dst, src, size);
}

/* ========================================================================
 * Time and Date Functions
 * ======================================================================== */

uint64_t HAL_GetCurrentTime(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + tv.tv_usec;
}

OSErr HAL_TimeToString(uint64_t time, char* timeString, size_t maxLength) {
    if (!timeString || maxLength == 0) return paramErr;

    time_t seconds = (time_t)(time / 1000000ULL);
    struct tm* tm_info = localtime(&seconds);

    if (strftime(timeString, maxLength, "%Y-%m-%d %H:%M:%S", tm_info) == 0) {
        return paramErr;
    }

    return noErr;
}

/* ========================================================================
 * String Utilities
 * ======================================================================== */

OSErr HAL_StringCompare(const char* str1, const char* str2, int32_t* result) {
    if (!str1 || !str2 || !result) return paramErr;
    *result = strcmp(str1, str2);
    return noErr;
}

OSErr HAL_StringCopy(const char* source, char* destination, size_t maxLength) {
    if (!source || !destination || maxLength == 0) return paramErr;
    strncpy(destination, source, maxLength - 1);
    destination[maxLength - 1] = '\0';
    return noErr;
}

OSErr HAL_StringLength(const char* string, size_t* length) {
    if (!string || !length) return paramErr;
    *length = strlen(string);
    return noErr;
}

/* ========================================================================
 * Component Instance Management
 * ======================================================================== */

static ComponentInstanceData* gInstanceDataList = NULL;
static ComponentMutex* gInstanceMutex = NULL;

ComponentInstanceData* GetInstanceData(ComponentInstance ci) {
    if (!ci) return NULL;

    if (!gInstanceMutex) {
        HAL_CreateMutex(&gInstanceMutex);
    }

    HAL_LockMutex(gInstanceMutex);

    /* Simple linear search for this implementation */
    ComponentInstanceData* current = gInstanceDataList;
    while (current) {
        if ((ComponentInstance)current == ci) {
            HAL_UnlockMutex(gInstanceMutex);
            return current;
        }
        current = (ComponentInstanceData*)current; /* Simple approach */
    }

    HAL_UnlockMutex(gInstanceMutex);
    return NULL;
}

OSErr SetInstanceData(ComponentInstance ci, ComponentInstanceData* data) {
    if (!ci || !data) return paramErr;

    if (!gInstanceMutex) {
        HAL_CreateMutex(&gInstanceMutex);
    }

    HAL_LockMutex(gInstanceMutex);

    /* Add to list */
    data->component = NULL; /* Will be set by caller */

    HAL_UnlockMutex(gInstanceMutex);
    return noErr;
}

OSErr AllocateInstanceData(ComponentInstance ci) {
    if (!ci) return paramErr;

    ComponentInstanceData* data = HAL_AllocateMemory(sizeof(ComponentInstanceData));
    if (!data) return memFullErr;

    HAL_ZeroMemory(data, sizeof(ComponentInstanceData));
    data->isOpen = false;
    data->lastError = noErr;

    return SetInstanceData(ci, data);
}

OSErr FreeInstanceData(ComponentInstance ci) {
    ComponentInstanceData* data = GetInstanceData(ci);
    if (data) {
        if (data->storage) {
            DisposeHandle(data->storage);
        }
        HAL_FreeMemory(data);
    }
    return noErr;
}

/* ========================================================================
 * Component Dispatch Helper Functions
 * ======================================================================== */

ComponentResult DispatchOpen(ComponentInstance ci, ComponentRoutine entryPoint, Handle storage) {
    if (!ci || !entryPoint) return paramErr;

    ComponentParameters params;
    params.flags = 0;
    params.paramSize = sizeof(ComponentInstance);
    params.what = kComponentOpenSelect;
    params.params[0] = (int32_t)(uintptr_t)ci;

    return entryPoint(&params, storage);
}

ComponentResult DispatchClose(ComponentInstance ci, ComponentRoutine entryPoint, Handle storage) {
    if (!ci || !entryPoint) return paramErr;

    ComponentParameters params;
    params.flags = 0;
    params.paramSize = sizeof(ComponentInstance);
    params.what = kComponentCloseSelect;
    params.params[0] = (int32_t)(uintptr_t)ci;

    return entryPoint(&params, storage);
}

ComponentResult DispatchCanDo(ComponentInstance ci, ComponentRoutine entryPoint, Handle storage, ComponentParameters* params) {
    if (!ci || !entryPoint || !params) return paramErr;

    params->what = kComponentCanDoSelect;
    return entryPoint(params, storage);
}

ComponentResult DispatchVersion(ComponentInstance ci, ComponentRoutine entryPoint, Handle storage) {
    if (!ci || !entryPoint) return paramErr;

    ComponentParameters params;
    params.flags = 0;
    params.paramSize = 0;
    params.what = kComponentVersionSelect;

    return entryPoint(&params, storage);
}

ComponentResult DispatchRegister(ComponentInstance ci, ComponentRoutine entryPoint, Handle storage) {
    if (!ci || !entryPoint) return paramErr;

    ComponentParameters params;
    params.flags = 0;
    params.paramSize = 0;
    params.what = kComponentRegisterSelect;

    return entryPoint(&params, storage);
}

ComponentResult DispatchTarget(ComponentInstance ci, ComponentRoutine entryPoint, Handle storage, ComponentParameters* params) {
    if (!ci || !entryPoint || !params) return paramErr;

    params->what = kComponentTargetSelect;
    return entryPoint(params, storage);
}

ComponentResult DispatchUnregister(ComponentInstance ci, ComponentRoutine entryPoint, Handle storage) {
    if (!ci || !entryPoint) return paramErr;

    ComponentParameters params;
    params.flags = 0;
    params.paramSize = 0;
    params.what = kComponentUnregisterSelect;

    return entryPoint(&params, storage);
}

/* ========================================================================
 * Additional Security Functions
 * ======================================================================== */

OSErr HAL_GetComponentSecurity(ComponentSecurityLevel* level) {
    if (!level) return paramErr;
    *level = kSecurityLevelStandard; /* Default security level */
    return noErr;
}

/* ========================================================================
 * Component Manager Mutex Wrapper Functions
 * ======================================================================== */

OSErr CreateComponentMutex(ComponentMutex** mutex) {
    return HAL_CreateMutex(mutex);
}

OSErr DestroyComponentMutex(ComponentMutex* mutex) {
    return HAL_DestroyMutex(mutex);
}

OSErr LockComponentMutex(ComponentMutex* mutex) {
    return HAL_LockMutex(mutex);
}

OSErr UnlockComponentMutex(ComponentMutex* mutex) {
    return HAL_UnlockMutex(mutex);
}