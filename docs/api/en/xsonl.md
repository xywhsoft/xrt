# XSONL

XSONL converts between a line-delimited record sequence and an xvalue Array, one element per record; individual values follow the XSON type and codec rules. Suited to logs, bulk import, and file exchange.

## Trimming and dependencies

| Public selection macro | Implementation macro | Direct dependencies |
|---|---|---|
| `XRT_MODULE_XSONL_CORE` | `XRT_FEATURE_XSONL_CORE` | core |
| `XRT_MODULE_XSONL_READ` | `XRT_FEATURE_XSONL_READ` | xsonl_core, xson_read |
| `XRT_MODULE_XSONL_WRITE` | `XRT_FEATURE_XSONL_WRITE` | xsonl_core, xson_write |
| `XRT_MODULE_XSONL_FILE` | `XRT_FEATURE_XSONL_FILE` | xsonl_read, xsonl_write, file_whole |
| `XRT_MODULE_XSONL` | `XRT_FEATURE_XSONL` | xsonl_file |

The header is `<xrt/xsonl.h>` and can also be included through the umbrella header `<xrt.h>`. Selecting only reading or only writing does not pull in the file module.

## Stability contract

- Blank lines are ignored by default: empty lines, and lines containing only ASCII spaces, tabs, or CR. LF, CRLF, and mixed endings are accepted; a lone CR is not a record separator. Unicode whitespace never turns a line into a blank line.
- Empty input and input consisting entirely of blank lines both return an empty Array. Strict blank-line mode still accepts zero-byte input and a single terminating newline after the last record; extra blank lines are errors.
- A line must contain exactly one complete value; the final complete record may omit the terminating newline. Multi-line values, multiple root values on one line, a BOM, invalid UTF-8, and a truncated trailing record all fail.
- An escaped newline inside a string does not split records. Array records preserve nesting; for example the text `[]\n` yields an Array containing one empty array.
- Deserialization hands over the owned Array only after every record succeeds; on failure it returns C NULL and frees all partial results.
- The serialization root must be `XVALUE_ARRAY`; null, the empty string, and empty containers are real records. The SKIP strategy cannot skip a whole record, it only applies to members inside a single root value.
- Output is compact, one LF appended per record, and an empty Array produces zero bytes. A PRETTY configuration is rejected. The configuration is snapshotted by value; a callback mutating the original configuration does not affect the in-flight call.
- `Valid` reuses the DOM-free syntax validation of XSON: blank lines are ignored, the default cumulative budgets and built-in tags are checked, and duplicate-key DOM policy does not participate.
- File writing completes the in-memory serialization first and then replaces the file atomically; a serialization failure leaves the target untouched. Synchronous callback writing may have committed partial records before failing; those bytes cannot be rolled back.
- Single-line comments and trailing commas are compatibility extensions enabled only inside Record; they must not continue across physical lines.
- XSONL supports the bytes, time, set, intmap, and non-finite float tags plus explicit custom codec callbacks; objects and side effects created by custom callbacks are the caller's responsibility, the cumulative value budget counts input syntax values without traversing callback-created objects, and the cumulative decode budget counts only built-in bytes tags.

Threading and ownership: the API carries no implicit locks; each call owns an independent workspace, and the caller synchronizes shared mutable input. Writing iterates a backing snapshot of the outer Array, so Value subtree ownership rules still apply during callbacks. Input text and configuration are borrowed until the call returns; the result Array is released with `xrtValueRelease`, the result string with `xrtFree`. The output callback consumes bytes before returning.

## Constants

| Constant | Value | Meaning |
|---|---|---|
| `XXSONL_READ_REJECT_EMPTY_LINES` | `UINT32_C(0x00000001)` | Blank lines become errors; off by default. |

Configurations must be initialized with Init; all budgets are non-zero. Record defaults come from the existing XSON initialization functions. Overall input/output defaults to 64 MiB; record count and syntax value count default to 1000000. The cumulative decode budget for built-in bytes tags defaults to 64 MiB.

## Types

### `xxsonlerror`

```c
typedef enum xxsonlerror {
	XXSONL_ERROR_CONFIG = 1801,
	XXSONL_ERROR_SYNTAX,
	XXSONL_ERROR_LIMIT,
	XXSONL_ERROR_RECORD,
	XXSONL_ERROR_TYPE,
	XXSONL_ERROR_OUTPUT,
	XXSONL_ERROR_IO,
	XXSONL_ERROR_STATE
} xxsonlerror;
```

Errors are obtained via `xrtGetError()` with format domain `xrt.xsonl`; argument and underlying OOM failures may also surface as Core errors directly. RECORD/OUTPUT/IO wrappers preserve the Cause Kind. If building the wrapper itself fails to allocate, the original cause error is kept and no location data is promised.

