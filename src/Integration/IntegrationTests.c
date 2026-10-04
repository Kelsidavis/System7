/*
 * IntegrationTests.c - checks run inside the booted kernel
 *
 * Built in with INTEGRATION_TESTS=1 and run once at boot; results go to the
 * serial port, where tests/run_integration_tests.py reads them.
 *
 * Every test here calls the code it names and checks what came back. The
 * suite this replaces did not: each test set a Boolean to true, tested it,
 * and reported a pass, so it passed whatever state the system was in.
 */

#include "SystemTypes.h"
#include "Integration/IntegrationTests.h"
#include <string.h>
#include "Errors/ErrorCodes.h"
#include "FileManager.h"
#include "FileManager_Internal.h"
#include "System71StdLib.h"
#include "MemoryMgr/MemoryManager.h"
#include "DialogManager/DialogResources.h"
#include "DialogManager/DialogResourceParser.h"
#include "ResourceManager.h"
#include "WindowManager/WindowManager.h"
#include "QuickDraw/QuickDraw.h"
#include "QuickDraw/ColorQuickDraw.h"
#include "SystemInternal.h"
#include "Platform/Framebuffer.h"
#include "QuickDrawConstants.h"
#include "DialogManager/DialogManager.h"
#include "DialogManager/DialogEditText.h"
#include "DialogManager/AlertDialogs.h"
#include "DialogManager/DITLBuilder.h"
#include "OSUtils/OSUtils.h"
#include "EventManager/EventManager.h"
#include "EventManager/EventManagerInternal.h"
#include "EventManager/KeyboardEvents.h"
#include "ExtensionManager/ResourceLoader.h"
#include "FontManager/FontManager.h"
#include "FontManager/CJKFont.h"
#include "TextEncoding/CJKEncoding.h"
#include "TextEdit/TextEdit.h"
#include "FS/vfs.h"
#include "DeskManager/Calculator.h"
#include "DeskManager/Chooser.h"
#include "ProcessMgr/ProcessMgr.h"
#include "MacTypes.h"
#include "math.h"

#include "CPU/CPUBackend.h"
#include "CPU/M68KInterp.h"
#include "CPU/M68KHeap.h"
#include "SegmentLoader/MacBinary.h"
#include "SegmentLoader/SegmentLoader.h"
extern UInt32 M68K_Read32(M68KAddressSpace* as, UInt32 addr);
extern void M68K_Write32(M68KAddressSpace* as, UInt32 addr, UInt32 value);

/* Straight to the serial port. Results are the point of a test build, and
 * serial_logf filters the System module below Warn - which is why the old
 * suite's output never reached the runner at all. */
#define IT_OUT(prefix, fmt, ...) do { \
    char it_line_[256]; \
    snprintf(it_line_, sizeof it_line_, prefix fmt "\n", ##__VA_ARGS__); \
    serial_puts(it_line_); \
} while (0)
#define IT_LOG_INFO(fmt, ...) IT_OUT("", fmt, ##__VA_ARGS__)
#define IT_LOG_PASS(fmt, ...) IT_OUT("✓ PASS: ", fmt, ##__VA_ARGS__)
#define IT_LOG_FAIL(fmt, ...) IT_OUT("✗ FAIL: ", fmt, ##__VA_ARGS__)
#define IT_LOG_WARN(fmt, ...) IT_OUT("⚠ WARN: ", fmt, ##__VA_ARGS__)

static int test_count = 0;
static int test_pass = 0;
static int test_fail = 0;

typedef struct {
    const char* name;
    Boolean passed;
    const char* reason;
} TestResult;

static TestResult results[128];
static int result_count = 0;

static void RecordTest(const char* name, Boolean passed, const char* reason) {
    if (result_count < (int)(sizeof results / sizeof results[0])) {
        results[result_count].name = name;
        results[result_count].passed = passed;
        results[result_count].reason = reason;
        result_count++;
    }
    test_count++;
    if (passed) {
        test_pass++;
        IT_LOG_PASS("%s", name);
    } else {
        test_fail++;
        IT_LOG_FAIL("%s: %s", name, reason);
    }
}

/* The first check that fails decides the test's reason. */
#define CHECK(cond, why) do { if (!(cond)) { RecordTest(test_name, false, why); return; } } while (0)

/* ----------------------------------------------------------------------------
 * Memory Manager: handle state
 * ------------------------------------------------------------------------- */

static void Test_Memory_HandleStateRoundTrip(void) {
    const char* test_name = "Memory_HandleStateRoundTrip";
    Handle h = NewHandle(16);
    CHECK(h && *h, "NewHandle failed");

    UInt8 saved = HGetState(h);
    CHECK(saved == 0, "a new handle reported locked, purgeable or a resource");

    HLock(h);
    HPurge(h);
    CHECK(HGetState(h) == 0xC0, "lock and purge not reported in bits 7 and 6");

    /* Restoring the saved state must restore every handle flag. */
    HSetState(h, saved);
    CHECK(HGetState(h) == 0, "HSetState did not restore the saved state");

    HLock(h);
    HLock(h);
    UInt8 locked = HGetState(h);
    HUnlock(h);
    HUnlock(h);
    HSetState(h, locked);
    CHECK(HGetState(h) == 0x80, "HSetState did not restore a locked state");
    HUnlock(h);
    CHECK(HGetState(h) == 0, "restored lock did not come off with one HUnlock");

    DisposeHandle(h);
    RecordTest(test_name, true, "");
}

/* ----------------------------------------------------------------------------
 * Dialog Manager: templates from resource data
 * ------------------------------------------------------------------------- */

/* GetHandleSize and GetPtrSize answer the size asked for, not the block's,
 * and follow SetHandleSize and SetPtrSize (Inside Macintosh: Memory). */
static void Test_Memory_LogicalSizes(void) {
    const char* test_name = "Memory_LogicalSizes";
    static const u32 sizes[] = { 0, 1, 7, 10, 13, 100, 1001 };
    for (u32 i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        Handle h = NewHandle(sizes[i]);
        Ptr p = NewPtr(sizes[i]);
        u32 hs = GetHandleSize(h), ps = GetPtrSize(p);
        if (h) DisposeHandle(h);
        if (p) DisposePtr(p);
        CHECK(h && p, "allocation failed");
        CHECK(hs == sizes[i] || (sizes[i] == 0 && hs == 0), "GetHandleSize is not the size asked for");
        CHECK(ps == sizes[i], "GetPtrSize is not the size asked for");
    }

    Handle h = NewHandle(40);
    CHECK(h, "NewHandle failed");
    Boolean shrank = SetHandleSize(h, 30) && GetHandleSize(h) == 30;
    Boolean grew = SetHandleSize(h, 500) && GetHandleSize(h) == 500;
    DisposeHandle(h);
    CHECK(shrank, "SetHandleSize to smaller left GetHandleSize unchanged");
    CHECK(grew, "SetHandleSize to larger failed");

    Ptr p = NewPtr(40);
    CHECK(p, "NewPtr failed");
    Boolean pshrank = SetPtrSize(p, 20) && GetPtrSize(p) == 20;
    Boolean pregrew = SetPtrSize(p, 40) && GetPtrSize(p) == 40;
    DisposePtr(p);
    CHECK(pshrank && pregrew, "SetPtrSize could not change size within its block");
    RecordTest(test_name, true, "");
}

static void Test_DateTime_CalendarConversions(void) {
    const char* test_name = "DateTime_CalendarConversions";
    DateTimeRec date;

    Secs2Date(0, &date);
    CHECK(date.year == 1904 && date.month == 1 && date.day == 1,
          "Mac epoch did not decode to 1904-01-01");
    CHECK(date.dayOfWeek == 6, "Mac epoch weekday was not Friday");

    const UInt32 leapDayEnd = 59u * 86400u + 86399u;
    Secs2Date(leapDayEnd, &date);
    CHECK(date.year == 1904 && date.month == 2 && date.day == 29,
          "1904 leap day did not decode correctly");
    CHECK(date.hour == 23 && date.minute == 59 && date.second == 59,
          "end-of-day time fields did not decode correctly");
    CHECK(date.dayOfWeek == 2, "1904-02-29 weekday was not Monday");

    UInt32 roundTrip = 0;
    Date2Secs(&date, &roundTrip);
    CHECK(roundTrip == leapDayEnd, "date-to-seconds did not round-trip leap day");

    Secs2Date(3029529600u, &date);
    CHECK(date.year == 2000 && date.month == 1 && date.day == 1,
          "Unix epoch conversion did not decode to 2000-01-01");
    CHECK(date.dayOfWeek == 7, "2000-01-01 weekday was not Saturday");

    RecordTest(test_name, true, "");
}

static Handle HandleFromBytes(const UInt8* bytes, u32 size) {
    Handle h = NewHandle(size);
    if (h && *h) memcpy(*h, bytes, size);
    return h;
}

static void Test_Dialog_ParseDLOG(void) {
    const char* test_name = "Dialog_ParseDLOG";
    static const UInt8 dlog[] = {
        0x00, 0x28, 0x00, 0x50, 0x00, 0xC8, 0x01, 0x7C,  /* bounds 40,80,200,380 */
        0x00, 0x01,                                      /* procID 1 */
        0x01, 0x00,                                      /* visible, filler */
        0x00, 0x00,                                      /* goAwayFlag, filler */
        0x12, 0x34, 0x56, 0x78,                          /* refCon */
        0x00, 0x80,                                      /* itemsID 128 */
        0x05, 'H', 'e', 'l', 'l', 'o'                    /* title */
    };
    Handle h = HandleFromBytes(dlog, sizeof dlog);
    CHECK(h, "NewHandle failed");

    DialogTemplate* t = NULL;
    OSErr err = ParseDLOGResource(h, &t);
    DisposeHandle(h);
    CHECK(err == noErr && t, "parse failed");
    CHECK(t->boundsRect.top == 40 && t->boundsRect.left == 80 &&
          t->boundsRect.bottom == 200 && t->boundsRect.right == 380, "bounds wrong");
    CHECK(t->procID == 1, "procID wrong");
    CHECK(t->visible && !t->goAwayFlag, "flags wrong");
    CHECK(t->refCon == 0x12345678, "refCon wrong");
    CHECK(t->itemsID == 128, "itemsID wrong");
    CHECK(t->title[0] == 5 && memcmp(&t->title[1], "Hello", 5) == 0, "title wrong");
    DisposeDialogTemplate(t);
    RecordTest(test_name, true, "");
}

static void Test_Dialog_ParseALRT(void) {
    const char* test_name = "Dialog_ParseALRT";
    static const UInt8 alrt[] = {
        0x00, 0x32, 0x00, 0x3C, 0x00, 0x96, 0x01, 0x68,  /* bounds 50,60,150,360 */
        0x00, 0x81,                                      /* itemsID 129 */
        0x12, 0x34                                       /* stages */
    };
    Handle h = HandleFromBytes(alrt, sizeof alrt);
    CHECK(h, "NewHandle failed");

    AlertTemplate* t = NULL;
    OSErr err = ParseALRTResource(h, &t);
    DisposeHandle(h);
    CHECK(err == noErr && t, "parse failed");
    CHECK(t->boundsRect.top == 50 && t->boundsRect.left == 60 &&
          t->boundsRect.bottom == 150 && t->boundsRect.right == 360, "bounds wrong");
    CHECK(t->itemsID == 129, "itemsID wrong");
    CHECK((UInt16)t->stages == 0x1234, "stages wrong");
    DisposeAlertTemplate(t);
    RecordTest(test_name, true, "");
}

static void Test_Dialog_ParseDLOGTruncated(void) {
    const char* test_name = "Dialog_ParseDLOGTruncated";
    static const UInt8 shortData[10] = {0};
    Handle h = HandleFromBytes(shortData, sizeof shortData);
    CHECK(h, "NewHandle failed");

    DialogTemplate* t = (DialogTemplate*)1;
    OSErr err = ParseDLOGResource(h, &t);
    DisposeHandle(h);
    CHECK(err != noErr, "accepted ten bytes as a dialog template");
    CHECK(t == NULL, "left a template behind after failing");
    RecordTest(test_name, true, "");
}

static void Test_Dialog_ParseDITLTruncatedAfterText(void) {
    const char* test_name = "Dialog_ParseDITLTruncatedAfterText";
    static const UInt8 ditl[] = {
        0x00, 0x01,                                      /* two items */
        0x00, 0x00, 0x00, 0x00,                          /* item 1 handle */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x14, /* bounds */
        statText, 0x01, 'A', 0x00,                       /* text and padding */
        0x00, 0x00, 0x00, 0x00, 0x00                     /* truncated item 2 */
    };
    Handle h = HandleFromBytes(ditl, sizeof ditl);
    CHECK(h, "NewHandle failed");

    DialogItemEx* items = (DialogItemEx*)1;
    SInt16 itemCount = 1;
    OSErr err = ParseDITL(h, &items, &itemCount);
    DisposeHandle(h);
    CHECK(err != noErr, "accepted a truncated second item");
    CHECK(items == NULL && itemCount == 0,
          "left partial parser output after a DITL error");
    RecordTest(test_name, true, "");
}

