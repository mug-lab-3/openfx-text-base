#!/bin/bash
set -e

cd "$(dirname "$0")/.."

DO_CLEAN=false
DO_FULL_CLEAN=false
DO_MAC=false
ENABLE_LOG="false"
ENABLE_TRIAL="false"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --clean)  DO_CLEAN=true; shift ;;
        --all)    DO_FULL_CLEAN=true; shift ;;
        --debug)  ENABLE_LOG="true"; shift ;;
        --trial)  ENABLE_TRIAL="true"; shift ;;
        --mac)    DO_MAC=true; shift ;;
        --help)
            echo "Usage: $0 [--clean] [--all] [--debug] [--trial] [--mac]"
            echo "  --clean   Remove build dir (keep library cache)"
            echo "  --all     Remove entire build dir and ccache"
            echo "  --debug   Enable UDP logging"
            echo "  --trial   Enable trial watermark"
            echo "  --mac     Native macOS build (no Docker)"
            exit 0
            ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

BUILD_WIN="build/win"
BUILD_MAC="build/mac"
IMAGE_NAME="ofx-base-win64-builder"
TARGET_BUILD_DIR="$( [ "$DO_MAC" = true ] && echo "$BUILD_MAC" || echo "$BUILD_WIN" )"

if [ "$DO_FULL_CLEAN" = true ]; then
    echo "Performing FULL clean (removing build/ and .ccache)..."
    rm -rf build .ccache
elif [ "$DO_CLEAN" = true ]; then
    echo "Cleaning $TARGET_BUILD_DIR (keeping library cache)..."
    if [ -d "$TARGET_BUILD_DIR" ]; then
        find "$TARGET_BUILD_DIR" -maxdepth 1 \
            -not -name "cache" \
            -not -name "$(basename "$TARGET_BUILD_DIR")" \
            -not -name "." \
            -exec rm -rf {} +
    fi
fi

if [ "$DO_MAC" = true ]; then
    if [ ! -d ".sdk/ofx-sdk" ]; then
        echo "--- .sdk/ofx-sdk not found. Running setup-editor.sh first... ---"
        if [[ "$(docker images -q $IMAGE_NAME 2>/dev/null)" == "" ]]; then
            echo "Docker image not found. Building $IMAGE_NAME..."
            docker build -t $IMAGE_NAME -f docker/Dockerfile .
        fi
        ./scripts/setup-editor.sh
    fi

    CMAKE_LOG_VAL="OFF";  [ "$ENABLE_LOG"   = "true" ] && CMAKE_LOG_VAL="ON"
    CMAKE_TRIAL_VAL="OFF"; [ "$ENABLE_TRIAL" = "true" ] && CMAKE_TRIAL_VAL="ON"

    echo "Starting native macOS build (Release, UDP Log: $ENABLE_LOG, Trial: $ENABLE_TRIAL)..."
    mkdir -p "$BUILD_MAC"
    cmake -B "$BUILD_MAC" -GNinja \
        --no-warn-unused-cli \
        -DCMAKE_BUILD_TYPE=Release \
        -DENABLE_UDP_LOG=$CMAKE_LOG_VAL \
        -DTRIAL_BUILD=$CMAKE_TRIAL_VAL

    cmake --build "$BUILD_MAC"

    mkdir -p build
    cp "$BUILD_MAC/compile_commands.json" build/compile_commands.json 2>/dev/null || true

    if [ "$ENABLE_LOG" != "true" ] && [ "$ENABLE_TRIAL" != "true" ] && [ "$SKIP_ZIP" != "true" ]; then
        echo "--- Creating Release ZIP for macOS ---"
        VERSION_MAJOR=$(grep "kVersionMajor" src/Version.h | sed -E 's/.*kVersionMajor\s*=\s*([0-9]+).*/\1/')
        VERSION_MINOR=$(grep "kVersionMinor" src/Version.h | sed -E 's/.*kVersionMinor\s*=\s*([0-9]+).*/\1/')
        VERSION_REVISION=$(grep "kVersionRevision" src/Version.h | grep -oE '"[^"]*"' | sed 's/"//g')
        VERSION="${VERSION_MAJOR}.${VERSION_MINOR}${VERSION_REVISION}"

        TEMP_DIR="build/zip_temp"
        rm -rf "$TEMP_DIR"
        mkdir -p "$TEMP_DIR"

        cp 3rdparty-licenses.md "$TEMP_DIR/"
        cp -r "$BUILD_MAC/OfxBase.ofx.bundle" "$TEMP_DIR/"
        cp scripts/install-mac.command "$TEMP_DIR/"

        xattr -cr "$TEMP_DIR"

        if [[ "$(docker images -q $IMAGE_NAME 2>/dev/null)" == "" ]]; then
            echo "Docker image not found. Building $IMAGE_NAME..."
            docker build -t $IMAGE_NAME -f docker/Dockerfile .
        fi

        docker run --rm \
            --user $(id -u):$(id -g) \
            -v "$(pwd):/workspace" \
            -w "/workspace/$TEMP_DIR" \
            $IMAGE_NAME \
            zip -r "../for_mac_${VERSION}.zip" .

        rm -rf "$TEMP_DIR"
        echo "--- Release ZIP Created: build/for_mac_${VERSION}.zip ---"
    fi

    echo ""
    echo "========================================"
    echo "  Build Summary"
    echo "========================================"
    echo "  Platform:    macOS"
    echo "  UDP Logging: $( [ "$ENABLE_LOG"   = "true" ] && echo "Enabled" || echo "Disabled" )"
    echo "  Trial Mode:  $( [ "$ENABLE_TRIAL" = "true" ] && echo "Enabled" || echo "Disabled" )"
    echo "========================================"
    echo "Build complete. Output: $BUILD_MAC/OfxBase.ofx.bundle"
