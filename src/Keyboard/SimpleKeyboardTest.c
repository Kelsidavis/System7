/*
 * Simple Keyboard Control Panel Test
 * Tests basic functionality without complex Handle management
 */

#include "../../include/Keyboard/keyboard_control_panel.h"
#include <stdio.h>
#include <stdlib.h>

/* Simple stub implementations */
Handle NewHandle(long size) {
    void **h = (void**)malloc(sizeof(void*));
    if (h) {
        *h = malloc(size);
        if (!*h) {
            free(h);
            return NULL;
        }
    }
    return (Handle)h;
}

void DisposeHandle(Handle h) {
    if (h) {
        void **handle = (void**)h;
        if (*handle) free(*handle);
        free(handle);
    }
}

void HLock(Handle h) { (void)h; }
void HUnlock(Handle h) { (void)h; }
Ptr NewPtrClear(long size) { return (Ptr)calloc(1, size); }
void DisposPtr(Ptr p) { if (p) free(p); }
OSErr MemError(void) { return noErr; }
OSErr ResError(void) { return noErr; }
void LoadResource(Handle h) { (void)h; }
void ReleaseResource(Handle h) { DisposeHandle(h); }

Handle GetResource(ResType type, short id) {
    (void)type; (void)id;
    return NewHandle(256);
}

int main(void) {
    printf("=== Simple Keyboard Control Panel Test ===\n");

    /* Test basic initialization */
    CdevParam param = {0};
    param.what = initDev;
    param.item = 0;
    param.userData = NULL;

    printf("Testing initDev...\n");
    long result = KeyboardControlPanel_main(&param);
    printf("initDev result: %ld\n", result);

    /* Test cleanup */
    if (param.userData) {
        printf("Testing closeDev...\n");
        param.what = closeDev;
        KeyboardControlPanel_main(&param);
        printf("closeDev completed\n");
    }

    printf("=== Test Complete ===\n");
    return 0;
}