# Persistence API

> 状态：中文逐函数参考，待人工审阅。

Persistence API 负责把 run、event、checkpoint、artifact、agent pool、task graph、remote control plane 和 replay cassette 保存到 durable backend。

## 模块定位

Persistence 为 xwork 提供本地 durable agent run 能力。内置 file backend 不是分布式数据库，也不是多写者存储；如果需要远程 DB 或对象存储，应该实现自定义 `xwork_persistence_backend`。

## 本页覆盖声明

| 类别 | 声明 |
| --- | --- |
| 结构体 | `xwork_persistence_backend`, `xwork_file_persistence_options`, `xwork_file_persistence` |
| 函数 | 本页所有 `### xwork_*` 小节 |

## 格式版本

当前 `XWORK_PERSISTENCE_FORMAT_VERSION` 为 `14`。

读取规则：

- 当前或已支持的旧版本：按兼容默认值加载。
- 未知更新版本：返回 `XWORK_ERROR_UNSUPPORTED`。
- 损坏或不完整文件：返回 `XWORK_ERROR_EXTERNAL_FAILURE`。
- 对象不存在：返回 `XWORK_ERROR_NOT_FOUND`。

## 所有权规则

- runtime options 中的 `pPersistenceBackend` 会按值复制。
- backend callback 函数指针和 `pUserData` 是 borrowed，必须覆盖 runtime 生命周期。
- `xwork_file_persistence_configure_backend` 会复制 root path 到 store，并把 backend callback 表配置为指向该 store。
- 所有 list/load/query 输出结构由调用方拥有；填充后必须调用对应 reset。
- recover API 返回的 pool/graph/control plane/run 为 owned object，调用方负责销毁。

## 常见错误码

- `XWORK_ERROR_INVALID_ARGUMENT`：store、backend、runtime、run id、object id 或输出结构无效。
- `XWORK_ERROR_NOT_FOUND`：对象不存在。
- `XWORK_ERROR_UNSUPPORTED`：格式版本更新或能力不支持。
- `XWORK_ERROR_NO_MEMORY`：分配或 deep-copy 失败。
- `XWORK_ERROR_EXTERNAL_FAILURE`：文件 I/O、损坏记录或 backend callback 失败。

## 通用范例

```c
#include "xwork.h"

int configure_store(void) {
    xwork_file_persistence_options options;
    xwork_file_persistence store;
    xwork_persistence_backend backend;

    xwork_file_persistence_options_init(&options);
    xwork_file_persistence_init(&store);
    xwork_persistence_backend_init(&backend);

    options.sRootPath = ".xwork_store";
    if (xwork_file_persistence_configure_backend(&store, &options, &backend) != XWORK_OK) {
        return 1;
    }

    xwork_file_persistence_reset(&store);
    return 0;
}
```

---

### xwork_persistence_backend_init

初始化 persistence backend callback 表。

**功能：**

创建自定义 backend 或接收 file backend 配置前，将 callback 表清零。

**函数原型：**

```c
XWORK_API void xwork_persistence_backend_init(xwork_persistence_backend *pBackend);
```

**参数：**

- `pBackend`：输出参数。可为 `NULL`；非 `NULL` 时清零。

**返回值：**

无。

**资源归属：**

函数不分配资源。callback 与 `pUserData` 由调用方管理。

**补充说明：**

- runtime 会按值复制 backend 表。

**范例代码：**

```c
xwork_persistence_backend backend;
xwork_persistence_backend_init(&backend);
```

**相关 API：**

- `xwork_file_persistence_configure_backend`

---

### xwork_file_persistence_options_init

初始化 file persistence options。

**功能：**

准备配置内置 file backend。

**函数原型：**

```c
XWORK_API void xwork_file_persistence_options_init(
    xwork_file_persistence_options *pOptions
);
```

**参数：**

- `pOptions`：输出参数。可为 `NULL`；非 `NULL` 时清零。

**返回值：**

无。

**资源归属：**

函数不分配资源。`sRootPath` 由调用方借用提供，configure 时复制。

**补充说明：**

- `sRootPath` 是配置 file backend 的必填字段。

**范例代码：**

```c
xwork_file_persistence_options options;
xwork_file_persistence_options_init(&options);
options.sRootPath = ".xwork_store";
```

**相关 API：**

- `xwork_file_persistence_configure_backend`

---

### xwork_file_persistence_init

初始化 file persistence store。

**功能：**

准备一个 `xwork_file_persistence` 结构用于配置 file backend。

**函数原型：**

```c
XWORK_API void xwork_file_persistence_init(xwork_file_persistence *pStore);
```

**参数：**

- `pStore`：输出参数。可为 `NULL`；非 `NULL` 时清零。

**返回值：**

无。

**资源归属：**

函数不分配资源。configure 后 store 拥有 root path 副本。

**补充说明：**

- 使用完成后调用 `xwork_file_persistence_reset`。

**范例代码：**

```c
xwork_file_persistence store;
xwork_file_persistence_init(&store);
```

**相关 API：**

- `xwork_file_persistence_reset`

---

### xwork_file_persistence_reset

释放并重置 file persistence store。

**功能：**

释放 file backend store 持有的 root path，并回到 init 状态。

**函数原型：**

```c
XWORK_API void xwork_file_persistence_reset(xwork_file_persistence *pStore);
```

**参数：**

- `pStore`：输入/输出参数。可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放 store 拥有的 root path 副本。

**补充说明：**

- 不会删除磁盘上的 persistence 数据。

**范例代码：**

```c
xwork_file_persistence_reset(&store);
```

**相关 API：**

- `xwork_file_persistence_init`

---

### xwork_file_persistence_configure_backend

配置内置 file backend。

**功能：**

把 file store 绑定到 root path，并填充 `xwork_persistence_backend` callback 表供 runtime 使用。

**函数原型：**

```c
XWORK_API xwork_status xwork_file_persistence_configure_backend(
    xwork_file_persistence *pStore,
    const xwork_file_persistence_options *pOptions,
    xwork_persistence_backend *pBackend
);
```

**参数：**

- `pStore`：输入/输出参数。必须非 `NULL`。
- `pOptions`：输入参数。必须非 `NULL`，且 `sRootPath` 必须为非空字符串。
- `pBackend`：输出参数。必须非 `NULL`。成功时接收 callback 表。

**返回值：**

返回 `XWORK_OK` 或通用 persistence 错误码。

**资源归属：**

store 拥有 root path 副本；backend callback 表借用 store 作为 user data，store 必须覆盖 runtime 使用期。

**补充说明：**

- 函数会创建必要目录。
- 重新配置会先 reset store/backend。

**范例代码：**

```c
xwork_file_persistence_configure_backend(&store, &options, &backend);
```

**相关 API：**

