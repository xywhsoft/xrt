# Explicitly Ingesting Conversation Memory

This case shows how a host writes user-approved long-term conversation memory into `xllm_memory`, then retrieves and injects it before a later turn.

[Back to Case Studies](README.en.md) | [Conversation Memory Introduction](../guide/conversation-memory-intro.en.md) | [Memory Ingest API](../api/api-memory-ingest.en.md)

## Problem

During conversation, users may tell the assistant information that should be remembered later, such as answer style, project habits, current tasks, and long-term preferences. A session only stores current short-term history and is not suitable as long-term memory.

Conversation memory works like this:

1. The host decides that one piece of information is worth keeping long term.
2. The host asks for user approval when needed.
3. The host writes it into `XLLM_MEMORY_SCOPE_MEMORY`.
4. Before later conversations, the host retrieves relevant memory.
5. The host injects memory as a context block into the request.

The complete example is:

```text
examples/conversation_memory/conversation_memory.c
```

## Architecture

```text
conversation summary / user preference / task fact
  -> explicit ingest
  -> memory scope
next user question
  -> memory search
  -> context block
  -> model request
```

The key is "explicit". Do not write every chat sentence into long-term memory by default.

## Step 1: Create Memory

```c
xllm_memory_options tMemoryOptions;

xllm_memory_options_init(&tMemoryOptions);
tMemoryOptions.sNamespace = "example-conversation-memory";
tMemoryOptions.eScheme = XLLM_MEMORY_SCHEME_BUILTIN_SPARSE;
tMemoryOptions.uDefaultMaxHits = 3u;

xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
```

Long-term memory should use a stable namespace. Multi-user systems should isolate namespaces or underlying storage by tenant, user, or workspace.

## Step 2: Ingest an Initial Summary

The example wraps summary ingestion in a helper:

```c
static int ingest_conversation_summary(
    xllm_memory *pMemory,
    const char *sRecordId,
    const char *sTitle,
    const char *sSourceUri,
    const char *sSummaryText,
    xllm_error *pError
)
{
    xllm_memory_ingest_options tOptions;

    xllm_memory_ingest_options_init(&tOptions);
    tOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tOptions.sRecordId = sRecordId;
    tOptions.sTitle = sTitle;
    tOptions.sSourceUri = sSourceUri;
    tOptions.sText = sSummaryText;
    tOptions.bReplaceExisting = true;
    tOptions.uChunkChars = 1024u;

    return xllm_memory_ingest_text(pMemory, &tOptions, pError);
}
```

Call:

```c
ingest_conversation_summary(
    pMemory,
    "thread-001-summary",
    "Conversation summary",
    "memory://conversation/thread-001/summary",
    "summary: User prefers concise answers and uses claw for repository automation.",
    &tError
);
```

This saves two kinds of information: answer-style preference and the automation tool the user uses. In real applications, preference-like information should usually be approved by the user.

## Step 3: Retrieve and Inject Before the Next Turn

```c
xllm_memory_search_options tSearch;
xllm_memory_context_options tContext;
xllm_request tRequest;

xllm_memory_search_options_init(&tSearch);
xllm_memory_context_options_init(&tContext);
xllm_request_init(&tRequest);

tSearch.eScope = XLLM_MEMORY_SCOPE_MEMORY;
tSearch.sQuery = "What should I remember about user answer style and claw automation?";
tSearch.uMaxHits = 1u;

tContext.sLabel = "Conversation memory:";
tContext.uMaxHits = 1u;
tContext.uMaxCharsPerHit = 512u;

xllm_memory_search_and_apply_to_request(
    pMemory,
    &tSearch,
    &tRequest,
    &tContext,
    &tError
);
```

The injected context block contains the label and matched text. The example checks that it contains:

- `Conversation memory:`
- `concise answers`
- `claw`

Then add the current user message to the request or turn and send it to the model.

## Step 4: Ingest a New Summary After Chat

After a conversation ends, explicitly ingest new information only if it is worth keeping long term:

```c
ingest_conversation_summary(
    pMemory,
    "thread-001-turn-002-summary",
    "Conversation summary",
    "memory://conversation/thread-001/turn-002-summary",
    "summary: Assistant explained that conversation memory should be explicitly written after chat.",
    &tError
);
```

Then verify that the new memory can be retrieved:

```c
xllm_memory_search_options_init(&tSearch);
tSearch.eScope = XLLM_MEMORY_SCOPE_MEMORY;
tSearch.sQuery = "explicitly written after chat";
tSearch.uMaxHits = 1u;

xllm_memory_search(pMemory, &tSearch, &tResult, &tError);
```

## User Approval Strategy

Long-term memory affects future answers, so distinguish three kinds of information:

| Information | Recommendation |
| --- | --- |
| User explicitly says "remember this" | Can be ingested, but still show what will be saved |
| Preferences, habits, personal information | Require user approval |
| Temporary context, one-off task details | Usually do not write into long-term memory |

The UI can show: "I can remember: you prefer concise Chinese answers. Save this?" Ingest only after the user confirms.

## Deletion and Governance

Long-term memory should provide deletion. Common deletion methods:

```c
uint32 uRemoved = 0;

xllm_memory_remove(
    pMemory,
    XLLM_MEMORY_SCOPE_MEMORY,
    "thread-001-summary",
    &tError
);

xllm_memory_remove_by_source_uri(
    pMemory,
    XLLM_MEMORY_SCOPE_MEMORY,
    "memory://conversation/thread-001/summary",
    &uRemoved,
    &tError
);
```

If your application writes metadata for memory, you can also use `xllm_memory_remove_by_metadata` to delete by user, conversation, project, or tag.

## Key APIs

| API | Purpose |
| --- | --- |
| `xllm_memory_ingest_text` | Ingest summary-style long-term memory |
| `xllm_memory_ingest_turn_response` | Ingest summary, task, fact, or preference from a turn/response |
| `xllm_memory_ingest_task` | Ingest a task |
| `xllm_memory_ingest_fact` | Ingest a structured fact |
| `xllm_memory_ingest_preference` | Ingest user preference |
| `xllm_memory_search_and_apply_to_request` | Retrieve and inject into a request |
| `xllm_memory_remove` | Delete by record ID |
| `xllm_memory_remove_by_source_uri` | Delete by source URI |

## Extension Points

You can add:

- Typed memory: use task/fact/preference instead of plain summaries.
- Metadata: tag by user, project, and conversation.
- Expiration: automatically remove temporary tasks when they expire.
- User-visible management: list, edit, and delete long-term memory.
- Summary compacting: compress multiple old memories into a shorter summary.

## Common Questions

Do not write all chat content into memory. Long-term memory should be small and precise.

Do not omit source URI. Every long-term memory item should be traceable to user approval or a conversation turn.

Do not save sensitive information by default. Preferences and personal information should have user confirmation and a deletion entry point.

Do not only write memory without retrieving it. Long-term memory affects model answers only when it is retrieved and injected before later requests.