| Value | Meaning |
|---|---|
| `XXSONL_ERROR_CONFIG` | Configuration not initialized, reserved bits non-zero, a zero budget, or PRETTY enabled. |
| `XXSONL_ERROR_SYNTAX` | A blank line in strict blank-line mode. |
| `XXSONL_ERROR_LIMIT` | Overall or per-record budget exhausted. |
| `XXSONL_ERROR_RECORD` | Encoding/decoding a single value or appending it to the Array failed; the Cause keeps the specific reason. |
| `XXSONL_ERROR_TYPE` | The serialization root is not an Array. |
| `XXSONL_ERROR_OUTPUT` | Synchronous output callback failed. |
| `XXSONL_ERROR_IO` | Budgeted file read or atomic file replacement failed. |
| `XXSONL_ERROR_STATE` | Reserved code for an invalid line-processing state. |

### `xxsonllocation`

```c
typedef struct xxsonllocation {
	size_t Offset;
	size_t Line;
	size_t Column;
	size_t RecordIndex;
} xxsonllocation;
```

| Field | Type | Meaning |
|---|---|---|
| `Offset` | `size_t` | Zero-based byte offset in the raw input. |
| `Line` | `size_t` | One-based physical line number split on LF; CRLF counts as one line, and ignored blank lines still count. |
| `Column` | `size_t` | One-based UTF-8 byte column within the current physical line; not counted in Unicode characters. |
| `RecordIndex` | `size_t` | Zero-based record index; blank lines do not increment it, and a blank-line error points at the next pending record index. |

### `xxsonlreadflag`

```c
typedef enum xxsonlreadflag {
	XXSONL_READ_REJECT_EMPTY_LINES = UINT32_C(0x00000001)
} xxsonlreadflag;
```

| Value | Meaning |
|---|---|
| `XXSONL_READ_REJECT_EMPTY_LINES` | Turns the blank lines ignored by default into syntax errors. |

### `xxsonlreadconfig`

```c
typedef struct xxsonlreadconfig {
	xxsonreadconfig Record;
	uint32 Flags;
	size_t MaxInputBytes;
	size_t MaxRecords;
	size_t MaxTotalValues;
	size_t MaxTotalDecodedBytes;
	uint32 Reserved[4];
} xxsonlreadconfig;
```

| Field | Type | Meaning |
|---|---|---|
| `Record` | `xxsonreadconfig` | Per-record configuration; MaxInputBytes/MaxOutputBytes exclude the CRLF/LF separators, and depth counts from each record's root value. |
| `Flags` | `uint32` | Defaults to 0; XXSONL_READ_REJECT_EMPTY_LINES enables blank-line errors. |
| `MaxInputBytes` | `size_t` | Cap on the whole raw input, including ignored blank lines and all separators; default 64 MiB. |
| `MaxRecords` | `size_t` | Cap on non-blank record count, equal to the maximum element count of the result Array; default 1000000. |
| `MaxTotalValues` | `size_t` | Cumulative cap on syntax values across all records, including values dropped by duplicate-key policy and excluding the synthesized Array; default 1000000. |
| `MaxTotalDecodedBytes` | `size_t` | Cap on cumulative decoded bytes for built-in bytes tags, checked before buffer allocation; default 64 MiB. |
| `Reserved` | `uint32[4]` | Reserved space; must be all zero. |

### `xxsonlwriteconfig`

```c
typedef struct xxsonlwriteconfig {
	xxsonwriteconfig Record;
	size_t MaxOutputBytes;
	size_t MaxRecords;
	uint32 Reserved[4];
} xxsonlwriteconfig;
```

| Field | Type | Meaning |
|---|---|---|
| `Record` | `xxsonwriteconfig` | Per-record configuration; MaxInputBytes/MaxOutputBytes exclude the CRLF/LF separators, and depth counts from each record's root value. |
| `MaxOutputBytes` | `size_t` | Cap on total output bytes, including every LF and excluding the trailing NUL of the result; default 64 MiB. |
| `MaxRecords` | `size_t` | Cap on non-blank record count, equal to the maximum element count of the result Array; default 1000000. |
| `Reserved` | `uint32[4]` | Reserved space; must be all zero. |

## Text, configuration, and file interfaces

### `xrtXsonlErrorLocation`

Reads the global byte offset, one-based physical line/column, and zero-based record index; leaves the output unchanged when no location is present.

