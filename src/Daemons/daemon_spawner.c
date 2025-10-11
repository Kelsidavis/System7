/* System 7X - Filesystem Daemon Spawner
 *
 * Spawns and initializes all filesystem daemons.
 */

#include "../../include/Daemons/fs_daemons.h"
#include "../../include/System71StdLib.h"

/* Spawn all filesystem daemons */
void spawn_fs_daemons(void) {
    serial_printf("[DAEMONS] Spawning filesystem daemons...\n");

#ifdef ENABLE_FS_HFS
    /* Spawn HFS daemon */
    if (HFSd_Spawn()) {
        serial_printf("[DAEMONS] HFS daemon started\n");
    } else {
        serial_printf("[DAEMONS] WARNING: Failed to start HFS daemon\n");
    }
#endif

#ifdef ENABLE_FS_FAT32
    /* Spawn FAT32 daemon */
    if (FATd_Spawn()) {
        serial_printf("[DAEMONS] FAT32 daemon started\n");
    } else {
        serial_printf("[DAEMONS] WARNING: Failed to start FAT32 daemon\n");
    }
#endif

    serial_printf("[DAEMONS] Filesystem daemon initialization complete\n");
}
