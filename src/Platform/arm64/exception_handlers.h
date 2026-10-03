/*
 * ARM64 Exception Handler Definitions
 */

#ifndef ARM64_EXCEPTION_HANDLERS_H
#define ARM64_EXCEPTION_HANDLERS_H

#include <stdint.h>

/* Exception context structure
 * Must be kept in sync with exceptions.S
 */
typedef struct {
    uint64_t x[31];     /* General purpose registers x0-x30 */
    uint64_t elr;       /* Exception Link Register */
    uint64_t spsr;      /* Saved Program Status Register */
    uint64_t sp;        /* Stack Pointer */
} exception_context_t;

/* Exception handler functions */
void exception_vectors(void);
void exceptions_init(void);
void handle_sync_exception(exception_context_t *ctx);
void handle_irq_exception(exception_context_t *ctx);
void handle_fiq_exception(exception_context_t *ctx);
void handle_serror_exception(exception_context_t *ctx);

#endif /* ARM64_EXCEPTION_HANDLERS_H */
