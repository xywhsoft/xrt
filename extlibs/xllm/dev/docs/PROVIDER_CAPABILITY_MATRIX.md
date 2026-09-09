# Provider Capability Matrix

This document tracks xllm adapter capability coverage for agent infrastructure work. It is based on the local smoke/probe matrix and accepted limitations recorded in `OLD_DESIGN_CLOSEOUT.md`; it is not a live upstream provider guarantee.

Machine-readable companion: `docs/provider_capabilities.json`.
Adapter/plugin ownership boundary: `docs\PROVIDER_ADAPTER_BOUNDARY.md`.

Legend:

- `yes`: implemented and covered by local smoke/probe entry.
- `partial`: implemented with documented caveats or best-effort behavior.
- `unsupported`: intentionally rejected locally with a stable error baseline.
- `provider-sensitive`: xllm path exists, but observed behavior depends on endpoint/model/gateway limits.
- `not-baselined`: no stable xllm baseline yet.

## Matrix

| Provider adapter | Text chat | Stream | Tool call | Tool result | Image URL | Inline image bytes | Image file_id | File URL | Inline file bytes | File file_id | JSON/schema | Notable limitations |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `openai_compat` | yes | yes | yes | yes | yes | yes | yes | yes | yes | yes | yes | Real behavior depends on OpenAI-compatible endpoint/model capability. |
| `azure_openai` via `openai_compat` | yes | yes | yes | yes | yes | provider-sensitive | yes | yes | yes | yes | yes | Large inline image bodies may be gateway/endpoint sensitive; prefer `image_url` for real multimodal probes. |
| `anthropic_native` | yes | yes | yes | yes | yes | yes | yes | partial | partial | yes | partial | `json_schema` strict mode is unsupported baseline; document/file support is primarily PDF-focused. |
| `ollama_native` | yes | yes | yes | not-baselined | yes | yes | unsupported | unsupported | unsupported | unsupported | partial | `tool_choice=required` unsupported; file inputs and image `file_id` unsupported. |
| `glm_native` | yes | not-baselined | yes | not-baselined | yes | provider-sensitive | unsupported | unsupported | unsupported | unsupported | not-baselined | File inputs unsupported; image `file_id` unsupported; larger inline images may be endpoint sensitive. |
| `minimax_native` | yes | not-baselined | yes | not-baselined | yes | provider-sensitive | unsupported | unsupported | unsupported | unsupported | not-baselined | File inputs unsupported; image `file_id` unsupported. |
| `kimi_native` | yes | not-baselined | yes | not-baselined | yes | provider-sensitive | yes | unsupported | unsupported | unsupported | not-baselined | File inputs unsupported; larger inline images may be endpoint sensitive. |
| `qwen_native` | yes | not-baselined | yes | yes | yes | provider-sensitive | unsupported | unsupported | unsupported | unsupported | not-baselined | File inputs unsupported; image `file_id` unsupported; named tool forcing with thinking is unsupported. |
| `doubao_native` | yes | not-baselined | yes | yes | yes | provider-sensitive | unsupported | unsupported | unsupported | unsupported | not-baselined | File inputs unsupported; image `file_id` unsupported. |
| `gemini_native` | yes | not-baselined | yes | partial | yes | yes | yes | yes | yes | yes | yes | Tool result currently supports text/json parts only; image/file tool-result parts are unsupported. |
| `vertex_gemini_native` | yes | not-baselined | yes | partial | yes | yes | yes | yes | yes | yes | yes | Same Gemini tool-result image/file limitation baseline. |

## Stable Unsupported Baselines

These limitations are intentionally part of the regression surface. They should not be silently relaxed or removed without updating both this document and `docs/provider_capabilities.json`.

| Probe case | Adapter | Expected error | Contract |
| --- | --- | --- | --- |
| `probe_anthropic_json_schema_unsupported` | `anthropic_native` | `unsupported_capability` | Strict `json_schema` output is not claimed for Anthropic; adapter may still offer best-effort structured output where explicitly enabled. |
| `probe_ollama_tool_choice_required_unsupported` | `ollama_native` | `unsupported_capability` | Ollama supports `tool_choice=auto/none`; `required` is rejected locally. |
| `probe_ollama_file_alias_envs_unsupported` | `ollama_native` | `unsupported_input_type` | File input is not supported by the native Ollama chat path. |
| `probe_ollama_image_file_id_alias_envs_unsupported` | `ollama_native` | `unsupported_input_type` | Image `provider_file_id` is not supported by the native Ollama chat path. |
| `probe_glm_file_alias_envs_unsupported` | `glm_native` | `unsupported_input_type` | File input is not supported by the native GLM path. |
| `probe_glm_image_file_id_alias_envs_unsupported` | `glm_native` | `unsupported_input_type` | Image `provider_file_id` is not supported by the native GLM path. |
| `probe_minimax_file_alias_envs_unsupported` | `minimax_native` | `unsupported_input_type` | File input is not supported by the native MiniMax path. |
| `probe_minimax_image_file_id_alias_envs_unsupported` | `minimax_native` | `unsupported_input_type` | Image `provider_file_id` is not supported by the native MiniMax path. |
| `probe_kimi_file_alias_envs_unsupported` | `kimi_native` | `unsupported_input_type` | File input is not supported by the native Kimi path. |
| `probe_qwen_named_tool_with_thinking_unsupported` | `qwen_native` | `unsupported_capability` | Thinking/reasoning mode cannot force a specific named tool. |
| `probe_qwen_file_alias_envs_unsupported` | `qwen_native` | `unsupported_input_type` | File input is not supported by the native Qwen path. |
| `probe_qwen_image_file_id_alias_envs_unsupported` | `qwen_native` | `unsupported_input_type` | Image `provider_file_id` is not supported by the native Qwen path. |
| `probe_doubao_file_alias_envs_unsupported` | `doubao_native` | `unsupported_input_type` | File input is not supported by the native Doubao path. |
| `probe_doubao_image_file_id_alias_envs_unsupported` | `doubao_native` | `unsupported_input_type` | Image `provider_file_id` is not supported by the native Doubao path. |
| `probe_gemini_tool_result_image_unsupported` | `gemini_native` | `unsupported_input_type` | Tool result image/file parts are not supported; use text/json tool-result parts. |
| `probe_vertex_gemini_tool_result_image_unsupported` | `vertex_gemini_native` | `unsupported_input_type` | Same Gemini tool-result limitation on Vertex Gemini. |

## Integration Guidance

- Treat this matrix as the xllm adapter contract layer. The selected upstream model may still reject a capability that the adapter can encode.
- Agent hosts should select tools and multimodal paths from this matrix before sending a request.
- `openai_compat` should be used for OpenAI-compatible endpoints, including Azure OpenAI, when the endpoint supports the requested API shape.
- Prefer URL-based multimodal inputs for real-provider probes when a provider has known inline body size sensitivity.
- Unsupported baselines should produce local `xllm_error` failures before a network call whenever the limitation can be detected locally.
