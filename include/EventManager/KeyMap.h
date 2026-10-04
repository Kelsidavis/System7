#ifndef EVENTMANAGER_KEYMAP_H
#define EVENTMANAGER_KEYMAP_H

#include "EventManager/EventTypes.h"
#include <string.h>

/* KeyMap stores four native-endian words but may be byte-aligned. */
static inline Boolean KeyMapHasKey(const KeyMap map, UInt16 scanCode)
{
    if (scanCode >= 128) return false;
    UInt32 word;
    memcpy(&word, map + (scanCode / 32) * sizeof(word), sizeof(word));
    return (word & (1U << (scanCode % 32))) != 0;
}

static inline void KeyMapSetKey(KeyMap map, UInt16 scanCode, Boolean isDown)
{
    if (scanCode >= 128) return;
    UInt32 word;
    UInt32 mask = 1U << (scanCode % 32);
    UInt8* slot = map + (scanCode / 32) * sizeof(word);
    memcpy(&word, slot, sizeof(word));
    if (isDown) word |= mask;
    else word &= ~mask;
    memcpy(slot, &word, sizeof(word));
}

static inline UInt16 KeyMapModifiers(const KeyMap map)
{
    UInt16 mods = 0;
    if (KeyMapHasKey(map, kScanCommand) || KeyMapHasKey(map, kScanRightCommand)) mods |= cmdKey;
    if (KeyMapHasKey(map, kScanShift)) mods |= shiftKey;
    if (KeyMapHasKey(map, kScanRightShift)) mods |= shiftKey | rightShiftKey;
    if (KeyMapHasKey(map, kScanOption)) mods |= optionKey;
    if (KeyMapHasKey(map, kScanRightOption)) mods |= optionKey | rightOptionKey;
    if (KeyMapHasKey(map, kScanControl)) mods |= controlKey;
    if (KeyMapHasKey(map, kScanRightControl)) mods |= controlKey | rightControlKey;
    if (KeyMapHasKey(map, kScanCapsLock)) mods |= alphaLock;
    return mods;
}

#endif
