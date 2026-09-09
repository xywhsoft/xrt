# Memory RAG Introduction

Memory RAG is the flow of writing external knowledge into `xllm_memory`, retrieving relevant fragments for a user question, and injecting those fragments into the model context.

[Back to Tutorials](README.en.md) | [Memory API](../api/api-memory.en.md) | [Memory Search API](../api/api-memory-search.en.md)

## What You Will Learn

This guide walks you through a minimal RAG flow:

- Create a memory store.
- Ingest one piece of knowledge text.
- Retrieve matching fragments with a natural-language query.
- Convert retrieval results into context blocks.
- Put context blocks into a request or turn so the model can answer with that knowledge.

## Difference Between Memory and Session

A session stores the current conversation history and is suitable for short-term context. Memory stores searchable knowledge and is suitable for long-term use across turns and sessions.

| Requirement | Use |
| --- | --- |
| What the user just said | `xllm_session` |
| Project docs, code notes, product knowledge base | `xllm_memory` |
| Long-term facts, preferences, and tasks from the current conversation | `xllm_memory_ingest_fact`, `xllm_memory_ingest_preference`, `xllm_memory_ingest_task` |
| Automatically attach relevant knowledge before each turn | memory search + apply |

A common chat system uses both: session manages short-term chat history, memory manages long-term knowledge, and before each user turn enters the session, you search memory and inject relevant fragments into the turn.

## Minimal RAG Flow

The minimal flow is:

1. Create `xllm_runtime`.
2. Initialize `xllm_memory_options`.
3. Call `xllm_memory_create`.
4. Ingest knowledge with `xllm_memory_ingest_text`.
5. Search knowledge with `xllm_memory_search`.
6. Inject context with `xllm_memory_apply_search_to_request` or `xllm_memory_apply_search_to_turn`.
7. Call the model.
8. Reset the search result, then destroy memory and runtime.

## Creating a Memory Store

```c
xllm_runtime *pRuntime = NULL;
xllm_memory *pMemory = NULL;
xllm_memory_options tMemoryOptions;
xllm_error tError;

xllm_error_init(&tError);
xllm_memory_options_init(&tMemoryOptions);

if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK ) {
    return 1;
}

tMemoryOptions.sNamespace = "demo";
tMemoryOptions.eScheme = XLLM_MEMORY_SCHEME_BUILTIN_SPARSE;
tMemoryOptions.sSqlitePath = "demo-memory.sqlite";

if ( xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory) != XRT_NET_OK ) {
    fprintf(stderr, "memory create failed: %s\n",
            tError.sMessage ? tError.sMessage : "(null)");
    return 1;
}
```

If `sSqlitePath` is `NULL`, the implementation may use temporary or in-memory storage, which is suitable for smoke tests and learning. In real applications, set a SQLite path so knowledge survives process restarts.

`eScheme` decides the retrieval scheme. Start with `XLLM_MEMORY_SCHEME_BUILTIN_SPARSE` while learning; it does not require ONNX embedding assets.

## Ingesting Knowledge

`xllm_memory_ingest_text` is suitable for ingesting text that is already in memory.

```c
xllm_memory_ingest_options tIngest;

xllm_memory_ingest_options_init(&tIngest);
tIngest.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
tIngest.sRecordId = "memory-design";
tIngest.sTitle = "memory design";
tIngest.sSourceUri = "workspace://memory_design";
tIngest.sText =
    "xllm-memory should provide long-term memory, knowledge ingestion, "
    "chunking, retrieval, and a bridge that turns retrieved chunks into "
    "context blocks.";
tIngest.bReplaceExisting = true;

if ( xllm_memory_ingest_text(pMemory, &tIngest, &tError) != XRT_NET_OK ) {
    fprintf(stderr, "ingest failed: %s\n",
            tError.sMessage ? tError.sMessage : "(null)");
}
```

Several fields are worth using consistently:

| Field | Recommendation |
| --- | --- |
| `eScope` | Use `XLLM_MEMORY_SCOPE_KNOWLEDGE` for docs, code, and knowledge bases; use `XLLM_MEMORY_SCOPE_MEMORY` for personal memory |
| `sRecordId` | Use a stable ID so records can be replaced or deleted later |
| `sTitle` | A human-readable title for people and debugging tools |
| `sSourceUri` | Record the source clearly, such as `workspace://...`, `file://...`, or `conversation://...` |
| `bReplaceExisting` | Set to `true` when repeatedly updating the same record |

## Searching Knowledge

Initialize `xllm_memory_search_options`, then set the query text and hit count.

```c
xllm_memory_search_options tSearch;
xllm_memory_search_result tResult;

memset(&tResult, 0, sizeof(tResult));
xllm_memory_search_options_init(&tSearch);

tSearch.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
tSearch.sQuery = "How should xllm memory support retrieval and context blocks?";
tSearch.uMaxHits = 3u;

if ( xllm_memory_search(pMemory, &tSearch, &tResult, &tError) == XRT_NET_OK ) {
    for ( size_t i = 0u; i < tResult.iHitCount; ++i ) {
        const xllm_memory_hit *pHit = &tResult.pHits[i];
        printf("[%u] score=%.3f title=%s\n",
               (unsigned)(i + 1u),
               pHit->fScore,
               pHit->sTitle ? pHit->sTitle : "(null)");
        printf("%s\n", pHit->sText ? pHit->sText : "");
    }
}

xllm_memory_search_result_reset(&tResult);
```

