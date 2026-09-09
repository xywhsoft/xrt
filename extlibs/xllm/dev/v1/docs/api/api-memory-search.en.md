# xllm Memory Search API

> Header: `xllm-memory.h`

This page explains how to search xllm memory, inspect hits, list records/chunks, generate retrieval debug dumps, and inject retrieval results into `xllm_request` or `xllm_turn`.  
The second half also covers maintenance APIs closely related to retrieval: deleting records, removing expired memory, trimming, and compacting conversation memory.

## What You Will Learn

- How to query memory with `xllm_memory_search`.
- What each field in `xllm_memory_hit` means.
- How to release search, list, and debug result objects.
- How to inject hit content as context blocks into requests.
- How to delete memory by record/source/conversation/metadata.
- How to remove expired memory, trim conversations, and write conversation summaries.

## Minimal Search Flow

```c
xllm_memory_search_options options;
xllm_memory_search_result result;

xllm_memory_search_options_init(&options);
memset(&result, 0, sizeof(result));

options.sQuery = "how to create a memory store";
options.eScope = XLLM_MEMORY_SCOPE_ANY;
options.uMaxHits = 5;

if (xllm_memory_search(memory, &options, &result, NULL) == XRT_NET_OK) {
    for (size_t i = 0; i < result.iHitCount; ++i) {
        printf("%s\n", result.pHits[i].sText);
    }
}

xllm_memory_search_result_reset(&result);
```

Pay attention to the last line: search results contain strings and arrays allocated by xllm, so they must be reset after use.

## Types

### xllm_memory_search_options

**Purpose:** describes one memory search.

```c
typedef struct {
    xllm_memory_scope eScope;
    const char *sQuery;
    const char *sConversationId;
    const char *sTurnId;
    const char *sRecordId;
    const char *sSourceUri;
    const char *sRecordIdContains;
    const char *sTitleContains;
    const char *sSourceUriContains;
    const char *sTextContains;
    const char *sMetadataKey;
    const char *sMetadataValue;
    uint32 uMaxHits;
    xllm_opt_f64 tMinScore;
    xllm_opt_f64 tPriorityWeight;
    xllm_opt_f64 tRecencyWeight;
    bool bSkipExpired;
    int64 iNowUnix;
    uint32 uMaxCharsPerHit;
    xvalue tVendorExtra;
} xllm_memory_search_options;
```

| Field | Meaning |
| --- | --- |
| `eScope` | Search scope. Default is `ANY`. |
| `sQuery` | Query text, usually required. |
| `sConversationId` | Search only memory from one conversation. |
| `sTurnId` | Search only memory from one turn. |
| `sRecordId` | Search only one record. |
| `sSourceUri` | Search only one source URI. |
| `sRecordIdContains` | Record ID contains filter. |
| `sTitleContains` | Title contains filter. |
| `sSourceUriContains` | Source URI contains filter. |
| `sTextContains` | Chunk text contains filter. |
| `sMetadataKey` | Metadata key filter. |
| `sMetadataValue` | Metadata value filter; requires `sMetadataKey`. |
| `uMaxHits` | Maximum hits. Default is `5`. |
| `tMinScore` | Optional minimum score. If unset, implementation defaults are used. |
| `tPriorityWeight` | Optional priority weighting. |
| `tRecencyWeight` | Optional recency weighting. |
| `bSkipExpired` | Whether to skip expired memory. |
| `iNowUnix` | Current Unix time; `0` lets xllm get current time. |
| `uMaxCharsPerHit` | Maximum returned characters per hit. Default is `800`. |
| `tVendorExtra` | Extension field. |

**Notes:** `sQuery` is the text used for scoring; other fields are mostly filters. For example, you can use `sQuery = "user preference"` and `eScope = MEMORY` to search only conversation memory.

### xllm_memory_hit

**Purpose:** represents one retrieved chunk.

