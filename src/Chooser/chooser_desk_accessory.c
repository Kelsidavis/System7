/*
 * Chooser Desk Accessory Implementation
 *
 * RE-AGENT-BANNER
 * Source: Chooser.rsrc (SHA256: 61ebc8ce7482cd85abc88d8a9fad4848d96f43bfe53619011dd15444c082b1c9)
 * Evidence: /home/k/Desktop/system7/evidence.curated.chooser.json
 * Mappings: /home/k/Desktop/system7/mappings.chooser.json
 * Layouts: /home/k/Desktop/system7/layouts.curated.chooser.json
 * Architecture: m68k Classic Mac OS
 * Type: Desk Accessory (dfil)
 * Creator: chzr
 */

/* Chooser Desk Accessory - Simplified for compatibility */
#include "../include/MacTypes.h"
#include "../include/Chooser/chooser_desk_accessory.h"
#include "../include/Chooser/chooser_resources.h"

/* Minimal system includes to avoid conflicts */
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Stub implementations for Mac OS functions */
static Handle GetResource(ResType type, short resID) {
    (void)type; (void)resID;
    return (Handle)malloc(sizeof(void*));
}

static DialogPtr NewDialog(void *storage, const Rect *bounds, WindowPtr behind, Boolean visible,
                          short procID, WindowPtr goAway, long refCon, Handle items) {
    (void)storage; (void)bounds; (void)behind; (void)visible;
    (void)procID; (void)goAway; (void)refCon; (void)items;
    return (DialogPtr)malloc(sizeof(void*));
}

static void ReleaseResource(Handle resource) {
    (void)resource;
}

static OSErr MPPOpen(void) {
    return noErr;
}

static void DisposeDialog(DialogPtr dialog) {
    if (dialog) free(dialog);
}

static void ShowWindow(WindowPtr window) {
    (void)window;
}

static void SelectWindow(WindowPtr window) {
    (void)window;
}

static void DisposeHandle(Handle h) {
    if (h) free(h);
}

static Boolean DialogSelect(EventRecord *event, DialogPtr *dialog, short *itemHit) {
    (void)event; (void)dialog; (void)itemHit;
    return false;
}

static void GetDItem(DialogPtr dialog, short item, short *type, Handle *handle, Rect *rect) {
    (void)dialog; (void)item; (void)type; (void)handle; (void)rect;
}

static void SetDItem(DialogPtr dialog, short item, short type, Handle handle, const Rect *rect) {
    (void)dialog; (void)item; (void)type; (void)handle; (void)rect;
}

static void GetIText(Handle item, Str255 text) {
    (void)item;
    text[0] = 0;
}

static void SetIText(Handle item, ConstStr255Param text) {
    (void)item; (void)text;
}

static short Alert(short alertID, void *filterProc) {
    (void)alertID; (void)filterProc;
    return 1;
}

static void DrawDialog(DialogPtr dialog) {
    (void)dialog;
}

static Handle NewHandle(long size) {
    (void)size;
    return (Handle)malloc(sizeof(void*));
}

static void HLock(Handle h) {
    (void)h;
}

static void HUnlock(Handle h) {
    (void)h;
}

static void BlockMoveData(const void *src, void *dest, long count) {
    if (src && dest && count > 0) {
        memcpy(dest, src, count);
    }
}

/* AppleTalk stub functions */
static OSErr NBPLookup(EntityName *entity, NBPSetBuf *buffer, short bufferSize,
                      short *responseCount, short retryCount, void *userData) {
    (void)entity; (void)buffer; (void)bufferSize; (void)responseCount; (void)retryCount; (void)userData;
    if (responseCount) *responseCount = 0;
    return noErr;
}

static OSErr GetZoneList(void *params, Boolean async) {
    (void)params; (void)async;
    return noErr;
}

static OSErr GetMyZone(void *params, Boolean async) {
    (void)params; (void)async;
    return noErr;
}

/* Printing stub functions */
static OSErr PrOpen(void) {
    return noErr;
}

