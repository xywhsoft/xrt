# xllm-session public API reference

此文件由 `tools/generate_api_reference.py` 从 `extlibs/xllm-session/config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[../../README.md](../../README.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `49` 个函数、`26` 个常量或宏、
`30` 个公共类型。

## `extlibs/xllm-session/include/xllm-session/api.h`

[查看带契约注释的公共头](../../include/xllm-session/api.h)

### 函数 (49)

- `xllmSessionAddAssistantResponse`
- `xllmSessionAddMessage`
- `xllmSessionAddReference`
- `xllmSessionAddText`
- `xllmSessionAddToolResult`
- `xllmSessionAddToolResultWithImage`
- `xllmSessionBeginTurn`
- `xllmSessionBindClient`
- `xllmSessionBuildRequest`
- `xllmSessionBuildRequestView`
- `xllmSessionCheckpoint`
- `xllmSessionCommitCompaction`
- `xllmSessionComplete`
- `xllmSessionComputeOutputReserve`
- `xllmSessionComputeSafetyReserve`
- `xllmSessionConfigInit`
- `xllmSessionCreate`
- `xllmSessionCreateBound`
- `xllmSessionCreateForTest`
- `xllmSessionCurrentTurn`
- `xllmSessionDefaultCompactionOps`
- `xllmSessionDestroy`
- `xllmSessionDisableJournal`
- `xllmSessionEnableJournal`
- `xllmSessionFork`
- `xllmSessionForwardDriver`
- `xllmSessionGetConfig`
- `xllmSessionGetFileLedger`
- `xllmSessionGetStats`
- `xllmSessionGetSummary`
- `xllmSessionGetTail`
- `xllmSessionJournalPath`
- `xllmSessionLoad`
- `xllmSessionMaybeCompact`
- `xllmSessionNoteFileModified`
- `xllmSessionNoteFileRead`
- `xllmSessionOverflowLadder`
- `xllmSessionPendingToolCallAt`
- `xllmSessionPendingToolCallCount`
- `xllmSessionPrepareCompaction`
- `xllmSessionRecordUsage`
- `xllmSessionRecover`
- `xllmSessionRunWithTools`
- `xllmSessionSave`
- `xllmSessionSend`
- `xllmSessionSetCompactionOps`
- `xllmSessionSetHooks`
- `xllmSessionSetSystemPrompt`
- `xllmSessionSetTestCall`

### 常量与宏 (26)

- `XLLM_SESSION_DEFAULT_CONTEXT_WINDOW_TOKENS`
- `XLLM_SESSION_DEFAULT_MAX_OUTPUT_TOKENS`
- `XLLM_SESSION_ENTRY_PINNED`
- `XLLM_SESSION_ENTRY_SYNTHETIC`
- `XLLM_SESSION_EVENT_CHECKPOINT_SAVED`
- `XLLM_SESSION_EVENT_COMPACT_ABORT`
- `XLLM_SESSION_EVENT_COMPACT_COMMIT`
- `XLLM_SESSION_EVENT_COMPACT_EVALUATE`
- `XLLM_SESSION_EVENT_COMPACT_PLAN`
- `XLLM_SESSION_EVENT_COMPACT_PREPARE`
- `XLLM_SESSION_EVENT_COMPACT_PROMPT`
- `XLLM_SESSION_EVENT_COMPACT_SUMMARY`
- `XLLM_SESSION_EVENT_ENTRY_ADDED`
- `XLLM_SESSION_EVENT_FILL_UPDATED`
- `XLLM_SESSION_EVENT_JOURNAL_RECORD`
- `XLLM_SESSION_EVENT_LADDER_TRUNCATE`
- `XLLM_SESSION_EVENT_PRESSURE_CHANGED`
- `XLLM_SESSION_EVENT_SESSION_FORKED`
- `XLLM_SESSION_EVENT_SESSION_RECOVERED`
- `XLLM_SESSION_EVENT_TURN_BEGIN`
- `XLLM_SESSION_EVENT_TURN_END`
- `XLLM_SESSION_FEATURE_XLLM_SESSION`
- `XLLM_SESSION_PRESSURE_COMPACT`
- `XLLM_SESSION_PRESSURE_NONE`
- `XLLM_SESSION_PRESSURE_OVERFLOW`
- `XLLM_SESSION_PRESSURE_PRUNE`

### 类型 (30)

- `xllm_client`
- `xllm_compact_decision`
- `xllm_compaction`
- `xllm_compaction_ops`
- `xllm_compaction_plan`
- `xllm_compaction_quality`
- `xllm_error`
- `xllm_executor`
- `xllm_file_ledger`
- `xllm_message`
- `xllm_pending_tool_call`
- `xllm_render_action`
- `xllm_request`
- `xllm_response`
- `xllm_result`
- `xllm_role`
- `xllm_run_policy`
- `xllm_run_summary`
- `xllm_session`
- `xllm_session_config`
- `xllm_session_event`
- `xllm_session_event_type`
- `xllm_session_hooks`
- `xllm_session_pressure`
- `xllm_session_stats`
- `xllm_session_summary`
- `xllm_session_tail`
- `xllm_stream_callbacks`
- `xllm_test_call_proc`
- `xllm_usage`