```c
struct xllm_memory_hit {
    xllm_memory_scope eScope;
    const char *sRecordId;
    const char *sChunkId;
    const char *sTitle;
    const char *sSourceUri;
    const char *sText;
    double fScore;
    double fLexicalScore;
    double fVectorScore;
    double fRrfScore;
    uint32 uChunkIndex;
    size_t iStartByte;
    size_t iEndByte;
    uint32 uLexicalRank;
    uint32 uVectorRank;
    const char *sRetrievalProfileId;
    const char *sEmbedProfileId;
    const char *sIndexProfileId;
    xvalue tMetadata;
    xvalue tVendorExtra;
};
```

| Field | Meaning |
| --- | --- |
| `eScope` | Whether the hit comes from `MEMORY` or `KNOWLEDGE`. |
| `sRecordId` | Owning record ID. |
| `sChunkId` | Hit chunk ID. |
| `sTitle` | Record title. |
| `sSourceUri` | Record source. |
| `sText` | Hit text. |
| `fScore` | Combined score. |
| `fLexicalScore` | Lexical retrieval score. |
| `fVectorScore` | Vector retrieval score. |
| `fRrfScore` | RRF fusion score. |
| `uChunkIndex` | Chunk index inside the record. |
| `iStartByte` / `iEndByte` | Byte range in original text. |
| `uLexicalRank` / `uVectorRank` | Rank in lexical/vector candidates. |
| `sRetrievalProfileId` | Retrieval profile. |
| `sEmbedProfileId` | Embedding profile. |
| `sIndexProfileId` | Index profile. |
| `tMetadata` | Metadata carried by record or chunk. |
| `tVendorExtra` | Extension field. |

### xllm_memory_search_result

**Purpose:** stores search results.

```c
struct xllm_memory_search_result {
    xllm_memory_hit *pHits;
    size_t iHitCount;
    xvalue tVendorExtra;
};
```

**Cleanup rule:** call `xllm_memory_search_result_reset` after use.

### xllm_memory_list_options

**Purpose:** filtering and paging parameters used when listing records or chunks.

```c
typedef struct {
    xllm_memory_scope eScope;
    const char *sConversationId;
    const char *sTurnId;
    const char *sRecordId;
    const char *sSourceUri;
    const char *sRecordIdContains;
    const char *sChunkId;
    const char *sTitleContains;
    const char *sSourceUriContains;
    const char *sTextContains;
    const char *sMetadataKey;
    const char *sMetadataValue;
    uint32 uOffset;
    uint32 uMaxItems;
    xllm_opt_bool tSortByUpdatedAtDesc;
    bool bSkipExpired;
    int64 iNowUnix;
    uint32 uMaxCharsPerText;
    xvalue tVendorExtra;
} xllm_memory_list_options;
```

**Defaults:** scope is `ANY`, maximum items is `32`, and each text segment is at most `800` characters.

### xllm_memory_record_info

**Purpose:** describes one record.

```c
typedef struct {
    xllm_memory_scope eScope;
    const char *sRecordId;
    const char *sTitle;
    const char *sSourceUri;
    const char *sMemoryProfileId;
    const char *sRetrievalProfileId;
    const char *sEmbedProfileId;
    const char *sIndexProfileId;
    size_t iTextLength;
    uint32 uChunkCount;
    uint32 uProfileVersion;
    xvalue tMetadata;
    xvalue tVendorExtra;
} xllm_memory_record_info;
```

### xllm_memory_chunk_info

**Purpose:** describes one chunk.

```c
typedef struct {
    xllm_memory_scope eScope;
    const char *sRecordId;
    const char *sChunkId;
    const char *sTitle;
    const char *sSourceUri;
    const char *sText;
    const char *sMemoryProfileId;
    const char *sChunkProfileId;
    const char *sRetrievalProfileId;
    const char *sEmbedProfileId;
    const char *sIndexProfileId;
    const char *sPreviousChunkId;
    const char *sNextChunkId;
    size_t iTextLength;
    size_t iStartByte;
    size_t iEndByte;
    uint64 uContentHash;
    uint32 uChunkIndex;
    uint32 uEmbeddingDim;
    uint32 uProfileVersion;
    xvalue tMetadata;
    xvalue tVendorExtra;
} xllm_memory_chunk_info;
```