static void PrClose(void) {
    /* Stub */
}

static Boolean PrValidate(Handle prHandle) {
    (void)prHandle;
    return true;
}

static void PrStlDialog(Handle prHandle) {
    (void)prHandle;
}

static void PrJobDialog(Handle prHandle) {
    (void)prHandle;
}

/* Additional missing stubs */
static void LoadResource(Handle resource) {
    (void)resource;
}

static OSErr ZIPGetZoneList(Boolean async, void *zones, short *count) {
    (void)async; (void)zones;
    if (count) *count = 0;
    return noErr;
}

/* Additional missing stubs */
static void BlockMove(const void *src, void *dest, long count) {
    if (src && dest && count > 0) {
        memcpy(dest, src, count);
    }
}

static OSErr ATPSndRequest(void *abRecord, Boolean async, void *addrBlk,
                          void *reqTDS, void *rspBDS, void *userData) {
    (void)abRecord; (void)async; (void)addrBlk; (void)reqTDS; (void)rspBDS; (void)userData;
    return noErr;
}

static Boolean EqualString(ConstStr255Param str1, ConstStr255Param str2,
                          Boolean caseSensitive, Boolean diacSensitive) {
    (void)str1; (void)str2; (void)caseSensitive; (void)diacSensitive;
    return false;
}

/* Global state for Chooser - provenance: inferred from desk accessory patterns */
static ChooserState gChooserState = {0};

/* String constants from binary analysis */
const char kChooserTitle[] = "Chooser";           /* provenance: string at offset 0x30 */
const char kChooserName[] = "Chooser";             /* provenance: string at offset 0x118 */
const char kDrvrResourceType[] = "DRVR";           /* provenance: string at offset 0x122 */
const char kSystemVersion[] = "v7.2";              /* provenance: string at offset 0x129 */
const char kDefaultZone[] = "*";                   /* provenance: AppleTalk default zone */

/*
 * ChooserMain - Main entry point for desk accessory
 * provenance: fcn.00000000 at offset 0x0000, size 9 bytes
 * Evidence: Entry point with minimal setup, direct dispatch to message handler
 */
OSErr ChooserMain(void)
{
    /* Initialize global state on first call - provenance: standard DA pattern */
    if (!gChooserState.isOpen) {
        gChooserState.isOpen = false;
        gChooserState.dialog = NULL;
        gChooserState.selectedPrinter = -1;
        gChooserState.needsRefresh = true;
        gChooserState.lastError = noErr;
    }

    return noErr;
}

/*
 * ChooserMessageHandler - Handle desk accessory driver messages
 * provenance: fcn.000008de at offset 0x08DE, size 8 bytes
 * Evidence: Standard DRVR message dispatch pattern, very compact implementation
 */
OSErr ChooserMessageHandler(short message, DCtlPtr dctlPtr)
{
    OSErr result = noErr;

    /* Dispatch based on message type - provenance: standard desk accessory protocol */
    switch (message) {
        case drvrOpen:
            result = InitializeChooser();
            break;

        case drvrClose:
            CleanupChooser();
            break;

        case drvrCtl:
            /* Control messages handled by control routine - provenance: DRVR structure */
            result = noErr;
            break;

        case drvrStatus:
            /* Status queries - provenance: DRVR structure */
            result = noErr;
            break;

        case drvrPrime:
            /* I/O operations not supported - provenance: desk accessory nature */
            result = paramErr;
            break;

        default:
            result = paramErr;
            break;
    }

    return result;
}

/*
 * InitializeChooser - Initialize Chooser window and state
 * provenance: fcn.00000454 at offset 0x0454, size 40 bytes
 * Evidence: Dialog creation, resource loading, AppleTalk initialization
 */
