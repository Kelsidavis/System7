/************************************************************
 *
 * TextSelection.h
 * System 7.1 Portable - TextEdit Selection Management
 *
 * Selection handling, highlighting, cursor management, and
 * navigation APIs for TextEdit. Provides comprehensive
 * text selection and cursor positioning capabilities.
 *
 * Copyright (c) 2024 System7.1-Portable Project
 * Derived from ROM analysis: TextEdit Manager 1985-1992
 *
 ************************************************************/

#ifndef __TEXTSELECTION_H__
#define __TEXTSELECTION_H__

#include "SystemTypes.h"

#ifndef __TYPES_H__
#include "SystemTypes.h"
#endif

#ifndef __QUICKDRAW_H__
#include "QuickDraw/QuickDraw.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/************************************************************
 * Selection Constants
 ************************************************************/

/* Selection types */

/* Selection modes */

/* Highlight styles */

/* Caret styles */

/* Navigation directions */

/************************************************************
 * Selection Data Structures
 ************************************************************/

/* Selection range */

/* Selection state */

/* Highlight information */

/* Selection anchor for drag operations */

/************************************************************
 * Core Selection Functions
 ************************************************************/

/* Selection management */
OSErr TESetSelection(TEHandle hTE, long start, long end);
OSErr TEGetSelection(TEHandle hTE, long* start, long* end);
OSErr TEGetSelectionRange(TEHandle hTE, TESelectionRange* range);
OSErr TESetSelectionRange(TEHandle hTE, const TESelectionRange* range);

/* Selection types and modes */
OSErr TESetSelectionType(TEHandle hTE, short type);
short TEGetSelectionType(TEHandle hTE);
OSErr TESetSelectionMode(TEHandle hTE, short mode);
short TEGetSelectionMode(TEHandle hTE);

/* Selection extension */
OSErr TEExtendSelection(TEHandle hTE, long newEnd);
OSErr TEExtendSelectionToPoint(TEHandle hTE, Point pt);
OSErr TEExtendSelectionByAmount(TEHandle hTE, long amount);

/* Selection anchoring */
OSErr TESetSelectionAnchor(TEHandle hTE, long position);
OSErr TEGetSelectionAnchor(TEHandle hTE, long* position);
OSErr TEClearSelectionAnchor(TEHandle hTE);

/************************************************************
 * Specialized Selection Operations
 ************************************************************/

/* Word selection */
OSErr TESelectWord(TEHandle hTE, long position);
OSErr TESelectWordAt(TEHandle hTE, Point pt);
OSErr TEExtendSelectionToWord(TEHandle hTE, long position);
OSErr TEGetWordBoundaries(TEHandle hTE, long position, long* start, long* end);

/* Line selection */
OSErr TESelectLine(TEHandle hTE, long position);
OSErr TESelectLineAt(TEHandle hTE, Point pt);
OSErr TEExtendSelectionToLine(TEHandle hTE, long position);
OSErr TEGetLineBoundaries(TEHandle hTE, long position, long* start, long* end);

/* Paragraph selection */
OSErr TESelectParagraph(TEHandle hTE, long position);
OSErr TESelectParagraphAt(TEHandle hTE, Point pt);
OSErr TEExtendSelectionToParagraph(TEHandle hTE, long position);
OSErr TEGetParagraphBoundaries(TEHandle hTE, long position, long* start, long* end);

/* Document-wide selection */
OSErr TESelectAll(TEHandle hTE);
OSErr TESelectNone(TEHandle hTE);
Boolean TEHasSelection(TEHandle hTE);
Boolean TEIsSelectionEmpty(TEHandle hTE);

/************************************************************
 * Caret Management
 ************************************************************/

/* Caret positioning */
OSErr TESetCaretPosition(TEHandle hTE, long position);
long TEGetCaretPosition(TEHandle hTE);
OSErr TESetCaretPoint(TEHandle hTE, Point pt);
Point TEGetCaretPoint(TEHandle hTE);

