#!/bin/bash
set -e

# Ensure we are in the repository root
cd "$(dirname "$0")/.."

IMAGE_NAME="ofx-base-win64-builder"
SDK_DIR=".sdk"

# Install clang-format on host if missing
if ! command -v clang-format &> /dev/null; then
    echo "=== Installing clang-format ==="
    if [[ "$(uname)" == "Darwin" ]]; then
        brew install clang-format
    else
        sudo apt-get update && sudo apt-get install -y clang-format
    fi
fi

echo "=== Syncing SDK headers for Editor IntelliSense ==="

# Create local SDK directory
mkdir -p $SDK_DIR

# Run a temporary container to copy headers out
# We use a temp container to avoid permission issues
echo "Copying headers from container..."

copy_dir() {
    echo "mkdir -p /mnt/$2 && cd $1 && tar -h --exclude='.git' -cf - . | tar -xf - -C /mnt/$2"
}

COPY_CMD="
    $(copy_dir /opt/ofx-sdk ofx-sdk)
    $(copy_dir /opt/3rdparty/blend2d blend2d)
    $(copy_dir /opt/3rdparty/asmjit asmjit)
    $(copy_dir /opt/3rdparty/utf8cpp utf8cpp)
    $(copy_dir /opt/3rdparty/glaze glaze)
    $(copy_dir /opt/3rdparty/freetype freetype)
    $(copy_dir /opt/3rdparty/harfbuzz harfbuzz)
    $(copy_dir /usr/share/mingw-w64/include mingw/include)
    $(copy_dir /usr/lib/gcc/x86_64-w64-mingw32/13-win32/include/c++ mingw/include/c++)
    $(copy_dir /usr/lib/llvm-18/lib/clang/18/include mingw/lib/clang/include)
"
docker run --rm \
    --user "$(id -u):$(id -g)" \
    -v "$(pwd)/$SDK_DIR:/mnt" $IMAGE_NAME bash -c "$COPY_CMD"

echo "=== Sync Complete! ==="
echo "Headers are now available in: $(pwd)/$SDK_DIR"
echo ""
echo "Note: If you use VS Code, add these paths to your includePath."
echo "Or, your editor may now pick up the headers via compile_commands.json."
