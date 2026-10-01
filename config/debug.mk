# Debug Build Configuration
# Unoptimised, with symbols and the self-tests

# Include default settings
include config/default.mk

# Override for debugging
OPT_LEVEL = 0
DEBUG_SYMBOLS = 1

# Enable self-tests
CFLAGS += -DSCRAP_SELFTEST=1 -DDEBUG_DOUBLECLICK=1
