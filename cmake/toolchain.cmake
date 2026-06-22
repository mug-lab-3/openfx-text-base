# Refined Professional LLVM Toolchain
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Target triple that matches MinGW installation prefix
set(TARGET_TRIPLE x86_64-w64-mingw32)

# Compilers
set(CMAKE_C_COMPILER clang)
set(CMAKE_C_COMPILER_TARGET ${TARGET_TRIPLE})
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_CXX_COMPILER_TARGET ${TARGET_TRIPLE})

# LLVM Tools
set(CMAKE_AR llvm-ar)
set(CMAKE_RANLIB llvm-ranlib)
set(CMAKE_LINKER ld.lld)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
set(CMAKE_RC_FLAGS "--include-dir=/usr/x86_64-w64-mingw32/include")

# Adjust find_package behavior
set(CMAKE_FIND_ROOT_PATH /usr/${TARGET_TRIPLE})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Linker flags to use LLD
set(CMAKE_EXE_LINKER_FLAGS_INIT "-fuse-ld=lld")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-fuse-ld=lld")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "-fuse-ld=lld")

# Stability settings
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Platform settings consumed by CMakeLists.txt
set(PLATFORM_COMPILE_OPTIONS "-mavx2;-mfma;-flto=thin"                                                              CACHE INTERNAL "")
set(PLATFORM_LINK_LIBS       ""                                                                                      CACHE INTERNAL "")
set(PLATFORM_LINK_FLAGS      "-flto=thin;-static;-Wl,--export-all-symbols;-lws2_32;-lOpenCL;-lcuda;-ldwrite;-lpthread" CACHE INTERNAL "")
set(PLATFORM_BUNDLE          ON                                                                                      CACHE INTERNAL "")
