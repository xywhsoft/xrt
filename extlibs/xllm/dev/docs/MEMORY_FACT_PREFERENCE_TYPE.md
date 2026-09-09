# Fact and Preference Memory

`fact.v1` and `preference.v1` are explicit host-written typed memories. `xllm` does not infer these from raw chat by default because incorrect durable memory is worse than missing memory.

## Fact

Use `xllm_memory_ingest_fact` for durable statements the host has validated.

Metadata:

- `memory_type=fact.v1`
- `extraction_policy=fact`
- `fact_id`
- `fact_subject`
- `fact_predicate`
- `fact_object`
- `source_conversation_id`
- `source_turn_id`

Default record/source identity:

- record id: `fact:<fact_id>`
- source URI: `fact://<fact_id>`

## Preference

Use `xllm_memory_ingest_preference` for stable user or workspace preferences.

Metadata:

- `memory_type=preference.v1`
- `extraction_policy=preference`
- `preference_id`
- `preference_subject`
- `preference_key`
- `preference_value`
- `source_conversation_id`
- `source_turn_id`

Default record/source identity:

- record id: `preference:<preference_id>`
- source URI: `preference://<preference_id>`

## Boundary

- `xllm` owns storage, typed metadata, search/list filtering, and explicit write APIs.
- `xwork` or the host owns extraction decisions, conflict resolution, user confirmation, and delete/export governance.
- Automatic fact/preference extraction should remain opt-in and should write through these explicit APIs after host policy approval.
- User-approved write policy is detailed in `docs\MEMORY_WRITE_APPROVAL.md`.