- `xwork_runtime_create`

---

### xwork_file_persistence_list_runs

列出持久化 run id。

**函数原型：**

```c
XWORK_API xwork_status xwork_file_persistence_list_runs(
    const xwork_file_persistence *pStore,
    xwork_string_list *pList
);
```

**功能：**

扫描 file backend 中已保存的 run。

**参数：**

- `pStore`：输入参数。必须已配置。
- `pList`：输出参数。必须非 `NULL`，接收 owned 字符串列表。

**返回值：**

返回 `XWORK_OK` 或通用 persistence 错误码。

**资源归属：**

调用者必须用 `xwork_string_list_reset` 释放列表。

**补充说明：**

- 返回的是 run id，不加载 run 内容。

**范例代码：**

```c
xwork_string_list list;
xwork_string_list_init(&list);
xwork_file_persistence_list_runs(&store, &list);
xwork_string_list_reset(&list);
```

**相关 API：**

- `xwork_runtime_list_persisted_runs`

---

### xwork_file_persistence_list_run_summaries

列出所有 run summary。

**函数原型：**

```c
XWORK_API xwork_status xwork_file_persistence_list_run_summaries(
    const xwork_file_persistence *pStore,
    xwork_run_summary_list *pList
);
```

**功能：**

加载 file backend 中每个 run 的 summary。

**参数：**

- `pStore`：输入参数。必须已配置。
- `pList`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或通用 persistence 错误码。

**资源归属：**

调用者必须 `xwork_run_summary_list_reset`。

**补充说明：**

- 用于 run history UI。

**范例代码：**

```c
xwork_run_summary_list list;
xwork_run_summary_list_init(&list);
xwork_file_persistence_list_run_summaries(&store, &list);
xwork_run_summary_list_reset(&list);
```

**相关 API：**

- `xwork_runtime_list_persisted_run_summaries`

---

### xwork_file_persistence_list_run_index

列出 run index。

**函数原型：**

```c
XWORK_API xwork_status xwork_file_persistence_list_run_index(
    const xwork_file_persistence *pStore,
    xwork_run_index_list *pList
);
```

**功能：**

返回包含 summary 与 last objects 的 run index。

**参数：**

- `pStore`：输入参数。必须已配置。
- `pList`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或通用 persistence 错误码。

**资源归属：**

调用者必须 `xwork_run_index_list_reset`。

**补充说明：**

- 等价于无 query 条件的 run index 查询。

**范例代码：**

```c
xwork_run_index_list list;
xwork_run_index_list_init(&list);
xwork_file_persistence_list_run_index(&store, &list);
xwork_run_index_list_reset(&list);
```

**相关 API：**

- `xwork_file_persistence_query_run_index`

---

### xwork_file_persistence_query_run_index

按条件查询 run index。

**函数原型：**

```c
XWORK_API xwork_status xwork_file_persistence_query_run_index(
    const xwork_file_persistence *pStore,
    const xwork_run_index_query *pQuery,
    xwork_run_index_list *pList
);
```

**功能：**

按 run state、autonomy、last event/checkpoint/approval 等条件查询 run index。

**参数：**

- `pStore`：输入参数。必须已配置。
- `pQuery`：输入参数。可为 `NULL`。
- `pList`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或通用 persistence 错误码。

**资源归属：**

调用者必须 reset 输出列表。

**补充说明：**

- query 字符串字段为 borrowed。

**范例代码：**

```c
xwork_run_index_query query;
xwork_run_index_query_init(&query);
```

**相关 API：**

- `xwork_run_index_query_init`

---

### xwork_file_persistence_list_checkpoints

列出 run 的 checkpoint id。

**函数原型：**

```c
XWORK_API xwork_status xwork_file_persistence_list_checkpoints(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    xwork_string_list *pList
);
```

**功能：**

扫描指定 run 的 checkpoint 目录。

**参数：**

- `pStore`：输入参数。必须已配置。
- `sRunId`：输入参数。必须为非空 run id。
- `pList`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或通用 persistence 错误码。

**资源归属：**

调用者必须 `xwork_string_list_reset`。

**补充说明：**

- 只返回 id，不加载 checkpoint 内容。

**范例代码：**

```c
xwork_file_persistence_list_checkpoints(&store, "run-1", &list);
```

**相关 API：**

- `xwork_file_persistence_load_checkpoint`

---

### xwork_file_persistence_list_events

列出 run 的 event id。

**函数原型：**

```c
XWORK_API xwork_status xwork_file_persistence_list_events(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    xwork_string_list *pList
);
```

**功能：**

列出指定 run 的 event log 中可加载 event id。

**参数：**

- `pStore`：输入参数。必须已配置。
- `sRunId`：输入参数。必须非空。
- `pList`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或通用 persistence 错误码。

**资源归属：**

调用者 reset 列表。

**补充说明：**

- 用于审计历史遍历。

**范例代码：**

```c
xwork_file_persistence_list_events(&store, "run-1", &list);
```

**相关 API：**

- `xwork_file_persistence_load_event`

---

### xwork_file_persistence_list_artifacts

列出 run 的 artifact id。

**函数原型：**

```c
XWORK_API xwork_status xwork_file_persistence_list_artifacts(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    xwork_string_list *pList
);
```

**功能：**

列出指定 run 已保存 artifact。

**参数：**

- `pStore`：输入参数。必须已配置。
- `sRunId`：输入参数。必须非空。
- `pList`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或通用 persistence 错误码。

**资源归属：**

调用者 reset 列表。

**补充说明：**

- 只返回 id，不加载 artifact 内容。

**范例代码：**

```c
xwork_file_persistence_list_artifacts(&store, "run-1", &list);
```

**相关 API：**

- `xwork_file_persistence_load_artifact`

---

### xwork_file_persistence_list_artifact_summaries

列出 run 的 artifact summary。

**函数原型：**

```c
XWORK_API xwork_status xwork_file_persistence_list_artifact_summaries(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    xwork_artifact_summary_list *pList
);
```

**功能：**

加载指定 run 的 artifact metadata 列表。

**参数：**

- `pStore`：输入参数。必须已配置。
- `sRunId`：输入参数。必须非空。
- `pList`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或通用 persistence 错误码。

**资源归属：**

调用者必须 `xwork_artifact_summary_list_reset`。

**补充说明：**

- 不加载完整 content text。

**范例代码：**

```c
xwork_file_persistence_list_artifact_summaries(&store, "run-1", &summaries);
```

**相关 API：**

- `xwork_file_persistence_query_artifact_summaries`

---

### xwork_file_persistence_query_artifact_summaries

按条件查询 artifact summary。

**函数原型：**

```c
XWORK_API xwork_status xwork_file_persistence_query_artifact_summaries(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    const xwork_artifact_summary_query *pQuery,
    xwork_artifact_summary_list *pList
);
```

