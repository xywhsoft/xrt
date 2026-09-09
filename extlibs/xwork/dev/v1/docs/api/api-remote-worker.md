# Remote Worker API

Remote Worker API 提供 control plane、worker registry、lease、assignment queue、remote task、artifact blob chunk 和 output chunk 的公共对象。它用于把 agent 工作拆到本地或远端 worker 执行，同时保留可审计、可恢复的任务状态。

## 模块边界

- `XWORK_REMOTE_TRANSPORT_IN_PROCESS` 表示 control plane 和 worker 在同一进程内通过内存 API 交互。
- `XWORK_REMOTE_TRANSPORT_HTTP_BOUNDARY` 只定义解码后的控制平面边界；网络、认证、租户隔离、重试和 blob streaming 由宿主实现。
- control plane 不杀死 OS 进程，也不恢复 live terminal/process handle；snapshot 恢复时 assigned/running task 会转为 orphaned。
- control plane 和 worker 不提供并发 mutation 容器语义；start/stop/register/heartbeat/enqueue/claim/complete/fail/cancel/upload/query 应由调用方串行化。
- 当前远程协议版本为 `1`，对应 `XWORK_REMOTE_PROTOCOL_VERSION_CURRENT`。

## 所有权约定

| 对象 | 所有权 |
| --- | --- |
| `xwork_control_plane_create` | 返回 owned plane，用 `xwork_control_plane_destroy` 释放。 |
| `xwork_control_plane_register_worker` | worker 由 plane 拥有，返回 borrowed 指针。 |
| options/result/upload/chunk 输入 | API 调用时复制需要保留的字符串和数组；runtime 指针为 borrowed。 |
| summary/list/snapshot 输出 | 输出结构拥有 deep-copy 内容，使用匹配 `*_reset` 释放。 |
| assignment 输出 | 输出结构拥有 deep-copy 字段，使用 `xwork_remote_task_assignment_reset` 释放。 |

## 典型流程

```text
xwork_control_plane_options_init
xwork_control_plane_create
xwork_control_plane_start
xwork_worker_options_init
xwork_control_plane_register_worker
xwork_remote_task_options_init
xwork_control_plane_enqueue_task
xwork_control_plane_claim_task
xwork_control_plane_complete_task
xwork_control_plane_get_snapshot
xwork_control_plane_destroy
```

## 初始化与释放 API

### xwork_control_plane_options_init

初始化 control plane options。

**功能：**

设置 control plane 创建参数默认值。

**函数原型：**

```c
XWORK_API void xwork_control_plane_options_init(xwork_control_plane_options *pOptions);
```

**参数：**

- `pOptions`：要初始化的 options；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源。

**补充说明：**

默认 transport 为 in-process，protocol version 为当前版本。创建 plane 前必须设置非空 `sPlaneId`。

**范例代码：**

```c
xwork_control_plane_options opts;
xwork_control_plane_options_init(&opts);
opts.sPlaneId = "plane-1";
opts.pRuntime = runtime;
```

**相关 API：**

- `xwork_control_plane_create`

---

### xwork_worker_options_init

初始化 worker options。

**功能：**

准备 worker 注册参数。

**函数原型：**

```c
XWORK_API void xwork_worker_options_init(xwork_worker_options *pOptions);
```

**参数：**

- `pOptions`：要初始化的 options；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源。

**补充说明：**

默认 protocol version 为当前版本。`pRuntime` 可覆盖 plane runtime；否则 worker 使用 plane runtime。

**范例代码：**

```c
xwork_worker_options opts;
xwork_worker_options_init(&opts);
opts.sWorkerId = "worker-1";
opts.pRuntime = runtime;
```

**相关 API：**

- `xwork_control_plane_register_worker`

---

### xwork_worker_summary_init

初始化 worker summary。

**功能：**

准备 worker 查询结果结构。

**函数原型：**

```c
XWORK_API void xwork_worker_summary_init(xwork_worker_summary *pSummary);
```

**参数：**

- `pSummary`：要初始化的 summary；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源。

**补充说明：**

默认状态为 `XWORK_WORKER_REGISTERED`。

**范例代码：**

```c
xwork_worker_summary summary;
xwork_worker_summary_init(&summary);
```

**相关 API：**

- `xwork_control_plane_list_workers`

---

### xwork_worker_summary_reset

释放 worker summary。

**功能：**

释放 worker id、显示名、endpoint 等 deep-copy 字符串。

**函数原型：**

```c
XWORK_API void xwork_worker_summary_reset(xwork_worker_summary *pSummary);
```

**参数：**

- `pSummary`：要释放的 summary；可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放内部资源，不释放结构体本身。

**补充说明：**

调用后恢复为 init 状态。

**范例代码：**

```c
xwork_worker_summary_reset(&summary);
```

**相关 API：**

- `xwork_worker_summary_init`

---

### xwork_worker_summary_list_init

初始化 worker summary 列表。

**功能：**

准备一个空列表，用于接收 worker registry 查询结果。

**函数原型：**

```c
XWORK_API void xwork_worker_summary_list_init(xwork_worker_summary_list *pList);
```

**参数：**

- `pList`：要初始化的列表；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源。

**补充说明：**

调用 `xwork_control_plane_list_workers` 前应初始化。

**范例代码：**

```c
xwork_worker_summary_list list;
xwork_worker_summary_list_init(&list);
```

**相关 API：**

- `xwork_control_plane_list_workers`

---

### xwork_worker_summary_list_reset

释放 worker summary 列表。

**功能：**

释放列表中的所有 worker summary 和数组。

**函数原型：**

```c
XWORK_API void xwork_worker_summary_list_reset(xwork_worker_summary_list *pList);
```

**参数：**

- `pList`：要释放的列表；可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放列表拥有的内容。

**补充说明：**

释放后列表可复用。

**范例代码：**

```c
xwork_worker_summary_list_reset(&list);
```