/* Caret visibility */
OSErr TEShowCaret(TEHandle hTE);
OSErr TEHideCaret(TEHandle hTE);
Boolean TEIsCaretVisible(TEHandle hTE);
OSErr TEToggleCaretVisibility(TEHandle hTE);

/* Caret styling */
OSErr TESetCaretStyle(TEHandle hTE, short style);
short TEGetCaretStyle(TEHandle hTE);
OSErr TESetCaretColor(TEHandle hTE, const RGBColor* color);
OSErr TEGetCaretColor(TEHandle hTE, RGBColor* color);

/* Caret blinking */
OSErr TESetCaretBlinkRate(TEHandle hTE, long blinkRate);
long TEGetCaretBlinkRate(TEHandle hTE);
OSErr TEStartCaretBlinking(TEHandle hTE);
OSErr TEStopCaretBlinking(TEHandle hTE);

/************************************************************
 * Highlighting and Drawing
 ************************************************************/

/* Highlight management */
OSErr TESetHighlightStyle(TEHandle hTE, short style);
short TEGetHighlightStyle(TEHandle hTE);
OSErr TESetHighlightColor(TEHandle hTE, const RGBColor* color, const RGBColor* bgColor);
OSErr TEGetHighlightColor(TEHandle hTE, RGBColor* color, RGBColor* bgColor);

/* Selection drawing */
OSErr TEDrawSelection(TEHandle hTE);
OSErr TEEraseSelection(TEHandle hTE);
OSErr TEInvertSelection(TEHandle hTE);
OSErr TEDrawSelectionOutline(TEHandle hTE);

/* Custom highlighting */
OSErr TESetCustomHighlight(TEHandle hTE, const TEHighlightInfo* info);
OSErr TEGetCustomHighlight(TEHandle hTE, TEHighlightInfo* info);
OSErr TEDrawCustomSelection(TEHandle hTE, const TEHighlightInfo* info);

/* Highlight regions */
OSErr TEGetSelectionRegion(TEHandle hTE, RgnHandle* region);
OSErr TEGetCaretRegion(TEHandle hTE, RgnHandle* region);

/************************************************************
 * Navigation and Movement
 ************************************************************/

/* Character-based navigation */
OSErr TEMoveLeft(TEHandle hTE, Boolean extend);
OSErr TEMoveRight(TEHandle hTE, Boolean extend);
OSErr TEMoveUp(TEHandle hTE, Boolean extend);
OSErr TEMoveDown(TEHandle hTE, Boolean extend);

/* Word-based navigation */
OSErr TEMoveWordLeft(TEHandle hTE, Boolean extend);
OSErr TEMoveWordRight(TEHandle hTE, Boolean extend);
OSErr TEMoveToWordBoundary(TEHandle hTE, long position, Boolean forward, Boolean extend);

/* Line-based navigation */
OSErr TEMoveToLineStart(TEHandle hTE, Boolean extend);
OSErr TEMoveToLineEnd(TEHandle hTE, Boolean extend);
OSErr TEMoveToLine(TEHandle hTE, short lineNumber, Boolean extend);

/* Document navigation */
OSErr TEMoveToDocumentStart(TEHandle hTE, Boolean extend);
OSErr TEMoveToDocumentEnd(TEHandle hTE, Boolean extend);
OSErr TEMoveToPosition(TEHandle hTE, long position, Boolean extend);

/* Page-based navigation */
OSErr TEMovePageUp(TEHandle hTE, Boolean extend);
OSErr TEMovePageDown(TEHandle hTE, Boolean extend);
OSErr TEMoveByPages(TEHandle hTE, short pageCount, Boolean extend);

/* Generic navigation */
OSErr TENavigate(TEHandle hTE, short direction, Boolean extend);
OSErr TENavigateByAmount(TEHandle hTE, short direction, long amount, Boolean extend);

/************************************************************
 * Hit Testing and Point Conversion
 ************************************************************/

/* Point to offset conversion */
long TEPointToOffset(TEHandle hTE, Point pt);
long TEPointToNearestOffset(TEHandle hTE, Point pt);
OSErr TEPointToOffsetEx(TEHandle hTE, Point pt, long* offset, Boolean* leadingEdge);

