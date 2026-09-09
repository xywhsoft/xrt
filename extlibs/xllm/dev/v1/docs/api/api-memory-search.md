# xllm Memory Search API

> 状态：中文初稿已生成，待审阅。  
> 头文件：`xllm-memory.h`

本页讲解如何从 xllm memory 中检索内容、查看命中结果、列出 record/chunk、生成 retrieval debug dump，并把检索结果注入 `xllm_request` 或 `xllm_turn`。  
页面后半部分还介绍和检索密切相关的维护接口：删除记录、清理过期记忆、裁剪和压缩会话记忆。

## 你将学到什么

- 如何使用 `xllm_memory_search` 查询 memory。
- `xllm_memory_hit` 中每个字段是什么意思。
- 如何释放 search、list、debug 结果对象。
- 如何把命中内容作为 context block 注入请求。
- 如何按 record/source/conversation/metadata 删除记忆。
- 如何清理过期记忆、裁剪会话、写入会话摘要。

## 检索的最小流程

```c
xllm_memory_search_options options;
xllm_memory_search_result result;

xllm_memory_search_options_init(&options);
memset(&result, 0, sizeof(result));

options.sQuery = "如何创建 memory store";
options.eScope = XLLM_MEMORY_SCOPE_ANY;
options.uMaxHits = 5;

if (xllm_memory_search(memory, &options, &result, NULL) == XRT_NET_OK) {
    for (size_t i = 0; i < result.iHitCount; ++i) {
        printf("%s\n", result.pHits[i].sText);
    }
}

xllm_memory_search_result_reset(&result);
```

请注意最后一行：search 结果内部包含由 xllm 分配的字符串和数组，使用后必须 reset。

## 类型

### xllm_memory_search_options

**功能**：描述一次 memory 检索。

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

| 字段 | 含义 |
| --- | --- |
| `eScope` | 检索范围，默认 `ANY`。 |
| `sQuery` | 查询文本，通常必填。 |
| `sConversationId` | 只检索指定会话的记忆。 |
| `sTurnId` | 只检索指定 turn 的记忆。 |
| `sRecordId` | 只检索指定 record。 |
| `sSourceUri` | 只检索指定来源 URI。 |
| `sRecordIdContains` | record id 包含过滤。 |
| `sTitleContains` | 标题包含过滤。 |
| `sSourceUriContains` | 来源 URI 包含过滤。 |
| `sTextContains` | chunk 文本包含过滤。 |
| `sMetadataKey` | metadata key 过滤。 |
| `sMetadataValue` | metadata value 过滤；设置它时必须设置 `sMetadataKey`。 |
| `uMaxHits` | 最大命中数，默认 `5`。 |
| `tMinScore` | 最小分数，可选；未设置时实现使用默认阈值。 |
| `tPriorityWeight` | priority 加权，可选。 |
| `tRecencyWeight` | recency 加权，可选。 |
| `bSkipExpired` | 是否跳过过期记忆。 |
| `iNowUnix` | 当前 Unix 时间；为 `0` 时由 xllm 获取当前时间。 |
| `uMaxCharsPerHit` | 每条命中文本最多返回字符数，默认 `800`。 |
| `tVendorExtra` | 扩展字段。 |

**补充说明**：`sQuery` 是用于打分的文本；其他字段更多是过滤条件。比如你可以用 `sQuery = "用户偏好"`，同时用 `eScope = MEMORY` 限定只查对话记忆。

### xllm_memory_hit

**功能**：表示一个检索命中的 chunk。

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

| 字段 | 含义 |
| --- | --- |
| `eScope` | 命中来自 `MEMORY` 还是 `KNOWLEDGE`。 |
| `sRecordId` | 所属 record ID。 |
| `sChunkId` | 命中的 chunk ID。 |
| `sTitle` | record 标题。 |
| `sSourceUri` | record 来源。 |
| `sText` | 命中文本。 |
| `fScore` | 综合得分。 |
| `fLexicalScore` | 词法检索得分。 |
| `fVectorScore` | 向量检索得分。 |
| `fRrfScore` | RRF 融合得分。 |
| `uChunkIndex` | chunk 在 record 中的序号。 |
| `iStartByte` / `iEndByte` | chunk 在原文中的字节范围。 |
| `uLexicalRank` / `uVectorRank` | 在词法/向量候选中的排名。 |
| `sRetrievalProfileId` | 检索 profile。 |
| `sEmbedProfileId` | embedding profile。 |
| `sIndexProfileId` | 索引 profile。 |
| `tMetadata` | record 或 chunk 携带的 metadata。 |
| `tVendorExtra` | 扩展字段。 |