static void Test_Dialog_ParseDITLRejectsNegativeLongLength(void) {
    const char* test_name = "Dialog_ParseDITLRejectsNegativeLongLength";
    static const UInt8 ditl[] = {
        0x00, 0x00,                                      /* one item */
        0x00, 0x00, 0x00, 0x00,                          /* item handle */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x14, /* bounds */
        statText, 0xFF, 0x80, 0x00                       /* negative signed length */
    };
    Handle h = HandleFromBytes(ditl, sizeof ditl);
    CHECK(h, "NewHandle failed");

    DialogItemEx* items = (DialogItemEx*)1;
    SInt16 itemCount = 1;
    OSErr err = ParseDITL(h, &items, &itemCount);
    DisposeHandle(h);
    CHECK(err != noErr, "accepted a long data length with its high bit set");
    CHECK(items == NULL && itemCount == 0,
          "left parser outputs set after rejecting a negative length");
    RecordTest(test_name, true, "");
}

static void Test_Dialog_LoadMissingTemplate(void) {
    const char* test_name = "Dialog_LoadMissingTemplate";
    DialogTemplate* t = (DialogTemplate*)1;
    OSErr err = LoadDialogTemplate(32000, &t);
    CHECK(err == resNotFound, "a missing DLOG did not answer resNotFound");
    CHECK(t == NULL, "left a template behind for a missing resource");
    RecordTest(test_name, true, "");
}

static void Test_Dialog_ActionDebounce(void) {
    const char* test_name = "Dialog_ActionDebounce";
    UInt32 finalTicks;

    Delay(8, &finalTicks);
    CHECK(!DM_DebounceAction(1), "first keyboard action was suppressed");
    CHECK(!DM_DebounceAction(1), "repeated keyboard action was suppressed");
    CHECK(DM_DebounceAction(2), "cross-kind action was not suppressed");

    Delay(8, &finalTicks);
    CHECK(!DM_DebounceAction(2), "action remained suppressed after debounce window");
    RecordTest(test_name, true, "");
}

/* ----------------------------------------------------------------------------
 * Resource Manager: resource files named by FSSpec
 * ------------------------------------------------------------------------- */

static void SetSpec(FSSpec* spec, const char* name) {
    memset(spec, 0, sizeof(*spec));
    size_t len = strlen(name);
    spec->name[0] = (UInt8)len;
    memcpy(&spec->name[1], name, len);
}

static void Test_File_WriteReadRoundTrip(void) {
    const char* test_name = "File_WriteReadRoundTrip";
    FSSpec spec;
    SetSpec(&spec, "ITest Data");
    static const char kText[] = "System 7 wrote this";
    const UInt32 kLen = sizeof(kText) - 1;
    FSDelete(spec.name, 0);

    CHECK(FSCreate(spec.name, 0, FOURCC('I', 'T', 's', 't'), FOURCC('T', 'E', 'X', 'T')) == noErr, "FSCreate failed");
    FileRefNum ref = 0;
    CHECK(FSOpen(spec.name, 0, &ref) == noErr && ref > 0, "FSOpen failed");

    UInt32 n = kLen;
    OSErr err = FSWrite(ref, &n, kText);
    if (err != noErr || n != kLen) { FSClose(ref); CHECK(false, "FSWrite failed"); }

    char back[32];
    memset(back, 0, sizeof back);
    n = kLen;
    err = FSSetFPos(ref, fsFromStart, 0);
    if (err == noErr) err = FSRead(ref, &n, back);
    if (err != noErr || n != kLen || memcmp(back, kText, kLen) != 0) {
        FSClose(ref);
        CHECK(false, "read back did not match what was written");
    }

    /* A second read at the end must say so rather than hand back the start. */
    n = 4;
    err = FSRead(ref, &n, back);
    FSClose(ref);
    CHECK(err == eofErr && n == 0, "reading at the end did not report eofErr");

    CHECK(FSOpen(spec.name, 0, &ref) == noErr, "reopen failed");
    UInt32 eof = 0;
    err = FSGetEOF(ref, &eof);
    FSClose(ref);
    CHECK(err == noErr && eof == kLen, "reopened file had the wrong length");

    CHECK(FSDelete(spec.name, 0) == noErr, "FSDelete failed");
    CHECK(FSOpen(spec.name, 0, &ref) == fnfErr, "file still opens after FSDelete");
    RecordTest(test_name, true, "");
}

static void Test_File_Metadata(void) {
    const char* test_name = "File_Metadata";
    FSSpec spec;
    SetSpec(&spec, "ITest Meta");
    FSDelete(spec.name, 0);

    CHECK(FSCreate(spec.name, 0, FOURCC('I', 'T', 's', 't'), FOURCC('T', 'E', 'X', 'T')) == noErr, "FSCreate failed");
    FInfo info;
    memset(&info, 0, sizeof info);
    CHECK(FSGetFInfo(spec.name, 0, &info) == noErr, "FSGetFInfo failed");
    CHECK(info.fdType == FOURCC('T', 'E', 'X', 'T') && info.fdCreator == FOURCC('I', 'T', 's', 't'),
          "the type and creator FSCreate set did not stick");

    FileRefNum ref = 0;
    CHECK(FSOpen(spec.name, 0, &ref) == noErr, "FSOpen failed");
    UInt32 eof = 0;
    OSErr err = FSSetEOF(ref, 100);
    if (err == noErr) err = FSGetEOF(ref, &eof);
    char buf[100];
    UInt32 n = sizeof buf;
    Boolean zeros = false;
    if (err == noErr && FSSetFPos(ref, fsFromStart, 0) == noErr && FSRead(ref, &n, buf) == noErr) {
        zeros = (n == 100);
        for (UInt32 i = 0; i < n; i++) if (buf[i] != 0) zeros = false;
    }
    OSErr shrink = FSSetEOF(ref, 10);
    UInt32 shrunk = 0;
    if (shrink == noErr) shrink = FSGetEOF(ref, &shrunk);
    FSClose(ref);
    FSDelete(spec.name, 0);
    CHECK(err == noErr && eof == 100, "FSSetEOF did not grow the file");
    CHECK(zeros, "the grown part did not read back as zeros");
    CHECK(shrink == noErr && shrunk == 10, "FSSetEOF did not shorten the file");
    RecordTest(test_name, true, "");
}

static void Test_File_FoldersAndWorkingDirectories(void) {
    const char* test_name = "File_FoldersAndWorkingDirectories";
    FSSpec spec;
    SetSpec(&spec, "ITest Folder");
    FSDeleteDir(spec.name, 0);

    DirID dir = 0;
    CHECK(FSCreateDir(spec.name, 0, &dir) == noErr && dir > 2, "FSCreateDir failed");

    WDRefNum wd = 0;
    CHECK(FSOpenWD(0, dir, FOURCC('I', 'T', 's', 't'), &wd) == noErr && wd < 0, "FSOpenWD failed");
    DirID gotDir = 0;
    UInt32 gotProc = 0;
    OSErr err = FSGetWDInfo(wd, NULL, &gotDir, &gotProc);
    OSErr closed = FSCloseWD(wd);
    CHECK(err == noErr && gotDir == dir && gotProc == FOURCC('I', 'T', 's', 't'), "FSGetWDInfo gave back something else");
    CHECK(closed == noErr && FSGetWDInfo(wd, NULL, NULL, NULL) != noErr,
          "the working directory was still there after FSCloseWD");

    CHECK(FSDeleteDir(spec.name, 0) == noErr, "FSDeleteDir failed");
    CHECK(FSDeleteDir(spec.name, 0) != noErr, "the folder was still there after FSDeleteDir");
    RecordTest(test_name, true, "");
}

static void Test_VFS_ApplicationsCatalog(void) {
    const char* test_name = "VFS_ApplicationsCatalog";
    VRefNum vref = VFS_GetBootVRef();
    VolumeControlBlock volume;
    CatEntry applications;
    CatEntry entries[8];
    int count = 0;
    Boolean simpleText = false;
    Boolean textEdit = false;
    Boolean macPaint = false;

    CHECK(VFS_GetVolumeInfo(vref, &volume), "could not read boot volume info");
    CHECK(VFS_Lookup(vref, volume.rootID, "Applications", &applications) &&
          applications.kind == kNodeDir,
          "Applications directory was not in the root catalog");
    CHECK(VFS_Enumerate(vref, applications.id, entries, 8, &count),
          "could not enumerate the Applications directory");
    CHECK(count == 3, "Applications did not enumerate exactly three entries");

    for (int i = 0; i < count; i++) {
        if (entries[i].kind != kNodeFile ||
            entries[i].parent != (DirID)applications.id ||
            entries[i].type != FOURCC('A', 'P', 'P', 'L')) {
            continue;
        }
        if (strcmp(entries[i].name, "SimpleText") == 0 &&
            entries[i].creator == FOURCC('t', 't', 'x', 't')) simpleText = true;
        if (strcmp(entries[i].name, "TextEdit") == 0 &&
            entries[i].creator == FOURCC('t', 'e', 'd', 't')) textEdit = true;
        if (strcmp(entries[i].name, "MacPaint") == 0 &&
            entries[i].creator == FOURCC('M', 'A', 'P', 'P')) macPaint = true;
    }

    CHECK(simpleText && textEdit && macPaint,
          "built-in application catalog metadata was incomplete");
    RecordTest(test_name, true, "");
}

/* A file made in a folder is in that folder, and found there by FSSpec, by
 * dirID and through a working directory - and not in the root. */
static void Test_File_InFolder(void) {
    const char* test_name = "File_InFolder";
    FSSpec folder, file;
    SetSpec(&folder, "ITest Nest");
    SetSpec(&file, "ITest Nested");
    FSDelete(file.name, 0);

    DirID dir = 0;
    CHECK(FSCreateDir(folder.name, 0, &dir) == noErr, "FSCreateDir failed");

    FSSpec spec;
    CHECK(FSMakeFSSpec(0, dir, file.name, &spec) == fnfErr, "FSMakeFSSpec found a file not yet made");
    CHECK(spec.parID == dir, "FSMakeFSSpec lost the folder");
    OSErr made = FSpCreate(&spec, FOURCC('I', 'T', 's', 't'), FOURCC('T', 'E', 'X', 'T'), 0);

    FInfo info;
    OSErr inRoot = HGetFInfo(0, 0, file.name, &info);
    OSErr byDir = HGetFInfo(0, dir, file.name, &info);
    OSErr bySpec = FSpGetFInfo(&spec, &info);

    WDRefNum wd = 0;
    OSErr viaWD = FSOpenWD(0, dir, FOURCC('I', 'T', 's', 't'), &wd);
    FileRefNum ref = 0;
    if (viaWD == noErr) viaWD = FSOpen(file.name, wd, &ref);
    if (viaWD == noErr) FSClose(ref);
    FSSpec wdSpec;
    OSErr wdMake = FSMakeFSSpec(wd, 0, file.name, &wdSpec);
    FSCloseWD(wd);

    OSErr gone = FSpDelete(&spec);
    OSErr dirGone = FSDeleteDir(folder.name, 0);

    CHECK(made == noErr, "FSpCreate in the folder failed");
    CHECK(inRoot == fnfErr, "the file was made in the root, not the folder");
    CHECK(byDir == noErr && bySpec == noErr, "the file was not found in its folder");
    CHECK(info.fdType == FOURCC('T', 'E', 'X', 'T'), "the file in the folder lost its type");
    CHECK(viaWD == noErr, "FSOpen through a working directory failed");
    CHECK(wdMake == noErr && wdSpec.parID == dir && wdSpec.vRefNum == spec.vRefNum,
          "FSMakeFSSpec did not turn the working directory into its volume and folder");
    CHECK(gone == noErr, "FSpDelete in the folder failed");
    CHECK(dirGone == noErr, "the folder was not empty after FSpDelete");
    RecordTest(test_name, true, "");
}

/*
 * A file whose extents run past the three the catalog holds, so the rest
 * come from the extents overflow B-tree. It needs the disk
 * tools/make_fragmented_hfs.sh makes, attached as an IDE drive; without it
 * the test says so and is not counted.
 *
 * The file is 80000 bytes of ((i * 7) ^ (i >> 9)) & 0xFF, read in pieces
 * that do not line up with its 512-byte allocation blocks, then read again
 * from a position deep in the overflow extents.
 */
static UInt8 FragByte(UInt32 i) { return (UInt8)(((i * 7) ^ (i >> 9)) & 0xFF); }

