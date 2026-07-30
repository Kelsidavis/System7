*** Begin Patch
*** Add File: include/FS/ostype_utils.h
+#pragma once
+
+#include <stdint.h>
+
+void OSTypeToString(uint32_t code, char *out, size_t outLen);
+uint32_t StringToOSType(const char *s);
+
*** End Patch
