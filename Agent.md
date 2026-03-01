# Agent Task Report: Emscripten WebAssembly Port

## Overview
This document records the AI-assisted development work on the Clemens IIGS Emscripten (WebAssembly) port.

---

## Task 1: Emscripten Disk Selection

### Objective
Implement a disk selection mechanism for the Emscripten build, allowing users to mount local disk images into the emulator running in the browser.

### Work Accomplished
1.  **Platform Abstraction**:
    *   Defined `clem_host_platform_select_disk` as the entry point for triggering disk selection.
    *   Added `clemens_host_import_disk` to bridge the platform-specific file import back to the application core.

2.  **Emscripten Implementation (`host/platform/host_emscripten.c`)**:
    *   Implemented `get_local_user_directory` and related functions to return valid VFS paths (e.g., `/home/web_user`), resolving startup crashes.
    *   Used `EM_JS` to embed the `clemens_prompt_disk` JavaScript function directly within C code. This function:
        *   Creates a hidden `<input type="file">`.
        *   Reads the user-selected file into the Emscripten MEMFS.
        *   Calls back to C++ using `Module.ccall`.
    *   Implemented `clemens_emscripten_mount_disk` (exported via `EMSCRIPTEN_KEEPALIVE`) to receive the file path from JS and call `clemens_host_import_disk`.

3.  **Frontend Integration**:
    *   Modified `host/clem_front.cpp` to conditionally call `clem_host_platform_select_disk` when disk icons are clicked in Emscripten builds.
    *   Updated `ClemensFrontend` to handle `insertDisk` and `insertSmartDisk` requests from the host.

4.  **Build Configuration**:
    *   Updated `host/CMakeLists.txt` to export `ccall` and `UTF8ToString` runtime methods, preventing runtime aborts when JS calls C++.

### Modified Files
*   `host/clem_front.hpp`
*   `host/clem_front.cpp`
*   `host/clem_host.hpp`
*   `host/clem_host_app.cpp`
*   `host/clem_host_platform.h`
*   `host/platform/host_emscripten.c`
*   `host/CMakeLists.txt`
*   `host/resources/emscripten_shell.html` (Cleanup)
*   `.gitignore` (Updated)

### Key Technical Decisions
*   **EM_JS vs HTML Template**: Switched to `EM_JS` for the disk prompt logic to keep the implementation self-contained and avoid cache/template update issues.
*   **VFS Usage**: Files are written to the browser's memory filesystem (MEMFS). This is temporary and lost on page reload, which is expected behavior for disk images.

---

## Task 2: Web Version Shortcut Fix

### Objective
Fix the "Fast Mode" shortcut (Ctrl+Left Alt+F8) which was reported as not working in the web version.

### Diagnosis
The `host/clem_front.cpp` logic was specifically checking for `ImGuiKey_LeftAlt`. On the web platform (Emscripten/Sokol), standard Alt key modifiers might not map reliably to `LeftAlt`. Generalizing the shortcut allows for better usability across all platforms.

### Work Accomplished
1.  **Fast Mode & General Shortcuts (`host/clem_front.cpp`)**:
    *   Generalized shortcut logic to use `ImGui::GetIO().KeyAlt` and `KeyCtrl` instead of specific `LeftAlt`/`LeftCtrl` checks.
    *   This ensures `Ctrl + Alt + F8` works regardless of left/right modifier keys on all platforms.
    *   **Removed** alternative numeric key bindings (e.g., `8`, `5`, `0`, `-`) to prevent conflicts with emulated Apple IIGS software. Shortcuts are now:
        *   `Ctrl + Alt + F8` — Fast Mode
        *   `F5` — Pause
        *   `F10` — Mouse Lock
        *   `F11` — Debugger

2.  **IIGS Reset / Control Panel (`host/clem_host_app.cpp`)**:
    *   Implemented a platform-specific key mapping for Emscripten to handle `Ctrl + LAlt + RAlt + F1`.
    *   This combination is intercepted and mapped to `SAPP_KEYCODE_ESCAPE`, simulating `Apple + Control + Esc` / `Ctrl + Reset` to access the IIGS Control Panel.

3.  **Build System Improvements**:
    *   Created `build_emscripten.bat` — automatically downloads Emscripten SDK and Ninja, then compiles the project.
    *   Updated `README.md` to reflect the simplified one-step build process.

### Modified Files
*   `host/clem_front.cpp` (Shortcut logic)
*   `host/clem_host_app.cpp` (Key mapping for Reset/Control Panel)
*   `build_emscripten.bat` (New automated build script)
*   `README.md` (Updated instructions)

---

## Task 3: PR Feedback Resolution — Restore Workflows & ROM Handling

### Objective
Address upstream author (samkusin) feedback on PR #159:
1. Original CI workflows were accidentally removed.
2. Apple IIgs ROM file (`rom/rom.v3`) was committed to the repository.

### Work Accomplished

