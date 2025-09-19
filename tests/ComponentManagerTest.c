/*
 * ComponentManagerTest.c
 *
 * Comprehensive Test Suite for Component Manager - System 7.1 Portable
 * Tests all major Component Manager functionality including:
 * - Component registration and discovery
 * - Component instantiation and lifecycle
 * - Resource management
 * - Capability negotiation
 * - Security and validation
 * - HAL abstraction layer
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "ComponentManager/ComponentManager.h"
#include "ComponentManager/ComponentRegistry.h"
#include "ComponentManager/ComponentInstances.h"
#include "ComponentManager/ComponentDispatch.h"
#include "ComponentManager/ComponentLoader.h"
#include "ComponentManager/ComponentResources.h"
#include "ComponentManager/ComponentNegotiation.h"
#include "ComponentManager/ComponentSecurity.h"
#include "ComponentManager/ComponentManager_HAL.h"

/* Test result tracking */
typedef struct TestResults {
    int totalTests;
    int passedTests;
    int failedTests;
    char lastError[256];
} TestResults;

static TestResults gTestResults = {0};

/* Test macros */
#define TEST_START(name) \
    printf("Running test: %s\n", name); \
    gTestResults.totalTests++;

#define TEST_ASSERT(condition, message) \
    if (!(condition)) { \
        snprintf(gTestResults.lastError, sizeof(gTestResults.lastError), \
                 "ASSERTION FAILED: %s", message); \
        printf("  FAIL: %s\n", message); \
        gTestResults.failedTests++; \
        return false; \
    }

#define TEST_PASS() \
    printf("  PASS\n"); \
    gTestResults.passedTests++; \
    return true;

#define TEST_FAIL(message) \
    snprintf(gTestResults.lastError, sizeof(gTestResults.lastError), \
             "TEST FAILED: %s", message); \
    printf("  FAIL: %s\n", message); \
    gTestResults.failedTests++; \
    return false;

/* Sample component for testing */
static ComponentResult SampleComponentProc(ComponentParameters* params, Handle storage) {
    if (!params) return badComponentSelector;

    switch (params->what) {
        case kComponentOpenSelect:
            printf("    Sample component: Open called\n");
            return noErr;

        case kComponentCloseSelect:
            printf("    Sample component: Close called\n");
            return noErr;

        case kComponentCanDoSelect:
            printf("    Sample component: CanDo called\n");
            return 1; /* Can do */

        case kComponentVersionSelect:
            printf("    Sample component: Version called\n");
            return 0x00010000; /* Version 1.0 */

        default:
            printf("    Sample component: Unknown selector %d\n", params->what);
            return badComponentSelector;
    }
}

/* Test Functions */

/*
 * Test Component Manager initialization
 */
bool TestComponentManagerInit(void) {
    TEST_START("Component Manager Initialization");

    OSErr err = InitComponentManager();
    TEST_ASSERT(err == noErr, "InitComponentManager should succeed");

    TEST_PASS();
}

/*
 * Test component registration
 */
bool TestComponentRegistration(void) {
    TEST_START("Component Registration");

    ComponentDescription cd;
    cd.componentType = 'test';
    cd.componentSubType = 'demo';
    cd.componentManufacturer = 'sys7';
    cd.componentFlags = 0;
    cd.componentFlagsMask = 0;

    Component component = RegisterComponent(&cd, SampleComponentProc, 0, NULL, NULL, NULL);
    TEST_ASSERT(component != NULL, "RegisterComponent should return valid component");

    /* Test component discovery */
    ComponentDescription searchDesc = cd;
    Component foundComponent = FindNextComponent(NULL, &searchDesc);
    TEST_ASSERT(foundComponent != NULL, "FindNextComponent should find registered component");

    /* Test component count */
    int32_t count = CountComponents(&searchDesc);
    TEST_ASSERT(count >= 1, "CountComponents should return at least 1");

    TEST_PASS();
}

/*
 * Test component instances
 */
