#include "clem_host_platform.h"
#include <emscripten.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

extern void clemens_host_import_disk(int driveIndex, bool isSmart, const char *path);

// ---------------------------------------------------------------------------
// ROM state machine
// ---------------------------------------------------------------------------
// 0 = idle/unknown, 1 = loading (async in progress), 2 = ready, 3 = error
static int s_rom_state = 0;

EMSCRIPTEN_KEEPALIVE
void clemens_emscripten_rom_ready(void) {
    printf("[C] ROM is ready at /rom.v3\n");
    s_rom_state = 2;
}

EMSCRIPTEN_KEEPALIVE
void clemens_emscripten_rom_error(void) {
    printf("[C] ROM load failed\n");
    s_rom_state = 3;
}

int clem_host_platform_rom_state(void) {
    return s_rom_state;
}

// ---------------------------------------------------------------------------
// ROM loader: tries IndexedDB first, then falls back to file picker.
// Once loaded, the ROM bytes are written to MEMFS at /rom.v3 AND persisted
// to IndexedDB so future page loads skip the file picker.
// ---------------------------------------------------------------------------
EM_JS(void, clemens_load_rom, (void), {
    var ROM_IDB_KEY = 'clemens_iigs_rom_v3';
    var ROM_MEMFS_PATH = '/rom.v3';

    function writeRomToMemfs(data) {
        try {
            try { FS.unlink(ROM_MEMFS_PATH); } catch(e) {}
            var stream = FS.open(ROM_MEMFS_PATH, 'w+');
            FS.write(stream, new Uint8Array(data), 0, data.byteLength, 0);
            FS.close(stream);
            console.log('[ROM] Written to MEMFS: ' + data.byteLength + ' bytes');
            Module.ccall('clemens_emscripten_rom_ready', 'void', [], []);
        } catch(e) {
            console.error('[ROM] MEMFS write failed: ' + e);
            Module.ccall('clemens_emscripten_rom_error', 'void', [], []);
        }
    }

    function saveRomToIDB(data) {
        var req = indexedDB.open('ClemensIIGS', 1);
        req.onupgradeneeded = function(e) {
            e.target.result.createObjectStore('assets');
        };
        req.onsuccess = function(e) {
            var db = e.target.result;
            var tx = db.transaction('assets', 'readwrite');
            tx.objectStore('assets').put(data, ROM_IDB_KEY);
            tx.oncomplete = function() {
                console.log('[ROM] Saved to IndexedDB for future sessions.');
            };
        };
    }

    function tryLoadFromIDB() {
        var req = indexedDB.open('ClemensIIGS', 1);
        req.onupgradeneeded = function(e) {
            e.target.result.createObjectStore('assets');
        };
        req.onsuccess = function(e) {
            var db = e.target.result;
            var tx = db.transaction('assets', 'readonly');
            var getReq = tx.objectStore('assets').get(ROM_IDB_KEY);
            getReq.onsuccess = function(e) {
                if (e.target.result) {
                    console.log('[ROM] Found in IndexedDB, loading...');
                    writeRomToMemfs(e.target.result);
                } else {
                    console.log('[ROM] Not in IndexedDB, prompting file picker.');
                    showFilePicker();
                }
            };
            getReq.onerror = function() {
                console.warn('[ROM] IDB read error, falling back to file picker.');
                showFilePicker();
            };
        };
        req.onerror = function() {
            console.warn('[ROM] IDB open error, falling back to file picker.');
            showFilePicker();
        };
    }

    function showFilePicker() {
        // Show an overlay so the user knows what to do
        var overlay = document.getElementById('rom-overlay');
        if (!overlay) {
            overlay = document.createElement('div');
            overlay.id = 'rom-overlay';
            overlay.style.cssText = [
                'position:fixed','top:0','left:0','width:100%','height:100%',
                'background:rgba(0,0,0,0.82)','display:flex','flex-direction:column',
                'align-items:center','justify-content:center','z-index:9999',
                'font-family:monospace','color:#e8e8e8','gap:16px'
            ].join(';');
            overlay.innerHTML =
                '<div style="font-size:1.4em;font-weight:bold;color:#f5c842;">&#127758; Clemens IIGS</div>' +
                '<div style="font-size:0.95em;max-width:480px;text-align:center;line-height:1.6;">' +
                'An Apple IIgs <strong>ROM 3</strong> image (<code>rom.v3</code>, 256&nbsp;KB) is required.<br>' +
                'It is <em>not</em> bundled due to copyright &mdash; please supply your own.<br>' +
                'Your ROM will be saved in this browser for future visits.' +
                '</div>' +
                '<button id="rom-pick-btn" style="' +
                'padding:12px 32px;font-size:1em;background:#f5c842;color:#111;' +
                'border:none;border-radius:6px;cursor:pointer;font-weight:bold;">Select ROM file&hellip;</button>' +
                '<div id="rom-status" style="font-size:0.85em;color:#aaa;min-height:1.4em;"></div>';
            document.body.appendChild(overlay);
        }
        overlay.style.display = 'flex';

        document.getElementById('rom-pick-btn').onclick = function() {
            var input = document.createElement('input');
            input.type = 'file';
            input.accept = '.v3,.rom,.bin,*';
            input.onchange = function(e) {
                var file = e.target.files[0];
                if (!file) return;
                document.getElementById('rom-status').textContent = 'Loading ' + file.name + '...';
                var reader = new FileReader();
                reader.onload = function(e) {
                    var buf = e.target.result;
                    if (buf.byteLength !== 262144) {
                        document.getElementById('rom-status').textContent =
                            'Error: expected 262144 bytes, got ' + buf.byteLength +
                            '. Is this really a ROM 3 image?';
                        return;
                    }
                    saveRomToIDB(buf);
                    // Hide overlay before writing to MEMFS (writeRomToMemfs calls back into C)
                    overlay.style.display = 'none';
                    writeRomToMemfs(buf);
                };
                reader.onerror = function() {
                    document.getElementById('rom-status').textContent = 'Failed to read file.';
                };
                reader.readAsArrayBuffer(file);
            };
            input.click();
        };
    }

    tryLoadFromIDB();
});

