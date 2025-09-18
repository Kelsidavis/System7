#!/bin/bash
# Update copyright headers to MIT License for System7 Project

LICENSE_HEADER="/*
 * Copyright (c) 2024 System7 Project
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the \"Software\"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */"

# Update files with simple headers
find src include -name "*.c" -o -name "*.h" | while read file; do
    # Check if file already has copyright
    if ! grep -q "Copyright.*2024.*System7" "$file"; then
        # Create temp file with new header
        echo "$LICENSE_HEADER" > "$file.tmp"
        echo "" >> "$file.tmp"
        
        # Skip existing comment headers if present
        awk '/^\/\*/{p=1} p&&/\*\//{p=0;next} !p' "$file" >> "$file.tmp"
        
        mv "$file.tmp" "$file"
        echo "Updated: $file"
    fi
done

echo "Copyright update complete"
