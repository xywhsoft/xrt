# xllm Memory Workspace API

> Header: `xllm-memory.h`

This page explains xllm memory workspace synchronization APIs. Unlike one-time `ingest_directory`, workspace APIs focus on directories that keep changing: when files are created, updated, deleted, or renamed, memory records should sync with them.

If you only need the first directory import, start with [Memory Ingest API](api-memory-ingest.en.md). If you need to connect file watchers or event queues, continue with [Memory Watcher API](api-memory-watcher.en.md).

## When to Use Workspace API

| Scenario | Recommended API |
| --- | --- |
| First import of a source repository | `xllm_memory_ingest_workspace` |
| Rescan workspace and delete memory records for files that no longer exist | `xllm_memory_sync_workspace` |
| Sync one file only | `xllm_memory_sync_file` |
| Sync multiple known files at once | `xllm_memory_sync_files` |
| Sync created/updated/deleted/renamed file events | `xllm_memory_sync_file_events` |
| Inspect current workspace record summary | `xllm_memory_get_workspace_status` |
| Check whether memory storage is healthy | `xllm_memory_check_health` |

## Types

### xllm_memory_sync_file_options

**Purpose:** configuration used when syncing one file.

```c
typedef struct {
    xllm_memory_scope eScope;
    const char *sPath;
    const char *sRootPath;
    const char *sRecordId;
    const char *sRecordIdPrefix;
    const char *sTitle;
    const char *sSourceUri;
    bool bReplaceExisting;
    bool bUseWorkspaceDefaults;
    bool bSkipHidden;
    bool bSkipUnchanged;
    const char *sAllowedExtensions;
    const char *sIgnoredDirectories;
    const char *sIgnoredExtensions;
    const char *sIgnoredPathPatterns;
    const char *sSourceUriPrefix;
    uint64 uMaxFileBytes;
    uint32 uChunkChars;
    uint32 uChunkOverlapChars;
    xvalue tMetadata;
    xvalue tVendorExtra;
} xllm_memory_sync_file_options;
```

| Field | Meaning |
| --- | --- |
| `eScope` | Target scope. Default is `KNOWLEDGE`. |
| `sPath` | File path, required. |
| `sRootPath` | Workspace root path, used for relative path and source URI. |
| `sRecordId` | Explicit record ID. |
| `sRecordIdPrefix` | Prefix used when generating record ID automatically. |
| `sTitle` | Record title. |
| `sSourceUri` | Explicit source URI. |
| `bReplaceExisting` | Whether to replace existing record. Default is `true`. |
| `bUseWorkspaceDefaults` | Whether to enable workspace default filters. |
| `bSkipHidden` | Whether to skip hidden files. Default is `true`. |
| `bSkipUnchanged` | Whether to skip unchanged files. Default is `true`. |
| `sAllowedExtensions` | Allowed extension set. |
| `sIgnoredDirectories` | Ignored directory set. |
| `sIgnoredExtensions` | Ignored extension set. |
| `sIgnoredPathPatterns` | Ignored path pattern set. |
| `sSourceUriPrefix` | Prefix used when generating source URI. |
| `uMaxFileBytes` | File size limit. |
| `uChunkChars` / `uChunkOverlapChars` | Chunk settings for this sync. |
| `tMetadata` | Metadata written to the record. |

### xllm_memory_sync_files_options

**Purpose:** syncs multiple files.

```c
typedef struct {
    const xllm_memory_sync_file_options *pItems;
    size_t iItemCount;
    bool bContinueOnError;
    xvalue tVendorExtra;
} xllm_memory_sync_files_options;
```

**Default:** `bContinueOnError = true`, meaning a single file failure does not stop later files.

### xllm_memory_file_event_kind

**Purpose:** describes one file change event.

```c
typedef enum {
    XLLM_MEMORY_FILE_EVENT_CREATED = 1,
    XLLM_MEMORY_FILE_EVENT_UPDATED,
    XLLM_MEMORY_FILE_EVENT_DELETED,
    XLLM_MEMORY_FILE_EVENT_RENAMED
} xllm_memory_file_event_kind;
```

### xllm_memory_file_event

**Purpose:** input item used when syncing file events.

```c
typedef struct {
    xllm_memory_file_event_kind eKind;
    const char *sPath;
    const char *sPreviousPath;
    xvalue tVendorExtra;
} xllm_memory_file_event;
```

| Field | Meaning |
| --- | --- |
| `eKind` | Event kind. |
| `sPath` | Current path. |
| `sPreviousPath` | Path before rename, commonly used only for rename. |
| `tVendorExtra` | Extension field. |

### xllm_memory_sync_file_events_options

**Purpose:** applies a batch of file events to memory.

```c
typedef struct {
    xllm_memory_sync_file_options tBaseOptions;
    const xllm_memory_file_event *pItems;
    size_t iItemCount;
    bool bContinueOnError;
    xvalue tVendorExtra;
} xllm_memory_sync_file_events_options;
```

