# Default Build Configuration
# This is the standard development configuration

# Feature flags
ENABLE_RESOURCES ?= 1
ENABLE_GESTALT ?= 1
ENABLE_SCRAP ?= 1
ENABLE_LIST ?= 1
MODERN_INPUT_ONLY ?= 1
GESTALT_MACHINE_TYPE ?= 0
BEZEL_STYLE ?= rounded

# Test/smoke test flags (disabled by default)
CTRL_SMOKE_TEST ?= 0
LIST_SMOKE_TEST ?= 0
ALERT_SMOKE_TEST ?= 0

# Optimization and debug settings
OPT_LEVEL ?= 1
DEBUG_SYMBOLS ?= 1

# Pattern resource path
PATTERN_RESOURCE ?= resources/patterns_authentic_color.json
