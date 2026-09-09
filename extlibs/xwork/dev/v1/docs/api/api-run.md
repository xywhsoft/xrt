# Run API

> 状态：中文逐函数参考，待人工审阅。本文先覆盖 run 主路径、event/step/checkpoint/summary/query/async 的完整函数索引；字段级细节后续可继续扩写。

`xwork_run` 表示一次 Agent 任务运行。它承载 instruction、workspace 引用、生命周期状态、event、step、checkpoint、approval、artifact 和 summary。

## 模块定位

Run API 负责 xwork 任务的生命周期、状态推进、查询、同步/异步执行和可恢复对象快照。它不直接决定模型如何回答，也不直接执行 host side effect；这些由 orchestrator、tool、policy 和 host service 协作完成。

## 本页覆盖声明

| 类别 | 声明 |
| --- | --- |
| 不透明对象 | `xwork_run`, `xwork_run_async` |
| 结构体 | `xwork_run_options`, `xwork_event`, `xwork_run_step`, `xwork_run_step_list`, `xwork_run_step_query`, `xwork_checkpoint`, `xwork_run_snapshot`, `xwork_run_summary`, `xwork_run_summary_list`, `xwork_run_index_entry`, `xwork_run_index_list`, `xwork_run_index_query` |
| 函数 | 本页所有 `### xwork_*` 小节 |

## 所有权规则

- `xwork_run_create` 返回的 run 附着到 runtime，调用方可显式 `xwork_run_destroy`，也可由 `xwork_runtime_destroy` 统一释放。
- run options 中的 run id、parent run id、agent id、task id、instruction、profile id 和 workspace id 会被复制。
- workspace id 指向的 workspace 必须已注册在 runtime 中，并在 run 生命周期内保持有效。
- `*_get_*` 输出到结构体的 API 会 deep-copy 可变字符串；调用者必须调用对应 `*_reset`。
- getter 返回的 `const char *` 是借用指针，run 销毁后失效。
- async handle 由 `xwork_run_execute_async` 创建，必须用 `xwork_run_async_destroy` 释放。

## 常见调用顺序

```text
xwork_run_options_init
xwork_run_create
xwork_run_execute / xwork_run_execute_async
xwork_run_get_summary / xwork_run_get_snapshot
xwork_run_destroy / xwork_runtime_destroy
```

## 通用 init/reset 约定

以下 init 函数都允许传入 `NULL`，传入 `NULL` 时不执行操作；非 `NULL` 时写入默认值。reset 函数允许传入 `NULL`，会释放结构体拥有的深拷贝字段并重新初始化。

---

### xwork_run_options_init

初始化 run 创建选项。

**功能：**

在调用 `xwork_run_create` 前设置稳定默认值，再填写 instruction、workspace id 和 profile/autonomy 配置。

**函数原型：**

```c
XWORK_API void xwork_run_options_init(xwork_run_options *pOptions);
```

**参数：**

- `pOptions`：输出参数。可为 `NULL`；非 `NULL` 时清零，默认 `eAutonomy` 为 `XWORK_AUTONOMY_SEMI_AUTO`，并初始化 `tSessionPolicy`。

**返回值：**

无。

**资源归属：**

函数不分配资源。所有字符串和 workspace id 数组字段由调用方在 create 前提供。

**补充说明：**

- `sInstruction` 是创建 run 的核心输入，通常必须为非空。
- `psWorkspaceIds` 可以为空；非空时每个 id 必须能在 runtime 中找到。

**范例代码：**

```c
#include "xwork.h"

int main(void) {
    xwork_run_options options;
    xwork_run_options_init(&options);
    options.sInstruction = "Summarize the workspace.";
    return 0;
}
```

**相关 API：**

- `xwork_run_create`
- `xwork_profile_apply_run_options`

---

### xwork_run_summary_init

初始化 run summary。

**功能：**

用于准备接收 `xwork_run_get_summary` 或 persistence 查询返回的 summary。

**函数原型：**

```c
XWORK_API void xwork_run_summary_init(xwork_run_summary *pSummary);
```

**参数：**

- `pSummary`：输出参数。可为 `NULL`；非 `NULL` 时清零并写入默认 autonomy/state。

**返回值：**

无。

**资源归属：**

函数不分配资源。后续 API 写入的字符串副本由 summary 拥有，需要 `xwork_run_summary_reset` 释放。

**补充说明：**

- 默认 `eAutonomy` 为 `XWORK_AUTONOMY_SEMI_AUTO`。
- 默认 `eState` 为 `XWORK_RUN_CREATED`。

**范例代码：**

```c
#include "xwork.h"

int main(void) {
    xwork_run_summary summary;
    xwork_run_summary_init(&summary);
    xwork_run_summary_reset(&summary);
    return 0;
}
```

**相关 API：**

- `xwork_run_get_summary`
- `xwork_run_summary_reset`

---

### xwork_run_summary_reset

释放并重置 run summary。

**功能：**

释放 summary 中由 xwork deep-copy 的字符串字段，使结构体可复用或安全结束生命周期。

**函数原型：**

```c
XWORK_API void xwork_run_summary_reset(xwork_run_summary *pSummary);
```

**参数：**

- `pSummary`：输入/输出参数。可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放 `sRunId`、`sParentRunId`、`sAgentId`、`sTaskId`、`sInstruction` 等 owned 字符串副本。

**补充说明：**

- 对未初始化结构调用 reset 不安全；先使用 init。

**范例代码：**

```c
xwork_run_summary summary;
xwork_run_summary_init(&summary);
/* fill summary */
xwork_run_summary_reset(&summary);
```

**相关 API：**

- `xwork_run_summary_init`
- `xwork_run_get_summary`

---

### xwork_run_summary_list_init

初始化 run summary 列表。

**功能：**

准备接收 persistence 或 runtime 查询返回的 run summary 列表。

**函数原型：**

```c
XWORK_API void xwork_run_summary_list_init(xwork_run_summary_list *pList);
```

**参数：**

- `pList`：输出参数。可为 `NULL`；非 `NULL` 时清零。

**返回值：**

无。

**资源归属：**

函数不分配资源。查询函数填充后，列表拥有 `pItems` 及内部 summary 字段副本。

**补充说明：**

- 使用后调用 `xwork_run_summary_list_reset`。

**范例代码：**

```c
xwork_run_summary_list list;
xwork_run_summary_list_init(&list);
xwork_run_summary_list_reset(&list);
```

**相关 API：**

- `xwork_run_summary_list_reset`
- `xwork_runtime_list_persisted_run_summaries`

---

### xwork_run_summary_list_reset

释放并重置 run summary 列表。

**功能：**

释放列表数组和每个 summary 的 deep-copy 字段。

**函数原型：**

```c
XWORK_API void xwork_run_summary_list_reset(xwork_run_summary_list *pList);
```

**参数：**

- `pList`：输入/输出参数。可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放 `pItems` 数组及每个 `xwork_run_summary` 拥有的字符串。

**补充说明：**

- reset 后列表回到 init 状态。

**范例代码：**

```c
xwork_run_summary_list list;
xwork_run_summary_list_init(&list);
xwork_run_summary_list_reset(&list);
```

**相关 API：**

- `xwork_run_summary_list_init`

---

### xwork_run_index_entry_init

初始化 run index entry。

**功能：**

准备接收 run index 查询项，包括 summary、last approval/event/checkpoint/artifact。

**函数原型：**

```c
XWORK_API void xwork_run_index_entry_init(xwork_run_index_entry *pEntry);
```

**参数：**

- `pEntry`：输出参数。可为 `NULL`；非 `NULL` 时初始化嵌套对象。

**返回值：**

无。

**资源归属：**

函数不分配资源。填充后的 entry 拥有嵌套对象的 deep-copy 字段。

**补充说明：**

- 使用后调用 `xwork_run_index_entry_reset`。

**范例代码：**

```c
xwork_run_index_entry entry;
xwork_run_index_entry_init(&entry);
xwork_run_index_entry_reset(&entry);
```

**相关 API：**

- `xwork_run_index_entry_reset`
- `xwork_runtime_query_persisted_run_index`

---

### xwork_run_index_entry_reset

释放并重置 run index entry。

**功能：**

释放 entry 内嵌 summary、approval、event、checkpoint、artifact 的 owned 字段。

**函数原型：**

```c
XWORK_API void xwork_run_index_entry_reset(xwork_run_index_entry *pEntry);
```

**参数：**

- `pEntry`：输入/输出参数。可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放 entry 拥有的所有深拷贝字段。

**补充说明：**

