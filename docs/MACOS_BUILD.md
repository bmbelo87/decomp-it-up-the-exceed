# macOS Universal Build & Packaging Documentation

This document explains the macOS build system, universal binary architecture (`arm64` + `x86_64`), application bundle structure, and asset resolution mechanism.

---

## 1. Overview

The macOS target is built as a **Universal 2 Binary (Fat Mach-O)** supporting both Apple Silicon (`arm64`) and Intel Macs (`x86_64`).

The build system produces two artifacts simultaneously without bundling copyrighted game data:
1. **`Pumpy.app`**: A standalone, notarization-ready macOS application bundle for desktop users.
2. **`Pumpy`**: A standalone universal command-line binary for terminal execution.

Both artifacts are free of proprietary assets. Users place them in their legitimate Pump It Up installation directory alongside the `BGA`, `AUDIO`, `STEP`, and `WAVE` folders.

---

## 2. Technical Architecture & Modifications

### 2.1 OpenGL Header Compatibility
* **File:** `include/pumpy.h`
* **Change:** Standard Apple SDKs do not store OpenGL headers under `GL/`. The header includes `<OpenGL/gl.h>` and `<OpenGL/glu.h>` when `__APPLE__` is defined.
* **Deprecation Notice:** `#define GL_SILENCE_DEPRECATION` is defined before inclusion to suppress Apple Clang deprecation warnings for legacy OpenGL (OpenGL 1.1 / 2.1).

### 2.2 32-bit Win32 Scalar Types
* **File:** `include/platform_linux.h`
* **Change:** In 64-bit POSIX environments (LP64 data model), `unsigned long` is 64 bits wide. To maintain binary compatibility with original Win32 structures (LLP64 data model), `DWORD` and `ULONG` are defined as `uint32_t`, and `LONG` is defined as `int32_t`.

### 2.3 Native System zlib
* **File:** `src/zlibinflate.c`
* **Change:** macOS provides hardware-accelerated `libz.1.dylib` in its base system SDK for both architectures. Adding `defined(__APPLE__)` allows macOS to link directly against system zlib.

### 2.4 Application Bundle Working Directory (CWD) Resolution
* **Files:** `src/platform_posix.c`, `include/platform_linux.h`, `src/main.c`
* **Mechanism:** When launched via Finder, macOS sets the working directory to `/` or the user's home folder. The `Platform_InitCWD()` routine inspects runtime conditions:
  1. If game folders (`Stage.cfg`, `BGA`, `AUDIO`, `STEP`, `WAVE`) are already present in the current working directory, no change is made.
  2. If the executable runs inside an application bundle (`.../Pumpy.app/Contents/MacOS/Pumpy`), it queries `_NSGetExecutablePath()`, resolves the canonical path with `realpath()`, and changes the working directory (`chdir`) to the parent directory containing `Pumpy.app`.
  3. If no assets are located, the engine presents the native `"ASSETS NOT FOUND"` screen.

---

## 3. Bundle Structure & Dependencies

The generated `Pumpy.app` directory structure is shown below:

```text
Pumpy.app/
└── Contents/
    ├── Info.plist
    ├── MacOS/
    │   └── Pumpy                      (Mach-O Universal: arm64 + x86_64)
    ├── Frameworks/
    │   └── SDL2.framework/            (Universal fat framework with symlinks preserved)
    │       ├── SDL2 -> Versions/Current/SDL2
    │       ├── Headers -> Versions/Current/Headers
    │       └── Versions/A/SDL2
    ├── Resources/
    │   └── AppIcon.icns               (Generated from IA_LOGO.png, 16px to 1024px Retina)
    └── _CodeSignature/                (Ad-hoc signature via codesign)
```

### Dynamic Linking & RPATH
The binary is linked with the following runpath search paths:
* `@executable_path/../Frameworks` (locates `SDL2.framework` inside `Pumpy.app`)
* `@executable_path/Frameworks` (locates `Frameworks` beside standalone CLI binary)
* `/Library/Frameworks` (fallback for system-wide framework installations)

The executable requires no external package managers (such as Homebrew or MacPorts).

---

## 4. Build Instructions

### Prerequisites
* macOS 11.0 or newer
* Xcode Command Line Tools (`xcode-select --install`)
* CMake 3.10 or newer

### Building the Universal Binaries
Run the following commands from the repository root:

```bash
# Configure the build (Universal arm64 + x86_64 is enabled by default on macOS)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Compile both Pumpy.app and the CLI binary
cmake --build build --target Pumpy --parallel
```

### Packaging for Distribution
To create the standalone distributable ZIP archive (`Pumpy-macOS-Universal.zip`):

```bash
cmake --build build --target package_mac
```

The resulting archive contains `Pumpy.app` and the standalone `Pumpy` executable.

---

## 5. Deployment Instructions for End Users

1. Extract `Pumpy-macOS-Universal.zip`.
2. Move `Pumpy.app` into your Pump It Up game folder (alongside `BGA/`, `AUDIO/`, `STEP/`, and `WAVE/`).
3. Double-click `Pumpy.app` in Finder to play.
4. Alternatively, open Terminal in the game folder and run `./Pumpy`.
