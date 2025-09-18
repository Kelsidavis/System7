/************************************************************
 *
 * TextInput.c
 * System 7.1 Portable - TextEdit Input Handling
 *
 * Keyboard input processing, character insertion, modern
 * input method support, and platform-specific input
 * handling for TextEdit.
 *
 * Copyright (c) 2024 System7.1-Portable Project
 * Based on Apple Computer, Inc. TextEdit Manager 1985-1992
 *
 ************************************************************/

#include "TextEdit/TextEdit.h"
#include "TextEdit/TextTypes.h"
#include "TextEdit/TextInput.h"
#include "TextEdit/TextSelection.h"
#include "Memory.h"
#include "Events.h"
#include <string.h>
#include <stdlib.h>

/************************************************************
 * Global Input State
 ************************************************************/

static Boolean gInputInitialized = false;
static TEInputContext gDefaultInputContext = {0};

/************************************************************
 * Internal Input Utilities
 ************************************************************/

static OSErr TEValidateInputContext(TEInputContext* context)
{
    if (!context) return paramErr;

    /* Validate input method type */
    if (context->inputMethod < kTEInputMethodNone ||
        context->inputMethod > kTEInputMethodIME) {
        context->inputMethod = kTEInputMethodBasic;
    }

    /* Validate composition state */
    if (context->compositionActive && !context->compositionText) {
        context->compositionActive = false;
    }

    return noErr;
}

static Boolean TEIsControlCharacter(char ch)
{
    return (ch < 0x20 && ch != returnChar && ch != tabChar) || ch == 0x7F;
}

static Boolean TEIsNavigationKey(short keyCode)
{
    return (keyCode == leftArrowChar || keyCode == rightArrowChar ||
            keyCode == upArrowChar || keyCode == downArrowChar);
}

static Boolean TEIsEditingKey(short keyCode)
{
    return (keyCode == backspaceChar || keyCode == 0x7F); /* Delete key */
}

/************************************************************
 * Input System Initialization
 ************************************************************/

OSErr TEInputInit(void)
{
    if (gInputInitialized) return noErr;

    /* Initialize default input context */
    memset(&gDefaultInputContext, 0, sizeof(TEInputContext));
    gDefaultInputContext.inputMethod = kTEInputMethodBasic;
    gDefaultInputContext.compositionActive = false;
    gDefaultInputContext.compositionText = NULL;
    gDefaultInputContext.candidateText = NULL;
    gDefaultInputContext.selectedCandidate = 0;

    /* Initialize platform-specific input */
    TEPlatformInputInit();

    gInputInitialized = true;
    return noErr;
}

void TEInputCleanup(void)
{
    if (!gInputInitialized) return;

    /* Clean up platform-specific input */
    TEPlatformInputCleanup();

    /* Clean up default context */
    if (gDefaultInputContext.compositionText) {
        DisposeHandle(gDefaultInputContext.compositionText);
        gDefaultInputContext.compositionText = NULL;
    }
    if (gDefaultInputContext.candidateText) {
        DisposeHandle(gDefaultInputContext.candidateText);
        gDefaultInputContext.candidateText = NULL;
    }

    gInputInitialized = false;
}

/************************************************************
 * Input Context Management
 ************************************************************/

OSErr TECreateInputContext(TEHandle hTE, TEInputContext **context)
{
    TEInputContext *newContext;

    if (!hTE || !context) return paramErr;

    newContext = (TEInputContext*)malloc(sizeof(TEInputContext));
    if (!newContext) return memFullErr;

    /* Initialize with default values */
    *newContext = gDefaultInputContext;
    newContext->compositionText = NULL;
    newContext->candidateText = NULL;

    *context = newContext;
    return noErr;
}

void TEDisposeInputContext(TEInputContext *context)
{
    if (!context) return;

    if (context->compositionText) {
        DisposeHandle(context->compositionText);
    }
    if (context->candidateText) {
        DisposeHandle(context->candidateText);
    }

    free(context);
}

OSErr TESetInputContext(TEHandle hTE, TEInputContext *context)
{
    /* In full implementation, would store context in TERec or dispatch record */
    if (!hTE || !context) return paramErr;

    return TEValidateInputContext(context);
}

TEInputContext* TEGetInputContext(TEHandle hTE)
{
    /* In full implementation, would retrieve from TERec or dispatch record */
    if (!hTE) return NULL;

    return &gDefaultInputContext;
}

/************************************************************
 * Input Configuration
 ************************************************************/