- reset 后 entry 回到 init 状态。

**范例代码：**

```c
xwork_run_index_entry entry;
xwork_run_index_entry_init(&entry);
xwork_run_index_entry_reset(&entry);
```

**相关 API：**

- `xwork_run_index_entry_init`

---

### xwork_run_index_list_init

初始化 run index 列表。

**功能：**

准备接收 run index 查询结果。

**函数原型：**

```c
XWORK_API void xwork_run_index_list_init(xwork_run_index_list *pList);
```

**参数：**

- `pList`：输出参数。可为 `NULL`；非 `NULL` 时清零。

**返回值：**

无。

**资源归属：**

函数不分配资源。查询填充后，列表拥有 `pItems` 数组。

**补充说明：**

- 使用后调用 `xwork_run_index_list_reset`。

**范例代码：**

```c
xwork_run_index_list list;
xwork_run_index_list_init(&list);
xwork_run_index_list_reset(&list);
```

**相关 API：**

- `xwork_run_index_list_reset`

---

### xwork_run_index_list_reset

释放并重置 run index 列表。

**功能：**

释放列表数组以及每个 index entry 拥有的 deep-copy 字段。

**函数原型：**

```c
XWORK_API void xwork_run_index_list_reset(xwork_run_index_list *pList);
```

**参数：**

- `pList`：输入/输出参数。可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放列表拥有的 `pItems` 及其元素内容。

**补充说明：**

- reset 后可复用同一列表变量接收下一次查询。

**范例代码：**

```c
xwork_run_index_list list;
xwork_run_index_list_init(&list);
xwork_run_index_list_reset(&list);
```

**相关 API：**

- `xwork_run_index_list_init`

---

### xwork_run_index_query_init

初始化 run index 查询条件。

**功能：**

用于构造持久化 run index 查询过滤和排序条件。

**函数原型：**

```c
XWORK_API void xwork_run_index_query_init(xwork_run_index_query *pQuery);
```

**参数：**

- `pQuery`：输出参数。可为 `NULL`；非 `NULL` 时写入默认过滤值和排序值。

**返回值：**

无。

**资源归属：**

函数不分配资源。查询字符串字段由调用方借用提供。

**补充说明：**

- 默认 state 为 `XWORK_RUN_CREATED`，autonomy 为 `XWORK_AUTONOMY_SEMI_AUTO`，排序为 run id 升序。
- 具体字段含义由 persistence API 使用。

**范例代码：**

```c
xwork_run_index_query query;
xwork_run_index_query_init(&query);
```

**相关 API：**

- `xwork_runtime_query_persisted_run_index`

---

### xwork_event_init

初始化 event。

**功能：**

准备接收 run event 或 persistence event 查询结果。

**函数原型：**

```c
XWORK_API void xwork_event_init(xwork_event *pEvent);
```

**参数：**

- `pEvent`：输出参数。可为 `NULL`；非 `NULL` 时清零并写入默认 kind/state。

**返回值：**

无。

**资源归属：**

函数不分配资源。填充后的 event 拥有 deep-copy 字符串，使用后调用 `xwork_event_reset`。

**补充说明：**

- event 表示 run 生命周期、tool、approval、checkpoint 等事件。

**范例代码：**

```c
xwork_event event;
xwork_event_init(&event);
xwork_event_reset(&event);
```

**相关 API：**

- `xwork_run_get_event`
- `xwork_event_reset`

---

### xwork_event_reset

释放并重置 event。

**功能：**

释放 event 中的 id、run id、tool id、summary 等 deep-copy 字段。

**函数原型：**

```c
XWORK_API void xwork_event_reset(xwork_event *pEvent);
```

**参数：**

- `pEvent`：输入/输出参数。可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放 event 拥有的字符串副本。

**补充说明：**

- 对查询输出结构，应在不再使用时 reset。

**范例代码：**

```c
xwork_event event;
xwork_event_init(&event);
xwork_event_reset(&event);
```

**相关 API：**

- `xwork_event_init`
- `xwork_run_get_last_event`

---

### xwork_run_step_init

初始化 run step。

**功能：**

准备接收按 event/checkpoint 聚合出的 step 结果。

**函数原型：**

```c
XWORK_API void xwork_run_step_init(xwork_run_step *pStep);
```

**参数：**

- `pStep`：输出参数。可为 `NULL`；非 `NULL` 时清零并写入默认 run state。

**返回值：**

无。

**资源归属：**

函数不分配资源。填充后 step 拥有 deep-copy 字段，需要 reset。

**补充说明：**

- step 是面向 UI/查询的投影，不是独立执行对象。

**范例代码：**

```c
xwork_run_step step;
xwork_run_step_init(&step);
xwork_run_step_reset(&step);
```

**相关 API：**

- `xwork_run_get_step`
- `xwork_run_step_reset`

---

### xwork_run_step_reset

释放并重置 run step。

**功能：**

释放 step 中由 xwork deep-copy 的字符串字段。

**函数原型：**

```c
XWORK_API void xwork_run_step_reset(xwork_run_step *pStep);
```

**参数：**

- `pStep`：输入/输出参数。可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放 step 拥有的字符串副本。

**补充说明：**

- reset 后 step 回到 init 状态。

**范例代码：**

```c
xwork_run_step step;
xwork_run_step_init(&step);
xwork_run_step_reset(&step);
```

**相关 API：**

- `xwork_run_step_init`

---

### xwork_run_step_list_init

初始化 step 列表。

**功能：**

准备接收 `xwork_run_query_steps` 返回的 step 列表。

**函数原型：**

```c
XWORK_API void xwork_run_step_list_init(xwork_run_step_list *pList);
```

**参数：**

- `pList`：输出参数。可为 `NULL`；非 `NULL` 时清零。

**返回值：**

无。

**资源归属：**

函数不分配资源。查询填充后列表拥有 `pItems`。

**补充说明：**

- 使用后调用 `xwork_run_step_list_reset`。

**范例代码：**

```c
xwork_run_step_list list;
xwork_run_step_list_init(&list);
xwork_run_step_list_reset(&list);
```

**相关 API：**

- `xwork_run_query_steps`
- `xwork_run_step_list_reset`

---

### xwork_run_step_list_reset

释放并重置 step 列表。

**功能：**

释放列表数组和每个 step 的 deep-copy 字段。

**函数原型：**

```c
XWORK_API void xwork_run_step_list_reset(xwork_run_step_list *pList);
```

**参数：**

- `pList`：输入/输出参数。可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放 `pItems` 数组及其元素内容。

**补充说明：**

- reset 后可复用列表。

**范例代码：**

```c
xwork_run_step_list list;
xwork_run_step_list_init(&list);
xwork_run_step_list_reset(&list);
```

**相关 API：**

- `xwork_run_step_list_init`

---

### xwork_run_step_query_init

初始化 step 查询条件。

**功能：**

用于构造 `xwork_run_query_steps` 的过滤条件，例如事件类型、状态、序列范围和 limit。

**函数原型：**

```c
XWORK_API void xwork_run_step_query_init(xwork_run_step_query *pQuery);
```

**参数：**

- `pQuery`：输出参数。可为 `NULL`；非 `NULL` 时清零。

**返回值：**

无。

**资源归属：**

函数不分配资源。查询字符串字段由调用方借用提供。

**补充说明：**

- 空 query 表示不过滤。

**范例代码：**

```c
xwork_run_step_query query;
xwork_run_step_query_init(&query);
query.iLimit = 20u;
```

**相关 API：**

- `xwork_run_query_steps`

---

### xwork_checkpoint_init

初始化 checkpoint。

**功能：**

准备接收 run checkpoint 或 persistence checkpoint 查询结果。

**函数原型：**

```c
XWORK_API void xwork_checkpoint_init(xwork_checkpoint *pCheckpoint);
```

**参数：**

- `pCheckpoint`：输出参数。可为 `NULL`；非 `NULL` 时清零并写入默认 kind/state。

**返回值：**

无。

**资源归属：**

函数不分配资源。填充后的 checkpoint 拥有 deep-copy 字符串，需要 reset。

**补充说明：**

- checkpoint 保存可恢复边界，不保存 live 线程、进程或 callback 栈。

**范例代码：**

```c
xwork_checkpoint checkpoint;
xwork_checkpoint_init(&checkpoint);
xwork_checkpoint_reset(&checkpoint);
```

**相关 API：**

- `xwork_run_get_checkpoint`
- `xwork_checkpoint_reset`

---

### xwork_checkpoint_reset

释放并重置 checkpoint。

**功能：**

