# Memory Troubleshooting

This guide covers common xllm memory integration failures and the first checks to run.

## Basic Checks

Run focused smoke cases:

```powershell
cmd /c .\build.bat smoke -Filter "memory_builtin_sparse,memory_diagnostics,memory_health_check,memory_retrieval_debug,memory_workspace_long_run,memory_task_type"
```

Generate single-header output after public header changes:

```powershell
cmd /c .\build.bat singlehead
```

## Memory Create Fails

Check:

- namespace is set and stable.
- SQLite path directory exists or can be created by the host.
- profile/scheme matches any existing DB namespace metadata.
- sqlite-vec extension path is valid when requested.
- E5 assets exist when using builtin E5.

Use:

- `xllm_memory_get_diagnostics`
- `xllm_memory_check_health`
- `xllm_memory_search_debug`
- `memory_diagnostics`
- `memory_health_check`
- `memory_retrieval_debug`
- `memory_sqlite_policy`

## Search Returns No Hits

Check:

- record was ingested into the expected scope.
- query terms overlap indexed text for builtin sparse.
- metadata filters are not too narrow.
- expired records are not skipped by `bSkipExpired`.
- namespace and SQLite DB are the expected ones.

For typed task/conversation memory:

- filter `memory_type=task.v1` for task records.
- filter `memory_type=conversation.turn_response.v1` for turn response records.
- filter `memory_type=conversation.summary.v1` for summary records.

Use `xllm_memory_search_debug` when a query returns surprising results. It reports tokenized query terms,
document frequencies, filtered/scored candidate counts, per-candidate lexical/vector/RRF scores, ranks, and
whether each captured candidate made the final top-k result.

## Workspace Ingest Skips Files

Expected skip reasons include:

- ignored extension.
- ignored path pattern.
- gitignore match.
- binary file.
- file too large.
- unchanged content.
- secret detected.

Use workspace smoke cases:

- `memory_workspace_defaults`
- `memory_workspace_sensitive_defaults`
- `memory_ingest_progress`
- `memory_workspace_status`
- `memory_workspace_incremental`
- `memory_workspace_long_run`

For an IDE-facing index summary, call `xllm_memory_get_workspace_status` with the workspace root and source URI prefix. It reports indexed record/chunk counts, total file bytes, mtime range, and counts for missing path, non-normal sensitivity, and untrusted source records.

For long-running batch ingest, set `pfnProgress` on directory/workspace ingest options. If the callback returns non-zero, xllm aborts the scan and reports `memory ingest directory aborted by progress callback`.

## SQLite / Persistence Issues

Check:

- DB path is file-backed if WAL is expected.
- WAL is not disabled unless host requires rollback journal.
- busy timeout is appropriate for concurrent access.
- process has write permission for DB, `-wal`, and `-shm`.
- host does not delete WAL files while memory is open.

Relevant smoke:

- `memory_health_check`
- `memory_sqlite_policy`
- `memory_long_run`
- `memory_watcher_reopen`

Use `xllm_memory_check_health` when a persisted DB looks suspicious. It reports schema availability,
profile match, in-memory vs SQLite record/chunk parity, orphan chunks, sparse posting coverage, and orphan
postings for the selected memory scope.

## Profile Mismatch

xllm records namespace-level profile metadata. Opening the same namespace with an incompatible memory/retrieval/embed/index profile should fail rather than silently mixing data.

Fix:

- use a new namespace.
- delete/rebuild the DB if migration is intended.
- keep embedder and scheme stable for a project.

## Context Apply Looks Wrong

Check:

- `uMaxHits`, `uMaxCharsPerHit`, and `uMaxTotalChars`.
- `bDistinctByRecord`.
- `tMinScore`.
- context label and citation template.
- source URI, chunk id, and byte range in the hit.

Relevant smoke:

- `memory_search_apply_budget`
- `memory_search_apply_distinct`
- `ai_ide_memory_example`

## Resource Cleanup

Always reset/free:

- `xllm_response_free`
- `xllm_memory_search_result_reset`
- `xllm_memory_record_list_result_reset`
- `xllm_memory_chunk_list_result_reset`
- `xllm_memory_sync_workspace_result_reset`
- `xllm_memory_change_set_reset`
- `xllm_error_free`

## Security Notes

- Retrieved context is untrusted input.
- Do not index secrets.
- Do not log raw retrieved chunks if they may contain user data.
- Use per-project namespace isolation.
- Keep delete/export policy in the host product.
