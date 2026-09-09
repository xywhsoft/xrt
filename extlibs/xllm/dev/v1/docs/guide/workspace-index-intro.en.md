# Workspace Index Introduction

Workspace indexing writes source code, Markdown, text files, and similar files from a directory into `xllm_memory`, so the model can retrieve project knowledge by question.

[Back to Tutorials](README.en.md) | [Memory RAG Introduction](memory-rag-intro.en.md) | [Workspace API](../api/api-memory-workspace.en.md)

## What You Will Learn

This guide helps you understand:

- The difference between `ingest_workspace` and `sync_workspace`.
- How to configure extensions, ignored directories, `.gitignore`, and maximum file size.
- How to represent file sources with stable record IDs and source URIs.
- How to query index status and health checks.
- When to use a watcher for incremental updates.

## When to Index a Workspace

When your application needs to answer questions such as "how do I use this module in the project", "where is this interface defined", or "does the documentation define a convention", you should write the workspace into memory first, then perform RAG retrieval.

Workspace indexing is suitable for:

- C/C++ headers and example code.
- Markdown documentation.
- Configuration files and small text files.
- Business knowledge files after filtering.

Workspace indexing is not suitable for:

- Large binary files.
- Build outputs, caches, and dependency directories.
- Keys, certificates, and database dumps.
- Huge logs that require real-time character-by-character reading.

## Difference Between Ingest and Sync

| API | Usage |
| --- | --- |
| `xllm_memory_ingest_workspace` | Scan a directory and write matching files into memory. Suitable for first indexing or simple rebuilds. |
| `xllm_memory_sync_workspace` | Scan a directory, compare it with existing records, then add, update, and remove indexed records. Suitable for long-term maintenance. |
| `xllm_memory_sync_file` | Update the record for one changed file. |
| `xllm_memory_sync_file_events` | Batch sync when you already collected a set of file events. |

When learning, start with `xllm_memory_ingest_workspace`. When you build an IDE, background service, or continuous indexer, move to `sync_workspace` or a watcher.

## Minimal Workspace Index

The following is a minimal flow. For a complete smoke example, see `examples/smoke_memory_ingest_workspace.c`.

```c
xllm_memory_ingest_workspace_options tWorkspace;
xllm_memory_ingest_directory_result tResult;
xllm_error tError;

xllm_error_init(&tError);
xllm_memory_ingest_workspace_options_init(&tWorkspace);
xllm_memory_ingest_directory_result_init(&tResult);

tWorkspace.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
tWorkspace.sPath = "D:\\git\\xllm";
tWorkspace.sRecordIdPrefix = "workspace";
tWorkspace.sSourceUriPrefix = "workspace://";
tWorkspace.sAllowedExtensions = ".c;.h;.md;.txt";
tWorkspace.uMaxFileBytes = 256u * 1024u;

if ( xllm_memory_ingest_workspace(
        pMemory,
        &tWorkspace,
        &tResult,
        &tError
    ) != XRT_NET_OK ) {
    fprintf(stderr, "workspace ingest failed: %s\n",
            tError.sMessage ? tError.sMessage : "(null)");
}

printf("visited=%u ingested=%u skipped=%u failed=%u\n",
       tResult.uVisitedFileCount,
       tResult.uIngestedFileCount,
       tResult.uSkippedFileCount,
       tResult.uFailedFileCount);

xllm_memory_ingest_directory_result_reset(&tResult);
```

Field meanings:

| Field | Recommendation |
| --- | --- |
| `eScope` | Workspace knowledge usually uses `XLLM_MEMORY_SCOPE_KNOWLEDGE` |
| `sPath` | Root directory to index |
| `sRecordIdPrefix` | Prefix used to generate record IDs; keep it stable |
| `sSourceUriPrefix` | Prefix used to generate source URIs, such as `workspace://` |
| `sAllowedExtensions` | Semicolon-separated extension list |
| `uMaxFileBytes` | Maximum bytes per file, preventing large files from entering the index |

## Ignore Rules

Workspace indexing should filter first, then ingest. Common filtering fields:

```c
tWorkspace.bRecursive = true;
tWorkspace.bSkipHidden = true;
tWorkspace.bLoadGitIgnore = true;
tWorkspace.sIgnoredDirectories = ".git;build;out;node_modules;.venv";
tWorkspace.sIgnoredExtensions = ".exe;.dll;.obj;.pdb;.zip;.png;.jpg";
tWorkspace.sIgnoredPathPatterns = "*secret*;*.key;*.pem";
tWorkspace.sIgnoreFiles = ".gitignore;.xllmignore";
```