释放 checkpoint id、pending step、session state ref、tool output ref 等 deep-copy 字段。

**函数原型：**

```c
XWORK_API void xwork_checkpoint_reset(xwork_checkpoint *pCheckpoint);
```

**参数：**

- `pCheckpoint`：输入/输出参数。可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放 checkpoint 拥有的字符串副本。

**补充说明：**

- reset 后 checkpoint 回到 init 状态。

**范例代码：**

```c
xwork_checkpoint checkpoint;
xwork_checkpoint_init(&checkpoint);
xwork_checkpoint_reset(&checkpoint);
```

**相关 API：**

- `xwork_checkpoint_init`

---

### xwork_run_snapshot_init

初始化 run snapshot。

**功能：**

准备接收 `xwork_run_get_snapshot` 或 persistence 加载的 run snapshot。

**函数原型：**

```c
XWORK_API void xwork_run_snapshot_init(xwork_run_snapshot *pSnapshot);
```

**参数：**

- `pSnapshot`：输出参数。可为 `NULL`；非 `NULL` 时清零并写入默认状态。

**返回值：**

无。

**资源归属：**

函数不分配资源。填充后的 snapshot 拥有深拷贝字段和 workspace id 数组，需要 reset。

**补充说明：**

- snapshot 用于恢复 run 的可序列化状态，不包含 live runtime 资源。

**范例代码：**

```c
xwork_run_snapshot snapshot;
xwork_run_snapshot_init(&snapshot);
xwork_run_snapshot_reset(&snapshot);
```

**相关 API：**

- `xwork_run_get_snapshot`
- `xwork_run_snapshot_reset`

---

### xwork_run_snapshot_reset

释放并重置 run snapshot。

**功能：**

释放 snapshot 拥有的字符串、workspace id 数组和嵌套状态字段。

**函数原型：**

```c
XWORK_API void xwork_run_snapshot_reset(xwork_run_snapshot *pSnapshot);
```

**参数：**

- `pSnapshot`：输入/输出参数。可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放 snapshot 拥有的所有深拷贝字段。

**补充说明：**

- 使用 `xwork_runtime_recover_run` 前后都应明确 snapshot 生命周期。

**范例代码：**

```c
xwork_run_snapshot snapshot;
xwork_run_snapshot_init(&snapshot);
xwork_run_snapshot_reset(&snapshot);
```

**相关 API：**

- `xwork_run_snapshot_init`
- `xwork_runtime_recover_run`

---

### xwork_run_create

创建 run。

**功能：**

创建一次 agent 任务运行，并把它附着到 runtime。

**函数原型：**

```c
XWORK_API xwork_status xwork_run_create(
    xwork_runtime *pRuntime,
    const xwork_run_options *pOptions,
    xwork_run **ppRun
);
```

**参数：**

- `pRuntime`：输入/输出参数。必须非 `NULL`。
- `pOptions`：输入参数。必须非 `NULL`，通常需要非空 `sInstruction`。
- `ppRun`：输出参数。必须非 `NULL`。成功时接收 run 指针；失败时为 `NULL`。

**返回值：**

- `XWORK_OK`：创建成功。
- `XWORK_ERROR_INVALID_ARGUMENT`：runtime、options、输出指针或必需字段无效。
- `XWORK_ERROR_NOT_FOUND`：引用的 workspace id 未注册。
- `XWORK_ERROR_NO_MEMORY`：分配或字符串复制失败。

**资源归属：**

成功后 run 由 runtime 拥有。调用方可以显式销毁，也可以交给 runtime 销毁。

**补充说明：**

- 创建时会记录 `XWORK_EVENT_RUN_CREATED`。
- workspace id 数组会 deep-copy，但 workspace 本身保持 runtime 所有权。

**范例代码：**

```c
#include "xwork.h"

int create_run(xwork_runtime *runtime) {
    xwork_run_options options;
    xwork_run *run = NULL;
    xwork_run_options_init(&options);
    options.sInstruction = "Inspect this repository.";
    return xwork_run_create(runtime, &options, &run) == XWORK_OK ? 0 : 1;
}
```

**相关 API：**

- `xwork_run_options_init`
- `xwork_run_destroy`

---

### xwork_run_destroy

销毁 run。

**功能：**

提前释放 run 及其事件、checkpoint、artifact、snapshot 相关内存。

**函数原型：**

```c
XWORK_API void xwork_run_destroy(xwork_run *pRun);
```

**参数：**

- `pRun`：输入/销毁参数。可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放 run 自身及内部 owned 字段，并从 runtime 链表摘除。

**补充说明：**

- 不要在 run 正在异步执行时销毁它；应先 cancel/wait async handle。

**范例代码：**

```c
void close_run(xwork_run *run) {
    xwork_run_destroy(run);
}
```

**相关 API：**

- `xwork_run_create`
- `xwork_run_async_destroy`

---

### xwork_run_start

将 run 推进到 running。

**功能：**

用于手动生命周期推进或 orchestrator 执行入口，把可启动 run 置为运行中并记录事件。

**函数原型：**

```c
XWORK_API xwork_status xwork_run_start(xwork_run *pRun);
```

**参数：**

- `pRun`：输入/输出参数。必须非 `NULL`。

**返回值：**

- `XWORK_OK`：状态推进成功。
- `XWORK_ERROR_INVALID_ARGUMENT`：run 为空。
- `XWORK_ERROR_INVALID_STATE`：当前状态不允许启动。

**资源归属：**

不转移资源所有权。

**补充说明：**

- orchestrator 会在需要时自动调用。

**范例代码：**

```c
xwork_status status = xwork_run_start(run);
```

**相关 API：**

- `xwork_run_complete`
- `xwork_run_execute`

---

### xwork_run_set_waiting_approval

将 run 标记为等待审批。

**功能：**

用于在工具或 side effect 需要人工/策略审批时进入暂停边界。

**函数原型：**

```c
XWORK_API xwork_status xwork_run_set_waiting_approval(xwork_run *pRun);
```

**参数：**

- `pRun`：输入/输出参数。必须非 `NULL`。

**返回值：**

- `XWORK_OK`：状态推进成功。
- `XWORK_ERROR_INVALID_ARGUMENT`：run 为空。
- `XWORK_ERROR_INVALID_STATE`：当前状态不允许该转换。

**资源归属：**

不转移资源所有权。

**补充说明：**

- 审批请求详情通常通过 approval API 或 orchestrator 记录。

**范例代码：**

```c
xwork_status status = xwork_run_set_waiting_approval(run);
```

**相关 API：**

- `xwork_run_submit_approval`
- `xwork_run_resume`

---

### xwork_run_set_waiting_tool

将 run 标记为等待工具。

**功能：**

用于工具调用已挂起、等待外部工具结果或恢复 pending tool 的场景。

**函数原型：**

```c
XWORK_API xwork_status xwork_run_set_waiting_tool(xwork_run *pRun);
```

**参数：**

- `pRun`：输入/输出参数。必须非 `NULL`。

**返回值：**

- `XWORK_OK`：状态推进成功。
- `XWORK_ERROR_INVALID_ARGUMENT`：run 为空。
- `XWORK_ERROR_INVALID_STATE`：当前状态不允许该转换。

**资源归属：**

不转移资源所有权。

**补充说明：**

- 一般由 orchestrator/tool loop 使用。

**范例代码：**

```c
xwork_status status = xwork_run_set_waiting_tool(run);
```

**相关 API：**

- `xwork_run_resume`
- `xwork_runtime_find_tool`

---

### xwork_run_set_paused

将 run 标记为 paused。

**功能：**

用于进入可恢复暂停边界，并记录 paused 事件。

**函数原型：**

```c
XWORK_API xwork_status xwork_run_set_paused(xwork_run *pRun);
```

**参数：**

- `pRun`：输入/输出参数。必须非 `NULL`。

**返回值：**

- `XWORK_OK`：暂停成功。
- `XWORK_ERROR_INVALID_ARGUMENT`：run 为空。
- `XWORK_ERROR_INVALID_STATE`：当前状态不允许暂停。

**资源归属：**

不转移资源所有权。

**补充说明：**

- paused 状态可通过 `xwork_run_resume` 尝试恢复。

**范例代码：**

```c
xwork_status status = xwork_run_set_paused(run);
```

**相关 API：**

- `xwork_run_resume`

---

### xwork_run_submit_approval

提交审批结果。

**功能：**

把外部审批决策写回 run，用于恢复等待审批的工具/side effect。

**函数原型：**

```c
XWORK_API xwork_status xwork_run_submit_approval(
    xwork_run *pRun,
    xwork_approval_state eDecision
);
```