**相关 API：**

- `xwork_worker_summary_reset`

---

### xwork_worker_snapshot_init

初始化 worker snapshot。

**功能：**

准备 worker snapshot，用于恢复 worker registry。

**函数原型：**

```c
XWORK_API void xwork_worker_snapshot_init(xwork_worker_snapshot *pSnapshot);
```

**参数：**

- `pSnapshot`：要初始化的 snapshot；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源。

**补充说明：**

默认 protocol version 为当前版本，默认状态为 registered。

**范例代码：**

```c
xwork_worker_snapshot snapshot;
xwork_worker_snapshot_init(&snapshot);
```

**相关 API：**

- `xwork_control_plane_get_snapshot`

---

### xwork_worker_snapshot_reset

释放 worker snapshot。

**功能：**

释放 worker snapshot 中的字符串、capability 数组和 label 数组。

**函数原型：**

```c
XWORK_API void xwork_worker_snapshot_reset(xwork_worker_snapshot *pSnapshot);
```

**参数：**

- `pSnapshot`：要释放的 snapshot；可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放内部 deep-copy 资源。

**补充说明：**

`xwork_control_plane_create_from_snapshot` 不接管 snapshot 所有权。

**范例代码：**

```c
xwork_worker_snapshot_reset(&snapshot);
```

**相关 API：**

- `xwork_worker_snapshot_init`

---

### xwork_worker_snapshot_list_init

初始化 worker snapshot 列表。

**功能：**

准备空 worker snapshot list。

**函数原型：**

```c
XWORK_API void xwork_worker_snapshot_list_init(xwork_worker_snapshot_list *pList);
```

**参数：**

- `pList`：要初始化的列表；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源。

**补充说明：**

该列表通常作为 `xwork_control_plane_snapshot.tWorkers` 使用。

**范例代码：**

```c
xwork_worker_snapshot_list list;
xwork_worker_snapshot_list_init(&list);
```

**相关 API：**

- `xwork_worker_snapshot_list_reset`

---

### xwork_worker_snapshot_list_reset

释放 worker snapshot 列表。

**功能：**

释放列表中每个 worker snapshot 和数组。

**函数原型：**

```c
XWORK_API void xwork_worker_snapshot_list_reset(xwork_worker_snapshot_list *pList);
```

**参数：**

- `pList`：要释放的列表；可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放列表拥有的内容。

**补充说明：**

`xwork_control_plane_snapshot_reset` 会间接调用它。

**范例代码：**

```c
xwork_worker_snapshot_list_reset(&list);
```

**相关 API：**

- `xwork_control_plane_snapshot_reset`

---

### xwork_remote_task_options_init

初始化 remote task options。

**功能：**

准备一个可入队的 remote task。

**函数原型：**

```c
XWORK_API void xwork_remote_task_options_init(xwork_remote_task_options *pOptions);
```

**参数：**

- `pOptions`：要初始化的 options；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源。

**补充说明：**

默认 task kind 为 host tool，默认 host service 为 process。入队前必须设置 task id 和 request JSON；host tool 任务还必须设置 operation id。

**范例代码：**

```c
xwork_remote_task_options opts;
xwork_remote_task_options_init(&opts);
opts.sTaskId = "task-1";
opts.sOperationId = XWORK_HOST_PROCESS_EXEC;
opts.sRequestJson = "{\"cmd\":\"echo hi\"}";
```

**相关 API：**

- `xwork_control_plane_enqueue_task`

---

### xwork_remote_task_summary_init

初始化 remote task summary。

**功能：**

准备 remote task 查询结果结构。

**函数原型：**

```c
XWORK_API void xwork_remote_task_summary_init(xwork_remote_task_summary *pSummary);
```

**参数：**

- `pSummary`：要初始化的 summary；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源。

**补充说明：**

默认 task kind 为 host tool，默认状态为 queued。

**范例代码：**

```c
xwork_remote_task_summary summary;
xwork_remote_task_summary_init(&summary);
```

**相关 API：**

- `xwork_control_plane_get_task_summary`

---

### xwork_remote_task_summary_reset

释放 remote task summary。

**功能：**

释放 task summary 中的字符串、artifact summary 数组和 output chunk 数组。

**函数原型：**

```c
XWORK_API void xwork_remote_task_summary_reset(xwork_remote_task_summary *pSummary);
```

**参数：**

- `pSummary`：要释放的 summary；可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放 summary 内部拥有资源。

**补充说明：**

适用于 `get_task_summary` 和 `list_tasks` 填充的结果。

**范例代码：**

```c
xwork_remote_task_summary_reset(&summary);
```

**相关 API：**

- `xwork_remote_task_summary_init`

---

### xwork_remote_task_summary_list_init

初始化 remote task summary 列表。

**功能：**

准备空 task summary list。

**函数原型：**

```c
XWORK_API void xwork_remote_task_summary_list_init(xwork_remote_task_summary_list *pList);
```

**参数：**

- `pList`：要初始化的列表；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源。

**补充说明：**

调用 `xwork_control_plane_list_tasks` 前应初始化。

**范例代码：**

```c
xwork_remote_task_summary_list list;
xwork_remote_task_summary_list_init(&list);
```

**相关 API：**

- `xwork_control_plane_list_tasks`

---

### xwork_remote_task_summary_list_reset

释放 remote task summary 列表。

**功能：**

释放列表中所有 task summary 和数组。

**函数原型：**

```c
XWORK_API void xwork_remote_task_summary_list_reset(xwork_remote_task_summary_list *pList);
```

**参数：**

- `pList`：要释放的列表；可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放列表拥有的内容。

**补充说明：**

释放后列表可复用。

**范例代码：**

```c
xwork_remote_task_summary_list_reset(&list);
```

**相关 API：**

- `xwork_remote_task_summary_reset`

---

### xwork_remote_task_snapshot_init

初始化 remote task snapshot。

**功能：**

