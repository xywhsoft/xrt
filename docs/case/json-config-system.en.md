# Configuration System with xvalue + JSON

> A minimal but complete composition case: use `xvalue` for runtime configuration, JSON for persistence and exchange, and keep defaults, patching, and business-side reads on one mainline.

[Back to Case Studies](README.en.md) | [Back to API Index](../api/README.en.md)

---

## 1. Scenario

Many programs need to:

- load configuration from a file
- provide defaults
- patch missing fields later
- keep reads simple in business code
- write configuration back to JSON

The current XRT mainline recommends:

- runtime configuration as `xvalue table`
- JSON as the file format
- loading, patching, reading, and writing on one data model

---

## 2. Why This Mainline

The point is that:

- `json`
- `xvalue`
- strings
- files

are not unrelated helpers in XRT. They are intended to connect naturally.

So you can:

1. use `xrtParseJSON_File()` to get an `xvalue`
2. inject defaults with `xvoTableSet*()`
3. read business values with `xvoTableGet*()`
4. write back with `xrtStringifyJSON_File()`

---

## 3. Target Structure

Assume `config.json` looks like this:

```json
{
	"server": {
		"host": "127.0.0.1",
		"port": 8080
	},
	"log": {
		"level": "info"
	}
}
```

The program should:

- create defaults if the file does not exist
- patch missing fields if the file exists but is incomplete
- always let business logic read from one unified config object

---

## 4. What This Case Demonstrates

### One Internal Model

You do not maintain:

- one JSON structure
- one runtime struct
- one extra serializer shape for writing back

Everything stays centered on one `xvalue table`.

### One API Family for Defaults and Patching

Creating defaults, patching missing fields, and runtime changes all reuse:

- `xvoTableSet*`
- `xvoTableGet*`
- `xvoTableExists`

### Natural Filesystem Integration

The file boundary is already covered by:

- `xrtParseJSON_File()`
- `xrtStringifyJSON_File()`

### Easy Growth into HTTP, Template, and AI Cases

Because config already lives as `xvalue`, it can later flow into:

- template rendering
- HTTP request building
- AI request payloads
- hot-reload logic

---

## 5. Suggested Reading

- [Value API](../api/api-value.en.md)
- [JSON API](../api/api-json.en.md)
- [Template API](../api/api-template.en.md)
- [Minimal HTTP Service with XRT](minimal-http-service.en.md)

---

**One-sentence summary:** In the current XRT mainline, `xvalue + JSON` gives configuration one stable internal model instead of splitting persistence, defaults, and business reads across multiple unrelated structures.