**参数：**

- `pRun`：输入/输出参数。必须非 `NULL`。
- `eDecision`：输入参数。审批状态，例如 approved/denied。

**返回值：**

- `XWORK_OK`：提交成功。
- `XWORK_ERROR_INVALID_ARGUMENT`：run 无效。
- `XWORK_ERROR_INVALID_STATE`：run 当前没有可提交的审批边界。

**资源归属：**

不转移资源所有权。

**补充说明：**

- 提交后通常继续调用 `xwork_run_resume` 或让 orchestrator 恢复。

**范例代码：**

```c
xwork_status status = xwork_run_submit_approval(run, XWORK_APPROVAL_APPROVED);
```

**相关 API：**

- `xwork_run_get_last_approval_request`
- `xwork_run_resume`

---

### xwork_run_load_checkpoint

加载指定 checkpoint 到 run。

**功能：**

将 run 的恢复目标设置为指定 checkpoint，用于后续 resume。

**函数原型：**

```c
XWORK_API xwork_status xwork_run_load_checkpoint(
    xwork_run *pRun,
    const char *sCheckpointId
);
```

**参数：**

- `pRun`：输入/输出参数。必须非 `NULL`。
- `sCheckpointId`：输入参数。必须为非空 checkpoint id。

**返回值：**

- `XWORK_OK`：加载成功。
- `XWORK_ERROR_INVALID_ARGUMENT`：参数无效。
- `XWORK_ERROR_NOT_FOUND`：checkpoint id 不存在。
- `XWORK_ERROR_INVALID_STATE`：当前状态不可加载。

**资源归属：**

不转移 checkpoint 所有权；run 内部复制/引用其可恢复字段。

**补充说明：**

- 加载 checkpoint 不恢复 live process/thread/terminal。

**范例代码：**

```c
xwork_status status = xwork_run_load_checkpoint(run, "checkpoint-1");
```

**相关 API：**

- `xwork_run_get_checkpoint`
- `xwork_run_resume`

---

### xwork_run_resume

恢复 run。

**功能：**

从 paused、waiting 或 checkpoint 边界恢复 run 状态，使 orchestrator 可以继续执行。

**函数原型：**

```c
XWORK_API xwork_status xwork_run_resume(xwork_run *pRun);
```

**参数：**

- `pRun`：输入/输出参数。必须非 `NULL`。

**返回值：**

- `XWORK_OK`：恢复成功。
- `XWORK_ERROR_INVALID_ARGUMENT`：run 为空。
- `XWORK_ERROR_INVALID_STATE`：terminal run 或不可恢复边界。

**资源归属：**

不转移资源所有权。

**补充说明：**

- 如果存在 pending tool call，恢复前需要有兼容的工具注册和审批状态。

**范例代码：**

```c
xwork_status status = xwork_run_resume(run);
```

**相关 API：**

- `xwork_run_load_checkpoint`
- `xwork_run_submit_approval`

---

### xwork_run_complete

将 run 标记为完成。

**功能：**

用于手动生命周期推进或 orchestrator 成功结束时，把 run 置为 terminal completed。

**函数原型：**

```c
XWORK_API xwork_status xwork_run_complete(xwork_run *pRun);
```

**参数：**

- `pRun`：输入/输出参数。必须非 `NULL`。

**返回值：**

- `XWORK_OK`：完成成功。
- `XWORK_ERROR_INVALID_ARGUMENT`：run 为空。
- `XWORK_ERROR_INVALID_STATE`：当前状态不允许完成。

**资源归属：**

不转移资源所有权。

**补充说明：**

- 成功后会记录 run completed 事件。

**范例代码：**

```c
xwork_status status = xwork_run_complete(run);
```

**相关 API：**

- `xwork_run_start`
- `xwork_run_fail`

---

### xwork_run_cancel

将 run 标记为取消。

**功能：**

用于协作式取消完成后，把 run 置为 terminal cancelled。

**函数原型：**

```c
XWORK_API xwork_status xwork_run_cancel(xwork_run *pRun);
```

**参数：**

- `pRun`：输入/输出参数。必须非 `NULL`。

**返回值：**

- `XWORK_OK`：取消状态写入成功。
- `XWORK_ERROR_INVALID_ARGUMENT`：run 为空。
- `XWORK_ERROR_INVALID_STATE`：当前状态不允许取消。

**资源归属：**

不转移资源所有权。

**补充说明：**

- 异步执行取消应优先调用 `xwork_run_async_cancel`。

**范例代码：**

```c
xwork_status status = xwork_run_cancel(run);
```

**相关 API：**

- `xwork_run_async_cancel`
- `xwork_run_fail`

---

### xwork_run_fail

将 run 标记为失败。

**功能：**

用于不可恢复错误后，把 run 置为 terminal failed。

**函数原型：**

```c
XWORK_API xwork_status xwork_run_fail(xwork_run *pRun);
```

**参数：**

- `pRun`：输入/输出参数。必须非 `NULL`。

**返回值：**

- `XWORK_OK`：失败状态写入成功。
- `XWORK_ERROR_INVALID_ARGUMENT`：run 为空。
- `XWORK_ERROR_INVALID_STATE`：当前状态不允许失败转换。

**资源归属：**

不转移资源所有权。

**补充说明：**

- 成功后会记录 run failed 事件。

**范例代码：**

```c
xwork_status status = xwork_run_fail(run);
```

**相关 API：**

- `xwork_run_complete`
- `xwork_run_cancel`

---

### xwork_run_get_id

获取 run id。

**功能：**

用于日志、UI、持久化索引或父子任务关联。

**函数原型：**

```c
XWORK_API const char *xwork_run_get_id(const xwork_run *pRun);
```

**参数：**

- `pRun`：输入参数。可为 `NULL`。

**返回值：**

返回借用 run id；`pRun` 为 `NULL` 时返回 `NULL`。

**资源归属：**

返回值由 run 拥有，调用者不能释放。

**补充说明：**

- 指针在 run 销毁后失效。

**范例代码：**

```c
const char *id = xwork_run_get_id(run);
```

**相关 API：**

- `xwork_run_get_instruction`

---

### xwork_run_get_instruction

获取 run instruction。

**功能：**

读取创建 run 时复制的用户任务指令。

**函数原型：**

```c
XWORK_API const char *xwork_run_get_instruction(const xwork_run *pRun);
```

**参数：**

- `pRun`：输入参数。可为 `NULL`。

**返回值：**

返回借用 instruction；`pRun` 为 `NULL` 时返回 `NULL`。

**资源归属：**

返回值由 run 拥有，调用者不能释放。

**补充说明：**

- 不要修改返回字符串。

**范例代码：**

```c
const char *text = xwork_run_get_instruction(run);
```

**相关 API：**

- `xwork_run_get_id`

---

### xwork_run_get_state

获取 run 当前状态。

**功能：**

用于 UI 状态展示、调度判断或测试断言。

**函数原型：**

```c
XWORK_API xwork_run_state xwork_run_get_state(const xwork_run *pRun);
```

**参数：**

- `pRun`：输入参数。可为 `NULL`。

**返回值：**

返回当前状态；`pRun` 为 `NULL` 时返回 `XWORK_RUN_FAILED`。

**资源归属：**

不返回指针，不转移所有权。

**补充说明：**

- 状态只表示 run 当前内存状态，不等同于 persistence 中的历史状态。

**范例代码：**

```c
xwork_run_state state = xwork_run_get_state(run);
```

**相关 API：**

- `xwork_run_start`
- `xwork_run_complete`

---

### xwork_run_get_autonomy

获取 run 自主性模式。

**功能：**

读取 run 创建时配置或 profile 应用后的 autonomy。

**函数原型：**

```c
XWORK_API xwork_autonomy_mode xwork_run_get_autonomy(const xwork_run *pRun);
```

**参数：**

- `pRun`：输入参数。可为 `NULL`。

**返回值：**

返回 run autonomy；`pRun` 为 `NULL` 时返回 `XWORK_AUTONOMY_MANUAL`。

**资源归属：**

不返回指针。

**补充说明：**

- autonomy 会影响 policy/orchestrator 的默认行为，但不替代产品级安全策略。

**范例代码：**

```c
xwork_autonomy_mode mode = xwork_run_get_autonomy(run);
```

**相关 API：**

- `xwork_profile_apply_run_options`

---

### xwork_run_get_workspace_count

获取 run 引用的 workspace 数量。

**功能：**

用于遍历 run 绑定的 workspace id。

**函数原型：**

```c
XWORK_API size_t xwork_run_get_workspace_count(const xwork_run *pRun);
```

