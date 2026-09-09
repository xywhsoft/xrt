# User-Approved Memory Write Flow

This document defines the product-facing approval flow for durable memory writes. xllm provides explicit memory primitives; xwork or the host owns extraction, user consent, conflict resolution, and UI.

## Default Rule

xllm must not silently write durable long-term memory from normal chat.

Current defaults already follow this rule:

- `xllm_memory_chat_bridge_options_init` enables search-before-chat but keeps `bIngestAfterChat=false`.
- `fact.v1` and `preference.v1` are explicit host-written memories.
- conversation `turn_response` / `summary` writes require explicit ingest calls or opt-in bridge options.
- workspace/file ingest is explicit and uses sensitive-file defaults.

## Approval Modes

Products can choose one of these modes per namespace, memory type, or user setting:

- `off`: never write durable memory.
- `manual`: only write when the user explicitly asks to remember something.
- `review`: generate memory candidates, show them to the user, and write only approved items.
- `auto-low-risk`: automatically write low-risk project/workspace records, but require review for personal facts, preferences, summaries, and cross-project memories.
- `admin-policy`: enterprise host policy decides what can be written; xllm still receives only explicit write calls.

xllm does not need a public enum for these modes yet because they are product policy, not storage behavior.

## Recommended Candidate Shape

Before calling an xllm ingest API, the host should build a candidate object with:

- candidate id.
- memory type: `conversation.turn_response.v1`, `conversation.summary.v1`, `task.v1`, `fact.v1`, `preference.v1`, or workspace/document type.
- proposed title and preview text.
- source conversation id, turn id, workspace path, or tool output id.
- target namespace and scope.
- sensitivity label, source trust, and user scope.
- conflict target if it would replace an existing fact/preference/task.
- expiry or retention hint when applicable.
- reason shown to the user, such as "remembered from your instruction" or "summarized from this completed task".

The candidate object is host-owned. xllm should only receive approved writes through existing typed ingest APIs.

## Review Flow

1. xwork or the host observes a chat turn, tool result, task transition, or user command.
2. the host generates candidate memories using product policy and optional model extraction.
3. the host filters candidates for secrets, tenant/project boundaries, and conflicts.
4. the host shows candidates in UI with source, preview, target namespace, and action.
5. the user or policy approves, edits, rejects, or postpones each candidate.
6. the host calls the corresponding xllm ingest API only for approved candidates.
7. the host stores an audit row mapping candidate id to xllm record id/source URI when product governance requires it.

## API Mapping

- Approved conversation turn: `xllm_memory_ingest_turn_response` with `XLLM_MEMORY_EXTRACTION_POLICY_TURN_RESPONSE`.
- Approved conversation summary: `xllm_memory_ingest_turn_response` with `XLLM_MEMORY_EXTRACTION_POLICY_SUMMARY`, or `xllm_memory_compact_conversation` when merging existing turn records.
- Approved task state: `xllm_memory_ingest_task`.
- Approved durable fact: `xllm_memory_ingest_fact`.
- Approved stable preference: `xllm_memory_ingest_preference`.
- Approved workspace/index material: `xllm_memory_ingest_file`, `xllm_memory_ingest_directory`, `xllm_memory_ingest_workspace`, or sync APIs.

Use stable `source_uri` and typed metadata so later delete/export/review flows can identify what was written.

## Conflict And Edit Rules

- For fact/preference candidates, the host should detect existing records by typed metadata or stable source URI before writing.
- If the user edits a candidate, write the edited text and metadata, not the raw model suggestion.
- If replacing an existing memory, prefer `bReplaceExisting=true` with a stable id/source URI where the typed API supports it.
- If the conflict cannot be resolved safely, do not write; surface the conflict to the user.
- Do not use retrieved memory itself as proof that a new durable memory is true.

## Audit And Undo

The host should keep enough information to implement "show what changed" and "undo":

- candidate id and approval decision.
- xllm source URI / record id / memory type.
- source conversation/turn/task/workspace reference.
- approving user or policy id.
- timestamp and target namespace.

Undo should call the appropriate remove primitive, usually by source URI, conversation id, metadata, or task/fact/preference stable id.

## xllm Boundary

xllm owns:

- explicit typed write APIs.
- metadata persistence and retrieval.
- source URI and record identity handling.
- remove/list/search primitives used by approval, review, export, and undo flows.

xwork or host owns:

- candidate generation.
- user consent UI.
- model-based extraction prompts.
- conflict resolution.
- audit logging.
- cross-project or enterprise policy.

This keeps xllm usable as a generic agent infrastructure library while allowing AI IDE / claw to implement product-specific approval UX.
