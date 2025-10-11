/* System 7X Nanokernel - VFS Path Resolution Implementation
 *
 * POSIX-style path normalization and resolution.
 */

#include "../../../include/Nanokernel/vfs_path.h"
#include "../../../include/Nanokernel/vfs.h"
#include "../../../include/System71StdLib.h"
#include <string.h>
#include <ctype.h>

/* Simple string token state for strtok */
static char* strtok_state = NULL;

/* Simple strtok implementation */
static char* simple_strtok(char* str, const char* delim) {
    if (str) strtok_state = str;
    if (!strtok_state) return NULL;

    /* Skip leading delimiters */
    while (*strtok_state && strchr(delim, *strtok_state)) {
        strtok_state++;
    }

    if (!*strtok_state) return NULL;

    char* token = strtok_state;

    /* Find next delimiter */
    while (*strtok_state && !strchr(delim, *strtok_state)) {
        strtok_state++;
    }

    if (*strtok_state) {
        *strtok_state = '\0';
        strtok_state++;
    }

    return token;
}

/* Simple strrchr implementation */
static const char* simple_strrchr(const char* s, int c) {
    const char* last = NULL;
    while (*s) {
        if (*s == (char)c) last = s;
        s++;
    }
    return last;
}

/* Simple strncat implementation */
static char* simple_strncat(char* dest, const char* src, size_t n) {
    char* d = dest + strlen(dest);
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        d[i] = src[i];
    }
    d[i] = '\0';
    return dest;
}

/* Current working directory (global for now, TODO: per-process) */
static char g_cwd[256] = "/Volumes/BOOT";

/* Initialize path resolution subsystem */
void vfs_path_init(void) {
    serial_printf("[VFS-PATH] Path resolution subsystem initialized\n");
    serial_printf("[VFS-PATH] CWD: %s\n", g_cwd);
}