**功能：**

按 kind、output class、role、name、MIME、storage ref、exit code 和 sequence 查询 artifact summary。

**参数：**

- `pStore`：输入参数。必须已配置。
- `sRunId`：输入参数。必须非空。
- `pQuery`：输入参数。可为 `NULL`。
- `pList`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或通用 persistence 错误码。

**资源归属：**

调用者 reset 输出列表。

**补充说明：**

- 空 query 等同于 list summaries。

**范例代码：**

```c
xwork_artifact_summary_query query;
xwork_artifact_summary_query_init(&query);
```

**相关 API：**

- `xwork_artifact_summary_query_init`

---

### xwork_file_persistence_query_run_steps

查询持久化 run step。

**函数原型：**

```c
XWORK_API xwork_status xwork_file_persistence_query_run_steps(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    const xwork_run_step_query *pQuery,
    xwork_run_step_list *pList
);
```

**功能：**

从持久化 event/checkpoint 中生成 step 列表。

**参数：**

- `pStore`：输入参数。必须已配置。
- `sRunId`：输入参数。必须非空。
- `pQuery`：输入参数。可为 `NULL`。
- `pList`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或通用 persistence 错误码。

**资源归属：**

调用者必须 `xwork_run_step_list_reset`。

**补充说明：**

- step 是由 event/checkpoint 派生的查询投影。

**范例代码：**

```c
xwork_file_persistence_query_run_steps(&store, "run-1", NULL, &steps);
```

**相关 API：**

- `xwork_run_step_query_init`

---

### xwork_file_persistence_load_event

加载指定 event。

**函数原型：**

```c
XWORK_API xwork_status xwork_file_persistence_load_event(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    const char *sEventId,
    xwork_event *pEvent
);
```

**功能：**

从 file backend 加载指定 run 的指定 event。

**参数：**

- `pStore`：输入参数。必须已配置。
- `sRunId`：输入参数。必须非空。
- `sEventId`：输入参数。必须非空。
- `pEvent`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或通用 persistence 错误码。

**资源归属：**

event 接收 owned 字段，调用者 reset。

**补充说明：**

- 输出结构建议先 init。

**范例代码：**

```c
xwork_event event;
xwork_event_init(&event);
xwork_file_persistence_load_event(&store, "run-1", "event-1", &event);
xwork_event_reset(&event);
```

**相关 API：**

- `xwork_file_persistence_list_events`

---

### xwork_file_persistence_load_last_event

加载最后一个 event。

**函数原型：**

```c
XWORK_API xwork_status xwork_file_persistence_load_last_event(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    xwork_event *pEvent
);
```

**功能：**

加载指定 run 的最后一个 event。

**参数：**

- `pStore`：输入参数。必须已配置。
- `sRunId`：输入参数。必须非空。
- `pEvent`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或通用 persistence 错误码。

**资源归属：**

调用者 reset event。

**补充说明：**

- 没有 event 时返回 `XWORK_ERROR_NOT_FOUND`。

**范例代码：**

```c
xwork_file_persistence_load_last_event(&store, "run-1", &event);
```

**相关 API：**

- `xwork_runtime_load_persisted_last_event`

---

### xwork_file_persistence_load_run_snapshot

加载 latest run snapshot。

**函数原型：**

```c
XWORK_API xwork_status xwork_file_persistence_load_run_snapshot(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    xwork_run_snapshot *pSnapshot
);
```

**功能：**

加载指定 run 的 latest snapshot，用于恢复 run。

**参数：**

- `pStore`：输入参数。必须已配置。
- `sRunId`：输入参数。必须非空。
- `pSnapshot`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或通用 persistence 错误码。

**资源归属：**

snapshot 接收 owned 字段，调用者 reset。

**补充说明：**

- 恢复前必须先重新注册 workspace/tool/xllm/host service。

**范例代码：**

```c
xwork_run_snapshot snapshot;
xwork_run_snapshot_init(&snapshot);
xwork_file_persistence_load_run_snapshot(&store, "run-1", &snapshot);
xwork_run_snapshot_reset(&snapshot);
```

**相关 API：**

- `xwork_runtime_recover_run`

---

### xwork_file_persistence_load_checkpoint_snapshot

加载 checkpoint 对应 run snapshot。

**函数原型：**

```c
XWORK_API xwork_status xwork_file_persistence_load_checkpoint_snapshot(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    const char *sCheckpointId,
    xwork_run_snapshot *pSnapshot
);
```

**功能：**

加载指定 checkpoint 保存的 run snapshot。

**参数：**

- `pStore`：输入参数。必须已配置。
- `sRunId`：输入参数。必须非空。
- `sCheckpointId`：输入参数。必须非空。
- `pSnapshot`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或通用 persistence 错误码。

**资源归属：**

调用者 reset snapshot。

**补充说明：**

- 用于从历史 checkpoint 恢复。

**范例代码：**

```c
xwork_file_persistence_load_checkpoint_snapshot(&store, "run-1", "ckpt-1", &snapshot);
```

**相关 API：**

- `xwork_file_persistence_load_run_snapshot`

---

### xwork_file_persistence_store_task_graph_snapshot

保存 task graph snapshot。

**函数原型：**

```c
XWORK_API xwork_status xwork_file_persistence_store_task_graph_snapshot(
    const xwork_file_persistence *pStore,
    const xwork_task_graph_snapshot *pSnapshot
);
```

**功能：**

把 multi-agent task graph 状态写入 file backend。

**参数：**

- `pStore`：输入参数。必须已配置。
- `pSnapshot`：输入参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或通用 persistence 错误码。

**资源归属：**

函数读取 snapshot，不接管所有权。

**补充说明：**

- snapshot id 必须有效。

**范例代码：**

```c
xwork_file_persistence_store_task_graph_snapshot(&store, &snapshot);
```

**相关 API：**

- `xwork_file_persistence_load_task_graph_snapshot`

---

### xwork_file_persistence_load_task_graph_snapshot

加载 task graph snapshot。

**函数原型：**

```c
XWORK_API xwork_status xwork_file_persistence_load_task_graph_snapshot(
    const xwork_file_persistence *pStore,
    const char *sGraphId,
    xwork_task_graph_snapshot *pSnapshot
);
```

**功能：**

加载指定 task graph 的持久化状态。

**参数：**

- `pStore`：输入参数。必须已配置。
- `sGraphId`：输入参数。必须非空。
- `pSnapshot`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或通用 persistence 错误码。

**资源归属：**

调用者 reset snapshot。

**补充说明：**

- 恢复执行还需要 agent pool 和 runtime。

**范例代码：**