OSErr TESetInputConfig(TEHandle hTE, const TEInputConfig *config)
{
    TEInputContext *context;

    if (!hTE || !config) return paramErr;

    context = TEGetInputContext(hTE);
    if (!context) return paramErr;

    /* Apply configuration */
    if (config->enableUnicode) {
        context->inputMethod = kTEInputMethodUnicode;
    } else if (config->enableIME) {
        context->inputMethod = kTEInputMethodIME;
    } else {
        context->inputMethod = kTEInputMethodBasic;
    }

    return noErr;
}

OSErr TEGetInputConfig(TEHandle hTE, TEInputConfig *config)
{
    TEInputContext *context;

    if (!hTE || !config) return paramErr;

    context = TEGetInputContext(hTE);
    if (!context) return paramErr;

    /* Fill configuration */
    memset(config, 0, sizeof(TEInputConfig));
    config->useModernInput = (context->inputMethod != kTEInputMethodNone);
    config->enableUnicode = (context->inputMethod == kTEInputMethodUnicode);
    config->enableIME = (context->inputMethod == kTEInputMethodIME);
    config->enableDeadKeys = true;
    config->encoding = 0; /* Mac Roman */
    config->keyboardLayout = 0; /* US */

    return noErr;
}

/************************************************************
 * Key Event Processing
 ************************************************************/

OSErr TEHandleKeyEvent(TEHandle hTE, const TEKeyEvent *event)
{
    if (!hTE || !event) return paramErr;

    switch (event->eventType) {
        case kTEKeyDown:
            return TEProcessKeyDown(hTE, event->keyCode, event->charCode, event->modifiers);

        case kTEKeyUp:
            return TEProcessKeyUp(hTE, event->keyCode, event->charCode, event->modifiers);

        case kTEKeyRepeat:
            return TEProcessKeyRepeat(hTE, event->keyCode, event->charCode, event->modifiers);

        case kTECompositionUpdate:
            if (event->unicodeText && event->unicodeLength > 0) {
                return TEUpdateComposition(hTE, *event->unicodeText, event->unicodeLength, 0, 0);
            }
            break;

        default:
            return paramErr;
    }

    return noErr;
}

OSErr TEProcessKeyDown(TEHandle hTE, short keyCode, char charCode, long modifiers)
{
    TERec **teRec;
    OSErr err = noErr;

    if (!hTE || !*hTE) return paramErr;

    teRec = (TERec **)hTE;

    /* Handle modifier key combinations */
    if (modifiers & kTECommandKey) {
        switch (charCode) {
            case 'a': case 'A':
                return TESelectAll(hTE);

            case 'c': case 'C':
                TECopy(hTE);
                return noErr;

            case 'x': case 'X':
                TECut(hTE);
                return noErr;

            case 'v': case 'V':
                TEPaste(hTE);
                return noErr;

            case 'z': case 'Z':
                /* Undo functionality would go here */
                return noErr;
        }
    }

    /* Handle special navigation keys */
    if (TEIsNavigationKey(keyCode)) {
        Boolean extend = (modifiers & kTEShiftKey) != 0;
        Boolean word = (modifiers & kTEOptionKey) != 0;

        switch (keyCode) {
            case leftArrowChar:
                if (word) {
                    err = TEMoveWordLeft(hTE, extend);
                } else {
                    err = TEMoveLeft(hTE, extend);
                }
                break;

            case rightArrowChar:
                if (word) {
                    err = TEMoveWordRight(hTE, extend);
                } else {
                    err = TEMoveRight(hTE, extend);
                }
                break;

            case upArrowChar:
                /* Move up one line - simplified implementation */
                err = TEMoveLeft(hTE, extend);
                break;

            case downArrowChar:
                /* Move down one line - simplified implementation */
                err = TEMoveRight(hTE, extend);
                break;
        }
        return err;
    }

    /* Handle editing keys */
    if (TEIsEditingKey(keyCode)) {
        switch (keyCode) {
            case backspaceChar:
                if ((**teRec).selStart == (**teRec).selEnd && (**teRec).selStart > 0) {
                    /* Move selection back one character */
                    (**teRec).selStart--;
                }
                if ((**teRec).selStart < (**teRec).selEnd) {
                    TEDelete(hTE);
                }
                break;

            case 0x7F: /* Forward delete */
                if ((**teRec).selStart == (**teRec).selEnd && (**teRec).selEnd < (**teRec).teLength) {
                    /* Extend selection forward one character */
                    (**teRec).selEnd++;
                }
                if ((**teRec).selStart < (**teRec).selEnd) {
                    TEDelete(hTE);
                }
                break;
        }
        return noErr;
    }

    /* Handle regular character input */
    if (!TEIsControlCharacter(charCode) || charCode == returnChar || charCode == tabChar) {
        err = TEInsertChar(hTE, charCode);
    }

    return err;
}

