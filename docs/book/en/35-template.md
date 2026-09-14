---
num: 35
slug: template
title: The Template Engine
volume: 卷四 文本与结构化数据
type: practice
lead: Compile once, render a million times — dynamic path access, typed formatting, control structures and function calls.
api: template, value
---

## Orientation

`xtemplate` standardizes "data + template → text": **compile/render separation** — `xrtTemplateCompile` parses the template into an immutable node tree (reference counted, the same lifecycle as Chapter 30's regex), and `xrtTemplateRender` renders owning text from a value tree (Chapter 31's xvalue). Three interpolation prefixes cover three common needs: `{$path}` direct output, `{%path:fmt}` number formatting (the format string is Chapter 26's syntax), and `{&path:fmt}` time formatting; control structures (loops/conditionals) and function calls unfold in the tour sample. The standard usage for config templates, report headers, and notification messages: **compile at startup, render at high frequency** — the parsing cost is paid once. This chapter shares its two-layer lifecycle mental model with Chapter 30's regex; learn one and the other comes at zero cost.

## Introduction

A notification system's need (nearly every online system has this module): "Dear {username}, your order {order-id} shipped at {time}." — three fields, three types (string, number, time), plus channel variants (a short SMS version, a long mail version). Hand-assembling with `xrtFormat` means fetching each field, formatting each, concatenating each — noodles as soon as fields multiply; Chapter 25's builder is better, but the "template" still lives scattered in code — changing one wording means changing code, recompiling, redeploying.

The template engine pulls the "shape" out of the code: the template is data (a file, a string in config) — change the wording, change the template: no release, no downtime; rendering is a pure function — same template plus same data always yields the same output, testable and cacheable. Compile/render separation adds the performance link (its essential difference from "string replacement"): template parsing (building the node tree, validating paths) happens once at compile time, and rendering does only "fetch + format + write" — the cost of the hot path (rendering a notification per request) is essentially independent of template complexity.

## Concepts

### Compile/render separation (memorize the model before the functions)

```diagram flow
- Compile: xrtTemplateCompile(template text) -> immutable node tree (reference counted, Retain/Release)
- Data: xvalue value tree (Chapter 31) - built with ObjectSet/SetNew
- Render: xrtTemplateRender(compiled object, value tree, ...) -> owning output
- Reuse: the same compiled object renders with different value trees - template once, data ever-changing
```

The compiled object is immutable and shareable — multiple threads rendering the same template need only their own value trees (rendering only reads the template). This is fully isomorphic with Chapter 30's regex two-layer model: the heavy operation (parsing) once, the light operation (rendering) at high frequency.

### Three interpolation prefixes (a one-character semantic division each)

| Prefix | Syntax | Semantics |
| --- | --- | --- |
| Direct output | `{$user.name}` | the string as-is; the path navigates the value tree by dots |
| Number formatting | `{%count:,d}` | after the colon, Chapter 26's format string (thousands, base, percentage) |
| Time formatting | `{&updated:%F}` | time values rendered by format string (ISO date, etc.) |

The path syntax `a.b.c` navigates the value tree (dot access into nested objects, consistent with most template languages) — at render time the value is fetched by path; a miss (path absent or type mismatch) is handled per the render configuration (error or empty string, chosen explicitly). **Typed formatting** is the watershed between templates and "string replacement" (data keeps its type, display keeps its flexibility — the same number field takes the compact format in the SMS template and thousands in the report template; the template decides): number and time formatting are declared in the template, not pre-formatted into strings in code before being stuffed in — data keeps its type, display keeps its flexibility.

### Control structures and functions (letting shape adapt to data)

Templates are more than interpolation: loops (walking a value-tree array, one loop variable per item) and conditionals (branching on a value's truth) make "data shape decides output structure" possible — reports loop by row, notifications branch on attachment presence. Function calls (the tour sample's `call` form) register reusable formatting logic as template-invocable units — display logic like case conversion and plural forms stays on the template side. The boundary discipline for control and functions: **templates make display decisions, not business computations** — the moment a complex expression appears in a template, move it back to the code layer's data preparation.

### Error location (the template's compile-time defense line)

Compile errors carry line numbers (the tour sample verifies `error-location line=1`) — a syntax error inside the template surfaces at compile time, not as mangled rendered text. Render errors (path misses) are likewise reported structurally — Chapter 4's error system applies as usual.

## Examples

### Complete program: compile, value tree, and the three interpolations

From the repository sample `examples/template/core/main.c`:

```embed path="examples/template/core/main.c" title="examples/template/core/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/template/core/main.c -lws2_32 -liphlpapi
xrt handled 42 requests on 2026-04-02
```

**What just happened.** (1) The data side is built with Chapter 31's value tree: `ObjectNew` + `SetNew` (String/Int/Time fields of each type) — the template eats value trees, so the products of JSON/XSON parsing (Chapters 32/33) plug in directly, zero conversion. (2) `{$name}` outputs the string directly; `{%count:,d}` runs the number through Chapter 26's thousands formatting (42 without separators is still legal); `{&day:%F}` renders the time as an ISO date — the three prefixes each take one turn. (3) `Render` returns owning text, freed with `xrtFree`; the compiled object is returned with `Release`. Swap in another value tree and the same code produces another notification — the separation of template and data is at its most vivid here.

### Complete program: control structures and loops

From `examples/template/control/main.c`, list loops and conditional branches:

```embed path="examples/template/control/main.c" title="examples/template/control/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/template/control/main.c -lws2_32 -liphlpapi
Users: Alice,Bob; range: 13
```

**What just happened.** (1) The loop structure walks the value tree's user array — each element enters the loop variable and the template segment renders repeatedly; the comma-separated output shape is decided by the template fragment inside the loop body. (2) Conditionals and ranges: `range: 13` shows the output of conditional/aggregate structures — control structures let the template adapt to the data's shape, not the data bend to a fixed template. (3) The tour sample is left as further reading: it covers loading templates from files, streaming render output, function calls and registries — the four capability groups are the template's full form in real projects, with the four ok lines corresponding one-to-one.

## Contracts

- **Two-layer model**: the compiled object is immutable and shared by reference counting; rendering only reads the template and runs at high frequency concurrently (each thread with its own value tree).
- **Three prefixes**: `{$}` direct output / `{%:fmt}` Chapter 26 number format string / `{&:fmt}` time format string.
- **Data is a value tree**: render input is xvalue — JSON/XSON products plug in with zero conversion.
- **Error location**: compile errors carry line numbers; render errors (path/type) are reported structurally.
- **Boundary discipline**: templates make display decisions, not business computations — complex expressions move back to the data-preparation layer.
- **Owning products**: render output is `str`, freed with `xrtFree`; streaming rendering can wire straight to a sink.

### From examples to engineering: three hosts of templates

**Notification/message generation** (most common): compile global templates at startup, build a value tree when events arrive, render and send — Pitfall 1's good form is exactly its skeleton; multilingual scenarios compile one per language and select by the user's language. **Reports and page skeletons**: data rows render in loops, headers branch on conditions — the main battleground of control structures; large reports use streaming renders wired to sinks (the tour sample's streamed form), a million rows never assembling a whole page in memory. **Config-driven UI copy**: copy templates stored as config (change copy without a release), fields fetched from the config value tree — both template and data become hot-updatable assets. The shared evolution path of the three hosts: template from code constant → config file → hot-updatable asset, each step leaning on the two-layer design of "compiled object replaceable, rendering stateless".

### The division-of-labor map with JSON/XSON/regex

The second half of Volume 4 has four modules (plus the value tree) frequently sharing the stage; one map prevents confusion. **JSON/XSON** (Chapters 32/33): structured data in and out — parse into value trees; serialization of value trees back out. **Value tree** (Chapter 31): the intermediate representation — the common currency of all modules. **Regex** (Chapter 30): **extracts** structure from unstructured text (log fields, config lines) → produces value trees. **Templates** (this chapter): **generates** text from value trees (notifications, reports) → produces strings. Extraction and generation run in opposite directions, mirrors of each other — Chapter 36's combination chapter chains "extract → value tree → template-generate" into a complete pipeline. On the map, every module does exactly one thing; composition goes through the value tree, the intermediate currency.

### A design view: templates are interfaces too

Between template and rendering code sits an implicit interface: **the set of paths and types the template references**. When the interface changes (a field renamed, a type moving from int to string) and the template doesn't follow, rendering errors at runtime. Three habits make the implicit explicit: a rendering smoke test (minimal value tree + all-prefix render with asserted output) enters CI; template files live in the same repository and review as code; the field-change checklist gains a column, "affected templates". Chapter 36's combination chapter will extend this "interface" idea into a data contract for the complete pipeline.

## Pitfalls

### Pitfall 1: recompiling on every render (isomorphic with regex Pitfall 1)

Symptom: abnormal CPU on the high-frequency notification/report path; the hotspot sits in `xrtTemplateCompile` — fully isomorphic with Chapter 30's regex Pitfall 1.

Cause: compiling (parsing into a node tree) is the heavy operation; rendering is the light one. The heavy operation got into the loop.

```c bad
str render(const xvalue* pData, xstrview Tpl)
{
	xtemplate* T = xrtTemplateCompile(Tpl);   /* compiled on every render */
	str s = xrtTemplateRender(T, pData, NULL, NULL);
	xrtTemplateRelease(T);
	return s;
}
```

```c good
/* compile once at startup; template changes go through "recompile + atomic swap" */
static xtemplate* gTpl;
str render(const xvalue* pData)
{
	return xrtTemplateRender(gTpl, pData, NULL, NULL);   /* only the light path */
}
```

### Pitfall 2: stuffing business computation into templates (the display layer overreaching)

Symptom: templates grow longer and increasingly resemble another programming language; changing business rules means changing templates; template tests can't cover the logic branches.

Cause: the template engine offers expressiveness and the team moved data preparation into it — the boundary between "display decisions" and "business computation" fell.

```c bad
<!-- 模板里做业务计算：满减规则、运费、税费全在模板表达式里 -->
Total: {%cart.subtotal - discount(cart.promo) + shipping(cart.weight) + tax(...):,.2f}
```

```c good
<!-- 模板只展示；计算在代码层完成、结果进值树 -->
Total: {%order.total:,.2f}
（order.total 由代码层按业务规则算好再构造值树）
```

## Exercises

### Basic: render one with each prefix

Build a value tree with a string, a number, and a time field; write a template rendering each with its prefix; swap in another data set and re-render to verify template reuse.

### Advanced: a looping notification (control structures in practice)

Load a user array into the value tree (name + each one's order count); loop-render the summary line "everyone {name}({count})"; output "no orders" for an empty array — conditionals and loops combined.

### Challenge: a config-driven report (template and config, both hot)

A JSON config (Chapter 32) defines report metadata (title, columns, data paths); the template dynamically renders the header and data rows per the config — column count and names all come from config, not hard-coding. Acceptance criteria: the same template plus two different configs renders two different tables; a missing column reports an error with the path; compilation happens exactly once (verified across multiple renders).

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Two-layer model | compile immutable and shareable (Retain/Release) / render read-only, high-frequency concurrent |
| Three prefixes | `{$path}` direct output / `{%path:fmt}` number format string / `{&path:fmt}` time format string |
| Data | value trees plug in — JSON/XSON products zero-conversion; the value tree is every module's common currency |
| Control | loops walk arrays / conditionals branch / function calls (display logic to the template, business computation to code) |
| Errors | compile errors carry line numbers; render errors structured (path/type) |
| Performance discipline | compile once, share globally; hot paths only render; the same two-layer model as regex |
| Boundary | display decisions to the template, business computation to the code layer |
| Three hosts | notification generation / report pages (streaming renders) / config-driven copy — every evolution leans on the two-layer design |
| Division map | JSON/XSON move structure in and out, the value tree is currency, regex extracts, templates generate |
| Interface view | the paths and types a template references are an implicit interface — render smoke tests in CI prevent drift |