```c
xwork_file_persistence_load_task_graph_snapshot(&store, "graph-1", &snapshot);
```

**相关 API：**

- `xwork_file_persistence_recover_task_graph`

---

### xwork_file_persistence_store_agent_pool_snapshot

保存 agent pool snapshot。

**函数原型：**

```c
XWORK_API xwork_status xwork_file_persistence_store_agent_pool_snapshot(
    const xwork_file_persistence *pStore,
    const xwork_agent_pool_snapshot *pSnapshot
);
```

**功能：**

保存 agent pool 配置和 agent snapshot。

**参数：**

- `pStore`：输入参数。必须已配置。
- `pSnapshot`：输入参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或通用 persistence 错误码。

**资源归属：**

函数读取 snapshot，不接管所有权。

**补充说明：**

- 用于 multi-agent 恢复。

**范例代码：**

```c
xwork_file_persistence_store_agent_pool_snapshot(&store, &pool_snapshot);
```

**相关 API：**

- `xwork_file_persistence_load_agent_pool_snapshot`

---

### xwork_file_persistence_load_agent_pool_snapshot

加载 agent pool snapshot。

**函数原型：**

```c
XWORK_API xwork_status xwork_file_persistence_load_agent_pool_snapshot(
    const xwork_file_persistence *pStore,
    const char *sPoolId,
    xwork_agent_pool_snapshot *pSnapshot
);
```

**功能：**

加载指定 agent pool 的持久化配置。

**参数：**

- `pStore`：输入参数。必须已配置。
- `sPoolId`：输入参数。必须非空。
- `pSnapshot`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或通用 persistence 错误码。

**资源归属：**

调用者 reset snapshot。

**补充说明：**

- load 只返回数据，不创建 live pool。

**范例代码：**

```c
xwork_file_persistence_load_agent_pool_snapshot(&store, "pool-1", &snapshot);
```

**相关 API：**

- `xwork_agent_pool_create_from_snapshot`

---

### xwork_file_persistence_store_control_plane_snapshot

保存 control plane snapshot。

**函数原型：**

```c
XWORK_API xwork_status xwork_file_persistence_store_control_plane_snapshot(
    const xwork_file_persistence *pStore,
    const xwork_control_plane_snapshot *pSnapshot
);
```

**功能：**

保存 remote worker control plane 状态。

**参数：**

- `pStore`：输入参数。必须已配置。
- `pSnapshot`：输入参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或通用 persistence 错误码。

**资源归属：**

函数读取 snapshot，不接管所有权。

**补充说明：**

- 包含 worker、task、lease、output/blob chunk 摘要。

**范例代码：**

```c
xwork_file_persistence_store_control_plane_snapshot(&store, &snapshot);
```

**相关 API：**

- `xwork_file_persistence_load_control_plane_snapshot`

---

### xwork_file_persistence_load_control_plane_snapshot

加载 control plane snapshot。

**函数原型：**

```c
XWORK_API xwork_status xwork_file_persistence_load_control_plane_snapshot(
    const xwork_file_persistence *pStore,
    const char *sPlaneId,
    xwork_control_plane_snapshot *pSnapshot
);
```

**功能：**

加载 remote worker control plane 持久化状态。

**参数：**

- `pStore`：输入参数。必须已配置。
- `sPlaneId`：输入参数。必须非空。
- `pSnapshot`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或通用 persistence 错误码。

**资源归属：**

调用者 reset snapshot。

**补充说明：**

- load 只返回 snapshot，不启动 control plane。

**范例代码：**

```c
xwork_file_persistence_load_control_plane_snapshot(&store, "plane-1", &snapshot);
```

**相关 API：**

- `xwork_file_persistence_recover_control_plane`

---

### xwork_file_persistence_store_replay

保存 replay engine cassette。

**函数原型：**

```c
XWORK_API xwork_status xwork_file_persistence_store_replay(
    const xwork_file_persistence *pStore,
    const xwork_replay_engine *pEngine
);
```

**功能：**

保存 replay manifest、entries、events、filesystem refs 和 result。

**参数：**

- `pStore`：输入参数。必须已配置。
- `pEngine`：输入参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或通用 persistence 错误码。

**资源归属：**

函数读取 replay engine，不接管所有权。

**补充说明：**

- replay engine 的具体记录/回放能力见 Replay API。

**范例代码：**

```c
xwork_file_persistence_store_replay(&store, engine);
```

**相关 API：**

- `xwork_file_persistence_load_replay_engine`

---

### xwork_file_persistence_list_replays

列出 replay id。

**函数原型：**

```c
XWORK_API xwork_status xwork_file_persistence_list_replays(
    const xwork_file_persistence *pStore,
    xwork_string_list *pList
);
```

**功能：**

扫描 file backend 中已保存的 replay cassette。

**参数：**

- `pStore`：输入参数。必须已配置。
- `pList`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或通用 persistence 错误码。

**资源归属：**

调用者 reset 字符串列表。

**补充说明：**

- 只返回 replay id。

**范例代码：**

```c
xwork_file_persistence_list_replays(&store, &list);
```

**相关 API：**

- `xwork_file_persistence_load_replay_manifest`

---

### xwork_file_persistence_load_replay_manifest

加载 replay manifest。

**函数原型：**

```c
XWORK_API xwork_status xwork_file_persistence_load_replay_manifest(
    const xwork_file_persistence *pStore,
    const char *sReplayId,
    xwork_replay_manifest *pManifest
);
```

**功能：**

加载 replay 的 manifest 元数据。

**参数：**

- `pStore`：输入参数。必须已配置。
- `sReplayId`：输入参数。必须非空。
- `pManifest`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或通用 persistence 错误码。

**资源归属：**

调用者 reset manifest。

**补充说明：**

- 不加载 entry 列表。

**范例代码：**

```c
xwork_file_persistence_load_replay_manifest(&store, "replay-1", &manifest);
```

**相关 API：**

- `xwork_replay_manifest_reset`

---

### xwork_file_persistence_load_replay_entries

加载 replay entry summaries。

**函数原型：**

```c
XWORK_API xwork_status xwork_file_persistence_load_replay_entries(
    const xwork_file_persistence *pStore,
    const char *sReplayId,
    xwork_replay_entry_summary_list *pList
);
```

**功能：**

加载 replay cassette 的 entry summary 列表。

**参数：**

- `pStore`：输入参数。必须已配置。
- `sReplayId`：输入参数。必须非空。
- `pList`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或通用 persistence 错误码。

**资源归属：**

调用者 reset entry summary list。

**补充说明：**

- 只加载 summary，不加载完整 payload。

**范例代码：**

```c
xwork_file_persistence_load_replay_entries(&store, "replay-1", &entries);
```

**相关 API：**

- `xwork_replay_entry_summary_list_reset`

---

