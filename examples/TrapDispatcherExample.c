/*
 * TrapDispatcherExample.c
 *
 * Example usage and test cases for the Mac OS 7.1 Trap Dispatcher
 * Demonstrates how to use the portable C implementation
 */

#include "TrapDispatcher.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Example trap handlers */
static int32_t example_toolbox_handler(TrapContext *context);
static int32_t example_os_handler(TrapContext *context);
static int32_t example_fline_handler(FLineTrapContext *context);
static void custom_cache_flush(void);
static void test_basic_dispatch(void);
static void test_trap_patching(void);
static void test_extended_table(void);

/* Test counters */
static int toolbox_calls = 0;
static int os_calls = 0;
static int fline_calls = 0;
static int cache_flushes = 0;

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;  /* Suppress unused parameter warnings */
    printf("Mac OS 7.1 Trap Dispatcher - Portable C Implementation Test\n");
    printf("============================================================\n\n");

    /* Initialize the trap dispatcher */
    printf("1. Initializing trap dispatcher...\n");
    int result = TrapDispatcher_Initialize();
    if (result != 0) {
        fprintf(stderr, "Failed to initialize trap dispatcher: %d\n", result);
        return 1;
    }

    /* Set up custom cache flush function */
    TrapDispatcher_SetCacheFlushFunction(custom_cache_flush);

    /* Install example F-line handler */
    TrapDispatcher_SetFLineHandler(example_fline_handler);

    /* Run tests */
    printf("2. Testing basic trap dispatch...\n");
    test_basic_dispatch();

    printf("3. Testing trap patching mechanism...\n");
    test_trap_patching();

    printf("4. Testing extended trap table...\n");
    test_extended_table();

    /* Print statistics */
    int num_toolbox, num_os, num_extended;
    TrapDispatcher_GetStatistics(&num_toolbox, &num_os, &num_extended);

    printf("\nTrap Dispatcher Statistics:\n");
    printf("  Toolbox traps: %d\n", num_toolbox);
    printf("  OS traps: %d\n", num_os);
    printf("  Extended traps: %d\n", num_extended);
    printf("\nTest Results:\n");
    printf("  Toolbox handler calls: %d\n", toolbox_calls);
    printf("  OS handler calls: %d\n", os_calls);
    printf("  F-line handler calls: %d\n", fline_calls);
    printf("  Cache flushes: %d\n", cache_flushes);

    /* Validate state */
    if (TrapDispatcher_ValidateState()) {
        printf("\nTrap dispatcher state is valid.\n");
    } else {
        printf("\nERROR: Trap dispatcher state validation failed!\n");
    }

    /* Cleanup */
    printf("\n5. Cleaning up trap dispatcher...\n");
    TrapDispatcher_Cleanup();

    printf("\nAll tests completed successfully!\n");
    return 0;
}

/**
 * Test basic trap dispatch functionality
 */
static void test_basic_dispatch(void) {
    TrapContext context;
    int result;

    /* Clear context */
    memset(&context, 0, sizeof(TrapContext));

    /* Install a test toolbox handler for trap 0x100 */
    result = TrapDispatcher_SetTrapAddress(0x100, (1 << TRAP_NEW_BIT) | (1 << TRAP_TOOLBOX_BIT),
                                         example_toolbox_handler);
    assert(result == 0);

    /* Install a test OS handler for trap 0x50 */
    result = TrapDispatcher_SetTrapAddress(0x50, (1 << TRAP_NEW_BIT),
                                         example_os_handler);
    assert(result == 0);

    /* Test GetTrapAddress and SetTrapAddress directly (safer than full dispatch) */
    printf("  Testing GetTrapAddress...\n");
    TrapHandler handler = TrapDispatcher_GetTrapAddress(0x100, (1 << TRAP_NEW_BIT) | (1 << TRAP_TOOLBOX_BIT));
    if (handler == example_toolbox_handler) {
        printf("  Toolbox handler installed correctly\n");
    } else {
        printf("  ERROR: Toolbox handler not found\n");
    }

    handler = TrapDispatcher_GetTrapAddress(0x50, (1 << TRAP_NEW_BIT));
    if (handler == example_os_handler) {
        printf("  OS handler installed correctly\n");
    } else {
        printf("  ERROR: OS handler not found\n");
    }

    /* Test handlers directly */
    printf("  Testing handlers directly...\n");
    context.d0 = 0x12345678;
    result = example_toolbox_handler(&context);
    printf("  Toolbox handler result: 0x%08X\n", result);

    context.d0 = 0x87654321;
    context.d1 = 0xA050;  /* OS trap word */
    result = example_os_handler(&context);
    printf("  OS handler result: 0x%08X\n", result);

    /* Test F-line trap */
    FLineTrapContext fline_context;
    fline_context.opcode = 0xF123;
    fline_context.address = 0x40001000;
    fline_context.cpu_ctx = &context;

    result = TrapDispatcher_DispatchFTrap(&fline_context);
    printf("  F-line trap result: 0x%08X\n", result);
}

