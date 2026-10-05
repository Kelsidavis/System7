/* Platform integration for menu keyboard modifiers. */

#include "MenuManager/menu_private.h"
#include "Platform/PS2Input.h"

Boolean Platform_GetKeyModifiers(unsigned long* modifiers)
{
    if (!modifiers) {
        return false;
    }

    *modifiers = (unsigned long)GetPS2Modifiers();
    return true;
}
