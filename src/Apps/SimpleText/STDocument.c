/*
 * STDocument.c - SimpleText Document Management
 *
 * Handles document windows, file associations, and dirty state
 */

#include <string.h>
#include "Apps/SimpleText.h"
#include "MemoryMgr/MemoryManager.h"
#include "System/SystemLogging.h"

/* Static variables */
static int g_untitledCount = 1;

/* Static helper functions */
static void AddDocumentToList(STDocument* doc);
static void RemoveDocumentFromList(STDocument* doc);
static void BuildWindowTitle(STDocument* doc, Str255 title);

/*
 * STDoc_New - Create new untitled document
 */
STDocument* STDoc_New(void) {
    STDocument* doc;
    Rect windowBounds;
    Str255 title;

    ST_Log("Creating new document\n");

    /* Allocate document structure */
    doc = (STDocument*)NewPtr(sizeof(STDocument));
    if (!doc) {
        ST_Log("Failed to allocate document\n");
        return NULL;
    }

    /* Initialize document fields */
    memset(doc, 0, sizeof(STDocument));
    doc->dirty = false;
    doc->untitled = true;
    doc->fileType = 'TEXT';
    doc->fileCreator = 'ttxt';

    /* Build untitled name */
    if (g_untitledCount == 1) {
        memcpy((char*)&doc->fileName[1], "Untitled", 8);
        doc->fileName[0] = 8;
    } else {
        char numStr[16];
        int len, i;
        memcpy((char*)&doc->fileName[1], "Untitled ", 9);

        /* Convert number to string manually */
        i = g_untitledCount;
        len = 0;
        do {
            numStr[len++] = '0' + (i % 10);
            i /= 10;
        } while (i > 0);

        /* Append reversed number */
        for (i = 0; i < len; i++) {
            doc->fileName[9 + i + 1] = numStr[len - i - 1];
        }
        doc->fileName[0] = 9 + len;
    }
    g_untitledCount++;

    /* Create window */
    SetRect(&windowBounds, 100 + (g_ST.firstDoc ? 20 : 0),
            100 + (g_ST.firstDoc ? 20 : 0), 500, 400);

    BuildWindowTitle(doc, title);
    doc->window = NewWindow(NULL, &windowBounds, title, true,
                           documentProc, (WindowPtr)-1L, true, (long)doc);

    if (!doc->window) {
        ST_Log("Failed to create window\n");
        DisposePtr((Ptr)doc);
        return NULL;
    }

    /* Create TextEdit view */
    STView_Create(doc);

    /* Add to document list */
    AddDocumentToList(doc);

    /* Make it active */
    STDoc_Activate(doc);

    ST_Log("Created new document: %s\n", doc->fileName);
    return doc;
}

/*
 * STDoc_Open - Open existing document
 */
STDocument* STDoc_Open(const char* path) {
    STDocument* doc;
    Rect windowBounds;
    Str255 title;
    const char* filename;

    ST_Log("Opening document: %s\n", path);

    /* Allocate document structure */
    doc = (STDocument*)NewPtr(sizeof(STDocument));
    if (!doc) {
        return NULL;
    }

    /* Initialize document fields */
    memset(doc, 0, sizeof(STDocument));
    doc->dirty = false;
    doc->untitled = false;
    doc->fileType = 'TEXT';
    doc->fileCreator = 'ttxt';

    /* Safe path copy with bounds checking */
    if (strlen(path) >= sizeof(doc->filePath)) {
        DisposePtr((Ptr)doc);
        return NULL;  /* Path too long */
    }
    strncpy(doc->filePath, path, sizeof(doc->filePath) - 1);
    doc->filePath[sizeof(doc->filePath) - 1] = '\0';

    /* Extract filename from path */
    filename = path;
    const char* p = path;
    while (*p) {
        if (*p == '/') filename = p + 1;
        p++;
    }

    /* Build Pascal filename */
    {
        int len = strlen(filename);
        if (len > 255) len = 255;
        doc->fileName[0] = len;
        memcpy(&doc->fileName[1], filename, len);

        /* Debug: log the extracted filename */
        serial_logf(kLogModuleWindow, kLogLevelDebug,
                   "[STDOC] Extracted filename: '%s' len=%d\n", filename, len);
    }

    /* Create window */
    SetRect(&windowBounds, 120, 120, 520, 420);
    BuildWindowTitle(doc, title);
    doc->window = NewWindow(NULL, &windowBounds, title, true,
                           documentProc, (WindowPtr)-1L, true, (long)doc);

    if (!doc->window) {
        DisposePtr((Ptr)doc);
        return NULL;
    }

    /* Create TextEdit view */
    STView_Create(doc);

    /* Load file content */
    if (!STIO_ReadFile(doc, path)) {
        /* Failed to read file */
        DisposeWindow(doc->window);
        DisposePtr((Ptr)doc);
        return NULL;
    }

    /* Add to document list */
    AddDocumentToList(doc);

    /* Make it active */
    STDoc_Activate(doc);
    STView_Draw(doc);

    ST_Log("Opened document successfully\n");
    return doc;
}

