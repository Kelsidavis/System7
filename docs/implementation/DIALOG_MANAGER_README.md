# Dialog Manager - System 7.1 Portable Implementation

## Overview

The Dialog Manager is a critical component of the Mac System 7.1 operating system, providing comprehensive support for modal and modeless dialogs, alert notifications, file selection dialogs, and user interface interactions. This portable implementation maintains exact behavioral compatibility with the original Mac OS Dialog Manager while enabling modern platform integration.

## Architecture

### Core Components

The Dialog Manager consists of several interconnected modules:

1. **DialogManagerCore** - Core dialog creation, disposal, and management
2. **ModalDialogs** - Modal dialog event loop and user interaction processing
3. **DialogItems** - Dialog item management, drawing, and interaction handling
4. **AlertDialogs** - Alert dialog handling and system notifications
5. **FileDialogs** - Standard File Package for file and folder selection
6. **DialogResources** - DLOG/DITL resource loading and template processing
7. **DialogEvents** - Dialog-specific event handling and filtering
8. **Platform Integration** - Native platform dialog system abstraction

### Key Features

#### Complete Mac OS 7.1 Compatibility
- Exact API compatibility with original Dialog Manager
- Support for all dialog functions (NewDialog, ModalDialog, Alert, etc.)
- Proper DialogRecord structure with all Mac OS dialog types
- Modal dialog event loop with correct event filtering
- Dialog item handling (buttons, text, icons, controls)
- Alert dialog variants (Stop, Note, Caution alerts)
- Standard File Package for file operations
- DLOG/DITL resource loading and template processing

#### Modern Platform Integration
- Native platform dialog systems (Cocoa, Win32, GTK)
- File picker integration with modern file systems
- Accessibility and screen reader support
- High-DPI dialog rendering and scaling
- Keyboard navigation and focus management
- Dialog animations and visual effects
- Multi-monitor dialog positioning

#### Advanced Features
- Unicode text support for international applications
- Color and theme customization
- Platform-native look and feel
- Touch and gesture support on compatible platforms
- Transparency and visual effects
- Custom dialog templates and layouts

## API Reference

### Core Dialog Functions

```c
// Dialog Manager initialization
void InitDialogs(ResumeProcPtr resumeProc);
void ErrorSound(SoundProcPtr soundProc);

// Dialog creation and disposal
DialogPtr NewDialog(void* wStorage, const Rect* boundsRect,
                    const unsigned char* title, bool visible,
                    int16_t procID, WindowPtr behind, bool goAwayFlag,
                    int32_t refCon, Handle itmLstHndl);
DialogPtr GetNewDialog(int16_t dialogID, void* dStorage, WindowPtr behind);
DialogPtr NewColorDialog(void* dStorage, const Rect* boundsRect,
                        const unsigned char* title, bool visible,
                        int16_t procID, WindowPtr behind, bool goAwayFlag,
                        int32_t refCon, Handle items);
void CloseDialog(DialogPtr theDialog);
void DisposDialog(DialogPtr theDialog);
void DisposeDialog(DialogPtr theDialog);

// Modal dialog processing
void ModalDialog(ModalFilterProcPtr filterProc, int16_t* itemHit);
bool IsDialogEvent(const EventRecord* theEvent);
bool DialogSelect(const EventRecord* theEvent, DialogPtr* theDialog,
                  int16_t* itemHit);

// Dialog drawing and updating
void DrawDialog(DialogPtr theDialog);
void UpdateDialog(DialogPtr theDialog, RgnHandle updateRgn);
void UpdtDialog(DialogPtr theDialog, RgnHandle updateRgn);
```

### Alert Dialog Functions

```c
// Alert dialog display
int16_t Alert(int16_t alertID, ModalFilterProcPtr filterProc);
int16_t StopAlert(int16_t alertID, ModalFilterProcPtr filterProc);
int16_t NoteAlert(int16_t alertID, ModalFilterProcPtr filterProc);
int16_t CautionAlert(int16_t alertID, ModalFilterProcPtr filterProc);

// Alert stage management
int16_t GetAlertStage(void);
void ResetAlertStage(void);

// Parameter text substitution
void ParamText(const unsigned char* param0, const unsigned char* param1,
               const unsigned char* param2, const unsigned char* param3);

// Modern alert functions
int16_t ShowAlert(const char* title, const char* message,
                  int16_t buttons, int16_t alertType);
```

### Dialog Item Management