### xllm_memory_context_options

**Purpose:** controls context blocks generated when retrieval results are injected into a request/turn.

```c
typedef struct {
    xllm_context_block_kind eKindOverride;
    int32 iPriority;
    bool bPinned;
    bool bDistinctByRecord;
    xllm_opt_f64 tMinScore;
    const char *sLabel;
    uint32 uMaxHits;
    uint32 uMaxCharsPerHit;
    uint32 uMaxTotalChars;
    xllm_memory_context_render_fn pfnRender;
    void *pRenderCtx;
    xvalue tVendorExtra;
} xllm_memory_context_options;
```

| Field | Meaning |
| --- | --- |
| `eKindOverride` | Overrides context block type. `0` means derive from scope. |
| `iPriority` | Context block priority. Default is `10`. |
| `bPinned` | Whether the context is pinned. |
| `bDistinctByRecord` | Whether to take at most one hit per record. |
| `tMinScore` | Minimum score for injection. |
| `sLabel` | Context text label. |
| `uMaxHits` | Maximum hits to inject. Default is `4`. |
| `uMaxCharsPerHit` | Maximum characters per hit. Default is `600`. |
| `uMaxTotalChars` | Total injected text limit. `0` means no additional limit. |
| `pfnRender` | Custom render callback. |
| `pRenderCtx` | Render callback context. |

### xllm_memory_turn_query_mode

**Purpose:** chooses which text to use when automatically generating a search query from a turn.

```c
typedef enum {
    XLLM_MEMORY_TURN_QUERY_LAST_USER_TEXT = 0,
    XLLM_MEMORY_TURN_QUERY_ALL_USER_TEXT,
    XLLM_MEMORY_TURN_QUERY_VISIBLE_TEXT
} xllm_memory_turn_query_mode;
```

| Value | Meaning |
| --- | --- |
| `LAST_USER_TEXT` | Use the last user text. Default. |
| `ALL_USER_TEXT` | Use all user text. |
| `VISIBLE_TEXT` | Use visible text, usually broader than user-only text. |

### xllm_memory_turn_search_apply_options

**Purpose:** extracts a query from one turn, searches memory, and injects results into a request or another turn.

```c
typedef struct {
    xllm_memory_search_options tSearchOptions;
    xllm_memory_context_options tContextOptions;
    xllm_memory_turn_query_mode eQueryMode;
    bool bIncludeSystemPrompt;
    bool bIncludeContextBlocks;
    uint32 uMaxQueryChars;
    xvalue tVendorExtra;
} xllm_memory_turn_search_apply_options;
```

**Defaults:** use the last user text as query, and do not include system prompt or existing context blocks.

## API: Search and Cleanup

### xllm_memory_search_options_init

**Purpose:** initializes search options.

```c
XLLM_API void xllm_memory_search_options_init(
    xllm_memory_search_options *pOptions
);
```

**Defaults:** scope is `ANY`, `uMaxHits = 5`, `uMaxCharsPerHit = 800`.

### xllm_memory_search

**Purpose:** performs memory search.

```c
XLLM_API int xllm_memory_search(
    xllm_memory *pMemory,
    const xllm_memory_search_options *pOptions,
    xllm_memory_search_result *pResult,
    xllm_error *pError
);
```

**Parameters:**

| Parameter | Description |
| --- | --- |
| `pMemory` | Memory object. |
| `pOptions` | Search parameters. May be `NULL`, but usually should provide `sQuery`. |
| `pResult` | Output result. |
| `pError` | Error object, may be `NULL`. |

**Return Value:**