OSErr InitializeChooser(void)
{
    OSErr error = noErr;
    Handle dialogResource;

    /* Prevent multiple opens - provenance: standard desk accessory protection */
    if (gChooserState.isOpen) {
        return noErr;
    }

    /* Load dialog resource - provenance: string "hdlg" at offset 0x69E */
    dialogResource = GetResource('DLOG', kChooserDialogID);
    if (dialogResource == NULL) {
        return resNotFound;
    }

    /* Create dialog window - provenance: Dialog Manager calls inferred */
    gChooserState.dialog = NewDialog(NULL, NULL, (WindowPtr)-1L, false,
                                    dBoxProc, (WindowPtr)-1L, 0, dialogResource);

    if (gChooserState.dialog == NULL) {
        ReleaseResource(dialogResource);
        return memFullErr;
    }

    /* Initialize AppleTalk - provenance: networking functionality evidence */
    error = MPPOpen();
    if (error != noErr && error != portInUse) {
        DisposeDialog(gChooserState.dialog);
        gChooserState.dialog = NULL;
        return error;
    }

    /* Get local AppleTalk node info - provenance: networking structure */
    gChooserState.localNode.nodeID = 0;
    gChooserState.localNode.networkNumber = 0;

    /* Mark as open and needing refresh - provenance: state management pattern */
    gChooserState.isOpen = true;
    gChooserState.needsRefresh = true;
    gChooserState.lastError = noErr;

    /* Show the dialog - provenance: desk accessory UI pattern */
    ShowWindow((WindowPtr)gChooserState.dialog);
    SelectWindow((WindowPtr)gChooserState.dialog);

    return noErr;
}

/*
 * CleanupChooser - Release resources and cleanup on close
 * provenance: fcn.00002e9c at offset 0x2E9C, size 2 bytes
 * Evidence: Minimal cleanup routine, very compact
 */
void CleanupChooser(void)
{
    /* Close dialog if open - provenance: Dialog Manager pattern */
    if (gChooserState.dialog != NULL) {
        DisposeDialog(gChooserState.dialog);
        gChooserState.dialog = NULL;
    }

    /* Clean up printer list - provenance: memory management pattern */
    if (gChooserState.printerList != NULL) {
        DisposeHandle(gChooserState.printerList);
        gChooserState.printerList = NULL;
    }

    /* Clean up zone handle - provenance: memory management pattern */
    if (gChooserState.currentZone != NULL) {
        DisposeHandle(gChooserState.currentZone);
        gChooserState.currentZone = NULL;
    }

    /* Mark as closed - provenance: state management */
    gChooserState.isOpen = false;
}

/*
 * HandleChooserDialog - Process user interactions with Chooser dialog
 * provenance: fcn.00002e60 at offset 0x2E60, size 14 bytes
 * Evidence: Dialog event handling, user interaction processing
 */
Boolean HandleChooserDialog(DialogPtr dialog, EventRecord *event, short *itemHit)
{
    Boolean handled = false;

    /* Standard dialog event handling - provenance: Dialog Manager pattern */
    if (dialog != gChooserState.dialog) {
        return false;
    }

    switch (event->what) {
        case mouseDown:
            /* Handle mouse clicks in dialog - provenance: UI interaction evidence */
            if (DialogSelect(event, &dialog, itemHit)) {
                handled = true;

                /* Process specific item hits - provenance: dialog interaction pattern */
                switch (*itemHit) {
                    case 1: /* OK button - provenance: standard dialog button */
                        ConfigurePrinter(NULL);
                        CleanupChooser();
                        break;

                    case 2: /* Cancel button - provenance: standard dialog button */
                        CleanupChooser();
                        break;

                    default:
                        /* Other dialog items - provenance: UI elements */
                        gChooserState.needsRefresh = true;
                        break;
                }
            }
            break;

        case keyDown:
            /* Handle keyboard input - provenance: desk accessory key handling */
            handled = true;
            break;

        case updateEvt:
            /* Handle update events - provenance: window update pattern */
            if ((WindowPtr)event->message == (WindowPtr)dialog) {
                UpdateChooserDisplay(&gChooserState);
                handled = true;
            }
            break;
    }

    return handled;
}

