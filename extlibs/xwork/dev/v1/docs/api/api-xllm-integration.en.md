# xllm Integration API

> Status: Chinese function-by-function reference, waiting for manual review.

xwork integrates with xllm through `xllm_runtime`, profile/bootstrap options, session policy, workspace memory and cancel token. xwork does not implement provider protocol adaptation, but puts xllm's model capabilities into an approvable and resumable run.

## Module positioning

The xllm integration API is responsible for describing how xwork borrows or creates `xllm_runtime`, how to configure provider profile/transport, and how to connect workspace memory, model stream event and cancellation boundary to agent run.

## This page covers the statement

| Category | Statement |
| --- | --- |
| External opaque objects | `xllm_runtime`, `xllm_session`, `xllm_memory`,
| Structure | `xwork_xllm_profile_options`, `xwork_xllm_transport_options`, `xwork_xllm_bootstrap_options`,
| Functions | `xwork_xllm_transport_options_init`, `xwork_xllm_profile_options_init`, `xwork_xllm_bootstrap_options_init`,

## Integrated mode

| Mode | Description |
| --- | --- |
| Borrow existing runtime | The caller creates `xllm_runtime` and lends it to xwork via `xwork_runtime_options::pLlmRuntime`. |
| Bootstrap runtime | The caller provides `xwork_runtime_options::pLlmBootstrap`, xwork creates and owns the xllm runtime when it creates the runtime. |

`pLlmRuntime` and `pLlmBootstrap` cannot be used at the same time.

## Structure fields

### xwork_xllm_profile_options

| Field | Description |
| --- | --- |
| `sProfileId` | xllm profile id. |
| `sDisplayName` | Display name. |
| `sProvider` | provider name. |
| `sAdapter` | adapter id, for example, `XWORK_XLLM_ADAPTER_OPENAI_COMPAT`. |
| `sBaseUrl` | provider base URL. |
| `sModelId` | model id. |
| `sApiKey` | API key. The caller is responsible for confidentiality and lifecycle. |
| `sOpenAIOrganizationId` | OpenAI-compatible organization id. |
| `sOpenAIProjectId` | OpenAI-compatible project id. |
| `sAnthropicApiVersion` | Anthropic API version. |
| `psAnthropicBetaHeaders` | Anthropic beta header array. |
| `iAnthropicBetaHeaderCount` | beta header quantity. |
| `iMaxOutputTokens` | The default maximum number of output tokens. `0` means not set by xwork. |

### xwork_xllm_transport_options

| Field | Description |
| --- | --- |
| `bSetConnectTimeoutMs` / `iConnectTimeoutMs` | Whether to set the connection timeout and timeout milliseconds. |
| `bSetReadTimeoutMs` / `iReadTimeoutMs` | Whether to set read timeout and timeout milliseconds. |
| `bSetVerifyPeer` / `bVerifyPeer` | Whether to set TLS peer verification. |
| `eProxyKind` | proxy type. Default `XWORK_XLLM_PROXY_UNSPECIFIED`. |
| `sProxyHost` | proxy host. |
| `bSetProxyPort` / `iProxyPort` | Whether to set proxy port and port value. |
| `sProxyUser` / `sProxyPass` | proxy credentials. |
| `sCaBundlePath` | CA bundle path. |
| `sClientCertPath` / `sClientKeyPath` | mTLS client certificate/key path. |

### xwork_xllm_bootstrap_options

| Field | Description |
| --- | --- |
| `pProfiles` | Borrowed xllm profile options array. |
| `iProfileCount` | Number of profiles. |
| `eDebugMode` | xllm debug output mode. |
| `eRedactMode` | Log desensitization mode. |
| `tTransportDefaults` | transport Default value. |

## Adapter constants

