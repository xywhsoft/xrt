# Runtime API

> 状态：中文逐函数参考，待人工审阅。

`xwork_runtime` 是 xwork 的顶层对象，负责持有工作区、工具注册表、run、策略、host services、persistence backend、可选 xllm runtime 和 replay engine。

## 模块定位

Runtime 负责组织 xwork 内部共享状态。它不负责 UI、网络服务生命周期、具体模型 provider 协议或产品级 planner 策略。xllm 相关 getter 详见 [xllm 集成 API](api-xllm-integration.md)，工具/host service 调用详见 [Tool API](api-tools.md)，持久化 facade 后续归入 persistence 文档。

## 本页覆盖声明

| 类别 | 声明 |
| --- | --- |
| 不透明对象 | `xwork_runtime` |
| 结构体 | `xwork_runtime_options` |
| 函数 | `xwork_runtime_options_init`, `xwork_runtime_create`, `xwork_runtime_destroy`, `xwork_runtime_get_host_services`, `xwork_runtime_get_persistence_backend`, `xwork_runtime_get_policy_options`, `xwork_runtime_get_workspace_count`, `xwork_runtime_get_tool_count`, `xwork_runtime_get_run_count` |

## xwork_runtime_options 字段

| 字段 | 说明 |
| --- | --- |
| `pLlmRuntime` | 借用的 `xllm_runtime *`。不能和 `pLlmBootstrap` 同时设置。 |
| `pLlmBootstrap` | xllm bootstrap 配置。create 时读取；若创建成功，xwork runtime 拥有生成的 `xllm_runtime`。 |
| `pReplayEngine` | 借用的 replay engine。host service record/replay 期间必须保持有效。 |
| `pHostServices` | host service 表。结构体按值复制，callback 与 user data 借用。 |
| `pPersistenceBackend` | persistence backend 表。结构体按值复制，callback 与 user data 借用。 |
| `tPolicy` | runtime 默认 policy。 |
| `pUserData` | runtime 级用户数据指针，xwork 不解释且不释放。 |

## 核心所有权

- `xwork_runtime_create` 返回 owned runtime，使用 `xwork_runtime_destroy` 释放。
- runtime destroy 会释放仍附着的 workspace、tool 和 run。
- 使用 `pLlmRuntime` 时，xwork 只借用现有 xllm runtime。
- 使用 `pLlmBootstrap` 时，xwork 创建并拥有 xllm runtime。
- `pHostServices` / `pPersistenceBackend` 的结构体会按值复制，但回调函数和 `pUserData` 借用。
- `pReplayEngine` 借用，不由 runtime 销毁。

## 常见调用顺序

```text
xwork_runtime_options_init
填充 host/persistence/xllm/policy options
xwork_runtime_create
xwork_runtime_add_workspace
xwork_runtime_register_builtin_tool
xwork_run_create
xwork_runtime_destroy
```

---

### xwork_runtime_options_init

初始化 runtime options。

**功能：**

你可以在创建 runtime 前调用该函数，把 xllm、host service、persistence、policy 和 user data 字段置为稳定默认值。

**函数原型：**

```c
XWORK_API void xwork_runtime_options_init(xwork_runtime_options *pOptions);
```

**参数：**

- `pOptions`：输出参数。可为 `NULL`；为 `NULL` 时不执行任何操作。非 `NULL` 时会清零并初始化嵌套 policy。

**返回值：**

无。

**资源归属：**

函数不分配资源。所有指针字段初始化为 `NULL`。

**补充说明：**

- `pLlmRuntime` 和 `pLlmBootstrap` 二选一，不能同时设置。
- 调用 profile apply 函数时，建议先 init，再 apply profile，再做产品层覆盖。

**范例代码：**

```c
#include "xwork.h"

int main(void) {
    xwork_runtime_options options;
    xwork_runtime_options_init(&options);
    return 0;
}
```

**相关 API：**

- `xwork_runtime_create`
- `xwork_profile_apply_runtime_options`

---

### xwork_runtime_create

创建 runtime。

**功能：**

你可以用该函数创建 xwork 顶层对象，并把 xllm runtime、host services、persistence backend、replay engine 和默认 policy 接入后续 agent run。

**函数原型：**

```c
XWORK_API xwork_status xwork_runtime_create(
    const xwork_runtime_options *pOptions,
    xwork_runtime **ppRuntime
);
```

**参数：**

- `pOptions`：输入参数。可为 `NULL`；为 `NULL` 时使用默认配置。
- `ppRuntime`：输出参数。必须非 `NULL`。成功时接收 owned runtime；失败时保持为 `NULL`。

**返回值：**

- `XWORK_OK`：创建成功。
- `XWORK_ERROR_INVALID_ARGUMENT`：输出指针为空，或同时设置了 `pLlmRuntime` 与 `pLlmBootstrap`。
- `XWORK_ERROR_NO_MEMORY`：runtime 分配失败。
- 其他 `xwork_status`：xllm bootstrap 失败时透传 bootstrap 路径返回的状态。

**资源归属：**

