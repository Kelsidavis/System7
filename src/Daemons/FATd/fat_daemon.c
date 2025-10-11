/* System 7X - FAT32 Filesystem Daemon (FATd)
 *
 * User-space FAT32 filesystem daemon.
 * Handles all FAT32 filesystem operations via IPC with kernel VFS.
 */

#include "../common/fs_daemon_api.h"
#include "../../../include/ProcessMgr/ProcessTypes.h"
#include <string.h>

/* Forward declaration from FAT32 driver */
extern FileSystemOps* FAT32_GetOps(void);

/* FAT daemon private data */
static struct {
    MessagePort request_port;
    MessagePort response_port;
    FileSystemOps* fs_ops;
    ProcessID pid;
    bool initialized;
} g_fatd = { 0 };

/* FAT daemon thread entry point */
static void* fatd_thread_main(void* arg) {
    serial_printf("[FATd] Starting FAT32 daemon...\n");

    /* Get FAT32 filesystem operations */
    g_fatd.fs_ops = FAT32_GetOps();
    if (!g_fatd.fs_ops) {
        serial_printf("[FATd] ERROR: Failed to get FAT32 operations\n");
        return NULL;
    }

    serial_printf("[FATd] FAT32 operations initialized (%s v%u)\n",
                  g_fatd.fs_ops->fs_name, g_fatd.fs_ops->fs_version);

    /* Enter event loop */
    fsd_event_loop("FATd",
                   g_fatd.request_port,
                   g_fatd.response_port,
                   g_fatd.fs_ops,
                   NULL);

    serial_printf("[FATd] Daemon terminated\n");
    return NULL;
}

/* Initialize and spawn FAT daemon */
bool FATd_Spawn(void) {
    if (g_fatd.initialized) {
        serial_printf("[FATd] Already initialized\n");
        return false;
    }

    /* Create message queues */
    g_fatd.request_port = IPC_CreateQueue("fatd_req");
    g_fatd.response_port = IPC_CreateQueue("fatd_resp");

    if (!g_fatd.request_port || !g_fatd.response_port) {
        serial_printf("[FATd] ERROR: Failed to create message queues\n");
        return false;
    }

    /* Spawn daemon thread */
    g_fatd.pid = Proc_New("FATd", fatd_thread_main, NULL, 8192, 10);
    if (g_fatd.pid == 0) {
        serial_printf("[FATd] ERROR: Failed to spawn daemon thread\n");
        IPC_DestroyQueue(g_fatd.request_port);
        IPC_DestroyQueue(g_fatd.response_port);
        return false;
    }

    /* Register with FSD subsystem */
    if (!FSD_Register("FATd", g_fatd.pid, g_fatd.request_port, g_fatd.response_port)) {
        serial_printf("[FATd] ERROR: Failed to register with FSD\n");
        return false;
    }

    g_fatd.initialized = true;

    serial_printf("[FATd] Daemon spawned successfully (pid %u)\n", g_fatd.pid);
    return true;
}

/* Shutdown FAT daemon */
void FATd_Shutdown(void) {
    if (!g_fatd.initialized) {
        return;
    }

    serial_printf("[FATd] Shutting down...\n");

    /* Send shutdown message */
    FSRequest req = {
        .type = FS_MSG_SHUTDOWN,
        .request_id = 0,
    };
    IPC_Send(g_fatd.request_port, &req, sizeof(FSRequest));

    /* Unregister from FSD */
    FSD_Unregister("FATd");

    /* Cleanup */
    IPC_DestroyQueue(g_fatd.request_port);
    IPC_DestroyQueue(g_fatd.response_port);

    g_fatd.initialized = false;
    serial_printf("[FATd] Shutdown complete\n");
}

/* Get FATd daemon PID */
ProcessID FATd_GetPID(void) {
    return g_fatd.pid;
}
