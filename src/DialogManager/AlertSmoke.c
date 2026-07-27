/* src/DialogManager/AlertSmoke.c - Alert Dialog Smoke Tests */
#include "DialogManager/DialogInternal.h"
#include "SystemTypes.h"
#include "DialogManager/DialogManager.h"
#include "DialogManager/AlertDialogs.h"
#include "DialogManager/DialogLogging.h"

#ifdef ALERT_SMOKE_TEST

extern void CenterDialogOnScreen(DialogPtr dlg);
extern void ShowWindow(WindowPtr w);
extern void HideWindow(WindowPtr w);
extern void InvalRect(const Rect* r);
/* PostEvent declared in EventManager.h */

/* Minimal inline DLOG/DITL fallbacks if resources aren't wired yet */
static short kTestDLOG = 128; /* if you have real resources, use them */
static short kStopAlert  = 129;
static short kNoteAlert  = 130;
static short kCautionAlert = 131;

static void ShowAlertAndLog(const char* name, short id) {
    short item = 0;
    // DIALOG_LOG_DEBUG("[ALERT] Opening %s (id=%d)\n", name, id);

    /* For smoke testing: inject a Return key event to auto-dismiss the alert */
    /* keyDown event (3), Return character (0x0D) in low byte */
    PostEvent(3 /* keyDown */, 0x0D /* '\r' */);

    item = Alert(id, NULL);  /* uses your RunAlertDialog + ModalDialog pipeline */
    // DIALOG_LOG_DEBUG("[ALERT] %s dismissed with item=%d\n", name, item);
}

void DoAlertSmokeTests(void) {
    /* ParamText test */
    ParamText((const unsigned char*)PSTR("Disk 'DevHD'"),
              (const unsigned char*)PSTR("can't be ejected"),
              (const unsigned char*)PSTR("(close apps first)"),
              (const unsigned char*)PSTR(" "));
    ShowAlertAndLog("StopAlert",   kStopAlert);

    ClearParamText();
    ParamText((const unsigned char*)PSTR("Update complete"),
              (const unsigned char*)PSTR(" "),
              (const unsigned char*)PSTR(" "),
              (const unsigned char*)PSTR(" "));
    ShowAlertAndLog("NoteAlert",   kNoteAlert);

    ClearParamText();
    ParamText((const unsigned char*)PSTR("Low battery"),
              (const unsigned char*)PSTR("Plug in the adapter."),
              (const unsigned char*)PSTR(" "),
              (const unsigned char*)PSTR(" "));
    ShowAlertAndLog("CautionAlert", kCautionAlert);

    /* Generic Alert() path using a DLOG that includes 1-3 buttons */
    ClearParamText();
    ParamText((const unsigned char*)PSTR("Generic DLOG"),
              (const unsigned char*)PSTR("with 3 buttons"),
              (const unsigned char*)PSTR("Default=1, Cancel=2"),
              (const unsigned char*)PSTR(" "));
    ShowAlertAndLog("Generic Alert", kTestDLOG);
}

/*
 * InitAlertSmokeTest - Initialize and run alert smoke tests
 */
void InitAlertSmokeTest(void) {
    // DIALOG_LOG_DEBUG("[ALERT SMOKE] Enabled\n");
    DoAlertSmokeTests();
    // DIALOG_LOG_DEBUG("[ALERT SMOKE] Completed\n");
}

#else
/* Stub when smoke test is disabled */
void InitAlertSmokeTest(void) {
    /* No-op */
}
#endif
