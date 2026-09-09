# Multi-Agent API

Multi-Agent API 提供 in-process agent pool、task graph、依赖调度、handoff、child run 映射、暂停/恢复/取消和 snapshot 恢复能力。它面向 AI IDE、claw、自动化研发流水线等场景，用于把多个角色 agent 组织成可审计、可恢复的任务图。

## 模块边界

- agent pool 管理 agent 定义，不执行模型推理。
- task graph 管理任务节点、依赖、并发限制、失败策略、handoff 和执行状态。
- 每个任务执行时会映射到 `xwork_run`，实际 model-turn 和 tool-loop 由 run/orchestrator 层完成。
- snapshot 只恢复可序列化状态；native thread、callback 栈、外部 live handle 不会恢复。
- graph mutation 应与 execute/query 串行化；task callback 若并发运行，需要自行保护共享资源。

## 所有权约定

| 对象 | 所有权 |
| --- | --- |
| `xwork_agent_pool_create` | 返回 owned pool，调用者用 `xwork_agent_pool_destroy` 释放。 |
| `xwork_agent_pool_add_agent` | pool 复制 agent options 字符串，返回 borrowed agent。 |
| `xwork_task_graph_create` | 返回 owned graph，调用者用 `xwork_task_graph_destroy` 释放。 |
| `xwork_task_graph` | 借用 agent pool、cancel token、execute callback 和 callback user data。 |
| `xwork_task_graph_get_snapshot` | 深拷贝 snapshot 内容，调用者用 `xwork_task_graph_snapshot_reset` 释放。 |
| `*_list_reset` / `*_snapshot_reset` | 释放 API 分配的字符串、数组和嵌套列表。 |

## 典型调用顺序

```text
xwork_agent_pool_options_init
xwork_agent_pool_create
xwork_agent_options_init
xwork_agent_pool_add_agent
xwork_task_graph_options_init
xwork_task_graph_create
xwork_task_node_options_init
xwork_task_graph_add_node
xwork_task_graph_add_dependency
xwork_task_graph_execute
xwork_task_graph_get_snapshot
xwork_task_graph_destroy
xwork_agent_pool_destroy
```

## 初始化与释放约定

所有 `*_init` 函数都允许传入 `NULL`，此时不执行任何操作。所有 `*_reset` 函数也允许传入 `NULL`，并会在释放后恢复为 init 后状态。调用获取 summary/list/snapshot 的 API 前，建议先 init；复用同一结构前先 reset，避免泄漏旧内容。

## Agent Pool 与 Agent

### xwork_agent_pool_options_init

初始化 `xwork_agent_pool_options`。

**功能：**

将 pool options 清零，为创建 agent pool 做准备。

**函数原型：**

```c
XWORK_API void xwork_agent_pool_options_init(xwork_agent_pool_options *pOptions);
```

**参数：**

- `pOptions`：要初始化的 options；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源；调用方仍拥有结构体本身。

**补充说明：**

- 创建 pool 前必须设置 `pRuntime`。
- `sPoolId` 为空时，当前实现使用 `"default"`。

**范例代码：**

```c
xwork_agent_pool_options opts;
xwork_agent_pool_options_init(&opts);
opts.sPoolId = "main";
opts.pRuntime = runtime;
```

**相关 API：**

- `xwork_agent_pool_create`

---

### xwork_agent_options_init

初始化 `xwork_agent_options`。

**功能：**

设置 agent options 默认值，用于注册 planner、coder、reviewer 等 agent。

**函数原型：**

```c
XWORK_API void xwork_agent_options_init(xwork_agent_options *pOptions);
```

**参数：**

- `pOptions`：要初始化的 options；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源；字符串字段由调用方在 `xwork_agent_pool_add_agent` 调用期间提供，pool 会复制需要保留的值。

**补充说明：**

- 默认角色为 `XWORK_AGENT_ROLE_CUSTOM`。
- 默认自治模式为 `XWORK_AUTONOMY_SEMI_AUTO`。

**范例代码：**

```c
xwork_agent_options opts;
xwork_agent_options_init(&opts);
opts.sAgentId = "coder";
opts.eRole = XWORK_AGENT_ROLE_CODER;
```

**相关 API：**

- `xwork_agent_pool_add_agent`

---

### xwork_agent_snapshot_init

初始化 `xwork_agent_snapshot`。

**功能：**

准备一个可由 snapshot API 填充或手工构造的 agent snapshot。

**函数原型：**

```c
XWORK_API void xwork_agent_snapshot_init(xwork_agent_snapshot *pSnapshot);
```

**参数：**

- `pSnapshot`：要初始化的 snapshot；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源。

**补充说明：**

默认角色和自治模式与 `xwork_agent_options_init` 一致。

**范例代码：**

```c
xwork_agent_snapshot snapshot;
xwork_agent_snapshot_init(&snapshot);
```

**相关 API：**

- `xwork_agent_snapshot_reset`
- `xwork_agent_pool_get_snapshot`

---

### xwork_agent_snapshot_reset

释放并重置 `xwork_agent_snapshot`。

**功能：**

释放 snapshot 内部字符串，并恢复为 init 状态。

**函数原型：**

```c
XWORK_API void xwork_agent_snapshot_reset(xwork_agent_snapshot *pSnapshot);
```

**参数：**

- `pSnapshot`：要重置的 snapshot；可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放 snapshot 持有的深拷贝字段，不释放结构体本身。

**补充说明：**

仅对 xwork API 生成或按相同所有权规则构造的 snapshot 调用 reset。

**范例代码：**

```c
xwork_agent_snapshot_reset(&snapshot);
```

**相关 API：**

- `xwork_agent_snapshot_init`

---

### xwork_agent_snapshot_list_init

初始化 agent snapshot 列表。

**功能：**

将列表置为空，便于 snapshot API 填充。

**函数原型：**

```c
XWORK_API void xwork_agent_snapshot_list_init(xwork_agent_snapshot_list *pList);
```

**参数：**

- `pList`：要初始化的列表；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源。

**补充说明：**

列表元素通常由 `xwork_agent_pool_get_snapshot` 深拷贝生成。

