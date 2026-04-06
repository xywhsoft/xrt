# XTE Form Block Demo Notes

## Goal

Validate two things:

1. Current xte custom block syntax can be registered for `{{#form}} ... {{#end}}`, as long as parse option `sBracket` is configured as `{{}}`.
2. Form JSON can be compiled once during template parse, instead of generating HTML on every render.

## Demo files

- `D:\Git\xrt\dev\template\xte_form_block_demo.c`
- `D:\Git\xrt\dev\template\build_gcc_xte_form_block_demo.bat`

## What the demo proves

The demo registers two custom statements with `sBracket = "{{}}"`:

- `form`
  - Parses the raw JSON body once.
  - Generates final HTML once during `procParse`.
  - Stores the generated HTML in statement `pData`.
  - Render phase only writes cached HTML.

- `form_runtime`
  - Stores raw JSON during `procParse`.
  - Re-parses JSON and regenerates HTML on every render.

After parsing one template and rendering it 3 times, the counters should show:

- `form`
  - parse count = 1
  - generate count = 1
  - render count = 3

- `form_runtime`
  - parse count = 1
  - generate count = 3
  - render count = 3

This means the current `xte` extension model already supports "compile once, render many" for custom statements.

## What current xte does NOT support directly

Current public statement parse API gives:

- raw body
- args
- body span
- error output
- custom `pData`

But it does **not** give a public way to:

- replace the current statement node with a `TEXT` node
- inject generated HTML into the template string pool and rewrite the AST node type

So the current public API supports:

- compile-time pre-generation into `pData`

It does **not** support:

- true statement-to-text-node rewrite

## Suggested xte enhancement

Add a public helper for parse-time node replacement, for example:

```c
XXAPI int xteStmtParseCompileText(XTE_StmtParseCtx* pCtx, const char* sText, size_t iSize);
```

Expected behavior:

1. Copy `sText` into template string pool.
2. Rewrite current node:
   - `iType = XTE_NODE_TEXT`
   - set `Data.Text.iTextOff`
   - set `Data.Text.iTextSize`
3. Clear statement-only fields:
   - args
   - body
   - raw body
   - `pData`
4. Free any previous statement parse data safely.

That would let custom statements truly precompile into normal text nodes.

## Recommendation

For `xadmin` integration, the current public API is already enough for a V1 precompile model:

- use `{{#form}} ...JSON... {{#end}}`
- parse JSON once in `procParse`
- generate final form HTML once
- store HTML in `pData`
- render by direct write

This avoids per-render form generation cost even before `xte` gains real text-node rewrite support.
