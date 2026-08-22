# XSON Processing Library

> The current mainline private serialization format for full `xvalue` round-trip, while remaining compatible with JSON

[English](api-xson.en.md) | [中文](api-xson.md) | [Back to Index](README.en.md)

---

## Current Status

The Chinese document is currently the source of truth for XSON:

- [api-xson.md](api-xson.md)

This English page is kept as the official entry so the mainline API index stays complete.



## What XSON Is

`XSON` is the XRT private extension format for full `xvalue` serialization.

Current boundary:

- `JSON` remains the standard exchange format
- `XSON` is used when `xvalue` must preserve `time / list / set / class`
- every valid JSON text must remain valid XSON with the same meaning



## Main APIs

```c
XXAPI xvalue xrtParseXSON(str sText, size_t iSize);
XXAPI xvalue xrtParseXSONEx(str sText, size_t iSize, uint32 iFlags);
XXAPI xvalue xrtParseXSON_File(str sFile);
XXAPI xvalue xrtParseXSON_FileEx(str sFile, uint32 iFlags);

XXAPI str xrtStringifyXSON(xvalue varVal, int bFormat, uint32 iFlags, size_t* pRetSize);
XXAPI int xrtStringifyXSON_File(str sFile, xvalue varVal, int bFormat, uint32 iFlags);
```

Flags:

```c
#define XSON_F_IGNORE_UNSUPPORTED_ENCODE	0x0001u
#define XSON_F_IGNORE_UNSUPPORTED_DECODE	0x0002u
```



## Supported Types

- `null`
- `bool`
- `int`
- `float`
- `text`
- `time`
- `array`
- `list`
- `dict(table)`
- `set(coll)`
- `class`

Unsupported:

- `ptr(point)`
- `func`
- `custom`



## Syntax Summary

- JSON-compatible values keep their original JSON syntax
- XSON adds:
	- `array[...]`
	- `list[...]`
	- `dict{...}`
	- `set{...}`
	- `time(...)`
	- `class(BASE64)`
- Empty containers:
	- `[]` is empty array
	- `{}` is empty dict
	- `list[]` is empty list
	- `set{}` is empty set



## Benchmark Snapshot

The current benchmark entry is:

```bash
release/x64/test.exe json_xson_bench
```

Documented results were measured on `2026-03-25` with:

- `rounds=100`
- `warmup=10`
- `items=128`

On the JSON-compatible payload in that run:

- XSON parse was `3.23%` slower than JSON parse
- XSON compact stringify was `58.08%` slower than JSON stringify
- XSON formatted stringify was `94.99%` slower than JSON formatted stringify

For full details, see the Chinese document:

- [api-xson.md](api-xson.md)



## Related Documents

- [JSON Processing Library](api-json.en.md)
- [Value Dynamic Type System](api-value.en.md)
- [API Index](README.en.md)