**范例代码：**

```c
xwork_agent_snapshot_list list;
xwork_agent_snapshot_list_init(&list);
```

**相关 API：**

- `xwork_agent_snapshot_list_reset`

---

### xwork_agent_snapshot_list_reset

释放 agent snapshot 列表。

**功能：**

释放列表内每个 agent snapshot 及列表数组。

**函数原型：**

```c
XWORK_API void xwork_agent_snapshot_list_reset(xwork_agent_snapshot_list *pList);
```

**参数：**

- `pList`：要释放的列表；可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放列表拥有的元素和数组，不释放列表结构体本身。

**补充说明：**

调用后列表回到空状态，可再次传给填充 API。

**范例代码：**

```c
xwork_agent_snapshot_list_reset(&list);
```

**相关 API：**

- `xwork_agent_snapshot_reset`

---

### xwork_agent_pool_snapshot_init

初始化 agent pool snapshot。

**功能：**

准备一个 pool snapshot，用于持久化或恢复 agent pool。

**函数原型：**

```c
XWORK_API void xwork_agent_pool_snapshot_init(xwork_agent_pool_snapshot *pSnapshot);
```

**参数：**

- `pSnapshot`：要初始化的 snapshot；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源。

**补充说明：**

内部会初始化 `tAgents`。

**范例代码：**

```c
xwork_agent_pool_snapshot snapshot;
xwork_agent_pool_snapshot_init(&snapshot);
```

**相关 API：**

- `xwork_agent_pool_get_snapshot`
- `xwork_agent_pool_snapshot_reset`

---

### xwork_agent_pool_snapshot_reset

释放 agent pool snapshot。

**功能：**

释放 pool id 和 agent snapshot 列表。

**函数原型：**

```c
XWORK_API void xwork_agent_pool_snapshot_reset(xwork_agent_pool_snapshot *pSnapshot);
```

**参数：**

- `pSnapshot`：要释放的 snapshot；可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放 snapshot 内部拥有的深拷贝资源，不释放结构体本身。

**补充说明：**

`xwork_agent_pool_create_from_snapshot` 不接管 snapshot 所有权。

**范例代码：**

```c
xwork_agent_pool_snapshot_reset(&snapshot);
```

**相关 API：**

- `xwork_agent_pool_create_from_snapshot`

---

### xwork_agent_pool_create

创建 agent pool。

**功能：**

基于 runtime 创建一个 in-process agent pool，用于注册 agent 并供 task graph 借用。

**函数原型：**

```c
XWORK_API xwork_status xwork_agent_pool_create(
    const xwork_agent_pool_options *pOptions,
    xwork_agent_pool **ppPool
);
```

**参数：**

- `pOptions`：pool 创建参数；必须包含有效 `pRuntime`。
- `ppPool`：输出 owned pool。

**返回值：**

- `XWORK_OK`：创建成功。
- `XWORK_ERROR_INVALID_ARGUMENT`：参数为空或 runtime 无效。
- `XWORK_ERROR_NO_MEMORY`：内存分配失败。

**资源归属：**

成功后 `*ppPool` 归调用者所有，使用 `xwork_agent_pool_destroy` 释放。pool 借用 runtime，runtime 必须比 pool 活得更久。

**补充说明：**

`sPoolId` 会被复制；未设置时使用默认 id。

**范例代码：**

```c
xwork_agent_pool *pool = NULL;
xwork_agent_pool_options opts;
xwork_agent_pool_options_init(&opts);
opts.pRuntime = runtime;
opts.sPoolId = "main";
xwork_status st = xwork_agent_pool_create(&opts, &pool);
```

**相关 API：**

- `xwork_agent_pool_destroy`
- `xwork_agent_pool_add_agent`

---

### xwork_agent_pool_create_from_snapshot

从 snapshot 恢复 agent pool。

**功能：**

使用先前获取的 pool snapshot 重建 agent pool 和其中的 agent 定义。

**函数原型：**

```c
XWORK_API xwork_status xwork_agent_pool_create_from_snapshot(
    xwork_runtime *pRuntime,
    const xwork_agent_pool_snapshot *pSnapshot,
    xwork_agent_pool **ppPool
);
```

**参数：**

- `pRuntime`：恢复后 pool 借用的 runtime。
- `pSnapshot`：agent pool snapshot。
- `ppPool`：输出 owned pool。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

成功后 pool 归调用者所有；snapshot 不被接管，仍由调用者 reset。

**补充说明：**

恢复只包含 agent 元数据，不恢复 runtime 内外部 live 状态。

**范例代码：**

```c
xwork_agent_pool *pool = NULL;
xwork_status st = xwork_agent_pool_create_from_snapshot(runtime, &snapshot, &pool);
```

**相关 API：**

- `xwork_agent_pool_get_snapshot`
- `xwork_agent_pool_destroy`

---

### xwork_agent_pool_destroy

销毁 agent pool。

**功能：**

释放 pool 及其拥有的所有 agent 定义。

**函数原型：**

```c
XWORK_API void xwork_agent_pool_destroy(xwork_agent_pool *pPool);
```

**参数：**

- `pPool`：要销毁的 pool；可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放 pool 拥有的资源；不会释放借用的 runtime。

**补充说明：**

销毁 pool 前，应先销毁借用该 pool 的 task graph。

**范例代码：**

```c
xwork_agent_pool_destroy(pool);
```

**相关 API：**

- `xwork_agent_pool_create`

---

### xwork_agent_pool_add_agent

向 pool 注册 agent。

**功能：**

复制 agent options，将 agent 定义加入 pool，并可返回 borrowed agent 指针。

**函数原型：**

```c
XWORK_API xwork_status xwork_agent_pool_add_agent(
    xwork_agent_pool *pPool,
    const xwork_agent_options *pOptions,
    xwork_agent **ppAgent
);
```

**参数：**

- `pPool`：目标 agent pool。
- `pOptions`：agent 定义；必须包含非空 `sAgentId`。
- `ppAgent`：可选输出 borrowed agent。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

agent 由 pool 拥有；`ppAgent` 返回借用指针，pool 销毁后失效。

