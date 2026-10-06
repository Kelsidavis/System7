#include "EventManager/EventManagerInternal.h"
#include "ProcessMgr/ProcessMgr.h"
#include "SystemInternal.h"
#include "System71StdLib.h"
#include "WindowManager/WindowManager.h"
#include "check.h"

static UInt32 ticks = 123;
static UInt16 modifiers = shiftKey;

void serial_logf(SystemLogModule module, SystemLogLevel level, const char* fmt, ...)
{
    (void)module;
    (void)level;
    (void)fmt;
}

void ProcessModernInput(void) {}
void UpdateCursorDisplay(void) {}
void Proc_UnblockEvent(EventRecord* event) { (void)event; }
WindowPtr WM_FindWindowNeedingUpdate(void) { return NULL; }
UInt32 TickCount(void) { return ticks; }
UInt16 GetModifierState(void) { return modifiers; }

void GetMouse(Point* point)
{
    point->h = 123;
    point->v = 234;
}

static int TestSelectiveRead(void)
{
    EventRecord event;
    Event_InitQueue();
    CHECK(PostEvent(mouseDown, 1) == noErr, 1);
    CHECK(PostEvent(keyDown, 2) == noErr, 2);
    CHECK(PostEvent(mouseUp, 3) == noErr, 3);
    CHECK(EventAvail(keyDownMask, &event) && event.message == 2, 4);
    CHECK(Event_QueueCount() == 3, 5);
    CHECK(GetNextEvent(keyDownMask, &event) && event.message == 2, 6);
    CHECK(Event_QueueCount() == 2, 7);
    CHECK(GetNextEvent(everyEvent, &event) && event.message == 1, 8);
    CHECK(GetNextEvent(everyEvent, &event) && event.message == 3, 9);
    CHECK(!GetNextEvent(everyEvent, &event), 10);
    CHECK(event.what == nullEvent && event.message == 0 && event.when == ticks, 11);
    CHECK(event.where.h == 123 && event.where.v == 234 && event.modifiers == modifiers, 12);
    CHECK(!EventAvail(everyEvent, NULL) && !GetNextEvent(everyEvent, NULL), 13);
    return 0;
}

static int TestFlushStop(void)
{
    EventRecord event;
    Event_InitQueue();
    /* Place the next group across the end of the ring. */
    for (UInt32 i = 0; i < 61; ++i) {
        PostEvent(diskEvt, i);
        CHECK(GetOSEvent(diskMask, &event) && event.message == i, 1);
    }
    PostEvent(diskEvt, 10);
    PostEvent(keyDown, 20);
    PostEvent(diskEvt, 30);
    PostEvent(mouseUp, 40);
    FlushEvents(diskMask | keyDownMask, keyDownMask);
    CHECK(Event_QueueCount() == 3, 2);
    PostEvent(mouseDown, 50);
    CHECK(Event_QueueCount() == 4, 3);
    for (UInt32 message = 20; message <= 50; message += 10) {
        CHECK(GetOSEvent((SInt16)everyEvent, &event) && event.message == message, 4);
    }
    CHECK(Event_QueueCount() == 0, 5);

    PostEvent(mouseDown, 60);
    PostEvent(keyDown, 70);
    FlushEvents(everyEvent, mDownMask);
    CHECK(Event_QueueCount() == 2, 6);
    FlushEvents(everyEvent, 0);
    CHECK(Event_QueueCount() == 0, 7);
    return 0;
}

static int TestFullQueue(void)
{
    EventRecord event;
    Event_InitQueue();
    for (UInt32 i = 0; i <= 64; ++i) PostEvent(diskEvt, i);
    CHECK(Event_QueueCount() == 64, 1);
    for (UInt32 i = 1; i <= 64; ++i) {
        CHECK(GetOSEvent(diskMask, &event) && event.message == i, 2);
    }
    CHECK(Event_QueueCount() == 0, 3);

    for (UInt32 i = 0; i < 64; ++i) {
        PostEvent(i == 20 ? keyDown : diskEvt, i);
    }
    CHECK(GetOSEvent(keyDownMask, &event) && event.message == 20, 4);
    CHECK(Event_QueueCount() == 63, 5);
    PostEvent(mouseUp, 64);
    for (UInt32 i = 0; i <= 64; ++i) {
        if (i == 20) continue;
        CHECK(GetOSEvent((SInt16)everyEvent, &event) && event.message == i, 6);
    }
    CHECK(Event_QueueCount() == 0, 7);
    return 0;
}

static int TestMaskBounds(void)
{
    EventRecord event;
    EventMask highBit = (EventMask)1U << 31;
    Event_InitQueue();
    PostEvent(32, 100);
    PostEventWithModifiers(31, 200, cmdKey);
    CHECK(EventAvail(highBit, &event) && event.message == 200, 1);
    CHECK(event.modifiers == cmdKey, 2);
    CHECK(GetNextEvent(highBit, &event) && event.message == 200, 3);
    CHECK(!GetNextEvent(~(EventMask)0, &event) && event.what == nullEvent, 4);
    PostEvent(31, 300);
    FlushEvents(highBit, 0);
    CHECK(Event_QueueCount() == 1, 5);
    return 0;
}