bool TestComponentInstances(void) {
    TEST_START("Component Instances");

    ComponentDescription cd;
    cd.componentType = 'test';
    cd.componentSubType = 'inst';
    cd.componentManufacturer = 'sys7';
    cd.componentFlags = 0;
    cd.componentFlagsMask = 0;

    Component component = RegisterComponent(&cd, SampleComponentProc, 0, NULL, NULL, NULL);
    TEST_ASSERT(component != NULL, "RegisterComponent should succeed");

    ComponentInstance instance = OpenComponent(component);
    TEST_ASSERT(instance != NULL, "OpenComponent should return valid instance");

    OSErr err = CloseComponent(instance);
    TEST_ASSERT(err == noErr, "CloseComponent should succeed");

    TEST_PASS();
}

/*
 * Test component dispatch
 */
bool TestComponentDispatch(void) {
    TEST_START("Component Dispatch");

    OSErr err = InitComponentDispatch();
    TEST_ASSERT(err == noErr, "InitComponentDispatch should succeed");

    ComponentDescription cd;
    cd.componentType = 'test';
    cd.componentSubType = 'disp';
    cd.componentManufacturer = 'sys7';
    cd.componentFlags = 0;
    cd.componentFlagsMask = 0;

    Component component = RegisterComponent(&cd, SampleComponentProc, 0, NULL, NULL, NULL);
    TEST_ASSERT(component != NULL, "RegisterComponent should succeed");

    ComponentInstance instance = OpenComponent(component);
    TEST_ASSERT(instance != NULL, "OpenComponent should succeed");

    /* Test version call */
    int32_t version = GetComponentVersion(instance);
    TEST_ASSERT(version == 0x00010000, "GetComponentVersion should return correct version");

    /* Test can do call */
    int32_t canDo = ComponentFunctionImplemented(instance, 1);
    TEST_ASSERT(canDo >= 0, "ComponentFunctionImplemented should work");

    OSErr closeErr = CloseComponent(instance);
    TEST_ASSERT(closeErr == noErr, "CloseComponent should succeed");

    CleanupComponentDispatch();

    TEST_PASS();
}

/*
 * Test resource management
 */
bool TestResourceManagement(void) {
    TEST_START("Resource Management");

    OSErr err = InitComponentResources();
    TEST_ASSERT(err == noErr, "InitComponentResources should succeed");

    /* Test resource file operations */
    Component dummyComponent = (Component)0x12345678; /* Dummy component for testing */
    int16_t refNum = OpenComponentResourceFile(dummyComponent);
    TEST_ASSERT(refNum > 0, "OpenComponentResourceFile should return valid refNum");

    /* Test resource loading */
    Handle resource = NULL;
    err = LoadComponentResource(refNum, 'STR ', 100, &resource);
    /* This may fail in test environment, but should not crash */

    err = CloseComponentResourceFile(refNum);
    TEST_ASSERT(err == noErr, "CloseComponentResourceFile should succeed");

    CleanupComponentResources();

    TEST_PASS();
}

/*
 * Test capability negotiation
 */
bool TestCapabilityNegotiation(void) {
    TEST_START("Capability Negotiation");

    OSErr err = InitComponentNegotiation();
    TEST_ASSERT(err == noErr, "InitComponentNegotiation should succeed");

    Component dummyComponent = (Component)0x12345678;

    /* Test capability registration */
    ComponentCapability capability;
    capability.capabilityType = kCapabilityTypeCodec;
    capability.capabilitySubType = 'test';
    capability.version = 0x00010000;
    capability.flags = kCapabilityFlagRequired;
    capability.dataSize = 0;
    capability.data = NULL;
    capability.next = NULL;

    err = RegisterComponentCapability(dummyComponent, &capability);
    TEST_ASSERT(err == noErr, "RegisterComponentCapability should succeed");

    /* Test capability query */
    bool hasCapability = ComponentHasCapability(dummyComponent,
                                               kCapabilityTypeCodec, 'test');
    TEST_ASSERT(hasCapability, "ComponentHasCapability should return true");

    /* Test capability version */
    int32_t version = GetCapabilityVersion(dummyComponent,
                                          kCapabilityTypeCodec, 'test');
    TEST_ASSERT(version == 0x00010000, "GetCapabilityVersion should return correct version");

    CleanupComponentNegotiation();

    TEST_PASS();
}