**补充说明：**

agent id 应在同一 pool 内唯一。

**范例代码：**

```c
xwork_agent *agent = NULL;
xwork_agent_options opts;
xwork_agent_options_init(&opts);
opts.sAgentId = "reviewer";
opts.eRole = XWORK_AGENT_ROLE_REVIEWER;
xwork_agent_pool_add_agent(pool, &opts, &agent);
```

**相关 API：**

- `xwork_agent_pool_find_agent`
- `xwork_agent_get_id`

---

### xwork_agent_pool_get_agent_count

获取 agent 数量。

**功能：**

返回 pool 中已注册的 agent 个数。

**函数原型：**

```c
XWORK_API size_t xwork_agent_pool_get_agent_count(const xwork_agent_pool *pPool);
```

**参数：**

- `pPool`：agent pool；可为 `NULL`。

**返回值：**

返回 agent 数量；`pPool` 为 `NULL` 时返回 `0`。

**资源归属：**

不分配资源。

**补充说明：**

该值是当前 in-memory pool 的快照式读数。

**范例代码：**

```c
size_t count = xwork_agent_pool_get_agent_count(pool);
```

**相关 API：**

- `xwork_agent_pool_add_agent`

---

### xwork_agent_pool_find_agent

按 id 查找 agent。

**功能：**

在 pool 中查找指定 agent id。

**函数原型：**

```c
XWORK_API xwork_agent *xwork_agent_pool_find_agent(
    const xwork_agent_pool *pPool,
    const char *sAgentId
);
```

**参数：**

- `pPool`：agent pool。
- `sAgentId`：agent id。

**返回值：**

找到时返回 borrowed agent；未找到或参数无效时返回 `NULL`。

**资源归属：**

返回值由 pool 拥有，调用者不能释放。

**补充说明：**

返回指针在 agent pool 被销毁后失效。

**范例代码：**

```c
xwork_agent *coder = xwork_agent_pool_find_agent(pool, "coder");
```

**相关 API：**

- `xwork_agent_get_role`

---

### xwork_agent_pool_get_snapshot

获取 agent pool snapshot。

**功能：**

深拷贝 pool id 和所有 agent 定义，生成可持久化 snapshot。

**函数原型：**

```c
XWORK_API xwork_status xwork_agent_pool_get_snapshot(
    const xwork_agent_pool *pPool,
    xwork_agent_pool_snapshot *pSnapshot
);
```

**参数：**

- `pPool`：源 pool。
- `pSnapshot`：输出 snapshot；调用前应 init。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

snapshot 拥有深拷贝内容，调用者用 `xwork_agent_pool_snapshot_reset` 释放。

**补充说明：**

函数会重置输出 snapshot 的旧内容。

**范例代码：**

```c
xwork_agent_pool_snapshot snapshot;
xwork_agent_pool_snapshot_init(&snapshot);
xwork_agent_pool_get_snapshot(pool, &snapshot);
xwork_agent_pool_snapshot_reset(&snapshot);
```

**相关 API：**

- `xwork_agent_pool_create_from_snapshot`

---

### xwork_agent_get_id

获取 agent id。

**功能：**

返回 agent 的 id 字符串。

**函数原型：**

```c
XWORK_API const char *xwork_agent_get_id(const xwork_agent *pAgent);
```

**参数：**

- `pAgent`：agent 指针；可为 `NULL`。

**返回值：**

返回 borrowed id；`pAgent` 为 `NULL` 时返回 `NULL`。

**资源归属：**

返回值由 agent pool 拥有，调用者不能释放。

**补充说明：**

该 id 可用于 task node 的 `sAgentId`。

**范例代码：**

```c
const char *id = xwork_agent_get_id(agent);
```

**相关 API：**

- `xwork_task_graph_add_node`

---

### xwork_agent_get_role

获取 agent 角色。

**功能：**

返回 agent 的 `xwork_agent_role`。

**函数原型：**

```c
XWORK_API xwork_agent_role xwork_agent_get_role(const xwork_agent *pAgent);
```

**参数：**

- `pAgent`：agent 指针；可为 `NULL`。

**返回值：**

返回角色；`pAgent` 为 `NULL` 时返回 `XWORK_AGENT_ROLE_CUSTOM`。

**资源归属：**

不分配资源。

**补充说明：**

角色用于宿主 UI、日志和调度策略判断。

**范例代码：**

```c
xwork_agent_role role = xwork_agent_get_role(agent);
```

**相关 API：**

- `xwork_agent_pool_add_agent`

---

## Task Node 与 Task Graph

### xwork_task_node_options_init

初始化 task node options。

**功能：**

准备任务节点定义，用于加入 task graph。

**函数原型：**

```c
XWORK_API void xwork_task_node_options_init(xwork_task_node_options *pOptions);
```

**参数：**

- `pOptions`：要初始化的 options；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源；`xwork_task_graph_add_node` 会复制需要保留的字符串和数组。

**补充说明：**

默认自治模式为 `XWORK_AUTONOMY_SEMI_AUTO`。

**范例代码：**

```c
xwork_task_node_options opts;
xwork_task_node_options_init(&opts);
opts.sTaskId = "implement";
opts.sAgentId = "coder";
opts.sInstruction = "Implement the feature.";
```

**相关 API：**

- `xwork_task_graph_add_node`

---

### xwork_task_graph_options_init

初始化 task graph options。

**功能：**

准备 task graph 创建参数。

**函数原型：**

```c
XWORK_API void xwork_task_graph_options_init(xwork_task_graph_options *pOptions);
```

**参数：**

- `pOptions`：要初始化的 options；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源。

**补充说明：**

- 默认最大并发为 `1`。
- 默认失败策略为 `XWORK_TASK_FAILURE_FAIL_FAST`。
- 创建 graph 前必须设置 `pAgentPool`。

**范例代码：**

```c
xwork_task_graph_options opts;
xwork_task_graph_options_init(&opts);
opts.sGraphId = "feature-graph";
opts.pAgentPool = pool;
opts.iMaxConcurrency = 2;
```

**相关 API：**

