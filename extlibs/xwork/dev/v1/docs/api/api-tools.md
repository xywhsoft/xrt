# Tool API

> 状态：中文逐函数参考，待人工审阅。

Tool API 定义模型可调用工具、工具注册、工具执行器和 host service 桥接。它是 xwork 将模型 tool call 转为可审批、可追踪副作用的入口。

## 模块定位

Tool API 负责统一工具定义、注册、查找和执行入口。它不直接替代产品的文件系统、进程、终端、网络或编辑器实现；这些能力通过 host service 由宿主提供，或通过 xwork local host helper 适配。

## 本页覆盖声明

| 类别 | 声明 |
| --- | --- |
| 结构体 | `xwork_tool_def`, `xwork_tool_call`, `xwork_tool_result`, `xwork_tool_exec_context` |
| 回调 | `xwork_tool_exec_fn`, `xwork_tool_exec_ex_fn` |
| 函数 | `xwork_tool_def_init`, `xwork_tool_result_init`, `xwork_runtime_register_tool`, `xwork_get_builtin_tool_def`, `xwork_runtime_register_builtin_tool`, `xwork_runtime_find_tool`, `xwork_runtime_invoke_host_service`, `xwork_runtime_invoke_host_service_ex`, `xwork_tool_exec_context_should_cancel` |

## 结构体字段

### xwork_tool_def

| 字段 | 说明 |
| --- | --- |
| `sToolId` | 稳定工具 id。注册时必填且非空，注册函数会复制。 |
| `sDisplayName` | 展示名。可选，注册函数会复制。 |
| `sDescription` | 工具说明。可选，注册函数会复制。 |
| `eKind` | 工具类型，默认 `XWORK_TOOL_HOST_SERVICE`。 |
| `eHostService` | host service 类型，host-service 工具需要设置。 |
| `sOperationId` | host operation id。注册时借用，必须在工具注册期间保持有效。 |
| `eSideEffect` | 副作用等级，默认 `XWORK_SIDE_EFFECT_READ_ONLY`。 |
| `eApprovalMode` | 审批模式，默认 `XWORK_APPROVAL_DEFAULT`。 |
| `bSupportsStreaming` | 工具是否支持流式输出。 |

### xwork_tool_call

| 字段 | 说明 |
| --- | --- |
| `sCallId` | 模型生成的 tool call id。借用字符串。 |
| `sToolId` | 工具 id。借用字符串。 |
| `sArgumentsJson` | JSON 参数。借用字符串。 |

### xwork_tool_result

| 字段 | 说明 |
| --- | --- |
| `sOutputText` | 工具返回给模型的文本。由执行方按其 contract 管理生命周期。 |
| `sVisibleSummary` | 给用户界面展示的摘要。由执行方按其 contract 管理生命周期。 |
| `bRetryable` | 失败是否可重试。orchestrator 可据此执行 retry。 |

## 内置工具 ID

主要内置工具包括：

- `filesystem.read_text`
- `filesystem.write_text`
- `filesystem.list`
- `filesystem.stat`
- `filesystem.glob`
- `filesystem.mkdir`
- `filesystem.move`
- `filesystem.delete`
- `filesystem.apply_patch`
- `process.exec`
- `process.start_terminal`
- `process.list_terminals`
- `process.terminal_read`
- `process.terminal_write`
- `process.terminal_resize`
- `process.terminal_stop`
- `vcs.status`
- `vcs.diff`
- `vcs.log`
- `vcs.branch`
- `editor.open_buffer`
- `editor.apply_edit`

## 所有权规则

- `xwork_runtime_register_tool` 会复制 `sToolId`、`sDisplayName`、`sDescription`。
- `xwork_tool_def::sOperationId` 是借用字符串，必须在工具注册期间保持有效。
- `xwork_runtime_find_tool` 和 `xwork_get_builtin_tool_def` 返回借用指针。
- `xwork_tool_result` 本身不拥有字符串；具体字符串生命周期由 tool executor 或 host service contract 决定。
- runtime 销毁时会释放已注册工具记录。

## 常见调用顺序

```text
xwork_runtime_options_init
xwork_runtime_create
xwork_tool_def_init
xwork_runtime_register_tool / xwork_runtime_register_builtin_tool
xwork_runtime_find_tool
xwork_run_execute / xwork_runtime_invoke_host_service
xwork_runtime_destroy
```

