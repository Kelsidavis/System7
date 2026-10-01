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
#include "Errors/ErrorCodes.h"
#include "FileManager_Internal.h"
#include "System71StdLib.h"
#include "MemoryMgr/MemoryManager.h"
#include "DialogManager/DialogResources.h"
#include "ResourceManager.h"
#include "WindowManager/WindowManager.h"
#include "QuickDraw/QuickDraw.h"
#include "QuickDrawConstants.h"
#include "DialogManager/DialogManager.h"
#include "DialogManager/AlertDialogs.h"
#include "DialogManager/DITLBuilder.h"
extern QDGlobals qd;
#include "MacTypes.h"
#include "math.h"

/* The File Manager calls the tests use, as FileManager.c defines them. */
extern OSErr FSDelete(ConstStr255Param fileName, VolumeRefNum vRefNum);
extern OSErr FSCreate(ConstStr255Param fileName, VolumeRefNum vRefNum, UInt32 creator, UInt32 fileType);
extern OSErr FSOpen(ConstStr255Param fileName, VolumeRefNum vRefNum, FileRefNum* refNum);
extern OSErr FSClose(FileRefNum refNum);
extern OSErr FSRead(FileRefNum refNum, UInt32* count, void* buffer);
extern OSErr FSWrite(FileRefNum refNum, UInt32* count, const void* buffer);
extern OSErr FSSetFPos(FileRefNum refNum, UInt16 posMode, SInt32 posOffset);
extern OSErr FSGetEOF(FileRefNum refNum, UInt32* eof);
extern OSErr FSSetEOF(FileRefNum refNum, UInt32 eof);
extern OSErr FSGetFInfo(ConstStr255Param fileName, VolumeRefNum vRefNum, FInfo* fndrInfo);
extern OSErr FSCreateDir(ConstStr255Param dirName, VolumeRefNum vRefNum, DirID* createdDirID);
extern OSErr FSDeleteDir(ConstStr255Param dirName, VolumeRefNum vRefNum);
extern OSErr FSOpenWD(VolumeRefNum vRefNum, DirID dirID, UInt32 procID, WDRefNum* wdRefNum);
extern OSErr FSGetWDInfo(WDRefNum wdRefNum, VolumeRefNum* vRefNum, DirID* dirID, UInt32* procID);
extern OSErr FSCloseWD(WDRefNum wdRefNum);
extern OSErr FSMakeFSSpec(VolumeRefNum vRefNum, DirID dirID, ConstStr255Param fileName, FSSpec* spec);
extern OSErr FSpCreate(const FSSpec* spec, OSType creator, OSType fileType, ScriptCode scriptTag);
extern OSErr FSpOpenDF(const FSSpec* spec, SInt8 permission, FileRefNum* refNum);
extern OSErr FSpGetFInfo(const FSSpec* spec, FInfo* fndrInfo);
extern OSErr FSpDelete(const FSSpec* spec);
extern OSErr HGetFInfo(short vRefNum, long dirID, ConstStr255Param fileName, FInfo* fndrInfo);
#include <string.h>

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

/* Called from main.c when the kernel is built with INTEGRATION_TESTS=1. */
OSErr IntegrationTests_Initialize(void);
void IntegrationTests_Run(void);
void IntegrationTests_Cleanup(void);

static int test_count = 0;
static int test_pass = 0;
static int test_fail = 0;

typedef struct {
    const char* name;
    Boolean passed;
    const char* reason;
} TestResult;

static TestResult results[32];
static int result_count = 0;

