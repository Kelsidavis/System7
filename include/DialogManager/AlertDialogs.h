/*
 * AlertDialogs.h - Alert Dialog Management API
 *
 * This header defines the alert dialog functionality for displaying
 * system notifications and user confirmations, maintaining exact
 * Mac System 7.1 behavioral compatibility.
 */

#ifndef ALERT_DIALOGS_H
#define ALERT_DIALOGS_H

#include "DialogTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Alert dialog types */
enum {
    kAlertType_Stop         = 0,    /* Stop alert with stop icon */
    kAlertType_Note         = 1,    /* Note alert with note icon */
    kAlertType_Caution      = 2     /* Caution alert with caution icon */
};

/* Alert button configurations */
enum {
    kAlertButtons_OK                = 1,    /* Just OK button */
    kAlertButtons_OKCancel         = 2,    /* OK and Cancel buttons */
    kAlertButtons_YesNo            = 3,    /* Yes and No buttons */
    kAlertButtons_YesNoCancel      = 4,    /* Yes, No, and Cancel buttons */
    kAlertButtons_RetryCancel      = 5,    /* Retry and Cancel buttons */
    kAlertButtons_AbortRetryIgnore = 6     /* Abort, Retry, and Ignore buttons */
};

/* Alert button return values */
enum {
    kAlertButton_OK         = 1,
    kAlertButton_Cancel     = 2,
    kAlertButton_Yes        = 1,
    kAlertButton_No         = 2,
    kAlertButton_Retry      = 1,
    kAlertButton_Abort      = 1,
    kAlertButton_Ignore     = 3
};

/* Core alert dialog functions */

/*
 * Alert - Display a generic alert dialog
 *
 * This function displays an alert dialog with the specified resource ID.
 * The alert's appearance and behavior are defined by ALRT and DITL resources.
 *
 * Parameters:
 *   alertID    - Resource ID of the ALRT resource
 *   filterProc - Optional filter procedure for custom event handling
 *
 * Returns:
 *   The item number of the button that was clicked
 */
int16_t Alert(int16_t alertID, ModalFilterProcPtr filterProc);

/*
 * StopAlert - Display a stop alert dialog
 *
 * This displays an alert with a stop icon, typically used for
 * serious errors or warnings that require immediate attention.
 *
 * Parameters:
 *   alertID    - Resource ID of the ALRT resource
 *   filterProc - Optional filter procedure
 *
 * Returns:
 *   The item number of the button that was clicked
 */
int16_t StopAlert(int16_t alertID, ModalFilterProcPtr filterProc);

/*
 * NoteAlert - Display a note alert dialog
 *
 * This displays an alert with a note icon, typically used for
 * informational messages and non-critical notifications.
 *
 * Parameters:
 *   alertID    - Resource ID of the ALRT resource
 *   filterProc - Optional filter procedure
 *
 * Returns:
 *   The item number of the button that was clicked
 */
int16_t NoteAlert(int16_t alertID, ModalFilterProcPtr filterProc);

/*
 * CautionAlert - Display a caution alert dialog
 *
 * This displays an alert with a caution icon, typically used for
 * warnings about potentially destructive actions.
 *
 * Parameters:
 *   alertID    - Resource ID of the ALRT resource
 *   filterProc - Optional filter procedure
 *
 * Returns:
 *   The item number of the button that was clicked
 */
int16_t CautionAlert(int16_t alertID, ModalFilterProcPtr filterProc);

/* Alert stage management */

/*
 * GetAlertStage - Get current alert stage
 *
 * The alert stage determines which stage of a multi-stage alert
 * should be displayed. Each stage can have different sounds and
 * button configurations.
 *
 * Returns:
 *   Current alert stage (0-3)
 */
int16_t GetAlertStage(void);

/*
 * ResetAlertStage - Reset alert stage to 0
 *
 * This function resets the alert stage counter to 0, causing
 * subsequent alerts to start from the first stage.
 */
void ResetAlertStage(void);

/*
 * SetAlertStage - Set current alert stage
 *
 * Parameters:
 *   stage - The alert stage to set (0-3)
 */
void SetAlertStage(int16_t stage);

/* Parameter text substitution */

/*
 * ParamText - Set parameter text for alerts
 *
 * This function sets up to four parameter strings that can be
 * substituted into alert text using ^0, ^1, ^2, and ^3 placeholders.
 *
 * Parameters:
 *   param0 - First parameter string (Pascal string)
 *   param1 - Second parameter string (Pascal string)
 *   param2 - Third parameter string (Pascal string)
 *   param3 - Fourth parameter string (Pascal string)
 */
void ParamText(const unsigned char* param0, const unsigned char* param1,
               const unsigned char* param2, const unsigned char* param3);

/*
 * GetParamText - Get current parameter text
 *
 * Parameters:
 *   paramIndex - Parameter index (0-3)
 *   text       - Buffer to receive the parameter text
 */
void GetParamText(int16_t paramIndex, unsigned char* text);

/*
 * ClearParamText - Clear all parameter text
 */
void ClearParamText(void);

/* Modern alert dialog functions */

/*
 * ShowAlert - Show alert with modern parameters
 *
 * This function provides a modern interface for displaying alerts
 * without requiring resource files.
 *
 * Parameters:
 *   title     - Alert title (C string)
 *   message   - Alert message (C string)
 *   buttons   - Button configuration
 *   alertType - Alert type (stop, note, caution)
 *
 * Returns:
 *   The button that was clicked
 */
int16_t ShowAlert(const char* title, const char* message,
                  int16_t buttons, int16_t alertType);