### xwork_file_persistence_load_replay_result

加载 replay result。

**函数原型：**

```c
XWORK_API xwork_status xwork_file_persistence_load_replay_result(
    const xwork_file_persistence *pStore,
    const char *sReplayId,
    xwork_replay_result *pResult
);
```

**功能：**

加载 replay 执行结果和首个 divergence。

**参数：**

- `pStore`：输入参数。必须已配置。
- `sReplayId`：输入参数。必须非空。
- `pResult`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或通用 persistence 错误码。

**资源归属：**

调用者 reset replay result。

**补充说明：**

- 用于 replay 历史 UI 或 CI gate。

**范例代码：**

```c
xwork_file_persistence_load_replay_result(&store, "replay-1", &result);
```

**相关 API：**

- `xwork_replay_result_reset`

---

### xwork_file_persistence_load_replay_engine

加载 replay engine。

**函数原型：**

```c
XWORK_API xwork_status xwork_file_persistence_load_replay_engine(
    const xwork_file_persistence *pStore,
    const char *sReplayId,
    const xwork_replay_options *pOptions,
    xwork_replay_engine **ppEngine
);
```

**功能：**

从保存的 replay cassette 构建 live replay engine。

**参数：**

- `pStore`：输入参数。必须已配置。
- `sReplayId`：输入参数。必须非空。
- `pOptions`：输入参数。可为 `NULL` 使用默认选项。
- `ppEngine`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或通用 persistence 错误码。

**资源归属：**

成功后 `*ppEngine` 由调用者拥有，必须 `xwork_replay_engine_destroy`。

**补充说明：**

- live replay engine 不是 snapshot，需要明确销毁。

**范例代码：**

```c
xwork_replay_engine *engine = NULL;
xwork_file_persistence_load_replay_engine(&store, "replay-1", NULL, &engine);
xwork_replay_engine_destroy(engine);
```

**相关 API：**

- `xwork_file_persistence_store_replay`

---

### xwork_file_persistence_recover_task_graph

恢复 agent pool 和 task graph。

**函数原型：**

```c
XWORK_API xwork_status xwork_file_persistence_recover_task_graph(
    const xwork_file_persistence *pStore,
    xwork_runtime *pRuntime,
    const char *sPoolId,
    const char *sGraphId,
    const xwork_task_graph_options *pExecutionOptions,
    xwork_agent_pool **ppPool,
    xwork_task_graph **ppGraph
);
```

**功能：**

从持久化 agent pool snapshot 与 task graph snapshot 创建 live 对象。

**参数：**

- `pStore`：输入参数。必须已配置。
- `pRuntime`：输入/输出参数。必须非 `NULL`。
- `sPoolId`：输入参数。必须非空。
- `sGraphId`：输入参数。必须非空。
- `pExecutionOptions`：输入参数。可为 `NULL`。
- `ppPool`：输出参数。必须非 `NULL`。
- `ppGraph`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或通用 persistence/multi-agent 错误码。

**资源归属：**

成功后 pool 和 graph 由调用者拥有，分别用对应 destroy 函数释放。

**补充说明：**

- runtime 必须已重新配置 workspace、tool、host service 和 xllm。

**范例代码：**

```c
xwork_file_persistence_recover_task_graph(&store, runtime, "pool-1", "graph-1", NULL, &pool, &graph);
```

**相关 API：**

- `xwork_task_graph_destroy`
- `xwork_agent_pool_destroy`

---

### xwork_file_persistence_recover_control_plane

恢复 remote control plane。

**函数原型：**

```c
XWORK_API xwork_status xwork_file_persistence_recover_control_plane(
    const xwork_file_persistence *pStore,
    xwork_runtime *pRuntime,
    const char *sPlaneId,
    const xwork_control_plane_options *pOptions,
    xwork_control_plane **ppPlane
);
```

**功能：**

从 control plane snapshot 创建 live control plane。

**参数：**

- `pStore`：输入参数。必须已配置。
- `pRuntime`：输入/输出参数。必须非 `NULL`。
- `sPlaneId`：输入参数。必须非空。
- `pOptions`：输入参数。可为 `NULL`。
- `ppPlane`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或通用 persistence/remote 错误码。

**资源归属：**

成功后 `*ppPlane` 由调用者拥有，必须 `xwork_control_plane_destroy`。

**补充说明：**

- 恢复不自动重连 worker 网络连接。

**范例代码：**

```c
xwork_file_persistence_recover_control_plane(&store, runtime, "plane-1", NULL, &plane);
```

**相关 API：**

- `xwork_control_plane_destroy`

---

### xwork_file_persistence_load_last_approval_request

加载最后一个 approval request。

**函数原型：**

```c
XWORK_API xwork_status xwork_file_persistence_load_last_approval_request(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    xwork_approval_request *pRequest
);
```

**功能：**

读取指定 run 最后记录的审批请求。

**参数：**

- `pStore`：输入参数。必须已配置。
- `sRunId`：输入参数。必须非空。
- `pRequest`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或通用 persistence 错误码。

**资源归属：**

调用者 reset request。

**补充说明：**

- 无审批请求时返回 `XWORK_ERROR_NOT_FOUND`。

**范例代码：**

```c
xwork_file_persistence_load_last_approval_request(&store, "run-1", &request);
```

**相关 API：**

- `xwork_runtime_load_persisted_last_approval_request`

---

### xwork_file_persistence_load_run_summary

加载 run summary。

**函数原型：**

```c
XWORK_API xwork_status xwork_file_persistence_load_run_summary(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    xwork_run_summary *pSummary
);
```

**功能：**

读取指定 run 的 summary。

**参数：**

- `pStore`：输入参数。必须已配置。
- `sRunId`：输入参数。必须非空。
- `pSummary`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或通用 persistence 错误码。

**资源归属：**

调用者 reset summary。

**补充说明：**

- summary 适合列表页，不包含完整 run snapshot。

**范例代码：**

```c
xwork_file_persistence_load_run_summary(&store, "run-1", &summary);
```

**相关 API：**

- `xwork_file_persistence_load_run_snapshot`

---

### xwork_file_persistence_load_checkpoint

加载 checkpoint。

**函数原型：**

```c
XWORK_API xwork_status xwork_file_persistence_load_checkpoint(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    const char *sCheckpointId,
    xwork_checkpoint *pCheckpoint
);
```

**功能：**

读取指定 run 的指定 checkpoint metadata。

**参数：**

- `pStore`：输入参数。必须已配置。
- `sRunId`：输入参数。必须非空。
- `sCheckpointId`：输入参数。必须非空。
- `pCheckpoint`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或通用 persistence 错误码。

**资源归属：**

调用者 reset checkpoint。

**补充说明：**

- 若要恢复 run state，使用 checkpoint snapshot。

