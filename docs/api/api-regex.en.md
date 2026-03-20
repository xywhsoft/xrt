# Regex Mainline

> XRT's regex module currently uses the built-in `bbre` engine internally, while its public surface is now exposed as native XRT API.

[Back to Index](README.en.md)

---

## 1. Positioning

This module is aimed at:

- text matching
- substring search
- capture extraction
- multi-pattern batch scanning
- lightweight rule checks over config, logs, HTTP text, and template fragments

Its design goals are:

- lightweight footprint
- embeddability
- cross-platform use
- public naming and types aligned with the XRT API system


## 2. Core Types

### `xregex`

Single regex object.

Typical flow:

1. `xrtRegexCreate()` or `xrtRegexCreateFromBuilder()`
2. `xrtRegexIsMatch()` / `xrtRegexFind()` / `xrtRegexCaptures()`
3. `xrtRegexDestroy()`


### `xregexbuilder`

Advanced builder.

Useful when you need to:

- pass explicit pattern length
- set flags
- specify a custom allocator


### `xregexset`

Multi-pattern set object.

Useful when you need to:

- test multiple rules together
- scan the same text against many patterns


### `xregexsetbuilder`

Builder for regex sets.

Useful when you need to:

- build multiple `xregex` objects first
- assemble them into one `xregexset`


### `xregexspan`

Match range:

```c
typedef struct {
	size_t iBegin;
	size_t iEnd;
} xregexspan;
```

Commonly used with:

- `xrtRegexFind()`
- `xrtRegexCaptures()`


### `xregexalloc`

Custom allocator:

```c
typedef ptr (*xregexallocproc)(ptr pUserData, ptr pMem, size_t iPrevSize, size_t iNextSize);

typedef struct {
	ptr pUserData;
	xregexallocproc procAlloc;
} xregexalloc;
```


## 3. Flags And Error Codes

### 3.1 Flags

```c
XRT_REGEX_FLAG_INSENSITIVE
XRT_REGEX_FLAG_MULTILINE
XRT_REGEX_FLAG_DOTNEWLINE
XRT_REGEX_FLAG_UNGREEDY
```

They mean:

- case-insensitive matching
- multiline mode
- `.` matches newline
- ungreedy quantifiers


### 3.2 Error Codes

```c
XRT_REGEX_ERR_MEM
XRT_REGEX_ERR_PARSE
XRT_REGEX_ERR_LIMIT
```


## 4. Public API

### 4.1 Single Pattern

```c
XXAPI xregex* xrtRegexCreate(const char* sPatternNt);
XXAPI int xrtRegexCreateFromBuilder(xregex** ppRegex, const xregexbuilder* pBuilder, const xregexalloc* pAlloc);
XXAPI void xrtRegexDestroy(xregex* pRegex);

XXAPI int xrtRegexIsMatch(xregex* pRegex, const char* sText, size_t iTextSize);
XXAPI int xrtRegexFind(xregex* pRegex, const char* sText, size_t iTextSize, xregexspan* pOutSpan);
XXAPI int xrtRegexCaptures(xregex* pRegex, const char* sText, size_t iTextSize, xregexspan* pOutCaptures, uint32 iCaptureCount);

XXAPI int xrtRegexClone(xregex** ppOut, const xregex* pRegex, const xregexalloc* pAlloc);
```


### 4.2 Builder

```c
XXAPI int xrtRegexBuilderCreate(xregexbuilder** ppBuilder, const char* sPattern, size_t iPatternSize, const xregexalloc* pAlloc);
XXAPI void xrtRegexBuilderDestroy(xregexbuilder* pBuilder);
XXAPI void xrtRegexBuilderSetFlags(xregexbuilder* pBuilder, xregexflags iFlags);
```


### 4.3 Multi-Pattern

```c
XXAPI int xrtRegexSetBuilderCreate(xregexsetbuilder** ppBuilder, const xregexalloc* pAlloc);
XXAPI void xrtRegexSetBuilderDestroy(xregexsetbuilder* pBuilder);
XXAPI int xrtRegexSetBuilderAdd(xregexsetbuilder* pBuilder, const xregex* pRegex);

XXAPI xregexset* xrtRegexSetCreate(const char* const* arrPatternsNt, size_t iPatternCount);
XXAPI int xrtRegexSetCreateFromBuilder(xregexset** ppSet, const xregexsetbuilder* pBuilder, const xregexalloc* pAlloc);
XXAPI void xrtRegexSetDestroy(xregexset* pSet);

XXAPI int xrtRegexSetIsMatch(xregexset* pSet, const char* sText, size_t iTextSize);
XXAPI int xrtRegexSetMatches(xregexset* pSet, const char* sText, size_t iTextSize, uint32* pOutIndexes, uint32 iMaxIndexes, uint32* pOutIndexCount);
XXAPI int xrtRegexSetClone(xregexset** ppOut, const xregexset* pSet, const xregexalloc* pAlloc);
```


## 5. Examples

### 5.1 Quick Match

```c
xregex* pRegex = xrtRegexCreate("hello");
if ( pRegex ) {
	int iMatch = xrtRegexIsMatch(pRegex, "hello world", strlen("hello world"));
	xrtRegexDestroy(pRegex);
}
```


### 5.2 Find The First Match Range

```c
xregexspan tSpan;
xregex* pRegex = xrtRegexCreate("[0-9]+");

if ( pRegex ) {
	if ( xrtRegexFind(pRegex, "id=12345", strlen("id=12345"), &tSpan) > 0 ) {
		printf("begin=%zu end=%zu\n", tSpan.iBegin, tSpan.iEnd);
	}
	xrtRegexDestroy(pRegex);
}
```


### 5.3 Get Captures

```c
xregexspan arrCaps[8];
xregex* pRegex = xrtRegexCreate("([A-Za-z]+)=([0-9]+)");

if ( pRegex ) {
	int iRet = xrtRegexCaptures(pRegex, "count=42", strlen("count=42"), arrCaps, 8u);
	if ( iRet > 0 ) {
		printf("all=%zu..%zu\n", arrCaps[0].iBegin, arrCaps[0].iEnd);
		printf("key=%zu..%zu\n", arrCaps[1].iBegin, arrCaps[1].iEnd);
		printf("val=%zu..%zu\n", arrCaps[2].iBegin, arrCaps[2].iEnd);
	}
	xrtRegexDestroy(pRegex);
}
```


### 5.4 Multi-Pattern Batch Scan

```c
const char* arrPatterns[] = {
	"error",
	"warn",
	"timeout"
};
uint32 arrIndexes[8];
uint32 iCount = 0;
const char* sText = "warn: network timeout";
xregexset* pSet = xrtRegexSetCreate(arrPatterns, 3u);

if ( pSet ) {
	xrtRegexSetMatches(pSet, sText, strlen(sText), arrIndexes, 8u, &iCount);
	xrtRegexSetDestroy(pSet);
}
```


## 6. Notes

- The current engine implementation comes from `bbre`, but business code should no longer expose `bbre_*` directly.
- External code should use `xregex`, `xregexset`, `xregexspan`, and `xrtRegex*`.
- The public wrappers for `xrtRegexFind()` and `xrtRegexCaptures()` handle output-buffer adaptation instead of exposing low-level assertion constraints directly.
- For multi-threaded use, clone the object with `xrtRegexClone()` or `xrtRegexSetClone()` when you need an independent instance.
