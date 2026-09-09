# AI IDE / xwork Agent Loop Integration

This case shows how an AI IDE host can combine session, memory, tool execution, permissions, and diagnostics into a controlled agent loop.

[Back to Case Studies](README.en.md) | [Tool Loop Agent](tool-loop-agent.en.md) | [Workspace RAG](workspace-rag.en.md)

## Problem

The model inside an AI IDE cannot only "chat". It usually needs to:

- Understand the current workspace.
- Retrieve relevant code and documentation.
- Call host tools to inspect state, read files, or run tests.
- Follow user permissions and approval rules.
- Produce diagnosable logs and traces when something fails.

This case combines the earlier capabilities into a minimal integration blueprint for an AI IDE.

Related examples:

```text
examples/agent_loop/agent_loop.c
examples/ai_ide_memory/ai_ide_memory.c
```

## Architecture

```text
AI IDE UI
  -> current user turn
  -> workspace memory search/apply
  -> conversation memory search/apply
  -> xllm_session_chat_ex
     -> model tool call
     -> permission gate
     -> xwork-like executor
     -> tool result
     -> final answer
  -> diagnostics/log/trace
  -> optional memory write-back
```

This is not "letting the model control the IDE". The host program controls the IDE. The model only proposes tool calls; the host validates, authorizes, and executes them.

## Component Responsibilities

| Component | Responsibility |
| --- | --- |
| `xllm_session` | Stores current short-term chat history |
| `xllm_memory` | Stores workspace index and long-term conversation memory |
| `xllm_tool_def` | Declares IDE capabilities to the model |
| `xllm_tool_executor` | Executes tools in the host |
| permission gate | Decides whether a tool call needs user confirmation |
| diagnostics | Records errors, traces, and tool loop state |

## Step 1: Index the Workspace

When the AI IDE starts or opens a workspace, first index text files that are allowed to be shown to the model:

```c
xllm_memory_ingest_workspace_options tWorkspace;

xllm_memory_ingest_workspace_options_init(&tWorkspace);
tWorkspace.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
tWorkspace.sPath = sWorkspaceRoot;
tWorkspace.bSkipHidden = true;
tWorkspace.sRecordIdPrefix = "workspace";
tWorkspace.sSourceUriPrefix = "workspace://";
tWorkspace.uMaxFileBytes = 4096u;
tWorkspace.uChunkChars = 512u;

xllm_memory_ingest_workspace(pMemory, &tWorkspace, &tWorkspaceResult, &tError);
```

The example `examples/ai_ide_memory/ai_ide_memory.c` writes a mock workspace and verifies that `.env` does not enter context.

## Step 2: Inject Workspace Context by User Question

```c
xllm_memory_search_options tSearch;
xllm_memory_context_options tContext;

xllm_memory_search_options_init(&tSearch);
xllm_memory_context_options_init(&tContext);

tSearch.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
tSearch.sQuery = "How should the AI IDE inject workspace memory before provider execution?";
tSearch.uMaxHits = 2u;
tSearch.uMaxCharsPerHit = 512u;

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

The injected text should contain source URI, chunk ID, and byte range. This lets the UI show "which file fragment the answer referred to".

## Step 3: Declare IDE Tools

Tool declarations should be small and clear. For example:

```c
xllm_tool_def tTool;
memset(&tTool, 0, sizeof(tTool));

tTool.sToolId = "xwork.workspace.inspect";
tTool.sWireName = "inspect_workspace";
tTool.sDescription = "Inspect workspace state using the host xwork-like executor.";

xllm_turn_add_tool(&tTurn, &tTool);
```

Common IDE tools can include:

| Tool | Risk |
| --- | --- |
| `workspace.inspect` | Low, reads state |
| `file.read` | Medium, may read sensitive files |
| `file.patch` | High, modifies workspace |
| `test.run` | Medium to high, executes commands |
| `git.diff` | Low to medium, reads changes |

High-risk tools should require user confirmation by default.

## Step 4: Install the Executor

```c
xllm_tool_executor tExecutor;

memset(&tExecutor, 0, sizeof(tExecutor));
tExecutor.pCtx = &tState;
tExecutor.pfnExecute = xwork_like_execute;