- `xwork_task_graph_create`

---

### xwork_task_node_summary_init

初始化 task node summary。

**功能：**

准备一个任务摘要结构，用于查询单个或多个节点状态。

**函数原型：**

```c
XWORK_API void xwork_task_node_summary_init(xwork_task_node_summary *pSummary);
```

**参数：**

- `pSummary`：要初始化的 summary；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源。

**补充说明：**

默认任务状态为 `XWORK_TASK_PENDING`。

**范例代码：**

```c
xwork_task_node_summary summary;
xwork_task_node_summary_init(&summary);
```

**相关 API：**

- `xwork_task_graph_get_node_summary`

---

### xwork_task_node_summary_reset

释放 task node summary。

**功能：**

释放 summary 中由 API 深拷贝的字符串并恢复为 init 状态。

**函数原型：**

```c
XWORK_API void xwork_task_node_summary_reset(xwork_task_node_summary *pSummary);
```

**参数：**

- `pSummary`：要释放的 summary；可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放 summary 内部拥有资源，不释放结构体本身。

**补充说明：**

`pUserData` 是借用指针，不会被释放。

**范例代码：**

```c
xwork_task_node_summary_reset(&summary);
```

**相关 API：**

- `xwork_task_node_summary_init`

---

### xwork_task_node_summary_list_init

初始化 task node summary 列表。

**功能：**

准备一个空列表，用于接收 graph 节点摘要。

**函数原型：**

```c
XWORK_API void xwork_task_node_summary_list_init(xwork_task_node_summary_list *pList);
```

**参数：**

- `pList`：要初始化的列表；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源。

**补充说明：**

调用 `xwork_task_graph_list_node_summaries` 前应初始化该列表。

**范例代码：**

```c
xwork_task_node_summary_list list;
xwork_task_node_summary_list_init(&list);
```

**相关 API：**

- `xwork_task_graph_list_node_summaries`

---

### xwork_task_node_summary_list_reset

释放 task node summary 列表。

**功能：**

释放列表中所有 node summary 和列表数组。

**函数原型：**

```c
XWORK_API void xwork_task_node_summary_list_reset(xwork_task_node_summary_list *pList);
```

**参数：**

- `pList`：要释放的列表；可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放列表拥有的内容，不释放列表结构体本身。

**补充说明：**

释放后列表可重新传给查询 API。

**范例代码：**

```c
xwork_task_node_summary_list_reset(&list);
```

**相关 API：**

- `xwork_task_node_summary_reset`

---

### xwork_task_node_snapshot_init

初始化 task node snapshot。

**功能：**

准备一个任务节点 snapshot，用于恢复或持久化。

**函数原型：**

```c
XWORK_API void xwork_task_node_snapshot_init(xwork_task_node_snapshot *pSnapshot);
```

**参数：**

- `pSnapshot`：要初始化的 snapshot；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源。

**补充说明：**

初始化后字段为空，状态为默认 pending 语义。

**范例代码：**

```c
xwork_task_node_snapshot snapshot;
xwork_task_node_snapshot_init(&snapshot);
```

**相关 API：**

- `xwork_task_graph_get_snapshot`

---

### xwork_task_node_snapshot_reset

释放 task node snapshot。

**功能：**

释放任务节点 snapshot 中的字符串、workspace id 数组和依赖 id 数组。

**函数原型：**

```c
XWORK_API void xwork_task_node_snapshot_reset(xwork_task_node_snapshot *pSnapshot);
```

**参数：**

- `pSnapshot`：要释放的 snapshot；可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放 snapshot 内部拥有资源，不释放结构体本身。

**补充说明：**

调用后 snapshot 回到 init 状态。

**范例代码：**

```c
xwork_task_node_snapshot_reset(&snapshot);
```

**相关 API：**

- `xwork_task_node_snapshot_init`

---

### xwork_task_node_snapshot_list_init

初始化 task node snapshot 列表。

**功能：**

准备一个空 snapshot 列表。

**函数原型：**

```c
XWORK_API void xwork_task_node_snapshot_list_init(xwork_task_node_snapshot_list *pList);
```

**参数：**

- `pList`：要初始化的列表；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源。

**补充说明：**

该列表通常作为 `xwork_task_graph_snapshot.tNodes` 使用。

**范例代码：**

```c
xwork_task_node_snapshot_list list;
xwork_task_node_snapshot_list_init(&list);
```

**相关 API：**

- `xwork_task_node_snapshot_list_reset`

---

### xwork_task_node_snapshot_list_reset

释放 task node snapshot 列表。

**功能：**

释放列表中所有 task node snapshot 和数组。

**函数原型：**

```c
XWORK_API void xwork_task_node_snapshot_list_reset(xwork_task_node_snapshot_list *pList);
```

**参数：**

- `pList`：要释放的列表；可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放列表拥有的内容，不释放列表结构体本身。

**补充说明：**

`xwork_task_graph_snapshot_reset` 会间接调用它。

**范例代码：**

```c
xwork_task_node_snapshot_list_reset(&list);
```

**相关 API：**

- `xwork_task_graph_snapshot_reset`

---

### xwork_task_graph_result_init

初始化 task graph result。

**功能：**

将 graph 执行结果计数清零。

**函数原型：**

```c
XWORK_API void xwork_task_graph_result_init(xwork_task_graph_result *pResult);
```

**参数：**

- `pResult`：要初始化的 result；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源。

**补充说明：**

`xwork_task_graph_execute` 可填充该结构。

**范例代码：**

```c
xwork_task_graph_result result;
xwork_task_graph_result_init(&result);
```

**相关 API：**

- `xwork_task_graph_execute`

---

### xwork_task_graph_snapshot_init

初始化 task graph snapshot。

**功能：**

准备 graph snapshot，用于接收完整任务图状态。

**函数原型：**

```c
XWORK_API void xwork_task_graph_snapshot_init(xwork_task_graph_snapshot *pSnapshot);
```

**参数：**

- `pSnapshot`：要初始化的 snapshot；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源；内部列表初始化为空。

**补充说明：**

调用 `xwork_task_graph_get_snapshot` 前应初始化该结构。

