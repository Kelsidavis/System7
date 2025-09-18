/************************************************************
 *
 * TextInput.h
 * System 7.1 Portable - TextEdit Input Handling
 *
 * Input handling APIs for TextEdit, including keyboard
 * input, modern input methods, Unicode support, and
 * platform-specific input abstractions.
 *
 * Copyright (c) 2024 System7.1-Portable Project
 * Based on Apple Computer, Inc. TextEdit Manager 1985-1992
 *
 ************************************************************/

#ifndef __TEXTINPUT_H__
#define __TEXTINPUT_H__

#ifndef __TYPES_H__
#include <Types.h>
#endif

#ifndef __EVENTS_H__
#include <Events.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/************************************************************
 * Input Event Types
 ************************************************************/

/* Key event types */
enum {
    kTEKeyDown = 0,             /* Key down event */
    kTEKeyUp = 1,               /* Key up event */
    kTEKeyRepeat = 2,           /* Key repeat event */
    kTEDeadKey = 3,             /* Dead key event */
    kTECompositionUpdate = 4    /* Text composition update */
};

/* Input method types */
enum {
    kTEInputMethodNone = 0,     /* No input method */
    kTEInputMethodBasic = 1,    /* Basic input method */
    kTEInputMethodUnicode = 2,  /* Unicode input method */
    kTEInputMethodIME = 3       /* Input Method Editor */
};

/* Key modifier flags */
enum {
    kTEShiftKey = 0x0001,       /* Shift key */
    kTEControlKey = 0x0002,     /* Control key */
    kTEOptionKey = 0x0004,      /* Option/Alt key */
    kTECommandKey = 0x0008,     /* Command key */
    kTEFunctionKey = 0x0010,    /* Function key */
    kTENumericPadKey = 0x0020   /* Numeric pad key */
};

/************************************************************
 * Input Data Structures
 ************************************************************/

/* Key event record */
typedef struct TEKeyEvent {
    short eventType;            /* Event type */
    short keyCode;              /* Virtual key code */
    char charCode;              /* Character code */
    char padding;               /* Padding */
    long modifiers;             /* Modifier flags */
    long timestamp;             /* Event timestamp */
    Handle unicodeText;         /* Unicode text (if any) */
    short unicodeLength;        /* Unicode text length */
} TEKeyEvent;

/* Input method context */
typedef struct TEInputContext {
    short inputMethod;          /* Input method type */
    Boolean compositionActive;  /* Composition in progress */
    char padding;               /* Padding */
    long compositionStart;      /* Composition start offset */
    long compositionEnd;        /* Composition end offset */
    Handle compositionText;     /* Composition text buffer */
    Handle candidateText;       /* Candidate text list */
    short selectedCandidate;    /* Selected candidate index */
} TEInputContext;

/* Input configuration */
typedef struct TEInputConfig {
    Boolean useModernInput;     /* Use modern input methods */
    Boolean enableUnicode;      /* Enable Unicode input */
    Boolean enableIME;          /* Enable Input Method Editor */
    Boolean enableDeadKeys;     /* Enable dead key processing */
    TextEncoding encoding;      /* Text encoding */
    short keyboardLayout;       /* Keyboard layout ID */
} TEInputConfig;

/************************************************************
 * Core Input Functions
 ************************************************************/

/* Input system initialization */
OSErr TEInputInit(void);
void TEInputCleanup(void);

/* Input context management */
OSErr TECreateInputContext(TEHandle hTE, TEInputContext **context);
void TEDisposeInputContext(TEInputContext *context);
OSErr TESetInputContext(TEHandle hTE, TEInputContext *context);
TEInputContext* TEGetInputContext(TEHandle hTE);

/* Input configuration */
OSErr TESetInputConfig(TEHandle hTE, const TEInputConfig *config);
OSErr TEGetInputConfig(TEHandle hTE, TEInputConfig *config);

/* Key event processing */
OSErr TEHandleKeyEvent(TEHandle hTE, const TEKeyEvent *event);
OSErr TEProcessKeyDown(TEHandle hTE, short keyCode, char charCode, long modifiers);
OSErr TEProcessKeyUp(TEHandle hTE, short keyCode, char charCode, long modifiers);
OSErr TEProcessKeyRepeat(TEHandle hTE, short keyCode, char charCode, long modifiers);

/* Character insertion */
OSErr TEInsertChar(TEHandle hTE, char ch);
OSErr TEInsertUnicodeText(TEHandle hTE, const void* unicodeText, short length);
OSErr TEInsertTextWithStyle(TEHandle hTE, const void* text, short length,
                           const TextStyle* style);

/* Composition text handling */
OSErr TEStartComposition(TEHandle hTE);
OSErr TEUpdateComposition(TEHandle hTE, const void* text, short length,
                         short selStart, short selEnd);
OSErr TEEndComposition(TEHandle hTE, Boolean commit);
Boolean TEIsCompositionActive(TEHandle hTE);

/************************************************************
 * Modern Input Method Support
 ************************************************************/

/* Input Method Editor (IME) support */
OSErr TEEnableIME(TEHandle hTE, Boolean enable);
Boolean TEIsIMEEnabled(TEHandle hTE);
OSErr TEGetIMECandidates(TEHandle hTE, Handle* candidates, short* count);
OSErr TESelectIMECandidate(TEHandle hTE, short index);

/* Unicode input support */
OSErr TESetUnicodeMode(TEHandle hTE, Boolean enable);
Boolean TEGetUnicodeMode(TEHandle hTE);
OSErr TEInsertUTF8Text(TEHandle hTE, const char* utf8Text, long length);
OSErr TEInsertUTF16Text(TEHandle hTE, const UniChar* utf16Text, long length);

