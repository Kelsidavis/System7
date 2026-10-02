/*
 * ARM Device Tree Parser
 * Parses Device Tree Blob (DTB) provided by bootloader
 * on Raspberry Pi 3/4/5
 *
 * Supports FDT (Flattened Device Tree) format
 * Extracts model, memory, CPU, and clock information
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include "System71StdLib.h"
#include "device_tree.h"

/* FDT (Flattened Device Tree) Format Tokens */
#define FDT_BEGIN_NODE      0x00000001
#define FDT_END_NODE        0x00000002
#define FDT_PROP            0x00000003
#define FDT_NOP             0x00000004
#define FDT_END             0x00000009

/* Device Tree Blob (DTB) header structure */
typedef struct {
    uint32_t magic;              /* 0xd00dfeed */
    uint32_t totalsize;
    uint32_t off_dt_struct;
    uint32_t off_dt_strings;
    uint32_t off_mem_rsvmap;
    uint32_t version;
    uint32_t last_comp_version;
    uint32_t boot_cpuid_phys;
    uint32_t size_dt_strings;
    uint32_t size_dt_struct;
} device_tree_header_t;

/* Device tree extracted info */
typedef struct {
    char model[256];
    uint32_t memory_size;
    uint32_t cpu_count;
    uint32_t cpu_freq;
    char bootargs[256];
} device_tree_info_t;

/* Current device tree pointer and cached info */
static device_tree_header_t *device_tree = NULL;
static device_tree_info_t device_info = {0};
static int device_info_cached = 0;

/* Big-endian read helpers */
static inline uint32_t be32(const void *ptr) {
    const uint8_t *bytes = (const uint8_t *)ptr;
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
           ((uint32_t)bytes[2] << 8) | (uint32_t)bytes[3];
}

static inline uint32_t be16(const void *ptr) {
    const uint8_t *bytes = (const uint8_t *)ptr;
    return ((uint32_t)bytes[0] << 8) | (uint32_t)bytes[1];
}

/*
 * Get property from current node (internal parser)
 */
static const void* device_tree_get_node_property(const char *prop_name,
                                                   uint32_t *prop_len) {
    if (!device_tree || !prop_name) return NULL;

    const uint8_t *struct_base = (uint8_t *)device_tree + device_tree->off_dt_struct;
    const uint8_t *strings_base = (uint8_t *)device_tree + device_tree->off_dt_strings;
    const uint8_t *ptr = struct_base;
    const uint8_t *end = struct_base + device_tree->size_dt_struct;

    while (ptr < end) {
        uint32_t token = be32(ptr);
        ptr += 4;

        if (token == FDT_BEGIN_NODE) {
            /* Skip node name (null-terminated) */
            while (ptr < end && *ptr != 0) ptr++;
            ptr++;
            /* Align to 4 bytes */
            while ((uintptr_t)ptr & 3) ptr++;

        } else if (token == FDT_PROP) {
            uint32_t len = be32(ptr);
            uint32_t nameoff = be32(ptr + 4);
            ptr += 8;

            const char *name = (const char *)(strings_base + nameoff);
            const void *value = ptr;

            if (strcmp(name, prop_name) == 0) {
                *prop_len = len;
                return value;
            }

            ptr += len;
            /* Align to 4 bytes */
            while ((uintptr_t)ptr & 3) ptr++;

        } else if (token == FDT_END_NODE || token == FDT_END || token == FDT_NOP) {
            if (token == FDT_END) break;
        }
    }

    return NULL;
}

/*
 * Initialize device tree from bootloader
 * Called early in boot with DTB pointer from firmware
 */