static void Test_File_ReadThroughExtentsOverflow(void) {
    const char* test_name = "File_ReadThroughExtentsOverflow";
    static const UInt8 volName[] = "\x08" "ITestHFS";
    static const UInt8 fileName[] = "\x0A" "Fragmented";
    enum { kSize = 80000 };

    VCB* vcb = VCB_FindByName(volName);
    if (!vcb) {
        serial_puts("[IT] SKIP: File_ReadThroughExtentsOverflow (no ITestHFS disk attached)\n");
        return;
    }
    VolumeRefNum vref = vcb->base.vcbVRefNum;

    FileRefNum ref = 0;
    CHECK(FSOpen(fileName, vref, &ref) == noErr, "could not open the fragmented file");
    UInt32 eof = 0;
    FSGetEOF(ref, &eof);

    static UInt8 buf[777];
    UInt32 at = 0;
    Boolean same = true;
    OSErr err = noErr;
    while (err == noErr && at < kSize) {
        UInt32 n = sizeof buf;
        err = FSRead(ref, &n, buf);
        for (UInt32 i = 0; i < n; i++) if (buf[i] != FragByte(at + i)) same = false;
        at += n;
        if (n == 0) break;
    }

    static UInt8 tail[3000];
    UInt32 n = sizeof tail;
    Boolean seekSame = FSSetFPos(ref, fsFromStart, 70001) == noErr &&
                       FSRead(ref, &n, tail) == noErr && n == sizeof tail;
    for (UInt32 i = 0; seekSame && i < n; i++) {
        if (tail[i] != FragByte(70001 + i)) seekSame = false;
    }
    FSClose(ref);

    CHECK(eof == kSize, "the file's length was wrong");
    CHECK(at == kSize, "reading stopped before the end of the file");
    CHECK(same, "bytes past the catalog's three extents read back wrong");
    CHECK(seekSame, "a read from a position in an overflow extent came back wrong");
    RecordTest(test_name, true, "");
}

/*
 * Drawing in a window that another one partly covers stays out of the
 * covered part: rectangles and text, read back from the screen.
 */
static UInt32 ScreenPixel(int x, int y) {
    return *(UInt32*)((UInt8*)framebuffer + y * fb_pitch + x * 4);
}

static void Test_Draw_ClippedToVisibleRegion(void) {
    const char* test_name = "Draw_ClippedToVisibleRegion";
    Rect backR  = { 150, 520, 350, 700 };    /* top, left, bottom, right */
    Rect frontR = { 220, 600, 420, 780 };
    WindowPtr back = NewWindow(NULL, &backR, (ConstStr255Param)"\x06ITBack", true,
                               0, (WindowPtr)-1, false, 0);
    WindowPtr front = NewWindow(NULL, &frontR, (ConstStr255Param)"\x07ITFront", true,
                                0, (WindowPtr)-1, false, 0);
    CHECK(back && front, "NewWindow failed");

    const int px = 650, py = 300;            /* inside both: front covers it */
    const int qx = 560, qy = 200;            /* in the back window only */
    /* The screen may not be drawn yet: give the front window white
     * content, so the back one's black has something to show against. */
    GrafPtr save;
    GetPort(&save);
    SetPort((GrafPtr)front);
    EraseRect(&front->port.portRect);
    UInt32 before = ScreenPixel(px, py);

    SetPort((GrafPtr)back);
    PaintRect(&back->port.portRect);
    MoveTo(0, 150);
    DrawString((ConstStr255Param)"\x14WWWWWWWWWWWWWWWWWWWW");
    SetPort(save);

    UInt32 covered = ScreenPixel(px, py);
    UInt32 uncovered = ScreenPixel(qx, qy);
    DisposeWindow(front);
    DisposeWindow(back);

    CHECK((before & 0x00FFFFFF) == 0x00FFFFFF, "the front window's content did not erase to white");
    CHECK(covered == before, "the back window drew over the front one");
    CHECK((uncovered & 0x00FFFFFF) == 0, "the back window's own uncovered part was not painted");
    RecordTest(test_name, true, "");
}

/* Transfer modes (Inside Macintosh: Imaging With QuickDraw, 3-8), read
 * back from the screen. */
static void Test_Draw_PenModes(void) {
    const char* test_name = "Draw_PenModes";
    Rect wr = { 150, 520, 300, 700 };
    WindowPtr w = NewWindow(NULL, &wr, (ConstStr255Param)"\x06ITPens", true,
                            0, (WindowPtr)-1, false, 0);
    CHECK(w, "NewWindow failed");
    GrafPtr save;
    GetPort(&save);
    SetPort((GrafPtr)w);
    EraseRect(&w->port.portRect);

    /* (gx,gy) is global for local (10,10) */
    int gx = (*w->contRgn)->rgnBBox.left + 10, gy = (*w->contRgn)->rgnBBox.top + 10;
    Rect r = { 5, 5, 25, 25 };

    PenNormal();
    PenMode(patXor);
    PaintRect(&r);
    UInt32 once = ScreenPixel(gx, gy);
    PaintRect(&r);
    UInt32 twice = ScreenPixel(gx, gy);

    PenMode(patCopy);
    PaintRect(&r);                       /* black */
    PenPat(&qd.gray);
    PenMode(patBic);
    PaintRect(&r);                       /* clears every other pixel */
    UInt32 a = ScreenPixel(gx, gy), b = ScreenPixel(gx + 1, gy);

    PenNormal();
    EraseRect(&w->port.portRect);
    PenMode(patXor);
    FrameRect(&r);
    UInt32 edge = ScreenPixel(gx - 5, gy);   /* local (5,10): the left edge */
    UInt32 corner = ScreenPixel(gx - 5, gy - 5);   /* local (5,5): a corner */
    PenNormal();

    /* A 3-pixel frame lies inside its rectangle */
    EraseRect(&w->port.portRect);
    PenSize(3, 3);
    FrameRect(&r);
    PenNormal();
    UInt32 innerRight = ScreenPixel(gx + 14, gy);   /* local (24,10): last column inside */
    UInt32 outsideRight = ScreenPixel(gx + 15, gy); /* local (25,10): just outside */
    UInt32 hole = ScreenPixel(gx + 1, gy + 1);      /* local (11,11): inside the band */

    SetPort(save);
    DisposeWindow(w);

    CHECK((once & 0x00FFFFFF) == 0, "patXor did not invert white to black");
    CHECK((twice & 0x00FFFFFF) == 0x00FFFFFF, "a second patXor did not restore white");
    CHECK(((a ^ b) & 0x00FFFFFF) == 0x00FFFFFF, "patBic grey did not clear every other pixel");
    CHECK((edge & 0x00FFFFFF) == 0, "a patXor frame was not drawn");
    CHECK((corner & 0x00FFFFFF) == 0, "a patXor frame's corner was inverted twice");
    CHECK((innerRight & 0x00FFFFFF) == 0 && (outsideRight & 0x00FFFFFF) == 0x00FFFFFF,
          "a thick frame did not lie inside its rectangle");
    CHECK((hole & 0x00FFFFFF) == 0x00FFFFFF, "a thick frame filled its middle");
    RecordTest(test_name, true, "");
}

/* An alert built with no resources: OK is item 1 and Cancel item 2, the
 * message is the ParamText text, and the icon is drawn. The alert is looked
 * at from its filter procedure, then dismissed with item 1. */
static struct { Boolean seen, okFirst, cancelSecond, iconDrawn; unsigned char text[64]; } gAlertSeen;

static Boolean ITest_AlertFilter(DialogPtr d, EventRecord* e, SInt16* itemHit) {
    (void)e;
    if (!gAlertSeen.seen) {
        gAlertSeen.seen = true;
            SInt16 type;
        Handle h;
        Rect r;
        GetDialogItem(d, 1, &type, &h, &r);
        gAlertSeen.okFirst = (type & 0x7F) == (ctrlItem + btnCtrl);
        GetDialogItem(d, 2, &type, &h, &r);
        gAlertSeen.cancelSecond = (type & 0x7F) == (ctrlItem + btnCtrl);

        SInt16 n = CountDITL(d);
        for (SInt16 i = 1; i <= n; i++) {
            GetDialogItem(d, i, &type, &h, &r);
            if ((type & 0x7F) == statText) {
                GetDialogItemText(h, gAlertSeen.text);
            }
        }

        /* The icon well is the last item; draw it and look at its middle */
        GrafPtr save;
        GetPort(&save);
        SetPort((GrafPtr)d);
        DrawDialog(d);
        GetDialogItem(d, n, &type, &h, &r);
        int black = 0;
        for (int y = r.top; y < r.bottom; y++) {
            for (int x = r.left; x < r.right; x++) {
                Point g = { (short)y, (short)x };
                LocalToGlobal(&g);
                if ((ScreenPixel(g.h, g.v) & 0x00FFFFFF) == 0) black++;
            }
        }
        gAlertSeen.iconDrawn = black > 100;
        SetPort(save);
    }
    *itemHit = 1;
    return true;
}

static void Test_Dialog_AlertLayout(void) {
    const char* test_name = "Dialog_AlertLayout";
    memset(&gAlertSeen, 0, sizeof gAlertSeen);
    ParamText((ConstStr255Param)"\x0BITest alert", (ConstStr255Param)"", (ConstStr255Param)"", (ConstStr255Param)"");
    SInt16 hit = CautionAlert(128, ITest_AlertFilter);
    CHECK(gAlertSeen.seen, "the alert's filter was never called");
    CHECK(gAlertSeen.okFirst && gAlertSeen.cancelSecond, "OK and Cancel are not items 1 and 2");
    /* The item holds ^0; drawing substitutes ParamText's text for it */
    CHECK(gAlertSeen.text[0] == 2 && gAlertSeen.text[1] == '^' && gAlertSeen.text[2] == '0',
          "the message item is not ^0");
    SubstituteAlertParameters(gAlertSeen.text);
    CHECK(gAlertSeen.text[0] == 11 && memcmp(&gAlertSeen.text[1], "ITest alert", 11) == 0,
          "^0 did not become the ParamText text");
    CHECK(gAlertSeen.iconDrawn, "the caution icon was not drawn");
    CHECK(hit == 1, "the alert did not answer the item its filter chose");
    RecordTest(test_name, true, "");
}

/* Reordering and hiding windows redraw what they uncover. */
static void PaintContent(WindowPtr w, Boolean black) {
    GrafPtr save;
    GetPort(&save);
    SetPort((GrafPtr)w);
    if (black) PaintRect(&w->port.portRect); else EraseRect(&w->port.portRect);
    SetPort(save);
}

static void Test_Window_ReorderAndHide(void) {
    const char* test_name = "Window_ReorderAndHide";
    Rect aR = { 150, 520, 350, 700 };
    Rect bR = { 220, 600, 420, 780 };
    WindowPtr a = NewWindow(NULL, &aR, (ConstStr255Param)"\x01" "A", true, 0, (WindowPtr)-1, false, 0);
    WindowPtr b = NewWindow(NULL, &bR, (ConstStr255Param)"\x01" "B", true, 0, (WindowPtr)-1, false, 0);
    CHECK(a && b, "NewWindow failed");
    const int ox = 650, oy = 300;    /* in both contents */
    const int dx = 760, dy = 400;    /* in B only, over the desktop */

    PaintContent(a, false);
    PaintContent(b, true);
    UInt32 bInFront = ScreenPixel(ox, oy);

    BringToFront(a);                 /* A now covers the overlap */
    UInt32 aBrought = ScreenPixel(ox, oy);

    SendBehind(a, NULL);             /* B in front again */
    PaintContent(b, true);
    UInt32 bAgain = ScreenPixel(ox, oy);

    HideWindow(b);
    UInt32 afterHide = ScreenPixel(dx, dy);
    UInt32 aShows = ScreenPixel(ox, oy);

    DisposeWindow(b);
    DisposeWindow(a);

    CHECK((bInFront & 0x00FFFFFF) == 0, "the front window's black did not show");
    CHECK((aBrought & 0x00FFFFFF) != 0, "BringToFront left the other window's content over it");
    CHECK((bAgain & 0x00FFFFFF) == 0, "after SendBehind the window now in front did not show");
    CHECK((afterHide & 0x00FFFFFF) != 0x00FFFFFF, "HideWindow left white where the desktop was");
    CHECK((aShows & 0x00FFFFFF) != 0, "HideWindow left the hidden window's content over the one behind");
    RecordTest(test_name, true, "");
}

static void Test_Dialog_NestedModalWindowState(void) {
    const char* test_name = "Dialog_NestedModalWindowState";
    Rect backBounds = { 150, 520, 300, 700 };
    Rect firstBounds = { 180, 540, 330, 720 };
    Rect nestedBounds = { 210, 560, 360, 740 };
    WindowPtr back = NewWindow(NULL, &backBounds, (ConstStr255Param)"\x04" "Back",
                               true, 0, (WindowPtr)-1, false, 0);
    WindowPtr first = NewWindow(NULL, &firstBounds, (ConstStr255Param)"\x05" "First",
                                true, dBoxProc, (WindowPtr)-1, false, 0);
    WindowPtr nested = NewWindow(NULL, &nestedBounds, (ConstStr255Param)"\x06" "Nested",
                                 true, dBoxProc, (WindowPtr)-1, false, 0);
    Boolean passed = back && first && nested;
    Boolean firstStarted = false;
    Boolean nestedStarted = false;

    if (passed) {
        firstStarted = BeginModalDialog((DialogPtr)first) == noErr;
        passed = firstStarted &&
                 !back->hilited && GetFrontModalDialog() == (DialogPtr)first;
    }
    if (passed) {
        nestedStarted = BeginModalDialog((DialogPtr)nested) == noErr;
        passed = nestedStarted &&
                 !first->hilited && !back->hilited &&
                 GetFrontModalDialog() == (DialogPtr)nested;
    }
    if (nestedStarted) EndModalDialog((DialogPtr)nested);
    if (passed) {
        passed = first->hilited && !back->hilited &&
                 GetFrontModalDialog() == (DialogPtr)first;
    }
    if (firstStarted) EndModalDialog((DialogPtr)first);
    if (passed) {
        passed = back->hilited && GetFrontModalDialog() == NULL;
    }

    if (nested) DisposeWindow(nested);
    if (first) DisposeWindow(first);
    if (back) DisposeWindow(back);
    RecordTest(test_name, passed,
               "modal windows were not disabled, restored, or cleared in stack order");
}