- 成功后 `*ppRuntime` 由调用者拥有，必须用 `xwork_runtime_destroy` 释放。
- 如果通过 `pLlmBootstrap` 创建 xllm runtime，xwork runtime 拥有它。
- 如果传入 `pLlmRuntime`，xwork runtime 只借用它。
- host services、persistence backend 结构体按值复制，但其内部 callback/user data 借用。

**补充说明：**

- 创建 runtime 不会自动注册 workspace、tool 或 run。
- 创建 runtime 不会自动从 persistence 恢复状态。

**范例代码：**

```c
#include "xwork.h"

int main(void) {
    xwork_runtime_options options;
    xwork_runtime *runtime = NULL;

    xwork_runtime_options_init(&options);
    if (xwork_runtime_create(&options, &runtime) != XWORK_OK) {
        return 1;
    }

    xwork_runtime_destroy(runtime);
    return 0;
}
```

**相关 API：**

- `xwork_runtime_options_init`
- `xwork_runtime_destroy`
- `xwork_runtime_add_workspace`

---

### xwork_runtime_destroy

销毁 runtime。

**功能：**

你可以用该函数释放 runtime 以及仍附着在 runtime 下的 run、workspace 和 tool 记录。

**函数原型：**

```c
XWORK_API void xwork_runtime_destroy(xwork_runtime *pRuntime);
```

**参数：**

- `pRuntime`：输入/销毁参数。可为 `NULL`；为 `NULL` 时不执行任何操作。

**返回值：**

无。

**资源归属：**

- 函数释放 runtime 自身。
- 函数释放仍附着的 run、workspace、tool 记录。
- 如果 runtime 拥有 bootstrap 创建的 xllm runtime，函数会销毁它。
- 函数不会销毁借用的 `pLlmRuntime`、`pReplayEngine`、host service user data 或 persistence backend user data。

**补充说明：**

- 不要在异步 run 仍执行时销毁 runtime，除非产品层已经完成取消和 wait。
- 销毁后，所有通过 runtime 获取的借用指针都失效。

**范例代码：**

```c
#include "xwork.h"

void close_runtime(xwork_runtime *runtime) {
    xwork_runtime_destroy(runtime);
}
```

**相关 API：**

- `xwork_runtime_create`
- `xwork_run_async_cancel`
- `xwork_run_async_wait`

---

### xwork_runtime_get_host_services

获取 runtime 当前 host service 表。

**功能：**

你可以用该函数检查 runtime 配置的 host service callback，用于诊断、桥接或高级宿主集成。

**函数原型：**

```c
XWORK_API const xwork_host_services *xwork_runtime_get_host_services(const xwork_runtime *pRuntime);
```

**参数：**

- `pRuntime`：输入参数。可为 `NULL`；为 `NULL` 时返回 `NULL`。

**返回值：**

- 返回 host service 表借用指针。
- 如果 `pRuntime` 为 `NULL`，返回 `NULL`。

**资源归属：**

返回值由 runtime 拥有，调用者不能释放或在 runtime 销毁后继续使用。

**补充说明：**

- 返回的是 runtime 内部复制的 host service 表。
- callback 和 user data 的生命周期仍由调用方保证。

**范例代码：**

```c
#include "xwork.h"

int has_host_services(const xwork_runtime *runtime) {
    return xwork_runtime_get_host_services(runtime) != NULL ? 1 : 0;
}
```

**相关 API：**

- `xwork_host_services_init`
- `xwork_runtime_invoke_host_service`

---

### xwork_runtime_get_persistence_backend

获取 runtime 当前 persistence backend。

**功能：**

你可以用该函数检查 runtime 是否配置了持久化 backend，或在诊断 UI 中展示当前持久化能力边界。

**函数原型：**

```c
XWORK_API const xwork_persistence_backend *xwork_runtime_get_persistence_backend(
    const xwork_runtime *pRuntime
);
```

**参数：**

- `pRuntime`：输入参数。可为 `NULL`；为 `NULL` 时返回 `NULL`。

**返回值：**

- 返回 persistence backend 借用指针。
- 如果 `pRuntime` 为 `NULL`，返回 `NULL`。

**资源归属：**

返回值由 runtime 拥有，调用者不能释放。backend 内部 callback/user data 仍由调用方管理。

**补充说明：**

- 返回指针存在不代表所有 persistence callback 都已配置。
- 具体存储/加载能力应按 backend contract 判断。

**范例代码：**

```c
#include "xwork.h"

int has_persistence_backend(const xwork_runtime *runtime) {
    return xwork_runtime_get_persistence_backend(runtime) != NULL ? 1 : 0;
}
```

**相关 API：**

- `xwork_persistence_backend_init`
- `xwork_file_persistence_configure_backend`

---

### xwork_runtime_get_policy_options

复制 runtime 当前 policy options。

**功能：**

你可以用该函数读取 runtime 默认 policy，供 UI 展示、诊断报告或动态决策使用。

**函数原型：**

```c
XWORK_API xwork_status xwork_runtime_get_policy_options(
    const xwork_runtime *pRuntime,
    xwork_policy_options *pOptions
);
```

**参数：**