### xllm_memory_search_result

**功能**：保存 search 结果。

```c
struct xllm_memory_search_result {
    xllm_memory_hit *pHits;
    size_t iHitCount;
    xvalue tVendorExtra;
};
```

**释放规则**：使用后调用 `xllm_memory_search_result_reset`。

### xllm_memory_list_options

**功能**：列出 record 或 chunk 时使用的过滤和分页参数。

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

**默认值**：scope 为 `ANY`，最大条目数 `32`，每段文本最多 `800` 字符。

### xllm_memory_record_info

**功能**：描述一个 record。

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

**功能**：描述一个 chunk。

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

**功能**：控制把检索结果注入 request/turn 时生成的 context block。

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

| 字段 | 含义 |
| --- | --- |
| `eKindOverride` | 覆盖 context block 类型；为 `0` 时按 scope 推导。 |
| `iPriority` | context block 优先级，默认 `10`。 |
| `bPinned` | 是否固定在上下文中。 |
| `bDistinctByRecord` | 是否每个 record 只取一个命中。 |
| `tMinScore` | 注入时的最低分数。 |
| `sLabel` | context 文本标签。 |
| `uMaxHits` | 最多注入命中数，默认 `4`。 |
| `uMaxCharsPerHit` | 每条命中最多字符数，默认 `600`。 |
| `uMaxTotalChars` | 注入文本总字符上限，`0` 表示不额外限制。 |
| `pfnRender` | 自定义渲染回调。 |
| `pRenderCtx` | 渲染回调上下文。 |

### xllm_memory_turn_query_mode

**功能**：从 turn 自动生成搜索查询时，选择使用哪些文本。

```c
typedef enum {
    XLLM_MEMORY_TURN_QUERY_LAST_USER_TEXT = 0,
    XLLM_MEMORY_TURN_QUERY_ALL_USER_TEXT,
    XLLM_MEMORY_TURN_QUERY_VISIBLE_TEXT
} xllm_memory_turn_query_mode;
```

| 值 | 含义 |
| --- | --- |
| `LAST_USER_TEXT` | 使用最后一条用户文本，默认。 |
| `ALL_USER_TEXT` | 使用所有用户文本。 |
| `VISIBLE_TEXT` | 使用可见文本，通常比仅用户文本更宽。 |

### xllm_memory_turn_search_apply_options

**功能**：从一个 turn 中提取查询、搜索 memory，并把结果注入 request 或另一个 turn。

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

**默认值**：使用最后一条用户文本作为 query，不包含 system prompt 和已有 context blocks。

## API：检索与释放

### xllm_memory_search_options_init

**功能**：初始化 search options。

**原型**：

```c
XLLM_API void xllm_memory_search_options_init(
    xllm_memory_search_options *pOptions
);
```

**默认值**：scope 为 `ANY`，`uMaxHits = 5`，`uMaxCharsPerHit = 800`。

### xllm_memory_search

**功能**：执行 memory 检索。

**原型**：

```c
XLLM_API int xllm_memory_search(
    xllm_memory *pMemory,
    const xllm_memory_search_options *pOptions,
    xllm_memory_search_result *pResult,
    xllm_error *pError
);
```

**参数**：

| 参数 | 说明 |
| --- | --- |
| `pMemory` | memory 对象。 |
| `pOptions` | 检索参数。可为 `NULL`，但通常应提供并设置 `sQuery`。 |
| `pResult` | 输出结果。 |
| `pError` | 错误对象，可为 `NULL`。 |

**返回值**：

| 返回值 | 含义 |
| --- | --- |
| `XRT_NET_OK` | 检索成功，命中数可能为 `0`。 |
| `XRT_NET_ERROR` | 参数无效、query 为空、metadata 过滤不完整、embedding 或存储查询失败。 |

**补充说明**：

- `sQuery` 为空会失败。
- 如果设置 `sMetadataValue`，必须同时设置 `sMetadataKey`。
- 结果会按综合得分排序。
- 如果 memory 启用了 hybrid search 且有 embedder，会同时使用词法和向量候选。

