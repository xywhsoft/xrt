# JSONL

JSONL converts between a line-delimited record sequence and an xvalue Array, one element per record; individual values follow the JSON type and codec rules. Suited to logs, bulk import, and file exchange.

## Trimming and dependencies

| Public selection macro | Implementation macro | Direct dependencies |
|---|---|---|
| `XRT_MODULE_JSONL_CORE` | `XRT_FEATURE_JSONL_CORE` | core |
| `XRT_MODULE_JSONL_READ` | `XRT_FEATURE_JSONL_READ` | jsonl_core, json_read |
| `XRT_MODULE_JSONL_WRITE` | `XRT_FEATURE_JSONL_WRITE` | jsonl_core, json_write |
| `XRT_MODULE_JSONL_FILE` | `XRT_FEATURE_JSONL_FILE` | jsonl_read, jsonl_write, file_whole |
| `XRT_MODULE_JSONL` | `XRT_FEATURE_JSONL` | jsonl_file |

The header is `<xrt/jsonl.h>` and can also be included through the umbrella header `<xrt.h>`. Selecting only reading or only writing does not pull in the file module.

## Stability contract

- Blank lines are ignored by default: empty lines, and lines containing only ASCII spaces, tabs, or CR. LF, CRLF, and mixed endings are accepted; a lone CR is not a record separator. Unicode whitespace never turns a line into a blank line.
- Empty input and input consisting entirely of blank lines both return an empty Array. Strict blank-line mode still accepts zero-byte input and a single terminating newline after the last record; extra blank lines are errors.
- A line must contain exactly one complete value; the final complete record may omit the terminating newline. Multi-line values, multiple root values on one line, a BOM, invalid UTF-8, and a truncated trailing record all fail.
- An escaped newline inside a string does not split records. Array records preserve nesting; for example the text `[]\n` yields an Array containing one empty array.
- Deserialization hands over the owned Array only after every record succeeds; on failure it returns C NULL and frees all partial results.
- The serialization root must be `XVALUE_ARRAY`; null, the empty string, and empty containers are real records. The SKIP strategy cannot skip a whole record, it only applies to members inside a single root value.
- Output is compact, one LF appended per record, and an empty Array produces zero bytes. A PRETTY configuration is rejected. The configuration is snapshotted by value; a callback mutating the original configuration does not affect the in-flight call.
- `Valid` reuses the DOM-free syntax validation of JSON: blank lines are ignored, the default cumulative budgets are checked, and duplicate-key DOM policy does not participate.
- File writing completes the in-memory serialization first and then replaces the file atomically; a serialization failure leaves the target untouched. Synchronous callback writing may have committed partial records before failing; those bytes cannot be rolled back.
- Single-line comments and trailing commas are compatibility extensions enabled only inside Record; they must not continue across physical lines.

Threading and ownership: the API carries no implicit locks; each call owns an independent workspace, and the caller synchronizes shared mutable input. Writing iterates a backing snapshot of the outer Array, so Value subtree ownership rules still apply during callbacks. Input text and configuration are borrowed until the call returns; the result Array is released with `xrtValueRelease`, the result string with `xrtFree`. The output callback consumes bytes before returning.

## Constants

| Constant | Value | Meaning |
|---|---|---|
| `XJSONL_READ_REJECT_EMPTY_LINES` | `UINT32_C(0x00000001)` | Blank lines become errors; off by default. |

Configurations must be initialized with Init; all budgets are non-zero. Record defaults come from the existing JSON initialization functions. Overall input/output defaults to 64 MiB; record count and syntax value count default to 1000000.

## Types

### `xjsonlerror`

```c
typedef enum xjsonlerror {
	XJSONL_ERROR_CONFIG = 1701,
	XJSONL_ERROR_SYNTAX,
	XJSONL_ERROR_LIMIT,
	XJSONL_ERROR_RECORD,
	XJSONL_ERROR_TYPE,
	XJSONL_ERROR_OUTPUT,
	XJSONL_ERROR_IO,
	XJSONL_ERROR_STATE
} xjsonlerror;
```

Errors are obtained via `xrtGetError()` with format domain `xrt.jsonl`; argument and underlying OOM failures may also surface as Core errors directly. RECORD/OUTPUT/IO wrappers preserve the Cause Kind. If building the wrapper itself fails to allocate, the original cause error is kept and no location data is promised.