`tBaseOptions` provides common configuration such as root path, filters, and record ID prefix. Each event only needs event kind and path.

### xllm_memory_change_kind / xllm_memory_change_info

```c
typedef enum {
    XLLM_MEMORY_CHANGE_CREATED = 1,
    XLLM_MEMORY_CHANGE_UPDATED,
    XLLM_MEMORY_CHANGE_REMOVED,
    XLLM_MEMORY_CHANGE_SKIPPED,
    XLLM_MEMORY_CHANGE_FAILED
} xllm_memory_change_kind;
```

```c
typedef struct {
    xllm_memory_change_kind eKind;
    xllm_memory_record_info tRecord;
    xllm_memory_skipped_file_info tSkipped;
    xllm_memory_failed_file_info tFailed;
} xllm_memory_change_info;
```

Read the field matching `eKind`: `CREATED/UPDATED/REMOVED` mainly use `tRecord`, `SKIPPED` uses `tSkipped`, and `FAILED` uses `tFailed`.

### xllm_memory_change_set

```c
typedef struct {
    xllm_memory_change_info *pChanges;
    size_t iChangeCount;
} xllm_memory_change_set;
```

**Cleanup rule:** call `xllm_memory_change_set_reset` after use.

### xllm_memory_sync_workspace_result

```c
typedef struct {
    xllm_memory_ingest_directory_result tIngest;
    uint32 uExaminedRecordCount;
    uint32 uRemovedRecordCount;
    xllm_memory_record_info *pRemovedRecords;
    size_t iRemovedDetailCount;
} xllm_memory_sync_workspace_result;
```

| Field | Meaning |
| --- | --- |
| `tIngest` | Statistics from rescan and ingest. |
| `uExaminedRecordCount` | Number of existing records examined. |
| `uRemovedRecordCount` | Number of records removed because files no longer exist. |
| `pRemovedRecords` | Details of removed records. |
| `iRemovedDetailCount` | Removed detail count. |

### Workspace Status and Health Types

`xllm_memory_workspace_status_options` filters workspace status by scope, root path, or source URI prefix. `xllm_memory_workspace_status` reports record/chunk counts, missing-path records, sensitive/untrusted records, total file bytes, and mtime range.

`xllm_memory_health_check_options` configures health checks. Defaults are scope `ANY`, SQLite check enabled, and sparse postings check enabled. `xllm_memory_health_check` reports whether storage is OK, whether SQLite/schema/profile/postings are OK, and count consistency.

## API: Workspace Ingest and Sync

### xllm_memory_sync_workspace_result_init / reset

```c
XLLM_API void xllm_memory_sync_workspace_result_init(
    xllm_memory_sync_workspace_result *pResult
);

XLLM_API void xllm_memory_sync_workspace_result_reset(
    xllm_memory_sync_workspace_result *pResult
);
```

Initializes and releases workspace sync result detail arrays.

### xllm_memory_sync_workspace

Scans a workspace, ingests new or changed files, and removes records for files that no longer exist.

```c
XLLM_API int xllm_memory_sync_workspace(
    xllm_memory *pMemory,
    const xllm_memory_ingest_workspace_options *pOptions,
    xllm_memory_sync_workspace_result *pResult,
    xllm_error *pError
);
```

This is the best API for periodic full workspace sync. It handles creates/updates and also checks whether existing memory records still have corresponding files.

### xllm_memory_sync_file_options_init / sync_file

```c
XLLM_API void xllm_memory_sync_file_options_init(
    xllm_memory_sync_file_options *pOptions
);

XLLM_API int xllm_memory_sync_file(
    xllm_memory *pMemory,
    const xllm_memory_sync_file_options *pOptions,
    xllm_memory_change_set *pResult,
    xllm_error *pError
);
```

Defaults: scope `KNOWLEDGE`, replace existing records, skip hidden files, skip unchanged files. `sPath` is required.

### xllm_memory_sync_files_options_init / sync_files

```c
XLLM_API void xllm_memory_sync_files_options_init(
    xllm_memory_sync_files_options *pOptions
);

XLLM_API int xllm_memory_sync_files(
    xllm_memory *pMemory,
    const xllm_memory_sync_files_options *pOptions,
    xllm_memory_change_set *pResult,
    xllm_error *pError
);
```

Use this when an external system already tells you "these files changed". It is lighter than rescanning the whole workspace.

### xllm_memory_sync_file_event_init / sync_file_events_options_init / sync_file_events

```c
XLLM_API void xllm_memory_sync_file_event_init(
    xllm_memory_file_event *pEvent
);

XLLM_API void xllm_memory_sync_file_events_options_init(
    xllm_memory_sync_file_events_options *pOptions
);

XLLM_API int xllm_memory_sync_file_events(
    xllm_memory *pMemory,
    const xllm_memory_sync_file_events_options *pOptions,
    xllm_memory_change_set *pResult,
    xllm_error *pError
);
```

