#ifndef TEXTEDIT_INTERNAL_H
#define TEXTEDIT_INTERNAL_H

#include "TextEdit/TextEdit.h"

typedef struct TEExtRec {
    TERec base;
    Handle hLines;
    SInt16 nLines;
    Handle hStyles;
    Boolean dirty;
    Boolean readOnly;
    Boolean wordWrap;
    SInt16 dragAnchor;
    Boolean inDragSel;
    UInt32 lastClickTime;
    SInt16 clickCount;
    SInt16 viewDH;
    SInt16 viewDV;
    Boolean autoViewEnabled;
} TEExtRec;

typedef TEExtRec *TEExtPtr, **TEExtHandle;

#endif