| Value | Meaning |
|---|---|
| `XJSONL_ERROR_CONFIG` | Configuration not initialized, reserved bits non-zero, a zero budget, or PRETTY enabled. |
| `XJSONL_ERROR_SYNTAX` | A blank line in strict blank-line mode. |
| `XJSONL_ERROR_LIMIT` | Overall or per-record budget exhausted. |
| `XJSONL_ERROR_RECORD` | Encoding/decoding a single value or appending it to the Array failed; the Cause keeps the specific reason. |
| `XJSONL_ERROR_TYPE` | The serialization root is not an Array. |
| `XJSONL_ERROR_OUTPUT` | Synchronous output callback failed. |
| `XJSONL_ERROR_IO` | Budgeted file read or atomic file replacement failed. |
| `XJSONL_ERROR_STATE` | Reserved code for an invalid line-processing state. |

### `xjsonllocation`

```c
typedef struct xjsonllocation {
	size_t Offset;
	size_t Line;
	size_t Column;
	size_t RecordIndex;
} xjsonllocation;
```

| Field | Type | Meaning |
|---|---|---|
| `Offset` | `size_t` | Zero-based byte offset in the raw input. |
| `Line` | `size_t` | One-based physical line number split on LF; CRLF counts as one line, and ignored blank lines still count. |
| `Column` | `size_t` | One-based UTF-8 byte column within the current physical line; not counted in Unicode characters. |
| `RecordIndex` | `size_t` | Zero-based record index; blank lines do not increment it, and a blank-line error points at the next pending record index. |

### `xjsonlreadflag`

```c
typedef enum xjsonlreadflag {
	XJSONL_READ_REJECT_EMPTY_LINES = UINT32_C(0x00000001)
} xjsonlreadflag;
```

| Value | Meaning |
|---|---|
| `XJSONL_READ_REJECT_EMPTY_LINES` | Turns the blank lines ignored by default into syntax errors. |

### `xjsonlreadconfig`

```c
typedef struct xjsonlreadconfig {
	xjsonreadconfig Record;
	uint32 Flags;
	size_t MaxInputBytes;
	size_t MaxRecords;
	size_t MaxTotalValues;
	uint32 Reserved[4];
} xjsonlreadconfig;
```

| Field | Type | Meaning |
|---|---|---|
| `Record` | `xjsonreadconfig` | Per-record configuration; MaxInputBytes/MaxOutputBytes exclude the CRLF/LF separators, and depth counts from each record's root value. |
| `Flags` | `uint32` | Defaults to 0; XJSONL_READ_REJECT_EMPTY_LINES enables blank-line errors. |
| `MaxInputBytes` | `size_t` | Cap on the whole raw input, including ignored blank lines and all separators; default 64 MiB. |
| `MaxRecords` | `size_t` | Cap on non-blank record count, equal to the maximum element count of the result Array; default 1000000. |
| `MaxTotalValues` | `size_t` | Cumulative cap on syntax values across all records, including values dropped by duplicate-key policy and excluding the synthesized Array; default 1000000. |
| `Reserved` | `uint32[4]` | Reserved space; must be all zero. |

### `xjsonlwriteconfig`

```c
typedef struct xjsonlwriteconfig {
	xjsonwriteconfig Record;
	size_t MaxOutputBytes;
	size_t MaxRecords;
	uint32 Reserved[4];
} xjsonlwriteconfig;
```

| Field | Type | Meaning |
|---|---|---|
| `Record` | `xjsonwriteconfig` | Per-record configuration; MaxInputBytes/MaxOutputBytes exclude the CRLF/LF separators, and depth counts from each record's root value. |
| `MaxOutputBytes` | `size_t` | Cap on total output bytes, including every LF and excluding the trailing NUL of the result; default 64 MiB. |
| `MaxRecords` | `size_t` | Cap on non-blank record count, equal to the maximum element count of the result Array; default 1000000. |
| `Reserved` | `uint32[4]` | Reserved space; must be all zero. |

## Text, configuration, and file interfaces

### `xrtJsonlErrorLocation`

Reads the global byte offset, one-based physical line/column, and zero-based record index; leaves the output unchanged when no location is present.

```c
bool xrtJsonlErrorLocation(
	const xerror* pError,
	xjsonllocation* pLocation
);
```

#### Parameters