/*
 * ShowAlertWithParams - Show alert with parameter substitution
 *
 * Parameters:
 *   title     - Alert title (C string)
 *   message   - Alert message with placeholders (C string)
 *   buttons   - Button configuration
 *   alertType - Alert type
 *   param0    - First parameter (C string)
 *   param1    - Second parameter (C string)
 *   param2    - Third parameter (C string)
 *   param3    - Fourth parameter (C string)
 *
 * Returns:
 *   The button that was clicked
 */
int16_t ShowAlertWithParams(const char* title, const char* message,
                           int16_t buttons, int16_t alertType,
                           const char* param0, const char* param1,
                           const char* param2, const char* param3);

/* Alert dialog customization */

/*
 * SetAlertSound - Set sound for alert type
 *
 * This function sets the sound that should be played when
 * displaying alerts of a specific type.
 *
 * Parameters:
 *   alertType - Alert type (stop, note, caution)
 *   soundID   - Sound resource ID (0 for system beep)
 */
void SetAlertSound(int16_t alertType, int16_t soundID);

/*
 * GetAlertSound - Get sound for alert type
 *
 * Parameters:
 *   alertType - Alert type
 *
 * Returns:
 *   Sound resource ID for the alert type
 */
int16_t GetAlertSound(int16_t alertType);

/*
 * SetAlertPosition - Set position for alert dialogs
 *
 * This function sets the position where alert dialogs should appear.
 *
 * Parameters:
 *   position - Position specification
 *              (0 = center screen, 1 = main screen center, 2 = parent center)
 */
void SetAlertPosition(int16_t position);

/* Alert dialog icons */

/*
 * SetAlertIcon - Set custom icon for alert type
 *
 * Parameters:
 *   alertType - Alert type
 *   iconID    - Icon resource ID (0 for system default)
 */
void SetAlertIcon(int16_t alertType, int16_t iconID);

/*
 * GetAlertIcon - Get icon for alert type
 *
 * Parameters:
 *   alertType - Alert type
 *
 * Returns:
 *   Icon resource ID for the alert type
 */
int16_t GetAlertIcon(int16_t alertType);

/* Alert dialog accessibility */

/*
 * SetAlertAccessibility - Enable/disable alert accessibility
 *
 * Parameters:
 *   enabled - true to enable accessibility features
 */
void SetAlertAccessibility(bool enabled);

/*
 * AnnounceAlert - Announce alert to screen reader
 *
 * This function announces an alert's content to accessibility
 * technologies when it's displayed.
 *
 * Parameters:
 *   title   - Alert title
 *   message - Alert message
 */
void AnnounceAlert(const char* title, const char* message);

/* Platform-native alert integration */

/*
 * ShowNativeAlert - Show platform-native alert
 *
 * This function displays an alert using the platform's native
 * alert dialog system when available and appropriate.
 *
 * Parameters:
 *   title     - Alert title
 *   message   - Alert message
 *   buttons   - Button configuration
 *   alertType - Alert type
 *
 * Returns:
 *   The button that was clicked, or -1 if native alerts not available
 */
int16_t ShowNativeAlert(const char* title, const char* message,
                       int16_t buttons, int16_t alertType);

/*
 * SetUseNativeAlerts - Enable/disable native alert dialogs
 *
 * Parameters:
 *   useNative - true to use native dialogs when available
 */
void SetUseNativeAlerts(bool useNative);

/*
 * GetUseNativeAlerts - Check if native alerts are enabled
 *
 * Returns:
 *   true if native alerts are enabled
 */
bool GetUseNativeAlerts(void);

/* Alert dialog theming */

/*
 * SetAlertTheme - Set visual theme for alerts
 *
 * Parameters:
 *   theme - Pointer to theme structure
 */
void SetAlertTheme(const DialogColorTheme* theme);

/*
 * GetAlertTheme - Get current alert theme
 *
 * Parameters:
 *   theme - Buffer to receive theme information
 */
void GetAlertTheme(DialogColorTheme* theme);

/* Alert dialog utilities */

/*
 * CreateAlertFromTemplate - Create alert from template
 *
 * This function creates an alert dialog from an ALRT template
 * resource, allowing for customization before display.
 *
 * Parameters:
 *   alertID - Resource ID of ALRT template
 *
 * Returns:
 *   DialogPtr to the created alert, or NULL on failure
 */
DialogPtr CreateAlertFromTemplate(int16_t alertID);

/*
 * RunAlert - Run an alert dialog
 *
 * This function runs the modal loop for an alert dialog
 * created with CreateAlertFromTemplate.
 *
 * Parameters:
 *   alertDialog - The alert dialog to run
 *   filterProc  - Optional filter procedure
 *
 * Returns:
 *   The button that was clicked
 */
int16_t RunAlert(DialogPtr alertDialog, ModalFilterProcPtr filterProc);

/*
 * PlayAlertSound - Play sound for alert
 *
 * Parameters:
 *   alertType - Alert type
 *   stage     - Alert stage (affects sound selection)
 */
void PlayAlertSound(int16_t alertType, int16_t stage);

/* Internal alert dialog functions */
void InitAlertDialogs(void);
void CleanupAlertDialogs(void);
OSErr LoadAlertTemplate(int16_t alertID, AlertTemplate** alertTemplate);
DialogPtr CreateAlertDialog(const AlertTemplate* alertTemplate);
void ProcessAlertStages(int16_t alertType, int16_t stage);
void SubstituteParamText(char* text, size_t textSize);

/* Backwards compatibility aliases */
#define GetAlrtStage    GetAlertStage
#define ResetAlrtStage  ResetAlertStage

#ifdef __cplusplus
}
#endif

#endif /* ALERT_DIALOGS_H */