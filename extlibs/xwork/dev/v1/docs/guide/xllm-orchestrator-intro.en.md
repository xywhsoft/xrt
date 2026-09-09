# xllm orchestration and tool loop

>Status: First draft in Chinese, awaiting manual review.

`xwork` does not replace `xllm`. `xllm` is responsible for the model runtime, provider, session, memory, and tool calling protocols; `xwork` is responsible for putting these model capabilities into an agent run that can be approved, restored, and persisted.

## What is the orchestrator responsible for?

| Capabilities | Description |
| --- | --- |
| Model turn | Organizes run instructions, workspace, profile, and tool definitions into a single model call. |
| Tool loop | Read the tool call returned by the model, execute the local or remote tool, and then send the result back to the model. |
| Approval pause | When a policy requires manual approval, the run pauses and exposes `xwork_approval_request`. |
| Event forwarding | Writes model streaming events, tool events, approval events, and cancellation events to the run event stream. |
| Artifact synthesis | Generate artifacts from files, patches, commands, terminals, and report results. |
| Cancel / interrupt | Cooperative cancellation via xllm cancel token, async cancel and host tool context. |

## Minimum closed loop

```text
create runtime with xllm runtime or bootstrap options
register workspace
register builtin host tools
create run
execute orchestrator
  model turn
  tool call
  policy check
  host execution or approval pause
  tool result
  next model turn
complete run
query summary / artifacts / events
```

## xllm bootstrap example

If the caller wants xwork to create and hold the xllm runtime, it can provide the model profile via `xwork_xllm_bootstrap_options`:

```c
xwork_xllm_profile_options tLlmProfile;
xwork_xllm_bootstrap_options tBootstrap;
xwork_runtime_options tRuntime;

xwork_xllm_profile_options_init(&tLlmProfile);
xwork_xllm_bootstrap_options_init(&tBootstrap);
xwork_runtime_options_init(&tRuntime);

tLlmProfile.sProfileId = "default";
tLlmProfile.sProvider = "openai_compat";
tLlmProfile.sAdapter = XWORK_XLLM_ADAPTER_OPENAI_COMPAT;
tLlmProfile.sBaseUrl = "https://provider.example/v1";
tLlmProfile.sModelId = "model-id";
tLlmProfile.sApiKey = "...";

tBootstrap.pProfiles = &tLlmProfile;
tBootstrap.iProfileCount = 1;
tBootstrap.eRedactMode = XWORK_XLLM_REDACT_STRICT;

tRuntime.pLlmBootstrap = &tBootstrap;
```

If the product has created `xllm_runtime`, it should be lent to xwork using `tRuntime.pLlmRuntime`, and `pLlmBootstrap` cannot be set at the same time.

## Orchestrator options example

```c
xwork_orchestrator_options tOptions;

xwork_orchestrator_options_init(&tOptions);
tOptions.eModelStreamMode = XWORK_MODEL_STREAM_PREFER;
tOptions.iMaxTurns = 8;
tOptions.bAutoApprove = false;
tOptions.eToolChoiceMode = XWORK_TOOL_CHOICE_AUTO;
```

`xwork_run_execute_async()` will shallow-copy `tOptions`, where callback, user data, profile strings, and caller-owned cancel token must survive until the async handle completes or is destroyed.

## Approval suspension semantics

The orchestrator does not bypass tool execution when a tool requested by a model triggers a policy interception. It will:

- Log tool calls and approval requests.
- Advance run to a resumable paused state.
- Wait for the caller to submit an approval decision.
- Continue execution after recovery using persisted tool parameters.

This allows the AI ​​IDE to display approvals as UI operations, and also allows claw to automatically approve low-risk actions based on profile policies.

## Boundary with xllm

| xllm | xwork |
| --- | --- |
| provider protocol adaptation | workflow orchestration |
| request/response standardization | run state machine |
| stream event analysis | event persistence and product observation |
| session / memory primitive | workspace memory access strategy |
| tool call protocol | tool execution, approval, artifact and recovery |

## Related examples

- [AI IDE Agent Example](../case/ai-ide-agent.md)
- [claw Autonomous Agent Example](../case/claw-autonomous-agent.md)
- [xllm Integration API](../api/api-xllm-integration.md)