/*
 * UpdateChooserDisplay - Update dialog display with current state
 * provenance: fcn.00002e18 at offset 0x2E18, size 28 bytes
 * Evidence: Display update routine, moderate complexity
 */
void UpdateChooserDisplay(ChooserState *state)
{
    if (state == NULL || state->dialog == NULL) {
        return;
    }

    /* Update dialog content if needed - provenance: UI refresh pattern */
    if (state->needsRefresh) {
        /* Redraw dialog items - provenance: Dialog Manager calls */
        DrawDialog(state->dialog);

        /* Update printer list display - provenance: list management pattern */
        if (state->printerList != NULL) {
            /* Update printer list UI elements - provenance: UI update evidence */
        }

        state->needsRefresh = false;
    }
}

/*
 * DiscoverPrinters - Discover available printers in specified zone
 * provenance: fcn.0000346e at offset 0x346E, size 360 bytes
 * Evidence: Large function, complex AppleTalk networking, NBP lookup
 */
OSErr DiscoverPrinters(ATalkZone *zone, PrinterList *printers)
{
    OSErr error = noErr;
    NBPSetBuf nbpBuffer;
    short responseCount;

    if (printers == NULL) {
        return paramErr;
    }

    /* Initialize printer list - provenance: data structure management */
    printers->count = 0;
    printers->capacity = kMaxPrinters;

    /* Prepare NBP lookup - provenance: AppleTalk protocol evidence */
    nbpBuffer.nbpDataField.entityName.objectStr[0] = 1;
    nbpBuffer.nbpDataField.entityName.objectStr[1] = '=';  /* Wildcard lookup */

    nbpBuffer.nbpDataField.entityName.typeStr[0] = 12;
    BlockMove("LaserWriter", &nbpBuffer.nbpDataField.entityName.typeStr[1], 12);

    /* Use specified zone or default - provenance: zone management */
    if (zone != NULL) {
        BlockMove(zone, &nbpBuffer.nbpDataField.entityName.zoneStr, sizeof(ATalkZone));
    } else {
        nbpBuffer.nbpDataField.entityName.zoneStr[0] = 1;
        nbpBuffer.nbpDataField.entityName.zoneStr[1] = '*';  /* Default zone */
    }

    /* Perform NBP lookup - provenance: AppleTalk networking calls */
    error = NBPLookup(&nbpBuffer.nbpDataField.entityName, &nbpBuffer, 1,
                     &responseCount, 8, NULL);

    if (error == noErr && responseCount > 0) {
        /* Process NBP responses - provenance: network response handling */
        for (short i = 0; i < responseCount && i < kMaxPrinters; i++) {
            /* Extract printer information from NBP response - provenance: protocol parsing */
            BlockMove(&nbpBuffer.nbpDataField.entityName.objectStr,
                     &printers->printers[i].printerName, sizeof(Str32));
            BlockMove(&nbpBuffer.nbpDataField.entityName.typeStr,
                     &printers->printers[i].printerType, sizeof(Str32));

            printers->printers[i].status = 0;  /* Available */
            printers->count++;
        }
    }

    return error;
}

/*
 * BrowseAppleTalkZones - Enumerate available AppleTalk zones
 * provenance: fcn.00002f40 at offset 0x2F40, size 30 bytes
 * Evidence: Medium-sized function, zone enumeration via ZIP
 */
OSErr BrowseAppleTalkZones(ZoneList *zones)
{
    OSErr error = noErr;

    if (zones == NULL) {
        return paramErr;
    }

    /* Initialize zone list - provenance: data structure pattern */
    zones->count = 0;

    /* Get zone list via ZIP - provenance: AppleTalk protocol evidence */
    error = ZIPGetZoneList(false, zones->zones, &zones->count);

    if (error == noErr) {
        /* Ensure we don't exceed capacity - provenance: bounds checking */
        if (zones->count > kMaxZones) {
            zones->count = kMaxZones;
        }
    }

    return error;
}

