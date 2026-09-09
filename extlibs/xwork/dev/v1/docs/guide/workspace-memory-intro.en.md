# Workspace Memory

>Status: First draft in Chinese, awaiting review.

xwork can synchronize workspace files to `xllm_memory`, allowing Agent to have project context before calling the model. xwork is responsible for the workspace side strategy, and the memory storage and retrieval semantics are provided by xllm.

## Enable memory

```c
xwork_workspace_options tWorkspace;

xwork_workspace_options_init(&tWorkspace);
tWorkspace.sWorkspaceId = "main";
tWorkspace.sRootPath = "D:/git/project";
tWorkspace.bEnableMemory = true;
tWorkspace.pMemory = pMemory; /* borrowed xllm_memory* */
tWorkspace.sMemorySyncAllowedExtensions = ".c,.h,.md";
tWorkspace.sMemorySyncIgnoredDirectories = ".git,build";
tWorkspace.iMemorySyncMaxFileBytes = 1024 * 1024;
```

`pMemory` is borrowed and must outlive the workspace.

## Synchronize the entire workspace

```c
xwork_workspace_memory_sync_summary tSummary;

xwork_workspace_memory_sync_summary_init(&tSummary);
status = xwork_workspace_sync_memory(pWorkspace, &tSummary);
```

summary contains counts of visited, ingested, created, updated, skipped, failed, etc.

## Synchronize a single file

```c
xwork_workspace_memory_file_sync_summary tSummary;

xwork_workspace_memory_file_sync_summary_init(&tSummary);
status = xwork_workspace_sync_memory_file(pWorkspace, "src/main.c", &tSummary);
```

Suitable for file saving and incremental updates after patch application.

## Strategy suggestions

- Use extension allowlist to avoid sending binary files into memory.
- Set `iMemorySyncMaxFileBytes` to avoid large files polluting the context.
- Ignore `.git`, build, cache, vendor directories.
- Re-register workspace and memory before restoring run; snapshot does not restore live `xllm_memory *`.

## Next step

- [Workspace API](../api/api-workspace.md)
- [xllm Integration API](../api/api-xllm-integration.md)
- [xllm Orchestration and Tool Loop](xllm-orchestrator-intro.md)
