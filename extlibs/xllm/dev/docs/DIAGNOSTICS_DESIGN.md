# xllm Core / Session Diagnostics Design

This document defines the diagnostics contract for AI IDE / claw integrations. Diagnostics are local, opt-in, and collected by the host through public result fields, trace callbacks, and explicit diagnostics APIs.

## Goals

- Give hosts enough data to debug model calls, session compaction, tool loops, and memory retrieval.
- Keep diagnostics telemetry-free by default.
- Keep core/session diagnostics independent from `xwork`; xwork can attach its own task/tool execution ids at the host layer.
- Preserve stable names for trace payload keys so IDE panels and logs can consume them.

## Existing Surfaces

- `xllm_runtime_set_trace_callback` emits `XLLM_TRACE_REQUEST`, `XLLM_TRACE_RESPONSE`, `XLLM_TRACE_STREAM`, `XLLM_TRACE_COMPACT`, and `XLLM_TRACE_TOOL_LOOP`.
- `xllm_log_event_name`, `xllm_log_level_name`, and `xllm_trace_kind_name` expose stable names for host-side structured log rows.
- `xllm_response.sId` carries provider response id when available.
- `xllm_error.sRequestId`, `iHttpStatus`, `sProviderCode`, and `sProviderMessage` expose upstream error diagnostics.
- `xllm_memory_get_diagnostics` exposes memory scheme/profile/storage/embedder/vector state.
- Session compact and tool loop emit structured trace payloads from `xllm-session`.

## Structured Log Taxonomy

The public log callback ABI remains `level/component/message` for compatibility. Hosts that need structured rows should attach the stable event name returned by `xllm_log_event_name` when emitting their own collector records.

Stable row fields:

- `event`: one of the stable names below.
- `level`: value from `xllm_log_level_name`.
- `component`: xllm component or adapter.
- `message`: human-readable local message.
- `timestamp_unix_ms`: host-supplied collection time.

Optional correlation fields:

- `request_id`
- `response_id`
- `profile_id`
- `adapter`
- `model`
- `scope`
- `namespace`
- `record_id`
- `chunk_id`
- `xwork_task_id`
- `tool_call_id`
- `status`
- `duration_ms`

Stable event names:

- `runtime.create`
- `runtime.destroy`
- `provider.request_start`
- `provider.response_complete`
- `provider.response_failed`
- `provider.retry_scheduled`
- `stream.event`
- `session.compact_triggered`
- `session.compact_result`
- `tool_loop.round`
- `tool_loop.execute`
- `tool_loop.stop`
- `memory.ingest`
- `memory.search`
- `memory.health_check`
- `workspace.sync`
- `watcher.event`

Redaction rules match trace diagnostics: structured log rows must not include API keys, auth headers, raw request bodies, raw tool secrets, or full file contents by default.

## Core Request Diagnostics

Every provider adapter should emit a request trace before the upstream call when trace is enabled.

Required keys:

- `phase=request`
- `adapter`
- `profile_id`
- `model`
- `streaming`
- `live`
- `message_count`
- `context_block_count`
- `tool_count`
- `body_bytes`
- `attempt`

Optional keys:

- `request_id` when generated before the upstream call.
- `provider_tool_count` when the provider has native server-side tools.
- `schema_mode` when a JSON/schema response format is requested.

## Core Response Diagnostics

Every provider adapter should emit a response trace after success or failure.

Required keys:

- `phase=response`
- `adapter`
- `streaming`
- `live`
- `transport_status`
- `attempt`
- `retryable`
- `http_status` when HTTP was reached.
- `success`
- `response_status`

Success keys:

- `response_id`
- `model`
- `finish_reason`
- `output_count`
- `input_tokens`
- `output_tokens`

Failure keys:

- `request_id`
- `error_code`
- `error_message`
- `provider_code`
- `provider_message`

## Session Diagnostics

Session diagnostics remain trace-first rather than a mutable global object. Hosts should correlate events by task id or request id outside xllm.

Compact trace payload:

- `phase=auto_check|compact_result`
- `triggered`
- `strategy`
- `summary_profile_id`
- `compacted`
- `summarized`
- `remote_summary_attempted`
- `remote_summary_succeeded`
- `local_fallback_used`
- `committed_turns`
- `keep_recent_turns`
- `removed_turns`
- `summary_turns_before`
- `summary_turns_after`
- `history_turns_after`
- `input_tokens_before`
- `input_tokens_after`
- `target_input_tokens`
- `trigger_turns`

Tool loop trace payload:

- `phase=round_response|tool_result|stop`
- `round`
- `response_id`
- `response_status`
- `finish_reason`
- `tool_call_count`
- `call_id`
- `tool_id`
- `tool_name`
- `resolved_tool_id`
- `resolved_wire_name`
- `exec_status`
- `result_part_count`
- `reason`
- `chat_status`

## Memory Diagnostics

Memory diagnostics are exposed through `xllm_memory_get_diagnostics`, not the trace callback, because hosts often need a point-in-time health snapshot.

Important fields for AI IDE / claw:

- namespace/profile/schema identifiers.
- SQLite open/WAL/busy-timeout state.
- sqlite-vec requested/loaded/vector-table-ready state.
- record/chunk counts split by memory vs knowledge.
- embedder kind/model/repo/assets/dimensions.
- retrieval weights and default chunk/search settings.

## Host Correlation Model

xllm does not own the global task id. AI IDE / claw should record:

- `xwork_task_id`
- `xllm_profile_id`
- provider `request_id`
- provider `response_id`
- session compact trace events
- memory namespace and top hit source/chunk ids
- xwork tool execution ids

## Stability Rules

- Trace keys listed as required should not be renamed.
- New trace keys may be added without a breaking change.
- Missing upstream ids must be represented by absence, not placeholder strings.
- Diagnostics must not include secret headers, API keys, raw auth tokens, or full request bodies by default.
- Diagnostics collection is local and opt-in; xllm must not send telemetry.
