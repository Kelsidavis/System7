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

#include "ComponentManager/ComponentManager.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

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

OSErr CreateComponentMutex(ComponentMutex** mutex) {
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

void DestroyComponentMutex(ComponentMutex* mutex) {
    if (!mutex || !mutex->initialized) return;

    pthread_mutex_destroy(&mutex->mutex);
    mutex->initialized = false;
    free(mutex);
}

void LockComponentMutex(ComponentMutex* mutex) {
    if (!mutex || !mutex->initialized) return;
    pthread_mutex_lock(&mutex->mutex);
}

void UnlockComponentMutex(ComponentMutex* mutex) {
    if (!mutex || !mutex->initialized) return;
    pthread_mutex_unlock(&mutex->mutex);
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

OSErr HAL_GetPlatformInfo(ComponentPlatformInfo* info) {
    if (!info) return paramErr;

    memset(info, 0, sizeof(ComponentPlatformInfo));

#ifdef __APPLE__
    #ifdef __arm64__
    info->platformType = gestaltPowerPC;  /* Using PowerPC constant for ARM */
    #else
    info->platformType = gestalt68k;      /* Using 68k constant for x86 */
    #endif
#elif defined(__linux__)
    #ifdef __aarch64__
    info->platformType = gestaltPowerPC;
    #else
    info->platformType = gestalt68k;
    #endif
#elif defined(_WIN32)
    #ifdef _M_ARM64
    info->platformType = gestaltPowerPC;
    #else
    info->platformType = gestalt68k;
    #endif
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