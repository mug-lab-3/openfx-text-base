#!/bin/bash
set -e

# 引数がある場合は、そのコマンドを実行
if [ $# -gt 0 ]; then
    exec "$@"
fi

BUILD_DIR="build/win"
BUILD_TYPE="Release"
ENABLE_LOG="OFF"
TRIAL_BUILD_VAL="OFF"

if [ "$ENABLE_UDP_LOG" = "true" ]; then
    ENABLE_LOG="ON"
fi

if [ "$TRIAL_BUILD" = "true" ]; then
    TRIAL_BUILD_VAL="ON"
fi

echo "--- Starting OpenFX Build ($BUILD_TYPE, UDP Log: $ENABLE_LOG, Trial: $TRIAL_BUILD_VAL) ---"

mkdir -p $BUILD_DIR
cmake -B $BUILD_DIR -GNinja \
      -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain.cmake \
      -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
      -DENABLE_UDP_LOG=$ENABLE_LOG \
      -DTRIAL_BUILD=$TRIAL_BUILD_VAL

cmake --build $BUILD_DIR

echo "--- Build Finished: $BUILD_DIR/OfxBase.ofx ---"

if [ "$ENABLE_UDP_LOG" != "true" ] && [ "$SKIP_ZIP" != "true" ]; then
    echo "--- Creating Release ZIP for Windows ---"
    VERSION_MAJOR=$(grep "kVersionMajor" src/Version.h | sed -E 's/.*kVersionMajor\s*=\s*([0-9]+).*/\1/')
    VERSION_MINOR=$(grep "kVersionMinor" src/Version.h | sed -E 's/.*kVersionMinor\s*=\s*([0-9]+).*/\1/')
    VERSION_REVISION=$(grep "kVersionRevision" src/Version.h | grep -oE '"[^"]*"' | sed 's/"//g')
    VERSION="${VERSION_MAJOR}.${VERSION_MINOR}${VERSION_REVISION}"

    TEMP_DIR="build/zip_temp"
    rm -rf "$TEMP_DIR"
    mkdir -p "$TEMP_DIR"

    cp 3rdparty-licenses.md "$TEMP_DIR/"
    cp -r "$BUILD_DIR/OfxBase.ofx.bundle" "$TEMP_DIR/"
    cp scripts/install-win.bat "$TEMP_DIR/"

    (cd "$TEMP_DIR" && zip -r "../for_windows_${VERSION}.zip" *)
    rm -rf "$TEMP_DIR"
    echo "--- Release ZIP Created: build/for_windows_${VERSION}.zip ---"
fi
