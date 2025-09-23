/* All stubs in one file to avoid duplicates */
#include "../include/SystemTypes.h"

/* Essential string functions */
void* memcpy(void* dest, const void* src, size_t n) {
    unsigned char* d = dest;
    const unsigned char* s = src;
    while (n--) *d++ = *s++;
    return dest;
}

void* memset(void* s, int c, size_t n) {
    unsigned char* p = s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

void* memmove(void* dest, const void* src, size_t n) {
    return memcpy(dest, src, n);
}

int memcmp(const void* s1, const void* s2, size_t n) {
    const unsigned char* p1 = s1;
    const unsigned char* p2 = s2;
    while (n--) {
        if (*p1 != *p2) return *p1 - *p2;
        p1++; p2++;
    }
    return 0;
}

size_t strlen(const char* s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++));
    return dest;
}

char* strncpy(char* dest, const char* src, size_t n) {
    char* d = dest;
    while (n-- && (*d++ = *src++));
    while (n--) *d++ = 0;
    return dest;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && *s1 == *s2) { s1++; s2++; }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

int strncmp(const char* s1, const char* s2, size_t n) {
    while (n-- && *s1 && *s1 == *s2) { s1++; s2++; }
    return n == (size_t)-1 ? 0 : *(unsigned char*)s1 - *(unsigned char*)s2;
}

char* strcat(char* dest, const char* src) {
    char* d = dest;
    while (*d) d++;
    while ((*d++ = *src++));
    return dest;
}

char* strchr(const char* s, int c) {
    while (*s) {
        if (*s == c) return (char*)s;
        s++;
    }
    return NULL;
}

char* strrchr(const char* s, int c) {
    const char* last = NULL;
    while (*s) {
        if (*s == c) last = s;
        s++;
    }
    return (char*)last;
}

char* strstr(const char* haystack, const char* needle) {
    if (!*needle) return (char*)haystack;
    while (*haystack) {
        const char* h = haystack;
        const char* n = needle;
        while (*h && *n && *h == *n) { h++; n++; }
        if (!*n) return (char*)haystack;
        haystack++;
    }
    return NULL;
}

int atoi(const char* str) {
    int result = 0;
    int sign = 1;
    if (*str == '-') { sign = -1; str++; }
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    return result * sign;
}

long atol(const char* str) {
    return (long)atoi(str);
}

