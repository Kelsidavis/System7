/*
 * ADBExample.c - Example usage of the portable ADB Manager
 *
 * This example demonstrates how to initialize and use the ADB Manager
 * for keyboard and mouse input in a portable System 7.1 implementation.
 */

#include "ADBManager.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

/* External hardware interface functions */
extern ADBHardwareInterface* ADBManager_CreateHardwareInterface(void);
extern void ADBManager_DestroyHardwareInterface(ADBHardwareInterface* interface);
extern void ADBManager_ProcessHardwareEvents(ADBManager* mgr);
extern void ADBManager_SimulateKeypress(ADBManager* mgr, uint8_t keycode, bool isDown);
extern void ADBManager_SimulateMouseMove(ADBManager* mgr, int16_t deltaX, int16_t deltaY, bool buttonDown);

/* Global state */
static ADBManager* g_adbManager = NULL;
static ADBHardwareInterface* g_hardware = NULL;
static volatile bool g_running = true;

/* Event callback for handling input events */
static void handleInputEvent(int eventType, uint32_t eventData, void* userData) {
    switch (eventType) {
        case KEY_DOWN_EVENT: {
            uint16_t keyCode = eventData & 0xFF;
            uint8_t modifiers = (eventData >> 8) & 0xFF;
            uint16_t asciiCode = (eventData >> 16) & 0xFFFF;
            printf("Key Down: code=0x%02X, mods=0x%02X, ascii=0x%04X\n",
                   keyCode, modifiers, asciiCode);
            break;
        }

        case KEY_UP_EVENT: {
            uint16_t keyCode = eventData & 0xFF;
            uint8_t modifiers = (eventData >> 8) & 0xFF;
            uint16_t asciiCode = (eventData >> 16) & 0xFFFF;
            printf("Key Up: code=0x%02X, mods=0x%02X, ascii=0x%04X\n",
                   keyCode, modifiers, asciiCode);
            break;
        }

        case 1: /* Mouse event */
            {
                uint8_t buttonState = (eventData >> 24) & 0xFF;
                int16_t deltaX = (int8_t)((eventData >> 16) & 0xFF);
                int16_t deltaY = (int8_t)((eventData >> 8) & 0xFF);
                printf("Mouse: dx=%d, dy=%d, buttons=0x%02X\n",
                       deltaX, deltaY, buttonState);
            }
            break;

        default:
            printf("Unknown event type: %d, data=0x%08X\n", eventType, eventData);
            break;
    }
}

/* Signal handler for clean shutdown */
static void signalHandler(int sig) {
    printf("\nShutting down...\n");
    g_running = false;
}

/* Demonstrate ADB Manager initialization and usage */
int main(int argc, char* argv[]) {
    printf("System 7.1 Portable ADB Manager Example\n");
    printf("=======================================\n\n");

    /* Set up signal handler for clean shutdown */
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    /* Create hardware interface */
    printf("Creating hardware interface...\n");
    g_hardware = ADBManager_CreateHardwareInterface();
    if (!g_hardware) {
        fprintf(stderr, "Failed to create hardware interface\n");
        return 1;
    }

    /* Create and initialize ADB Manager */
    g_adbManager = malloc(sizeof(ADBManager));
    if (!g_adbManager) {
        fprintf(stderr, "Failed to allocate ADB Manager\n");
        ADBManager_DestroyHardwareInterface(g_hardware);
        return 1;
    }

    printf("Initializing ADB Manager...\n");
    int result = ADBManager_Initialize(g_adbManager, g_hardware);
    if (result != ADB_NO_ERROR) {
        fprintf(stderr, "Failed to initialize ADB Manager: %s\n",
                ADBManager_GetErrorString(result));
        free(g_adbManager);
        ADBManager_DestroyHardwareInterface(g_hardware);
        return 1;
    }

    /* Set up event callback */
    ADBManager_SetEventCallback(g_adbManager, handleInputEvent, NULL);

    /* Display device information */
    printf("\nADB Device Information:\n");
    printf("----------------------\n");

    int deviceCount = ADBManager_CountDevices(g_adbManager);
    printf("Total devices: %d\n", deviceCount);

    for (int i = 1; i <= deviceCount; i++) {
        ADBDeviceEntry info;
        int address = ADBManager_GetIndexedDevice(g_adbManager, i, &info);
        if (address > 0) {
            const char* deviceName = "Unknown";
            if (info.originalAddr == ADB_KEYBOARD_ADDR) {
                deviceName = "Keyboard";
            } else if (info.originalAddr == ADB_MOUSE_ADDR) {
                deviceName = "Mouse";
            }

            printf("Device %d: %s (type=%d, orig_addr=%d, curr_addr=%d)\n",
                   i, deviceName, info.deviceType,
                   info.originalAddr, info.currentAddr);
        }
    }

    /* Dump initial state */
    printf("\nInitial ADB Manager State:\n");
    ADBManager_DumpState(g_adbManager);

    /* Main event loop */
    printf("\nStarting event loop...\n");
    printf("Commands:\n");
    printf("  k - Simulate key press/release (A key)\n");
    printf("  m - Simulate mouse movement\n");
    printf("  q - Quit\n");
    printf("  s - Show status\n");
    printf("\nPress Ctrl+C to exit\n\n");

    int commandCounter = 0;

    while (g_running) {
        /* Process hardware events */
        ADBManager_ProcessHardwareEvents(g_adbManager);

        /* Simple demonstration commands every few seconds */
        if (++commandCounter >= 300) { /* ~3 seconds at 100Hz */
            commandCounter = 0;

            /* Demonstrate keyboard input */
            printf("Demo: Simulating 'A' key press...\n");
            ADBManager_SimulateKeypress(g_adbManager, 0x00, true);  /* A key down */
            usleep(100000); /* 100ms delay */
            ADBManager_SimulateKeypress(g_adbManager, 0x00, false); /* A key up */

            /* Demonstrate mouse input */
            printf("Demo: Simulating mouse movement...\n");
            ADBManager_SimulateMouseMove(g_adbManager, 10, -5, false);
        }

        /* Sleep for ~10ms to simulate 100Hz polling */
        usleep(10000);
    }

    /* Clean up */
    printf("\nCleaning up...\n");
    ADBManager_Shutdown(g_adbManager);
    free(g_adbManager);
    ADBManager_DestroyHardwareInterface(g_hardware);

    printf("Example completed successfully.\n");
    return 0;
}

