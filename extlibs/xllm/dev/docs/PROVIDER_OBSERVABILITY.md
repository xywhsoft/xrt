# Provider Request / Response Observability

This document defines how xllm exposes provider request ids, response ids, upstream HTTP status, and retry metadata.

## v2 Current Surface

`xllm_response.tDiagnostics` and `xllm_error.tDiagnostics` carry the final attempt snapshot. The snapshot includes attempt/max-attempt counts, retry-after delay, retry exhaustion, response/model-delivery guards, xrt transport error and phase names, system error, connection reuse, monotonic timing milestones, derived durations, and byte counts.

`xllmClientComplete()` performs bounded transient retries. `xllmClientStart()` plus `xllmCallWait()` remains a single-attempt cancellable primitive. A failed attempt is retried only before any model SSE event has been parsed and delivered. Cross-task rate limiting and account/model quota policy remain host or xwork responsibilities.

## Field Definitions

- `request_id`: upstream request correlation id returned in response headers or provider error payloads.
- `response_id`: provider response object id returned in the response JSON/SSE payload.
- `http_status`: upstream HTTP status code when a provider request reached HTTP.
- `transport_status`: xrt transport status for the network operation.
- `response_status`: normalized xllm response status, for example `completed`, `incomplete`, `refused`, or `errored`.
- `attempt`: one-based adapter attempt count when retry policy is active.
- `retryable`: whether the observed failure was treated as retryable by the adapter.

## Public Surfaces

Success path:

- `xllm_response.sId` stores `response_id` when the provider returns one.
- response trace payload includes `response_id`, `response_status`, `http_status`, `transport_status`, `attempt`, token usage, model, and finish reason.

Error path:

- `xllm_error.sRequestId` stores `request_id` when the provider exposes one.
- `xllm_error.iHttpStatus` stores upstream HTTP status when available.
- `xllm_error.sProviderCode` and `xllm_error.sProviderMessage` preserve provider error details.
- response trace payload includes request id, upstream status, retryability, and normalized error code.

## Header Mapping

Adapters should check provider-specific headers first, then common fallbacks.

OpenAI-compatible:

- `x-request-id`
- `request-id`

Anthropic:

- `request-id`
- `x-request-id`

Google/Gemini style:

- response id is usually payload-derived.
- request id may be absent; do not synthesize one unless the provider returns it.

Other OpenAI-like providers:

- prefer `x-request-id`.
- preserve payload `id` as response id.

## Trace Requirements

Every adapter should emit:

- request trace before send.
- response trace after success, failure, or exhausted retry.
- stream trace for SSE payload blocks when stream tracing is enabled.

Response trace should include:

- `request_id` if available.
- `response_id` if available.
- `http_status` if HTTP was reached.
- `transport_status` for network result.
- `attempt` and `retryable`.
- `success`.

## Retry Semantics

- Each attempt should emit a request trace.
- Each failed attempt should emit a response trace with `retryable=true` if it will retry.
- Final failure should emit `retryable=false` when no further retry will happen.
- Successful retry should emit the final success response trace with the final provider ids.

## Host Guidance

AI IDE / claw should persist:

- `xwork_task_id`
- `adapter`
- `profile_id`
- `model`
- `request_id`
- `response_id`
- `http_status`
- `transport_status`
- `attempt`
- `response_status`
- normalized `xllm_error_code` on failure.

Do not log request bodies, API keys, auth headers, or raw files by default.

## Probe Stable Summary

`tests/probe/run_real_provider_probe_matrix.ps1` writes both detailed and stable reports:

- `real_provider_probe_report.txt` / `real_provider_probe_report.json`: detailed local run report with paths, durations, timestamps, proxy detail, and raw per-case summaries.
- `real_provider_probe_stable_summary.txt` / `real_provider_probe_stable_summary.json`: release-comparison summary with volatile fields removed.

The stable summary schema is versioned by `schema_version`. It keeps:

- matrix status: `success`, `phase`, `adapter`, `model`, normalized `case_filter`, executed/failed/skipped counts, and missing environment variable names.
- per-case status: `name`, `outcome`, `exit_code`, `success`, normalized response `status`, `error_code`, `http_status`, `finish_reason`, response format, stream mode, multimodal/tool flags, and output part counts.
- skipped cases: `name` and stable reason text.

It intentionally excludes base URLs, request ids, response ids, visible model output, log paths, output directories, timestamps, and durations so release checks can diff summaries across runs without noisy changes.

## Compatibility

This strategy is compatible with current xllm fields:

- `xllm_response.sId`
- `xllm_error.sRequestId`
- `xllm_error.iHttpStatus`
- trace callback payloads

Future extensions should add fields to result/diagnostics structs at the tail and keep existing trace key names stable.