准备 task snapshot，用于 control plane 持久化和恢复。

**函数原型：**

```c
XWORK_API void xwork_remote_task_snapshot_init(xwork_remote_task_snapshot *pSnapshot);
```

**参数：**

- `pSnapshot`：要初始化的 snapshot；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源。

**补充说明：**

默认 protocol version 为当前版本，默认状态为 queued。

**范例代码：**

```c
xwork_remote_task_snapshot snapshot;
xwork_remote_task_snapshot_init(&snapshot);
```

**相关 API：**

- `xwork_control_plane_get_snapshot`

---

### xwork_remote_task_snapshot_reset

释放 remote task snapshot。

**功能：**

释放 task snapshot 中的字符串、artifact summary 数组和 output chunk 数组。

**函数原型：**

```c
XWORK_API void xwork_remote_task_snapshot_reset(xwork_remote_task_snapshot *pSnapshot);
```

**参数：**

- `pSnapshot`：要释放的 snapshot；可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放 snapshot 内部拥有资源。

**补充说明：**

恢复 API 不接管该 snapshot。

**范例代码：**

```c
xwork_remote_task_snapshot_reset(&snapshot);
```

**相关 API：**

- `xwork_remote_task_snapshot_init`

---

### xwork_remote_task_snapshot_list_init

初始化 remote task snapshot 列表。

**功能：**

准备空 task snapshot list。

**函数原型：**

```c
XWORK_API void xwork_remote_task_snapshot_list_init(xwork_remote_task_snapshot_list *pList);
```

**参数：**

- `pList`：要初始化的列表；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源。

**补充说明：**

该列表通常作为 `xwork_control_plane_snapshot.tTasks` 使用。

**范例代码：**

```c
xwork_remote_task_snapshot_list list;
xwork_remote_task_snapshot_list_init(&list);
```

**相关 API：**

- `xwork_remote_task_snapshot_list_reset`

---

### xwork_remote_task_snapshot_list_reset

释放 remote task snapshot 列表。

**功能：**

释放列表中所有 remote task snapshot 和数组。

**函数原型：**

```c
XWORK_API void xwork_remote_task_snapshot_list_reset(xwork_remote_task_snapshot_list *pList);
```

**参数：**

- `pList`：要释放的列表；可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放列表拥有的内容。

**补充说明：**

`xwork_control_plane_snapshot_reset` 会间接调用它。

**范例代码：**

```c
xwork_remote_task_snapshot_list_reset(&list);
```

**相关 API：**

- `xwork_control_plane_snapshot_reset`

---

### xwork_remote_task_assignment_init

初始化 remote task assignment。

**功能：**

准备 worker claim task 后的 assignment 输出结构。

**函数原型：**

```c
XWORK_API void xwork_remote_task_assignment_init(xwork_remote_task_assignment *pAssignment);
```

**参数：**

- `pAssignment`：要初始化的 assignment；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源。

**补充说明：**

调用 `xwork_control_plane_claim_task` 前应初始化。

**范例代码：**

```c
xwork_remote_task_assignment assignment;
xwork_remote_task_assignment_init(&assignment);
```

**相关 API：**

- `xwork_control_plane_claim_task`

---

### xwork_remote_task_assignment_reset

释放 remote task assignment。

**功能：**

释放 assignment 中的 task id、assignment id、worker id、request JSON 等字符串。

**函数原型：**

```c
XWORK_API void xwork_remote_task_assignment_reset(xwork_remote_task_assignment *pAssignment);
```

**参数：**

- `pAssignment`：要释放的 assignment；可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放 assignment 内部拥有资源。

**补充说明：**

worker 完成任务后仍应 reset assignment 输出。

**范例代码：**

```c
xwork_remote_task_assignment_reset(&assignment);
```

**相关 API：**

- `xwork_remote_task_assignment_init`

---

### xwork_remote_task_result_init

初始化 remote task result。

**功能：**

准备 worker 完成任务时提交的结果。

**函数原型：**

```c
XWORK_API void xwork_remote_task_result_init(xwork_remote_task_result *pResult);
```

**参数：**

- `pResult`：要初始化的 result；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源；`complete_task` 调用时复制需要保留的内容。

**补充说明：**

默认状态码为 `XWORK_OK`，默认 protocol version 为当前版本。

**范例代码：**

```c
xwork_remote_task_result result;
xwork_remote_task_result_init(&result);
result.sVisibleSummary = "done";
```

**相关 API：**

- `xwork_control_plane_complete_task`

---

### xwork_remote_output_chunk_init

初始化 remote output chunk。

**功能：**

准备 stdout/stderr 文本 chunk 上传请求。

**函数原型：**

```c
XWORK_API void xwork_remote_output_chunk_init(xwork_remote_output_chunk *pChunk);
```

**参数：**

- `pChunk`：要初始化的 chunk；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源；上传时 control plane 复制文本和元数据。

**补充说明：**

默认 stream 为 stdout。

**范例代码：**

```c
xwork_remote_output_chunk chunk;
xwork_remote_output_chunk_init(&chunk);
chunk.sTaskId = "task-1";
chunk.sWorkerId = "worker-1";
chunk.sText = "line\n";
```

**相关 API：**

- `xwork_control_plane_upload_output_chunk`

---

### xwork_remote_output_chunk_summary_init

初始化 output chunk summary。

**功能：**

准备 output chunk 查询/快照元素。

**函数原型：**

```c
XWORK_API void xwork_remote_output_chunk_summary_init(
    xwork_remote_output_chunk_summary *pSummary
);
```

**参数：**

- `pSummary`：要初始化的 summary；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源。

**补充说明：**

默认 stream 为 stdout。

**范例代码：**

```c
xwork_remote_output_chunk_summary summary;
xwork_remote_output_chunk_summary_init(&summary);
```

**相关 API：**

- `xwork_remote_output_chunk_summary_reset`

---

