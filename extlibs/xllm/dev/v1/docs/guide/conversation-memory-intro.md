# Conversation Memory 入门

Conversation memory 用来把对话中值得长期保留的事实、偏好、任务或摘要写入 `xllm_memory`，供后续会话检索使用。

[返回教程入口](README.md) | [Memory RAG 入门](memory-rag-intro.md) | [Memory 写入 API](../api/api-memory-ingest.md)

## 你会学到什么

本文会解释：

- 为什么长期记忆需要显式写入。
- summary、task、fact、preference 分别适合保存什么。
- 怎样在下一轮对话前检索 conversation memory。
- 怎样控制用户批准、来源 URI 和覆盖更新。

## 为什么要显式写入

Session 会自动保存当前对话历史，但它不是长期记忆。长期记忆应该有明确来源、类型和生命周期。也就是说，应用应在合适时机决定“这条信息值得保存”，然后调用 memory ingest API。

一个稳妥的流程是：

1. 用户或系统提出可保存的信息。
2. 应用判断是否需要用户批准。
3. 应用把信息整理成 summary、fact、preference 或 task。
4. 写入 `XLLM_MEMORY_SCOPE_MEMORY`。
5. 后续每轮对话前，从 memory 检索相关内容并注入上下文。

## 记忆类型怎么选

| 类型 | API | 适合保存 |
| --- | --- | --- |
| 摘要 | `xllm_memory_ingest_text` 或 `xllm_memory_ingest_turn_response` | 一段对话的总结、背景说明 |
| 任务 | `xllm_memory_ingest_task` | 待办事项、负责人、截止时间、状态 |
| 事实 | `xllm_memory_ingest_fact` | “项目 X 使用库 Y”这类可陈述事实 |
| 偏好 | `xllm_memory_ingest_preference` | 用户喜欢的回答风格、语言、格式 |

初学时可以先用摘要。等你需要筛选、更新、过期和结构化查询时，再使用 task/fact/preference。

## 写入一段对话摘要

`examples/conversation_memory/conversation_memory.c` 使用了最简单的摘要写入方式：

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

调用示例：

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

这里有几个好习惯：

- `eScope` 使用 `XLLM_MEMORY_SCOPE_MEMORY`，表示这是长期记忆而不是项目知识。
- `sRecordId` 包含 conversation/thread ID，便于覆盖和删除。
- `sSourceUri` 指向记忆来源，便于审计。
- `bReplaceExisting = true`，让同一摘要可以更新。

## 下一轮对话前检索记忆

用户再次提问时，你先搜索 memory，再注入请求：

```c
xllm_memory_search_options tSearch;
xllm_memory_context_options tContext;
xllm_request tRequest;

xllm_memory_search_options_init(&tSearch);
xllm_memory_context_options_init(&tContext);
xllm_request_init(&tRequest);

tSearch.eScope = XLLM_MEMORY_SCOPE_MEMORY;
tSearch.sQuery = "What should I remember about user answer style?";
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

注入后的 request 会多出一个 context block。你再添加当前用户消息并发送给模型，模型就能看到相关长期记忆。

## 写入 Fact

事实适合保存结构化陈述。它通常有 subject、predicate、object：

```c
xllm_memory_ingest_fact_options tFact;

xllm_memory_ingest_fact_options_init(&tFact);
tFact.eScope = XLLM_MEMORY_SCOPE_MEMORY;
tFact.sFactId = "project-xllm-language";
tFact.sSubject = "xllm";
tFact.sPredicate = "primary_language";
tFact.sObject = "C";
tFact.sRecordId = "fact:project-xllm-language";
tFact.sTitle = "Project language";
tFact.sSourceUri = "memory://conversation/thread-001/fact/project-language";
tFact.bReplaceExisting = true;
tFact.iPriority = 10;

xllm_memory_ingest_fact(pMemory, &tFact, &tError);
```

当你以后想按项目、用户、会话或元数据过滤时，结构化事实比普通摘要更容易维护。

## 写入 Preference

偏好适合保存“用户希望以后怎么做”的信息：

```c
xllm_memory_ingest_preference_options tPreference;

