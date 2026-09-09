# 用 Tool Loop 写一个最小 Agent

本案例展示一个最小 agent loop：模型先读取 memory 上下文，再发出 tool call，宿主执行工具，最后模型基于工具结果回答。

[返回范例解析](README.md) | [Tool Loop 入门](../guide/tool-loop-intro.md) | [Tools API](../api/api-tools.md)

## 问题

很多 agent 不是简单聊天，而是需要“先查上下文，再调用工具，再综合回答”。例如一个代码助手在编辑前应先检查工作区状态；一个运维助手在回答前应先查询服务健康；一个业务助手需要读取内部系统数据。

这个案例用最小结构展示 agent loop 的四个组成部分：

- `xllm_session`：保存当前对话。
- `xllm_memory`：提供长期策略或项目上下文。
- `xllm_tool_def`：告诉模型可用工具。
- `xllm_tool_executor`：由宿主真正执行工具。

## 架构

```text
用户输入
  -> memory search/apply
  -> session chat
    -> 模型返回 tool call
    -> xllm 调用宿主 executor
    -> tool result 回到模型上下文
    -> 模型返回 final answer
  -> 可选：把本轮摘要写回 memory
```

完整示例见：

```text
examples/agent_loop/agent_loop.c
```

## 步骤 1：声明支持工具的 Profile

工具调用需要 profile 声明工具相关能力：

```c
tProfile.tModels.tText.tCaps.uFlags =
    XLLM_CAP_TEXT_IN |
    XLLM_CAP_TEXT_OUT |
    XLLM_CAP_TOOL_CALL_OUT |
    XLLM_CAP_TOOL_RESULT_IN;
```

缺少 `XLLM_CAP_TOOL_CALL_OUT` 时，模型不能发出工具调用。缺少 `XLLM_CAP_TOOL_RESULT_IN` 时，工具结果不能可靠回传给模型。

## 步骤 2：写入 Agent Memory

示例把一条策略写入 `XLLM_MEMORY_SCOPE_MEMORY`：

```c
xllm_memory_ingest_options tIngest;

xllm_memory_ingest_options_init(&tIngest);
tIngest.eScope = XLLM_MEMORY_SCOPE_MEMORY;
tIngest.sRecordId = "agent-loop-policy";
tIngest.sTitle = "Agent loop policy";
tIngest.sSourceUri = "memory://agent-loop/policy";
tIngest.sText =
    "project memory: agent loop should inspect workspace before editing "
    "and use an xwork-like executor for tools.";
tIngest.bReplaceExisting = true;

xllm_memory_ingest_text(pMemory, &tIngest, &tError);
```

这一步模拟“长期记忆里已有 agent 行为约束”。真实应用可以把用户偏好、项目政策、任务规则或安全边界写入 memory。

## 步骤 3：创建 Session 并安装 Executor

```c
xllm_session_options tSessionOptions;
xllm_tool_executor tExecutor;

xllm_session_options_init(&tSessionOptions);
tSessionOptions.sProfileId = "mock-agent-loop";
tSessionOptions.sSystemPrompt = "You are an agent loop integration demo.";

xllm_session_create(pRuntime, &tSessionOptions, &pSession);

memset(&tExecutor, 0, sizeof(tExecutor));
tExecutor.pCtx = &tState;
tExecutor.pfnExecute = xwork_like_execute;

xllm_session_set_tool_executor(pSession, &tExecutor);
```

Executor 是宿主程序的工具执行入口。模型只决定“要调用什么工具、参数是什么”；实际执行必须由宿主完成。

## 步骤 4：定义工具

```c
xllm_tool_def tTool;
memset(&tTool, 0, sizeof(tTool));

tTool.sToolId = "xwork.workspace.inspect";
tTool.sWireName = "inspect_workspace";
tTool.sDescription = "Inspect workspace state using the host xwork-like executor.";

xllm_turn_add_tool(&tTurn, &tTool);
```

生产环境建议补充 `tInputSchema`，让模型知道参数结构。示例为了聚焦流程，省略了 JSON schema。

## 步骤 5：把 Memory 注入 Turn

在调用模型前，先用用户 turn 作为查询来源，检索 memory 并注入同一个 turn：