### xwork_remote_output_chunk_summary_reset

释放 output chunk summary。

**功能：**

释放 content hash 和文本内容。

**函数原型：**

```c
XWORK_API void xwork_remote_output_chunk_summary_reset(
    xwork_remote_output_chunk_summary *pSummary
);
```

**参数：**

- `pSummary`：要释放的 summary；可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放内部 deep-copy 字符串。

**补充说明：**

该结构也嵌套在 remote task summary/snapshot 中。

**范例代码：**

```c
xwork_remote_output_chunk_summary_reset(&summary);
```

**相关 API：**

- `xwork_remote_output_chunk_summary_init`

---

### xwork_remote_output_chunk_summary_list_init

初始化 output chunk summary 列表。

**功能：**

准备空 output chunk summary list。

**函数原型：**

```c
XWORK_API void xwork_remote_output_chunk_summary_list_init(
    xwork_remote_output_chunk_summary_list *pList
);
```

**参数：**

- `pList`：要初始化的列表；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源。

**补充说明：**

列表元素通常来自 task summary 或 snapshot。

**范例代码：**

```c
xwork_remote_output_chunk_summary_list list;
xwork_remote_output_chunk_summary_list_init(&list);
```

**相关 API：**

- `xwork_remote_output_chunk_summary_list_reset`

---

### xwork_remote_output_chunk_summary_list_reset

释放 output chunk summary 列表。

**功能：**

释放列表中所有 output chunk summary 和数组。

**函数原型：**

```c
XWORK_API void xwork_remote_output_chunk_summary_list_reset(
    xwork_remote_output_chunk_summary_list *pList
);
```

**参数：**

- `pList`：要释放的列表；可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放列表拥有的内容。

**补充说明：**

释放后可复用。

**范例代码：**

```c
xwork_remote_output_chunk_summary_list_reset(&list);
```

**相关 API：**

- `xwork_remote_output_chunk_summary_reset`

---

### xwork_remote_blob_chunk_summary_init

初始化 artifact blob chunk summary。

**功能：**

准备 artifact blob chunk 查询结果元素。

**函数原型：**

```c
XWORK_API void xwork_remote_blob_chunk_summary_init(
    xwork_remote_blob_chunk_summary *pSummary
);
```

**参数：**

- `pSummary`：要初始化的 summary；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源。

**补充说明：**

blob chunk 可保存二进制数据指针和大小；查询结果中的数据由 summary 持有。

**范例代码：**

```c
xwork_remote_blob_chunk_summary summary;
xwork_remote_blob_chunk_summary_init(&summary);
```

**相关 API：**

- `xwork_control_plane_list_artifact_blobs`

---

### xwork_remote_blob_chunk_summary_reset

释放 artifact blob chunk summary。

**功能：**

释放 task/assignment/worker/artifact/blob/hash 字符串和 chunk 数据副本。

**函数原型：**

```c
XWORK_API void xwork_remote_blob_chunk_summary_reset(
    xwork_remote_blob_chunk_summary *pSummary
);
```

**参数：**

- `pSummary`：要释放的 summary；可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放内部 deep-copy 资源。

**补充说明：**

调用后 summary 回到 init 状态。

**范例代码：**

```c
xwork_remote_blob_chunk_summary_reset(&summary);
```

**相关 API：**

- `xwork_remote_blob_chunk_summary_init`

---

### xwork_remote_blob_chunk_summary_list_init

初始化 artifact blob chunk summary 列表。

**功能：**

准备空 blob chunk summary list。

**函数原型：**

```c
XWORK_API void xwork_remote_blob_chunk_summary_list_init(
    xwork_remote_blob_chunk_summary_list *pList
);
```

**参数：**

- `pList`：要初始化的列表；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源。

**补充说明：**

调用 `xwork_control_plane_list_artifact_blobs` 前应初始化。

**范例代码：**

```c
xwork_remote_blob_chunk_summary_list list;
xwork_remote_blob_chunk_summary_list_init(&list);
```

**相关 API：**

- `xwork_control_plane_list_artifact_blobs`

---

### xwork_remote_blob_chunk_summary_list_reset

释放 artifact blob chunk summary 列表。

**功能：**

释放列表中所有 blob chunk summary 和数组。

**函数原型：**

```c
XWORK_API void xwork_remote_blob_chunk_summary_list_reset(
    xwork_remote_blob_chunk_summary_list *pList
);
```

**参数：**

- `pList`：要释放的列表；可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放列表拥有的内容。

**补充说明：**

`xwork_control_plane_snapshot_reset` 会释放 snapshot 内的 blob chunk 列表。

**范例代码：**

```c
xwork_remote_blob_chunk_summary_list_reset(&list);
```

**相关 API：**

- `xwork_remote_blob_chunk_summary_reset`

---

### xwork_remote_artifact_upload_init

初始化 remote artifact upload。

**功能：**

准备 artifact summary 与 blob chunk 上传请求。

**函数原型：**

```c
XWORK_API void xwork_remote_artifact_upload_init(xwork_remote_artifact_upload *pUpload);
```

**参数：**

- `pUpload`：要初始化的 upload；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源；上传时 control plane 复制 artifact summary 和 chunk 数据。

**补充说明：**

必须设置 task id、worker id、artifact summary；有 chunk 数据时 `pChunkData` 和 `iChunkSize` 必须匹配。

**范例代码：**

```c
xwork_remote_artifact_upload upload;
xwork_remote_artifact_upload_init(&upload);
upload.sTaskId = "task-1";
upload.sWorkerId = "worker-1";
upload.pArtifact = &artifactSummary;
```

**相关 API：**

- `xwork_control_plane_upload_artifact`

---

### xwork_control_plane_snapshot_init

初始化 control plane snapshot。

**功能：**

准备 control plane snapshot，用于保存 worker、task 和 blob chunk 状态。

**函数原型：**

