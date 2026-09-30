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
#include "System71StdLib.h"
#include "MemoryMgr/MemoryManager.h"
#include "DialogManager/DialogResources.h"
#include "ResourceManager.h"
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

    IT_LOG_INFO("--- Dialog Manager ---");
    Test_Dialog_ParseDLOG();
    Test_Dialog_ParseALRT();
    Test_Dialog_ParseDLOGTruncated();
    Test_Dialog_LoadMissingTemplate();

    IT_LOG_INFO("--- Math ---");
    Test_Math_Accuracy();

    IT_LOG_INFO("--- File Manager ---");
    Test_File_WriteReadRoundTrip();

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
