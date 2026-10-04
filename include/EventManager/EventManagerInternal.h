#ifndef EVENTMANAGERINTERNAL_H
#define EVENTMANAGERINTERNAL_H

#include "SystemTypes.h"
#include "EventManager.h"

#ifdef __cplusplus
extern "C" {
#endif

// Event Dispatcher
void InitEventDispatcher(void);
Boolean DispatchEvent(EventRecord* evt);
Boolean HandleUpdate(EventRecord* event);
WindowPtr GetActiveWindow(void);
void SetActiveWindow(WindowPtr window);

// Modern Input
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

#ifdef __cplusplus
}
#endif

#endif