/* MoveWindow places the content's corner, for any kind of window; zooming
 * in and back out restores the window. */
static void Test_Window_MoveAndZoom(void) {
    const char* test_name = "Window_MoveAndZoom";
    Rect r = { 150, 520, 300, 700 };
    WindowPtr doc = NewWindow(NULL, &r, (ConstStr255Param)"\x03" "Doc", true, zoomDocProc, (WindowPtr)-1, true, 0);
    WindowPtr dlg = NewWindow(NULL, &r, (ConstStr255Param)"", true, dBoxProc, (WindowPtr)-1, false, 0);
    CHECK(doc && dlg, "NewWindow failed");

    Boolean ok = true;
    WindowPtr ws[2] = { doc, dlg };
    for (int i = 0; i < 2; i++) {
        Rect f0 = (*ws[i]->strucRgn)->rgnBBox, c0 = (*ws[i]->contRgn)->rgnBBox;
        MoveWindow(ws[i], 300, 200, false);
        Rect f1 = (*ws[i]->strucRgn)->rgnBBox, c1 = (*ws[i]->contRgn)->rgnBBox;
        if (c1.left != 300 || c1.top != 200) ok = false;
        if (c1.left - f1.left != c0.left - f0.left || c1.top - f1.top != c0.top - f0.top) ok = false;
    }
    Rect before = (*doc->contRgn)->rgnBBox;
    ZoomWindow(doc, inZoomOut + 1 /* inZoomIn */, false);
    Rect zoomed = (*doc->contRgn)->rgnBBox;
    Boolean zoomedBuffered = doc->offscreenGWorld != NULL;
    ZoomWindow(doc, inZoomOut, false);
    Rect after = (*doc->contRgn)->rgnBBox;

    DisposeWindow(dlg);
    DisposeWindow(doc);
    CHECK(ok, "MoveWindow did not put the content's corner where asked, or changed the frame");
    CHECK(zoomed.top >= 20 && (zoomed.right - zoomed.left) > (before.right - before.left),
          "zooming in did not enlarge the window below the menu bar");
    CHECK(zoomedBuffered, "the zoomed window lost its offscreen buffer");
    CHECK(after.left == before.left && after.top == before.top &&
          after.right == before.right && after.bottom == before.bottom,
          "zooming back out did not restore the window");
    RecordTest(test_name, true, "");
}

/* A dialog's icon item draws the icon, not a placeholder: ID 2 is the
 * system caution icon. */
static void Test_Dialog_IconItem(void) {
    const char* test_name = "Dialog_IconItem";
    DITLBuilder b;
    CHECK(DITL_Begin(&b, 256), "DITL_Begin failed");
    Rect well = { 10, 10, 42, 42 };
    DITL_AddItemPascal(&b, iconItem, &well, (ConstStr255Param)"\x02\x00\x02");
    Handle items = DITL_Finish(&b);
    CHECK(items, "DITL_Finish failed");
    Rect r = { 150, 520, 230, 700 };
    DialogPtr d = NewDialog(NULL, &r, (ConstStr255Param)"", true, dBoxProc,
                            (WindowPtr)-1, false, 0, items);
    CHECK(d, "NewDialog failed");

    GrafPtr save;
    GetPort(&save);
    SetPort((GrafPtr)d);
    DrawDialog(d);
    int black = 0, diagonal = 0;
    for (int y = well.top; y < well.bottom; y++) {
        for (int x = well.left; x < well.right; x++) {
            Point g = { (short)y, (short)x };
            LocalToGlobal(&g);
            if ((ScreenPixel(g.h, g.v) & 0x00FFFFFF) == 0) {
                black++;
                if (x - well.left == y - well.top) diagonal++;
            }
        }
    }
    SetPort(save);
    DisposeDialog(d);

    CHECK(black > 100, "the icon was not drawn");
    CHECK(diagonal < 30, "a placeholder X was drawn instead of the icon");
    RecordTest(test_name, true, "");
}

static void Test_Dialog_EditTextFocusBeyond32Items(void) {
    const char* test_name = "Dialog_EditTextFocusBeyond32Items";
    DITLBuilder b;
    CHECK(DITL_Begin(&b, 4096), "DITL_Begin failed");
    for (int i = 0; i < 32; i++) {
        DITL_AddText(&b, 0, 0, 10, 40, "label");
    }
    DITL_AddEditText(&b, 12, 0, 22, 40, "first");
    DITL_AddEditText(&b, 24, 0, 34, 40, "second");
    Handle items = DITL_Finish(&b);
    CHECK(items, "DITL_Finish failed");

    Rect bounds = { 100, 100, 160, 180 };
    DialogPtr d = NewDialog(NULL, &bounds, (ConstStr255Param)"", false,
                            dBoxProc, (WindowPtr)-1, false, 0, items);
    CHECK(d, "NewDialog failed");
    CHECK(GetDialogEditTextFocus(d) == 33,
          "dialog initialization did not focus edit item 33");
    AdvanceDialogEditTextFocus(d, false);
    CHECK(GetDialogEditTextFocus(d) == 34,
          "forward focus traversal did not reach edit item 34");
    AdvanceDialogEditTextFocus(d, true);
    CHECK(GetDialogEditTextFocus(d) == 33,
          "backward focus traversal did not return to edit item 33");
    DisposeDialog(d);
    RecordTest(test_name, true, "");
}

static void Test_Chooser_InitializeLayout(void) {
    const char* test_name = "Chooser_InitializeLayout";
    Chooser chooser;
    CHECK(Chooser_Initialize(&chooser) == CHOOSER_ERR_NONE,
          "Chooser_Initialize failed");
    CHECK(chooser.windowBounds.left == 100 && chooser.windowBounds.top == 100 &&
          chooser.windowBounds.right == 500 && chooser.windowBounds.bottom == 400,
          "Chooser window bounds were overwritten by a content rectangle");
    CHECK(chooser.deviceListRect.left == 20 && chooser.deviceListRect.top == 40 &&
          chooser.deviceListRect.right == 180 && chooser.deviceListRect.bottom == 200,
          "Chooser device-list bounds were not initialized");
    CHECK(chooser.zoneListRect.left == 200 && chooser.zoneListRect.top == 40 &&
          chooser.zoneListRect.right == 360 && chooser.zoneListRect.bottom == 120,
          "Chooser zone-list bounds were not initialized");
    CHECK(chooser.deviceInfoRect.left == 20 && chooser.deviceInfoRect.top == 220 &&
          chooser.deviceInfoRect.right == 380 && chooser.deviceInfoRect.bottom == 280,
          "Chooser device-info bounds were not initialized");
    CHECK(Chooser_ScanDevices(&chooser, DEVICE_TYPE_UNKNOWN) == 2,
          "Chooser sample scan did not return both devices");
    Point devicePoint = { .v = 45, .h = 25 };
    CHECK(Chooser_HandleClick(&chooser, devicePoint, 0) == CHOOSER_ERR_NONE &&
          chooser.selectedDeviceIndex == 0,
          "device-list click did not select the first device");
    Point zonePoint = { .v = 45, .h = 205 };
    CHECK(Chooser_HandleClick(&chooser, zonePoint, 0) == CHOOSER_ERR_NONE &&
          chooser.selectedZoneIndex == 0,
          "zone-list click did not select the default zone");
    Chooser_Shutdown(&chooser);
    RecordTest(test_name, true, "");
}

/* After SetOrigin, a point's new coordinates draw where the old ones did:
 * shapes, text-free, and the clip all move together. */
static void Test_Draw_SetOrigin(void) {
    const char* test_name = "Draw_SetOrigin";
    Rect wr = { 150, 520, 300, 700 };
    WindowPtr w = NewWindow(NULL, &wr, (ConstStr255Param)"\x06ITOrig", true, 0, (WindowPtr)-1, false, 0);
    CHECK(w, "NewWindow failed");
    GrafPtr save;
    GetPort(&save);
    SetPort((GrafPtr)w);
    EraseRect(&w->port.portRect);
    int gx = (*w->contRgn)->rgnBBox.left, gy = (*w->contRgn)->rgnBBox.top;

    SetOrigin(100, 50);
    Rect r = { 50 + 10, 100 + 10, 50 + 20, 100 + 20 };   /* old local (10,10)-(20,20) */
    PaintRect(&r);
    Point p = { 50 + 15, 100 + 15 };
    LocalToGlobal(&p);
    Rect clip = { 50, 100, 50 + 5, 100 + 5 };            /* old local (0,0)-(5,5) */
    ClipRect(&clip);
    Rect big = { 50, 100, 50 + 40, 100 + 40 };
    PaintRect(&big);                                     /* only the clip may darken */
    ClipRect(&w->port.portRect);
    SetOrigin(0, 0);

    UInt32 painted = ScreenPixel(gx + 15, gy + 15);
    UInt32 clipped = ScreenPixel(gx + 2, gy + 2);
    UInt32 outside = ScreenPixel(gx + 30, gy + 30);
    SetPort(save);
    DisposeWindow(w);

    CHECK((painted & 0x00FFFFFF) == 0, "a rectangle drew elsewhere after SetOrigin");
    CHECK(p.h == gx + 15 && p.v == gy + 15, "LocalToGlobal disagreed with drawing after SetOrigin");
    CHECK((clipped & 0x00FFFFFF) == 0 && (outside & 0x00FFFFFF) == 0x00FFFFFF,
          "ClipRect after SetOrigin clipped somewhere else");
    RecordTest(test_name, true, "");
}

/* ScrollRect moves the contents, down and right included, clears what it
 * uncovers and reports exactly that as the update region. */
static void Test_Draw_ScrollRect(void) {
    const char* test_name = "Draw_ScrollRect";
    Rect wr = { 150, 520, 300, 700 };
    WindowPtr w = NewWindow(NULL, &wr, (ConstStr255Param)"\x06ITScrl", true, 0, (WindowPtr)-1, false, 0);
    CHECK(w, "NewWindow failed");
    GrafPtr save;
    GetPort(&save);
    SetPort((GrafPtr)w);
    EraseRect(&w->port.portRect);
    int gx = (*w->contRgn)->rgnBBox.left, gy = (*w->contRgn)->rgnBBox.top;

    Rect sq = { 10, 10, 20, 20 };
    PaintRect(&sq);
    RgnHandle upd = NewRgn();
    Rect area = { 0, 0, 100, 150 };
    ScrollRect(&area, 25, 30, upd);
    Rect u = (*upd)->rgnBBox;
    DisposeRgn(upd);

    UInt32 movedTL = ScreenPixel(gx + 35, gy + 40);   /* old (10,10) */
    UInt32 movedBR = ScreenPixel(gx + 44, gy + 49);   /* old (19,19) */
    UInt32 oldSpot = ScreenPixel(gx + 15, gy + 15);
    SetPort(save);
    DisposeWindow(w);

    CHECK((movedTL & 0x00FFFFFF) == 0 && (movedBR & 0x00FFFFFF) == 0,
          "the square did not arrive whole where it was scrolled to");
    CHECK((oldSpot & 0x00FFFFFF) == 0x00FFFFFF, "the uncovered area was not cleared");
    CHECK(u.left == 0 && u.top == 0 && u.right == 150 && u.bottom == 100,
          "the update region is not the uncovered area");
    RecordTest(test_name, true, "");
}

/* A region with a hole: a rectangle less one inside it. */
static void Test_Region_Hole(void) {
    const char* test_name = "Region_Hole";
    RgnHandle a = NewRgn(), b = NewRgn();
    CHECK(a && b, "NewRgn failed");
    Rect ra = { 0, 0, 100, 100 }, rb = { 20, 20, 50, 50 };
    RectRgn(a, &ra);
    RectRgn(b, &rb);
    DiffRgn(a, b, a);
    Point inHole = { 30, 30 }, left = { 30, 10 }, below = { 60, 30 }, right = { 30, 60 }, above = { 10, 30 };
    Boolean ok = !PtInRgn(inHole, a) && PtInRgn(left, a) && PtInRgn(below, a) &&
                 PtInRgn(right, a) && PtInRgn(above, a);
    Rect probe = { 25, 25, 45, 45 };
    Boolean holeEmpty = !RectInRgn(&probe, a);
    DisposeRgn(a);
    DisposeRgn(b);
    CHECK(ok, "DiffRgn did not leave a hole surrounded by the rest");
    CHECK(holeEmpty, "RectInRgn found the rectangle inside the hole");
    RecordTest(test_name, true, "");
}