```c
XWORK_API void xwork_control_plane_snapshot_init(xwork_control_plane_snapshot *pSnapshot);
```

**参数：**

- `pSnapshot`：要初始化的 snapshot；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源。

**补充说明：**

默认 transport 为 in-process，protocol version 为当前版本。

**范例代码：**

```c
xwork_control_plane_snapshot snapshot;
xwork_control_plane_snapshot_init(&snapshot);
```

**相关 API：**

- `xwork_control_plane_get_snapshot`

---

### xwork_control_plane_snapshot_reset

释放 control plane snapshot。

**功能：**

释放 plane id、worker snapshot list、remote task snapshot list 和 blob chunk list。

**函数原型：**

```c
XWORK_API void xwork_control_plane_snapshot_reset(xwork_control_plane_snapshot *pSnapshot);
```

**参数：**

- `pSnapshot`：要释放的 snapshot；可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放 snapshot 内部拥有资源。

**补充说明：**

恢复 API 不接管该 snapshot。

**范例代码：**

```c
xwork_control_plane_snapshot_reset(&snapshot);
```

**相关 API：**

- `xwork_control_plane_create_from_snapshot`

---

## Control Plane 生命周期

### xwork_control_plane_create

创建 control plane。

**功能：**

创建远程任务控制平面，保存 worker registry、任务队列和上传数据。

**函数原型：**

```c
XWORK_API xwork_status xwork_control_plane_create(
    const xwork_control_plane_options *pOptions,
    xwork_control_plane **ppPlane
);
```

**参数：**

- `pOptions`：创建参数；必须包含非空 `sPlaneId`。
- `ppPlane`：输出 owned plane。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

成功后 `*ppPlane` 归调用者所有，用 `xwork_control_plane_destroy` 释放；runtime 为 borrowed。

**补充说明：**

protocol version 必须受支持；allowed capability allowlist 会被复制。

**范例代码：**

```c
xwork_control_plane *plane = NULL;
xwork_control_plane_options opts;
xwork_control_plane_options_init(&opts);
opts.sPlaneId = "plane-1";
opts.pRuntime = runtime;
xwork_control_plane_create(&opts, &plane);
```

**相关 API：**

- `xwork_control_plane_destroy`
- `xwork_control_plane_start`

---

### xwork_control_plane_create_from_snapshot

从 snapshot 恢复 control plane。

**功能：**

重建 worker registry、task queue、result、output chunk 和 blob chunk。

**函数原型：**

```c
XWORK_API xwork_status xwork_control_plane_create_from_snapshot(
    const xwork_control_plane_options *pOptions,
    const xwork_control_plane_snapshot *pSnapshot,
    xwork_control_plane **ppPlane
);
```

**参数：**

- `pOptions`：可选恢复参数；可覆盖 runtime、plane id、transport、policy 等运行环境。
- `pSnapshot`：源 snapshot。
- `ppPlane`：输出 owned plane。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

成功后 plane 归调用者所有；snapshot 不被接管。

**补充说明：**

恢复时 assigned/running task 会标记为 `XWORK_REMOTE_TASK_ORPHANED`，状态码为 cancelled，并记录 orphaned 错误信息。

**范例代码：**

```c
xwork_control_plane *plane = NULL;
xwork_control_plane_create_from_snapshot(&opts, &snapshot, &plane);
```

**相关 API：**

- `xwork_control_plane_get_snapshot`

---

### xwork_control_plane_destroy

销毁 control plane。

**功能：**

释放 plane、worker registry、task records、blob chunks 和 capability allowlist。

**函数原型：**

```c
XWORK_API void xwork_control_plane_destroy(xwork_control_plane *pPlane);
```

**参数：**

- `pPlane`：要销毁的 plane；可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放 plane 拥有资源；不会释放 borrowed runtime，也不会杀死外部进程或网络连接。

**补充说明：**

销毁前应由宿主先停止外部 transport。

**范例代码：**

```c
xwork_control_plane_destroy(plane);
```

**相关 API：**

- `xwork_control_plane_create`

---

### xwork_control_plane_start

启动 control plane 调度状态。

**功能：**

允许 worker claim queued task。

**函数原型：**

```c
XWORK_API xwork_status xwork_control_plane_start(xwork_control_plane *pPlane);
```

**参数：**

- `pPlane`：目标 plane。

**返回值：**

返回 `XWORK_OK` 或 `XWORK_ERROR_INVALID_ARGUMENT`。

**资源归属：**

不分配资源。

**补充说明：**

start 只改变内存调度标记，不启动网络 server。

**范例代码：**

```c
xwork_control_plane_start(plane);
```

**相关 API：**

- `xwork_control_plane_stop`
- `xwork_control_plane_claim_task`

---

### xwork_control_plane_stop

停止 control plane 调度状态。

**功能：**

禁止新的 claim 继续获取任务。

**函数原型：**

```c
XWORK_API xwork_status xwork_control_plane_stop(xwork_control_plane *pPlane);
```

**参数：**

- `pPlane`：目标 plane。

**返回值：**

返回 `XWORK_OK` 或 `XWORK_ERROR_INVALID_ARGUMENT`。

**资源归属：**

不分配资源。

**补充说明：**

stop 不取消已经 assigned/running 的任务，也不停止 OS 进程。

**范例代码：**

```c
xwork_control_plane_stop(plane);
```

**相关 API：**

- `xwork_control_plane_start`

---

### xwork_control_plane_set_time

设置 control plane 当前时间。

**功能：**

更新 plane 内部 `nowMs`，用于 lease、heartbeat 和 snapshot。

**函数原型：**

```c
XWORK_API xwork_status xwork_control_plane_set_time(
    xwork_control_plane *pPlane,
    size_t iNowMs
);
```

**参数：**

- `pPlane`：目标 plane。
- `iNowMs`：当前时间，单位毫秒，由宿主提供。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

不分配资源。

**补充说明：**

