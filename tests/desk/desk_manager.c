#include "DeskManager/DeskAccessory.h"
#include "MemoryMgr/MemoryManager.h"
#include "QuickDraw/QuickDrawInternal.h"
#include "WindowManager/WindowPlatform.h"
#include "MenuManager/MenuManager.h"
#include "System71StdLib.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        printf("%s:%d: desk regression failed: %s\n", __FILE__, __LINE__, #condition); \
        abort(); \
    } \
} while (0)

static unsigned allocations;
static unsigned allocationAttempts;
static unsigned failAllocation;
static unsigned closes;
static unsigned windowDisposals;
static unsigned activations;
static unsigned deactivations;
static unsigned menuItems;
static unsigned menuShutdowns;
static int openMode;
static GrafPort callerPort;
static GrafPtr currentPort = &callerPort;
static Point lastLocalPoint;
static UInt32 lastEventTime;
QDGlobals qd;

void* NewPtrClear(u32 size)
{
    if (++allocationAttempts == failAllocation) return NULL;
    void* result = calloc(1, size);
    if (result) ++allocations;
    return result;
}

void DisposePtr(void* pointer)
{
    if (pointer) {
        CHECK(allocations > 0);
        --allocations;
        free(pointer);
    }
}

void GetPort(GrafPtr* port) { *port = currentPort; }
void SetPort(GrafPtr port) { currentPort = port; }
void ShowWindow(WindowPtr window) { (void)window; }
void SelectWindow(WindowPtr window) { (void)window; }
void MenuBar_UpdateClock(void) {}
void SystemMenu_Update(void) {}
void SystemMenu_Shutdown(void) { ++menuShutdowns; menuItems = 0; }
int SystemMenu_AddDA(DeskAccessory* da) { (void)da; ++menuItems; return 0; }
void SystemMenu_RemoveDA(DeskAccessory* da) { (void)da; CHECK(menuItems > 0); --menuItems; }
int DeskManager_RegisterBuiltinDAs(void) { return 0; }

WindowPtr NewWindow(void* storage, const Rect* bounds, ConstStr255Param title,
                    Boolean visible, short proc, WindowPtr behind, Boolean goAway, long refCon)
{
    (void)storage; (void)title; (void)visible; (void)proc;
    (void)behind; (void)goAway; (void)refCon;
    if (openMode == 3) return NULL;
    WindowPtr window = NewPtrClear(sizeof(*window));
    if (window) window->port.portRect = *bounds;
    return window;
}

void DisposeWindow(WindowPtr window)
{
    if (currentPort == (GrafPtr)window) currentPort = &callerPort;
    ++windowDisposals;
    DisposePtr(window);
}

void c2pstrcpy(unsigned char* destination, const char* source)
{
    size_t length = strlen(source);
    CHECK(length <= 255);
    destination[0] = (UInt8)length;
    memcpy(destination + 1, source, length);
}

void GlobalToLocalWindow(WindowPtr window, Point* point)
{
    point->h -= window->port.portRect.left;
    point->v -= window->port.portRect.top;
}
short Platform_WindowHitTest(WindowPtr window, Point point)
{ (void)window; (void)point; return wInContent; }
Boolean TrackGoAway(WindowPtr window, Point point)
{ (void)window; (void)point; return false; }
void DragWindow(WindowPtr window, Point point, const Rect* bounds)
{ (void)window; (void)point; (void)bounds; }

static int Initialize(DeskAccessory* da)
{
    da->driverData = NewPtrClear(8);
    if (!da->driverData) return DESK_ERR_NO_MEMORY;
    if (openMode == 1) return -123;
    if (openMode == 4) return 0;
    DAWindowAttr attr = {.bounds = {100, 100, 200, 200}, .title = "Test", .visible = true};
    int result = DA_CreateWindow(da, &attr);
    return result != 0 ? result : openMode == 2 ? -123 : 0;
}

static int Terminate(DeskAccessory* da)
{
    ++closes;
    DisposePtr(da->driverData);
    da->driverData = NULL;
    return 0;
}

static int Activate(DeskAccessory* da, Boolean active)
{
    (void)da;
    if (active) ++activations; else ++deactivations;
    return 0;
}

static int Event(DeskAccessory* da, const DAEventInfo* event)
{
    CHECK(currentPort == (GrafPtr)da->window);
    CHECK(event->where.h == 110 && event->where.v == 120);
    lastLocalPoint = (Point){.h = event->h, .v = event->v};
    lastEventTime = event->when;
    return 0;
}

static DAInterface interface = {.initialize = Initialize, .terminate = Terminate,
                               .activate = Activate, .processEvent = Event};

static void Register(const char* name)
{
    DARegistryEntry entry = {.interface = &interface, .type = 42, .menuID = 123,
                             .flags = DA_FLAG_NEEDS_EVENTS};
    CHECK(strlen(name) < sizeof(entry.name));
    strcpy(entry.name, name);
    CHECK(DA_Register(&entry) == 0);
}

