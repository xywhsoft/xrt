# xllm Session API

> Session API 用来管理短期对话历史、system prompt、自动 compact、工具循环和会话状态导入导出。

[返回 API 索引](README.md) | [Tools API](api-tools.md) | [Request / Response API](api-request-response.md)

---

## 目录

- [模块定位](#模块定位)
- [两种会话入口](#两种会话入口)
- [Turn 构造 API](#turn-构造-api)
- [轻量 xllm 对象](#轻量-xllm-对象)
- [Session 生命周期](#session-生命周期)
- [Session Chat](#session-chat)
- [Compact](#compact)
- [Session State](#session-state)
- [常见用法](#常见用法)
- [常见错误](#常见错误)
- [相关示例](#相关示例)

---

## 模块定位

Session 解决的是“同一个任务里多轮短期上下文怎么保留”的问题。它适合：

- AI IDE 中一次任务会连续多轮对话。
- 需要把用户输入、assistant 输出、tool call 和 tool result 组织成历史。
- 需要在历史太长时 compact 或 summary。
- 需要导出 session state，稍后恢复任务。

Session 不等于长期 memory。长期知识、项目索引、用户事实和偏好应使用 memory API。

---

## 两种会话入口

| 入口 | 类型 | 适合场景 |
| --- | --- | --- |
| 轻量对象 | `xllm *` | 只想复用 profile、system prompt、默认 call options，并使用 `xllm_send_ex` 从 turn 构造 request。它不维护长期历史。 |
| Session 对象 | `xllm_session *` | 需要短期历史、compact、state export/import 和自动提交 turn/response。 |

新集成一般优先使用 `xllm_session`。`xllm *` 更像 convenience wrapper。

---

## Turn 构造 API

### xllm_turn_init

初始化 `xllm_turn`。

**函数原型：**

```c
XLLM_API void xllm_turn_init(xllm_turn *pTurn);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `pTurn` | 输出 | 可为 `NULL` | 要初始化的 turn。 |

**返回值：**

无。

**补充说明：**

默认值包括 `XLLM_SLOT_AUTO`、`XLLM_SYSTEM_INHERIT`、`XLLM_TOOL_CHOICE_AUTO`、`XLLM_RESPONSE_TEXT` 和 `XLLM_REASONING_DEFAULT`。

**范例代码：**

```c
xllm_turn turn;
xllm_turn_init(&turn);
```

---

### xllm_turn_reset

释放 turn 内部资源并恢复默认值。

**函数原型：**

```c
XLLM_API void xllm_turn_reset(xllm_turn *pTurn);
```

**资源归属：**

释放通过 turn helper 添加的文本、图片、文件、工具、stop sequence、schema 等内部资源。

---

### xllm_turn_clone

克隆 turn。

**函数原型：**

```c
XLLM_API int xllm_turn_clone(xllm_turn *pOut, const xllm_turn *pIn);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `pOut` | 输出 | 否 | 克隆目标。 |
| `pIn` | 输入 | 否 | 被克隆的 turn。 |

**返回值：**

- `XRT_NET_OK`：克隆成功。
- `XRT_NET_ERROR`：参数错误或分配失败。

**资源归属：**

`pOut` 成功后持有独立资源，最终用 `xllm_turn_reset` 清理。

---

### xllm_turn_set_system_prompt

设置本轮 system prompt。

**函数原型：**

```c
XLLM_API int xllm_turn_set_system_prompt(xllm_turn *pTurn, const char *sText);
```

**说明：**

函数会复制 `sText`。传 `NULL` 可清除本轮 system prompt。

---

### xllm_turn_set_system_mode

设置本轮 system prompt 如何作用于 session。

**函数原型：**

```c
XLLM_API int xllm_turn_set_system_mode(xllm_turn *pTurn, xllm_system_mode eMode);
```

| 值 | 说明 |
| --- | --- |
| `XLLM_SYSTEM_INHERIT` | 使用 session 默认 system prompt。 |
| `XLLM_SYSTEM_REPLACE` | 本轮替换 system prompt。 |
| `XLLM_SYSTEM_APPEND` | 本轮追加 system prompt。 |

---

### xllm_turn_set_stop_sequences

设置 stop sequences。

**函数原型：**

```c
XLLM_API int xllm_turn_set_stop_sequences(xllm_turn *pTurn, const char **psStop, size_t iStopCount);
```

函数会复制 stop sequence 字符串。`iStopCount=0` 可清空。

---

### xllm_turn_set_json_schema_response

要求本轮返回 JSON schema 输出。

**函数原型：**

```c
XLLM_API int xllm_turn_set_json_schema_response(
    xllm_turn *pTurn,
    const char *sSchemaName,
    xvalue tJsonSchema,
    xvalue tVendorExtra
);
```

**补充说明：**

- `sSchemaName` 会被复制。
- `tJsonSchema` 和 `tVendorExtra` 会按 xvalue 引用规则持有。
- provider 不支持 strict schema 时可能返回 `XLLM_ERROR_UNSUPPORTED_CAPABILITY`。

---

### 输入添加函数

这些函数向 turn 添加用户输入 part。

```c
XLLM_API int xllm_turn_add_user_text(xllm_turn *pTurn, const char *sText);
XLLM_API int xllm_turn_add_image_url(xllm_turn *pTurn, const char *sUrl, const char *sMimeType);
XLLM_API int xllm_turn_add_image_file(xllm_turn *pTurn, const char *sPath, const char *sMimeType);
XLLM_API int xllm_turn_add_image_file_id(xllm_turn *pTurn, const char *sFileId, const char *sMimeType);
XLLM_API int xllm_turn_add_file_url(xllm_turn *pTurn, const char *sUrl, const char *sMimeType);
XLLM_API int xllm_turn_add_file(xllm_turn *pTurn, const char *sPath, const char *sMimeType);
XLLM_API int xllm_turn_add_file_file_id(xllm_turn *pTurn, const char *sFileId, const char *sMimeType);
```

**返回值：**

- `XRT_NET_OK`：添加成功。
- `XRT_NET_ERROR`：参数错误或分配失败。

**资源归属：**

函数会复制输入字符串，随 `xllm_turn_reset` 释放。

**范例代码：**

```c
xllm_turn_add_user_text(&turn, "解释这个错误。");
xllm_turn_add_image_url(&turn, "https://example.com/a.png", "image/png");
```

工具相关的 `xllm_turn_add_tool` 和 `xllm_turn_set_tool_choice` 见 [api-tools.md](api-tools.md)。

---

## 轻量 xllm 对象

### xllm_create / xllm_destroy

创建和销毁轻量 `xllm` 对象。

```c
XLLM_API xllm *xllm_create(xllm_runtime *pRuntime, const xllm_create_options *pOptions);
XLLM_API void xllm_destroy(xllm *pLlm);
```

**说明：**

- `xllm_create` 需要已有 runtime。
- 如果 `pOptions->sInitialProfileId` 不存在，创建失败并返回 `NULL`。
- `xllm_destroy` 可接收 `NULL`。

### xllm_bind_profile

绑定默认 profile。

```c
XLLM_API int xllm_bind_profile(xllm *pLlm, const char *sProfileId);
```

成功后 `xllm_send_ex` 使用该 profile 构造 request。

### xllm_set_system_prompt / xllm_get_system_prompt

设置和读取轻量对象的 system prompt。

```c
XLLM_API int xllm_set_system_prompt(xllm *pLlm, const char *sText);
XLLM_API const char *xllm_get_system_prompt(const xllm *pLlm);
```

返回的 system prompt 指针属于 `pLlm`，不要释放。

### xllm_send / xllm_send_ex

从 turn 构造一次 request 并调用模型。

```c
XLLM_API int xllm_send(
    xllm *pLlm,
    const xllm_turn *pTurn,
    const xllm_call_options *pOptions,
    xllm_response **ppResponse
);

XLLM_API int xllm_send_ex(
    xllm *pLlm,
    const xllm_turn *pTurn,
    const xllm_call_options *pOptions,
    xllm_response **ppResponse,
    xllm_error *pError
);
```

`xllm_send_ex` 会使用默认 profile 和 system prompt，但不会维护 session 历史。

### xllm_send_async_*

```c
XLLM_API xfuture *xllm_send_async_thread(xllm *pLlm, const xllm_turn *pTurn, const xllm_call_options *pOptions);
XLLM_API xfuture *xllm_send_async_engine(xllm *pLlm, xnetengine *pEngine, uint32 uAffinityKey, const xllm_turn *pTurn, const xllm_call_options *pOptions);
XLLM_API xfuture *xllm_send_async_co(xllm *pLlm, xcosched *pSched, const xllm_turn *pTurn, const xllm_call_options *pOptions, size_t iStackSize);
```

异步 future 的读取和释放遵循 xrt `xfuture` 规则。

---

## Session 生命周期

### xllm_session_options_init

初始化 session options。

```c
XLLM_API void xllm_session_options_init(xllm_session_options *pOptions);
```

默认值：

- `bEnableAutoCompact = true`
- `fCompactTriggerRatio = 0.85`
- `uCompactTriggerTurns = 16`
- `uKeepRecentTurns = 8`
- `bKeepActiveToolChain = true`
- `eCompactStrategy = XLLM_COMPACT_SUMMARIZE`

### xllm_session_create / xllm_session_destroy

创建和销毁 session。

```c
XLLM_API int xllm_session_create(
    xllm_runtime *pRuntime,
    const xllm_session_options *pOptions,
    xllm_session **ppSession
);

XLLM_API void xllm_session_destroy(xllm_session *pSession);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `pRuntime` | 输入 | 否 | runtime。 |
| `pOptions` | 输入 | 可为 `NULL` | session 配置。 |
| `ppSession` | 输出 | 否 | 成功时写入 session。 |

**资源归属：**

成功后 session 归调用者所有，用 `xllm_session_destroy` 释放。

### xllm_session_set_system_prompt

设置 session 默认 system prompt。

```c
XLLM_API int xllm_session_set_system_prompt(xllm_session *pSession, const char *sText);
```

`sText` 会被复制。传 `NULL` 清除。

### xllm_session_clear_history

清空 session 历史和 summary。

```c
XLLM_API int xllm_session_clear_history(xllm_session *pSession);
```

保留 session 对象、profile、system prompt 和 options，只清理已提交历史。

工具 executor 设置见 [api-tools.md](api-tools.md)。

---

## Session Chat

### xllm_session_chat / xllm_session_chat_ex

用 session 历史 + 本轮 turn 调用模型。

```c
XLLM_API int xllm_session_chat(
    xllm_session *pSession,
    const xllm_turn_request *pTurn,
    const xllm_call_options *pOptions,
    xllm_response **ppResponse
);

XLLM_API int xllm_session_chat_ex(
    xllm_session *pSession,
    const xllm_turn_request *pTurn,
    const xllm_call_options *pOptions,
    xllm_response **ppResponse,
    xllm_error *pError
);
```

**功能：**

函数会从 session 当前历史创建请求快照，调用模型，成功后把本轮 turn、可能的工具链补充消息和最终 response 提交到 session 历史。

**返回值：**

- `XRT_NET_OK`：调用成功。
- 非 `XRT_NET_OK`：参数错误、构造快照失败、provider 调用失败等。

**资源归属：**

成功后 `*ppResponse` 由调用者用 `xllm_response_free` 释放。session 保留自己的历史副本。

### xllm_session_chat_async_*

```c
XLLM_API xfuture *xllm_session_chat_async_thread(xllm_session *pSession, const xllm_turn_request *pTurn, const xllm_call_options *pOptions);
XLLM_API xfuture *xllm_session_chat_async_engine(xllm_session *pSession, xnetengine *pEngine, uint32 uAffinityKey, const xllm_turn_request *pTurn, const xllm_call_options *pOptions);
XLLM_API xfuture *xllm_session_chat_async_co(xllm_session *pSession, xcosched *pSched, const xllm_turn_request *pTurn, const xllm_call_options *pOptions, size_t iStackSize);
```

session、runtime、engine/scheduler 必须在 future 完成前保持有效。

---

## Compact

### xllm_compact_options_init

初始化 compact options。

```c
XLLM_API void xllm_compact_options_init(xllm_compact_options *pOptions);
```

默认：

- `eMode = XLLM_COMPACT_TO_FIT_CURRENT_MODEL`
- `eStrategy = XLLM_COMPACT_SUMMARIZE`

### xllm_session_compact

手动 compact session 历史。

```c
XLLM_API int xllm_session_compact(
    xllm_session *pSession,
    const xllm_compact_options *pOptions,
    xllm_compact_result *pResult
);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `pSession` | 输入/输出 | 否 | session。 |
| `pOptions` | 输入 | 可为 `NULL` | compact 选项；传 `NULL` 使用 session 默认策略。 |
| `pResult` | 输出 | 否 | compact 结果。 |

**返回值：**

- `XRT_NET_OK`：compact 流程完成；是否实际压缩看 `pResult->bCompacted`。
- `XRT_NET_ERROR`：参数错误或内部失败。

**补充说明：**

- summarize 策略会优先尝试模型摘要，失败时可能使用本地 rolling summary。
- 如果摘要后 token 没有下降，可能退回 truncate 或不应用候选结果。
- 自动 compact 由 session chat 成功提交后触发。

---

## Session State

### xllm_session_export_state

导出 session state。

```c
XLLM_API int xllm_session_export_state(
    xllm_session *pSession,
    xllm_session_state **ppState
);
```

成功后 `*ppState` 由调用者用 `xllm_session_state_free` 释放。

### xllm_session_import_state

从 state 恢复 session。

```c
XLLM_API int xllm_session_import_state(
    xllm_runtime *pRuntime,
    const xllm_session_state *pState,
    xllm_session **ppSession
);
```

runtime 必须已经注册 state 所需 profile。

### xllm_session_state_to_xvalue

把 state 转成 `xvalue`。

```c
XLLM_API int xllm_session_state_to_xvalue(
    const xllm_session_state *pState,
    xvalue *ptValue
);
```

当前导出格式写入 `type = "xllm_session_state"` 和 `version = 2`，并包含 profile、system prompt、summary、compact options 和 history。

### xllm_session_state_from_xvalue

从 `xvalue` 解析 state。

```c
XLLM_API int xllm_session_state_from_xvalue(
    xvalue tValue,
    xllm_session_state **ppState
);
```

成功后 `*ppState` 用 `xllm_session_state_free` 释放。

### xllm_session_state_free

释放 state。

```c
XLLM_API void xllm_session_state_free(xllm_session_state *pState);
```

可接收 `NULL`。

---

## 常见用法

### 最小 session chat

```c
xllm_session_options opt;
xllm_session *session = NULL;
xllm_turn turn;
xllm_response *response = NULL;
xllm_error err;

xllm_session_options_init(&opt);
opt.sProfileId = "demo";
opt.sSystemPrompt = "你是一个简洁的助手。";

xllm_session_create(runtime, &opt, &session);
xllm_turn_init(&turn);
xllm_error_init(&err);

xllm_turn_add_user_text(&turn, "解释 xllm session 的用途。");
if (xllm_session_chat_ex(session, &turn, NULL, &response, &err) == XRT_NET_OK) {
    printf("%s\n", xllm_response_get_text(response));
}

xllm_response_free(response);
xllm_error_free(&err);
xllm_turn_reset(&turn);
xllm_session_destroy(session);
```

### 导出和恢复

```c
xllm_session_state *state = NULL;
xllm_session *restored = NULL;

if (xllm_session_export_state(session, &state) == XRT_NET_OK) {
    xllm_session_import_state(runtime, state, &restored);
}

xllm_session_destroy(restored);
xllm_session_state_free(state);
```

---

## 常见错误

| 问题 | 常见原因 | 处理方式 |
| --- | --- | --- |
| `session chat arguments are invalid` | session 没有 profile，turn 或 response 输出参数为空。 | 创建 session 时设置 `sProfileId`，或确认导入 state 后 profile 存在。 |
| 历史越来越长 | 没有启用 compact 或 keep recent turns 太大。 | 使用默认 auto compact，或手动调用 `xllm_session_compact`。 |
| session 被当作长期记忆 | session 只保存短期任务上下文。 | 长期知识使用 memory API。 |
| 恢复 state 失败 | runtime 未注册 state 所需 profile。 | 先注册 adapter/profile，再 import state。 |
| 异步 session 崩溃 | future 未完成前销毁 session/runtime。 | 等 future 完成后再释放对象。 |

## 相关示例

- `examples\agent_loop\agent_loop.c`
- `examples\glm\glm_session.c`
- `examples\gemini\gemini_session.c`
- `examples\smoke_session_history.c`
- `examples\smoke_session_compact.c`
- `examples\smoke_session_state_options.c`
