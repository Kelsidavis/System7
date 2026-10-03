#include "MemoryMgr/MemoryManager.h"
#include "MenuManager/menu_private.h"
#include <string.h>
/*
 * MenuResources.c - Menu Resource Loading Implementation
 *
 * This file implements MENU and MBAR resource loading and menu creation from resources.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Derived from System 7 ROM analysis (Ghidra) Menu Manager
 */

#include "SystemTypes.h"
#include "System71StdLib.h"
#include "ResourceManager.h"

#include "MenuManager/MenuManager.h"
#include "MenuManager/MenuTypes.h"
#include "MenuManager/MenuLogging.h"

/* ===== MENU Resource Parser ===== */

/*
 * ParseMenuItemString
 * Parses a single menu item string and extracts:
 * - Command key (/ or ^)
 * - Mark character (!)
 * - Text style (<)
 * - Disabled status (()
 * - Submenu indicator (>)
 * - Separator (-)
 *
 * Returns the cleaned item text
 */
static void ParseMenuItemString(const uint8_t* itemData, uint8_t itemLen,
                                 uint8_t* outText, uint8_t* outTextLen,
                                 uint8_t* outCmdChar, uint8_t* outMark,
                                 uint8_t* outStyle, Boolean* outDisabled,
                                 Boolean* outIsSubmenu)
{
    if (!itemData || itemLen == 0) {
        if (outTextLen) *outTextLen = 0;
        return;
    }

    uint8_t cleanText[256];
    uint8_t cleanLen = 0;
    uint8_t pos = 0;

    if (outCmdChar) *outCmdChar = 0;
    if (outMark) *outMark = 0;
    if (outStyle) *outStyle = 0;
    if (outDisabled) *outDisabled = false;
    if (outIsSubmenu) *outIsSubmenu = false;

    /* Parse item for special codes */
    while (pos < itemLen) {
        uint8_t ch = itemData[pos];

        /* Check for special codes */
        if (ch == '/' && pos + 1 < itemLen) {
            /* Command key: /X means Cmd+X */
            if (outCmdChar) *outCmdChar = itemData[pos + 1];
            pos += 2;
            continue;
        }

        if (ch == '^' && pos + 1 < itemLen) {
            /* Alt command: ^X */
            if (outCmdChar) *outCmdChar = itemData[pos + 1] | 0x80;  /* Set high bit for alt */
            pos += 2;
            continue;
        }

        if (ch == '!' && pos + 1 < itemLen) {
            /* Mark character: !C */
            if (outMark) *outMark = itemData[pos + 1];
            pos += 2;
            continue;
        }

        if (ch == '<' && pos + 1 < itemLen) {
            /* Style: <B = bold, <I = italic, <U = underline, <O = outline, <S = shadow */
            uint8_t styleChar = itemData[pos + 1];
            if (outStyle) {
                switch (styleChar) {
                    case 'B': *outStyle |= 0x01; break;  /* bold */
                    case 'I': *outStyle |= 0x02; break;  /* italic */
                    case 'U': *outStyle |= 0x04; break;  /* underline */
                    case 'O': *outStyle |= 0x08; break;  /* outline */
                    case 'S': *outStyle |= 0x10; break;  /* shadow */
                    default: break;
                }
            }
            pos += 2;
            continue;
        }

        if (ch == '(' && pos == 0) {
            /* Disabled: ( prefix */
            if (outDisabled) *outDisabled = true;
            pos++;
            continue;
        }

        if (ch == '-' && cleanLen == 0) {
            /* Separator: - on its own */
            cleanText[cleanLen++] = '-';
            pos++;
            break;
        }

        if (ch == '>') {
            /* Submenu indicator - mark as submenu item */
            if (outIsSubmenu) *outIsSubmenu = true;
            pos++;
            continue;
        }

        /* Regular text */
        if (cleanLen < 255) {
            cleanText[cleanLen++] = ch;
        }
        pos++;
    }

    if (outText && outTextLen) {
        *outTextLen = cleanLen;
        if (cleanLen > 0) {
            memcpy(outText, cleanText, cleanLen);
        }
    }
}

/*
 * ParseMENUResource
 * Parses a MENU resource and returns a MenuHandle
 *
 * MENU resource format (big-endian):
 * 0x00-0x01: menuID (int16)
 * 0x02-0x03: menuWidth (int16, 0 = auto)
 * 0x04-0x05: menuHeight (int16, 0 = auto)
 * 0x06-0x09: reserved (should be 0xFFFFFFFF)
 * 0x0A: title length (byte)
 * 0x0B+: title text
 * Then: item count and items...
 */
