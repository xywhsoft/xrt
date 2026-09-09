# Tool Loop Introduction

A tool loop lets the model emit a tool call when it needs external capability. Your program executes the tool, sends the tool result back to the model, and the model continues answering.

[Back to Tutorials](README.en.md) | [Tools API](../api/api-tools.en.md) | [Session Introduction](session-intro.en.md)

## What You Will Learn

This guide helps you understand:

- What tool definitions, tool calls, tool executors, and tool results are.
- The difference between normal sending and automatic tool loops.
- How to install a tool executor on `xllm` or `xllm_session`.
- How to observe tool loop events and traces.
- The most common failure points in tool loops.

## What Problem Tool Loop Solves

The model itself cannot directly access your database, filesystem, business service, or real-time API. The tool loop connects "the model decides what to call" with "the host program actually executes it".

A typical process is:

1. You put the available tool list into the current request.
2. The model returns `XLLM_STATUS_TOOL_CALL_REQUIRED`, including the tool name and JSON arguments.
3. The host program executes the matching tool.
4. The host sends the result back as a tool result in the context.
5. The model reads the tool result and produces the final answer.

xllm supports two usage styles:

| Style | Best For |
| --- | --- |
| Manual loop | You want full control over each request, tool result format, loop limit, and audit record |
| Automatic tool loop | You want xllm to complete "model requests tool -> execute tool -> ask model again" inside one `xllm_send` or `xllm_session_chat` call |

When learning, first understand the manual-loop concept, then use the automatic tool loop.

## Three Core Objects

### `xllm_tool_def`

`xllm_tool_def` is the declaration that tells the model which tools are available.

Common fields:

| Field | Meaning |
| --- | --- |
| `sToolId` | Stable internal tool ID used by the host |
| `sWireName` | Tool name sent to the provider; this is usually what the model sees |
| `sDescription` | Tool description shown to the model |
| `eKind` | Tool kind, commonly `XLLM_TOOL_CLIENT` |
| `tInputSchema` | JSON schema for tool arguments, represented with `xvalue` |

A minimal definition can start with ID, wire name, and description:

```c
xllm_tool_def tTool;
memset(&tTool, 0, sizeof(tTool));

tTool.sToolId = "app.weather.get_current";
tTool.sWireName = "get_weather";
tTool.sDescription = "Get current weather";

xllm_turn_add_tool(&tTurn, &tTool);
```

In production, add `tInputSchema` so the model knows the argument structure and required fields.

### `xllm_output_tool_call`

When the model chooses to call a tool, the response contains tool calls:

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

Map `sToolId` or `sToolName` to the actual function in your host program, then parse `sArgumentsJson` into tool arguments. Do not directly concatenate the JSON string into shell commands or SQL.

### `xllm_tool_executor`

Automatic tool loops need an executor. The executor is a callback provided by the host. xllm calls it after receiving a tool call:

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

For brevity, the example returns `"sunny"` directly. In a real application, validate `sArgumentsJson`, call the external service, and return the result as a text or JSON part.

## Automatic Tool Loop

The key step for automatic tool loops is: create the `xllm` object first, then set its executor.

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

Then send a turn with tools:

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

If the first model response returns a tool call, xllm calls your executor, puts the result into the following request, and continues until it gets final text. `examples/smoke_auto_tool_loop.c` shows the complete flow.

A session can also install an executor:

```c
xllm_tool_executor tExecutor;
memset(&tExecutor, 0, sizeof(tExecutor));
tExecutor.pfnExecute = weather_execute;

xllm_session_set_tool_executor(pSession, &tExecutor);
```

This lets the same chat session reuse tools across multiple turns.

## Profiles Must Declare Tool Capabilities

Tool loops depend on model capabilities. The model capability flags in the profile should include at least:

```c
tProfile.tModels.tText.tCaps.uFlags =
    XLLM_CAP_TEXT_IN |
    XLLM_CAP_TEXT_OUT |
    XLLM_CAP_TOOL_CALL_OUT |
    XLLM_CAP_TOOL_RESULT_IN;
```

If the provider supports parallel tool calls, you can also declare `XLLM_CAP_PARALLEL_TOOL_CALL`. If capability flags are missing, the request may fail validation, or the provider may not receive the correct tool declaration.

## Tool Choice

By default, the model may decide whether to use tools. You can also set a policy on the turn:

```c
xllm_turn_set_tool_choice(
    &tTurn,
    XLLM_TOOL_CHOICE_REQUIRED,
    NULL,
    false
);
```

Common modes:

| Mode | Meaning |
| --- | --- |
| `XLLM_TOOL_CHOICE_AUTO` | Let the model decide; this is the most common mode |
| `XLLM_TOOL_CHOICE_NONE` | Disable tool calls for this turn |
| `XLLM_TOOL_CHOICE_REQUIRED` | Require the model to choose a tool this turn |
| `XLLM_TOOL_CHOICE_NAMED` | Require a specific tool this turn |

Forced or named tool calls are reliable only when the provider and model support the related capability. When adapting across providers, check the capability matrix and smoke examples first.

## Events and Traces

Two observation surfaces are useful in tool loops:

- Event callback: observe real-time events such as `XLLM_EVENT_TOOL_CALL_READY` and `XLLM_EVENT_TEXT_DELTA`.
- Trace callback: observe `XLLM_TRACE_TOOL_LOOP`, which helps debug each tool loop step.

Event callback example:

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

If you are building an agent, also record tool call arguments, tool execution status, a tool result summary, and the final answer. This makes debugging much easier.

## Common Mistakes

Do not define only the tool name and forget the executor. Without an executor, a normal send can stop at `XLLM_STATUS_TOOL_CALL_REQUIRED`. That is not a failure; it means the model is waiting for the host to execute the tool.

Do not trust `sArgumentsJson`. It comes from model output and must be validated against the schema, especially for paths, URLs, SQL, shell commands, amounts, and user IDs.

Do not return overly long tool results. Tool results enter the next model context. Large results increase cost and may trigger context overflow. When needed, return only a summary, ID, or paginated result.

Do not forget to free content allocated inside `xllm_tool_exec_result`. The automatic loop handles executor results at the appropriate time internally. If you write a manual loop, follow the ownership rules in the API page.

Do not ignore loop limits. Real agents should limit the maximum number of tool rounds to avoid repeated calls when the model or tool is in an error state.

## Next Steps

- To inspect every tool structure and executor field, read [Tools API](../api/api-tools.en.md).
- To put tool loops into multi-turn chat, read [Session Introduction](session-intro.en.md).
- To see a complete agent scenario, later read [Tool Loop Agent Case](../case/tool-loop-agent.en.md).
