# Workspace API

> Status: Chinese function-by-function reference, waiting for manual review.

`xwork_workspace` represents the workspace that the Agent can operate. It usually corresponds to a project root directory, and optionally accesses `xllm_memory` for workspace memory sync.

## Module positioning

Workspace is responsible for connecting the project root directory, workspace id, memory and path policies to xwork. It does not perform file system reads and writes directly; actual reads and writes are performed by the host tool, host service, and local/remote workers.

## This page covers the statement

| Category | Statement |
| --- | --- |
| Opaque objects | `xwork_workspace` |
| Structure | `xwork_workspace_options`, `xwork_workspace_memory_sync_summary`, `xwork_workspace_memory_file_sync_summary` |
| Function | `xwork_workspace_options_init`, `xwork_workspace_memory_sync_summary_init`, `xwork_workspace_memory_file_sync_summary_init`, `xwork_workspace_get_memory`, `xwork_workspace_sync_memory`, `xwork_workspace_sync_memory_file` |

## Structure fields

### xwork_workspace_options

| Field | Description |
| --- | --- |
| `sWorkspaceId` | Workspace id. Required, non-empty string. `xwork_runtime_add_workspace` copies the string. |
| `sRootPath` | Workspace root directory. Required, non-empty string. `xwork_runtime_add_workspace` copies the string. |
| `bEnableMemory` | Whether to enable workspace memory. Default `false`. |
| `pMemory` | Borrowed `xllm_memory *`. Memory sync must be non-`NULL` when enabled, and the lifecycle must be longer than the workspace. |
| `sMemorySyncAllowedExtensions` | Optional include list, for example `.c,.h,.md`. The string will be copied. |
| `sMemorySyncIgnoredDirectories` | Optional directory name exclusion list, for example, `.git,build`. The string will be copied. |
| `sMemorySyncIgnoredExtensions` | Optional extension exclusion list. The string will be copied. |
| `sMemorySyncIgnoredPathPatterns` | Optional relative path fragment/pattern exclusion list. The string will be copied. |
| `sMemorySyncIgnoredFiles` | Optional ignore List of filenames forwarded to xllm workspace sync. The string will be copied. |
| `iMemorySyncMaxFileBytes` | Maximum number of ingest bytes for a single file. `0` means not to set the xwork side upper limit. |

Memory sync policy strings use xllm's delimiting convention: commas, semicolons, pipes, or whitespace characters can be used as delimiters.

### xwork_workspace_memory_sync_summary

| Field | Description |
| --- | --- |
| `iVisitedFileCount` | Number of files accessed by the scan. |
| `iIngestedFileCount` | The actual number of files ingested. |
| `iCreatedRecordCount` | New memory record number. |
| `iUpdatedRecordCount` | Update memory record number. |
| `iSkippedFileCount` | Number of files skipped by policy or unchanged checks. |
| `iFailedFileCount` | Number of files that failed ingest. |
| `iExaminedRecordCount` | The number of historical records checked during the cleanup phase. |
| `iRemovedRecordCount` | The number of historical records removed during the cleanup phase. |

### xwork_workspace_memory_file_sync_summary

| Field | Description |
| --- | --- |
| `iChangeCount` | The total number of changes returned by xllm. |
| `iCreatedCount` | Number of memory records created. |
| `iUpdatedCount` | The number of updated memory records. |
| `iRemovedCount` | Number of memory records removed. |
| `iSkippedCount` | Number of file changes skipped. |
| `iFailedCount` | Number of failed file changes. |

## Ownership Rules

- The workspace object is mounted to the runtime linked list and is usually released uniformly by `xwork_runtime_destroy`.
- After `xwork_runtime_add_workspace` succeeds, `*ppWorkspace` is a borrowed pointer; do not continue to use it after the runtime releases it.
- `xwork_workspace_destroy` can destroy the workspace in advance and remove it from the runtime to which it belongs.
- `sWorkspaceId`, `sRootPath` and memory sync policy strings are copied.
- `pMemory` is a borrowed pointer, xwork does not destroy it.
- The string returned by the getter and `xllm_memory *` are both borrow pointers.

