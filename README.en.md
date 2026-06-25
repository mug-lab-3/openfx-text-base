# OfxBase — Template for Text OFX Plugin Development

A template repository for getting started with text-focused OFX plugin development,
with the core processing extracted from [Mug Typography](https://muglab3.booth.pm/items/8339678).
The build system, debug system, and core features are all implemented, along with sample code for interaction, rendering, and parameter definition.

> [!NOTE]
> Design notes for some parts are available under [docs/](docs/), but detailed documentation for the whole codebase is not available.
> The easiest way to understand each class and how to use it is to pass the entire repository to an AI (e.g. Claude) for analysis.
>
> - [docs/usecase-base-classes.md](docs/usecase-base-classes.md) — Design rationale and reference for the UseCase base classes
> - [docs/interaction-intent.md](docs/interaction-intent.md) — Design of the Intent system that loosely couples UseCases

![overview](docs/images/overview.webp)

---

## What's Included

### Third-Party Libraries

See [3rdparty-licenses.md](3rdparty-licenses.md) for the full license text of each library.

| Library | Version | License | Purpose |
|---|---|---|---|
| [OpenFX SDK](https://github.com/ofxa/openfx) | OFX_Release_1.5.1 | BSD-3-Clause | C++ wrapper for OFX plugins |
| [Blend2D](https://github.com/blend2d/blend2d) | latest | Zlib | High-quality 2D vector rendering engine (anti-aliasing, text support) |
| [AsmJit](https://github.com/asmjit/asmjit) | latest | Zlib | JIT backend for Blend2D |
| [utf8cpp](https://github.com/nemtrif/utfcpp) | v4.0.9 | BSL-1.0 | UTF-8 string handling |
| [glaze](https://github.com/stephenberry/glaze) | v2.2.0 | MIT | JSON serialization |
| [FreeType](https://github.com/freetype/freetype) | VER-2-14-3 | FTL | Font loading & glyph outline extraction |
| [HarfBuzz](https://github.com/harfbuzz/harfbuzz) | 14.2.0 | MIT | Text shaping |

### Build System (`scripts/build.sh`)

- Cross-compilation via Docker container (WSL2/Linux → Windows x64)
- Native macOS build support (`--mac` option)
- Fast incremental builds with Ninja + ccache
- Automatic release ZIP generation (bundle + installer + license file)
- Switchable trial build mode (`--trial`) and UDP log output (`--debug`)

### Debug System

- Structured JSON log transmission over UDP (`src/debugger/LogManager`)
- Python UDP log viewer (`debugger/udp-viewer/`) — PyQt6 GUI, receives on port 5555, supports filtering and CSV export

### Core Implementation

- **Parameters** (`src/params/`): Major parameter types including Boolean / Choice / Double / Double2D / Int / RGBA / String / PushButton / Group, tree-based management, conditional visibility (`VisibilityRule`)
- **Interaction** (`src/interaction/`): Abstraction of pen/mouse/keyboard input, behavior separation via UseCase pattern, priority control with `HandlingRole`, Intent metadata logging
- **Rendering** (`src/render/`, `src/overlay/`): CPU-path frame compositing with Blend2D, overlay rendering via OFX Draw Suite / OpenGL
- **GPU Processors** (`src/processors/`): Overlay compositing with OpenCL (Windows/Linux) and Metal (macOS)
- **Font** (`src/font/`): Platform-specific font enumeration for Win32 / macOS / Linux, LRU cache for FreeType faces

### Sample Implementations

- Draggable UI elements (`DragUseCase`, `GrabableAreaDisplayUseCase`)
- Cursor highlight (`CursorHighlightUseCase`)
- Drag feedback (`DragFeedbackUseCase`)
- Button-triggered command (`ResetPositionAndSizeCommand`) — resets Position and Size to defaults
- Keyboard shortcut (`ResetPositionAndSizeKeyUseCase`) — Alt+N triggers the same reset
- Examples of major parameter type definitions (`ParameterManager`)

---

## Project Structure

```text
.
├── CMakeLists.txt
├── docs/                        # Design notes for some parts
├── cmake/
│   ├── toolchain.cmake          # Windows cross-compile settings (LLVM/Clang/LLD)
│   ├── mac-settings.cmake       # macOS native build settings (Apple M1, LTO)
│   └── Info.plist.in            # Info.plist template for macOS bundle
├── docker/
│   ├── Dockerfile               # Build environment (Ubuntu 24.04, LLVM 18, MinGW, SDKs)
│   └── entrypoint.sh
├── scripts/
│   ├── build.sh                 # Main build script (Windows + macOS)
│   ├── docker-build.sh          # Simple Windows build script (legacy)
│   ├── rebuild_docker_image.sh  # Rebuild Docker image
│   ├── setup-editor.sh          # Sync SDK headers to .sdk/ for editor support
│   ├── install-win.bat          # Windows installer
│   └── install-mac.command      # macOS installer
├── debugger/
│   └── udp-viewer/              # UDP log viewer (Python / PyQt6)
│       ├── viewer.py            # GUI (filter, color coding, CSV export)
│       ├── run.sh / run.bat     # Launch scripts
│       └── setup.sh / setup.bat # Dependency setup
└── src/
    ├── main.cc / main.h         # Plugin entry point (OfxBasePlugin)
    ├── Version.h                # Version constants (1.0.0)
    ├── params/                  # Parameter definition and management
    │   ├── ParameterManager.h/cc
    │   ├── ParamIds.h           # Parameter ID constants
    │   └── core/                # Parameter types, tree, visibility rules
    ├── interaction/             # Interaction (input handling, UseCases)
    │   ├── Interact.h/cc        # OFX overlay interaction entry point
    │   ├── UseCaseRouter.h/cc   # UseCase lifecycle management and event routing
    │   ├── InteractionUseCase.h/cc  # UseCase base class
    │   ├── CurrentState.h / SnapshotState.h  # Runtime state and snapshot
    │   └── usecases/            # Concrete UseCase implementations (Drag / Highlight / DragFeedback / Command / Key etc.)
    ├── overlay/                 # Overlay rendering
    │   ├── OverlayRenderer.h    # Rendering interface
    │   ├── OfxDrawSuiteRenderer # OFX Draw Suite implementation
    │   └── OpenGLRenderer       # OpenGL implementation (disabled by default)
    ├── render/                  # Frame compositing
    │   ├── Renderer.h/cc        # Blend2D CPU + GPU compositing
    │   └── CoordTransform.h     # Coordinate transformation helpers
    ├── processors/              # GPU compositors
    │   ├── OpenCLCompositor     # For Windows / Linux
    │   └── MetalCompositor      # For macOS (Objective-C++)
    ├── font/                    # Platform-specific font management
    │   ├── FontManager.h/cc     # Common interface and LRU cache
    │   ├── Win32FontPlatform    # GDI font enumeration
    │   ├── AppleFontPlatform    # CoreText font enumeration
    │   └── LinuxFontPlatform    # Fontconfig font enumeration
    └── debugger/                # UDP log transmission
        └── LogManager.h/cc      # LOG_INFO / LOG_ERROR macros
```

---

## Setup

### WSL2 / Linux (Windows build)

```bash
# Install Docker
curl -fsSL https://get.docker.com -o get-docker.sh
sudo sh get-docker.sh

# Allow running without sudo
sudo usermod -aG docker $USER
newgrp docker
```

### macOS (Native build)

The following are required:

```bash
# Xcode Command Line Tools (if not already installed)
xcode-select --install

# Build tools
brew install cmake ninja ccache

# Docker (Docker Desktop or alternatives like Colima)
brew install colima docker
colima start
```

The Docker image build and `.sdk/` header sync happen automatically on the first run of `build.sh --mac`.

### Editor Setup (IntelliSense / clangd)

```bash
# Sync SDK headers from the container to .sdk/ (run manually if needed)
./scripts/setup-editor.sh
```

Use the [clangd extension](https://marketplace.visualstudio.com/items?itemName=llvm-vs-code-extensions.vscode-clangd). If the Microsoft C/C++ extension is enabled, it is recommended to disable it in favor of clangd.

---

## Building

```bash
# Windows build (Docker image is built automatically on first run)
./scripts/build.sh

# macOS native build
./scripts/build.sh --mac

# Options
./scripts/build.sh --debug    # Enable UDP logging
./scripts/build.sh --trial    # Trial build
./scripts/build.sh --clean    # Remove build directory only (keep cache)
./scripts/build.sh --all      # Full clean (remove build/ and .ccache)
```

**Output:**
- Plugin bundle: `build/win/OfxBase.ofx.bundle/` or `build/mac/OfxBase.ofx.bundle/`
- Release ZIP: `build/for_windows_X.Y.Z.zip` / `build/for_mac_X.Y.Z.zip` (when `--debug` is not set)
- Editor DB: `build/compile_commands.json`

---

## Installation

### Windows

Place `OfxBase.ofx.bundle/`, `install-win.bat`, and `3rdparty-licenses.md` in the same folder, then double-click `install-win.bat` (administrator privileges will be requested automatically).

Install location: `C:\Program Files\Common Files\OFX\Plugins\`

### macOS

Extract the release ZIP and double-click `install-mac.command`.

Install location: `/Library/OFX/Plugins/`

---

## UDP Log Viewer

When the plugin is built with `--debug`, logs are sent to UDP port 5555.

```bash
# First-time setup (creates Python virtual environment and installs PyQt6)
cd debugger/udp-viewer
./setup.sh        # macOS / Linux
setup.bat         # Windows

# Launch
./run.sh          # macOS / Linux
run.bat           # Windows
```

**Features:** Real-time log display, AND-based filtering, color coding (ERROR / WARN / INFO), CSV export

![udp-log-viewer](docs/images/udp-log-viewer.webp)

---

## Developer Commands

```bash
# Fully rebuild the Docker image (use when updating SDKs or if the image is corrupted)
./scripts/rebuild_docker_image.sh
```
