# Tool Loop 入门

Tool loop 让模型在需要外部能力时发出 tool call，由你的程序执行工具，再把 tool result 回传给模型继续回答。

[返回教程入口](README.md) | [Tools API](../api/api-tools.md) | [Session 入门](session-intro.md)

## 你会学到什么

本文会带你理解：

- tool definition、tool call、tool executor、tool result 分别是什么。
- 普通发送和自动 tool loop 的差别。
- 怎样给 `xllm` 或 `xllm_session` 安装工具执行器。
- 怎样观察工具循环事件和 trace。
- 工具循环里最常见的失败点。

## Tool Loop 解决什么问题

模型本身不能直接访问你的数据库、文件系统、业务服务或实时接口。Tool loop 的作用是把“模型决定要调用什么”和“宿主程序真正执行什么”连接起来。

一个典型过程是：

1. 你把可用工具列表放进本轮请求。
2. 模型返回 `XLLM_STATUS_TOOL_CALL_REQUIRED`，并给出工具名和 JSON 参数。
3. 宿主程序执行对应工具。
4. 宿主把执行结果作为 tool result 放回上下文。
5. 模型读取工具结果，生成最终回答。

xllm 提供两种使用方式：

| 方式 | 适合场景 |
| --- | --- |
| 手工循环 | 你想完全控制每轮请求、工具结果格式、循环上限和审计记录 |
| 自动 tool loop | 你希望 xllm 在一次 `xllm_send` 或 `xllm_session_chat` 中完成“模型请求工具 -> 执行工具 -> 再问模型” |

初学时建议先理解手工循环的概念，再使用自动 tool loop。

## 三个核心对象

### `xllm_tool_def`

`xllm_tool_def` 是你告诉模型“可以调用哪些工具”的声明。

常用字段：

| 字段 | 含义 |
| --- | --- |
| `sToolId` | 宿主内部识别工具的稳定 ID |
| `sWireName` | 发送给 provider 的工具名，模型通常看到这个名字 |
| `sDescription` | 给模型看的工具说明 |
| `eKind` | 工具类型，常用 `XLLM_TOOL_CLIENT` |
| `tInputSchema` | 工具参数的 JSON schema，使用 `xvalue` 表示 |

最小定义可以先写 ID、wire name 和说明：

```c
xllm_tool_def tTool;
memset(&tTool, 0, sizeof(tTool));

tTool.sToolId = "app.weather.get_current";
tTool.sWireName = "get_weather";
tTool.sDescription = "Get current weather";

xllm_turn_add_tool(&tTurn, &tTool);
```

生产环境中应补充 `tInputSchema`，让模型知道参数结构和必填字段。

### `xllm_output_tool_call`

当模型选择调用工具时，响应中会出现 tool call：

```c
size_t iToolCount = xllm_response_get_tool_call_count(pResponse);

for ( size_t i = 0u; i < iToolCount; ++i ) {
    const xllm_output_tool_call *pCall =
        xllm_response_get_tool_call(pResponse, i);

    printf("tool=%s args=%s\n",
           pCall->sToolName,
           pCall->sArgumentsJson);
}
```

你应该把 `sToolId` 或 `sToolName` 映射到宿主程序里的实际函数，并把 `sArgumentsJson` 解析成工具参数。不要直接把 JSON 字符串拼接进 shell 或 SQL。

### `xllm_tool_executor`

自动 tool loop 需要一个执行器。执行器是宿主程序提供的回调，xllm 在收到 tool call 后调用它：

```c
static int32 weather_execute(
    void *pCtx,
    const xllm_tool_exec_request *pRequest,
    xllm_tool_exec_result *pResult,
    xllm_error *pError
)
{
    (void)pCtx;
    (void)pError;

    if ( strcmp(pRequest->sToolId, "app.weather.get_current") != 0 ) {
        return XRT_NET_ERROR;
    }

    pResult->pParts = (xllm_content_part *)xrtCalloc(1u, sizeof(xllm_content_part));
    if ( !pResult->pParts ) {
        return XRT_NET_ERROR;
    }

    pResult->iPartCount = 1u;
    pResult->pParts[0].eKind = XLLM_PART_TEXT;
    pResult->pParts[0].as.tSource.eKind = XLLM_SOURCE_INLINE_TEXT;
    pResult->pParts[0].as.tSource.sMimeType = "text/plain";
    pResult->pParts[0].as.tSource.as.sText = "sunny";

    return XRT_NET_OK;
}
```

示例里为了简短直接返回 `"sunny"`。真实应用中，你应先校验 `sArgumentsJson`，再调用外部服务，并把结果写成文本或 JSON part。

## 自动 Tool Loop

自动 tool loop 的关键是：先创建 `xllm` 对象，再设置 executor。

```c
xllm *pLlm = NULL;
xllm_create_options tCreate;
xllm_tool_executor tExecutor;

memset(&tCreate, 0, sizeof(tCreate));
tCreate.sInitialProfileId = "mock-tool";

pLlm = xllm_create(pRuntime, &tCreate);
if ( !pLlm ) {
    return 1;
}

memset(&tExecutor, 0, sizeof(tExecutor));
tExecutor.pfnExecute = weather_execute;

if ( xllm_set_tool_executor(pLlm, &tExecutor) != XRT_NET_OK ) {
    return 1;
}
```