## Common calling sequence

```text
xwork_runtime_options_init
xwork_runtime_create
xwork_workspace_options_init
xwork_runtime_add_workspace
xwork_runtime_find_workspace / xwork_workspace_get_*
xwork_workspace_sync_memory / xwork_workspace_sync_memory_file
xwork_runtime_destroy
```

---

### xwork_workspace_options_init

Initialize `xwork_workspace_options`.

**Function:**

You can call this function before creating the workspace to set all fields to stable default values, and then fill in the workspace id, root directory, and memory sync policy as needed.

**Function prototype:**

```c
XWORK_API void xwork_workspace_options_init(xwork_workspace_options *pOptions);
```

**parameter:**

- `pOptions`: Output parameter. Can be `NULL`; if `NULL`, the function does nothing. If it is not `NULL`, it will be cleared entirely.

**Return value:**

none.

**Resource ownership:**

- Function does not allocate heap memory.
- The caller owns the `pOptions` structure storage.
- All string fields and `pMemory` are `NULL` after clearing.

**Additional Note:**

- `sWorkspaceId` and `sRootPath` must be filled in as non-empty strings before calling `xwork_runtime_add_workspace`.
- If `bEnableMemory` is `true`, it is usually also necessary to set `pMemory`.

**Example code:**

```c
#include "xwork.h"

int main(void) {
    xwork_workspace_options options;

    xwork_workspace_options_init(&options);
    options.sWorkspaceId = "main";
    options.sRootPath = ".";
    return 0;
}
```

**Related API:**

- `xwork_runtime_add_workspace`
- `xwork_workspace_destroy`

---

### xwork_workspace_memory_sync_summary_init

Initialize the workspace level memory sync statistics structure.

**Function:**

You can call this function to clear the statistics of the last synchronization before manually reusing `xwork_workspace_memory_sync_summary`.

**Function prototype:**

```c
XWORK_API void xwork_workspace_memory_sync_summary_init(
    xwork_workspace_memory_sync_summary *pSummary
);
```

**parameter:**

- `pSummary`: Output parameter. Can be `NULL`; if `NULL`, the function does nothing. If it is not `NULL`, it will be cleared entirely.

**Return value:**

none.

**Resource ownership:**

This structure only contains count fields, does not own heap memory, and does not require reset/destroy.

**Additional Note:**

- `xwork_workspace_sync_memory` will automatically clear this structure first when `pSummary` is not `NULL`.
- This function is mainly used in scenarios where the caller maintains the summary life cycle.

**Example code:**

```c
#include "xwork.h"

int main(void) {
    xwork_workspace_memory_sync_summary summary;
    xwork_workspace_memory_sync_summary_init(&summary);
    return summary.iFailedFileCount == 0 ? 0 : 1;
}
```

**Related API:**

- `xwork_workspace_sync_memory`
- `xwork_workspace_memory_file_sync_summary_init`

---

### xwork_workspace_memory_file_sync_summary_init

Initialize single file memory sync statistics structure.

**Function:**

You can call this function to clear the statistics of the last file synchronization before manually reusing `xwork_workspace_memory_file_sync_summary`.

**Function prototype:**

```c
XWORK_API void xwork_workspace_memory_file_sync_summary_init(
    xwork_workspace_memory_file_sync_summary *pSummary
);
```

**parameter:**

- `pSummary`: Output parameter. Can be `NULL`; if `NULL`, the function does nothing. If it is not `NULL`, it will be cleared entirely.

**Return value:**

none.

**Resource ownership:**

This structure only contains count fields, does not own heap memory, and does not require reset/destroy.

**Additional Note:**

- `xwork_workspace_sync_memory_file` will automatically clear this structure first when `pSummary` is not `NULL`.
- `iChangeCount` is the change number returned by xllm, and other fields are counts aggregated by change kind.

**Example code:**

```c
#include "xwork.h"

int main(void) {
    xwork_workspace_memory_file_sync_summary summary;
    xwork_workspace_memory_file_sync_summary_init(&summary);
    return summary.iChangeCount == 0 ? 0 : 1;
}
```