- `XWORK_XLLM_ADAPTER_OPENAI_COMPAT`
- `XWORK_XLLM_ADAPTER_GLM_NATIVE`
- `XWORK_XLLM_ADAPTER_MINIMAX_NATIVE`
- `XWORK_XLLM_ADAPTER_KIMI_NATIVE`
- `XWORK_XLLM_ADAPTER_GEMINI_NATIVE`
- `XWORK_XLLM_ADAPTER_VERTEX_GEMINI_NATIVE`
- `XWORK_XLLM_ADAPTER_QWEN_NATIVE`
- `XWORK_XLLM_ADAPTER_DOUBAO_NATIVE`
- `XWORK_XLLM_ADAPTER_ANTHROPIC_NATIVE`
- `XWORK_XLLM_ADAPTER_OLLAMA_NATIVE`

## Ownership Rules

- `xwork_runtime_options::pLlmRuntime` is a borrowed pointer, owned by the caller and destroyed.
- `xwork_runtime_options::pLlmBootstrap` is read on `xwork_runtime_create`; if bootstrap creates the xllm runtime, the xwork runtime owns it and releases it on destruction.
- `xwork_xllm_bootstrap_options::pProfiles` is a borrowed array.
- `xwork_workspace_options::pMemory` is borrowed from `xllm_memory *`.
- `xwork_runtime_get_llm_runtime` returns the borrow pointer.

---

### xwork_xllm_transport_options_init

Initialize xllm transport options.

**Function:**

You can call this function before configuring the provider transport to get stable defaults, and then set the timeout, TLS, proxy and certificate fields as needed.

**Function prototype:**

```c
XWORK_API void xwork_xllm_transport_options_init(xwork_xllm_transport_options *pOptions);
```

**parameter:**

- `pOptions`: Output parameter. Can be `NULL`; `NULL` does nothing. If it is not `NULL`, it will be cleared and the default proxy kind will be set.

**Return value:**

none.

**Resource ownership:**

Functions do not allocate resources. All string fields are caller-supplied borrow pointers.

**Additional Note:**

- Default `eProxyKind` is `XWORK_XLLM_PROXY_UNSPECIFIED`.
- The `bSet*` field is used to differentiate between "unspecified" and "explicitly set to 0/false".

**Example code:**

```c
#include "xwork.h"

int main(void) {
    xwork_xllm_transport_options options;
    xwork_xllm_transport_options_init(&options);
    options.bSetReadTimeoutMs = true;
    options.iReadTimeoutMs = 60000u;
    return 0;
}
```

**Related API:**

- `xwork_xllm_bootstrap_options_init`
- `xwork_runtime_create`

---

### xwork_xllm_profile_options_init

Initialize xllm profile options.

**Function:**

You can call this function to clear all model, adapter, endpoint, credential and token restriction fields before defining the provider profile.

**Function prototype:**

```c
XWORK_API void xwork_xllm_profile_options_init(xwork_xllm_profile_options *pOptions);
```

**parameter:**

- `pOptions`: Output parameter. Can be `NULL`; `NULL` does nothing. If it is not `NULL`, it will be cleared entirely.

**Return value:**

none.

**Resource ownership:**

Functions do not allocate resources. All strings and beta header arrays are borrowed data.

**Additional Note:**

- `sApiKey` is the caller's responsibility for confidentiality and lifecycle.
- You can apply `xwork_profile_apply_xllm_profile_options` first and then overwrite the provider details.

**Example code:**

```c
#include "xwork.h"

int main(void) {
    xwork_xllm_profile_options profile;
    xwork_xllm_profile_options_init(&profile);
    profile.sProfileId = "default";
    profile.sAdapter = XWORK_XLLM_ADAPTER_OPENAI_COMPAT;
    profile.sModelId = "gpt-example";
    return 0;
}
```

**Related API:**

- `xwork_profile_apply_xllm_profile_options`
- `xwork_xllm_bootstrap_options_init`

---

### xwork_xllm_bootstrap_options_init

Initialize xllm bootstrap options.

**Function:**

You can call this function to set the profile list, debug/redaction policy, and transport defaults before letting xwork create and own the xllm runtime.

**Function prototype:**

```c
XWORK_API void xwork_xllm_bootstrap_options_init(xwork_xllm_bootstrap_options *pOptions);
```

**parameter:**

- `pOptions`: Output parameter. Can be `NULL`; `NULL` does nothing. Cleared and initialized to default debug/redaction/transport when not `NULL`.