xwork 不读取系统时钟；宿主负责提供单调或业务时间。

**范例代码：**

```c
xwork_control_plane_set_time(plane, nowMs);
```

**相关 API：**

- `xwork_control_plane_worker_heartbeat`
- `xwork_control_plane_sweep_stale`

---

## Worker Registry

### xwork_control_plane_register_worker

注册 worker。

**功能：**

将 worker 加入 control plane registry，并初始化 lease 状态。

**函数原型：**

```c
XWORK_API xwork_status xwork_control_plane_register_worker(
    xwork_control_plane *pPlane,
    const xwork_worker_options *pOptions,
    xwork_worker **ppWorker
);
```

**参数：**

- `pPlane`：目标 plane。
- `pOptions`：worker 参数；必须包含非空 `sWorkerId`。
- `ppWorker`：可选输出 borrowed worker。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

worker 由 plane 拥有；返回指针不可释放，unregister/destroy 后失效。

**补充说明：**

worker protocol version 必须与 plane 匹配；capability allowlist 开启时，worker capability 必须被允许。

**范例代码：**

```c
xwork_worker *worker = NULL;
xwork_worker_options opts;
xwork_worker_options_init(&opts);
opts.sWorkerId = "worker-1";
xwork_control_plane_register_worker(plane, &opts, &worker);
```

**相关 API：**

- `xwork_control_plane_worker_heartbeat`
- `xwork_control_plane_unregister_worker`

---

### xwork_control_plane_unregister_worker

注销 worker。

**功能：**

将 worker 标记为 unregistered。

**函数原型：**

```c
XWORK_API xwork_status xwork_control_plane_unregister_worker(
    xwork_control_plane *pPlane,
    const char *sWorkerId
);
```

**参数：**

- `pPlane`：目标 plane。
- `sWorkerId`：worker id。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

不释放 plane 中 worker 记录，仅更新状态。

**补充说明：**

注销不会自动取消已分配任务；调用方应结合 sweep/cancel 策略处理。

**范例代码：**

```c
xwork_control_plane_unregister_worker(plane, "worker-1");
```

**相关 API：**

- `xwork_control_plane_sweep_stale`

---

### xwork_control_plane_worker_heartbeat

更新 worker heartbeat。

**功能：**

刷新 worker 的 last heartbeat、lease expires，并将状态置为 online。

**函数原型：**

```c
XWORK_API xwork_status xwork_control_plane_worker_heartbeat(
    xwork_control_plane *pPlane,
    const char *sWorkerId,
    size_t iNowMs
);
```

**参数：**

- `pPlane`：目标 plane。
- `sWorkerId`：worker id。
- `iNowMs`：当前时间，单位毫秒。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

不分配资源。

**补充说明：**

unregistered worker 不能 heartbeat。

**范例代码：**

```c
xwork_control_plane_worker_heartbeat(plane, "worker-1", nowMs);
```

**相关 API：**

- `xwork_control_plane_sweep_stale`

---

### xwork_control_plane_sweep_stale

清理过期 worker。

**功能：**

根据 lease 过期时间标记 stale worker，并将其 assigned/running 任务转为 orphaned。

**函数原型：**

```c
XWORK_API xwork_status xwork_control_plane_sweep_stale(
    xwork_control_plane *pPlane,
    size_t iNowMs,
    size_t *piOrphanedCount
);
```

**参数：**

- `pPlane`：目标 plane。
- `iNowMs`：当前时间，单位毫秒。
- `piOrphanedCount`：可选输出本次 orphaned task 数量。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

不转移所有权。

**补充说明：**

该 API 是恢复和 worker lease 管理的核心边界；orphaned 后由宿主决定重试、取消或人工处理。

**范例代码：**

```c
size_t orphaned = 0;
xwork_control_plane_sweep_stale(plane, nowMs, &orphaned);
```

**相关 API：**

- `xwork_control_plane_worker_heartbeat`

---

### xwork_control_plane_list_workers

列出 worker。

**功能：**

获取 control plane 中 worker registry 的摘要列表。

**函数原型：**

```c
XWORK_API xwork_status xwork_control_plane_list_workers(
    const xwork_control_plane *pPlane,
    xwork_worker_summary_list *pList
);
```

**参数：**

- `pPlane`：源 plane。
- `pList`：输出列表；调用前应 init。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

列表拥有 deep-copy 内容，用 `xwork_worker_summary_list_reset` 释放。

**补充说明：**

函数会重置输出列表旧内容。

**范例代码：**

```c
xwork_worker_summary_list list;
xwork_worker_summary_list_init(&list);
xwork_control_plane_list_workers(plane, &list);
xwork_worker_summary_list_reset(&list);
```

**相关 API：**

- `xwork_worker_summary_list_reset`

---

## Remote Task 生命周期

### xwork_control_plane_enqueue_task

入队 remote task。

**功能：**

将 remote task 放入 control plane 队列，等待 worker claim。

**函数原型：**

```c
XWORK_API xwork_status xwork_control_plane_enqueue_task(
    xwork_control_plane *pPlane,
    const xwork_remote_task_options *pOptions
);
```

**参数：**

- `pPlane`：目标 plane。
- `pOptions`：任务参数；必须包含 task id 和 request JSON。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

plane 复制 task 字段；`pUserData` 为 borrowed。

**补充说明：**

会执行 capability allowlist、task policy 和 network policy 检查。`XWORK_REMOTE_TASK_PROCESS_EXEC` 会映射到 process host service。

**范例代码：**

```c
xwork_remote_task_options task;
xwork_remote_task_options_init(&task);
task.sTaskId = "task-1";
task.sOperationId = XWORK_HOST_PROCESS_EXEC;
task.sRequestJson = "{}";
xwork_control_plane_enqueue_task(plane, &task);
```

**相关 API：**

- `xwork_control_plane_claim_task`

---

### xwork_control_plane_claim_task

worker 领取任务。

**功能：**