**范例代码：**

```c
xwork_task_graph_snapshot snapshot;
xwork_task_graph_snapshot_init(&snapshot);
```

**相关 API：**

- `xwork_task_graph_get_snapshot`
- `xwork_task_graph_snapshot_reset`

---

### xwork_task_graph_snapshot_reset

释放 task graph snapshot。

**功能：**

释放 graph id、暂停/取消原因、任务节点 snapshot 列表和 handoff 列表。

**函数原型：**

```c
XWORK_API void xwork_task_graph_snapshot_reset(xwork_task_graph_snapshot *pSnapshot);
```

**参数：**

- `pSnapshot`：要释放的 snapshot；可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放 snapshot 内部拥有资源，不释放结构体本身。

**补充说明：**

`xwork_task_graph_create_from_snapshot` 不接管 snapshot 所有权。

**范例代码：**

```c
xwork_task_graph_snapshot_reset(&snapshot);
```

**相关 API：**

- `xwork_task_graph_create_from_snapshot`

---

### xwork_task_graph_create

创建 task graph。

**功能：**

创建一个任务图，用于添加 task node、声明依赖、执行多 agent 工作流。

**函数原型：**

```c
XWORK_API xwork_status xwork_task_graph_create(
    const xwork_task_graph_options *pOptions,
    xwork_task_graph **ppGraph
);
```

**参数：**

- `pOptions`：创建参数；必须包含有效 `pAgentPool`。
- `ppGraph`：输出 owned graph。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

graph 归调用者所有，用 `xwork_task_graph_destroy` 释放；graph 借用 agent pool、cancel token 和 callback。

**补充说明：**

未设置 graph id 时使用默认 id。`iMaxConcurrency` 为 `0` 时按实现默认值处理。

**范例代码：**

```c
xwork_task_graph *graph = NULL;
xwork_task_graph_options opts;
xwork_task_graph_options_init(&opts);
opts.pAgentPool = pool;
opts.iMaxConcurrency = 2;
xwork_task_graph_create(&opts, &graph);
```

**相关 API：**

- `xwork_task_graph_destroy`
- `xwork_task_graph_add_node`

---

### xwork_task_graph_create_from_snapshot

从 snapshot 恢复 task graph。

**功能：**

基于 snapshot 重建 task graph 的节点、依赖、handoff 和暂停/取消状态。

**函数原型：**

```c
XWORK_API xwork_status xwork_task_graph_create_from_snapshot(
    const xwork_task_graph_options *pOptions,
    const xwork_task_graph_snapshot *pSnapshot,
    xwork_task_graph **ppGraph
);
```

**参数：**

- `pOptions`：恢复后的运行环境参数，必须提供兼容的 agent pool 和 callback。
- `pSnapshot`：源 graph snapshot。
- `ppGraph`：输出 owned graph。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

成功后 graph 归调用者所有；snapshot 不被接管。

**补充说明：**

READY、RUNNING、BLOCKED 等 live in-flight 状态会按恢复边界转成可继续调度的状态；外部线程和 live handle 不恢复。

**范例代码：**

```c
xwork_task_graph *graph = NULL;
xwork_task_graph_create_from_snapshot(&opts, &snapshot, &graph);
```

**相关 API：**

- `xwork_task_graph_get_snapshot`

---

### xwork_task_graph_destroy

销毁 task graph。

**功能：**

释放 graph、节点、handoff 和内部状态。

**函数原型：**

```c
XWORK_API void xwork_task_graph_destroy(xwork_task_graph *pGraph);
```

**参数：**

- `pGraph`：要销毁的 graph；可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放 graph 拥有资源，不释放借用的 agent pool、cancel token 或 callback user data。

**补充说明：**

不要在 graph 正在执行时销毁它。

**范例代码：**

```c
xwork_task_graph_destroy(graph);
```

**相关 API：**

- `xwork_task_graph_create`

---

### xwork_task_graph_add_node

添加任务节点。

**功能：**

向 graph 添加一个由指定 agent 执行的 task node。

**函数原型：**

```c
XWORK_API xwork_status xwork_task_graph_add_node(
    xwork_task_graph *pGraph,
    const xwork_task_node_options *pOptions
);
```

**参数：**

- `pGraph`：目标 graph。
- `pOptions`：节点定义；必须包含 `sTaskId` 和 `sAgentId`。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

graph 复制 task id、指令、profile id、workspace id 等字段；`pUserData` 为借用指针。

**补充说明：**

`sAgentId` 必须能在 graph 的 agent pool 中找到。

**范例代码：**

```c
xwork_task_node_options node;
xwork_task_node_options_init(&node);
node.sTaskId = "review";
node.sAgentId = "reviewer";
node.sInstruction = "Review the patch.";
xwork_task_graph_add_node(graph, &node);
```

**相关 API：**

- `xwork_task_graph_add_dependency`

---

### xwork_task_graph_add_dependency

添加任务依赖。

**功能：**

声明 `sAfterTaskId` 必须在 `sBeforeTaskId` 之后执行。

**函数原型：**

```c
XWORK_API xwork_status xwork_task_graph_add_dependency(
    xwork_task_graph *pGraph,
    const char *sBeforeTaskId,
    const char *sAfterTaskId
);
```

**参数：**

- `pGraph`：目标 graph。
- `sBeforeTaskId`：前置任务 id。
- `sAfterTaskId`：后置任务 id。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

graph 复制依赖 id。

**补充说明：**

两个任务都必须已存在；循环依赖会导致执行阶段无法推进。

**范例代码：**

```c
xwork_task_graph_add_dependency(graph, "implement", "review");
```

**相关 API：**

- `xwork_task_graph_execute`

---

### xwork_task_graph_get_node_count

获取任务节点数量。

**功能：**

返回 graph 中的 task node 数量。

**函数原型：**

```c
XWORK_API size_t xwork_task_graph_get_node_count(const xwork_task_graph *pGraph);
```

**参数：**

- `pGraph`：task graph；可为 `NULL`。

**返回值：**

返回节点数量；`pGraph` 为 `NULL` 时返回 `0`。

**资源归属：**

