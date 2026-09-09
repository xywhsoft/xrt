# Conversation Memory Introduction

Conversation memory writes facts, preferences, tasks, or summaries worth keeping from a conversation into `xllm_memory`, so later sessions can retrieve them.

[Back to Tutorials](README.en.md) | [Memory RAG Introduction](memory-rag-intro.en.md) | [Memory Ingest API](../api/api-memory-ingest.en.md)

## What You Will Learn

This guide explains:

- Why long-term memory needs explicit ingestion.
- What summary, task, fact, and preference are suitable for.
- How to retrieve conversation memory before the next turn.
- How to control user approval, source URI, and replacement updates.

## Why Explicit Ingestion Is Needed

A session automatically saves the current conversation history, but it is not long-term memory. Long-term memory should have clear source, type, and lifecycle. In other words, the application should decide at the right time that "this information is worth saving", then call a memory ingest API.

A careful flow is:

1. The user or system provides information that may be worth saving.
2. The application decides whether user approval is needed.
3. The application organizes the information as a summary, fact, preference, or task.
4. The application writes it into `XLLM_MEMORY_SCOPE_MEMORY`.
5. Before later turns, the application retrieves relevant content from memory and injects it into context.

## Choosing a Memory Type

| Type | API | Suitable For |
| --- | --- | --- |
| Summary | `xllm_memory_ingest_text` or `xllm_memory_ingest_turn_response` | Summary of a conversation section, background notes |
| Task | `xllm_memory_ingest_task` | To-do items, owners, deadlines, status |
| Fact | `xllm_memory_ingest_fact` | Declarative facts such as "project X uses library Y" |
| Preference | `xllm_memory_ingest_preference` | The user's preferred answer style, language, format |

When learning, start with summaries. Use task/fact/preference when you need filtering, updates, expiration, and structured queries.

## Ingesting a Conversation Summary

`examples/conversation_memory/conversation_memory.c` uses the simplest summary ingestion style:

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

Call example:

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

Good habits in this example:

- Use `XLLM_MEMORY_SCOPE_MEMORY` for `eScope`, because this is long-term memory rather than project knowledge.
- Include the conversation/thread ID in `sRecordId`, so it can be replaced or deleted later.
- Point `sSourceUri` to the memory source for auditing.
- Set `bReplaceExisting = true`, so the same summary can be updated.

## Retrieving Memory Before the Next Turn

When the user asks another question, search memory first, then inject it into the request:

```c
xllm_memory_search_options tSearch;
xllm_memory_context_options tContext;
xllm_request tRequest;

xllm_memory_search_options_init(&tSearch);
xllm_memory_context_options_init(&tContext);
xllm_request_init(&tRequest);

tSearch.eScope = XLLM_MEMORY_SCOPE_MEMORY;
tSearch.sQuery = "What should I remember about user answer style?";
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

The request now contains one additional context block. Add the current user message and send it to the model; the model can then see the relevant long-term memory.

## Ingesting a Fact

Facts are suitable for structured statements. They usually have subject, predicate, and object:

```c
xllm_memory_ingest_fact_options tFact;

xllm_memory_ingest_fact_options_init(&tFact);
tFact.eScope = XLLM_MEMORY_SCOPE_MEMORY;
tFact.sFactId = "project-xllm-language";
tFact.sSubject = "xllm";
tFact.sPredicate = "primary_language";
tFact.sObject = "C";
tFact.sRecordId = "fact:project-xllm-language";
tFact.sTitle = "Project language";
tFact.sSourceUri = "memory://conversation/thread-001/fact/project-language";
tFact.bReplaceExisting = true;
tFact.iPriority = 10;

xllm_memory_ingest_fact(pMemory, &tFact, &tError);
```

When you later want to filter by project, user, conversation, or metadata, structured facts are easier to maintain than plain summaries.

## Ingesting a Preference

Preferences are suitable for "how the user wants things done in the future":

```c
xllm_memory_ingest_preference_options tPreference;