| Parameter | Direction | Constraints | Description |
|---|---|---|---|
| `pError` | input | may be null; read-only borrow | Reads the location from an error of this format; returns false for no location or a foreign error domain. |
| `pLocation` | output | non-null | Filled with the global location on success; contents unchanged on failure. |

#### Return value

| Return | Meaning | State on failure |
|---|---|---|
| true | Location read. | — |
| false | No location or invalid argument. | Location output unchanged; absence of a location does not itself set an error. |

#### Errors

- `XERR_ARGUMENT` — the location output is null.

#### Example

[jsonl](../../examples/data/jsonl/main.c) · Call to xrtJsonlErrorLocation in the complete program; failure jumps to unified cleanup.

```c
if ( !xrtJsonlErrorLocation(xrtGetError(), &Location) ) goto done;
```

### `xrtJsonlReadConfigInit`

Initializes blank-line skipping by default, strict per-record syntax, and bounded cumulative budgets.

```c
void xrtJsonlReadConfigInit(
	xjsonlreadconfig* pConfig
);
```

#### Parameters

| Parameter | Direction | Constraints | Description |
|---|---|---|---|
| `pConfig` | output | non-null | Sets defaults and zeroes the reserved fields. |

#### Return value

| Return | Meaning | State on failure |
|---|---|---|
| void | Initialization complete. | A null pointer sets an argument error. |

#### Errors

- `XERR_ARGUMENT` — the configuration output is null.

#### Example

[jsonl](../../examples/data/jsonl/main.c) · Call to xrtJsonlReadConfigInit in the complete program; failure jumps to unified cleanup.

```c
xrtJsonlReadConfigInit(&Read);
```

### `xrtJsonlParse`

Parses the record sequence with default configuration; returns an owned Array on success, an empty Array for empty input.

```c
xvalue* xrtJsonlParse(
	xstrview Text
);
```

#### Parameters

| Parameter | Direction | Constraints | Description |
|---|---|---|---|
| `Text` | input | explicit byte length; NULL Data only at zero length | The whole text is borrowed until the call returns; input need not be NUL-terminated. |

#### Return value

| Return | Meaning | State on failure |
|---|---|---|
| non-null | Owned Array for the caller to release. | — |
| NULL | Argument, budget, syntax, or resource error. | No partial result; the error is obtained via xrtGetError(). |

#### Errors

- `XERR_ARGUMENT` — a required pointer is null, or a configuration field or reserved bit is invalid.
- `XERR_MEMORY` — workspace, result, or structured-error allocation failed.
- `XERR_RANGE` — a per-record or cumulative budget was exhausted.
- `XERR_PROTOCOL` — a non-blank line is not one complete valid value, or strict mode hit a blank line; a per-record error keeps the underlying cause chain.
Other per-record codec errors inherit the specific Kind and code of their Cause.

#### Example

[jsonl](../../examples/data/jsonl/main.c) · Call to xrtJsonlParse in the complete program; failure jumps to unified cleanup.

```c
pArray = xrtJsonlParse(XRT_STR_LITERAL("{\"id\":1}\n\n[2,3]\r\nnull\n"));
if ( pArray == NULL ) goto done;
```

### `xrtJsonlRead`

Parses all records per configuration; on failure frees partial results and returns NULL. Release the result with xrtValueRelease.

```c
xvalue* xrtJsonlRead(
	xstrview Text,
	const xjsonlreadconfig* pConfig
);
```

#### Parameters

| Parameter | Direction | Constraints | Description |
|---|---|---|---|
| `Text` | input | explicit byte length; NULL Data only at zero length | The whole text is borrowed until the call returns; input need not be NUL-terminated. |
| `pConfig` | input | non-null; must be initialized | Per-record configuration and cumulative budgets; the source is not modified by this call. |

#### Return value

| Return | Meaning | State on failure |
|---|---|---|
| non-null | Owned Array for the caller to release. | — |
| NULL | Argument, budget, syntax, or resource error. | No partial result; the error is obtained via xrtGetError(). |

#### Errors

- `XERR_ARGUMENT` — a required pointer is null, or a configuration field or reserved bit is invalid.
- `XERR_MEMORY` — workspace, result, or structured-error allocation failed.
- `XERR_RANGE` — a per-record or cumulative budget was exhausted.
- `XERR_PROTOCOL` — a non-blank line is not one complete valid value, or strict mode hit a blank line; a per-record error keeps the underlying cause chain.
Other per-record codec errors inherit the specific Kind and code of their Cause.

