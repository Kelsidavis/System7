/* System 7X - HFS Filesystem Daemon (HFSd)
 *
 * User-space HFS filesystem daemon.
 * Handles all HFS filesystem operations via IPC with kernel VFS.
 */

#include "../common/fs_daemon_api.h"
#include "../../../include/ProcessMgr/ProcessTypes.h"
#include <string.h>

/* Forward declaration from HFS driver */
extern FileSystemOps* HFS_GetOps(void);

/* HFS daemon private data */
static struct {
    MessagePort request_port;
    MessagePort response_port;
    FileSystemOps* fs_ops;
    ProcessID pid;
    bool initialized;
} g_hfsd = { 0 };

/* HFS daemon thread entry point */
static void* hfsd_thread_main(void* arg) {
    serial_printf("[HFSd] Starting HFS daemon...\n");

    /* Get HFS filesystem operations */
    g_hfsd.fs_ops = HFS_GetOps();
    if (!g_hfsd.fs_ops) {
        serial_printf("[HFSd] ERROR: Failed to get HFS operations\n");
        return NULL;
    }

    serial_printf("[HFSd] HFS operations initialized (%s v%u)\n",
                  g_hfsd.fs_ops->fs_name, g_hfsd.fs_ops->fs_version);

    /* Enter event loop */
    fsd_event_loop("HFSd",
                   g_hfsd.request_port,
                   g_hfsd.response_port,
                   g_hfsd.fs_ops,
                   NULL);

    serial_printf("[HFSd] Daemon terminated\n");
    return NULL;
}

/* Initialize and spawn HFS daemon */
bool HFSd_Spawn(void) {
    if (g_hfsd.initialized) {
        serial_printf("[HFSd] Already initialized\n");
        return false;
    }

    /* Create message queues */
    g_hfsd.request_port = IPC_CreateQueue("hfsd_req");
    g_hfsd.response_port = IPC_CreateQueue("hfsd_resp");

    if (!g_hfsd.request_port || !g_hfsd.response_port) {
        serial_printf("[HFSd] ERROR: Failed to create message queues\n");
        return false;
    }

    /* Spawn daemon thread */
    g_hfsd.pid = Proc_New("HFSd", hfsd_thread_main, NULL, 8192, 10);
    if (g_hfsd.pid == 0) {
        serial_printf("[HFSd] ERROR: Failed to spawn daemon thread\n");
        IPC_DestroyQueue(g_hfsd.request_port);
        IPC_DestroyQueue(g_hfsd.response_port);
        return false;
    }

    /* Register with FSD subsystem */
    if (!FSD_Register("HFSd", g_hfsd.pid, g_hfsd.request_port, g_hfsd.response_port)) {
        serial_printf("[HFSd] ERROR: Failed to register with FSD\n");
        return false;
    }

    g_hfsd.initialized = true;

    serial_printf("[HFSd] Daemon spawned successfully (pid %u)\n", g_hfsd.pid);
    return true;
}

/* Shutdown HFS daemon */
void HFSd_Shutdown(void) {
    if (!g_hfsd.initialized) {
        return;
    }

    serial_printf("[HFSd] Shutting down...\n");

    /* Send shutdown message */
    FSRequest req = {
        .type = FS_MSG_SHUTDOWN,
        .request_id = 0,
    };
    IPC_Send(g_hfsd.request_port, &req, sizeof(FSRequest));

    /* Unregister from FSD */
    FSD_Unregister("HFSd");

    /* Cleanup */
    IPC_DestroyQueue(g_hfsd.request_port);
    IPC_DestroyQueue(g_hfsd.response_port);

    g_hfsd.initialized = false;
    serial_printf("[HFSd] Shutdown complete\n");
}

/* Get HFSd daemon PID */
ProcessID HFSd_GetPID(void) {
    return g_hfsd.pid;
}
