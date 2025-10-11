/* System 7X - Filesystem Daemon Common API
 *
 * Common utilities for filesystem daemon implementation.
 * Used by HFSd, FATd, and other user-space filesystem daemons.
 */

#ifndef FS_DAEMON_API_H
#define FS_DAEMON_API_H

#include "../../../include/Nanokernel/fs_daemon.h"
#include "../../../include/Nanokernel/ipc.h"
#include "../../../include/Nanokernel/filesystem.h"
#include "../../../include/System71StdLib.h"

/* Daemon event loop - processes requests until shutdown */
void fsd_event_loop(const char* daemon_name,
                    MessagePort req_port,
                    MessagePort resp_port,
                    FileSystemOps* fs_ops,
                    void* fs_private);

/* Helper to send response */
bool fsd_send_response(MessagePort resp_port, const FSResponse* resp);

/* Helper to create error response */
FSResponse fsd_make_error_response(uint32_t request_id, int32_t error_code);

/* Helper to create success response */
FSResponse fsd_make_success_response(uint32_t request_id);

#endif /* FS_DAEMON_API_H */
