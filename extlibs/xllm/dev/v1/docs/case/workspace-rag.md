# 用 Memory 做工作区 RAG

本案例展示怎样把工作区文件写入 `xllm_memory`，按问题检索相关片段，并把带来源引用的上下文注入一次模型请求。

[返回范例解析](README.md) | [工作区索引入门](../guide/workspace-index-intro.md) | [Memory Workspace API](../api/api-memory-workspace.md)

## 问题

AI IDE、代码助手和文档问答系统都需要回答“当前项目里有什么”。直接把整个仓库塞给模型不可行：文件太多、噪声太大、可能包含敏感信息。

工作区 RAG 的做法是：

1. 先过滤并索引工作区。
2. 用户提问时只检索相关片段。
3. 把少量片段作为 context block 注入请求。
4. 让模型基于这些片段回答。

完整示例见：

```text
examples/ai_ide_memory/ai_ide_memory.c
```

## 架构

```text
workspace files
  -> ingest_workspace
  -> memory records/chunks
用户问题
  -> memory search
  -> context block with source/chunk/byte range
  -> xllm request
```

这个案例只演示 memory 和 request 注入，不依赖真实 provider。你可以把生成的 `xllm_request` 交给任意支持文本输入的 profile。

## 步骤 1：准备 Memory Store

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

学习阶段使用 `XLLM_MEMORY_SCHEME_BUILTIN_SPARSE` 足够简单。生产环境如果要使用 embedding 或混合检索，再配置 embedder 和 vector 相关选项。

## 步骤 2：索引工作区

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

示例工作区包含：

- `src/memory_bridge.c`
- `docs/architecture.md`
- `.env`

因为开启了 `bSkipHidden` 和 workspace 默认过滤，`.env` 不应进入索引。这是 AI IDE 场景里很重要的安全边界。

## 步骤 3：检查索引结果

```c
printf("ingested=%u skipped=%u failed=%u\n",
       tResult.uIngestedFileCount,
       tResult.uSkippedFileCount,
       tResult.uFailedFileCount);
```

你应该关注：

| 字段 | 含义 |
| --- | --- |
| `uVisitedFileCount` | 扫描到的文件数 |
| `uIngestedFileCount` | 成功写入 memory 的文件数 |
| `uSkippedFileCount` | 被规则跳过的文件数 |
| `uFailedFileCount` | 读取或写入失败的文件数 |

`skipped` 不一定是坏事，它通常说明过滤规则生效。`failed` 应该尽量为 0。

## 步骤 4：检索相关片段

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

每条 hit 都带有来源信息：

- `sRecordId`
- `sSourceUri`
- `sChunkId`
- `iStartByte`
- `iEndByte`
- `sText`

这些字段可以用于 UI 引用、日志、跳转到文件位置和调试检索质量。

## 步骤 5：按 Chunk 查详情

示例还展示了用 top hit 的 `sChunkId` 反查 chunk：

```c
xllm_memory_list_options tList;
xllm_memory_chunk_list_result tChunkList;

xllm_memory_list_options_init(&tList);
memset(&tChunkList, 0, sizeof(tChunkList));

tList.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
tList.sChunkId = tSearchResult.pHits[0].sChunkId;

xllm_memory_list_chunks(pMemory, &tList, &tChunkList, &tError);
```

这一步适合做 IDE UI：用户点击引用时，你可以根据 source URI 和 byte range 打开对应文件片段。

## 步骤 6：注入请求上下文

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

注入后，`tRequest.iContextBlockCount` 应大于 0。示例还检查了注入文本包含：

- 上下文标签。
- `Source: workspace://...`
- `Chunk: ...`
- `bytes=...`
- 不包含 `.env` 中的 `OPENAI_API_KEY`。

这说明 RAG 上下文既有引用，又没有把敏感隐藏文件注入模型。

## 步骤 7：发送给模型

本案例生成的是 `xllm_request`。实际发送时，你还需要添加当前用户消息：

```c
tRequest.sProfileId = "fast-text";

/* 实际项目中应按 Request / Response API 构造 XLLM_ROLE_USER 消息。
   如果你使用 turn/session 路径，可以改用 xllm_turn_add_user_text。 */
xllm_chat_ex(pRuntime, &tRequest, &tCallOptions, &pResponse, &tError);
```

如果你使用 session/turn，可以改用 `xllm_memory_search_and_apply_to_turn` 或 `xllm_memory_search_and_apply_from_turn_to_turn`。

## 关键 API

| API | 作用 |
| --- | --- |
| `xllm_memory_create` | 创建 memory store |
| `xllm_memory_ingest_workspace` | 扫描并写入工作区文件 |
| `xllm_memory_search` | 检索相关片段 |
| `xllm_memory_list_chunks` | 按 chunk 条件列出片段详情 |
| `xllm_memory_search_and_apply_to_request` | 检索并注入请求 |
| `xllm_request_reset` | 释放请求内 context block |

## 扩展点

可以继续加入：

- `xllm_memory_sync_workspace`：持续同步文件增删改。
- watcher worker：把文件系统事件批量同步进 memory。
- metadata 过滤：按语言、目录、项目模块过滤。
- citations UI：把 source URI 和 byte range 展示为可点击引用。
- 敏感规则：增加 `.env`、证书、密钥和构建产物过滤。

## 常见问题

不要把整个仓库无过滤写入 memory。先排除隐藏文件、构建产物、依赖目录和大文件。

不要忽略 source URI。没有来源信息，模型回答就很难追溯。

不要让 RAG 注入内容过长。设置 `uMaxHits`、`uMaxCharsPerHit` 和 `uMaxTotalChars`。

不要把工作区 RAG 当作精确语义解析器。它适合找相关片段；精确符号跳转仍应交给语言服务。
