# Orchestrator API

>Status: First draft in Chinese, awaiting review.

Model turn + tool loop on Orchestrator API driver `xwork_run`. It strings xllm model invocation, tool execution, approval suspension, artifact generation and cancellation semantics into a resumable Agent execution closed loop.

## Related Statements

- `xwork_orchestrator_options`
- `xwork_model_stream_mode`
- `xwork_model_event`
- `xwork_model_event_fn`
- `xwork_tool_choice_mode`
- `xwork_run_execute()`
- `xwork_run_execute_async()`
- `xwork_run_submit_approval()`
- `xwork_run_resume()`

## Module positioning

The Orchestrator is responsible for organizing execution processes around xllm. It is not responsible for provider protocol adaptation, does not have a complete planner built in, does not directly implement the UI, and does not bypass policy execution side effects.

## Execute closed loop

```text
run_execute
  prepare model input
  call xllm model turn
  forward stream events
  receive model output/tool calls
  evaluate policy
  execute tool or pause for approval
  emit events/artifacts/checkpoints
  continue next turn until final output or budget exhausted
```

## Function-by-function description

### xwork_orchestrator_options_init

Initialize orchestrator options.

**Function:**

Prepare model-turn + tool-loop configuration for use by `xwork_run_execute` / `xwork_run_execute_async`.

**Function prototype:**

```c
XWORK_API void xwork_orchestrator_options_init(xwork_orchestrator_options *pOptions);
```

**parameter:**

- `pOptions`: options to initialize; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated; inputs such as profile id, tool choice, callback user data, etc. are used according to the corresponding API rules during execution.

**Additional Note:**

After initialization, set the model profile, turn budget, stream callback, tool choice and execution strategy. Reentrancy is not allowed for the same run during execution.

**Example code:**

```c
xwork_orchestrator_options opts;
xwork_orchestrator_options_init(&opts);
opts.iMaxTurns = 8;
```

**Related API:**

- `xwork_run_execute`
- `xwork_run_execute_async`

---

## Model stream event

`xwork_model_event` is a model streaming event that is transparently transmitted and normalized from xllm. Common fields:

| Field | Description |
| --- | --- |
| `eType` | The underlying xllm event type. |
| `sText` | Text, thinking, refusal, or error content. |
| `sResponseId` / `sModel` | Response and model metadata. |
| `sToolCallId` / `sToolId` / `sToolName` | tool call metadata. |
| `sArgumentsDelta` | tool arguments increment. |
| `sArtifactId` / `pArtifactData` | artifact streaming data. |

The callback returning `false` will cancel the current model turn and propagate through `XWORK_ERROR_CANCELLED`.

Deprioritize:

- interrupt/cancel token check precedes user event callback.
- After the user event callback returns `false`, the current model turn is canceled.

## Tool loop and approval

When a model requests tools, the orchestrator:

1. Find tool definition.
2. Evaluate whether to allow based on side effect, approval mode and policy.
3. Execute tool executor or host service on allowed calls.
4. Create `xwork_approval_request` for calls that require approval and pause the run.
5. After resuming, continue execution using the saved tool arguments.

This allows the AI ​​IDE to approve patch, command, or terminal operations in the UI, and also allows claw to automatically approve low-risk actions based on the profile.

## Tool choice

The orchestrator supports passing tool selection intent into the model:

| Mode | Description |
| --- | --- |
| `XWORK_TOOL_CHOICE_AUTO` | Model selected by yourself. |
| `XWORK_TOOL_CHOICE_NONE` | Disable tool calls. |
| `XWORK_TOOL_CHOICE_REQUIRED` | Requests the model to call a tool. |
| `XWORK_TOOL_CHOICE_NAMED` | Specifies the tool. |

The specific fields are configured in `xwork_orchestrator_options`.

## Synchronous execution

```c
xwork_status status = xwork_run_execute(pRun, &tOptions);
if (status == XWORK_ERROR_PAUSED) {
    /* Query the approval request, submit the approval decision, then resume. */
}
```

## Asynchronous execution

Asynchronous execution is provided by the Run API:

```c
xwork_run_async *pAsync = NULL;
xwork_run_execute_async(pRun, &tOptions, &pAsync);
```

async cancel will enter the same set of collaborative cancel token paths, and tool executors and host services should check the context.

## Error code

- `XWORK_ERROR_INVALID_ARGUMENT`: Invalid options, run, or callback parameters.
- `XWORK_ERROR_INVALID_STATE`: The run status does not allow execution, or the same run can be re-entered for execution.
- `XWORK_ERROR_NOT_FOUND`: The model requested an unregistered tool.
- `XWORK_ERROR_PAUSED`: Requires approval or side-effect-blocking record mode suspension.
- `XWORK_ERROR_CANCELLED`: cancel token, interrupt, callback false or async cancel.
- `XWORK_ERROR_EXTERNAL_FAILURE`: xllm provider, host service, persistence or xrt failed.

## Thread boundaries

There can only be one orchestrator execution entry for the same run at the same time. During asynchronous execution, the caller can only observe, wait, or cancel through the async handle and should not directly mutation run.

## Restore boundaries

The orchestrator can restore pending tool, approval decision, and checkpoint related states from the run snapshot. Recovery requires the caller to re-provide runtime, workspace, tool registry, host service, persistence backend and xllm runtime/profile.

## Related documents

- [Run API](api-run.md)
- [Tool API](api-tools.md)
- [xllm Orchestration and Tool Loop](../guide/xllm-orchestrator-intro.md)
- [AI IDE Agent Example](../case/ai-ide-agent.md)