**Related API:**

- `xwork_workspace_sync_memory_file`
- `xwork_workspace_memory_sync_summary_init`

---

### xwork_runtime_add_workspace

Register the workspace with the runtime.

**Function:**

You can use this function to add the project root directory to the xwork runtime, so that subsequent run, tool, orchestrator, memory sync and recovery logic can reference the workspace through the workspace id.

**Function prototype:**

```c
XWORK_API xwork_status xwork_runtime_add_workspace(
    xwork_runtime *pRuntime,
    const xwork_workspace_options *pOptions,
    xwork_workspace **ppWorkspace
);
```

**parameter:**

- `pRuntime`: input/output parameters. Must not be `NULL`. The workspace will be mounted to the runtime.
- `pOptions`: input parameters. Must be other than `NULL`, and `sWorkspaceId`, `sRootPath` must be a non-empty string.
- `ppWorkspace`: Output parameter. Must not be `NULL`. Receives the workspace borrow pointer on success; sets to `NULL` on failure.

**Return value:**

- `XWORK_OK`: workspace registration successful.
- `XWORK_ERROR_INVALID_ARGUMENT`: Invalid runtime, options, output pointer, workspace id or root path.
- `XWORK_ERROR_ALREADY_EXISTS`: The same workspace id already exists in the same runtime.
- `XWORK_ERROR_NO_MEMORY`: Object or string copy allocation failed.

**Resource ownership:**

- After success the workspace is owned by the runtime.
- `*ppWorkspace` is a borrow pointer.
- Function copies `sWorkspaceId`, `sRootPath` and memory sync policy strings.
- Function borrows `pMemory` and does not destroy it.

**Additional Note:**

- workspace id must be unique within the same runtime.
- Explicitly calling `xwork_workspace_destroy` will remove the workspace from the runtime; otherwise, `xwork_runtime_destroy` will be released uniformly.
- If memory sync is enabled, the caller must ensure that `pMemory` survives at least until the workspace is destroyed.

**Example code:**

```c
#include "xwork.h"

int main(void) {
    xwork_runtime *runtime = NULL;
    xwork_runtime_options runtime_options;
    xwork_workspace_options workspace_options;
    xwork_workspace *workspace = NULL;

    xwork_runtime_options_init(&runtime_options);
    if (xwork_runtime_create(&runtime_options, &runtime) != XWORK_OK) {
        return 1;
    }

    xwork_workspace_options_init(&workspace_options);
    workspace_options.sWorkspaceId = "main";
    workspace_options.sRootPath = ".";

    if (xwork_runtime_add_workspace(runtime, &workspace_options, &workspace) != XWORK_OK) {
        xwork_runtime_destroy(runtime);
        return 1;
    }

    xwork_runtime_destroy(runtime);
    return 0;
}
```

**Related API:**

- `xwork_workspace_options_init`
- `xwork_runtime_find_workspace`
- `xwork_workspace_destroy`

---

### xwork_runtime_find_workspace

Find registered workspaces in the runtime by workspace id.

**Function:**

You can use this function to resolve the workspace id selected by external configuration, run snapshot or UI into a live workspace object at runtime.

**Function prototype:**

```c
XWORK_API xwork_workspace *xwork_runtime_find_workspace(
    const xwork_runtime *pRuntime,
    const char *sWorkspaceId
);
```

**parameter:**

- `pRuntime`: input parameters. Can be `NULL`; returns `NULL` if `NULL`.
- `sWorkspaceId`: input parameters. Can be `NULL` or the empty string; returns `NULL` if invalid.

**Return value:**

- Returns the workspace borrow pointer when found.
- Returns `NULL` if not found or if the parameter is invalid.

**Resource ownership:**

The return value is owned by the runtime and cannot be released by the caller. The return pointer becomes invalid after the workspace or runtime is destroyed.

**Additional Note:**

- The matching rule is exact match of workspace id string.
- This function does not create a workspace or load persistent state.

**Example code:**