MenuHandle ParseMENUResource(Handle resourceHandle)
{
    if (!resourceHandle) {
        return NULL;
    }

    /* CRITICAL: Lock handle before dereferencing to prevent heap compaction issues */
    HLock(resourceHandle);
    uint8_t* data = (uint8_t*)*resourceHandle;
    if (!data) {
        HUnlock(resourceHandle);
        return NULL;
    }

    /* Read menu ID (big-endian) */
    short menuID = (data[0] << 8) | data[1];

    /* Read menu dimensions (we'll auto-calculate in most cases) */
    /* short menuWidth = (data[2] << 8) | data[3]; */
    /* short menuHeight = (data[4] << 8) | data[5]; */

    /* Skip reserved field (0x06-0x09) */

    /* Read title length */
    uint8_t titleLen = data[0x0A];

    if (titleLen == 0) {
        MENU_LOG_WARN("ParseMENUResource: Menu %d has no title\n", menuID);
        titleLen = 1;  /* Use minimal title */
    }

    /* Extract title as Pascal string */
    Str255 titlePascal;
    titlePascal[0] = titleLen;
    memcpy(&titlePascal[1], &data[0x0B], titleLen);

    /* Create menu with parsed title */
    MenuHandle theMenu = NewMenu(menuID, titlePascal);
    if (!theMenu) {
        MENU_LOG_ERROR("ParseMENUResource: NewMenu failed for ID %d\n", menuID);
        HUnlock(resourceHandle);
        return NULL;
    }

    /* Parse menu items starting after title */
    uint8_t itemCount = data[0x0B + titleLen];
    int itemOffset = 0x0C + titleLen;  /* Start of first item (int for bounds checking) */

    for (int i = 0; i < itemCount && itemOffset < 512; i++) {
        /* Each item is: length byte + text */
        uint8_t itemLen = data[itemOffset];
        if (itemLen == 0) {
            break;
        }

        if (itemOffset + itemLen + 1 >= 512) {
            MENU_LOG_WARN("ParseMENUResource: Item %d exceeds resource bounds\n", i);
            break;
        }

        /* Parse this item */
        uint8_t itemText[256];
        uint8_t itemTextLen = 0;
        uint8_t cmdChar = 0, markChar = 0, style = 0;
        Boolean disabled = false;
        Boolean isSubmenu = false;

        ParseMenuItemString(&data[itemOffset + 1], itemLen,
                            itemText, &itemTextLen,
                            &cmdChar, &markChar, &style, &disabled, &isSubmenu);

        /* Create Pascal string for AppendMenu */
        Str255 itemPascal;
        itemPascal[0] = itemTextLen;
        memcpy(&itemPascal[1], itemText, itemTextLen);

        /* Append to menu */
        AppendMenu(theMenu, itemPascal);

        /* Get the item number (1-based index of newly added item) */
        short itemNum = CountMItems(theMenu);

        /* Store command key if present */
        if (cmdChar != 0) {
            SetItemCmd(theMenu, itemNum, (short)cmdChar);
            MENU_LOG_DEBUG("ParseMENUResource: Item %d cmd key = 0x%02X\n", itemNum, cmdChar);
        }

        /* Store mark character if present */
        if (markChar != 0) {
            SetItemMark(theMenu, itemNum, (short)markChar);
            MENU_LOG_DEBUG("ParseMENUResource: Item %d mark = 0x%02X\n", itemNum, markChar);
        }

        /* Store text style if present */
        if (style != 0) {
            SetItemStyle(theMenu, itemNum, (short)style);
            MENU_LOG_DEBUG("ParseMENUResource: Item %d style = 0x%02X\n", itemNum, style);
        }

        /* Disable item if marked as disabled */
        if (disabled) {
            DisableItem(theMenu, itemNum);
            MENU_LOG_DEBUG("ParseMENUResource: Item %d disabled\n", itemNum);
        }

        /* Mark as submenu item if it has submenu indicator */
        if (isSubmenu) {
            /* Set submenuID to 0 initially - will be set by application if needed */
            SetItemSubmenu(theMenu, itemNum, 0);
            MENU_LOG_DEBUG("ParseMENUResource: Item %d is submenu marker\n", itemNum);
        }

        itemOffset += itemLen + 1;
    }

    MENU_LOG_DEBUG("ParseMENUResource: Parsed menu ID=%d, title len=%d, items=%d\n",
                   menuID, titleLen, itemCount);

    /* Unlock handle before returning */
    HUnlock(resourceHandle);

    return theMenu;
}

/*
 * ParseMBARResource
 * Parses an MBAR resource and returns array of menu IDs
 *
 * MBAR resource format (big-endian):
 * 0x00-0x01: count of menus (int16)
 * 0x02+: array of menu IDs (int16 each)
 */
short* ParseMBARResource(Handle resourceHandle, short* outMenuCount)
{
    if (!resourceHandle || !outMenuCount) {
        return NULL;
    }

    /* CRITICAL: Lock handle before dereferencing to prevent heap compaction issues */
    HLock(resourceHandle);
    uint8_t* data = (uint8_t*)*resourceHandle;
    if (!data) {
        HUnlock(resourceHandle);
        return NULL;
    }

    /* Read menu count (big-endian) */
    short count = (data[0] << 8) | data[1];

    if (count <= 0 || count > 32) {
        MENU_LOG_ERROR("ParseMBARResource: Invalid menu count %d\n", count);
        HUnlock(resourceHandle);
        return NULL;
    }

    /* Allocate array for menu IDs with overflow protection */
    if ((size_t)count > SIZE_MAX / sizeof(short)) {
        HUnlock(resourceHandle);
        return NULL;  /* Would overflow */
    }

    short* menuIDs = (short*)NewPtr(count * sizeof(short));
    if (!menuIDs) {
        HUnlock(resourceHandle);
        return NULL;
    }

    /* Extract menu IDs (big-endian int16) */
    for (int i = 0; i < count; i++) {
        uint8_t* menuIDPtr = &data[2 + (i * 2)];
        /* Big-endian conversion */
        menuIDs[i] = (int16_t)(((int16_t)menuIDPtr[0] << 8) | menuIDPtr[1]);
    }

    *outMenuCount = count;

    /* Unlock handle before returning */
    HUnlock(resourceHandle);

    return menuIDs;
}

/* AddResMenu and InsertResMenu are in MenuResourceNames.c; InsertFontResMenu is in MenuItems.c. */
