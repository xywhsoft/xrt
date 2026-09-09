# Memory Internal Module Boundary

This document defines the target internal boundaries for Memory v3 refactoring. It is a migration contract, not a public API. Public entrypoints remain `xllm-memory.h` / `xllm-memory.c`.

Primary goal: split the current `src/xllm_memory/xllm_memory.c` monolith into mechanism layers without changing public behavior, singlehead generation, smoke matrix cases, or release bundle layout.

## Dependency Direction

Allowed direction:

```text
public API facade
  -> pipeline
      -> store
      -> lindex
      -> vindex
      -> chunk
      -> embed
```

Rules:

- Lower layers must not call public `xllm_memory_*` APIs except reset/init helpers that are explicitly documented as data helpers.
- `store`, `lindex`, `vindex`, `chunk`, and `embed` must not depend on `pipeline`.
- `chunk` must not depend on `store`, `lindex`, `vindex`, or provider/runtime adapters.
- `embed` must not depend on `store`, `lindex`, or `pipeline`; it may depend on xrt, xvalue, and optional ONNX/runtime compatibility code.
- `store` may persist metadata produced by `chunk`, `embed`, `lindex`, and `vindex`, but must not compute ranking or extraction policy.
- `lindex` and `vindex` may consume records/chunks from store-facing structs, but must not own SQLite schema migrations outside their own index tables.
- All layers must route user-visible failures through `xllm_error` with stable code/message semantics.

## Target Modules

| Layer | Target path | Owns | Must not own |
| --- | --- | --- | --- |
| facade | `xllm-memory.c`, `xllm-memory.h` | Public include surface, singlehead include compatibility, stable exported symbols | Internal ranking/storage logic |
| pipeline | `src/xllm_memory/xllm_memory.c` or future `xllm_memory_pipeline.c` | Ingest/search/apply orchestration, option defaults, public API validation, conversation extraction policies, workspace sync flow | SQLite schema DDL, BM25 math, vector similarity internals, ONNX details |
| store | `src/xllm_memory/xllm_memory_store.c` plus existing `xllm_memory_sqlite.c` | SQLite open/close, schema/profile metadata, migrations, record/chunk persistence, deletion, listing filters, diagnostics storage fields | Ranking, query tokenization, context rendering, extraction policy |
| lindex | `src/xllm_memory/xllm_memory_lindex.c` | Lexical tokenization, postings maintenance, BM25-like scoring, lexical rank, metadata/source/title/path boosts | SQLite namespace/profile ownership, vector candidate generation, context apply |
| vindex | `src/xllm_memory/xllm_memory_vindex.c` | Vector candidate generation, sqlite-vec integration, flat cosine fallback, vector rank/score diagnostics | Embedding model implementation, lexical score, record lifecycle |
| chunk | `src/xllm_chunk/` | Text splitting, chunk IDs, byte ranges, content hash, prev/next adjacency | Store persistence, retrieval ranking |
| embed | `src/xllm_embed/` | Builtin hash/E5 embedder creation, embed profile identity, ONNX Runtime compatibility, embedding reset/clone helpers | Retrieval fusion, SQLite schema, memory extraction |
| bridge | `src/xllm_memory/xllm_memory_bridge.c` | Opt-in session/request bridge helpers, search-before-chat, explicit post-chat ingest helpers | Session core ownership, provider adapter behavior |

## Current Baseline

Current state after the first extraction passes:

- `src/xllm_memory/xllm_memory.c` is now the internal implementation root: it owns shared private helpers, cross-module validation helpers, and include ordering, but no longer directly hosts public `XLLM_API` memory entrypoints.
- `src/xllm_memory/xllm_memory_sqlite.c` owns memory lifecycle, SQLite open/load/persist, ingest_text, list/remove/search, diagnostics, and store state getters.
- `src/xllm_memory/xllm_memory_lindex.c` owns sparse tokenization, postings maintenance, BM25-like scoring, lexical rank, and lexical diagnostics.
- `src/xllm_memory/xllm_memory_vindex.c` and `src/xllm_memory/xllm_memory_vindex_sqlite.c` own vector scoring/rank, flat fallback, sqlite-vec table/query/upsert/delete helpers, and vector diagnostics.
- `src/xllm_memory/xllm_memory_pipeline.c` and `src/xllm_memory/xllm_memory_pipeline_sqlite.c` own search/apply orchestration, result cloning/reset, hybrid fusion, debug dump, and context block rendering.
- `src/xllm_memory/xllm_memory_facade.c` owns public memory option init helpers, embedder facade entrypoints, and result/change-set init/reset glue that does not belong to store, lindex, vindex, chunk, or embed.
- `src/xllm_memory/xllm_memory_typed.c` owns typed memory ingest/compaction public orchestration for `turn_response`, `task`, `fact`, `preference`, and conversation summary compaction.
- `src/xllm_memory/xllm_memory_files.c` owns file, directory, workspace ingest plus sync file/files/workspace/file-events public orchestration.
- `src/xllm_memory/xllm_memory_watcher.c` owns file event queue lifecycle, coalescing, push helpers, drain orchestration, watcher bridge public flow, watcher pump public flow, worker timing/activity helpers, and watcher worker public flow.
- `src/xllm_chunk/` owns reusable generic text chunking mechanics.
- `src/xllm_embed/` owns embedder invocation/reset/cosine helpers plus builtin hash/E5 embedder mechanics.
- `src/xllm_memory/xllm_memory_bridge.c` is separated and remains opt-in.

