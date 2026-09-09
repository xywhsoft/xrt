# Artifact API

> 状态：中文逐函数参考，待人工审阅。

Artifact API 负责把 Agent 运行中的文件内容、patch、命令输出、终端状态、诊断和报告保存为可查询、可持久化、可审计的产物。

## 模块定位

Artifact 是 xwork 的 durable output 模型。它不是 UI 展示对象，也不是分布式 blob store；它提供稳定 metadata、summary、storage ref 和可选 content text，让产品层自行展示或同步。

## 本页覆盖声明

| 类别 | 声明 |
| --- | --- |
| 结构体 | `xwork_artifact_options`, `xwork_patch_artifact_options`, `xwork_report_artifact_options`, `xwork_output_artifact_options`, `xwork_command_artifact_options`, `xwork_artifact`, `xwork_artifact_summary`, `xwork_artifact_summary_list`, `xwork_artifact_summary_query` |
| 函数 | `xwork_artifact_options_init`, `xwork_patch_artifact_options_init`, `xwork_report_artifact_options_init`, `xwork_output_artifact_options_init`, `xwork_command_artifact_options_init`, `xwork_artifact_init`, `xwork_artifact_reset`, `xwork_artifact_summary_init`, `xwork_artifact_summary_reset`, `xwork_artifact_summary_list_init`, `xwork_artifact_summary_list_reset`, `xwork_artifact_summary_query_init` |

## Artifact Kind

| 类型 | 说明 |
| --- | --- |
| `XWORK_ARTIFACT_PATCH` | patch 文本、apply result 和文件摘要。 |
| `XWORK_ARTIFACT_REPORT` | 结构化报告，如 plan、review、diagnostics、final。 |
| `XWORK_ARTIFACT_COMMAND` | 命令文本、输出、exit code 和 stdout/stderr 统计。 |
| `XWORK_ARTIFACT_OUTPUT` | 文件内容、JSON、终端状态、终端 inventory 等通用输出。 |

## Schema 常量

- `XWORK_REPORT_SCHEMA_V1`
- `XWORK_DIAGNOSTICS_SCHEMA_V1`
- `XWORK_PATCH_APPLY_RESULT_SCHEMA_V1`
- `XWORK_PATCH_FILE_SUMMARY_SCHEMA_V1`
- `XWORK_TERMINAL_STATE_SCHEMA_V1`
- `XWORK_TERMINAL_INVENTORY_SCHEMA_V1`

## 所有权规则

- options 中的字符串和内容指针均为 borrowed；emit 时复制到 run/artifact 存储。
- `xwork_artifact` 和 `xwork_artifact_summary` 被查询/emit 输出填充后拥有 deep-copy 字段，使用后必须 reset。
- `xwork_artifact_summary_list` 拥有 `pItems` 数组及内部 summary 字段，使用后必须 reset。
- `sStorageRef` 指向的外部 blob 或文件由宿主系统负责；xwork 不把它变成 owned blob。

---

### xwork_artifact_options_init

初始化通用 artifact options。

**功能：**

用于直接调用 `xwork_run_emit_artifact` 前准备通用 artifact 元数据和内容字段。

**函数原型：**

```c
XWORK_API void xwork_artifact_options_init(xwork_artifact_options *pOptions);
```

**参数：**

- `pOptions`：输出参数。可为 `NULL`；非 `NULL` 时清零并设置默认 kind。

**返回值：**

无。

**资源归属：**

函数不分配资源。options 字段均为调用方借用提供。

**补充说明：**

- 默认 `eKind` 为 `XWORK_ARTIFACT_OUTPUT`。
- 直接使用该通用 options 时，调用方应明确填入 kind、name、mime/content/storage 等字段。

**范例代码：**

```c
xwork_artifact_options options;
xwork_artifact_options_init(&options);
options.sName = "output.txt";
options.sContentText = "hello";
```

**相关 API：**

- `xwork_run_emit_artifact`
- `xwork_artifact_init`

---

### xwork_patch_artifact_options_init

初始化 patch artifact options。

**功能：**

用于发出 patch artifact 前准备 patch text、target ref、apply result 和文件摘要 JSON。

**函数原型：**

```c
XWORK_API void xwork_patch_artifact_options_init(xwork_patch_artifact_options *pOptions);
```

**参数：**

- `pOptions`：输出参数。可为 `NULL`；非 `NULL` 时清零。

**返回值：**

无。

**资源归属：**

函数不分配资源。所有字符串字段由调用方借用提供。

**补充说明：**

- `xwork_run_emit_patch_artifact` 会把 kind 设置为 `XWORK_ARTIFACT_PATCH`，mime type 设置为 diff 类型。
- `sApplyResultJson` 建议使用 `XWORK_PATCH_APPLY_RESULT_SCHEMA_V1`。
- `sFileSummaryJson` 建议使用 `XWORK_PATCH_FILE_SUMMARY_SCHEMA_V1`。

