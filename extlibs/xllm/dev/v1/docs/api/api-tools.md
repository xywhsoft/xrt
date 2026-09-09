# xllm Tools API

> Tools API 让模型可以请求宿主执行工具，并把工具结果作为下一轮输入继续对话。

[返回 API 索引](README.md) | [Request / Response API](api-request-response.md) | [Session API](api-session.md)

---

## 目录

- [模块定位](#模块定位)
- [核心概念](#核心概念)
- [工具定义类型](#工具定义类型)
- [工具选择策略](#工具选择策略)
- [工具执行类型](#工具执行类型)
- [Turn 工具 API](#turn-工具-api)
  - [xllm_turn_add_tool](#xllm_turn_add_tool)
  - [xllm_turn_set_tool_choice](#xllm_turn_set_tool_choice)
- [执行器设置 API](#执行器设置-api)
  - [xllm_set_tool_executor](#xllm_set_tool_executor)
  - [xllm_set_tool_executor_async](#xllm_set_tool_executor_async)
  - [xllm_session_set_tool_executor](#xllm_session_set_tool_executor)
  - [xllm_session_set_tool_executor_async](#xllm_session_set_tool_executor_async)
- [工具结果释放](#工具结果释放)
  - [xllm_tool_exec_result_free](#xllm_tool_exec_result_free)
- [自动 Tool Loop](#自动-tool-loop)
- [范例代码](#范例代码)
- [常见错误](#常见错误)

---

## 模块定位

工具调用分为两种层次：

- **模型生成 tool call**：response 中出现 `XLLM_OUTPUT_TOOL_CALL`，宿主自己读取并执行。
- **xllm 自动 tool loop**：你设置 executor 后，`xllm_send_ex` 或 `xllm_session_chat_ex` 可以自动执行 client tool，并把 tool result 追加到请求继续调用模型。

xllm 只负责协议和循环，不负责真实工具业务。文件、进程、终端、权限、审批和审计仍属于宿主或 xwork。

---

## 核心概念

| 概念 | 类型 | 说明 |
| --- | --- | --- |
| 工具定义 | `xllm_tool_def` | 告诉模型有哪些工具、工具名、描述和输入 schema。 |
| 工具策略 | `xllm_tool_policy` | 控制自动、禁用、强制或指定工具。 |
| tool call | `xllm_output_tool_call` | 模型请求执行某个工具。 |
| 执行请求 | `xllm_tool_exec_request` | xllm 传给宿主 executor 的请求。 |
| 执行结果 | `xllm_tool_exec_result` | 宿主返回给 xllm 的工具结果。 |
| 同步执行器 | `xllm_tool_executor` | 直接执行并返回结果。 |
| 异步执行器 | `xllm_tool_executor_async` | 返回 `xfuture *`，xllm 等待并克隆结果。 |

---

## 工具定义类型

### `xllm_tool_def`

| 字段 | 说明 |
| --- | --- |
| `sToolId` | 宿主内部工具 ID。自动 tool loop 用它匹配工具。 |
| `sWireName` | 发给 provider 的工具名。通常是模型看到的函数名。 |
| `sDescription` | 给模型看的工具说明。 |
| `eKind` | `XLLM_TOOL_CLIENT` 或 `XLLM_TOOL_PROVIDER`。 |
| `tInputSchema` | 工具参数 JSON schema，使用 `xvalue` 表达。 |
| `tVendorExtra` | provider 扩展字段。 |

### `xllm_tool_kind`

| 值 | 说明 |
| --- | --- |
| `XLLM_TOOL_CLIENT` | 工具由宿主执行。自动 tool loop 只会执行这类工具。 |
| `XLLM_TOOL_PROVIDER` | 工具由 provider 原生处理，xllm 不调用宿主 executor。 |

**补充说明：**

- `sToolId` 用于宿主内部识别，建议稳定且带命名空间，例如 `xwork.workspace.inspect`。
- `sWireName` 应满足 provider 对 function/tool name 的格式要求。
- `tInputSchema` 越准确，模型越容易生成可执行参数。

---

## 工具选择策略

### `xllm_tool_policy`

| 字段 | 说明 |
| --- | --- |
| `eMode` | 工具选择模式。 |
| `sToolName` | `XLLM_TOOL_CHOICE_NAMED` 时指定工具名。 |
| `bAllowParallel` | 是否允许并行 tool call。 |

### `xllm_tool_choice_mode`

| 值 | 说明 |
| --- | --- |
| `XLLM_TOOL_CHOICE_AUTO` | 让模型自行决定是否调用工具。 |
| `XLLM_TOOL_CHOICE_NONE` | 禁止工具调用。 |
| `XLLM_TOOL_CHOICE_REQUIRED` | 要求模型必须调用工具。 |
| `XLLM_TOOL_CHOICE_NAMED` | 要求模型调用指定工具。 |

**补充说明：**

- 不是所有 provider 都支持 `required` 或 named tool forcing。
- `bAllowParallel=true` 也需要 provider 和模型支持并行 tool call。
- 如果 provider 不支持某个策略，通常应返回 `XLLM_ERROR_UNSUPPORTED_CAPABILITY`。

---

## 工具执行类型

### `xllm_tool_exec_request`

| 字段 | 说明 |
| --- | --- |
| `sToolId` | 匹配到的宿主工具 ID。 |
| `sWireName` | wire/tool name。 |
| `sCallId` | 本次 tool call ID，回传 tool result 时用于关联。 |
| `sArgumentsJson` | 模型生成的 JSON 参数字符串。 |
| `tContinuation` | provider continuation 信息。 |
| `tVendorExtra` | 扩展字段。 |

**资源归属：**

这些字段在 executor 调用期间有效。需要异步保存时必须复制。

### `xllm_tool_exec_result`

| 字段 | 说明 |
| --- | --- |
| `pParts` / `iPartCount` | 工具返回内容，通常是文本或 JSON part。 |
| `tVendorExtra` | 扩展字段。 |

**资源归属：**

宿主 executor 填充 `pResult`。调用链结束后用 `xllm_tool_exec_result_free()` 释放内部资源。

### Executor typedef

```c
typedef int32 (*xllm_tool_execute_fn)(
    void *pCtx,
    const xllm_tool_exec_request *pRequest,
    xllm_tool_exec_result *pResult,
    xllm_error *pError
);

typedef xfuture *(*xllm_tool_execute_async_fn)(
    void *pCtx,
    const xllm_tool_exec_request *pRequest
);
```

**返回值：**

- 同步 executor 返回 `XRT_NET_OK` 表示工具成功。
- 非 `XRT_NET_OK` 表示工具失败；可通过 `pError` 写入原因。
- 异步 executor 返回的 future 应按 xrt future 规则给出 `xllm_tool_exec_result` 或错误。

---

## Turn 工具 API

### xllm_turn_add_tool

向本轮 turn 添加一个工具定义。

**函数原型：**

```c
XLLM_API int xllm_turn_add_tool(xllm_turn *pTurn, const xllm_tool_def *pTool);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `pTurn` | 输入/输出 | 否 | 已初始化的 turn。 |
| `pTool` | 输入 | 否 | 工具定义。 |

**返回值：**

- `XRT_NET_OK`：添加成功。
- `XRT_NET_ERROR`：参数错误或克隆失败。

**资源归属：**

turn 会克隆工具定义。原始 `pTool` 仍由调用者持有。

**补充说明：**

- 如果没有添加工具，模型通常不会收到工具列表。
- 工具定义只对本次 turn 有效；session 是否保留工具调用结果取决于 session commit。

**范例代码：**

```c
xllm_tool_def tool;
memset(&tool, 0, sizeof(tool));
tool.sToolId = "xwork.workspace.inspect";
tool.sWireName = "inspect_workspace";
tool.sDescription = "Inspect workspace state.";
tool.eKind = XLLM_TOOL_CLIENT;

xllm_turn_add_tool(&turn, &tool);
```

**相关 API：**

- `xllm_turn_set_tool_choice`
- `xllm_session_set_tool_executor`

---

### xllm_turn_set_tool_choice

设置本轮工具选择策略。

**函数原型：**

```c
XLLM_API int xllm_turn_set_tool_choice(
    xllm_turn *pTurn,
    xllm_tool_choice_mode eMode,
    const char *sToolName,
    bool bAllowParallel
);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `pTurn` | 输入/输出 | 否 | 已初始化 turn。 |
| `eMode` | 输入 | 否 | 工具选择模式。 |
| `sToolName` | 输入 | 可为 `NULL` | named 模式下的工具名。函数会复制。 |
| `bAllowParallel` | 输入 | 否 | 是否允许并行工具调用。 |

**返回值：**

- `XRT_NET_OK`：设置成功。
- `XRT_NET_ERROR`：参数错误或复制失败。

**资源归属：**

`sToolName` 会被复制到 turn 内部，随 `xllm_turn_reset` 释放。

**补充说明：**

- `XLLM_TOOL_CHOICE_NAMED` 通常需要 `sToolName`。
- provider 不支持的 tool choice 会在请求验证或调用时失败。

**范例代码：**

```c
xllm_turn_set_tool_choice(&turn, XLLM_TOOL_CHOICE_AUTO, NULL, false);
```

**相关 API：**

- `xllm_turn_add_tool`

---

## 执行器设置 API

### xllm_set_tool_executor

给轻量 `xllm` 对象设置同步工具执行器。

**函数原型：**

```c
XLLM_API int xllm_set_tool_executor(xllm *pLlm, const xllm_tool_executor *pExecutor);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `pLlm` | 输入/输出 | 否 | `xllm_create` 创建的对象。 |
| `pExecutor` | 输入 | 可为 `NULL` | 执行器。传 `NULL` 清除执行器。 |

**返回值：**

- `XRT_NET_OK`：设置成功。
- `XRT_NET_ERROR`：`pLlm` 为 `NULL`。

**资源归属：**

xllm 复制 executor 结构体，但不接管 `pExecutor->pCtx`。

**相关 API：**

- `xllm_send_ex`

---

### xllm_set_tool_executor_async

给轻量 `xllm` 对象设置异步工具执行器。

**函数原型：**

```c
XLLM_API int xllm_set_tool_executor_async(xllm *pLlm, const xllm_tool_executor_async *pExecutor);
```

参数、返回值和资源归属与 `xllm_set_tool_executor` 类似，只是回调返回 `xfuture *`。

---

### xllm_session_set_tool_executor

给 session 设置同步工具执行器。

**函数原型：**

```c
XLLM_API int xllm_session_set_tool_executor(xllm_session *pSession, const xllm_tool_executor *pExecutor);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `pSession` | 输入/输出 | 否 | session 对象。 |
| `pExecutor` | 输入 | 可为 `NULL` | 执行器。传 `NULL` 清除。 |

**返回值：**

- `XRT_NET_OK`：设置成功。
- `XRT_NET_ERROR`：`pSession` 为 `NULL`。

**资源归属：**

session 复制 executor 结构体，不接管 `pCtx`。

**补充说明：**

设置后，`xllm_session_chat_ex` 遇到 client tool call 时会尝试自动执行。

**相关 API：**

- `xllm_session_chat_ex`

---

### xllm_session_set_tool_executor_async

给 session 设置异步工具执行器。

**函数原型：**

```c
XLLM_API int xllm_session_set_tool_executor_async(xllm_session *pSession, const xllm_tool_executor_async *pExecutor);
```

参数、返回值和资源归属与 `xllm_session_set_tool_executor` 类似。

---

## 工具结果释放

### xllm_tool_exec_result_free

释放工具执行结果内部资源。

**函数原型：**

```c
XLLM_API void xllm_tool_exec_result_free(xllm_tool_exec_result *pResult);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `pResult` | 输入/输出 | 可为 `NULL` | 工具执行结果。 |

**返回值：**

无。

**资源归属：**

释放 `pResult` 内部 parts 和扩展数据，不释放外层结构体。

---

## 自动 Tool Loop

自动 tool loop 的流程：

1. 宿主在 turn 中添加 `XLLM_TOOL_CLIENT` 工具定义。
2. 宿主在 `xllm` 或 `xllm_session` 上设置 executor。
3. 调用 `xllm_send_ex` 或 `xllm_session_chat_ex`。
4. 如果模型返回 `XLLM_STATUS_TOOL_CALL_REQUIRED`，xllm 查找对应工具定义。
5. xllm 调用同步或异步 executor。
6. xllm 把工具结果追加为 tool message，继续下一轮模型调用。
7. 循环直到模型返回最终响应、工具不可用、执行失败或达到最大轮数。

**边界：**

- xllm 不做权限审批。
- xllm 不解析业务参数，只传递 `sArgumentsJson`。
- xllm 不保证工具幂等，宿主应自己处理重试和副作用。

---

## 范例代码

```c
static char *dup_text(const char *s)
{
    size_t n;
    char *out;

    if (!s) {
        return NULL;
    }
    n = strlen(s);
    out = (char *)xrtCalloc(n + 1u, sizeof(char));
    if (!out) {
        return NULL;
    }
    memcpy(out, s, n);
    return out;
}

static int32 inspect_workspace(
    void *ctx,
    const xllm_tool_exec_request *req,
    xllm_tool_exec_result *result,
    xllm_error *err
) {
    (void)ctx;
    (void)err;

    if (!req || !result || !req->sToolId) {
        return XRT_NET_ERROR;
    }

    result->pParts = (xllm_content_part *)xrtCalloc(1u, sizeof(xllm_content_part));
    if (!result->pParts) {
        return XRT_NET_ERROR;
    }
    result->iPartCount = 1u;
    result->pParts[0].eKind = XLLM_PART_TEXT;
    result->pParts[0].as.tSource.eKind = XLLM_SOURCE_INLINE_TEXT;
    result->pParts[0].as.tSource.sMimeType = dup_text("text/plain");
    result->pParts[0].as.tSource.as.sText = dup_text("workspace_status=clean");
    if (!result->pParts[0].as.tSource.sMimeType ||
        !result->pParts[0].as.tSource.as.sText) {
        xllm_tool_exec_result_free(result);
        return XRT_NET_ERROR;
    }
    return XRT_NET_OK;
}

xllm_tool_executor executor;
memset(&executor, 0, sizeof(executor));
executor.pfnExecute = inspect_workspace;
xllm_session_set_tool_executor(session, &executor);
```

---

## 常见错误

| 问题 | 常见原因 | 处理方式 |
| --- | --- | --- |
| 模型返回 tool call 但 xllm 没有自动执行 | 没有设置 executor，或工具是 `XLLM_TOOL_PROVIDER`。 | 设置 client tool 和 executor。 |
| 自动 loop 停在 `tool_unavailable` | tool call 名称无法匹配 turn 中的工具定义。 | 确认 `sToolId` / `sWireName` 和模型返回一致。 |
| 工具执行后内存泄漏 | executor 填充 result 后没有被清理。 | 保证走 xllm 自动 loop，或手动调用 `xllm_tool_exec_result_free`。 |
| provider 拒绝 tool choice | provider 不支持 required/named/parallel。 | 改用 `AUTO` 或按 provider 能力降级。 |
| 工具执行有危险副作用 | xllm 不做权限控制。 | 在宿主/xwork 层做审批、沙箱和审计。 |

## 相关示例

- `examples\agent_loop\agent_loop.c`
- `examples\glm\glm_tool_loop.c`
- `examples\gemini\gemini_tool_loop.c`
- `examples\smoke_auto_tool_loop.c`
- `examples\smoke_auto_tool_loop_async_executor.c`
