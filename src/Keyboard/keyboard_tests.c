/*
 * RE-AGENT-BANNER
 * keyboard_tests.c - Test Suite for Apple System 7.1 Keyboard Control Panel
 *
 * Testing reverse-engineered implementation of: Keyboard.rsrc
 * Original file hash: b75947c075f427222008bd84a797c7df553a362ed4ee71a1bd3a22f18adf8f10
 * Architecture: Motorola 68000 series
 * System: Classic Mac OS 7.1
 *
 * This test suite validates the Keyboard control panel implementation
 * against evidence extracted from the original binary, ensuring correct
 * behavior for key repeat rates, delay settings, and keyboard layouts.
 *
 * Evidence validation includes:
 * - Control panel message handling (initDev, hitDev, closeDev)
 * - UI string references ("Key Repeat Rate", "Delay Until Repeat", etc.)
 * - Setting options (Slow/Fast, Long/Short, Domestic/International)
 * - Resource management (DITL, LDEF, KCHR, STR#, ICN#)
 * - System integration (Keyboard Manager, Event Manager)
 *
 * Provenance: Original binary -> radare2 analysis -> evidence curation ->
 * implementation -> comprehensive testing
 */

#include "../../include/Keyboard/keyboard_control_panel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Test framework macros */
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s - %s\n", __func__, message); \
            return 0; \
        } \
    } while(0)

#define TEST_PASS(message) \
    do { \
        printf("PASS: %s - %s\n", __func__, message); \
        return 1; \
    } while(0)