void device_tree_init(void *dtb_ptr) {
    if (!dtb_ptr) {
        Serial_WriteString("[DTB] No device tree provided\n");
        return;
    }

    device_tree = (device_tree_header_t *)dtb_ptr;

    /* Validate DTB magic number */
    if (device_tree->magic != 0xedfe0dd0) {  /* Byte-swapped for little-endian ARM */
        Serial_WriteString("[DTB] Invalid device tree magic (0x%lx)\n",
                           (unsigned long)device_tree->magic);
        device_tree = NULL;
        return;
    }

    Serial_Printf("[DTB] Device tree found (size: %lu bytes, version: %lu)\n",
                  (unsigned long)device_tree->totalsize,
                  (unsigned long)device_tree->version);

    /* Cache device info */
    memset(&device_info, 0, sizeof(device_info));

    /* Try to extract model string */
    uint32_t model_len = 0;
    const char *model = (const char *)device_tree_get_node_property("model", &model_len);
    if (model && model_len > 0 && model_len < sizeof(device_info.model)) {
        strncpy(device_info.model, model, model_len - 1);
        device_info.model[model_len - 1] = '\0';
        Serial_Printf("[DTB] Model: %s\n", device_info.model);
    }

    /* Update peripheral base addresses from compatibility list */
    uint32_t compat_len = 0;
    const char *compat = (const char *)device_tree_get_node_property("compatible", &compat_len);
    if (compat && compat_len > 0) {
        const char *p = compat;
        const char *end = compat + compat_len;
        while (p < end && *p) {
            if (strstr(p, "raspberrypi")) {
                serial_set_pl011_base(0x3F201000u);
                break;
            } else if (strstr(p, "arm,virt")) {
                serial_set_pl011_base(0x09000000u);
                break;
            }
            p += strlen(p) + 1;
        }
    }

    device_info_cached = 1;
}

/*
 * Get model string from device tree
 */
const char *device_tree_get_model(void) {
    if (device_info_cached && device_info.model[0] != '\0') {
        return device_info.model;
    }
    return NULL;
}

/*
 * Get memory size from device tree
 * Looks for /memory node's "reg" property
 */
uint32_t device_tree_get_memory_size(void) {
    if (!device_tree) {
        return 512 * 1024 * 1024;  /* 512MB default */
    }

    /* Typical Raspberry Pi configurations:
     * Pi 3:  1GB (0x40000000)
     * Pi 4:  1GB-8GB (typically 4GB = 0x100000000, but older may be 1GB)
     * Pi 5:  4GB-8GB
     *
     * The /memory node should contain the actual RAM size.
     * For now, use heuristic detection based on DTB model.
     */

    const char *model = device_tree_get_model();
    if (model) {
        /* Try to guess from model string */
        if (strstr(model, "virt")) {
            return 1 * 1024 * 1024 * 1024;
        } else if (strstr(model, "Pi 3") || strstr(model, "3B")) {
            return 1 * 1024 * 1024 * 1024;  /* Pi 3: 1GB */
        } else if (strstr(model, "Pi 4") || strstr(model, "4B")) {
            uint64_t mem = 4ULL * 1024 * 1024 * 1024;
            if (mem > UINT32_MAX) {
                mem = UINT32_MAX;
            }
            return (uint32_t)mem;          /* Pi 4: approx 4GB */
        } else if (strstr(model, "Pi 5") || strstr(model, "5B")) {
            uint64_t mem = 8ULL * 1024 * 1024 * 1024;
            if (mem > UINT32_MAX) {
                mem = UINT32_MAX;
            }
            return (uint32_t)mem;          /* Pi 5: approx 8GB */
        }
    }

    return 512 * 1024 * 1024;  /* Default fallback: 512MB */
}

/*
 * Get device tree property string value
 */
const char *device_tree_get_property_string(const char *node, const char *prop) {
    (void)node;  /* Reserved for full node path support */
    (void)prop;
    /* For now, only root properties supported */
    uint32_t len = 0;
    return (const char *)device_tree_get_node_property(prop, &len);
}

/*
 * Get device tree property integer value
 */
uint32_t device_tree_get_property_u32(const char *node, const char *prop, uint32_t default_val) {
    (void)node;  /* Reserved for full node path support */

    uint32_t len = 0;
    const uint8_t *value = (const uint8_t *)device_tree_get_node_property(prop, &len);

    if (value && len >= 4) {
        return be32(value);
    }

    return default_val;
}

/*
 * Find nodes with matching compatible string and return their reg ranges
 */
