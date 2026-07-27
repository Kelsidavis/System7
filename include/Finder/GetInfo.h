/*
 * GetInfo.h - Get Info Window API
 */

#ifndef __GET_INFO_H__
#define __GET_INFO_H__

#include "SystemTypes.h"
#include "FS/hfs_types.h"

/* Show Get Info window for a file/folder */
void GetInfo_Show(VRefNum vref, FileID fileID);

/* Close Get Info window if it matches */
Boolean GetInfo_CloseIf(WindowPtr w);

/* Handle update event for Get Info window */
Boolean GetInfo_HandleUpdate(WindowPtr w);

/* Check if window is the Get Info window */
Boolean GetInfo_IsInfoWindow(WindowPtr w);

#endif /* __GET_INFO_H__ */