#### Example

[jsonl](../../examples/data/jsonl/main.c) · Call to xrtJsonlRead in the complete program; failure jumps to unified cleanup.

```c
pRead = xrtJsonlRead((xstrview){ Text, Size }, &Read);
if ( pRead == NULL ) goto done;
```

### `xrtJsonlValid`

Ignores blank lines by default and validates per-line syntax and cumulative budgets without building a Value DOM; duplicate-key policy does not participate.

```c
bool xrtJsonlValid(
	xstrview Text
);
```

#### Parameters

| Parameter | Direction | Constraints | Description |
|---|---|---|---|
| `Text` | input | explicit byte length; NULL Data only at zero length | The whole text is borrowed until the call returns; input need not be NUL-terminated. |

#### Return value

| Return | Meaning | State on failure |
|---|---|---|
| true | Every non-blank line passed the default syntax and budget checks. | — |
| false | Operation failed. | No partial result; the error is obtained via xrtGetError(). |

#### Errors

- `XERR_ARGUMENT` — a required pointer is null, or a configuration field or reserved bit is invalid.
- `XERR_MEMORY` — workspace, result, or structured-error allocation failed.
- `XERR_RANGE` — a per-record or cumulative budget was exhausted.
- `XERR_PROTOCOL` — a non-blank line is not one complete valid value, or strict mode hit a blank line; a per-record error keeps the underlying cause chain.
Other per-record codec errors inherit the specific Kind and code of their Cause.

#### Example

[jsonl](../../examples/data/jsonl/main.c) · Call to xrtJsonlValid in the complete program; failure jumps to unified cleanup.

```c
if ( !xrtJsonlValid((xstrview){ Text, Size }) ) goto done;
```

### `xrtJsonlWriteConfigInit`

Initializes compact single-line output, LF separation, and bounded cumulative budgets; a PRETTY configuration is invalid.

```c
void xrtJsonlWriteConfigInit(
	xjsonlwriteconfig* pConfig
);
```

#### Parameters

| Parameter | Direction | Constraints | Description |
|---|---|---|---|
| `pConfig` | output | non-null | Sets defaults and zeroes the reserved fields. |

#### Return value

| Return | Meaning | State on failure |
|---|---|---|
| void | Initialization complete. | A null pointer sets an argument error. |

#### Errors

- `XERR_ARGUMENT` — the configuration output is null.

#### Example

[jsonl](../../examples/data/jsonl/main.c) · Call to xrtJsonlWriteConfigInit in the complete program; failure jumps to unified cleanup.

```c
xrtJsonlWriteConfigInit(&Write);
```

### `xrtJsonlStringify`

Writes each Array element as one line; returns NUL-terminated text released with xrtFree. On failure the optional pSize is left unchanged.

```c
str xrtJsonlStringify(
	const xvalue* pArray,
	size_t* pSize
);
```

#### Parameters

| Parameter | Direction | Constraints | Description |
|---|---|---|---|
| `pArray` | input | non-null and of type XVALUE_ARRAY | Elements to serialize; the call does not consume the caller's owned reference. |
| `pSize` | output | may be null | On success receives the byte count including LF and excluding the NUL; unchanged on failure. |

#### Return value

| Return | Meaning | State on failure |
|---|---|---|
| non-null | Owned NUL-terminated text; empty output still returns a releasable string. | — |
| NULL | Configuration, type, encoding, or resource error. | pSize unchanged; the error is obtained via xrtGetError(). |

#### Errors

- `XERR_ARGUMENT` — a required pointer is null, or a configuration field or reserved bit is invalid.
- `XERR_MEMORY` — workspace, result, or structured-error allocation failed.
- `XERR_RANGE` — a per-record or cumulative budget was exhausted.
- `XERR_TYPE` — the serialization root is not an Array.
- `XERR_UNSUPPORTED` — a single value cannot be encoded by the selected format.
Other per-record codec errors inherit the specific Kind and code of their Cause.

#### Example

[jsonl](../../examples/data/jsonl/main.c) · Call to xrtJsonlStringify in the complete program; failure jumps to unified cleanup.