---

### xwork_tool_def_init

初始化工具定义。

**功能：**

你可以在注册工具前调用该函数，获得稳定默认值，再填入 tool id、展示名、副作用和 host service 映射。

**函数原型：**

```c
XWORK_API void xwork_tool_def_init(xwork_tool_def *pDef);
```

**参数：**

- `pDef`：输出参数。可为 `NULL`；为 `NULL` 时不执行任何操作。非 `NULL` 时会清零并设置默认枚举值。

**返回值：**

无。

**资源归属：**

函数不分配堆内存。调用者拥有 `pDef` 结构体存储。

**补充说明：**

- 默认 `eKind` 为 `XWORK_TOOL_HOST_SERVICE`。
- 默认 `eHostService` 为 `XWORK_HOST_NONE`。
- 默认 `eSideEffect` 为 `XWORK_SIDE_EFFECT_READ_ONLY`。
- 默认 `eApprovalMode` 为 `XWORK_APPROVAL_DEFAULT`。

**范例代码：**

```c
#include "xwork.h"

int main(void) {
    xwork_tool_def def;
    xwork_tool_def_init(&def);
    def.sToolId = "custom.echo";
    def.sDisplayName = "Echo";
    return 0;
}
```

**相关 API：**

- `xwork_runtime_register_tool`
- `xwork_runtime_find_tool`

---

### xwork_tool_result_init

初始化工具执行结果。

**功能：**

你可以在工具 executor 或 host service 写入结果前调用该函数，确保输出文本、可见摘要和 retry 标志处于已知状态。

**函数原型：**

```c
XWORK_API void xwork_tool_result_init(xwork_tool_result *pResult);
```

**参数：**

- `pResult`：输出参数。可为 `NULL`；为 `NULL` 时不执行任何操作。非 `NULL` 时会整体清零。

**返回值：**

无。

**资源归属：**

函数不分配或释放字符串。`sOutputText` 和 `sVisibleSummary` 的生命周期由写入它们的一方负责。

**补充说明：**

- 当前结构没有 paired reset 函数，因为它不声明拥有堆资源。
- 若 executor 动态分配输出字符串，需要在 executor contract 中定义释放方式，或使用宿主长期有效缓冲区。

**范例代码：**

```c
#include "xwork.h"

static xwork_status echo_tool(
    xwork_run *run,
    const xwork_tool_call *call,
    xwork_tool_result *result,
    void *user_data
) {
    (void)run;
    (void)call;
    (void)user_data;
    xwork_tool_result_init(result);
    result->sOutputText = "ok";
    result->sVisibleSummary = "echo completed";
    return XWORK_OK;
}
```

**相关 API：**

- `xwork_runtime_invoke_host_service`
- `xwork_runtime_invoke_host_service_ex`

---

### xwork_runtime_register_tool

注册自定义工具。

**功能：**

你可以用该函数把产品自定义工具加入 runtime，使 orchestrator 能按 tool id 查找并执行它。

**函数原型：**

```c
XWORK_API xwork_status xwork_runtime_register_tool(
    xwork_runtime *pRuntime,
    const xwork_tool_def *pDef
);
```

**参数：**

- `pRuntime`：输入/输出参数。必须非 `NULL`。
- `pDef`：输入参数。必须非 `NULL`，且 `sToolId` 必须为非空字符串。

**返回值：**

- `XWORK_OK`：注册成功。
- `XWORK_ERROR_INVALID_ARGUMENT`：runtime、definition 或 tool id 无效。
- `XWORK_ERROR_ALREADY_EXISTS`：同一 runtime 已注册相同 tool id。
- `XWORK_ERROR_NO_MEMORY`：工具记录或字符串复制失败。

**资源归属：**

- runtime 拥有注册后的工具记录。
- 函数复制 `sToolId`、`sDisplayName`、`sDescription`。
- 函数借用 `sOperationId`，调用方必须保证其生命周期覆盖工具注册期。

**补充说明：**

- 工具注册应在 run 执行前完成，并由调用方串行化。
- `eSideEffect` 和 `eApprovalMode` 会参与 policy/approval 判断。

**范例代码：**

