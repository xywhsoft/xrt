# Artifact 查询

> 状态：中文初稿，待审阅。

artifact 是 xwork 面向产品 UI、审计和恢复的核心产物模型。本教程说明如何用 summary query 分页、过滤和展示 artifact。

## 适用场景

- 展示命令输出列表。
- 展示 patch 和文件变更。
- 展示 terminal state/inventory。
- 展示 diagnostics 和 final report。
- 恢复后只加载 summary，用户展开时再加载完整 artifact。

## 查询终端状态 artifact

```c
xwork_artifact_summary_query tQuery;
xwork_artifact_summary_list tList;

xwork_artifact_summary_query_init(&tQuery);
xwork_artifact_summary_list_init(&tList);

tQuery.bHasOutputClass = true;
tQuery.eOutputClass = XWORK_ARTIFACT_OUTPUT_TERMINAL_STATE;
tQuery.iLimit = 50;

status = xwork_runtime_query_persisted_artifact_summaries(
    pRuntime,
    "run-1",
    &tQuery,
    &tList
);

xwork_artifact_summary_list_reset(&tList);
```

## 分页

使用 `after_sequence + limit`：

```c
tQuery.bHasAfterSequence = true;
tQuery.iAfterSequence = lastSequence;
tQuery.iLimit = 100;
```

返回 list 中的 `bHasMore` 和 `iNextAfterSequence` 可作为下一页游标。

## 常用过滤

| 过滤 | 字段 |
| --- | --- |
| artifact kind | `bHasKind` / `eKind` |
| output class | `bHasOutputClass` / `eOutputClass` |
| report class | `bHasReportClass` / `eReportClass` |
| name 精确匹配 | `sArtifactName` |
| name 前缀 | `sNamePrefix` |
| MIME 前缀 | `sMimeTypePrefix` |
| storage ref 前缀 | `sStorageRefPrefix` |
| exit code | `bRequireExitCode` / `bHasExitCodeValue` / `iExitCode` |

## UI 展示建议

- 列表页使用 summary，不要默认加载完整 content。
- command artifact 显示 exit code、stdout/stderr byte count 和截断标记。
- patch artifact 显示 file/hunk/add/delete 统计。
- report artifact 按 report class 分区展示。
- terminal artifact 按 storage ref 或 output role 串联同一 session。

## 下一步

- [Artifact API](../api/api-artifacts.md)
- [Persistence API](../api/api-persistence.md)
- [AI IDE Agent 范例](../case/ai-ide-agent.md)