```c
Text = xrtJsonlStringify(pArray, &Size);
if ( Text == NULL ) goto done;
```

### `xrtJsonlWrite`

Writes each record and its LF to a synchronous chunked callback; the callback's borrowed bytes are valid only during the call, and bytes committed before a failure cannot be rolled back.

```c
bool xrtJsonlWrite(
	const xvalue* pArray,
	const xjsonlwriteconfig* pConfig,
	xjsonwriteproc pWrite,
	ptr pUserData
);
```

#### Parameters

| Parameter | Direction | Constraints | Description |
|---|---|---|---|
| `pArray` | input | non-null and of type XVALUE_ARRAY | Elements to serialize; the call does not consume the caller's owned reference. |
| `pConfig` | input | non-null; must be initialized | Per-record configuration and cumulative budgets; the source is not modified by this call. |
| `pWrite` | input | non-null synchronous callback | Consumes borrowed bytes in chunks; returning false aborts output. |
| `pUserData` | input | may be null | Passed through to the synchronous output callback. |

#### Return value

| Return | Meaning | State on failure |
|---|---|---|
| true | All output completed. | — |
| false | Operation failed. | Input references stay with the caller; committed bytes cannot be rolled back, and the error is obtained via xrtGetError(). |

#### Errors

- `XERR_ARGUMENT` — a required pointer is null, or a configuration field or reserved bit is invalid.
- `XERR_MEMORY` — workspace, result, or structured-error allocation failed.
- `XERR_RANGE` — a per-record or cumulative budget was exhausted.
- `XERR_TYPE` — the serialization root is not an Array.
- `XERR_UNSUPPORTED` — a single value cannot be encoded by the selected format.
- `XERR_IO` — a file operation or an output-callback failure that reported no specific error; other Kinds of the underlying cause are preserved as-is.
Other per-record codec errors inherit the specific Kind and code of their Cause.

#### Example

[jsonl](../../examples/data/jsonl/main.c) · Call to xrtJsonlWrite in the complete program; failure jumps to unified cleanup.

```c
if ( !xrtJsonlWrite(pArray, &Write, discard, NULL) ) goto done;
```

### `xrtJsonlParseFile`

Reads the file under the default budget and returns an owned Array.

```c
xvalue* xrtJsonlParseFile(
	cstr sPath
);
```

#### Parameters

| Parameter | Direction | Constraints | Description |
|---|---|---|---|
| `sPath` | input | non-null NUL-terminated path | File to read, or target file for atomic replacement. |

#### Return value

| Return | Meaning | State on failure |
|---|---|---|
| non-null | Owned Array for the caller to release. | — |
| NULL | Argument, budget, syntax, or resource error. | No partial result; the error is obtained via xrtGetError(). |

#### Errors

- `XERR_ARGUMENT` — a required pointer is null, or a configuration field or reserved bit is invalid.
- `XERR_MEMORY` — workspace, result, or structured-error allocation failed.
- `XERR_RANGE` — a per-record or cumulative budget was exhausted.
- `XERR_PROTOCOL` — a non-blank line is not one complete valid value, or strict mode hit a blank line; a per-record error keeps the underlying cause chain.
- `XERR_IO` — a file operation or an output-callback failure that reported no specific error; other Kinds of the underlying cause are preserved as-is.
Other per-record codec errors inherit the specific Kind and code of their Cause.

#### Example

[jsonl](../../examples/data/jsonl/main.c) · Call to xrtJsonlParseFile in the complete program; failure jumps to unified cleanup.

```c
pRead = xrtJsonlParseFile(Path);
if ( pRead == NULL ) goto done;
```

### `xrtJsonlReadFile`

Reads the file under the overall input cap and parses it line by line; on failure no partial Array is returned.

```c
xvalue* xrtJsonlReadFile(
	cstr sPath,
	const xjsonlreadconfig* pConfig
);
```

#### Parameters

| Parameter | Direction | Constraints | Description |
|---|---|---|---|
| `sPath` | input | non-null NUL-terminated path | File to read, or target file for atomic replacement. |
| `pConfig` | input | non-null; must be initialized | Per-record configuration and cumulative budgets; the source is not modified by this call. |

#### Return value

| Return | Meaning | State on failure |
|---|---|---|
| non-null | Owned Array for the caller to release. | — |
| NULL | Argument, budget, syntax, or resource error. | No partial result; the error is obtained via xrtGetError(). |

