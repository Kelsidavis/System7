*** Begin Patch
*** Update File: src/FS/vfs.c
@@
 VFS_SetCatEntryInfo(VRefNum vref, FileID id,
                          uint32_t type, uint32_t creator, uint16_t flags) {
@@
     oe->id = id;
     oe->entry = catEntry;
     oe->entry.type = type;
     oe->entry.creator = creator;
+    /* Keep printable string forms in sync */
+    OSTypeToString(creator, oe->entry.creator_str, sizeof(oe->entry.creator_str));
+    OSTypeToString(type, oe->entry.type_str, sizeof(oe->entry.type_str));
     oe->entry.flags = flags;
     return true;
 }
*** End Patch
