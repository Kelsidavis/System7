#ifndef CONTROL_PANEL_INTERNAL_H
#define CONTROL_PANEL_INTERNAL_H

#include <stddef.h>
#include "ControlManager/ControlManager.h"

static inline void ControlPanel_DisposeControls(ControlHandle** controls,
                                                size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        if (controls[i] && *controls[i]) {
            DisposeControl(*controls[i]);
            *controls[i] = NULL;
        }
    }
}

#endif /* CONTROL_PANEL_INTERNAL_H */