**范例代码：**

```c
xwork_file_persistence_load_checkpoint(&store, "run-1", "ckpt-1", &checkpoint);
```

**相关 API：**

- `xwork_file_persistence_load_checkpoint_snapshot`

---

### xwork_file_persistence_load_last_checkpoint

加载最后一个 checkpoint。

**函数原型：**

```c
XWORK_API xwork_status xwork_file_persistence_load_last_checkpoint(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    xwork_checkpoint *pCheckpoint
);
```

**功能：**

读取指定 run 最后记录的 checkpoint。

**参数：**

- `pStore`：输入参数。必须已配置。
- `sRunId`：输入参数。必须非空。
- `pCheckpoint`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或通用 persistence 错误码。

**资源归属：**

调用者 reset checkpoint。

**补充说明：**

- 无 checkpoint 时返回 `XWORK_ERROR_NOT_FOUND`。

**范例代码：**

```c
xwork_file_persistence_load_last_checkpoint(&store, "run-1", &checkpoint);
```

**相关 API：**

- `xwork_runtime_load_persisted_last_checkpoint`

---

### xwork_file_persistence_load_artifact

加载 artifact。

**函数原型：**

```c
XWORK_API xwork_status xwork_file_persistence_load_artifact(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    const char *sArtifactId,
    xwork_artifact *pArtifact
);
```

**功能：**

读取指定 run 的指定 artifact。

**参数：**

- `pStore`：输入参数。必须已配置。
- `sRunId`：输入参数。必须非空。
- `sArtifactId`：输入参数。必须非空。
- `pArtifact`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或通用 persistence 错误码。

**资源归属：**

调用者 reset artifact。

**补充说明：**

- content 是否内联取决于保存时的 artifact options。

**范例代码：**

```c
xwork_file_persistence_load_artifact(&store, "run-1", "artifact-1", &artifact);
```

**相关 API：**

- `xwork_file_persistence_list_artifacts`

---

### xwork_file_persistence_load_last_artifact

加载最后一个 artifact。

**函数原型：**

```c
XWORK_API xwork_status xwork_file_persistence_load_last_artifact(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    xwork_artifact *pArtifact
);
```

**功能：**

读取指定 run 最后记录的 artifact。

**参数：**

- `pStore`：输入参数。必须已配置。
- `sRunId`：输入参数。必须非空。
- `pArtifact`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或通用 persistence 错误码。

**资源归属：**

调用者 reset artifact。

**补充说明：**

- 无 artifact 时返回 `XWORK_ERROR_NOT_FOUND`。

**范例代码：**

```c
xwork_file_persistence_load_last_artifact(&store, "run-1", &artifact);
```

**相关 API：**

- `xwork_runtime_load_persisted_last_artifact`

---

### xwork_file_persistence_find_artifact_by_name

按名称查找 artifact。

**函数原型：**

```c
XWORK_API xwork_status xwork_file_persistence_find_artifact_by_name(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    const char *sArtifactName,
    xwork_artifact *pArtifact
);
```

**功能：**

在指定 run 的 artifact 中按 name 精确查找并加载。

**参数：**

- `pStore`：输入参数。必须已配置。
- `sRunId`：输入参数。必须非空。
- `sArtifactName`：输入参数。必须非空。
- `pArtifact`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或通用 persistence 错误码。

**资源归属：**

调用者 reset artifact。

**补充说明：**

- 多个同名 artifact 时返回实现定义的匹配项，建议业务层使用唯一 name。

**范例代码：**

```c
xwork_file_persistence_find_artifact_by_name(&store, "run-1", "final.md", &artifact);
```

**相关 API：**

- `xwork_runtime_find_persisted_artifact_by_name`

---

## Runtime Facade

Runtime facade 函数通过 runtime 当前配置的 `xwork_persistence_backend` 调用底层 backend。它们的输出所有权与对应 file backend 函数一致。

### xwork_runtime_list_persisted_runs

列出持久化 run id。

**函数原型：**

```c
XWORK_API xwork_status xwork_runtime_list_persisted_runs(
    const xwork_runtime *pRuntime,
    xwork_string_list *pList
);
```

**参数：**

- `pRuntime`：输入参数。必须配置 persistence backend。
- `pList`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或 backend 错误码。

**资源归属：**

调用者 reset list。

**补充说明：**

- facade 不关心 backend 是 file 还是自定义实现。

**范例代码：**

```c
xwork_runtime_list_persisted_runs(runtime, &list);
```

**相关 API：**

- `xwork_file_persistence_list_runs`

---

### xwork_runtime_list_persisted_checkpoints

列出持久化 checkpoint id。

**函数原型：**

```c
XWORK_API xwork_status xwork_runtime_list_persisted_checkpoints(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_string_list *pList
);
```

**参数：**

- `pRuntime`：输入参数。必须配置 backend。
- `sRunId`：输入参数。必须非空。
- `pList`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或 backend 错误码。

**资源归属：**

调用者 reset list。

**补充说明：**

- 仅列出 id。

**范例代码：**

```c
xwork_runtime_list_persisted_checkpoints(runtime, "run-1", &list);
```

**相关 API：**

- `xwork_runtime_load_persisted_checkpoint`

---

### xwork_runtime_list_persisted_events

列出持久化 event id。

**函数原型：**

```c
XWORK_API xwork_status xwork_runtime_list_persisted_events(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_string_list *pList
);
```

**参数：**

- `pRuntime`：输入参数。必须配置 backend。
- `sRunId`：输入参数。必须非空。
- `pList`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或 backend 错误码。

**资源归属：**

调用者 reset list。

**补充说明：**

- 仅列出 id。

**范例代码：**

```c
xwork_runtime_list_persisted_events(runtime, "run-1", &list);
```

**相关 API：**

- `xwork_runtime_load_persisted_event`

---

### xwork_runtime_list_persisted_artifacts

列出持久化 artifact id。

**函数原型：**

```c
XWORK_API xwork_status xwork_runtime_list_persisted_artifacts(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_string_list *pList
);
```

**参数：**

- `pRuntime`：输入参数。必须配置 backend。
- `sRunId`：输入参数。必须非空。
- `pList`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或 backend 错误码。

**资源归属：**

调用者 reset list。

**补充说明：**

- 仅列出 id。

**范例代码：**

```c
xwork_runtime_list_persisted_artifacts(runtime, "run-1", &list);
```

**相关 API：**

- `xwork_runtime_load_persisted_artifact`

---

### xwork_runtime_list_persisted_artifact_summaries

列出持久化 artifact summary。

**函数原型：**

```c
XWORK_API xwork_status xwork_runtime_list_persisted_artifact_summaries(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_artifact_summary_list *pList
);
```

**参数：**

