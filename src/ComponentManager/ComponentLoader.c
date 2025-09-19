/*
 * Copyright (c) 2024 System7 Project
 * MIT License - See LICENSE file
 */
/*
 * ComponentLoader.c
 *
 * Component dynamic loading implementation
 * Handles loading components from resources and dynamic libraries
 *
 * Based on Mac OS 7.1 Component Manager
 */

#include "ComponentManager/ComponentLoader.h"
#include "ComponentManager/ComponentRegistry.h"
#include "ComponentManager/ComponentResources.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef __APPLE__
#include <dlfcn.h>
#elif defined(__linux__)
#include <dlfcn.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

/* Component loader state */
typedef struct LoaderState {
    Boolean initialized;
    ComponentMutex* loaderMutex;

    /* Loaded libraries */
    struct LoadedLibrary {
        char path[256];
        void* handle;
        int32_t refCount;
        struct LoadedLibrary* next;
    }* loadedLibraries;

    /* Component search paths */
    char searchPaths[8][256];
    int32_t searchPathCount;
} LoaderState;

static LoaderState g_loaderState = {0};

/* ========================================================================
 * Initialization
 * ======================================================================== */

OSErr InitComponentLoader(void) {
    if (g_loaderState.initialized) {
        return noErr;
    }

    memset(&g_loaderState, 0, sizeof(LoaderState));

    /* Create loader mutex */
    OSErr err = CreateComponentMutex(&g_loaderState.loaderMutex);
    if (err != noErr) {
        return err;
    }

    /* Set default search paths */
    #ifdef __APPLE__
    strcpy(g_loaderState.searchPaths[0], "/Library/Components");
    strcpy(g_loaderState.searchPaths[1], "~/Library/Components");
    strcpy(g_loaderState.searchPaths[2], "./Components");
    g_loaderState.searchPathCount = 3;
    #elif defined(__linux__)
    strcpy(g_loaderState.searchPaths[0], "/usr/lib/components");
    strcpy(g_loaderState.searchPaths[1], "/usr/local/lib/components");
    strcpy(g_loaderState.searchPaths[2], "~/.components");
    strcpy(g_loaderState.searchPaths[3], "./components");
    g_loaderState.searchPathCount = 4;
    #elif defined(_WIN32)
    strcpy(g_loaderState.searchPaths[0], "C:\\Program Files\\Components");
    strcpy(g_loaderState.searchPaths[1], ".\\Components");
    g_loaderState.searchPathCount = 2;
    #endif

    g_loaderState.initialized = true;
    return noErr;
}

void CleanupComponentLoader(void) {
    if (!g_loaderState.initialized) {
        return;
    }

    LockComponentMutex(g_loaderState.loaderMutex);

    /* Unload all libraries */
    struct LoadedLibrary* lib = g_loaderState.loadedLibraries;
    while (lib) {
        struct LoadedLibrary* next = lib->next;
        #ifdef _WIN32
        FreeLibrary((HMODULE)lib->handle);
        #else
        dlclose(lib->handle);
        #endif
        free(lib);
        lib = next;
    }

    g_loaderState.initialized = false;

    UnlockComponentMutex(g_loaderState.loaderMutex);
    DestroyComponentMutex(g_loaderState.loaderMutex);
}

/* ========================================================================
 * Component Discovery
 * ======================================================================== */

OSErr ScanForComponents(void) {
    if (!g_loaderState.initialized) {
        return componentDllLoadErr;
    }

    LockComponentMutex(g_loaderState.loaderMutex);

    /* Scan each search path */
    for (int32_t i = 0; i < g_loaderState.searchPathCount; i++) {
        ScanDirectoryForComponents(g_loaderState.searchPaths[i]);
    }

    UnlockComponentMutex(g_loaderState.loaderMutex);
    return noErr;
}

OSErr ScanDirectoryForComponents(const char* directory) {
    /* Platform-specific directory scanning would go here */
    /* For now, we'll just return success */
    return noErr;
}

