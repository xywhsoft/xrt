# 显式写入 Conversation Memory

本案例展示宿主怎样把用户批准后的长期对话记忆写入 `xllm_memory`，并在下一轮对话前检索注入。

[返回范例解析](README.md) | [Conversation Memory 入门](../guide/conversation-memory-intro.md) | [Memory 写入 API](../api/api-memory-ingest.md)

## 问题

用户会在对话中告诉助手一些以后还需要记住的信息，例如回答风格、项目习惯、当前任务和长期偏好。Session 只能保存当前短期历史，不适合作为长期记忆。

Conversation memory 的做法是：

1. 宿主决定某条信息值得长期保存。
2. 必要时请求用户批准。
3. 把信息写入 `XLLM_MEMORY_SCOPE_MEMORY`。
4. 后续对话前检索相关记忆。
5. 把记忆作为 context block 注入请求。

完整示例见：

```text
examples/conversation_memory/conversation_memory.c
```

## 架构

```text
对话摘要 / 用户偏好 / 任务事实
  -> explicit ingest
  -> memory scope
下一轮用户问题
  -> memory search
  -> context block
  -> model request
```

重点是“显式”。不要默认把每一句聊天都写成长期记忆。

## 步骤 1：创建 Memory

```c
xllm_memory_options tMemoryOptions;

xllm_memory_options_init(&tMemoryOptions);
tMemoryOptions.sNamespace = "example-conversation-memory";
tMemoryOptions.eScheme = XLLM_MEMORY_SCHEME_BUILTIN_SPARSE;
tMemoryOptions.uDefaultMaxHits = 3u;

xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
```

长期记忆建议设置稳定 namespace。多用户系统应按租户、用户或 workspace 隔离 namespace 或底层存储。

## 步骤 2：写入初始摘要

示例封装了一个摘要写入函数：

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

调用：

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

这里保存了两类信息：回答风格偏好，以及用户使用的自动化工具。真实应用中，偏好类信息通常应经过用户批准。

## 步骤 3：下一轮前检索并注入

```c
xllm_memory_search_options tSearch;
xllm_memory_context_options tContext;
xllm_request tRequest;

xllm_memory_search_options_init(&tSearch);
xllm_memory_context_options_init(&tContext);
xllm_request_init(&tRequest);

tSearch.eScope = XLLM_MEMORY_SCOPE_MEMORY;
tSearch.sQuery = "What should I remember about user answer style and claw automation?";
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

注入后的 context block 会包含标签和命中文本。示例检查它包含：

- `Conversation memory:`
- `concise answers`
- `claw`

然后你再把当前用户消息加入 request 或 turn，发送给模型。

## 步骤 4：聊天后写入新摘要

对话结束后，如果确实产生了值得长期保存的信息，再显式写入：

```c
ingest_conversation_summary(
    pMemory,
    "thread-001-turn-002-summary",
    "Conversation summary",
    "memory://conversation/thread-001/turn-002-summary",
    "summary: Assistant explained that conversation memory should be explicitly written after chat.",
    &tError
);
```

然后可以验证新记忆能被检索：

```c
xllm_memory_search_options_init(&tSearch);
tSearch.eScope = XLLM_MEMORY_SCOPE_MEMORY;
tSearch.sQuery = "explicitly written after chat";
tSearch.uMaxHits = 1u;

xllm_memory_search(pMemory, &tSearch, &tResult, &tError);
```

## 用户批准策略

长期记忆会影响以后回答，建议明确区分三类信息：

| 信息 | 建议 |
| --- | --- |
| 用户明确要求“记住” | 可以写入，但仍应展示保存内容 |
| 偏好、习惯、个人信息 | 需要用户批准 |
| 临时上下文、一次性任务细节 | 通常不写入长期 memory |

UI 可以显示：“我可以记住：你偏好简洁中文回答。是否保存？”用户确认后再 ingest。

## 删除和治理

长期 memory 应提供删除能力。常见删除方式：

```c
uint32 uRemoved = 0;

xllm_memory_remove(
    pMemory,
    XLLM_MEMORY_SCOPE_MEMORY,
    "thread-001-summary",
    &tError
);

xllm_memory_remove_by_source_uri(
    pMemory,
    XLLM_MEMORY_SCOPE_MEMORY,
    "memory://conversation/thread-001/summary",
    &uRemoved,
    &tError
);
```

如果你的应用给记忆写入 metadata，也可以用 `xllm_memory_remove_by_metadata` 按用户、会话、项目或标签删除。

## 关键 API

| API | 作用 |
| --- | --- |
| `xllm_memory_ingest_text` | 写入摘要类长期记忆 |
| `xllm_memory_ingest_turn_response` | 从一轮 turn/response 写入摘要、任务、事实或偏好 |
| `xllm_memory_ingest_task` | 写入任务 |
| `xllm_memory_ingest_fact` | 写入结构化事实 |
| `xllm_memory_ingest_preference` | 写入用户偏好 |
| `xllm_memory_search_and_apply_to_request` | 检索并注入请求 |
| `xllm_memory_remove` | 按 record ID 删除 |
| `xllm_memory_remove_by_source_uri` | 按来源 URI 删除 |

## 扩展点

可以继续加入：

- typed memory：用 task/fact/preference 替代普通摘要。
- metadata：按用户、项目、会话打标签。
- 过期时间：临时任务到期自动移除。
- 用户可视化管理：列出、编辑、删除长期记忆。
- summary compact：把多条旧记忆压缩成更短摘要。

## 常见问题

不要把所有聊天都写入 memory。长期记忆应该少而准。

不要没有来源 URI。每条长期记忆都应能追溯到用户批准或某轮对话。

不要把敏感信息默认保存。偏好和个人信息应有用户确认和删除入口。

不要只写 memory 不检索。长期记忆只有在后续请求前被检索和注入，才会影响模型回答。