- `pRuntime`：输入参数。必须配置 backend。
- `sRunId`：输入参数。必须非空。
- `pList`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或 backend 错误码。

**资源归属：**

调用者 reset list。

**补充说明：**

- 不加载完整 content。

**范例代码：**

```c
xwork_runtime_list_persisted_artifact_summaries(runtime, "run-1", &summaries);
```

**相关 API：**

- `xwork_runtime_query_persisted_artifact_summaries`

---

### xwork_runtime_query_persisted_artifact_summaries

查询持久化 artifact summary。

**函数原型：**

```c
XWORK_API xwork_status xwork_runtime_query_persisted_artifact_summaries(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    const xwork_artifact_summary_query *pQuery,
    xwork_artifact_summary_list *pList
);
```

**参数：**

- `pRuntime`：输入参数。必须配置 backend。
- `sRunId`：输入参数。必须非空。
- `pQuery`：输入参数。可为 `NULL`。
- `pList`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或 backend 错误码。

**资源归属：**

调用者 reset list。

**补充说明：**

- 如果 backend 不提供原生 query，runtime 可回退到 list 后过滤。

**范例代码：**

```c
xwork_runtime_query_persisted_artifact_summaries(runtime, "run-1", NULL, &summaries);
```

**相关 API：**

- `xwork_artifact_summary_query_init`

---

### xwork_runtime_query_persisted_run_steps

查询持久化 run step。

**函数原型：**

```c
XWORK_API xwork_status xwork_runtime_query_persisted_run_steps(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    const xwork_run_step_query *pQuery,
    xwork_run_step_list *pList
);
```

**参数：**

- `pRuntime`：输入参数。必须配置 backend。
- `sRunId`：输入参数。必须非空。
- `pQuery`：输入参数。可为 `NULL`。
- `pList`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或 backend 错误码。

**资源归属：**

调用者 reset list。

**补充说明：**

- 如果 backend 不支持原生 query，runtime 可从 event/checkpoint 派生。

**范例代码：**

```c
xwork_runtime_query_persisted_run_steps(runtime, "run-1", NULL, &steps);
```

**相关 API：**

- `xwork_run_step_query_init`

---

### xwork_runtime_list_persisted_run_summaries

列出持久化 run summary。

**函数原型：**

```c
XWORK_API xwork_status xwork_runtime_list_persisted_run_summaries(
    const xwork_runtime *pRuntime,
    xwork_run_summary_list *pList
);
```

**参数：**

- `pRuntime`：输入参数。必须配置 backend。
- `pList`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或 backend 错误码。

**资源归属：**

调用者 reset list。

**补充说明：**

- 适合历史 run 列表 UI。

**范例代码：**

```c
xwork_runtime_list_persisted_run_summaries(runtime, &list);
```

**相关 API：**

- `xwork_runtime_list_persisted_run_index`

---

### xwork_runtime_list_persisted_run_index

列出持久化 run index。

**函数原型：**

```c
XWORK_API xwork_status xwork_runtime_list_persisted_run_index(
    const xwork_runtime *pRuntime,
    xwork_run_index_list *pList
);
```

**参数：**

- `pRuntime`：输入参数。必须配置 backend。
- `pList`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或 backend 错误码。

**资源归属：**

调用者 reset list。

**补充说明：**

- 等价于无 query 条件的 index 查询。

**范例代码：**

```c
xwork_runtime_list_persisted_run_index(runtime, &index);
```

**相关 API：**

- `xwork_runtime_query_persisted_run_index`

---

### xwork_runtime_query_persisted_run_index

查询持久化 run index。

**函数原型：**

```c
XWORK_API xwork_status xwork_runtime_query_persisted_run_index(
    const xwork_runtime *pRuntime,
    const xwork_run_index_query *pQuery,
    xwork_run_index_list *pList
);
```

**参数：**

- `pRuntime`：输入参数。必须配置 backend。
- `pQuery`：输入参数。可为 `NULL`。
- `pList`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或 backend 错误码。

**资源归属：**

调用者 reset list。

**补充说明：**

- query 字符串字段由调用方借用提供。

**范例代码：**

```c
xwork_runtime_query_persisted_run_index(runtime, NULL, &index);
```

**相关 API：**

- `xwork_run_index_query_init`

---

### xwork_runtime_load_persisted_run_summary

加载持久化 run summary。

**函数原型：**

```c
XWORK_API xwork_status xwork_runtime_load_persisted_run_summary(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_run_summary *pSummary
);
```

**参数：**

- `pRuntime`：输入参数。必须配置 backend。
- `sRunId`：输入参数。必须非空。
- `pSummary`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或 backend 错误码。

**资源归属：**

调用者 reset summary。

**补充说明：**

- 不恢复 live run。

**范例代码：**

```c
xwork_runtime_load_persisted_run_summary(runtime, "run-1", &summary);
```

**相关 API：**

- `xwork_runtime_recover_run_from_persistence`

---

### xwork_runtime_load_persisted_last_event

加载持久化最后 event。

**函数原型：**

```c
XWORK_API xwork_status xwork_runtime_load_persisted_last_event(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_event *pEvent
);
```

**参数：**

- `pRuntime`：输入参数。必须配置 backend。
- `sRunId`：输入参数。必须非空。
- `pEvent`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或 backend 错误码。

**资源归属：**

调用者 reset event。

**补充说明：**

- 无 event 时返回 not found。

**范例代码：**

```c
xwork_runtime_load_persisted_last_event(runtime, "run-1", &event);
```

**相关 API：**

- `xwork_runtime_load_persisted_event`

---

### xwork_runtime_load_persisted_last_approval_request

加载持久化最后 approval request。

**函数原型：**

```c
XWORK_API xwork_status xwork_runtime_load_persisted_last_approval_request(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_approval_request *pRequest
);
```

**参数：**

- `pRuntime`：输入参数。必须配置 backend。
- `sRunId`：输入参数。必须非空。
- `pRequest`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或 backend 错误码。

**资源归属：**

调用者 reset request。

**补充说明：**

- 用于恢复审批 UI。

**范例代码：**

```c
xwork_runtime_load_persisted_last_approval_request(runtime, "run-1", &request);
```

**相关 API：**

- `xwork_run_submit_approval`

---

### xwork_runtime_load_persisted_last_checkpoint

加载持久化最后 checkpoint。

**函数原型：**

```c
XWORK_API xwork_status xwork_runtime_load_persisted_last_checkpoint(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_checkpoint *pCheckpoint
);
```

**参数：**

- `pRuntime`：输入参数。必须配置 backend。
- `sRunId`：输入参数。必须非空。
- `pCheckpoint`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或 backend 错误码。

**资源归属：**

调用者 reset checkpoint。

**补充说明：**

