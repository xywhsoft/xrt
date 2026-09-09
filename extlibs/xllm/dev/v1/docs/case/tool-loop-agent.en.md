# Building a Minimal Agent with Tool Loop

This case shows a minimal agent loop: the model first reads memory context, then emits a tool call, the host executes the tool, and finally the model answers based on the tool result.

[Back to Case Studies](README.en.md) | [Tool Loop Introduction](../guide/tool-loop-intro.en.md) | [Tools API](../api/api-tools.en.md)

## Problem

Many agents are not simple chat applications. They need to "check context first, call a tool, then synthesize an answer". For example, a code assistant should inspect workspace state before editing; an operations assistant should query service health before answering; a business assistant may need to read internal system data.

This case uses a minimal structure to show four parts of an agent loop:

- `xllm_session`: stores the current conversation.
- `xllm_memory`: provides long-term policy or project context.
- `xllm_tool_def`: tells the model which tools are available.
- `xllm_tool_executor`: is where the host actually executes tools.

## Architecture

```text
user input
  -> memory search/apply
  -> session chat
    -> model returns tool call
    -> xllm calls host executor
    -> tool result returns to model context
    -> model returns final answer
  -> optional: write this turn summary back to memory
```

The complete example is:

```text
examples/agent_loop/agent_loop.c
```

## Step 1: Declare a Tool-Capable Profile

Tool calling requires the profile to declare tool-related capabilities:

```c
tProfile.tModels.tText.tCaps.uFlags =
    XLLM_CAP_TEXT_IN |
    XLLM_CAP_TEXT_OUT |
    XLLM_CAP_TOOL_CALL_OUT |
    XLLM_CAP_TOOL_RESULT_IN;
```

Without `XLLM_CAP_TOOL_CALL_OUT`, the model cannot emit tool calls. Without `XLLM_CAP_TOOL_RESULT_IN`, tool results cannot reliably be sent back to the model.

## Step 2: Ingest Agent Memory

The example writes one policy into `XLLM_MEMORY_SCOPE_MEMORY`:

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

This simulates "the long-term memory already contains agent behavior constraints". Real applications can write user preferences, project policies, task rules, or safety boundaries into memory.

## Step 3: Create a Session and Install the Executor

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

The executor is the host program's tool execution entry point. The model only decides which tool to call and with what arguments; actual execution must happen in the host.

## Step 4: Define the Tool

```c
xllm_tool_def tTool;
memset(&tTool, 0, sizeof(tTool));

tTool.sToolId = "xwork.workspace.inspect";
tTool.sWireName = "inspect_workspace";
tTool.sDescription = "Inspect workspace state using the host xwork-like executor.";

xllm_turn_add_tool(&tTurn, &tTool);
```

In production, add `tInputSchema` so the model knows the argument structure. The example omits JSON schema to keep the flow focused.

## Step 5: Inject Memory into the Turn

Before calling the model, use the user turn as the query source, search memory, and inject the result into the same turn:

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

This lets the model see the long-term memory "inspect workspace before editing" before it decides whether to call a tool.

## Step 6: Execute the Agent Loop

```c
xllm_response *pResponse = NULL;

xllm_session_chat_ex(pSession, &tTurn, NULL, &pResponse, &tError);

printf("%s\n", xllm_response_get_text(pResponse));
```

Because the session has an executor installed, `xllm_session_chat_ex` can internally complete:

1. The model returns an `inspect_workspace` tool call.
2. xllm calls `xwork_like_execute`.
3. The executor returns `workspace_status=clean; executor=xwork-like`.
4. xllm places the tool result into the following model request.
5. The model generates the final answer.

## Minimal Executor Implementation

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

A real executor should do more:

- Validate `sArgumentsJson`.
- Restrict accessible paths and commands.
- Set timeouts.
- Record audit logs.
- Return short and clear results.

## Step 7: Write the Result Back to Memory

After the agent finishes, you can explicitly write a summary back to memory:

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

This step is explicit ingestion. Do not write every agent process into long-term memory by default; save only information that is valuable later.

## Key APIs

| API | Purpose |
| --- | --- |
| `xllm_session_create` | Create an agent session with history |
| `xllm_session_set_tool_executor` | Install an automatic tool executor |
| `xllm_turn_add_tool` | Provide tools for the current turn |
| `xllm_memory_search_and_apply_from_turn_to_turn` | Retrieve and inject long-term memory |
| `xllm_session_chat_ex` | Execute the model request and automatic tool loop |
| `xllm_memory_ingest_turn_response` | Write this turn summary back to memory |

## Extension Points

You can extend this minimal agent into:

- Multi-tool agent: read files, search symbols, run tests.
- Async executor: return a future when tool execution takes longer.
- Approval-based agent: require user confirmation before dangerous tools run.
- Workspace RAG agent: give the model both code fragments and tool results.
- Persistent session: export/import session state.

## Common Questions

Do not let the model execute commands directly. The model only emits tool calls; the host must validate and execute them.

Do not make tool results too long. The longer the result, the more context it consumes, and the easier it is to affect the final answer.

Do not omit tool capability flags. Automatic loops require the model to support both tool call output and tool result input.

Do not permanently write every agent process into memory. Long-term memory should be filtered, summarized, and controlled by user policy.
