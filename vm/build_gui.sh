#!/bin/bash
#
# System 7.1 Portable - Build Graphical VM
#
# This script builds the graphical version with SDL2

set -e

echo "==========================================="
echo "System 7.1 Portable - Graphical VM Builder"
echo "==========================================="

OUTPUT_DIR="vm/output"
mkdir -p ${OUTPUT_DIR}

# Check for SDL2
echo ""
echo "Checking dependencies..."

if ! pkg-config --exists sdl2; then
    echo "SDL2 not found. Installing..."
    if command -v apt-get &> /dev/null; then
        sudo apt-get update
        sudo apt-get install -y libsdl2-dev libsdl2-ttf-dev
    elif command -v brew &> /dev/null; then
        brew install sdl2 sdl2_ttf
    else
        echo "Please install SDL2 and SDL2_ttf development packages"
        exit 1
    fi
fi

# Build the graphical version
echo ""
echo "Building graphical System 7.1..."

gcc -o ${OUTPUT_DIR}/system71_gui \
    vm/system71_gui.c \
    `pkg-config --cflags --libs sdl2` \
    -lSDL2_ttf \
    -lm \
    -Wall -O2 \
    2>/dev/null || {
        echo "Build failed. Trying without SDL_ttf..."
        gcc -o ${OUTPUT_DIR}/system71_gui \
            vm/system71_gui.c \
            `pkg-config --cflags --libs sdl2` \
            -lm \
            -Wall -O2 \
            -DNO_TTF
    }

echo "Build complete: ${OUTPUT_DIR}/system71_gui"

# Create run script
cat > ${OUTPUT_DIR}/run_gui.sh << 'EOF'
#!/bin/bash
#
# Run System 7.1 Portable Graphical VM

echo "Starting System 7.1 Portable (Graphical)..."
echo "=========================================="
echo ""
echo "Controls:"
echo "  - Click and drag windows"
echo "  - Double-click icons to open"
echo "  - Click menus to browse"
echo "  - Ctrl+Q to quit"
echo ""

# Check if running over SSH
if [ -n "$SSH_CONNECTION" ]; then
    echo "Note: Running over SSH. Setting display..."
    export DISPLAY=:0
fi

# Run the GUI
./system71_gui
EOF

chmod +x ${OUTPUT_DIR}/run_gui.sh

echo ""
echo "==========================================="
echo "Build Complete!"
echo "==========================================="
echo ""
echo "To run the graphical System 7.1:"
echo "  cd ${OUTPUT_DIR}"
echo "  ./run_gui.sh"
echo ""
echo "Or directly:"
echo "  ${OUTPUT_DIR}/system71_gui"
echo ""