- 只加载 checkpoint metadata。

**范例代码：**

```c
xwork_runtime_load_persisted_last_checkpoint(runtime, "run-1", &checkpoint);
```

**相关 API：**

- `xwork_runtime_load_persisted_checkpoint`

---

### xwork_runtime_load_persisted_last_artifact

加载持久化最后 artifact。

**函数原型：**

```c
XWORK_API xwork_status xwork_runtime_load_persisted_last_artifact(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_artifact *pArtifact
);
```

**参数：**

- `pRuntime`：输入参数。必须配置 backend。
- `sRunId`：输入参数。必须非空。
- `pArtifact`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或 backend 错误码。

**资源归属：**

调用者 reset artifact。

**补充说明：**

- 无 artifact 时返回 not found。

**范例代码：**

```c
xwork_runtime_load_persisted_last_artifact(runtime, "run-1", &artifact);
```

**相关 API：**

- `xwork_runtime_load_persisted_artifact`

---

### xwork_runtime_load_persisted_event

加载持久化 event。

**函数原型：**

```c
XWORK_API xwork_status xwork_runtime_load_persisted_event(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    const char *sEventId,
    xwork_event *pEvent
);
```

**参数：**

- `pRuntime`：输入参数。必须配置 backend。
- `sRunId`：输入参数。必须非空。
- `sEventId`：输入参数。必须非空。
- `pEvent`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或 backend 错误码。

**资源归属：**

调用者 reset event。

**补充说明：**

- 用于审计和 step 查询。

**范例代码：**

```c
xwork_runtime_load_persisted_event(runtime, "run-1", "event-1", &event);
```

**相关 API：**

- `xwork_runtime_list_persisted_events`

---

### xwork_runtime_load_persisted_checkpoint

加载持久化 checkpoint。

**函数原型：**

```c
XWORK_API xwork_status xwork_runtime_load_persisted_checkpoint(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    const char *sCheckpointId,
    xwork_checkpoint *pCheckpoint
);
```

**参数：**

- `pRuntime`：输入参数。必须配置 backend。
- `sRunId`：输入参数。必须非空。
- `sCheckpointId`：输入参数。必须非空。
- `pCheckpoint`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或 backend 错误码。

**资源归属：**

调用者 reset checkpoint。

**补充说明：**

- 只加载 checkpoint metadata。

**范例代码：**

```c
xwork_runtime_load_persisted_checkpoint(runtime, "run-1", "ckpt-1", &checkpoint);
```

**相关 API：**

- `xwork_runtime_load_persisted_last_checkpoint`

---

### xwork_runtime_load_persisted_artifact

加载持久化 artifact。

**函数原型：**

```c
XWORK_API xwork_status xwork_runtime_load_persisted_artifact(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    const char *sArtifactId,
    xwork_artifact *pArtifact
);
```

**参数：**

- `pRuntime`：输入参数。必须配置 backend。
- `sRunId`：输入参数。必须非空。
- `sArtifactId`：输入参数。必须非空。
- `pArtifact`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或 backend 错误码。

**资源归属：**

调用者 reset artifact。

**补充说明：**

- content 是否可用取决于 backend。

**范例代码：**

```c
xwork_runtime_load_persisted_artifact(runtime, "run-1", "artifact-1", &artifact);
```

**相关 API：**

- `xwork_runtime_find_persisted_artifact_by_name`

---

### xwork_runtime_find_persisted_artifact_by_name

按名称查找持久化 artifact。

**函数原型：**

```c
XWORK_API xwork_status xwork_runtime_find_persisted_artifact_by_name(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    const char *sArtifactName,
    xwork_artifact *pArtifact
);
```

**参数：**

- `pRuntime`：输入参数。必须配置 backend。
- `sRunId`：输入参数。必须非空。
- `sArtifactName`：输入参数。必须非空。
- `pArtifact`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或 backend 错误码。

**资源归属：**

调用者 reset artifact。

**补充说明：**

- 建议业务层保证 artifact name 唯一。

**范例代码：**

```c
xwork_runtime_find_persisted_artifact_by_name(runtime, "run-1", "final.md", &artifact);
```

**相关 API：**

- `xwork_runtime_load_persisted_artifact`

---

### xwork_runtime_recover_run

从 run snapshot 恢复 live run。

**函数原型：**

```c
XWORK_API xwork_status xwork_runtime_recover_run(
    xwork_runtime *pRuntime,
    const xwork_run_snapshot *pSnapshot,
    xwork_run **ppRun
);
```

**参数：**

- `pRuntime`：输入/输出参数。必须非 `NULL`。
- `pSnapshot`：输入参数。必须非 `NULL`。
- `ppRun`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或 run 恢复错误码。

**资源归属：**

成功后 run 附着到 runtime，由调用者显式 destroy 或 runtime destroy。

**补充说明：**

- 恢复前必须注册兼容 workspace/tool/xllm/host service。
- 不恢复 live process、terminal、thread 或 callback 栈。

**范例代码：**

```c
xwork_runtime_recover_run(runtime, &snapshot, &run);
```

**相关 API：**

- `xwork_file_persistence_load_run_snapshot`

---

### xwork_runtime_recover_run_from_persistence

从 persistence latest snapshot 恢复 live run。

**函数原型：**

```c
XWORK_API xwork_status xwork_runtime_recover_run_from_persistence(
    xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_run **ppRun
);
```

**参数：**

- `pRuntime`：输入/输出参数。必须配置 backend。
- `sRunId`：输入参数。必须非空。
- `ppRun`：输出参数。必须非 `NULL`。

**返回值：**

返回 `XWORK_OK` 或 backend/run 恢复错误码。

**资源归属：**

成功后 run 附着到 runtime。

**补充说明：**

- 这是加载 latest snapshot 后调用 `xwork_runtime_recover_run` 的便捷入口。

**范例代码：**

```c
xwork_runtime_recover_run_from_persistence(runtime, "run-1", &run);
```

**相关 API：**

- `xwork_runtime_recover_run`

## 恢复边界

可以恢复可序列化状态、workspace id、pending tool、approval decision、last checkpoint、artifact metadata、agent/task/worker/replay snapshot。不能恢复 live OS process handle、interactive terminal session、线程栈、网络连接、callback 栈或用户 UI 会话。

## 线程边界

内置 file backend 不设计为多进程/多写者数据库。对同一个 store root 的并发写入应由调用方串行化。runtime facade 的并发边界与底层 backend 一致。

## 相关文档

- [Run API](api-run.md)
- [Artifact API](api-artifacts.md)
- [Replay API](api-replay.md)
- [持久化、checkpoint 与 replay](../guide/persistence-replay-intro.md)
- [内部 persistence format](../../dev/docs/PERSISTENCE_FORMAT.md)