static void RecordTest(const char* name, Boolean passed, const char* reason) {
    if (result_count < 32) {
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

    /* Save, lock, restore - the pattern the stub broke. */
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

static void Test_Dialog_LoadMissingTemplate(void) {
    const char* test_name = "Dialog_LoadMissingTemplate";
    DialogTemplate* t = (DialogTemplate*)1;
    OSErr err = LoadDialogTemplate(32000, &t);
    CHECK(err == resNotFound, "a missing DLOG did not answer resNotFound");
    CHECK(t == NULL, "left a template behind for a missing resource");
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

    CHECK(FSCreate(spec.name, 0, 'ITst', 'TEXT') == noErr, "FSCreate failed");
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

    CHECK(FSCreate(spec.name, 0, 'ITst', 'TEXT') == noErr, "FSCreate failed");
    FInfo info;
    memset(&info, 0, sizeof info);
    CHECK(FSGetFInfo(spec.name, 0, &info) == noErr, "FSGetFInfo failed");
    CHECK(info.fdType == 'TEXT' && info.fdCreator == 'ITst',
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
    FSClose(ref);
    FSDelete(spec.name, 0);
    CHECK(err == noErr && eof == 100, "FSSetEOF did not grow the file");
    CHECK(zeros, "the grown part did not read back as zeros");
    CHECK(shrink != noErr, "FSSetEOF reported shrinking a file it cannot shrink");
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
    CHECK(FSOpenWD(0, dir, 'ITst', &wd) == noErr && wd < 0, "FSOpenWD failed");
    DirID gotDir = 0;
    UInt32 gotProc = 0;
    OSErr err = FSGetWDInfo(wd, NULL, &gotDir, &gotProc);
    OSErr closed = FSCloseWD(wd);
    CHECK(err == noErr && gotDir == dir && gotProc == 'ITst', "FSGetWDInfo gave back something else");
    CHECK(closed == noErr && FSGetWDInfo(wd, NULL, NULL, NULL) != noErr,
          "the working directory was still there after FSCloseWD");

    CHECK(FSDeleteDir(spec.name, 0) == noErr, "FSDeleteDir failed");
    CHECK(FSDeleteDir(spec.name, 0) != noErr, "the folder was still there after FSDeleteDir");
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
    OSErr made = FSpCreate(&spec, 'ITst', 'TEXT', 0);

    FInfo info;
    OSErr inRoot = HGetFInfo(0, 0, file.name, &info);
    OSErr byDir = HGetFInfo(0, dir, file.name, &info);
    OSErr bySpec = FSpGetFInfo(&spec, &info);

    WDRefNum wd = 0;
    OSErr viaWD = FSOpenWD(0, dir, 'ITst', &wd);
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
    CHECK(info.fdType == 'TEXT', "the file in the folder lost its type");
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
    extern void* framebuffer;
    extern uint32_t fb_pitch;
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
    ZoomWindow(doc, inZoomOut, false);
    Rect after = (*doc->contRgn)->rgnBBox;

    DisposeWindow(dlg);
    DisposeWindow(doc);
    CHECK(ok, "MoveWindow did not put the content's corner where asked, or changed the frame");
    CHECK(zoomed.top >= 20 && (zoomed.right - zoomed.left) > (before.right - before.left),
          "zooming in did not enlarge the window below the menu bar");
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

static void Test_Resource_CreateAndOpenResFile(void) {
    const char* test_name = "Resource_CreateAndOpenResFile";
    FSSpec spec;
    SetSpec(&spec, "ITest Resources");

    FSpCreateResFile(&spec, 'ITst', 'rsrc', 0);
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

void IntegrationTests_Run(void) {
    IT_LOG_INFO("%s", "");
    IT_LOG_INFO("============================================");
    IT_LOG_INFO("SYSTEM 7 INTEGRATION TEST SUITE");
    IT_LOG_INFO("============================================");

    IT_LOG_INFO("--- Memory Manager ---");
    Test_Memory_HandleStateRoundTrip();
    Test_Memory_LogicalSizes();

    IT_LOG_INFO("--- Dialog Manager ---");
    Test_Dialog_ParseDLOG();
    Test_Dialog_ParseALRT();
    Test_Dialog_ParseDLOGTruncated();
    Test_Dialog_LoadMissingTemplate();

    IT_LOG_INFO("--- Math ---");
    Test_Math_Accuracy();

    IT_LOG_INFO("--- File Manager ---");
    Test_File_WriteReadRoundTrip();
    Test_File_Metadata();
    Test_File_FoldersAndWorkingDirectories();
    Test_File_InFolder();
    Test_Draw_ClippedToVisibleRegion();
    Test_Draw_PenModes();
    Test_Draw_SetOrigin();
    Test_Draw_ScrollRect();
    Test_Dialog_AlertLayout();
    Test_Dialog_IconItem();
    Test_Window_ReorderAndHide();
    Test_Window_MoveAndZoom();
    Test_File_ReadThroughExtentsOverflow();

    IT_LOG_INFO("--- Resource Manager ---");
    Test_Resource_CreateAndOpenResFile();
    Test_Resource_OpenMissingResFile();

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
