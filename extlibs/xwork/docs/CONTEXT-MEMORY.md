# Compaction and memory orchestration

## Quality-controlled compaction

`xwork` treats summary generation as a fallible model operation. It evaluates
every candidate with the `xllm-session` quality policy before commit. A rejected
candidate emits `XWORK_EVENT_COMPACTION_REJECTED`, including attempt number,
token bounds, and missing-section masks. The default policy performs one
corrective retry and includes the rejected candidate plus deterministic policy
feedback in the retry request.

Only an accepted candidate can advance the durable checkpoint. Run results
separate committed compactions from rejected candidates, and compaction-done
events carry the accepted quality report.

## Layered memory retrieval

An optional borrowed `xllm_memory` store can be supplied in
`xwork_agent_config`. For each new user run, xwork independently queries:

1. the `memory` layer for durable facts, preferences, tasks, and summaries;
2. the `knowledge` layer for workspace or domain knowledge.

Each layer has an independent hit and byte budget. Results are rendered by
`xllm-memory` with namespace, scope, provenance, trust, sensitivity, revision,
and content fingerprints. They are inserted as synthetic user-role reference
material, never as trusted system instructions. Expired records and records
above the configured sensitivity ceiling are filtered before injection; the
default ceiling is `internal`, so `sensitive` and `secret` records are excluded.

Every query emits `XWORK_EVENT_MEMORY_RETRIEVED`, including scope, store
revision, hit count, and injected byte count—even when there are no hits. Run
results aggregate the same metrics. Resume does not repeat retrieval because
the synthetic reference messages are already durable session entries.

Memory mutation remains explicit and outside the agent loop. The product host
owns approval and mutation commands; xwork owns bounded retrieval, safe
injection, and audit events.