```c
// Item information
void GetDialogItem(DialogPtr theDialog, int16_t itemNo, int16_t* itemType,
                   Handle* item, Rect* box);
void SetDialogItem(DialogPtr theDialog, int16_t itemNo, int16_t itemType,
                   Handle item, const Rect* box);

// Item visibility and state
void HideDialogItem(DialogPtr theDialog, int16_t itemNo);
void ShowDialogItem(DialogPtr theDialog, int16_t itemNo);
void EnableDialogItem(DialogPtr theDialog, int16_t itemNo);
void DisableDialogItem(DialogPtr theDialog, int16_t itemNo);

// Item interaction
int16_t FindDialogItem(DialogPtr theDialog, Point thePt);
void DrawDialogItem(DialogPtr theDialog, int16_t itemNo);
void InvalDialogItem(DialogPtr theDialog, int16_t itemNo);

// Text management
void GetDialogItemText(Handle item, unsigned char* text);
void SetDialogItemText(Handle item, const unsigned char* text);
void SelectDialogItemText(DialogPtr theDialog, int16_t itemNo,
                         int16_t strtSel, int16_t endSel);

// Dialog item list manipulation
void AppendDITL(DialogPtr theDialog, Handle theHandle, DITLMethod method);
int16_t CountDITL(DialogPtr theDialog);
void ShortenDITL(DialogPtr theDialog, int16_t numberItems);
```

### File Dialog Functions

```c
// Standard File Package
void SFGetFile(Point where, const unsigned char* prompt,
               FileFilterProcPtr fileFilter, int16_t numTypes,
               const OSType* typeList, DlgHookProcPtr dlgHook,
               StandardFileReply* reply);
void SFPutFile(Point where, const unsigned char* prompt,
               const unsigned char* origName, DlgHookProcPtr dlgHook,
               StandardFileReply* reply);

// Enhanced file dialogs
void StandardGetFile(FileFilterProcPtr fileFilter, int16_t numTypes,
                     const OSType* typeList, StandardFileReply* reply);
void StandardPutFile(const unsigned char* prompt,
                     const unsigned char* defaultName,
                     StandardFileReply* reply);

// Modern file dialogs
bool ShowOpenFileDialog(const char* title, const char* defaultPath,
                       const char* fileTypes, bool multiSelect,
                       char* selectedPath, size_t pathSize);
bool ShowSaveFileDialog(const char* title, const char* defaultPath,
                       const char* defaultName, const char* fileTypes,
                       char* selectedPath, size_t pathSize);
bool ShowFolderDialog(const char* title, const char* defaultPath,
                     char* selectedPath, size_t pathSize);
```

## Resource Format Support

### DLOG Resources (Dialog Templates)
- Dialog bounds rectangle
- Window procedure ID
- Visibility flag
- Close box flag
- Reference constant
- Dialog item list resource ID
- Dialog title (Pascal string)

### DITL Resources (Dialog Item Lists)
- Item count
- Array of dialog items:
  - Item type and flags
  - Item bounds rectangle
  - Item handle or data
  - Item-specific data

### ALRT Resources (Alert Templates)
- Alert bounds rectangle
- Dialog item list resource ID
- Alert stage configuration
- Sound settings per stage

## Platform Integration

### Native Dialog Support

The Dialog Manager provides seamless integration with native platform dialog systems:

#### macOS (Cocoa)
- NSAlert for alert dialogs
- NSOpenPanel/NSSavePanel for file dialogs
- NSWindow for custom dialogs
- Accessibility support via NSAccessibility
- Retina display support

#### Windows (Win32)
- MessageBox for alert dialogs
- GetOpenFileName/GetSaveFileName for file dialogs
- CreateDialog for custom dialogs
- High-DPI awareness
- Windows accessibility APIs

#### Linux (GTK)
- GtkMessageDialog for alerts
- GtkFileChooserDialog for file operations
- GtkDialog for custom dialogs
- Accessibility through ATK
- Wayland and X11 support

### Configuration Options

```c
// Enable/disable native dialogs
void DialogManager_SetNativeDialogEnabled(bool enabled);
bool DialogManager_GetNativeDialogEnabled(void);

// Accessibility support
void DialogManager_SetAccessibilityEnabled(bool enabled);
bool DialogManager_GetAccessibilityEnabled(void);

// High-DPI scaling
void DialogManager_SetScaleFactor(float scale);
float DialogManager_GetScaleFactor(void);

// Theme customization
typedef struct DialogTheme {
    uint32_t backgroundColor;
    uint32_t textColor;
    uint32_t buttonColor;
    uint32_t borderColor;
    bool isDarkMode;
} DialogTheme;

void DialogManager_SetTheme(const DialogTheme* theme);
void DialogManager_GetTheme(DialogTheme* theme);
```

## Usage Examples

### Basic Modal Dialog

```c
#include "DialogManager/DialogManager.h"

void ShowSimpleDialog(void)
{
    // Initialize Dialog Manager
    InitDialogs(NULL);

    // Create dialog bounds
    Rect bounds = {100, 100, 300, 400};

    // Create dialog item list
    Handle itemList = CreateDialogItemList(2);

    // Add OK button
    Rect okBounds = {170, 250, 190, 320};
    AddButtonItem(itemList, 1, &okBounds, "\pOK", true);

    // Add Cancel button
    Rect cancelBounds = {170, 160, 190, 240};
    AddButtonItem(itemList, 2, &cancelBounds, "\pCancel", false);

    // Create and show dialog
    DialogPtr dialog = NewDialog(NULL, &bounds, "\pExample Dialog",
                                true, dBoxProc, (WindowPtr)-1,
                                false, 0, itemList);

    if (dialog) {
        int16_t itemHit;
        ModalDialog(NULL, &itemHit);

        if (itemHit == 1) {
            printf("User clicked OK\n");
        } else if (itemHit == 2) {
            printf("User clicked Cancel\n");
        }

        DisposeDialog(dialog);
    }
}
```

