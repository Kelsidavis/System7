#include <signal.h>

/* Stack protection stubs - provided by GCC runtime */
void __stack_chk_fail(void);
void __stack_chk_fail_local(void);

int raise(int signum) {
    /* Send signal to current process
     * In kernel environment without process/signal infrastructure,
     * handle critical signals directly */

    switch (signum) {
        case SIGABRT:
            /* Abnormal termination - halt system */
            extern void serial_puts(const char* s);
            serial_puts("ABORT: Process raised SIGABRT\n");
            while (1) {}  /* Halt */
            break;

        case SIGFPE:
            /* Floating point exception */
            extern void serial_puts(const char* s);
            serial_puts("ERROR: Floating point exception (SIGFPE)\n");
            while (1) {}  /* Halt */
            break;

        case SIGSEGV:
            /* Segmentation fault */
            extern void serial_puts(const char* s);
            serial_puts("ERROR: Segmentation fault (SIGSEGV)\n");
            while (1) {}  /* Halt */
            break;

        case SIGILL:
            /* Illegal instruction */
            extern void serial_puts(const char* s);
            serial_puts("ERROR: Illegal instruction (SIGILL)\n");
            while (1) {}  /* Halt */
            break;

        default:
            /* Other signals ignored in kernel environment */
            return 0;
    }

    return 0;  /* Never reached for fatal signals */
}

/* errno stub for static linking */
int errno = 0;

/* Stack protection stubs */
void __stack_chk_fail(void) {
    /* Stack check failed - hang */
    while (1) {}
}

void __stack_chk_fail_local(void) {
    __stack_chk_fail();
}