然后发送带工具的 turn：

```c
xllm_turn tTurn;
xllm_response *pResponse = NULL;

xllm_turn_init(&tTurn);
xllm_turn_add_user_text(&tTurn, "what is the weather?");
xllm_turn_add_tool(&tTurn, &tTool);

if ( xllm_send(pLlm, &tTurn, NULL, &pResponse) == XRT_NET_OK ) {
    printf("%s\n", xllm_response_get_text(pResponse));
    xllm_response_free(pResponse);
}

xllm_turn_reset(&tTurn);
```

如果模型第一次返回 tool call，xllm 会调用你的 executor，把结果放回后续请求，并继续得到最终文本。`examples/smoke_auto_tool_loop.c` 展示了完整流程。

Session 也可以安装 executor：

```c
xllm_tool_executor tExecutor;
memset(&tExecutor, 0, sizeof(tExecutor));
tExecutor.pfnExecute = weather_execute;

xllm_session_set_tool_executor(pSession, &tExecutor);
```

这样同一个聊天 session 可以在多轮中反复使用工具。

## Profile 必须声明工具能力

工具循环依赖模型能力。Profile 中的模型能力至少要包含：

```c
tProfile.tModels.tText.tCaps.uFlags =
    XLLM_CAP_TEXT_IN |
    XLLM_CAP_TEXT_OUT |
    XLLM_CAP_TOOL_CALL_OUT |
    XLLM_CAP_TOOL_RESULT_IN;
```

如果 provider 支持并行工具调用，可以再声明 `XLLM_CAP_PARALLEL_TOOL_CALL`。如果能力标记缺失，请求可能无法通过验证，或 provider 不会收到正确的工具声明。

## Tool Choice

默认情况下，模型可以自行决定是否使用工具。你也可以在 turn 上设置策略：

```c
xllm_turn_set_tool_choice(
    &tTurn,
    XLLM_TOOL_CHOICE_REQUIRED,
    NULL,
    false
);
```

常用模式：

| 模式 | 含义 |
| --- | --- |
| `XLLM_TOOL_CHOICE_AUTO` | 让模型自己决定，最常用 |
| `XLLM_TOOL_CHOICE_NONE` | 本轮禁止调用工具 |
| `XLLM_TOOL_CHOICE_REQUIRED` | 本轮必须选择一个工具 |
| `XLLM_TOOL_CHOICE_NAMED` | 本轮指定调用某个工具 |

只有当 provider 和模型支持相应能力时，强制或命名工具调用才可靠。跨 provider 适配时，应先查看能力矩阵和 smoke 示例。

## 事件和 Trace

工具循环中的两个观察面很有用：

- 事件回调：看到 `XLLM_EVENT_TOOL_CALL_READY`、`XLLM_EVENT_TEXT_DELTA` 等实时事件。
- trace 回调：看到 `XLLM_TRACE_TOOL_LOOP`，用于调试每一轮工具循环状态。

事件回调示例：

```c
static bool on_event(const xllm_event *pEvent, void *pUserData)
{
    (void)pUserData;

    if ( pEvent->eType == XLLM_EVENT_TOOL_CALL_READY ) {
        printf("tool call: %s\n",
               pEvent->as.tToolCallReady.tToolCall.sToolName);
    }

    if ( pEvent->eType == XLLM_EVENT_TEXT_DELTA ) {
        printf("%s", pEvent->as.tTextDelta.sText);
    }

    return true;
}
```

如果你正在构建 agent，建议同时记录 tool call 参数、工具执行状态、工具结果摘要和最终回答。这样调试会容易很多。

## 常见错误

不要只定义工具名而不提供 executor。没有 executor 时，普通发送会停在 `XLLM_STATUS_TOOL_CALL_REQUIRED`，这不是失败，而是模型在等待宿主执行工具。

不要信任 `sArgumentsJson`。它来自模型输出，必须按 schema 校验，尤其是路径、URL、SQL、shell 命令、金额和用户 ID。

不要把工具结果写得过长。工具结果会进入下一轮模型上下文，过大的结果会增加成本，并可能触发上下文溢出。必要时只返回摘要、ID 或分页结果。

不要忘记释放 `xllm_tool_exec_result` 中分配的内容。自动 loop 内部会在合适时机处理 executor 返回的 result；你自己手写循环时，需要遵循 API 页里的资源归属规则。

不要忽略循环上限。真实 agent 应限制最大工具轮数，避免模型和工具在错误状态下反复调用。

## 下一步

- 想看每个工具结构体和 executor 的详细字段，读 [Tools API](../api/api-tools.md)。
- 想把工具循环放进多轮聊天，读 [Session 入门](session-intro.md)。
- 想看完整 agent 场景，后续读 [工具循环 Agent 案例](../case/tool-loop-agent.md)。
