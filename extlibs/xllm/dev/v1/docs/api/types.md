# xllm 基础类型

> xllm 的公共类型、常量、枚举和资源约定。你读懂本页后，再看 `core`、`session`、`memory` API 会轻松很多。

[返回 API 索引](README.md) | [Core Runtime API](api-core.md) | [Request / Response API](api-request-response.md)

---

## 目录

- [版本与 Adapter 常量](#版本与-adapter-常量)
- [通用 Optional 类型](#通用-optional-类型)
- [日志与 Trace 类型](#日志与-trace-类型)
- [认证、代理与传输类型](#认证代理与传输类型)
- [模型能力类型](#模型能力类型)
- [Profile 类型](#profile-类型)
- [请求消息类型](#请求消息类型)
- [工具调用类型](#工具调用类型)
- [响应与事件类型](#响应与事件类型)
- [错误类型](#错误类型)
- [调用选项类型](#调用选项类型)
- [Session 相关类型](#session-相关类型)
- [运行时选项类型](#运行时选项类型)
- [资源归属总则](#资源归属总则)

---

## 版本与 Adapter 常量

### 版本常量

| 常量 | 当前值 | 说明 |
| --- | --- | --- |
| `XLLM_VERSION_MAJOR` | `0` | 主版本号。`0.x` 阶段代表 API 仍在收敛。 |
| `XLLM_VERSION_MINOR` | `1` | 次版本号。新增能力通常提升该值。 |
| `XLLM_VERSION_PATCH` | `0` | 修订版本号。修复类变更通常提升该值。 |

你通常不需要直接拼接这些宏。要获取运行时版本字符串，请使用 `xllm_version()`，详见 [api-core.md](api-core.md)。

### 内置 Adapter 名称

这些常量用于 `xllm_profile.sAdapter`，也可用于日志、配置文件或宿主 UI 中的能力选择。

| 常量 | 值 | 说明 |
| --- | --- | --- |
| `XLLM_ADAPTER_OPENAI_COMPAT` | `"openai_compat"` | OpenAI 兼容接口，包括很多兼容 OpenAI schema 的网关。 |
| `XLLM_ADAPTER_GLM_NATIVE` | `"glm_native"` | 智谱 GLM 原生适配器。 |
| `XLLM_ADAPTER_MINIMAX_NATIVE` | `"minimax_native"` | MiniMax 原生适配器。 |
| `XLLM_ADAPTER_KIMI_NATIVE` | `"kimi_native"` | Kimi 原生适配器。 |
| `XLLM_ADAPTER_GEMINI_NATIVE` | `"gemini_native"` | Gemini 原生适配器。 |
| `XLLM_ADAPTER_VERTEX_GEMINI_NATIVE` | `"vertex_gemini_native"` | Vertex Gemini 原生适配器。 |
| `XLLM_ADAPTER_QWEN_NATIVE` | `"qwen_native"` | Qwen 原生适配器。 |
| `XLLM_ADAPTER_DOUBAO_NATIVE` | `"doubao_native"` | Doubao 原生适配器。 |
| `XLLM_ADAPTER_ANTHROPIC_NATIVE` | `"anthropic_native"` | Anthropic 原生适配器。 |
| `XLLM_ADAPTER_OLLAMA_NATIVE` | `"ollama_native"` | Ollama 本地/自托管适配器。 |

**补充说明：**

- Adapter 名称只是协议适配层，不等于模型名称。
- 模型名称写在 `xllm_profile_models` 的 `sModelId` 中。
- 注册内置适配器后，profile 的 `sAdapter` 必须能匹配已注册的 adapter。

---

## 通用 Optional 类型

xllm 使用 `bSet + value` 形式表达“调用者是否显式设置了这个值”。这比用 `0` 表示默认值更安全，因为很多参数本身可以合法为 `0`。

| 类型 | 字段 | 说明 |
| --- | --- | --- |
| `xllm_opt_bool` | `bSet`, `bValue` | 可选布尔值。`bSet=false` 表示使用默认策略。 |
| `xllm_opt_i32` | `bSet`, `iValue` | 可选 `int32`。 |
| `xllm_opt_u32` | `bSet`, `iValue` | 可选 `uint32`。常用于毫秒超时、token 数等。 |
| `xllm_opt_u64` | `bSet`, `uValue` | 可选 `uint64`。常用于字节大小或更大计数。 |
| `xllm_opt_f64` | `bSet`, `fValue` | 可选浮点数。常用于 temperature、top_p 等参数。 |

**使用示例：**

```c
xllm_generation_params gen;
memset(&gen, 0, sizeof(gen));

gen.tTemperature.bSet = true;
gen.tTemperature.fValue = 0.2;

gen.tMaxOutputTokens.bSet = true;
gen.tMaxOutputTokens.iValue = 1024;
```

**补充说明：**

- 优先使用对应的 `*_init()` 初始化外层 options，再设置需要覆盖的字段。
- 如果 `bSet=false`，即使 value 字段里有旧值，也应该被视为“未设置”。

---

## 日志与 Trace 类型

### `xllm_log_level`

日志等级用于 `xllm_log_callback`。

| 值 | 说明 | 建议用途 |
| --- | --- | --- |
| `XLLM_LOG_ERROR` | 错误 | 请求失败、初始化失败、不可恢复问题。 |
| `XLLM_LOG_WARN` | 警告 | 降级、重试、可恢复异常。 |
| `XLLM_LOG_INFO` | 信息 | runtime 创建、provider 调用摘要。 |
| `XLLM_LOG_DEBUG` | 调试 | 本地排查时打开。 |
| `XLLM_LOG_TRACE` | 细粒度跟踪 | 仅在定位复杂问题时使用。 |

### `xllm_log_event`

结构化日志事件。它帮助宿主把日志归类，而不是只靠字符串搜索。

| 值 | 说明 |
| --- | --- |
| `XLLM_LOG_EVENT_UNKNOWN` | 未分类事件。 |
| `XLLM_LOG_EVENT_RUNTIME_CREATE` | runtime 创建。 |
| `XLLM_LOG_EVENT_RUNTIME_DESTROY` | runtime 销毁。 |
| `XLLM_LOG_EVENT_PROVIDER_REQUEST_START` | provider 请求开始。 |
| `XLLM_LOG_EVENT_PROVIDER_RESPONSE_COMPLETE` | provider 请求成功完成。 |
| `XLLM_LOG_EVENT_PROVIDER_RESPONSE_FAILED` | provider 请求失败。 |
| `XLLM_LOG_EVENT_PROVIDER_RETRY_SCHEDULED` | 已安排重试。 |
| `XLLM_LOG_EVENT_STREAM_EVENT` | 流式事件。 |
| `XLLM_LOG_EVENT_SESSION_COMPACT_TRIGGERED` | session compact 被触发。 |
| `XLLM_LOG_EVENT_SESSION_COMPACT_RESULT` | session compact 结果。 |
| `XLLM_LOG_EVENT_TOOL_LOOP_ROUND` | tool loop 轮次。 |
| `XLLM_LOG_EVENT_TOOL_LOOP_EXECUTE` | tool 执行。 |
| `XLLM_LOG_EVENT_TOOL_LOOP_STOP` | tool loop 停止。 |
| `XLLM_LOG_EVENT_MEMORY_INGEST` | memory 入库。 |
| `XLLM_LOG_EVENT_MEMORY_SEARCH` | memory 检索。 |
| `XLLM_LOG_EVENT_MEMORY_HEALTH_CHECK` | memory 健康检查。 |
| `XLLM_LOG_EVENT_WORKSPACE_SYNC` | 工作区同步。 |
| `XLLM_LOG_EVENT_WATCHER_EVENT` | 文件监听事件。 |

### `xllm_trace_kind`

Trace 比日志更适合机器消费。trace payload 使用 `xvalue`，宿主可把它转成 JSON 保存。

| 值 | 说明 |
| --- | --- |
| `XLLM_TRACE_EVENT` | 通用事件。 |
| `XLLM_TRACE_REQUEST` | 请求构造或发送前信息。 |
| `XLLM_TRACE_RESPONSE` | 响应解析或完成信息。 |
| `XLLM_TRACE_STREAM` | 流式事件。 |
| `XLLM_TRACE_COMPACT` | session compact 信息。 |
| `XLLM_TRACE_TOOL_LOOP` | tool loop 信息。 |

### `xllm_debug_mode` 与 `xllm_redact_mode`

| 类型 | 值 | 说明 |
| --- | --- | --- |
| `xllm_debug_mode` | `XLLM_DEBUG_NONE` | 不输出额外调试材料。 |
| `xllm_debug_mode` | `XLLM_DEBUG_HEADERS` | 调试 header 级别信息。 |
| `xllm_debug_mode` | `XLLM_DEBUG_BODY` | 调试请求/响应正文级别信息。 |
| `xllm_debug_mode` | `XLLM_DEBUG_WIRE` | 调试 wire 级别信息，信息量最大。 |
| `xllm_redact_mode` | `XLLM_REDACT_DEFAULT` | 默认脱敏策略。 |
| `xllm_redact_mode` | `XLLM_REDACT_OFF` | 不脱敏。只应在本地临时排查时使用。 |
| `xllm_redact_mode` | `XLLM_REDACT_STRICT` | 严格脱敏，适合共享日志或 CI。 |

### 回调类型

```c
typedef void (*xllm_log_callback)(
    void *pCtx,
    xllm_log_level eLevel,
    const char *sComponent,
    const char *sMessage
);

typedef void (*xllm_trace_callback)(
    void *pCtx,
    xllm_trace_kind eKind,
    const xvalue *pPayload
);
```

**资源归属：**

- `pCtx` 由宿主传入，xllm 只保存指针，不接管生命周期。
- `sComponent`、`sMessage`、`pPayload` 在回调期间有效；如果宿主要异步保存，必须复制内容。

---

## 认证、代理与传输类型

### `xllm_auth_kind` / `xllm_auth`

| 值 | 说明 |
| --- | --- |
| `XLLM_AUTH_NONE` | 不附加认证信息。适合本地 Ollama 或由网关注入认证的场景。 |
| `XLLM_AUTH_BEARER` | 使用 `Authorization: Bearer <secret>`。 |
| `XLLM_AUTH_API_KEY_HEADER` | 使用自定义 header 携带 API key。 |

`xllm_auth` 字段：

| 字段 | 说明 |
| --- | --- |
| `eKind` | 认证方式。 |
| `sSecret` | 密钥字符串。调用者持有；runtime/profile 注册时会按实现需要复制或引用。 |
| `sHeaderName` | `XLLM_AUTH_API_KEY_HEADER` 使用的 header 名。 |
| `sScheme` | 自定义认证 scheme。一般 Bearer 不需要手动设置。 |

### `xllm_proxy_kind`

| 值 | 说明 |
| --- | --- |
| `XLLM_PROXY_UNSPECIFIED` | 未指定，使用 runtime 或 adapter 默认策略。 |
| `XLLM_PROXY_NONE` | 禁用代理。 |
| `XLLM_PROXY_SOCKS5` | 使用 SOCKS5 代理。 |
| `XLLM_PROXY_HTTP_CONNECT` | 使用 HTTP CONNECT 代理。 |

### `xllm_transport_options`

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `tConnectTimeoutMs` | `xllm_opt_u32` | 连接超时，单位毫秒。 |
| `tReadTimeoutMs` | `xllm_opt_u32` | 读取超时，单位毫秒。 |
| `tVerifyPeer` | `xllm_opt_bool` | 是否验证 TLS peer。生产环境通常应开启。 |
| `eProxyKind` | `xllm_proxy_kind` | 代理类型。 |
| `sProxyHost` | `const char *` | 代理主机。 |
| `tProxyPort` | `xllm_opt_u32` | 代理端口。 |
| `sProxyUser` / `sProxyPass` | `const char *` | 代理认证信息。 |
| `sCaBundlePath` | `const char *` | CA bundle 路径。 |
| `sClientCertPath` / `sClientKeyPath` | `const char *` | 客户端证书和私钥路径。 |
| `tVendorExtra` | `xvalue` | provider 或宿主扩展字段。 |

---

## 模型能力类型

### `xllm_capability_flags`

能力位描述一个模型/adapter 声称支持什么。宿主可以用它决定是否允许图片、文件、JSON 输出或工具调用。

| 常量 | 说明 |
| --- | --- |
| `XLLM_CAP_TEXT_IN` | 支持文本输入。 |
| `XLLM_CAP_IMAGE_IN` | 支持图片输入。 |
| `XLLM_CAP_FILE_IN` | 支持文件输入。 |
| `XLLM_CAP_AUDIO_IN` | 支持音频输入。 |
| `XLLM_CAP_VIDEO_IN` | 支持视频输入。 |
| `XLLM_CAP_TOOL_RESULT_IN` | 支持工具结果作为输入。 |
| `XLLM_CAP_TEXT_OUT` | 支持文本输出。 |
| `XLLM_CAP_IMAGE_OUT` | 支持图片输出。 |
| `XLLM_CAP_FILE_OUT` | 支持文件输出。 |
| `XLLM_CAP_AUDIO_OUT` | 支持音频输出。 |
| `XLLM_CAP_VIDEO_OUT` | 支持视频输出。 |
| `XLLM_CAP_JSON_OUT` | 支持 JSON 或结构化输出。 |
| `XLLM_CAP_TOOL_CALL_OUT` | 支持模型发起 tool call。 |
| `XLLM_CAP_THINKING_SUMMARY_OUT` | 支持输出思考摘要。 |
| `XLLM_CAP_THINKING_FULL_OUT` | 支持输出完整思考内容。 |
| `XLLM_CAP_STREAM` | 支持流式输出。 |
| `XLLM_CAP_REASONING_CONTROL` | 支持 reasoning 参数控制。 |
| `XLLM_CAP_PARALLEL_TOOL_CALL` | 支持并行 tool call。 |
| `XLLM_CAP_CITATION_OUT` | 支持引用/来源输出。 |

### 参数规则

`xllm_float_rule` 和 `xllm_u32_rule` 用于描述 provider 对 temperature、top_p、max output tokens 等参数的限制。

| `xllm_param_rule_kind` | 说明 |
| --- | --- |
| `XLLM_PARAM_RULE_UNSPECIFIED` | 未声明规则。 |
| `XLLM_PARAM_RULE_UNSUPPORTED` | 不支持该参数。 |
| `XLLM_PARAM_RULE_FIXED` | 固定值，调用者设置会被拒绝或归一化。 |
| `XLLM_PARAM_RULE_RANGE` | 允许范围，使用 `fMin/fMax` 或 `uMin/uMax`。 |
| `XLLM_PARAM_RULE_PASSTHROUGH` | 透传给 provider。 |

### `xllm_model_caps`

| 字段 | 说明 |
| --- | --- |
| `uFlags` | 能力位。 |
| `psSupportedMimeTypes` / `iSupportedMimeTypeCount` | 支持的 MIME 类型列表。 |
| `eWindowMode` | 上下文窗口模式。 |
| `uMaxContextTokens` | 最大上下文 token。 |
| `uMaxInputTokens` / `uMaxOutputTokens` | 输入/输出 token 限制。 |
| `uRecommendedOutputReserve` | 建议为输出预留的 token。 |
| `uMaxPartsPerMessage` | 单条消息最大 part 数。 |
| `uMaxImages` / `uMaxFiles` | 图片/文件数量限制。 |
| `uMaxPartBytes` | 单个 part 的字节限制。 |
| `sTokenizerId` | tokenizer 标识。 |
| `tTemperatureRule` / `tTopPRule` / `tMaxOutputTokensRule` | 参数规则。 |
| `tVendorExtra` | provider 扩展。 |

---

## Profile 类型

`xllm_profile` 是 xllm 选择 provider、adapter、模型、认证和默认参数的核心配置。

### `xllm_profile`

| 字段 | 说明 |
| --- | --- |
| `sId` | profile ID。请求里通过 `sProfileId` 引用它。 |
| `sName` | 给用户看的名称。 |
| `sProvider` | provider 名称，如 `openai`、`anthropic`、`ollama`。 |
| `sAdapter` | adapter 名称，通常使用 `XLLM_ADAPTER_*` 常量。 |
| `sBaseUrl` | provider endpoint。 |
| `tAuth` | 认证信息。 |
| `pDefaultHeaders` / `iDefaultHeaderCount` | 默认 HTTP headers。 |
| `tProviderOptions` | provider 专用选项。 |
| `tTransport` | 传输选项。 |
| `tModels` | 文本/多模态模型绑定。 |
| `tDefaults` | 默认生成、reasoning、response format 参数。 |
| `tVendorExtra` | 扩展字段。 |

### `xllm_model_binding`

| 字段 | 说明 |
| --- | --- |
| `sModelId` | 实际发送给 provider 的模型 ID。 |
| `sAliasOf` | 别名来源。用于 UI 或配置继承。 |
| `eCapMode` | 能力合并方式。 |
| `tCaps` | 模型能力。 |
| `tVendorExtra` | 扩展字段。 |

### `xllm_cap_mode`

| 值 | 说明 |
| --- | --- |
| `XLLM_CAP_MODE_AUTO` | 使用 adapter 默认能力推断。 |
| `XLLM_CAP_MODE_MERGE` | 合并 adapter 默认能力和 profile 显式能力。 |
| `XLLM_CAP_MODE_EXACT` | 只使用 profile 中声明的能力。 |

---

## 请求消息类型

### `xllm_request` 与 `xllm_turn`

`xllm_request` 用于 core 直接调用。`xllm_turn` 用于 session 层单轮输入。两者字段相近，但 `xllm_turn` 支持 `sSystemPrompt` 和 `eSystemMode`，更适合会话场景。

| 字段 | 出现在 | 说明 |
| --- | --- | --- |
| `sProfileId` | `xllm_request` | 本次请求使用的 profile。 |
| `eSlot` | 两者 | 自动、文本或多模态模型槽位。 |
| `sSystemPrompt` / `eSystemMode` | `xllm_turn` | 本轮 system prompt 如何作用于 session。 |
| `pMessages` / `iMessageCount` | 两者 | 消息列表。 |
| `pContextBlocks` / `iContextBlockCount` | 两者 | 额外上下文块。 |
| `pTools` / `iToolCount` | 两者 | 可用工具定义。 |
| `tToolPolicy` | 两者 | 工具选择策略。 |
| `tGeneration` | 两者 | 生成参数。 |
| `tResponseFormat` | 两者 | 响应格式要求。 |
| `tReasoning` | 两者 | reasoning 选项。 |
| `tVendorExtra` | 两者 | 扩展字段。 |

### 消息与内容

| 类型 | 说明 |
| --- | --- |
| `xllm_role` | 消息角色：system、user、assistant、tool。 |
| `xllm_part_kind` | 内容类型：text、image、file、audio、video、json。 |
| `xllm_source_kind` | 内容来源：inline text、inline bytes、URL、provider file id。 |
| `xllm_data_source` | 一个内容 part 的实际来源。 |
| `xllm_content_part` | 消息中的一个内容片段。 |
| `xllm_message` | 一条消息，可包含多个 part 和 tool call。 |
| `xllm_context_block` | 带优先级和 pinned 标记的上下文块。 |

**补充说明：**

- 字符串、数组和 `xvalue` 的生命周期应至少覆盖 API 调用期间。
- 如果使用 session 或 memory bridge，相关 API 可能会复制必要内容；具体以对应 API 页为准。

---

## 工具调用类型

| 类型 | 说明 |
| --- | --- |
| `xllm_tool_def` | 宿主暴露给模型的工具定义。 |
| `xllm_tool_policy` | 本次请求是否允许工具、是否强制工具、是否指定某个工具。 |
| `xllm_tool_choice_mode` | `auto`、`none`、`required`、`named`。 |
| `xllm_tool_call` | 请求消息中携带的 tool call。 |
| `xllm_output_tool_call` | 响应中模型生成的 tool call。 |
| `xllm_tool_exec_request` | xllm 请求宿主执行工具时传给 executor 的参数。 |
| `xllm_tool_exec_result` | 宿主执行工具后返回给 xllm 的结果。 |
| `xllm_tool_executor` | 同步工具执行器。 |
| `xllm_tool_executor_async` | 异步工具执行器。 |

---

## 响应与事件类型

### `xllm_response`

| 字段 | 说明 |
| --- | --- |
| `sId` | provider 响应 ID。 |
| `sProvider` / `sProfileId` / `sModel` | 实际调用信息。 |
| `eStatus` | 响应状态。 |
| `sFinishReason` | provider finish reason。 |
| `pOutputs` / `iOutputCount` | 结构化输出项。 |
| `sVisibleText` | 便于直接显示的合并文本。 |
| `tUsage` | token 用量。 |
| `tRefusal` / `tSafety` | 拒绝和安全信息。 |
| `tEffectiveParams` | 实际生效参数。 |
| `bHasError` / `tError` | 响应内错误信息。 |
| `tRaw` / `tVendorExtra` | 原始或扩展数据。 |

响应由 `xllm_chat` / `xllm_chat_ex` 等 API 分配，调用者必须用 `xllm_response_free()` 释放。

### 输出与状态枚举

| 类型 / 值 | 说明 |
| --- | --- |
| `XLLM_OUTPUT_MESSAGE` | 普通消息输出。 |
| `XLLM_OUTPUT_THINKING` | thinking 输出。 |
| `XLLM_OUTPUT_TOOL_CALL` | tool call 输出。 |
| `XLLM_OUTPUT_REFUSAL` | 拒绝输出。 |
| `XLLM_STATUS_COMPLETED` | 正常完成。 |
| `XLLM_STATUS_INCOMPLETE` | 未完整完成。 |
| `XLLM_STATUS_TOOL_CALL_REQUIRED` | 需要宿主执行工具。 |
| `XLLM_STATUS_REFUSED` | 模型拒绝。 |
| `XLLM_STATUS_CONTENT_FILTERED` | 内容被过滤。 |
| `XLLM_STATUS_CANCELLED` | 被取消。 |
| `XLLM_STATUS_ERRORED` | 出错。 |

### 流式事件

`xllm_event` 用于流式回调。事件类型包括开始、输出开始、文本 delta、thinking delta、tool call delta、artifact、usage、error 和结束。

事件回调：

```c
typedef bool (*xllm_event_callback)(const xllm_event *pEvent, void *pUserData);
```

回调返回 `false` 通常表示宿主不希望继续处理后续事件，具体行为以调用 API 的说明为准。

---

## 错误类型

### `xllm_error_code`

| 值 | 说明 |
| --- | --- |
| `XLLM_ERROR_NONE` | 无错误。 |
| `XLLM_ERROR_AUTH` | 认证失败。 |
| `XLLM_ERROR_QUOTA` | 额度不足。 |
| `XLLM_ERROR_RATE_LIMIT` | 触发限流。 |
| `XLLM_ERROR_TIMEOUT` | 超时。 |
| `XLLM_ERROR_NETWORK` | 网络错误。 |
| `XLLM_ERROR_CANCELLED` | 请求被取消。 |
| `XLLM_ERROR_INVALID_REQUEST` | 请求不合法。 |
| `XLLM_ERROR_UNSUPPORTED_CAPABILITY` | 能力不支持。 |
| `XLLM_ERROR_UNSUPPORTED_INPUT_TYPE` | 输入类型不支持。 |
| `XLLM_ERROR_UNSUPPORTED_MIME_TYPE` | MIME 类型不支持。 |
| `XLLM_ERROR_INPUT_TOO_LARGE` | 输入过大。 |
| `XLLM_ERROR_TOO_MANY_INPUT_PARTS` | 输入 part 过多。 |
| `XLLM_ERROR_MISSING_MULTIMODAL_MODEL` | 需要多模态模型但未配置。 |
| `XLLM_ERROR_MODEL_NOT_FOUND` | 找不到模型或 profile。 |
| `XLLM_ERROR_UPSTREAM_4XX` | 上游 4xx。 |
| `XLLM_ERROR_UPSTREAM_5XX` | 上游 5xx。 |
| `XLLM_ERROR_PARSE` | 解析失败。 |
| `XLLM_ERROR_INTERNAL` | 内部错误。 |
| `XLLM_ERROR_SESSION_CONTEXT_OVERFLOW` | session 上下文超限。 |
| `XLLM_ERROR_SESSION_COMPACT_FAILED` | session compact 失败。 |
| `XLLM_ERROR_SESSION_SUMMARY_FAILED` | session summary 失败。 |
| `XLLM_ERROR_SESSION_REQUIRES_MODEL_LIMITS` | session 需要模型限制信息才能估算上下文。 |

### `xllm_error`

`xllm_error` 是可由调用者持有的错误对象。使用前调用 `xllm_error_init()`，复用前调用 `xllm_error_reset()`，不再使用时调用 `xllm_error_free()`。

| 字段 | 说明 |
| --- | --- |
| `eCode` | xllm 归一化错误码。 |
| `iStatus` | xllm 内部状态或扩展状态。 |
| `iHttpStatus` | HTTP 状态码。 |
| `sMessage` | 面向开发者的错误消息。 |
| `sProviderCode` / `sProviderMessage` | provider 返回的错误码和消息。 |
| `sRequestId` | provider request id。 |
| `iMessageIndex` / `iPartIndex` | 出错的消息/part 索引。 |
| `uRequiredCapability` | 缺失的能力位。 |
| `sSelectedModel` | 已选择或尝试选择的模型。 |
| `sMimeType` | 相关 MIME 类型。 |
| `tVendorExtra` | 扩展错误信息。 |

---

## 调用选项类型

### `xllm_call_options`

| 字段 | 说明 |
| --- | --- |
| `eStreamMode` | 流式策略：auto、off、prefer、require。 |
| `uTimeoutMs` | 本次调用超时，单位毫秒。 |
| `pCancelToken` | 取消 token。调用者持有。 |
| `pfnOnEvent` / `pUserData` | 流式事件回调和用户上下文。 |
| `eArtifactPolicy` | artifact 处理策略。 |
| `pArtifactSink` | artifact sink。调用者持有。 |
| `uMaxRetries` | 最大重试次数。 |
| `uRetryBackoffBaseMs` / `uRetryBackoffMaxMs` | 重试退避时间。 |
| `fRetryJitter` | 重试抖动比例。 |
| `bBestEffortStructuredOutput` | 是否允许尽力而为的结构化输出。 |
| `eLocalFilePolicy` | 本地文件输入策略。 |
| `tVendorExtra` | 扩展字段。 |

相关枚举：

| 类型 | 值 | 说明 |
| --- | --- | --- |
| `xllm_stream_mode` | `XLLM_STREAM_AUTO` | 自动选择流式。 |
| `xllm_stream_mode` | `XLLM_STREAM_OFF` | 禁用流式。 |
| `xllm_stream_mode` | `XLLM_STREAM_PREFER` | 优先流式，不支持时可降级。 |
| `xllm_stream_mode` | `XLLM_STREAM_REQUIRE` | 必须流式，不支持则失败。 |
| `xllm_artifact_policy` | `XLLM_ARTIFACT_REFERENCE_ONLY` | 只保留引用。 |
| `xllm_artifact_policy` | `XLLM_ARTIFACT_INLINE_SMALL` | 小 artifact 内联。 |
| `xllm_artifact_policy` | `XLLM_ARTIFACT_STREAM_TO_SINK` | 流式写入 sink。 |
| `xllm_local_file_policy` | `XLLM_LOCAL_FILE_AUTO` | 自动选择文件处理方式。 |
| `xllm_local_file_policy` | `XLLM_LOCAL_FILE_INLINE_FIRST` | 优先内联。 |
| `xllm_local_file_policy` | `XLLM_LOCAL_FILE_UPLOAD_REUSE_FIRST` | 优先上传并复用 provider file id。 |

---

## Session 相关类型

| 类型 | 说明 |
| --- | --- |
| `xllm_create_options` | 创建 `xllm` convenience 对象时使用。 |
| `xllm_session_options` | 创建 session 时使用。 |
| `xllm_compact_options` | 手动 compact 时使用。 |
| `xllm_compact_result` | compact 结果。 |
| `xllm_token_count_result` | token 计数结果。 |

Session 详细 API 见 [api-session.md](api-session.md)。

---

## 运行时选项类型

### `xllm_runtime_options`

| 字段 | 说明 |
| --- | --- |
| `tAllocator` | 自定义分配器。未设置时使用默认分配策略。 |
| `pfnLog` / `pLogCtx` | 初始日志回调。 |
| `pfnTrace` / `pTraceCtx` | 初始 trace 回调。 |
| `eDebugMode` | 调试级别。 |
| `eRedactMode` | 脱敏策略。 |
| `tTransportDefaults` | runtime 级默认传输配置。 |
| `tVendorExtra` | 扩展字段。 |

运行时创建、销毁和回调设置见 [api-core.md](api-core.md)。

---

## 资源归属总则

- `*_init()`：初始化调用者提供的结构体，不分配长期资源，通常可安全用于栈上对象。
- `*_reset()`：清理结构体内部持有的资源，让对象可以复用或安全丢弃。
- `*_free()`：释放由 xllm 分配或填充的结果对象内部资源；有些函数会连同外层对象一起释放。
- `*_destroy()`：销毁 opaque handle，如 `xllm_runtime`、`xllm_cancel_token`、`xllm_session`。
- `const char *` 输入：默认由调用者保证在 API 调用期间有效；是否被复制要看具体 API 文档。
- 返回的 `const char *`：除非对应 API 明确说明，否则视为借用指针，生命周期跟随所属对象。
- `xvalue`：通常用于扩展字段或原始 JSON 数据；如果要跨对象长期保存，应按 xrt/xvalue 的规则复制或持有。

---

## 相关示例

- `examples\smoke_chat_ex_low_level.c`
- `examples\smoke_log_event_taxonomy.c`
- `build.bat smoke -Filter "chat_ex_low_level,log_event_taxonomy"`