1.  **Restored Upstream CI Workflows**:
    *   Restored `.github/workflows/build-linux.yml`, `build-macos.yml`, `build-windows.yml` verbatim from upstream (`samkusin/clemens_iigs`).
    *   Added `.github/workflows/deploy-emscripten.yml` as an independent new workflow that does **not** interfere with the existing CI pipelines.

2.  **ROM: Runtime Browser Loading (IndexedDB + File Picker)**:
    *   Removed `rom/rom.v3` from git tracking (`git rm --cached`) and added `rom/` to `.gitignore`.
    *   **The ROM is now loaded entirely at runtime in the browser**, eliminating any build-time dependency on the ROM file.

    **How it works** (`host/platform/host_emscripten.c`):
    *   At startup, `clem_host_platform_load_rom()` is called once. It runs async JS that:
        1. Checks **IndexedDB** (`ClemensIIGS` db, `assets` store) for a previously cached ROM.
        2. If found: loads it directly into MEMFS at `/rom.v3` and signals ready.
        3. If not found: shows a styled full-screen overlay with a **"Select ROM file…"** button.
    *   When the user selects a file, it is validated (must be exactly 262,144 bytes), written to MEMFS, and **saved to IndexedDB** for all future sessions.
    *   C++ polls `clem_host_platform_rom_state()` each frame; when it returns `2` (ready), startup continues normally.
    *   On subsequent page loads, the ROM is restored from IndexedDB silently — **users only need to select it once**.

    **Startup flow** (`host/clem_startup_view.cpp`, `host/clem_startup_view.hpp`):
    *   Added `Mode::WaitForRom` between `Initial` and `Preamble` (Emscripten only, guarded by `#ifdef __EMSCRIPTEN__`).
    *   Non-Emscripten builds are **completely unaffected**.

    **Build system** (`host/CMakeLists.txt`):
    *   Removed `--preload-file rom/rom.v3` from Emscripten link options.
    *   No ROM file is needed at build time.

    **CI/CD** (`.github/workflows/deploy-emscripten.yml`):
    *   No ROM secrets or special handling required — the workflow builds and deploys cleanly.

3.  **Updated `.gitignore`**:
    *   Added `rom/` directory.
    *   Added `rom_secret_part*.txt` (generated by helper script).
    *   Added `tools/` (Emscripten SDK and Ninja, downloaded at build time).

### Modified Files
*   `host/platform/host_emscripten.c` — ROM loader state machine + IndexedDB JS + file picker overlay
*   `host/clem_host_platform.h` — Added `clem_host_platform_load_rom()`, `clem_host_platform_rom_state()`
*   `host/clem_startup_view.hpp` — Added `WaitForRom` mode + `romLoadTriggered_` flag
*   `host/clem_startup_view.cpp` — Implemented `WaitForRom` case (Emscripten only)
*   `host/CMakeLists.txt` — Removed ROM preload from Emscripten build
*   `.github/workflows/build-linux.yml` — Restored from upstream
*   `.github/workflows/build-macos.yml` — Restored from upstream
*   `.github/workflows/build-windows.yml` — Restored from upstream
*   `.github/workflows/deploy-emscripten.yml` — Simplified, no ROM secrets
*   `.gitignore` — Added `rom/`, `rom_secret_part*.txt`
*   `build_emscripten.bat` — Removed ROM check, added first-launch note
*   `split_rom_secret.ps1` — Helper script (generates base64 chunks, output is gitignored)

---

## How to Build and Run (Emscripten / Web)

### Local Build (Windows)
1.  Open a terminal in the project root.
2.  Run `.\build_emscripten.bat` — automatically downloads Emscripten SDK and Ninja, then compiles.
3.  Run `.\run_emscripten.bat` to start the local server.
4.  Open `http://localhost:6931/clemens_iigs.html`.
5.  On first launch, you will be prompted to select your Apple IIgs ROM 3 file (`rom.v3`). It will be saved in the browser for future visits.

### Local Build (Linux / macOS)
```bash
./build_emscripten.sh
```

### Web Demo
The web demo is automatically deployed to GitHub Pages on every push to `main`:
**https://anomixer.github.io/clemens_iigs/**

---

## Key Technical Decisions

| Decision | Rationale |
|---|---|
| **IndexedDB for ROM persistence** | Survives page reloads; no server needed; works offline after first use |
| **Runtime ROM loading (not build-time)** | Avoids copyright issues; no ROM in repo; no CI secrets needed |
| **`#ifdef __EMSCRIPTEN__` guards** | All changes are strictly isolated; desktop/native builds untouched |
| **262,144 byte validation** | ROM 3 is always exactly 256 KB; this catches wrong files early with a clear error |
| **EM_JS for JS logic** | Keeps JS code co-located with C code; avoids HTML template cache issues |
| **Styled overlay (not ImGui)** | Runs before WASM main loop; robust; no dependency on ImGui being ready |
