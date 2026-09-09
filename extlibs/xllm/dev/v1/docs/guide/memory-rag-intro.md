# Memory RAG 入门

Memory RAG 是把外部知识写入 `xllm_memory`，按用户问题检索相关片段，再把片段注入模型上下文的流程。

[返回教程入口](README.md) | [Memory API](../api/api-memory.md) | [Memory 搜索 API](../api/api-memory-search.md)

## 你会学到什么

本文会带你完成一个最小 RAG 流程：

- 创建 memory store。
- 写入一段知识文本。
- 用自然语言查询检索命中片段。
- 把检索结果转成 context block。
- 把 context block 放进请求或 turn，让模型带着知识回答。

## Memory 和 Session 的区别

Session 保存当前对话历史，适合短期上下文。Memory 保存可检索知识，适合长期、跨轮、跨会话使用。

| 需求 | 使用 |
| --- | --- |
| 用户刚刚说过的话 | `xllm_session` |
| 项目文档、代码说明、产品知识库 | `xllm_memory` |
| 当前对话里的长期事实、偏好、任务 | `xllm_memory_ingest_fact`、`xllm_memory_ingest_preference`、`xllm_memory_ingest_task` |
| 每轮发送前自动带上相关知识 | memory search + apply |

一个常见聊天系统会同时使用二者：session 管短期聊天历史，memory 管长期知识；每轮用户输入进入 session 前，先从 memory 检索相关片段并注入 turn。

## 最小 RAG 流程

最小流程是：

1. 创建 `xllm_runtime`。
2. 初始化 `xllm_memory_options`。
3. 调用 `xllm_memory_create`。
4. 用 `xllm_memory_ingest_text` 写入知识。
5. 用 `xllm_memory_search` 检索知识。
6. 用 `xllm_memory_apply_search_to_request` 或 `xllm_memory_apply_search_to_turn` 注入上下文。
7. 调用模型。
8. reset 搜索结果，销毁 memory 和 runtime。

## 创建 Memory Store

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

如果 `sSqlitePath` 为 `NULL`，实现可以使用临时或内存型存储，适合 smoke 和学习。实际应用建议设置 SQLite 路径，让知识能在进程重启后继续存在。

`eScheme` 决定检索方案。学习阶段可以从 `XLLM_MEMORY_SCHEME_BUILTIN_SPARSE` 开始，它不要求你先配置 ONNX embedding 资产。

## 写入知识

`xllm_memory_ingest_text` 适合写入一段已经在内存中的文本。

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

几个字段值得养成习惯：

| 字段 | 建议 |
| --- | --- |
| `eScope` | 文档、代码、知识库用 `XLLM_MEMORY_SCOPE_KNOWLEDGE`；个人记忆用 `XLLM_MEMORY_SCOPE_MEMORY` |
| `sRecordId` | 使用稳定 ID，便于覆盖更新和删除 |
| `sTitle` | 写给人和调试工具看的标题 |
| `sSourceUri` | 写清来源，例如 `workspace://...`、`file://...`、`conversation://...` |
| `bReplaceExisting` | 对同一 record 反复更新时设为 `true` |

## 搜索知识

检索时初始化 `xllm_memory_search_options`，设置查询文本和命中数量。

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

搜索结果由 `xllm_memory_search` 填充，使用完必须调用 `xllm_memory_search_result_reset`。结果里的字符串指针属于结果对象，不要长期保存这些指针。

## 把搜索结果注入请求

只搜索还不够，你还需要把命中片段交给模型。`xllm_memory_apply_search_to_request` 会把结果转换成 `xllm_context_block`：

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

之后你再给 `tRequest` 添加用户消息并调用 `xllm_chat_ex`。如果你使用 session/turn，则调用 `xllm_memory_apply_search_to_turn`：

