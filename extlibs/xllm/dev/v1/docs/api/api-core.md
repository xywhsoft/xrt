# xllm Core Runtime API

> Core Runtime 负责创建 xllm 运行时、注册 provider adapter 和 profile，并发起一次无 session 的模型调用。

[返回 API 索引](README.md) | [基础类型](types.md) | [Request / Response API](api-request-response.md) | [Provider API](api-providers.md)

---

## 目录

- [模块定位](#模块定位)
- [标准调用顺序](#标准调用顺序)
- [运行时生命周期](#运行时生命周期)
  - [xllm_version](#xllm_version)
  - [xllm_runtime_options_init](#xllm_runtime_options_init)
  - [xllm_runtime_create](#xllm_runtime_create)
  - [xllm_runtime_destroy](#xllm_runtime_destroy)
- [运行时诊断配置](#运行时诊断配置)
  - [xllm_runtime_set_log_callback](#xllm_runtime_set_log_callback)
  - [xllm_runtime_set_trace_callback](#xllm_runtime_set_trace_callback)
  - [xllm_runtime_set_debug_mode](#xllm_runtime_set_debug_mode)
- [Adapter 与 Profile 注册](#adapter-与-profile-注册)
  - [xllm_register_adapter](#xllm_register_adapter)
  - [内置 Adapter 注册函数](#内置-adapter-注册函数)
  - [xllm_register_profile](#xllm_register_profile)
- [请求校验与调用](#请求校验与调用)
  - [xllm_validate_request](#xllm_validate_request)
  - [xllm_count_tokens](#xllm_count_tokens)
  - [xllm_chat](#xllm_chat)
  - [xllm_chat_ex](#xllm_chat_ex)
- [异步调用](#异步调用)
  - [xllm_chat_async_thread](#xllm_chat_async_thread)
  - [xllm_chat_async_engine](#xllm_chat_async_engine)
  - [xllm_chat_async_co](#xllm_chat_async_co)
- [取消控制](#取消控制)
  - [xllm_cancel_token_create](#xllm_cancel_token_create)
  - [xllm_cancel_token_destroy](#xllm_cancel_token_destroy)
  - [xllm_cancel_token_cancel](#xllm_cancel_token_cancel)
  - [xllm_cancel_token_is_cancelled](#xllm_cancel_token_is_cancelled)
- [完整示例](#完整示例)
- [常见错误](#常见错误)

---

## 模块定位

你在以下场景使用 Core Runtime：

- 你只想发起一次直接模型调用，不需要 session 历史。
- 你要在宿主程序启动时注册 provider adapter 和 profile。
- 你要统一设置日志、trace、debug 和传输默认值。
- 你要在调用前验证请求是否符合 profile/model 能力。

Core Runtime 不负责长期记忆，也不负责 session 历史。如果你需要短期多轮对话，请看 [api-session.md](api-session.md)。如果你需要 RAG 或本地记忆，请看 [api-memory.md](api-memory.md)。

---

## 标准调用顺序

最小顺序如下：

1. `xllm_runtime_options_init` 初始化运行时选项，或直接传 `NULL` 使用默认选项。
2. `xllm_runtime_create` 创建 runtime。
3. `xllm_register_*_adapter` 注册一个或多个 adapter。
4. `xllm_profile_init` 初始化 profile，并填写 `sId`、`sAdapter`、`sBaseUrl`、认证和模型。
5. `xllm_register_profile` 注册 profile。
6. 构造 `xllm_request` 和 `xllm_call_options`。
7. 调用 `xllm_chat_ex`。
8. 用 `xllm_response_free` 释放响应，用 `xllm_request_reset` 释放请求内部资源。
9. 程序退出前调用 `xllm_runtime_destroy`。

---

## 运行时生命周期

### xllm_version

获取 xllm 运行时版本字符串。

**功能：**

你可以在日志、诊断报告、release gate 或宿主 UI 中记录该版本，方便后续定位问题。

**函数原型：**

```c
XLLM_API const char *xllm_version(void);
```

**参数：**

无。

**返回值：**

- 返回静态字符串，例如当前实现返回 `"0.1.0"`。
- 调用者不能释放返回值。

**资源归属：**

返回值是借用指针，生命周期为进程生命周期。

**补充说明：**

- 如果你需要数值版本，可使用 `XLLM_VERSION_MAJOR`、`XLLM_VERSION_MINOR`、`XLLM_VERSION_PATCH`。
- 版本字符串用于人类阅读，不建议用字符串比较判断兼容性。

**范例代码：**

```c
#include "xllm.h"
#include <stdio.h>

int main(void) {
    printf("xllm version: %s\n", xllm_version());
    return 0;
}
```

**相关 API：**

- `XLLM_VERSION_MAJOR`
- `XLLM_VERSION_MINOR`
- `XLLM_VERSION_PATCH`

---

### xllm_runtime_options_init

初始化 `xllm_runtime_options`。

**功能：**

你在需要自定义日志、trace、debug、分配器或传输默认值时使用它。对于最小程序，也可以跳过 options，直接给 `xllm_runtime_create` 传 `NULL`。

**函数原型：**

```c
XLLM_API void xllm_runtime_options_init(xllm_runtime_options *pOptions);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `pOptions` | 输出 | 可为 `NULL` | 要初始化的 options。传 `NULL` 时函数直接返回。 |

**返回值：**

无。

**资源归属：**

该函数不分配资源。`pOptions` 由调用者持有，可以放在栈上。

**补充说明：**

- 初始化后 `eDebugMode = XLLM_DEBUG_NONE`。
- 初始化后 `eRedactMode = XLLM_REDACT_DEFAULT`。
- 如果你手动填 `tAllocator`，需要同时提供 malloc/realloc/free，避免混用分配器。

**范例代码：**

```c
xllm_runtime_options opt;
xllm_runtime_options_init(&opt);
opt.eDebugMode = XLLM_DEBUG_HEADERS;
opt.eRedactMode = XLLM_REDACT_STRICT;
```

**相关 API：**

- `xllm_runtime_create`
- `xllm_runtime_set_debug_mode`

---

### xllm_runtime_create

创建 xllm runtime。

**功能：**

runtime 是 xllm 的顶层对象。adapter、profile、默认诊断配置和内部网络执行环境都挂在 runtime 上。只要你要调用 `xllm_chat_ex` 或 session/memory 上层能力，就需要先创建 runtime。

**函数原型：**

```c
XLLM_API int xllm_runtime_create(const xllm_runtime_options *pOptions, xllm_runtime **ppRuntime);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `pOptions` | 输入 | 可为 `NULL` | 运行时选项。传 `NULL` 时使用默认 allocator、默认 debug/redact 和默认传输设置。 |
| `ppRuntime` | 输出 | 否 | 成功时写入新建 runtime；失败时写入 `NULL`。 |

**返回值：**

- `XRT_NET_OK`：创建成功。
- `XRT_NET_ERROR`：参数错误、内存分配失败或 options 克隆失败。

**资源归属：**

- 成功后 `*ppRuntime` 归调用者所有。
- 调用者必须用 `xllm_runtime_destroy(*ppRuntime)` 释放。
- runtime 销毁时会释放其内部复制的 adapter 和 profile。

**补充说明：**

- 传入的 `pOptions` 会被克隆到 runtime 内部。你可以在创建后释放或复用原始 options。
- runtime 内部会尝试创建并启动一个默认网络 engine；如果 engine 启动失败，runtime 仍可能创建成功，但部分异步路径会受限。
- 一个进程可以创建多个 runtime，但通常一个宿主进程共享一个 runtime 更简单。

**范例代码：**

```c
xllm_runtime *runtime = NULL;
int rc = xllm_runtime_create(NULL, &runtime);
if (rc != XRT_NET_OK || !runtime) {
    return 1;
}

xllm_runtime_destroy(runtime);
```

**相关 API：**

- `xllm_runtime_options_init`
- `xllm_runtime_destroy`

---

### xllm_runtime_destroy

销毁 runtime。

**功能：**

释放 runtime、已注册 adapter/profile 的内部副本、内部网络 engine 和 runtime options 中的内部资源。

**函数原型：**

```c
XLLM_API void xllm_runtime_destroy(xllm_runtime *pRuntime);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `pRuntime` | 输入 | 可为 `NULL` | 要销毁的 runtime。传 `NULL` 时直接返回。 |

**返回值：**

无。

**资源归属：**

- `pRuntime` 必须来自 `xllm_runtime_create`。
- 调用后不能再使用该指针。
- runtime 不会销毁你传给 callback 的 `pCtx`，也不会销毁仍由宿主持有的外部 `xvalue` 或工具资源。

**补充说明：**

- 请先结束或取消仍在使用该 runtime 的异步任务，再销毁 runtime。
- 如果 response 仍被调用者持有，应先用 `xllm_response_free` 释放 response。

**范例代码：**

```c
if (runtime) {
    xllm_runtime_destroy(runtime);
    runtime = NULL;
}
```

**相关 API：**

- `xllm_runtime_create`

---

## 运行时诊断配置

### xllm_runtime_set_log_callback

设置 runtime 日志回调。

**功能：**

当你希望把 xllm 日志接入宿主日志系统时使用它。日志适合给人读；如果你需要机器可解析的结构化事件，请同时使用 trace callback。

**函数原型：**

```c
XLLM_API int xllm_runtime_set_log_callback(
    xllm_runtime *pRuntime,
    xllm_log_callback pfnLog,
    void *pLogCtx
);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `pRuntime` | 输入 | 否 | 目标 runtime。 |
| `pfnLog` | 输入 | 可为 `NULL` | 日志回调。传 `NULL` 表示清除日志回调。 |
| `pLogCtx` | 输入 | 可为 `NULL` | 回调上下文，xllm 只保存指针，不接管生命周期。 |

**返回值：**

- `XRT_NET_OK`：设置成功。
- `XRT_NET_ERROR`：`pRuntime` 为 `NULL`。

**资源归属：**

`pLogCtx` 由调用者持有，必须保证在回调可能发生期间有效。

**补充说明：**

- 回调中收到的字符串只在回调期间可靠；需要异步保存时请复制。
- 日志回调应避免执行耗时操作，避免拖慢 provider 调用路径。

**范例代码：**

```c
static void on_log(void *ctx, xllm_log_level level, const char *component, const char *message)
{
    (void)ctx;
    printf("[%s] %s: %s\n", xllm_log_level_name(level), component, message);
}

xllm_runtime_set_log_callback(runtime, on_log, NULL);
```

**相关 API：**

- `xllm_log_level_name`
- `xllm_runtime_set_trace_callback`

---

### xllm_runtime_set_trace_callback

设置 runtime trace 回调。

**功能：**

trace 回调适合把 provider 请求、响应、流式事件、compact 和 tool loop 信息保存为结构化诊断数据。

**函数原型：**

```c
XLLM_API int xllm_runtime_set_trace_callback(
    xllm_runtime *pRuntime,
    xllm_trace_callback pfnTrace,
    void *pTraceCtx
);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `pRuntime` | 输入 | 否 | 目标 runtime。 |
| `pfnTrace` | 输入 | 可为 `NULL` | trace 回调。传 `NULL` 表示清除 trace 回调。 |
| `pTraceCtx` | 输入 | 可为 `NULL` | 回调上下文。 |

**返回值：**

- `XRT_NET_OK`：设置成功。
- `XRT_NET_ERROR`：`pRuntime` 为 `NULL`。

**资源归属：**

`pTraceCtx` 由调用者持有。`pPayload` 是回调期间有效的借用指针。

**补充说明：**

- trace payload 是 `xvalue`，宿主如需保存应转换或复制。
- 当 debug mode 较高时，trace 可能包含更多请求/响应信息；请配合 redact mode 使用。

**范例代码：**

```c
static void on_trace(void *ctx, xllm_trace_kind kind, const xvalue *payload)
{
    (void)ctx;
    (void)payload;
    printf("trace kind: %s\n", xllm_trace_kind_name(kind));
}

xllm_runtime_set_trace_callback(runtime, on_trace, NULL);
```

**相关 API：**

- `xllm_trace_kind_name`
- `xllm_runtime_set_debug_mode`

---

### xllm_runtime_set_debug_mode

设置运行时调试级别和脱敏策略。

**功能：**

你在排查 provider 请求、header、body 或 wire 问题时使用它。生产环境建议保持 `XLLM_DEBUG_NONE` 或严格脱敏。

**函数原型：**

```c
XLLM_API int xllm_runtime_set_debug_mode(
    xllm_runtime *pRuntime,
    xllm_debug_mode eMode,
    xllm_redact_mode eRedactMode
);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `pRuntime` | 输入 | 否 | 目标 runtime。 |
| `eMode` | 输入 | 否 | 调试级别。 |
| `eRedactMode` | 输入 | 否 | 脱敏策略。 |

**返回值：**

- `XRT_NET_OK`：设置成功。
- `XRT_NET_ERROR`：`pRuntime` 为 `NULL`。

**资源归属：**

不分配资源。

**补充说明：**

- `XLLM_REDACT_OFF` 可能暴露 API key、prompt 或用户数据，只应在本地短时间使用。
- 推荐在 CI 或可共享日志里使用 `XLLM_REDACT_STRICT`。

**范例代码：**

```c
xllm_runtime_set_debug_mode(runtime, XLLM_DEBUG_HEADERS, XLLM_REDACT_STRICT);
```

**相关 API：**

- `xllm_runtime_options_init`

---

## Adapter 与 Profile 注册

### xllm_register_adapter

注册自定义 adapter。

**功能：**

当内置 adapter 不满足你的 provider 协议时，宿主可以注册自己的 `xllm_adapter`。多数使用者应优先使用内置 adapter 注册函数。

**函数原型：**

```c
XLLM_API int xllm_register_adapter(xllm_runtime *pRuntime, const xllm_adapter *pAdapter);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `pRuntime` | 输入/输出 | 否 | 目标 runtime。 |
| `pAdapter` | 输入 | 否 | adapter 定义，必须有 `sName`。 |

**返回值：**

- `XRT_NET_OK`：注册成功。
- `XRT_NET_ERROR`：参数错误、名称重复、克隆失败或追加失败。

**资源归属：**

- runtime 会克隆 adapter 定义。
- `pAdapter` 原始对象仍由调用者持有。
- `pAdapter->pCtx` 指向的上下文是否可释放，取决于你的 adapter 回调设计；xllm 不会自动销毁它。

**补充说明：**

- `sName` 必须唯一。
- `pfnChat` 是实际聊天调用入口。
- `pfnCountTokens` 可选；如果缺失，token 计数功能可能不可用或只能估算。

**范例代码：**

```c
xllm_adapter adapter;
memset(&adapter, 0, sizeof(adapter));
adapter.sName = "my_adapter";
adapter.pCtx = my_ctx;
adapter.pfnChat = my_chat_fn;
adapter.pfnCountTokens = my_count_tokens_fn;

if (xllm_register_adapter(runtime, &adapter) != XRT_NET_OK) {
    /* 处理注册失败 */
}
```

**相关 API：**

- `xllm_register_profile`
- `xllm_register_openai_compat_adapter`

---

### 内置 Adapter 注册函数

注册 xllm 自带的 provider adapter。

**功能：**

内置 adapter 把不同 provider 的协议差异统一成 xllm request/response 语义。你使用某个 profile 前，必须先注册该 profile 需要的 adapter。

**函数原型：**

```c
XLLM_API int xllm_register_openai_compat_adapter(xllm_runtime *pRuntime);
XLLM_API int xllm_register_glm_native_adapter(xllm_runtime *pRuntime);
XLLM_API int xllm_register_minimax_native_adapter(xllm_runtime *pRuntime);
XLLM_API int xllm_register_kimi_native_adapter(xllm_runtime *pRuntime);
XLLM_API int xllm_register_gemini_native_adapter(xllm_runtime *pRuntime);
XLLM_API int xllm_register_vertex_gemini_native_adapter(xllm_runtime *pRuntime);
XLLM_API int xllm_register_qwen_native_adapter(xllm_runtime *pRuntime);
XLLM_API int xllm_register_doubao_native_adapter(xllm_runtime *pRuntime);
XLLM_API int xllm_register_anthropic_native_adapter(xllm_runtime *pRuntime);
XLLM_API int xllm_register_ollama_native_adapter(xllm_runtime *pRuntime);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `pRuntime` | 输入/输出 | 否 | 目标 runtime。 |

**返回值：**

- `XRT_NET_OK`：注册成功。
- `XRT_NET_ERROR`：runtime 无效、adapter 重复或内部注册失败。

**资源归属：**

adapter 注册到 runtime 内部，随 `xllm_runtime_destroy` 释放。

**补充说明：**

- 重复注册同名 adapter 会失败。
- 你可以只注册实际会用到的 adapter。
- profile 的 `sAdapter` 应使用对应 `XLLM_ADAPTER_*` 常量。

**范例代码：**

```c
if (xllm_register_openai_compat_adapter(runtime) != XRT_NET_OK) {
    return 2;
}
```

**相关 API：**

- `xllm_register_adapter`
- `xllm_register_profile`

---

### xllm_register_profile

注册模型调用 profile。

**功能：**

profile 把 adapter、base URL、认证、模型、能力和默认参数绑定成一个可引用配置。请求通过 `xllm_request.sProfileId` 选择 profile。

**函数原型：**

```c
XLLM_API int xllm_register_profile(xllm_runtime *pRuntime, const xllm_profile *pProfile);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `pRuntime` | 输入/输出 | 否 | 目标 runtime。 |
| `pProfile` | 输入 | 否 | profile 定义。`sId` 和 `sAdapter` 必须存在。 |

**返回值：**

- `XRT_NET_OK`：注册成功。
- `XRT_NET_ERROR`：参数错误、profile ID 重复、克隆失败或追加失败。

**资源归属：**

- runtime 会克隆 profile。
- 原始 `pProfile` 仍由调用者持有，可以在注册后释放或复用。
- profile 内字符串、headers、model caps 等会按内部规则复制到 runtime。

**补充说明：**

- 注册 profile 前应先注册对应 adapter。
- `sId` 必须唯一。
- 至少应配置 `tModels.tText.sModelId`，如果要处理图片/文件等多模态输入，还应配置 `tModels.tMultimodal` 及能力位。

**范例代码：**

```c
xllm_profile profile;
xllm_profile_init(&profile);

profile.sId = "local-openai";
profile.sProvider = "openai";
profile.sAdapter = XLLM_ADAPTER_OPENAI_COMPAT;
profile.sBaseUrl = "https://api.openai.com/v1";
profile.tAuth.eKind = XLLM_AUTH_BEARER;
profile.tAuth.sSecret = getenv("OPENAI_API_KEY");
profile.tModels.tText.sModelId = "gpt-4.1-mini";
profile.tModels.tText.tCaps.uFlags = XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT | XLLM_CAP_STREAM;

if (xllm_register_profile(runtime, &profile) != XRT_NET_OK) {
    return 3;
}
```

**相关 API：**

- `xllm_profile_init`
- `xllm_register_openai_compat_adapter`

---

## 请求校验与调用

### xllm_validate_request

验证请求是否符合 runtime、profile、adapter 和模型能力约束。

**功能：**

你可以在真正调用 provider 前先检查请求，提前发现 profile 不存在、输入类型不支持、MIME 类型不支持、模型能力不足等问题。

**函数原型：**

```c
XLLM_API int xllm_validate_request(
    xllm_runtime *pRuntime,
    const xllm_request *pRequest,
    const xllm_call_options *pOptions,
    xllm_error *pError
);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `pRuntime` | 输入 | 否 | 已创建的 runtime。 |
| `pRequest` | 输入 | 否 | 要验证的请求。 |
| `pOptions` | 输入 | 可为 `NULL` | 调用选项。用于验证 stream、artifact、本地文件等策略。 |
| `pError` | 输出 | 可为 `NULL` | 失败时写入详细错误。 |

**返回值：**

- `XRT_NET_OK`：请求可继续提交。
- 非 `XRT_NET_OK`：请求不合法或能力不足。

**资源归属：**

不接管 `pRequest` 或 `pOptions`。如果传入 `pError`，调用者负责用 `xllm_error_free` 清理。

**补充说明：**

- `xllm_chat_ex` 内部会自动调用该函数。
- 对用户可修正的问题，优先把 `pError->eCode` 和 `pError->sMessage` 展示给宿主日志或 UI。

**范例代码：**

```c
xllm_error err;
xllm_error_init(&err);
if (xllm_validate_request(runtime, &request, NULL, &err) != XRT_NET_OK) {
    fprintf(stderr, "invalid request: %s\n", err.sMessage ? err.sMessage : "(unknown)");
}
xllm_error_free(&err);
```

**相关 API：**

- `xllm_chat_ex`
- `xllm_error`

---

### xllm_count_tokens

统计请求 token。

**功能：**

你在发送请求前估算上下文大小、决定是否 compact、或给 UI 展示 token 用量时使用它。

**函数原型：**

```c
XLLM_API int xllm_count_tokens(
    xllm_runtime *pRuntime,
    const xllm_request *pRequest,
    xllm_token_count_result *pResult,
    xllm_error *pError
);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `pRuntime` | 输入 | 否 | 已创建的 runtime。 |
| `pRequest` | 输入 | 否 | 要计数的请求。 |
| `pResult` | 输出 | 否 | 写入 token 计数结果。 |
| `pError` | 输出 | 可为 `NULL` | 失败时写入错误。 |

**返回值：**

- `XRT_NET_OK`：计数成功。
- 非 `XRT_NET_OK`：profile/adapter 不可用、adapter 不支持计数或请求无效。

**资源归属：**

`pResult` 由调用者提供，函数只写入字段，不要求额外释放。

**补充说明：**

- 计数能力取决于 adapter 的 `pfnCountTokens`。
- `pResult->bEstimated` 表示结果是否是估算值。
- 计数结果应作为预算参考，不应替代 provider 最终 token 用量。

**范例代码：**

```c
xllm_token_count_result count;
memset(&count, 0, sizeof(count));

if (xllm_count_tokens(runtime, &request, &count, &err) == XRT_NET_OK) {
    printf("input tokens: %u\n", count.uInputTokens);
}
```

**相关 API：**

- `xllm_validate_request`
- `xllm_session_compact`

---

### xllm_chat

发起一次聊天调用，不接收详细错误对象。

**功能：**

这是简化版调用入口。你只关心成功/失败，不需要读取 `xllm_error` 细节时可以使用它。实际集成中更推荐 `xllm_chat_ex`。

**函数原型：**

```c
XLLM_API int xllm_chat(
    xllm_runtime *pRuntime,
    const xllm_request *pRequest,
    const xllm_call_options *pOptions,
    xllm_response **ppResponse
);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `pRuntime` | 输入 | 否 | 已创建 runtime。 |
| `pRequest` | 输入 | 否 | 请求。 |
| `pOptions` | 输入 | 可为 `NULL` | 调用选项。 |
| `ppResponse` | 输出 | 否 | 成功时写入 response。 |

**返回值：**

- `XRT_NET_OK`：调用成功。
- 非 `XRT_NET_OK`：调用失败，但错误细节不会返回给调用者。

**资源归属：**

成功后 `*ppResponse` 归调用者所有，必须用 `xllm_response_free` 释放。

**补充说明：**

- 该函数内部仍会创建临时 `xllm_error`，但调用结束后释放。
- 新代码优先用 `xllm_chat_ex`，便于定位错误。

**范例代码：**

```c
xllm_response *response = NULL;
if (xllm_chat(runtime, &request, NULL, &response) == XRT_NET_OK) {
    puts(xllm_response_get_text(response));
}
xllm_response_free(response);
```

**相关 API：**

- `xllm_chat_ex`
- `xllm_response_free`

---

### xllm_chat_ex

发起一次聊天调用，并返回详细错误。

**功能：**

这是 core 层最重要的调用入口。它会验证请求、检查取消 token、找到 profile 和 adapter，然后调用 adapter 的聊天实现。

**函数原型：**

```c
XLLM_API int xllm_chat_ex(
    xllm_runtime *pRuntime,
    const xllm_request *pRequest,
    const xllm_call_options *pOptions,
    xllm_response **ppResponse,
    xllm_error *pError
);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `pRuntime` | 输入 | 否 | 已创建 runtime。 |
| `pRequest` | 输入 | 否 | 请求。 |
| `pOptions` | 输入 | 可为 `NULL` | 调用选项。 |
| `ppResponse` | 输出 | 否 | 成功时写入 response；失败时写入 `NULL`。 |
| `pError` | 输出 | 可为 `NULL` | 失败时写入错误。 |

**返回值：**

- `XRT_NET_OK`：调用成功。
- `XRT_NET_CANCELLED`：取消 token 已取消。
- `XRT_NET_ERROR` 或其他非成功值：验证失败、adapter 不可用、provider 调用失败或内部错误。

**资源归属：**

- 成功后 `*ppResponse` 归调用者所有，用 `xllm_response_free` 释放。
- `pError` 由调用者提供，使用前建议 `xllm_error_init`，结束后 `xllm_error_free`。
- `pRequest` 和 `pOptions` 不被接管。

**补充说明：**

- 函数开始时会 reset `pError`，因此可以复用同一个错误对象。
- 如果 adapter 失败但没有设置错误码，xllm 会补一个 `XLLM_ERROR_INTERNAL`。
- 如果 `pOptions->pCancelToken` 在派发前已取消，会返回取消错误。

**范例代码：**

```c
xllm_response *response = NULL;
xllm_error err;
xllm_error_init(&err);

int rc = xllm_chat_ex(runtime, &request, NULL, &response, &err);
if (rc != XRT_NET_OK) {
    fprintf(stderr, "chat failed: %s\n", err.sMessage ? err.sMessage : "(unknown)");
} else {
    printf("%s\n", xllm_response_get_text(response));
}

xllm_response_free(response);
xllm_error_free(&err);
```

**相关 API：**

- `xllm_validate_request`
- `xllm_response_get_text`
- `xllm_response_free`

---

## 异步调用

### xllm_chat_async_thread

在线程中异步执行聊天调用。

**功能：**

你希望不阻塞当前线程，但又没有自己的 `xnetengine` 或协程调度器时，可以使用它。

**函数原型：**

```c
XLLM_API xfuture *xllm_chat_async_thread(
    xllm_runtime *pRuntime,
    const xllm_request *pRequest,
    const xllm_call_options *pOptions
);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `pRuntime` | 输入 | 否 | runtime。 |
| `pRequest` | 输入 | 否 | 请求。函数会为异步任务创建内部副本。 |
| `pOptions` | 输入 | 可为 `NULL` | 调用选项。函数会为异步任务创建内部副本。 |

**返回值：**

- 返回 `xfuture *`。
- 创建任务失败时也会返回一个带错误结果的 future，而不是直接返回 `NULL`。

**资源归属：**

future 的等待、取结果和释放遵循 xrt `xfuture` 规则。请求和 options 原始对象仍由调用者持有。

**补充说明：**

- runtime 必须在 future 完成前保持有效。
- 异步结果通常包含 `xllm_response *` 或错误状态，具体读取方式遵循 xrt future 约定。

**范例代码：**

```c
xfuture *future = xllm_chat_async_thread(runtime, &request, NULL);
/* 按 xrt future 规则等待和读取结果 */
```

**相关 API：**

- `xllm_chat_ex`
- `xllm_chat_async_engine`

---

### xllm_chat_async_engine

在指定 `xnetengine` 上异步执行聊天调用。

**功能：**

当宿主已经有统一网络/任务 engine，并希望把 xllm 调用纳入同一个调度体系时使用。

**函数原型：**

```c
XLLM_API xfuture *xllm_chat_async_engine(
    xllm_runtime *pRuntime,
    xnetengine *pEngine,
    uint32 uAffinityKey,
    const xllm_request *pRequest,
    const xllm_call_options *pOptions
);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `pRuntime` | 输入 | 否 | runtime。 |
| `pEngine` | 输入 | 否 | xrt network engine。 |
| `uAffinityKey` | 输入 | 否 | engine 调度亲和 key。 |
| `pRequest` | 输入 | 否 | 请求。 |
| `pOptions` | 输入 | 可为 `NULL` | 调用选项。 |

**返回值：**

- 返回 `xfuture *`。
- 如果 `pEngine` 为 `NULL`，返回带错误结果的 future。

**资源归属：**

future 按 xrt 规则释放。`pEngine` 仍由宿主持有，必须在任务完成前有效。

**补充说明：**

- 适合已经有统一后台任务队列的宿主。
- `uAffinityKey` 的语义由 xrt engine 决定。

**范例代码：**

```c
xfuture *future = xllm_chat_async_engine(runtime, engine, 0, &request, &options);
```

**相关 API：**

- `xllm_chat_async_thread`
- `xllm_chat_async_co`

---

### xllm_chat_async_co

在 xrt 协程调度器中异步执行聊天调用。

**功能：**

当宿主使用 xrt coroutine，并希望把 LLM 调用作为协程任务运行时使用。

**函数原型：**

```c
XLLM_API xfuture *xllm_chat_async_co(
    xllm_runtime *pRuntime,
    xcosched *pSched,
    const xllm_request *pRequest,
    const xllm_call_options *pOptions,
    size_t iStackSize
);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `pRuntime` | 输入 | 否 | runtime。 |
| `pSched` | 输入 | 否 | 协程调度器。 |
| `pRequest` | 输入 | 否 | 请求。 |
| `pOptions` | 输入 | 可为 `NULL` | 调用选项。 |
| `iStackSize` | 输入 | 否 | 协程栈大小。传 `0` 时使用 xrt 默认策略。 |

**返回值：**

- 返回 `xfuture *`。
- 如果 `pSched` 为 `NULL` 或构建时禁用了 coroutine，返回带错误结果的 future。

**资源归属：**

future 按 xrt 规则释放。`pSched` 由宿主持有。

**补充说明：**

- 仅在 xrt coroutine 支持启用时可用。
- runtime 和 scheduler 必须在任务完成前保持有效。

**范例代码：**

```c
xfuture *future = xllm_chat_async_co(runtime, sched, &request, NULL, 0);
```

**相关 API：**

- `xllm_chat_async_thread`

---

## 取消控制

### xllm_cancel_token_create

创建取消 token。

**功能：**

取消 token 用于让宿主在调用过程中请求取消。它可以放入 `xllm_call_options.pCancelToken`。

**函数原型：**

```c
XLLM_API int xllm_cancel_token_create(xllm_cancel_token **ppToken);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `ppToken` | 输出 | 否 | 成功时写入新建 token。 |

**返回值：**

- `XRT_NET_OK`：创建成功。
- `XRT_NET_ERROR`：参数错误、内存分配失败或 mutex 创建失败。

**资源归属：**

成功后 token 归调用者所有，必须用 `xllm_cancel_token_destroy` 释放。

**补充说明：**

- token 内部使用 mutex 保护取消状态。
- token 初始状态为未取消。

**范例代码：**

```c
xllm_cancel_token *token = NULL;
if (xllm_cancel_token_create(&token) == XRT_NET_OK) {
    /* 放入 xllm_call_options */
}
xllm_cancel_token_destroy(token);
```

**相关 API：**

- `xllm_cancel_token_cancel`
- `xllm_cancel_token_is_cancelled`

---

### xllm_cancel_token_destroy

销毁取消 token。

**功能：**

释放 token、内部 mutex 和取消原因字符串。

**函数原型：**

```c
XLLM_API void xllm_cancel_token_destroy(xllm_cancel_token *pToken);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `pToken` | 输入 | 可为 `NULL` | 要销毁的 token。 |

**返回值：**

无。

**资源归属：**

`pToken` 必须来自 `xllm_cancel_token_create`。销毁后不能继续使用。

**补充说明：**

- 不要在仍有后台调用可能读取该 token 时销毁它。
- 如果多个请求共享 token，应等所有请求结束后再销毁。

**范例代码：**

```c
xllm_cancel_token_destroy(token);
token = NULL;
```

**相关 API：**

- `xllm_cancel_token_create`

---

### xllm_cancel_token_cancel

把 token 标记为已取消。

**功能：**

宿主在用户点击取消、任务超时或上层流程中断时调用它。后续检查该 token 的调用路径会看到取消状态。

**函数原型：**

```c
XLLM_API void xllm_cancel_token_cancel(xllm_cancel_token *pToken, const char *sReason);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `pToken` | 输入/输出 | 可为 `NULL` | 要取消的 token。传 `NULL` 时直接返回。 |
| `sReason` | 输入 | 可为 `NULL` | 取消原因。函数会复制该字符串。 |

**返回值：**

无。

**资源归属：**

`sReason` 由调用者持有，函数内部会复制一份。旧取消原因会被释放。

**补充说明：**

- 多次调用会保持取消状态，并替换取消原因。
- 当前公开 API 只提供取消状态查询，不提供取消原因读取函数。

**范例代码：**

```c
xllm_cancel_token_cancel(token, "user cancelled");
```

**相关 API：**

- `xllm_cancel_token_is_cancelled`

---

### xllm_cancel_token_is_cancelled

查询 token 是否已取消。

**功能：**

宿主或 adapter 可以用它检查是否应该停止当前工作。

**函数原型：**

```c
XLLM_API bool xllm_cancel_token_is_cancelled(const xllm_cancel_token *pToken);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `pToken` | 输入 | 可为 `NULL` | 要检查的 token。传 `NULL` 返回 `false`。 |

**返回值：**

- `true`：已取消。
- `false`：未取消，或 `pToken` 为 `NULL`。

**资源归属：**

不分配资源，不接管 token。

**补充说明：**

- 查询过程使用 mutex 保护。
- `xllm_chat_ex` 在派发 adapter 前会检查 `pOptions->pCancelToken`。

**范例代码：**

```c
if (xllm_cancel_token_is_cancelled(token)) {
    return XRT_NET_CANCELLED;
}
```

**相关 API：**

- `xllm_cancel_token_cancel`

---

## 完整示例

下面示例展示 core runtime 的最小结构。真实 provider 调用需要填写可用 endpoint、API key 和模型能力。

```c
#include "xllm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

int main(void)
{
    xllm_runtime *runtime = NULL;
    xllm_profile profile;
    xllm_request request;
    xllm_response *response = NULL;
    xllm_error error;
    int rc;

    xllm_profile_init(&profile);
    xllm_request_init(&request);
    xllm_error_init(&error);

    rc = xllm_runtime_create(NULL, &runtime);
    if (rc != XRT_NET_OK || !runtime) {
        fprintf(stderr, "create runtime failed\n");
        return 1;
    }

    rc = xllm_register_openai_compat_adapter(runtime);
    if (rc != XRT_NET_OK) {
        fprintf(stderr, "register adapter failed\n");
        xllm_runtime_destroy(runtime);
        return 2;
    }

    profile.sId = "demo";
    profile.sProvider = "openai";
    profile.sAdapter = XLLM_ADAPTER_OPENAI_COMPAT;
    profile.sBaseUrl = "https://api.openai.com/v1";
    profile.tAuth.eKind = XLLM_AUTH_BEARER;
    profile.tAuth.sSecret = getenv("OPENAI_API_KEY");
    profile.tModels.tText.sModelId = "gpt-4.1-mini";
    profile.tModels.tText.tCaps.uFlags = XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT;

    rc = xllm_register_profile(runtime, &profile);
    if (rc != XRT_NET_OK) {
        fprintf(stderr, "register profile failed\n");
        xllm_runtime_destroy(runtime);
        return 3;
    }

    request.sProfileId = dup_text("demo");
    request.iMessageCount = 1u;
    request.pMessages = (xllm_message *)xrtCalloc(1u, sizeof(xllm_message));
    request.pMessages[0].eRole = XLLM_ROLE_USER;
    request.pMessages[0].iPartCount = 1u;
    request.pMessages[0].pParts = (xllm_content_part *)xrtCalloc(1u, sizeof(xllm_content_part));
    request.pMessages[0].pParts[0].eKind = XLLM_PART_TEXT;
    request.pMessages[0].pParts[0].as.tSource.eKind = XLLM_SOURCE_INLINE_TEXT;
    request.pMessages[0].pParts[0].as.tSource.sMimeType = dup_text("text/plain");
    request.pMessages[0].pParts[0].as.tSource.as.sText = dup_text("用一句话介绍 xllm。");

    if (!request.sProfileId || !request.pMessages || !request.pMessages[0].pParts ||
        !request.pMessages[0].pParts[0].as.tSource.sMimeType ||
        !request.pMessages[0].pParts[0].as.tSource.as.sText) {
        fprintf(stderr, "allocate request failed\n");
        xllm_request_reset(&request);
        xllm_runtime_destroy(runtime);
        return 4;
    }

    rc = xllm_chat_ex(runtime, &request, NULL, &response, &error);
    if (rc != XRT_NET_OK) {
        fprintf(stderr, "chat failed: %s\n", error.sMessage ? error.sMessage : "(unknown)");
    } else {
        printf("%s\n", xllm_response_get_text(response));
    }

    xllm_response_free(response);
    xllm_error_free(&error);
    xllm_request_reset(&request);
    xllm_runtime_destroy(runtime);
    return rc == XRT_NET_OK ? 0 : 5;
}
```

---

## 常见错误

| 问题 | 常见原因 | 处理方式 |
| --- | --- | --- |
| `xllm_chat_ex` 返回 `XLLM_ERROR_MODEL_NOT_FOUND` | `sProfileId` 未注册或写错。 | 先调用 `xllm_register_profile`，并确认 request 使用同一个 ID。 |
| 返回 `profile adapter is not available` | profile 的 `sAdapter` 没有注册。 | 先调用对应 `xllm_register_*_adapter`。 |
| 多模态输入被拒绝 | profile 未配置多模态模型或能力位。 | 配置 `tModels.tMultimodal` 和 `XLLM_CAP_IMAGE_IN` / `XLLM_CAP_FILE_IN`。 |
| 没有错误细节 | 使用了 `xllm_chat`。 | 改用 `xllm_chat_ex` 并传入 `xllm_error`。 |
| 异步调用崩溃或行为不稳定 | runtime、engine、scheduler 或 cancel token 提前释放。 | 等 future 完成后再销毁相关对象。 |

## 相关示例

- `examples\smoke_chat_ex_low_level.c`
- `examples\smoke_mock_adapter.c`
- `examples\smoke_openai_compat_adapter.c`
- `build.bat smoke -Filter "chat_ex_low_level,mock_adapter,openai_compat_adapter"`
