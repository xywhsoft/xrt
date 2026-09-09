# xllm Providers API

> Provider API explains how to choose, register, and configure adapters and profiles for different model services.

[Back to API Index](README.en.md) | [Core Runtime API](api-core.en.md) | [Basic Types](types.en.md)

---

## Table of Contents

- [Module Role](#module-role)
- [Relationship Between Adapter and Profile](#relationship-between-adapter-and-profile)
- [Provider Constants](#provider-constants)
- [Profile Configuration Types](#profile-configuration-types)
  - [xllm_profile_init](#xllm_profile_init)
  - [xllm_profile](#xllm_profile)
  - [xllm_auth](#xllm_auth)
  - [xllm_provider_options](#xllm_provider_options)
  - [xllm_transport_options](#xllm_transport_options)
  - [xllm_profile_models](#xllm_profile_models)
  - [xllm_model_caps](#xllm_model_caps)
- [Built-in Provider Adapters](#built-in-provider-adapters)
- [Capability Matrix](#capability-matrix)
- [Stable Unsupported Items](#stable-unsupported-items)
- [Profile Examples](#profile-examples)
- [Common Mistakes](#common-mistakes)

---

## Module Role

xllm splits provider integration into two layers:

- **Adapter**: converts xllm request/response models to a provider wire protocol.
- **Profile**: stores one concrete call configuration, including adapter, endpoint, authentication, model ID, capabilities, default generation parameters, and provider-specific options.

You usually register adapters first, then register one or more profiles. During a call, you only set `sProfileId` in the request. You do not need to repeat endpoint, key, model, and capability configuration every time.

---

## Relationship Between Adapter and Profile

```text
xllm_runtime
  adapter: openai_compat
  adapter: anthropic_native
  profile: openai-fast -> adapter=openai_compat, model=gpt-4.1-mini
  profile: local-ollama -> adapter=ollama_native, model=llama3.2
```

One adapter can serve multiple profiles. For example, `openai_compat` can configure OpenAI, Azure OpenAI, or third-party OpenAI-compatible gateways at the same time.

---

## Provider Constants

| Constant | Adapter Name | Description |
| --- | --- | --- |
| `XLLM_ADAPTER_OPENAI_COMPAT` | `openai_compat` | OpenAI-compatible protocol. |
| `XLLM_ADAPTER_GLM_NATIVE` | `glm_native` | Native GLM protocol. |
| `XLLM_ADAPTER_MINIMAX_NATIVE` | `minimax_native` | Native MiniMax protocol. |
| `XLLM_ADAPTER_KIMI_NATIVE` | `kimi_native` | Native Kimi protocol. |
| `XLLM_ADAPTER_GEMINI_NATIVE` | `gemini_native` | Native Gemini protocol. |
| `XLLM_ADAPTER_VERTEX_GEMINI_NATIVE` | `vertex_gemini_native` | Native Vertex Gemini protocol. |
| `XLLM_ADAPTER_QWEN_NATIVE` | `qwen_native` | Native Qwen protocol. |
| `XLLM_ADAPTER_DOUBAO_NATIVE` | `doubao_native` | Native Doubao protocol. |
| `XLLM_ADAPTER_ANTHROPIC_NATIVE` | `anthropic_native` | Native Anthropic protocol. |
| `XLLM_ADAPTER_OLLAMA_NATIVE` | `ollama_native` | Native Ollama protocol. |

---

## Profile Configuration Types

### xllm_profile_init

Initializes a profile.

**Purpose:**

After creating `xllm_profile` on the stack, call this first to get safe defaults, then fill ID, adapter, endpoint, authentication, and model.

**Prototype:**

```c
XLLM_API void xllm_profile_init(xllm_profile *pProfile);
```

**Parameters:**

| Parameter | Direction | Can Be `NULL` | Description |
| --- | --- | --- | --- |
| `pProfile` | output | yes | Profile to initialize. |

**Return Value:**

None.

**Resource Ownership:**

Does not allocate resources. `pProfile` is caller-owned.

**Notes:**

After initialization:

- `tAuth.eKind = XLLM_AUTH_NONE`
- `tModels.tText.eCapMode = XLLM_CAP_MODE_AUTO`
- `tModels.tMultimodal.eCapMode = XLLM_CAP_MODE_AUTO`

**Example Code:**

```c
xllm_profile profile;
xllm_profile_init(&profile);
profile.sId = "demo";
profile.sAdapter = XLLM_ADAPTER_OPENAI_COMPAT;
```

**Related APIs:**

- `xllm_register_profile`

---

### xllm_profile

The profile is the main structure for provider call configuration.

| Field | Required | Description |
| --- | --- | --- |
| `sId` | yes | Unique profile ID. Requests choose configuration through it. |
| `sName` | no | Display name. |
| `sProvider` | recommended | Provider name, such as `openai`, `anthropic`, or `ollama`. |
| `sAdapter` | yes | Adapter name, usually using `XLLM_ADAPTER_*`. |
| `sBaseUrl` | required for most providers | Endpoint base URL. |
| `tAuth` | depends on provider | Authentication configuration. |
| `pDefaultHeaders` / `iDefaultHeaderCount` | optional | Default HTTP headers. |
| `tProviderOptions` | optional | Provider-specific options. |
| `tTransport` | optional | Transport options such as timeout, proxy, and TLS. |
| `tModels` | yes | Text/multimodal model bindings. |
| `tDefaults` | optional | Default generation parameters, reasoning, and response format. |
| `tVendorExtra` | optional | Extension fields. |

**Resource Ownership:**

`xllm_register_profile` clones the profile. After successful registration, the original profile can leave scope.

---

### xllm_auth

Authentication configuration.

| Field | Description |
| --- | --- |
| `eKind` | Authentication kind. |
| `sSecret` | API key, token, or other secret. |
| `sHeaderName` | API key header name. |
| `sScheme` | Custom Authorization scheme. |

| `xllm_auth_kind` | Description |
| --- | --- |
| `XLLM_AUTH_NONE` | Send no authentication. |
| `XLLM_AUTH_BEARER` | Send `Authorization: Bearer <secret>`. |
| `XLLM_AUTH_API_KEY_HEADER` | Send a custom API key header. |

**Notes:**

- Do not write real secrets into the repository.
- Logs and traces should use `XLLM_REDACT_DEFAULT` or `XLLM_REDACT_STRICT`.

---

### xllm_provider_options

Provider-specific options.

| Field | Description |
| --- | --- |
| `sOpenAIOrganizationId` | OpenAI organization header. |
| `sOpenAIProjectId` | OpenAI project header. |
| `sAnthropicApiVersion` | Anthropic API version. |
| `psAnthropicBetaHeaders` / `iAnthropicBetaHeaderCount` | Anthropic beta headers. |
| `tVendorExtra` | Other provider extensions. |

**Notes:**

These fields only matter in corresponding adapters. Setting them for unrelated providers usually has no effect.

---

### xllm_transport_options

Transport configuration.

| Field | Description |
| --- | --- |
| `tConnectTimeoutMs` / `tReadTimeoutMs` | Connect/read timeout in milliseconds. |
| `tVerifyPeer` | Whether to verify TLS peer. |
| `eProxyKind` | Proxy type. |
| `sProxyHost` / `tProxyPort` | Proxy address and port. |
| `sProxyUser` / `sProxyPass` | Proxy credentials. |
| `sCaBundlePath` | CA bundle path. |
| `sClientCertPath` / `sClientKeyPath` | Client certificate and private key. |
| `tVendorExtra` | Extension fields. |

**Suggestions:**

- Interactive requests can start with 30-90 second timeouts.
- Long code-generation or reasoning requests can use 120-300 seconds.
- Multimodal upload requests usually need longer timeouts.

---

### xllm_profile_models

Model bindings.

| Field | Description |
| --- | --- |
| `tText` | Text model binding. |
| `tMultimodal` | Multimodal model binding. |

`xllm_model_binding` fields:

| Field | Description |
| --- | --- |
| `sModelId` | Model ID used by the provider. |
| `sAliasOf` | Optional alias source. |
| `eCapMode` | Capability declaration mode. |
| `tCaps` | Model capabilities. |
| `tVendorExtra` | Extension fields. |

---

### xllm_model_caps

Model capabilities.

| Field | Description |
| --- | --- |
| `uFlags` | `XLLM_CAP_*` capability bits. |
| `psSupportedMimeTypes` / `iSupportedMimeTypeCount` | Supported MIME types. |
| `uMaxContextTokens` | Maximum context. |
| `uMaxInputTokens` / `uMaxOutputTokens` | Input/output token limits. |
| `uRecommendedOutputReserve` | Recommended output reserve. |
| `uMaxPartsPerMessage` | Maximum parts per message. |
| `uMaxImages` / `uMaxFiles` | Image/file count limits. |
| `uMaxPartBytes` | Byte limit per part. |
| `sTokenizerId` | Tokenizer identifier. |
| parameter rule fields | Limits for temperature, top_p, max output tokens. |

**Notes:**

The more accurate the capability declaration is, the better `xllm_validate_request` can catch errors before a network request.

---

## Built-in Provider Adapters

For detailed registration semantics, see [api-core.en.md](api-core.en.md#built-in-adapter-registration-functions).

| Registration Function | Constant | Scenario |
| --- | --- | --- |
| `xllm_register_openai_compat_adapter` | `XLLM_ADAPTER_OPENAI_COMPAT` | OpenAI, Azure OpenAI, OpenAI-compatible gateways. |
| `xllm_register_anthropic_native_adapter` | `XLLM_ADAPTER_ANTHROPIC_NATIVE` | Anthropic Messages API. |
| `xllm_register_ollama_native_adapter` | `XLLM_ADAPTER_OLLAMA_NATIVE` | Local or self-hosted Ollama. |
| `xllm_register_gemini_native_adapter` | `XLLM_ADAPTER_GEMINI_NATIVE` | Gemini API. |
| `xllm_register_vertex_gemini_native_adapter` | `XLLM_ADAPTER_VERTEX_GEMINI_NATIVE` | Vertex Gemini. |
| `xllm_register_glm_native_adapter` | `XLLM_ADAPTER_GLM_NATIVE` | Native GLM API. |
| `xllm_register_minimax_native_adapter` | `XLLM_ADAPTER_MINIMAX_NATIVE` | Native MiniMax API. |
| `xllm_register_kimi_native_adapter` | `XLLM_ADAPTER_KIMI_NATIVE` | Native Kimi API. |
| `xllm_register_qwen_native_adapter` | `XLLM_ADAPTER_QWEN_NATIVE` | Native Qwen API. |
| `xllm_register_doubao_native_adapter` | `XLLM_ADAPTER_DOUBAO_NATIVE` | Native Doubao API. |

---

## Capability Matrix

The following table is a learner-friendly summary of the current archived capability baseline. It represents xllm adapter-layer implementation and stable test baseline. It does not guarantee that every upstream model, region, account, or gateway supports the same capability.

| Adapter | Text | Stream | Tool call | Tool result | Image URL | Inline image | Image file_id | File URL | Inline file | File file_id | JSON/schema | Main Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `openai_compat` | yes | yes | yes | yes | yes | yes | yes | yes | yes | yes | yes | Actual capability depends on endpoint and model. |
| `azure_openai` via `openai_compat` | yes | yes | yes | yes | yes | endpoint-sensitive | yes | yes | yes | yes | yes | Large inline images may be gateway-limited. |
| `anthropic_native` | yes | yes | yes | yes | yes | yes | yes | partial | partial | yes | partial | Strict JSON schema is stably unsupported. |
| `ollama_native` | yes | yes | yes | no stable baseline | yes | yes | no | no | no | no | partial | `tool_choice=required`, file input, and image file_id are unsupported. |
| `glm_native` | yes | no stable baseline | yes | no stable baseline | yes | endpoint-sensitive | no | no | no | no | no stable baseline | File input and image file_id are unsupported. |
| `minimax_native` | yes | no stable baseline | yes | no stable baseline | yes | endpoint-sensitive | no | no | no | no | no stable baseline | File input and image file_id are unsupported. |
| `kimi_native` | yes | no stable baseline | yes | no stable baseline | yes | endpoint-sensitive | yes | no | no | no | no stable baseline | File input is unsupported. |
| `qwen_native` | yes | no stable baseline | yes | yes | yes | endpoint-sensitive | no | no | no | no | no stable baseline | Forced named tool under thinking is unsupported. |
| `doubao_native` | yes | no stable baseline | yes | yes | yes | endpoint-sensitive | no | no | no | no | no stable baseline | File input and image file_id are unsupported. |
| `gemini_native` | yes | no stable baseline | yes | partial | yes | yes | yes | yes | yes | yes | yes | Tool result currently focuses on text/json. |
| `vertex_gemini_native` | yes | no stable baseline | yes | partial | yes | yes | yes | yes | yes | yes | yes | Similar tool-result limits to Gemini. |

---

## Stable Unsupported Items

The following limitations are part of the regression-test surface. Hosts should not assume these capabilities may "secretly work".

| Adapter | Unsupported Item | Typical Error |
| --- | --- | --- |
| `anthropic_native` | strict `json_schema` output | `XLLM_ERROR_UNSUPPORTED_CAPABILITY` |
| `ollama_native` | `tool_choice=required` | `XLLM_ERROR_UNSUPPORTED_CAPABILITY` |
| `ollama_native` | file input, image `provider_file_id` | `XLLM_ERROR_UNSUPPORTED_INPUT_TYPE` |
| `glm_native` | file input, image `provider_file_id` | `XLLM_ERROR_UNSUPPORTED_INPUT_TYPE` |
| `minimax_native` | file input, image `provider_file_id` | `XLLM_ERROR_UNSUPPORTED_INPUT_TYPE` |
| `kimi_native` | file input | `XLLM_ERROR_UNSUPPORTED_INPUT_TYPE` |
| `qwen_native` | forced named tool in thinking mode, file input, image `provider_file_id` | `XLLM_ERROR_UNSUPPORTED_CAPABILITY` or `XLLM_ERROR_UNSUPPORTED_INPUT_TYPE` |
| `doubao_native` | file input, image `provider_file_id` | `XLLM_ERROR_UNSUPPORTED_INPUT_TYPE` |
| `gemini_native` / `vertex_gemini_native` | image/file parts in tool result | `XLLM_ERROR_UNSUPPORTED_INPUT_TYPE` |

---

## Profile Examples

### OpenAI-compatible Text Profile

```c
xllm_profile profile;
xllm_profile_init(&profile);

profile.sId = "openai-text";
profile.sProvider = "openai";
profile.sAdapter = XLLM_ADAPTER_OPENAI_COMPAT;
profile.sBaseUrl = "https://api.openai.com/v1";
profile.tAuth.eKind = XLLM_AUTH_BEARER;
profile.tAuth.sSecret = getenv("OPENAI_API_KEY");
profile.tModels.tText.sModelId = "gpt-4.1-mini";
profile.tModels.tText.tCaps.uFlags =
    XLLM_CAP_TEXT_IN |
    XLLM_CAP_TEXT_OUT |
    XLLM_CAP_STREAM |
    XLLM_CAP_JSON_OUT |
    XLLM_CAP_TOOL_CALL_OUT;

xllm_register_openai_compat_adapter(runtime);
xllm_register_profile(runtime, &profile);
```

### Local Ollama Profile

```c
xllm_profile profile;
xllm_profile_init(&profile);

profile.sId = "ollama-local";
profile.sProvider = "ollama";
profile.sAdapter = XLLM_ADAPTER_OLLAMA_NATIVE;
profile.sBaseUrl = "http://127.0.0.1:11434";
profile.tAuth.eKind = XLLM_AUTH_NONE;
profile.tModels.tText.sModelId = "llama3.2";
profile.tModels.tText.tCaps.uFlags = XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT | XLLM_CAP_STREAM;

xllm_register_ollama_native_adapter(runtime);
xllm_register_profile(runtime, &profile);
```

### Gemini Multimodal Profile

```c
xllm_profile profile;
xllm_profile_init(&profile);

profile.sId = "gemini-mm";
profile.sProvider = "gemini";
profile.sAdapter = XLLM_ADAPTER_GEMINI_NATIVE;
profile.sBaseUrl = "https://generativelanguage.googleapis.com";
profile.tAuth.eKind = XLLM_AUTH_API_KEY_HEADER;
profile.tAuth.sHeaderName = "x-goog-api-key";
profile.tAuth.sSecret = getenv("GEMINI_API_KEY");
profile.tModels.tText.sModelId = "gemini-2.0-flash";
profile.tModels.tText.tCaps.uFlags = XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT;
profile.tModels.tMultimodal.sModelId = "gemini-2.0-flash";
profile.tModels.tMultimodal.tCaps.uFlags =
    XLLM_CAP_TEXT_IN |
    XLLM_CAP_IMAGE_IN |
    XLLM_CAP_FILE_IN |
    XLLM_CAP_TEXT_OUT |
    XLLM_CAP_JSON_OUT;

xllm_register_gemini_native_adapter(runtime);
xllm_register_profile(runtime, &profile);
```

---

## Common Mistakes

| Problem | Common Cause | Fix |
| --- | --- | --- |
| Profile registers but request fails | Adapter was not registered first, or `sAdapter` spelling does not match. | Use `XLLM_ADAPTER_*` constants and register adapter before profile. |
| Multimodal request is rejected | Only `tText` is configured; no `tMultimodal` or capability bits. | Configure a multimodal model and `XLLM_CAP_IMAGE_IN` / `XLLM_CAP_FILE_IN`. |
| Provider returns 401 | `tAuth` is wrong or secret is empty. | Check `eKind`, header name, and environment variable. |
| OpenAI-compatible gateway rejects a field | Compatible protocol does not mean full OpenAI capability. | Restrict host UI with capability flags and fall back when needed. |
| Secret leak risk in logs | Debug is too high and redaction is disabled. | Use `XLLM_REDACT_DEFAULT` or `XLLM_REDACT_STRICT`. |

## Related Examples

- `examples\openai\azure_openai\azure_openai_stateless.c`
- `examples\gemini\gemini_multimodal.c`
- `examples\anthropic\glm\glm_stateless.c`
- `examples\qwen\qwen_tool_loop.c`
- `examples\smoke_openai_compat_adapter.c`
- `examples\smoke_ollama_native_adapter.c`