OSErr TEProcessKeyUp(TEHandle hTE, short keyCode, char charCode, long modifiers)
{
    /* Key up events typically don't need special handling in TextEdit */
    return noErr;
}

OSErr TEProcessKeyRepeat(TEHandle hTE, short keyCode, char charCode, long modifiers)
{
    /* Handle key repeat the same as key down */
    return TEProcessKeyDown(hTE, keyCode, charCode, modifiers);
}

/************************************************************
 * Character Insertion Functions
 ************************************************************/

OSErr TEInsertChar(TEHandle hTE, char ch)
{
    TERec **teRec;

    if (!hTE || !*hTE) return paramErr;

    teRec = (TERec **)hTE;

    /* Delete selection first if any */
    if ((**teRec).selStart != (**teRec).selEnd) {
        TEDelete(hTE);
    }

    /* Insert the character */
    return (OSErr)TEInsert(&ch, 1, hTE);
}

OSErr TEInsertUnicodeText(TEHandle hTE, const void* unicodeText, short length)
{
    /* In full implementation, would convert Unicode to current encoding */
    /* For now, treat as regular text insertion */
    if (!hTE || !unicodeText || length <= 0) return paramErr;

    return (OSErr)TEInsert(unicodeText, length, hTE);
}

OSErr TEInsertTextWithStyle(TEHandle hTE, const void* text, short length,
                           const TextStyle* style)
{
    OSErr err;

    if (!hTE || !text || length <= 0) return paramErr;

    /* Insert text first */
    err = (OSErr)TEInsert(text, length, hTE);
    if (err != noErr) return err;

    /* Apply style if provided */
    if (style) {
        TERec **teRec = (TERec **)hTE;
        long start = (**teRec).selStart - length;
        long end = (**teRec).selStart;

        /* In full implementation, would apply style to inserted text */
        /* For now, just update basic text attributes */
        (**teRec).txFont = style->tsFont;
        (**teRec).txFace = style->tsFace;
        (**teRec).txSize = style->tsSize;
    }

    return noErr;
}

/************************************************************
 * Composition Text Handling
 ************************************************************/

OSErr TEStartComposition(TEHandle hTE)
{
    TEInputContext *context;

    if (!hTE) return paramErr;

    context = TEGetInputContext(hTE);
    if (!context) return paramErr;

    if (context->compositionActive) {
        return noErr; /* Already in composition */
    }

    context->compositionActive = true;
    context->compositionStart = TEGetCaretPosition(hTE);
    context->compositionEnd = context->compositionStart;

    if (!context->compositionText) {
        context->compositionText = NewHandle(256);
        if (!context->compositionText) {
            context->compositionActive = false;
            return MemError();
        }
    }

    SetHandleSize(context->compositionText, 0);
    return noErr;
}

OSErr TEUpdateComposition(TEHandle hTE, const void* text, short length,
                         short selStart, short selEnd)
{
    TEInputContext *context;
    OSErr err;

    if (!hTE || !text || length < 0) return paramErr;

    context = TEGetInputContext(hTE);
    if (!context || !context->compositionActive) return paramErr;

    /* Update composition text storage */
    SetHandleSize(context->compositionText, length);
    err = MemError();
    if (err != noErr) return err;

    if (length > 0) {
        HLock(context->compositionText);
        memcpy(*context->compositionText, text, length);
        HUnlock(context->compositionText);
    }

    /* Replace current composition in TextEdit record */
    TESetSelection(hTE, context->compositionStart, context->compositionEnd);
    if (length > 0) {
        TEInsert(text, length, hTE);
    }

    context->compositionEnd = context->compositionStart + length;

    return noErr;
}

OSErr TEEndComposition(TEHandle hTE, Boolean commit)
{
    TEInputContext *context;

    if (!hTE) return paramErr;

    context = TEGetInputContext(hTE);
    if (!context || !context->compositionActive) return paramErr;

    if (!commit) {
        /* Cancel composition - remove composed text */
        TESetSelection(hTE, context->compositionStart, context->compositionEnd);
        TEDelete(hTE);
        TESetSelection(hTE, context->compositionStart, context->compositionStart);
    }

    context->compositionActive = false;
    context->compositionStart = 0;
    context->compositionEnd = 0;

    if (context->compositionText) {
        SetHandleSize(context->compositionText, 0);
    }

    return noErr;
}