/* Additional utility functions for demonstration */

void demonstrateADBOperations(ADBManager* mgr) {
    printf("\n=== ADB Operations Demonstration ===\n");

    /* Test flush command */
    printf("Testing flush command...\n");
    int result = ADBManager_Flush(mgr, ADB_KEYBOARD_ADDR);
    printf("Flush result: %s\n", ADBManager_GetErrorString(result));

    /* Test talk command */
    printf("Testing talk command...\n");
    uint8_t buffer[16];
    int bufferLen = sizeof(buffer);
    result = ADBManager_Talk(mgr, ADB_KEYBOARD_ADDR, 0, buffer, &bufferLen);
    printf("Talk result: %s, data length: %d\n",
           ADBManager_GetErrorString(result), bufferLen);

    /* Test asynchronous command */
    printf("Testing asynchronous ADB command...\n");
    ADBOpBlock opBlock = {0};
    opBlock.dataBuffer = buffer;
    opBlock.serviceRoutine = NULL;
    opBlock.dataArea = NULL;

    uint8_t command = (ADB_KEYBOARD_ADDR << 4) | ADB_TALK_CMD | 0;
    result = ADBManager_SendCommand(mgr, command, &opBlock);
    printf("Async command result: %s\n", ADBManager_GetErrorString(result));
}

void demonstrateKeyboardSupport(ADBManager* mgr) {
    printf("\n=== Keyboard Support Demonstration ===\n");

    /* Demonstrate various key codes */
    uint8_t testKeys[] = {
        0x00, /* A */
        0x0B, /* B */
        0x08, /* C */
        0x02, /* D */
        0x0E, /* E */
        0x31, /* Space */
        0x24, /* Return */
        0x33, /* Backspace */
    };

    printf("Simulating key sequence: A-B-C-D-E-Space-Return-Backspace\n");

    for (int i = 0; i < sizeof(testKeys); i++) {
        printf("Key 0x%02X down\n", testKeys[i]);
        ADBManager_SimulateKeypress(mgr, testKeys[i], true);
        usleep(50000); /* 50ms */

        printf("Key 0x%02X up\n", testKeys[i]);
        ADBManager_SimulateKeypress(mgr, testKeys[i], false);
        usleep(50000); /* 50ms */
    }
}

void demonstrateMouseSupport(ADBManager* mgr) {
    printf("\n=== Mouse Support Demonstration ===\n");

    /* Simulate mouse movements in a pattern */
    printf("Simulating mouse movement pattern...\n");

    struct {
        int16_t dx, dy;
        bool button;
    } movements[] = {
        {10, 0, false},    /* Right */
        {0, 10, false},    /* Down */
        {-10, 0, false},   /* Left */
        {0, -10, false},   /* Up */
        {5, 5, true},      /* Diagonal with button */
        {0, 0, false},     /* Button release */
    };

    for (int i = 0; i < sizeof(movements) / sizeof(movements[0]); i++) {
        printf("Mouse: dx=%d, dy=%d, button=%s\n",
               movements[i].dx, movements[i].dy,
               movements[i].button ? "down" : "up");

        ADBManager_SimulateMouseMove(mgr, movements[i].dx, movements[i].dy,
                                     movements[i].button);
        usleep(200000); /* 200ms */
    }
}