```c
xllm_turn tTurn;
xllm_turn_init(&tTurn);
xllm_turn_add_user_text(&tTurn, "请解释 xllm memory 的检索流程。");

xllm_memory_apply_search_to_turn(&tTurn, &tResult, &tContext, &tError);
xllm_session_chat_ex(pSession, &tTurn, &tCallOptions, &pResponse, &tError);
```

## 从用户 Turn 自动提取查询

很多聊天场景中，你不想手工把用户问题复制到 `tSearch.sQuery`。可以使用 `xllm_memory_search_and_apply_from_turn_to_turn`，让 xllm 从 source turn 中提取查询文本，再把检索结果注入目标 turn：

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

这种写法适合“每轮用户输入自动 RAG”的聊天程序。你仍然应该限制 `uMaxHits` 和 `uMaxTotalChars`，避免把过多检索内容塞进上下文。

## Scope 怎么选

`xllm_memory_scope` 用来区分不同类型的记忆：

| Scope | 用法 |
| --- | --- |
| `XLLM_MEMORY_SCOPE_KNOWLEDGE` | 项目文档、工作区代码、产品知识、静态资料 |
| `XLLM_MEMORY_SCOPE_MEMORY` | 用户偏好、对话中抽取的事实、任务状态 |
| `XLLM_MEMORY_SCOPE_ANY` | 搜索或统计时跨两个 scope 查询 |

初学 RAG 时优先使用 `KNOWLEDGE`。当你开始做“记住用户偏好”或“从对话中沉淀任务”时，再引入 `MEMORY`。

## 控制上下文预算

RAG 的常见问题不是“搜不到”，而是“搜到了太多”。这些字段可以帮你控制注入内容：

| 字段 | 作用 |
| --- | --- |
| `xllm_memory_search_options.uMaxHits` | 最多返回多少命中 |
| `xllm_memory_search_options.uMaxCharsPerHit` | 每条命中最多保留多少字符 |
| `xllm_memory_context_options.uMaxHits` | 注入上下文时最多使用多少命中 |
| `xllm_memory_context_options.uMaxCharsPerHit` | 注入时每条命中最多多少字符 |
| `xllm_memory_context_options.uMaxTotalChars` | 本次注入总字符预算 |
| `xllm_memory_context_options.bDistinctByRecord` | 避免同一文档多个 chunk 占满结果 |

实际应用建议从较小预算开始，例如 3 到 6 条命中、总计 3000 到 8000 字符，再根据回答质量和模型上下文窗口调整。

## 清理资源

最小清理顺序：

```c
xllm_request_reset(&tRequest);
xllm_memory_search_result_reset(&tResult);
xllm_memory_destroy(pMemory);
xllm_runtime_destroy(pRuntime);
xllm_error_free(&tError);
```

如果你使用 list、debug、workspace ingest 等结果对象，也要调用对应的 `*_reset`。Memory API 大量返回对象包含内部字符串和数组，不 reset 会泄漏内存。

## 常见错误

不要把所有检索结果无预算地注入上下文。RAG 内容越多不一定越好，噪声会降低模型判断质量。

不要把 `sRecordId` 设成随机值后反复 ingest 同一文件。这样会生成重复记录，搜索结果会被旧版本污染。工作区文件建议使用稳定路径或 source URI 派生 record ID。

不要混淆 `MEMORY` 和 `KNOWLEDGE`。项目文档通常放 `KNOWLEDGE`；用户偏好和对话事实通常放 `MEMORY`。

不要长期保存 `xllm_memory_hit` 里的指针。搜索结果 reset 后，这些指针就不再有效。

不要把机密文件直接写入 memory。工作区索引时应使用忽略规则，跳过密钥、构建产物和大型二进制文件。

## 下一步

- 想索引目录和代码工作区，继续读 [工作区索引入门](workspace-index-intro.md)。
- 想把对话中的事实、偏好和任务沉淀为长期记忆，继续读 [对话记忆入门](conversation-memory-intro.md)。
- 想查每个搜索和注入 API 的字段，读 [Memory 搜索 API](../api/api-memory-search.md)。
