# JSON Processing Library

> JSON parsing, serialization, and the `xvalue`-centered structured data workflow

[English](api-json.en.md) | [中文](api-json.md) | [Back to Index](README.en.md)

---

## Table of Contents

- [Position in the Current Mainline](#position-in-the-current-mainline)
- [Boundary Between JSON and XSON](#boundary-between-json-and-xson)
- [What This Module Solves](#what-this-module-solves)
- [Two Ways to Work with JSON](#two-ways-to-work-with-json)
- [Recommended Mainline: xvalue + JSON](#recommended-mainline-xvalue--json)
- [Parse and Stringify](#parse-and-stringify)
- [JSON as an Exchange Format](#json-as-an-exchange-format)
- [Shared-Root Considerations](#shared-root-considerations)
- [When to Use SAX Parsing](#when-to-use-sax-parsing)
- [Recommended Reading Path](#recommended-reading-path)

---

## Position in the Current Mainline

In current XRT, JSON is **not** the internal data model of the runtime.

The mainline is:

- `xvalue` is the runtime’s structured data model
- JSON is the exchange format
- HTTP, template rendering, and AI-facing data flows are expected to converge on:
	- parse JSON into `xvalue`
	- process data in `xvalue`
	- stringify `xvalue` back to JSON when needed

The preferred model is:

**JSON in, `xvalue` inside, JSON out**

---

## Boundary Between JSON and XSON

In the current mainline, JSON and XSON have different responsibilities:

- `JSON`
	- standard exchange format
	- use `xrtParseJSON()` / `xrtStringifyJSON()`
- `XSON`
	- XRT private extension format
	- full `xvalue` serialization
	- remains JSON-compatible while adding `time / list / set / class`
	- use `xrtParseXSON()` / `xrtStringifyXSON()`

If your data contains any of these types, switch from JSON to XSON:

- `time`
- `list`
- `set(coll)`
- `class`

See:

- [XSON Processing Library](api-xson.en.md)

---

## What This Module Solves

The JSON module provides three layers:

### 1. JSON parsing

- parse JSON text into `xvalue`
- parse JSON files into `xvalue`
- validate whether input is structurally valid JSON

### 2. JSON serialization

- stringify `xvalue` to compact JSON
- stringify `xvalue` to formatted JSON
- write JSON files directly

### 3. Low-level streaming processing

The SAX-style API lets you:

- process large JSON payloads incrementally
- inspect tokens as they arrive
- build stream-oriented transforms or filters

For most application code, the `xvalue` path is the mainline.

---

## Two Ways to Work with JSON

### 1. High-level mainline: `xvalue`

Recommended for almost all normal application code:

- configuration files
- HTTP request/response bodies
- AI request payloads
- AI response parsing
- templating input
- business-layer structured data

Main APIs:

```c
xvalue xrtParseJSON(str sText, size_t iSize);
xvalue xrtParseJSON_File(str sFile);
str xrtStringifyJSON(xvalue pVal, int bFormat, size_t* pRetSize);
int xrtStringifyJSON_File(str sFile, xvalue pVal, int bFormat);
```

### 2. Low-level path: SAX

Recommended only when you truly need streaming behavior or token-level control.

```c
int xrtJsonParseSAX(str sText, size_t iSize, json_sax_cb_t procCB);
```

Use SAX when:

- the input is very large
- you want to process and discard incrementally
- you need custom token-driven handling

For normal application development, prefer the `xvalue` path.

---

## Recommended Mainline: xvalue + JSON

The recommended workflow is:

1. Parse JSON into `xvalue`
2. Use `table / array / list / coll` APIs to process it
3. Pass the result through HTTP, template, coroutine, future, or service logic
4. Stringify back to JSON only at the boundary

### Example: parse, modify, stringify

```c
#include "xrt.h"
#include <stdio.h>

int main(void)
{
	str sJSON;
	str sOut;
	size_t iOutSize;
	xvalue pDoc;

	xrtInit();

	sJSON = (str)"{\"name\":\"Alice\",\"age\":25,\"skills\":[\"C\",\"XRT\"]}";
	pDoc = xrtParseJSON(sJSON, 0);
	if ( pDoc == NULL ) {
		printf("parse failed: %s\n", (char*)xrtGetError());
		xrtUnit();
		return 1;
	}

	xvoTableSetInt(pDoc, "age", 0, 26);
	xvoTableSetBool(pDoc, "active", 0, TRUE);

	sOut = xrtStringifyJSON(pDoc, TRUE, &iOutSize);
	if ( sOut != NULL ) {
		printf("%s\n", (char*)sOut);
		xrtFree(sOut);
	}

	xvoUnref(pDoc);
	xrtUnit();
	return 0;
}
```

### Example: build runtime data first, then emit JSON

```c
xvalue pUser = xvoCreateTable();
xvalue pTags = xvoCreateArray();
size_t iSize = 0;
str sJSON = NULL;

xvoTableSetText(pUser, "name", 0, "Tom", 0, FALSE);
xvoTableSetInt(pUser, "age", 0, 30);
xvoArrayAppendText(pTags, "network", 0, FALSE);
xvoArrayAppendText(pTags, "async", 0, FALSE);
xvoTableSetValue(pUser, "tags", 0, pTags, TRUE);

sJSON = xrtStringifyJSON(pUser, FALSE, &iSize);
if ( sJSON != NULL ) {
	printf("%s\n", (char*)sJSON);
	xrtFree(sJSON);
}

xvoUnref(pUser);
```

---

## Parse and Stringify

### Parse from text

```c
xvalue xrtParseJSON(str sText, size_t iSize);
```

Typical cases:

- HTTP response body
- buffered request payload
- LLM result text
- config text loaded from another source

### Parse from file

```c
xvalue xrtParseJSON_File(str sFile);
```

### Stringify to text

```c
str xrtStringifyJSON(xvalue pVal, int bFormat, size_t* pRetSize);
```

Notes:

- `bFormat = FALSE` for compact output
- `bFormat = TRUE` for readable formatted output
- returned string must be released with `xrtFree`

### Stringify to file

```c
int xrtStringifyJSON_File(str sFile, xvalue pVal, int bFormat);
```

---

## JSON as an Exchange Format

In current XRT, JSON should usually be treated as a boundary format, not as the center of your in-memory design.

Recommended:

- parse incoming JSON once
- do all internal processing with `xvalue`
- stringify only when leaving the runtime boundary

Not recommended:

- repeatedly parse small JSON fragments inside business logic
- keep both a manual JSON tree and an `xvalue` tree in sync
- use JSON as your primary in-memory container when `xvalue` already exists

---

## Shared-Root Considerations

If parsed JSON needs to cross threads, treat it as a normal `xvalue` ownership problem.

Create or promote data into an explicit shared root before cross-thread access:

```c
xvalue pShared = xvoCreateTableEx(XRT_OBJMODE_SHARED);
```

JSON itself does not create a special concurrency model. Once parsed, it becomes normal `xvalue` data.

---

## When to Use SAX Parsing

Use the SAX API when you need incremental or token-driven handling.

Typical fit:

- very large JSON files
- large HTTP payloads
- custom analytics or filters
- import pipelines where you consume and discard

For normal app code, prefer `xrtParseJSON`.

---

## Recommended Reading Path

For the current mainline, read in this order:

1. [Value Dynamic Type System](api-value.en.md)
2. This page
3. [Template Engine](api-template.en.md)
4. [Future / Task / Promise](api-future-task-promise.en.md)
5. [XHTTP](api-xhttp.en.md)
6. [XHTTPD](api-xhttpd.en.md)

If your target is AI or service-side development, continue with:

- [XNet v2 Mainline](api-xnet-v2.en.md)
- [TLS Session Mainline](api-network-tls.en.md)

---

## Related Documents

- [Value Dynamic Type System](api-value.en.md)
- [Template Engine](api-template.en.md)
- [Future / Task / Promise](api-future-task-promise.en.md)
- [XHTTP](api-xhttp.en.md)
- [XHTTPD](api-xhttpd.en.md)
- [API Index](README.en.md)