xllm_memory_ingest_preference_options_init(&tPreference);
tPreference.eScope = XLLM_MEMORY_SCOPE_MEMORY;
tPreference.sPreferenceId = "user-answer-style";
tPreference.sSubject = "user";
tPreference.sKey = "answer_style";
tPreference.sValue = "concise Chinese explanations";
tPreference.sRecordId = "preference:user-answer-style";
tPreference.sTitle = "User answer style";
tPreference.sSourceUri = "memory://conversation/thread-001/preference/answer-style";
tPreference.bReplaceExisting = true;
tPreference.iPriority = 20;

xllm_memory_ingest_preference(pMemory, &tPreference, &tError);
```

偏好通常应获得用户明确同意，尤其是涉及个人信息、工作习惯和长期画像时。

## 写入 Task

任务适合保存待办事项：

```c
xllm_memory_ingest_task_options tTask;

xllm_memory_ingest_task_options_init(&tTask);
tTask.eScope = XLLM_MEMORY_SCOPE_MEMORY;
tTask.sTaskId = "review-docs-api";
tTask.eStatus = XLLM_MEMORY_TASK_STATUS_OPEN;
tTask.sOwner = "user";
tTask.sRecordId = "task:review-docs-api";
tTask.sTitle = "Review xllm API docs";
tTask.sSourceUri = "memory://conversation/thread-001/task/review-docs-api";
tTask.sText = "User plans to review the Chinese xllm API documentation before English translation.";
tTask.iPriority = 30;

xllm_memory_ingest_task(pMemory, &tTask, &tError);
```

任务后续可以按 conversation、metadata 或 record ID 更新状态，也可以设置截止时间和过期时间。

## 从 Turn/Response 提取记忆

如果你希望把一轮对话和模型回答作为来源，可以使用 `xllm_memory_ingest_turn_response`：

```c
xllm_memory_ingest_turn_response_options tTurnMemory;

xllm_memory_ingest_turn_response_options_init(&tTurnMemory);
tTurnMemory.eScope = XLLM_MEMORY_SCOPE_MEMORY;
tTurnMemory.eExtractionPolicy = XLLM_MEMORY_EXTRACTION_POLICY_SUMMARY;
tTurnMemory.pTurn = &tTurn;
tTurnMemory.pResponse = pResponse;
tTurnMemory.sConversationId = "thread-001";
tTurnMemory.sTurnId = "turn-002";
tTurnMemory.sRecordId = "thread-001-turn-002-summary";
tTurnMemory.sTitle = "Turn summary";
tTurnMemory.sSourceUri = "memory://conversation/thread-001/turn-002";
tTurnMemory.bReplaceExisting = true;

xllm_memory_ingest_turn_response(pMemory, &tTurnMemory, &tError);
```

`eExtractionPolicy` 表示你希望如何处理这轮内容。实际应用中建议先由应用或模型生成可审阅摘要，再写入 memory。

## 用户批准和隐私

长期记忆会影响以后的回答，所以需要比普通上下文更谨慎：

- 明确告诉用户哪些内容会被记住。
- 对偏好和个人信息要求用户确认。
- 给每条记忆写入 `sSourceUri`，便于用户追溯。
- 提供删除和清理入口。
- 对过期信息设置 `iExpiresAtUnix`。

## 常见错误

不要把每一句对话都写入长期 memory。这样会制造大量噪声，让检索质量下降。

不要只写摘要、不写来源。没有 `sConversationId`、`sTurnId` 或 `sSourceUri`，以后很难解释这条记忆从哪里来。

不要把 session history 当作 memory。Session 结束、compact 或导出导入策略变化时，短期历史不等同于可治理的长期记忆。

不要把敏感信息默认长期保存。长期记忆应有用户批准、过滤和删除策略。

## 下一步

- 想学习检索和注入，读 [Memory RAG 入门](memory-rag-intro.md)。
- 想控制注入预算，读 [Context Packing 入门](context-packing-intro.md)。
- 想看完整场景，后续读 [对话记忆案例](../case/conversation-memory.md)。