/* ========================================================================
 * Dynamic Library Loading
 * ======================================================================== */

OSErr LoadComponentLibrary(const char* path, void** handle) {
    if (!path || !handle) {
        return paramErr;
    }

    LockComponentMutex(g_loaderState.loaderMutex);

    /* Check if already loaded */
    struct LoadedLibrary* lib = g_loaderState.loadedLibraries;
    while (lib) {
        if (strcmp(lib->path, path) == 0) {
            lib->refCount++;
            *handle = lib->handle;
            UnlockComponentMutex(g_loaderState.loaderMutex);
            return noErr;
        }
        lib = lib->next;
    }

    /* Load the library */
    #ifdef _WIN32
    HMODULE libHandle = LoadLibrary(path);
    if (!libHandle) {
        UnlockComponentMutex(g_loaderState.loaderMutex);
        return componentDllLoadErr;
    }
    #else
    void* libHandle = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
    if (!libHandle) {
        UnlockComponentMutex(g_loaderState.loaderMutex);
        return componentDllLoadErr;
    }
    #endif

    /* Add to loaded libraries list */
    lib = malloc(sizeof(struct LoadedLibrary));
    if (!lib) {
        #ifdef _WIN32
        FreeLibrary(libHandle);
        #else
        dlclose(libHandle);
        #endif
        UnlockComponentMutex(g_loaderState.loaderMutex);
        return memFullErr;
    }

    strncpy(lib->path, path, sizeof(lib->path) - 1);
    lib->handle = libHandle;
    lib->refCount = 1;
    lib->next = g_loaderState.loadedLibraries;
    g_loaderState.loadedLibraries = lib;

    *handle = libHandle;

    UnlockComponentMutex(g_loaderState.loaderMutex);
    return noErr;
}

OSErr UnloadComponentLibrary(void* handle) {
    if (!handle) {
        return paramErr;
    }

    LockComponentMutex(g_loaderState.loaderMutex);

    /* Find the library */
    struct LoadedLibrary** prev = &g_loaderState.loadedLibraries;
    struct LoadedLibrary* lib = g_loaderState.loadedLibraries;

    while (lib) {
        if (lib->handle == handle) {
            lib->refCount--;
            if (lib->refCount <= 0) {
                /* Unload the library */
                *prev = lib->next;
                #ifdef _WIN32
                FreeLibrary((HMODULE)lib->handle);
                #else
                dlclose(lib->handle);
                #endif
                free(lib);
            }
            UnlockComponentMutex(g_loaderState.loaderMutex);
            return noErr;
        }
        prev = &lib->next;
        lib = lib->next;
    }

    UnlockComponentMutex(g_loaderState.loaderMutex);
    return componentDllLoadErr;
}

/* ========================================================================
 * Component Entry Point Resolution
 * ======================================================================== */

OSErr GetComponentEntryPoint(void* handle, const char* symbolName, ComponentRoutine* entryPoint) {
    if (!handle || !symbolName || !entryPoint) {
        return paramErr;
    }

    #ifdef _WIN32
    FARPROC proc = GetProcAddress((HMODULE)handle, symbolName);
    if (!proc) {
        return componentDllEntryErr;
    }
    *entryPoint = (ComponentRoutine)proc;
    #else
    void* proc = dlsym(handle, symbolName);
    if (!proc) {
        return componentDllEntryErr;
    }
    *entryPoint = (ComponentRoutine)proc;
    #endif

    return noErr;
}

/* ========================================================================
 * Resource-Based Component Loading
 * ======================================================================== */

OSErr LoadComponentFromResource(Handle resourceHandle, ComponentRoutine* entryPoint) {
    if (!resourceHandle || !entryPoint) {
        return paramErr;
    }

    /* Parse component resource */
    ComponentResource* resource;
    OSErr err = ParseComponentResource(resourceHandle, &resource);
    if (err != noErr) {
        return err;
    }

    /* For resource-based components, we need to load the CODE resource */
    /* This is a simplified implementation */
    *entryPoint = NULL;  /* Would load actual code here */

    free(resource);
    return noErr;
}

