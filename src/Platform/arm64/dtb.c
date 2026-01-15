/*
 * ARM64 Device Tree Blob Parser
 * Simple DTB parser for Raspberry Pi 3/4/5
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* String function declarations */
extern size_t strlen(const char *s);
extern int strcmp(const char *s1, const char *s2);

/* DTB header structure */
typedef struct {
    uint32_t magic;             /* 0xD00DFEED */
    uint32_t totalsize;         /* Total size of DTB */
    uint32_t off_dt_struct;     /* Offset to structure block */
    uint32_t off_dt_strings;    /* Offset to strings block */
    uint32_t off_mem_rsvmap;    /* Offset to memory reserve map */
    uint32_t version;           /* DTB version */
    uint32_t last_comp_version; /* Last compatible version */
    uint32_t boot_cpuid_phys;   /* Physical CPU ID */
    uint32_t size_dt_strings;   /* Size of strings block */
    uint32_t size_dt_struct;    /* Size of structure block */
} dtb_header_t;

/* DTB tokens */
#define FDT_BEGIN_NODE  0x00000001
#define FDT_END_NODE    0x00000002
#define FDT_PROP        0x00000003
#define FDT_NOP         0x00000004
#define FDT_END         0x00000009

/* DTB magic number */
#define DTB_MAGIC       0xD00DFEED

/* Pointer to DTB */
static void *dtb_ptr = NULL;
static dtb_header_t *dtb_header = NULL;

/*
 * Swap bytes for big-endian to little-endian conversion
 */
static uint32_t bswap32(uint32_t val) {
    return ((val & 0xFF000000) >> 24) |
           ((val & 0x00FF0000) >> 8)  |
           ((val & 0x0000FF00) << 8)  |
           ((val & 0x000000FF) << 24);
}

/*
 * Initialize DTB parser
 */
bool dtb_init(void *dtb) {
    if (!dtb) {
        return false;
    }

    dtb_ptr = dtb;
    dtb_header = (dtb_header_t *)dtb;

    /* Verify magic number */
    uint32_t magic = bswap32(dtb_header->magic);
    if (magic != DTB_MAGIC) {
        dtb_ptr = NULL;
        dtb_header = NULL;
        return false;
    }

    return true;
}

/*
 * Get DTB version
 */
uint32_t dtb_get_version(void) {
    if (!dtb_header) return 0;
    return bswap32(dtb_header->version);
}

/*
 * Get DTB total size
 */
uint32_t dtb_get_size(void) {
    if (!dtb_header) return 0;
    return bswap32(dtb_header->totalsize);
}

/*
 * Find a property in the DTB
 * Returns property value and length, or NULL if not found
 */
const void *dtb_get_property(const char *node_path, const char *property, uint32_t *len) {
    if (!dtb_header || !node_path || !property) return NULL;

    uint32_t *struct_ptr = (uint32_t *)((uint8_t *)dtb_ptr + bswap32(dtb_header->off_dt_struct));
    const char *strings = (const char *)dtb_ptr + bswap32(dtb_header->off_dt_strings);

    /* Skip leading slash in path */
    if (node_path[0] == '/') node_path++;

    /* Current depth in tree */
    int depth = 0;
    bool in_target_node = false;

    /* Target depth for matching:
     * - Empty path ("") = root node, match at depth 0
     * - Non-empty path = direct child of root, match at depth 1 */
    int target_depth = (node_path[0] == '\0') ? 0 : 1;
    int matched_depth = -1;

    while (1) {
        uint32_t token = bswap32(*struct_ptr++);

        switch (token) {
            case FDT_BEGIN_NODE: {
                const char *name = (const char *)struct_ptr;
                int name_len = strlen(name);

                /* Check if this is our target node (at depth 1 = direct child of root) */
                if (depth == target_depth && strcmp(name, node_path) == 0) {
                    in_target_node = true;
                    matched_depth = depth;
                }

                /* Align to 4-byte boundary */
                struct_ptr = (uint32_t *)(((uintptr_t)struct_ptr + name_len + 1 + 3) & ~3);
                depth++;
                break;
            }

            case FDT_END_NODE:
                depth--;
                if (in_target_node && depth == matched_depth) {
                    in_target_node = false;
                    matched_depth = -1;
                }
                break;

            case FDT_PROP: {
                uint32_t prop_len = bswap32(*struct_ptr++);
                uint32_t nameoff = bswap32(*struct_ptr++);
                const char *prop_name = strings + nameoff;
                const void *prop_value = struct_ptr;

                /* Check if this is the property we're looking for */
                if (in_target_node && strcmp(prop_name, property) == 0) {
                    if (len) *len = prop_len;
                    return prop_value;
                }

                /* Align to 4-byte boundary */
                struct_ptr = (uint32_t *)(((uintptr_t)struct_ptr + prop_len + 3) & ~3);
                break;
            }

            case FDT_NOP:
                break;

            case FDT_END:
                return NULL;

            default:
                return NULL;
        }
    }
}

/*
 * Get a 32-bit property value
 */
bool dtb_get_property_u32(const char *node_path, const char *property, uint32_t *value) {
    uint32_t len;
    const uint32_t *prop = dtb_get_property(node_path, property, &len);

    if (!prop || len < 4) return false;

    *value = bswap32(*prop);
    return true;
}

/*
 * Get memory information from DTB
 */
bool dtb_get_memory(uint64_t *base, uint64_t *size) {
    if (!dtb_header) return false;

    /* Look for memory node - try various common paths */
    uint32_t len;
    const uint32_t *reg = dtb_get_property("memory", "reg", &len);

    if (!reg || len < 8) {
        reg = dtb_get_property("memory@0", "reg", &len);
    }
    if (!reg || len < 8) {
        /* QEMU virt machine uses memory@40000000 */
        reg = dtb_get_property("memory@40000000", "reg", &len);
    }
    if (!reg || len < 8) {
        return false;
    }

    /* Parse reg property (address-cells and size-cells are typically 2 for 64-bit) */
    if (len == 16) {
        /* 64-bit address and size */
        uint64_t addr_high = bswap32(reg[0]);
        uint64_t addr_low = bswap32(reg[1]);
        uint64_t size_high = bswap32(reg[2]);
        uint64_t size_low = bswap32(reg[3]);

        *base = (addr_high << 32) | addr_low;
        *size = (size_high << 32) | size_low;
    } else if (len == 8) {
        /* 32-bit address and size */
        *base = bswap32(reg[0]);
        *size = bswap32(reg[1]);
    } else {
        return false;
    }

    return true;
}

/*
 * Get model string from DTB
 */
const char *dtb_get_model(void) {
    uint32_t len;
    const char *model = dtb_get_property("", "model", &len);
    return model;
}

/*
 * Check if DTB is initialized
 */
bool dtb_is_initialized(void) {
    return dtb_header != NULL;
}
