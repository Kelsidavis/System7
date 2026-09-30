#ifndef QD_REGIONS_H
#define QD_REGIONS_H

#include "../SystemTypes.h"

// Forward declarations
Boolean PtInRect(Point pt, const Rect *r);
Boolean EmptyRect(const Rect *r);
void InsetRect(Rect* r, short dh, short dv);
void OffsetRect(Rect* r, short dh, short dv);
Boolean SectRect(const Rect* src1, const Rect* src2, Rect* dst);
void UnionRect(const Rect* src1, const Rect* src2, Rect* dst);
Boolean EqualRect(const Rect* rect1, const Rect* rect2);

// Region operations
RgnHandle NewRgn(void);
void OpenRgn(void);
void CloseRgn(RgnHandle rgn);
void DisposeRgn(RgnHandle rgn);
void CopyRgn(RgnHandle srcRgn, RgnHandle dstRgn);
void SetEmptyRgn(RgnHandle rgn);
void SetRectRgn(RgnHandle rgn, short left, short top, short right, short bottom);
void RectRgn(RgnHandle rgn, const Rect* r);
void OffsetRgn(RgnHandle rgn, short dh, short dv);
void InsetRgn(RgnHandle rgn, short dh, short dv);
void SectRgn(RgnHandle srcRgnA, RgnHandle srcRgnB, RgnHandle dstRgn);
void UnionRgn(RgnHandle srcRgnA, RgnHandle srcRgnB, RgnHandle dstRgn);
void DiffRgn(RgnHandle srcRgnA, RgnHandle srcRgnB, RgnHandle dstRgn);
void XorRgn(RgnHandle srcRgnA, RgnHandle srcRgnB, RgnHandle dstRgn);
Boolean PtInRgn(Point pt, RgnHandle rgn);
Boolean RectInRgn(const Rect* r, RgnHandle rgn);
Boolean EqualRgn(RgnHandle rgnA, RgnHandle rgnB);
Boolean EmptyRgn(RgnHandle rgn);

// Frame and paint operations
void FrameRgn(RgnHandle rgn);
void PaintRgn(RgnHandle rgn);
void EraseRgn(RgnHandle rgn);
void InvertRgn(RgnHandle rgn);
void FillRgn(RgnHandle rgn, const Pattern* pat);

// Inline implementations for simple checks
inline Boolean IsEmptyRgn(RgnHandle region)
{
    if (region == NULL) return true;
    return ((*region)->rgnSize == 10 &&
            (*region)->rgnBBox.left == (*region)->rgnBBox.right);
}

inline Boolean IsRectRgn(RgnHandle region)
{
    if (region == NULL) return false;
    return ((*region)->rgnSize == 10);
}

inline Boolean SimplePtInRgn(Point pt, RgnHandle region)
{
    if (region == NULL) return false;
    if (IsEmptyRgn(region)) return false;
    return PtInRect(pt, &(*region)->rgnBBox);
}

#endif // QD_REGIONS_H