/* Offset to point conversion */
Point TEOffsetToPoint(TEHandle hTE, long offset);
OSErr TEOffsetToPointEx(TEHandle hTE, long offset, Point* pt, short* lineHeight);
OSErr TEGetOffsetBounds(TEHandle hTE, long offset, Rect* bounds);

/* Line information */
short TEOffsetToLine(TEHandle hTE, long offset);
long TELineToOffset(TEHandle hTE, short lineNumber);
OSErr TEGetLineInfo(TEHandle hTE, short lineNumber, long* start, long* end, Point* topLeft);

/* Hit testing */
Boolean TEPointInSelection(TEHandle hTE, Point pt);
Boolean TEPointInText(TEHandle hTE, Point pt);
OSErr TEGetHitInfo(TEHandle hTE, Point pt, long* offset, short* edge, short* lineNumber);

/************************************************************
 * Selection Utilities
 ************************************************************/

/* Selection analysis */
long TEGetSelectionLength(TEHandle hTE);
OSErr TEGetSelectedText(TEHandle hTE, Handle* textHandle);
OSErr TEGetSelectedTextPtr(TEHandle hTE, Ptr* textPtr, long* length);

/* Selection validation */
OSErr TEValidateSelection(TEHandle hTE, long* start, long* end);
OSErr TEClampSelection(TEHandle hTE, long* position);
Boolean TEIsValidSelectionRange(TEHandle hTE, long start, long end);

/* Selection state management */
OSErr TEGetSelectionState(TEHandle hTE, TESelectionState* state);
OSErr TESetSelectionState(TEHandle hTE, const TESelectionState* state);
OSErr TEPushSelectionState(TEHandle hTE);
OSErr TEPopSelectionState(TEHandle hTE);

/* Multi-selection support (for future extension) */
OSErr TEAddSelection(TEHandle hTE, long start, long end);
OSErr TERemoveSelection(TEHandle hTE, short index);
short TEGetSelectionCount(TEHandle hTE);
OSErr TEGetSelectionByIndex(TEHandle hTE, short index, long* start, long* end);

/************************************************************
 * Drag and Drop Selection
 ************************************************************/

/* Drag operations */
OSErr TEStartDragSelection(TEHandle hTE, Point startPt);
OSErr TEUpdateDragSelection(TEHandle hTE, Point currentPt);
OSErr TEEndDragSelection(TEHandle hTE, Point endPt, Boolean copy);
OSErr TECancelDragSelection(TEHandle hTE);

/* Drop operations */
OSErr TECanAcceptDrop(TEHandle hTE, Point dropPt, Boolean* canAccept);
OSErr TEGetDropPosition(TEHandle hTE, Point dropPt, long* position);
OSErr TEPerformDrop(TEHandle hTE, Point dropPt, Handle textData, Boolean move);

/* Drag feedback */
OSErr TEDrawDragFeedback(TEHandle hTE, Point currentPt);
OSErr TEEraseDragFeedback(TEHandle hTE);

/************************************************************
 * Advanced Selection Features
 ************************************************************/

/* Smart selection */
OSErr TEEnableSmartSelection(TEHandle hTE, Boolean enable);
Boolean TEIsSmartSelectionEnabled(TEHandle hTE);
OSErr TESmartExtendSelection(TEHandle hTE, Point pt);

/* Selection persistence */
OSErr TESetStickySelection(TEHandle hTE, Boolean sticky);
Boolean TEIsStickySelection(TEHandle hTE);
OSErr TEMaintainSelectionOnEdit(TEHandle hTE, Boolean maintain);

/* Selection callbacks */

OSErr TESetSelectionChangedProc(TEHandle hTE, TESelectionChangedProc proc);
TESelectionChangedProc TEGetSelectionChangedProc(TEHandle hTE);

/* Accessibility selection support */
OSErr TEAnnounceSelectionChange(TEHandle hTE);
OSErr TEGetSelectionDescription(TEHandle hTE, StringPtr description, short maxLength);

#ifdef __cplusplus
}
#endif

#endif /* __TEXTSELECTION_H__ */