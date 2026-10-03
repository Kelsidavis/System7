#ifndef EVENTMANAGERINTERNAL_H
#define EVENTMANAGERINTERNAL_H

#include "../SystemTypes.h"
#include "EventManager.h"

// Internal event structures
typedef struct EventQueueEntry {
    EventRecord event;
    struct EventQueueEntry* next;
} EventQueueEntry;

typedef struct EventManagerState {
    EventQueueEntry* eventQueue;
    EventQueueEntry* eventQueueTail;
    short queueSize;
    Boolean mouseDown;
    Point lastMousePos;
    UInt32 lastClickTime;
} EventManagerState;

// Event Dispatcher
void InitEventDispatcher(void);
Boolean DispatchEvent(EventRecord* evt);
WindowPtr GetActiveWindow(void);
void SetActiveWindow(WindowPtr window);

// Modern Input
void UpdateMouseState(Point newPos, UInt8 buttonState);
void UpdateKeyboardState(const KeyMap newKeyMap);
Boolean IsModernInputInitialized(void);
const char* GetModernInputPlatform(void);

// System Events
SInt16 GenerateSystemEventEx(SInt16 eventType, SInt16 eventSubtype, void* eventData, WindowPtr targetWindow);

// Process Manager Event Integration
OSErr Proc_PostEvent(EventMask what, UInt32 message);
Boolean Proc_GetNextEvent(EventMask eventMask, EventRecord* theEvent);
Boolean Proc_EventAvail(EventMask eventMask, EventRecord* theEvent);
void Event_InitQueue(void);
UInt16 Event_QueueCount(void);
void Event_DumpQueue(void);
/* GetNextEvent, EventAvail, PostEvent, FlushEvents declared in EventManager.h */

extern EventManagerState gEventState;

#endif
