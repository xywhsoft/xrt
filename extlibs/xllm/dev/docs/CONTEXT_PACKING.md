# Context Packing

`xllm` provides a context block model and memory search-result packing. The host
or `xwork` remains responsible for the global prompt budget across all sources.

## Current Scope

- `xllm_memory_apply_search_to_request` and `xllm_memory_apply_search_to_turn`
  convert memory search results into `xllm_context_block`.
- `xllm_memory_context_options` controls max hits, per-hit character budget,
  total character budget, min-score filtering, distinct-by-record filtering,
  context block kind, priority, pinned flag, and an optional label.
- Packed memory blocks include score, record id, title, source URI, chunk id,
  chunk index, byte range, and content.
- Retrieved context is untrusted reference material. Product prompts should tell
  the model not to treat retrieved text as instructions.

## Multi-Source Priority Model

Recommended AI IDE / claw ordering:

- System and developer policy: owned by the host, outside memory packing, highest
  authority, always pinned by the final renderer.
- Session summary: `XLLM_CONTEXT_SESSION_SUMMARY`, high priority, usually pinned
  or near-pinned because it preserves short-term continuity.
- Active file or current selection: host/xwork supplied block, usually
  `XLLM_CONTEXT_USER` until a more specific host renderer exists, high priority
  because it reflects the immediate user task.
- Retrieved memory: `XLLM_CONTEXT_MEMORY` or `XLLM_CONTEXT_KNOWLEDGE`, medium
  priority, untrusted, trimmed first when active task context needs space.
- Tool result: `XLLM_CONTEXT_TOOL_RESULT`, priority depends on recency and
  whether the current tool call requires continuation. It is also untrusted.
- History: managed by session policy and compaction, lower priority than active
  file, current tool result, and explicit session summary.

`xllm` should not silently arbitrate between product-level sources. It exposes
block kind, priority, and pinned flags so the host can implement an explicit
global packer with predictable product behavior.

## Eval

Run the deterministic context packer eval:

```powershell
cmd /c .\build.bat context-packer-eval
```

The eval uses synthetic search hits and writes JSON/TXT reports covering:

- full retention;
- output reduction under total character budget;
- duplicate removal with `bDistinctByRecord`;
- low-score removal with `tMinScore`.