**参数：**

- `pRun`：输入参数。可为 `NULL`。

**返回值：**

返回 workspace id 数量；`pRun` 为 `NULL` 时返回 `0`。

**资源归属：**

不返回指针。

**补充说明：**

- 返回的是 run 创建时复制的 workspace id 数量。

**范例代码：**

```c
size_t count = xwork_run_get_workspace_count(run);
```

**相关 API：**

- `xwork_run_get_workspace_id`

---

### xwork_run_get_workspace_id

按索引获取 run 的 workspace id。

**功能：**

用于遍历 run 引用的 workspace id，并解析到 runtime workspace。

**函数原型：**

```c
XWORK_API const char *xwork_run_get_workspace_id(const xwork_run *pRun, size_t iIndex);
```

**参数：**

- `pRun`：输入参数。可为 `NULL`。
- `iIndex`：输入参数。0-based 索引，必须小于 workspace count。

**返回值：**

返回借用 workspace id；参数无效或越界时返回 `NULL`。

**资源归属：**

返回值由 run 拥有，调用者不能释放。

**补充说明：**

- 可用 `xwork_runtime_find_workspace` 解析该 id。

**范例代码：**

```c
const char *workspace_id = xwork_run_get_workspace_id(run, 0u);
```

**相关 API：**

- `xwork_run_get_workspace_count`
- `xwork_runtime_find_workspace`

---

### xwork_run_get_summary

获取 run summary。

**功能：**

复制 run 的摘要信息，供 UI、日志或 persistence 使用。

**函数原型：**

```c
XWORK_API xwork_status xwork_run_get_summary(const xwork_run *pRun, xwork_run_summary *pSummary);
```

**参数：**

- `pRun`：输入参数。必须非 `NULL`。
- `pSummary`：输出参数。必须非 `NULL`，建议已 init。

**返回值：**

- `XWORK_OK`：复制成功。
- `XWORK_ERROR_INVALID_ARGUMENT`：参数无效。
- `XWORK_ERROR_NO_MEMORY`：字符串复制失败。

**资源归属：**

`pSummary` 接收 owned 字符串副本，调用者必须 `xwork_run_summary_reset`。

**补充说明：**

- 调用前若 summary 已持有数据，函数会重置并覆盖。

**范例代码：**

```c
xwork_run_summary summary;
xwork_run_summary_init(&summary);
if (xwork_run_get_summary(run, &summary) == XWORK_OK) {
    xwork_run_summary_reset(&summary);
}
```

**相关 API：**

- `xwork_run_summary_init`
- `xwork_run_summary_reset`

---

### xwork_run_get_snapshot

获取 run snapshot。

**功能：**

复制 run 的可序列化状态，用于持久化、恢复或测试。

**函数原型：**

```c
XWORK_API xwork_status xwork_run_get_snapshot(
    const xwork_run *pRun,
    xwork_run_snapshot *pSnapshot
);
```

**参数：**

- `pRun`：输入参数。必须非 `NULL`。
- `pSnapshot`：输出参数。必须非 `NULL`，建议已 init。

**返回值：**

- `XWORK_OK`：复制成功。
- `XWORK_ERROR_INVALID_ARGUMENT`：参数无效。
- `XWORK_ERROR_NO_MEMORY`：复制失败。

**资源归属：**

snapshot 获得 owned 字段，调用者必须 `xwork_run_snapshot_reset`。

**补充说明：**

- snapshot 不包含 live model session、线程、进程和 callback 栈。

**范例代码：**

```c
xwork_run_snapshot snapshot;
xwork_run_snapshot_init(&snapshot);
xwork_run_get_snapshot(run, &snapshot);
xwork_run_snapshot_reset(&snapshot);
```

**相关 API：**

- `xwork_run_snapshot_init`
- `xwork_runtime_recover_run`

---

### xwork_run_get_last_event

获取最后一个 event。

**功能：**

读取 run 最近记录的事件。

**函数原型：**

```c
XWORK_API xwork_status xwork_run_get_last_event(const xwork_run *pRun, xwork_event *pEvent);
```

**参数：**

- `pRun`：输入参数。必须非 `NULL`。
- `pEvent`：输出参数。必须非 `NULL`。

**返回值：**

- `XWORK_OK`：复制成功。
- `XWORK_ERROR_INVALID_ARGUMENT`：参数无效。
- `XWORK_ERROR_NOT_FOUND`：尚无 event。
- `XWORK_ERROR_NO_MEMORY`：复制失败。

**资源归属：**

event 接收 owned 字段，调用者必须 `xwork_event_reset`。

**补充说明：**

- 创建 run 后通常已有 `RUN_CREATED` 事件。

**范例代码：**

```c
xwork_event event;
xwork_event_init(&event);
xwork_run_get_last_event(run, &event);
xwork_event_reset(&event);
```

**相关 API：**

- `xwork_run_get_event`
- `xwork_event_reset`

---

### xwork_run_get_last_approval_request

获取最后一个 approval request。

**功能：**

读取最近一次审批请求，供 UI 展示或恢复审批流程。

**函数原型：**

```c
XWORK_API xwork_status xwork_run_get_last_approval_request(
    const xwork_run *pRun,
    xwork_approval_request *pRequest
);
```

**参数：**

- `pRun`：输入参数。必须非 `NULL`。
- `pRequest`：输出参数。必须非 `NULL`。

**返回值：**

- `XWORK_OK`：复制成功。
- `XWORK_ERROR_INVALID_ARGUMENT`：参数无效。
- `XWORK_ERROR_NOT_FOUND`：没有审批请求。
- `XWORK_ERROR_NO_MEMORY`：复制失败。

**资源归属：**

request 接收 owned 字段，调用者必须 `xwork_approval_request_reset`。

**补充说明：**

- 审批状态由 `xwork_run_submit_approval` 更新。

**范例代码：**

```c
xwork_approval_request request;
xwork_approval_request_init(&request);
xwork_run_get_last_approval_request(run, &request);
xwork_approval_request_reset(&request);
```

**相关 API：**

- `xwork_run_submit_approval`
- `xwork_approval_request_reset`

---

### xwork_run_get_last_checkpoint

获取最后一个 checkpoint。

**功能：**

读取最近一次 checkpoint，用于 UI、诊断或恢复提示。

**函数原型：**

```c
XWORK_API xwork_status xwork_run_get_last_checkpoint(
    const xwork_run *pRun,
    xwork_checkpoint *pCheckpoint
);
```

**参数：**

- `pRun`：输入参数。必须非 `NULL`。
- `pCheckpoint`：输出参数。必须非 `NULL`。

**返回值：**

- `XWORK_OK`：复制成功。
- `XWORK_ERROR_INVALID_ARGUMENT`：参数无效。
- `XWORK_ERROR_NOT_FOUND`：没有 checkpoint。
- `XWORK_ERROR_NO_MEMORY`：复制失败。

**资源归属：**

checkpoint 接收 owned 字段，调用者必须 `xwork_checkpoint_reset`。

**补充说明：**

- checkpoint 是可恢复状态，不代表 live 资源可恢复。

**范例代码：**

```c
xwork_checkpoint checkpoint;
xwork_checkpoint_init(&checkpoint);
xwork_run_get_last_checkpoint(run, &checkpoint);
xwork_checkpoint_reset(&checkpoint);
```

**相关 API：**

- `xwork_run_get_checkpoint`
- `xwork_checkpoint_reset`

---

### xwork_run_get_last_memory_context

获取最后一次 memory context。

**功能：**

读取 orchestrator 最近解析出的 memory context 文本和 workspace 数。

**函数原型：**

```c
XWORK_API xwork_status xwork_run_get_last_memory_context(
    const xwork_run *pRun,
    xwork_memory_context *pContext
);
```

**参数：**

- `pRun`：输入参数。必须非 `NULL`。
- `pContext`：输出参数。必须非 `NULL`，建议已 init。

**返回值：**

- `XWORK_OK`：复制成功。
- `XWORK_ERROR_INVALID_ARGUMENT`：参数无效。
- `XWORK_ERROR_NOT_FOUND`：没有 memory context。
- `XWORK_ERROR_NO_MEMORY`：复制失败。

**资源归属：**

context 接收 owned `sText` 副本，调用者必须 `xwork_memory_context_reset`。

**补充说明：**

- memory context 不等同于 workspace memory 对象本身。

**范例代码：**

```c
xwork_memory_context context;
xwork_memory_context_init(&context);
xwork_run_get_last_memory_context(run, &context);
xwork_memory_context_reset(&context);
```

