/*
 * System 7.1 Portable - VM Init System (Simplified)
 *
 * This is a simplified init process for the System 7.1 Portable VM.
 * It demonstrates the System 7.1 interface in a terminal environment.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <signal.h>
#include <errno.h>
#include <termios.h>

// Global state
static int running = 1;

// Signal handler for clean shutdown
void signal_handler(int sig) {
    if (sig == SIGTERM || sig == SIGINT) {
        printf("\nShutting down System 7.1 Portable...\n");
        running = 0;
    }
}

// Mount essential filesystems
int mount_filesystems() {
    // Mount /proc
    if (mount("proc", "/proc", "proc", 0, NULL) < 0) {
        if (errno != EBUSY) {
            perror("Failed to mount /proc");
            return -1;
        }
    }

    // Mount /sys
    if (mount("sysfs", "/sys", "sysfs", 0, NULL) < 0) {
        if (errno != EBUSY) {
            perror("Failed to mount /sys");
            return -1;
        }
    }

    // Mount /dev as devtmpfs
    if (mount("devtmpfs", "/dev", "devtmpfs", 0, NULL) < 0) {
        if (errno != EBUSY) {
            perror("Failed to mount /dev");
            return -1;
        }
    }

    // Mount /tmp as tmpfs
    if (mount("tmpfs", "/tmp", "tmpfs", 0, "size=64M") < 0) {
        if (errno != EBUSY) {
            perror("Failed to mount /tmp");
            return -1;
        }
    }

    return 0;
}

// Show startup screen
void show_startup() {
    printf("\033[2J\033[H");  // Clear screen
    printf("\n\n\n");
    printf("        ╔════════════════════════════════════╗\n");
    printf("        ║                                    ║\n");
    printf("        ║      Welcome to Macintosh          ║\n");
    printf("        ║                                    ║\n");
    printf("        ║            🖥️  😊                  ║\n");
    printf("        ║                                    ║\n");
    printf("        ║      System 7.1 Portable           ║\n");
    printf("        ║         VM Edition                 ║\n");
    printf("        ║                                    ║\n");
    printf("        ╚════════════════════════════════════╝\n");
    printf("\n\n");
    sleep(3);
}

// Initialize system components
void init_components() {
    printf("\033[2J\033[H");  // Clear screen
    printf("\n  System 7.1 Portable - Initialization\n");
    printf("  =====================================\n\n");

    const char* components[] = {
        "Resource Manager",
        "Memory Manager",
        "File Manager",
        "QuickDraw",
        "Window Manager",
        "Menu Manager",
        "Event Manager",
        "Dialog Manager",
        "Control Manager",
        "TextEdit",
        "List Manager",
        "Scrap Manager",
        "Print Manager",
        "Finder"
    };

    int num_components = sizeof(components) / sizeof(components[0]);

    for (int i = 0; i < num_components; i++) {
        printf("  Loading %-20s", components[i]);
        fflush(stdout);
        usleep(100000);  // 100ms delay
        printf(" [✓]\n");
    }

    printf("\n  All components loaded successfully!\n");
    sleep(2);
}

// Show desktop
void show_desktop() {
    printf("\033[2J\033[H");  // Clear screen
    printf("╔════════════════════════════════════════════════════════════════════╗\n");
    printf("║ 🍎 File  Edit  View  Special  Help                                ║\n");
    printf("╠════════════════════════════════════════════════════════════════════╣\n");
    printf("║                                                                    ║\n");
    printf("║                  System 7.1 Portable Desktop                      ║\n");
    printf("║                                                                    ║\n");
    printf("║      📁 System Folder          📁 Applications                   ║\n");
    printf("║                                                                    ║\n");
    printf("║      📁 Documents              📁 Utilities                      ║\n");
    printf("║                                                                    ║\n");
    printf("║      🗑️  Trash (Empty)                                            ║\n");
    printf("║                                                                    ║\n");
    printf("╟────────────────────────────────────────────────────────────────────╢\n");
    printf("║ Memory: 512MB  |  Disk: 1GB  |  System 7.1 Portable v0.92        ║\n");
    printf("╚════════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

// Simple menu system
void run_menu() {
    char choice[10];

    while (running) {
        printf("\n");
        printf("  Commands:\n");
        printf("  1. About System 7.1 Portable\n");
        printf("  2. Open System Folder\n");
        printf("  3. Open Applications\n");
        printf("  4. System Information\n");
        printf("  5. Restart\n");
        printf("  6. Shutdown\n");
        printf("\n");
        printf("  Enter choice (1-6): ");
        fflush(stdout);

        if (fgets(choice, sizeof(choice), stdin) == NULL) {
            break;
        }

        int ch = choice[0] - '0';

        switch (ch) {
            case 1:
                printf("\n");
                printf("  ╔════════════════════════════════════╗\n");
                printf("  ║   System 7.1 Portable              ║\n");
                printf("  ║   Version 0.92 (Beta)              ║\n");
                printf("  ║                                    ║\n");
                printf("  ║   A modern reimplementation of     ║\n");
                printf("  ║   Mac OS System 7.1                ║\n");
                printf("  ║                                    ║\n");
                printf("  ║   92%% Complete                     ║\n");
                printf("  ╚════════════════════════════════════╝\n");
                break;

            case 2:
                printf("\n  System Folder:\n");
                printf("  - Extensions (12 items)\n");
                printf("  - Control Panels (8 items)\n");
                printf("  - Preferences (15 items)\n");
                printf("  - Fonts (24 items)\n");
                break;

            case 3:
                printf("\n  Applications:\n");
                printf("  - SimpleText\n");
                printf("  - Calculator\n");
                printf("  - Note Pad\n");
                printf("  - Scrapbook\n");
                break;

            case 4:
                printf("\n  System Information:\n");
                printf("  - Architecture: x86_64\n");
                printf("  - Memory: 512 MB\n");
                printf("  - Processor: Virtual CPU\n");
                printf("  - System: 7.1 Portable\n");
                printf("  - Build: 0.92.0\n");
                break;

            case 5:
                printf("\n  Restarting...\n");
                sleep(2);
                show_startup();
                init_components();
                show_desktop();
                break;

            case 6:
                running = 0;
                break;

            default:
                printf("  Invalid choice\n");
                break;
        }
    }
}

// Cleanup
void cleanup() {
    // Unmount filesystems
    umount("/tmp");
    umount("/dev");
    umount("/sys");
    umount("/proc");
}

int main(int argc, char *argv[]) {
    // Set up signal handlers
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    // Check if we're running as init (PID 1)
    if (getpid() == 1) {
        printf("System 7.1 Portable VM Init v1.0\n");
        printf("================================\n\n");

        // Mount essential filesystems
        if (mount_filesystems() < 0) {
            fprintf(stderr, "Failed to mount filesystems\n");
            // Continue anyway for testing
        }
    }

    // Show startup sequence
    show_startup();

    // Initialize components
    init_components();

    // Show desktop
    show_desktop();

    // Run menu system
    printf("  System 7.1 Portable is ready!\n");
    printf("  Press Enter to continue...\n");
    getchar();

    run_menu();

    // Cleanup
    printf("\nShutting down...\n");
    cleanup();

    // If we're init, sync and halt
    if (getpid() == 1) {
        sync();
        reboot(RB_POWER_OFF);
    }

    return 0;
}