```c
#include "xwork.h"

int register_echo_tool(xwork_runtime *runtime) {
    xwork_tool_def def;
    xwork_tool_def_init(&def);
    def.sToolId = "custom.echo";
    def.sDisplayName = "Echo";
    def.sDescription = "Return a static response.";
    def.eSideEffect = XWORK_SIDE_EFFECT_READ_ONLY;
    return xwork_runtime_register_tool(runtime, &def) == XWORK_OK ? 0 : 1;
}
```

**相关 API：**

- `xwork_tool_def_init`
- `xwork_runtime_find_tool`
- `xwork_runtime_register_builtin_tool`

---

### xwork_get_builtin_tool_def

获取内置工具定义。

**功能：**

你可以用该函数查看 xwork 已知内置工具的定义，或在注册前检查 tool id 是否为内置工具。

**函数原型：**

```c
XWORK_API const xwork_tool_def *xwork_get_builtin_tool_def(const char *sToolId);
```

**参数：**

- `sToolId`：输入参数。可为 `NULL` 或空字符串；无效或未知时返回 `NULL`。

**返回值：**

- 找到时返回内置工具定义的借用指针。
- 未找到时返回 `NULL`。

**资源归属：**

返回值指向静态存储，调用者不能释放或修改。

**补充说明：**

- 返回定义可传给 `xwork_runtime_register_tool`，但更常见的是直接使用 `xwork_runtime_register_builtin_tool`。
- 内置定义的 `sOperationId` 也是静态借用字符串。

**范例代码：**

```c
#include "xwork.h"

int has_exec_tool(void) {
    return xwork_get_builtin_tool_def(XWORK_TOOL_PROCESS_EXEC) != NULL ? 1 : 0;
}
```

**相关 API：**

- `xwork_runtime_register_builtin_tool`
- `xwork_runtime_find_tool`

---

### xwork_runtime_register_builtin_tool

注册内置工具。

**功能：**

你可以用该函数把 xwork 内置工具定义注册到 runtime。实际执行仍由对应 host service 提供。

**函数原型：**

```c
XWORK_API xwork_status xwork_runtime_register_builtin_tool(
    xwork_runtime *pRuntime,
    const char *sToolId
);
```

**参数：**

- `pRuntime`：输入/输出参数。必须非 `NULL`。
- `sToolId`：输入参数。必须为内置工具 id 非空字符串。

**返回值：**

- `XWORK_OK`：注册成功。
- `XWORK_ERROR_INVALID_ARGUMENT`：runtime 或 tool id 无效。
- `XWORK_ERROR_NOT_FOUND`：不存在该内置工具。
- `XWORK_ERROR_ALREADY_EXISTS`：同一 runtime 已注册相同 tool id。
- `XWORK_ERROR_NO_MEMORY`：工具记录或字符串复制失败。

**资源归属：**

注册后的工具记录由 runtime 拥有。内置定义本身仍为静态借用数据。

**补充说明：**

- 注册内置工具不等于安装实际能力；还需要配置 `xwork_host_services` 或 local host。
- 文件写入、process、terminal 等工具仍应经过 policy/approval gate。

**范例代码：**

```c
#include "xwork.h"

int register_read_text(xwork_runtime *runtime) {
    return xwork_runtime_register_builtin_tool(
        runtime,
        XWORK_TOOL_FILESYSTEM_READ_TEXT
    ) == XWORK_OK ? 0 : 1;
}
```

**相关 API：**

- `xwork_get_builtin_tool_def`
- `xwork_runtime_register_tool`
- `xwork_runtime_find_tool`

---

### xwork_runtime_find_tool

按 tool id 查找已注册工具。

**功能：**

你可以用该函数在执行前检查 runtime 是否已注册某个工具，也可用于调试模型请求的 tool id 是否可解析。

**函数原型：**

```c
XWORK_API const xwork_tool_def *xwork_runtime_find_tool(
    const xwork_runtime *pRuntime,
    const char *sToolId
);
```

**参数：**

- `pRuntime`：输入参数。可为 `NULL`；无效时返回 `NULL`。
- `sToolId`：输入参数。可为 `NULL` 或空字符串；无效时返回 `NULL`。

**返回值：**

- 找到时返回工具定义借用指针。
- 未找到或参数无效时返回 `NULL`。

**资源归属：**

返回值由 runtime 拥有，调用者不能释放。runtime 销毁或工具记录被释放后指针失效。

**补充说明：**

- 匹配规则是 tool id 字符串精确匹配。
- 该函数不触发工具执行。

**范例代码：**

