set(CMAKE_OSX_DEPLOYMENT_TARGET "13.3" CACHE STRING "Minimum macOS deployment target")
# Override for universal binary: cmake -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"

# Platform settings consumed by CMakeLists.txt
set(PLATFORM_COMPILE_OPTIONS "-mcpu=apple-m1;-flto=thin" CACHE INTERNAL "")
set(PLATFORM_LINK_LIBS       ""                          CACHE INTERNAL "")
set(PLATFORM_LINK_FLAGS      "-flto=thin"               CACHE INTERNAL "")
set(PLATFORM_BUNDLE          ON                          CACHE INTERNAL "")
