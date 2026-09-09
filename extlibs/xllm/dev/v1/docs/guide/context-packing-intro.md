# Context Packing 入门

Context packing 是把系统提示词、历史、工具结果、memory 检索结果和用户输入放进模型上下文时的取舍过程。

[返回教程入口](README.md) | [Session API](../api/api-session.md) | [Memory 搜索 API](../api/api-memory-search.md)

## 你会学到什么

本文会解释：

- 为什么不能把所有内容都塞进上下文。
- 怎样给 memory 检索结果设置数量、字符预算、去重和分数门槛。
- 怎样使用 session compact 控制对话历史。
- 怎样避免检索上下文覆盖用户目标。

## 为什么需要 Context Packing

模型上下文窗口是有限的。即使某个模型支持很长上下文，也不代表你应该把所有历史和所有检索结果都放进去。过多上下文会带来三个问题：

- 成本变高。
- 关键信息被噪声淹没。
- 旧目标、旧工具结果或低相关片段影响模型判断。

因此你需要为不同内容设置优先级：

| 内容 | 通常优先级 |
| --- | --- |
| 当前用户输入 | 最高 |
| 系统提示词和安全边界 | 很高，通常 pinned |
| 当前工具链结果 | 高 |
| 最近几轮对话 | 高 |
| 高相关 memory 命中 | 中高 |
| 旧历史和低相关命中 | 低，可摘要或丢弃 |

## 控制 Memory 注入预算

Memory RAG 的 packing 主要由 `xllm_memory_search_options` 和 `xllm_memory_context_options` 控制。

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

可以这样理解：

- Search options 决定“最多找回来什么”。
- Context options 决定“最终放进上下文什么”。

你可以搜索多一点，再在注入阶段用更严格的预算筛选。

## 去重：避免一个文档占满上下文

长文档会被切成多个 chunk。如果查询刚好命中同一个文档的多个相邻 chunk，它们可能占满全部结果。设置 `bDistinctByRecord` 可以优先让不同记录都有机会进入上下文：

```c
tContext.bDistinctByRecord = true;
```

适合打开去重的场景：

- 项目有很多小文档，希望回答能综合多个来源。
- 搜索结果经常来自同一个大文件。
- 你只需要知道“哪个文档相关”，不需要连续读取大段内容。

不适合打开去重的场景：

- 用户明确问某个长文档中的细节。
- 你需要保留连续上下文。

## 分数门槛

`tMinScore` 可以过滤低相关命中：

```c
tSearch.tMinScore.bSet = true;
tSearch.tMinScore.fValue = 0.20;
```

不同检索 scheme 的分数尺度可能不同，所以不要一开始就设置过高门槛。建议流程：

1. 先不设门槛，打印命中和分数。
2. 观察正常问题和无关问题的分数分布。
3. 再设置一个保守门槛。
4. 对低于门槛但仍需要兜底的场景，改用更小预算而不是完全丢弃。

## 标签和优先级

`xllm_memory_context_options.sLabel` 会出现在注入文本中，帮助模型理解这段内容是什么：

```c
tContext.sLabel = "Relevant project knowledge";
```

标签应短而明确。常见标签：

- `Relevant project knowledge`
- `Conversation memory`
- `Retrieved documentation`
- `Tool result context`

`iPriority` 表示 context block 的相对优先级。系统提示词和当前用户输入通常更重要；memory 检索结果应有用但不应压过用户当前目标。

## Session Compact 控制历史

对话历史的 packing 由 session compact 管理。常用配置：

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

`uKeepRecentTurns` 很重要。它告诉 session compact 时尽量保留最近几轮原文，旧轮次可以被截断或摘要。`examples/smoke_session_compact.c` 展示了 compact 后仍保留最近用户消息的行为。

也可以手工 compact：

```c
xllm_compact_options tCompact;
xllm_compact_result tResult;

xllm_compact_options_init(&tCompact);
tCompact.eMode = XLLM_COMPACT_TO_FIT_CURRENT_MODEL;
tCompact.eStrategy = XLLM_COMPACT_TRUNCATE;

xllm_session_compact(pSession, &tCompact, &tResult);
```

如果 `tResult.bCompacted` 为 `true`，表示历史已经被压缩或裁剪。

## 推荐 Packing 顺序

构造一轮请求时，可以按这个顺序思考：

1. 系统提示词：清晰、短、稳定。
2. 当前用户输入：完整保留。
3. 当前工具链结果：只保留回答所需内容。
4. 最近对话历史：由 session 保存。
5. Conversation memory：少量、高相关。
6. Workspace knowledge：少量、高相关、去重。
7. 旧历史：compact 或 summary。

如果上下文不足，优先减少旧历史和低相关 memory，而不是删当前用户输入。

## 避免覆盖用户目标

RAG 内容很容易让模型“跑偏”。例如用户问“请重构这个函数”，检索结果里出现旧的设计目标，模型可能优先解释旧目标而不是执行重构。

可以用这些方式降低风险：

- 给检索块加标签，说明它是参考材料。
- 在系统提示词中说明“当前用户目标优先于检索上下文”。
- 限制检索总字符数。
- 使用更具体的查询，不要把整段聊天历史都作为查询。
- 对 memory 命中做去重和分数门槛。

## 常见预算起点

这些不是固定规则，但适合作为初始值：

| 场景 | 建议 |
| --- | --- |
| 小模型、普通聊天 | 2 到 4 条 memory，总计 2000 到 4000 字符 |
| 项目问答 | 4 到 8 条 knowledge，总计 4000 到 12000 字符 |
| 代码助手 | 最近几轮对话 + 当前文件片段 + 3 到 6 条检索结果 |
| 长对话 | 保留最近 4 到 8 轮，旧历史摘要 |

调优时先看回答质量，再看 token 成本。不要只追求命中数量。

## 常见错误

不要让 memory 搜索结果无限增长。`uMaxHits` 和 `uMaxTotalChars` 应该总是有明确上限。

不要把所有上下文都设成 pinned。Pinned 内容过多时，compact 就没有空间做取舍。

不要让旧工具结果长期留在高优先级上下文。工具结果通常只对当前任务有用。

不要用很宽泛的问题做检索查询。例如“帮我看看”通常搜不到好结果，应优先使用当前用户明确目标或由应用生成的查询。

## 下一步

- 想学习工作区索引，读 [工作区索引入门](workspace-index-intro.md)。
- 想学习长期对话记忆，读 [Conversation Memory 入门](conversation-memory-intro.md)。
- 想看 compact API 细节，读 [Session API](../api/api-session.md)。