xllm_memory_ingest_preference_options_init(&tPreference);
tPreference.eScope = XLLM_MEMORY_SCOPE_MEMORY;
tPreference.sPreferenceId = "user-answer-style";
tPreference.sSubject = "user";
tPreference.sKey = "answer_style";
tPreference.sValue = "concise Chinese explanations";
tPreference.sRecordId = "preference:user-answer-style";
tPreference.sTitle = "User answer style";
tPreference.sSourceUri = "memory://conversation/thread-001/preference/answer-style";
tPreference.bReplaceExisting = true;
tPreference.iPriority = 20;

xllm_memory_ingest_preference(pMemory, &tPreference, &tError);
```

Preferences should usually require explicit user consent, especially when they involve personal information, work habits, or long-term profiling.

## Ingesting a Task

Tasks are suitable for to-do items:

```c
xllm_memory_ingest_task_options tTask;

xllm_memory_ingest_task_options_init(&tTask);
tTask.eScope = XLLM_MEMORY_SCOPE_MEMORY;
tTask.sTaskId = "review-docs-api";
tTask.eStatus = XLLM_MEMORY_TASK_STATUS_OPEN;
tTask.sOwner = "user";
tTask.sRecordId = "task:review-docs-api";
tTask.sTitle = "Review xllm API docs";
tTask.sSourceUri = "memory://conversation/thread-001/task/review-docs-api";
tTask.sText = "User plans to review the Chinese xllm API documentation before English translation.";
tTask.iPriority = 30;

xllm_memory_ingest_task(pMemory, &tTask, &tError);
```

Tasks can later update status by conversation, metadata, or record ID. They can also have deadlines and expiration times.

## Extracting Memory from Turn/Response

If you want to use a turn and model response as the source, use `xllm_memory_ingest_turn_response`:

```c
xllm_memory_ingest_turn_response_options tTurnMemory;

xllm_memory_ingest_turn_response_options_init(&tTurnMemory);
tTurnMemory.eScope = XLLM_MEMORY_SCOPE_MEMORY;
tTurnMemory.eExtractionPolicy = XLLM_MEMORY_EXTRACTION_POLICY_SUMMARY;
tTurnMemory.pTurn = &tTurn;
tTurnMemory.pResponse = pResponse;
tTurnMemory.sConversationId = "thread-001";
tTurnMemory.sTurnId = "turn-002";
tTurnMemory.sRecordId = "thread-001-turn-002-summary";
tTurnMemory.sTitle = "Turn summary";
tTurnMemory.sSourceUri = "memory://conversation/thread-001/turn-002";
tTurnMemory.bReplaceExisting = true;

xllm_memory_ingest_turn_response(pMemory, &tTurnMemory, &tError);
```

`eExtractionPolicy` indicates how you want to process the turn content. In real applications, it is better to let the application or model generate a reviewable summary first, then write it into memory.

## User Approval and Privacy

Long-term memory affects future answers, so handle it more carefully than ordinary context:

- Clearly tell users what will be remembered.
- Require confirmation for preferences and personal information.
- Write `sSourceUri` for every memory item so users can trace it.
- Provide delete and cleanup entry points.
- Set `iExpiresAtUnix` for expiring information.

## Common Mistakes

Do not write every sentence into long-term memory. That creates a large amount of noise and reduces retrieval quality.

Do not write only summaries without sources. Without `sConversationId`, `sTurnId`, or `sSourceUri`, it is hard to explain where a memory came from later.

Do not treat session history as memory. When a session ends, compacts, or changes export/import strategy, short-term history is not the same as governable long-term memory.

Do not save sensitive information long term by default. Long-term memory should have user approval, filtering, and deletion policies.

## Next Steps

- To learn retrieval and injection, read [Memory RAG Introduction](memory-rag-intro.en.md).
- To control injection budget, read [Context Packing Introduction](context-packing-intro.en.md).
- To see a complete scenario, later read [Conversation Memory Case](../case/conversation-memory.en.md).
