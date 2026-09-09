# xllm 编排与工具循环

> 状态：中文初稿，待人工审阅。

`xwork` 不替代 `xllm`。`xllm` 负责模型 runtime、provider、session、memory 和工具调用协议；`xwork` 负责把这些模型能力放进可审批、可恢复、可持久化的 Agent run。

## 编排器负责什么

| 能力 | 说明 |
| --- | --- |
| Model turn | 将 run instruction、workspace、profile 和工具定义组织为一次模型调用。 |
| Tool loop | 读取模型返回的 tool call，执行本地或远程工具，再把结果送回模型。 |
| Approval pause | 当策略要求人工审批时，run 暂停并暴露 `xwork_approval_request`。 |
| Event forwarding | 将模型流式事件、工具事件、审批事件和取消事件写入 run event 流。 |
| Artifact synthesis | 对文件、patch、命令、终端和报告类结果生成 artifact。 |
| Cancel / interrupt | 通过 xllm cancel token、async cancel 和 host tool 上下文进行协作式取消。 |

## 最小闭环

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

## xllm bootstrap 示例

如果调用方希望 xwork 创建并持有 xllm runtime，可以通过 `xwork_xllm_bootstrap_options` 提供模型 profile：

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

如果产品已经创建了 `xllm_runtime`，则应使用 `tRuntime.pLlmRuntime` 借给 xwork，不能同时设置 `pLlmBootstrap`。

## Orchestrator options 示例

```c
xwork_orchestrator_options tOptions;

xwork_orchestrator_options_init(&tOptions);
tOptions.eModelStreamMode = XWORK_MODEL_STREAM_PREFER;
tOptions.iMaxTurns = 8;
tOptions.bAutoApprove = false;
tOptions.eToolChoiceMode = XWORK_TOOL_CHOICE_AUTO;
```

`xwork_run_execute_async()` 会 shallow-copy `tOptions`，其中 callback、user data、profile strings 和 caller-owned cancel token 必须活到 async handle 完成或销毁。

## 审批暂停语义

当模型请求的工具触发策略拦截时，编排器不会绕过工具执行。它会：

- 记录工具调用和审批请求。
- 将 run 推进到可恢复的暂停状态。
- 等待调用方提交 approval decision。
- 在恢复后使用已持久化的工具参数继续执行。

这使 AI IDE 可以把审批显示为 UI 操作，也使 claw 可以按 profile 策略自动审批低风险动作。

## 与 xllm 的边界

| xllm | xwork |
| --- | --- |
| provider 协议适配 | 工作流编排 |
| request/response 标准化 | run 状态机 |
| stream 事件解析 | event 持久化和产品观察 |
| session / memory 原语 | workspace memory 接入策略 |
| tool call 协议 | tool 执行、审批、artifact 和恢复 |

## 相关范例

- [AI IDE Agent 范例](../case/ai-ide-agent.md)
- [claw 自主 Agent 范例](../case/claw-autonomous-agent.md)
- [xllm Integration API](../api/api-xllm-integration.md)