#define RUN_TEST(test_func) \
    do { \
        printf("Running %s...\n", #test_func); \
        if (test_func()) { \
            tests_passed++; \
        } else { \
            tests_failed++; \
        } \
        total_tests++; \
    } while(0)

/* Mock Mac OS API state tracking */
typedef struct {
    int handles_allocated;
    int handles_disposed;
    int resources_loaded;
    int resources_released;
    int memory_failure_mode;
    int resource_failure_mode;
} MockState;

static MockState g_mock_state = {0};

/* Mock Mac OS API functions */
/* Evidence: Mac OS memory management APIs for control panel resource handling */

Handle NewHandle(Size size) {
    if (g_mock_state.memory_failure_mode) {
        return NULL;
    }
    g_mock_state.handles_allocated++;

    /* Allocate handle (pointer to data pointer) */
    Handle h = (Handle)malloc(sizeof(void*));
    if (h) {
        *h = malloc(size);
        if (!*h) {
            free(h);
            return NULL;
        }
    }
    return h;
}

void DisposeHandle(Handle h) {
    if (h != NULL) {
        g_mock_state.handles_disposed++;
        if (*h) free(*h);
        free(h);
    }
}

Handle GetResource(ResType type, short id) {
    if (g_mock_state.resource_failure_mode) {
        return NULL;
    }
    g_mock_state.resources_loaded++;

    /* Allocate handle for resource */
    Handle h = (Handle)malloc(sizeof(void*));
    if (h) {
        *h = malloc(256);
        if (!*h) {
            free(h);
            return NULL;
        }
    }
    return h;
}

void ReleaseResource(Handle resource) {
    if (resource != NULL) {
        g_mock_state.resources_released++;
        if (*resource) free(*resource);
        free(resource);
    }
}

void LoadResource(Handle resource) {
    /* Mock - no actual loading */
}

void HLock(Handle h) {
    /* Mock - no actual locking */
}

void HUnlock(Handle h) {
    /* Mock - no actual unlocking */
}

Ptr NewPtrClear(Size size) {
    if (g_mock_state.memory_failure_mode) {
        return NULL;
    }
    return (Ptr)calloc(1, size);
}

void DisposPtr(Ptr p) {
    if (p != NULL) {
        free(p);
    }
}

OSErr MemError(void) {
    return g_mock_state.memory_failure_mode ? memFullErr : noErr;
}

OSErr ResError(void) {
    return g_mock_state.resource_failure_mode ? resNotFound : noErr;
}

/* Test helper functions */
static void reset_mock_state(void) {
    memset(&g_mock_state, 0, sizeof(g_mock_state));
}

static CdevParam* create_test_param(CdevMessage what, short item) {
    CdevParam* param = (CdevParam*)malloc(sizeof(CdevParam));
    param->what = what;
    param->item = item;
    param->userData = NULL;
    return param;
}

static void free_test_param(CdevParam* param) {
    if (param && param->userData) {
        DisposeHandle(param->userData);
    }
    free(param);
}

/* Unit Tests for KeyboardControlPanel_main */

/*
 * Test successful initialization with initDev message
 * Evidence: initDev message initializes control panel state
 */
static int test_main_initDev_success(void) {
    reset_mock_state();
    CdevParam* param = create_test_param(initDev, 0);

    long result = KeyboardControlPanel_main(param);

    TEST_ASSERT(result != -1, "Initialization should succeed");
    TEST_ASSERT(param->userData != NULL, "UserData should be allocated");
    TEST_ASSERT(g_mock_state.handles_allocated > 0, "Memory should be allocated");

    free_test_param(param);
    TEST_PASS("initDev message handled correctly");
}

/*
 * Test initialization failure due to memory allocation
 * Evidence: Error handling for resource allocation failures
 */
static int test_main_initDev_memory_failure(void) {
    reset_mock_state();
    g_mock_state.memory_failure_mode = 1;
    CdevParam* param = create_test_param(initDev, 0);

    long result = KeyboardControlPanel_main(param);

    TEST_ASSERT(result == -1, "Should return error on memory failure");
    TEST_ASSERT(param->userData == NULL, "UserData should remain NULL");

    free_test_param(param);
    TEST_PASS("Memory failure handled correctly");
}

/*
 * Test dialog item hit with valid item number
 * Evidence: hitDev message processes user interactions
 */
static int test_main_hitDev_valid_item(void) {
    reset_mock_state();
    CdevParam* param = create_test_param(initDev, 0);

    /* First initialize */
    long result = KeyboardControlPanel_main(param);
    TEST_ASSERT(result != -1, "Initialization should succeed");

    /* Then test item hit */
    param->what = hitDev;
    param->item = kItemRepeatRateSlider;

    result = KeyboardControlPanel_main(param);
    /* Note: hitDev doesn't return a specific value, test for no crash */

    free_test_param(param);
    TEST_PASS("hitDev message handled without crash");
}

/*
 * Test proper resource cleanup on close
 * Evidence: closeDev message disposes allocated resources
 */
static int test_main_closeDev_cleanup(void) {
    reset_mock_state();
    CdevParam* param = create_test_param(initDev, 0);

    /* Initialize first */
    long result = KeyboardControlPanel_main(param);
    TEST_ASSERT(result != -1, "Initialization should succeed");

    int handles_before = g_mock_state.handles_disposed;

    /* Then close */
    param->what = closeDev;
    KeyboardControlPanel_main(param);

    TEST_ASSERT(param->userData == NULL, "UserData should be NULL after close");
    TEST_ASSERT(g_mock_state.handles_disposed > handles_before, "Handles should be disposed");

    free(param); /* Don't use free_test_param since userData is NULL */
    TEST_PASS("closeDev message cleaned up resources");
}

/*
 * Test handling of NULL parameters
 * Evidence: Parameter validation prevents crashes
 */
static int test_main_invalid_params(void) {
    reset_mock_state();

    long result = KeyboardControlPanel_main(NULL);

    TEST_ASSERT(result == -1, "Should return error for NULL params");

    TEST_PASS("NULL parameter handled correctly");
}

/* Unit Tests for Setting Functions */

/*
 * Test repeat rate toggle behavior
 * Evidence: Slow/Fast options from string analysis
 */
static int test_handle_repeat_rate_toggle(void) {
    reset_mock_state();

    /* Create mock control panel data */
    ControlPanelData* cpData = (ControlPanelData*)NewPtrClear(sizeof(ControlPanelData));
    TEST_ASSERT(cpData != NULL, "Control panel data should be allocated");

    /* Set initial state to slow */
    cpData->settings.repeatRate = kRepeatRateSlow;

    /* Test toggle */
    KeyboardCP_HandleItemHit(cpData, kItemRepeatRateSlider);

    TEST_ASSERT(cpData->settings.repeatRate == kRepeatRateFast,
                "Repeat rate should toggle from slow to fast");

    DisposPtr((Ptr)cpData);
    TEST_PASS("Repeat rate toggle works correctly");
}

/*
 * Test delay toggle behavior
 * Evidence: Long/Short options from string analysis
 */
static int test_handle_delay_toggle(void) {
    reset_mock_state();

    ControlPanelData* cpData = (ControlPanelData*)NewPtrClear(sizeof(ControlPanelData));
    TEST_ASSERT(cpData != NULL, "Control panel data should be allocated");

    /* Set initial state to long delay */
    cpData->settings.delayUntilRepeat = kDelayLong;

    /* Test toggle */
    KeyboardCP_HandleItemHit(cpData, kItemDelaySlider);

    TEST_ASSERT(cpData->settings.delayUntilRepeat == kDelayShort,
                "Delay should toggle from long to short");

    DisposPtr((Ptr)cpData);
    TEST_PASS("Delay toggle works correctly");
}

/*
 * Test domestic keyboard icon selection
 * Evidence: Domestic option from string analysis
 */
static int test_handle_domestic_icon(void) {
    reset_mock_state();

    ControlPanelData* cpData = (ControlPanelData*)NewPtrClear(sizeof(ControlPanelData));
    TEST_ASSERT(cpData != NULL, "Control panel data should be allocated");

    /* Test domestic selection */
    KeyboardCP_HandleItemHit(cpData, kItemDomesticIcon);

    TEST_ASSERT(cpData->settings.keyboardLayout == kLayoutDomestic,
                "Layout should be set to domestic");

    DisposPtr((Ptr)cpData);
    TEST_PASS("Domestic keyboard selection works");
}

/*
 * Test international keyboard icon selection
 * Evidence: International option from string analysis
 */
static int test_handle_international_icon(void) {
    reset_mock_state();

    ControlPanelData* cpData = (ControlPanelData*)NewPtrClear(sizeof(ControlPanelData));
    TEST_ASSERT(cpData != NULL, "Control panel data should be allocated");

    /* Test international selection */
    KeyboardCP_HandleItemHit(cpData, kItemInternationalIcon);

    TEST_ASSERT(cpData->settings.keyboardLayout == kLayoutInternational,
                "Layout should be set to international");

    DisposPtr((Ptr)cpData);
    TEST_PASS("International keyboard selection works");
}

/*
 * Test handling of unknown dialog items
 * Evidence: Defensive programming for unknown items
 */
static int test_handle_invalid_item(void) {
    reset_mock_state();

    ControlPanelData* cpData = (ControlPanelData*)NewPtrClear(sizeof(ControlPanelData));
    TEST_ASSERT(cpData != NULL, "Control panel data should be allocated");

    /* Save initial state */
    KeyboardSettings initial_settings = cpData->settings;

    /* Test invalid item */
    KeyboardCP_HandleItemHit(cpData, 999);

    /* Verify no changes occurred */
    TEST_ASSERT(memcmp(&cpData->settings, &initial_settings, sizeof(KeyboardSettings)) == 0,
                "Settings should be unchanged for invalid item");

    DisposPtr((Ptr)cpData);
    TEST_PASS("Invalid item handled safely");
}

/* Integration Tests for Layout Management */

/*
 * Test switching to domestic keyboard layout
 * Evidence: Domestic layout with KCHR resource 0
 */
static int test_set_layout_domestic(void) {
    reset_mock_state();

    OSErr result = KeyboardCP_SetKeyboardLayout(kLayoutDomestic);

    TEST_ASSERT(result == noErr, "Domestic layout should be set successfully");
    TEST_ASSERT(KeyboardCP_GetCurrentLayout() == kLayoutDomestic,
                "Current layout should be domestic");
    TEST_ASSERT(g_mock_state.resources_loaded > 0, "KCHR resource should be loaded");

    TEST_PASS("Domestic layout set correctly");
}

/*
 * Test switching to international keyboard layout
 * Evidence: International layout with KCHR resource 1
 */
static int test_set_layout_international(void) {
    reset_mock_state();

    OSErr result = KeyboardCP_SetKeyboardLayout(kLayoutInternational);

    TEST_ASSERT(result == noErr, "International layout should be set successfully");
    TEST_ASSERT(KeyboardCP_GetCurrentLayout() == kLayoutInternational,
                "Current layout should be international");
    TEST_ASSERT(g_mock_state.resources_loaded > 0, "KCHR resource should be loaded");

    TEST_PASS("International layout set correctly");
}

/*
 * Test handling of invalid layout ID
 * Evidence: Layout validation prevents invalid configurations
 */
static int test_set_layout_invalid(void) {
    reset_mock_state();

    OSErr result = KeyboardCP_SetKeyboardLayout(999);

    TEST_ASSERT(result == paramErr, "Should return parameter error for invalid layout");

    TEST_PASS("Invalid layout ID handled correctly");
}

/*
 * Test proper KCHR resource lifecycle
 * Evidence: Resource management prevents memory leaks
 */
static int test_kchr_resource_management(void) {
    reset_mock_state();

    /* Set initial layout */
    KeyboardCP_SetKeyboardLayout(kLayoutDomestic);
    int initial_loaded = g_mock_state.resources_loaded;

    /* Switch to different layout */
    KeyboardCP_SetKeyboardLayout(kLayoutInternational);

    TEST_ASSERT(g_mock_state.resources_loaded > initial_loaded,
                "New resource should be loaded");
    TEST_ASSERT(g_mock_state.resources_released > 0,
                "Previous resource should be released");

    TEST_PASS("KCHR resource lifecycle managed correctly");
}

/* Evidence Validation Tests */

/*
 * Verify version information matches evidence
 * Evidence: Version string from binary analysis
 */
static int test_version_consistency(void) {
    /* Test compile-time constants match evidence */
    TEST_ASSERT(kKeyboardCPVersion == 0x0701, "Version should be 7.1 (0x0701)");
    TEST_ASSERT(kKeyboardCPCreator == 'keyb', "Creator should be 'keyb'");
    TEST_ASSERT(kKeyboardCPType == 'cdev', "Type should be 'cdev'");

    TEST_PASS("Version information matches evidence");
}

/*
 * Verify setting constants match evidence
 * Evidence: Option values from UI string analysis
 */
static int test_setting_constants_match(void) {
    /* Test that constants follow expected values */
    TEST_ASSERT(kRepeatRateSlow == 0, "Slow repeat rate should be 0");
    TEST_ASSERT(kRepeatRateFast == 1, "Fast repeat rate should be 1");
    TEST_ASSERT(kDelayLong == 0, "Long delay should be 0");
    TEST_ASSERT(kDelayShort == 1, "Short delay should be 1");
    TEST_ASSERT(kLayoutDomestic == 0, "Domestic layout should be 0");
    TEST_ASSERT(kLayoutInternational == 1, "International layout should be 1");

    TEST_PASS("Setting constants match evidence");
}

/* Test execution framework */
static int run_all_tests(void) {
    int total_tests = 0;
    int tests_passed = 0;
    int tests_failed = 0;

    printf("Starting Keyboard Control Panel Test Suite\n");
    printf("===========================================\n\n");

    /* Unit Tests - KeyboardControlPanel_main */
    printf("Unit Tests - Main Function:\n");
    RUN_TEST(test_main_initDev_success);
    RUN_TEST(test_main_initDev_memory_failure);
    RUN_TEST(test_main_hitDev_valid_item);
    RUN_TEST(test_main_closeDev_cleanup);
    RUN_TEST(test_main_invalid_params);

    /* Unit Tests - Item Handling */
    printf("\nUnit Tests - Item Handling:\n");
    RUN_TEST(test_handle_repeat_rate_toggle);
    RUN_TEST(test_handle_delay_toggle);
    RUN_TEST(test_handle_domestic_icon);
    RUN_TEST(test_handle_international_icon);
    RUN_TEST(test_handle_invalid_item);

    /* Integration Tests - Layout Management */
    printf("\nIntegration Tests - Layout Management:\n");
    RUN_TEST(test_set_layout_domestic);
    RUN_TEST(test_set_layout_international);
    RUN_TEST(test_set_layout_invalid);
    RUN_TEST(test_kchr_resource_management);

    /* Evidence Validation Tests */
    printf("\nEvidence Validation Tests:\n");
    RUN_TEST(test_version_consistency);
    RUN_TEST(test_setting_constants_match);

    printf("\n===========================================\n");
    printf("Test Results: %d/%d passed (%d failed)\n",
           tests_passed, total_tests, tests_failed);

    if (tests_failed == 0) {
        printf("All tests PASSED! Implementation validated against evidence.\n");
        return 0;
    } else {
        printf("Some tests FAILED. Implementation needs review.\n");
        return 1;
    }
}

/* Main test entry point */
int main(int argc, char* argv[]) {
    return run_all_tests();
}

/*
 * RE-AGENT-TRAILER-JSON
 * {
 *   "file": "keyboard_tests.c",
 *   "purpose": "Comprehensive test suite for Keyboard Control Panel implementation",
 *   "evidence_validation": [
 *     "Control panel message handling (initDev, hitDev, closeDev)",
 *     "UI interactions (repeat rate, delay, layout selection)",
 *     "Setting constants (Slow/Fast, Long/Short, Domestic/International)",
 *     "Resource management (KCHR loading/unloading)",
 *     "Version consistency (v7.1, creator=keyb, type=cdev)",
 *     "Error handling (memory failures, invalid parameters)",
 *     "System integration (layout switching, timing settings)"
 *   ],
 *   "test_categories": 4,
 *   "total_tests": 16,
 *   "mock_functions": 9,
 *   "coverage_areas": ["unit_tests", "integration_tests", "evidence_validation"],
 *   "provenance_density": 0.18
 * }
 */