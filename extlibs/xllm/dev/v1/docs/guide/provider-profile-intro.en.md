# Provider and Profile Introduction

> Status: Chinese draft reviewed; English translation generated.

When learning xllm, the easiest concepts to mix up are provider, adapter, profile, and model. This page separates them and explains when to add a profile and when to switch adapters.

## One-Sentence View

- **provider**: the service provider, such as OpenAI, GLM, Gemini, or Ollama.
- **adapter**: xllm code that connects to a specific protocol.
- **profile**: a concrete call configuration registered into the runtime.
- **model binding**: the model used by a slot such as text, vision, or multimodal inside a profile.

Remember it like this:

```text
adapter decides "how to send the request"
profile decides "where to send it, which secret to use, and which model to call"
```

## Adapter and Profile Relationship

One adapter can correspond to multiple profiles. For example, you can use the same `openai_compat` adapter to register:

- An OpenAI profile.
- An Azure OpenAI profile.
- A local OpenAI-compatible gateway profile.

Each profile has its own `sId`. When calling, you select a profile, not an adapter directly.

## Common Built-In Adapters

| Adapter Constant | Use Case |
| --- | --- |
| `XLLM_ADAPTER_OPENAI_COMPAT` | OpenAI-compatible APIs. |
| `XLLM_ADAPTER_GLM_NATIVE` | GLM native API. |
| `XLLM_ADAPTER_MINIMAX_NATIVE` | MiniMax native API. |
| `XLLM_ADAPTER_KIMI_NATIVE` | Kimi native API. |
| `XLLM_ADAPTER_GEMINI_NATIVE` | Gemini native API. |
| `XLLM_ADAPTER_VERTEX_GEMINI_NATIVE` | Vertex Gemini. |
| `XLLM_ADAPTER_QWEN_NATIVE` | Qwen native API. |
| `XLLM_ADAPTER_DOUBAO_NATIVE` | Doubao native API. |
| `XLLM_ADAPTER_ANTHROPIC_NATIVE` | Anthropic native API. |
| `XLLM_ADAPTER_OLLAMA_NATIVE` | Local or remote Ollama service. |

Registration functions look like:

```c
xllm_register_openai_compat_adapter(runtime);
xllm_register_glm_native_adapter(runtime);
xllm_register_gemini_native_adapter(runtime);
```

## The Most Important Profile Fields

```c
typedef struct {
    const char *sId;
    const char *sName;
    const char *sProvider;
    const char *sAdapter;
    const char *sBaseUrl;
    xllm_auth tAuth;
    xllm_header *pDefaultHeaders;
    size_t iDefaultHeaderCount;
    xllm_provider_options tProviderOptions;
    xllm_transport_options tTransport;
    xllm_profile_models tModels;
    xllm_profile_defaults tDefaults;
    xvalue tVendorExtra;
} xllm_profile;
```

For beginners, focus on these fields:

| Field | When to Fill It |
| --- | --- |
| `sId` | Required. Used later when binding the profile. |
| `sProvider` | Recommended for logs and diagnostics. |
| `sAdapter` | Required. Must match a registered adapter. |
| `sBaseUrl` | Usually required for real providers. |
| `tAuth` | Fill when auth is required. |
| `tModels.tText.sModelId` | Text model used for text calls. |
| `tModels.tText.tCaps.uFlags` | Declares model capabilities. |

## Configuring Auth

Bearer token:

```c
profile.tAuth.eKind = XLLM_AUTH_BEARER;
profile.tAuth.sSecret = getenv("GLM_API_KEY");
```

API key header:

```c
profile.tAuth.eKind = XLLM_AUTH_API_KEY_HEADER;
profile.tAuth.sHeaderName = "api-key";
profile.tAuth.sSecret = getenv("AZURE_OPENAI_API_KEY");
```

No auth:

```c
profile.tAuth.eKind = XLLM_AUTH_NONE;
```

Local services such as Ollama often use no auth.

## Why Declare Model Capabilities?

