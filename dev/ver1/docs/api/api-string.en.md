# String Processing Library

> String manipulation, search, replace, and encoding/decoding functions

[English](api-string.en.md) | [中文](api-string.md) | [Back to Index](README.en.md)

---

## 📑 Table of Contents

- [Constant Definitions](#constant-definitions)
- [String Copying](#string-copying)
- [String Comparison](#string-comparison)
- [Case Conversion](#case-conversion)
- [String Search](#string-search)
- [Wildcard Matching](#wildcard-matching)
- [String Trimming](#string-trimming)
- [String Filtering](#string-filtering)
- [String Operations](#string-operations)
- [Number Formatting](#number-formatting)
- [String Similarity](#string-similarity)
- [String Approximate Equal](#string-approximate-equal)
- [Encoding and Decoding](#encoding-and-decoding)
- [UTF-8 Utility Functions](#utf-8-utility-functions)
- [Use Cases](#use-cases)
- [Best Practices](#best-practices)

---

## Constant Definitions

### RandStringDefaultTemplate

Default template for random strings (internal use).

**Definition:**
```c
static const str RandStringDefaultTemplate = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_";
```

**Description:**
- Used when `sTemplate` parameter of `xrtRandStr` is `NULL`
- Contains 64 characters: digits + uppercase letters + lowercase letters + hyphen + underscore

### Base64EncodeTable

Standard Base64 encoding table (internal use).

**Definition:**
```c
static const str Base64EncodeTable = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
```

**Description:**
- Used when `sTable` parameter of `xrtBase64Encode` / `xrtBase64Decode` is `NULL`

---

## String Copying

### xrtCopyStr

Copy UTF-8 string.

**Prototype:**
```c
XXAPI str xrtCopyStr(str sText, size_t iSize);
```

**Parameters:**
- `sText` - Source string
- `iSize` - Byte length (0 for auto-calculate)

**Return Value:**
- Newly allocated string copy
- Returns the empty-string sentinel on failure

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str original = (str)"Hello World";
    str copy = xrtCopyStr(original, 0);
    
    printf("Original: %s\n", original);
    printf("Copy: %s\n", copy);
	printf("Length: %zu\n", strlen((char*)copy));
    
    xrtFree(copy);
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- If `sText` is `NULL` or length is 0, returns the empty-string sentinel
- Returned string has `\0` terminator automatically added

---

### xrtCopyStrU16

Copy UTF-16 string.

**Prototype:**
```c
XXAPI u16str xrtCopyStrU16(u16str sText, size_t iSize);
```

**Parameters:**
- `sText` - Source UTF-16 string
- `iSize` - Character count (not bytes, 0 for auto-calculate)

**Return Value:**
- Newly allocated UTF-16 string copy

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
	u16str original = (u16str)L"Hello";
	u16str copy = xrtCopyStrU16(original, 0);
	printf("Character count: 5\n");
    
    xrtFree(copy);
    xrtUnit();
    return 0;
}
```

---

### xrtCopyStrU32

Copy UTF-32 string.

**Prototype:**
```c
XXAPI u32str xrtCopyStrU32(u32str sText, size_t iSize);
```

**Parameters:**
- `sText` - Source UTF-32 string
- `iSize` - Character count (not bytes, 0 for auto-calculate)

**Return Value:**
- Newly allocated UTF-32 string copy

**Memory Release:** ✅ Requires `xrtFree` to release

---

### xrtCopyMem

Copy memory block.

**Prototype:**
```c
XXAPI ptr xrtCopyMem(ptr pMem, size_t iSize);
```

**Parameters:**
- `pMem` - Source memory
- `iSize` - Byte count

**Return Value:**
- Newly allocated memory copy

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    uint8 data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    ptr copy = xrtCopyMem(data, sizeof(data));
    
	printf("Bytes copied: %zu\n", sizeof(data));
    
    // Verify copied content
    uint8* p = (uint8*)copy;
    for (int i = 0; i < 5; i++) {
        printf("%02X ", p[i]);
    }
    printf("\n");
    
    xrtFree(copy);
    xrtUnit();
    return 0;
}
```

---

## String Comparison

### xrtStrComp

Compare strings.

**Prototype:**
```c
XXAPI int xrtStrComp(str s1, str s2, size_t iSize, bool bCase);
```

**Parameters:**
- `s1` - String 1
- `s2` - String 2
- `iSize` - Comparison length (0 for all)
- `bCase` - Whether to ignore case
  - `TRUE` - **Ignore** case (case-insensitive)
  - `FALSE` - **Distinguish** case (case-sensitive)

**Return Value:**
- `0` - Equal
- `<0` - s1 < s2
- `>0` - s1 > s2

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Case-sensitive (bCase=FALSE)
    int r1 = xrtStrComp((str)"Hello", (str)"hello", 0, FALSE);
    printf("Case-sensitive: %d (not equal)\n", r1);
    
    // Case-insensitive (bCase=TRUE)
    int r2 = xrtStrComp((str)"Hello", (str)"hello", 0, TRUE);
    printf("Case-insensitive: %d (equal)\n", r2);
    
    // Compare first 3 characters
    int r3 = xrtStrComp((str)"Hello", (str)"Help", 3, FALSE);
    printf("First 3 chars: %d (equal)\n", r3);
    
    // Compare first 4 characters
    int r4 = xrtStrComp((str)"Hello", (str)"Help", 4, FALSE);
    printf("First 4 chars: %d (not equal)\n", r4);
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Windows uses `stricmp`/`strnicmp`
- Linux/macOS uses `strcasecmp`/`strncasecmp`

---

## Case Conversion

### xrtLCase

Convert to lowercase.

**Prototype:**
```c
XXAPI str xrtLCase(str sText, size_t iSize, bool bSrcRevise);
```

**Parameters:**
- `sText` - Source string
- `iSize` - Length (0 for auto)
- `bSrcRevise` - Whether to modify source string
  - `TRUE` - Modify source directly, return source pointer
  - `FALSE` - Create new string

**Return Value:**
- Lowercase string pointer

**Memory Release:**
- `bSrcRevise=TRUE` - ⭕ No release needed
- `bSrcRevise=FALSE` - ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Create new string
    str lower = xrtLCase((str)"HELLO WORLD", 0, FALSE);
    printf("New string: %s\n", lower);  // "hello world"
    xrtFree(lower);
    
    // Modify source string
    str text = xrtCopyStr((str)"WORLD", 0);
    xrtLCase(text, 0, TRUE);
    printf("Modified: %s\n", text);   // "world"
    xrtFree(text);
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Automatically skips OEM encoded characters (bytes with high bit set to 1)
- Only processes letters in ASCII range

---

### xrtUCase

Convert to uppercase.

**Prototype:**
```c
XXAPI str xrtUCase(str sText, size_t iSize, bool bSrcRevise);
```

**Parameters:** Same as `xrtLCase`

**Memory Release:** Same as `xrtLCase`

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str upper = xrtUCase((str)"hello world", 0, FALSE);
    printf("%s\n", upper);  // "HELLO WORLD"
    xrtFree(upper);
    
    xrtUnit();
    return 0;
}
```

---

## String Search

### xrtFindStr

Find substring.

**Prototype:**
```c
XXAPI str xrtFindStr(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bCase);
```

**Parameters:**
- `sText` - String to search in
- `iSize` - Length (0 for auto)
- `sSubText` - Substring to find
- `iSubSize` - Substring length (0 for auto)
- `bCase` - Whether to ignore case (`TRUE`=ignore)

**Return Value:**
- Found: Returns pointer to substring position
- Not found: Returns `NULL`

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str text = (str)"Hello World, Hello XRT";
    
    // Case-sensitive search
	str found = xrtFindStr(text, 0, (str)"World", 0, FALSE);
	if (found) {
		printf("Found: %s\n", found);  // "World, Hello XRT"
		printf("Position: %td\n", found - text + 1);  // 7
	}
    
    // Case-insensitive search
    found = xrtFindStr(text, 0, (str)"world", 0, TRUE);
    if (found) {
        printf("Case-insensitive found: %s\n", found);
    }
    
    // Not found
    found = xrtFindStr(text, 0, (str)"xyz", 0, FALSE);
    if (!found) {
        printf("xyz not found\n");
    }
    
    xrtUnit();
    return 0;
}
```

---

### xrtInStr

Find substring position (returns index).

**Prototype:**
```c
XXAPI uint xrtInStr(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bCase);
```

**Return Value:**
- Found: Returns position index (1-based)
- Not found: Returns 0

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str text = (str)"Hello World";
    uint pos = xrtInStr(text, 0, (str)"World", 0, FALSE);
    printf("Position: %u\n", pos);  // 7
    
    pos = xrtInStr(text, 0, (str)"xyz", 0, FALSE);
    printf("Not found: %u\n", pos);  // 0
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Internally derives the 1-based index from `xrtFindStr`

---

### xrtCheckStr

Check if string contains specified characters.

**Prototype:**
```c
XXAPI str xrtCheckStr(str sText, size_t iSize, str sSubText, size_t iSubSize);
```

**Parameters:**
- `sText` - String to check
- `iSize` - Length (0 for auto)
- `sSubText` - Character set
- `iSubSize` - Character set length (0 for auto)

**Return Value:**
- Found: Returns position of first matching character
- Not found: Returns `NULL`

**Description:**
- Checks if `sText` contains **any character** from `sSubText`
- Supports UTF-8 multi-byte characters (up to 6 bytes)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str text = (str)"Hello123World";
    
    // Check for digits
    str found = xrtCheckStr(text, 0, (str)"0123456789", 0);
    if (found) {
        printf("Found digit: %s\n", found);  // "123World"
    }
    
    // Check for special characters
    found = xrtCheckStr(text, 0, (str)"!@#$%", 0);
    if (!found) {
        printf("No special characters\n");
    }
    
    xrtUnit();
    return 0;
}
```

---

## Wildcard Matching

### xrtStrLike

Wildcard pattern matching.

**Prototype:**
```c
XXAPI bool xrtStrLike(str sText, size_t iTextSize, str sPattern, size_t iPatSize, bool bCase);
```

**Parameters:**
- `sText` - String to match
- `iTextSize` - String length (0 for auto-calculate)
- `sPattern` - Wildcard pattern
- `iPatSize` - Pattern length (0 for auto-calculate)
- `bCase` - Whether to ignore case
  - `TRUE` - **Ignore** case (case-insensitive)
  - `FALSE` - **Distinguish** case (case-sensitive)

**Wildcard Description:**
- `*` - Matches any character sequence (including empty sequence)
- `?` - Matches a single UTF-8 character (supports multi-byte characters)

**Return Value:**
- `TRUE` - Match successful
- `FALSE` - Match failed

**Algorithm Characteristics:**
- Uses greedy matching algorithm
- Time complexity: O(n*m) worst case
- Space complexity: O(1)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Basic matching
    printf("%d\n", xrtStrLike((str)"hello", 0, (str)"hello", 0, FALSE));  // 1
    printf("%d\n", xrtStrLike((str)"hello", 0, (str)"world", 0, FALSE));  // 0
    
    // * wildcard: matches any character sequence
    printf("%d\n", xrtStrLike((str)"hello", 0, (str)"*", 0, FALSE));      // 1
    printf("%d\n", xrtStrLike((str)"hello", 0, (str)"h*", 0, FALSE));     // 1
    printf("%d\n", xrtStrLike((str)"hello", 0, (str)"*o", 0, FALSE));     // 1
    printf("%d\n", xrtStrLike((str)"hello", 0, (str)"h*o", 0, FALSE));    // 1
    printf("%d\n", xrtStrLike((str)"hello", 0, (str)"*ll*", 0, FALSE));   // 1
    
    // ? wildcard: matches single character
    printf("%d\n", xrtStrLike((str)"hello", 0, (str)"h?llo", 0, FALSE));  // 1
    printf("%d\n", xrtStrLike((str)"hello", 0, (str)"?????", 0, FALSE));  // 1
    printf("%d\n", xrtStrLike((str)"hello", 0, (str)"??????", 0, FALSE)); // 0
    
    // Mixed usage
    printf("%d\n", xrtStrLike((str)"hello world", 0, (str)"h*o ?orld", 0, FALSE));  // 1
    
    // Case-insensitive
    printf("%d\n", xrtStrLike((str)"HELLO", 0, (str)"hello", 0, FALSE));  // 0
    printf("%d\n", xrtStrLike((str)"HELLO", 0, (str)"hello", 0, TRUE));   // 1
    printf("%d\n", xrtStrLike((str)"HeLLo", 0, (str)"h*O", 0, TRUE));     // 1
    
	// UTF-8 support (? matches one complete UTF-8 character)
	printf("%d\n", xrtStrLike((str)"café", 0, (str)"*fé", 0, FALSE));     // 1
	printf("%d\n", xrtStrLike((str)"café", 0, (str)"ca?é", 0, FALSE));    // 1
	printf("%d\n", xrtStrLike((str)"café", 0, (str)"????", 0, FALSE));    // 1
	printf("%d\n", xrtStrLike((str)"café", 0, (str)"?????", 0, FALSE));   // 0
    
    xrtUnit();
    return 0;
}
```

**Use Case:**
```c
#include "xrt.h"
#include <stdio.h>

// Filename matching
bool MatchFileName(str filename, str pattern) {
    return xrtStrLike(filename, 0, pattern, 0, TRUE);  // Filenames usually case-insensitive
}

int main() {
    xrtInit();
    
    // Match all .txt files
    printf("%d\n", MatchFileName((str)"readme.txt", (str)"*.txt"));      // 1
    printf("%d\n", MatchFileName((str)"README.TXT", (str)"*.txt"));      // 1
    printf("%d\n", MatchFileName((str)"document.pdf", (str)"*.txt"));    // 0
    
    // Match files starting with test
    printf("%d\n", MatchFileName((str)"test_main.c", (str)"test*"));     // 1
    printf("%d\n", MatchFileName((str)"main_test.c", (str)"test*"));     // 0
    
    // Match single character variations
    printf("%d\n", MatchFileName((str)"file1.log", (str)"file?.log"));   // 1
    printf("%d\n", MatchFileName((str)"file10.log", (str)"file?.log"));  // 0
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- `?` matches a complete UTF-8 character, not a single byte
- Empty pattern only matches empty string
- Empty string can only be matched by patterns consisting entirely of `*`
- Case conversion only effective for ASCII letters

---

## String Trimming

### xrtLTrim

Left trim.

**Prototype:**
```c
XXAPI str xrtLTrim(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bSrcRevise, size_t* iRetSize);
```

**Parameters:**
- `sText` - Source string
- `iSize` - Length (0 for auto)
- `sSubText` - Character set to trim (`NULL` defaults to ` \t\r\n`)
- `iSubSize` - Character set length (0 for auto)
- `bSrcRevise` - Whether to modify source
- `iRetSize` - Optional output parameter for trimmed byte length

**Return Value:**
- Trimmed string

**Memory Release:**
- `bSrcRevise=FALSE` - ✅ Needs to be released

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
	xrtInit();
	
	str text = (str)"   Hello World   ";
	size_t iTrimmedSize = 0;
	
	// Trim left spaces
	str trimmed = xrtLTrim(text, 0, (str)" ", 0, FALSE, &iTrimmedSize);
	printf("[%s]\n", trimmed);  // "[Hello World   ]"
	printf("Trimmed length: %zu\n", iTrimmedSize);
	xrtFree(trimmed);
    
    // Use default character set (space\t\r\n)
    str text2 = (str)"\t\n  Hello";
	str trimmed2 = xrtLTrim(text2, 0, NULL, 0, FALSE, NULL);
    printf("[%s]\n", trimmed2);  // "[Hello]"
    xrtFree(trimmed2);
    
    xrtUnit();
    return 0;
}
```

---

### xrtRTrim

Right trim.

**Prototype:**
```c
XXAPI str xrtRTrim(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bSrcRevise, size_t* iRetSize);
```

**Parameters:** Same as `xrtLTrim`

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str text = (str)"   Hello   ";
	str trimmed = xrtRTrim(text, 0, (str)" ", 0, FALSE, NULL);
    printf("[%s]\n", trimmed);  // "[   Hello]"
    xrtFree(trimmed);
    
    xrtUnit();
    return 0;
}
```

---

### xrtTrim

Trim both sides.

**Prototype:**
```c
XXAPI str xrtTrim(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bSrcRevise, size_t* iRetSize);
```

**Parameters:** Same as `xrtLTrim`

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Trim whitespace
    str text = (str)"   Hello World   ";
	str trimmed = xrtTrim(text, 0, (str)" \t\r\n", 0, FALSE, NULL);
    printf("[%s]\n", trimmed);  // "[Hello World]"
    xrtFree(trimmed);
    
    // Use default character set
    str text2 = (str)"\n\t  Content  \t\n";
	str trimmed2 = xrtTrim(text2, 0, NULL, 0, FALSE, NULL);
    printf("[%s]\n", trimmed2);  // "[Content]"
    xrtFree(trimmed2);
    
    // Trim specific characters
    str text3 = (str)"###Title###";
	str trimmed3 = xrtTrim(text3, 0, (str)"#", 0, FALSE, NULL);
    printf("[%s]\n", trimmed3);  // "[Title]"
    xrtFree(trimmed3);
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Supports UTF-8 multi-byte characters
- When `sSubText` is `NULL`, defaults to trimming ` \t\r\n`

---

## String Filtering

### xrtFilterStr

Filter characters.

**Prototype:**
```c
XXAPI str xrtFilterStr(str sText, size_t iSize, str sFilter, size_t iSubSize, bool bSrcRevise, size_t* iRetSize);
```

**Parameters:**
- `sText` - Source string
- `iSize` - Length (0 for auto)
- `sFilter` - Character set to filter out
- `iSubSize` - Character set length (0 for auto)
- `bSrcRevise` - Whether to modify source
- `iRetSize` - Optional output parameter for filtered byte length

**Return Value:**
- Filtered string

**Memory Release:**
- `bSrcRevise=FALSE` - ✅ Needs to be released

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
	xrtInit();
	
	str text = (str)"Hello123World456";
	size_t iFilteredSize = 0;
	
	// Filter digits
	str filtered = xrtFilterStr(text, 0, (str)"0123456789", 0, FALSE, &iFilteredSize);
	printf("%s\n", filtered);  // "HelloWorld"
	printf("Filtered result length: %zu\n", iFilteredSize);  // 10
	xrtFree(filtered);
    
    // Filter spaces and newlines
    str text2 = (str)"Hello World\nNew Line";
	str filtered2 = xrtFilterStr(text2, 0, (str)" \n", 0, FALSE, NULL);
    printf("%s\n", filtered2);  // "HelloWorldNewLine"
    xrtFree(filtered2);
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Supports UTF-8 multi-byte characters
- When `sFilter` is `NULL`, returns copy of original string directly

---

## String Operations

### xrtFormat

Format string

**Prototype:**
```c
XXAPI str xrtFormat(str sFormat, ...);
```

**Parameters:**
- `sFormat` - Format string (similar to `printf`)
- `...` - Variable arguments

**Return Value:**
- Formatted string

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Basic formatting
    str text = xrtFormat((str)"Hello %s, you are %d years old", "Tom", 25);
    printf("%s\n", text);  // "Hello Tom, you are 25 years old"
    xrtFree(text);
    
    // Build path
    str path = xrtFormat((str)"%s\\%s", "C:\\folder", "file.txt");
    printf("%s\n", path);  // "C:\folder\file.txt"
    xrtFree(path);
    
    // Format numbers
    str num = xrtFormat((str)"Value: %.2f, Count: %06d", 3.14159, 42);
    printf("%s\n", num);  // "Value: 3.14, Count: 000042"
    xrtFree(num);
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Internally uses `vsnprintf`, supports all standard format specifiers
- If you need the result length, use `strlen((char*)...)`

---

### xrtReplace

String replacement

**Prototype:**
```c
XXAPI str xrtReplace(str sText, size_t iSize, str sSubText, size_t iSubSize, str sRepText, size_t iRepSize, size_t* iRetSize);
```

**Parameters:**
- `sText` - Source string
- `iSize` - Source string length (0 for auto)
- `sSubText` - Substring to replace
- `iSubSize` - Substring length (0 for auto)
- `sRepText` - Replacement string
- `iRepSize` - Replacement length (0 for auto)
- `iRetSize` - Optional output parameter for resulting byte length

**Return Value:**
- New string after replacement

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Basic replacement
	str replaced = xrtReplace((str)"Hello World", 0, (str)"World", 0, (str)"XRT", 0, NULL);
    printf("%s\n", replaced);  // "Hello XRT"
    xrtFree(replaced);
    
    // Replace all (all matches)
	str result = xrtReplace((str)"a b a b a", 0, (str)"a", 0, (str)"c", 0, NULL);
    printf("%s\n", result);  // "c b c b c"
    xrtFree(result);
    
    // Delete substring (replace with empty)
	str deleted = xrtReplace((str)"Hello World", 0, (str)"World", 0, (str)"", 0, NULL);
    printf("[%s]\n", deleted);  // "[Hello ]"
    xrtFree(deleted);
    
    // Replace with different length substring
	size_t iLongerSize = 0;
	str longer = xrtReplace((str)"AB", 0, (str)"B", 0, (str)"XYZ", 0, &iLongerSize);
	printf("%s (length: %zu)\n", longer, iLongerSize);  // "AXYZ (length: 4)"
    xrtFree(longer);
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Use `iRetSize` if you need the resulting length
- Replaces **all** matching substrings, not just the first one

---

### xrtSplit

Split string

**Prototype:**
```c
XXAPI str* xrtSplit(str sText, size_t iSize, str sSepText, size_t iSepSize, bool bSrcRevise, size_t* iRetSize);
```

**Parameters:**
- `sText` - Source string
- `iSize` - Source string length (0 for auto)
- `sSepText` - Separator
- `iSepSize` - Separator length (0 for auto)
- `bSrcRevise` - Whether to modify source
  - `TRUE` - Modify source directly (insert `\0` in original string)
  - `FALSE` - Create new strings
- `iRetSize` - Optional output parameter for split item count

**Return Value:**
- Array of string pointers (NULL-terminated)

**Memory Release:** ✅ **Always requires** `xrtFree` to release array

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
	// Split and modify source string (bSrcRevise=TRUE)
	str text1 = xrtCopyStr((str)"apple,banana,orange", 0);
	size_t iPartCount1 = 0;
	str* parts1 = xrtSplit(text1, 0, (str)",", 0, TRUE, &iPartCount1);
	printf("Split count: %zu\n", iPartCount1);  // 3
    for (int i = 0; parts1[i] != NULL; i++) {
        printf("%d: %s\n", i, parts1[i]);
    }
    xrtFree(parts1);  // Release array
    xrtFree(text1);   // Release source string
    
    printf("---\n");
    
    // Split without modifying source (bSrcRevise=FALSE)
    str text2 = (str)"one|two|three";
	str* parts2 = xrtSplit(text2, 0, (str)"|", 0, FALSE, NULL);
    for (int i = 0; parts2[i] != NULL; i++) {
        printf("%d: %s\n", i, parts2[i]);
    }
    xrtFree(parts2);  // Only need to release array, strings are in array memory
    
    printf("---\n");
    
    // Multi-character separator
    str csv = xrtCopyStr((str)"a::b::c", 0);
	str* parts3 = xrtSplit(csv, 0, (str)"::", 0, TRUE, NULL);
    for (int i = 0; parts3[i] != NULL; i++) {
        printf("%d: [%s]\n", i, parts3[i]);
    }
    xrtFree(parts3);
    xrtFree(csv);
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Use `iRetSize` if you need the split count
- Returned array is NULL-terminated
- When `bSrcRevise=TRUE`, `\0` is inserted into source string, destroying original data
- When `bSrcRevise=FALSE`, string data and array are in same memory block, only need one release

---

### xrtRandStr

Generate random string

**Prototype:**
```c
XXAPI str xrtRandStr(str sTemplate, size_t iSize, size_t iLen);
```

**Parameters:**
- `sTemplate` - Character template (randomly select from)
- `iSize` - Template length (0 for auto)
- `iLen` - Generation length

**Return Value:**
- Random string

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Generate 8-digit numeric PIN
    str pin = xrtRandStr((str)"0123456789", 0, 8);
    printf("PIN: %s\n", pin);  // e.g., "57382910"
    xrtFree(pin);
    
    // Generate 16-character mixed password
    str password = xrtRandStr(
        (str)"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
        0, 16
    );
    printf("Password: %s\n", password);
    xrtFree(password);
    
    // Use default template (NULL)
    str token = xrtRandStr(NULL, 0, 32);
    printf("Token: %s\n", token);  // Contains digits+letters+-_
    xrtFree(token);
    
    // Generate hexadecimal string
    str hex = xrtRandStr((str)"0123456789ABCDEF", 0, 8);
    printf("Hex: %s\n", hex);  // e.g., "3F7A9C2E"
    xrtFree(hex);
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- When `sTemplate` is `NULL`, uses default template (64 characters: digits+uppercase+lowercase+-_)
- Internally uses `xrtRandRange` to generate random indices

---

## Number Formatting

### xrtIntFormat

Integer formatting.

**Prototype:**
```c
XXAPI str xrtIntFormat(int64 value, str format);
```

**Parameters:**
- `value` - 64-bit integer to format
- `format` - Format string (`NULL` for default format)

**Format Specifiers:**
| Specifier | Description | Example |
|-----------|-------------|--------|
| `,` | Thousands separator | `1,234,567` |
| `0N` | Pad with leading zeros to N digits | `00000042` |
| `+` | Always show sign | `+123` |
| `x` | Hexadecimal (lowercase) | `ff` |
| `X` | Hexadecimal (uppercase) | `FF` |
| `o` | Octal | `100` |
| `b` | Binary | `1010` |

**Return Value:**
- Formatted string
- Returns the empty-string sentinel on failure

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Basic conversion
    str s1 = xrtIntFormat(1234567, NULL);
    printf("%s\n", s1);  // "1234567"
    xrtFree(s1);
    
    // Thousands separator
    str s2 = xrtIntFormat(1234567, (str)",");
    printf("%s\n", s2);  // "1,234,567"
    xrtFree(s2);
    
    // Leading zeros
    str s3 = xrtIntFormat(42, (str)"08");
    printf("%s\n", s3);  // "00000042"
    xrtFree(s3);
    
    // Positive sign
    str s4 = xrtIntFormat(123, (str)"+");
    printf("%s\n", s4);  // "+123"
    xrtFree(s4);
    
    // Hexadecimal
    str s5 = xrtIntFormat(255, (str)"X");
    printf("%s\n", s5);  // "FF"
    xrtFree(s5);
    
    // Hexadecimal + leading zeros
    str s6 = xrtIntFormat(255, (str)"08X");
    printf("%s\n", s6);  // "000000FF"
    xrtFree(s6);
    
    // Octal
    str s7 = xrtIntFormat(64, (str)"o");
    printf("%s\n", s7);  // "100"
    xrtFree(s7);
    
    // Binary
    str s8 = xrtIntFormat(10, (str)"b");
    printf("%s\n", s8);  // "1010"
    xrtFree(s8);
    
    // Negative + thousands
    str s9 = xrtIntFormat(-1234567, (str)",");
    printf("%s\n", s9);  // "-1,234,567"
    xrtFree(s9);
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Non-decimal bases don't support thousands separator or sign display
- Format specifiers can be combined, e.g., `+08` for positive sign + 8-digit zero padding

---

### xrtNumFormat

Floating-point number formatting.

**Prototype:**
```c
XXAPI str xrtNumFormat(double value, str format);
```

**Parameters:**
- `value` - Double precision floating-point number to format
- `format` - Format string (`NULL` for default 6 decimal places)

**Format Specifiers:**
| Specifier | Description | Example |
|-----------|-------------|--------|
| `,` | Thousands separator (integer part) | `1,234,567.89` |
| `.N` | N decimal places | `3.14` |
| `+` | Always show sign | `+3.14` |
| `%` | Percentage (auto ×100) | `12.34%` |

**Return Value:**
- Formatted string
- Returns the empty-string sentinel on failure

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Default format (6 decimal places)
    str s1 = xrtNumFormat(3.14159, NULL);
    printf("%s\n", s1);  // "3.141590"
    xrtFree(s1);
    
    // Specify decimal places
    str s2 = xrtNumFormat(3.14159, (str)".2");
    printf("%s\n", s2);  // "3.14"
    xrtFree(s2);
    
    // Zero padding
    str s3 = xrtNumFormat(3.1, (str)".3");
    printf("%s\n", s3);  // "3.100"
    xrtFree(s3);
    
    // Thousands + decimals
    str s4 = xrtNumFormat(1234567.89, (str)",.2");
    printf("%s\n", s4);  // "1,234,567.89"
    xrtFree(s4);
    
    // Percentage
    str s5 = xrtNumFormat(0.1234, (str)".2%");
    printf("%s\n", s5);  // "12.34%"
    xrtFree(s5);
    
    // Percentage (no decimals)
    str s6 = xrtNumFormat(0.75, (str)".0%");
    printf("%s\n", s6);  // "75%"
    xrtFree(s6);
    
    // Positive sign
    str s7 = xrtNumFormat(3.14, (str)"+.2");
    printf("%s\n", s7);  // "+3.14"
    xrtFree(s7);
    
    // Negative number
    str s8 = xrtNumFormat(-1234.56, (str)",.2");
    printf("%s\n", s8);  // "-1,234.56"
    xrtFree(s8);
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Maximum 15 decimal places supported
- Special value handling: `NaN`, `Inf`, `-Inf`
- Format specifiers can be combined, e.g., `,.2%` for thousands + 2 decimals + percentage

---

## String Similarity

### xrtStrSim

Calculate similarity between two strings (based on Levenshtein edit distance).

**Prototype:**
```c
XXAPI double xrtStrSim(str s1, size_t len1, str s2, size_t len2);
```

**Parameters:**
- `s1` - First string
- `len1` - First string length (0 for auto-calculate)
- `s2` - Second string
- `len2` - Second string length (0 for auto-calculate)

**Return Value:**
- Similarity between `0.0` and `1.0`
  - `1.0` - Identical
  - `0.0` - Completely different

**Algorithm Characteristics:**
- Based on Levenshtein edit distance algorithm
- Time complexity: O(m*n)
- Space complexity: O(min(m,n)) (optimized 1D DP array)
- Similarity formula: `1 - (editDistance / maxLength)`

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Identical strings
    printf("%.4f\n", xrtStrSim("hello", 0, "hello", 0));  // 1.0000
    
    // One character difference
    printf("%.4f\n", xrtStrSim("hello", 0, "helo", 0));   // 0.8000
    
    // Completely different
    printf("%.4f\n", xrtStrSim("abc", 0, "xyz", 0));      // 0.0000
    
    // Classic test case: kitten -> sitting
    printf("%.4f\n", xrtStrSim("kitten", 0, "sitting", 0)); // 0.5714
    
    // Empty strings
    printf("%.4f\n", xrtStrSim("", 0, "", 0));            // 1.0000
    printf("%.4f\n", xrtStrSim("abc", 0, "", 0));         // 0.0000
    
    xrtUnit();
    return 0;
}
```

**Application Scenarios:**
```c
#include "xrt.h"
#include <stdio.h>

// Fuzzy search: find similar words
bool FuzzyMatch(str input, str target, double threshold) {
    return xrtStrSim(input, 0, target, 0) >= threshold;
}

int main() {
    xrtInit();
    
    str words[] = {"apple", "application", "apply", "banana", "orange"};
    str search = "appla";  // User input (possible typo)
    
    printf("Searching: %s\n", search);
    for (int i = 0; i < 5; i++) {
        double sim = xrtStrSim(search, 0, words[i], 0);
        if (sim >= 0.6) {
            printf("  Match: %s (similarity: %.2f)\n", words[i], sim);
        }
    }
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- `NULL` pointers are treated as empty strings
- Not suitable for very long strings (>10KB), consider using Dice/SimHash algorithms instead

---

## String Approximate Equal

### Configuration Constants

String approximate equal mode constants.

```c
#define XRT_STR_APPROX_LIKE     0   // Wildcard mode (s2 is pattern)
#define XRT_STR_APPROX_SIM      1   // Similarity threshold mode
```

### xCore Configuration

String approximate equal is configured through the `xCore` global variable.

| Config Item | Type | Description | Default Value |
|-------------|------|-------------|---------------|
| `xCore.iApproxStrMode` | int | Mode selection | `XRT_STR_APPROX_SIM` (1) |
| `xCore.fApproxStrTol` | double | Similarity threshold | `0.95` (95%) |
| `xCore.bApproxStrCase` | bool | Wildcard case-sensitivity | `FALSE` (case-sensitive) |

### xrtStrApprox

String approximate equality check (uses xCore configuration).

**Prototype:**
```c
XXAPI bool xrtStrApprox(str s1, size_t len1, str s2, size_t len2);
```

**Parameters:**
- `s1` - First string
- `len1` - First string length (0 for auto-calculate)
- `s2` - Second string (pattern string in wildcard mode)
- `len2` - Second string length (0 for auto-calculate)

**Return Value:**
- `TRUE` - Approximately equal
- `FALSE` - Not equal

**Mode Description:**
- **Wildcard mode** (`XRT_STR_APPROX_LIKE`): Uses `xrtStrLike` matching, `s2` is pattern
- **Similarity mode** (`XRT_STR_APPROX_SIM`): Uses `xrtStrSim` calculation, result >= threshold means approximately equal

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // === Similarity mode (default) ===
    // Default 95% threshold
    printf("%d\n", xrtStrApprox("hello", 0, "hello", 0));  // 1 (identical)
    printf("%d\n", xrtStrApprox("hello", 0, "helo", 0));   // 0 (80% < 95%)
    
    // Adjust threshold to 80%
    xCore.fApproxStrTol = 0.80;
    printf("%d\n", xrtStrApprox("hello", 0, "helo", 0));   // 1 (80% >= 80%)
    
    // === Wildcard mode ===
    xCore.iApproxStrMode = XRT_STR_APPROX_LIKE;
    printf("%d\n", xrtStrApprox("hello", 0, "h*o", 0));    // 1
    printf("%d\n", xrtStrApprox("hello", 0, "h???o", 0));  // 1
    printf("%d\n", xrtStrApprox("hello", 0, "world", 0));  // 0
    
    // Case-insensitive
    xCore.bApproxStrCase = TRUE;
    printf("%d\n", xrtStrApprox("HELLO", 0, "h*o", 0));    // 1
    
    // Restore default configuration
    xCore.iApproxStrMode = XRT_STR_APPROX_SIM;
    xCore.fApproxStrTol = 0.95;
    xCore.bApproxStrCase = FALSE;
    
    xrtUnit();
    return 0;
}
```

**Application Scenarios:**
```c
#include "xrt.h"
#include <stdio.h>

// Setup for filename fuzzy matching mode
void SetupFileMatch() {
    xCore.iApproxStrMode = XRT_STR_APPROX_LIKE;
    xCore.bApproxStrCase = TRUE;  // Filenames usually case-insensitive
}

// Setup for text similarity detection mode
void SetupTextSimilarity(double threshold) {
    xCore.iApproxStrMode = XRT_STR_APPROX_SIM;
    xCore.fApproxStrTol = threshold;
}

int main() {
    xrtInit();
    
    // Filename matching
    SetupFileMatch();
    printf("Match *.txt: %d\n", xrtStrApprox("readme.txt", 0, "*.txt", 0));
    printf("Match *.txt: %d\n", xrtStrApprox("README.TXT", 0, "*.txt", 0));
    
    // Spell checking (90% similarity)
    SetupTextSimilarity(0.90);
    printf("Spell check: %d\n", xrtStrApprox("receive", 0, "recieve", 0));
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Consistent style with `xrtIntApprox`, `xrtNumApprox`, `xrtTimeApprox`
- Configuration initialized to default values in `xrtInit()`
- Configuration changes affect all subsequent calls

---

## Encoding and Decoding

### xrtHexEncode

HEX encoding

**Prototype:**
```c
XXAPI str xrtHexEncode(ptr pMem, size_t iSize);
```

**Parameters:**
- `pMem` - Binary data
- `iSize` - Data length

**Return Value:**
- HEX string (uppercase)

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    uint8 data[] = {0x12, 0x34, 0x56, 0xAB, 0xCD};
	str hex = xrtHexEncode(data, sizeof(data));
	
	printf("%s\n", hex);  // "123456ABCD"
	printf("Encoded length: %zu\n", strlen((char*)hex));  // 10
    
    xrtFree(hex);
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Output is uppercase hexadecimal characters
- Encoded length equals input length × 2 and can be read with `strlen((char*)...)`

---

### xrtHexDecode

HEX decoding

**Prototype:**
```c
XXAPI ptr xrtHexDecode(str pText, size_t iSize);
```

**Parameters:**
- `pText` - HEX string
- `iSize` - String length (0 for auto)

**Return Value:**
- Binary data

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
	// Decode HEX of "Hello"
	str hex = (str)"48656C6C6F";
	ptr data = xrtHexDecode(hex, 0);
	size_t len = strlen((char*)hex) / 2;
    
    printf("Decoded: %.*s\n", (int)len, (char*)data);  // "Hello"
    printf("Decoded length: %zu\n", len);  // 5
    
    xrtFree(data);
    
	// Lowercase hex also supported
	str hex2 = (str)"576f726c64";  // "World"
	ptr data2 = xrtHexDecode(hex2, 0);
	printf("Decoded: %.*s\n", (int)(strlen((char*)hex2) / 2), (char*)data2);
	xrtFree(data2);
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Supports both uppercase and lowercase hexadecimal characters
- Decoded length equals input length / 2

---

### xrtBase64Encode

Base64 encoding

**Prototype:**
```c
XXAPI str xrtBase64Encode(ptr pMem, size_t iSize, str sTable);
```

**Parameters:**
- `pMem` - Binary data
- `iSize` - Data length
- `sTable` - Encoding table (`NULL` for standard table)

**Return Value:**
- Base64 string

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

int main() {
    xrtInit();
    
	// Basic encoding
	str text = (str)"Hello World";
	str b64 = xrtBase64Encode(text, strlen((char*)text), NULL);
	printf("Encoded: %s\n", b64);  // "SGVsbG8gV29ybGQ="
	printf("Encoded length: %zu\n", strlen((char*)b64));  // 16
	xrtFree(b64);
    
    // Encode binary data
    uint8 binary[] = {0x00, 0x01, 0x02, 0xFF};
    str b64_bin = xrtBase64Encode(binary, sizeof(binary), NULL);
    printf("Binary encoded: %s\n", b64_bin);  // "AAEC/w=="
    xrtFree(b64_bin);
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Encoded length can be read with `strlen((char*)...)`
- Automatically adds padding character `=`

---

### xrtBase64Decode

Base64 decoding

**Prototype:**
```c
XXAPI ptr xrtBase64Decode(str sText, size_t iSize, str sTable);
```

**Parameters:**
- `sText` - Base64 string
- `iSize` - Length (0 for auto)
- `sTable` - Decoding table (`NULL` for standard table)

**Return Value:**
- Binary data

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
	// Basic decoding
	str b64 = (str)"SGVsbG8gV29ybGQ=";
	ptr data = xrtBase64Decode(b64, 0, NULL);
	size_t len = 11;
    
    printf("Decoded: %.*s\n", (int)len, (char*)data);  // "Hello World"
    printf("Decoded length: %zu\n", len);  // 11
    xrtFree(data);
    
	// Decode binary data
	str b64_bin = (str)"AAEC/w==";
	ptr binary = xrtBase64Decode(b64_bin, 0, NULL);
	uint8* bytes = (uint8*)binary;
	printf("Binary: ");
	for (size_t i = 0; i < 4; i++) {
		printf("%02X ", bytes[i]);
	}
    printf("\n");  // "00 01 02 FF"
    xrtFree(binary);
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Input length must be multiple of 4, otherwise returns error
- When illegal characters are present, retrieve the current error with `xrtGetError()`
- In examples like these, the decoded length can be derived from known input; prefer APIs with explicit out-size parameters when that contract is available

---

## UTF-8 Utility Functions

### xrtCharLenU8

Get the byte count of a UTF-8 character.

**Prototype:**
```c
static inline int xrtCharLenU8(unsigned char c);
```

**Parameters:**
- `c` - First byte of UTF-8 character

**Return Value:**
- Number of bytes the UTF-8 character occupies (1-6)

**UTF-8 Encoding Rules:**
| First Byte Pattern | Bytes | Unicode Range |
|--------------------|-------|---------------|
| `0xxxxxxx` | 1 | U+0000 - U+007F (ASCII) |
| `110xxxxx` | 2 | U+0080 - U+07FF |
| `1110xxxx` | 3 | U+0800 - U+FFFF |
| `11110xxx` | 4 | U+10000 - U+1FFFFF |
| `111110xx` | 5 | U+200000 - U+3FFFFFF |
| `1111110x` | 6 | U+4000000 - U+7FFFFFFF |

**Characteristics:**
- Inline function, zero overhead
- Returns 1 for invalid bytes

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // ASCII characters (1 byte)
    printf("%d\n", xrtCharLenU8('A'));         // 1
    printf("%d\n", xrtCharLenU8('0'));         // 1
    printf("%d\n", xrtCharLenU8(' '));         // 1
    
	// Latin extended characters (2 bytes in UTF-8)
	str latin = (str)"é";
	printf("%d\n", xrtCharLenU8(latin[0]));    // 2
    
    // Emoji (4 bytes)
    str emoji = (str)"😀";
    printf("%d\n", xrtCharLenU8(emoji[0]));    // 4
    
	// Iterate through UTF-8 string
	str text = (str)"Helloé😀";
    size_t i = 0;
    int charCount = 0;
    while (text[i]) {
        int len = xrtCharLenU8(text[i]);
        printf("Char %d: bytes=%d\n", charCount, len);
        i += len;
        charCount++;
    }
	// Output: byte lengths are 1,1,1,1,1,2,4
    
    xrtUnit();
    return 0;
}
```

**Use Case:**
```c
#include "xrt.h"
#include <stdio.h>

// Count UTF-8 string characters (not bytes)
size_t Utf8CharCount(str text) {
    if (!text) return 0;
    size_t count = 0;
    size_t i = 0;
    while (text[i]) {
        i += xrtCharLenU8(text[i]);
        count++;
    }
    return count;
}

// Get first N characters of UTF-8 string
str Utf8Substring(str text, size_t n) {
if ( (!text) || (n == 0) ) return (str)"";
    size_t i = 0;
    size_t count = 0;
    while (text[i] && count < n) {
        i += xrtCharLenU8(text[i]);
        count++;
    }
    return xrtCopyStr(text, i);
}

int main() {
    xrtInit();
    
	str text = (str)"Helloé😀World";
	printf("Char count: %zu\n", Utf8CharCount(text));  // 12
	printf("Byte count: %zu\n", strlen((char*)text));  // 17
	
	str sub = Utf8Substring(text, 7);
	printf("First 7 chars: %s\n", sub);  // "Helloé😀"
    xrtFree(sub);
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- This is a `static inline` function, can be called in multiple places without function call overhead
- Used to correctly iterate through UTF-8 strings
- `xrtStrLike` internally uses this function to handle `?` wildcard

---

## Use Cases

### 1. Text Processing

```c
#include "xrt.h"
#include <stdio.h>

// Clean user input
str CleanInput(str input) {
    // Trim whitespace
    str trimmed = xrtTrim(input, 0, (str)" \t\r\n", 0, FALSE);
    // Convert to lowercase
    str lower = xrtLCase(trimmed, 0, TRUE);
    // trimmed has been modified in-place, return directly
    return lower;
}

int main() {
    xrtInit();
    
    str input = (str)"   Hello WORLD   ";
    str clean = CleanInput(input);
    printf("Cleaned: [%s]\n", clean);  // "[hello world]"
    xrtFree(clean);
    
    xrtUnit();
    return 0;
}
```

---

### 2. Config Parsing

```c
#include "xrt.h"
#include <stdio.h>

// Parse key=value
void ParseConfig(str line) {
    str* parts = xrtSplit(line, 0, (str)"=", 0, FALSE);
    if (parts[0] && parts[1]) {
        str key = xrtTrim(parts[0], 0, (str)" ", 0, FALSE);
        str value = xrtTrim(parts[1], 0, (str)" ", 0, FALSE);
        printf("%s = %s\n", key, value);
        xrtFree(key);
        xrtFree(value);
    }
    xrtFree(parts);
}

int main() {
    xrtInit();
    
    ParseConfig((str)"  name = XRT Library  ");
    ParseConfig((str)"version=1.0.0");
    ParseConfig((str)"  path = C:\\Program Files  ");
    
    xrtUnit();
    return 0;
}
```

---

### 3. URL Encoding

```c
#include "xrt.h"
#include <stdio.h>

str UrlEncode(str text) {
    // Simplified: replace common special characters
    str step1 = xrtReplace(text, 0, (str)" ", 0, (str)"%20", 0);
    str step2 = xrtReplace(step1, 0, (str)"&", 0, (str)"%26", 0);
    str step3 = xrtReplace(step2, 0, (str)"=", 0, (str)"%3D", 0);
    xrtFree(step1);
    xrtFree(step2);
    return step3;
}

int main() {
    xrtInit();
    
    str url = (str)"name=Hello World&value=1+2=3";
    str encoded = UrlEncode(url);
    printf("Encoded: %s\n", encoded);
    xrtFree(encoded);
    
    xrtUnit();
    return 0;
}
```

---

## Best Practices

### 1. Memory Management

```c
#include "xrt.h"
#include <stdio.h>

void UseString(str s) {
    printf("Using: %s\n", s);
}

int main() {
    xrtInit();
    
    // ✓ Correct: release promptly
    str lower = xrtLCase((str)"TEXT", 0, FALSE);
    UseString(lower);
    xrtFree(lower);
    
    // ✗ Memory leak example (don't do this)
    // str leaked = xrtLCase((str)"TEXT", 0, FALSE);
    // Forgot to release...
    
    xrtUnit();
    return 0;
}
```

---

### 2. In-place Modification

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

int main() {
    xrtInit();
    
    // When source data can be modified, in-place modification is more efficient
    str text = xrtMalloc(100);
    strcpy((char*)text, "HELLO WORLD");
    
    printf("Before: %s\n", text);
    xrtLCase(text, 0, TRUE);  // In-place modification
    printf("After: %s\n", text);  // "hello world"
    
    xrtFree(text);
    xrtUnit();
    return 0;
}
```

---

## Related Documents

- [Charset Conversion](api-charset.en.md)
- [Path Processing](api-path.en.md)
- [Main Index](README.en.md)

---

<div align="center">

[⬆️ Back to Top](#string-processing-library)

</div>
