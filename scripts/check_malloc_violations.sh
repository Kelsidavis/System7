#!/bin/bash
#
# Check for malloc/free/calloc/realloc violations in source files
# Returns 0 if clean, 1 if violations found
#

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

VIOLATIONS=0

echo "Checking for malloc/free violations in source code..."

# Find all .c files except MemoryManager.c
C_FILES=$(find src -name "*.c" ! -path "*/MemoryMgr/MemoryManager.c")

for FILE in $C_FILES; do
    # Remove comments and blank lines for checking (preserve line numbers with grep -v)
    # This avoids false positives from comments containing "free" or "malloc"

    # Check for malloc (excluding comments)
    if grep -v '^\s*//' "$FILE" | grep -v '^\s*/\*' | grep -v '^\s*\*' | grep '\bmalloc\s*(' > /dev/null 2>&1; then
        echo -e "${RED}VIOLATION${NC}: $FILE uses malloc()"
        VIOLATIONS=$((VIOLATIONS + 1))
    fi

    # Check for free (excluding comments and the word "free" in comments)
    # Only match "free(" to avoid matching "free" in documentation
    if grep -v '^\s*//' "$FILE" | grep -v '^\s*/\*' | grep -v '^\s*\*' | grep '\bfree\s*(' > /dev/null 2>&1; then
        echo -e "${RED}VIOLATION${NC}: $FILE uses free()"
        VIOLATIONS=$((VIOLATIONS + 1))
    fi

    # Check for calloc (excluding comments)
    if grep -v '^\s*//' "$FILE" | grep -v '^\s*/\*' | grep -v '^\s*\*' | grep '\bcalloc\s*(' > /dev/null 2>&1; then
        echo -e "${RED}VIOLATION${NC}: $FILE uses calloc()"
        VIOLATIONS=$((VIOLATIONS + 1))
    fi

    # Check for realloc (excluding comment-only lines)
    if grep -v '^\s*//' "$FILE" | grep -v '^\s*/\*' | grep -v '^\s*\*' | grep '\brealloc\s*(' > /dev/null 2>&1; then
        echo -e "${RED}VIOLATION${NC}: $FILE uses realloc()"
        VIOLATIONS=$((VIOLATIONS + 1))
    fi
done

if [ $VIOLATIONS -gt 0 ]; then
    echo ""
    echo -e "${RED}Found $VIOLATIONS malloc/free violations!${NC}"
    echo "Use Memory Manager instead: NewPtr/DisposePtr/NewPtrClear"
    echo "See docs/MEMORY_MANAGEMENT.md for guidelines"
    exit 1
else
    echo -e "${GREEN}✓ No malloc/free violations found${NC}"
    exit 0
fi