**范例代码：**

```c
xwork_patch_artifact_options options;
xwork_patch_artifact_options_init(&options);
options.sPatchText = "--- a/file\n+++ b/file\n";
```

**相关 API：**

- `xwork_run_emit_patch_artifact`

---

### xwork_report_artifact_options_init

初始化 report artifact options。

**功能：**

用于发出报告、诊断、计划、review 或 final 输出前准备报告字段。

**函数原型：**

```c
XWORK_API void xwork_report_artifact_options_init(xwork_report_artifact_options *pOptions);
```

**参数：**

- `pOptions`：输出参数。可为 `NULL`；非 `NULL` 时清零并设置默认 MIME。

**返回值：**

无。

**资源归属：**

函数不分配资源。所有字符串字段由调用方借用提供。

**补充说明：**

- 默认 `sMimeType` 为 `text/markdown`。
- `xwork_run_emit_report_artifact` 会把 kind 设置为 `XWORK_ARTIFACT_REPORT`。

**范例代码：**

```c
xwork_report_artifact_options options;
xwork_report_artifact_options_init(&options);
options.eReportClass = XWORK_ARTIFACT_REPORT_FINAL;
options.sReportText = "# Result\n";
```

**相关 API：**

- `xwork_run_emit_report_artifact`
- `XWORK_REPORT_SCHEMA_V1`

---

### xwork_output_artifact_options_init

初始化 output artifact options。

**功能：**

用于发出普通文本、JSON、文件内容、终端状态或其他通用输出。

**函数原型：**

```c
XWORK_API void xwork_output_artifact_options_init(xwork_output_artifact_options *pOptions);
```

**参数：**

- `pOptions`：输出参数。可为 `NULL`；非 `NULL` 时清零并设置默认 MIME。

**返回值：**

无。

**资源归属：**

函数不分配资源。所有字符串字段由调用方借用提供。

**补充说明：**

- 默认 `sMimeType` 为 `text/plain`。
- JSON 输出建议显式设置 `sMimeType = "application/json"` 和 `eOutputClass = XWORK_ARTIFACT_OUTPUT_JSON`。

**范例代码：**

```c
xwork_output_artifact_options options;
xwork_output_artifact_options_init(&options);
options.sOutputText = "done";
```

**相关 API：**

- `xwork_run_emit_output_artifact`

---

### xwork_command_artifact_options_init

初始化 command artifact options。

**功能：**

用于记录命令文本、输出、exit code 和 stdout/stderr 统计。

**函数原型：**

```c
XWORK_API void xwork_command_artifact_options_init(xwork_command_artifact_options *pOptions);
```

**参数：**

- `pOptions`：输出参数。可为 `NULL`；非 `NULL` 时清零并设置默认 MIME。

**返回值：**

无。

**资源归属：**

函数不分配资源。所有字符串字段由调用方借用提供。

**补充说明：**

- 默认 `sMimeType` 为 `text/plain`。
- 如果设置 exit code，应同时设置 `bHasExitCode = true`。

**范例代码：**

```c
xwork_command_artifact_options options;
xwork_command_artifact_options_init(&options);
options.sCommandText = "git status";
options.bHasExitCode = true;
options.iExitCode = 0;
```

**相关 API：**

- `xwork_run_emit_command_artifact`

---

### xwork_artifact_init

初始化 artifact。

**功能：**

准备接收 run emit、run get 或 persistence load 返回的 artifact。

**函数原型：**

```c
XWORK_API void xwork_artifact_init(xwork_artifact *pArtifact);
```

**参数：**

- `pArtifact`：输出参数。可为 `NULL`；非 `NULL` 时清零并设置默认 kind。

**返回值：**

无。

**资源归属：**

函数不分配资源。填充后的 artifact 拥有 deep-copy 字段，必须 reset。

**补充说明：**

- 默认 `eKind` 为 `XWORK_ARTIFACT_OUTPUT`。

**范例代码：**

```c
xwork_artifact artifact;
xwork_artifact_init(&artifact);
xwork_artifact_reset(&artifact);
```

**相关 API：**

- `xwork_artifact_reset`
- `xwork_run_get_artifact`

---

### xwork_artifact_reset

释放并重置 artifact。

**功能：**

释放 artifact 中的 id、run id、name、mime、storage ref、summary、content 和 typed metadata 字段。

**函数原型：**

```c
XWORK_API void xwork_artifact_reset(xwork_artifact *pArtifact);
```

**参数：**

- `pArtifact`：输入/输出参数。可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放 artifact 拥有的字符串副本。

**补充说明：**

- reset 后 artifact 回到 init 状态。

**范例代码：**

```c
xwork_artifact_reset(&artifact);
```

**相关 API：**

- `xwork_artifact_init`

---

### xwork_artifact_summary_init

初始化 artifact summary。

