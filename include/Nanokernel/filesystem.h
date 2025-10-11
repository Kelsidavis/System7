/* System 7X Nanokernel - Filesystem Driver Interface
 *
 * This header defines the interface that filesystem drivers must implement
 * to integrate with the VFS layer. Each filesystem (HFS, FAT32, ext2, ISO9660)
 * provides a FileSystemOps structure with function pointers for all operations.
 *
 * The VFS core uses these operations to interact with filesystem implementations
 * in a uniform way, allowing easy addition of new filesystem support.
 */

#ifndef NK_FILESYSTEM_H
#define NK_FILESYSTEM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Forward declarations */
typedef struct BlockDevice BlockDevice;
typedef struct VFSVolume VFSVolume;

/* Filesystem driver operations */
typedef struct FileSystemOps {
    /* Filesystem identification */
    const char* fs_name;              /* e.g., "HFS", "FAT32", "ext2" */
    uint32_t    fs_version;           /* Driver version */

    /* Probe operation - detect if block device contains this filesystem
     * Returns: true if filesystem signature is detected
     */
    bool (*probe)(BlockDevice* dev);

    /* Mount operation - initialize filesystem structures
     * Returns: filesystem-specific private data (stored in vol->fs_private)
     */
    void* (*mount)(VFSVolume* vol, BlockDevice* dev);

    /* Unmount operation - cleanup filesystem structures */
    void (*unmount)(VFSVolume* vol);

    /* Read operation - read data from file
     * Parameters:
     *   vol       - Volume context
     *   file_id   - Filesystem-specific file identifier
     *   offset    - Offset in file
     *   buffer    - Output buffer
     *   length    - Bytes to read
     *   bytes_read - Actual bytes read
     * Returns: true on success
     */
    bool (*read)(VFSVolume* vol, uint64_t file_id, uint64_t offset,
                 void* buffer, size_t length, size_t* bytes_read);

    /* Write operation - write data to file
     * Parameters:
     *   vol         - Volume context
     *   file_id     - Filesystem-specific file identifier
     *   offset      - Offset in file
     *   buffer      - Data to write
     *   length      - Bytes to write
     *   bytes_written - Actual bytes written
     * Returns: true on success
     */
    bool (*write)(VFSVolume* vol, uint64_t file_id, uint64_t offset,
                  const void* buffer, size_t length, size_t* bytes_written);

    /* Directory enumeration - list directory contents
     * Parameters:
     *   vol        - Volume context
     *   dir_id     - Directory ID
     *   callback   - Called for each entry
     *   user_data  - Passed to callback
     * Returns: true on success
     */
    bool (*enumerate)(VFSVolume* vol, uint64_t dir_id,
                      bool (*callback)(void* user_data, const char* name,
                                       uint64_t id, bool is_dir),
                      void* user_data);

    /* Lookup operation - find entry by name in directory
     * Parameters:
     *   vol     - Volume context
     *   dir_id  - Parent directory ID
     *   name    - Entry name to find
     *   entry_id - Output: found entry ID
     *   is_dir  - Output: true if entry is directory
     * Returns: true if found
     */
    bool (*lookup)(VFSVolume* vol, uint64_t dir_id, const char* name,
                   uint64_t* entry_id, bool* is_dir);

    /* Get filesystem statistics
     * Parameters:
     *   vol         - Volume context
     *   total_bytes - Output: total volume size
     *   free_bytes  - Output: free space
     * Returns: true on success
     */
    bool (*get_stats)(VFSVolume* vol, uint64_t* total_bytes, uint64_t* free_bytes);

    /* Optional operations (can be NULL) */

    /* Format operation - create new filesystem on block device */
    bool (*format)(BlockDevice* dev, const char* volume_name);

    /* Create directory */
    bool (*mkdir)(VFSVolume* vol, uint64_t parent_dir_id, const char* name,
                  uint64_t* new_dir_id);

    /* Create file */
    bool (*create_file)(VFSVolume* vol, uint64_t parent_dir_id, const char* name,
                        uint64_t* new_file_id);

    /* Delete entry (file or directory) */
    bool (*delete)(VFSVolume* vol, uint64_t entry_id);

    /* Rename entry */
    bool (*rename)(VFSVolume* vol, uint64_t entry_id, const char* new_name);

} FileSystemOps;

#endif /* NK_FILESYSTEM_H */