The search result is filled by `xllm_memory_search`. After use, call `xllm_memory_search_result_reset`. String pointers inside the result belong to the result object; do not keep them long term.

## Injecting Search Results into a Request

Searching alone is not enough. You still need to pass the matching fragments to the model. `xllm_memory_apply_search_to_request` converts results into `xllm_context_block`:

```c
xllm_request tRequest;
xllm_memory_context_options tContext;

xllm_request_init(&tRequest);
xllm_memory_context_options_init(&tContext);

tContext.sLabel = "Retrieved project knowledge";
tContext.uMaxHits = 3u;
tContext.uMaxTotalChars = 4000u;
tContext.bDistinctByRecord = true;

if ( xllm_memory_apply_search_to_request(
        &tRequest,
        &tResult,
        &tContext,
        &tError
    ) != XRT_NET_OK ) {
    fprintf(stderr, "apply failed: %s\n",
            tError.sMessage ? tError.sMessage : "(null)");
}
```

After that, add the user message to `tRequest` and call `xllm_chat_ex`. If you use session/turn, call `xllm_memory_apply_search_to_turn`:

```c
xllm_turn tTurn;
xllm_turn_init(&tTurn);
xllm_turn_add_user_text(&tTurn, "Please explain the retrieval flow in xllm memory.");

xllm_memory_apply_search_to_turn(&tTurn, &tResult, &tContext, &tError);
xllm_session_chat_ex(pSession, &tTurn, &tCallOptions, &pResponse, &tError);
```

## Automatically Extracting a Query from a User Turn

In many chat scenarios, you do not want to manually copy the user's question into `tSearch.sQuery`. Use `xllm_memory_search_and_apply_from_turn_to_turn` to let xllm extract the query text from a source turn and inject retrieval results into the target turn:

```c
xllm_memory_turn_search_apply_options tApply;

xllm_memory_turn_search_apply_options_init(&tApply);
tApply.tSearchOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
tApply.tSearchOptions.uMaxHits = 4u;
tApply.tContextOptions.sLabel = "Relevant knowledge";
tApply.tContextOptions.uMaxTotalChars = 4000u;
tApply.eQueryMode = XLLM_MEMORY_TURN_QUERY_LAST_USER_TEXT;

xllm_memory_search_and_apply_from_turn_to_turn(
    pMemory,
    &tTurn,
    &tTurn,
    &tApply,
    &tError
);
```

This style fits chat programs that perform automatic RAG for every user turn. You should still limit `uMaxHits` and `uMaxTotalChars` to avoid stuffing too much retrieved content into the context.

## Choosing a Scope

`xllm_memory_scope` distinguishes different kinds of memory:

| Scope | Usage |
| --- | --- |
| `XLLM_MEMORY_SCOPE_KNOWLEDGE` | Project docs, workspace code, product knowledge, static material |
| `XLLM_MEMORY_SCOPE_MEMORY` | User preferences, extracted conversation facts, task state |
| `XLLM_MEMORY_SCOPE_ANY` | Search or statistics across both scopes |

When learning RAG, start with `KNOWLEDGE`. Introduce `MEMORY` when you begin to remember user preferences or extract tasks from conversations.

## Controlling Context Budget

A common RAG problem is not "no results", but "too many results". These fields help control injected content:

| Field | Purpose |
| --- | --- |
| `xllm_memory_search_options.uMaxHits` | Maximum number of hits returned by search |
| `xllm_memory_search_options.uMaxCharsPerHit` | Maximum characters kept per hit during search |
| `xllm_memory_context_options.uMaxHits` | Maximum hits used when injecting context |
| `xllm_memory_context_options.uMaxCharsPerHit` | Maximum characters per injected hit |
| `xllm_memory_context_options.uMaxTotalChars` | Total character budget for this injection |
| `xllm_memory_context_options.bDistinctByRecord` | Prevent multiple chunks from the same document from filling all results |

In real applications, start with a small budget, such as 3 to 6 hits and 3000 to 8000 characters total. Then tune based on answer quality and the model's context window.

## Cleaning Up Resources

Minimal cleanup order:

```c
xllm_request_reset(&tRequest);
xllm_memory_search_result_reset(&tResult);
xllm_memory_destroy(pMemory);
xllm_runtime_destroy(pRuntime);
xllm_error_free(&tError);
```

If you use list, debug, workspace ingest, or other result objects, also call the matching `*_reset`. Many Memory API result objects contain internal strings and arrays; skipping reset leaks memory.

## Common Mistakes

Do not inject all retrieval results without a budget. More RAG content is not always better; noise can reduce model judgment quality.

Do not assign a random `sRecordId` every time you ingest the same file. That creates duplicate records, and search results may be polluted by older versions. For workspace files, derive the record ID from a stable path or source URI.

Do not confuse `MEMORY` and `KNOWLEDGE`. Project docs usually belong in `KNOWLEDGE`; user preferences and conversation facts usually belong in `MEMORY`.

Do not keep pointers from `xllm_memory_hit` long term. After the search result is reset, those pointers are no longer valid.

Do not ingest secret files directly into memory. When indexing a workspace, use ignore rules to skip keys, build outputs, and large binary files.

## Next Steps

- To index directories and code workspaces, continue with [Workspace Index Introduction](workspace-index-intro.en.md).
- To turn conversation facts, preferences, and tasks into long-term memory, continue with [Conversation Memory Introduction](conversation-memory-intro.en.md).
- To inspect every search and injection API field, read [Memory Search API](../api/api-memory-search.en.md).