void clem_host_platform_load_rom(void) {
    s_rom_state = 1; // loading
    clemens_load_rom();
}

// ---------------------------------------------------------------------------
// Disk selection (unchanged)
// ---------------------------------------------------------------------------
EM_JS(void, clemens_prompt_disk, (int driveIndex, bool isSmart), {
    console.log("[JS] Prompting disk for drive=" + driveIndex + ", smart=" + isSmart);
    var input = document.getElementById('disk-input');
    if (!input) {
        input = document.createElement('input');
        input.type = 'file';
        input.id = 'disk-input';
        input.style.display = 'none';
        document.body.appendChild(input);
    }
    input.value = "";
    input.onchange = function(e) {
        var file = e.target.files[0];
        if (!file) {
            console.warn("[JS] No file selected.");
            return;
        }

        var reader = new FileReader();
        reader.onload = function(e) {
            try {
                var data = new Uint8Array(e.target.result);
                var filename = file.name;
                var path = '/' + filename;
                console.log("[JS] Writing file to VFS: " + path + " (" + data.length + " bytes)");

                try {
                    try { FS.unlink(path); } catch (e) {}
                    var stream = FS.open(path, 'w+');
                    FS.write(stream, data, 0, data.length, 0);
                    FS.close(stream);
                } catch (fsErr) {
                    console.error("[JS] FS Write failed: " + fsErr);
                    return;
                }

                try {
                    var stat = FS.stat(path);
                    console.log("[JS] Verification: File exists, size=" + stat.size);
                } catch (statErr) {
                    console.error("[JS] Verification failed: " + statErr);
                    return;
                }

                console.log("[JS] Calling C++ backend: clemens_emscripten_mount_disk...");
                Module.ccall('clemens_emscripten_mount_disk', 'void',
                             [ 'number', 'boolean', 'string' ], [ driveIndex, isSmart, path ]);
                console.log("[JS] C++ backend notified.");
            } catch (err) {
                console.error("[JS] Unexpected error: " + err);
            }
        };
        reader.readAsArrayBuffer(file);
    };
    input.click();
});

void clem_host_platform_select_disk(int driveIndex, bool isSmart) {
    clemens_prompt_disk(driveIndex, isSmart);
}

EMSCRIPTEN_KEEPALIVE
void clemens_emscripten_mount_disk(int driveIndex, bool isSmart, const char *path) {
    printf("[C] clemens_emscripten_mount_disk: driveIndex=%d, isSmart=%d, path='%s'\n", driveIndex,
           isSmart, path);
    if (path && path[0] != '\0') {
        clemens_host_import_disk(driveIndex, isSmart, path);
    } else {
        printf("[C] Error: Invalid path received\n");
    }
}

// ---------------------------------------------------------------------------
// System paths
// ---------------------------------------------------------------------------
char *get_process_executable_path(char *outpath, size_t *outpath_size) {
    const char *path = "/app/clemens_iigs";
    size_t len = strlen(path);
    if (outpath) {
        if (*outpath_size > len) {
            strcpy(outpath, path);
        } else {
            return NULL;
        }
    }
    if (outpath_size)
        *outpath_size = len + 1;
    return outpath;
}

char *get_local_user_directory(char *outpath, size_t outpath_size) {
    const char *path = "/home/web_user";
    if (outpath_size <= strlen(path))
        return NULL;
    strcpy(outpath, path);
    return outpath;
}

char *get_local_user_data_directory(char *outpath, size_t outpath_size, const char *company_name,
                                    const char *app_name) {
    const char *path = "/home/web_user/.local/share/clemens-iigs";
    if (outpath_size <= strlen(path))
        return NULL;
    strcpy(outpath, path);
    return outpath;
}

