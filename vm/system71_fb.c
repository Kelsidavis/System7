/*
 * System 7.1 Portable - Framebuffer VM Implementation
 *
 * Direct framebuffer graphics for minimal Linux systems
 * No external dependencies required
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <termios.h>
#include <time.h>

// Framebuffer globals
static int fbfd = -1;
static struct fb_var_screeninfo vinfo;
static struct fb_fix_screeninfo finfo;
static char *fbp = NULL;
static long screensize = 0;

// Input globals
static int mousefd = -1;
static int kbdfd = -1;
static struct termios orig_termios;

// UI state
static int running = 1;
static int mouse_x, mouse_y;
static int screen_width, screen_height;

// Window structure
typedef struct {
    int x, y, w, h;
    char title[64];
    int active;
    int visible;
} Window;

// Desktop icons
typedef struct {
    int x, y;
    char label[32];
    int selected;
} Icon;

#define MAX_WINDOWS 10
#define MAX_ICONS 10

static Window windows[MAX_WINDOWS];
static int window_count = 0;
static Icon icons[MAX_ICONS];
static int icon_count = 0;

// Signal handler
void signal_handler(int sig) {
    running = 0;
}

// Set pixel in framebuffer
void set_pixel(int x, int y, uint32_t color) {
    if (x < 0 || x >= screen_width || y < 0 || y >= screen_height)
        return;

    long location = (x + vinfo.xoffset) * (vinfo.bits_per_pixel / 8) +
                    (y + vinfo.yoffset) * finfo.line_length;

    if (vinfo.bits_per_pixel == 32) {
        *((uint32_t*)(fbp + location)) = color;
    } else if (vinfo.bits_per_pixel == 16) {
        // Convert to RGB565
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = color & 0xFF;
        uint16_t rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        *((uint16_t*)(fbp + location)) = rgb565;
    }
}

// Draw rectangle
void draw_rect(int x, int y, int w, int h, uint32_t color, int filled) {
    if (filled) {
        for (int dy = 0; dy < h; dy++) {
            for (int dx = 0; dx < w; dx++) {
                set_pixel(x + dx, y + dy, color);
            }
        }
    } else {
        for (int dx = 0; dx < w; dx++) {
            set_pixel(x + dx, y, color);
            set_pixel(x + dx, y + h - 1, color);
        }
        for (int dy = 0; dy < h; dy++) {
            set_pixel(x, y + dy, color);
            set_pixel(x + w - 1, y + dy, color);
        }
    }
}

// Draw text (simple bitmap font)
void draw_char(int x, int y, char c, uint32_t color) {
    // Simple 5x7 bitmap font for ASCII 32-126
    static const uint8_t font[][5] = {
        {0x00,0x00,0x00,0x00,0x00}, // space
        {0x00,0x00,0x5F,0x00,0x00}, // !
        {0x00,0x07,0x00,0x07,0x00}, // "
        // ... simplified, just show blocks for now
    };

    // For simplicity, just draw a small rectangle for each char
    draw_rect(x, y, 6, 8, color, 0);
}

void draw_text(int x, int y, const char* text, uint32_t color) {
    int dx = 0;
    while (*text) {
        draw_char(x + dx, y, *text, color);
        dx += 7;
        text++;
    }
}

// Draw desktop pattern
void draw_desktop_pattern() {
    // Gray desktop with pattern
    for (int y = 0; y < screen_height; y++) {
        for (int x = 0; x < screen_width; x++) {
            uint32_t color = ((x + y) & 1) ? 0x808080 : 0xA0A0A0;
            set_pixel(x, y, color);
        }
    }
}

// Draw menu bar
void draw_menubar() {
    // Menu bar background
    draw_rect(0, 0, screen_width, 20, 0xEEEEEE, 1);
    draw_rect(0, 20, screen_width, 1, 0x000000, 1);

    // Apple menu
    draw_text(10, 6, "Apple", 0x000000);

    // File menu
    draw_text(60, 6, "File", 0x000000);

    // Edit menu
    draw_text(110, 6, "Edit", 0x000000);

    // View menu
    draw_text(160, 6, "View", 0x000000);

    // Special menu
    draw_text(210, 6, "Special", 0x000000);

    // Clock
    time_t t = time(NULL);
    struct tm* tm = localtime(&t);
    char timeStr[16];
    sprintf(timeStr, "%02d:%02d", tm->tm_hour, tm->tm_min);
    draw_text(screen_width - 50, 6, timeStr, 0x000000);
}

// Draw window
void draw_window(Window* win) {
    if (!win->visible) return;

    // Shadow
    draw_rect(win->x + 3, win->y + 3, win->w, win->h, 0x606060, 1);

    // Window background
    draw_rect(win->x, win->y, win->w, win->h, 0xFFFFFF, 1);

    // Window border
    draw_rect(win->x, win->y, win->w, win->h, 0x000000, 0);

    // Title bar
    if (win->active) {
        // Active title bar with stripes
        for (int i = 0; i < 20; i += 2) {
            draw_rect(win->x + 1, win->y + i + 1, win->w - 2, 1, 0x000000, 1);
        }
    } else {
        draw_rect(win->x + 1, win->y + 1, win->w - 2, 20, 0xCCCCCC, 1);
    }

    // Close box
    draw_rect(win->x + 8, win->y + 4, 12, 12, 0xFFFFFF, 1);
    draw_rect(win->x + 8, win->y + 4, 12, 12, 0x000000, 0);

    // Title
    draw_text(win->x + win->w/2 - strlen(win->title)*3,
              win->y + 6, win->title,
              win->active ? 0xFFFFFF : 0x000000);

    // Content area
    draw_rect(win->x + 1, win->y + 21, win->w - 2, win->h - 22, 0xFFFFFF, 1);
}

// Draw icon
void draw_icon(Icon* icon) {
    // Icon folder shape
    draw_rect(icon->x, icon->y, 32, 24, 0xCCCCCC, 1);
    draw_rect(icon->x, icon->y, 32, 24, 0x000000, 0);
    draw_rect(icon->x + 2, icon->y - 4, 12, 6, 0xCCCCCC, 1);

    // Selection highlight
    if (icon->selected) {
        draw_rect(icon->x - 2, icon->y - 6, 36, 40, 0x0000FF, 0);
    }

    // Label
    draw_text(icon->x - 10, icon->y + 28, icon->label, 0x000000);
}

// Initialize framebuffer
int init_framebuffer() {
    // Open framebuffer device
    fbfd = open("/dev/fb0", O_RDWR);
    if (fbfd < 0) {
        perror("Error opening framebuffer");
        return -1;
    }

    // Get variable screen info
    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        perror("Error reading variable screen info");
        return -1;
    }

    // Get fixed screen info
    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo) < 0) {
        perror("Error reading fixed screen info");
        return -1;
    }

    screen_width = vinfo.xres;
    screen_height = vinfo.yres;

    // Calculate screensize
    screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;

    // Map framebuffer to memory
    fbp = (char*)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
    if ((intptr_t)fbp == -1) {
        perror("Error mapping framebuffer");
        return -1;
    }

    printf("Framebuffer initialized: %dx%d, %d bpp\n",
           screen_width, screen_height, vinfo.bits_per_pixel);

    return 0;
}

// Initialize input
void init_input() {
    // Try to open mouse device
    mousefd = open("/dev/input/mice", O_RDONLY | O_NONBLOCK);
    if (mousefd < 0) {
        mousefd = open("/dev/input/mouse0", O_RDONLY | O_NONBLOCK);
    }

    // Set terminal to raw mode for keyboard input
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    // Make stdin non-blocking
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);

    mouse_x = screen_width / 2;
    mouse_y = screen_height / 2;
}

// Cleanup
void cleanup() {
    // Restore terminal
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);

    // Unmap framebuffer
    if (fbp) munmap(fbp, screensize);
    if (fbfd >= 0) close(fbfd);
    if (mousefd >= 0) close(mousefd);
}

// Handle input
void handle_input() {
    // Read mouse
    if (mousefd >= 0) {
        signed char mouse_packet[3];
        if (read(mousefd, mouse_packet, 3) == 3) {
            mouse_x += mouse_packet[1];
            mouse_y -= mouse_packet[2];

            // Clamp to screen
            if (mouse_x < 0) mouse_x = 0;
            if (mouse_x >= screen_width) mouse_x = screen_width - 1;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_y >= screen_height) mouse_y = screen_height - 1;

            // Check buttons
            if (mouse_packet[0] & 0x01) { // Left button
                // Handle click
            }
        }
    }

    // Read keyboard
    char key;
    if (read(STDIN_FILENO, &key, 1) == 1) {
        if (key == 'q' || key == 'Q') {
            running = 0;
        }
    }
}

// Draw mouse cursor
void draw_cursor() {
    // Simple arrow cursor
    for (int i = 0; i < 10; i++) {
        set_pixel(mouse_x + i, mouse_y, 0x000000);
        set_pixel(mouse_x, mouse_y + i, 0x000000);
    }
    for (int i = 1; i < 8; i++) {
        set_pixel(mouse_x + i, mouse_y + i, 0x000000);
    }
}

// Show startup screen
void show_startup() {
    // Clear to gray
    for (int y = 0; y < screen_height; y++) {
        for (int x = 0; x < screen_width; x++) {
            set_pixel(x, y, 0xC0C0C0);
        }
    }

    // Draw startup box
    int bx = screen_width/2 - 150;
    int by = screen_height/2 - 100;
    draw_rect(bx, by, 300, 200, 0xFFFFFF, 1);
    draw_rect(bx, by, 300, 200, 0x000000, 0);

    // Draw Happy Mac (simplified)
    draw_rect(bx + 125, by + 40, 50, 60, 0x000000, 0);
    draw_rect(bx + 135, by + 60, 5, 5, 0x000000, 1); // left eye
    draw_rect(bx + 160, by + 60, 5, 5, 0x000000, 1); // right eye
    draw_rect(bx + 135, by + 80, 30, 2, 0x000000, 1); // smile

    // Draw text
    draw_text(bx + 80, by + 120, "Welcome to Macintosh", 0x000000);
    draw_text(bx + 85, by + 140, "System 7.1 Portable", 0x000000);

    sleep(3);
}

// Initialize desktop
void init_desktop() {
    // Create desktop icons
    icon_count = 4;

    strcpy(icons[0].label, "System");
    icons[0].x = 50;
    icons[0].y = 60;
    icons[0].selected = 0;

    strcpy(icons[1].label, "Apps");
    icons[1].x = 150;
    icons[1].y = 60;
    icons[1].selected = 0;

    strcpy(icons[2].label, "Docs");
    icons[2].x = 50;
    icons[2].y = 140;
    icons[2].selected = 0;

    strcpy(icons[3].label, "Trash");
    icons[3].x = screen_width - 80;
    icons[3].y = screen_height - 100;
    icons[3].selected = 0;

    // Create initial window
    window_count = 1;
    windows[0].x = 100;
    windows[0].y = 100;
    windows[0].w = 400;
    windows[0].h = 300;
    strcpy(windows[0].title, "Welcome");
    windows[0].active = 1;
    windows[0].visible = 1;
}

// Main render function
void render() {
    // Draw desktop
    draw_desktop_pattern();

    // Draw icons
    for (int i = 0; i < icon_count; i++) {
        draw_icon(&icons[i]);
    }

    // Draw windows
    for (int i = 0; i < window_count; i++) {
        draw_window(&windows[i]);
    }

    // Draw menu bar (on top)
    draw_menubar();

    // Draw cursor
    draw_cursor();
}

int main() {
    printf("System 7.1 Portable - Framebuffer VM\n");
    printf("=====================================\n\n");

    // Set up signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Initialize framebuffer
    if (init_framebuffer() < 0) {
        fprintf(stderr, "Failed to initialize framebuffer\n");
        fprintf(stderr, "Make sure /dev/fb0 exists and is accessible\n");
        fprintf(stderr, "You may need to run as root or add user to video group\n");
        return 1;
    }

    // Initialize input
    init_input();

    // Show startup screen
    show_startup();

    // Initialize desktop
    init_desktop();

    // Main loop
    printf("System running. Press 'Q' to quit.\n");

    while (running) {
        // Handle input
        handle_input();

        // Render
        render();

        // Small delay
        usleep(16666); // ~60 FPS
    }

    // Cleanup
    cleanup();

    printf("\nSystem 7.1 Portable shutdown complete.\n");
    return 0;
}