static void TestFailedOpens(void)
{
    unsigned baseline = allocations;
    for (openMode = 1; openMode <= 3; ++openMode) {
        unsigned beforeCloses = closes;
        SInt16 result = OpenDeskAcc("Test");
        CHECK(result == (openMode == 3 ? DESK_ERR_NO_MEMORY : -123));
        CHECK(allocations == baseline && closes == beforeCloses + 1);
        CHECK(DeskManager_GetDACount() == 0 && DA_GetActive() == NULL);
        CHECK(currentPort == &callerPort && menuItems == 0);
    }
    openMode = 0;
    failAllocation = allocationAttempts + 1;
    CHECK(OpenDeskAcc("Test") == DESK_ERR_NO_MEMORY && allocations == baseline);
    failAllocation = allocationAttempts + 2;
    CHECK(OpenDeskAcc("Test") == DESK_ERR_NO_MEMORY && allocations == baseline);
    failAllocation = 0;
    CHECK(OpenDeskAcc("Missing") == DESK_ERR_NOT_FOUND && allocations == baseline);
}

static void TestLifecycle(void)
{
    unsigned baseline = allocations;
    SInt16 first = OpenDeskAcc("Test");
    CHECK(first > 0 && currentPort == &callerPort);
    DeskAccessory* da = DA_GetByRefNum(first);
    CHECK(da && da->window && da->window->windowKind == -first);
    CHECK(da->flags == DA_FLAG_NEEDS_EVENTS && da->menuID == 123 && da->type == 42);
    CHECK(OpenDeskAcc("Test") == first && DeskManager_GetDACount() == 1);
    EventRecord event = {.what = mouseDown, .where = {.h = 110, .v = 120}, .when = 0xffffffff};
    CHECK(SystemEvent(&event));
    CHECK(lastLocalPoint.h == 10 && lastLocalPoint.v == 20 && lastEventTime == 0xffffffff);
    CHECK(currentPort == &callerPort);
    SInt16 second = OpenDeskAcc("Other");
    CHECK(second > 0 && DA_GetActive()->refNum == second);
    unsigned beforeActivation = activations;
    CloseDeskAcc(second);
    CHECK(DA_GetActive() == da && activations == beforeActivation + 1);
    CHECK(deactivations > 0);
    unsigned beforeDispose = windowDisposals;
    CloseDeskAcc(first);
    CloseDeskAcc(first);
    CHECK(windowDisposals == beforeDispose + 1 && allocations == baseline);
    CHECK(DA_GetActive() == NULL && DeskManager_GetDACount() == 0 && menuItems == 0);
}

static void TestReferenceRolloverAndLimits(void)
{
    openMode = 4;
    SInt16 held = OpenDeskAcc("Test");
    for (unsigned i = 0; i < 32770; ++i) {
        SInt16 id = OpenDeskAcc("Other");
        CHECK(id > 0 && id != held && DA_GetByRefNum(id));
        CloseDeskAcc(id);
    }
    char names[MAX_DESK_ACCESSORIES][32];
    SInt16 refs[MAX_DESK_ACCESSORIES];
    for (unsigned i = 0; i < MAX_DESK_ACCESSORIES; ++i) {
        snprintf(names[i], sizeof(names[i]), "Accessory %u", i);
        Register(names[i]);
        refs[i] = OpenDeskAcc(names[i]);
        CHECK(i == MAX_DESK_ACCESSORIES - 1 ? refs[i] == DESK_ERR_NO_MEMORY : refs[i] > 0);
    }
    CHECK(DeskManager_GetDACount() == MAX_DESK_ACCESSORIES);
    CHECK(OpenDeskAcc("Test") == held);
    for (unsigned i = 0; i < MAX_DESK_ACCESSORIES; ++i) {
        if (refs[i] > 0) CloseDeskAcc(refs[i]);
        DA_Unregister(names[i]);
    }
    CloseDeskAcc(held);
    openMode = 0;
}

int main(void)
{
    CHECK(DeskManager_Initialize() == 0);
    Register("Test");
    Register("Other");
    DARegistryEntry bad;
    memset(&bad, 'x', sizeof(bad));
    CHECK(DA_Register(&bad) == DESK_ERR_INVALID_PARAM);
    DARegistryEntry* entries[2];
    CHECK(DA_GetRegisteredDAs(entries, 2) == 2);
    TestFailedOpens();
    TestLifecycle();
    TestReferenceRolloverAndLimits();
    CHECK(OpenDeskAcc("Test") > 0);
    CHECK(OpenDeskAcc("Other") > 0);
    DeskManager_Shutdown();
    CHECK(menuShutdowns == 1 && DeskManager_GetDACount() == 0 && allocations == 2);
    CHECK(DeskManager_Initialize() == 0);
    CHECK(OpenDeskAcc("Test") > 0);
    DeskManager_Shutdown();
    DA_Unregister("Test");
    DA_Unregister("Other");
    CHECK(allocations == 0);
    puts("Desk accessory lifecycle regressions passed.");
    return 0;
}