size_t device_tree_find_compatible(const char *compatible,
                                   device_tree_reg_t *regs,
                                   size_t max_entries) {
    if (!device_tree || !compatible || !regs || max_entries == 0) {
        return 0;
    }

    const uint8_t *struct_base = (uint8_t *)device_tree + device_tree->off_dt_struct;
    const uint8_t *strings_base = (uint8_t *)device_tree + device_tree->off_dt_strings;
    const uint8_t *ptr = struct_base;
    const uint8_t *end = struct_base + device_tree->size_dt_struct;

    typedef struct {
        bool compat_match;
        bool has_reg;
        uint64_t base;
        uint64_t size;
    } node_state_t;

    enum { kMaxDepth = 32 };
    node_state_t stack[kMaxDepth];
    int depth = 0;
    size_t count = 0;

    while (ptr < end) {
        uint32_t token = be32(ptr);
        ptr += 4;

        if (token == FDT_BEGIN_NODE) {
            if (depth >= kMaxDepth) {
                Serial_WriteString("[DTB] Node depth exceeded parser stack\n");
                return count;
            }

            node_state_t *state = &stack[depth++];
            state->compat_match = false;
            state->has_reg = false;
            state->base = 0;
            state->size = 0;

            while (ptr < end && *ptr != 0) ptr++;
            ptr++;
            while ((uintptr_t)ptr & 3) ptr++;

        } else if (token == FDT_PROP) {
            uint32_t len = be32(ptr);
            uint32_t nameoff = be32(ptr + 4);
            ptr += 8;

            const char *name = (const char *)(strings_base + nameoff);
            const uint8_t *value = ptr;
            ptr += len;
            while ((uintptr_t)ptr & 3) ptr++;

            if (depth == 0) {
                continue;
            }

            node_state_t *state = &stack[depth - 1];

            if (strcmp(name, "compatible") == 0) {
                size_t offset = 0;
                while (offset < len) {
                    const char *cmp = (const char *)(value + offset);
                    size_t cmp_len = strlen(cmp);
                    if (cmp_len == 0) {
                        offset++;
                        continue;
                    }
                    if (strcmp(cmp, compatible) == 0) {
                        state->compat_match = true;
                        break;
                    }
                    offset += cmp_len + 1;
                }
            } else if (strcmp(name, "reg") == 0) {
                if (len >= 16) {
                    uint64_t upper_addr = (uint64_t)be32(value);
                    uint64_t lower_addr = (uint64_t)be32(value + 4);
                    uint64_t upper_size = (uint64_t)be32(value + 8);
                    uint64_t lower_size = (uint64_t)be32(value + 12);
                    state->base = (upper_addr << 32) | lower_addr;
                    state->size = (upper_size << 32) | lower_size;
                    state->has_reg = true;
                } else if (len >= 8) {
                    state->base = be32(value);
                    state->size = be32(value + 4);
                    state->has_reg = true;
                }
            }

        } else if (token == FDT_END_NODE) {
            if (depth == 0) {
                continue;
            }

            node_state_t *state = &stack[depth - 1];
            if (state->compat_match && state->has_reg && count < max_entries) {
                regs[count].base = state->base;
                regs[count].size = state->size;
                count++;
            }
            depth--;

        } else if (token == FDT_END) {
            break;
        }
    }

    return count;
}

/*
 * Dump device tree info (debugging)
 */
void device_tree_dump(void) {
    if (!device_tree) {
        Serial_WriteString("[DTB] No device tree available\n");
        return;
    }

    Serial_Printf("[DTB] Total size: %lu bytes\n", (unsigned long)device_tree->totalsize);
    Serial_Printf("[DTB] Structure offset: 0x%lx\n", (unsigned long)device_tree->off_dt_struct);
    Serial_Printf("[DTB] Strings offset: 0x%lx\n", (unsigned long)device_tree->off_dt_strings);
    Serial_Printf("[DTB] Version: %lu\n", (unsigned long)device_tree->version);
    Serial_Printf("[DTB] Boot CPU: %lu\n", (unsigned long)device_tree->boot_cpuid_phys);

    if (device_info_cached) {
        if (device_info.model[0] != '\0') {
            Serial_Printf("[DTB] Model: %s\n", device_info.model);
        }
        if (device_info.memory_size > 0) {
            Serial_Printf("[DTB] Memory: %lu MB\n",
                          (unsigned long)(device_info.memory_size / (1024 * 1024)));
        }
        if (device_info.cpu_freq > 0) {
            Serial_Printf("[DTB] CPU Freq: %lu MHz\n",
                          (unsigned long)(device_info.cpu_freq / 1000000));
        }
    }
}
