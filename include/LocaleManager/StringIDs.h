/*
 * StringIDs.h - Localized String Resource IDs and Indices
 *
 * Defines STR# resource IDs and 1-based string indices for all
 * user-visible strings in the system. Used with GetLocalizedString()
 * to retrieve locale-specific text.
 *
 * Numbering scheme:
 *   128-139: Finder menus
 *   140-149: Finder dialogs and labels
 *   150-159: Finder desktop items
 *   200-209: SimpleText
 *   300-309: MacPaint
 *   400-409: Window Manager
 *   500-509: Dialog Manager
 *   600-609: Standard File
 *   700-709: Control Panels
 *   800-809: System strings
 */

#ifndef STRING_IDS_H
#define STRING_IDS_H

/* ---- STR# Resource IDs -------------------------------------------------- */

/* Finder menu titles (Apple, File, Edit, View, Label, Special) */
#define kSTRListFinderMenuTitles      128

/* Finder menu items */
#define kSTRListFinderAppleMenu       129
#define kSTRListFinderFileMenu        130
#define kSTRListFinderEditMenu        131
#define kSTRListFinderViewMenu        132
#define kSTRListFinderLabelMenu       133
#define kSTRListFinderSpecialMenu     134
#define kSTRListFinderControlPanels   135

/* Finder dialogs and labels */
#define kSTRListFinderAbout           140
#define kSTRListFinderGetInfo         141
#define kSTRListFinderFind            142
#define kSTRListFinderTrash           143

/* Finder desktop items */
#define kSTRListFinderDesktop         150

/* SimpleText */
#define kSTRListSimpleTextMenus       200
#define kSTRListSimpleTextDialogs     201

/* MacPaint */
#define kSTRListMacPaintMenus         300
#define kSTRListMacPaintDialogs       301

/* Window Manager */
#define kSTRListWindowMgr             400

/* Dialog Manager */
#define kSTRListDialogMgr             500

/* Standard File */
#define kSTRListStandardFile          600

/* Control Panels */
#define kSTRListControlPanelLabels    700

/* System strings */
#define kSTRListSystem                800

/* ---- String Indices (1-based) ------------------------------------------- */

/* kSTRListFinderMenuTitles (128) - Menu bar titles */
#define kStrMenuFile                  1
#define kStrMenuEdit                  2
#define kStrMenuView                  3
#define kStrMenuLabel                 4
#define kStrMenuSpecial               5
#define kStrMenuControlPanels         6

/* kSTRListFinderAppleMenu (129) - Apple menu items */
#define kStrAboutThisMacintosh        1
#define kStrControlPanelsSubmenu      2
#define kStrNotepad                   3

/* kSTRListFinderFileMenu (130) - File menu items */
#define kStrNewFolder                 1
#define kStrOpen                      2
#define kStrPrint                     3
#define kStrClose                     4
#define kStrGetInfo                   5
#define kStrSharing                   6
#define kStrDuplicate                 7
#define kStrMakeAlias                 8
#define kStrPutAway                   9
#define kStrFindEllipsis              10
#define kStrFindAgain                 11
/* Two states of the Close item. System 7 says "Close Window" for a Finder
 * window and "Close" for anything else, so both belong to the File menu's own
 * list. kStrClose above carries the "/W" that AppendMenu parses when the menu
 * is built; these are the plain texts SetMenuItemText swaps in later, which
 * takes them literally. */
#define kStrCloseWindow               12
#define kStrClosePlain                13

/* kSTRListFinderEditMenu (131) - Edit menu items */
#define kStrUndo                      1
#define kStrCut                       2
#define kStrCopy                      3
#define kStrPaste                     4
#define kStrClear                     5
#define kStrSelectAll                 6
/* Two states of the Undo item, swapped in by SetMenuItemText - hence no
 * command key in the text, which that call would show literally. */
#define kStrUndoMove                  7
#define kStrCantUndo                  8

/* kSTRListFinderViewMenu (132) - View menu items */
#define kStrByIcon                    1
#define kStrByName                    2
#define kStrBySize                    3
#define kStrByKind                    4
#define kStrByLabel                   5
#define kStrByDate                    6

/* kSTRListFinderLabelMenu (133) - Label menu items */
#define kStrLabelNone                 1
#define kStrLabelEssential            2
#define kStrLabelHot                  3
#define kStrLabelInProgress           4
#define kStrLabelCool                 5
#define kStrLabelPersonal             6
#define kStrLabelProject1             7
#define kStrLabelProject2             8

/* kSTRListFinderSpecialMenu (134) - Special menu items
 *
 * The first item is one control whose title depends on what is in front:
 * "Clean Up Desktop", "Clean Up Window", or "Clean Up Selection". All three
 * live here because they are three states of Special's item 1 - they used to
 * sit in the View menu list, which is not where System 7 puts them.
 */
#define kStrCleanUpDesktop            1
#define kStrCleanUpWindow             2
#define kStrCleanUpSelection          3
#define kStrEmptyTrash                4
#define kStrEject                     5
#define kStrEraseDisk                 6
#define kStrRestart                   7
#define kStrShutDown                  8

/* kSTRListFinderControlPanels (135) - Control Panels submenu */
#define kStrCPDesktopPatterns         1
#define kStrCPDateTime                2
#define kStrCPSound                   3
#define kStrCPMouse                   4
#define kStrCPKeyboard                5
#define kStrCPControlStrip            6

/* kSTRListFinderAbout (140) - About This Macintosh dialog */
#define kStrAboutTitle                1
#define kStrBuiltInMemory             2
#define kStrLargestUnusedBlock        3
#define kStrSystemSoftware            4
#define kStrTotalMemory               5
#define kStrMemoryLabel               6
#define kStrSystemLabel               7
#define kStrApplicationsLabel         8
#define kStrUnusedLabel               9

/* kSTRListFinderGetInfo (141) - Get Info dialog */
#define kStrInfoTitle                 1
#define kStrInfoKind                  2
#define kStrInfoSize                  3
#define kStrInfoWhere                 4
#define kStrInfoCreated               5
#define kStrInfoModified              6

/* kSTRListFinderFind (142) - Find dialog */
#define kStrFindTitle                 1
#define kStrFindPrompt                2
#define kStrFindButton                3

/* kSTRListFinderTrash (143) - Trash operations */
#define kStrTrashName                 1
#define kStrEmptyTrashConfirm         2
#define kStrTrashItemCount            3

/* kSTRListFinderDesktop (150) - Desktop items */
#define kStrMacintoshHD               1
#define kStrTrash                     2
#define kStrApplicationsFolder        3
#define kStrDocumentsFolder           4
#define kStrSystemFolder              5

/* kSTRListWindowMgr (400) - Window Manager */
#define kStrUntitled                  1

/* kSTRListDialogMgr (500) - Dialog Manager */
#define kStrOK                        1
#define kStrCancel                    2
#define kStrYes                       3
#define kStrNo                        4
#define kStrSave                      5
#define kStrDontSave                  6

/* kSTRListStandardFile (600) - Standard File */
#define kStrSFOpen                    1
#define kStrSFSave                    2
#define kStrSFCancel                  3
#define kStrSFEject                   4
#define kStrSFDesktop                 5
#define kStrSFUntitled                6

/* kSTRListSystem (800) - System strings */
#define kStrSysRestart                1
#define kStrSysShutDown               2
#define kStrSysError                  3
#define kStrSysOutOfMemory            4

#endif /* STRING_IDS_H */