/*
 * STDoc_Close - Close document
 */
void STDoc_Close(STDocument* doc) {
    if (!doc) return;

    ST_Log("Closing document: %s\n", doc->fileName);

    /* Check for unsaved changes */
    if (doc->dirty) {
        if (!ST_ConfirmClose(doc)) {
            return;  /* User cancelled */
        }
    }

    /* Deactivate if active */
    if (g_ST.activeDoc == doc) {
        STDoc_Deactivate(doc);
        g_ST.activeDoc = NULL;
    }

    /* Dispose TextEdit view */
    STView_Dispose(doc);

    /* Remove from document list */
    RemoveDocumentFromList(doc);

    /* Dispose window */
    if (doc->window) {
        DisposeWindow(doc->window);
    }

    /* Dispose undo buffer */
    if (doc->undoText) {
        DisposeHandle(doc->undoText);
    }

    /* Dispose style runs */
    if (doc->styles.hRuns) {
        DisposeHandle(doc->styles.hRuns);
    }

    /* Free document structure */
    DisposePtr((Ptr)doc);

    /* If no more documents, create new untitled */
    if (!g_ST.firstDoc && g_ST.running) {
        STDoc_New();
    }
}

/*
 * STDoc_Save - Save document
 */
void STDoc_Save(STDocument* doc) {
    if (!doc) return;

    ST_Log("Saving document: %s\n", doc->fileName);

    if (doc->untitled) {
        /* Need Save As dialog */
        STDoc_SaveAs(doc);
        return;
    }

    /* Save to existing file */
    if (STIO_WriteFile(doc, doc->filePath)) {
        doc->dirty = false;
        doc->lastSaveLen = TEGetText(doc->hTE) ? GetHandleSize(TEGetText(doc->hTE)) : 0;
        STDoc_UpdateTitle(doc);
        ST_Log("Document saved successfully\n");
    } else {
        ST_ErrorAlert("Could not save file");
    }
}

/*
 * STDoc_SaveAs - Save document with new name
 */
void STDoc_SaveAs(STDocument* doc) {
    char newPath[512];

    if (!doc) return;

    ST_Log("Save As for document: %s\n", doc->fileName);

    /* Get new filename from user */
    if (!STIO_SaveDialog(doc, newPath)) {
        return;  /* User cancelled */
    }

    /* Update document info - safe path copy */
    if (strlen(newPath) >= sizeof(doc->filePath)) {
        return;  /* Path too long */
    }
    strncpy(doc->filePath, newPath, sizeof(doc->filePath) - 1);
    doc->filePath[sizeof(doc->filePath) - 1] = '\0';
    doc->untitled = false;

    /* Extract new filename */
    {
        const char* filename = newPath;
        const char* p = newPath;
        while (*p) {
            if (*p == '/') filename = p + 1;
            p++;
        }

        int len = strlen(filename);
        if (len > 255) len = 255;
        doc->fileName[0] = len;
        memcpy(&doc->fileName[1], filename, len);
    }

    /* Save to new file */
    if (STIO_WriteFile(doc, newPath)) {
        doc->dirty = false;
        doc->lastSaveLen = TEGetText(doc->hTE) ? GetHandleSize(TEGetText(doc->hTE)) : 0;
        STDoc_UpdateTitle(doc);
        ST_Log("Document saved as: %s\n", newPath);
    } else {
        ST_ErrorAlert("Could not save file");
    }
}