Deleted events usually remove the corresponding record. Renamed events usually process the old path first, then the new path. Inspect the returned `xllm_memory_change_set` for exact changes.

### xllm_memory_change_set_init / reset

```c
XLLM_API void xllm_memory_change_set_init(
    xllm_memory_change_set *pResult
);

XLLM_API void xllm_memory_change_set_reset(
    xllm_memory_change_set *pResult
);
```

### xllm_memory_make_change_set_from_ingest / from_sync

```c
XLLM_API int xllm_memory_make_change_set_from_ingest(
    const xllm_memory_ingest_directory_result *pIngest,
    xllm_memory_change_set *pResult,
    xllm_error *pError
);

XLLM_API int xllm_memory_make_change_set_from_sync(
    const xllm_memory_sync_workspace_result *pSync,
    xllm_memory_change_set *pResult,
    xllm_error *pError
);
```

These convert ingest/sync results into a unified change set, so UI or logs can display created, updated, removed, skipped, and failed changes in one structure.

## API: Status and Health

```c
XLLM_API void xllm_memory_workspace_status_options_init(
    xllm_memory_workspace_status_options *pOptions
);

XLLM_API void xllm_memory_workspace_status_init(
    xllm_memory_workspace_status *pStatus
);

XLLM_API int xllm_memory_get_workspace_status(
    const xllm_memory *pMemory,
    const xllm_memory_workspace_status_options *pOptions,
    xllm_memory_workspace_status *pStatus,
    xllm_error *pError
);
```

`xllm_memory_get_workspace_status` does not scan disk. It reports existing workspace records in memory.

```c
XLLM_API void xllm_memory_health_check_options_init(
    xllm_memory_health_check_options *pOptions
);

XLLM_API void xllm_memory_health_check_init(
    xllm_memory_health_check *pHealth
);

XLLM_API int xllm_memory_check_health(
    const xllm_memory *pMemory,
    const xllm_memory_health_check_options *pOptions,
    xllm_memory_health_check *pHealth,
    xllm_error *pError
);
```

Health checks help confirm whether SQLite is open, schema is valid, profile matches, record/chunk counts are consistent, and sparse postings are missing or orphaned.

## Example: Sync the Whole Workspace

```c
xllm_memory_ingest_workspace_options options;
xllm_memory_sync_workspace_result result;

xllm_memory_ingest_workspace_options_init(&options);
xllm_memory_sync_workspace_result_init(&result);

options.sPath = "D:/git/xllm";
options.sRecordIdPrefix = "workspace:xllm:";
options.sAllowedExtensions = ".h;.c;.md;.txt";

if (xllm_memory_sync_workspace(memory, &options, &result, NULL) == XRT_NET_OK) {
    printf("ingested=%u removed=%u failed=%u\n",
        result.tIngest.uIngestedFileCount,
        result.uRemovedRecordCount,
        result.tIngest.uFailedFileCount);
}

xllm_memory_sync_workspace_result_reset(&result);
```

## Example: Sync from File Events

```c
xllm_memory_file_event events[2];
xllm_memory_sync_file_events_options options;
xllm_memory_change_set changes;

xllm_memory_sync_file_event_init(&events[0]);
events[0].eKind = XLLM_MEMORY_FILE_EVENT_UPDATED;
events[0].sPath = "docs/api/api-memory.md";

xllm_memory_sync_file_event_init(&events[1]);
events[1].eKind = XLLM_MEMORY_FILE_EVENT_DELETED;
events[1].sPath = "docs/old.md";

xllm_memory_sync_file_events_options_init(&options);
xllm_memory_change_set_init(&changes);

options.tBaseOptions.sRootPath = "D:/git/xllm";
options.tBaseOptions.sRecordIdPrefix = "workspace:xllm:";
options.pItems = events;
options.iItemCount = 2;

xllm_memory_sync_file_events(memory, &options, &changes, NULL);

for (size_t i = 0; i < changes.iChangeCount; ++i) {
    printf("change kind=%d\n", (int)changes.pChanges[i].eKind);
}

xllm_memory_change_set_reset(&changes);
```

## Common Mistakes

### Expecting ingest_workspace to delete old records

`xllm_memory_ingest_workspace` imports and updates files, but does not clean memory records for files that no longer exist. Use `xllm_memory_sync_workspace` when you need cleanup.

### Not setting root path

When syncing one file or file events, set `sRootPath`. This lets xllm generate stable relative paths, source URIs, and record IDs.

### Forgetting to reset change set

`xllm_memory_change_set` contains record details, skipped reasons, failed reasons, and similar data. Call `xllm_memory_change_set_reset` after use.

## Related Documentation

- [Memory Ingest API](api-memory-ingest.en.md)
- [Memory Watcher API](api-memory-watcher.en.md)
- [Memory Search API](api-memory-search.en.md)
- [Back to API Index](README.en.md)