else
    echo "Ensuring Docker image is ready..."
    docker build -t $IMAGE_NAME -f docker/Dockerfile .

    echo "Starting Windows cross-compile build (UDP Log: $ENABLE_LOG, Trial: $ENABLE_TRIAL)..."
    mkdir -p .ccache "$BUILD_WIN"
    docker run --rm \
        --user $(id -u):$(id -g) \
        -v "$(pwd):/workspace" \
        -v "$(pwd)/.ccache:/tmp/.ccache" \
        -e HOME=/tmp \
        -e CCACHE_DIR=/tmp/.ccache \
        -e ENABLE_UDP_LOG=$ENABLE_LOG \
        -e TRIAL_BUILD=$ENABLE_TRIAL \
        $IMAGE_NAME

    if [ -f "$BUILD_WIN/compile_commands.json" ]; then
        echo "Fixing paths in compile_commands.json for host editor..."
        # BSD sed (macOS) requires an explicit backup suffix arg for -i ('' = no backup);
        # GNU sed (Linux) does not. Branch on the OS like the upstream build script.
        if [[ "$(uname)" == "Darwin" ]]; then
            sed -i '' "s|/opt/ofx-sdk|$(pwd)/.sdk/ofx-sdk|g" "$BUILD_WIN/compile_commands.json"
            sed -i '' "s|/opt/3rdparty|$(pwd)/.sdk|g" "$BUILD_WIN/compile_commands.json"
            sed -i '' "s|/workspace|$(pwd)|g" "$BUILD_WIN/compile_commands.json"
            sed -i '' "s|/usr/bin/clang++|clang++|g" "$BUILD_WIN/compile_commands.json"
            sed -i '' "s|--target=x86_64-w64-mingw32|--target=x86_64-w64-mingw32 -isystem $(pwd)/.sdk/mingw/include/c++ -isystem $(pwd)/.sdk/mingw/lib/clang/include -isystem $(pwd)/.sdk/mingw/include|g" "$BUILD_WIN/compile_commands.json"
        else
            sed -i "s|/opt/ofx-sdk|$(pwd)/.sdk/ofx-sdk|g" "$BUILD_WIN/compile_commands.json"
            sed -i "s|/opt/3rdparty|$(pwd)/.sdk|g" "$BUILD_WIN/compile_commands.json"
            sed -i "s|/workspace|$(pwd)|g" "$BUILD_WIN/compile_commands.json"
            sed -i "s|/usr/bin/clang++|clang++|g" "$BUILD_WIN/compile_commands.json"
            sed -i "s|--target=x86_64-w64-mingw32|--target=x86_64-w64-mingw32 -isystem $(pwd)/.sdk/mingw/include/c++ -isystem $(pwd)/.sdk/mingw/lib/clang/include -isystem $(pwd)/.sdk/mingw/include|g" "$BUILD_WIN/compile_commands.json"
        fi
        cp "$BUILD_WIN/compile_commands.json" build/compile_commands.json
    fi

    echo ""
    echo "========================================"
    echo "  Build Summary"
    echo "========================================"
    echo "  Platform:    Windows"
    echo "  UDP Logging: $( [ "$ENABLE_LOG"   = "true" ] && echo "Enabled" || echo "Disabled" )"
    echo "  Trial Mode:  $( [ "$ENABLE_TRIAL" = "true" ] && echo "Enabled" || echo "Disabled" )"
    echo "========================================"
    echo "Build complete. Output: $BUILD_WIN/OfxBase.ofx.bundle"
fi