**相关 API：**

- `xwork_memory_context_init`
- `xwork_memory_context_reset`

---

### xwork_run_get_event_count

获取 event 数量。

**功能：**

用于遍历 run event log。

**函数原型：**

```c
XWORK_API size_t xwork_run_get_event_count(const xwork_run *pRun);
```

**参数：**

- `pRun`：输入参数。可为 `NULL`。

**返回值：**

返回 event 数量；run 为空时返回 `0`。

**资源归属：**

不返回指针。

**补充说明：**

- event 索引为 0-based。

**范例代码：**

```c
size_t count = xwork_run_get_event_count(run);
```

**相关 API：**

- `xwork_run_get_event`

---

### xwork_run_get_event

按索引获取 event。

**功能：**

复制指定 event log 项。

**函数原型：**

```c
XWORK_API xwork_status xwork_run_get_event(
    const xwork_run *pRun,
    size_t iIndex,
    xwork_event *pEvent
);
```

**参数：**

- `pRun`：输入参数。必须非 `NULL`。
- `iIndex`：输入参数。0-based event 索引。
- `pEvent`：输出参数。必须非 `NULL`。

**返回值：**

- `XWORK_OK`：复制成功。
- `XWORK_ERROR_INVALID_ARGUMENT`：参数无效。
- `XWORK_ERROR_NOT_FOUND`：索引越界。
- `XWORK_ERROR_NO_MEMORY`：复制失败。

**资源归属：**

event 接收 owned 字段，调用者必须 reset。

**补充说明：**

- 遍历前先读取 `xwork_run_get_event_count`。

**范例代码：**

```c
xwork_event event;
xwork_event_init(&event);
xwork_run_get_event(run, 0u, &event);
xwork_event_reset(&event);
```

**相关 API：**

- `xwork_run_get_event_count`

---

### xwork_run_get_step_count

获取 step 数量。

**功能：**

返回可投影为 step 的 event 数量。

**函数原型：**

```c
XWORK_API size_t xwork_run_get_step_count(const xwork_run *pRun);
```

**参数：**

- `pRun`：输入参数。可为 `NULL`。

**返回值：**

返回 step 数量；run 为空时返回 `0`。

**资源归属：**

不返回指针。

**补充说明：**

- 当前实现以 event count 作为 step count。

**范例代码：**

```c
size_t count = xwork_run_get_step_count(run);
```

**相关 API：**

- `xwork_run_get_step`

---

### xwork_run_get_step

按索引获取 step。

**功能：**

把 event 和可关联 checkpoint 投影为 UI/查询友好的 step。

**函数原型：**

```c
XWORK_API xwork_status xwork_run_get_step(
    const xwork_run *pRun,
    size_t iIndex,
    xwork_run_step *pStep
);
```

**参数：**

- `pRun`：输入参数。必须非 `NULL`。
- `iIndex`：输入参数。0-based 索引。
- `pStep`：输出参数。必须非 `NULL`。

**返回值：**

- `XWORK_OK`：复制成功。
- `XWORK_ERROR_INVALID_ARGUMENT`：参数无效。
- `XWORK_ERROR_NOT_FOUND`：索引越界。
- `XWORK_ERROR_NO_MEMORY`：复制失败。

**资源归属：**

step 接收 owned 字段，调用者必须 reset。

**补充说明：**

- step 不是独立存储对象，而是由 event/checkpoint 派生。

**范例代码：**

```c
xwork_run_step step;
xwork_run_step_init(&step);
xwork_run_get_step(run, 0u, &step);
xwork_run_step_reset(&step);
```

**相关 API：**

- `xwork_run_get_step_count`
- `xwork_run_query_steps`

---

### xwork_run_query_steps

查询 step 列表。

**功能：**

按 query 条件过滤 run step，并返回列表。

**函数原型：**

```c
XWORK_API xwork_status xwork_run_query_steps(
    const xwork_run *pRun,
    const xwork_run_step_query *pQuery,
    xwork_run_step_list *pList
);
```

**参数：**

- `pRun`：输入参数。必须非 `NULL`。
- `pQuery`：输入参数。可为 `NULL`；为 `NULL` 时不过滤。
- `pList`：输出参数。必须非 `NULL`，建议已 init。

**返回值：**

- `XWORK_OK`：查询成功。
- `XWORK_ERROR_INVALID_ARGUMENT`：参数无效。
- `XWORK_ERROR_NO_MEMORY`：列表分配或字段复制失败。

**资源归属：**

list 接收 owned `pItems` 和每个 step 字段，调用者必须 `xwork_run_step_list_reset`。

**补充说明：**

- `iLimit` 命中时会设置 `bHasMore` 和 `iNextAfterSequence`。

**范例代码：**

```c
xwork_run_step_query query;
xwork_run_step_list list;
xwork_run_step_query_init(&query);
xwork_run_step_list_init(&list);
query.iLimit = 10u;
xwork_run_query_steps(run, &query, &list);
xwork_run_step_list_reset(&list);
```

**相关 API：**

- `xwork_run_step_query_init`
- `xwork_run_step_list_reset`

---

### xwork_run_get_checkpoint_count

获取 checkpoint 数量。

**功能：**

用于遍历 run 已记录 checkpoint。

**函数原型：**

```c
XWORK_API size_t xwork_run_get_checkpoint_count(const xwork_run *pRun);
```

**参数：**

- `pRun`：输入参数。可为 `NULL`。

**返回值：**

返回 checkpoint 数量；run 为空时返回 `0`。

**资源归属：**

不返回指针。

**补充说明：**

- checkpoint 索引为 0-based。

**范例代码：**

```c
size_t count = xwork_run_get_checkpoint_count(run);
```

**相关 API：**

- `xwork_run_get_checkpoint`

---

### xwork_run_get_checkpoint

按索引获取 checkpoint。

**功能：**

复制 run 中指定 checkpoint。

**函数原型：**

```c
XWORK_API xwork_status xwork_run_get_checkpoint(
    const xwork_run *pRun,
    size_t iIndex,
    xwork_checkpoint *pCheckpoint
);
```

**参数：**

- `pRun`：输入参数。必须非 `NULL`。
- `iIndex`：输入参数。0-based checkpoint 索引。
- `pCheckpoint`：输出参数。必须非 `NULL`。

**返回值：**

- `XWORK_OK`：复制成功。
- `XWORK_ERROR_INVALID_ARGUMENT`：参数无效。
- `XWORK_ERROR_NOT_FOUND`：索引越界。
- `XWORK_ERROR_NO_MEMORY`：复制失败。

**资源归属：**

checkpoint 接收 owned 字段，调用者必须 reset。

**补充说明：**

- 若已知 checkpoint id，可用 `xwork_run_load_checkpoint` 选择恢复目标。

**范例代码：**

```c
xwork_checkpoint checkpoint;
xwork_checkpoint_init(&checkpoint);
xwork_run_get_checkpoint(run, 0u, &checkpoint);
xwork_checkpoint_reset(&checkpoint);
```

**相关 API：**

- `xwork_run_get_checkpoint_count`
- `xwork_run_load_checkpoint`

---

### xwork_run_get_artifact_count

获取 artifact 数量。

**功能：**

用于遍历 run 内已发出的 artifact。

**函数原型：**

```c
XWORK_API size_t xwork_run_get_artifact_count(const xwork_run *pRun);
```

**参数：**

- `pRun`：输入参数。可为 `NULL`。

**返回值：**

返回 artifact 数量；run 为空时返回 `0`。

**资源归属：**

不返回指针。

**补充说明：**

- artifact 发出 API 在 artifact 文档中继续说明。

**范例代码：**

```c
size_t count = xwork_run_get_artifact_count(run);
```

**相关 API：**

- `xwork_run_get_artifact`
- `xwork_run_emit_artifact`

---

### xwork_run_get_artifact

按索引获取 artifact。

**功能：**

复制 run 中指定 artifact。

**函数原型：**

```c
XWORK_API xwork_status xwork_run_get_artifact(
    const xwork_run *pRun,
    size_t iIndex,
    xwork_artifact *pArtifact
);
```

**参数：**

- `pRun`：输入参数。必须非 `NULL`。
- `iIndex`：输入参数。0-based artifact 索引。
- `pArtifact`：输出参数。必须非 `NULL`，建议已 init。

**返回值：**

- `XWORK_OK`：复制成功。
- `XWORK_ERROR_INVALID_ARGUMENT`：参数无效。
- `XWORK_ERROR_NOT_FOUND`：索引越界。
- `XWORK_ERROR_NO_MEMORY`：复制失败。

