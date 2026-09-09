# Context Packing Introduction

Context packing is the process of deciding what to keep when system prompts, history, tool results, memory retrieval results, and user input are placed into the model context.

[Back to Tutorials](README.en.md) | [Session API](../api/api-session.en.md) | [Memory Search API](../api/api-memory-search.en.md)

## What You Will Learn

This guide explains:

- Why you should not put everything into context.
- How to set hit count, character budget, deduplication, and score thresholds for memory retrieval results.
- How to use session compacting to control conversation history.
- How to prevent retrieved context from overriding the user's current goal.

## Why Context Packing Is Needed

The model context window is limited. Even if a model supports a long context, that does not mean you should put all history and all retrieval results into it. Too much context causes three problems:

- Higher cost.
- Key information is buried by noise.
- Old goals, old tool results, or low-relevance fragments affect model judgment.

Therefore, give different content different priority:

| Content | Usual Priority |
| --- | --- |
| Current user input | Highest |
| System prompt and safety boundaries | Very high, usually pinned |
| Current tool-chain results | High |
| Recent conversation turns | High |
| Highly relevant memory hits | Medium-high |
| Old history and low-relevance hits | Low, can be summarized or dropped |

## Controlling Memory Injection Budget

Memory RAG packing is mainly controlled by `xllm_memory_search_options` and `xllm_memory_context_options`.

```c
xllm_memory_search_options tSearch;
xllm_memory_context_options tContext;

xllm_memory_search_options_init(&tSearch);
xllm_memory_context_options_init(&tContext);

tSearch.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
tSearch.sQuery = "How does the session compact API work?";
tSearch.uMaxHits = 8u;
tSearch.uMaxCharsPerHit = 1200u;
tSearch.tMinScore.bSet = true;
tSearch.tMinScore.fValue = 0.15;

tContext.sLabel = "Relevant project knowledge";
tContext.uMaxHits = 4u;
tContext.uMaxCharsPerHit = 900u;
tContext.uMaxTotalChars = 3000u;
tContext.bDistinctByRecord = true;
tContext.iPriority = 20;
```

A useful way to think about it:

- Search options decide "what may be retrieved at most".
- Context options decide "what finally enters the context".

You can search a little more broadly, then apply a stricter budget during injection.

## Deduplication: Avoid Letting One Document Fill the Context

Long documents are split into multiple chunks. If a query matches several adjacent chunks from the same document, they may fill the entire result list. Set `bDistinctByRecord` to give different records a chance to enter the context:

```c
tContext.bDistinctByRecord = true;
```

Good cases for enabling deduplication:

- The project has many small documents, and the answer should combine several sources.
- Search results often come from one large file.
- You only need to know "which document is relevant", not read a long continuous section.

Cases where deduplication may be less suitable:

- The user explicitly asks about details in one long document.
- You need to preserve continuous context.

## Score Thresholds

`tMinScore` filters low-relevance hits:

```c
tSearch.tMinScore.bSet = true;
tSearch.tMinScore.fValue = 0.20;
```

Different retrieval schemes may use different score scales, so do not start with an overly high threshold. Recommended flow:

1. First, do not set a threshold; print hits and scores.
2. Observe score distributions for normal questions and unrelated questions.
3. Set a conservative threshold.
4. For fallback cases below the threshold, use a smaller budget instead of dropping everything completely.

## Labels and Priority

`xllm_memory_context_options.sLabel` appears in the injected text and helps the model understand what this content is:

```c
tContext.sLabel = "Relevant project knowledge";
```

Labels should be short and clear. Common labels:

- `Relevant project knowledge`
- `Conversation memory`
- `Retrieved documentation`
- `Tool result context`

`iPriority` indicates the relative priority of the context block. System prompts and current user input are usually more important. Memory retrieval results should be useful, but should not override the user's current goal.

## Session Compacting Controls History

Conversation-history packing is managed by session compacting. Common configuration:

```c
xllm_session_options tOptions;

xllm_session_options_init(&tOptions);
tOptions.sProfileId = "main";
tOptions.sSystemPrompt = "You are a concise assistant.";
tOptions.bEnableAutoCompact = true;
tOptions.uKeepRecentTurns = 4u;
tOptions.uReserveOutputTokens = 1024u;
tOptions.eCompactStrategy = XLLM_COMPACT_TRUNCATE;
```

`uKeepRecentTurns` is important. It tells the session to try to keep the most recent raw turns when compacting; older turns can be truncated or summarized. `examples/smoke_session_compact.c` demonstrates that the most recent user messages are still retained after compacting.

You can also compact manually:

```c
xllm_compact_options tCompact;
xllm_compact_result tResult;

xllm_compact_options_init(&tCompact);
tCompact.eMode = XLLM_COMPACT_TO_FIT_CURRENT_MODEL;
tCompact.eStrategy = XLLM_COMPACT_TRUNCATE;

xllm_session_compact(pSession, &tCompact, &tResult);
```

If `tResult.bCompacted` is `true`, history has been compressed or trimmed.

## Recommended Packing Order

When constructing a request turn, think in this order:

1. System prompt: clear, short, stable.
2. Current user input: keep it complete.
3. Current tool-chain results: keep only what is needed for the answer.
4. Recent conversation history: stored by session.
5. Conversation memory: small amount, highly relevant.
6. Workspace knowledge: small amount, highly relevant, deduplicated.
7. Old history: compact or summarize.

If context is insufficient, reduce old history and low-relevance memory before removing current user input.

## Avoid Overriding the User's Goal

RAG content can easily pull the model off track. For example, if the user asks "please refactor this function", and retrieval results contain an old design goal, the model may explain the old goal instead of performing the refactor.

Use these methods to reduce risk:

- Label retrieved blocks and make clear that they are reference material.
- State in the system prompt that the current user goal has priority over retrieved context.
- Limit total retrieved characters.
- Use a more specific query; do not use the entire chat history as the query.
- Deduplicate memory hits and use score thresholds.

## Common Budget Starting Points

These are not fixed rules, but they are useful starting values:

| Scenario | Suggestion |
| --- | --- |
| Small model, normal chat | 2 to 4 memory hits, 2000 to 4000 characters total |
| Project Q&A | 4 to 8 knowledge hits, 4000 to 12000 characters total |
| Code assistant | Recent turns + current file fragment + 3 to 6 retrieval results |
| Long conversation | Keep the most recent 4 to 8 turns, summarize old history |

When tuning, look at answer quality first, then token cost. Do not optimize only for hit count.

## Common Mistakes

Do not let memory search results grow without limit. `uMaxHits` and `uMaxTotalChars` should always have explicit upper bounds.

Do not mark everything as pinned. When too much content is pinned, compacting has no room to make tradeoffs.

Do not keep old tool results in high-priority context long term. Tool results are usually useful only for the current task.

Do not use very broad questions as retrieval queries. For example, "help me look at this" usually cannot retrieve good results. Prefer the current explicit user goal or a query generated by the application.

## Next Steps

- To learn workspace indexing, read [Workspace Index Introduction](workspace-index-intro.en.md).
- To learn long-term conversation memory, read [Conversation Memory Introduction](conversation-memory-intro.en.md).
- To see compact API details, read [Session API](../api/api-session.en.md).