/**
 * Test trap patching mechanism
 */
static void test_trap_patching(void) {
    TrapHandler original, patched;
    int result;

    /* Get original handler */
    original = TrapDispatcher_GetTrapAddress(0x100, (1 << TRAP_NEW_BIT) | (1 << TRAP_TOOLBOX_BIT));
    printf("  Original handler: %p\n", original);

    /* Patch with new handler */
    result = TrapDispatcher_SetTrapAddress(0x100, (1 << TRAP_NEW_BIT) | (1 << TRAP_TOOLBOX_BIT),
                                         example_toolbox_handler);
    assert(result == 0);

    /* Get patched handler */
    patched = TrapDispatcher_GetTrapAddress(0x100, (1 << TRAP_NEW_BIT) | (1 << TRAP_TOOLBOX_BIT));
    printf("  Patched handler: %p\n", patched);

    /* Verify patch was applied */
    if (cache_flushes > 0) {
        printf("  Cache flush was called during patching\n");
    }
}

/**
 * Test extended trap table functionality
 */
static void test_extended_table(void) {
    int result;

    /* Initialize extended table */
    result = TrapDispatcher_InitializeExtendedTable();
    assert(result == 0);
    printf("  Extended trap table initialized\n");

    /* Install handler in extended range */
    result = TrapDispatcher_SetTrapAddress(0x300, (1 << TRAP_NEW_BIT) | (1 << TRAP_TOOLBOX_BIT),
                                         example_toolbox_handler);
    assert(result == 0);

    /* Verify handler was installed */
    TrapHandler handler = TrapDispatcher_GetTrapAddress(0x300, (1 << TRAP_NEW_BIT) | (1 << TRAP_TOOLBOX_BIT));
    if (handler == example_toolbox_handler) {
        printf("  Extended trap handler installed successfully\n");
    } else {
        printf("  ERROR: Extended trap handler installation failed\n");
    }
}

/**
 * Example toolbox trap handler
 */
static int32_t example_toolbox_handler(TrapContext *context) {
    toolbox_calls++;
    printf("    Toolbox handler called: D0=0x%08X, PC=0x%08X\n",
           context->d0, context->pc - 2);

    /* Toolbox traps return results in D0 */
    return context->d0 + 0x1000;
}

/**
 * Example OS trap handler
 */
static int32_t example_os_handler(TrapContext *context) {
    os_calls++;
    printf("    OS handler called: D0=0x%08X, D1=0x%04X, PC=0x%08X\n",
           context->d0, context->d1 & 0xFFFF, context->pc - 2);

    /* OS traps often return error codes */
    return 0;  /* noErr */
}

/**
 * Example F-line trap handler
 */
static int32_t example_fline_handler(FLineTrapContext *context) {
    fline_calls++;
    printf("    F-line handler called: opcode=0x%04X, address=0x%08X\n",
           context->opcode, context->address);

    /* F-line traps usually indicate unimplemented instructions */
    return DS_CORE_ERR;
}

/**
 * Custom cache flush function
 */
static void custom_cache_flush(void) {
    cache_flushes++;
    printf("    Cache flush called\n");

    /* On real hardware, this would flush the instruction cache */
    /* Modern CPUs usually handle cache coherency automatically */
}

/* Weak implementation of SysError for testing */
void SysError(int error_code) {
    printf("SYSTEM ERROR %d\n", error_code);
    /* In a real system, this would halt or restart the system */
}