# XRT Case Study: Minimal HTTP Service

> A minimal service chain built on the current mainline: runtime + xhttpd + xvalue + JSON.

[中文](minimal-http-service.md) | [Back to Case Studies](README.en.md)

---

## Mainline View

The point of this case is not “how to print hello world”.

The real point is to show the current service-side chain:

- runtime
- `xhttpd`
- `xvalue`
- JSON

This is the foundation for many current XRT server-side scenarios.

---

## Why This Is the Recommended Server Entry

If you are building a current XRT service, the recommended first entry is usually:

- `xhttpd`

and not old low-level public network surfaces.

That keeps the service layer aligned with:

- the current network mainline
- the current TLS session model
- the current structured-data mainline

---

## Related Reading

- [XHTTPD](../api/api-xhttpd.en.md)
- [Value](../api/api-value.en.md)
- [JSON](../api/api-json.en.md)
- [Guide: Minimal HTTP Service Intro](../guide/minimal-http-service-intro.en.md)
