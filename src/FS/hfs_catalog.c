*** Begin Patch
*** Update File: src/FS/hfs_catalog.c
@@
     /* Convert on-disk name bytes (now treated as UTF-8) to NUL-terminated C string */
     HFS_NameToUTF8(entry->name, key->name, key->nameLength, sizeof(entry->name));
+
+    /* Initialize printable type/creator strings empty; populated elsewhere */
+    if (sizeof(entry->creator_str) > 0) entry->creator_str[0] = '\0';
+    if (sizeof(entry->type_str) > 0) entry->type_str[0] = '\0';
*** End Patch