### xllm_memory_search_result_reset

**功能**：释放 search 结果。

**原型**：

```c
XLLM_API void xllm_memory_search_result_reset(
    xllm_memory_search_result *pResult
);
```

**参数**：

| 参数 | 说明 |
| --- | --- |
| `pResult` | 要释放的 search result。 |

**返回值**：无。

**补充说明**：这个函数会释放 `pHits` 及命中内部字符串、metadata 引用，并把结构体清零。

## API：debug 与 list

### xllm_memory_retrieval_debug_options_init

**功能**：初始化 retrieval debug options。

**原型**：

```c
XLLM_API void xllm_memory_retrieval_debug_options_init(
    xllm_memory_retrieval_debug_options *pOptions
);
```

**默认值**：内部 search options 使用默认搜索参数，`uMaxCandidates = 32`，包含低于最小分数的候选。

### xllm_memory_search_debug

**功能**：执行一次可解释的检索，输出候选、term 和最终结果。

**原型**：

```c
XLLM_API int xllm_memory_search_debug(
    xllm_memory *pMemory,
    const xllm_memory_retrieval_debug_options *pOptions,
    xllm_memory_retrieval_debug_dump *pDump,
    xllm_error *pError
);
```

**返回值**：成功返回 `XRT_NET_OK`，失败返回 `XRT_NET_ERROR`。

**补充说明**：当你觉得“为什么这条记忆没有被召回”时，优先使用 debug dump。它会展示候选、分数、是否通过 min score，以及最终是否进入结果。

### xllm_memory_retrieval_debug_dump_reset

**功能**：释放 retrieval debug dump。

**原型**：

```c
XLLM_API void xllm_memory_retrieval_debug_dump_reset(
    xllm_memory_retrieval_debug_dump *pDump
);
```

### xllm_memory_list_options_init

**功能**：初始化 list options。

**原型**：

```c
XLLM_API void xllm_memory_list_options_init(
    xllm_memory_list_options *pOptions
);
```

**默认值**：scope 为 `ANY`，`uMaxItems = 32`，`uMaxCharsPerText = 800`。

### xllm_memory_list_records

**功能**：列出符合条件的 record。

**原型**：

```c
XLLM_API int xllm_memory_list_records(
    xllm_memory *pMemory,
    const xllm_memory_list_options *pOptions,
    xllm_memory_record_list_result *pResult,
    xllm_error *pError
);
```

**参数**：

| 参数 | 说明 |
| --- | --- |
| `pMemory` | memory 对象。 |
| `pOptions` | 过滤和分页参数。 |
| `pResult` | 输出 record 列表。 |
| `pError` | 错误对象，可为 `NULL`。 |

**返回值**：成功返回 `XRT_NET_OK`，失败返回 `XRT_NET_ERROR`。

### xllm_memory_record_list_result_reset

**功能**：释放 record list 结果。

**原型**：

```c
XLLM_API void xllm_memory_record_list_result_reset(
    xllm_memory_record_list_result *pResult
);
```

### xllm_memory_list_chunks

**功能**：列出符合条件的 chunk。

**原型**：

```c
XLLM_API int xllm_memory_list_chunks(
    xllm_memory *pMemory,
    const xllm_memory_list_options *pOptions,
    xllm_memory_chunk_list_result *pResult,
    xllm_error *pError
);
```

**返回值**：成功返回 `XRT_NET_OK`，失败返回 `XRT_NET_ERROR`。

### xllm_memory_chunk_list_result_reset

**功能**：释放 chunk list 结果。

**原型**：

```c
XLLM_API void xllm_memory_chunk_list_result_reset(
    xllm_memory_chunk_list_result *pResult
);
```

## API：注入上下文

### xllm_memory_context_options_init

**功能**：初始化 context 注入 options。

**原型**：

```c
XLLM_API void xllm_memory_context_options_init(
    xllm_memory_context_options *pOptions
);
```

**默认值**：priority 为 `10`，最多注入 `4` 条命中，每条最多 `600` 字符。

### xllm_memory_apply_search_to_request

**功能**：把已有 search 结果作为 context block 追加到 request。

**原型**：

```c
XLLM_API int xllm_memory_apply_search_to_request(
    xllm_request *pRequest,
    const xllm_memory_search_result *pResult,
    const xllm_memory_context_options *pOptions,
    xllm_error *pError
);
```

