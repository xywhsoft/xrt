# Workspace Memory

> 状态：中文初稿，待审阅。

xwork 可以把 workspace 文件同步到 `xllm_memory`，让 Agent 在模型调用前具备项目上下文。xwork 负责 workspace 侧策略，memory 存储和检索语义由 xllm 提供。

## 启用 memory

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

`pMemory` 是 borrowed，必须比 workspace 活得更久。

## 同步整个 workspace

```c
xwork_workspace_memory_sync_summary tSummary;

xwork_workspace_memory_sync_summary_init(&tSummary);
status = xwork_workspace_sync_memory(pWorkspace, &tSummary);
```

summary 包含 visited、ingested、created、updated、skipped、failed 等计数。

## 同步单个文件

```c
xwork_workspace_memory_file_sync_summary tSummary;

xwork_workspace_memory_file_sync_summary_init(&tSummary);
status = xwork_workspace_sync_memory_file(pWorkspace, "src/main.c", &tSummary);
```

适合文件保存、patch 应用后做增量更新。

## 策略建议

- 使用 extension allowlist，避免把二进制文件送入 memory。
- 设置 `iMemorySyncMaxFileBytes`，避免大文件污染上下文。
- 忽略 `.git`、build、cache、vendor 目录。
- 恢复 run 前重新注册 workspace 和 memory；snapshot 不恢复 live `xllm_memory *`。

## 下一步

- [Workspace API](../api/api-workspace.md)
- [xllm Integration API](../api/api-xllm-integration.md)
- [xllm 编排与工具循环](xllm-orchestrator-intro.md)
