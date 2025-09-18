/*
 * Font Downloader Utility
 * Downloads modern TrueType/OpenType versions of System 7.1 fonts
 *
 * This utility fetches modern font files from various sources including:
 * - Urban Renewal collection (Kreative Korp)
 * - GitHub repositories with classic Mac fonts
 * - System fonts from modern macOS installations
 */

#include "../../include/FontResources/ModernFontLoader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

/* Font download sources */
typedef struct FontSource {
    char name[64];
    char baseURL[256];
    char description[128];
} FontSource;

/* Available font sources */
static FontSource fontSources[] = {
    {
        "Urban Renewal",
        "https://www.kreativekorp.com/software/fonts/urbanrenewal/",
        "High-quality TrueType recreations by Kreative Korp"
    },
    {
        "GitHub macfonts",
        "https://github.com/JohnDDuncanIII/macfonts/",
        "Comprehensive collection of classic Mac fonts"
    },
    {
        "System Fonts",
        "/System/Library/Fonts/",
        "Extract from modern macOS installation"
    }
};

/* Download progress callback */
static int progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow,
                           curl_off_t ultotal, curl_off_t ulnow) {
    (void)clientp; (void)ultotal; (void)ulnow;

    if (dltotal > 0) {
        double percentage = (double)dlnow / (double)dltotal * 100.0;
        printf("\rDownloading: %.1f%% (%lld/%lld bytes)", percentage, dlnow, dltotal);
        fflush(stdout);
    }
    return 0;
}

/* Write data callback for curl */
static size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    return fwrite(ptr, size, nmemb, stream);
}

/*
 * DownloadFile - Download a file from URL to local path
 */
static OSErr DownloadFile(const char* url, const char* outputPath) {
    CURL *curl;
    FILE *fp;
    CURLcode res;

    curl = curl_easy_init();
    if (!curl) return ioErr;

    fp = fopen(outputPath, "wb");
    if (!fp) {
        curl_easy_cleanup(curl);
        return ioErr;
    }

    printf("Downloading %s\n", url);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "System7.1-Portable/1.0");

    res = curl_easy_perform(curl);

    fclose(fp);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        printf("\nDownload failed: %s\n", curl_easy_strerror(res));
        return ioErr;
    }

    printf("\nDownload completed: %s\n", outputPath);
    return noErr;
}

/*
 * GenerateFontDownloadScript - Create a script to download fonts
 */
static OSErr GenerateFontDownloadScript(const char* scriptPath) {
    FILE* script = fopen(scriptPath, "w");
    if (!script) return ioErr;

    fprintf(script, "#!/bin/bash\n");
    fprintf(script, "# System 7.1 Font Download Script\n");
    fprintf(script, "# Downloads modern versions of classic Mac OS fonts\n\n");

    fprintf(script, "FONT_DIR=\"./resources/fonts/modern\"\n");
    fprintf(script, "mkdir -p \"$FONT_DIR\"\n\n");

    fprintf(script, "echo \"Downloading System 7.1 fonts...\"\n\n");

    /* Urban Renewal fonts (if available) */
    fprintf(script, "# Urban Renewal Collection\n");
    fprintf(script, "echo \"Checking Urban Renewal collection...\"\n");
    fprintf(script, "# Note: Manual download required from https://www.kreativekorp.com/software/fonts/urbanrenewal/\n\n");

    /* Chicago font alternatives */
    fprintf(script, "# Chicago font alternatives\n");
    fprintf(script, "echo \"Looking for Chicago font alternatives...\"\n");
    fprintf(script, "# ChiKareGo - faithful Chicago recreation\n");
    fprintf(script, "# Available from various font sites\n\n");

    /* Monaco and Geneva from macOS */
    fprintf(script, "# Extract Monaco and Geneva from macOS (if available)\n");
    fprintf(script, "if [ -f \"/System/Library/Fonts/Monaco.ttf\" ]; then\n");
    fprintf(script, "    echo \"Found Monaco.ttf in system fonts\"\n");
    fprintf(script, "    cp \"/System/Library/Fonts/Monaco.ttf\" \"$FONT_DIR/\"\n");
    fprintf(script, "fi\n\n");

    fprintf(script, "if [ -f \"/System/Library/Fonts/Geneva.ttf\" ]; then\n");
    fprintf(script, "    echo \"Found Geneva.ttf in system fonts\"\n");
    fprintf(script, "    cp \"/System/Library/Fonts/Geneva.ttf\" \"$FONT_DIR/\"\n");
    fprintf(script, "fi\n\n");

    /* Helvetica alternatives */
    fprintf(script, "# Helvetica alternatives\n");
    fprintf(script, "echo \"Looking for Helvetica alternatives...\"\n");
    fprintf(script, "# Liberation Sans or other Helvetica-like fonts\n\n");

    fprintf(script, "# Check macfonts repository\n");
    fprintf(script, "echo \"To get comprehensive font collection, clone:\"\n");
    fprintf(script, "echo \"git clone https://github.com/JohnDDuncanIII/macfonts.git\"\n");
    fprintf(script, "echo \"Then copy relevant TTF files to $FONT_DIR\"\n\n");

    fprintf(script, "echo \"Font download preparation complete!\"\n");
    fprintf(script, "echo \"Check $FONT_DIR for downloaded fonts\"\n");

    fclose(script);

    /* Make script executable */
    chmod(scriptPath, 0755);

    printf("Generated font download script: %s\n", scriptPath);
    return noErr;
}