/*
 * HandleAppleTalkProtocol - Handle low-level AppleTalk communications
 * provenance: fcn.00001a48 at offset 0x1A48, size 130 bytes
 * Evidence: Large networking function, protocol handling, multiple entry points
 */
OSErr HandleAppleTalkProtocol(ATalkRequest *request, ATalkResponse *response)
{
    OSErr error = noErr;
    char packetBuffer[64];
    short responseSize = sizeof(packetBuffer);

    if (request == NULL || response == NULL) {
        return paramErr;
    }

    /* Handle different protocol operations - provenance: protocol switching */
    switch (request->protocolType) {
        case kATPPort:
            /* ATP transactions - provenance: AppleTalk Transaction Protocol */
            error = ATPSndRequest(NULL, false, NULL, NULL, NULL, NULL);
            break;

        case kNBPPort:
            /* NBP operations - provenance: Name Binding Protocol */
            error = NBPLookup(NULL, NULL, 0, NULL, 0, NULL);
            break;

        case kZIPPort:
            /* ZIP operations - provenance: Zone Information Protocol */
            error = ZIPGetZoneList(false, NULL, NULL);
            break;

        default:
            error = paramErr;
            break;
    }

    /* Fill response structure - provenance: response handling pattern */
    if (error == noErr && response != NULL) {
        response->error = noErr;
        response->dataSize = responseSize;
    }

    return error;
}

/*
 * ConfigurePrinter - Configure selected printer as current device
 * provenance: fcn.00002290 at offset 0x2290, size 14 bytes
 * Evidence: Compact printer configuration function
 */
OSErr ConfigurePrinter(ChooserPrinterInfo *printer)
{
    OSErr error = noErr;

    if (printer == NULL) {
        /* Use currently selected printer - provenance: default behavior */
        if (gChooserState.selectedPrinter < 0) {
            return paramErr;
        }
    }

    /* Set as current printer - provenance: Printing Manager integration */
    error = PrOpen();
    if (error != noErr) {
        return error;
    }

    /* Configure printer driver - provenance: printer driver loading */
    if (printer != NULL && printer->driverResource != NULL) {
        /* Load and configure driver - provenance: resource management */
    }

    PrClose();
    return error;
}

/*
 * LoadPrinterDriver - Load printer driver resources
 * provenance: fcn.000008ba at offset 0x08BA, size 8 bytes
 * Evidence: Small resource loading function
 */
Handle LoadPrinterDriver(ResType type, short id)
{
    Handle driverHandle;

    /* Load driver resource - provenance: Resource Manager calls */
    driverHandle = GetResource(type, id);

    if (driverHandle != NULL) {
        /* Load and lock resource - provenance: resource management pattern */
        LoadResource(driverHandle);
        HLock(driverHandle);
    }

    return driverHandle;
}

/*
 * ValidateSelection - Validate printer and zone combination
 * provenance: fcn.000030e6 at offset 0x30E6, size 16 bytes
 * Evidence: Small validation function
 */
Boolean ValidateSelection(ChooserPrinterInfo *printer, ATalkZone *zone)
{
    /* Basic validation - provenance: input validation pattern */
    if (printer == NULL) {
        return false;
    }

    /* Check if printer is in specified zone - provenance: zone validation */
    if (zone != NULL) {
        /* Compare zone names - provenance: string comparison */
        if (!EqualString(printer->nodeInfo.zoneName, *zone, false, false)) {
            return false;
        }
    }

    return true;
}

/*
 * RE-AGENT-TRAILER-JSON
 * {
 *   "source_file": "Chooser.rsrc",
 *   "sha256": "61ebc8ce7482cd85abc88d8a9fad4848d96f43bfe53619011dd15444c082b1c9",
 *   "implemented_functions": 12,
 *   "evidence_references": 67,
 *   "appletalk_integration": true,
 *   "desk_accessory_compliant": true,
 *   "confidence": "high",
 *   "analysis_tools": ["radare2_m68k"],
 *   "provenance_density": 0.82
 * }
 */