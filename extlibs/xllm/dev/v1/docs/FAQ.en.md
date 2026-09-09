# xllm FAQ

This page answers common questions about setup, builds, provider configuration, sessions, memory, tool calls, diagnostics, and release bundle consumption.

[Back to Documentation Center](README.en.md)

## What should I read first?

For a first pass, read in this order:

1. [Your First xllm Program](guide/first-xllm-program.en.md)
2. [Provider and Profile Introduction](guide/provider-profile-intro.en.md)
3. [Request / Response Introduction](guide/request-response-intro.en.md)
4. [Minimal Chat Call](case/minimal-chat.en.md)

If you are building an agent or AI IDE, continue with session, tool loop, memory, and case studies.

## How is xllm different from calling a provider SDK directly?

When calling provider SDKs directly, you usually handle request formats, tool protocols, errors, stream events, and model capabilities separately for each provider.

xllm provides unified structures:

- `xllm_profile` manages provider/model configuration.
- `xllm_request` / `xllm_turn` manage input.
- `xllm_response` manages output.
- `xllm_session` manages short-term history.
- `xllm_memory` manages long-term memory and RAG.
- `xllm_error` manages diagnostics.

## I only want to ask the model once. Do I need session?

No. For a single-turn request, use `xllm_create` + `xllm_send_ex`, or the low-level `xllm_chat_ex`.

Use `xllm_session` only when you want the model to remember previous turns inside the current conversation.

## Can session be used as long-term memory?

Not recommended. Session is short-term history and is affected by compact, import/export behavior, and context windows.

Long-term preferences, tasks, facts, and project knowledge should be written into `xllm_memory`.

## Are memory and RAG required?

No. Normal chat does not require memory.

Use memory when the model needs to reference project documents, workspace code, long-term user preferences, or conversation summaries.

## Why does my request say a capability is unsupported?

Usually the profile's `tCaps.uFlags` does not declare the capability required by the request, or the model truly does not support it.

Check:

1. `xllm_error.uRequiredCapability`.
2. `xllm_error.sSelectedModel`.
3. Text/multimodal model caps in the profile.
4. Provider probes if provider capability has changed.

## Why is there no streaming output?

Check:

- Whether the profile includes `XLLM_CAP_STREAM`.
- Whether `xllm_call_options.eStreamMode` is `XLLM_STREAM_PREFER` or `XLLM_STREAM_REQUIRE`.
- Whether `pfnOnEvent` is set.
- Whether the callback returns `true`.
- Whether the provider model currently supports streaming.

## A tool call was returned, but the tool did not execute. Why?

If you do not set an executor, a normal send can stop at `XLLM_STATUS_TOOL_CALL_REQUIRED`. That is expected.

For automatic tool execution, you need:

- An `xllm_tool_def` added to the turn.
- Profile capabilities `XLLM_CAP_TOOL_CALL_OUT` and `XLLM_CAP_TOOL_RESULT_IN`.
- A call to `xllm_set_tool_executor` or `xllm_session_set_tool_executor`.

## Can tool arguments be executed directly?

No. `sArgumentsJson` comes from model output and must be treated as untrusted input.

Parse JSON, validate schema, check permissions and path boundaries, then execute the tool.

## Memory cannot find content I just ingested. What should I check?

Check:

- Whether ingest scope and search scope match.
- Whether `sQuery` is related to the content.
- Whether `tMinScore` is set too high.
- Whether metadata filters were set but the records do not contain matching metadata.
- Whether you are reading old pointers after `xllm_memory_search_result_reset`.
- Whether record/chunk counts look correct.

Use `xllm_memory_get_diagnostics` and `xllm_memory_search_debug` for troubleshooting.

## Why did workspace indexing skip files?

Possible causes:

- The file is hidden.
- The extension is not in `sAllowedExtensions`.
- It matched ignored directories/extensions/patterns.
- The file exceeds `uMaxFileBytes`.
- `.gitignore` or ignore files applied.
- Sensitive defaults skipped it.

Inspect ingest result skipped details to locate the reason.

## Does xllm automatically upload my workspace?

xllm memory handles local ingest/search/apply. Whether retrieved snippets are sent to a provider depends on whether you inject them into a request/turn and call the model.

Filter sensitive files first, then control the RAG injection budget.

## When should I use `xllm_request` vs. `xllm_turn`?

`xllm_turn` is suitable for most learning and chat scenarios. It is more convenient.

`xllm_request` is for advanced scenarios where you need full control over messages, context blocks, tools, response format, and profile.

## What should I record when a call fails?

At minimum:

- `xllm_version()`
- API return value
- `xllm_error.eCode`
- `xllm_error.iHttpStatus`
- `xllm_error.sMessage`
- `xllm_error.sProviderCode`
- `xllm_error.sRequestId`
- Current profile ID and model

Do not log API keys or full sensitive bodies.

## What if release bundle verification fails?

First identify the failure type:

- `verify-version` failed: synchronize version files, header macros, and release notes.
- Checksum failed: rebuild or re-download the package.
- `verify-artifact` failed: check bundle layout and metadata.
- Downstream smoke failed: check whether the bundle missed headers, sources, or dependencies.

Continue with: [Release Bundle Consumption Case](case/release-bundle-consumption.en.md)

## Should English docs be generated now?

The documentation policy is to finish and review the Chinese version first, then generate `.en.md` translations. These English files are generated after that review step.

## Where do I look up function parameters and cleanup rules?

Read the [API Index](api/README.en.md). API pages follow the xrt-style standard with prototypes, parameters, return values, ownership, notes, and examples.