static void Test_Region_SetOperations(void) {
    const char* test_name = "Region_SetOperations";
    RgnHandle a = NewRgn(), b = NewRgn(), result = NewRgn();
    if (!a || !b || !result) {
        if (a) DisposeRgn(a);
        if (b) DisposeRgn(b);
        if (result) DisposeRgn(result);
        RecordTest(test_name, false, "NewRgn failed");
        return;
    }

    Rect rectA = { 0, 0, 60, 60 }, rectB = { 20, 20, 80, 80 };
    Point probes[] = { { 10, 10 }, { 30, 30 }, { 70, 70 }, { 90, 90 } };
    Boolean difference[4], intersection[4], unionResult[4], xorResult[4];
    RectRgn(a, &rectA);
    RectRgn(b, &rectB);

    DiffRgn(a, b, result);
    for (int i = 0; i < 4; i++) difference[i] = PtInRgn(probes[i], result);
    SectRgn(a, b, result);
    for (int i = 0; i < 4; i++) intersection[i] = PtInRgn(probes[i], result);
    UnionRgn(a, b, result);
    for (int i = 0; i < 4; i++) unionResult[i] = PtInRgn(probes[i], result);
    XorRgn(a, b, result);
    for (int i = 0; i < 4; i++) xorResult[i] = PtInRgn(probes[i], result);

    DisposeRgn(a);
    DisposeRgn(b);
    DisposeRgn(result);

    CHECK(difference[0] && !difference[1] && !difference[2] && !difference[3],
          "DiffRgn returned incorrect membership for overlapping rectangles");
    CHECK(!intersection[0] && intersection[1] && !intersection[2] && !intersection[3],
          "SectRgn returned incorrect membership for overlapping rectangles");
    CHECK(unionResult[0] && unionResult[1] && unionResult[2] && !unionResult[3],
          "UnionRgn returned incorrect membership for overlapping rectangles");
    CHECK(xorResult[0] && !xorResult[1] && xorResult[2] && !xorResult[3],
          "XorRgn returned incorrect membership for overlapping rectangles");
    RecordTest(test_name, true, "");
}

/* Repainting a window behind leaves a window inside it alone. */
static void Test_Window_RepaintAroundInner(void) {
    const char* test_name = "Window_RepaintAroundInner";
    Rect outerR = { 60, 300, 400, 790 };
    Rect innerR = { 150, 450, 250, 650 };
    WindowPtr outer = NewWindow(NULL, &outerR, (ConstStr255Param)"\x05Outer", true, 0, (WindowPtr)-1, false, 0);
    WindowPtr inner = NewWindow(NULL, &innerR, (ConstStr255Param)"\x05Inner", true, 0, (WindowPtr)-1, false, 0);
    CHECK(outer && inner, "NewWindow failed");
    PaintContent(inner, true);
    int ix = (*inner->contRgn)->rgnBBox.left + 20, iy = (*inner->contRgn)->rgnBBox.top + 20;
    UInt32 before = ScreenPixel(ix, iy);
    PaintOne(outer, NULL);
    UInt32 after = ScreenPixel(ix, iy);
    DisposeWindow(inner);
    DisposeWindow(outer);
    CHECK((before & 0x00FFFFFF) == 0, "the inner window was not black to begin with");
    CHECK((after & 0x00FFFFFF) == 0, "repainting the outer window painted over the inner one");
    RecordTest(test_name, true, "");
}

/* A window without an offscreen buffer, updating, draws around a window in
 * front of it. */
static void Test_Window_UpdateWithoutBuffer(void) {
    const char* test_name = "Window_UpdateWithoutBuffer";
    Rect outerR = { 60, 300, 400, 790 };
    Rect innerR = { 150, 450, 250, 650 };
    WindowPtr outer = NewWindow(NULL, &outerR, (ConstStr255Param)"\x05Outer", true, 0, (WindowPtr)-1, false, 0);
    WindowPtr inner = NewWindow(NULL, &innerR, (ConstStr255Param)"\x05Inner", true, 0, (WindowPtr)-1, false, 0);
    CHECK(outer && inner, "NewWindow failed");
    if (outer->offscreenGWorld) {
        DisposeGWorld(outer->offscreenGWorld);
        outer->offscreenGWorld = NULL;
    }
    PaintContent(inner, true);
    int ix = (*inner->contRgn)->rgnBBox.left + 20, iy = (*inner->contRgn)->rgnBBox.top + 20;

    GrafPtr save;
    GetPort(&save);
    InvalWindowRect(outer, &outer->port.portRect);
    BeginUpdate(outer);
    SetPort((GrafPtr)outer);
    EraseRect(&outer->port.portRect);
    PenPat(&qd.gray);
    PaintRect(&outer->port.portRect);
    PenNormal();
    EndUpdate(outer);
    SetPort(save);
    UInt32 after = ScreenPixel(ix, iy);

    DisposeWindow(inner);
    DisposeWindow(outer);
    CHECK((after & 0x00FFFFFF) == 0, "updating the window behind drew over the one in front");
    RecordTest(test_name, true, "");
}

/* What a moved window uncovers is repainted, not left showing the window. */
static void Test_Window_MoveRepaintsUncovered(void) {
    const char* test_name = "Window_MoveRepaintsUncovered";
    Rect r = { 470, 560, 570, 760 };   /* over the desktop, clear of the Finder's windows */
    WindowPtr w = NewWindow(NULL, &r, (ConstStr255Param)"\x04Move", true, 0, (WindowPtr)-1, false, 0);
    CHECK(w, "NewWindow failed");
    PaintContent(w, true);
    int x = (*w->contRgn)->rgnBBox.left + 10, y = (*w->contRgn)->rgnBBox.top + 10;
    UInt32 before = ScreenPixel(x, y);
    MoveWindow(w, 560, 300, false);
    UInt32 after = ScreenPixel(x, y);
    DisposeWindow(w);
    CHECK((before & 0x00FFFFFF) == 0, "the window was not black to begin with");
    CHECK((after & 0x00FFFFFF) != 0, "the uncovered area still shows the window");
    RecordTest(test_name, true, "");
}

/* A full event queue gives up its oldest event, so a click posted after a
 * flood of events nobody asks for still arrives. */
static void Test_Event_FullQueueKeepsNewest(void) {
    const char* test_name = "Event_FullQueueKeepsNewest";
    FlushEvents(everyEvent, 0);
    PostEvent(osEvt, 0);
    CHECK(Event_QueueCount() == 1, "posted event was not counted");
    InitEvents(20);
    CHECK(Event_QueueCount() == 0, "InitEvents did not reset the active queue");

    for (int i = 0; i < 100; i++) {
        PostEvent(osEvt, 0);
    }
    OSErr err = PostEvent(mouseDown, 0x1234);
    EventRecord e;
    EventRecord available;
    UInt16 queuedBeforePeek = Event_QueueCount();
    Boolean toolboxPeek = EventAvail(mDownMask, &available) &&
                          available.what == mouseDown && available.message == 0x1234;
    Boolean osPeek = OSEventAvail((SInt16)mDownMask, &available) &&
                     available.what == mouseDown && available.message == 0x1234;
    Boolean peeksPreservedQueue = Event_QueueCount() == queuedBeforePeek;
    Boolean got = false;
    for (int i = 0; i < 4 && !got; i++) {
        got = GetNextEvent(mDownMask, &e) && e.what == mouseDown && e.message == 0x1234;
    }
    FlushEvents(everyEvent, 0);
    CHECK(err == noErr, "posting to a full queue failed");
    CHECK(toolboxPeek && osPeek, "event availability did not find the queued click");
    CHECK(peeksPreservedQueue, "event availability consumed a queued event");
    CHECK(got, "the click posted after the flood was lost");
    RecordTest(test_name, true, "");
}

static void Test_KeyboardManagerTracksKeyState(void) {
    const char* test_name = "KeyboardManager_TracksKeyState";
    KeyMap keys;

    ProcessRawKeyboardEvent(kScanCommand, true, 0, TickCount());
    GetKeys(keys);
    Boolean commandDown =
        (keys[kScanCommand / 32] & (1U << (kScanCommand % 32))) != 0;
    Boolean modifierDown = (GetModifierState() & cmdKey) != 0;

    ProcessRawKeyboardEvent(kScanCommand, false, 0, TickCount());
    GetKeys(keys);
    Boolean commandReleased =
        (keys[kScanCommand / 32] & (1U << (kScanCommand % 32))) == 0;
    Boolean modifierReleased = (GetModifierState() & cmdKey) == 0;

    CHECK(commandDown && modifierDown,
          "keyboard manager did not track the command-key press");
    CHECK(commandReleased && modifierReleased,
          "keyboard manager did not track the command-key release");
    RecordTest(test_name, true, "");
}

static void Test_CJKFontFallback(void) {
    const char* test_name = "CJKFont_TofuFallback";
    const CJKFontData* font;
    UInt8 bitmap[CJK_GLYPH_BYTES];
    SInt16 width;
    SInt16 height;
    SInt16 unsupportedScript = (ScriptCode)0x7FFF;

    InitCJKFonts();
    font = GetCJKFont(kScriptJapanese);
    CHECK(font != NULL && font->loaded,
          "supported CJK font slot was not initialized");
    CHECK(GetCJKGlyph(font, 0, bitmap, &width, &height) == noErr,
          "missing CJK glyph did not return the fallback");
    CHECK(width == CJK_GLYPH_WIDTH && height == CJK_GLYPH_HEIGHT,
          "CJK fallback dimensions were incorrect");
    CHECK(bitmap[0] == 0xFF && bitmap[1] == 0xF0 &&
          bitmap[2] == 0x80 && bitmap[3] == 0x10 &&
          bitmap[CJK_GLYPH_BYTES - 2] == 0xFF &&
          bitmap[CJK_GLYPH_BYTES - 1] == 0xF0,
          "CJK tofu glyph bitmap was incorrect");
    CHECK(GetCJKFont(unsupportedScript) == NULL &&
          LoadCJKFont(unsupportedScript) == paramErr,
          "unsupported CJK script was accepted");
    RecordTest(test_name, true, "");
}

/* Both scrolling entry points clamp to the same measured content bounds. */
static void Test_TextEditScrollBounds(void) {
    const char* test_name = "TextEdit_ScrollBounds";
    Rect rect = {0, 0, 16, 24};
    char text[64];
    SInt32 length = 0;
    for (int i = 0; i < 40; i++) text[length++] = 'W';
    const char* tail = "\rA\rB\rC\rD";
    while (*tail) text[length++] = *tail++;

    TEHandle hTE = TENew(&rect, &rect);
    if (!hTE) {
        RecordTest(test_name, false, "TENew failed");
        return;
    }

    TESetWordWrap(false, hTE);
    TESetText(text, length, hTE);
    TECalText(hTE);

    GrafPtr savedPort = NULL;
    GetPort(&savedPort);
    short savedFont = savedPort ? savedPort->txFont : chicagoFont;
    short savedSize = savedPort ? savedPort->txSize : 12;
    UInt8 savedFace = savedPort ? savedPort->txFace : normal;
    TextFont((**hTE).txFont);
    TextSize((**hTE).txSize);
    TextFace((**hTE).txFace);

    SInt16 maxHScroll = (SInt16)(CharWidth('W') * 40 - (rect.right - rect.left));
    if (maxHScroll < 0) maxHScroll = 0;
    const SInt32* lineStarts = NULL;
    SInt16 lineCount = TE_LineInfo(hTE, &lineStarts);
    SInt16 maxVScroll = (SInt16)(lineCount * (**hTE).lineHeight -
                                 (rect.bottom - rect.top));
    if (maxVScroll < 0) maxVScroll = 0;

    SInt16 dh = 0, dv = 0;
    TEScroll(32767, 32767, hTE);
    TE_GetScroll(hTE, &dh, &dv);
    Boolean directScrollOK = dh == maxHScroll && dv == maxVScroll;

    TE_SetScroll(hTE, 0, 0);
    TEPinScroll(32767, 32767, hTE);
    TE_GetScroll(hTE, &dh, &dv);
    Boolean pinnedScrollOK = dh == maxHScroll && dv == maxVScroll;

    TextFont(savedFont);
    TextSize(savedSize);
    TextFace(savedFace);
    TEDispose(hTE);

    if (!directScrollOK || !pinnedScrollOK) {
        RecordTest(test_name, false, "scroll limits did not match measured text bounds");
        return;
    }
    RecordTest(test_name, true, "");
}

/* The Calculator keeps its first operand: 7 + 8 = is 15. */
static double CalcRun(Calculator* c, const char* keys) {
    Calculator_ClearAll(c);
    for (const char* k = keys; *k; k++) Calculator_KeyPress(c, *k);
    return c->value;
}

static void Test_Calculator_Arithmetic(void) {
    const char* test_name = "Calculator_Arithmetic";
    static Calculator calc;
    CHECK(Calculator_Initialize(&calc) == 0, "Calculator_Initialize failed");
    CHECK(CalcRun(&calc, "7+8=") == 15.0, "7+8= is not 15");
    CHECK(CalcRun(&calc, "7+8*2=") == 30.0, "7+8*2= is not 30 (chained left to right)");
    CHECK(CalcRun(&calc, "9-4=") == 5.0, "9-4= is not 5");
    CHECK(CalcRun(&calc, "6/3=") == 2.0, "6/3= is not 2");
    CHECK(CalcRun(&calc, "5+*3=") == 15.0, "a second operator did not replace the first");
    Calculator_Shutdown(&calc);
    RecordTest(test_name, true, "");
}

