# Rendering HTML with Template

> This case shows how `template + xvalue` form a natural rendering chain in the current XRT mainline.

[Back to Case Studies](README.en.md)

---

## 1. Scenario

Suppose you need to render a simple HTML page showing:

- a title
- a user name
- a status string
- a list of tags

In the current XRT mainline, the recommended pairing is:

- data model: `xvalue`
- output structure: `template`

---

## 2. Why `template + xvalue` Is Recommended

This pairing gives you:

- separation between page structure and business data
- more readable output logic
- easy reuse of the same data for JSON, config, or HTTP output
- simpler field growth later

It is much more stable than large string-concatenation blocks.

---

## 3. The Recommended Mainline

The recommended chain is:

1. build page data as `xvalue`
2. use it as template context
3. render the final HTML

That keeps the internal model aligned with the rest of XRT's structured-data mainline.

---

## 4. Why xvalue Fits Well Here

A page is naturally nested data:

- page metadata
- user object
- tag arrays
- conditional flags
- optional sections

`xvalue` is already designed to carry this combination of:

- table
- array
- string / int / bool

and it remains compatible with JSON, configuration, and network output on the same data mainline.

---

## 5. Suggested Reading

- [Template API](../api/api-template.en.md)
- [Value API](../api/api-value.en.md)
- [JSON API](../api/api-json.en.md)
- [Configuration System with xvalue + json](json-config-system.en.md)
- [Minimal HTTP Service with XRT](minimal-http-service.en.md)

---

**One-sentence summary:** In the current XRT mainline, `template + xvalue` is the natural way to render HTML because template handles structure while `xvalue` keeps data unified with the rest of the runtime.