- `pRuntime`：输入参数。必须非 `NULL`。
- `pOptions`：输出参数。必须非 `NULL`。成功时接收 policy options 的按值副本。

**返回值：**

- `XWORK_OK`：复制成功。
- `XWORK_ERROR_INVALID_ARGUMENT`：runtime 或输出指针为空。

**资源归属：**

函数按值复制 policy options，不分配堆资源。若 policy 中包含借用字符串，副本仍保持借用语义。

**补充说明：**

- 修改返回副本不会修改 runtime 内部 policy。
- 若需要改变 runtime policy，当前公共 API 不提供运行时 mutation，应在创建 runtime 前配置。

**范例代码：**

```c
#include "xwork.h"

int read_policy(const xwork_runtime *runtime) {
    xwork_policy_options policy;
    return xwork_runtime_get_policy_options(runtime, &policy) == XWORK_OK ? 0 : 1;
}
```

**相关 API：**

- `xwork_policy_options_init`
- `xwork_profile_apply_runtime_options`

---

### xwork_runtime_get_workspace_count

获取 runtime 中已注册 workspace 数量。

**功能：**

你可以用该函数在诊断、测试或 UI 中展示当前 runtime 管理的 workspace 数。

**函数原型：**

```c
XWORK_API size_t xwork_runtime_get_workspace_count(const xwork_runtime *pRuntime);
```

**参数：**

- `pRuntime`：输入参数。可为 `NULL`；为 `NULL` 时返回 `0`。

**返回值：**

返回 workspace 数量。参数无效时返回 `0`。

**资源归属：**

该函数不返回指针，不转移所有权。

**补充说明：**

- 统计的是当前附着在 runtime 链表中的 workspace。
- 并发注册/销毁 workspace 时，调用方应自行串行化。

**范例代码：**

```c
#include "xwork.h"

size_t workspace_count(const xwork_runtime *runtime) {
    return xwork_runtime_get_workspace_count(runtime);
}
```

**相关 API：**

- `xwork_runtime_add_workspace`
- `xwork_runtime_find_workspace`

---

### xwork_runtime_get_tool_count

获取 runtime 中已注册 tool 数量。

**功能：**

你可以用该函数检查当前 runtime 是否已完成工具注册，或在测试中断言内置工具数量。

**函数原型：**

```c
XWORK_API size_t xwork_runtime_get_tool_count(const xwork_runtime *pRuntime);
```

**参数：**

- `pRuntime`：输入参数。可为 `NULL`；为 `NULL` 时返回 `0`。

**返回值：**

返回已注册 tool 数量。参数无效时返回 `0`。

**资源归属：**

该函数不返回指针，不转移所有权。

**补充说明：**

- 统计的是 runtime 当前工具注册表中的记录。
- 并发注册 tool 时，调用方应自行串行化。

**范例代码：**

```c
#include "xwork.h"

size_t tool_count(const xwork_runtime *runtime) {
    return xwork_runtime_get_tool_count(runtime);
}
```

**相关 API：**

- `xwork_runtime_register_tool`
- `xwork_runtime_register_builtin_tool`

---

### xwork_runtime_get_run_count

获取 runtime 中仍附着的 run 数量。

**功能：**

你可以用该函数在诊断、测试或资源管理逻辑中观察 runtime 当前持有的 run 数。

**函数原型：**

```c
XWORK_API size_t xwork_runtime_get_run_count(const xwork_runtime *pRuntime);
```

**参数：**

- `pRuntime`：输入参数。可为 `NULL`；为 `NULL` 时返回 `0`。

**返回值：**

返回 runtime 当前 run 数量。参数无效时返回 `0`。

**资源归属：**

该函数不返回指针，不转移所有权。

**补充说明：**

- 统计的是仍附着在 runtime 的 live run，不是 persistence 中的历史 run。
- 若需要查询持久化 run，请使用 persistence API。

**范例代码：**

```c
#include "xwork.h"

size_t live_run_count(const xwork_runtime *runtime) {
    return xwork_runtime_get_run_count(runtime);
}
```

**相关 API：**

- `xwork_run_create`
- `xwork_run_destroy`

## 错误处理

- `XWORK_ERROR_INVALID_ARGUMENT`：输出指针为空、runtime 为空或互斥 options 同时设置。
- `XWORK_ERROR_NO_MEMORY`：runtime 分配失败。
- `XWORK_ERROR_EXTERNAL_FAILURE`：bootstrap 或外部依赖失败时可能由下层返回。

## 恢复边界

runtime 本身不从磁盘自动恢复全部状态。调用方需要配置 persistence backend，并显式调用恢复 API。host services、xllm runtime、replay engine 和 callback user data 需要由调用方重新提供。

## 线程边界

runtime 不是通用并发容器。注册 workspace、tool、run 或销毁 runtime 时，应由调用方串行化。异步 run 只同步 async handle 自身状态，不代表 runtime 任意 mutation 都线程安全。

## 相关文档

- [Workspace API](api-workspace.md)
- [Tool API](api-tools.md)
- [Run API](api-run.md)
- [xllm 集成 API](api-xllm-integration.md)