**功能：**

准备接收 artifact summary 查询结果。

**函数原型：**

```c
XWORK_API void xwork_artifact_summary_init(xwork_artifact_summary *pSummary);
```

**参数：**

- `pSummary`：输出参数。可为 `NULL`；非 `NULL` 时清零并设置默认 kind。

**返回值：**

无。

**资源归属：**

函数不分配资源。填充后的 summary 拥有 deep-copy 字段，必须 reset。

**补充说明：**

- summary 不包含完整 content text，只保留可查询 metadata 和统计。

**范例代码：**

```c
xwork_artifact_summary summary;
xwork_artifact_summary_init(&summary);
xwork_artifact_summary_reset(&summary);
```

**相关 API：**

- `xwork_artifact_summary_reset`
- `xwork_runtime_query_persisted_artifact_summaries`

---

### xwork_artifact_summary_reset

释放并重置 artifact summary。

**功能：**

释放 summary 中的 id、name、mime、storage ref、summary、role、report subject 和 patch JSON 字段。

**函数原型：**

```c
XWORK_API void xwork_artifact_summary_reset(xwork_artifact_summary *pSummary);
```

**参数：**

- `pSummary`：输入/输出参数。可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放 summary 拥有的字符串副本。

**补充说明：**

- reset 后 summary 回到 init 状态。

**范例代码：**

```c
xwork_artifact_summary_reset(&summary);
```

**相关 API：**

- `xwork_artifact_summary_init`

---

### xwork_artifact_summary_list_init

初始化 artifact summary 列表。

**功能：**

准备接收 artifact summary 查询列表。

**函数原型：**

```c
XWORK_API void xwork_artifact_summary_list_init(xwork_artifact_summary_list *pList);
```

**参数：**

- `pList`：输出参数。可为 `NULL`；非 `NULL` 时清零。

**返回值：**

无。

**资源归属：**

函数不分配资源。查询填充后列表拥有 `pItems` 数组。

**补充说明：**

- 使用后调用 `xwork_artifact_summary_list_reset`。

**范例代码：**

```c
xwork_artifact_summary_list list;
xwork_artifact_summary_list_init(&list);
xwork_artifact_summary_list_reset(&list);
```

**相关 API：**

- `xwork_artifact_summary_list_reset`

---

### xwork_artifact_summary_list_reset

释放并重置 artifact summary 列表。

**功能：**

释放列表数组及每个 summary 拥有的 deep-copy 字段。

**函数原型：**

```c
XWORK_API void xwork_artifact_summary_list_reset(xwork_artifact_summary_list *pList);
```

**参数：**

- `pList`：输入/输出参数。可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放 `pItems` 及其元素内容。

**补充说明：**

- reset 后可复用列表变量。

**范例代码：**

```c
xwork_artifact_summary_list_reset(&list);
```

**相关 API：**

- `xwork_artifact_summary_list_init`

---

### xwork_artifact_summary_query_init

初始化 artifact summary 查询条件。

**功能：**

用于按 kind、output class、role、report class、name、MIME、storage ref、exit code 和 sequence 范围过滤 artifact summary。

**函数原型：**

```c
XWORK_API void xwork_artifact_summary_query_init(xwork_artifact_summary_query *pQuery);
```

**参数：**

- `pQuery`：输出参数。可为 `NULL`；非 `NULL` 时清零。

**返回值：**

无。

**资源归属：**

函数不分配资源。查询字符串字段由调用方借用提供。

**补充说明：**

- 空 query 表示不过滤。
- `iLimit` 可用于分页；返回列表可设置 `bHasMore` 和 `iNextAfterSequence`。

**范例代码：**

```c
xwork_artifact_summary_query query;
xwork_artifact_summary_query_init(&query);
query.bHasOutputClass = true;
query.eOutputClass = XWORK_ARTIFACT_OUTPUT_TERMINAL_STATE;
query.iLimit = 50u;
```

**相关 API：**

- `xwork_runtime_query_persisted_artifact_summaries`

## 发出 Artifact

发出函数定义在 [Run API](api-run.md) 中，包括：

- `xwork_run_emit_artifact`
- `xwork_run_emit_patch_artifact`
- `xwork_run_emit_report_artifact`
- `xwork_run_emit_output_artifact`
- `xwork_run_emit_command_artifact`

## 恢复边界

artifact metadata 和 content text 可以持久化恢复。`sStorageRef` 指向的外部 blob 或文件由宿主系统负责；xwork 内置 file backend 不管理分布式 blob store。

## 线程边界

artifact init/reset/query 结构不访问全局状态。run emit/get/query 操作会读写 run 或 persistence backend，同一 run 的 mutation 应由调用方串行化。

## 相关文档

- [Run API](api-run.md)
- [Persistence API](api-persistence.md)
- [工具、审批与 artifact](../guide/tool-approval-artifact-intro.md)