/*
 * STDoc_SetDirty - Set document dirty flag
 */
void STDoc_SetDirty(STDocument* doc, Boolean dirty) {
    if (!doc || doc->dirty == dirty) return;

    doc->dirty = dirty;
    STDoc_UpdateTitle(doc);

    /* Update menu state */
    STMenu_Update();
}

/*
 * STDoc_UpdateTitle - Update window title with dirty indicator
 */
void STDoc_UpdateTitle(STDocument* doc) {
    Str255 title;

    if (!doc || !doc->window) return;

    BuildWindowTitle(doc, title);
    SetWTitle(doc->window, title);
}

/*
 * STDoc_FindByWindow - Find document by window
 */
STDocument* STDoc_FindByWindow(WindowPtr window) {
    STDocument* doc;

    for (doc = g_ST.firstDoc; doc; doc = doc->next) {
        if (doc->window == window) {
            return doc;
        }
    }
    return NULL;
}

/*
 * STDoc_Activate - Activate document
 */
void STDoc_Activate(STDocument* doc) {
    if (!doc) return;

    ST_Log("Activating document: %s\n", doc->fileName);

    /* Set as active document */
    g_ST.activeDoc = doc;

    /* Activate TextEdit */
    if (doc->hTE) {
        TEActivate(doc->hTE);
    }

    /* Update menus */
    STMenu_Update();
}

/*
 * STDoc_Deactivate - Deactivate document
 */
void STDoc_Deactivate(STDocument* doc) {
    if (!doc) return;

    ST_Log("Deactivating document: %s\n", doc->fileName);

    /* Deactivate TextEdit */
    if (doc->hTE) {
        TEDeactivate(doc->hTE);
    }

    /* Clear active if this was it */
    if (g_ST.activeDoc == doc) {
        g_ST.activeDoc = NULL;
    }
}

/*
 * AddDocumentToList - Add document to linked list
 */
static void AddDocumentToList(STDocument* doc) {
    doc->next = g_ST.firstDoc;
    g_ST.firstDoc = doc;
}

/*
 * RemoveDocumentFromList - Remove document from linked list
 */
static void RemoveDocumentFromList(STDocument* doc) {
    STDocument** pp;

    for (pp = &g_ST.firstDoc; *pp; pp = &(*pp)->next) {
        if (*pp == doc) {
            *pp = doc->next;
            break;
        }
    }
}

/*
 * BuildWindowTitle - Build window title with dirty indicator
 */
static void BuildWindowTitle(STDocument* doc, Str255 title) {
    int i, j;

    /* Start with empty title */
    j = 0;

    /* Add bullet if dirty */
    if (doc->dirty) {
        title[++j] = 0xA5;  /* Bullet character in Mac Roman */
        title[++j] = ' ';
    }

    /* Add filename */
    for (i = 1; i <= doc->fileName[0]; i++) {
        title[++j] = doc->fileName[i];
    }

    /* Set length */
    title[0] = j;

    /* Debug: log the built title */
    {
        char debugTitle[256];
        memcpy(debugTitle, &title[1], title[0]);
        debugTitle[title[0]] = '\0';
        serial_logf(kLogModuleWindow, kLogLevelDebug,
                   "[STDOC] BuildWindowTitle: '%s' len=%d dirty=%d\n",
                   debugTitle, j, doc->dirty);
    }
}