The first hard-boundary split is complete: public memory entrypoints now live in focused internal modules while `xllm_memory.c` remains the private implementation root. Future cleanup can still move private helper clusters into narrower files, but it is no longer required to stabilize the public module boundary.

## Migration Order

Follow this order to reduce regression risk:

1. Store extraction: move SQLite schema/profile/load/save/delete/list helpers out of `xllm_memory.c`, keeping public behavior identical.
2. Lindex extraction: move lexical tokenization, postings, BM25-like scoring, and lexical rank helpers.
3. Vindex extraction: move sqlite-vec candidate search, flat cosine fallback, vector rank helpers, and vector diagnostics.
4. Pipeline cleanup: leave public validation, option defaults, ingest/search/apply orchestration, and conversation/workspace policy in the pipeline layer.
5. Focused tests: add compile or smoke coverage per extracted layer before removing compatibility shims.

## Store Boundary

Store owns durable state and DB compatibility:

- SQLite database open/close and extension loading state.
- Namespace-level schema/profile metadata.
- Record/chunk/vector metadata persistence.
- Record deletion by id/source/conversation/metadata/expiry.
- Listing filters and diagnostics counts.
- SQLite migration and profile mismatch errors.

Store must expose narrow internal functions that accept already-validated internal record/chunk structs. It should not call back into search/apply pipeline code.

## Lexical Index Boundary

Lindex owns lexical retrieval mechanics:

- Query/document tokenization for lexical search.
- Postings maintenance for SQLite-backed sparse retrieval.
- BM25-like score computation.
- Source/title/path/metadata boosts that are part of lexical ranking.
- Lexical rank diagnostics.

Lindex must be callable without vector search enabled. Builtin sparse should become pipeline + store + lindex only.

## Vector Index Boundary

Vindex owns vector retrieval mechanics:

- Query embedding handoff to embedder.
- SQLite vec0 candidate retrieval when available.
- In-memory cosine fallback when sqlite-vec is unavailable.
- Vector score/rank diagnostics.
- Vector index profile selection.

Vindex should not create model-specific embedders. It receives configured embedder handles and embedding values from the embed layer.

## Pipeline Boundary

Pipeline keeps business orchestration:

- Public option validation and defaults.
- `xllm_memory_ingest_text`, file/workspace ingest, sync, watcher flow.
- Conversation `turn_response` extraction policy and future summary/task policies.
- Search orchestration and hybrid fusion across lindex/vindex.
- Context apply and turn/request rendering.
- Public result allocation/reset semantics.

Pipeline may coordinate multiple layers but must not reimplement their mechanism internals after migration.

## Error Contract

Each extracted layer must preserve current user-visible errors:

- Invalid caller input returns `XLLM_ERROR_INVALID_REQUEST`.
- Unsupported local capability returns `XLLM_ERROR_UNSUPPORTED_CAPABILITY` or `XLLM_ERROR_UNSUPPORTED_INPUT_TYPE`.
- Allocation, SQLite, and invariant failures return `XLLM_ERROR_INTERNAL` unless already mapped more precisely.
- Error messages must remain actionable and stable enough for smoke coverage such as `memory_error_consistency`.

## Build And Verification Gates

After each extraction slice:

```powershell
cmd /c build.bat singlehead
cmd /c build.bat static-analysis -SkipCompile
cmd /c build.bat smoke -Filter "memory_diagnostics,memory_builtin_sparse,memory_hybrid_rrf,memory_long_run,memory_error_consistency"
cmd /c build.bat release-gate -SkipSQLiteVec -SkipZip
```

The static-analysis gate includes a memory internal boundary check: `src/xllm_memory/xllm_memory.c` must not directly define public `XLLM_API` memory entrypoints. Public entrypoints belong in focused `src/xllm_memory/xllm_memory_*` modules.

If a slice touches E5/vector behavior, also run:

```powershell
cmd /c build.bat smoke -Filter "sqlite_vec_probe,memory_builtin_e5,memory_hybrid_rrf"
```

## Non-Goals

- Do not change public `xllm_memory_*` signatures during internal extraction.
- Do not make `session` depend on `memory`.
- Do not move provider adapter behavior into memory.
- Do not introduce a mandatory external vector DB.
- Do not require sqlite-vec for builtin sparse or flat fallback operation.
- Future HNSW / external vector DB policy is documented in `docs\MEMORY_VECTOR_INDEX_EVOLUTION.md`.
