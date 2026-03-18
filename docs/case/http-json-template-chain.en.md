# A Complete HTTP + JSON + Template Service Chain

> This case shows how HTTP service, structured data, and page rendering connect on one mainline in the current XRT design.

[Back to Case Studies](README.en.md)

---

## 1. What Problem This Case Solves

Many programs eventually need to:

- expose HTTP endpoints
- handle JSON requests and responses
- render HTML pages

The current XRT mainline prefers one chain instead of three unrelated models:

- `xhttpd`
- `xvalue`
- `json`
- `template`

---

## 2. The Recommended Structure

The clean model is:

- service entry: `xhttpd`
- data model: `xvalue`
- exchange format: `json`
- presentation output: `template`

That prevents the program from switching internal models between layers.

---

## 3. Why xvalue Is Central

The key point is:

> use `xvalue` as the unified internal structured-data model.

Then:

- JSON responses can be serialized from `xvalue`
- HTML pages can use the same `xvalue` tree as template context
- configuration, logs, AI responses, and rendered output can reuse the same object tree

---

## 4. Why This Is More Stable

If the API layer and page layer each keep their own model, you usually get:

- inconsistent field naming
- duplicated business logic
- drift between API output and page rendering

If both are unified around `xvalue`, JSON and HTML become just two different exits of the same internal data.

---

## 5. Suggested Reading

- [XHTTPD API](../api/api-xhttpd.en.md)
- [Value API](../api/api-value.en.md)
- [JSON API](../api/api-json.en.md)
- [Template API](../api/api-template.en.md)
- [Minimal HTTP Service with XRT](minimal-http-service.en.md)
- [Rendering HTML with Template](template-render-html.en.md)

---

**One-sentence summary:** In the current XRT mainline, the most stable way to combine HTTP, JSON, and template rendering is to keep them all centered on `xvalue` instead of letting three separate models grow in parallel.
