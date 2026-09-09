# xllm Performance and Release Notes

This page explains how to think about xllm performance, context budgets, memory retrieval, tool loop overhead, and release gate reports.

[Back to Documentation Center](README.en.md)

## Define Performance Goals First

LLM application performance is not only "how fast the request is." You should track:

- First-token latency.
- Total response time.
- Input token count.
- Output token count.
- Tool call rounds.
- RAG retrieval time.
- Memory ingest/sync time.
- Release bundle compile and downstream verification time.

Different scenarios have different goals: chat UI cares more about first-token latency; batch processing cares more about throughput; AI IDEs care about the full loop time across tools and RAG.

## Streaming Output

Streaming output does not reduce total model compute, but it improves perceived latency.

Recommendations:

- Use `XLLM_STREAM_PREFER` for chat UI.
- Use `XLLM_STREAM_REQUIRE` only when real-time output is mandatory.
- Fall back to non-streaming for models that do not support stream.
- Do not do expensive work inside the event callback.
- Returning `false` from the callback cancels the request.

Continue with: [Request / Response Introduction](guide/request-response-intro.en.md)

## Context Packing

Larger context usually means higher cost and higher latency. RAG and session history both need budgets.

Recommendations:

- Set `uMaxHits` for memory injection.
- Set `uMaxCharsPerHit` and `uMaxTotalChars`.
- Deduplicate multiple hits from the same record.
- Set `uKeepRecentTurns` for session.
- Enable compact for long conversations.
- Keep only the tool result content needed for the answer.

Continue with: [Context Packing Introduction](guide/context-packing-intro.en.md)

## Memory Ingest Performance

Memory ingest time comes from:

- File scanning.
- File reading.
- Chunking.
- Embedding or sparse indexing.
- SQLite writes.

Recommendations:

- Set `uMaxFileBytes`.
- Filter build directories, dependency directories, and binary files.
- For long-running workspaces, use sync or watcher instead of full ingest every time.
- Process large workspaces in batches.
- Use `bSkipUnchanged` to avoid duplicate writes.

Continue with: [Workspace Index Introduction](guide/workspace-index-intro.en.md)

## Memory Search Performance

Search cost comes from candidate retrieval, ranking, filtering, and context rendering.

Recommendations:

- Set `uMaxHits`.
- Set scope to avoid unnecessary `ANY` searches.
- Use metadata/source URI filters to narrow the search space.
- Cache repeated UI queries at the application layer.
- Use `xllm_memory_search_debug` to analyze low-quality retrieval instead of blindly increasing hit count.

## Tool Loop Performance

Tool loops add model rounds. One tool call often means at least one additional model request.

Recommendations:

- Keep tool results short.
- Set timeouts for slow tools.
- Parallelize host operations inside the executor when appropriate, but return clear results to the model.
- Limit maximum tool rounds.
- Run deterministic retrieval before the model call when the model does not need to decide.

Continue with: [Tool Loop Agent](case/tool-loop-agent.en.md)

## Session Compact Cost

Compact can reduce future context cost, but compact itself may have a cost.

| Strategy | Cost | Use Case |
| --- | --- | --- |
| truncate | Low | Learning, simple chat, discardable old history. |
| summarize | Medium to high | Long conversations where old history meaning should be preserved. |
| custom | Depends on implementation | You have your own summary or compression strategy. |

Start with truncate, then add summarize for long conversations.

## Diagnostics Overhead

Logs and trace have runtime and storage costs.

Recommendations:

- Enable detailed trace during development.
- In production, record error codes, status codes, and request IDs by default.
- Do not record large request/response bodies by default.
- User-exported diagnostic bundles must be redacted.

## Release Gate Reports

Release gate is not a performance benchmark, but it proves release package quality.

Watch:

- Whether `verify-version` passed.
- Whether `singlehead` passed.
- Whether `release-bundle -VerifyCompile` passed.
- Whether `verify-artifact` passed checksum and metadata checks.
- Whether `downstream-smoke` consumed the package in a clean environment.

Continue with: [Release Gate Introduction](guide/release-gate-intro.en.md)

## When to Benchmark

Create benchmarks when you need to:

- Compare providers or models.
- Compare memory schemes.
- Tune chunk size and retrieval count.
- Optimize first-index time for an AI IDE.
- Evaluate maximum tool loop rounds.
- Verify that a release did not significantly regress performance.

Benchmarks should record input size, model, profile, network conditions, memory scheme, SQLite path, stream mode, hit count, and tool rounds.

## Common Performance Issues

### Slow Response

Check first:

- Is input context too large?
- Did a tool loop trigger?
- Is summarize compact enabled?
- Is provider network slow?
- Is a large trace body being recorded?

### Poor RAG Answer

Check first:

- Did you index too many unrelated files?
- Is the query too broad?
- Is `uMaxHits` too low or too high?
- Are source URI and context label missing?
- Do you need metadata filters?

### Slow Workspace Indexing

Check first:

- Did you filter `build/`, dependency directories, and hidden files?
- Is `uMaxFileBytes` too large?
- Are you doing full ingest every time?
- Should you use `sync_workspace` or watcher?

## Suggested Defaults

These are starting points. Tune them for your model and data.

| Scenario | Starting Point |
| --- | --- |
| Normal chat | `XLLM_STREAM_PREFER` |
| Session | Keep the latest 4 to 8 turns. |
| Workspace RAG | 3 to 6 hits, 4000 to 8000 characters total. |
| Conversation memory | 1 to 3 hits, 512 to 1200 characters each. |
| Tool loop | Maximum 3 to 5 rounds. |
| Workspace files | Set explicit `uMaxFileBytes`, filter build and dependency directories. |

## Next Steps

- Learn context budgets: [Context Packing Introduction](guide/context-packing-intro.en.md)
- Learn workspace indexing: [Workspace Index Introduction](guide/workspace-index-intro.en.md)
- Learn release verification: [Release Bundle Consumption Case](case/release-bundle-consumption.en.md)