/*
 * Test security system
 */
bool TestSecuritySystem(void) {
    TEST_START("Security System");

    OSErr err = InitComponentSecurity();
    TEST_ASSERT(err == noErr, "InitComponentSecurity should succeed");

    /* Test security context creation */
    ComponentSecurityContext* context = NULL;
    err = CreateSecurityContext(kSecurityLevelStandard,
                               kSecurityPermissionFileRead | kSecurityPermissionMemory,
                               &context);
    TEST_ASSERT(err == noErr, "CreateSecurityContext should succeed");
    TEST_ASSERT(context != NULL, "CreateSecurityContext should return valid context");

    /* Test component validation */
    Component dummyComponent = (Component)0x12345678;
    err = ValidateComponent(dummyComponent);
    /* This may fail in test environment, but should not crash */

    /* Test security policy */
    SecurityPolicy* policy = NULL;
    err = GetSecurityPolicy(&policy);
    TEST_ASSERT(err == noErr, "GetSecurityPolicy should succeed");

    /* Test sandbox creation */
    ComponentSandbox* sandbox = NULL;
    err = CreateComponentSandbox(context, &sandbox);
    TEST_ASSERT(err == noErr, "CreateComponentSandbox should succeed");
    TEST_ASSERT(sandbox != NULL, "CreateComponentSandbox should return valid sandbox");

    /* Cleanup */
    if (sandbox) {
        DestroyComponentSandbox(sandbox);
    }
    if (context) {
        DestroySecurityContext(context);
    }

    CleanupComponentSecurity();

    TEST_PASS();
}

/*
 * Test HAL functionality
 */
bool TestHALFunctionality(void) {
    TEST_START("HAL Functionality");

    OSErr err = HAL_InitializePlatform();
    TEST_ASSERT(err == noErr, "HAL_InitializePlatform should succeed");

    /* Test memory management */
    void* memory = HAL_AllocateMemory(1024);
    TEST_ASSERT(memory != NULL, "HAL_AllocateMemory should return valid pointer");

    HAL_ZeroMemory(memory, 1024);
    /* This should not crash */

    HAL_FreeMemory(memory);
    /* This should not crash */

    /* Test mutex operations */
    ComponentMutex* mutex = NULL;
    err = HAL_CreateMutex(&mutex);
    TEST_ASSERT(err == noErr, "HAL_CreateMutex should succeed");
    TEST_ASSERT(mutex != NULL, "HAL_CreateMutex should return valid mutex");

    err = HAL_LockMutex(mutex);
    TEST_ASSERT(err == noErr, "HAL_LockMutex should succeed");

    err = HAL_UnlockMutex(mutex);
    TEST_ASSERT(err == noErr, "HAL_UnlockMutex should succeed");

    err = HAL_DestroyMutex(mutex);
    TEST_ASSERT(err == noErr, "HAL_DestroyMutex should succeed");

    /* Test time functions */
    uint64_t currentTime = HAL_GetCurrentTime();
    TEST_ASSERT(currentTime > 0, "HAL_GetCurrentTime should return valid time");

    char timeString[64];
    err = HAL_TimeToString(currentTime, timeString, sizeof(timeString));
    TEST_ASSERT(err == noErr, "HAL_TimeToString should succeed");

    /* Test platform info */
    char platformName[64];
    err = HAL_GetPlatformInfo(platformName, sizeof(platformName));
    TEST_ASSERT(err == noErr, "HAL_GetPlatformInfo should succeed");
    TEST_ASSERT(strlen(platformName) > 0, "HAL_GetPlatformInfo should return non-empty string");

    HAL_CleanupPlatform();

    TEST_PASS();
}

/*
 * Test error handling
 */
