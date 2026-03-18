# Template Engine Library

> Lightweight template rendering built around the current `xvalue` mainline

[English](api-template.en.md) | [中文](api-template.md) | [Back to Index](README.en.md)

---

## Table of Contents

- [Position in the Current Mainline](#position-in-the-current-mainline)
- [What This Module Is For](#what-this-module-is-for)
- [Core Design Idea](#core-design-idea)
- [Template Syntax Overview](#template-syntax-overview)
- [Recommended Data Model](#recommended-data-model)
- [Basic Rendering Flow](#basic-rendering-flow)
- [Conditional and Loop Control](#conditional-and-loop-control)
- [Sub-templates and Includes](#sub-templates-and-includes)
- [Rendering HTML, Reports, and AI Output](#rendering-html-reports-and-ai-output)
- [Best Practices](#best-practices)
- [Recommended Reading Path](#recommended-reading-path)

---

## Position in the Current Mainline

In current XRT, the template engine sits on top of the same structured-data mainline used by the rest of the runtime:

- `xvalue` is the primary in-memory data model
- JSON is the exchange format
- template rendering consumes `xvalue`
- HTTP handlers, HTTP clients, and AI workflows usually assemble `xvalue` first, then render or serialize from it

---

## What This Module Is For

The template engine is best used for:

- HTML page rendering
- HTML fragments
- email templates
- text reports
- config or manifest generation
- AI-oriented prompt or output assembly

It is especially valuable when you already use:

- `xvalue`
- `json`
- `xhttpd`
- `future / task / promise`

---

## Core Design Idea

The template system is designed around three principles:

### 1. Keep the data model unified

Use `xvalue` as the main input model.

### 2. Keep parsing and rendering separate

Templates are parsed into reusable template objects, then rendered multiple times.

### 3. Keep the runtime lightweight

The goal is not to become a giant framework template language. The goal is to provide a small, fast, reusable rendering layer inside a broader runtime.

---

## Template Syntax Overview

Current XRT template syntax covers:

- variable substitution
- numeric substitution
- time formatting
- conditionals
- loops
- sub-templates
- include blocks
- comments

### Variable substitution

```text
{$ name : Unknown}
{% age : 0}
{& created_at : YYYY-MM-DD}
```

### Conditionals

```text
{#if:score >= 90}
	A
{#elseif:score >= 60}
	Pass
{#else}
	Fail
{#end}
```

### Loops

```text
{#foreach:users}
	{$ name}
{#end}
```

```text
{#for:1:5:1}
	{% __index__}
{#end}
```

### Loop control

```text
{#break}
{#continue}
```

### Sub-template

```text
{#define:userCard}
	Name: {$ name}
{#end}

{= userCard}
```

---

## Recommended Data Model

The recommended mainline is:

- assemble a `table`
- nest arrays, lists, sub-tables, and plain values into it
- render from that structure

### Example: prepare data with `xvalue`

```c
xvalue pPage = xvoCreateTable();
xvalue pUsers = xvoCreateArray();
xvalue pUser = xvoCreateTable();

xvoTableSetText(pPage, "title", 0, "User List", 0, FALSE);
xvoTableSetText(pUser, "name", 0, "Alice", 0, FALSE);
xvoTableSetText(pUser, "email", 0, "alice@example.com", 0, FALSE);
xvoArrayAppendValue(pUsers, pUser, TRUE);
xvoTableSetValue(pPage, "users", 0, pUsers, TRUE);
```

### Why this is the preferred path

Because the same `xvalue` object can be:

- parsed from JSON
- modified in business logic
- shared with HTTP handlers
- serialized back to JSON
- rendered through template

---

## Basic Rendering Flow

### 1. Parse the template once

```c
XTE_LiteObject pTpl = xteParse(sTemplateText, 0, NULL);
```

### 2. Prepare `xvalue` input

```c
xvalue pData = xvoCreateTable();
xvoTableSetText(pData, "title", 0, "Hello XRT", 0, FALSE);
```

### 3. Render

```c
size_t iOutSize = 0;
str sOut = xteMake(pTpl, pData, NULL, NULL, &iOutSize);
```

### 4. Release

```c
xrtFree(sOut);
xvoUnref(pData);
xteParseFree(pTpl);
```

---

## Conditional and Loop Control

The template engine supports expressions, loops, and loop control, which makes it suitable for normal service-side rendering rather than only trivial text substitution.

### Conditional logic

```text
{#if:user.active and user.level >= 3}
	Active premium user
{#else}
	Normal user
{#end}
```

### Array iteration

```text
{#foreach:users}
	<li>{$ name} - {$ email}</li>
{#end}
```

### Table iteration

```text
{#foreach:config}
	{$__key__} = {$__value__}
{#end}
```

---

## Sub-templates and Includes

Sub-templates are useful when you want to keep repeated render fragments local and reusable.

### Sub-template

```text
{#define:userCard}
	<div class="user">
		<span>{$ name}</span>
	</div>
{#end}

{#foreach:users}
	{= userCard}
{#end}
```

### Include

Use includes when different compiled template objects should be composed together.

This is particularly useful for:

- page layout split into header/body/footer
- shared render fragments
- report sections

---

## Rendering HTML, Reports, and AI Output

### HTML

Recommended stack:

- `xhttpd`
- `xvalue`
- `template`
- `json` only at boundaries

### Reports and generated text

Recommended stack:

- `xvalue`
- `template`
- optional `json` input

### AI-oriented prompt or output assembly

Recommended stack:

- `xhttp`
- `xtlssession`
- `future / task / promise`
- `xvalue`
- `template`

---

## Best Practices

### 1. Parse once, render many times

Do not parse the same template text repeatedly in hot paths.

### 2. Keep rendering input in `xvalue`

Do not maintain separate ad-hoc maps just for templates.

### 3. Keep business logic outside the template

Templates may contain conditionals and loops, but core business logic should remain in C and in runtime data structures.

### 4. Use JSON only at boundaries

If input comes as JSON:

- parse into `xvalue`
- process in `xvalue`
- render from `xvalue`

---

## Recommended Reading Path

For the current mainline, read in this order:

1. [Value Dynamic Type System](api-value.en.md)
2. [JSON Processing](api-json.en.md)
3. This page
4. [XHTTPD](api-xhttpd.en.md)
5. [Future / Task / Promise](api-future-task-promise.en.md)

If you want more teaching-oriented material, continue with:

- [Guide Index](../guide/README.en.md)
- [Case Index](../case/README.en.md)

---

## Related Documents

- [Value Dynamic Type System](api-value.en.md)
- [JSON Processing](api-json.en.md)
- [XHTTPD](api-xhttpd.en.md)
- [Future / Task / Promise](api-future-task-promise.en.md)
- [Guide Index](../guide/README.en.md)
- [Case Index](../case/README.en.md)
- [API Index](README.en.md)