| Return Value | Meaning |
| --- | --- |
| `XRT_NET_OK` | Search succeeded; hit count may be `0`. |
| `XRT_NET_ERROR` | Invalid parameter, empty query, incomplete metadata filter, embedding failure, or storage query failure. |

**Notes:**

- Empty `sQuery` fails.
- If `sMetadataValue` is set, `sMetadataKey` must also be set.
- Results are sorted by combined score.
- If memory enables hybrid search and has an embedder, both lexical and vector candidates are used.

### xllm_memory_search_result_reset

**Purpose:** releases a search result.

```c
XLLM_API void xllm_memory_search_result_reset(
    xllm_memory_search_result *pResult
);
```

This releases `pHits`, strings inside hits, metadata references, and zeroes the structure.

## API: Debug and List

### xllm_memory_retrieval_debug_options_init

Initializes retrieval debug options.

```c
XLLM_API void xllm_memory_retrieval_debug_options_init(
    xllm_memory_retrieval_debug_options *pOptions
);
```

Defaults: internal search options use default search parameters, `uMaxCandidates = 32`, and candidates below min score are included.

### xllm_memory_search_debug

Performs an explainable retrieval and outputs candidates, terms, and final results.

```c
XLLM_API int xllm_memory_search_debug(
    xllm_memory *pMemory,
    const xllm_memory_retrieval_debug_options *pOptions,
    xllm_memory_retrieval_debug_dump *pDump,
    xllm_error *pError
);
```

Use this first when you wonder "why was this memory not retrieved". The dump shows candidates, scores, min-score pass/fail, and whether a candidate entered final results.

### xllm_memory_retrieval_debug_dump_reset

Releases a retrieval debug dump.

```c
XLLM_API void xllm_memory_retrieval_debug_dump_reset(
    xllm_memory_retrieval_debug_dump *pDump
);
```

### xllm_memory_list_options_init

Initializes list options.

```c
XLLM_API void xllm_memory_list_options_init(
    xllm_memory_list_options *pOptions
);
```

Defaults: scope is `ANY`, `uMaxItems = 32`, `uMaxCharsPerText = 800`.

### xllm_memory_list_records

Lists records that match conditions.

```c
XLLM_API int xllm_memory_list_records(
    xllm_memory *pMemory,
    const xllm_memory_list_options *pOptions,
    xllm_memory_record_list_result *pResult,
    xllm_error *pError
);
```

### xllm_memory_record_list_result_reset

Releases record list results.

```c
XLLM_API void xllm_memory_record_list_result_reset(
    xllm_memory_record_list_result *pResult
);
```

### xllm_memory_list_chunks

Lists chunks that match conditions.

```c
XLLM_API int xllm_memory_list_chunks(
    xllm_memory *pMemory,
    const xllm_memory_list_options *pOptions,
    xllm_memory_chunk_list_result *pResult,
    xllm_error *pError
);
```

### xllm_memory_chunk_list_result_reset

Releases chunk list results.

```c
XLLM_API void xllm_memory_chunk_list_result_reset(
    xllm_memory_chunk_list_result *pResult
);
```

## API: Context Injection

### xllm_memory_context_options_init

Initializes context injection options.

```c
XLLM_API void xllm_memory_context_options_init(
    xllm_memory_context_options *pOptions
);
```

Defaults: priority is `10`, inject at most `4` hits, each at most `600` characters.

### xllm_memory_apply_search_to_request

Appends existing search results to a request as context blocks.

```c
XLLM_API int xllm_memory_apply_search_to_request(
    xllm_request *pRequest,
    const xllm_memory_search_result *pResult,
    const xllm_memory_context_options *pOptions,
    xllm_error *pError
);
```

If request/result is empty or hit count is `0`, the function succeeds and does not modify the request.

### xllm_memory_apply_search_to_turn

Appends existing search results to a turn as context blocks.

