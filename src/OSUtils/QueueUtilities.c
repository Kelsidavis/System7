/*
 * QueueUtilities.c - Queue Management Utilities
 *
 * Implements Mac OS queue management functions for adding and removing
 * elements from linked lists. These are fundamental utilities used by
 * many managers (Device, Time, File, etc.).
 *
 * Based on Inside Macintosh: Operating System Utilities
 */

#include "SystemTypes.h"
#include "System71StdLib.h"

/* Debug logging */
#define QUEUE_DEBUG 0

#if QUEUE_DEBUG
#define QUEUE_LOG(...) do { \
    char buf[256]; \
    snprintf(buf, sizeof(buf), "[Queue] " __VA_ARGS__); \
    serial_puts(buf); \
} while(0)
#else
#define QUEUE_LOG(...)
#endif

/* Forward declarations */
void Enqueue(QElemPtr qElement, QHdr* qHeader);
OSErr Dequeue(QElemPtr qElement, QHdr* qHeader);

/*
 * Enqueue - Add an element to the end of a queue
 *
 * Adds a queue element to the end (tail) of a queue. The queue header
 * maintains pointers to the head and tail of the queue, and each element
 * contains a link pointer to the next element.
 *
 * Parameters:
 *   qElement - Pointer to queue element to add
 *   qHeader - Pointer to queue header
 *
 * Queue structure:
 *   QHdr contains qHead (first element) and qTail (last element)
 *   Each QElem contains qLink (pointer to next element)
 *
 * This function is used by Device Manager, Time Manager, File Manager,
 * and other system components for managing linked lists.
 *
 * Based on Inside Macintosh: Operating System Utilities, Chapter 8
 */
void Enqueue(QElemPtr qElement, QHdr* qHeader) {
    if (!qElement || !qHeader) {
        QUEUE_LOG("Enqueue: NULL pointer\n");
        return;
    }

    /* Clear the new element's link */
    qElement->qLink = NULL;

    /* If queue is empty, this becomes both head and tail */
    if (qHeader->qHead == NULL) {
        qHeader->qHead = qElement;
        qHeader->qTail = qElement;
        QUEUE_LOG("Enqueue: Added first element to queue\n");
    } else {
        /* Add to the end of the queue */
        if (qHeader->qTail) {
            qHeader->qTail->qLink = qElement;
        }
        qHeader->qTail = qElement;
        QUEUE_LOG("Enqueue: Added element to tail\n");
    }
}

/*
 * Dequeue - Remove an element from a queue
 *
 * Removes a specified queue element from anywhere in a queue. The function
 * searches through the queue to find the element and removes it, updating
 * the links appropriately.
 *
 * Parameters:
 *   qElement - Pointer to queue element to remove
 *   qHeader - Pointer to queue header
 *
 * Returns:
 *   noErr (0) if successful
 *   qErr (-1) if element not found in queue
 *
 * Note: Unlike Enqueue which always adds to the tail, Dequeue can remove
 * an element from anywhere in the queue by searching for it.
 *
 * Based on Inside Macintosh: Operating System Utilities, Chapter 8
 */
OSErr Dequeue(QElemPtr qElement, QHdr* qHeader) {
    QElemPtr current;
    QElemPtr previous;

    if (!qElement || !qHeader) {
        QUEUE_LOG("Dequeue: NULL pointer\n");
        return qErr;
    }

    /* Empty queue */
    if (qHeader->qHead == NULL) {
        QUEUE_LOG("Dequeue: Queue is empty\n");
        return qErr;
    }

    /* Check if it's the first element */
    if (qHeader->qHead == qElement) {
        qHeader->qHead = qElement->qLink;

        /* If this was also the last element, update tail */
        if (qHeader->qTail == qElement) {
            qHeader->qTail = NULL;
        }

        qElement->qLink = NULL;
        QUEUE_LOG("Dequeue: Removed head element\n");
        return noErr;
    }

    /* Search for the element in the queue */
    previous = qHeader->qHead;
    current = previous->qLink;

    while (current != NULL) {
        if (current == qElement) {
            /* Found it - remove from chain */
            previous->qLink = current->qLink;

            /* If this was the tail, update tail pointer */
            if (qHeader->qTail == current) {
                qHeader->qTail = previous;
            }

            current->qLink = NULL;
            QUEUE_LOG("Dequeue: Removed element from middle/end\n");
            return noErr;
        }

        previous = current;
        current = current->qLink;
    }

    /* Element not found in queue */
    QUEUE_LOG("Dequeue: Element not found in queue\n");
    return qErr;
}
