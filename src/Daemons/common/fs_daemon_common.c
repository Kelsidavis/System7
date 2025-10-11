/* System 7X - Filesystem Daemon Common Implementation
 *
 * Shared event loop and utilities for filesystem daemons.
 */

#include "fs_daemon_api.h"
#include <string.h>

/* Send response to kernel */
bool fsd_send_response(MessagePort resp_port, const FSResponse* resp) {
    return IPC_Send(resp_port, resp, sizeof(FSResponse));
}

/* Create error response */
FSResponse fsd_make_error_response(uint32_t request_id, int32_t error_code) {
    FSResponse resp = {
        .request_id = request_id,
        .result = error_code,
        .data_length = 0,
    };
    return resp;
}

/* Create success response */
FSResponse fsd_make_success_response(uint32_t request_id) {
    FSResponse resp = {
        .request_id = request_id,
        .result = 0,
        .data_length = 0,
    };
    return resp;
}

/* Main event loop for filesystem daemons */
void fsd_event_loop(const char* daemon_name,
                    MessagePort req_port,
                    MessagePort resp_port,
                    FileSystemOps* fs_ops,
                    void* fs_private) {
    serial_printf("[%s] Event loop started\n", daemon_name);

    bool running = true;
    while (running) {
        /* Receive request from kernel */
        FSRequest req;
        size_t req_len = 0;

        if (!IPC_Receive(req_port, &req, sizeof(FSRequest), &req_len)) {
            serial_printf("[%s] ERROR: Failed to receive request\n", daemon_name);
            continue;
        }

        if (req_len != sizeof(FSRequest)) {
            serial_printf("[%s] ERROR: Invalid request size (%zu != %zu)\n",
                          daemon_name, req_len, sizeof(FSRequest));
            continue;
        }

        /* Process request based on type */
        FSResponse resp = { .request_id = req.request_id };

        switch (req.type) {
            case FS_MSG_READ_FILE:
                serial_printf("[%s] READ file_id=%llu offset=%llu length=%u\n",
                              daemon_name, req.file_id, req.offset, req.length);

                /* Call filesystem read function */
                if (fs_ops && fs_ops->read) {
                    size_t bytes_read = 0;
                    bool success = fs_ops->read(NULL, req.file_id, req.offset,
                                                resp.data, req.length, &bytes_read);
                    if (success) {
                        resp.result = 0;
                        resp.data_length = (uint32_t)bytes_read;
                        serial_printf("[%s] READ OK (%zu bytes)\n", daemon_name, bytes_read);
                    } else {
                        resp.result = -1;
                        resp.data_length = 0;
                        serial_printf("[%s] READ FAILED\n", daemon_name);
                    }
                } else {
                    resp = fsd_make_error_response(req.request_id, -1);
                }
                break;

            case FS_MSG_WRITE_FILE:
                serial_printf("[%s] WRITE file_id=%llu offset=%llu length=%u\n",
                              daemon_name, req.file_id, req.offset, req.length);

                /* Call filesystem write function */
                if (fs_ops && fs_ops->write) {
                    /* TODO: Extract write data from request */
                    resp = fsd_make_success_response(req.request_id);
                    resp.param1 = req.length;  /* Bytes written */
                } else {
                    resp = fsd_make_error_response(req.request_id, -1);
                }
                break;

            case FS_MSG_LIST_DIR:
                serial_printf("[%s] LIST_DIR dir_id=%llu\n", daemon_name, req.file_id);

                /* Call filesystem enumerate function */
                if (fs_ops && fs_ops->enumerate) {
                    bool success = fs_ops->enumerate(NULL, req.file_id, NULL, NULL);
                    resp.result = success ? 0 : -1;
                } else {
                    resp = fsd_make_error_response(req.request_id, -1);
                }
                break;

            case FS_MSG_LOOKUP:
                serial_printf("[%s] LOOKUP dir_id=%llu path='%s'\n",
                              daemon_name, req.file_id, req.path);

                /* Call filesystem lookup function */
                if (fs_ops && fs_ops->lookup) {
                    uint64_t entry_id = 0;
                    bool is_dir = false;
                    bool success = fs_ops->lookup(NULL, req.file_id, req.path,
                                                  &entry_id, &is_dir);
                    if (success) {
                        resp.result = 0;
                        resp.param1 = entry_id;
                        resp.param2 = is_dir ? 1 : 0;
                        serial_printf("[%s] LOOKUP OK (id=%llu, is_dir=%d)\n",
                                      daemon_name, entry_id, is_dir);
                    } else {
                        resp.result = -1;
                        serial_printf("[%s] LOOKUP FAILED\n", daemon_name);
                    }
                } else {
                    resp = fsd_make_error_response(req.request_id, -1);
                }
                break;

            case FS_MSG_GET_STATS:
                serial_printf("[%s] GET_STATS\n", daemon_name);

                /* Call filesystem get_stats function */
                if (fs_ops && fs_ops->get_stats) {
                    uint64_t total = 0, free = 0;
                    bool success = fs_ops->get_stats(NULL, &total, &free);
                    if (success) {
                        resp.result = 0;
                        resp.param1 = total;
                        resp.param2 = free;
                        serial_printf("[%s] STATS OK (total=%llu, free=%llu)\n",
                                      daemon_name, total, free);
                    } else {
                        resp.result = -1;
                    }
                } else {
                    resp = fsd_make_error_response(req.request_id, -1);
                }
                break;

            case FS_MSG_UNMOUNT:
                serial_printf("[%s] UNMOUNT\n", daemon_name);
                resp = fsd_make_success_response(req.request_id);
                break;

            case FS_MSG_SHUTDOWN:
                serial_printf("[%s] SHUTDOWN requested\n", daemon_name);
                resp = fsd_make_success_response(req.request_id);
                running = false;
                break;

            default:
                serial_printf("[%s] ERROR: Unknown request type %d\n",
                              daemon_name, req.type);
                resp = fsd_make_error_response(req.request_id, -1);
                break;
        }

        /* Send response back to kernel */
        if (!fsd_send_response(resp_port, &resp)) {
            serial_printf("[%s] ERROR: Failed to send response\n", daemon_name);
        }
    }

    serial_printf("[%s] Event loop terminated\n", daemon_name);
}