```c
#include "xwork.h"

int main(void) {
    xwork_runtime *runtime = NULL;
    xwork_workspace *workspace = NULL;
    xwork_runtime_options options;

    xwork_runtime_options_init(&options);
    if (xwork_runtime_create(&options, &runtime) != XWORK_OK) {
        return 1;
    }

    workspace = xwork_runtime_find_workspace(runtime, "main");
    xwork_runtime_destroy(runtime);
    return workspace == NULL ? 0 : 1;
}
```

**Related API:**

- `xwork_runtime_add_workspace`
- `xwork_workspace_get_id`

---

### xwork_workspace_destroy

Destroy workspace.

**Function:**

You can use this function to remove and release a workspace in advance instead of waiting for `xwork_runtime_destroy` to be released simultaneously.

**Function prototype:**

```c
XWORK_API void xwork_workspace_destroy(xwork_workspace *pWorkspace);
```

**parameter:**

- `pWorkspace`: input/destroy parameters. Can be `NULL`; the function does nothing if it is `NULL`.

**Return value:**

none.

**Resource ownership:**

- The function frees the workspace itself and the strings copied inside.
- Function does not release borrowed `xllm_memory *`.
- If the workspace is still hanging on the runtime, the function will first remove it from the runtime list.

**Additional Note:**

- Do not destroy the same workspace repeatedly.
- After destruction, all pointers previously obtained through getter or find are invalid.

**Example code:**

```c
#include "xwork.h"

void close_workspace_early(xwork_workspace *workspace) {
    xwork_workspace_destroy(workspace);
}
```

**Related API:**

- `xwork_runtime_add_workspace`
- `xwork_runtime_destroy`

---

### xwork_workspace_get_id

Get workspace id.

**Function:**

You can use this function to read the stable id of a workspace in the log, UI, run summary, tool context, or recovery process.

**Function prototype:**

```c
XWORK_API const char *xwork_workspace_get_id(const xwork_workspace *pWorkspace);
```

**parameter:**

- `pWorkspace`: input parameters. Can be `NULL`; returns `NULL` if `NULL`.

**Return value:**

- Returns the borrowed string for the workspace id.
- If `pWorkspace` is `NULL`, return `NULL`.

**Resource ownership:**

The return value is owned by the workspace and cannot be released by the caller. The return pointer becomes invalid after the workspace is destroyed.

**Additional Note:**

- workspace id copied value from `xwork_workspace_options::sWorkspaceId`.
- Do not modify the return string.

**Example code:**

```c
#include "xwork.h"
#include <stdio.h>

void print_workspace_id(const xwork_workspace *workspace) {
    const char *id = xwork_workspace_get_id(workspace);
    printf("workspace: %s\n", id ? id : "(null)");
}
```

**Related API:**

- `xwork_runtime_find_workspace`
- `xwork_workspace_get_root_path`

---

### xwork_workspace_get_root_path

Get the workspace root directory.

**Function:**

You can use this function to map the workspace back to the host file system path for logs, UI display, host tool requests, or path policy judgment.

**Function prototype:**

```c
XWORK_API const char *xwork_workspace_get_root_path(const xwork_workspace *pWorkspace);
```

**parameter:**

- `pWorkspace`: input parameters. Can be `NULL`; returns `NULL` if `NULL`.

**Return value:**

- Returns the borrowed string for the root path.
- If `pWorkspace` is `NULL`, return `NULL`.

**Resource ownership:**

The return value is owned by the workspace and cannot be released by the caller. The return pointer becomes invalid after the workspace is destroyed.

**Additional Note:**

- xwork does not normalize paths in this getter.
- Whether the path exists, is accessible, and whether writing is allowed needs to be determined by the host service/policy layer.

**Example code:**

```c
#include "xwork.h"
#include <stdio.h>

void print_workspace_root(const xwork_workspace *workspace) {
    const char *root = xwork_workspace_get_root_path(workspace);
    printf("root: %s\n", root ? root : "(null)");
}
```

**Related API:**

- `xwork_workspace_get_id`
- `xwork_workspace_sync_memory`

---

### xwork_workspace_is_memory_enabled

Query whether the workspace has memory enabled.

**Function:**

You can use this function to determine whether the workspace declares memory enabled before executing sync, building a model context, or displaying the workspace status.