/* A released resource loads again intact, not as the freed handle. */
static void Test_Resource_ReleaseThenGet(void) {
    const char* test_name = "Resource_ReleaseThenGet";
    Handle h1 = GetResource(FOURCC('p', 'p', 'a', 't'), 304);
    CHECK(h1 && *h1, "ppat 304 did not load");
    u32 size1 = GetHandleSize(h1);
    UInt8 head[8];
    memcpy(head, *h1, sizeof(head));
    ReleaseResource(h1);

    Handle churn[8];
    for (int i = 0; i < 8; i++) {
        churn[i] = NewHandle(13);
        if (churn[i]) memset(*churn[i], 0x5A, 13);
    }
    Handle h2 = GetResource(FOURCC('p', 'p', 'a', 't'), 304);
    Boolean same = h2 && *h2 && GetHandleSize(h2) == size1 && memcmp(*h2, head, sizeof(head)) == 0;
    for (int i = 0; i < 8; i++) if (churn[i]) DisposeHandle(churn[i]);
    if (h2) ReleaseResource(h2);
    CHECK(same, "the resource loaded after ReleaseResource was not the resource");
    RecordTest(test_name, true, "");
}

/* OpenPoly hands out the polygon, ClosePoly fills it, and lines draw again. */
static void Test_Draw_PolygonRecording(void) {
    const char* test_name = "Draw_PolygonRecording";
    Rect r = { 470, 560, 570, 760 };
    WindowPtr w = NewWindow(NULL, &r, (ConstStr255Param)"\x04Poly", true, 0, (WindowPtr)-1, false, 0);
    CHECK(w, "NewWindow failed");
    GrafPtr save;
    GetPort(&save);
    SetPort((GrafPtr)w);

    PolyHandle poly = OpenPoly();
    MoveTo(10, 10);
    LineTo(50, 10);
    LineTo(30, 40);
    LineTo(10, 10);
    PolyHandle closed = ClosePoly();
    SInt16 points = poly ? (SInt16)(((*poly)->polySize - sizeof(SInt16) - sizeof(Rect)) / sizeof(Point)) : 0;

    /* Recording over, a line draws */
    PenNormal();
    MoveTo(5, 60);
    LineTo(80, 60);
    int x = (*w->contRgn)->rgnBBox.left + 40, y = (*w->contRgn)->rgnBBox.top + 60;
    UInt32 px = ScreenPixel(x, y);

    if (poly) KillPoly(poly);
    SetPort(save);
    DisposeWindow(w);
    CHECK(poly != NULL, "OpenPoly returned no polygon");
    CHECK(closed == poly, "ClosePoly did not fill the polygon OpenPoly returned");
    CHECK(points == 4, "the polygon does not hold the four points drawn");
    CHECK((px & 0x00FFFFFF) == 0, "a line after ClosePoly did not draw");
    RecordTest(test_name, true, "");
}

/* CopyBits from a 1-bit BitMap into a window draws its black bits black. */
static void Test_Draw_CopyBits1Bit(void) {
    const char* test_name = "Draw_CopyBits1Bit";
    Rect r = { 470, 560, 570, 760 };
    WindowPtr w = NewWindow(NULL, &r, (ConstStr255Param)"\x04Bits", true, 0, (WindowPtr)-1, false, 0);
    CHECK(w, "NewWindow failed");

    static UInt8 bits[16 * 16];
    memset(bits, 0, sizeof bits);
    memset(bits + 5 * 16, 0xFF, 16);          /* row 5 black */
    BitMap bm;
    bm.baseAddr = (Ptr)bits;
    bm.rowBytes = 16;
    SetRect(&bm.bounds, 0, 0, 128, 16);

    GrafPtr save;
    GetPort(&save);
    SetPort((GrafPtr)w);
    Rect dst = { 10, 10, 26, 138 };
    CopyBits(&bm, &((GrafPtr)w)->portBits, &bm.bounds, &dst, srcCopy, NULL);
    SetPort(save);

    int x = (*w->contRgn)->rgnBBox.left + 20;
    int y = (*w->contRgn)->rgnBBox.top;
    UInt32 black = ScreenPixel(x, y + 15), white = ScreenPixel(x, y + 12);
    DisposeWindow(w);
    CHECK((black & 0x00FFFFFF) == 0, "a set bit did not draw black");
    CHECK((white & 0x00FFFFFF) == 0x00FFFFFF, "a clear bit did not draw white");
    RecordTest(test_name, true, "");
}

static void Test_Cursor_VisibilityAndObscure(void) {
    const char* test_name = "Cursor_VisibilityAndObscure";
    Point mouse, moved;

    InitCursor();
    Boolean initializedVisible = IsCursorVisible();
    GetMouse(&mouse);
    CursorManager_HandleMouseMotion(mouse);

    HideCursor();
    HideCursor();
    Boolean hidden = !IsCursorVisible();
    ShowCursor();
    Boolean nestedHidePreserved = !IsCursorVisible();
    ShowCursor();
    Boolean shown = IsCursorVisible();

    ObscureCursor();
    Boolean obscured = !IsCursorVisible();
    moved = mouse;
    moved.h = moved.h == 32767 ? moved.h - 1 : moved.h + 1;
    CursorManager_HandleMouseMotion(moved);
    Boolean revealed = IsCursorVisible();

    InitCursor();
    CHECK(initializedVisible, "InitCursor did not make the cursor visible");
    CHECK(hidden, "HideCursor did not hide the cursor");
    CHECK(nestedHidePreserved, "ShowCursor cleared more than one hide level");
    CHECK(shown, "balanced ShowCursor calls did not reveal the cursor");
    CHECK(obscured, "ObscureCursor did not hide the cursor");
    CHECK(revealed, "mouse movement did not reveal the obscured cursor");
    RecordTest(test_name, true, "");
}

/* A 68K application's heap: handles follow their blocks when they grow,
 * RecoverHandle finds the master pointer, flags live in its top byte. */
static void Test_M68K_Heap(void) {
    const char* test_name = "M68K_Heap";
    const ICPUBackend* be = CPUBackend_Get("m68k_interp");
    CHECK(be, "no 68K backend");
    CPUAddressSpace cas = NULL;
    CHECK(be->CreateAddressSpace(NULL, &cas) == noErr, "CreateAddressSpace failed");
    M68KAddressSpace* as = (M68KAddressSpace*)cas;
    CPUAddr base = 0;
    CHECK(be->AllocateMemory(cas, 64 * 1024, kCPUMapA5World, &base) == noErr, "no heap memory");
    M68KHeap_Init(as, base, 64 * 1024);

    UInt32 h = M68KHeap_NewHandle(16, true);
    UInt32 p = M68KHeap_NewPtr(32, false);     /* right after it: h cannot grow in place */
    Boolean ok = h && p && M68KHeap_GetHandleSize(h) == 16;
    UInt32 first = ok ? M68KHeap_Deref(h) : 0;
    if (ok) {
        M68K_Write32(as, first, 0x12345678);
        ok = M68KHeap_SetHandleSize(h, 4000) == noErr && M68KHeap_GetHandleSize(h) == 4000;
    }
    Boolean moved = ok && M68KHeap_Deref(h) != first;
    Boolean kept = ok && M68K_Read32(as, M68KHeap_Deref(h)) == 0x12345678;
    Boolean recovered = ok && M68KHeap_RecoverHandle(M68KHeap_Deref(h)) == h;
    UInt32 after = ok ? M68KHeap_NewPtr(100, false) : 0;   /* now h cannot grow in place */
    if (ok) M68KHeap_SetState(h, 0x80);
    Boolean locked = ok && M68KHeap_GetState(h) == 0x80 && M68KHeap_Deref(h) != 0;
    Boolean lockedStays = ok && M68KHeap_SetHandleSize(h, 60000) == memFullErr;
    Boolean freed = ok && M68KHeap_DisposeHandle(h) == noErr && M68KHeap_DisposePtr(p) == noErr &&
                    M68KHeap_DisposePtr(after) == noErr && M68KHeap_FreeBytes() > 60000;
    be->DestroyAddressSpace(cas);

    CHECK(ok, "allocation or growth failed");
    CHECK(moved, "the block did not move when it could not grow in place");
    CHECK(kept, "growing lost the contents");
    CHECK(recovered, "RecoverHandle did not find the master pointer");
    CHECK(locked, "the lock flag is not in the master pointer's top byte");
    CHECK(lockedStays, "a locked handle was moved");
    CHECK(freed, "disposing did not give the memory back");
    RecordTest(test_name, true, "");
}

static void Test_Resource_CreateAndOpenResFile(void) {
    const char* test_name = "Resource_CreateAndOpenResFile";
    FSSpec spec;
    SetSpec(&spec, "ITest Resources");

    FSpCreateResFile(&spec, FOURCC('I', 'T', 's', 't'), FOURCC('r', 's', 'r', 'c'), 0);
    OSErr createErr = ResError();
    if (createErr != noErr) IT_LOG_INFO("FSpCreateResFile: ResError %d", createErr);
    CHECK(createErr == noErr, "FSpCreateResFile reported an error");

    SInt16 ref = FSpOpenResFile(&spec, 1);
    CHECK(ref > 0, "the file just created would not open as a resource file");
    CHECK(ResError() == noErr, "FSpOpenResFile opened it and reported an error");
    CloseResFile(ref);
    FSDelete(spec.name, spec.vRefNum);
    RecordTest(test_name, true, "");
}

/* Added, closed, opened again: the resource is in the file. Then changed,
 * renamed and removed, each surviving a close. */
static void Test_Resource_WriteAndReadBack(void) {
    const char* test_name = "Resource_WriteAndReadBack";
    FSSpec spec;
    SetSpec(&spec, "ITest Written");
    FSpCreateResFile(&spec, FOURCC('I', 'T', 's', 't'), FOURCC('r', 's', 'r', 'c'), 0);
    SInt16 saved = CurResFile();

    SInt16 ref = FSpOpenResFile(&spec, 3);
    CHECK(ref > 0, "could not open the new file");
    Handle h = NewHandle(5);
    BlockMoveData("hello", *h, 5);
    AddResource(h, FOURCC('I', 'T', 's', 't'), 200, PSTR("greeting"));
    CHECK(ResError() == noErr, "AddResource failed");
    Handle h2 = NewHandle(3);
    BlockMoveData("bye", *h2, 3);
    AddResource(h2, FOURCC('I', 'T', 's', 't'), 201, NULL);
    CloseResFile(ref);
    CHECK(ResError() == noErr, "CloseResFile could not write the file");

    ref = FSpOpenResFile(&spec, 3);
    Handle back = Get1Resource(FOURCC('I', 'T', 's', 't'), 200);
    CHECK(back && GetHandleSize(back) == 5 && memcmp(*back, "hello", 5) == 0,
          "the resource did not come back from the file");
    Str255 name;
    ResID id = 0;
    ResType type = 0;
    GetResInfo(back, &id, &type, (char*)name);
    CHECK(id == 200 && type == FOURCC('I', 'T', 's', 't') && name[0] == 8 && memcmp(name + 1, "greeting", 8) == 0,
          "GetResInfo did not give its ID, type and name");
    char cName[EXTENSION_RESOURCE_NAME_SIZE];
    Extension_GetResourceInfo(back, NULL, NULL, cName);
    CHECK(strcmp(cName, "greeting") == 0,
          "Extension Manager did not convert the Pascal resource name");
    Handle loaded = NULL;
    char loadedName[EXTENSION_RESOURCE_NAME_SIZE];
    CHECK(Extension_LoadResource(FOURCC('I', 'T', 's', 't'), 200,
                                 &loaded, loadedName) == noErr &&
          loaded == back && strcmp(loadedName, "greeting") == 0,
          "Extension Manager did not load a named resource");
    Handle missing = (Handle)1;
    char missingName[EXTENSION_RESOURCE_NAME_SIZE];
    CHECK(Extension_LoadResource(FOURCC('I', 'T', 's', 't'), 999,
                                 &missing, missingName) == extBadResource &&
          missing == NULL && missingName[0] == '\0',
          "Extension Manager did not clear a missing resource result");
    CHECK(Count1Resources(FOURCC('I', 'T', 's', 't')) == 2, "Count1Resources did not count both");

    /* Changed: longer, and renumbered */
    SetHandleSize(back, 7);
    BlockMoveData("goodbye", *back, 7);
    ChangedResource(back);
    SetResInfo(back, 300, PSTR("farewell"));
    RemoveResource(Get1Resource(FOURCC('I', 'T', 's', 't'), 201));
    UpdateResFile(ref);
    CHECK(ResError() == noErr, "UpdateResFile could not write the file");
    CloseResFile(ref);

    ref = FSpOpenResFile(&spec, 3);
    back = Get1Resource(FOURCC('I', 'T', 's', 't'), 300);
    CHECK(back && GetHandleSize(back) == 7 && memcmp(*back, "goodbye", 7) == 0,
          "the changed resource did not come back");
    CHECK(Get1Resource(FOURCC('I', 'T', 's', 't'), 200) == NULL, "the old ID is still there");
    CHECK(Get1Resource(FOURCC('I', 'T', 's', 't'), 201) == NULL, "the removed resource is still there");
    CloseResFile(ref);
    UseResFile(saved);
    FSDelete(spec.name, spec.vRefNum);
    RecordTest(test_name, true, "");
}