static UInt32 NextRandom(void)
{
    static UInt32 state = 0x714E71;
    state = state * 1664525U + 1013904223U;
    return state;
}

/* A linear list is the reference for the production ring buffer. */
static int ModelFind(const EventRecord* events, unsigned count, EventMask mask)
{
    for (unsigned i = 0; i < count; ++i) {
        if (events[i].what < 32 && (mask & ((EventMask)1U << events[i].what))) {
            return (int)i;
        }
    }
    return -1;
}

static void ModelRemove(EventRecord* events, unsigned* count, unsigned index)
{
    memmove(events + index, events + index + 1, (*count - index - 1) * sizeof(*events));
    --*count;
}

static Boolean EventsEqual(const EventRecord* a, const EventRecord* b)
{
    return a->what == b->what && a->message == b->message && a->when == b->when &&
           a->where.h == b->where.h && a->where.v == b->where.v &&
           a->modifiers == b->modifiers;
}

static int TestQueueModel(void)
{
    static const UInt16 types[] = {mouseDown, mouseUp, keyDown, keyUp, updateEvt, diskEvt, 31, 32};
    EventRecord expected[64];
    unsigned count = 0;
    Event_InitQueue();

    for (UInt32 step = 0; step < 20000; ++step) {
        UInt32 operation = (NextRandom() >> 16) % 5;
        EventMask mask = NextRandom();
        if (step % 2 == 0) mask &= 0U - mask;
        ++ticks;

        if (operation < 2) {
            EventRecord event = {0};
            event.what = types[(NextRandom() >> 16) % (sizeof(types) / sizeof(types[0]))];
            event.message = step;
            event.when = ticks;
            event.modifiers = (UInt16)NextRandom();
            GetMouse(&event.where);
            CHECK(PostEventWithModifiers(event.what, event.message, event.modifiers) == noErr, 1);
            if (count == 64) ModelRemove(expected, &count, 0);
            expected[count++] = event;
        } else if (operation == 4) {
            EventMask stopMask = NextRandom();
            int stopIndex = ModelFind(expected, count, stopMask);
            unsigned limit = stopIndex < 0 ? count : (unsigned)stopIndex;
            FlushEvents(mask, stopMask);
            while (limit > 0) {
                --limit;
                if (ModelFind(expected + limit, 1, mask) == 0) {
                    ModelRemove(expected, &count, limit);
                }
            }
        } else {
            EventRecord actual;
            int index = ModelFind(expected, count, mask);
            Boolean found = operation == 2 ? GetNextEvent(mask, &actual) : EventAvail(mask, &actual);
            CHECK(found == (index >= 0), 2);
            if (found) {
                CHECK(EventsEqual(&actual, &expected[index]), 3);
                if (operation == 2) ModelRemove(expected, &count, (unsigned)index);
            } else {
                CHECK(actual.what == nullEvent && actual.message == 0 && actual.when == ticks, 4);
            }
        }
        CHECK(Event_QueueCount() == count, 5);
    }
    return 0;
}

static int TestRemoveWindowEvents(void)
{
    EventRecord event;
    WindowPtr closed = (WindowPtr)(uintptr_t)0x1234;
    const UInt32 other = 0x5678;
    const EventMask types[] = {activateEvt, updateEvt, keyDown, diskEvt};
    Event_InitQueue();
    for (UInt32 i = 0; i < 61; ++i) {
        PostEvent(diskEvt, i);
        CHECK(GetOSEvent(diskMask, &event), 1);
    }
    for (UInt32 i = 0; i < 64; ++i) {
        UInt32 message = (i / 4) % 2 ? other : (UInt32)(uintptr_t)closed;
        CHECK(PostEventWithModifiers(types[i % 4], message, (UInt16)i) == noErr, 2);
    }
    Event_RemoveWindowEvents(NULL);
    CHECK(Event_QueueCount() == 64, 3);
    Event_RemoveWindowEvents(closed);
    CHECK(Event_QueueCount() == 48, 4);
    CHECK(PostEvent(mouseDown, 0x9ABC) == noErr, 5);
    for (UInt32 i = 0; i < 64; ++i) {
        UInt32 message = (i / 4) % 2 ? other : (UInt32)(uintptr_t)closed;
        if (message == (UInt32)(uintptr_t)closed && i % 4 < 2) continue;
        CHECK(GetOSEvent((SInt16)everyEvent, &event), 6);
        CHECK(event.what == types[i % 4] && (UInt32)event.message == message &&
              event.modifiers == (UInt16)i, 7);
    }
    CHECK(GetOSEvent((SInt16)everyEvent, &event) && event.what == mouseDown &&
          event.message == 0x9ABC, 8);
    Event_RemoveWindowEvents(closed);
    CHECK(Event_QueueCount() == 0, 9);
    return 0;
}

int main(void)
{
    int result = TestSelectiveRead();
    result |= TestFlushStop();
    result |= TestFullQueue();
    result |= TestMaskBounds();
    result |= TestQueueModel();
    result |= TestRemoveWindowEvents();
    return result;
}
