# xwork public API reference

此文件由 `tools/generate_api_reference.py` 从 `extlibs/xwork/config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[../../README.md](../../README.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `42` 个函数、`59` 个常量或宏、
`37` 个公共类型。

## `extlibs/xwork/include/xwork/api.h`

[查看带契约注释的公共头](../../include/xwork/api.h)

### 函数 (42)

- `xworkAgentCancel`
- `xworkAgentCompact`
- `xworkAgentConfigInit`
- `xworkAgentCreate`
- `xworkAgentDestroy`
- `xworkAgentRegisterBuiltinReadOnlyTools`
- `xworkAgentRegisterBuiltinTools`
- `xworkAgentRegisterSubagentType`
- `xworkAgentRegisterTool`
- `xworkAgentResume`
- `xworkAgentRun`
- `xworkAgentRunBegin`
- `xworkAgentRunEnd`
- `xworkAgentRunReadOnlySubagent`
- `xworkAgentSubagentTypeCount`
- `xworkAgentTakeTaskNotices`
- `xworkAgentToolAt`
- `xworkAgentToolCount`
- `xworkAgentToolRegistryGeneration`
- `xworkAgentUnregisterSubagentType`
- `xworkAgentUnregisterTool`
- `xworkAgentUnregisterToolsBySource`
- `xworkAgentWorkspaceRoot`
- `xworkErrorCodeName`
- `xworkErrorInit`
- `xworkExecutorBind`
- `xworkExecutorUnbind`
- `xworkMcpClientCallTool`
- `xworkMcpClientConnect`
- `xworkMcpClientCreate`
- `xworkMcpClientDestroy`
- `xworkMcpClientGetInfo`
- `xworkMcpClientRefreshTools`
- `xworkMcpStdioConfigInit`
- `xworkPathIsProtected`
- `xworkReadOnlySubagentConfigInit`
- `xworkRunResultUnit`
- `xworkTaskWatchdog`
- `xworkToolOutputInit`
- `xworkToolOutputSet`
- `xworkToolOutputSetImage`
- `xworkToolOutputUnit`

### 常量与宏 (59)

- `XWORK_APPROVAL_AUTO`
- `XWORK_APPROVAL_CALLBACK`
- `XWORK_APPROVAL_READ_ONLY`
- `XWORK_EOL_AUTO`
- `XWORK_EOL_FORCE_CRLF`
- `XWORK_EOL_FORCE_LF`
- `XWORK_EOL_PRESERVE`
- `XWORK_ERROR_CANCELLED`
- `XWORK_ERROR_CONTEXT`
- `XWORK_ERROR_INVALID_ARGUMENT`
- `XWORK_ERROR_IO`
- `XWORK_ERROR_LOOP_GUARD`
- `XWORK_ERROR_MODEL`
- `XWORK_ERROR_NONE`
- `XWORK_ERROR_OUT_OF_MEMORY`
- `XWORK_ERROR_POLICY`
- `XWORK_ERROR_TIMEOUT`
- `XWORK_ERROR_TOOL`
- `XWORK_EVENT_AGENT_DONE`
- `XWORK_EVENT_AGENT_START`
- `XWORK_EVENT_COMPACTION_DONE`
- `XWORK_EVENT_COMPACTION_REJECTED`
- `XWORK_EVENT_COMPACTION_START`
- `XWORK_EVENT_ERROR`
- `XWORK_EVENT_MODEL_DONE`
- `XWORK_EVENT_MODEL_REASONING_DELTA`
- `XWORK_EVENT_MODEL_START`
- `XWORK_EVENT_MODEL_TEXT_DELTA`
- `XWORK_EVENT_TOOL_DONE`
- `XWORK_EVENT_TOOL_START`
- `XWORK_FEATURE_XWORK`
- `XWORK_HOOK_AFTER_TOOL`
- `XWORK_HOOK_BEFORE_TOOL`
- `XWORK_HOOK_CANCEL`
- `XWORK_HOOK_CONTINUE`
- `XWORK_HOOK_DENY`
- `XWORK_PERMISSION_ALLOW`
- `XWORK_PERMISSION_DEFAULT`
- `XWORK_PERMISSION_DENY`
- `XWORK_RESOURCE_COMMAND`
- `XWORK_RESOURCE_NONE`
- `XWORK_RESOURCE_PATH`
- `XWORK_RESOURCE_PROCESS`
- `XWORK_RESULT_CANCELLED`
- `XWORK_RESULT_ERROR`
- `XWORK_RESULT_LIMIT`
- `XWORK_RESULT_OK`
- `XWORK_RESULT_TIMEOUT`
- `XWORK_RISK_HIGH`
- `XWORK_RISK_LOW`
- `XWORK_RISK_MEDIUM`
- `XWORK_TASK_AGENT`
- `XWORK_TASK_PROCESS`
- `XWORK_TOOL_EFFECT_PROCESS`
- `XWORK_TOOL_EFFECT_READ_ONLY`
- `XWORK_TOOL_EFFECT_WORKSPACE_WRITE`
- `XWORK_VERSION_MAJOR`
- `XWORK_VERSION_MINOR`
- `XWORK_VERSION_PATCH`

### 类型 (37)

- `xwork_agent`
- `xwork_agent_config`
- `xwork_approval_fn`
- `xwork_approval_mode`
- `xwork_eol_policy`
- `xwork_error`
- `xwork_error_code`
- `xwork_event`
- `xwork_event_fn`
- `xwork_event_kind`
- `xwork_executor_state`
- `xwork_hook_action`
- `xwork_hook_event`
- `xwork_hook_fn`
- `xwork_hook_phase`
- `xwork_mcp_client`
- `xwork_mcp_info`
- `xwork_mcp_stdio_config`
- `xwork_model_complete_fn`
- `xwork_permission_decision`
- `xwork_permission_fn`
- `xwork_permission_request`
- `xwork_readonly_subagent_config`
- `xwork_resource_kind`
- `xwork_result`
- `xwork_risk_level`
- `xwork_run_result`
- `xwork_subagent_type`
- `xwork_task_kind`
- `xwork_task_notice`
- `xwork_tool_context`
- `xwork_tool_definition`
- `xwork_tool_effect`
- `xwork_tool_execute_fn`
- `xwork_tool_info`
- `xwork_tool_output`
- `xwork_watchdog_digest`
