/*
 * Copyright (c) 2024 System7 Project
 * MIT License - See LICENSE file
 */

/*
 * Notepad_HAL.c - Hardware Abstraction Layer for Notepad
 * Provides platform-specific file operations and persistent storage
 */

#include "DeskManager/Notepad.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <Foundation/Foundation.h>
#else
#include <pwd.h>
#endif

#define NOTEPAD_DIR_NAME ".system7"
#define NOTEPAD_FILE "notepad.dat"

/* Platform-specific file paths */
static char gNotePadPath[1024] = {0};

/* Get user's home directory */
static const char* Notepad_HAL_GetHomeDir(void) {
#ifdef __APPLE__
    NSString *homePath = NSHomeDirectory();
    if (homePath) {
        return [homePath UTF8String];
    }
#else
    const char *homeDir = getenv("HOME");
    if (homeDir) {
        return homeDir;
    }

    struct passwd *pw = getpwuid(getuid());
    if (pw) {
        return pw->pw_dir;
    }
#endif

    return "/tmp"; /* Fallback */
}

/* Initialize HAL - create directories if needed */
void Notepad_HAL_Init(void) {
    const char *homeDir = Notepad_HAL_GetHomeDir();

    /* Build notepad directory path */
    snprintf(gNotePadPath, sizeof(gNotePadPath), "%s/%s",
            homeDir, NOTEPAD_DIR_NAME);

    /* Create directory if it doesn't exist */
    struct stat st;
    if (stat(gNotePadPath, &st) != 0) {
        mkdir(gNotePadPath, 0755);
    }

    /* Append filename to path */
    strncat(gNotePadPath, "/", sizeof(gNotePadPath) - strlen(gNotePadPath) - 1);
    strncat(gNotePadPath, NOTEPAD_FILE, sizeof(gNotePadPath) - strlen(gNotePadPath) - 1);
}

/* Load Notepad data from file */
OSErr Notepad_HAL_LoadFile(Handle pageData[NOTEPAD_MAX_PAGES], short *currentPage) {
    FILE *file;
    NotePadFileHeader header;

    /* Ensure HAL is initialized */
    if (gNotePadPath[0] == '\0') {
        Notepad_HAL_Init();
    }

    /* Open file */
    file = fopen(gNotePadPath, "rb");
    if (!file) {
        return fnfErr;
    }

    /* Read header */
    if (fread(&header, sizeof(NotePadFileHeader), 1, file) != 1) {
        fclose(file);
        return ioErr;
    }

    /* Verify signature */
    if (header.signature != NOTEPAD_SIGNATURE) {
        fclose(file);
        return badFileFormat;
    }

    /* Read current page */
    *currentPage = header.currentPage;
    if (*currentPage >= NOTEPAD_MAX_PAGES) {
        *currentPage = 0;
    }

    /* Read each page */
    for (short i = 0; i < NOTEPAD_MAX_PAGES; i++) {
        uint32_t pageSize;

        /* Read page size */
        if (fread(&pageSize, sizeof(uint32_t), 1, file) != 1) {
            break;
        }

        /* Allocate and read page data */
        if (pageSize > 0 && pageSize < 65536) { /* Sanity check */
            char *buffer = (char*)malloc(pageSize + 1);
            if (buffer) {
                if (fread(buffer, 1, pageSize, file) == pageSize) {
                    buffer[pageSize] = '\0';

                    /* Store in handle */
                    SetHandleSize(pageData[i], pageSize + 1);
                    if (MemError() == noErr) {
                        HLock(pageData[i]);
                        memcpy(*pageData[i], buffer, pageSize + 1);
                        HUnlock(pageData[i]);
                    }
                }
                free(buffer);
            } else {
                /* Skip this page */
                fseek(file, pageSize, SEEK_CUR);
            }
        }
    }

    fclose(file);
    return noErr;
}

/* Save Notepad data to file */
OSErr Notepad_HAL_SaveFile(Handle pageData[NOTEPAD_MAX_PAGES], short currentPage) {
    FILE *file;
    NotePadFileHeader header;

    /* Ensure HAL is initialized */
    if (gNotePadPath[0] == '\0') {
        Notepad_HAL_Init();
    }

    /* Open file for writing */
    file = fopen(gNotePadPath, "wb");
    if (!file) {
        return ioErr;
    }

    /* Prepare header */
    memset(&header, 0, sizeof(header));
    header.signature = NOTEPAD_SIGNATURE;
    header.version = 1;
    header.pageCount = NOTEPAD_MAX_PAGES;
    header.currentPage = currentPage;

    /* Write header */
    if (fwrite(&header, sizeof(NotePadFileHeader), 1, file) != 1) {
        fclose(file);
        return ioErr;
    }

    /* Write each page */
    for (short i = 0; i < NOTEPAD_MAX_PAGES; i++) {
        uint32_t pageSize = 0;

        if (pageData[i]) {
            pageSize = GetHandleSize(pageData[i]);
            if (pageSize > 0) {
                pageSize--; /* Don't include null terminator */
            }
        }

        /* Write page size */
        if (fwrite(&pageSize, sizeof(uint32_t), 1, file) != 1) {
            fclose(file);
            return ioErr;
        }

        /* Write page data */
        if (pageSize > 0) {
            HLock(pageData[i]);
            if (fwrite(*pageData[i], 1, pageSize, file) != pageSize) {
                HUnlock(pageData[i]);
                fclose(file);
                return ioErr;
            }
            HUnlock(pageData[i]);
        }
    }

    fclose(file);
    return noErr;
}

