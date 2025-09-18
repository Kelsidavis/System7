/*
 * Embedded Font System Test
 * Tests the complete integrated font system with embedded fallbacks
 */

#include "../../include/FontResources/SystemFonts.h"
#include "EmbeddedFonts.c" // Include implementation directly for testing
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("=== System 7.1 Embedded Font System Test ===\n");

    /* Test the embedded font system */
    return TestEmbeddedFonts();
}