```c
XLLM_API int xllm_memory_apply_search_to_turn(
    xllm_turn *pTurn,
    const xllm_memory_search_result *pResult,
    const xllm_memory_context_options *pOptions,
    xllm_error *pError
);
```

### xllm_memory_search_and_apply_to_request

Searches memory and injects results into a request.

```c
XLLM_API int xllm_memory_search_and_apply_to_request(
    xllm_memory *pMemory,
    const xllm_memory_search_options *pSearchOptions,
    xllm_request *pRequest,
    const xllm_memory_context_options *pContextOptions,
    xllm_error *pError
);
```

This is a convenience combination of `xllm_memory_search` + `xllm_memory_apply_search_to_request`; it releases the temporary search result internally.

### xllm_memory_search_and_apply_to_turn

Searches memory and injects results into a turn.

```c
XLLM_API int xllm_memory_search_and_apply_to_turn(
    xllm_memory *pMemory,
    const xllm_memory_search_options *pSearchOptions,
    xllm_turn *pTurn,
    const xllm_memory_context_options *pContextOptions,
    xllm_error *pError
);
```

### xllm_memory_turn_search_apply_options_init

Initializes options for automatic query extraction from turn and injection.

```c
XLLM_API void xllm_memory_turn_search_apply_options_init(
    xllm_memory_turn_search_apply_options *pOptions
);
```

Defaults: generate query from the last user text; do not include system prompt or existing context blocks.

### xllm_memory_search_and_apply_from_turn_to_request

Derives query from source turn, searches memory, and injects results into a request.

```c
XLLM_API int xllm_memory_search_and_apply_from_turn_to_request(
    xllm_memory *pMemory,
    const xllm_turn *pSourceTurn,
    xllm_request *pRequest,
    const xllm_memory_turn_search_apply_options *pOptions,
    xllm_error *pError
);
```

If `pOptions->tSearchOptions.sQuery` is already set, the function uses it; otherwise it extracts query text from `pSourceTurn` according to `eQueryMode`.

### xllm_memory_search_and_apply_from_turn_to_turn

Derives query from source turn, searches memory, and injects results into a target turn.

```c
XLLM_API int xllm_memory_search_and_apply_from_turn_to_turn(
    xllm_memory *pMemory,
    const xllm_turn *pSourceTurn,
    xllm_turn *pTurn,
    const xllm_memory_turn_search_apply_options *pOptions,
    xllm_error *pError
);
```

## API: Delete and Conversation Maintenance

### xllm_memory_remove

Deletes one record by record ID.

```c
XLLM_API int xllm_memory_remove(
    xllm_memory *pMemory,
    xllm_memory_scope eScope,
    const char *sRecordId,
    xllm_error *pError
);
```

### xllm_memory_remove_by_source_uri

Deletes records for a source URI.

```c
XLLM_API int xllm_memory_remove_by_source_uri(
    xllm_memory *pMemory,
    xllm_memory_scope eScope,
    const char *sSourceUri,
    uint32 *puRemovedCount,
    xllm_error *pError
);
```

### xllm_memory_remove_by_conversation

Deletes conversation memory by conversation ID and optional turn ID.

```c
XLLM_API int xllm_memory_remove_by_conversation(
    xllm_memory *pMemory,
    xllm_memory_scope eScope,
    const char *sConversationId,
    const char *sTurnId,
    uint32 *puRemovedCount,
    xllm_error *pError
);
```

If `sTurnId` is `NULL`, it usually means delete all matching records under that conversation.

### xllm_memory_remove_by_metadata_options_init / remove_by_metadata

```c
XLLM_API void xllm_memory_remove_by_metadata_options_init(
    xllm_memory_remove_by_metadata_options *pOptions
);

XLLM_API int xllm_memory_remove_by_metadata(
    xllm_memory *pMemory,
    const xllm_memory_remove_by_metadata_options *pOptions,
    uint32 *puRemovedCount,
    xllm_error *pError
);
```

Deletes records by metadata key/value.

### xllm_memory_remove_expired_options_init / remove_expired

