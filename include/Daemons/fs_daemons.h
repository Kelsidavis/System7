/* System 7X - Filesystem Daemon Management
 *
 * Interface for spawning and managing filesystem daemons.
 */

#ifndef FS_DAEMONS_H
#define FS_DAEMONS_H

#include <stdbool.h>
#include "../ProcessMgr/ProcessTypes.h"

/* HFS Daemon */
bool HFSd_Spawn(void);
void HFSd_Shutdown(void);
ProcessID HFSd_GetPID(void);

/* FAT32 Daemon */
bool FATd_Spawn(void);
void FATd_Shutdown(void);
ProcessID FATd_GetPID(void);

/* Spawn all filesystem daemons */
void spawn_fs_daemons(void);

#endif /* FS_DAEMONS_H */