/* ========================================================================
 * Component File Loading
 * ======================================================================== */

OSErr LoadComponentFile(const char* filePath, ComponentDescription* desc, ComponentRoutine* entryPoint) {
    if (!filePath || !desc || !entryPoint) {
        return paramErr;
    }

    /* Try to load as dynamic library first */
    void* handle;
    OSErr err = LoadComponentLibrary(filePath, &handle);
    if (err == noErr) {
        /* Get the component entry point */
        err = GetComponentEntryPoint(handle, "ComponentEntryPoint", entryPoint);
        if (err == noErr) {
            /* Get component description */
            ComponentRoutine getDescProc;
            err = GetComponentEntryPoint(handle, "GetComponentDescription", &getDescProc);
            if (err == noErr) {
                /* Call to get description */
                ComponentParameters params;
                params.what = kComponentVersionSelect;
                params.paramSize = sizeof(ComponentDescription);
                getDescProc(&params, NULL);
                memcpy(desc, &params.params[0], sizeof(ComponentDescription));
            }
        }

        if (err != noErr) {
            UnloadComponentLibrary(handle);
        }
        return err;
    }

    /* Try to load as resource file */
    /* This would involve parsing resource fork */
    return componentDllLoadErr;
}

/* ========================================================================
 * Component Registration from File
 * ======================================================================== */

OSErr RegisterComponentFromFile(const char* filePath, Boolean global) {
    ComponentDescription desc;
    ComponentRoutine entryPoint;

    OSErr err = LoadComponentFile(filePath, &desc, &entryPoint);
    if (err != noErr) {
        return err;
    }

    /* Register the component */
    Component comp = RegisterComponent(&desc, entryPoint, global ? 0 : 1, NULL, NULL, NULL);
    if (!comp) {
        return componentNotFound;
    }

    return noErr;
}

/* ========================================================================
 * Search Path Management
 * ======================================================================== */

OSErr AddComponentSearchPath(const char* path) {
    if (!path) {
        return paramErr;
    }

    LockComponentMutex(g_loaderState.loaderMutex);

    if (g_loaderState.searchPathCount >= 8) {
        UnlockComponentMutex(g_loaderState.loaderMutex);
        return memFullErr;
    }

    strncpy(g_loaderState.searchPaths[g_loaderState.searchPathCount],
            path, sizeof(g_loaderState.searchPaths[0]) - 1);
    g_loaderState.searchPathCount++;

    UnlockComponentMutex(g_loaderState.loaderMutex);
    return noErr;
}

OSErr RemoveComponentSearchPath(const char* path) {
    if (!path) {
        return paramErr;
    }

    LockComponentMutex(g_loaderState.loaderMutex);

    for (int32_t i = 0; i < g_loaderState.searchPathCount; i++) {
        if (strcmp(g_loaderState.searchPaths[i], path) == 0) {
            /* Remove this path */
            for (int32_t j = i; j < g_loaderState.searchPathCount - 1; j++) {
                strcpy(g_loaderState.searchPaths[j], g_loaderState.searchPaths[j + 1]);
            }
            g_loaderState.searchPathCount--;
            UnlockComponentMutex(g_loaderState.loaderMutex);
            return noErr;
        }
    }

    UnlockComponentMutex(g_loaderState.loaderMutex);
    return componentNotFound;
}

void GetComponentSearchPaths(char paths[][256], int32_t* count) {
    if (!paths || !count) {
        return;
    }

    LockComponentMutex(g_loaderState.loaderMutex);

    *count = g_loaderState.searchPathCount;
    for (int32_t i = 0; i < g_loaderState.searchPathCount; i++) {
        strcpy(paths[i], g_loaderState.searchPaths[i]);
    }

    UnlockComponentMutex(g_loaderState.loaderMutex);
}