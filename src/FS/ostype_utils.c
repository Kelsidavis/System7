*** Begin Patch
*** Add File: src/FS/ostype_utils.c
+/* Utilities to convert between 4-byte OSType codes and NUL-terminated strings
+ * This allows treating creator/type codes as printable UTF-8 where appropriate.
+ */
+
+#include <stdint.h>
+#include <string.h>
+
+void OSTypeToString(uint32_t code, char *out, size_t outLen) {
+    if (!out || outLen == 0) return;
+    if (code == 0) {
+        out[0] = '\0';
+        return;
+    }
+    // Pack as 4 ASCII bytes
+    char tmp[5];
+    tmp[0] = (char)((code >> 24) & 0xFF);
+    tmp[1] = (char)((code >> 16) & 0xFF);
+    tmp[2] = (char)((code >> 8) & 0xFF);
+    tmp[3] = (char)(code & 0xFF);
+    tmp[4] = '\0';
+    // Copy safely
+    strncpy(out, tmp, outLen - 1);
+    out[outLen - 1] = '\0';
+}
+
+uint32_t StringToOSType(const char *s) {
+    if (!s) return 0;
+    size_t len = strlen(s);
+    uint32_t code = 0;
+    // Pack up to 4 bytes; if shorter, pad with spaces (classic Mac behaviour)
+    char buf[4] = {' ', ' ', ' ', ' '};
+    for (size_t i = 0; i < 4 && i < len; ++i) buf[i] = s[i];
+    code = ((uint32_t)(uint8_t)buf[0] << 24) |
+           ((uint32_t)(uint8_t)buf[1] << 16) |
+           ((uint32_t)(uint8_t)buf[2] << 8)  |
+           ((uint32_t)(uint8_t)buf[3]);
+    return code;
+}
+
*** End Patch