```c
#include "xwork.h"

int runtime_has_tool(const xwork_runtime *runtime, const char *tool_id) {
    return xwork_runtime_find_tool(runtime, tool_id) != NULL ? 1 : 0;
}
```

**相关 API：**

- `xwork_runtime_register_tool`
- `xwork_runtime_register_builtin_tool`

---

### xwork_runtime_invoke_host_service

调用 runtime 配置的 host service。

**功能：**

你可以用该函数直接调用宿主提供的 host service，实现文件、进程、VCS、诊断或编辑器操作。orchestrator 执行 host-service 工具时也会进入同一边界。

**函数原型：**

```c
XWORK_API xwork_status xwork_runtime_invoke_host_service(
    const xwork_runtime *pRuntime,
    xwork_host_service_kind eKind,
    const char *sOperationId,
    const char *sRequestJson,
    xwork_tool_result *pResult
);
```

**参数：**

- `pRuntime`：输入参数。必须非 `NULL`，并且已配置对应 host service。
- `eKind`：输入参数。host service 类型，例如 `XWORK_HOST_FILESYSTEM` 或 `XWORK_HOST_PROCESS`。
- `sOperationId`：输入参数。必须为非空字符串，由具体 host service 解释。
- `sRequestJson`：输入参数。可为 `NULL`，但通常应为 JSON 请求字符串。
- `pResult`：输出参数。必须非 `NULL`。函数会写入工具结果。

**返回值：**

- `XWORK_OK`：host service 调用成功。
- `XWORK_ERROR_INVALID_ARGUMENT`：参数无效。
- `XWORK_ERROR_NOT_FOUND`：未配置对应 host service 或 operation 不存在。
- `XWORK_ERROR_EXTERNAL_FAILURE`：host service 或底层 xrt/xllm 调用失败。
- `XWORK_ERROR_CANCELLED`：底层执行观察到取消。

**资源归属：**

`pResult` 由调用者拥有；结果字段指针生命周期由 host service contract 决定。runtime 不转移 host service 的 `pUserData` 所有权。

**补充说明：**

- 该函数使用无 context 调用路径。
- 如果需要 cancel token、run 指针或 interrupt callback，应使用 `xwork_runtime_invoke_host_service_ex`。
- 若 runtime 绑定 replay engine，host service 调用可能被记录或由 replay cassette 回放。

**范例代码：**

```c
#include "xwork.h"

int read_readme(xwork_runtime *runtime) {
    xwork_tool_result result;
    xwork_tool_result_init(&result);
    return xwork_runtime_invoke_host_service(
        runtime,
        XWORK_HOST_FILESYSTEM,
        XWORK_HOST_FILESYSTEM_READ_TEXT,
        "{\"path\":\"README.md\"}",
        &result
    ) == XWORK_OK ? 0 : 1;
}
```

**相关 API：**

- `xwork_runtime_invoke_host_service_ex`
- `xwork_host_services_init`
- `xwork_local_host_configure_services`

---

### xwork_runtime_invoke_host_service_ex

带上下文调用 runtime 配置的 host service。

**功能：**

你可以用该函数在 host service 调用中传递 run、cancel token 和 interrupt callback，使长耗时文件/进程/终端操作支持协作式取消。

**函数原型：**

```c
XWORK_API xwork_status xwork_runtime_invoke_host_service_ex(
    const xwork_runtime *pRuntime,
    xwork_host_service_kind eKind,
    const char *sOperationId,
    const char *sRequestJson,
    const xwork_host_invoke_context *pContext,
    xwork_tool_result *pResult
);
```

**参数：**

- `pRuntime`：输入参数。必须非 `NULL`。
- `eKind`：输入参数。host service 类型。
- `sOperationId`：输入参数。必须为非空字符串。
- `sRequestJson`：输入参数。可为 `NULL`，但通常应为 JSON 请求字符串。
- `pContext`：输入参数。可为 `NULL`；非 `NULL` 时提供取消和 interrupt 信息。
- `pResult`：输出参数。必须非 `NULL`。

**返回值：**

- `XWORK_OK`：host service 调用成功。
- `XWORK_ERROR_INVALID_ARGUMENT`：参数无效。
- `XWORK_ERROR_NOT_FOUND`：未配置对应 host service 或 operation 不存在。
- `XWORK_ERROR_EXTERNAL_FAILURE`：host service 或底层依赖失败。
- `XWORK_ERROR_CANCELLED`：context 中的取消或 interrupt 被观察到。

