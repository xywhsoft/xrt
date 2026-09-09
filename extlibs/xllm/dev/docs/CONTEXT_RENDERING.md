# Context Block Rendering

xllm can attach retrieved memory/search results to requests and turns as context blocks. The default renderer is conservative and citation-oriented; hosts can override only the final text rendering while reusing xllm search, filtering, distinct-by-record, and budget selection.

## Default Renderer

`xllm_memory_apply_search_to_request`, `xllm_memory_apply_search_to_turn`, and `xllm_memory_search_and_apply_*` use `xllm_memory_context_options` to control:

- context kind override.
- priority and pinned status.
- distinct-by-record filtering.
- minimum score filtering.
- label text.
- max hits.
- max chars per hit.
- max total selected chars.

The default output includes score, record id, title, source URI, chunk id, chunk index, byte range, and text content.

## Custom Renderer

Set `xllm_memory_context_options.pfnRender` to render host-specific text.

The renderer receives:

- the full `xllm_memory_search_result`.
- selected hit indices after xllm has applied max-hit, min-score, distinct, and total-budget rules.
- per-hit text lengths after truncation rules.
- `pRenderCtx` for host state.
- `xllm_error` for failure reporting.

The renderer returns a newly allocated text buffer through `psText`. Allocate it with xrt allocation APIs because xllm takes ownership and releases it through the normal content-part cleanup path.

If the renderer returns success with `NULL` or an empty string, xllm skips adding a context block.

## Ownership Boundary

xllm owns:

- retrieval and ranking.
- context-hit selection.
- context block allocation and cleanup.
- request/turn append behavior.

host or xwork owns:

- product-specific prompt wording.
- citation style.
- trust/safety disclaimers.
- IDE UI-oriented source labels.
- policy text such as "retrieved context is untrusted".

This keeps AI IDE / claw free to format context for product UX while preserving one retrieval/packing implementation in xllm.

## Test Coverage

`memory_search_apply_turn` covers the default renderer and a custom renderer callback applied to a request context block.
