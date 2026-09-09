# Session State Compatibility

`xllm_session_state_to_xvalue` / `xllm_session_state_from_xvalue` are the persistence boundary for session checkpointing. The C `xllm_session_state` object remains opaque; hosts should persist the exported xvalue/JSON shape rather than depending on internal fields.

## Current Format

- `type`: `xllm_session_state`
- `version`: `2`
- required recovery fields: `profile_id`, `system_prompt`, `session_summary`, `committed_turns`, `summary_turns`, `history`
- v2 policy fields: `summarizer_profile_id`, `enable_auto_compact`, `compact_trigger_ratio`, `compact_trigger_turns`, `reserve_output_tokens`, `keep_recent_turns`, `keep_active_tool_chain`, `compact_strategy`, `vendor_extra`

`version=2` is the current format because compact/session policy is now part of the recoverable state. This lets AI IDE / claw restart long-running agent work without silently changing context retention policy.

## Compatibility Rules

- Older states are accepted on a best-effort basis. Missing v2 policy fields are initialized through the normal session defaults.
- Future states with a higher `version` are accepted when the known fields are still compatible. Unknown fields are ignored by xllm.
- Breaking serialized semantics must use a new `type` or a new import API rather than reusing `xllm_session_state` with incompatible meanings.
- Hosts that need lossless roundtrip of unknown future fields should keep the original serialized checkpoint blob alongside the imported xllm session.
- Hosts should treat failed import as a recoverable checkpoint failure and fall back to a new session plus explicit user-visible recovery policy.

## Recommended Host Checkpoint Envelope

Store xllm session state inside a host-owned envelope:

- host checkpoint schema version.
- xllm library version and release channel.
- provider adapter id, profile id, model id, and important provider capability flags.
- memory namespace/profile id if long-term memory is applied to the session.
- serialized `xllm_session_state` xvalue/JSON payload.
- optional host task/workspace identifiers owned by xwork or the product layer.

The envelope is not part of xllm because xllm should stay focused on model/session/memory primitives, while task/workspace ownership belongs to xwork or the application.

## Migration Flow

1. Load the host checkpoint envelope.
2. Validate envelope-level provider/profile/workspace assumptions.
3. Decode the `xllm_session_state` payload with `xllm_session_state_from_xvalue`.
4. Import with `xllm_session_import_state`.
5. Re-export with `xllm_session_state_to_xvalue` when the checkpoint should be upgraded to the current xllm format.
6. Keep the previous checkpoint until the upgraded checkpoint has been durably written.

## Test Coverage

`session_state_options` covers:

- current `version=2` export fields.
- v2 import preserving compact policy.
- `version=1` import compatibility with defaulted v2 policy fields.
- future-version best-effort import for known compatible fields.