/* Normalize a path (resolve . and ..) */
bool vfs_normalize_path(const char* path, char* normalized, size_t max_len) {
    if (!path || !normalized || max_len == 0) {
        return false;
    }

    char temp[256];
    strncpy(temp, path, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    /* Split into components */
    char* components[64];
    int count = 0;

    char* token = simple_strtok(temp, "/");
    while (token && count < 64) {
        if (strcmp(token, ".") == 0) {
            /* Skip current directory */
            token = simple_strtok(NULL, "/");
            continue;
        } else if (strcmp(token, "..") == 0) {
            /* Go up one level */
            if (count > 0) {
                count--;
            }
        } else if (strlen(token) > 0) {
            /* Add component */
            components[count++] = token;
        }
        token = simple_strtok(NULL, "/");
    }

    /* Rebuild path */
    normalized[0] = '\0';
    if (path[0] == '/') {
        strcat(normalized, "/");
    }

    for (int i = 0; i < count; i++) {
        if (i > 0 || path[0] == '/') {
            if (strlen(normalized) > 1) {  /* Don't add / after root / */
                strcat(normalized, "/");
            }
        }
        size_t remain = max_len - strlen(normalized) - 1;
        if (remain > 0) {
            simple_strncat(normalized, components[i], remain);
        }
    }

    /* Handle empty path */
    if (normalized[0] == '\0') {
        if (path[0] == '/') {
            strcat(normalized, "/");
        } else {
            strcat(normalized, ".");
        }
    }

    return true;
}

/* Split path into volume name and relative path */
bool vfs_split_path(const char* path, char* volume_name, size_t vol_len,
                    char* rel_path, size_t rel_len) {
    if (!path || !volume_name || !rel_path) {
        return false;
    }

    volume_name[0] = '\0';
    rel_path[0] = '\0';

    /* Check for /Volumes/NAME/... format */
    if (strncmp(path, "/Volumes/", 9) == 0) {
        const char* vol_start = path + 9;
        const char* slash = strchr(vol_start, '/');

        if (slash) {
            /* Extract volume name */
            size_t name_len = slash - vol_start;
            if (name_len < vol_len) {
                strncpy(volume_name, vol_start, name_len);
                volume_name[name_len] = '\0';
            }

            /* Extract relative path */
            strncpy(rel_path, slash + 1, rel_len - 1);
            rel_path[rel_len - 1] = '\0';
        } else {
            /* Just volume name, no path */
            strncpy(volume_name, vol_start, vol_len - 1);
            volume_name[vol_len - 1] = '\0';
            strcpy(rel_path, "/");
        }

        return true;
    }

    /* Check for absolute path without /Volumes */
    if (path[0] == '/') {
        /* Use default boot volume */
        strcpy(volume_name, "BOOT");
        strncpy(rel_path, path + 1, rel_len - 1);
        rel_path[rel_len - 1] = '\0';
        return true;
    }

    /* Relative path - use CWD */
    return vfs_split_path(g_cwd, volume_name, vol_len, rel_path, rel_len);
}

/* Resolve a path to a volume + inode */
bool vfs_resolve_path(const char* path, ResolvedPath* resolved) {
    if (!path || !resolved) {
        return false;
    }

    memset(resolved, 0, sizeof(ResolvedPath));

    /* Normalize path */
    char normalized[256];
    if (!vfs_normalize_path(path, normalized, sizeof(normalized))) {
        return false;
    }

    strncpy(resolved->normalized, normalized, sizeof(resolved->normalized) - 1);

    /* Split into volume and relative path */
    char volume_name[64];
    char rel_path[256];

    if (!vfs_split_path(normalized, volume_name, sizeof(volume_name),
                       rel_path, sizeof(rel_path))) {
        serial_printf("[VFS-PATH] ERROR: Failed to split path '%s'\n", path);
        return false;
    }

    /* Find volume */
    extern VFSVolume* MVFS_GetVolumeByName(const char* name);
    resolved->volume = MVFS_GetVolumeByName(volume_name);

    if (!resolved->volume) {
        serial_printf("[VFS-PATH] ERROR: Volume '%s' not found\n", volume_name);
        return false;
    }

    serial_printf("[VFS-PATH] Resolved '%s' → volume='%s', rel_path='%s'\n",
                  path, volume_name, rel_path);

    /* TODO: Use VFS lookup to find inode */
    /* For now, assume root directory (inode 1) */
    resolved->inode = 1;
    resolved->is_directory = true;
    resolved->exists = true;

    return true;
}

/* Join path components */
bool vfs_join_path(const char* base, const char* rel, char* result, size_t max_len) {
    if (!base || !rel || !result || max_len == 0) {
        return false;
    }

    /* Copy base */
    strncpy(result, base, max_len - 1);
    result[max_len - 1] = '\0';

    /* Add separator if needed */
    size_t len = strlen(result);
    if (len > 0 && result[len - 1] != '/') {
        if (len + 1 < max_len) {
            result[len] = '/';
            result[len + 1] = '\0';
        }
    }

    /* Append relative path */
    size_t remain = max_len - strlen(result) - 1;
    if (remain > 0) {
        simple_strncat(result, rel, remain);
    }

    return true;
}

/* Get basename from path */
const char* vfs_basename(const char* path) {
    if (!path || path[0] == '\0') {
        return ".";
    }

    const char* last_slash = simple_strrchr(path, '/');
    if (last_slash) {
        return last_slash + 1;
    }

    return path;
}

/* Get dirname from path */
bool vfs_dirname(const char* path, char* result, size_t max_len) {
    if (!path || !result || max_len == 0) {
        return false;
    }

    const char* last_slash = simple_strrchr(path, '/');
    if (!last_slash) {
        strcpy(result, ".");
        return true;
    }

    /* Handle root directory */
    if (last_slash == path) {
        strcpy(result, "/");
        return true;
    }

    /* Copy up to (but not including) last slash */
    size_t len = last_slash - path;
    if (len >= max_len) {
        len = max_len - 1;
    }

    strncpy(result, path, len);
    result[len] = '\0';

    return true;
}

/* Check if path is absolute */
bool vfs_is_absolute(const char* path) {
    return (path && path[0] == '/');
}

/* Convert relative path to absolute */
bool vfs_make_absolute(const char* rel_path, const char* cwd, char* abs_path, size_t max_len) {
    if (!rel_path || !cwd || !abs_path || max_len == 0) {
        return false;
    }

    /* If already absolute, just normalize */
    if (vfs_is_absolute(rel_path)) {
        return vfs_normalize_path(rel_path, abs_path, max_len);
    }

    /* Join with CWD */
    char joined[512];
    if (!vfs_join_path(cwd, rel_path, joined, sizeof(joined))) {
        return false;
    }

    /* Normalize result */
    return vfs_normalize_path(joined, abs_path, max_len);
}