**Return value:**

none.

**Resource ownership:**

Functions do not allocate resources. `pProfiles` is a borrowed array and the caller must ensure that it is valid during runtime create.

**Additional Note:**

- Default `eDebugMode` is `XWORK_XLLM_DEBUG_NONE`.
- Default `eRedactMode` is `XWORK_XLLM_REDACT_DEFAULT`.
- The function calls `xwork_xllm_transport_options_init` to initialize `tTransportDefaults`.

**Example code:**

```c
#include "xwork.h"

int main(void) {
    xwork_xllm_profile_options profile;
    xwork_xllm_bootstrap_options bootstrap;

    xwork_xllm_profile_options_init(&profile);
    profile.sProfileId = "default";

    xwork_xllm_bootstrap_options_init(&bootstrap);
    bootstrap.pProfiles = &profile;
    bootstrap.iProfileCount = 1u;
    return 0;
}
```

**Related API:**

- `xwork_runtime_options_init`
- `xwork_runtime_create`
- `xwork_profile_apply_xllm_bootstrap_options`

---

### xwork_runtime_get_llm_runtime

Get the `xllm_runtime` currently bound by the runtime.

**Function:**

You can use this function to access the xllm runtime used internally by the xwork runtime in diagnostics, host integration, or advanced product logic.

**Function prototype:**

```c
XWORK_API xllm_runtime *xwork_runtime_get_llm_runtime(const xwork_runtime *pRuntime);
```

**parameter:**

- `pRuntime`: input parameters. Can be `NULL`; returns `NULL` if `NULL`.

**Return value:**

- Returns the borrowed `xllm_runtime *`.
- If the runtime is invalid or the xllm runtime is not bound, return `NULL`.

**Resource ownership:**

The return value is a borrowed pointer. Do not destroy the runtime through this pointer; ownership remains with the original caller or the xwork runtime.

**Additional Note:**

- If the runtime is created via `pLlmRuntime`, the object lent to xwork by the caller is returned.
- If the runtime is created via `pLlmBootstrap`, the object owned by the xwork runtime is returned.
- Pointer invalidated after `xwork_runtime_destroy`.

**Example code:**

```c
#include "xwork.h"

int has_llm_runtime(const xwork_runtime *runtime) {
    return xwork_runtime_get_llm_runtime(runtime) != NULL ? 1 : 0;
}
```

**Related API:**

- `xwork_runtime_create`
- `xwork_runtime_destroy`
- `xwork_xllm_bootstrap_options_init`

## Model events

xwork converts xllm stream event into `xwork_model_event` and forwards it to callback. The callback returning `false` will cancel the current model turn and propagate through `XWORK_ERROR_CANCELLED`. interrupt/cancel token checks precede user callbacks.

## Workspace memory

`xwork_workspace_options::pMemory` is borrowed `xllm_memory *`. workspace memory sync will call the xllm memory capability to synchronize the workspace root or a single file. When resuming the run, the memory object will not be automatically restored from the snapshot; the caller should re-create or load compatible memory and then register the workspace.

## Error handling

The init/getter functions on this page do not return `xwork_status`. Actual bootstrap errors are returned via `xwork_runtime_create`, such as invalid parameters, bootstrap failure, or underlying xllm provider initialization failure.

## Restore boundaries

run snapshot does not save live `xllm_runtime`, `xllm_session`, `xllm_memory` or cancel tokens. During recovery, compatible xllm runtime/profile/session/workspace memory must be re-established, and then the run or task state must be loaded.

## Thread boundaries

xwork does not declare that `xllm_runtime` or `xllm_memory` can be mutated arbitrarily concurrently. Writes, sync, model turn, and cancellation boundaries to the same runtime/session/memory should be serialized by the host or orchestrator.

## Related documents

- [Runtime API](api-runtime.md)
- [Workspace API](api-workspace.md)
- [Orchestrator API](api-orchestrator.md)
- [xllm Orchestration and Tool Loop](../guide/xllm-orchestrator-intro.md)
