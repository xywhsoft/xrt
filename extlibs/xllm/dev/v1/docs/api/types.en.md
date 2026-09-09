# xllm Basic Types

> Public types, constants, enums, and resource conventions in xllm. After you understand this page, the `core`, `session`, and `memory` APIs will be much easier to read.

[Back to API Index](README.en.md) | [Core Runtime API](api-core.en.md) | [Request / Response API](api-request-response.en.md)

---

## Table of Contents

- [Version and Adapter Constants](#version-and-adapter-constants)
- [Common Optional Types](#common-optional-types)
- [Log and Trace Types](#log-and-trace-types)
- [Authentication, Proxy, and Transport Types](#authentication-proxy-and-transport-types)
- [Model Capability Types](#model-capability-types)
- [Profile Types](#profile-types)
- [Request Message Types](#request-message-types)
- [Tool Call Types](#tool-call-types)
- [Response and Event Types](#response-and-event-types)
- [Error Types](#error-types)
- [Call Option Types](#call-option-types)
- [Session-Related Types](#session-related-types)
- [Runtime Option Types](#runtime-option-types)
- [Resource Ownership Rules](#resource-ownership-rules)

---

## Version and Adapter Constants

### Version Constants

| Constant | Current Value | Description |
| --- | --- | --- |
| `XLLM_VERSION_MAJOR` | `0` | Major version. The `0.x` stage means the API is still converging. |
| `XLLM_VERSION_MINOR` | `1` | Minor version. New capabilities usually increase this value. |
| `XLLM_VERSION_PATCH` | `0` | Patch version. Fix-only changes usually increase this value. |

You usually do not need to concatenate these macros directly. To get the runtime version string, use `xllm_version()`; see [api-core.en.md](api-core.en.md).

### Built-in Adapter Names

These constants are used by `xllm_profile.sAdapter`. They can also be used in logs, configuration files, or host UI capability selection.

| Constant | Value | Description |
| --- | --- | --- |
| `XLLM_ADAPTER_OPENAI_COMPAT` | `"openai_compat"` | OpenAI-compatible API, including many gateways that follow the OpenAI schema. |
| `XLLM_ADAPTER_GLM_NATIVE` | `"glm_native"` | Native Zhipu GLM adapter. |
| `XLLM_ADAPTER_MINIMAX_NATIVE` | `"minimax_native"` | Native MiniMax adapter. |
| `XLLM_ADAPTER_KIMI_NATIVE` | `"kimi_native"` | Native Kimi adapter. |
| `XLLM_ADAPTER_GEMINI_NATIVE` | `"gemini_native"` | Native Gemini adapter. |
| `XLLM_ADAPTER_VERTEX_GEMINI_NATIVE` | `"vertex_gemini_native"` | Native Vertex Gemini adapter. |
| `XLLM_ADAPTER_QWEN_NATIVE` | `"qwen_native"` | Native Qwen adapter. |
| `XLLM_ADAPTER_DOUBAO_NATIVE` | `"doubao_native"` | Native Doubao adapter. |
| `XLLM_ADAPTER_ANTHROPIC_NATIVE` | `"anthropic_native"` | Native Anthropic adapter. |
| `XLLM_ADAPTER_OLLAMA_NATIVE` | `"ollama_native"` | Local/self-hosted Ollama adapter. |

**Notes:**

- Adapter names describe protocol adapters, not model names.
- Model names are written in `sModelId` inside `xllm_profile_models`.
- After registering a built-in adapter, the profile's `sAdapter` must match a registered adapter.

---

## Common Optional Types

xllm uses the `bSet + value` pattern to express whether the caller explicitly set a value. This is safer than using `0` as "default", because many parameters may legally be `0`.

| Type | Fields | Description |
| --- | --- | --- |
| `xllm_opt_bool` | `bSet`, `bValue` | Optional boolean. `bSet=false` means use the default policy. |
| `xllm_opt_i32` | `bSet`, `iValue` | Optional `int32`. |
| `xllm_opt_u32` | `bSet`, `iValue` | Optional `uint32`. Commonly used for millisecond timeouts and token counts. |
| `xllm_opt_u64` | `bSet`, `uValue` | Optional `uint64`. Commonly used for byte sizes or larger counters. |
| `xllm_opt_f64` | `bSet`, `fValue` | Optional floating point. Commonly used for temperature, top_p, and similar parameters. |

**Usage Example:**

```c
xllm_generation_params gen;
memset(&gen, 0, sizeof(gen));

gen.tTemperature.bSet = true;
gen.tTemperature.fValue = 0.2;

gen.tMaxOutputTokens.bSet = true;
gen.tMaxOutputTokens.iValue = 1024;
```

**Notes:**

- Prefer initializing the outer options object with the matching `*_init()` function, then set only the fields you want to override.
- If `bSet=false`, the value field should still be treated as "not set", even if it contains an old value.

---

## Log and Trace Types

### `xllm_log_level`

Log levels are used by `xllm_log_callback`.

| Value | Description | Suggested Use |
| --- | --- | --- |
| `XLLM_LOG_ERROR` | Error | Request failures, initialization failures, unrecoverable problems. |
| `XLLM_LOG_WARN` | Warning | Fallbacks, retries, recoverable exceptions. |
| `XLLM_LOG_INFO` | Info | Runtime creation, provider call summaries. |
| `XLLM_LOG_DEBUG` | Debug | Enable during local troubleshooting. |
| `XLLM_LOG_TRACE` | Fine-grained trace | Use only when diagnosing complex issues. |

### `xllm_log_event`

Structured log events help hosts classify logs instead of relying only on string search.

| Value | Description |
| --- | --- |
| `XLLM_LOG_EVENT_UNKNOWN` | Unclassified event. |
| `XLLM_LOG_EVENT_RUNTIME_CREATE` | Runtime creation. |
| `XLLM_LOG_EVENT_RUNTIME_DESTROY` | Runtime destruction. |
| `XLLM_LOG_EVENT_PROVIDER_REQUEST_START` | Provider request started. |
| `XLLM_LOG_EVENT_PROVIDER_RESPONSE_COMPLETE` | Provider request completed successfully. |
| `XLLM_LOG_EVENT_PROVIDER_RESPONSE_FAILED` | Provider request failed. |
| `XLLM_LOG_EVENT_PROVIDER_RETRY_SCHEDULED` | Retry scheduled. |
| `XLLM_LOG_EVENT_STREAM_EVENT` | Streaming event. |
| `XLLM_LOG_EVENT_SESSION_COMPACT_TRIGGERED` | Session compact triggered. |
| `XLLM_LOG_EVENT_SESSION_COMPACT_RESULT` | Session compact result. |
| `XLLM_LOG_EVENT_TOOL_LOOP_ROUND` | Tool loop round. |
| `XLLM_LOG_EVENT_TOOL_LOOP_EXECUTE` | Tool execution. |
| `XLLM_LOG_EVENT_TOOL_LOOP_STOP` | Tool loop stopped. |
| `XLLM_LOG_EVENT_MEMORY_INGEST` | Memory ingest. |
| `XLLM_LOG_EVENT_MEMORY_SEARCH` | Memory search. |
| `XLLM_LOG_EVENT_MEMORY_HEALTH_CHECK` | Memory health check. |
| `XLLM_LOG_EVENT_WORKSPACE_SYNC` | Workspace sync. |
| `XLLM_LOG_EVENT_WATCHER_EVENT` | File watcher event. |

### `xllm_trace_kind`

Trace is more suitable for machine consumption than logs. Trace payloads use `xvalue`, which the host can convert to JSON and save.

| Value | Description |
| --- | --- |
| `XLLM_TRACE_EVENT` | Generic event. |
| `XLLM_TRACE_REQUEST` | Request construction or pre-send information. |
| `XLLM_TRACE_RESPONSE` | Response parsing or completion information. |
| `XLLM_TRACE_STREAM` | Streaming event. |
| `XLLM_TRACE_COMPACT` | Session compact information. |
| `XLLM_TRACE_TOOL_LOOP` | Tool loop information. |

### `xllm_debug_mode` and `xllm_redact_mode`

| Type | Value | Description |
| --- | --- | --- |
| `xllm_debug_mode` | `XLLM_DEBUG_NONE` | Do not emit extra debug material. |
| `xllm_debug_mode` | `XLLM_DEBUG_HEADERS` | Debug header-level information. |
| `xllm_debug_mode` | `XLLM_DEBUG_BODY` | Debug request/response body information. |
| `xllm_debug_mode` | `XLLM_DEBUG_WIRE` | Debug wire-level information; highest volume. |
| `xllm_redact_mode` | `XLLM_REDACT_DEFAULT` | Default redaction policy. |
| `xllm_redact_mode` | `XLLM_REDACT_OFF` | No redaction. Use only for short local troubleshooting. |
| `xllm_redact_mode` | `XLLM_REDACT_STRICT` | Strict redaction, suitable for shared logs or CI. |

### Callback Types

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

**Ownership:**

- `pCtx` is passed by the host. xllm only stores the pointer and does not take ownership.
- `sComponent`, `sMessage`, and `pPayload` are valid during the callback. If the host needs to save them asynchronously, it must copy the content.

---

## Authentication, Proxy, and Transport Types

### `xllm_auth_kind` / `xllm_auth`

| Value | Description |
| --- | --- |
| `XLLM_AUTH_NONE` | Do not attach authentication. Suitable for local Ollama or gateways that inject authentication. |
| `XLLM_AUTH_BEARER` | Use `Authorization: Bearer <secret>`. |
| `XLLM_AUTH_API_KEY_HEADER` | Use a custom header to carry the API key. |

`xllm_auth` fields:

| Field | Description |
| --- | --- |
| `eKind` | Authentication kind. |
| `sSecret` | Secret string. Caller-owned; runtime/profile registration copies or references it as required by the implementation. |
| `sHeaderName` | Header name used by `XLLM_AUTH_API_KEY_HEADER`. |
| `sScheme` | Custom authentication scheme. Bearer usually does not need manual setup. |

### `xllm_proxy_kind`

| Value | Description |
| --- | --- |
| `XLLM_PROXY_UNSPECIFIED` | Not specified; use runtime or adapter default policy. |
| `XLLM_PROXY_NONE` | Disable proxy. |
| `XLLM_PROXY_SOCKS5` | Use SOCKS5 proxy. |
| `XLLM_PROXY_HTTP_CONNECT` | Use HTTP CONNECT proxy. |

### `xllm_transport_options`

| Field | Type | Description |
| --- | --- | --- |
| `tConnectTimeoutMs` | `xllm_opt_u32` | Connection timeout in milliseconds. |
| `tReadTimeoutMs` | `xllm_opt_u32` | Read timeout in milliseconds. |
| `tVerifyPeer` | `xllm_opt_bool` | Whether to verify TLS peer. Usually enabled in production. |
| `eProxyKind` | `xllm_proxy_kind` | Proxy kind. |
| `sProxyHost` | `const char *` | Proxy host. |
| `tProxyPort` | `xllm_opt_u32` | Proxy port. |
| `sProxyUser` / `sProxyPass` | `const char *` | Proxy credentials. |
| `sCaBundlePath` | `const char *` | CA bundle path. |
| `sClientCertPath` / `sClientKeyPath` | `const char *` | Client certificate and private key paths. |
| `tVendorExtra` | `xvalue` | Provider or host extension fields. |

---

## Model Capability Types

### `xllm_capability_flags`

Capability bits describe what a model/adapter claims to support. Hosts can use them to decide whether to allow images, files, JSON output, or tool calls.

| Constant | Description |
| --- | --- |
| `XLLM_CAP_TEXT_IN` | Supports text input. |
| `XLLM_CAP_IMAGE_IN` | Supports image input. |
| `XLLM_CAP_FILE_IN` | Supports file input. |
| `XLLM_CAP_AUDIO_IN` | Supports audio input. |
| `XLLM_CAP_VIDEO_IN` | Supports video input. |
| `XLLM_CAP_TOOL_RESULT_IN` | Supports tool results as input. |
| `XLLM_CAP_TEXT_OUT` | Supports text output. |
| `XLLM_CAP_IMAGE_OUT` | Supports image output. |
| `XLLM_CAP_FILE_OUT` | Supports file output. |
| `XLLM_CAP_AUDIO_OUT` | Supports audio output. |
| `XLLM_CAP_VIDEO_OUT` | Supports video output. |
| `XLLM_CAP_JSON_OUT` | Supports JSON or structured output. |
| `XLLM_CAP_TOOL_CALL_OUT` | Supports model-initiated tool calls. |
| `XLLM_CAP_THINKING_SUMMARY_OUT` | Supports thinking summary output. |
| `XLLM_CAP_THINKING_FULL_OUT` | Supports full thinking output. |
| `XLLM_CAP_STREAM` | Supports streaming output. |
| `XLLM_CAP_REASONING_CONTROL` | Supports reasoning parameter control. |
| `XLLM_CAP_PARALLEL_TOOL_CALL` | Supports parallel tool calls. |
| `XLLM_CAP_CITATION_OUT` | Supports citation/source output. |

### Parameter Rules

`xllm_float_rule` and `xllm_u32_rule` describe provider limits for parameters such as temperature, top_p, and max output tokens.

| `xllm_param_rule_kind` | Description |
| --- | --- |
| `XLLM_PARAM_RULE_UNSPECIFIED` | No rule declared. |
| `XLLM_PARAM_RULE_UNSUPPORTED` | Parameter is not supported. |
| `XLLM_PARAM_RULE_FIXED` | Fixed value; caller-provided values may be rejected or normalized. |
| `XLLM_PARAM_RULE_RANGE` | Allowed range, using `fMin/fMax` or `uMin/uMax`. |
| `XLLM_PARAM_RULE_PASSTHROUGH` | Pass through to provider. |

### `xllm_model_caps`

| Field | Description |
| --- | --- |
| `uFlags` | Capability bits. |
| `psSupportedMimeTypes` / `iSupportedMimeTypeCount` | Supported MIME type list. |
| `eWindowMode` | Context window mode. |
| `uMaxContextTokens` | Maximum context tokens. |
| `uMaxInputTokens` / `uMaxOutputTokens` | Input/output token limits. |
| `uRecommendedOutputReserve` | Recommended token reserve for output. |
| `uMaxPartsPerMessage` | Maximum number of parts in one message. |
| `uMaxImages` / `uMaxFiles` | Image/file count limits. |
| `uMaxPartBytes` | Byte limit for one part. |
| `sTokenizerId` | Tokenizer identifier. |
| `tTemperatureRule` / `tTopPRule` / `tMaxOutputTokensRule` | Parameter rules. |
| `tVendorExtra` | Provider extension fields. |

---

## Profile Types

`xllm_profile` is the core configuration that lets xllm choose provider, adapter, model, authentication, and default parameters.

### `xllm_profile`

| Field | Description |
| --- | --- |
| `sId` | Profile ID. Requests reference it through `sProfileId`. |
| `sName` | User-facing name. |
| `sProvider` | Provider name, such as `openai`, `anthropic`, or `ollama`. |
| `sAdapter` | Adapter name, usually one of the `XLLM_ADAPTER_*` constants. |
| `sBaseUrl` | Provider endpoint. |
| `tAuth` | Authentication information. |
| `pDefaultHeaders` / `iDefaultHeaderCount` | Default HTTP headers. |
| `tProviderOptions` | Provider-specific options. |
| `tTransport` | Transport options. |
| `tModels` | Text/multimodal model bindings. |
| `tDefaults` | Default generation, reasoning, and response format parameters. |
| `tVendorExtra` | Extension fields. |

### `xllm_model_binding`

| Field | Description |
| --- | --- |
| `sModelId` | Actual model ID sent to the provider. |
| `sAliasOf` | Alias source, used for UI or configuration inheritance. |
| `eCapMode` | Capability merge mode. |
| `tCaps` | Model capabilities. |
| `tVendorExtra` | Extension fields. |

### `xllm_cap_mode`

| Value | Description |
| --- | --- |
| `XLLM_CAP_MODE_AUTO` | Use adapter default capability inference. |
| `XLLM_CAP_MODE_MERGE` | Merge adapter default capabilities with explicit profile capabilities. |
| `XLLM_CAP_MODE_EXACT` | Use only capabilities declared in the profile. |

---

## Request Message Types

### `xllm_request` and `xllm_turn`

`xllm_request` is used for direct core calls. `xllm_turn` is used for one input turn at the session layer. Their fields are similar, but `xllm_turn` supports `sSystemPrompt` and `eSystemMode`, making it more suitable for conversation scenarios.

| Field | Appears In | Description |
| --- | --- | --- |
| `sProfileId` | `xllm_request` | Profile used by this request. |
| `eSlot` | both | Automatic, text, or multimodal model slot. |
| `sSystemPrompt` / `eSystemMode` | `xllm_turn` | How this turn's system prompt applies to the session. |
| `pMessages` / `iMessageCount` | both | Message list. |
| `pContextBlocks` / `iContextBlockCount` | both | Additional context blocks. |
| `pTools` / `iToolCount` | both | Available tool definitions. |
| `tToolPolicy` | both | Tool choice policy. |
| `tGeneration` | both | Generation parameters. |
| `tResponseFormat` | both | Response format requirement. |
| `tReasoning` | both | Reasoning options. |
| `tVendorExtra` | both | Extension fields. |

### Messages and Content

| Type | Description |
| --- | --- |
| `xllm_role` | Message role: system, user, assistant, tool. |
| `xllm_part_kind` | Content type: text, image, file, audio, video, json. |
| `xllm_source_kind` | Content source: inline text, inline bytes, URL, provider file id. |
| `xllm_data_source` | Actual source for one content part. |
| `xllm_content_part` | One content part in a message. |
| `xllm_message` | One message, which may contain multiple parts and tool calls. |
| `xllm_context_block` | Context block with priority and pinned flag. |

**Notes:**

- Strings, arrays, and `xvalue` must remain valid at least for the duration of the API call.
- If you use session or memory bridge APIs, those APIs may copy required content; see the corresponding API page for details.

---

## Tool Call Types

| Type | Description |
| --- | --- |
| `xllm_tool_def` | Tool definition exposed by the host to the model. |
| `xllm_tool_policy` | Whether this request allows, requires, or names a tool. |
| `xllm_tool_choice_mode` | `auto`, `none`, `required`, `named`. |
| `xllm_tool_call` | Tool call carried in a request message. |
| `xllm_output_tool_call` | Tool call generated by the model in a response. |
| `xllm_tool_exec_request` | Arguments passed to the executor when xllm asks the host to execute a tool. |
| `xllm_tool_exec_result` | Result returned by the host to xllm after tool execution. |
| `xllm_tool_executor` | Synchronous tool executor. |
| `xllm_tool_executor_async` | Asynchronous tool executor. |

---

## Response and Event Types

### `xllm_response`

| Field | Description |
| --- | --- |
| `sId` | Provider response ID. |
| `sProvider` / `sProfileId` / `sModel` | Actual call information. |
| `eStatus` | Response status. |
| `sFinishReason` | Provider finish reason. |
| `pOutputs` / `iOutputCount` | Structured output items. |
| `sVisibleText` | Merged text suitable for direct display. |
| `tUsage` | Token usage. |
| `tRefusal` / `tSafety` | Refusal and safety information. |
| `tEffectiveParams` | Effective parameters. |
| `bHasError` / `tError` | Error information inside the response. |
| `tRaw` / `tVendorExtra` | Raw or extension data. |

Responses are allocated by APIs such as `xllm_chat` / `xllm_chat_ex`. Callers must free them with `xllm_response_free()`.

### Output and Status Enums

| Type / Value | Description |
| --- | --- |
| `XLLM_OUTPUT_MESSAGE` | Normal message output. |
| `XLLM_OUTPUT_THINKING` | Thinking output. |
| `XLLM_OUTPUT_TOOL_CALL` | Tool call output. |
| `XLLM_OUTPUT_REFUSAL` | Refusal output. |
| `XLLM_STATUS_COMPLETED` | Completed normally. |
| `XLLM_STATUS_INCOMPLETE` | Not fully completed. |
| `XLLM_STATUS_TOOL_CALL_REQUIRED` | Host must execute a tool. |
| `XLLM_STATUS_REFUSED` | Model refused. |
| `XLLM_STATUS_CONTENT_FILTERED` | Content was filtered. |
| `XLLM_STATUS_CANCELLED` | Cancelled. |
| `XLLM_STATUS_ERRORED` | Errored. |

### Streaming Events

`xllm_event` is used by streaming callbacks. Event types include start, output start, text delta, thinking delta, tool call delta, artifact, usage, error, and end.

Event callback:

```c
typedef bool (*xllm_event_callback)(const xllm_event *pEvent, void *pUserData);
```

Returning `false` usually means the host does not want to continue processing later events. The exact behavior depends on the calling API.

---

## Error Types

### `xllm_error_code`

| Value | Description |
| --- | --- |
| `XLLM_ERROR_NONE` | No error. |
| `XLLM_ERROR_AUTH` | Authentication failed. |
| `XLLM_ERROR_QUOTA` | Quota exhausted. |
| `XLLM_ERROR_RATE_LIMIT` | Rate limit hit. |
| `XLLM_ERROR_TIMEOUT` | Timeout. |
| `XLLM_ERROR_NETWORK` | Network error. |
| `XLLM_ERROR_CANCELLED` | Request cancelled. |
| `XLLM_ERROR_INVALID_REQUEST` | Invalid request. |
| `XLLM_ERROR_UNSUPPORTED_CAPABILITY` | Capability unsupported. |
| `XLLM_ERROR_UNSUPPORTED_INPUT_TYPE` | Input type unsupported. |
| `XLLM_ERROR_UNSUPPORTED_MIME_TYPE` | MIME type unsupported. |
| `XLLM_ERROR_INPUT_TOO_LARGE` | Input too large. |
| `XLLM_ERROR_TOO_MANY_INPUT_PARTS` | Too many input parts. |
| `XLLM_ERROR_MISSING_MULTIMODAL_MODEL` | Multimodal model required but not configured. |
| `XLLM_ERROR_MODEL_NOT_FOUND` | Model or profile not found. |
| `XLLM_ERROR_UPSTREAM_4XX` | Upstream 4xx. |
| `XLLM_ERROR_UPSTREAM_5XX` | Upstream 5xx. |
| `XLLM_ERROR_PARSE` | Parse failure. |
| `XLLM_ERROR_INTERNAL` | Internal error. |
| `XLLM_ERROR_SESSION_CONTEXT_OVERFLOW` | Session context overflow. |
| `XLLM_ERROR_SESSION_COMPACT_FAILED` | Session compact failed. |
| `XLLM_ERROR_SESSION_SUMMARY_FAILED` | Session summary failed. |
| `XLLM_ERROR_SESSION_REQUIRES_MODEL_LIMITS` | Session needs model limits to estimate context. |

### `xllm_error`

`xllm_error` is an error object held by the caller. Call `xllm_error_init()` before use, `xllm_error_reset()` before reuse, and `xllm_error_free()` when done.

| Field | Description |
| --- | --- |
| `eCode` | Normalized xllm error code. |
| `iStatus` | xllm internal status or extension status. |
| `iHttpStatus` | HTTP status code. |
| `sMessage` | Developer-facing error message. |
| `sProviderCode` / `sProviderMessage` | Error code and message returned by provider. |
| `sRequestId` | Provider request ID. |
| `iMessageIndex` / `iPartIndex` | Message/part index where the error occurred. |
| `uRequiredCapability` | Missing capability bit. |
| `sSelectedModel` | Selected or attempted model. |
| `sMimeType` | Related MIME type. |
| `tVendorExtra` | Extended error information. |

---

## Call Option Types

### `xllm_call_options`

| Field | Description |
| --- | --- |
| `eStreamMode` | Streaming policy: auto, off, prefer, require. |
| `uTimeoutMs` | Timeout for this call in milliseconds. |
| `pCancelToken` | Cancel token. Caller-owned. |
| `pfnOnEvent` / `pUserData` | Streaming event callback and user context. |
| `eArtifactPolicy` | Artifact handling policy. |
| `pArtifactSink` | Artifact sink. Caller-owned. |
| `uMaxRetries` | Maximum retries. |
| `uRetryBackoffBaseMs` / `uRetryBackoffMaxMs` | Retry backoff times. |
| `fRetryJitter` | Retry jitter ratio. |
| `bBestEffortStructuredOutput` | Whether best-effort structured output is allowed. |
| `eLocalFilePolicy` | Local file input policy. |
| `tVendorExtra` | Extension fields. |

Related enums:

| Type | Value | Description |
| --- | --- | --- |
| `xllm_stream_mode` | `XLLM_STREAM_AUTO` | Choose streaming automatically. |
| `xllm_stream_mode` | `XLLM_STREAM_OFF` | Disable streaming. |
| `xllm_stream_mode` | `XLLM_STREAM_PREFER` | Prefer streaming, but allow fallback. |
| `xllm_stream_mode` | `XLLM_STREAM_REQUIRE` | Require streaming; fail if unsupported. |
| `xllm_artifact_policy` | `XLLM_ARTIFACT_REFERENCE_ONLY` | Keep references only. |
| `xllm_artifact_policy` | `XLLM_ARTIFACT_INLINE_SMALL` | Inline small artifacts. |
| `xllm_artifact_policy` | `XLLM_ARTIFACT_STREAM_TO_SINK` | Stream to sink. |
| `xllm_local_file_policy` | `XLLM_LOCAL_FILE_AUTO` | Choose file handling automatically. |
| `xllm_local_file_policy` | `XLLM_LOCAL_FILE_INLINE_FIRST` | Prefer inline. |
| `xllm_local_file_policy` | `XLLM_LOCAL_FILE_UPLOAD_REUSE_FIRST` | Prefer upload and provider file ID reuse. |

---

## Session-Related Types

| Type | Description |
| --- | --- |
| `xllm_create_options` | Used when creating a convenience `xllm` object. |
| `xllm_session_options` | Used when creating a session. |
| `xllm_compact_options` | Used for manual compacting. |
| `xllm_compact_result` | Compact result. |
| `xllm_token_count_result` | Token count result. |

For detailed session APIs, see [api-session.en.md](api-session.en.md).

---

## Runtime Option Types

### `xllm_runtime_options`

| Field | Description |
| --- | --- |
| `tAllocator` | Custom allocator. If unset, the default allocation strategy is used. |
| `pfnLog` / `pLogCtx` | Initial log callback. |
| `pfnTrace` / `pTraceCtx` | Initial trace callback. |
| `eDebugMode` | Debug level. |
| `eRedactMode` | Redaction policy. |
| `tTransportDefaults` | Runtime-level default transport configuration. |
| `tVendorExtra` | Extension fields. |

For runtime creation, destruction, and callback setup, see [api-core.en.md](api-core.en.md).

---

## Resource Ownership Rules

- `*_init()`: initializes a caller-provided structure, does not allocate long-lived resources, and is usually safe for stack objects.
- `*_reset()`: cleans resources held inside a structure so the object can be reused or safely discarded.
- `*_free()`: frees resources allocated or filled by xllm; some functions also free the outer object.
- `*_destroy()`: destroys opaque handles, such as `xllm_runtime`, `xllm_cancel_token`, and `xllm_session`.
- `const char *` input: by default, the caller guarantees it remains valid during the API call; whether it is copied depends on the specific API.
- Returned `const char *`: unless the API explicitly says otherwise, treat it as a borrowed pointer whose lifetime follows the owning object.
- `xvalue`: usually used for extension fields or raw JSON data. If it must be stored across objects long term, copy or retain it according to xrt/xvalue rules.

---

## Related Examples

- `examples\smoke_chat_ex_low_level.c`
- `examples\smoke_log_event_taxonomy.c`
- `build.bat smoke -Filter "chat_ex_low_level,log_event_taxonomy"`