xllm_session_set_tool_executor(pSession, &tExecutor);
```

Do not trust model arguments directly inside the executor. Recommended execution order:

1. Parse `sArgumentsJson`.
2. Validate tool ID and argument schema.
3. Check permissions and workspace boundaries.
4. Request user confirmation if there is risk.
5. Execute the tool.
6. Return a short result.
7. Record an audit log.

## Step 5: Execute the Session Agent Loop

```c
xllm_session_chat_ex(
    pSession,
    &tTurn,
    &tCallOptions,
    &pResponse,
    &tError
);
```

If the model returns a tool call, xllm calls the executor and puts the tool result into the following request. Read the final response with `xllm_response_get_text`.

An AI IDE UI usually also shows:

- Which tool the model wants to call.
- Tool arguments.
- Whether the user approved it.
- Tool execution result summary.
- Final answer.

## Step 6: Write Back Valuable Long-Term Memory

After an agent loop, you can write useful long-term information back to memory:

```c
xllm_memory_ingest_turn_response_options tWrite;

xllm_memory_ingest_turn_response_options_init(&tWrite);
tWrite.eExtractionPolicy = XLLM_MEMORY_EXTRACTION_POLICY_SUMMARY;
tWrite.pTurn = &tTurn;
tWrite.pResponse = pResponse;
tWrite.sConversationId = "agent-loop-demo";
tWrite.sTurnId = "turn-001";
tWrite.sRecordId = "agent-loop-summary";
tWrite.sSummaryText = "summary: Agent inspected workspace before editing.";
tWrite.bReplaceExisting = true;

xllm_memory_ingest_turn_response(pMemory, &tWrite, &tError);
```

Do not write every tool log into long-term memory. Long-term memory should be concise, traceable, and useful later.

## Diagnostics Integration

AI IDE scenarios should attach both logs and traces:

```c
xllm_runtime_set_log_callback(pRuntime, on_log, pLogCtx);
xllm_runtime_set_trace_callback(pRuntime, on_trace, pTraceCtx);
```

Key traces:

| Trace | Use |
| --- | --- |
| `XLLM_TRACE_REQUEST` | Inspect the final context injected into the model |
| `XLLM_TRACE_TOOL_LOOP` | Inspect each tool loop round |
| `XLLM_TRACE_COMPACT` | Inspect session history compression |
| `XLLM_TRACE_RESPONSE` | Inspect model response parsing |

Sanitize logs and traces. Do not write API keys, `.env`, certificates, or full private files into diagnostic packages.

## Building Examples

Agent loop example:

```bat
cmd /c .\examples\agent_loop\build.bat
```

AI IDE memory example:

```bat
cmd /c .\examples\ai_ide_memory\build.bat
```

The scripts compile with gcc from the repository root:

```bat
gcc -std=c11 -Wall -Wextra -I. -Ilib -Ilib\sqlite ^
    -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION ^
    examples\agent_loop\agent_loop.c ^
    lib\sqlite\sqlite3.c ^
    -o build\agent_loop.exe ^
    -lws2_32 -liphlpapi -lshell32 -lcrypt32
```

## Key APIs

| API | Purpose |
| --- | --- |
| `xllm_memory_ingest_workspace` | Index a workspace |
| `xllm_memory_search_and_apply_to_request` | Inject workspace context |
| `xllm_session_create` | Create short-term conversation |
| `xllm_session_set_tool_executor` | Install tool executor |
| `xllm_turn_add_tool` | Provide tools for this turn |
| `xllm_session_chat_ex` | Execute the agent loop |
| `xllm_memory_ingest_turn_response` | Write back long-term memory |
| `xllm_runtime_set_trace_callback` | Attach diagnostics |

## Extension Points

Real AI IDEs can extend this with:

- RAG enhanced by language service results.
- Diff preview and user approval for file patch tools.
- Command allowlists for `test.run`.
- User confirmation before memory ingestion.
- Watcher workers for workspace sync.
- Citation UI for context injection.

## Common Questions

Do not let the model bypass host permissions. All tool execution must go through host validation.

Do not inject hidden files or keys into the model. Workspace indexing needs safe default filters.

Do not put unlimited tool results back into context. Tool results should be short, structured, and related to the current task.

Do not save only the final answer. When debugging agents, tool calls, arguments, execution status, and traces are just as important.
