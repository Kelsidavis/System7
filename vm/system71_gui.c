/*
 * System 7.1 Portable - Graphical VM Implementation
 *
 * This provides a full graphical System 7.1 experience using
 * framebuffer graphics and SDL2 for cross-platform support.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <math.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

// System constants
#define SCREEN_WIDTH 1024
#define SCREEN_HEIGHT 768
#define MENUBAR_HEIGHT 20
#define WINDOW_TITLEBAR_HEIGHT 20
#define DESKTOP_PATTERN_SIZE 8

// Colors (Mac OS Classic palette)
#define COLOR_WHITE     0xFFFFFFFF
#define COLOR_BLACK     0xFF000000
#define COLOR_GRAY_LIGHT 0xFFDDDDDD
#define COLOR_GRAY_MED   0xFF999999
#define COLOR_GRAY_DARK  0xFF666666
#define COLOR_BLUE      0xFF0000FF
#define COLOR_MENU_BG   0xFFEEEEEE
#define COLOR_WINDOW_BG 0xFFFFFFFF
#define COLOR_DESKTOP   0xFF808080

// Window types
typedef enum {
    WINDOW_DOCUMENT,
    WINDOW_DIALOG,
    WINDOW_ALERT,
    WINDOW_DESKTOP
} WindowType;

// Window structure
typedef struct Window {
    int id;
    char title[256];
    SDL_Rect bounds;
    WindowType type;
    int visible;
    int active;
    struct Window* next;
} Window;

// Menu item structure
typedef struct MenuItem {
    char text[256];
    int enabled;
    void (*handler)(void);
} MenuItem;

// Menu structure
typedef struct Menu {
    char title[256];
    MenuItem* items;
    int itemCount;
    int visible;
    SDL_Rect bounds;
} Menu;

// Icon structure
typedef struct Icon {
    char label[256];
    SDL_Rect bounds;
    int selected;
    void (*doubleClickHandler)(void);
} Icon;

// Global state
static SDL_Window* sdlWindow = NULL;
static SDL_Renderer* renderer = NULL;
static TTF_Font* systemFont = NULL;
static TTF_Font* menuFont = NULL;
static int running = 1;
static Window* windowList = NULL;
static Window* activeWindow = NULL;
static Menu* menuBar = NULL;
static int menuBarCount = 0;
static Icon* desktopIcons = NULL;
static int desktopIconCount = 0;
static int mouseX = 0, mouseY = 0;
static int mouseDown = 0;
static int dragWindow = 0;
static SDL_Point dragOffset;

// Forward declarations
void init_graphics(void);
void cleanup_graphics(void);
void create_system_menus(void);
void create_desktop_icons(void);
void draw_desktop(void);
void draw_menubar(void);
void draw_windows(void);
void draw_window(Window* win);
void draw_desktop_pattern(void);
void handle_events(void);
void handle_mouse_down(int x, int y);
void handle_mouse_up(int x, int y);
void handle_mouse_motion(int x, int y);
Window* create_window(const char* title, int x, int y, int w, int h, WindowType type);
void destroy_window(Window* win);
void bring_to_front(Window* win);
void draw_text(const char* text, int x, int y, uint32_t color);
void draw_rect(SDL_Rect* rect, uint32_t color, int filled);

// Menu handlers
void menu_about(void);
void menu_new_folder(void);
void menu_quit(void);
void menu_system_info(void);

// Icon handlers
void open_system_folder(void);
void open_applications(void);
void open_trash(void);

// Initialize graphics system
void init_graphics(void) {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
        exit(1);
    }

    // Initialize SDL_ttf
    if (TTF_Init() < 0) {
        fprintf(stderr, "SDL_ttf initialization failed: %s\n", TTF_GetError());
        exit(1);
    }

    // Create window
    sdlWindow = SDL_CreateWindow(
        "System 7.1 Portable",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        SDL_WINDOW_SHOWN
    );

    if (!sdlWindow) {
        fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
        exit(1);
    }

    // Create renderer
    renderer = SDL_CreateRenderer(sdlWindow, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    if (!renderer) {
        fprintf(stderr, "Renderer creation failed: %s\n", SDL_GetError());
        exit(1);
    }

    // Load fonts (using a fallback to monospace if Chicago not available)
    systemFont = TTF_OpenFont("/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf", 12);
    if (!systemFont) {
        // Try another common font path
        systemFont = TTF_OpenFont("/System/Library/Fonts/Helvetica.ttc", 12);
    }
    if (!systemFont) {
        fprintf(stderr, "Warning: Could not load system font\n");
    }

    menuFont = systemFont; // Use same font for menus
}

// Cleanup graphics
void cleanup_graphics(void) {
    // Clean up windows
    Window* win = windowList;
    while (win) {
        Window* next = win->next;
        free(win);
        win = next;
    }

    // Clean up fonts
    if (systemFont) TTF_CloseFont(systemFont);

    // Clean up SDL
    if (renderer) SDL_DestroyRenderer(renderer);
    if (sdlWindow) SDL_DestroyWindow(sdlWindow);
    TTF_Quit();
    SDL_Quit();
}

// Draw desktop pattern
void draw_desktop_pattern(void) {
    // Classic Mac OS gray pattern
    uint8_t pattern[8] = {0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55};

    SDL_SetRenderDrawColor(renderer, 128, 128, 128, 255);
    SDL_RenderClear(renderer);

    // Draw pattern
    SDL_SetRenderDrawColor(renderer, 192, 192, 192, 255);
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            if (pattern[y % 8] & (1 << (x % 8))) {
                SDL_RenderDrawPoint(renderer, x, y);
            }
        }
    }
}

// Draw rectangle
void draw_rect(SDL_Rect* rect, uint32_t color, int filled) {
    SDL_SetRenderDrawColor(renderer,
        (color >> 16) & 0xFF,
        (color >> 8) & 0xFF,
        color & 0xFF,
        (color >> 24) & 0xFF);

    if (filled) {
        SDL_RenderFillRect(renderer, rect);
    } else {
        SDL_RenderDrawRect(renderer, rect);
    }
}

// Draw text (simplified without font)
void draw_text(const char* text, int x, int y, uint32_t color) {
    if (!systemFont) {
        // Fallback: just draw a placeholder
        SDL_SetRenderDrawColor(renderer,
            (color >> 16) & 0xFF,
            (color >> 8) & 0xFF,
            color & 0xFF,
            (color >> 24) & 0xFF);
        SDL_Rect r = {x, y, strlen(text) * 6, 12};
        SDL_RenderDrawRect(renderer, &r);
        return;
    }

    SDL_Color c = {
        (color >> 16) & 0xFF,
        (color >> 8) & 0xFF,
        color & 0xFF,
        (color >> 24) & 0xFF
    };

    SDL_Surface* surface = TTF_RenderText_Solid(systemFont, text, c);
    if (surface) {
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (texture) {
            SDL_Rect dest = {x, y, surface->w, surface->h};
            SDL_RenderCopy(renderer, texture, NULL, &dest);
            SDL_DestroyTexture(texture);
        }
        SDL_FreeSurface(surface);
    }
}

// Draw desktop
void draw_desktop(void) {
    draw_desktop_pattern();

    // Draw desktop icons
    for (int i = 0; i < desktopIconCount; i++) {
        Icon* icon = &desktopIcons[i];

        // Draw icon background if selected
        if (icon->selected) {
            SDL_Rect sel = icon->bounds;
            sel.x -= 2;
            sel.y -= 2;
            sel.w += 4;
            sel.h += 4;
            draw_rect(&sel, COLOR_BLACK, 0);
        }

        // Draw icon (simplified as folder)
        SDL_Rect folder = {
            icon->bounds.x + 16,
            icon->bounds.y + 8,
            32, 24
        };
        draw_rect(&folder, COLOR_GRAY_LIGHT, 1);
        draw_rect(&folder, COLOR_BLACK, 0);

        // Draw label
        draw_text(icon->label,
            icon->bounds.x + (icon->bounds.w - strlen(icon->label) * 6) / 2,
            icon->bounds.y + 40,
            COLOR_BLACK);
    }
}

// Draw menu bar
void draw_menubar(void) {
    // Draw menu bar background
    SDL_Rect menuRect = {0, 0, SCREEN_WIDTH, MENUBAR_HEIGHT};
    draw_rect(&menuRect, COLOR_MENU_BG, 1);
    draw_rect(&menuRect, COLOR_BLACK, 0);

    // Draw menus
    int x = 10;
    for (int i = 0; i < menuBarCount; i++) {
        Menu* menu = &menuBar[i];
        menu->bounds.x = x;
        menu->bounds.y = 2;
        menu->bounds.w = strlen(menu->title) * 8 + 16;
        menu->bounds.h = MENUBAR_HEIGHT - 4;

        // Highlight if menu is open
        if (menu->visible) {
            draw_rect(&menu->bounds, COLOR_BLACK, 1);
            draw_text(menu->title, x + 8, 4, COLOR_WHITE);
        } else {
            draw_text(menu->title, x + 8, 4, COLOR_BLACK);
        }

        x += menu->bounds.w + 10;
    }

    // Draw clock on the right
    time_t t = time(NULL);
    struct tm* tm_info = localtime(&t);
    char timeStr[32];
    strftime(timeStr, sizeof(timeStr), "%I:%M %p", tm_info);
    draw_text(timeStr, SCREEN_WIDTH - 80, 4, COLOR_BLACK);
}

// Draw window
void draw_window(Window* win) {
    if (!win->visible) return;

    // Draw window shadow
    SDL_Rect shadow = win->bounds;
    shadow.x += 3;
    shadow.y += 3;
    draw_rect(&shadow, COLOR_GRAY_DARK, 1);

    // Draw window background
    draw_rect(&win->bounds, COLOR_WINDOW_BG, 1);
    draw_rect(&win->bounds, COLOR_BLACK, 0);

    // Draw title bar
    SDL_Rect titleBar = {
        win->bounds.x,
        win->bounds.y,
        win->bounds.w,
        WINDOW_TITLEBAR_HEIGHT
    };

    if (win->active) {
        // Active window title bar (horizontal lines pattern)
        for (int y = titleBar.y; y < titleBar.y + titleBar.h; y += 2) {
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderDrawLine(renderer, titleBar.x + 1, y,
                              titleBar.x + titleBar.w - 2, y);
        }
    } else {
        draw_rect(&titleBar, COLOR_GRAY_LIGHT, 1);
    }

    // Draw close box
    SDL_Rect closeBox = {
        win->bounds.x + 8,
        win->bounds.y + 4,
        12, 12
    };
    draw_rect(&closeBox, COLOR_WHITE, 1);
    draw_rect(&closeBox, COLOR_BLACK, 0);

    // Draw title
    draw_text(win->title,
        win->bounds.x + (win->bounds.w - strlen(win->title) * 6) / 2,
        win->bounds.y + 4,
        win->active ? COLOR_WHITE : COLOR_BLACK);

    // Draw content area
    SDL_Rect content = {
        win->bounds.x + 1,
        win->bounds.y + WINDOW_TITLEBAR_HEIGHT,
        win->bounds.w - 2,
        win->bounds.h - WINDOW_TITLEBAR_HEIGHT - 1
    };
    draw_rect(&content, COLOR_WHITE, 1);
}

// Draw all windows
void draw_windows(void) {
    Window* win = windowList;
    while (win) {
        draw_window(win);
        win = win->next;
    }
}

// Create window
Window* create_window(const char* title, int x, int y, int w, int h, WindowType type) {
    Window* win = (Window*)calloc(1, sizeof(Window));
    if (!win) return NULL;

    static int nextId = 1;
    win->id = nextId++;
    strncpy(win->title, title, sizeof(win->title) - 1);
    win->bounds.x = x;
    win->bounds.y = y;
    win->bounds.w = w;
    win->bounds.h = h;
    win->type = type;
    win->visible = 1;
    win->active = 1;

    // Deactivate other windows
    Window* other = windowList;
    while (other) {
        other->active = 0;
        other = other->next;
    }

    // Add to front of list
    win->next = windowList;
    windowList = win;
    activeWindow = win;

    return win;
}

// Bring window to front
void bring_to_front(Window* win) {
    if (!win || win == windowList) return;

    // Remove from list
    Window* prev = NULL;
    Window* curr = windowList;
    while (curr && curr != win) {
        prev = curr;
        curr = curr->next;
    }

    if (!curr) return;

    if (prev) {
        prev->next = curr->next;
    }

    // Add to front
    win->next = windowList;
    windowList = win;

    // Update active window
    Window* w = windowList;
    while (w) {
        w->active = (w == win);
        w = w->next;
    }
    activeWindow = win;
}

// Create system menus
void create_system_menus(void) {
    menuBarCount = 5;
    menuBar = (Menu*)calloc(menuBarCount, sizeof(Menu));

    // Apple menu
    strcpy(menuBar[0].title, "🍎");
    menuBar[0].itemCount = 2;
    menuBar[0].items = (MenuItem*)calloc(2, sizeof(MenuItem));
    strcpy(menuBar[0].items[0].text, "About System 7.1...");
    menuBar[0].items[0].handler = menu_about;
    menuBar[0].items[0].enabled = 1;

    // File menu
    strcpy(menuBar[1].title, "File");
    menuBar[1].itemCount = 3;
    menuBar[1].items = (MenuItem*)calloc(3, sizeof(MenuItem));
    strcpy(menuBar[1].items[0].text, "New Folder");
    menuBar[1].items[0].handler = menu_new_folder;
    menuBar[1].items[0].enabled = 1;
    strcpy(menuBar[1].items[2].text, "Quit");
    menuBar[1].items[2].handler = menu_quit;
    menuBar[1].items[2].enabled = 1;

    // Edit menu
    strcpy(menuBar[2].title, "Edit");
    menuBar[2].itemCount = 5;
    menuBar[2].items = (MenuItem*)calloc(5, sizeof(MenuItem));
    strcpy(menuBar[2].items[0].text, "Undo");
    strcpy(menuBar[2].items[1].text, "Cut");
    strcpy(menuBar[2].items[2].text, "Copy");
    strcpy(menuBar[2].items[3].text, "Paste");
    strcpy(menuBar[2].items[4].text, "Select All");

    // View menu
    strcpy(menuBar[3].title, "View");

    // Special menu
    strcpy(menuBar[4].title, "Special");
    menuBar[4].itemCount = 1;
    menuBar[4].items = (MenuItem*)calloc(1, sizeof(MenuItem));
    strcpy(menuBar[4].items[0].text, "System Info...");
    menuBar[4].items[0].handler = menu_system_info;
    menuBar[4].items[0].enabled = 1;
}

// Create desktop icons
void create_desktop_icons(void) {
    desktopIconCount = 4;
    desktopIcons = (Icon*)calloc(desktopIconCount, sizeof(Icon));

    // System Folder
    strcpy(desktopIcons[0].label, "System Folder");
    desktopIcons[0].bounds = (SDL_Rect){50, 50, 80, 60};
    desktopIcons[0].doubleClickHandler = open_system_folder;

    // Applications
    strcpy(desktopIcons[1].label, "Applications");
    desktopIcons[1].bounds = (SDL_Rect){150, 50, 80, 60};
    desktopIcons[1].doubleClickHandler = open_applications;

    // Documents
    strcpy(desktopIcons[2].label, "Documents");
    desktopIcons[2].bounds = (SDL_Rect){50, 150, 80, 60};

    // Trash
    strcpy(desktopIcons[3].label, "Trash");
    desktopIcons[3].bounds = (SDL_Rect){SCREEN_WIDTH - 100, SCREEN_HEIGHT - 100, 80, 60};
    desktopIcons[3].doubleClickHandler = open_trash;
}

// Menu handlers
void menu_about(void) {
    create_window("About System 7.1 Portable",
        SCREEN_WIDTH/2 - 200, SCREEN_HEIGHT/2 - 100,
        400, 200, WINDOW_DIALOG);
}

void menu_new_folder(void) {
    static int folderNum = 1;
    char name[256];
    snprintf(name, sizeof(name), "New Folder %d", folderNum++);

    // Add new icon to desktop
    desktopIconCount++;
    desktopIcons = realloc(desktopIcons, desktopIconCount * sizeof(Icon));
    Icon* newIcon = &desktopIcons[desktopIconCount - 1];
    strcpy(newIcon->label, name);
    newIcon->bounds = (SDL_Rect){250 + (folderNum * 20), 250, 80, 60};
    newIcon->selected = 0;
    newIcon->doubleClickHandler = NULL;
}

void menu_quit(void) {
    running = 0;
}

void menu_system_info(void) {
    create_window("System Information",
        SCREEN_WIDTH/2 - 250, SCREEN_HEIGHT/2 - 150,
        500, 300, WINDOW_DIALOG);
}

// Icon handlers
void open_system_folder(void) {
    create_window("System Folder",
        100, 100, 400, 300, WINDOW_DOCUMENT);
}

void open_applications(void) {
    create_window("Applications",
        150, 150, 400, 300, WINDOW_DOCUMENT);
}

void open_trash(void) {
    create_window("Trash",
        200, 200, 300, 200, WINDOW_DOCUMENT);
}

// Handle mouse down
void handle_mouse_down(int x, int y) {
    mouseDown = 1;

    // Check menu bar
    if (y < MENUBAR_HEIGHT) {
        for (int i = 0; i < menuBarCount; i++) {
            if (x >= menuBar[i].bounds.x &&
                x < menuBar[i].bounds.x + menuBar[i].bounds.w) {
                menuBar[i].visible = !menuBar[i].visible;
                // Close other menus
                for (int j = 0; j < menuBarCount; j++) {
                    if (j != i) menuBar[j].visible = 0;
                }
                return;
            }
        }
    }

    // Check windows
    Window* win = windowList;
    while (win) {
        if (win->visible &&
            x >= win->bounds.x && x < win->bounds.x + win->bounds.w &&
            y >= win->bounds.y && y < win->bounds.y + win->bounds.h) {

            bring_to_front(win);

            // Check close box
            if (x >= win->bounds.x + 8 && x < win->bounds.x + 20 &&
                y >= win->bounds.y + 4 && y < win->bounds.y + 16) {
                win->visible = 0;
                return;
            }

            // Start dragging if in title bar
            if (y < win->bounds.y + WINDOW_TITLEBAR_HEIGHT) {
                dragWindow = 1;
                dragOffset.x = x - win->bounds.x;
                dragOffset.y = y - win->bounds.y;
            }
            return;
        }
        win = win->next;
    }

    // Check desktop icons
    for (int i = 0; i < desktopIconCount; i++) {
        Icon* icon = &desktopIcons[i];
        if (x >= icon->bounds.x && x < icon->bounds.x + icon->bounds.w &&
            y >= icon->bounds.y && y < icon->bounds.y + icon->bounds.h) {

            // Deselect all others
            for (int j = 0; j < desktopIconCount; j++) {
                desktopIcons[j].selected = (i == j);
            }
            return;
        }
    }

    // Deselect all icons
    for (int i = 0; i < desktopIconCount; i++) {
        desktopIcons[i].selected = 0;
    }
}

// Handle mouse up
void handle_mouse_up(int x, int y) {
    mouseDown = 0;
    dragWindow = 0;

    // Check for double-click on icons
    static Uint32 lastClickTime = 0;
    Uint32 currentTime = SDL_GetTicks();

    if (currentTime - lastClickTime < 500) { // 500ms for double-click
        for (int i = 0; i < desktopIconCount; i++) {
            Icon* icon = &desktopIcons[i];
            if (icon->selected && icon->doubleClickHandler) {
                icon->doubleClickHandler();
                break;
            }
        }
    }

    lastClickTime = currentTime;
}

// Handle mouse motion
void handle_mouse_motion(int x, int y) {
    mouseX = x;
    mouseY = y;

    if (dragWindow && activeWindow) {
        activeWindow->bounds.x = x - dragOffset.x;
        activeWindow->bounds.y = y - dragOffset.y;
    }
}

// Handle events
void handle_events(void) {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                running = 0;
                break;

            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    handle_mouse_down(event.button.x, event.button.y);
                }
                break;

            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    handle_mouse_up(event.button.x, event.button.y);
                }
                break;

            case SDL_MOUSEMOTION:
                handle_mouse_motion(event.motion.x, event.motion.y);
                break;

            case SDL_KEYDOWN:
                if (event.key.keysym.mod & KMOD_CTRL) {
                    switch (event.key.keysym.sym) {
                        case SDLK_q:
                            running = 0;
                            break;
                    }
                }
                break;
        }
    }
}

// Show startup screen
void show_startup_screen(void) {
    SDL_SetRenderDrawColor(renderer, 192, 192, 192, 255);
    SDL_RenderClear(renderer);

    // Draw startup box
    SDL_Rect box = {
        SCREEN_WIDTH/2 - 150,
        SCREEN_HEIGHT/2 - 100,
        300, 200
    };
    draw_rect(&box, COLOR_WHITE, 1);
    draw_rect(&box, COLOR_BLACK, 0);

    // Draw Happy Mac icon (simplified)
    SDL_Rect mac = {
        SCREEN_WIDTH/2 - 32,
        SCREEN_HEIGHT/2 - 60,
        64, 64
    };
    draw_rect(&mac, COLOR_BLACK, 0);

    // Draw smile
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderDrawLine(renderer, mac.x + 20, mac.y + 30, mac.x + 25, mac.y + 30);
    SDL_RenderDrawLine(renderer, mac.x + 40, mac.y + 30, mac.x + 45, mac.y + 30);
    SDL_RenderDrawLine(renderer, mac.x + 20, mac.y + 40, mac.x + 44, mac.y + 40);

    // Draw text
    draw_text("Welcome to Macintosh",
        SCREEN_WIDTH/2 - 70, SCREEN_HEIGHT/2 + 20, COLOR_BLACK);
    draw_text("System 7.1 Portable",
        SCREEN_WIDTH/2 - 65, SCREEN_HEIGHT/2 + 40, COLOR_BLACK);

    SDL_RenderPresent(renderer);
    SDL_Delay(3000);
}

// Main function
int main(int argc, char* argv[]) {
    printf("System 7.1 Portable - Graphical VM\n");
    printf("===================================\n\n");

    // Initialize graphics
    init_graphics();

    // Show startup screen
    show_startup_screen();

    // Create system UI
    create_system_menus();
    create_desktop_icons();

    // Create initial about window
    create_window("Welcome",
        SCREEN_WIDTH/2 - 200, SCREEN_HEIGHT/2 - 100,
        400, 200, WINDOW_DIALOG);

    // Main loop
    while (running) {
        // Handle events
        handle_events();

        // Clear screen
        SDL_SetRenderDrawColor(renderer, 128, 128, 128, 255);
        SDL_RenderClear(renderer);

        // Draw everything
        draw_desktop();
        draw_windows();
        draw_menubar();

        // Present
        SDL_RenderPresent(renderer);

        // Small delay to control framerate
        SDL_Delay(16); // ~60 FPS
    }

    // Cleanup
    cleanup_graphics();

    printf("\nSystem 7.1 Portable shutdown complete.\n");
    return 0;
}