**资源归属：**

context、request JSON 和 result 结构都由调用者拥有。函数不接管 cancel token 或 user data 所有权。

**补充说明：**

- 如果 host service 提供 `pfnInvokeEx`，优先调用扩展入口；否则会退回基础入口。
- 长耗时 host service 应在关键阶段调用 `xwork_host_invoke_context_should_cancel`。

**范例代码：**

```c
#include "xwork.h"

int invoke_with_context(
    xwork_runtime *runtime,
    const xwork_host_invoke_context *context
) {
    xwork_tool_result result;
    xwork_tool_result_init(&result);
    return xwork_runtime_invoke_host_service_ex(
        runtime,
        XWORK_HOST_PROCESS,
        XWORK_HOST_PROCESS_EXEC,
        "{\"command\":\"git status\"}",
        context,
        &result
    ) == XWORK_OK ? 0 : 1;
}
```

**相关 API：**

- `xwork_runtime_invoke_host_service`
- `xwork_host_invoke_context_should_cancel`
- `xwork_tool_exec_context_should_cancel`

---

### xwork_tool_exec_context_should_cancel

检查工具执行上下文是否请求取消。

**功能：**

你可以在自定义工具 executor 的关键阶段调用该函数，统一检查 cancel token 和 interrupt callback。

**函数原型：**

```c
XWORK_API bool xwork_tool_exec_context_should_cancel(
    const xwork_run *pRun,
    const xwork_tool_exec_context *pContext,
    const char *sPhase
);
```

**参数：**

- `pRun`：输入参数。可为 `NULL`。传给 interrupt callback，用于判断当前 run 是否应中断。
- `pContext`：输入参数。可为 `NULL`；为 `NULL` 时返回 `false`。
- `sPhase`：输入参数。可为 `NULL`；用于描述当前检查阶段。

**返回值：**

- 返回 `true` 表示工具应尽快停止并返回 `XWORK_ERROR_CANCELLED`。
- 返回 `false` 表示当前未观察到取消。

**资源归属：**

该函数不分配资源，不接管 context 中 cancel token 或 user data 所有权。

**补充说明：**

- 函数会检查 context 中的 cancel token 和 interrupt callback。
- 工具 executor 应在 spawn 前、长循环中、外部 IO 返回后分别检查，避免取消延迟过长。

**范例代码：**

```c
#include "xwork.h"

static xwork_status long_tool(
    xwork_run *run,
    const xwork_tool_call *call,
    const xwork_tool_exec_context *context,
    xwork_tool_result *result,
    void *user_data
) {
    (void)call;
    (void)user_data;
    xwork_tool_result_init(result);
    if (xwork_tool_exec_context_should_cancel(run, context, "before_work")) {
        return XWORK_ERROR_CANCELLED;
    }
    result->sOutputText = "done";
    return XWORK_OK;
}
```

**相关 API：**

- `xwork_runtime_invoke_host_service_ex`
- `xwork_host_invoke_context_should_cancel`
- `xwork_run_async_cancel`

## 错误处理

- `XWORK_ERROR_INVALID_ARGUMENT`：tool id、definition、runtime、operation 或 result 参数无效。
- `XWORK_ERROR_ALREADY_EXISTS`：重复注册同一 tool id。
- `XWORK_ERROR_NOT_FOUND`：查找或执行不存在的工具/host service。
- `XWORK_ERROR_CANCELLED`：执行上下文观察到取消。
- `XWORK_ERROR_EXTERNAL_FAILURE`：host service 或底层 xrt/xllm 失败。

## 恢复边界

run snapshot 可以保存 pending tool id、arguments 和审批状态，但不会保存 callback 栈、live host handle 或 executor 局部变量。恢复后必须重新注册相同 tool id 和兼容 host service。

## 线程边界

工具注册应在执行前完成并由调用方串行化。工具执行 callback 可能被多 agent 或异步路径并发调用，callback 自己必须保护共享 `pUserData` 和外部资源。

## 相关文档

- [工具、审批与 artifact](../guide/tool-approval-artifact-intro.md)
- [Orchestrator API](api-orchestrator.md)
- [Run API](api-run.md)
- [内部 host tool contract](../../dev/docs/HOST_TOOL_CONTRACTS.md)
