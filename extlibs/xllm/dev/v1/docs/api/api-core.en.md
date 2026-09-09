# xllm Core Runtime API

> Core Runtime creates the xllm runtime, registers provider adapters and profiles, and sends one stateless model call.

[Back to API Index](README.en.md) | [Basic Types](types.en.md) | [Request / Response API](api-request-response.en.md) | [Provider API](api-providers.en.md)

---

## Table of Contents

- [Module Role](#module-role)
- [Standard Call Order](#standard-call-order)
- [Runtime Lifecycle](#runtime-lifecycle)
  - [xllm_version](#xllm_version)
  - [xllm_runtime_options_init](#xllm_runtime_options_init)
  - [xllm_runtime_create](#xllm_runtime_create)
  - [xllm_runtime_destroy](#xllm_runtime_destroy)
- [Runtime Diagnostics Configuration](#runtime-diagnostics-configuration)
  - [xllm_runtime_set_log_callback](#xllm_runtime_set_log_callback)
  - [xllm_runtime_set_trace_callback](#xllm_runtime_set_trace_callback)
  - [xllm_runtime_set_debug_mode](#xllm_runtime_set_debug_mode)
- [Adapter and Profile Registration](#adapter-and-profile-registration)
  - [xllm_register_adapter](#xllm_register_adapter)
  - [Built-in Adapter Registration Functions](#built-in-adapter-registration-functions)
  - [xllm_register_profile](#xllm_register_profile)
- [Request Validation and Calls](#request-validation-and-calls)
  - [xllm_validate_request](#xllm_validate_request)
  - [xllm_count_tokens](#xllm_count_tokens)
  - [xllm_chat](#xllm_chat)
  - [xllm_chat_ex](#xllm_chat_ex)
- [Async Calls](#async-calls)
  - [xllm_chat_async_thread](#xllm_chat_async_thread)
  - [xllm_chat_async_engine](#xllm_chat_async_engine)
  - [xllm_chat_async_co](#xllm_chat_async_co)
- [Cancellation Control](#cancellation-control)
  - [xllm_cancel_token_create](#xllm_cancel_token_create)
  - [xllm_cancel_token_destroy](#xllm_cancel_token_destroy)
  - [xllm_cancel_token_cancel](#xllm_cancel_token_cancel)
  - [xllm_cancel_token_is_cancelled](#xllm_cancel_token_is_cancelled)
- [Complete Example](#complete-example)
- [Common Mistakes](#common-mistakes)

---

## Module Role

Use Core Runtime when:

- You only want to send one direct model call and do not need session history.
- You need to register provider adapters and profiles when the host program starts.
- You want to configure logs, traces, debug mode, and transport defaults in one place.
- You want to validate whether a request matches profile/model capabilities before sending it.

Core Runtime does not manage long-term memory or session history. For short-term multi-turn conversations, see [api-session.en.md](api-session.en.md). For RAG or local memory, see [api-memory.en.md](api-memory.en.md).

---

## Standard Call Order

The minimal order is:

1. Initialize runtime options with `xllm_runtime_options_init`, or pass `NULL` to use defaults.
2. Create a runtime with `xllm_runtime_create`.
3. Register one or more adapters with `xllm_register_*_adapter`.
4. Initialize a profile with `xllm_profile_init`, then fill `sId`, `sAdapter`, `sBaseUrl`, authentication, and model.
5. Register the profile with `xllm_register_profile`.
6. Build `xllm_request` and `xllm_call_options`.
7. Call `xllm_chat_ex`.
8. Free the response with `xllm_response_free`, and release request internals with `xllm_request_reset`.
9. Call `xllm_runtime_destroy` before program exit.

---

## Runtime Lifecycle

### xllm_version

Gets the xllm runtime version string.

**Purpose:**

Record this version in logs, diagnostic reports, release gates, or host UI so later troubleshooting can identify the running package.

**Prototype:**

```c
XLLM_API const char *xllm_version(void);
```

**Parameters:**

None.

**Return Value:**

- Returns a static string. The current implementation returns `"0.1.0"`.
- Callers must not free the returned value.

**Resource Ownership:**

The return value is a borrowed pointer whose lifetime is the process lifetime.

**Notes:**

- If you need numeric version components, use `XLLM_VERSION_MAJOR`, `XLLM_VERSION_MINOR`, and `XLLM_VERSION_PATCH`.
- The version string is for humans. Do not rely on string comparison for compatibility decisions.

**Example Code:**

```c
#include "xllm.h"
#include <stdio.h>

int main(void) {
    printf("xllm version: %s\n", xllm_version());
    return 0;
}
```

**Related APIs:**

- `XLLM_VERSION_MAJOR`
- `XLLM_VERSION_MINOR`
- `XLLM_VERSION_PATCH`

---

### xllm_runtime_options_init

Initializes `xllm_runtime_options`.

**Purpose:**

Use it when you want to customize logs, traces, debug mode, allocator, or default transport settings. For a minimal program, you can skip options and pass `NULL` directly to `xllm_runtime_create`.

**Prototype:**

```c
XLLM_API void xllm_runtime_options_init(xllm_runtime_options *pOptions);
```

**Parameters:**

| Parameter | Direction | Can Be `NULL` | Description |
| --- | --- | --- | --- |
| `pOptions` | output | yes | Options to initialize. If `NULL`, the function returns immediately. |

**Return Value:**

None.

**Resource Ownership:**

This function does not allocate resources. `pOptions` is caller-owned and can be a stack object.

**Notes:**

- After initialization, `eDebugMode = XLLM_DEBUG_NONE`.
- After initialization, `eRedactMode = XLLM_REDACT_DEFAULT`.
- If you manually fill `tAllocator`, provide malloc/realloc/free together to avoid mixing allocators.

**Example Code:**

```c
xllm_runtime_options opt;
xllm_runtime_options_init(&opt);
opt.eDebugMode = XLLM_DEBUG_HEADERS;
opt.eRedactMode = XLLM_REDACT_STRICT;
```

**Related APIs:**

- `xllm_runtime_create`
- `xllm_runtime_set_debug_mode`

---

### xllm_runtime_create

Creates an xllm runtime.

**Purpose:**

The runtime is the top-level xllm object. Adapters, profiles, default diagnostics configuration, and internal network execution state are attached to it. You need a runtime before calling `xllm_chat_ex` or higher-level session/memory features.

**Prototype:**

```c
XLLM_API int xllm_runtime_create(const xllm_runtime_options *pOptions, xllm_runtime **ppRuntime);
```

**Parameters:**

| Parameter | Direction | Can Be `NULL` | Description |
| --- | --- | --- | --- |
| `pOptions` | input | yes | Runtime options. If `NULL`, default allocator, debug/redact, and transport settings are used. |
| `ppRuntime` | output | no | Receives the new runtime on success; receives `NULL` on failure. |

**Return Value:**

- `XRT_NET_OK`: created successfully.
- `XRT_NET_ERROR`: invalid parameter, allocation failure, or options clone failure.

**Resource Ownership:**

- On success, `*ppRuntime` is owned by the caller.
- The caller must release it with `xllm_runtime_destroy(*ppRuntime)`.
- When the runtime is destroyed, it releases internal copies of adapters and profiles.

**Notes:**

- `pOptions` is cloned into the runtime. You can release or reuse the original options after creation.
- The runtime tries to create and start a default network engine. If that engine fails to start, runtime creation may still succeed, but some async paths may be limited.
- A process can create multiple runtimes, but one shared runtime per host process is usually simpler.

**Example Code:**

```c
xllm_runtime *runtime = NULL;
int rc = xllm_runtime_create(NULL, &runtime);
if (rc != XRT_NET_OK || !runtime) {
    return 1;
}

xllm_runtime_destroy(runtime);
```

**Related APIs:**

- `xllm_runtime_options_init`
- `xllm_runtime_destroy`

---

### xllm_runtime_destroy

Destroys a runtime.

**Purpose:**

Releases the runtime, internal copies of registered adapters/profiles, the internal network engine, and internal resources in runtime options.

**Prototype:**

```c
XLLM_API void xllm_runtime_destroy(xllm_runtime *pRuntime);
```

**Parameters:**

| Parameter | Direction | Can Be `NULL` | Description |
| --- | --- | --- | --- |
| `pRuntime` | input | yes | Runtime to destroy. If `NULL`, the function returns immediately. |

**Return Value:**

None.

**Resource Ownership:**

- `pRuntime` must come from `xllm_runtime_create`.
- Do not use the pointer after destroying it.
- The runtime does not destroy `pCtx` values passed to callbacks, external `xvalue` values still owned by the host, or tool resources.

**Notes:**

- Finish or cancel async tasks using the runtime before destroying it.
- If the caller still owns a response, free it first with `xllm_response_free`.

**Example Code:**

```c
if (runtime) {
    xllm_runtime_destroy(runtime);
    runtime = NULL;
}
```

**Related APIs:**

- `xllm_runtime_create`

---

## Runtime Diagnostics Configuration

### xllm_runtime_set_log_callback

Sets the runtime log callback.

**Purpose:**

Use it when you want xllm logs to go into the host logging system. Logs are human-readable. If you need machine-readable structured events, also use the trace callback.

**Prototype:**

```c
XLLM_API int xllm_runtime_set_log_callback(
    xllm_runtime *pRuntime,
    xllm_log_callback pfnLog,
    void *pLogCtx
);
```

**Parameters:**

| Parameter | Direction | Can Be `NULL` | Description |
| --- | --- | --- | --- |
| `pRuntime` | input | no | Target runtime. |
| `pfnLog` | input | yes | Log callback. Passing `NULL` clears the log callback. |
| `pLogCtx` | input | yes | Callback context. xllm only stores the pointer and does not take ownership. |

**Return Value:**

- `XRT_NET_OK`: set successfully.
- `XRT_NET_ERROR`: `pRuntime` is `NULL`.

**Resource Ownership:**

`pLogCtx` is caller-owned and must remain valid while callbacks may occur.

**Notes:**

- Strings received by the callback are only reliable during the callback. Copy them if you need to store them asynchronously.
- Keep the log callback lightweight so it does not slow provider call paths.

**Example Code:**

```c
static void on_log(void *ctx, xllm_log_level level, const char *component, const char *message)
{
    (void)ctx;
    printf("[%s] %s: %s\n", xllm_log_level_name(level), component, message);
}

xllm_runtime_set_log_callback(runtime, on_log, NULL);
```

**Related APIs:**

- `xllm_log_level_name`
- `xllm_runtime_set_trace_callback`

---

### xllm_runtime_set_trace_callback

Sets the runtime trace callback.

**Purpose:**

Trace callbacks are useful for saving provider requests, responses, streaming events, compacting, and tool loop information as structured diagnostic data.

**Prototype:**

```c
XLLM_API int xllm_runtime_set_trace_callback(
    xllm_runtime *pRuntime,
    xllm_trace_callback pfnTrace,
    void *pTraceCtx
);
```

**Parameters:**

| Parameter | Direction | Can Be `NULL` | Description |
| --- | --- | --- | --- |
| `pRuntime` | input | no | Target runtime. |
| `pfnTrace` | input | yes | Trace callback. Passing `NULL` clears the trace callback. |
| `pTraceCtx` | input | yes | Callback context. |

**Return Value:**

- `XRT_NET_OK`: set successfully.
- `XRT_NET_ERROR`: `pRuntime` is `NULL`.

**Resource Ownership:**

`pTraceCtx` is caller-owned. `pPayload` is a borrowed pointer valid during the callback.

**Notes:**

- Trace payloads are `xvalue`; convert or copy them if the host needs to save them.
- Higher debug modes may include more request/response details in trace. Use redact mode together with it.

**Example Code:**

```c
static void on_trace(void *ctx, xllm_trace_kind kind, const xvalue *payload)
{
    (void)ctx;
    (void)payload;
    printf("trace kind: %s\n", xllm_trace_kind_name(kind));
}

xllm_runtime_set_trace_callback(runtime, on_trace, NULL);
```

**Related APIs:**

- `xllm_trace_kind_name`
- `xllm_runtime_set_debug_mode`

---

### xllm_runtime_set_debug_mode

Sets the runtime debug level and redaction policy.

**Purpose:**

Use it when troubleshooting provider requests, headers, bodies, or wire-level issues. In production, prefer `XLLM_DEBUG_NONE` or strict redaction.

**Prototype:**

```c
XLLM_API int xllm_runtime_set_debug_mode(
    xllm_runtime *pRuntime,
    xllm_debug_mode eMode,
    xllm_redact_mode eRedactMode
);
```

**Parameters:**

| Parameter | Direction | Can Be `NULL` | Description |
| --- | --- | --- | --- |
| `pRuntime` | input | no | Target runtime. |
| `eMode` | input | no | Debug level. |
| `eRedactMode` | input | no | Redaction policy. |

**Return Value:**

- `XRT_NET_OK`: set successfully.
- `XRT_NET_ERROR`: `pRuntime` is `NULL`.

**Resource Ownership:**

Does not allocate resources.

**Notes:**

- `XLLM_REDACT_OFF` may expose API keys, prompts, or user data. Use it only briefly on a local machine.
- Prefer `XLLM_REDACT_STRICT` in CI or shareable logs.

**Example Code:**

```c
xllm_runtime_set_debug_mode(runtime, XLLM_DEBUG_HEADERS, XLLM_REDACT_STRICT);
```

**Related APIs:**

- `xllm_runtime_options_init`

---

## Adapter and Profile Registration

### xllm_register_adapter

Registers a custom adapter.

**Purpose:**

When built-in adapters do not match your provider protocol, the host can register its own `xllm_adapter`. Most users should prefer built-in adapter registration functions.

**Prototype:**

```c
XLLM_API int xllm_register_adapter(xllm_runtime *pRuntime, const xllm_adapter *pAdapter);
```

**Parameters:**

| Parameter | Direction | Can Be `NULL` | Description |
| --- | --- | --- | --- |
| `pRuntime` | input/output | no | Target runtime. |
| `pAdapter` | input | no | Adapter definition. It must have `sName`. |

**Return Value:**

- `XRT_NET_OK`: registered successfully.
- `XRT_NET_ERROR`: invalid parameter, duplicate name, clone failure, or append failure.

**Resource Ownership:**

- The runtime clones the adapter definition.
- The original `pAdapter` object remains caller-owned.
- Whether the context pointed to by `pAdapter->pCtx` can be released depends on your adapter callback design; xllm does not destroy it automatically.

**Notes:**

- `sName` must be unique.
- `pfnChat` is the actual chat call entry point.
- `pfnCountTokens` is optional. If missing, token counting may be unavailable or estimated only.

**Example Code:**

```c
xllm_adapter adapter;
memset(&adapter, 0, sizeof(adapter));
adapter.sName = "my_adapter";
adapter.pCtx = my_ctx;
adapter.pfnChat = my_chat_fn;
adapter.pfnCountTokens = my_count_tokens_fn;

if (xllm_register_adapter(runtime, &adapter) != XRT_NET_OK) {
    /* Handle registration failure. */
}
```

**Related APIs:**

- `xllm_register_profile`
- `xllm_register_openai_compat_adapter`

---

### Built-in Adapter Registration Functions

Registers xllm's built-in provider adapters.

**Purpose:**

Built-in adapters unify provider protocol differences into xllm request/response semantics. Before using a profile, register the adapter required by that profile.

**Prototype:**

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

**Parameters:**

| Parameter | Direction | Can Be `NULL` | Description |
| --- | --- | --- | --- |
| `pRuntime` | input/output | no | Target runtime. |

**Return Value:**

- `XRT_NET_OK`: registered successfully.
- `XRT_NET_ERROR`: invalid runtime, duplicate adapter, or internal registration failure.

**Resource Ownership:**

The adapter is registered inside the runtime and released with `xllm_runtime_destroy`.

**Notes:**

- Registering the same adapter twice fails.
- You can register only the adapters you actually use.
- The profile's `sAdapter` should use the matching `XLLM_ADAPTER_*` constant.

**Example Code:**

```c
if (xllm_register_openai_compat_adapter(runtime) != XRT_NET_OK) {
    return 2;
}
```

**Related APIs:**

- `xllm_register_adapter`
- `xllm_register_profile`

---

### xllm_register_profile

Registers a model call profile.

**Purpose:**

A profile binds adapter, base URL, authentication, model, capabilities, and default parameters into a named configuration. Requests choose a profile through `xllm_request.sProfileId`.

**Prototype:**

```c
XLLM_API int xllm_register_profile(xllm_runtime *pRuntime, const xllm_profile *pProfile);
```

**Parameters:**

| Parameter | Direction | Can Be `NULL` | Description |
| --- | --- | --- | --- |
| `pRuntime` | input/output | no | Target runtime. |
| `pProfile` | input | no | Profile definition. `sId` and `sAdapter` are required. |

**Return Value:**

- `XRT_NET_OK`: registered successfully.
- `XRT_NET_ERROR`: invalid parameter, duplicate profile ID, clone failure, or append failure.

**Resource Ownership:**

- The runtime clones the profile.
- The original `pProfile` remains caller-owned and can be released or reused after registration.
- Profile strings, headers, model caps, and similar fields are copied into the runtime according to internal rules.

**Notes:**

- Register the matching adapter before registering the profile.
- `sId` must be unique.
- Configure at least `tModels.tText.sModelId`. If you handle image/file multimodal input, also configure `tModels.tMultimodal` and capability bits.

**Example Code:**

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

**Related APIs:**

- `xllm_profile_init`
- `xllm_register_openai_compat_adapter`

---

## Request Validation and Calls

### xllm_validate_request

Validates whether a request satisfies runtime, profile, adapter, and model capability constraints.

**Purpose:**

Call it before contacting the provider to find problems early, such as missing profile, unsupported input type, unsupported MIME type, or insufficient model capability.

**Prototype:**

```c
XLLM_API int xllm_validate_request(
    xllm_runtime *pRuntime,
    const xllm_request *pRequest,
    const xllm_call_options *pOptions,
    xllm_error *pError
);
```

**Parameters:**

| Parameter | Direction | Can Be `NULL` | Description |
| --- | --- | --- | --- |
| `pRuntime` | input | no | Created runtime. |
| `pRequest` | input | no | Request to validate. |
| `pOptions` | input | yes | Call options. Used to validate stream, artifact, local file, and similar policies. |
| `pError` | output | yes | Receives detailed error on failure. |

**Return Value:**

- `XRT_NET_OK`: request can be submitted.
- Non-`XRT_NET_OK`: request is invalid or lacks capability.

**Resource Ownership:**

Does not take ownership of `pRequest` or `pOptions`. If `pError` is provided, the caller cleans it with `xllm_error_free`.

**Notes:**

- `xllm_chat_ex` calls this function internally.
- For user-fixable issues, show `pError->eCode` and `pError->sMessage` in host logs or UI.

**Example Code:**

```c
xllm_error err;
xllm_error_init(&err);
if (xllm_validate_request(runtime, &request, NULL, &err) != XRT_NET_OK) {
    fprintf(stderr, "invalid request: %s\n", err.sMessage ? err.sMessage : "(unknown)");
}
xllm_error_free(&err);
```

**Related APIs:**

- `xllm_chat_ex`
- `xllm_error`

---

### xllm_count_tokens

Counts request tokens.

**Purpose:**

Use it before sending a request to estimate context size, decide whether compacting is needed, or display token usage in UI.

**Prototype:**

```c
XLLM_API int xllm_count_tokens(
    xllm_runtime *pRuntime,
    const xllm_request *pRequest,
    xllm_token_count_result *pResult,
    xllm_error *pError
);
```

**Parameters:**

| Parameter | Direction | Can Be `NULL` | Description |
| --- | --- | --- | --- |
| `pRuntime` | input | no | Created runtime. |
| `pRequest` | input | no | Request to count. |
| `pResult` | output | no | Receives token count result. |
| `pError` | output | yes | Receives error on failure. |

**Return Value:**

- `XRT_NET_OK`: counted successfully.
- Non-`XRT_NET_OK`: profile/adapter unavailable, adapter does not support counting, or request invalid.

**Resource Ownership:**

`pResult` is provided by the caller. The function only writes fields and does not require extra cleanup.

**Notes:**

- Counting depends on adapter `pfnCountTokens`.
- `pResult->bEstimated` indicates whether the result is estimated.
- Treat token count as a budgeting reference, not a replacement for final provider token usage.

**Example Code:**

```c
xllm_token_count_result count;
memset(&count, 0, sizeof(count));

if (xllm_count_tokens(runtime, &request, &count, &err) == XRT_NET_OK) {
    printf("input tokens: %u\n", count.uInputTokens);
}
```

**Related APIs:**

- `xllm_validate_request`
- `xllm_session_compact`

---

### xllm_chat

Starts one chat call without receiving a detailed error object.

**Purpose:**

This is a simplified call entry point. Use it when you only care about success/failure and do not need `xllm_error` details. In real integrations, prefer `xllm_chat_ex`.

**Prototype:**

```c
XLLM_API int xllm_chat(
    xllm_runtime *pRuntime,
    const xllm_request *pRequest,
    const xllm_call_options *pOptions,
    xllm_response **ppResponse
);
```

**Parameters:**

| Parameter | Direction | Can Be `NULL` | Description |
| --- | --- | --- | --- |
| `pRuntime` | input | no | Created runtime. |
| `pRequest` | input | no | Request. |
| `pOptions` | input | yes | Call options. |
| `ppResponse` | output | no | Receives response on success. |

**Return Value:**

- `XRT_NET_OK`: call succeeded.
- Non-`XRT_NET_OK`: call failed, but error details are not returned to the caller.

**Resource Ownership:**

On success, `*ppResponse` is owned by the caller and must be freed with `xllm_response_free`.

**Notes:**

- The function still creates a temporary `xllm_error` internally, but frees it before returning.
- New code should prefer `xllm_chat_ex` for easier troubleshooting.

**Example Code:**

```c
xllm_response *response = NULL;
if (xllm_chat(runtime, &request, NULL, &response) == XRT_NET_OK) {
    puts(xllm_response_get_text(response));
}
xllm_response_free(response);
```

**Related APIs:**

- `xllm_chat_ex`
- `xllm_response_free`

---

### xllm_chat_ex

Starts one chat call and returns detailed error information.

**Purpose:**

This is the most important core-layer call entry point. It validates the request, checks the cancel token, finds the profile and adapter, then calls the adapter chat implementation.

**Prototype:**

```c
XLLM_API int xllm_chat_ex(
    xllm_runtime *pRuntime,
    const xllm_request *pRequest,
    const xllm_call_options *pOptions,
    xllm_response **ppResponse,
    xllm_error *pError
);
```

**Parameters:**

| Parameter | Direction | Can Be `NULL` | Description |
| --- | --- | --- | --- |
| `pRuntime` | input | no | Created runtime. |
| `pRequest` | input | no | Request. |
| `pOptions` | input | yes | Call options. |
| `ppResponse` | output | no | Receives response on success; receives `NULL` on failure. |
| `pError` | output | yes | Receives error on failure. |

**Return Value:**

- `XRT_NET_OK`: call succeeded.
- `XRT_NET_CANCELLED`: cancel token was already cancelled.
- `XRT_NET_ERROR` or another non-success value: validation failed, adapter unavailable, provider call failed, or internal error.

**Resource Ownership:**

- On success, `*ppResponse` is owned by the caller and freed with `xllm_response_free`.
- `pError` is caller-provided. Usually call `xllm_error_init` before use and `xllm_error_free` after use.
- `pRequest` and `pOptions` are not taken over.

**Notes:**

- The function resets `pError` at the beginning, so the same error object can be reused.
- If the adapter fails but does not set an error code, xllm fills in `XLLM_ERROR_INTERNAL`.
- If `pOptions->pCancelToken` is already cancelled before dispatch, it returns a cancellation error.

**Example Code:**

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

**Related APIs:**

- `xllm_validate_request`
- `xllm_response_get_text`
- `xllm_response_free`

---

## Async Calls

### xllm_chat_async_thread

Runs a chat call asynchronously in a thread.

**Purpose:**

Use it when you do not want to block the current thread and do not have your own `xnetengine` or coroutine scheduler.

**Prototype:**

```c
XLLM_API xfuture *xllm_chat_async_thread(
    xllm_runtime *pRuntime,
    const xllm_request *pRequest,
    const xllm_call_options *pOptions
);
```

**Parameters:**

| Parameter | Direction | Can Be `NULL` | Description |
| --- | --- | --- | --- |
| `pRuntime` | input | no | Runtime. |
| `pRequest` | input | no | Request. The function creates an internal copy for the async task. |
| `pOptions` | input | yes | Call options. The function creates an internal copy for the async task. |

**Return Value:**

- Returns `xfuture *`.
- If task creation fails, it still returns a future containing an error result instead of returning `NULL`.

**Resource Ownership:**

Waiting, reading, and freeing the future follow xrt `xfuture` rules. Original request and options remain caller-owned.

**Notes:**

- The runtime must remain valid until the future completes.
- Async results usually contain `xllm_response *` or an error status; read them according to xrt future conventions.

**Example Code:**

```c
xfuture *future = xllm_chat_async_thread(runtime, &request, NULL);
/* Wait and read the result according to xrt future rules. */
```

**Related APIs:**

- `xllm_chat_ex`
- `xllm_chat_async_engine`

---

### xllm_chat_async_engine

Runs a chat call asynchronously on a specified `xnetengine`.

**Purpose:**

Use it when the host already has a unified network/task engine and wants xllm calls to join the same scheduling system.

**Prototype:**

```c
XLLM_API xfuture *xllm_chat_async_engine(
    xllm_runtime *pRuntime,
    xnetengine *pEngine,
    uint32 uAffinityKey,
    const xllm_request *pRequest,
    const xllm_call_options *pOptions
);
```

**Parameters:**

| Parameter | Direction | Can Be `NULL` | Description |
| --- | --- | --- | --- |
| `pRuntime` | input | no | Runtime. |
| `pEngine` | input | no | xrt network engine. |
| `uAffinityKey` | input | no | Engine scheduling affinity key. |
| `pRequest` | input | no | Request. |
| `pOptions` | input | yes | Call options. |

**Return Value:**

- Returns `xfuture *`.
- If `pEngine` is `NULL`, returns a future containing an error result.

**Resource Ownership:**

Free the future according to xrt rules. `pEngine` remains host-owned and must remain valid until the task completes.

**Notes:**

- Suitable for hosts that already have a unified background task queue.
- The meaning of `uAffinityKey` is determined by the xrt engine.

**Example Code:**

```c
xfuture *future = xllm_chat_async_engine(runtime, engine, 0, &request, &options);
```

**Related APIs:**

- `xllm_chat_async_thread`
- `xllm_chat_async_co`

---

### xllm_chat_async_co

Runs a chat call asynchronously in the xrt coroutine scheduler.

**Purpose:**

Use it when the host uses xrt coroutines and wants LLM calls to run as coroutine tasks.

**Prototype:**

```c
XLLM_API xfuture *xllm_chat_async_co(
    xllm_runtime *pRuntime,
    xcosched *pSched,
    const xllm_request *pRequest,
    const xllm_call_options *pOptions,
    size_t iStackSize
);
```

**Parameters:**

| Parameter | Direction | Can Be `NULL` | Description |
| --- | --- | --- | --- |
| `pRuntime` | input | no | Runtime. |
| `pSched` | input | no | Coroutine scheduler. |
| `pRequest` | input | no | Request. |
| `pOptions` | input | yes | Call options. |
| `iStackSize` | input | no | Coroutine stack size. Pass `0` to use xrt defaults. |

**Return Value:**

- Returns `xfuture *`.
- If `pSched` is `NULL` or coroutine support is disabled at build time, returns a future containing an error result.

**Resource Ownership:**

Free the future according to xrt rules. `pSched` is host-owned.

**Notes:**

- Available only when xrt coroutine support is enabled.
- The runtime and scheduler must remain valid until the task completes.

**Example Code:**

```c
xfuture *future = xllm_chat_async_co(runtime, sched, &request, NULL, 0);
```

**Related APIs:**

- `xllm_chat_async_thread`

---

## Cancellation Control

### xllm_cancel_token_create

Creates a cancel token.

**Purpose:**

A cancel token lets the host request cancellation during a call. Put it into `xllm_call_options.pCancelToken`.

**Prototype:**

```c
XLLM_API int xllm_cancel_token_create(xllm_cancel_token **ppToken);
```

**Parameters:**

| Parameter | Direction | Can Be `NULL` | Description |
| --- | --- | --- | --- |
| `ppToken` | output | no | Receives the new token on success. |

**Return Value:**

- `XRT_NET_OK`: created successfully.
- `XRT_NET_ERROR`: invalid parameter, allocation failure, or mutex creation failure.

**Resource Ownership:**

On success, the token is owned by the caller and must be freed with `xllm_cancel_token_destroy`.

**Notes:**

- The token uses an internal mutex to protect cancellation state.
- Initial state is not cancelled.

**Example Code:**

```c
xllm_cancel_token *token = NULL;
if (xllm_cancel_token_create(&token) == XRT_NET_OK) {
    /* Put it into xllm_call_options. */
}
xllm_cancel_token_destroy(token);
```

**Related APIs:**

- `xllm_cancel_token_cancel`
- `xllm_cancel_token_is_cancelled`

---

### xllm_cancel_token_destroy

Destroys a cancel token.

**Purpose:**

Releases the token, its internal mutex, and cancellation reason string.

**Prototype:**

```c
XLLM_API void xllm_cancel_token_destroy(xllm_cancel_token *pToken);
```

**Parameters:**

| Parameter | Direction | Can Be `NULL` | Description |
| --- | --- | --- | --- |
| `pToken` | input | yes | Token to destroy. |

**Return Value:**

None.

**Resource Ownership:**

`pToken` must come from `xllm_cancel_token_create`. Do not use it after destruction.

**Notes:**

- Do not destroy a token while a background call may still read it.
- If multiple requests share a token, destroy it after all requests finish.

**Example Code:**

```c
xllm_cancel_token_destroy(token);
token = NULL;
```

**Related APIs:**

- `xllm_cancel_token_create`

---

### xllm_cancel_token_cancel

Marks a token as cancelled.

**Purpose:**

Call it when the user clicks cancel, a task times out, or an upper-level flow is interrupted. Later paths that check the token will see the cancelled state.

**Prototype:**

```c
XLLM_API void xllm_cancel_token_cancel(xllm_cancel_token *pToken, const char *sReason);
```

**Parameters:**

| Parameter | Direction | Can Be `NULL` | Description |
| --- | --- | --- | --- |
| `pToken` | input/output | yes | Token to cancel. If `NULL`, the function returns immediately. |
| `sReason` | input | yes | Cancellation reason. The function copies this string. |

**Return Value:**

None.

**Resource Ownership:**

`sReason` is caller-owned. The function copies it internally. The previous cancellation reason is released.

**Notes:**

- Calling it multiple times keeps the token cancelled and replaces the reason.
- The current public API exposes cancellation state query, but not cancellation reason query.

**Example Code:**

```c
xllm_cancel_token_cancel(token, "user cancelled");
```

**Related APIs:**

- `xllm_cancel_token_is_cancelled`

---

### xllm_cancel_token_is_cancelled

Checks whether a token is cancelled.

**Purpose:**

Hosts or adapters can use it to decide whether current work should stop.

**Prototype:**

```c
XLLM_API bool xllm_cancel_token_is_cancelled(const xllm_cancel_token *pToken);
```

**Parameters:**

| Parameter | Direction | Can Be `NULL` | Description |
| --- | --- | --- | --- |
| `pToken` | input | yes | Token to check. Passing `NULL` returns `false`. |

**Return Value:**

- `true`: cancelled.
- `false`: not cancelled, or `pToken` is `NULL`.

**Resource Ownership:**

Does not allocate resources and does not take ownership of the token.

**Notes:**

- Query uses a mutex internally.
- `xllm_chat_ex` checks `pOptions->pCancelToken` before dispatching the adapter.

**Example Code:**

```c
if (xllm_cancel_token_is_cancelled(token)) {
    return XRT_NET_CANCELLED;
}
```

**Related APIs:**

- `xllm_cancel_token_cancel`

---

## Complete Example

The following example shows the minimal structure of Core Runtime. A real provider call needs a usable endpoint, API key, and model capabilities.

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
    request.pMessages[0].pParts[0].as.tSource.as.sText = dup_text("Introduce xllm in one sentence.");

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

## Common Mistakes

| Problem | Common Cause | Fix |
| --- | --- | --- |
| `xllm_chat_ex` returns `XLLM_ERROR_MODEL_NOT_FOUND` | `sProfileId` is not registered or is misspelled. | Call `xllm_register_profile` first and make sure the request uses the same ID. |
| Returns `profile adapter is not available` | The profile's `sAdapter` was not registered. | Call the matching `xllm_register_*_adapter` first. |
| Multimodal input is rejected | The profile has no multimodal model or missing capability bits. | Configure `tModels.tMultimodal` and `XLLM_CAP_IMAGE_IN` / `XLLM_CAP_FILE_IN`. |
| No error details | Code used `xllm_chat`. | Use `xllm_chat_ex` and pass `xllm_error`. |
| Async call crashes or behaves unpredictably | Runtime, engine, scheduler, or cancel token was released too early. | Wait for the future to complete before destroying related objects. |

## Related Examples

- `examples\smoke_chat_ex_low_level.c`
- `examples\smoke_mock_adapter.c`
- `examples\smoke_openai_compat_adapter.c`
- `build.bat smoke -Filter "chat_ex_low_level,mock_adapter,openai_compat_adapter"`