#### Errors

- `XERR_ARGUMENT` — a required pointer is null, or a configuration field or reserved bit is invalid.
- `XERR_MEMORY` — workspace, result, or structured-error allocation failed.
- `XERR_RANGE` — a per-record or cumulative budget was exhausted.
- `XERR_PROTOCOL` — a non-blank line is not one complete valid value, or strict mode hit a blank line; a per-record error keeps the underlying cause chain.
- `XERR_IO` — a file operation or an output-callback failure that reported no specific error; other Kinds of the underlying cause are preserved as-is.
Other per-record codec errors inherit the specific Kind and code of their Cause.

#### Example

[jsonl](../../examples/data/jsonl/main.c) · Call to xrtJsonlReadFile in the complete program; failure jumps to unified cleanup.

```c
pRead = xrtJsonlReadFile(Path, &Read);
if ( pRead == NULL ) goto done;
```

### `xrtJsonlStringifyFile`

Fully serializes the Array with default configuration and then replaces the file atomically.

```c
bool xrtJsonlStringifyFile(
	cstr sPath,
	const xvalue* pArray
);
```

#### Parameters

| Parameter | Direction | Constraints | Description |
|---|---|---|---|
| `sPath` | input | non-null NUL-terminated path | File to read, or target file for atomic replacement. |
| `pArray` | input | non-null and of type XVALUE_ARRAY | Elements to serialize; the call does not consume the caller's owned reference. |

#### Return value

| Return | Meaning | State on failure |
|---|---|---|
| true | All output completed. | — |
| false | Operation failed. | No partial result; the error is obtained via xrtGetError(). |

#### Errors

- `XERR_ARGUMENT` — a required pointer is null, or a configuration field or reserved bit is invalid.
- `XERR_MEMORY` — workspace, result, or structured-error allocation failed.
- `XERR_RANGE` — a per-record or cumulative budget was exhausted.
- `XERR_TYPE` — the serialization root is not an Array.
- `XERR_UNSUPPORTED` — a single value cannot be encoded by the selected format.
- `XERR_IO` — a file operation or an output-callback failure that reported no specific error; other Kinds of the underlying cause are preserved as-is.
Other per-record codec errors inherit the specific Kind and code of their Cause.

#### Example

[jsonl](../../examples/data/jsonl/main.c) · Call to xrtJsonlStringifyFile in the complete program; failure jumps to unified cleanup.

```c
if ( !xrtJsonlStringifyFile(Path, pArray) ) goto done;
```

### `xrtJsonlWriteFile`

Fully serializes with the advanced configuration and then replaces the file atomically; a serialization failure keeps the original file.

```c
bool xrtJsonlWriteFile(
	cstr sPath,
	const xvalue* pArray,
	const xjsonlwriteconfig* pConfig
);
```

#### Parameters

| Parameter | Direction | Constraints | Description |
|---|---|---|---|
| `sPath` | input | non-null NUL-terminated path | File to read, or target file for atomic replacement. |
| `pArray` | input | non-null and of type XVALUE_ARRAY | Elements to serialize; the call does not consume the caller's owned reference. |
| `pConfig` | input | non-null; must be initialized | Per-record configuration and cumulative budgets; the source is not modified by this call. |

#### Return value

| Return | Meaning | State on failure |
|---|---|---|
| true | All output completed. | — |
| false | Operation failed. | No partial result; the error is obtained via xrtGetError(). |

#### Errors

- `XERR_ARGUMENT` — a required pointer is null, or a configuration field or reserved bit is invalid.
- `XERR_MEMORY` — workspace, result, or structured-error allocation failed.
- `XERR_RANGE` — a per-record or cumulative budget was exhausted.
- `XERR_TYPE` — the serialization root is not an Array.
- `XERR_UNSUPPORTED` — a single value cannot be encoded by the selected format.
- `XERR_IO` — a file operation or an output-callback failure that reported no specific error; other Kinds of the underlying cause are preserved as-is.
Other per-record codec errors inherit the specific Kind and code of their Cause.

#### Example

[jsonl](../../examples/data/jsonl/main.c) · Call to xrtJsonlWriteFile in the complete program; failure jumps to unified cleanup.

```c
if ( !xrtJsonlWriteFile(Path, pArray, &Write) ) goto done;
```

