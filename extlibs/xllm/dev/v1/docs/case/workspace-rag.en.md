# Workspace RAG with Memory

This case shows how to write workspace files into `xllm_memory`, retrieve relevant fragments by question, and inject cited context into one model request.

[Back to Case Studies](README.en.md) | [Workspace Index Introduction](../guide/workspace-index-intro.en.md) | [Memory Workspace API](../api/api-memory-workspace.en.md)

## Problem

AI IDEs, code assistants, and documentation Q&A systems all need to answer "what is in the current project". Passing the entire repository to the model is not practical: there are too many files, too much noise, and possibly sensitive information.

Workspace RAG works like this:

1. Filter and index the workspace first.
2. Retrieve only relevant fragments when the user asks a question.
3. Inject a small number of fragments as context blocks.
4. Let the model answer based on those fragments.

The complete example is:

```text
examples/ai_ide_memory/ai_ide_memory.c
```

## Architecture

```text
workspace files
  -> ingest_workspace
  -> memory records/chunks
user question
  -> memory search
  -> context block with source/chunk/byte range
  -> xllm request
```

This case demonstrates memory and request injection only. It does not depend on a real provider. You can pass the generated `xllm_request` to any profile that supports text input.

## Step 1: Prepare the Memory Store

```c
xllm_memory_options tMemoryOptions;

xllm_memory_options_init(&tMemoryOptions);
tMemoryOptions.sNamespace = "example-ai-ide-memory";
tMemoryOptions.sSqlitePath = "build\\ai_ide_memory.db";
tMemoryOptions.eScheme = XLLM_MEMORY_SCHEME_BUILTIN_SPARSE;
tMemoryOptions.bEnableHybridSearch = false;
tMemoryOptions.uDefaultChunkChars = 512u;

xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
```

`XLLM_MEMORY_SCHEME_BUILTIN_SPARSE` is simple enough while learning. In production, configure embedder and vector-related options if you need embeddings or hybrid search.

## Step 2: Index the Workspace

```c
xllm_memory_ingest_workspace_options tWorkspace;
xllm_memory_ingest_directory_result tResult;

xllm_memory_ingest_workspace_options_init(&tWorkspace);
xllm_memory_ingest_directory_result_init(&tResult);

tWorkspace.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
tWorkspace.sPath = "build\\ai_ide_memory_workspace";
tWorkspace.bSkipHidden = true;
tWorkspace.sRecordIdPrefix = "workspace";
tWorkspace.sSourceUriPrefix = "workspace://";
tWorkspace.uMaxFileBytes = 4096u;
tWorkspace.uChunkChars = 512u;

xllm_memory_ingest_workspace(pMemory, &tWorkspace, &tResult, &tError);
```

The example workspace contains:

- `src/memory_bridge.c`
- `docs/architecture.md`
- `.env`

Because `bSkipHidden` and the default workspace filters are enabled, `.env` should not enter the index. This is an important safety boundary in AI IDE scenarios.

## Step 3: Check Index Results

```c
printf("ingested=%u skipped=%u failed=%u\n",
       tResult.uIngestedFileCount,
       tResult.uSkippedFileCount,
       tResult.uFailedFileCount);
```

Watch these fields:

| Field | Meaning |
| --- | --- |
| `uVisitedFileCount` | Number of files scanned |
| `uIngestedFileCount` | Number of files successfully written into memory |
| `uSkippedFileCount` | Number of files skipped by rules |
| `uFailedFileCount` | Number of files that failed to read or ingest |

`skipped` is not necessarily bad; it usually means filters are working. `failed` should ideally be 0.

## Step 4: Retrieve Relevant Fragments

```c
xllm_memory_search_options tSearch;
xllm_memory_search_result tSearchResult;

memset(&tSearchResult, 0, sizeof(tSearchResult));
xllm_memory_search_options_init(&tSearch);

tSearch.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
tSearch.sQuery =
    "How should the AI IDE inject workspace memory before provider execution?";
tSearch.uMaxHits = 2u;
tSearch.uMaxCharsPerHit = 512u;

xllm_memory_search(pMemory, &tSearch, &tSearchResult, &tError);
```

Each hit includes source information:

- `sRecordId`
- `sSourceUri`
- `sChunkId`
- `iStartByte`
- `iEndByte`
- `sText`

These fields can be used for UI citations, logs, jumping to file locations, and debugging retrieval quality.

## Step 5: Look Up Chunk Details

The example also shows how to look up a chunk by the top hit's `sChunkId`:

```c
xllm_memory_list_options tList;
xllm_memory_chunk_list_result tChunkList;

xllm_memory_list_options_init(&tList);
memset(&tChunkList, 0, sizeof(tChunkList));

tList.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
tList.sChunkId = tSearchResult.pHits[0].sChunkId;

xllm_memory_list_chunks(pMemory, &tList, &tChunkList, &tError);
```

This is useful for IDE UI. When the user clicks a citation, you can open the matching file fragment based on source URI and byte range.

## Step 6: Inject Request Context

```c
xllm_request tRequest;
xllm_memory_context_options tContext;

xllm_request_init(&tRequest);
xllm_memory_context_options_init(&tContext);

tContext.sLabel = "AI IDE workspace context:";
tContext.eKindOverride = XLLM_CONTEXT_MEMORY;
tContext.uMaxHits = 2u;
tContext.uMaxCharsPerHit = 256u;

xllm_memory_search_and_apply_to_request(
    pMemory,
    &tSearch,
    &tRequest,
    &tContext,
    &tError
);
```

After injection, `tRequest.iContextBlockCount` should be greater than 0. The example also checks that the injected text contains:

- The context label.
- `Source: workspace://...`
- `Chunk: ...`
- `bytes=...`
- No `OPENAI_API_KEY` from `.env`.

This means the RAG context includes citations and does not inject sensitive hidden files into the model.

## Step 7: Send to the Model

This case generates an `xllm_request`. Before actually sending, add the current user message:

```c
tRequest.sProfileId = "fast-text";

/* In a real project, build an XLLM_ROLE_USER message through the Request / Response API.
   If you use the turn/session path, use xllm_turn_add_user_text instead. */
xllm_chat_ex(pRuntime, &tRequest, &tCallOptions, &pResponse, &tError);
```

If you use session/turn, use `xllm_memory_search_and_apply_to_turn` or `xllm_memory_search_and_apply_from_turn_to_turn`.

## Key APIs

| API | Purpose |
| --- | --- |
| `xllm_memory_create` | Create a memory store |
| `xllm_memory_ingest_workspace` | Scan and ingest workspace files |
| `xllm_memory_search` | Retrieve relevant fragments |
| `xllm_memory_list_chunks` | List chunk details by chunk conditions |
| `xllm_memory_search_and_apply_to_request` | Retrieve and inject into a request |
| `xllm_request_reset` | Release context blocks inside a request |

## Extension Points

You can add:

- `xllm_memory_sync_workspace`: continuously sync file adds, updates, and deletes.
- Watcher worker: batch filesystem events into memory.
- Metadata filters: filter by language, directory, or project module.
- Citations UI: show source URI and byte range as clickable references.
- Sensitive rules: add filters for `.env`, certificates, keys, and build outputs.

## Common Questions

Do not ingest the whole repository without filtering. Exclude hidden files, build outputs, dependency directories, and large files first.

Do not ignore source URI. Without source information, model answers are hard to trace.

Do not inject overly long RAG content. Set `uMaxHits`, `uMaxCharsPerHit`, and `uMaxTotalChars`.

Do not treat workspace RAG as a precise semantic parser. It is good at finding relevant fragments; exact symbol navigation should still be handled by language services.
