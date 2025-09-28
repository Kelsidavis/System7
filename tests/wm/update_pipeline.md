# T-WM-update-01: Update Event Pipeline

**Provenance**: IM:Windows Vol I pp. 2-76 to 2-82 - "Update Events" and BeginUpdate/EndUpdate semantics

## Test Description

This test validates the Window Manager update event pipeline, ensuring that:
1. CheckUpdate correctly reports pending updates
2. BeginUpdate/EndUpdate properly manages update regions
3. Window Manager draws chrome only, never content
4. Update region clipping works correctly

## Test Steps

1. **Create Hidden Window**
   ```c
   WindowPtr w = NewWindow(NULL, &bounds, "\pTest Window", false, documentProc,
                           (WindowPtr)-1L, true, 0L);
   ```
   - Window created but not visible
   - Expected: No update event pending

2. **Show and Invalidate**
   ```c
   ShowWindow(w);
   Rect innerRect = bounds;
   InsetRect(&innerRect, 10, 10);
   InvalRect(&innerRect);
   ```
   - Make window visible
   - Invalidate portion of content area
   - Expected: Window chrome drawn by WM, content area marked for update

3. **Check Update Event**
   ```c
   Boolean hasUpdate = CheckUpdate(w);
   ```
   - Expected: `hasUpdate == true`
   - CheckUpdate validates but does NOT draw
   - Update region should still be pending

4. **Process Update with BeginUpdate/EndUpdate**
   ```c
   BeginUpdate(w);
   // Application draws content here (if any)
   EndUpdate(w);
   ```
   - BeginUpdate sets clipping to (visRgn ∩ updateRgn)
   - EndUpdate validates the update region
   - Expected: Update region cleared after EndUpdate

5. **Verify Update Cleared**
   ```c
   Boolean hasUpdate2 = CheckUpdate(w);
   ```
   - Expected: `hasUpdate2 == false`
   - Update region should be empty after EndUpdate

## Expected Behavior

- **Window Manager Responsibility**: Draw chrome (title bar, frame, controls)
- **Application Responsibility**: Draw content inside BeginUpdate/EndUpdate
- **Never**: Window Manager draws content, Application draws chrome

## Validation Points

1. CheckUpdate returns true when updateRgn is non-empty
2. CheckUpdate returns false when updateRgn is empty
3. BeginUpdate sets port and clipping correctly
4. EndUpdate validates update region
5. Window chrome drawn by DrawWindow during ShowWindow
6. Content area never touched by Window Manager

## Test ID

- **ID**: T-WM-update-01
- **Category**: Window Manager / Update Events
- **Priority**: Critical
- **Status**: Specification Complete