为 online worker 查找匹配 capability 的 queued task，生成 assignment。

**函数原型：**

```c
XWORK_API xwork_status xwork_control_plane_claim_task(
    xwork_control_plane *pPlane,
    const char *sWorkerId,
    xwork_remote_task_assignment *pAssignment
);
```

**参数：**

- `pPlane`：目标 plane。
- `sWorkerId`：领取任务的 worker id。
- `pAssignment`：输出 assignment；调用前应 init。

**返回值：**

返回 `XWORK_OK`、`XWORK_ERROR_NOT_FOUND` 或其他错误码。

**资源归属：**

assignment 拥有 deep-copy 字段，用 `xwork_remote_task_assignment_reset` 释放。

**补充说明：**

plane 必须已 start；worker 必须 online；无可领取任务时返回 not found。

**范例代码：**

```c
xwork_remote_task_assignment assignment;
xwork_remote_task_assignment_init(&assignment);
xwork_control_plane_claim_task(plane, "worker-1", &assignment);
xwork_remote_task_assignment_reset(&assignment);
```

**相关 API：**

- `xwork_control_plane_complete_task`

---

### xwork_control_plane_complete_task

完成 remote task。

**功能：**

根据 assignment id 提交任务结果，并把任务标记为 completed 或 failed。

**函数原型：**

```c
XWORK_API xwork_status xwork_control_plane_complete_task(
    xwork_control_plane *pPlane,
    const char *sAssignmentId,
    const xwork_remote_task_result *pResult
);
```

**参数：**

- `pPlane`：目标 plane。
- `sAssignmentId`：assignment id。
- `pResult`：任务结果。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

plane 复制 output、summary、error 和 artifact summary。

**补充说明：**

结果 protocol version 必须与任务一致；`iStatus == XWORK_OK` 时任务 completed，否则 failed。

**范例代码：**

```c
xwork_remote_task_result result;
xwork_remote_task_result_init(&result);
result.sVisibleSummary = "completed";
xwork_control_plane_complete_task(plane, assignment.sAssignmentId, &result);
```

**相关 API：**

- `xwork_control_plane_fail_task`

---

### xwork_control_plane_fail_task

快速标记 remote task 失败。

**功能：**

用错误文本构造标准 remote task result，并提交为失败结果。

**函数原型：**

```c
XWORK_API xwork_status xwork_control_plane_fail_task(
    xwork_control_plane *pPlane,
    const char *sAssignmentId,
    const char *sErrorText,
    bool bRetryable
);
```

**参数：**

- `pPlane`：目标 plane。
- `sAssignmentId`：assignment id。
- `sErrorText`：错误文本；可为 `NULL`。
- `bRetryable`：是否建议重试。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

错误文本会被复制到 task record。

**补充说明：**

该 API 等价于提交 `XWORK_ERROR_EXTERNAL_FAILURE` 的 complete result。

**范例代码：**

```c
xwork_control_plane_fail_task(plane, assignmentId, "tool failed", true);
```

**相关 API：**

- `xwork_control_plane_complete_task`

---

### xwork_control_plane_cancel_task

取消 remote task。

**功能：**

按 task id 将非终态任务标记为 cancelled。

**函数原型：**

```c
XWORK_API xwork_status xwork_control_plane_cancel_task(
    xwork_control_plane *pPlane,
    const char *sTaskId,
    const char *sReason
);
```

**参数：**

- `pPlane`：目标 plane。
- `sTaskId`：任务 id。
- `sReason`：可选取消原因。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

plane 复制取消原因到 task 错误信息。

**补充说明：**

取消不会停止已经由 worker 启动的外部 OS 进程；worker/transport 需要自行接收和执行取消。

**范例代码：**

```c
xwork_control_plane_cancel_task(plane, "task-1", "user cancelled");
```

**相关 API：**

- `xwork_control_plane_get_task_summary`

---

### xwork_control_plane_execute_next_local

在本地 worker runtime 上执行下一个任务。

**功能：**

封装 claim、调用 worker runtime host service、complete/fail 的本地快捷路径。

**函数原型：**

```c
XWORK_API xwork_status xwork_control_plane_execute_next_local(
    xwork_control_plane *pPlane,
    const char *sWorkerId,
    xwork_remote_task_assignment *pAssignment
);
```

**参数：**

- `pPlane`：目标 plane。
- `sWorkerId`：本地 worker id。
- `pAssignment`：可选输出实际执行的 assignment。

**返回值：**

返回 host service 执行状态或 control plane 错误码。

**资源归属：**

输出 assignment 如被填充，由调用者 reset。

**补充说明：**

worker 必须注册并拥有 runtime。该 API 适合 in-process worker mock、测试和单机 agent shell。

**范例代码：**

```c
xwork_remote_task_assignment assignment;
xwork_remote_task_assignment_init(&assignment);
xwork_control_plane_execute_next_local(plane, "worker-1", &assignment);
xwork_remote_task_assignment_reset(&assignment);
```

**相关 API：**

- `xwork_runtime_invoke_host_service`

---

### xwork_control_plane_get_task_summary

查询单个 remote task。

**功能：**

按 task id 获取任务状态、assignment、结果、artifact 和 output chunk 摘要。

**函数原型：**

```c
XWORK_API xwork_status xwork_control_plane_get_task_summary(
    const xwork_control_plane *pPlane,
    const char *sTaskId,
    xwork_remote_task_summary *pSummary
);
```

**参数：**

- `pPlane`：源 plane。
- `sTaskId`：任务 id。
- `pSummary`：输出 summary；调用前应 init。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

summary 拥有 deep-copy 内容，用 `xwork_remote_task_summary_reset` 释放。

**补充说明：**

函数会重置输出 summary 旧内容。

**范例代码：**

```c
xwork_remote_task_summary summary;
xwork_remote_task_summary_init(&summary);
xwork_control_plane_get_task_summary(plane, "task-1", &summary);
xwork_remote_task_summary_reset(&summary);
```

