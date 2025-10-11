/* System 7X Nanokernel - Inter-Process Communication (IPC)
 *
 * Simple message queue system for kernel-daemon communication.
 * Provides blocking send/receive primitives for filesystem daemons.
 */

#ifndef NANOKERNEL_IPC_H
#define NANOKERNEL_IPC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define IPC_MAX_MESSAGE_SIZE 8192
#define IPC_QUEUE_DEPTH 16

/* Message queue structure */
typedef struct MessageQueue {
    uint8_t messages[IPC_QUEUE_DEPTH][IPC_MAX_MESSAGE_SIZE];
    size_t  message_lengths[IPC_QUEUE_DEPTH];
    int     head;
    int     tail;
    int     count;
    bool    initialized;
    char    name[32];
} MessageQueue;

/* MessagePort handle */
typedef MessageQueue* MessagePort;

/* IPC Initialization */
void IPC_Initialize(void);

/* Message Queue Operations */
MessagePort IPC_CreateQueue(const char* name);
void IPC_DestroyQueue(MessagePort port);

/* Send message (blocking if queue full) */
bool IPC_Send(MessagePort port, const void* message, size_t length);

/* Receive message (blocking if queue empty) */
bool IPC_Receive(MessagePort port, void* buffer, size_t max_length, size_t* actual_length);

/* Non-blocking operations */
bool IPC_TrySend(MessagePort port, const void* message, size_t length);
bool IPC_TryReceive(MessagePort port, void* buffer, size_t max_length, size_t* actual_length);

/* Queue status */
int IPC_GetQueueCount(MessagePort port);
bool IPC_IsQueueEmpty(MessagePort port);
bool IPC_IsQueueFull(MessagePort port);

#endif /* NANOKERNEL_IPC_H */