xllm uses capability declarations for request validation and routing decisions. For example:

```c
profile.tModels.tText.tCaps.uFlags =
    XLLM_CAP_TEXT_IN |
    XLLM_CAP_TEXT_OUT |
    XLLM_CAP_STREAM;
```

This means the text model supports text input, text output, and streaming output. If you send an image to a model that only declares text capabilities, `xllm_validate_request` or the call path can detect the problem earlier.

## When to Add a Profile

Usually add a profile when:

- You want to switch models under the same provider.
- You want to connect to different endpoints under the same adapter.
- The same endpoint has different secrets, organizations, projects, or tenants.
- You want separate defaults for "text model" and "multimodal model."

## When to Switch Adapter

Usually switch adapters when:

- The provider protocol is not OpenAI-compatible.
- You need provider-native capabilities, such as native multimodal, native tools, or special headers.
- You connect to local Ollama instead of a remote HTTP API.

## Example: Register a GLM Profile

```c
xllm_profile profile;
xllm_profile_init(&profile);

xllm_register_glm_native_adapter(runtime);

profile.sId = "glm-native";
profile.sProvider = "zhipu";
profile.sAdapter = XLLM_ADAPTER_GLM_NATIVE;
profile.sBaseUrl = "https://open.bigmodel.cn/api/paas/v4";
profile.tAuth.eKind = XLLM_AUTH_BEARER;
profile.tAuth.sSecret = getenv("GLM_API_KEY");
profile.tModels.tText.sModelId = "glm-5-turbo";
profile.tModels.tText.tCaps.uFlags =
    XLLM_CAP_TEXT_IN |
    XLLM_CAP_TEXT_OUT |
    XLLM_CAP_STREAM;

xllm_register_profile(runtime, &profile);
```

## Example: Register an OpenAI-Compatible Profile

```c
xllm_profile profile;
xllm_profile_init(&profile);

xllm_register_openai_compat_adapter(runtime);

profile.sId = "openai";
profile.sProvider = "openai";
profile.sAdapter = XLLM_ADAPTER_OPENAI_COMPAT;
profile.sBaseUrl = "https://api.openai.com/v1";
profile.tAuth.eKind = XLLM_AUTH_BEARER;
profile.tAuth.sSecret = getenv("OPENAI_API_KEY");
profile.tModels.tText.sModelId = "gpt-4.1-mini";
profile.tModels.tText.tCaps.uFlags =
    XLLM_CAP_TEXT_IN |
    XLLM_CAP_TEXT_OUT |
    XLLM_CAP_JSON_OUT |
    XLLM_CAP_TOOL_CALL_OUT |
    XLLM_CAP_STREAM;

xllm_register_profile(runtime, &profile);
```

## Two Ways to Select a Profile

Lightweight `xllm` object:

```c
xllm *llm = xllm_create(runtime, NULL);
xllm_bind_profile(llm, "glm-native");
```

Low-level request:

```c
xllm_request request;
memset(&request, 0, sizeof(request));
request.sProfileId = "glm-native";
```

For beginners, `xllm_create` + `xllm_bind_profile` is recommended because it reduces repeated configuration per turn.

## Common Mistakes

### `sAdapter` was not registered

If you set `XLLM_ADAPTER_GLM_NATIVE` but did not call `xllm_register_glm_native_adapter(runtime)` first, profile registration or later calls can fail.

### Empty secret

If `getenv("GLM_API_KEY")` returns `NULL`, the request fails with an auth error. When debugging, first check whether the environment variable exists. Do not put secrets into docs or repositories.

### Too few capabilities declared

If you use JSON, tools, multimodal, or streaming output, declare the corresponding capability in caps. Otherwise xllm will assume the current model does not support the request.

## Next Steps

- Continue with [Request / Response Introduction](request-response-intro.en.md).
- For function and field details, see [Provider API](../api/api-providers.en.md) and [Core API](../api/api-core.en.md).

[Back to Tutorials](README.en.md)