**返回值**：成功返回 `XRT_NET_OK`，失败返回 `XRT_NET_ERROR`。如果 request、result 为空或命中数为 `0`，函数返回成功且不做修改。

### xllm_memory_apply_search_to_turn

**功能**：把已有 search 结果作为 context block 追加到 turn。

**原型**：

```c
XLLM_API int xllm_memory_apply_search_to_turn(
    xllm_turn *pTurn,
    const xllm_memory_search_result *pResult,
    const xllm_memory_context_options *pOptions,
    xllm_error *pError
);
```

**返回值**：成功返回 `XRT_NET_OK`，失败返回 `XRT_NET_ERROR`。

### xllm_memory_search_and_apply_to_request

**功能**：搜索 memory，并把结果注入 request。

**原型**：

```c
XLLM_API int xllm_memory_search_and_apply_to_request(
    xllm_memory *pMemory,
    const xllm_memory_search_options *pSearchOptions,
    xllm_request *pRequest,
    const xllm_memory_context_options *pContextOptions,
    xllm_error *pError
);
```

**补充说明**：这是 `xllm_memory_search` + `xllm_memory_apply_search_to_request` 的便捷组合，会在内部释放临时 search result。

### xllm_memory_search_and_apply_to_turn

**功能**：搜索 memory，并把结果注入 turn。

**原型**：

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

**功能**：初始化从 turn 自动查询并注入的 options。

**原型**：

```c
XLLM_API void xllm_memory_turn_search_apply_options_init(
    xllm_memory_turn_search_apply_options *pOptions
);
```

**默认值**：从最后一条用户文本生成 query，不包含 system prompt 和已有 context blocks。

### xllm_memory_search_and_apply_from_turn_to_request

**功能**：从 source turn 推导 query，搜索 memory，并把结果注入 request。

**原型**：

```c
XLLM_API int xllm_memory_search_and_apply_from_turn_to_request(
    xllm_memory *pMemory,
    const xllm_turn *pSourceTurn,
    xllm_request *pRequest,
    const xllm_memory_turn_search_apply_options *pOptions,
    xllm_error *pError
);
```

**补充说明**：如果 `pOptions->tSearchOptions.sQuery` 已经设置，函数会使用它；否则根据 `eQueryMode` 从 `pSourceTurn` 提取查询文本。

### xllm_memory_search_and_apply_from_turn_to_turn

**功能**：从 source turn 推导 query，搜索 memory，并把结果注入目标 turn。

**原型**：

```c
XLLM_API int xllm_memory_search_and_apply_from_turn_to_turn(
    xllm_memory *pMemory,
    const xllm_turn *pSourceTurn,
    xllm_turn *pTurn,
    const xllm_memory_turn_search_apply_options *pOptions,
    xllm_error *pError
);
```

## API：删除与会话维护

### xllm_memory_remove

**功能**：按 record id 删除一条记录。

**原型**：

```c
XLLM_API int xllm_memory_remove(
    xllm_memory *pMemory,
    xllm_memory_scope eScope,
    const char *sRecordId,
    xllm_error *pError
);
```

**参数**：

| 参数 | 说明 |
| --- | --- |
| `pMemory` | memory 对象。 |
| `eScope` | 删除范围。 |
| `sRecordId` | 要删除的 record id。 |
| `pError` | 错误对象，可为 `NULL`。 |

**返回值**：成功返回 `XRT_NET_OK`，失败返回 `XRT_NET_ERROR`。

### xllm_memory_remove_by_source_uri

**功能**：删除指定来源 URI 对应的记录。

**原型**：

```c
XLLM_API int xllm_memory_remove_by_source_uri(
    xllm_memory *pMemory,
    xllm_memory_scope eScope,
    const char *sSourceUri,
    uint32 *puRemovedCount,
    xllm_error *pError
);
```

**返回值**：成功返回 `XRT_NET_OK`，并通过 `puRemovedCount` 输出删除数量。

### xllm_memory_remove_by_conversation

**功能**：按 conversation id 和可选 turn id 删除对话记忆。

**原型**：

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

**补充说明**：如果 `sTurnId` 为 `NULL`，通常表示删除该 conversation 下的全部匹配记录。

### xllm_memory_remove_by_metadata_options_init

**功能**：初始化按 metadata 删除的 options。

**原型**：

