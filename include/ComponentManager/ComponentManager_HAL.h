/*
 * ComponentManager_HAL.h
 *
 * Hardware Abstraction Layer for Component Manager - System 7.1 Portable
 * Provides cross-platform support for Component Manager functionality
 */

#ifndef COMPONENTMANAGER_HAL_H
#define COMPONENTMANAGER_HAL_H

#include "ComponentManager.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* Forward include for security types */
#ifndef COMPONENTSECURITY_H
typedef enum {
    kSecurityLevelNone = 0,
    kSecurityLevelMinimal = 1,
    kSecurityLevelStandard = 2,
    kSecurityLevelHigh = 3,
    kSecurityLevelMaximum = 4
} ComponentSecurityLevel;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Additional types needed for HAL */
typedef bool Boolean;
typedef uint32_t Size;

/* Additional error codes */
#define fnfErr                  -43     /* File not found */
#define paramErr                -50     /* Error in parameter list */
#define memFullErr              -108    /* Not enough memory */
#define resNotFound             -192    /* Resource not found */

/* Forward declarations */
typedef struct ComponentInstanceData ComponentInstanceData;
typedef struct ComponentRegistryEntry ComponentRegistryEntry;
typedef struct ComponentMutex ComponentMutex;

/* Threading and synchronization */
OSErr HAL_CreateMutex(ComponentMutex** mutex);
OSErr HAL_DestroyMutex(ComponentMutex* mutex);
OSErr HAL_LockMutex(ComponentMutex* mutex);
OSErr HAL_UnlockMutex(ComponentMutex* mutex);

/* Memory management */
void* HAL_AllocateMemory(size_t size);
void HAL_FreeMemory(void* ptr);
void* HAL_ReallocateMemory(void* ptr, size_t newSize);
void HAL_ZeroMemory(void* ptr, size_t size);
void HAL_CopyMemory(const void* src, void* dst, size_t size);

/* Handle management (Mac OS compatibility) */
Handle NewHandle(Size size);
void DisposeHandle(Handle h);
Size GetHandleSize(Handle h);
OSErr SetHandleSize(Handle h, Size newSize);
void HLock(Handle h);
void HUnlock(Handle h);

/* File system operations */
OSErr HAL_OpenFile(const char* filePath, int32_t* fileRef);
OSErr HAL_CloseFile(int32_t fileRef);
OSErr HAL_ReadFile(int32_t fileRef, void* buffer, size_t bytesToRead, size_t* bytesRead);
OSErr HAL_WriteFile(int32_t fileRef, const void* buffer, size_t bytesToWrite, size_t* bytesWritten);
OSErr HAL_GetFileSize(int32_t fileRef, size_t* fileSize);
OSErr HAL_FileExists(const char* filePath, bool* exists);

/* Dynamic library loading */
OSErr HAL_LoadLibrary(const char* libraryPath, void** libraryHandle);
OSErr HAL_UnloadLibrary(void* libraryHandle);
OSErr HAL_GetProcAddress(void* libraryHandle, const char* functionName, void** functionPtr);

/* Time and date */
uint64_t HAL_GetCurrentTime(void);
OSErr HAL_TimeToString(uint64_t time, char* timeString, size_t maxLength);

/* String utilities */
OSErr HAL_StringCompare(const char* str1, const char* str2, int32_t* result);
OSErr HAL_StringCopy(const char* source, char* destination, size_t maxLength);
OSErr HAL_StringLength(const char* string, size_t* length);

/* Path utilities */
OSErr HAL_GetCurrentDirectory(char* path, size_t maxLength);
OSErr HAL_SetCurrentDirectory(const char* path);
OSErr HAL_GetAbsolutePath(const char* relativePath, char* absolutePath, size_t maxLength);
OSErr HAL_PathExists(const char* path, bool* exists);

/* Process and system information */
OSErr HAL_GetProcessId(uint32_t* processId);
OSErr HAL_GetSystemVersion(uint32_t* majorVersion, uint32_t* minorVersion);
OSErr HAL_GetPlatformInfo(char* platformName, size_t maxLength);