/* Get file modification time */
Boolean Notepad_HAL_FileExists(void) {
    struct stat st;

    /* Ensure HAL is initialized */
    if (gNotePadPath[0] == '\0') {
        Notepad_HAL_Init();
    }

    return (stat(gNotePadPath, &st) == 0);
}

/* Delete Notepad file */
OSErr Notepad_HAL_DeleteFile(void) {
    /* Ensure HAL is initialized */
    if (gNotePadPath[0] == '\0') {
        Notepad_HAL_Init();
    }

    if (unlink(gNotePadPath) == 0) {
        return noErr;
    }

    return fnfErr;
}

/* Platform-specific text operations */

/* Convert line endings for platform */
void Notepad_HAL_NormalizeText(char *text, size_t maxLen) {
    if (!text) return;

#ifdef _WIN32
    /* Convert LF to CRLF for Windows */
    char *src = text;
    char *temp = (char*)malloc(maxLen);
    char *dst = temp;
    size_t remaining = maxLen - 1;

    while (*src && remaining > 1) {
        if (*src == '\n' && (src == text || *(src-1) != '\r')) {
            if (remaining > 1) {
                *dst++ = '\r';
                remaining--;
            }
        }
        *dst++ = *src++;
        remaining--;
    }
    *dst = '\0';

    strncpy(text, temp, maxLen);
    free(temp);
#else
    /* Convert CRLF to LF for Unix/Mac */
    char *src = text;
    char *dst = text;

    while (*src) {
        if (*src == '\r' && *(src+1) == '\n') {
            src++; /* Skip CR */
        }
        *dst++ = *src++;
    }
    *dst = '\0';
#endif
}

/* Auto-save support */
static Boolean gAutoSaveEnabled = true;
static uint32_t gAutoSaveInterval = 60; /* seconds */
static uint32_t gLastAutoSave = 0;

/* Enable/disable auto-save */
void Notepad_HAL_SetAutoSave(Boolean enable, uint32_t intervalSeconds) {
    gAutoSaveEnabled = enable;
    if (intervalSeconds > 0) {
        gAutoSaveInterval = intervalSeconds;
    }
}

/* Check if auto-save is needed */
Boolean Notepad_HAL_ShouldAutoSave(void) {
    if (!gAutoSaveEnabled) {
        return false;
    }

    uint32_t currentTime = (uint32_t)time(NULL);
    if (currentTime - gLastAutoSave >= gAutoSaveInterval) {
        gLastAutoSave = currentTime;
        return true;
    }

    return false;
}

/* Get temporary file path for crash recovery */
const char* Notepad_HAL_GetTempPath(void) {
    static char tempPath[1024];
    const char *homeDir = Notepad_HAL_GetHomeDir();

    snprintf(tempPath, sizeof(tempPath), "%s/%s/notepad.tmp",
            homeDir, NOTEPAD_DIR_NAME);

    return tempPath;
}

/* Save temporary backup */
OSErr Notepad_HAL_SaveTemp(Handle pageData[NOTEPAD_MAX_PAGES], short currentPage) {
    FILE *file;
    const char *tempPath = Notepad_HAL_GetTempPath();

    file = fopen(tempPath, "wb");
    if (!file) {
        return ioErr;
    }

    /* Simple format - just dump all pages */
    for (short i = 0; i < NOTEPAD_MAX_PAGES; i++) {
        if (pageData[i]) {
            uint32_t size = GetHandleSize(pageData[i]);
            fwrite(&size, sizeof(uint32_t), 1, file);

            if (size > 0) {
                HLock(pageData[i]);
                fwrite(*pageData[i], 1, size, file);
                HUnlock(pageData[i]);
            }
        } else {
            uint32_t zero = 0;
            fwrite(&zero, sizeof(uint32_t), 1, file);
        }
    }

    /* Write current page at end */
    fwrite(&currentPage, sizeof(short), 1, file);

    fclose(file);
    return noErr;
}

/* Recover from temporary backup */
OSErr Notepad_HAL_RecoverTemp(Handle pageData[NOTEPAD_MAX_PAGES], short *currentPage) {
    FILE *file;
    const char *tempPath = Notepad_HAL_GetTempPath();

    file = fopen(tempPath, "rb");
    if (!file) {
        return fnfErr;
    }

    /* Read all pages */
    for (short i = 0; i < NOTEPAD_MAX_PAGES; i++) {
        uint32_t size;

        if (fread(&size, sizeof(uint32_t), 1, file) != 1) {
            break;
        }

        if (size > 0 && size < 65536) {
            char *buffer = (char*)malloc(size);
            if (buffer) {
                if (fread(buffer, 1, size, file) == size) {
                    SetHandleSize(pageData[i], size);
                    if (MemError() == noErr) {
                        HLock(pageData[i]);
                        memcpy(*pageData[i], buffer, size);
                        HUnlock(pageData[i]);
                    }
                }
                free(buffer);
            }
        }
    }

    /* Read current page */
    fread(currentPage, sizeof(short), 1, file);

    fclose(file);

    /* Delete temp file after recovery */
    unlink(tempPath);

    return noErr;
}