**资源归属：**

artifact 接收 owned 字段，调用者必须 `xwork_artifact_reset`。

**补充说明：**

- 大内容是否内联取决于 artifact 创建选项和 persistence backend。

**范例代码：**

```c
xwork_artifact artifact;
xwork_artifact_init(&artifact);
xwork_run_get_artifact(run, 0u, &artifact);
xwork_artifact_reset(&artifact);
```

**相关 API：**

- `xwork_run_get_artifact_count`
- `xwork_artifact_reset`

---

### xwork_run_get_last_output_text

获取最后一次模型输出文本。

**功能：**

读取 run 中记录的最后 output text，适合 UI 快速展示。

**函数原型：**

```c
XWORK_API const char *xwork_run_get_last_output_text(const xwork_run *pRun);
```

**参数：**

- `pRun`：输入参数。可为 `NULL`。

**返回值：**

返回借用字符串；run 为空或尚无输出时返回 `NULL`。

**资源归属：**

返回值由 run 拥有，调用者不能释放。

**补充说明：**

- 如需持久化完整输出，应使用 artifact 或 persistence API。

**范例代码：**

```c
const char *text = xwork_run_get_last_output_text(run);
```

**相关 API：**

- `xwork_run_execute`
- `xwork_run_emit_output_artifact`

---

### xwork_run_execute

同步执行 run。

**功能：**

运行最小 model-turn + tool-loop orchestrator，直到完成、暂停、取消或失败。

**函数原型：**

```c
XWORK_API xwork_status xwork_run_execute(
    xwork_run *pRun,
    const xwork_orchestrator_options *pOptions
);
```

**参数：**

- `pRun`：输入/输出参数。必须非 `NULL`，并且属于有效 runtime。
- `pOptions`：输入参数。可为 `NULL`；为 `NULL` 时使用默认 orchestrator options。

**返回值：**

- `XWORK_OK`：执行完成。
- `XWORK_ERROR_INVALID_ARGUMENT`：参数或 options 无效。
- `XWORK_ERROR_INVALID_STATE`：run 当前状态不可执行或重入执行。
- `XWORK_ERROR_PAUSED`：执行在审批/暂停边界停止。
- `XWORK_ERROR_CANCELLED`：协作式取消。
- `XWORK_ERROR_EXTERNAL_FAILURE`：模型、工具、host service 或 persistence 失败。

**资源归属：**

不转移 run 或 options 所有权。options 中 callback/user data/cancel token 由调用者管理。

**补充说明：**

- 同一 run 不允许并发执行。
- 执行过程中会记录 event、checkpoint、artifact 和 summary 状态。

**范例代码：**

```c
xwork_orchestrator_options options;
xwork_orchestrator_options_init(&options);
xwork_status status = xwork_run_execute(run, &options);
```

**相关 API：**

- `xwork_orchestrator_options_init`
- `xwork_run_execute_async`

---

### xwork_run_execute_async

异步执行 run。

**功能：**

在后台线程中执行 run，并返回 async handle 用于 wait、timeout、status 和 cancel。

**函数原型：**

```c
XWORK_API xwork_status xwork_run_execute_async(
    xwork_run *pRun,
    const xwork_orchestrator_options *pOptions,
    xwork_run_async **ppAsync
);
```

**参数：**

- `pRun`：输入/输出参数。必须非 `NULL` 且属于有效 runtime。
- `pOptions`：输入参数。可为 `NULL`；非 `NULL` 时 shallow-copy 到 async handle。
- `ppAsync`：输出参数。必须非 `NULL`。成功时接收 owned async handle。

**返回值：**

- `XWORK_OK`：后台执行已启动。
- `XWORK_ERROR_INVALID_ARGUMENT`：参数或 orchestrator options 无效。
- `XWORK_ERROR_NO_MEMORY`：handle 或 cancel token 分配失败。
- `XWORK_ERROR_UNSUPPORTED`：当前平台不支持线程入口。
- `XWORK_ERROR_EXTERNAL_FAILURE`：线程创建失败。

**资源归属：**

async handle 由调用者拥有，必须 `xwork_run_async_destroy`。如果未提供 cancel token，handle 会创建并拥有一个内部 token。

**补充说明：**

- options 为 shallow-copy，callback、user data、profile strings 和 caller-owned cancel token 必须活到 handle 完成或销毁。
- 不要对同一 run 同时启动多个 async/sync 执行。

**范例代码：**

```c
xwork_run_async *async = NULL;
if (xwork_run_execute_async(run, NULL, &async) == XWORK_OK) {
    (void)xwork_run_async_wait(async);
    xwork_run_async_destroy(async);
}
```

**相关 API：**

- `xwork_run_async_wait`
- `xwork_run_async_cancel`
- `xwork_run_async_destroy`

---

### xwork_run_async_wait

等待异步 run 完成。

**功能：**

阻塞直到 async handle 关联的后台执行结束，并返回 run 执行状态。

**函数原型：**

```c
XWORK_API xwork_status xwork_run_async_wait(xwork_run_async *pAsync);
```

**参数：**

- `pAsync`：输入/输出参数。必须非 `NULL`。

**返回值：**

- 返回后台 `xwork_run_execute` 的最终状态。
- `XWORK_ERROR_INVALID_ARGUMENT`：handle 为空。
- `XWORK_ERROR_EXTERNAL_FAILURE`：线程等待或状态读取异常。

**资源归属：**

不销毁 handle；调用者仍需 `xwork_run_async_destroy`。

**补充说明：**

- wait 可与 destroy 分开调用。

**范例代码：**

```c
xwork_status status = xwork_run_async_wait(async);
```

**相关 API：**

- `xwork_run_async_wait_timeout`
- `xwork_run_async_destroy`

---

### xwork_run_async_wait_timeout

带超时等待异步 run。

**功能：**

等待最多指定毫秒数，并通过 `pbCompleted` 告诉调用方是否完成。

**函数原型：**

```c
XWORK_API xwork_status xwork_run_async_wait_timeout(
    xwork_run_async *pAsync,
    size_t iTimeoutMs,
    bool *pbCompleted
);
```

**参数：**

- `pAsync`：输入/输出参数。必须非 `NULL`。
- `iTimeoutMs`：输入参数。超时时间，单位毫秒。
- `pbCompleted`：输出参数。必须非 `NULL`。

**返回值：**

- `XWORK_OK` 且 `*pbCompleted == false`：等待超时但不是错误。
- 完成时返回后台 run 最终状态，并设置 `*pbCompleted == true`。
- `XWORK_ERROR_INVALID_ARGUMENT`：参数无效。
- `XWORK_ERROR_EXTERNAL_FAILURE`：线程等待或状态读取失败。

**资源归属：**

不销毁 handle。

**补充说明：**

- 超时后可调用 `xwork_run_async_cancel`。

**范例代码：**

```c
bool completed = false;
xwork_status status = xwork_run_async_wait_timeout(async, 1000u, &completed);
```

**相关 API：**

- `xwork_run_async_cancel`
- `xwork_run_async_wait`

---

### xwork_run_async_get_status

读取 async handle 当前状态。

**功能：**

非阻塞读取后台执行状态和完成标志。

**函数原型：**

```c
XWORK_API xwork_status xwork_run_async_get_status(
    const xwork_run_async *pAsync,
    xwork_status *pStatus,
    bool *pbCompleted
);
```

**参数：**

- `pAsync`：输入参数。必须非 `NULL`。
- `pStatus`：输出参数。必须非 `NULL`。
- `pbCompleted`：输出参数。必须非 `NULL`。

**返回值：**

- `XWORK_OK`：读取成功。
- `XWORK_ERROR_INVALID_ARGUMENT`：参数无效。

**资源归属：**

不转移所有权。

**补充说明：**

- 返回状态在未完成时是当前记录状态，不代表最终结果。

**范例代码：**

```c
xwork_status run_status;
bool completed;
xwork_run_async_get_status(async, &run_status, &completed);
```

**相关 API：**

- `xwork_run_async_wait`

---

### xwork_run_async_cancel

请求取消异步 run。

**功能：**

通过 async handle 的 cancel token 和线程停止边界请求后台执行尽快停止。

**函数原型：**

```c
XWORK_API xwork_status xwork_run_async_cancel(
    xwork_run_async *pAsync,
    const char *sReason
);
```

**参数：**

- `pAsync`：输入/输出参数。必须非 `NULL`。
- `sReason`：输入参数。可为 `NULL`；为 `NULL` 时使用默认取消原因。

**返回值：**

