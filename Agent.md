# Agent Task Report: Emscripten Disk Selection

## Task Objective
Implement a disk selection mechanism for the Emscripten build of Clemens IIGS, allowing users to mount local disk images into the emulator running in the browser.

## Work Accomplished
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

## Modified Files
*   `host/clem_front.hpp`
*   `host/clem_front.cpp`
*   `host/clem_host.hpp`
*   `host/clem_host_app.cpp`
*   `host/clem_host_platform.h`
*   `host/platform/host_emscripten.c`
*   `host/CMakeLists.txt`
*   `host/resources/emscripten_shell.html` (Cleanup)
*   `.gitignore` (Updated)

## How to Build and Run
1.  Open a terminal in the project root.
2.  Run `.\build_emscripten.bat` to compile.
3.  Run `.\run_emscripten.bat` to start the local server.
4.  Open `http://localhost:6931/clemens_iigs.html`.

## Key Technical Decisions
*   **EM_JS vs HTML Template**: Switched to `EM_JS` for the disk prompt logic to keep the implementation self-contained and avoid cache/template update issues.
*   **VFS Usage**: Files are written to the browser's memory filesystem (MEMFS). This is temporary and lost on page reload, which is expected behavior for this feature.