static void Test_Resource_OpenMissingResFile(void) {
    const char* test_name = "Resource_OpenMissingResFile";
    FSSpec spec;
    SetSpec(&spec, "ITest No Such File");

    SInt16 ref = FSpOpenResFile(&spec, 1);
    CHECK(ref == -1, "opened a file that does not exist");
    CHECK(ResError() != noErr, "failed to open and reported no error");
    RecordTest(test_name, true, "");
}

/* ----------------------------------------------------------------------------
 * Math library
 * ------------------------------------------------------------------------- */

/* Within 1e-12 of the expected value, relative (absolute near zero). */
static Boolean Near(double got, double want) {
    double diff = fabs(got - want);
    double scale = fabs(want) > 1.0 ? fabs(want) : 1.0;
    return diff <= 1e-12 * scale;
}

static void Test_Math_Accuracy(void) {
    const char* test_name = "Math_Accuracy";
    const double pi = 3.14159265358979323846;
    /* The values the old series got furthest wrong come first. */
    CHECK(Near(cos(3.1), -0.99913515027327948), "cos(3.1)");
    CHECK(Near(sin(3.0), 0.14112000805986721), "sin(3.0)");
    CHECK(Near(atan(1.0), pi / 4), "atan(1)");
    CHECK(Near(exp(-20.0), 2.0611536224385579e-09), "exp(-20)");
    CHECK(Near(exp(50.0), 5.1847055285870724e+21), "exp(50)");
    CHECK(Near(asin(0.999), 1.526071239626163), "asin(0.999)");
    CHECK(Near(sin(pi / 6), 0.5) && Near(cos(pi / 3), 0.5), "sin/cos of 30/60 degrees");
    CHECK(Near(sin(100.0), -0.50636564110975879), "sin(100)");
    CHECK(Near(tan(1.0), 1.5574077246549023), "tan(1)");
    CHECK(Near(atan2(-1.0, -1.0), -3 * pi / 4), "atan2(-1,-1)");
    CHECK(Near(atan2(1.0, 0.0), pi / 2), "atan2(1,0)");
    CHECK(Near(acos(-1.0), pi) && Near(asin(1.0), pi / 2), "acos(-1), asin(1)");
    CHECK(Near(exp(1.0), 2.7182818284590452), "exp(1)");
    CHECK(Near(sqrt(2.0), 1.4142135623730951) && sqrt(0.0) == 0.0, "sqrt");
    CHECK(Near(log(10.0), 2.3025850929940457), "log(10)");
    CHECK(Near(log(1e-300), -690.77552789821368), "log(1e-300)");
    CHECK(Near(log10(1000.0), 3.0), "log10(1000)");
    CHECK(floor(-2.5) == -3.0 && floor(2.5) == 2.0 && floor(1e20) == 1e20, "floor");
    CHECK(fabs(-0.0) == 0.0 && fabs(-3.5) == 3.5, "fabs");
    CHECK(ceil(-2.5) == -2.0 && ceil(2.1) == 3.0, "ceil");
    CHECK(pow(2.0, 10.0) == 1024.0 && pow(-2.0, 3.0) == -8.0 && pow(2.0, -2.0) == 0.25, "pow, whole exponents");
    CHECK(Near(pow(2.0, 0.5), 1.4142135623730951) && Near(pow(10.0, 2.5), 316.22776601683796), "pow, fractional exponents");
    CHECK(isnan(pow(-2.0, 0.5)), "pow of a negative base to a fraction");
    CHECK(isnan(log(-1.0)) && isnan(asin(2.0)) && !isnan(1.0), "NaN for domain errors");
    RecordTest(test_name, true, "");
}

/* ----------------------------------------------------------------------------
 * Running and reporting
 * ------------------------------------------------------------------------- */

static void PrintTestSummary(void) {
    IT_LOG_INFO("%s", "");
    IT_LOG_INFO("============================================");
    IT_LOG_INFO("INTEGRATION TEST SUMMARY");
    IT_LOG_INFO("============================================");
    IT_LOG_INFO("Total tests: %d", test_count);
    IT_LOG_INFO("Passed:      %d", test_pass);
    IT_LOG_INFO("Failed:      %d", test_fail);
    IT_LOG_INFO("============================================");

    if (test_fail > 0) {
        IT_LOG_WARN("SOME TESTS FAILED - See details below:");
        for (int i = 0; i < result_count; i++) {
            if (!results[i].passed) {
                IT_LOG_FAIL("[%s] %s", results[i].name, results[i].reason);
            }
        }
    } else if (test_count > 0) {
        IT_LOG_PASS("ALL TESTS PASSED!");
    }
    IT_LOG_INFO("============================================");
    IT_LOG_INFO("%s", "");
}

static void PutBE16(UInt8* p, UInt16 v) { p[0] = v >> 8; p[1] = v; }
static void PutBE32(UInt8* p, UInt32 v) { PutBE16(p, v >> 16); PutBE16(p + 2, v); }

/* Lays out a MacBinary II archive as tests/m68k/mkapp.py writes one: forks
 * padded to 128 bytes, header CRC at 124. Returns its length. */
static UInt32 BuildMacBinary(UInt8* a, const UInt8* data, UInt32 dataLen,
                             const UInt8* rsrc, UInt32 rsrcLen) {
    memset(a, 0, 128);
    a[1] = 5;
    memcpy(a + 2, "Hello", 5);
    memcpy(a + 65, "APPL", 4);
    memcpy(a + 69, "DMSE", 4);
    PutBE32(a + 83, dataLen);
    PutBE32(a + 87, rsrcLen);
    a[122] = kMacBinVersionII;
    a[123] = kMacBinVersionII;
    PutBE16(a + 124, MacBinary_CRC16(a, 124));
    UInt32 dataPadded = (dataLen + 127) & ~127u;
    memset(a + 128, 0, dataPadded);
    memcpy(a + 128, data, dataLen);
    memcpy(a + 128 + dataPadded, rsrc, rsrcLen);
    return 128 + dataPadded + rsrcLen;
}

static void Test_SegmentLoader_RevivePurgeableMapping(void) {
    const char* test_name = "SegmentLoader_RevivePurgeableMapping";
    SegmentLoaderContext ctx;
    memset(&ctx, 0, sizeof ctx);
    ctx.initialized = true;
    ctx.numSegments = 1;

    CodeSegment* segment = &ctx.segments[0];
    segment->handle = (CPUCodeHandle)(void*)&ctx;
    segment->baseAddr = 0x123400;
    segment->entryAddr = 0x123420;
    segment->state = kSegmentLoaded;
    segment->refCount = 1;
    CPUCodeHandle originalHandle = segment->handle;
    CPUAddr originalBase = segment->baseAddr;

    OSErr unloadErr = UnloadSegment(&ctx, 0);
    OSErr reloadErr = LoadSegment(&ctx, 0);
    Boolean revived = segment->state == kSegmentLoaded && !segment->purgeable &&
                      segment->refCount == 1 && segment->handle == originalHandle &&
                      segment->baseAddr == originalBase;

    CHECK(unloadErr == noErr, "UnloadSegment failed");
    CHECK(reloadErr == noErr, "LoadSegment did not revive the resident mapping");
    CHECK(revived, "reviving the mapping replaced or corrupted its descriptor");
    RecordTest(test_name, true, "");
}

static void Test_MacBinary_Unpack(void) {
    const char* test_name = "MacBinary_Unpack";
    static UInt8 archive[1024];
    UInt8 rsrc[16 + 28];
    const UInt8 data[] = {0x4E, 0x71, 0x4E, 0x75};
    MacBinaryArchive out;

    /* An empty resource fork: header, no data, a bare 28-byte map. */
    memset(rsrc, 0, sizeof(rsrc));
    PutBE32(rsrc + 0, 16);
    PutBE32(rsrc + 4, 16);
    PutBE32(rsrc + 8, 0);
    PutBE32(rsrc + 12, 28);

    UInt32 size = BuildMacBinary(archive, data, sizeof(data), rsrc, sizeof(rsrc));
    CHECK(MacBinary_IsMacBinary(archive, size), "MacBinary II archive not recognised");
    CHECK(MacBinary_Parse(archive, size, &out) == noErr, "MacBinary II archive did not parse");
    CHECK(out.header.version == kMacBinVersionII, "version not read from byte 122");
    CHECK(out.header.fileName[0] == 5 && memcmp(out.header.fileName + 1, "Hello", 5) == 0,
          "file name wrong");
    CHECK(memcmp(out.header.fileTypeId, "APPL", 4) == 0, "type not read from byte 65");
    CHECK(memcmp(out.header.fileCreator, "DMSE", 4) == 0, "creator not read from byte 69");
    CHECK(out.dataFork == archive + 128 && out.dataForkSize == sizeof(data),
          "data fork not at 128");
    CHECK(out.resourceFork == archive + 256 && out.resourceForkSize == sizeof(rsrc),
          "resource fork not at the next 128-byte boundary");

    /* MacBinary I has no CRC; the zero bytes and fork lengths identify it. */
    memset(archive + 99, 0, 29);
    CHECK(MacBinary_Parse(archive, size, &out) == noErr && out.header.version == 0,
          "MacBinary I archive not accepted");
    archive[82] = 1;
    CHECK(!MacBinary_IsMacBinary(archive, size), "byte 82 set and no CRC was accepted");

    /* Text that happens to start with a zero byte is not MacBinary. */
    size = BuildMacBinary(archive, data, sizeof(data), rsrc, sizeof(rsrc));
    archive[0] = 'H';
    CHECK(!MacBinary_IsMacBinary(archive, size), "nonzero byte 0 accepted");

    /* Forks that run past the end of the file. */
    size = BuildMacBinary(archive, data, sizeof(data), rsrc, sizeof(rsrc));
    CHECK(MacBinary_Parse(archive, size - 1, &out) != noErr, "truncated resource fork accepted");
    PutBE32(archive + 83, 0xFFFFFF80);
    PutBE16(archive + 124, MacBinary_CRC16(archive, 124));
    CHECK(MacBinary_Parse(archive, size, &out) != noErr, "wrapping data fork length accepted");

    /* A resource map that points outside the fork. */
    PutBE32(rsrc + 4, 17);
    size = BuildMacBinary(archive, data, sizeof(data), rsrc, sizeof(rsrc));
    CHECK(MacBinary_Parse(archive, size, &out) == mapReadErr, "resource map outside the fork accepted");

    CHECK(MacBinary_Parse(NULL, size, &out) != noErr && MacBinary_Parse(archive, size, NULL) != noErr,
          "NULL accepted");
    RecordTest(test_name, true, "");
}

/* An archive on disk unpacks into the file it carries, beside it, and a
 * second unpack does not overwrite the first. */
static void Test_MacBinary_UnpackFile(void) {
    const char* test_name = "MacBinary_UnpackFile";
    static UInt8 archive[1024];
    UInt8 rsrc[16 + 28];
    const UInt8 data[] = {'d', 'a', 't', 'a'};
    FSSpec spec, out;
    SetSpec(&spec, "ITest Archive.bin");
    static const UInt8 kName[] = "\005Hello";
    static const UInt8 kName1[] = "\007Hello.1";
    FSDelete(spec.name, 0);
    FSDelete(kName, 0);
    FSDelete(kName1, 0);

    memset(rsrc, 0, sizeof(rsrc));
    PutBE32(rsrc + 0, 16);
    PutBE32(rsrc + 4, 16);
    PutBE32(rsrc + 12, 28);
    UInt32 size = BuildMacBinary(archive, data, sizeof(data), rsrc, sizeof(rsrc));
    archive[73] = 0x01;      /* kHasBeenInited's byte: a sender's Finder state */
    archive[101] = 0x01;     /* kIsOnDesk */
    PutBE16(archive + 124, MacBinary_CRC16(archive, 124));

    CHECK(FSCreate(spec.name, 0, FOURCC('B', 'I', 'N', 'A'), FOURCC('B', 'I', 'N', 'A')) == noErr, "FSCreate failed");
    FileRefNum ref = 0;
    CHECK(FSOpen(spec.name, 0, &ref) == noErr, "FSOpen failed");
    UInt32 n = size;
    OSErr err = FSWrite(ref, &n, archive);
    FSClose(ref);
    CHECK(err == noErr && n == size, "writing the archive failed");

    CHECK(MacBinary_IsMacBinaryFile(0, 0, spec.name), "archive on disk not recognised");
    err = MacBinary_UnpackFile(0, 0, spec.name, &out);
    Boolean named = err == noErr && memcmp(out.name, kName, sizeof(kName) - 1) == 0;

    FInfo info;
    memset(&info, 0, sizeof info);
    OSErr infoErr = FSGetFInfo(kName, 0, &info);
    UInt8 back[sizeof(data)];
    UInt32 dataEOF = 0, rsrcEOF = 0;
    n = sizeof(back);                   /* the fork's length: past it is eofErr */
    Boolean dataOK = FSOpen(kName, 0, &ref) == noErr;
    if (dataOK) {
        dataOK = FSGetEOF(ref, &dataEOF) == noErr && FSRead(ref, &n, back) == noErr &&
                 dataEOF == sizeof(data) && memcmp(back, data, sizeof(data)) == 0;
        FSClose(ref);
    }
    Boolean rsrcOK = FSOpenRF(kName, 0, &ref) == noErr;
    if (rsrcOK) {
        rsrcOK = FSGetEOF(ref, &rsrcEOF) == noErr && rsrcEOF == sizeof(rsrc);
        FSClose(ref);
    }
    OSErr again = MacBinary_UnpackFile(0, 0, spec.name, &out);
    Boolean named1 = again == noErr && memcmp(out.name, kName1, sizeof(kName1) - 1) == 0;
    Boolean archiveKept = MacBinary_IsMacBinaryFile(0, 0, spec.name);
    Boolean textIsNot = !MacBinary_IsMacBinaryFile(0, 0, kName);

    FSDelete(spec.name, 0);
    FSDelete(kName, 0);
    FSDelete(kName1, 0);
    CHECK(err == noErr, "unpacking failed");
    CHECK(named, "unpacked file not named from the header");
    CHECK(infoErr == noErr && info.fdType == FOURCC('A', 'P', 'P', 'L') && info.fdCreator == FOURCC('D', 'M', 'S', 'E'),
          "type and creator not carried over");
    CHECK((info.fdFlags & kMacBinSenderFinderFlags) == 0, "sender's Finder state kept");
    CHECK(dataOK, "data fork not written");
    CHECK(rsrcOK, "resource fork not written");
    CHECK(named1, "second unpack did not take the next free name");
    CHECK(archiveKept, "the archive was changed");
    CHECK(textIsNot, "an unpacked file was taken for an archive");
    RecordTest(test_name, true, "");
}