### Alert Dialog

```c
void ShowAlertExample(void)
{
    // Set parameter text for substitution
    ParamText("\pFileName.txt", "\p", "\p", "\p");

    // Show stop alert
    int16_t result = StopAlert(128, NULL);

    if (result == 1) {
        printf("User acknowledged the alert\n");
    }
}
```

### Modern File Dialog

```c
void ShowFileDialogExample(void)
{
    char selectedPath[1024];

    bool result = ShowOpenFileDialog(
        "Select a file to open",
        NULL,  // Default path
        "txt,doc,rtf",  // File types
        false,  // Single selection
        selectedPath,
        sizeof(selectedPath)
    );

    if (result) {
        printf("Selected file: %s\n", selectedPath);
    }
}
```

### Custom Dialog with Resources

```c
void ShowResourceDialog(void)
{
    // Load dialog from resource
    DialogPtr dialog = GetNewDialog(128, NULL, (WindowPtr)-1);

    if (dialog) {
        // Set initial text in edit field
        Handle editItem;
        int16_t itemType;
        Rect itemBox;
        GetDialogItem(dialog, 3, &itemType, &editItem, &itemBox);
        SetDialogItemText(editItem, "\pDefault text");

        // Show modal dialog
        int16_t itemHit;
        ModalDialog(NULL, &itemHit);

        if (itemHit == 1) {  // OK button
            // Get text from edit field
            unsigned char text[256];
            GetDialogItemText(editItem, text);
            printf("User entered: %.*s\n", text[0], &text[1]);
        }

        DisposeDialog(dialog);
    }
}
```

## Building and Integration

### Compilation

The Dialog Manager can be built using the provided makefile:

```bash
# Build Dialog Manager library
make dialog-manager

# Build with tests
make dialog-manager-test

# Build examples
make dialog-manager-examples

# Install system-wide
make dialog-manager-install
```

### Platform-Specific Build Options

```bash
# Linux with GTK
make dialog-manager PLATFORM=linux

# macOS with Cocoa
make dialog-manager PLATFORM=macos

# Windows with Win32
make dialog-manager PLATFORM=windows
```

### Dependencies

- **Core**: Standard C library, System 7.1 Portable base components
- **Linux**: GTK+ 3.0, GLib, ATK (for accessibility)
- **macOS**: Cocoa framework, Foundation framework
- **Windows**: User32, ComCtl32, ComDlg32

## Error Handling

The Dialog Manager provides comprehensive error reporting:

```c
// Error codes
enum {
    dialogErr_NoError               = 0,
    dialogErr_OutOfMemory          = -108,
    dialogErr_InvalidDialog        = -1700,
    dialogErr_InvalidItem          = -1701,
    dialogErr_ResourceNotFound     = -1702,
    dialogErr_BadResourceFormat    = -1703,
    dialogErr_PlatformError        = -1704,
    dialogErr_NotInitialized       = -1705,
    dialogErr_AlreadyModal         = -1706,
    dialogErr_NotModal             = -1707
};

// Error handling example
OSErr result = LoadDialogTemplate(128, &template);
if (result != dialogErr_NoError) {
    printf("Failed to load dialog template: %d\n", result);
    return;
}
```

## Performance Considerations

### Memory Management
- Dialog structures are allocated efficiently
- Resource caching reduces I/O overhead
- Item lists are cached for fast access
- Platform resources are managed automatically

### Event Processing
- Optimized event filtering for modal dialogs
- Efficient hit-testing for dialog items
- Minimal overhead for non-dialog events
- Smart invalidation for redrawing

### Platform Integration
- Native dialogs used when appropriate
- Fallback to portable implementation
- Minimal platform-specific code paths
- Efficient resource translation

## Accessibility Features

The Dialog Manager provides comprehensive accessibility support:

- Screen reader integration
- Keyboard navigation
- High contrast support
- Voice control compatibility
- Alternative input method support

## Thread Safety

The Dialog Manager is designed for single-threaded use but provides:

- Thread-safe initialization
- Re-entrant dialog functions where appropriate
- Safe cleanup on application termination
- Platform-specific thread considerations

## Future Enhancements

Planned improvements include:

- Enhanced Unicode support
- Modern theme integration
- Touch and gesture support
- Animation and transitions
- Cloud file dialog integration
- Improved accessibility features

## License and Compatibility

This implementation maintains compatibility with:

- Original Mac System 7.1 applications
- Modern development environments
- Cross-platform deployment
- Open source licensing requirements

The Dialog Manager is an essential component for any application requiring user interaction through dialogs, alerts, and file operations, providing the familiar Mac OS experience while enabling modern platform capabilities.