```c
XLLM_API void xllm_memory_remove_by_metadata_options_init(
    xllm_memory_remove_by_metadata_options *pOptions
);
```

### xllm_memory_remove_by_metadata

**功能**：按 metadata key/value 删除记录。

**原型**：

```c
XLLM_API int xllm_memory_remove_by_metadata(
    xllm_memory *pMemory,
    const xllm_memory_remove_by_metadata_options *pOptions,
    uint32 *puRemovedCount,
    xllm_error *pError
);
```

### xllm_memory_remove_expired_options_init

**功能**：初始化过期清理 options。

**原型**：

```c
XLLM_API void xllm_memory_remove_expired_options_init(
    xllm_memory_remove_expired_options *pOptions
);
```

**默认值**：scope 为 `MEMORY`，使用对话来源 URI 前缀和过期时间 metadata key。

### xllm_memory_remove_expired

**功能**：删除已经过期的记忆。

**原型**：

```c
XLLM_API int xllm_memory_remove_expired(
    xllm_memory *pMemory,
    const xllm_memory_remove_expired_options *pOptions,
    uint32 *puRemovedCount,
    xllm_error *pError
);
```

**补充说明**：配合 typed ingest 中的 `iExpiresAtUnix` 使用。适合定期清理临时任务、短期偏好或会话级记忆。

### xllm_memory_trim_conversation_options_init

**功能**：初始化会话裁剪 options。

**原型**：

```c
XLLM_API void xllm_memory_trim_conversation_options_init(
    xllm_memory_trim_conversation_options *pOptions
);
```

**默认值**：scope 为 `MEMORY`，保留最新 `32` 条记录。

### xllm_memory_trim_conversation

**功能**：裁剪某个 conversation 的旧记录。

**原型**：

```c
XLLM_API int xllm_memory_trim_conversation(
    xllm_memory *pMemory,
    const xllm_memory_trim_conversation_options *pOptions,
    uint32 *puRemovedCount,
    xllm_error *pError
);
```

**补充说明**：当你持续保存每轮对话时，旧记录会越来越多。trim 用来保留最近记录或限制总字符、总 chunk。

### xllm_memory_compact_conversation_options_init

**功能**：初始化会话压缩 options。

**原型**：

```c
XLLM_API void xllm_memory_compact_conversation_options_init(
    xllm_memory_compact_conversation_options *pOptions
);
```

**默认值**：scope 为 `MEMORY`，替换 summary，移除源记录。

### xllm_memory_compact_conversation_result_init

**功能**：初始化会话压缩结果。

**原型**：

```c
XLLM_API void xllm_memory_compact_conversation_result_init(
    xllm_memory_compact_conversation_result *pResult
);
```

### xllm_memory_compact_conversation

**功能**：用摘要记录替代一个 conversation 的多条历史记录。

**原型**：

```c
XLLM_API int xllm_memory_compact_conversation(
    xllm_memory *pMemory,
    const xllm_memory_compact_conversation_options *pOptions,
    xllm_memory_compact_conversation_result *pResult,
    xllm_error *pError
);
```

**补充说明**：这个 API 不负责生成摘要文本。你需要先让模型生成 `sSummaryText`，再调用 compact 写入 summary record。

## 范例：搜索并注入 request

```c
xllm_memory_search_options search;
xllm_memory_context_options context;

xllm_memory_search_options_init(&search);
xllm_memory_context_options_init(&context);

search.sQuery = "用户喜欢什么回答风格";
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
    /* 处理检索或 context 注入失败。 */
}
```

## 范例：列出知识库记录

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

## 常见错误

### 只检查返回值，不检查命中数

`XRT_NET_OK` 只表示检索过程成功，不表示一定有命中。要检查 `result.iHitCount`。

### 忘记 reset search/list/debug 结果

`xllm_memory_search_result`、`xllm_memory_record_list_result`、`xllm_memory_chunk_list_result`、`xllm_memory_retrieval_debug_dump` 都需要 reset。

### 把所有命中无上限地注入上下文

检索结果越多不一定越好。给 `uMaxHits`、`uMaxCharsPerHit` 和 `uMaxTotalChars` 设置合理上限，能减少上下文污染。

## 相关文档

- [Memory API](api-memory.md)
- [Memory Ingest API](api-memory-ingest.md)
- [Memory Workspace API](api-memory-workspace.md)
- [返回 API 索引](README.md)