不分配资源。

**补充说明：**

该值不代表已完成任务数量。

**范例代码：**

```c
size_t count = xwork_task_graph_get_node_count(graph);
```

**相关 API：**

- `xwork_task_graph_list_node_summaries`

---

### xwork_task_graph_get_node_summary

查询单个节点摘要。

**功能：**

按 task id 获取节点状态、尝试次数、run id、依赖数量等摘要信息。

**函数原型：**

```c
XWORK_API xwork_status xwork_task_graph_get_node_summary(
    const xwork_task_graph *pGraph,
    const char *sTaskId,
    xwork_task_node_summary *pSummary
);
```

**参数：**

- `pGraph`：源 graph。
- `sTaskId`：任务 id。
- `pSummary`：输出 summary；调用前应 init。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

summary 拥有深拷贝字符串，用 `xwork_task_node_summary_reset` 释放。

**补充说明：**

函数会重置输出 summary 的旧内容。

**范例代码：**

```c
xwork_task_node_summary summary;
xwork_task_node_summary_init(&summary);
xwork_task_graph_get_node_summary(graph, "review", &summary);
xwork_task_node_summary_reset(&summary);
```

**相关 API：**

- `xwork_task_graph_list_node_summaries`

---

### xwork_task_graph_list_node_summaries

列出所有节点摘要。

**功能：**

获取 graph 中所有 task node 的摘要列表。

**函数原型：**

```c
XWORK_API xwork_status xwork_task_graph_list_node_summaries(
    const xwork_task_graph *pGraph,
    xwork_task_node_summary_list *pList
);
```

**参数：**

- `pGraph`：源 graph。
- `pList`：输出列表；调用前应 init。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

列表拥有 deep-copy 元素，用 `xwork_task_node_summary_list_reset` 释放。

**补充说明：**

函数会重置输出列表的旧内容。

**范例代码：**

```c
xwork_task_node_summary_list list;
xwork_task_node_summary_list_init(&list);
xwork_task_graph_list_node_summaries(graph, &list);
xwork_task_node_summary_list_reset(&list);
```

**相关 API：**

- `xwork_task_node_summary_list_reset`

---

### xwork_task_graph_get_node_run

获取节点对应 run。

**功能：**

返回某个 task node 关联的 `xwork_run`。

**函数原型：**

```c
XWORK_API xwork_run *xwork_task_graph_get_node_run(
    const xwork_task_graph *pGraph,
    const char *sTaskId
);
```

**参数：**

- `pGraph`：源 graph。
- `sTaskId`：任务 id。

**返回值：**

找到时返回 borrowed run；不存在或尚未创建时返回 `NULL`。

**资源归属：**

返回值由 graph/run 层拥有，调用者不能销毁。

**补充说明：**

常用于查看 child run 事件、summary 或 artifact。

**范例代码：**

```c
xwork_run *run = xwork_task_graph_get_node_run(graph, "implement");
```

**相关 API：**

- `xwork_run_get_summary`

---

### xwork_task_graph_get_snapshot

获取 task graph snapshot。

**功能：**

深拷贝 graph 当前状态，包括节点、依赖、handoff、执行结果和暂停/取消标记。

**函数原型：**

```c
XWORK_API xwork_status xwork_task_graph_get_snapshot(
    const xwork_task_graph *pGraph,
    xwork_task_graph_snapshot *pSnapshot
);
```

**参数：**

- `pGraph`：源 graph。
- `pSnapshot`：输出 snapshot；调用前应 init。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

snapshot 拥有 deep-copy 内容，用 `xwork_task_graph_snapshot_reset` 释放。

**补充说明：**

snapshot 可交给 persistence backend 持久化，再用 create-from-snapshot 恢复。

**范例代码：**

```c
xwork_task_graph_snapshot snapshot;
xwork_task_graph_snapshot_init(&snapshot);
xwork_task_graph_get_snapshot(graph, &snapshot);
xwork_task_graph_snapshot_reset(&snapshot);
```

**相关 API：**

- `xwork_task_graph_create_from_snapshot`

---

### xwork_task_graph_execute

执行 task graph。

**功能：**

按依赖关系、最大并发和失败策略调度任务节点，调用每个节点对应的 run 执行逻辑。

**函数原型：**

```c
XWORK_API xwork_status xwork_task_graph_execute(
    xwork_task_graph *pGraph,
    xwork_task_graph_result *pResult
);
```

**参数：**

- `pGraph`：要执行的 graph。
- `pResult`：可选输出执行结果；传 `NULL` 时仅更新 graph 内部结果。

**返回值：**

返回 `XWORK_OK`、执行错误或 graph 状态错误。

**资源归属：**

不转移 graph 所有权；`pResult` 不含动态资源。

**补充说明：**

graph 防止 execute re-entry；暂停会在调度边界停止推进，取消会传播到配置的 cancel token。

**范例代码：**

```c
xwork_task_graph_result result;
xwork_task_graph_result_init(&result);
xwork_status st = xwork_task_graph_execute(graph, &result);
```

**相关 API：**

- `xwork_task_graph_pause`
- `xwork_task_graph_cancel`

---

### xwork_task_graph_cancel

取消 task graph。

**功能：**

设置 graph 取消状态，并向配置的 xllm cancel token 传播取消原因。

**函数原型：**

```c
XWORK_API xwork_status xwork_task_graph_cancel(
    xwork_task_graph *pGraph,
    const char *sReason
);
```

**参数：**

- `pGraph`：目标 graph。
- `sReason`：可选取消原因。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

graph 复制取消原因字符串。

**补充说明：**

取消不会销毁 graph；调用者仍可查询 snapshot 和 summary。

**范例代码：**

```c
xwork_task_graph_cancel(graph, "user requested stop");
```

**相关 API：**

- `xwork_task_graph_is_cancelled`

---

### xwork_task_graph_is_cancelled

检查 graph 是否已请求取消。

**功能：**

返回 graph 当前取消标记。

**函数原型：**

```c
XWORK_API bool xwork_task_graph_is_cancelled(const xwork_task_graph *pGraph);
```

**参数：**