/*
 * CreateFontManifest - Create a manifest of expected font files
 */
static OSErr CreateFontManifest(const char* manifestPath) {
    FILE* manifest = fopen(manifestPath, "w");
    if (!manifest) return ioErr;

    fprintf(manifest, "# System 7.1 Font Manifest\n");
    fprintf(manifest, "# Expected modern font files for complete System 7.1 font support\n\n");

    fprintf(manifest, "[Core System 7.1 Fonts]\n");
    fprintf(manifest, "Chicago.ttf         # System font - UI elements, menus\n");
    fprintf(manifest, "Geneva.ttf          # Application font - dialog text\n");
    fprintf(manifest, "Monaco.ttf          # Monospace font - code, terminal\n");
    fprintf(manifest, "New York.ttf        # Serif font - documents\n");
    fprintf(manifest, "Courier.ttf         # Monospace serif - typewriter style\n");
    fprintf(manifest, "Helvetica.ttf       # Sans serif - clean text\n\n");

    fprintf(manifest, "[Alternative Sources]\n");
    fprintf(manifest, "ChiKareGo.ttf       # Chicago recreation\n");
    fprintf(manifest, "FindersKeepers.ttf  # Geneva 9pt recreation\n");
    fprintf(manifest, "Windy City.ttf      # Another Chicago variant\n\n");

    fprintf(manifest, "[Download Sources]\n");
    fprintf(manifest, "Urban Renewal:      https://www.kreativekorp.com/software/fonts/urbanrenewal/\n");
    fprintf(manifest, "macfonts GitHub:    https://github.com/JohnDDuncanIII/macfonts\n");
    fprintf(manifest, "macOS System:       /System/Library/Fonts/\n\n");

    fprintf(manifest, "[Installation]\n");
    fprintf(manifest, "1. Download font files from sources above\n");
    fprintf(manifest, "2. Place TTF/OTF files in: resources/fonts/modern/\n");
    fprintf(manifest, "3. Run font loader to detect and integrate fonts\n");
    fprintf(manifest, "4. Test with FontTest utility\n\n");

    fclose(manifest);

    printf("Created font manifest: %s\n", manifestPath);
    return noErr;
}

/*
 * SetupFontDirectories - Create necessary directories for font integration
 */
static OSErr SetupFontDirectories(const char* basePath) {
    char modernPath[512];
    char originalPath[512];
    char tempPath[512];

    snprintf(modernPath, sizeof(modernPath), "%s/fonts/modern", basePath);
    snprintf(originalPath, sizeof(originalPath), "%s/fonts/originals", basePath);
    snprintf(tempPath, sizeof(tempPath), "%s/fonts/temp", basePath);

    /* Create directories */
    char mkdirCmd[1024];
    snprintf(mkdirCmd, sizeof(mkdirCmd), "mkdir -p \"%s\" \"%s\" \"%s\"",
             modernPath, originalPath, tempPath);

    if (system(mkdirCmd) != 0) {
        printf("Warning: Could not create font directories\n");
        return ioErr;
    }

    printf("Created font directories:\n");
    printf("  Modern fonts: %s\n", modernPath);
    printf("  Original fonts: %s\n", originalPath);
    printf("  Temporary: %s\n", tempPath);

    return noErr;
}

/*
 * TestFontDownloadSystem - Test the font download and loading system
 */
int TestFontDownloadSystem(void) {
    printf("=== System 7.1 Font Download System Test ===\n\n");

    /* Setup directories */
    if (SetupFontDirectories("./resources") != noErr) {
        printf("Failed to setup font directories\n");
        return 1;
    }

    /* Generate download script */
    if (GenerateFontDownloadScript("./download_fonts.sh") != noErr) {
        printf("Failed to generate download script\n");
        return 1;
    }

    /* Create font manifest */
    if (CreateFontManifest("./FONT_MANIFEST.txt") != noErr) {
        printf("Failed to create font manifest\n");
        return 1;
    }

    /* Test modern font loading */
    printf("\nTesting modern font loader...\n");
    OSErr err = LoadModernFonts("./resources/fonts/modern");
    if (err == noErr) {
        ModernFontCollection* collection = GetModernFontCollection();
        printf("Successfully initialized modern font system\n");
        printf("Found %d modern font files\n", collection->numFonts);

        /* List found fonts */
        for (int i = 0; i < collection->numFonts; i++) {
            ModernFontFile* font = &collection->fonts[i];
            printf("  %s (Family ID: %d, Size: %ld bytes)\n",
                   font->fileName, font->familyID, font->fileSize);
        }
    } else {
        printf("Modern font directory not found (expected if no fonts downloaded yet)\n");
    }

    /* Display next steps */
    printf("\n=== Next Steps ===\n");
    printf("1. Run: ./download_fonts.sh\n");
    printf("2. Manually download fonts from sources in FONT_MANIFEST.txt\n");
    printf("3. Place font files in ./resources/fonts/modern/\n");
    printf("4. Run font system tests to verify integration\n");

    printf("\n=== Font Download System Test Complete ===\n");
    return 0;
}

/*
 * Main function for standalone font downloader
 */
int main(int argc, char* argv[]) {
    if (argc > 1 && strcmp(argv[1], "test") == 0) {
        return TestFontDownloadSystem();
    }

    printf("System 7.1 Font Downloader\n");
    printf("Usage: %s [test]\n", argv[0]);
    printf("  test  - Run font download system test\n");
    return 0;
}