char *get_local_user_config_directory(char *outpath, size_t outpath_size, const char *company_name,
                                      const char *app_name) {
    const char *path = "/home/web_user/.config/clemens-iigs";
    if (outpath_size <= strlen(path))
        return NULL;
    strcpy(outpath, path);
    return outpath;
}

void open_system_folder_view(const char *folder_path) {
    // No-op for Emscripten
}

void clem_joystick_open_devices(const char *provider) {}

unsigned clem_joystick_poll(ClemensHostJoystick *joysticks) { return 0; }

void clem_joystick_close_devices(void) {}

unsigned clem_host_get_processor_number(void) { return 1; }

void clem_host_uuid_gen(ClemensHostUUID *uuid) {}

    console.log("[JS] Prompting disk for drive=" + driveIndex + ", smart=" + isSmart);
    var input = document.getElementById('disk-input');
    if (!input) {
        input = document.createElement('input');
        input.type = 'file';
        input.id = 'disk-input';
        input.style.display = 'none';
        document.body.appendChild(input);
    }
    input.value = "";
    input.onchange = function(e) {
        var file = e.target.files[0];
        if (!file) {
            console.warn("[JS] No file selected.");
            return;
        }

        var reader = new FileReader();
        reader.onload = function(e) {
            try {
                var data = new Uint8Array(e.target.result);
                var filename = file.name;
                // Ensure filename is specialized for the path
                var path = '/' + filename;
                console.log("[JS] Writing file to VFS: " + path + " (" + data.length + " bytes)");

                try {
                    // Start fresh
                    try {
                        FS.unlink(path);
                    } catch (e) {
                    }
                    var stream = FS.open(path, 'w+');
                    FS.write(stream, data, 0, data.length, 0);
                    FS.close(stream);
                } catch (fsErr) {
                    console.error("[JS] FS Write failed: " + fsErr);
                    return;
                }

                // Verification
                try {
                    var stat = FS.stat(path);
                    console.log("[JS] Verification: File exists, size=" + stat.size);
                } catch (statErr) {
                    console.error("[JS] Verification failed: " + statErr);
                    return;
                }

                console.log("[JS] Calling C++ backend: clemens_emscripten_mount_disk...");
                // Explicitly use string type for path to ensure correct marshalling
                Module.ccall('clemens_emscripten_mount_disk', 'void',
                             [ 'number', 'boolean', 'string' ], [ driveIndex, isSmart, path ]);
                console.log("[JS] C++ backend notified.");
            } catch (err) {
                console.error("[JS] Unexpected error: " + err);
            }
        };
        reader.readAsArrayBuffer(file);
    };
    input.click();
});

void clem_host_platform_select_disk(int driveIndex, bool isSmart) {
    clemens_prompt_disk(driveIndex, isSmart);
}

EMSCRIPTEN_KEEPALIVE
void clemens_emscripten_mount_disk(int driveIndex, bool isSmart, const char *path) {
    printf("[C] clemens_emscripten_mount_disk: driveIndex=%d, isSmart=%d, path='%s'\n", driveIndex,
           isSmart, path);
    // Ensure path is valid before passing to import
    if (path && path[0] != '\0') {
        clemens_host_import_disk(driveIndex, isSmart, path);
    } else {
        printf("[C] Error: Invalid path received\n");
    }
}

char *get_process_executable_path(char *outpath, size_t *outpath_size) {
    const char *path = "/app/clemens_iigs";
    size_t len = strlen(path);
    if (outpath) {
        if (*outpath_size > len) {
            strcpy(outpath, path);
        } else {
            return NULL;
        }
    }
    if (outpath_size)
        *outpath_size = len + 1;
    return outpath;
}

char *get_local_user_directory(char *outpath, size_t outpath_size) {
    const char *path = "/home/web_user";
    if (outpath_size <= strlen(path))
        return NULL;
    strcpy(outpath, path);
    return outpath;
}

char *get_local_user_data_directory(char *outpath, size_t outpath_size, const char *company_name,
                                    const char *app_name) {
    const char *path = "/home/web_user/.local/share/clemens-iigs";
    if (outpath_size <= strlen(path))
        return NULL;
    strcpy(outpath, path);
    return outpath;
}

char *get_local_user_config_directory(char *outpath, size_t outpath_size, const char *company_name,
                                      const char *app_name) {
    const char *path = "/home/web_user/.config/clemens-iigs";
    if (outpath_size <= strlen(path))
        return NULL;
    strcpy(outpath, path);
    return outpath;
}

void open_system_folder_view(const char *folder_path) {
    // No-op for Emscripten
}

void clem_joystick_open_devices(const char *provider) {}

unsigned clem_joystick_poll(ClemensHostJoystick *joysticks) { return 0; }

void clem_joystick_close_devices(void) {}

unsigned clem_host_get_processor_number(void) { return 1; }

void clem_host_uuid_gen(ClemensHostUUID *uuid) {}
