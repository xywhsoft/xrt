# Provider Limitations

This document summarizes accepted provider limitations and operational guidance. Capability-level support is tracked in `docs\PROVIDER_CAPABILITY_MATRIX.md` and `docs\provider_capabilities.json`.

## Common Limitations

- Providers differ on tool choice support, named tool forcing, JSON schema strictness, multimodal inputs, and file sources.
- Some OpenAI-compatible providers accept the OpenAI wire shape but reject specific advanced fields.
- Streaming and non-streaming responses may expose different metadata.
- Response ids and request ids may be absent for some providers.
- Token usage can be missing or partial.

## Timeout Guidance

Host defaults should be conservative:

- short interactive chat: 30-90 seconds.
- long code-generation or reasoning request: 120-300 seconds.
- file/multimodal upload request: provider-specific, usually longer.
- stream idle timeout should be separate from total timeout when supported by the host network layer.

Provider adapters should expose transport failures as `XLLM_ERROR_TRANSPORT` or normalized upstream errors when HTTP status is available.

## Retry Guidance

Retry is appropriate for:

- transport interruptions.
- HTTP 408, 409, 425, 429.
- HTTP 500, 502, 503, 504.
- provider-specific transient overload codes.

Retry is not appropriate for:

- invalid request schema.
- unsupported capability.
- authentication failure.
- quota exhaustion that is not rate-limit retryable.
- content/policy refusal.

Adapters should trace each attempt with `attempt` and `retryable`.

## Rate Limit Guidance

Host or xwork should own global rate limiting because limits are often account, org, project, or model scoped.

xllm should:

- preserve upstream HTTP status.
- preserve provider request id.
- expose retryability in trace payload.
- avoid silently retrying non-idempotent host operations.

## Capability Handling

If a provider does not support a requested capability:

- return `XLLM_ERROR_UNSUPPORTED_CAPABILITY` for local unsupported combinations.
- return normalized upstream error for provider rejection.
- document accepted unsupported cases in the capability matrix.

Do not emulate provider-native capabilities with hidden behavior unless the public API explicitly promises it.

## Provider-Specific Notes

OpenAI-compatible:

- Broadest baseline for text, tools, streaming, JSON/schema, image/file variants.
- Compatibility varies substantially across third-party OpenAI-compatible providers.

Anthropic:

- Strong native tools and streaming support.
- Document/file support differs from OpenAI-style file handling.

Ollama:

- Local model behavior depends on installed model and Ollama version.
- Some structured-output/tool features are model-dependent.

Gemini / Vertex Gemini:

- Multimodal support is strong but file/tool-result image semantics differ from OpenAI-style adapters.
- Some response ids are payload-derived and request ids may be unavailable.

GLM / Qwen / Kimi / MiniMax / Doubao:

- Often OpenAI-like but with accepted unsupported cases for some file/image/tool/schema combinations.
- Treat capability matrix as the source of truth before enabling features in a host UI.

## Release Expectations

Before release:

- run local provider smoke subsets.
- run real-provider probes only when credentials are available.
- compare probe summaries when stable report generation is available.
- update `PROVIDER_CAPABILITY_MATRIX.md` and `provider_capabilities.json` when behavior changes.