/* Enqueue, Dequeue, GetAppParms, UnloadSeg, the package calls, OSEventAvail
 * and SysError, called by a 68K program (M68KToolboxTest.c) */
extern Boolean M68KToolbox_RunTrapTest(const char** why);
extern Boolean M68KToolbox_RunCMPFlagsTest(const char** why);

extern Boolean M68KToolbox_RunSANETest(const char** why);

/* Arithmetic, conversion, comparison and formatting through _FP68K and
 * _Pack7, by a 68K program */
static void Test_M68K_SANE(void) {
    const char* test_name = "M68K_SANE";
    const char* why = "";
    CHECK(M68KToolbox_RunSANETest(&why), why);
    RecordTest(test_name, true, "");
}

extern Boolean M68KToolbox_RunListTest(const char** why);

/* A list in a window, built, read, searched and taken apart through _Pack0
 * by a 68K program */
static void Test_M68K_Lists(void) {
    const char* test_name = "M68K_Lists";
    const char* why = "";
    CHECK(M68KToolbox_RunListTest(&why), why);
    RecordTest(test_name, true, "");
}

extern Boolean M68KToolbox_RunWindowTest(const char** why);

/* KeyTrans through a KCHR, a window's picture, and DragGrayRgn */
static void Test_M68K_WindowCalls(void) {
    const char* test_name = "M68K_WindowCalls";
    const char* why = "";
    CHECK(M68KToolbox_RunWindowTest(&why), why);
    RecordTest(test_name, true, "");
}

extern Boolean M68KToolbox_RunTimerTest(const char** why);

/* A VBL task and a Time Manager task, run while a 68K program waits */
static void Test_M68K_Timers(void) {
    const char* test_name = "M68K_Timers";
    const char* why = "";
    CHECK(M68KToolbox_RunTimerTest(&why), why);
    RecordTest(test_name, true, "");
}

extern Boolean M68KToolbox_RunIconTest(const char** why);

/* An icon drawn through its mask, plain and selected, by _IconDispatch */
static void Test_M68K_Icons(void) {
    const char* test_name = "M68K_Icons";
    const char* why = "";
    CHECK(M68KToolbox_RunIconTest(&why), why);
    RecordTest(test_name, true, "");
}

extern Boolean M68KToolbox_Run68020Test(const char** why);

/* The 68020's instructions and addressing modes, run by a 68K program */
static void Test_M68K_68020(void) {
    const char* test_name = "M68K_68020";
    const char* why = "";
    CHECK(M68KToolbox_Run68020Test(&why), why);
    RecordTest(test_name, true, "");
}

static void Test_M68K_Traps(void) {
    const char* test_name = "M68K_Traps";
    const char* why = "";
    CHECK(M68KToolbox_RunTrapTest(&why), why);
    RecordTest(test_name, true, "");
}

static void Test_M68K_CMPFlags(void) {
    const char* test_name = "M68K_CMPFlags";
    const char* why = "";
    CHECK(M68KToolbox_RunCMPFlagsTest(&why), why);
    RecordTest(test_name, true, "");
}

/* ------------------------------------------------------------------------
 * _Launch: one 68K application starting another
 *
 * Two applications are made here, each a CODE 0 and a CODE 1 (as
 * tests/m68k/mkapp.py makes them): the parent _Launches the child by name,
 * and the child creates a file. The file is there afterwards only if the
 * parent ran, launched, ended, and the child ran in its place.
 * ------------------------------------------------------------------------ */

static Handle HandleWith(const UInt8* bytes, Size n) {
    Handle h = NewHandle(n);
    if (h) BlockMoveData(bytes, *h, n);
    return h;
}

static Boolean MakeApplication(const char* name, const UInt8* code, Size codeLen) {
    FSSpec spec;
    Str255 pname;
    pname[0] = (UInt8)strlen(name);
    memcpy(pname + 1, name, pname[0]);
    FSMakeFSSpec(0, 0, pname, &spec);
    FSpDelete(&spec);
    FSpCreateResFile(&spec, FOURCC('I', 'T', 's', 't'), FOURCC('A', 'P', 'P', 'L'), 0);
    SInt16 ref = FSpOpenResFile(&spec, 3);
    if (ref <= 0) return false;

    /* CODE 0: above A5 (the jump table), below A5, the table's size and its
     * offset from A5 - then the table itself: one entry, CODE 1's offset 0,
     * by _LoadSeg */
    UInt8 code0[16 + 8];
    PutBE32(code0 + 0, 32 + 8);
    PutBE32(code0 + 4, 0x400);
    PutBE32(code0 + 8, 8);
    PutBE32(code0 + 12, 32);
    PutBE16(code0 + 16, 0);
    PutBE16(code0 + 18, 0x3F3C);
    PutBE16(code0 + 20, 1);
    PutBE16(code0 + 22, 0xA9F0);
    UInt8 code1[256];
    PutBE16(code1, 0);                                  /* first entry's offset */
    PutBE16(code1 + 2, 1);                              /* one entry */
    memcpy(code1 + 4, code, (size_t)codeLen);
    AddResource(HandleWith(code0, sizeof(code0)), FOURCC('C', 'O', 'D', 'E'), 0, NULL);
    AddResource(HandleWith(code1, codeLen + 4), FOURCC('C', 'O', 'D', 'E'), 1, NULL);
    CloseResFile(ref);
    return ResError() == noErr;
}

static void Test_M68K_Launch(void) {
    const char* test_name = "M68K_Launch";
    static const char kMark[] = "ITest Launched";

    /* Child: LEA pb(PC),A0; LEA name(PC),A1; MOVE.L A1,18(A0); _Create;
     * _ExitToShell; pb: 80 bytes; name: "ITest Launched" */
    UInt8 child[16 + 80 + 16];
    memset(child, 0, sizeof(child));
    static const UInt16 kChild[] = { 0x41FA, 14, 0x43FA, 90, 0x2149, 18, 0xA008, 0xA9F4 };
    for (int i = 0; i < 8; i++) PutBE16(child + 2 * i, kChild[i]);
    child[96] = (UInt8)(sizeof(kMark) - 1);
    memcpy(child + 97, kMark, sizeof(kMark) - 1);

    /* Parent: LEA pb(PC),A0; LEA name(PC),A1; MOVE.L A1,(A0); _Launch;
     * _ExitToShell; NOP; pb: name pointer and configuration; name */
    UInt8 parent[24 + 12];
    memset(parent, 0, sizeof(parent));
    static const UInt16 kParent[] = { 0x41FA, 14, 0x43FA, 18, 0x2089, 0xA9F2, 0xA9F4, 0x4E71 };
    for (int i = 0; i < 8; i++) PutBE16(parent + 2 * i, kParent[i]);
    parent[24] = 11;
    memcpy(parent + 25, "ITest Child", 11);

    FSSpec mark;
    Str255 pmark;
    pmark[0] = (UInt8)(sizeof(kMark) - 1);
    memcpy(pmark + 1, kMark, pmark[0]);
    FSMakeFSSpec(0, 0, pmark, &mark);
    FSpDelete(&mark);

    Boolean made = MakeApplication("ITest Child", child, sizeof(child)) &&
                   MakeApplication("ITest Parent", parent, sizeof(parent));
    OSErr err = paramErr;
    FSSpec app;
    if (made) {
        FSMakeFSSpec(0, 0, PSTR("ITest Parent"), &app);
        LaunchParamBlockRec lp;
        memset(&lp, 0, sizeof(lp));
        lp.launchAppSpec = &app;
        lp.launchPreferredSize = 512 * 1024;
        err = LaunchApplication(&lp);
    }
    FInfo info;
    Boolean childRan = FSpGetFInfo(&mark, &info) == noErr;

    FSpDelete(&mark);
    FSSpec gone;
    FSMakeFSSpec(0, 0, PSTR("ITest Child"), &gone);
    FSpDelete(&gone);
    FSMakeFSSpec(0, 0, PSTR("ITest Parent"), &gone);
    FSpDelete(&gone);
    CHECK(made, "could not make the two applications");
    CHECK(err == noErr, "LaunchApplication failed");
    CHECK(childRan, "the launched application did not run");
    RecordTest(test_name, true, "");
}

void IntegrationTests_Run(void) {
    IT_LOG_INFO("%s", "");
    IT_LOG_INFO("============================================");
    IT_LOG_INFO("SYSTEM 7 INTEGRATION TEST SUITE");
    IT_LOG_INFO("============================================");

    IT_LOG_INFO("--- Memory Manager ---");
    Test_Memory_HandleStateRoundTrip();
    Test_Memory_LogicalSizes();

    IT_LOG_INFO("--- Date & Time ---");
    Test_DateTime_CalendarConversions();

    IT_LOG_INFO("--- Dialog Manager ---");
    Test_Dialog_ParseDLOG();
    Test_Dialog_ParseALRT();
    Test_Dialog_ParseDLOGTruncated();
    Test_Dialog_ParseDITLTruncatedAfterText();
    Test_Dialog_ParseDITLRejectsNegativeLongLength();
    Test_Dialog_LoadMissingTemplate();
    Test_Dialog_ActionDebounce();

    IT_LOG_INFO("--- Math ---");
    Test_Math_Accuracy();

    IT_LOG_INFO("--- File Manager ---");
    Test_File_WriteReadRoundTrip();
    Test_File_Metadata();
    Test_File_FoldersAndWorkingDirectories();
    Test_VFS_ApplicationsCatalog();
    Test_File_InFolder();
    Test_Draw_ClippedToVisibleRegion();
    Test_Draw_PenModes();
    Test_Draw_SetOrigin();
    Test_Draw_ScrollRect();
    Test_Region_Hole();
    Test_Region_SetOperations();
    Test_Window_RepaintAroundInner();
    Test_Window_UpdateWithoutBuffer();
    Test_Window_MoveRepaintsUncovered();
    Test_Event_FullQueueKeepsNewest();
    Test_KeyboardManagerTracksKeyState();
    Test_CJKFontFallback();
    Test_TextEditScrollBounds();
    Test_Calculator_Arithmetic();
    Test_Resource_ReleaseThenGet();
    Test_Draw_PolygonRecording();
    Test_Draw_CopyBits1Bit();
    Test_Cursor_VisibilityAndObscure();
    Test_M68K_Heap();
    Test_Dialog_AlertLayout();
    Test_Dialog_IconItem();
    Test_Dialog_EditTextFocusBeyond32Items();
    Test_Chooser_InitializeLayout();
    Test_Window_ReorderAndHide();
    Test_Dialog_NestedModalWindowState();
    Test_Window_MoveAndZoom();
    Test_File_ReadThroughExtentsOverflow();

    IT_LOG_INFO("--- Resource Manager ---");
    Test_Resource_CreateAndOpenResFile();
    Test_Resource_WriteAndReadBack();
    Test_Resource_OpenMissingResFile();

    IT_LOG_INFO("--- Segment Loader ---");
    Test_SegmentLoader_RevivePurgeableMapping();
    Test_MacBinary_Unpack();
    Test_MacBinary_UnpackFile();

    IT_LOG_INFO("--- 68K traps ---");
    Test_M68K_CMPFlags();
    Test_M68K_Traps();
    Test_M68K_SANE();
    Test_M68K_Lists();
    Test_M68K_WindowCalls();
    Test_M68K_Timers();
    Test_M68K_Icons();
    Test_M68K_Launch();
    Test_M68K_68020();

    PrintTestSummary();
}

OSErr IntegrationTests_Initialize(void) {
    IT_LOG_INFO("Initializing Integration Tests...");
    test_count = 0;
    test_pass = 0;
    test_fail = 0;
    result_count = 0;
    return noErr;
}

void IntegrationTests_Cleanup(void) {
    IT_LOG_INFO("Integration Tests cleanup complete");
}