```c
XLLM_API void xllm_memory_remove_expired_options_init(
    xllm_memory_remove_expired_options *pOptions
);

XLLM_API int xllm_memory_remove_expired(
    xllm_memory *pMemory,
    const xllm_memory_remove_expired_options *pOptions,
    uint32 *puRemovedCount,
    xllm_error *pError
);
```

Defaults: scope is `MEMORY`, using conversation source URI prefix and expiration metadata key. Use with `iExpiresAtUnix` from typed ingest for temporary tasks, short-term preferences, or session-level memory.

### xllm_memory_trim_conversation_options_init / trim_conversation

```c
XLLM_API void xllm_memory_trim_conversation_options_init(
    xllm_memory_trim_conversation_options *pOptions
);

XLLM_API int xllm_memory_trim_conversation(
    xllm_memory *pMemory,
    const xllm_memory_trim_conversation_options *pOptions,
    uint32 *puRemovedCount,
    xllm_error *pError
);
```

Defaults: scope is `MEMORY`, keep latest `32` records. Use this when saving every conversation turn creates too many old records.

### xllm_memory_compact_conversation_options_init / compact_conversation

```c
XLLM_API void xllm_memory_compact_conversation_options_init(
    xllm_memory_compact_conversation_options *pOptions
);

XLLM_API void xllm_memory_compact_conversation_result_init(
    xllm_memory_compact_conversation_result *pResult
);

XLLM_API int xllm_memory_compact_conversation(
    xllm_memory *pMemory,
    const xllm_memory_compact_conversation_options *pOptions,
    xllm_memory_compact_conversation_result *pResult,
    xllm_error *pError
);
```

This API replaces many records in one conversation with a summary record. It does not generate the summary text; generate `sSummaryText` first, then call compact to write the summary record.

## Example: Search and Inject into Request

```c
xllm_memory_search_options search;
xllm_memory_context_options context;

xllm_memory_search_options_init(&search);
xllm_memory_context_options_init(&context);

search.sQuery = "what answer style does the user prefer";
search.eScope = XLLM_MEMORY_SCOPE_MEMORY;
search.uMaxHits = 3;

context.sLabel = "Relevant memory";
context.uMaxHits = 3;
context.uMaxCharsPerHit = 400;

if (xllm_memory_search_and_apply_to_request(
        memory,
        &search,
        &request,
        &context,
        NULL) != XRT_NET_OK) {
    /* Handle retrieval or context injection failure. */
}
```

## Example: List Knowledge Records

```c
xllm_memory_list_options options;
xllm_memory_record_list_result records;

xllm_memory_list_options_init(&options);
memset(&records, 0, sizeof(records));

options.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
options.uMaxItems = 20;

if (xllm_memory_list_records(memory, &options, &records, NULL) == XRT_NET_OK) {
    for (size_t i = 0; i < records.iRecordCount; ++i) {
        printf("%s %s\n",
            records.pRecords[i].sRecordId,
            records.pRecords[i].sTitle ? records.pRecords[i].sTitle : "");
    }
}

xllm_memory_record_list_result_reset(&records);
```

## Common Mistakes

### Checking return value but not hit count

`XRT_NET_OK` means the search process succeeded. It does not mean there were hits. Check `result.iHitCount`.

### Forgetting to reset search/list/debug results

`xllm_memory_search_result`, `xllm_memory_record_list_result`, `xllm_memory_chunk_list_result`, and `xllm_memory_retrieval_debug_dump` all need reset.

### Injecting all hits without limits

More retrieved content is not always better. Set reasonable limits for `uMaxHits`, `uMaxCharsPerHit`, and `uMaxTotalChars` to reduce context pollution.

## Related Documentation

- [Memory API](api-memory.en.md)
- [Memory Ingest API](api-memory-ingest.en.md)
- [Memory Workspace API](api-memory-workspace.en.md)
- [Back to API Index](README.en.md)
