/* System 7X Nanokernel - Inter-Process Communication Implementation
 *
 * Simple message queue for kernel-daemon communication.
 */

#include "../../include/Nanokernel/ipc.h"
#include "../../include/System71StdLib.h"
#include "../../include/ProcessMgr/ProcessTypes.h"
#include <string.h>
#include <stdlib.h>

#define IPC_MAX_QUEUES 32

static struct {
    MessageQueue queues[IPC_MAX_QUEUES];
    int queue_count;
    bool initialized;
} g_ipc_system = { 0 };

/* Initialize IPC subsystem */
void IPC_Initialize(void) {
    if (g_ipc_system.initialized) {
        serial_printf("[IPC] Already initialized\n");
        return;
    }

    memset(&g_ipc_system, 0, sizeof(g_ipc_system));
    g_ipc_system.initialized = true;

    serial_printf("[IPC] Message queue system initialized\n");
}

/* Create a new message queue */
MessagePort IPC_CreateQueue(const char* name) {
    if (!g_ipc_system.initialized) {
        serial_printf("[IPC] ERROR: System not initialized\n");
        return NULL;
    }

    if (g_ipc_system.queue_count >= IPC_MAX_QUEUES) {
        serial_printf("[IPC] ERROR: Too many queues (max %d)\n", IPC_MAX_QUEUES);
        return NULL;
    }

    /* Find free queue slot */
    MessageQueue* queue = NULL;
    for (int i = 0; i < IPC_MAX_QUEUES; i++) {
        if (!g_ipc_system.queues[i].initialized) {
            queue = &g_ipc_system.queues[i];
            break;
        }
    }

    if (!queue) {
        serial_printf("[IPC] ERROR: No free queue slots\n");
        return NULL;
    }

    /* Initialize queue */
    memset(queue, 0, sizeof(MessageQueue));
    strncpy(queue->name, name, sizeof(queue->name) - 1);
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    queue->initialized = true;

    g_ipc_system.queue_count++;

    serial_printf("[IPC] Created queue: %s\n", name);
    return queue;
}

/* Destroy a message queue */
void IPC_DestroyQueue(MessagePort port) {
    if (!port || !port->initialized) {
        return;
    }

    serial_printf("[IPC] Destroying queue: %s\n", port->name);

    port->initialized = false;
    g_ipc_system.queue_count--;
}

/* Send message (blocking if queue full) */
bool IPC_Send(MessagePort port, const void* message, size_t length) {
    if (!port || !port->initialized) {
        serial_printf("[IPC] ERROR: Invalid message port\n");
        return false;
    }

    if (length > IPC_MAX_MESSAGE_SIZE) {
        serial_printf("[IPC] ERROR: Message too large (%zu > %d)\n",
                      length, IPC_MAX_MESSAGE_SIZE);
        return false;
    }

    /* Block while queue is full */
    while (port->count >= IPC_QUEUE_DEPTH) {
        Proc_Yield();  /* Cooperative yield */
    }

    /* Add message to queue */
    memcpy(port->messages[port->tail], message, length);
    port->message_lengths[port->tail] = length;
    port->tail = (port->tail + 1) % IPC_QUEUE_DEPTH;
    port->count++;

    return true;
}

/* Receive message (blocking if queue empty) */
bool IPC_Receive(MessagePort port, void* buffer, size_t max_length, size_t* actual_length) {
    if (!port || !port->initialized) {
        serial_printf("[IPC] ERROR: Invalid message port\n");
        return false;
    }

    /* Block while queue is empty */
    while (port->count == 0) {
        Proc_Yield();  /* Cooperative yield */
    }

    /* Get message from queue */
    size_t msg_len = port->message_lengths[port->head];

    if (msg_len > max_length) {
        serial_printf("[IPC] ERROR: Buffer too small (need %zu, have %zu)\n",
                      msg_len, max_length);
        return false;
    }

    memcpy(buffer, port->messages[port->head], msg_len);
    port->head = (port->head + 1) % IPC_QUEUE_DEPTH;
    port->count--;

    if (actual_length) {
        *actual_length = msg_len;
    }

    return true;
}

/* Non-blocking send */
bool IPC_TrySend(MessagePort port, const void* message, size_t length) {
    if (!port || !port->initialized) {
        return false;
    }

    if (length > IPC_MAX_MESSAGE_SIZE) {
        return false;
    }

    /* Check if queue is full */
    if (port->count >= IPC_QUEUE_DEPTH) {
        return false;
    }

    /* Add message to queue */
    memcpy(port->messages[port->tail], message, length);
    port->message_lengths[port->tail] = length;
    port->tail = (port->tail + 1) % IPC_QUEUE_DEPTH;
    port->count++;

    return true;
}

/* Non-blocking receive */
bool IPC_TryReceive(MessagePort port, void* buffer, size_t max_length, size_t* actual_length) {
    if (!port || !port->initialized) {
        return false;
    }

    /* Check if queue is empty */
    if (port->count == 0) {
        return false;
    }

    /* Get message from queue */
    size_t msg_len = port->message_lengths[port->head];

    if (msg_len > max_length) {
        return false;
    }

    memcpy(buffer, port->messages[port->head], msg_len);
    port->head = (port->head + 1) % IPC_QUEUE_DEPTH;
    port->count--;

    if (actual_length) {
        *actual_length = msg_len;
    }

    return true;
}

/* Get queue message count */
int IPC_GetQueueCount(MessagePort port) {
    if (!port || !port->initialized) {
        return 0;
    }
    return port->count;
}

/* Check if queue is empty */
bool IPC_IsQueueEmpty(MessagePort port) {
    if (!port || !port->initialized) {
        return true;
    }
    return port->count == 0;
}

/* Check if queue is full */
bool IPC_IsQueueFull(MessagePort port) {
    if (!port || !port->initialized) {
        return false;
    }
    return port->count >= IPC_QUEUE_DEPTH;
}
