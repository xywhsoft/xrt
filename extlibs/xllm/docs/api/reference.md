# xllm public API reference

此文件由 `tools/generate_api_reference.py` 从 `extlibs/xllm/config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[../../README.md](../../README.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `68` 个函数、`73` 个常量或宏、
`34` 个公共类型。

## `extlibs/xllm/include/xllm/api.h`

[查看带契约注释的公共头](../../include/xllm/api.h)

### 函数 (68)

- `xllmCallCancel`
- `xllmCallDestroy`
- `xllmCallFuture`
- `xllmCallWait`
- `xllmClientBuildRequestJson`
- `xllmClientComplete`
- `xllmClientConfigInit`
- `xllmClientCreate`
- `xllmClientDestroy`
- `xllmClientGetModelProfile`
- `xllmClientSetHooks`
- `xllmClientSetModelProfile`
- `xllmClientStart`
- `xllmErrorCodeName`
- `xllmErrorInit`
- `xllmErrorRetryable`
- `xllmEstimateMessageTokens`
- `xllmEstimateTextTokens`
- `xllmFinishReasonName`
- `xllmFree`
- `xllmHistoryAdd`
- `xllmHistoryAddFromResponse`
- `xllmHistoryAddText`
- `xllmHistoryAddToolResult`
- `xllmHistoryAppendInto`
- `xllmHistoryAt`
- `xllmHistoryCount`
- `xllmHistoryCreate`
- `xllmHistoryDestroy`
- `xllmHistoryRemove`
- `xllmMessageAddPart`
- `xllmMessageAddToolCall`
- `xllmMessageFromResponse`
- `xllmMessageInit`
- `xllmMessageSetContent`
- `xllmMessageSetNative`
- `xllmMessageSetReasoning`
- `xllmMessageSetToolCallId`
- `xllmMessageUnit`
- `xllmModelProfileBuiltin`
- `xllmModelProfileInit`
- `xllmModelProfileSupports`
- `xllmModelProfileValidate`
- `xllmModelProfileValidateRequest`
- `xllmPartInit`
- `xllmPartSetAudioData`
- `xllmPartSetFileData`
- `xllmPartSetImageData`
- `xllmPartSetImageUrl`
- `xllmPartSetNative`
- `xllmPartSetText`
- `xllmPartUnit`
- `xllmRequestAddMessage`
- `xllmRequestAddMessageView`
- `xllmRequestAddTextMessage`
- `xllmRequestAddTool`
- `xllmRequestAddToolResult`
- `xllmRequestInit`
- `xllmRequestSetCancel`
- `xllmRequestSetDeadline`
- `xllmRequestSetExtraBody`
- `xllmRequestSetModel`
- `xllmRequestSetReasoningEffort`
- `xllmRequestSetStop`
- `xllmRequestSetToolChoice`
- `xllmRequestSetToolsView`
- `xllmRequestUnit`
- `xllmResponseDestroy`

### 常量与宏 (73)

- `XLLM_BLOCK_REASONING`
- `XLLM_BLOCK_TEXT`
- `XLLM_BLOCK_TOOL_CALL`
- `XLLM_CAP_DEVELOPER_ROLE`
- `XLLM_CAP_IMAGE_IN`
- `XLLM_CAP_JSON_OUT`
- `XLLM_CAP_MAX_COMPLETION_TOKENS`
- `XLLM_CAP_PARALLEL_TOOL_CALL`
- `XLLM_CAP_REASONING_CONTROL`
- `XLLM_CAP_REASONING_OUT`
- `XLLM_CAP_STREAM`
- `XLLM_CAP_TEXT_IN`
- `XLLM_CAP_TEXT_OUT`
- `XLLM_CAP_TOOL_CALL_OUT`
- `XLLM_CAP_TOOL_RESULT_IN`
- `XLLM_ERROR_AUTH`
- `XLLM_ERROR_CANCELLED`
- `XLLM_ERROR_HOOK`
- `XLLM_ERROR_INVALID_ARGUMENT`
- `XLLM_ERROR_LIMIT`
- `XLLM_ERROR_MODEL_NOT_FOUND`
- `XLLM_ERROR_NETWORK`
- `XLLM_ERROR_NONE`
- `XLLM_ERROR_OUT_OF_MEMORY`
- `XLLM_ERROR_PARSE`
- `XLLM_ERROR_PROTOCOL`
- `XLLM_ERROR_RATE_LIMIT`
- `XLLM_ERROR_TIMEOUT`
- `XLLM_ERROR_UPSTREAM`
- `XLLM_EVENT_BLOCK_META`
- `XLLM_EVENT_REASONING_DELTA`
- `XLLM_EVENT_RESPONSE_DONE`
- `XLLM_EVENT_RESPONSE_START`
- `XLLM_EVENT_TEXT_DELTA`
- `XLLM_EVENT_TOOL_CALL_DELTA`
- `XLLM_EVENT_USAGE`
- `XLLM_FEATURE_XLLM`
- `XLLM_FINISH_CONTENT_FILTER`
- `XLLM_FINISH_LENGTH`
- `XLLM_FINISH_OTHER`
- `XLLM_FINISH_REFUSAL`
- `XLLM_FINISH_STOP`
- `XLLM_FINISH_TOOL_CALLS`
- `XLLM_JSON_NONE`
- `XLLM_JSON_OBJECT`
- `XLLM_PART_AUDIO`
- `XLLM_PART_FILE`
- `XLLM_PART_IMAGE`
- `XLLM_PART_NATIVE`
- `XLLM_PART_REASONING`
- `XLLM_PART_TEXT`
- `XLLM_PROVIDER_ANTHROPIC`
- `XLLM_PROVIDER_GLM`
- `XLLM_PROVIDER_OPENAI_COMPAT`
- `XLLM_PROVIDER_OPENAI_RESPONSES`
- `XLLM_RESULT_CANCELLED`
- `XLLM_RESULT_ERROR`
- `XLLM_RESULT_OK`
- `XLLM_RESULT_TIMEOUT`
- `XLLM_ROLE_ASSISTANT`
- `XLLM_ROLE_SYSTEM`
- `XLLM_ROLE_TOOL`
- `XLLM_ROLE_USER`
- `XLLM_TOOL_CHOICE_AUTO`
- `XLLM_TOOL_CHOICE_NAMED`
- `XLLM_TOOL_CHOICE_NONE`
- `XLLM_TOOL_CHOICE_REQUIRED`
- `XLLM_VERSION_MAJOR`
- `XLLM_VERSION_MINOR`
- `XLLM_VERSION_PATCH`
- `XLLM_WINDOW_SHARED_CONTEXT`
- `XLLM_WINDOW_SPLIT_INPUT_OUTPUT`
- `XLLM_WINDOW_UNSPECIFIED`

### 类型 (34)

- `xllm_block`
- `xllm_block_kind`
- `xllm_call`
- `xllm_capability_flags`
- `xllm_client`
- `xllm_client_config`
- `xllm_diagnostics`
- `xllm_error`
- `xllm_error_code`
- `xllm_event`
- `xllm_event_fn`
- `xllm_event_kind`
- `xllm_finish`
- `xllm_header`
- `xllm_history`
- `xllm_hooks`
- `xllm_json_mode`
- `xllm_message`
- `xllm_model_profile`
- `xllm_part`
- `xllm_part_kind`
- `xllm_provider`
- `xllm_request`
- `xllm_response`
- `xllm_result`
- `xllm_role`
- `xllm_stats`
- `xllm_stream_callbacks`
- `xllm_tool`
- `xllm_tool_call`
- `xllm_tool_choice`
- `xllm_usage`
- `xllm_window_mode`
- `xllm_wire`