/* Error handling */
OSErr HAL_GetLastError(void);
OSErr HAL_SetLastError(OSErr error);
OSErr HAL_ErrorToString(OSErr error, char* errorString, size_t maxLength);

/* Component resource loading */
OSErr HAL_LoadComponentResource(int16_t refNum, OSType resourceType, int16_t resourceID, Handle* resource);
OSErr HAL_SaveComponentResource(int16_t refNum, OSType resourceType, int16_t resourceID, Handle resource);
OSErr HAL_DeleteComponentResource(int16_t refNum, OSType resourceType, int16_t resourceID);

/* Component validation and security */
OSErr HAL_ValidateComponent(Component component);
OSErr HAL_SetComponentSecurity(ComponentSecurityLevel level);
OSErr HAL_GetComponentSecurity(ComponentSecurityLevel* level);

/* Component instance management functions */
ComponentInstanceData* GetInstanceData(ComponentInstance ci);
OSErr SetInstanceData(ComponentInstance ci, ComponentInstanceData* data);
OSErr AllocateInstanceData(ComponentInstance ci);
OSErr FreeInstanceData(ComponentInstance ci);

/* Component dispatch helper functions */
ComponentResult DispatchOpen(ComponentInstance ci, ComponentRoutine entryPoint, Handle storage);
ComponentResult DispatchClose(ComponentInstance ci, ComponentRoutine entryPoint, Handle storage);
ComponentResult DispatchCanDo(ComponentInstance ci, ComponentRoutine entryPoint, Handle storage, ComponentParameters* params);
ComponentResult DispatchVersion(ComponentInstance ci, ComponentRoutine entryPoint, Handle storage);
ComponentResult DispatchRegister(ComponentInstance ci, ComponentRoutine entryPoint, Handle storage);
ComponentResult DispatchTarget(ComponentInstance ci, ComponentRoutine entryPoint, Handle storage, ComponentParameters* params);
ComponentResult DispatchUnregister(ComponentInstance ci, ComponentRoutine entryPoint, Handle storage);

/* Platform-specific implementations */
#ifdef _WIN32
#include <windows.h>
typedef HANDLE HAL_ThreadMutex;
typedef HMODULE HAL_LibraryHandle;
#define HAL_INVALID_LIBRARY NULL
#elif defined(__APPLE__)
#include <pthread.h>
#include <dlfcn.h>
typedef pthread_mutex_t HAL_ThreadMutex;
typedef void* HAL_LibraryHandle;
#define HAL_INVALID_LIBRARY NULL
#elif defined(__linux__)
#include <pthread.h>
#include <dlfcn.h>
typedef pthread_mutex_t HAL_ThreadMutex;
typedef void* HAL_LibraryHandle;
#define HAL_INVALID_LIBRARY NULL
#else
/* Generic implementation */
typedef void* HAL_ThreadMutex;
typedef void* HAL_LibraryHandle;
#define HAL_INVALID_LIBRARY NULL
#endif

/* Debug and logging support */
#ifdef DEBUG
#define HAL_DebugLog(format, ...) printf("[HAL DEBUG] " format "\n", ##__VA_ARGS__)
#define HAL_ErrorLog(format, ...) printf("[HAL ERROR] " format "\n", ##__VA_ARGS__)
#else
#define HAL_DebugLog(format, ...)
#define HAL_ErrorLog(format, ...)
#endif

/* Utility macros */
#define HAL_ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))
#define HAL_MIN(a, b) ((a) < (b) ? (a) : (b))
#define HAL_MAX(a, b) ((a) > (b) ? (a) : (b))

/* Component Manager initialization helpers */
OSErr HAL_InitializePlatform(void);
void HAL_CleanupPlatform(void);


#ifdef __cplusplus
}
#endif

#endif /* COMPONENTMANAGER_HAL_H */