- `pGraph`：task graph；可为 `NULL`。

**返回值：**

已取消返回 `true`；否则返回 `false`。

**资源归属：**

不分配资源。

**补充说明：**

该函数只检查请求状态，不保证所有运行中任务已经停止。

**范例代码：**

```c
if (xwork_task_graph_is_cancelled(graph)) {
    /* stop launching extra work */
}
```

**相关 API：**

- `xwork_task_graph_cancel`

---

### xwork_task_graph_pause

暂停 task graph。

**功能：**

设置暂停请求，使执行在调度边界停止继续启动新任务。

**函数原型：**

```c
XWORK_API xwork_status xwork_task_graph_pause(
    xwork_task_graph *pGraph,
    const char *sReason
);
```

**参数：**

- `pGraph`：目标 graph。
- `sReason`：可选暂停原因。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

graph 复制暂停原因字符串。

**补充说明：**

暂停不同于取消；恢复后 graph 可继续调度未完成任务。

**范例代码：**

```c
xwork_task_graph_pause(graph, "waiting for approval");
```

**相关 API：**

- `xwork_task_graph_resume`
- `xwork_task_graph_is_paused`

---

### xwork_task_graph_resume

恢复 task graph。

**功能：**

清除暂停请求和暂停原因。

**函数原型：**

```c
XWORK_API xwork_status xwork_task_graph_resume(xwork_task_graph *pGraph);
```

**参数：**

- `pGraph`：目标 graph。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

释放 graph 内保存的暂停原因。

**补充说明：**

恢复不会自动调用 `xwork_task_graph_execute`；调用者需要再次推进执行。

**范例代码：**

```c
xwork_task_graph_resume(graph);
xwork_task_graph_execute(graph, &result);
```

**相关 API：**

- `xwork_task_graph_pause`

---

### xwork_task_graph_is_paused

检查 graph 是否已请求暂停。

**功能：**

返回 graph 当前暂停标记。

**函数原型：**

```c
XWORK_API bool xwork_task_graph_is_paused(const xwork_task_graph *pGraph);
```

**参数：**

- `pGraph`：task graph；可为 `NULL`。

**返回值：**

已暂停返回 `true`；否则返回 `false`。

**资源归属：**

不分配资源。

**补充说明：**

该函数检查请求状态，不代表没有任务正在执行。

**范例代码：**

```c
bool paused = xwork_task_graph_is_paused(graph);
```

**相关 API：**

- `xwork_task_graph_pause`
- `xwork_task_graph_resume`

---

## Handoff

### xwork_handoff_request_options_init

初始化 handoff request options。

**功能：**

准备一个 handoff 请求，用于在两个 task node 之间传递上下文。

**函数原型：**

```c
XWORK_API void xwork_handoff_request_options_init(
    xwork_handoff_request_options *pOptions
);
```

**参数：**

- `pOptions`：要初始化的 options；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源；request API 会复制需要保留的字符串和数组。

**补充说明：**

必须设置 handoff id、from task id 和 to task id。

**范例代码：**

```c
xwork_handoff_request_options opts;
xwork_handoff_request_options_init(&opts);
opts.sHandoffId = "h1";
opts.sFromTaskId = "implement";
opts.sToTaskId = "review";
```

**相关 API：**

- `xwork_task_graph_request_handoff`

---

### xwork_handoff_result_options_init

初始化 handoff result options。

**功能：**

准备 handoff 处理结果，用于接受、拒绝或完成 handoff。

**函数原型：**

```c
XWORK_API void xwork_handoff_result_options_init(
    xwork_handoff_result_options *pOptions
);
```

**参数：**

- `pOptions`：要初始化的 options；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源；resolve API 会复制 message。

**补充说明：**

`eState` 应设置为非 `XWORK_HANDOFF_PENDING` 的最终或中间处理状态。

**范例代码：**

```c
xwork_handoff_result_options opts;
xwork_handoff_result_options_init(&opts);
opts.sHandoffId = "h1";
opts.eState = XWORK_HANDOFF_ACCEPTED;
```

**相关 API：**

- `xwork_task_graph_resolve_handoff`

---

### xwork_handoff_summary_init

初始化 handoff summary。

**功能：**

准备一个 handoff summary，用于接收请求或查询结果。

**函数原型：**

```c
XWORK_API void xwork_handoff_summary_init(xwork_handoff_summary *pSummary);
```

**参数：**

- `pSummary`：要初始化的 summary；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源。

**补充说明：**

默认状态为 pending 语义。

**范例代码：**

```c
xwork_handoff_summary summary;
xwork_handoff_summary_init(&summary);
```

**相关 API：**

- `xwork_task_graph_request_handoff`

---

### xwork_handoff_summary_reset

释放 handoff summary。

**功能：**

释放 handoff summary 中的字符串和引用数组。

**函数原型：**

```c
XWORK_API void xwork_handoff_summary_reset(xwork_handoff_summary *pSummary);
```

**参数：**

- `pSummary`：要释放的 summary；可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放 summary 内部拥有资源，不释放结构体本身。

**补充说明：**

artifact refs、memory refs 和 workspace ids 都按字符串数组释放。

**范例代码：**

```c
xwork_handoff_summary_reset(&summary);
```

**相关 API：**

- `xwork_handoff_summary_init`

---

### xwork_handoff_summary_list_init

初始化 handoff summary 列表。

**功能：**

准备一个空列表，用于接收 graph 中所有 handoff。

**函数原型：**

```c
XWORK_API void xwork_handoff_summary_list_init(xwork_handoff_summary_list *pList);
```

**参数：**

- `pList`：要初始化的列表；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源。

**补充说明：**

调用 `xwork_task_graph_list_handoffs` 前应初始化。

**范例代码：**

```c
xwork_handoff_summary_list list;
xwork_handoff_summary_list_init(&list);
```

**相关 API：**

- `xwork_task_graph_list_handoffs`

---

### xwork_handoff_summary_list_reset

释放 handoff summary 列表。

**功能：**

释放列表中所有 handoff summary 和列表数组。

**函数原型：**

