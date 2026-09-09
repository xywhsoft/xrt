# Conversation Memory Compaction

`xllm_memory_compact_conversation` compacts existing typed conversation turn memory into one typed summary memory record.

## Boundary

`xllm` owns:

- finding `conversation.turn_response.v1` records for a conversation.
- writing a `conversation.summary.v1` record with stable conversation metadata.
- optionally removing the source turn records after the summary is written.
- returning counts for matched, compacted, and removed records.

The host or `xwork` owns:

- deciding when compaction should run.
- generating or approving the summary text.
- choosing whether source turn records are deleted or retained.
- user-visible retention and recovery policy.

`xllm` does not call a model to summarize memory inside this API. That keeps provider cost, prompt policy, and user confirmation under the host.

## Minimal Flow

```c
xllm_memory_compact_conversation_options opt;
xllm_memory_compact_conversation_result result;
xllm_error err;

xllm_memory_compact_conversation_options_init(&opt);
xllm_memory_compact_conversation_result_init(&result);
xllm_error_init(&err);

opt.eScope = XLLM_MEMORY_SCOPE_MEMORY;
opt.sConversationId = "chat-123";
opt.sSummaryText = "Host-approved summary text.";
opt.sSummaryRecordId = "conversation-summary:chat-123";
opt.sSummarySourceUri = "conversation://chat-123/summary";
opt.bRemoveSourceRecords = true;

if (xllm_memory_compact_conversation(memory, &opt, &result, &err) != XRT_NET_OK) {
    /* handle err */
}
```

## Metadata

The summary is written through the existing summary extraction path:

- `memory_type=conversation.summary.v1`
- `extraction_policy=summary`
- `conversation_kind=summary`
- `conversation_id=<conversation>`
- `created_at_unix` / `updated_at_unix`
- optional `priority` and expiry metadata if provided by the caller.

Source records selected for compaction are restricted to:

- the requested scope.
- the requested `conversation_id`.
- `memory_type=conversation.turn_response.v1`.

Existing summary records are not selected as source records.

## Deletion Semantics

When `bRemoveSourceRecords=true`, source records are removed by exact source URI after the summary write succeeds.

This gives a simple failure model:

- if source listing fails, no summary is written.
- if summary write fails, no source records are removed.
- if source removal fails after summary write, the error is returned and the host can retry compaction or run explicit cleanup.

## Recommended Policy

- Compact only after a host-visible summary has been generated or approved.
- Keep recent turns un-compacted when the product still needs high-fidelity local context.
- Use stable summary record ids per conversation if repeated compaction should replace the rolling summary.
- Keep compaction separate from `session` summarization; session summary is short-term context, memory compaction is long-term retention.