**Function prototype:**

```c
XWORK_API bool xwork_workspace_is_memory_enabled(const xwork_workspace *pWorkspace);
```

**parameter:**

- `pWorkspace`: input parameters. Can be `NULL`; returns `false` if `NULL`.

**Return value:**

- Returning `true` indicates that memory is enabled in the workspace configuration.
- Returns `false` if memory is not enabled or the parameter is `NULL`.

**Resource ownership:**

This function does not return a pointer and does not transfer resource ownership.

**Additional Note:**

- Returning `true` does not mean that `pMemory` is definitely available; `pMemory` will still be verified when calling `xwork_workspace_sync_memory`.
- If you only need to obtain the memory pointer, you can call `xwork_workspace_get_memory` directly and check the return value.

**Example code:**

```c
#include "xwork.h"

int has_workspace_memory(const xwork_workspace *workspace) {
    return xwork_workspace_is_memory_enabled(workspace) ? 1 : 0;
}
```

**Related API:**

- `xwork_workspace_get_memory`
- `xwork_workspace_sync_memory`

---

### xwork_workspace_get_memory

Get the `xllm_memory` bound to the workspace.

**Function:**

You can use this function to expose workspace memory to an upper orchestrator, diagnostic tool, or host UI for inspection or appending context.

**Function prototype:**

```c
XWORK_API xllm_memory *xwork_workspace_get_memory(const xwork_workspace *pWorkspace);
```

**parameter:**

- `pWorkspace`: input parameters. Can be `NULL`; returns `NULL` if `NULL`.

**Return value:**

- Returns the borrowed `xllm_memory *`.
- If the workspace has no associated memory or the parameter is `NULL`, return `NULL`.

**Resource ownership:**

The return value is owned by the original owner of the caller, xwork only borrows it. The caller cannot take ownership via the workspace getter.

**Additional Note:**

- `pMemory` from `xwork_workspace_options::pMemory`.
- xwork is not responsible for destroying this memory; it must outlive the workspace.

**Example code:**

```c
#include "xwork.h"

int workspace_has_memory_object(const xwork_workspace *workspace) {
    return xwork_workspace_get_memory(workspace) != NULL ? 1 : 0;
}
```

**Related API:**

- `xwork_workspace_is_memory_enabled`
- `xwork_workspace_sync_memory`

---

### xwork_workspace_sync_memory

Scan the workspace root directory and sync to `xllm_memory`.

**Function:**

You can call this function before run starts or when the user refreshes explicitly to ingest the workspace text file to the bound `xllm_memory` for model context retrieval.

**Function prototype:**

```c
XWORK_API xwork_status xwork_workspace_sync_memory(
    xwork_workspace *pWorkspace,
    xwork_workspace_memory_sync_summary *pSummary
);
```

**parameter:**

- `pWorkspace`: input/output parameters. Must be non-`NULL`, and the workspace must be memory enabled, bound to `pMemory`, and have a non-empty root path.
- `pSummary`: Output parameter. Can be `NULL`. When it is not `NULL`, the function will clear it first and then write the synchronization statistics.

**Return value:**

- `XWORK_OK`: Synchronization completed.
- `XWORK_ERROR_INVALID_ARGUMENT`: The workspace is invalid, memory is not enabled, memory is not bound, or the root path is invalid.
- `XWORK_ERROR_EXTERNAL_FAILURE`: The underlying `xllm_memory_sync_workspace` failed.

**Resource ownership:**

- Function does not transfer workspace or memory ownership.
- `pSummary` is owned by the caller and the function only writes the count field.
- The underlying xllm temporary result is released before the function returns.

**Additional Note:**

- The current implementation recursively scans the workspace root, skips hidden files by default, loads `.gitignore`, skips unchanged files, and uses `workspace://` as the source URI prefix.
- The policy string and `iMemorySyncMaxFileBytes` come from the options copied when the workspace was created.
- This function will modify `xllm_memory` and should not be executed concurrently with other write operations to the same memory, unless the host explicitly provides serialization or the underlying object supports concurrency.

**Example code:**

