# xllm Request / Response API

> 本页讲解如何构造一次 core 请求、读取模型响应、处理 JSON/tool call 输出，并正确释放相关资源。

[返回 API 索引](README.md) | [基础类型](types.md) | [Core Runtime API](api-core.md) | [Tools API](api-tools.md)

---

## 目录

- [模块定位](#模块定位)
- [核心对象](#核心对象)
- [请求生命周期](#请求生命周期)
  - [xllm_request_init](#xllm_request_init)
  - [xllm_request_reset](#xllm_request_reset)
  - [xllm_call_options_init](#xllm_call_options_init)
- [错误对象生命周期](#错误对象生命周期)
  - [xllm_error_init](#xllm_error_init)
  - [xllm_error_reset](#xllm_error_reset)
  - [xllm_error_free](#xllm_error_free)
- [响应读取](#响应读取)
  - [xllm_response_get_text](#xllm_response_get_text)
  - [xllm_response_get_output_count](#xllm_response_get_output_count)
  - [xllm_response_get_output](#xllm_response_get_output)
  - [xllm_response_get_tool_call_count](#xllm_response_get_tool_call_count)
  - [xllm_response_get_tool_call](#xllm_response_get_tool_call)
  - [xllm_response_get_json](#xllm_response_get_json)
  - [xllm_response_get_first_json](#xllm_response_get_first_json)
  - [xllm_response_free](#xllm_response_free)
- [工具执行结果释放](#工具执行结果释放)
  - [xllm_tool_exec_result_free](#xllm_tool_exec_result_free)
- [常见用法](#常见用法)
- [常见错误](#常见错误)
- [相关示例](#相关示例)

---

## 模块定位

Request / Response API 是 xllm 的“数据面”：

- `xllm_request` 描述你要发给模型的内容、profile、工具、生成参数和输出格式。
- `xllm_call_options` 描述本次调用如何执行，例如流式、超时、取消、artifact 和重试。
- `xllm_response` 描述模型返回的文本、JSON、tool call、thinking、refusal、usage 和原始扩展数据。
- `xllm_error` 描述失败原因，适合日志、UI 提示和自动降级。

如果你使用 `xllm_session_chat`，通常会构造 `xllm_turn` 而不是 `xllm_request`；但响应读取和错误处理仍然使用本页的规则。

---

## 核心对象

### `xllm_request`

| 字段 | 说明 |
| --- | --- |
| `sProfileId` | 本次请求使用的 profile ID。必须匹配已注册 profile。 |
| `eSlot` | 模型槽位，`XLLM_SLOT_AUTO`、`XLLM_SLOT_TEXT` 或 `XLLM_SLOT_MULTIMODAL`。 |
| `pMessages` / `iMessageCount` | 消息数组。 |
| `pContextBlocks` / `iContextBlockCount` | 额外上下文块，常由 memory/context packing 注入。 |
| `pTools` / `iToolCount` | 可用工具定义。 |
| `tToolPolicy` | 工具选择策略。 |
| `tGeneration` | temperature、top_p、max tokens、stop 等生成参数。 |
| `tResponseFormat` | 文本、JSON 或 JSON schema 输出要求。 |
| `tReasoning` | reasoning 相关参数。 |
| `tVendorExtra` | provider 或宿主扩展字段。 |

**资源归属：**

低层构造 `xllm_request` 时，如果你把字符串、数组、`xvalue` 放进 request，并希望 `xllm_request_reset()` 释放它们，应使用和 xllm/xrt 兼容的分配方式，并遵守内部释放规则。最安全的学习路径是参考仓库示例中的构造方式，或者使用 session 层 helper（如 `xllm_turn_add_user_text`）减少手动管理。

### `xllm_call_options`

| 字段 | 说明 |
| --- | --- |
| `eStreamMode` | 是否使用流式，默认 `XLLM_STREAM_AUTO`。 |
| `uTimeoutMs` | 调用总超时，`0` 表示使用默认策略。 |
| `pCancelToken` | 可选取消 token。 |
| `pfnOnEvent` / `pUserData` | 流式事件回调。 |
| `eArtifactPolicy` / `pArtifactSink` | artifact 处理策略。 |
| `uMaxRetries` 等重试字段 | provider/网络重试策略。 |
| `bBestEffortStructuredOutput` | 是否允许尽力而为的结构化输出。 |
| `eLocalFilePolicy` | 本地文件处理策略。 |
| `tVendorExtra` | 扩展字段。 |

### `xllm_response`

`xllm_response` 由调用 API 分配，调用者不应直接栈上创建。读取后用 `xllm_response_free()` 释放。

| 字段 | 说明 |
| --- | --- |
| `sVisibleText` | 适合直接显示的文本。 |
| `pOutputs` / `iOutputCount` | 结构化输出数组。 |
| `tUsage` | token 用量。 |
| `tRefusal` / `tSafety` | 拒绝和安全信息。 |
| `tEffectiveParams` | 实际生效参数。 |
| `bHasError` / `tError` | 响应内部错误。 |
| `tRaw` / `tVendorExtra` | 原始或扩展信息。 |

---

## 请求生命周期

### xllm_request_init

初始化 `xllm_request`。

**功能：**

你在栈上或堆上创建 `xllm_request` 后，先调用本函数获得安全默认值。

**函数原型：**

```c
XLLM_API void xllm_request_init(xllm_request *pRequest);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `pRequest` | 输出 | 可为 `NULL` | 要初始化的请求对象。传 `NULL` 时直接返回。 |

**返回值：**

无。

**资源归属：**

不分配资源。`pRequest` 由调用者持有。

**补充说明：**

初始化后：

- `eSlot = XLLM_SLOT_AUTO`
- `tToolPolicy.eMode = XLLM_TOOL_CHOICE_AUTO`
- `tResponseFormat.eKind = XLLM_RESPONSE_TEXT`
- `tReasoning.eLevel = XLLM_REASONING_DEFAULT`

**范例代码：**

```c
xllm_request req;
xllm_request_init(&req);
req.sProfileId = "demo";
```

**相关 API：**

- `xllm_request_reset`
- `xllm_chat_ex`

---

### xllm_request_reset

释放 request 内部资源并恢复默认值。

**功能：**

当一次请求结束后，调用它释放 request 内部字符串、数组、content part、context block、tool、`xvalue` 等资源，并把对象重新初始化。

**函数原型：**

```c
XLLM_API void xllm_request_reset(xllm_request *pRequest);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `pRequest` | 输入/输出 | 可为 `NULL` | 要清理的请求。 |

**返回值：**

无。

**资源归属：**

该函数会释放 xllm 认为由 request 持有的内部资源。调用后 request 可继续复用。

**补充说明：**

- 不要把字符串字面量或非 xllm/xrt 分配的内存放入会被 reset 释放的字段，除非对应构造 API 明确说明会复制。
- 如果你只临时借用外部内存，应在 reset 前确认该字段不会被释放，或改用 helper API。

**范例代码：**

```c
xllm_request req;
xllm_request_init(&req);

/* 构造并调用 */

xllm_request_reset(&req);
```

**相关 API：**

- `xllm_request_init`

---

### xllm_call_options_init

初始化 `xllm_call_options`。

**功能：**

你需要设置流式、超时、取消、重试或 artifact 处理时使用该结构。只要使用非默认调用行为，就先 init 再改字段。

**函数原型：**

```c
XLLM_API void xllm_call_options_init(xllm_call_options *pOptions);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `pOptions` | 输出 | 可为 `NULL` | 要初始化的调用选项。 |

**返回值：**

无。

**资源归属：**

不分配资源。`pOptions` 由调用者持有。

**补充说明：**

初始化后：

- `eStreamMode = XLLM_STREAM_AUTO`
- `eArtifactPolicy = XLLM_ARTIFACT_INLINE_SMALL`
- `eLocalFilePolicy = XLLM_LOCAL_FILE_AUTO`

**范例代码：**

```c
xllm_call_options opt;
xllm_call_options_init(&opt);
opt.eStreamMode = XLLM_STREAM_PREFER;
opt.uTimeoutMs = 90000;
```

**相关 API：**

- `xllm_chat_ex`
- `xllm_cancel_token_create`

---

## 错误对象生命周期

### xllm_error_init

初始化错误对象。

**函数原型：**

```c
XLLM_API void xllm_error_init(xllm_error *pError);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `pError` | 输出 | 可为 `NULL` | 要初始化的错误对象。 |

**返回值：**

无。初始化后 `eCode = XLLM_ERROR_NONE`。

**资源归属：**

不分配资源。

**范例代码：**

```c
xllm_error err;
xllm_error_init(&err);
```

**相关 API：**

- `xllm_error_reset`
- `xllm_error_free`

---

### xllm_error_reset

释放错误对象内部字符串和扩展数据，并恢复为无错误。

**函数原型：**

```c
XLLM_API void xllm_error_reset(xllm_error *pError);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `pError` | 输入/输出 | 可为 `NULL` | 要重置的错误对象。 |

**返回值：**

无。

**资源归属：**

释放 `sMessage`、provider 错误字段、request id、selected model、MIME type 和 `tVendorExtra`。

**范例代码：**

```c
xllm_error_reset(&err);
```

**相关 API：**

- `xllm_chat_ex`

---

### xllm_error_free

释放错误对象内部资源。

**函数原型：**

```c
XLLM_API void xllm_error_free(xllm_error *pError);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `pError` | 输入/输出 | 可为 `NULL` | 要释放内部资源的错误对象。 |

**返回值：**

无。

**资源归属：**

该函数不释放 `pError` 外层结构体本身，只释放内部资源。栈上对象调用后可直接离开作用域。

**范例代码：**

```c
xllm_error err;
xllm_error_init(&err);
/* 调用 API */
xllm_error_free(&err);
```

**相关 API：**

- `xllm_error_init`

---

## 响应读取

### xllm_response_get_text

获取最适合直接显示的文本。

**功能：**

你只想把模型输出显示给用户时，优先使用这个函数。它会按顺序返回 `sVisibleText`、refusal 文本、输出项中的 refusal 文本，最后查找第一段文本 message part。

**函数原型：**

```c
XLLM_API const char *xllm_response_get_text(const xllm_response *pResponse);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `pResponse` | 输入 | 可为 `NULL` | 响应对象。 |

**返回值：**

- 返回文本借用指针。
- 没有可显示文本或 `pResponse=NULL` 时返回 `NULL`。

**资源归属：**

返回指针属于 `pResponse`，不要释放。需要长期保存时复制字符串。

**范例代码：**

```c
const char *text = xllm_response_get_text(response);
printf("%s\n", text ? text : "");
```

**相关 API：**

- `xllm_response_get_output`
- `xllm_response_free`

---

### xllm_response_get_output_count

获取响应输出项数量。

**函数原型：**

```c
XLLM_API size_t xllm_response_get_output_count(const xllm_response *pResponse);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `pResponse` | 输入 | 可为 `NULL` | 响应对象。 |

**返回值：**

- 输出项数量。
- `pResponse=NULL` 时返回 `0`。

**资源归属：**

不分配资源。

**范例代码：**

```c
for (size_t i = 0; i < xllm_response_get_output_count(response); ++i) {
    const xllm_output_item *out = xllm_response_get_output(response, i);
}
```

**相关 API：**

- `xllm_response_get_output`

---

### xllm_response_get_output

按索引读取输出项。

**函数原型：**

```c
XLLM_API const xllm_output_item *xllm_response_get_output(const xllm_response *pResponse, size_t iIndex);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `pResponse` | 输入 | 可为 `NULL` | 响应对象。 |
| `iIndex` | 输入 | 否 | 输出项索引，从 `0` 开始。 |

**返回值：**

- 成功返回输出项借用指针。
- `pResponse=NULL` 或索引越界时返回 `NULL`。

**资源归属：**

返回指针属于 response，不能释放或修改。

**范例代码：**

```c
const xllm_output_item *out = xllm_response_get_output(response, 0);
if (out && out->eKind == XLLM_OUTPUT_MESSAGE) {
    /* 读取 out->as.tMessage */
}
```

**相关 API：**

- `xllm_response_get_output_count`

---

### xllm_response_get_tool_call_count

统计响应中的 tool call 数量。

**函数原型：**

```c
XLLM_API size_t xllm_response_get_tool_call_count(const xllm_response *pResponse);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `pResponse` | 输入 | 可为 `NULL` | 响应对象。 |

**返回值：**

- tool call 输出项数量。
- `pResponse=NULL` 时返回 `0`。

**资源归属：**

不分配资源。

**范例代码：**

```c
size_t n = xllm_response_get_tool_call_count(response);
```

**相关 API：**

- `xllm_response_get_tool_call`

---

### xllm_response_get_tool_call

按 tool call 序号读取 tool call。

**函数原型：**

```c
XLLM_API const xllm_output_tool_call *xllm_response_get_tool_call(const xllm_response *pResponse, size_t iIndex);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `pResponse` | 输入 | 可为 `NULL` | 响应对象。 |
| `iIndex` | 输入 | 否 | 第几个 tool call，不是 `pOutputs` 的原始索引。 |

**返回值：**

- 成功返回 tool call 借用指针。
- 不存在时返回 `NULL`。

**资源归属：**

返回指针属于 response。`sArgumentsJson` 等字符串也属于 response。

**补充说明：**

该函数会跳过非 `XLLM_OUTPUT_TOOL_CALL` 输出项，所以 `iIndex=0` 表示第一个 tool call，而不是第一个 output。

**范例代码：**

```c
for (size_t i = 0; i < xllm_response_get_tool_call_count(response); ++i) {
    const xllm_output_tool_call *call = xllm_response_get_tool_call(response, i);
    printf("tool: %s args: %s\n", call->sToolName, call->sArgumentsJson);
}
```

**相关 API：**

- `xllm_tool_exec_result_free`
- [api-tools.md](api-tools.md)

---

### xllm_response_get_json

读取指定输出项、指定 part 中的 JSON 值。

**函数原型：**

```c
XLLM_API const xvalue *xllm_response_get_json(const xllm_response *pResponse, size_t iOutputIndex, size_t iPartIndex);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `pResponse` | 输入 | 可为 `NULL` | 响应对象。 |
| `iOutputIndex` | 输入 | 否 | 输出项索引。 |
| `iPartIndex` | 输入 | 否 | message part 索引。 |

**返回值：**

- 如果该位置是 `XLLM_PART_JSON`，返回 `xvalue` 借用指针。
- 位置无效或不是 JSON part 时返回 `NULL`。

**资源归属：**

返回的 `xvalue` 属于 response。要长期保存需按 xvalue 规则复制。

**范例代码：**

```c
const xvalue *json = xllm_response_get_json(response, 0, 0);
if (json) {
    /* 读取 xvalue */
}
```

**相关 API：**

- `xllm_response_get_first_json`

---

### xllm_response_get_first_json

查找响应中的第一个 JSON part。

**函数原型：**

```c
XLLM_API const xvalue *xllm_response_get_first_json(const xllm_response *pResponse, size_t *piOutputIndex, size_t *piPartIndex);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `pResponse` | 输入 | 可为 `NULL` | 响应对象。 |
| `piOutputIndex` | 输出 | 可为 `NULL` | 写入 JSON 所在 output 索引；未找到时写入 `(size_t)-1`。 |
| `piPartIndex` | 输出 | 可为 `NULL` | 写入 JSON 所在 part 索引；未找到时写入 `(size_t)-1`。 |

**返回值：**

- 找到时返回第一个 JSON `xvalue` 借用指针。
- 未找到或 `pResponse=NULL` 时返回 `NULL`。

**资源归属：**

返回的 `xvalue` 属于 response。

**范例代码：**

```c
size_t output_index;
size_t part_index;
const xvalue *json = xllm_response_get_first_json(response, &output_index, &part_index);
if (json) {
    printf("json at output=%zu part=%zu\n", output_index, part_index);
}
```

**相关 API：**

- `xllm_response_get_json`

---

### xllm_response_free

释放 response。

**函数原型：**

```c
XLLM_API void xllm_response_free(xllm_response *pResponse);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `pResponse` | 输入 | 可为 `NULL` | 要释放的响应。 |

**返回值：**

无。

**资源归属：**

释放 response 外层对象以及内部字符串、outputs、usage/refusal/safety/effective params/error/raw/vendor extra。

**补充说明：**

- 只能释放 xllm 返回的 response。
- 释放后所有从 response getter 拿到的指针都失效。

**范例代码：**

```c
xllm_response_free(response);
response = NULL;
```

**相关 API：**

- `xllm_chat_ex`

---

## 工具执行结果释放

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

释放 `pResult` 内部 content parts 和扩展数据，但不释放 `pResult` 外层结构体本身。

**补充说明：**

如果你实现了 `xllm_tool_execute_fn`，并在 `pResult` 里分配了返回内容，调用链结束后应按该函数清理。

**范例代码：**

```c
xllm_tool_exec_result result;
memset(&result, 0, sizeof(result));
/* 填充 result */
xllm_tool_exec_result_free(&result);
```

**相关 API：**

- [api-tools.md](api-tools.md)

---

## 常见用法

### 读取普通文本

```c
xllm_response *response = NULL;
xllm_error err;
xllm_error_init(&err);

if (xllm_chat_ex(runtime, &request, NULL, &response, &err) == XRT_NET_OK) {
    const char *text = xllm_response_get_text(response);
    printf("%s\n", text ? text : "");
}

xllm_response_free(response);
xllm_error_free(&err);
```

### 读取 JSON 输出

```c
const xvalue *json = xllm_response_get_first_json(response, NULL, NULL);
if (!json) {
    fprintf(stderr, "response does not contain JSON\n");
}
```

### 读取 tool call

```c
size_t count = xllm_response_get_tool_call_count(response);
for (size_t i = 0; i < count; ++i) {
    const xllm_output_tool_call *call = xllm_response_get_tool_call(response, i);
    printf("call %s: %s\n", call->sToolName, call->sArgumentsJson);
}
```

---

## 常见错误

| 问题 | 常见原因 | 处理方式 |
| --- | --- | --- |
| `xllm_response_get_text` 返回 `NULL` | 响应只有 JSON、tool call、artifact，或调用失败。 | 先检查 `eStatus` 和 output 类型。 |
| tool call 索引和 output 索引混淆 | `xllm_response_get_tool_call` 使用的是“第几个 tool call”。 | 需要原始 output 时用 `xllm_response_get_output`。 |
| 释放后继续使用文本指针 | getter 返回的是借用指针。 | 在 `xllm_response_free` 前复制需要保存的字符串。 |
| `xllm_request_reset` 崩溃 | request 内部混入了不该由 xllm 释放的外部内存。 | 使用 helper 构造，或按示例使用 xrt 分配并保持归属清晰。 |
| 错误对象泄漏 | 调用后没有 `xllm_error_free`。 | 每个 `xllm_error_init` 最终配对 `xllm_error_free`。 |

## 相关示例

- `examples\doubao\doubao_json_schema.c`
- `examples\gemini\gemini_json_schema.c`
- `examples\glm\glm_tool_loop.c`
- `examples\smoke_chat_ex_low_level.c`
- `build.bat smoke -Filter "chat_ex_low_level"`