Recommended rules:

- Scan recursively by default, but exclude build directories and dependency directories.
- Enable `.gitignore` so indexing rules stay aligned with existing project rules.
- Use `sIgnoredPathPatterns` to skip keys, certificates, and sensitive configuration.
- Set a clear `uMaxFileBytes` limit so logs and generated files do not fill memory.

## Record ID and Source URI

Workspace files change over time. You need a stable way to identify "the new version of the same file". Therefore:

- `sRecordIdPrefix` is used to generate stable record IDs.
- `sSourceUriPrefix` is used to generate readable, traceable source URIs.

For example, with `sRecordIdPrefix = "workspace"` and `sSourceUriPrefix = "workspace://"`, a record may contain:

```text
record id:  workspace:src/main.c
source uri: workspace://src/main.c
```

Do not use random record IDs on each indexing run. Otherwise, older versions of the same file remain in memory and may be retrieved later.

## Syncing an Existing Workspace

Use `xllm_memory_sync_workspace` when you want to remove records for files that no longer exist and update changed files:

```c
xllm_memory_sync_workspace_result tSync;

xllm_memory_sync_workspace_result_init(&tSync);

if ( xllm_memory_sync_workspace(
        pMemory,
        &tWorkspace,
        &tSync,
        &tError
    ) == XRT_NET_OK ) {
    printf("examined=%u removed=%u\n",
           tSync.uExaminedRecordCount,
           tSync.uRemovedRecordCount);
}

xllm_memory_sync_workspace_result_reset(&tSync);
```

`sync_workspace` is better for long-running applications. It aligns the current filesystem state with the existing index state.

## Status and Health Checks

After indexing, use status and health checks to confirm memory is usable:

```c
xllm_memory_workspace_status_options tStatusOptions;
xllm_memory_workspace_status tStatus;
xllm_memory_health_check_options tHealthOptions;
xllm_memory_health_check tHealth;

xllm_memory_workspace_status_options_init(&tStatusOptions);
xllm_memory_workspace_status_init(&tStatus);
xllm_memory_health_check_options_init(&tHealthOptions);
xllm_memory_health_check_init(&tHealth);

tStatusOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
tStatusOptions.sSourceUriPrefix = "workspace://";

xllm_memory_get_workspace_status(pMemory, &tStatusOptions, &tStatus, &tError);
xllm_memory_check_health(pMemory, &tHealthOptions, &tHealth, &tError);
```

Status answers "how many workspace records are in the index". Health checks answer "whether the memory backend is working".

## File Events and Watcher

If your application can receive file change events, you can put events into a queue first, then sync them in batches:

```c
xllm_memory_file_event_queue *pQueue = NULL;
xllm_memory_change_set tChanges;
xllm_memory_file_event_queue_drain_options tDrain;

xllm_memory_file_event_queue_create(&pQueue);
xllm_memory_file_event_queue_push_updated(pQueue, "D:\\git\\xllm\\xllm.h", &tError);

xllm_memory_change_set_init(&tChanges);
xllm_memory_file_event_queue_drain_options_init(&tDrain);
tDrain.tBaseOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
tDrain.tBaseOptions.sRootPath = "D:\\git\\xllm";
tDrain.tBaseOptions.sSourceUriPrefix = "workspace://";

xllm_memory_file_event_queue_drain(
    pMemory,
    pQueue,
    &tDrain,
    &tChanges,
    &tError
);
```

When event sources are numerous and changes are frequent, use the watcher bridge, pump, or worker. They merge events, debounce, and flush in batches.

## Common Mistakes

Do not ingest the entire repository without filters. `build/`, `.git/`, dependency directories, and binary files create a large amount of noise.

Do not disable stable record IDs. The most important behavior in workspace indexing is "when a file changes, update the same record", not "keep appending new records".

Do not ignore skipped and failed counts. A high `uSkippedFileCount` may simply mean filters are working; a high `uFailedFileCount` usually indicates permission, encoding, file-size, or path problems.

Do not treat workspace indexing as a code parser. Memory RAG can provide relevant fragments, but symbol navigation, AST analysis, and precise references should still be handled by dedicated language services.

## Next Steps

- To use indexed results for Q&A, read [Memory RAG Introduction](memory-rag-intro.en.md).
- To build continuous sync, read [Memory Watcher API](../api/api-memory-watcher.en.md).
- To see a complete workspace RAG scenario, later read [Workspace RAG Case](../case/workspace-rag.en.md).