int isdigit(int c) { return c >= '0' && c <= '9'; }
int isalpha(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
int isalnum(int c) { return isdigit(c) || isalpha(c); }
int isspace(int c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
int toupper(int c) { return (c >= 'a' && c <= 'z') ? c - 32 : c; }
int tolower(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

void* malloc(size_t size) { return NULL; }
void free(void* ptr) { }
void* calloc(size_t nmemb, size_t size) { return NULL; }
void* realloc(void* ptr, size_t size) { return NULL; }

void abort(void) { while(1); }
void exit(int status) { while(1); }

int printf(const char* format, ...) { return 0; }
int sprintf(char* str, const char* format, ...) { return 0; }
int snprintf(char* str, size_t size, const char* format, ...) { return 0; }

/* File I/O stubs */
FILE* stderr = (FILE*)0;
int fprintf(FILE* stream, const char* format, ...) { return 0; }
FILE* fopen(const char* filename, const char* mode) { return NULL; }
size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream) { return 0; }
int fclose(FILE* stream) { return 0; }
int fseek(FILE* stream, long offset, int whence) { return 0; }

/* System functions */
long sysconf(int name) { return 1024; }

/* Assert function */
void __assert_fail(const char *assertion, const char *file, unsigned int line, const char *function) {
    while (1) {
        __asm__ volatile("cli; hlt");
    }
}

/* Math functions */
double sin(double x) { return 0.0; }
double cos(double x) { return 1.0; }
double sqrt(double x) { return 0.0; }
double atan2(double y, double x) { return 0.0; }

/* GCC helper functions */
long long __divdi3(long long a, long long b) {
    if (b == 0) return 0;
    return a / b;
}

/* All QuickDraw/Window/Menu/etc stubs */
void MoveTo(short x, short y) {}
void LineTo(short x, short y) {}
void DrawString(const char* str) {}
void DrawText(const char* text, short offset, short length) {}
void TextFont(short font) {}
void TextFace(short face) {}
void TextMode(short mode) {}
void TextSize(short size) {}
void GetFontInfo(void* info) {}
void SetRect(Rect* r, short left, short top, short right, short bottom) {}
void OffsetRect(Rect* r, short dh, short dv) {}
void InsetRect(Rect* r, short dh, short dv) {}
Boolean EmptyRect(const Rect* r) { return true; }
Boolean PtInRect(Point pt, const Rect* r) { return false; }
void FrameRect(const Rect* r) {}
void PaintRect(const Rect* r) {}
void EraseRect(const Rect* r) {}
void InvertRect(const Rect* r) {}
void FillRect(const Rect* r, Pattern* pat) {}
RgnHandle NewRgn(void) { return NULL; }
void DisposeRgn(RgnHandle rgn) {}
void CopyRgn(RgnHandle srcRgn, RgnHandle dstRgn) {}
void SetEmptyRgn(RgnHandle rgn) {}
void RectRgn(RgnHandle rgn, const Rect* r) {}
void OpenRgn(void) {}
void CloseRgn(RgnHandle rgn) {}
void OffsetRgn(RgnHandle rgn, short dh, short dv) {}
Boolean EmptyRgn(RgnHandle rgn) { return true; }
Boolean PtInRgn(Point pt, RgnHandle rgn) { return false; }
void InitCursor(void) {}
void SetCursor(const Cursor* cursor) {}
void HideCursor(void) {}
void ShowCursor(void) {}
void ObscureCursor(void) {}
PicHandle OpenPicture(const Rect* picFrame) { return NULL; }
void ClosePicture(void) {}
void DrawPicture(PicHandle pic, const Rect* dstRect) {}
void KillPicture(PicHandle pic) {}
void InitPort(GrafPtr port) {}
void OpenPort(GrafPtr port) {}
void ClosePort(GrafPtr port) {}
void SetPort(GrafPtr port) {}
void GetPort(GrafPtr* port) {}
void InitGraf(void* port) {}
void SetOrigin(short h, short v) {}
void SetClip(RgnHandle rgn) {}
void GetClip(RgnHandle rgn) {}
void ClipRect(const Rect* r) {}
void CopyBits(const BitMap* srcBits, const BitMap* dstBits, const Rect* srcRect, const Rect* dstRect, short mode, RgnHandle maskRgn) {}
void ScrollRect(const Rect* r, short dh, short dv, RgnHandle updateRgn) {}

/* QuickDraw globals */
GrafPtr qd_thePort = NULL;
Pattern qd_white = {{0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}};
Pattern qd_black = {{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}};
Pattern qd_gray = {{0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55}};
Pattern qd_ltGray = {{0x88,0x22,0x88,0x22,0x88,0x22,0x88,0x22}};
Pattern qd_dkGray = {{0x77,0xDD,0x77,0xDD,0x77,0xDD,0x77,0xDD}};
Cursor qd_arrow = {0};
BitMap qd_screenBits = {0};
long qd_randSeed = 1;

/* Memory Manager */
Handle NewHandle(Size size) { return NULL; }
void DisposeHandle(Handle h) {}

/* Resource Manager */
Handle GetResource(ResType type, short id) { return NULL; }
void ReleaseResource(Handle resource) {}
Handle Get1Resource(ResType type, short id) { return NULL; }
short OpenResFile(const unsigned char* fileName) { return -1; }
void CloseResFile(short refNum) {}
void UseResFile(short refNum) {}
short CurResFile(void) { return 0; }
short HomeResFile(Handle resource) { return 0; }
void SetResLoad(Boolean load) {}
short CountResources(ResType type) { return 0; }
short Count1Resources(ResType type) { return 0; }
Handle GetIndResource(ResType type, short index) { return NULL; }
Handle Get1IndResource(ResType type, short index) { return NULL; }
void GetResInfo(Handle resource, short* id, ResType* type, unsigned char* name) {}
void SetResInfo(Handle resource, short id, const unsigned char* name) {}
void AddResource(Handle data, ResType type, short id, const unsigned char* name) {}
void WriteResource(Handle resource) {}
void RemoveResource(Handle resource) {}
void UpdateResFile(short refNum) {}
void SetResPurge(Boolean install) {}
short GetResFileAttrs(short refNum) { return 0; }
void SetResFileAttrs(short refNum, short attrs) {}
OSErr ResError(void) { return 0; }

/* Event Manager */
/* DISABLED - Using real Event Manager implementation
/* DISABLED - GetNextEvent now provided by EventManager/event_manager.c */
/* Boolean GetNextEvent(short eventMask, EventRecord* event) { return false; } */
Boolean WaitNextEvent(short eventMask, EventRecord* event, unsigned long sleep, RgnHandle mouseRgn) { return false; }
/* DISABLED - EventAvail now provided by EventManager/event_manager.c */
/* Boolean EventAvail(short eventMask, EventRecord* event) { return false; } */
*/
void GetMouse(Point* mouseLoc) {}
Boolean Button(void) { return false; }
Boolean StillDown(void) { return false; }
Boolean WaitMouseUp(void) { return false; }
unsigned long TickCount(void) { return 0; }
void GetKeys(void* keys) {}

/* Other managers - completely stubbed */
void InitFonts(void) {}
void InitWindows(void) {}
void InitMenus(void) {}
void TEInit(void) {}
void InitDialogs(void* proc) {}
void SysBeep(short duration) {}
void ExitToShell(void) { while(1); }
void InitApplZone(void) {}
void MaxApplZone(void) {}
void MoreMasters(void) {}
short Random(void) { return 0; }
void BlockMove(const void* src, void* dst, Size count) {}
void BlockMoveData(const void* src, void* dst, Size count) {}

/* QuickDraw platform stubs */
void QDPlatform_Initialize(void) {}
void QDPlatform_SetPixel(void* port, int x, int y, unsigned long color) {}
unsigned long QDPlatform_GetPixel(void* port, int x, int y) { return 0; }
void QDPlatform_DrawLine(void* port, int x1, int y1, int x2, int y2) {}
void QDPlatform_DrawRegion(void* port, void* rgn) {}
void QDPlatform_DrawShape(void* port, int shape, void* rect) {}

/* Global pointers */
void* g_currentCPort = NULL;
void* g_currentPort = NULL;

/* ExpandMem functions */
OSErr ExpandMemInit(void) { return 0; }
void ExpandMemInitKeyboard(void) {}
void ExpandMemSetAppleTalkInactive(void) {}
void SetAutoDecompression(Boolean enable) {}
void ResourceManager_SetDecompressionCacheSize(UInt32 size) {}
void InstallDecompressHook(void) {}
void ExpandMemInstallDecompressor(void) {}
void ExpandMemCleanup(void) {}
void ExpandMemDump(void) {}
OSErr ExpandMemValidate(void) { return 0; }

/* Serial output */
int serial_printf(const char* format, ...) { return 0; }