```c
XWORK_API void xwork_handoff_summary_list_reset(xwork_handoff_summary_list *pList);
```

**参数：**

- `pList`：要释放的列表；可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放列表拥有资源，不释放列表结构体本身。

**补充说明：**

释放后列表回到空状态。

**范例代码：**

```c
xwork_handoff_summary_list_reset(&list);
```

**相关 API：**

- `xwork_handoff_summary_reset`

---

### xwork_task_graph_request_handoff

创建 handoff 请求。

**功能：**

在两个 task node 之间记录一个 pending handoff，并可附带 artifact、memory context 和共享 workspace 引用。

**函数原型：**

```c
XWORK_API xwork_status xwork_task_graph_request_handoff(
    xwork_task_graph *pGraph,
    const xwork_handoff_request_options *pOptions,
    xwork_handoff_summary *pSummary
);
```

**参数：**

- `pGraph`：目标 graph。
- `pOptions`：handoff 请求参数。
- `pSummary`：可选输出 summary。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

graph 复制请求内容；summary 如被填充，由调用者 reset。

**补充说明：**

from/to task 必须存在；handoff id 应在 graph 内唯一。

**范例代码：**

```c
xwork_handoff_summary summary;
xwork_handoff_summary_init(&summary);
xwork_task_graph_request_handoff(graph, &opts, &summary);
xwork_handoff_summary_reset(&summary);
```

**相关 API：**

- `xwork_task_graph_resolve_handoff`
- `xwork_task_graph_list_handoffs`

---

### xwork_task_graph_resolve_handoff

处理 handoff。

**功能：**

更新已有 handoff 的状态、状态码和消息。

**函数原型：**

```c
XWORK_API xwork_status xwork_task_graph_resolve_handoff(
    xwork_task_graph *pGraph,
    const xwork_handoff_result_options *pOptions,
    xwork_handoff_summary *pSummary
);
```

**参数：**

- `pGraph`：目标 graph。
- `pOptions`：处理结果；必须包含 handoff id。
- `pSummary`：可选输出更新后的 summary。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

graph 复制 message；summary 如被填充，由调用者 reset。

**补充说明：**

`eState` 不能保持为 `XWORK_HANDOFF_PENDING`。

**范例代码：**

```c
xwork_handoff_result_options result;
xwork_handoff_result_options_init(&result);
result.sHandoffId = "h1";
result.eState = XWORK_HANDOFF_COMPLETED;
xwork_task_graph_resolve_handoff(graph, &result, NULL);
```

**相关 API：**

- `xwork_task_graph_request_handoff`

---

### xwork_task_graph_list_handoffs

列出 graph 中的 handoff。

**功能：**

获取当前 task graph 已记录的 handoff summary 列表。

**函数原型：**

```c
XWORK_API xwork_status xwork_task_graph_list_handoffs(
    const xwork_task_graph *pGraph,
    xwork_handoff_summary_list *pList
);
```

**参数：**

- `pGraph`：源 graph。
- `pList`：输出列表；调用前应 init。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

列表拥有 deep-copy 内容，用 `xwork_handoff_summary_list_reset` 释放。

**补充说明：**

该 API 可用于 UI 显示 pending handoff 或恢复后重建上下文关系。

**范例代码：**

```c
xwork_handoff_summary_list list;
xwork_handoff_summary_list_init(&list);
xwork_task_graph_list_handoffs(graph, &list);
xwork_handoff_summary_list_reset(&list);
```

**相关 API：**

- `xwork_task_graph_request_handoff`

---

## 报告与聚合 Artifact

### xwork_task_graph_emit_agent_result_report

为单个 agent task 生成结果报告 artifact。

**功能：**

把指定 task 的 child run 结果写入调用方提供的 artifact 对象。

**函数原型：**

```c
XWORK_API xwork_status xwork_task_graph_emit_agent_result_report(
    const xwork_task_graph *pGraph,
    const char *sTaskId,
    const char *sArtifactId,
    xwork_artifact *pArtifact
);
```

**参数：**

- `pGraph`：源 graph。
- `sTaskId`：任务 id。
- `sArtifactId`：输出 artifact id。
- `pArtifact`：输出 artifact；调用前应按 artifact API 初始化。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

artifact 内容由输出对象持有，调用者按 artifact API reset。

**补充说明：**

该报告面向单个 task，不替代 run event 审计流。

**范例代码：**

```c
xwork_artifact artifact;
xwork_artifact_init(&artifact);
xwork_task_graph_emit_agent_result_report(graph, "review", "review-report", &artifact);
xwork_artifact_reset(&artifact);
```

**相关 API：**

- `xwork_task_graph_emit_aggregate_report`
- `xwork_artifact_reset`

---

### xwork_task_graph_emit_aggregate_report

生成聚合报告 artifact。

**功能：**

把整个 task graph 的执行结果聚合到指定 run 的 artifact 中。

**函数原型：**

```c
XWORK_API xwork_status xwork_task_graph_emit_aggregate_report(
    const xwork_task_graph *pGraph,
    xwork_run *pRun,
    const char *sArtifactId,
    xwork_artifact *pArtifact
);
```

**参数：**

- `pGraph`：源 graph。
- `pRun`：接收聚合报告的 run。
- `sArtifactId`：输出 artifact id。
- `pArtifact`：输出 artifact。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

artifact 内容由输出对象持有，调用者按 artifact API reset；run 不被接管。

**补充说明：**

聚合报告适合作为 UI 总结或 pipeline 产物索引。

**范例代码：**

```c
xwork_artifact artifact;
xwork_artifact_init(&artifact);
xwork_task_graph_emit_aggregate_report(graph, run, "graph-report", &artifact);
xwork_artifact_reset(&artifact);
```

**相关 API：**

- `xwork_task_graph_emit_agent_result_report`
- `xwork_run`

---

## 相关文档

- [Run API](api-run.md)
- [Artifact API](api-artifacts.md)
- [多 Agent 任务图](../guide/multi-agent-intro.md)
- [多 Agent claw 范例](../case/multi-agent-claw.md)
- [内部 multi-agent contract](../../dev/docs/MULTI_AGENT.md)