/* Text encoding conversion */
OSErr TESetTextEncoding(TEHandle hTE, TextEncoding encoding);
TextEncoding TEGetTextEncoding(TEHandle hTE);
OSErr TEConvertTextEncoding(const void* srcText, long srcLength,
                           TextEncoding srcEncoding, void* dstText,
                           long* dstLength, TextEncoding dstEncoding);

/************************************************************
 * Keyboard Layout and Language Support
 ************************************************************/

/* Keyboard layout management */
OSErr TESetKeyboardLayout(TEHandle hTE, short layoutID);
short TEGetKeyboardLayout(TEHandle hTE);
OSErr TEGetAvailableLayouts(short* layouts, short* count);

/* Language and script support */
OSErr TESetLanguage(TEHandle hTE, short language);
short TEGetLanguage(TEHandle hTE);
OSErr TESetScript(TEHandle hTE, short script);
short TEGetScript(TEHandle hTE);

/* Dead key processing */
OSErr TEEnableDeadKeys(TEHandle hTE, Boolean enable);
Boolean TEAreDeadKeysEnabled(TEHandle hTE);
OSErr TEProcessDeadKey(TEHandle hTE, short keyCode, char* resultChar);

/************************************************************
 * Platform-Specific Input Handling
 ************************************************************/

/* Platform input initialization */
OSErr TEPlatformInputInit(void);
void TEPlatformInputCleanup(void);

/* Platform key event handling */
OSErr TEPlatformProcessKeyEvent(TEHandle hTE, void* platformEvent);
OSErr TEPlatformGetKeyboardState(void* keyState);
OSErr TEPlatformSetKeyboardState(const void* keyState);

/* Platform IME integration */
OSErr TEPlatformEnableIME(TEHandle hTE, Boolean enable);
OSErr TEPlatformGetIMEStatus(TEHandle hTE, Boolean* active, Rect* compositionRect);
OSErr TEPlatformSetIMEPosition(TEHandle hTE, Point position);

/* Touch and gesture support (for modern platforms) */
OSErr TEEnableTouchInput(TEHandle hTE, Boolean enable);
OSErr TEProcessTouchEvent(TEHandle hTE, short touchType, Point location, long timestamp);

/************************************************************
 * Advanced Input Features
 ************************************************************/

/* Auto-completion support */
typedef pascal Boolean (*TEAutoCompleteProc)(TEHandle hTE, const char* prefix,
                                             Handle* completions, short* count);
OSErr TESetAutoCompleteProc(TEHandle hTE, TEAutoCompleteProc proc);
TEAutoCompleteProc TEGetAutoCompleteProc(TEHandle hTE);
OSErr TEShowAutoComplete(TEHandle hTE);
OSErr TEHideAutoComplete(TEHandle hTE);

/* Input validation */
typedef pascal Boolean (*TEInputValidatorProc)(TEHandle hTE, const char* text,
                                               short length, short* errorPos);
OSErr TESetInputValidator(TEHandle hTE, TEInputValidatorProc proc);
TEInputValidatorProc TEGetInputValidator(TEHandle hTE);

/* Input transformation */
typedef pascal OSErr (*TEInputTransformProc)(TEHandle hTE, const char* input,
                                             short inputLength, char* output,
                                             short* outputLength);
OSErr TESetInputTransform(TEHandle hTE, TEInputTransformProc proc);
TEInputTransformProc TEGetInputTransform(TEHandle hTE);

/* Spell checking integration */
OSErr TEEnableSpellCheck(TEHandle hTE, Boolean enable);
Boolean TEIsSpellCheckEnabled(TEHandle hTE);
OSErr TECheckSpelling(TEHandle hTE, long start, long end, long* errorStart, long* errorEnd);
OSErr TEGetSpellingSuggestions(TEHandle hTE, long start, long end, Handle* suggestions, short* count);

/************************************************************
 * Accessibility Input Support
 ************************************************************/

/* Screen reader integration */
OSErr TEEnableScreenReader(TEHandle hTE, Boolean enable);
Boolean TEIsScreenReaderEnabled(TEHandle hTE);
OSErr TEAnnounceTextChange(TEHandle hTE, long start, long end, const char* newText);

/* Alternative input methods */
OSErr TEEnableVoiceInput(TEHandle hTE, Boolean enable);
OSErr TEProcessVoiceCommand(TEHandle hTE, const char* command);
OSErr TEEnableSwitchInput(TEHandle hTE, Boolean enable);
OSErr TEProcessSwitchInput(TEHandle hTE, short switchID, Boolean pressed);

/************************************************************
 * Input Event Utilities
 ************************************************************/

/* Event conversion utilities */
OSErr TEEventRecordToKeyEvent(const EventRecord* eventRec, TEKeyEvent* keyEvent);
OSErr TEKeyEventToEventRecord(const TEKeyEvent* keyEvent, EventRecord* eventRec);

/* Character classification */
Boolean TEIsControlChar(char ch);
Boolean TEIsArrowKey(short keyCode);
Boolean TEIsDeleteKey(short keyCode);
Boolean TEIsModifierKey(short keyCode);

/* Input state queries */
Boolean TEHasInputFocus(TEHandle hTE);
OSErr TESetInputFocus(TEHandle hTE, Boolean focus);
long TEGetLastInputTime(TEHandle hTE);

#ifdef __cplusplus
}
#endif

#endif /* __TEXTINPUT_H__ */