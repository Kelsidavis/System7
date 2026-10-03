/* Freestanding ARM runtime compatibility for Linux-targeting GCC toolchains. */

/*
 * Linux-targeting libgcc implements __aeabi_ldiv0 by calling raise(SIGFPE).
 * A bare-metal kernel has no signal runtime; reaching this path means integer
 * division by zero, so stop instead of leaving libgcc's Linux dependency
 * unresolved or silently returning a fabricated result.
 */
int raise(int signal_number);

int raise(int signal_number) {
    (void)signal_number;
    __builtin_trap();
}
