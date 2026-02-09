#include "clem_host_platform.h"
#include <emscripten.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

extern void clemens_host_import_disk(int driveIndex, bool isSmart, const char *path);

// Define clemens_prompt_disk directly in C using EM_JS
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