Boolean TEIsCompositionActive(TEHandle hTE)
{
    TEInputContext *context;

    if (!hTE) return false;

    context = TEGetInputContext(hTE);
    if (!context) return false;

    return context->compositionActive;
}

/************************************************************
 * Modern Input Method Support
 ************************************************************/

OSErr TEEnableIME(TEHandle hTE, Boolean enable)
{
    TEInputContext *context;

    if (!hTE) return paramErr;

    context = TEGetInputContext(hTE);
    if (!context) return paramErr;

    if (enable) {
        context->inputMethod = kTEInputMethodIME;
    } else if (context->inputMethod == kTEInputMethodIME) {
        context->inputMethod = kTEInputMethodBasic;
    }

    return TEPlatformEnableIME(hTE, enable);
}

Boolean TEIsIMEEnabled(TEHandle hTE)
{
    TEInputContext *context;

    if (!hTE) return false;

    context = TEGetInputContext(hTE);
    if (!context) return false;

    return context->inputMethod == kTEInputMethodIME;
}

OSErr TESetUnicodeMode(TEHandle hTE, Boolean enable)
{
    TEInputContext *context;

    if (!hTE) return paramErr;

    context = TEGetInputContext(hTE);
    if (!context) return paramErr;

    if (enable) {
        context->inputMethod = kTEInputMethodUnicode;
    } else if (context->inputMethod == kTEInputMethodUnicode) {
        context->inputMethod = kTEInputMethodBasic;
    }

    return noErr;
}

Boolean TEGetUnicodeMode(TEHandle hTE)
{
    TEInputContext *context;

    if (!hTE) return false;

    context = TEGetInputContext(hTE);
    if (!context) return false;

    return context->inputMethod == kTEInputMethodUnicode;
}

/************************************************************
 * Platform-Specific Input Handling Stubs
 ************************************************************/

OSErr TEPlatformInputInit(void)
{
    /* Platform-specific initialization would go here */
    return noErr;
}

void TEPlatformInputCleanup(void)
{
    /* Platform-specific cleanup would go here */
}

OSErr TEPlatformProcessKeyEvent(TEHandle hTE, void* platformEvent)
{
    /* Platform-specific event processing would go here */
    return unimpErr;
}

OSErr TEPlatformEnableIME(TEHandle hTE, Boolean enable)
{
    /* Platform-specific IME enable/disable would go here */
    return unimpErr;
}

/************************************************************
 * Event Conversion Utilities
 ************************************************************/

OSErr TEEventRecordToKeyEvent(const EventRecord* eventRec, TEKeyEvent* keyEvent)
{
    if (!eventRec || !keyEvent) return paramErr;

    memset(keyEvent, 0, sizeof(TEKeyEvent));

    switch (eventRec->what) {
        case keyDown:
            keyEvent->eventType = kTEKeyDown;
            break;
        case keyUp:
            keyEvent->eventType = kTEKeyUp;
            break;
        case autoKey:
            keyEvent->eventType = kTEKeyRepeat;
            break;
        default:
            return paramErr;
    }

    keyEvent->keyCode = (eventRec->message >> 8) & 0xFF;
    keyEvent->charCode = eventRec->message & 0xFF;
    keyEvent->modifiers = 0;

    /* Convert modifiers */
    if (eventRec->modifiers & shiftKey) keyEvent->modifiers |= kTEShiftKey;
    if (eventRec->modifiers & controlKey) keyEvent->modifiers |= kTEControlKey;
    if (eventRec->modifiers & optionKey) keyEvent->modifiers |= kTEOptionKey;
    if (eventRec->modifiers & cmdKey) keyEvent->modifiers |= kTECommandKey;

    keyEvent->timestamp = eventRec->when;
    keyEvent->unicodeText = NULL;
    keyEvent->unicodeLength = 0;

    return noErr;
}

/************************************************************
 * Character Classification Utilities
 ************************************************************/

Boolean TEIsControlChar(char ch)
{
    return TEIsControlCharacter(ch);
}

Boolean TEIsArrowKey(short keyCode)
{
    return TEIsNavigationKey(keyCode);
}

Boolean TEIsDeleteKey(short keyCode)
{
    return TEIsEditingKey(keyCode);
}

Boolean TEIsModifierKey(short keyCode)
{
    return (keyCode >= 0x36 && keyCode <= 0x3C); /* Modifier key range on Mac */
}