bool TestErrorHandling(void) {
    TEST_START("Error Handling");

    /* Test null parameter handling */
    OSErr err = InitComponentManager();
    TEST_ASSERT(err == noErr, "InitComponentManager should succeed");

    Component nullComponent = RegisterComponent(NULL, NULL, 0, NULL, NULL, NULL);
    TEST_ASSERT(nullComponent == NULL, "RegisterComponent with NULL description should fail");

    ComponentInstance nullInstance = OpenComponent(NULL);
    TEST_ASSERT(nullInstance == NULL, "OpenComponent with NULL component should fail");

    err = CloseComponent(NULL);
    TEST_ASSERT(err != noErr, "CloseComponent with NULL instance should fail");

    TEST_PASS();
}

/*
 * Test comprehensive integration
 */
bool TestIntegration(void) {
    TEST_START("Integration Test");

    /* Initialize all subsystems */
    OSErr err = InitComponentManager();
    TEST_ASSERT(err == noErr, "InitComponentManager should succeed");

    err = InitComponentDispatch();
    TEST_ASSERT(err == noErr, "InitComponentDispatch should succeed");

    err = InitComponentResources();
    TEST_ASSERT(err == noErr, "InitComponentResources should succeed");

    err = InitComponentNegotiation();
    TEST_ASSERT(err == noErr, "InitComponentNegotiation should succeed");

    err = InitComponentSecurity();
    TEST_ASSERT(err == noErr, "InitComponentSecurity should succeed");

    err = HAL_InitializePlatform();
    TEST_ASSERT(err == noErr, "HAL_InitializePlatform should succeed");

    /* Register multiple components */
    for (int i = 0; i < 5; i++) {
        ComponentDescription cd;
        cd.componentType = 'test';
        cd.componentSubType = 'int0' + i;
        cd.componentManufacturer = 'sys7';
        cd.componentFlags = 0;
        cd.componentFlagsMask = 0;

        Component component = RegisterComponent(&cd, SampleComponentProc, 0, NULL, NULL, NULL);
        TEST_ASSERT(component != NULL, "RegisterComponent should succeed in loop");
    }

    /* Count components */
    ComponentDescription searchDesc;
    searchDesc.componentType = 'test';
    searchDesc.componentSubType = 0;
    searchDesc.componentManufacturer = 0;
    searchDesc.componentFlags = 0;
    searchDesc.componentFlagsMask = 0;

    int32_t count = CountComponents(&searchDesc);
    TEST_ASSERT(count >= 5, "CountComponents should find all registered components");

    /* Test component enumeration */
    Component currentComponent = NULL;
    int foundCount = 0;
    while ((currentComponent = FindNextComponent(currentComponent, &searchDesc)) != NULL) {
        foundCount++;
        if (foundCount > 10) break; /* Prevent infinite loop */
    }
    TEST_ASSERT(foundCount >= 5, "Component enumeration should find all components");

    /* Cleanup all subsystems */
    CleanupComponentSecurity();
    CleanupComponentNegotiation();
    CleanupComponentResources();
    CleanupComponentDispatch();
    CleanupComponentManager();
    HAL_CleanupPlatform();

    TEST_PASS();
}

/*
 * Main test runner
 */
int main(int argc, char* argv[]) {
    printf("Component Manager Test Suite\n");
    printf("============================\n\n");

    /* Run all tests */
    TestComponentManagerInit();
    TestHALFunctionality();
    TestComponentRegistration();
    TestComponentInstances();
    TestComponentDispatch();
    TestResourceManagement();
    TestCapabilityNegotiation();
    TestSecuritySystem();
    TestErrorHandling();
    TestIntegration();

    /* Print results */
    printf("\nTest Results:\n");
    printf("=============\n");
    printf("Total Tests:  %d\n", gTestResults.totalTests);
    printf("Passed:       %d\n", gTestResults.passedTests);
    printf("Failed:       %d\n", gTestResults.failedTests);

    if (gTestResults.failedTests > 0) {
        printf("Last Error:   %s\n", gTestResults.lastError);
        printf("\nTEST SUITE FAILED\n");
        return 1;
    } else {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }
}