- `XWORK_OK`：取消请求已发出或 handle 已完成。
- `XWORK_ERROR_INVALID_ARGUMENT`：handle 为空。

**资源归属：**

不销毁 handle；调用者仍需 wait/destroy。

**补充说明：**

- 取消是协作式的，实际停止依赖 model/tool/host service 检查 cancel token。

**范例代码：**

```c
xwork_run_async_cancel(async, "timeout");
```

**相关 API：**

- `xwork_run_async_wait`
- `xwork_run_cancel`

---

### xwork_run_async_destroy

销毁 async handle。

**功能：**

释放 async handle、线程对象、内部 cancel token 和锁资源。

**函数原型：**

```c
XWORK_API void xwork_run_async_destroy(xwork_run_async *pAsync);
```

**参数：**

- `pAsync`：输入/销毁参数。可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放 handle 拥有的资源；如果 handle 创建了内部 cancel token，也会销毁它。

**补充说明：**

- 若后台尚未完成，destroy 会先请求取消并等待线程结束。
- destroy 不销毁 run。

**范例代码：**

```c
xwork_run_async_destroy(async);
```

**相关 API：**

- `xwork_run_execute_async`
- `xwork_run_async_cancel`

## Artifact Emission API

以下 artifact emission 函数属于 run API 的发出入口，但 artifact 结构字段、schema、query 和详细语义在 [Artifact API](api-artifacts.md) 中说明。

### xwork_run_emit_artifact

发出通用 artifact。

**功能：**

把工具、模型或产品层产物挂到 run 上，并纳入 event/persistence 管线。

**函数原型：**

```c
XWORK_API xwork_status xwork_run_emit_artifact(
    xwork_run *pRun,
    const xwork_artifact_options *pOptions,
    xwork_artifact *pArtifact
);
```

**参数：**

- `pRun`：输入/输出参数。必须非 `NULL`。
- `pOptions`：输入参数。必须非 `NULL`，具体字段见 Artifact API。
- `pArtifact`：输出参数。可为 `NULL`；非 `NULL` 时接收 deep-copy artifact。

**返回值：**

- `XWORK_OK`：发出成功。
- `XWORK_ERROR_INVALID_ARGUMENT`：参数或 artifact options 无效。
- `XWORK_ERROR_NO_MEMORY`：复制/分配失败。
- `XWORK_ERROR_EXTERNAL_FAILURE`：persistence 写入失败。

**资源归属：**

run 保存 artifact 副本；`pArtifact` 若非 `NULL`，调用者需 `xwork_artifact_reset`。

**补充说明：**

- 该函数是各种 typed artifact 发出函数的基础路径。

**范例代码：**

```c
xwork_artifact_options options;
xwork_artifact_options_init(&options);
/* fill options */
(void)xwork_run_emit_artifact(run, &options, NULL);
```

**相关 API：**

- `xwork_artifact_options_init`
- `xwork_run_get_artifact`

---

### xwork_run_emit_patch_artifact

发出 patch artifact。

**功能：**

记录 patch、apply result 和文件级 patch summary。

**函数原型：**

```c
XWORK_API xwork_status xwork_run_emit_patch_artifact(
    xwork_run *pRun,
    const xwork_patch_artifact_options *pOptions,
    xwork_artifact *pArtifact
);
```

**参数：**

- `pRun`：必须非 `NULL`。
- `pOptions`：必须非 `NULL`，字段见 Artifact API。
- `pArtifact`：可为 `NULL`；非 `NULL` 时接收副本。

**返回值：**

返回 `XWORK_OK` 或 artifact 发出路径的错误码。

**资源归属：**

run 保存副本；输出 artifact 由调用者 reset。

**补充说明：**

- 适合 `filesystem.apply_patch` 或代码编辑工具记录结构化产物。

**范例代码：**

```c
xwork_patch_artifact_options options;
xwork_patch_artifact_options_init(&options);
(void)xwork_run_emit_patch_artifact(run, &options, NULL);
```

**相关 API：**

- `xwork_patch_artifact_options_init`
- `XWORK_PATCH_APPLY_RESULT_SCHEMA_V1`

---

### xwork_run_emit_report_artifact

发出 report artifact。

**功能：**

记录诊断、总结、测试报告或 replay report。

**函数原型：**

```c
XWORK_API xwork_status xwork_run_emit_report_artifact(
    xwork_run *pRun,
    const xwork_report_artifact_options *pOptions,
    xwork_artifact *pArtifact
);
```

**参数：**

- `pRun`：必须非 `NULL`。
- `pOptions`：必须非 `NULL`。
- `pArtifact`：可为 `NULL`。

**返回值：**

返回 `XWORK_OK` 或 artifact 发出路径的错误码。

**资源归属：**

run 保存副本；输出 artifact 由调用者 reset。

**补充说明：**

- 报告分类、subject ref 和 schema 详见 Artifact API。

**范例代码：**

```c
xwork_report_artifact_options options;
xwork_report_artifact_options_init(&options);
(void)xwork_run_emit_report_artifact(run, &options, NULL);
```

**相关 API：**

- `xwork_report_artifact_options_init`
- `XWORK_REPORT_SCHEMA_V1`

---

### xwork_run_emit_output_artifact

发出 output artifact。

**功能：**

记录模型输出、工具输出或用户可见内容。

**函数原型：**

```c
XWORK_API xwork_status xwork_run_emit_output_artifact(
    xwork_run *pRun,
    const xwork_output_artifact_options *pOptions,
    xwork_artifact *pArtifact
);
```

**参数：**

- `pRun`：必须非 `NULL`。
- `pOptions`：必须非 `NULL`。
- `pArtifact`：可为 `NULL`。

**返回值：**

返回 `XWORK_OK` 或 artifact 发出路径的错误码。

**资源归属：**

run 保存副本；输出 artifact 由调用者 reset。

**补充说明：**

- output role 和 content stats 详见 Artifact API。

**范例代码：**

```c
xwork_output_artifact_options options;
xwork_output_artifact_options_init(&options);
(void)xwork_run_emit_output_artifact(run, &options, NULL);
```

**相关 API：**

- `xwork_output_artifact_options_init`
- `xwork_run_get_last_output_text`

---

### xwork_run_emit_command_artifact

发出 command artifact。

**功能：**

记录命令执行、stdout/stderr 统计和 terminal/process 结果。

**函数原型：**

```c
XWORK_API xwork_status xwork_run_emit_command_artifact(
    xwork_run *pRun,
    const xwork_command_artifact_options *pOptions,
    xwork_artifact *pArtifact
);
```

**参数：**

- `pRun`：必须非 `NULL`。
- `pOptions`：必须非 `NULL`。
- `pArtifact`：可为 `NULL`。

**返回值：**

返回 `XWORK_OK` 或 artifact 发出路径的错误码。

**资源归属：**

run 保存副本；输出 artifact 由调用者 reset。

**补充说明：**

- 适合 process/terminal host tools 记录结构化执行结果。

**范例代码：**

```c
xwork_command_artifact_options options;
xwork_command_artifact_options_init(&options);
(void)xwork_run_emit_command_artifact(run, &options, NULL);
```

**相关 API：**

- `xwork_command_artifact_options_init`
- `XWORK_TERMINAL_STATE_SCHEMA_V1`

## 错误处理

- `XWORK_ERROR_INVALID_ARGUMENT`：run/options/output 参数无效。
- `XWORK_ERROR_NOT_FOUND`：workspace、event、checkpoint、artifact 或 persisted object 不存在。
- `XWORK_ERROR_INVALID_STATE`：生命周期状态不允许该操作，或同一 run 重入执行。
- `XWORK_ERROR_PAUSED`：执行在审批或 side-effect-blocking 等可恢复边界暂停。
- `XWORK_ERROR_CANCELLED`：协作式取消。
- `XWORK_ERROR_EXTERNAL_FAILURE`：模型、host service、thread、persistence 或底层依赖失败。

## 恢复边界

run snapshot 可以恢复 run 的可序列化状态、workspace id、pending tool、approval decision、last checkpoint、artifact metadata 等。live process、terminal session、线程、model session 和 callback 栈不会恢复。

## 线程边界

同一 run 不允许并发执行或并发 mutation。`xwork_run_async_*` 只同步 async handle 状态；run 自身的其他访问仍需调用方遵守生命周期边界。

## 相关文档

- [Orchestrator API](api-orchestrator.md)
- [Artifact API](api-artifacts.md)
- [持久化、checkpoint 与 replay](../guide/persistence-replay-intro.md)
- [第一个 xwork 程序](../guide/first-xwork-program.md)
