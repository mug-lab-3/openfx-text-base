cmake_minimum_required(VERSION 3.10)

# Native macOS settings (when not cross-compiling via toolchain file)
if(APPLE AND NOT CMAKE_TOOLCHAIN_FILE)
    include(cmake/mac-settings.cmake)
endif()

project(OfxBase VERSION 1.0.0 LANGUAGES CXX OBJCXX)

# Enable ccache if available
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)

if(PLATFORM_COMPILE_OPTIONS)
    add_compile_options(${PLATFORM_COMPILE_OPTIONS})
endif()

if(PLATFORM_COMPILE_OPTIONS MATCHES "flto")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_compile_options(-flto=thin)
        add_link_options(-flto=thin)
    endif()
endif()

set(SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.sdk")

option(ENABLE_UDP_LOG "Enable UDP Logging" ON)
if(ENABLE_UDP_LOG)
    add_definitions(-DENABLE_UDP_LOG)
endif()

# --- SDK & Dependency Resolution ---

# 1. AsmJit
if(NOT DEFINED ASMJIT_DIR)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/asmjit")
        set(ASMJIT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/asmjit" CACHE PATH "")
    elseif(EXISTS "/opt/3rdparty/asmjit")
        set(ASMJIT_DIR "/opt/3rdparty/asmjit" CACHE PATH "")
    elseif(EXISTS "${SDK_DIR}/asmjit")
        set(ASMJIT_DIR "${SDK_DIR}/asmjit" CACHE PATH "")
    endif()
endif()

# 2. OpenFX SDK
if(NOT DEFINED OFX_SDK_DIR)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/openfx-OFX_Release_1.5.1")
        set(OFX_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/openfx-OFX_Release_1.5.1")
    elseif(EXISTS "/opt/ofx-sdk")
        set(OFX_SDK_DIR "/opt/ofx-sdk")
    elseif(EXISTS "${SDK_DIR}/ofx-sdk")
        set(OFX_SDK_DIR "${SDK_DIR}/ofx-sdk")
    endif()
endif()

# 3. Glaze (JSON - header only)
if(NOT DEFINED GLAZE_DIR)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/glaze/include")
        set(GLAZE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/glaze" CACHE PATH "")
    elseif(EXISTS "/opt/3rdparty/glaze/include")
        set(GLAZE_DIR "/opt/3rdparty/glaze" CACHE PATH "")
    elseif(EXISTS "${SDK_DIR}/glaze/include")
        set(GLAZE_DIR "${SDK_DIR}/glaze" CACHE PATH "")
    endif()
endif()

# 4. utf8cpp (header-only)
if(NOT DEFINED UTF8CPP_SRC_DIR)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/utf8cpp/source")
        set(UTF8CPP_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/utf8cpp" CACHE PATH "")
    elseif(EXISTS "/opt/3rdparty/utf8cpp/source")
        set(UTF8CPP_SRC_DIR "/opt/3rdparty/utf8cpp" CACHE PATH "")
    elseif(EXISTS "${SDK_DIR}/utf8cpp/source")
        set(UTF8CPP_SRC_DIR "${SDK_DIR}/utf8cpp" CACHE PATH "")
    endif()
endif()

# 5. FreeType
if(NOT DEFINED FREETYPE_SRC_DIR)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/freetype")
        set(FREETYPE_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/freetype" CACHE PATH "")
    elseif(EXISTS "/opt/3rdparty/freetype")
        set(FREETYPE_SRC_DIR "/opt/3rdparty/freetype" CACHE PATH "")
    elseif(EXISTS "${SDK_DIR}/freetype")
        set(FREETYPE_SRC_DIR "${SDK_DIR}/freetype" CACHE PATH "")
    endif()
endif()

# 6. HarfBuzz
if(NOT DEFINED HARFBUZZ_SRC_DIR)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/harfbuzz")
        set(HARFBUZZ_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/harfbuzz" CACHE PATH "")
    elseif(EXISTS "/opt/3rdparty/harfbuzz")
        set(HARFBUZZ_SRC_DIR "/opt/3rdparty/harfbuzz" CACHE PATH "")
    elseif(EXISTS "${SDK_DIR}/harfbuzz")
        set(HARFBUZZ_SRC_DIR "${SDK_DIR}/harfbuzz" CACHE PATH "")
    endif()
endif()

# 7. Blend2D
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/blend2d/blend2d")
    set(BLEND2D_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/blend2d")
elseif(EXISTS "/opt/3rdparty/blend2d/blend2d")
    set(BLEND2D_SRC_DIR "/opt/3rdparty/blend2d")
elseif(EXISTS "${SDK_DIR}/blend2d/blend2d")
    set(BLEND2D_SRC_DIR "${SDK_DIR}/blend2d")
endif()

set(OFX_INCLUDE_DIRS
    "${OFX_SDK_DIR}/include"
    "${OFX_SDK_DIR}/Support/include"
)

# --- Library Cache Management ---
set(ARCH_DIR "${CMAKE_SYSTEM_NAME}_${CMAKE_SYSTEM_PROCESSOR}")
set(CACHE_BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}/cache")

set(PREBUILT_OFX_LIB       "${CACHE_BASE_DIR}/ofxsupport/${ARCH_DIR}/libofxsupport.a")
set(PREBUILT_BLEND2D_LIB   "${CACHE_BASE_DIR}/blend2d/${ARCH_DIR}/libblend2d.a")
set(PREBUILT_FREETYPE_LIB  "${CACHE_BASE_DIR}/freetype/${ARCH_DIR}/libfreetype.a")
set(PREBUILT_HARFBUZZ_LIB  "${CACHE_BASE_DIR}/harfbuzz/${ARCH_DIR}/libharfbuzz.a")

message(STATUS "Architecture: ${ARCH_DIR}")
message(STATUS "Cache directory: ${CACHE_BASE_DIR}")

# OFX Support Library
if(EXISTS "${PREBUILT_OFX_LIB}")
    add_library(ofxsupport STATIC IMPORTED)
    set_target_properties(ofxsupport PROPERTIES IMPORTED_LOCATION "${PREBUILT_OFX_LIB}")
    message(STATUS "Using pre-built ofxsupport: ${PREBUILT_OFX_LIB}")
    set(OFX_SUPPORT_BUILT OFF)
else()
    set(OFX_SUPPORT_SOURCES
        "${OFX_SDK_DIR}/Support/Library/ofxsCore.cpp"
        "${OFX_SDK_DIR}/Support/Library/ofxsImageEffect.cpp"
        "${OFX_SDK_DIR}/Support/Library/ofxsInteract.cpp"
        "${OFX_SDK_DIR}/Support/Library/ofxsLog.cpp"
        "${OFX_SDK_DIR}/Support/Library/ofxsMultiThread.cpp"
        "${OFX_SDK_DIR}/Support/Library/ofxsParams.cpp"
        "${OFX_SDK_DIR}/Support/Library/ofxsProperty.cpp"
        "${OFX_SDK_DIR}/Support/Library/ofxsPropertyValidation.cpp"
    )
    add_library(ofxsupport STATIC ${OFX_SUPPORT_SOURCES})
    target_include_directories(ofxsupport PUBLIC ${OFX_INCLUDE_DIRS})
    set_target_properties(ofxsupport PROPERTIES POSITION_INDEPENDENT_CODE ON)
    set(OFX_SUPPORT_BUILT ON)
endif()

# Blend2D
if(EXISTS "${PREBUILT_BLEND2D_LIB}")
    add_library(blend2d STATIC IMPORTED)
    set_target_properties(blend2d PROPERTIES
        IMPORTED_LOCATION "${PREBUILT_BLEND2D_LIB}"
        INTERFACE_COMPILE_DEFINITIONS "BL_STATIC"
    )
    message(STATUS "Using pre-built blend2d: ${PREBUILT_BLEND2D_LIB}")
    set(BLEND2D_BUILT OFF)
else()
    set(BLEND2D_TEST OFF CACHE BOOL "" FORCE)
    set(BLEND2D_SAMPLES OFF CACHE BOOL "" FORCE)
    set(BLEND2D_STATIC ON CACHE BOOL "" FORCE)
    set(BLEND2D_NO_STDCXX OFF CACHE BOOL "" FORCE)

    if(BLEND2D_SRC_DIR)
        add_subdirectory("${BLEND2D_SRC_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/blend2d_build")
        if(TARGET blend2d)
            if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
                target_compile_options(blend2d PRIVATE -Wno-sign-conversion -Wno-conversion)
            endif()
        endif()
    endif()
    set(BLEND2D_BUILT ON)
endif()

# FreeType
if(EXISTS "${PREBUILT_FREETYPE_LIB}")
    add_library(freetype STATIC IMPORTED)
    set_target_properties(freetype PROPERTIES IMPORTED_LOCATION "${PREBUILT_FREETYPE_LIB}")
    message(STATUS "Using pre-built freetype: ${PREBUILT_FREETYPE_LIB}")
    set(FREETYPE_BUILT OFF)
else()
    set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
    set(FT_WITH_HARFBUZZ OFF CACHE BOOL "" FORCE)
    if(FREETYPE_SRC_DIR)
        add_subdirectory("${FREETYPE_SRC_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/freetype_build")
    endif()
    set(FREETYPE_BUILT ON)
endif()

# HarfBuzz
if(EXISTS "${PREBUILT_HARFBUZZ_LIB}")
    add_library(harfbuzz STATIC IMPORTED)
    set_target_properties(harfbuzz PROPERTIES IMPORTED_LOCATION "${PREBUILT_HARFBUZZ_LIB}")
    message(STATUS "Using pre-built harfbuzz: ${PREBUILT_HARFBUZZ_LIB}")
    set(HARFBUZZ_BUILT OFF)
else()
    set(HB_HAVE_FREETYPE ON CACHE BOOL "" FORCE)
    set(HB_HAVE_GLIB OFF CACHE BOOL "" FORCE)
    set(HB_HAVE_ICU OFF CACHE BOOL "" FORCE)
    set(HB_HAVE_GOBJECT OFF CACHE BOOL "" FORCE)
    set(HB_BUILD_SUBTESTS OFF CACHE BOOL "" FORCE)
    if(WIN32)
        set(HAVE_MMAP OFF CACHE BOOL "" FORCE)
        set(HAVE_MPROTECT OFF CACHE BOOL "" FORCE)
        set(HAVE_SYSCONF OFF CACHE BOOL "" FORCE)
        set(HAVE_GETPAGESIZE OFF CACHE BOOL "" FORCE)
        set(HAVE_ISATTY OFF CACHE BOOL "" FORCE)
    endif()
    if(HARFBUZZ_SRC_DIR)
        add_subdirectory("${HARFBUZZ_SRC_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/harfbuzz_build")
    endif()
    set(HARFBUZZ_BUILT ON)
endif()

# --- Define the Plugin Target ---
add_library(OfxBase SHARED
    src/main.cc
    src/render/Renderer.cc
    src/font/FontManager.cc
    src/font/FontPlatform.cc
    src/debugger/LogManager.cc
    src/params/ParameterManager.cc
    src/params/core/ParameterNode.cc
    src/params/core/ParameterGroup.cc
    src/params/core/RootParameterGroup.cc
    src/params/core/ParameterIndexerBase.cc
    src/params/core/ParameterTreeBuilder.cc
    src/params/core/DoubleParameter.cc
    src/params/core/Double2DParameter.cc
    src/params/core/RGBAParameter.cc
    src/params/core/ChoiceParameter.cc
    src/params/core/BooleanParameter.cc
    src/params/core/IntParameter.cc
    src/params/core/PushButtonParameter.cc
    src/params/core/StringParameter.cc
    src/params/core/GroupParameter.cc
    src/interaction/InteractionInput.cc
    src/interaction/Interact.cc
    src/interaction/UseCaseRouter.cc
    src/interaction/Intent.cc
    src/interaction/StateFactory.cc
    src/interaction/InteractionUseCase.cc
    src/interaction/KeyUseCase.cc
    src/overlay/OverlayRenderer.cc
    src/overlay/OfxDrawSuiteRenderer.cc
    # src/overlay/OpenGLRenderer.cc
    src/interaction/usecases/MoveUseCase.cc
    src/interaction/usecases/CursorHighlightUseCase.cc
    src/interaction/usecases/GrabableAreaDisplayUseCase.cc
    src/interaction/usecases/DragFeedbackUseCase.cc
)

if(WIN32)
    target_sources(OfxBase PRIVATE
        src/font/Win32FontPlatform.cc
        src/processors/OpenCLCompositor.cc
    )
elseif(APPLE)
    target_sources(OfxBase PRIVATE
        src/font/AppleFontPlatform.cc
        src/processors/MetalCompositor.mm
    )
else()
    target_sources(OfxBase PRIVATE
        src/font/LinuxFontPlatform.cc
        src/processors/OpenCLCompositor.cc
    )
endif()

target_include_directories(OfxBase PRIVATE
    ${OFX_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/font
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render
    ${CMAKE_CURRENT_SOURCE_DIR}/src/processors
    ${CMAKE_CURRENT_SOURCE_DIR}/src/debugger
    ${CMAKE_CURRENT_SOURCE_DIR}/src/params
    ${CMAKE_CURRENT_SOURCE_DIR}/src/params/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/interaction
    ${BLEND2D_SRC_DIR}
    ${BLEND2D_SRC_DIR}/blend2d
    ${GLAZE_DIR}/include
    ${UTF8CPP_SRC_DIR}/source
    ${FREETYPE_SRC_DIR}/include
    ${HARFBUZZ_SRC_DIR}/src
)

target_link_libraries(OfxBase PRIVATE ofxsupport blend2d freetype harfbuzz Threads::Threads ${PLATFORM_LINK_LIBS})

if(APPLE)
    target_link_libraries(OfxBase PRIVATE
        "-framework Metal"
        "-framework Foundation"
        "-framework CoreText"
        "-framework CoreFoundation"
        "-framework CoreGraphics"
        # "-framework OpenGL"
    )
elseif(NOT WIN32)
    find_package(Fontconfig REQUIRED)
    target_link_libraries(OfxBase PRIVATE Fontconfig::Fontconfig)
endif()

if(PLATFORM_LINK_FLAGS)
    target_link_options(OfxBase PRIVATE ${PLATFORM_LINK_FLAGS})
endif()

set_target_properties(OfxBase PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Post-build: cache built libraries
if(OFX_SUPPORT_BUILT)
    file(MAKE_DIRECTORY "${CACHE_BASE_DIR}/ofxsupport/${ARCH_DIR}")
    add_custom_command(TARGET OfxBase POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:ofxsupport> "${PREBUILT_OFX_LIB}"
        COMMENT "Caching ofxsupport for ${ARCH_DIR}"
    )
endif()
if(BLEND2D_BUILT)
    file(MAKE_DIRECTORY "${CACHE_BASE_DIR}/blend2d/${ARCH_DIR}")
    add_custom_command(TARGET OfxBase POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:blend2d> "${PREBUILT_BLEND2D_LIB}"
        COMMENT "Caching blend2d for ${ARCH_DIR}"
    )
endif()
if(FREETYPE_BUILT)
    file(MAKE_DIRECTORY "${CACHE_BASE_DIR}/freetype/${ARCH_DIR}")
    add_custom_command(TARGET OfxBase POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:freetype> "${PREBUILT_FREETYPE_LIB}"
        COMMENT "Caching freetype for ${ARCH_DIR}"
    )
endif()
if(HARFBUZZ_BUILT)
    file(MAKE_DIRECTORY "${CACHE_BASE_DIR}/harfbuzz/${ARCH_DIR}")
    add_custom_command(TARGET OfxBase POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:harfbuzz> "${PREBUILT_HARFBUZZ_LIB}"
        COMMENT "Caching harfbuzz for ${ARCH_DIR}"
    )
endif()

set_target_properties(OfxBase PROPERTIES
    OUTPUT_NAME "OfxBase"
    PREFIX ""
    SUFFIX ".ofx"
)

if(PLATFORM_BUNDLE)
    set(BUNDLE_DIR "${CMAKE_CURRENT_BINARY_DIR}/OfxBase.ofx.bundle/Contents")
    if(APPLE)
        set(BUNDLE_BIN_SUBDIR "MacOS")
    else()
        set(BUNDLE_BIN_SUBDIR "Win64")
    endif()

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Info.plist.in"
        "${BUNDLE_DIR}/Info.plist"
        @ONLY
    )

    add_custom_command(TARGET OfxBase POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BUNDLE_DIR}/${BUNDLE_BIN_SUBDIR}"
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:OfxBase> "${BUNDLE_DIR}/${BUNDLE_BIN_SUBDIR}/OfxBase.ofx"
        COMMENT "Creating OFX bundle"
    )

    if(APPLE)
        add_custom_command(TARGET OfxBase POST_BUILD
            COMMAND codesign --force --deep -s - "${CMAKE_CURRENT_BINARY_DIR}/OfxBase.ofx.bundle"
            COMMENT "Signing OFX bundle"
        )
    endif()
endif()