```c
bool xrtXsonlErrorLocation(
	const xerror* pError,
	xxsonllocation* pLocation
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

[xsonl](../../examples/data/xsonl/main.c) · Call to xrtXsonlErrorLocation in the complete program; failure jumps to unified cleanup.

```c
if ( !xrtXsonlErrorLocation(xrtGetError(), &Location) ) goto done;
```

### `xrtXsonlReadConfigInit`

Initializes blank-line skipping by default, strict per-record syntax, and bounded cumulative budgets.

```c
void xrtXsonlReadConfigInit(
	xxsonlreadconfig* pConfig
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

[xsonl](../../examples/data/xsonl/main.c) · Call to xrtXsonlReadConfigInit in the complete program; failure jumps to unified cleanup.

```c
xrtXsonlReadConfigInit(&Read);
```

### `xrtXsonlParse`

Parses the record sequence with default configuration; returns an owned Array on success, an empty Array for empty input.

```c
xvalue* xrtXsonlParse(
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

[xsonl](../../examples/data/xsonl/main.c) · Call to xrtXsonlParse in the complete program; failure jumps to unified cleanup.

```c
pArray = xrtXsonlParse(XRT_STR_LITERAL("{\"id\":1}\n\n[2,3]\r\nnull\n"));
if ( pArray == NULL ) goto done;
```

### `xrtXsonlRead`

Parses all records per configuration; on failure frees partial results and returns NULL. Release the result with xrtValueRelease.

```c
xvalue* xrtXsonlRead(
	xstrview Text,
	const xxsonlreadconfig* pConfig
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

[xsonl](../../examples/data/xsonl/main.c) · Call to xrtXsonlRead in the complete program; failure jumps to unified cleanup.

```c
pRead = xrtXsonlRead((xstrview){ Text, Size }, &Read);
if ( pRead == NULL ) goto done;
```

### `xrtXsonlValid`

Ignores blank lines by default and validates per-line syntax and cumulative budgets without building a Value DOM; duplicate-key policy does not participate.

```c
bool xrtXsonlValid(
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

[xsonl](../../examples/data/xsonl/main.c) · Call to xrtXsonlValid in the complete program; failure jumps to unified cleanup.

```c
if ( !xrtXsonlValid((xstrview){ Text, Size }) ) goto done;
```

### `xrtXsonlWriteConfigInit`

Initializes compact single-line output, LF separation, and bounded cumulative budgets; a PRETTY configuration is invalid.

```c
void xrtXsonlWriteConfigInit(
	xxsonlwriteconfig* pConfig
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

[xsonl](../../examples/data/xsonl/main.c) · Call to xrtXsonlWriteConfigInit in the complete program; failure jumps to unified cleanup.

```c
xrtXsonlWriteConfigInit(&Write);
```

### `xrtXsonlStringify`

Writes each Array element as one line; returns NUL-terminated text released with xrtFree. On failure the optional pSize is left unchanged.

```c
str xrtXsonlStringify(
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

[xsonl](../../examples/data/xsonl/main.c) · Call to xrtXsonlStringify in the complete program; failure jumps to unified cleanup.

```c
Text = xrtXsonlStringify(pArray, &Size);
if ( Text == NULL ) goto done;
```

### `xrtXsonlWrite`

Writes each record and its LF to a synchronous chunked callback; the callback's borrowed bytes are valid only during the call, and bytes committed before a failure cannot be rolled back.

```c
bool xrtXsonlWrite(
	const xvalue* pArray,
	const xxsonlwriteconfig* pConfig,
	xxsonwriteproc pWrite,
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

[xsonl](../../examples/data/xsonl/main.c) · Call to xrtXsonlWrite in the complete program; failure jumps to unified cleanup.

```c
if ( !xrtXsonlWrite(pArray, &Write, discard, NULL) ) goto done;
```

### `xrtXsonlParseFile`

Reads the file under the default budget and returns an owned Array.

```c
xvalue* xrtXsonlParseFile(
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

[xsonl](../../examples/data/xsonl/main.c) · Call to xrtXsonlParseFile in the complete program; failure jumps to unified cleanup.

```c
pRead = xrtXsonlParseFile(Path);
if ( pRead == NULL ) goto done;
```

### `xrtXsonlReadFile`

Reads the file under the overall input cap and parses it line by line; on failure no partial Array is returned.

```c
xvalue* xrtXsonlReadFile(
	cstr sPath,
	const xxsonlreadconfig* pConfig
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

[xsonl](../../examples/data/xsonl/main.c) · Call to xrtXsonlReadFile in the complete program; failure jumps to unified cleanup.

```c
pRead = xrtXsonlReadFile(Path, &Read);
if ( pRead == NULL ) goto done;
```

### `xrtXsonlStringifyFile`

Fully serializes the Array with default configuration and then replaces the file atomically.

```c
bool xrtXsonlStringifyFile(
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

[xsonl](../../examples/data/xsonl/main.c) · Call to xrtXsonlStringifyFile in the complete program; failure jumps to unified cleanup.

```c
if ( !xrtXsonlStringifyFile(Path, pArray) ) goto done;
```

### `xrtXsonlWriteFile`

Fully serializes with the advanced configuration and then replaces the file atomically; a serialization failure keeps the original file.

```c
bool xrtXsonlWriteFile(
	cstr sPath,
	const xvalue* pArray,
	const xxsonlwriteconfig* pConfig
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

[xsonl](../../examples/data/xsonl/main.c) · Call to xrtXsonlWriteFile in the complete program; failure jumps to unified cleanup.

```c
if ( !xrtXsonlWriteFile(Path, pArray, &Write) ) goto done;
```