**相关 API：**

- `xwork_control_plane_list_tasks`

---

### xwork_control_plane_list_tasks

列出 remote task。

**功能：**

获取 control plane 中所有任务的摘要列表。

**函数原型：**

```c
XWORK_API xwork_status xwork_control_plane_list_tasks(
    const xwork_control_plane *pPlane,
    xwork_remote_task_summary_list *pList
);
```

**参数：**

- `pPlane`：源 plane。
- `pList`：输出列表；调用前应 init。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

列表拥有 deep-copy 内容，用 `xwork_remote_task_summary_list_reset` 释放。

**补充说明：**

用于 UI 队列面板、恢复诊断和测试断言。

**范例代码：**

```c
xwork_remote_task_summary_list list;
xwork_remote_task_summary_list_init(&list);
xwork_control_plane_list_tasks(plane, &list);
xwork_remote_task_summary_list_reset(&list);
```

**相关 API：**

- `xwork_control_plane_get_task_summary`

---

## Artifact 与 Output 上传

### xwork_control_plane_upload_artifact

上传 remote artifact。

**功能：**

为 remote task 追加或更新 artifact summary，并保存可选 blob chunk。

**函数原型：**

```c
XWORK_API xwork_status xwork_control_plane_upload_artifact(
    xwork_control_plane *pPlane,
    const xwork_remote_artifact_upload *pUpload
);
```

**参数：**

- `pPlane`：目标 plane。
- `pUpload`：上传请求；必须包含 task id、worker id 和 artifact summary。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

plane 复制 artifact summary、blob metadata 和 chunk 数据。

**补充说明：**

任务不能处于 queued、cancelled 或 orphaned；assignment id 如提供，必须匹配任务当前 assignment。

**范例代码：**

```c
xwork_remote_artifact_upload upload;
xwork_remote_artifact_upload_init(&upload);
upload.sTaskId = "task-1";
upload.sWorkerId = "worker-1";
upload.pArtifact = &artifactSummary;
xwork_control_plane_upload_artifact(plane, &upload);
```

**相关 API：**

- `xwork_control_plane_list_artifact_blobs`

---

### xwork_control_plane_upload_output_chunk

上传 remote output chunk。

**功能：**

为 remote task 追加 stdout/stderr 文本 chunk。

**函数原型：**

```c
XWORK_API xwork_status xwork_control_plane_upload_output_chunk(
    xwork_control_plane *pPlane,
    const xwork_remote_output_chunk *pChunk
);
```

**参数：**

- `pPlane`：目标 plane。
- `pChunk`：输出 chunk；必须包含 task id、worker id 和文本。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

plane 复制文本、hash 和 chunk metadata。

**补充说明：**

任务不能处于 queued、cancelled 或 orphaned；assignment id 如提供，必须匹配。

**范例代码：**

```c
xwork_remote_output_chunk chunk;
xwork_remote_output_chunk_init(&chunk);
chunk.sTaskId = "task-1";
chunk.sWorkerId = "worker-1";
chunk.sText = "stdout line\n";
xwork_control_plane_upload_output_chunk(plane, &chunk);
```

**相关 API：**

- `xwork_remote_output_chunk_init`

---

### xwork_control_plane_list_artifact_blobs

列出 artifact blob chunks。

**功能：**

按 task id 和可选 artifact id 查询已上传的 blob chunk。

**函数原型：**

```c
XWORK_API xwork_status xwork_control_plane_list_artifact_blobs(
    const xwork_control_plane *pPlane,
    const char *sTaskId,
    const char *sArtifactId,
    xwork_remote_blob_chunk_summary_list *pList
);
```

**参数：**

- `pPlane`：源 plane。
- `sTaskId`：任务 id。
- `sArtifactId`：可选 artifact id；为空时返回该 task 的所有 blob chunk。
- `pList`：输出列表；调用前应 init。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

列表拥有 deep-copy 内容，用 `xwork_remote_blob_chunk_summary_list_reset` 释放。

**补充说明：**

该 API 可用于恢复 artifact blob、调试上传顺序或构建下载响应。

**范例代码：**

```c
xwork_remote_blob_chunk_summary_list list;
xwork_remote_blob_chunk_summary_list_init(&list);
xwork_control_plane_list_artifact_blobs(plane, "task-1", NULL, &list);
xwork_remote_blob_chunk_summary_list_reset(&list);
```

**相关 API：**

- `xwork_control_plane_upload_artifact`

---

## Snapshot

### xwork_control_plane_get_snapshot

获取 control plane snapshot。

**功能：**

深拷贝 control plane 的 worker registry、task 状态和 blob chunks。

**函数原型：**

```c
XWORK_API xwork_status xwork_control_plane_get_snapshot(
    const xwork_control_plane *pPlane,
    xwork_control_plane_snapshot *pSnapshot
);
```

**参数：**

- `pPlane`：源 plane。
- `pSnapshot`：输出 snapshot；调用前应 init。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

snapshot 拥有 deep-copy 内容，用 `xwork_control_plane_snapshot_reset` 释放。

**补充说明：**

snapshot 不包含 live network connection、thread、process 或 terminal handle。

**范例代码：**

```c
xwork_control_plane_snapshot snapshot;
xwork_control_plane_snapshot_init(&snapshot);
xwork_control_plane_get_snapshot(plane, &snapshot);
xwork_control_plane_snapshot_reset(&snapshot);
```

**相关 API：**

- `xwork_control_plane_create_from_snapshot`

---

## 相关文档

- [Host Tools API](api-host-tools.md)
- [Policy / Approval API](api-policy-approval.md)
- [远程 Worker 与控制平面](../guide/remote-worker-intro.md)
- [Remote Worker Agent 范例](../case/remote-worker-agent.md)
- [内部 remote worker contract](../../dev/docs/REMOTE_WORKER.md)
