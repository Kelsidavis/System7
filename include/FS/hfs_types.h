--- a/include/FS/hfs_types.h
+++ b/include/FS/hfs_types.h
@@
 typedef struct {
     char     name[32];      /* ASCII/UTF-8 converted from MacRoman */
     NodeKind kind;
-    uint32_t creator;       /* OSType - 4-char code */
-    uint32_t type;          /* OSType - 4-char code */
+    uint32_t creator;       /* OSType - 4-char code */
+    uint32_t type;          /* OSType - 4-char code */
+    char     creator_str[8];/* NUL-terminated printable representation (UTF-8) */
+    char     type_str[8];   /* NUL-terminated printable representation (UTF-8) */
     uint32_t size;          /* Data fork size */
     uint16_t flags;         /* Finder flags */
     uint32_t modTime;       /* Modification time - seconds since 1904 */
     uint32_t createTime;    /* Creation time - seconds since 1904 */
     DirID    parent;        /* Parent directory CNID */
     FileID   id;            /* This node's CNID */
 } CatEntry;