```c
#include "xwork.h"
#include <stdio.h>

int sync_workspace_memory(xwork_workspace *workspace) {
    xwork_workspace_memory_sync_summary summary;
    xwork_status status;

    status = xwork_workspace_sync_memory(workspace, &summary);
    if (status != XWORK_OK) {
        return 1;
    }

    printf("ingested: %zu\n", summary.iIngestedFileCount);
    return 0;
}
```

**Related API:**

- `xwork_workspace_sync_memory_file`
- `xwork_workspace_get_memory`
- `xwork_workspace_memory_sync_summary_init`

---

### xwork_workspace_sync_memory_file

Synchronize single file changes to `xllm_memory`.

**Function:**

You can call this function after file saving, editor buffer writing, or file watcher captures changes to only refresh the memory record corresponding to a path without rescanning the entire workspace.

**Function prototype:**

```c
XWORK_API xwork_status xwork_workspace_sync_memory_file(
    xwork_workspace *pWorkspace,
    const char *sPath,
    xwork_workspace_memory_file_sync_summary *pSummary
);
```

**parameter:**

- `pWorkspace`: input/output parameters. Must be non-`NULL`, and the workspace must be memory enabled, bound to `pMemory`, and have a non-empty root path.
- `sPath`: input parameters. Must be a non-empty string. Usually the file path under the workspace root.
- `pSummary`: Output parameter. Can be `NULL`. If it is not `NULL`, the function will clear it first and then write the change statistics.

**Return value:**

- `XWORK_OK`: File synchronization completed.
- `XWORK_ERROR_INVALID_ARGUMENT`: Invalid workspace, memory, root path or `sPath`.
- `XWORK_ERROR_EXTERNAL_FAILURE`: The underlying `xllm_memory_sync_file` failed.

**Resource ownership:**

- Function does not transfer workspace, memory, or path string ownership.
- `pSummary` is owned by the caller and the function only writes the count field.
- The underlying xllm change set will be released before the function returns.

**Additional Note:**

- The current implementation uses the workspace default policy, skips hidden/unchanged files, and uses `workspace://` as the source URI prefix.
- Whether `sPath` exists, is in the root, and is allowed by the policy is determined by xllm sync and the host policy. It is recommended to perform workspace boundary verification at the product layer before calling.
- This function modifies `xllm_memory` and should be serialized with other memory writes.

**Example code:**

```c
#include "xwork.h"
#include <stdio.h>

int sync_one_file(xwork_workspace *workspace, const char *path) {
    xwork_workspace_memory_file_sync_summary summary;
    xwork_status status;

    status = xwork_workspace_sync_memory_file(workspace, path, &summary);
    if (status != XWORK_OK) {
        return 1;
    }

    printf("changes: %zu\n", summary.iChangeCount);
    return 0;
}
```

**Related API:**

- `xwork_workspace_sync_memory`
- `xwork_workspace_get_root_path`
- `xwork_workspace_memory_file_sync_summary_init`

## Error handling

Common Workspace API errors include:

- `XWORK_ERROR_INVALID_ARGUMENT`: The runtime, workspace, options, id, root path, memory, or path parameters are invalid.
- `XWORK_ERROR_ALREADY_EXISTS`: Duplicate registration of workspace id in the same runtime.
- `XWORK_ERROR_NO_MEMORY`: Failed to create workspace or copy string.
- `XWORK_ERROR_EXTERNAL_FAILURE`: xllm memory sync returned failed.

## Restore boundaries

Persistent run snapshot saves the workspace id reference, but does not save the live `xllm_memory *`. Before resuming run, you should re-create the runtime, register a compatible workspace, and ensure that the required memory objects have been created or restored by the host.

## Thread boundaries

The workspace is not a concurrent mutation container. Add, destroy, memory sync, host tool write operations to the same workspace should be serialized by the caller or product layer policy. Read-only getters can be called when the object lifetime is valid and there is no concurrent destruction.

## Related documents

- [Runtime API](api-runtime.md)
- [xllm Integration API](api-xllm-integration.md)
- [Workspace Memory Tutorial](../guide/workspace-memory-intro.md)
