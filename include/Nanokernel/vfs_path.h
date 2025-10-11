/* System 7X Nanokernel - VFS Path Resolution
 *
 * POSIX-style path normalization and resolution with mount point support.
 * Supports:
 * - Absolute paths: /Volumes/DISK/System/Finder
 * - Relative paths: ../Desktop/file.txt
 * - Path normalization: . and .. components
 * - Case-insensitive (HFS) vs case-sensitive (FAT32) lookups
 */

#ifndef NANOKERNEL_VFS_PATH_H
#define NANOKERNEL_VFS_PATH_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Forward declarations */
typedef struct VFSVolume VFSVolume;

/* Resolved path structure */
typedef struct {
    VFSVolume*  volume;          /* Target volume */
    uint64_t    inode;           /* Target inode/catalog node */
    char        normalized[256]; /* Normalized path */
    bool        is_directory;    /* Is target a directory? */
    bool        exists;          /* Does target exist? */
} ResolvedPath;

/* Initialize path resolution subsystem */
void vfs_path_init(void);

/* Resolve a path to a volume + inode */
bool vfs_resolve_path(const char* path, ResolvedPath* resolved);

/* Normalize a path (resolve . and ..) */
bool vfs_normalize_path(const char* path, char* normalized, size_t max_len);

/* Split path into volume name and relative path */
bool vfs_split_path(const char* path, char* volume_name, size_t vol_len,
                    char* rel_path, size_t rel_len);

/* Join path components */
bool vfs_join_path(const char* base, const char* rel, char* result, size_t max_len);

/* Get basename from path */
const char* vfs_basename(const char* path);

/* Get dirname from path */
bool vfs_dirname(const char* path, char* result, size_t max_len);

/* Check if path is absolute */
bool vfs_is_absolute(const char* path);

/* Convert relative path to absolute */
bool vfs_make_absolute(const char* rel_path, const char* cwd, char* abs_path, size_t max_len);

#endif /* NANOKERNEL_VFS_PATH_H */