```c
xllm_memory_turn_search_apply_options tApply;

xllm_memory_turn_search_apply_options_init(&tApply);
tApply.tSearchOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
tApply.tSearchOptions.uMaxHits = 1u;
tApply.tContextOptions.sLabel = "Agent memory:";
tApply.tContextOptions.uMaxHits = 1u;
tApply.tContextOptions.uMaxCharsPerHit = 512u;

xllm_memory_search_and_apply_from_turn_to_turn(
    pMemory,
    &tTurn,
    &tTurn,
    &tApply,
    &tError
);
```

这一步让模型在决定是否调用工具前看到“编辑前应检查工作区”的长期记忆。

## 步骤 6：执行 Agent Loop

```c
xllm_response *pResponse = NULL;

xllm_session_chat_ex(pSession, &tTurn, NULL, &pResponse, &tError);

printf("%s\n", xllm_response_get_text(pResponse));
```

因为 session 已安装 executor，`xllm_session_chat_ex` 内部可以完成：

1. 模型返回 `inspect_workspace` tool call。
2. xllm 调用 `xwork_like_execute`。
3. executor 返回 `workspace_status=clean; executor=xwork-like`。
4. xllm 把 tool result 放入后续模型请求。
5. 模型生成 final answer。

## Executor 的最小实现

```c
static int32 xwork_like_execute(
    void *pCtx,
    const xllm_tool_exec_request *pRequest,
    xllm_tool_exec_result *pResult,
    xllm_error *pError
)
{
    (void)pCtx;
    (void)pError;

    if ( strcmp(pRequest->sToolId, "xwork.workspace.inspect") != 0 ) {
        return XRT_NET_ERROR;
    }

    pResult->pParts = (xllm_content_part *)xrtCalloc(1u, sizeof(xllm_content_part));
    pResult->iPartCount = 1u;
    pResult->pParts[0].eKind = XLLM_PART_TEXT;
    pResult->pParts[0].as.tSource.eKind = XLLM_SOURCE_INLINE_TEXT;
    pResult->pParts[0].as.tSource.sMimeType = "text/plain";
    pResult->pParts[0].as.tSource.as.sText =
        "workspace_status=clean; executor=xwork-like";

    return XRT_NET_OK;
}
```

真实 executor 应做更多事：

- 校验 `sArgumentsJson`。
- 限制可访问路径和命令。
- 设置超时。
- 记录审计日志。
- 返回短而清晰的结果。

## 步骤 7：把结果写回 Memory

Agent 完成后，可以把摘要显式写回 memory：

```c
xllm_memory_ingest_turn_response_options tWrite;

xllm_memory_ingest_turn_response_options_init(&tWrite);
tWrite.eExtractionPolicy = XLLM_MEMORY_EXTRACTION_POLICY_SUMMARY;
tWrite.pTurn = &tTurn;
tWrite.pResponse = pResponse;
tWrite.sRecordId = "agent-loop-summary";
tWrite.sConversationId = "agent-loop-demo";
tWrite.sTurnId = "turn-001";
tWrite.sSummaryText =
    "summary: Agent inspected workspace through xwork-like executor and produced a final response.";
tWrite.bReplaceExisting = true;

xllm_memory_ingest_turn_response(pMemory, &tWrite, &tError);
```

注意，这一步是显式写入。不要默认把每一次 agent 过程全部写入长期 memory，应该只保存对后续有价值的信息。

## 关键 API

| API | 作用 |
| --- | --- |
| `xllm_session_create` | 创建带历史的 agent 会话 |
| `xllm_session_set_tool_executor` | 安装自动工具执行器 |
| `xllm_turn_add_tool` | 给本轮提供工具 |
| `xllm_memory_search_and_apply_from_turn_to_turn` | 检索并注入长期记忆 |
| `xllm_session_chat_ex` | 执行模型请求和自动 tool loop |
| `xllm_memory_ingest_turn_response` | 把本轮摘要写回 memory |

## 扩展点

你可以把这个最小 agent 扩展为：

- 多工具 agent：读取文件、搜索符号、运行测试。
- 异步 executor：工具执行耗时较长时返回 future。
- 审批型 agent：危险工具执行前要求用户确认。
- Workspace RAG agent：把代码片段和工具结果一起给模型。
- 持久 session：导出/import session state。

## 常见问题

不要让模型直接执行命令。模型只输出 tool call，宿主必须校验和执行。

不要让 tool result 太长。结果越长，占用上下文越多，越容易影响 final answer。

不要省略工具能力标记。自动 loop 需要模型同时支持 tool call 输出和 tool result 输入。

不要把所有 agent 过程都永久写入 memory。长期记忆应经过筛选、摘要和用户策略控制。
