# Minimal HTTP Service Intro

> Goal: understand why the current mainline recommends building an HTTP service from `runtime + xhttpd + xvalue + json` instead of hand-assembling sockets and protocol parsing first.

[Back to Guides](README.en.md)

---

## 1. Where to Start

If your goal is already one of these:

- write a local HTTP service
- expose a health-check endpoint
- return JSON
- grow it into a larger service later

then the recommended starting point is:

- `xrtInit()`
- `xhttpd`
- `xvalue`
- `json`

not raw sockets, hand-written HTTP parsing, or manually built JSON strings.

---

## 2. Why This Mainline Is Better

XRT is not trying to be only a bucket of low-level parts. It is trying to provide a mainline that can keep growing without fragmenting.

Starting from:

- runtime
- HTTP service layer
- structured data model

makes later growth into routes, config, templates, futures, coroutines, and external API calls much cleaner.

---

## 3. What a Minimal Service Should Contain

A structurally correct minimal service usually needs:

1. initialize the XRT runtime
2. create and start an HTTP service
3. register one simple handler
4. build a structured response inside the handler
5. clean up runtime state at the end

---

## 4. Why Responses Should Start as xvalue

The recommended pattern is:

1. build the response as `xvalue`
2. serialize to JSON in the final output step

For example:

```c
xvalue vRes = xvoTable();

xvoTableSetBool( vRes, "ok", TRUE );
xvoTableSetText( vRes, "service", "xrt-demo" );
xvoTableSetText( vRes, "status", "running" );
```

That keeps later additions of fields, nested objects, and arrays straightforward.

---

## 5. Why xhttpd Is the Better Entry Than Raw Networking

If what you are building is already an HTTP service, then:

- routing
- request / response
- headers
- status codes

are all application-layer concerns.

The current mainline therefore recommends:

- keep low-level networking as the foundation
- build HTTP services directly on `xhttpd`

---

## 6. Suggested Reading

- [XHTTPD API](../api/api-xhttpd.en.md)
- [Value API](../api/api-value.en.md)
- [JSON API](../api/api-json.en.md)
- [Minimal HTTP Service with XRT](../case/minimal-http-service.en.md)

---

**One-sentence summary:** In the current XRT mainline, the right starting point for a minimal HTTP service is `runtime + xhttpd + xvalue + json`, because that is the shape that grows into a real service most naturally.
