# xvalue and JSON Intro

> Goal: understand why `xvalue` is the core dynamic data model in XRT, and how it should cooperate with JSON in the current mainline.

[Back to Guides](README.en.md)

---

## 1. Why Learn xvalue First

Many languages have a built-in dynamic object model:

- JavaScript: object / array
- Python: dict / list
- Lua: table

In XRT, this role is played by `xvalue`.

Its meaning is not “yet another dynamic type”, but a unified model for:

- configuration data
- JSON data
- template context
- structured HTTP request/response fields
- dynamic results around future / task / network mainlines

If you understand `xvalue`, you already understand one of the central design lines in XRT: handling complex data in a script-like way while still staying inside a C infrastructure library.

---

## 2. What xvalue Is Actually Doing

`xvalue` is not one single type. It is a unified entry for a family of data objects.

In the current mainline, you can think about it as:

- scalar values
	- null
	- bool
	- int
	- float
	- string
- container values
	- array
	- list
	- coll
	- table

What matters is not only the number of types, but the fact that:

- one API family organizes different data layers
- one model connects to JSON, templates, configuration, and network exchange
- the same objects can later participate in shared-root and multi-thread contracts

---

## 3. The Most Common Usage Pattern

### 3.1 Build a Table

```c
xvalue vUser = xvoTable();

xvoTableSetText( vUser, "name", "Alice" );
xvoTableSetInt( vUser, "age", 28 );
xvoTableSetBool( vUser, "active", TRUE );
```

### 3.2 Build an Array

```c
xvalue vTags = xvoArray();

xvoArrayAppendText( vTags, "c" );
xvoArrayAppendText( vTags, "network" );
xvoArrayAppendText( vTags, "runtime" );
```

### 3.3 Compose a Larger Object Tree

```c
xvalue vRoot = xvoTable();
xvalue vTags = xvoArray();

xvoArrayAppendText( vTags, "c" );
xvoArrayAppendText( vTags, "json" );

xvoTableSetText( vRoot, "project", "xrt" );
xvoTableSetValue( vRoot, "tags", vTags );
```

---

## 4. The Relationship Between xvalue and JSON

The recommended way to think about them is:

- `xvalue`: the internal program data model
- `JSON`: a data exchange format
- `XSON`: the private extension exchange format when `xvalue` must preserve `time / list / coll / class`

That means the program should usually manipulate `xvalue` internally, and only encode or decode JSON when exchanging data with the outside world.

If the data contains types that standard JSON cannot fully express, such as:

- `time`
- `list`
- `coll(set)`
- `class`

then the boundary format should switch from JSON to XSON.

### 4.1 Parse JSON into xvalue

```c
str sJson = "{ \"name\": \"Alice\", \"age\": 28 }";
xvalue vData = xjsonParse( sJson );
```

### 4.2 Serialize xvalue Back to JSON

```c
str sOut = xjsonStringify( vData );
printf( "%s\n", (char*)sOut );
```

---

## 5. Why JSON Should Not Become the Internal Main Model

The current XRT mainline prefers:

1. parse JSON into `xvalue`
2. manipulate `xvalue` inside the program
3. serialize back to JSON only at the boundary

That keeps structure, mutation, and output under one model.

---

## 6. Cooperation with Template, Config, and Networking

- template rendering works best when directly fed with `xvalue`
- config systems become cleaner on `xvalue + JSON`
- HTTP / WebSocket / LLM API payloads become easier to assemble when the internal mainline is already `xvalue`

---

## 7. Thread-Safety Boundary in the Current Mainline

- ordinary `xvalue` is not a free-for-all cross-thread object
- if you need cross-thread sharing, use the current shared-root contract

---

## 8. Recommended Learning Order

1. create `table / array`
2. learn `SetText / SetInt / SetBool / SetValue`
3. learn `xjsonParse / xjsonStringify`
4. move upward to templates, configuration, and HTTP

---

## 9. Suggested Reading

- [Value API](../api/api-value.en.md)
- [JSON API](../api/api-json.en.md)
- [Template API](../api/api-template.en.md)
- [Configuration System with xvalue + json](../case/json-config-system.en.md)

---

**One-sentence summary:** In XRT, `xvalue` is the unified internal dynamic data model, and JSON is one of its most important exchange formats. The earlier you organize data this way, the more naturally templates, configuration, networking, and AI-facing scenarios fit together.
