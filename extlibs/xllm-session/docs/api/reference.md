# xllm-session public API reference

此文件由 `tools/generate_api_reference.py` 从 `extlibs/xllm-session/config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[../../README.md](../../README.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `85` 个函数、`50` 个常量或宏、
`38` 个公共类型。

## `extlibs/xllm-session/include/xllm-session/api.h`

[查看带契约注释的公共头](../../include/xllm-session/api.h)

### 函数 (85)

- `xllmSessionAbortModelCall`
- `xllmSessionAddAssistantResponse`
- `xllmSessionAddMessage`
- `xllmSessionAddReference`
- `xllmSessionAddText`
- `xllmSessionAddToolResult`
- `xllmSessionAddToolResultWithImage`
- `xllmSessionApplyProfile`
- `xllmSessionBeginModelCall`
- `xllmSessionBeginTurn`
- `xllmSessionBindClient`
- `xllmSessionBudgetInit`
- `xllmSessionBuildRequest`
- `xllmSessionBuildRequestView`
- `xllmSessionCheckpoint`
- `xllmSessionClear`
- `xllmSessionCommitCompaction`
- `xllmSessionComplete`
- `xllmSessionComputeOutputReserve`
- `xllmSessionComputeSafetyReserve`
- `xllmSessionConfigInit`
- `xllmSessionConfigInitFromProfile`
- `xllmSessionCreate`
- `xllmSessionCreateBound`
- `xllmSessionCreateForProfile`
- `xllmSessionCreateForTest`
- `xllmSessionCurrentTurn`
- `xllmSessionDefaultCompactionOps`
- `xllmSessionDestroy`
- `xllmSessionDisableJournal`
- `xllmSessionDispatchRequest`
- `xllmSessionEnableJournal`
- `xllmSessionEntryAt`
- `xllmSessionEntryCount`
- `xllmSessionFlushJournal`
- `xllmSessionFork`
- `xllmSessionForkAt`
- `xllmSessionForwardDriver`
- `xllmSessionGetBudget`
- `xllmSessionGetConfig`
- `xllmSessionGetFileLedger`
- `xllmSessionGetLastPersistenceError`
- `xllmSessionGetState`
- `xllmSessionGetStats`
- `xllmSessionGetSummary`
- `xllmSessionGetSystemPrompt`
- `xllmSessionGetTail`
- `xllmSessionHasDriver`
- `xllmSessionJournalPath`
- `xllmSessionLastSequence`
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
- `xllmSessionRestore`
- `xllmSessionRestoreOptionsInit`
- `xllmSessionRunWithTools`
- `xllmSessionSave`
- `xllmSessionSend`
- `xllmSessionSetCompactionOps`
- `xllmSessionSetHooks`
- `xllmSessionSetSystemPrompt`
- `xllmSessionSetTestCall`
- `xllmSessionSnapshotBuildRequest`
- `xllmSessionSnapshotCreate`
- `xllmSessionSnapshotCurrentTurn`
- `xllmSessionSnapshotGetBudget`
- `xllmSessionSnapshotGetConfig`
- `xllmSessionSnapshotGetFileLedger`
- `xllmSessionSnapshotGetState`
- `xllmSessionSnapshotGetStats`
- `xllmSessionSnapshotGetSummary`
- `xllmSessionSnapshotGetTail`
- `xllmSessionSnapshotPendingToolCallAt`
- `xllmSessionSnapshotPendingToolCallCount`
- `xllmSessionSnapshotRelease`
- `xllmSessionSnapshotRetain`
- `xllmSessionStateName`
- `xllmSessionTruncateAfter`

### 常量与宏 (50)

- `XLLM_SESSION_ABI_VERSION`
- `XLLM_SESSION_BUDGET_ABI_VERSION`
- `XLLM_SESSION_DEFAULT_CONTEXT_WINDOW_TOKENS`
- `XLLM_SESSION_DEFAULT_MAX_OUTPUT_TOKENS`
- `XLLM_SESSION_DURABILITY_APPEND`
- `XLLM_SESSION_DURABILITY_FLUSH`
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
- `XLLM_SESSION_EVENT_SESSION_CLEARED`
- `XLLM_SESSION_EVENT_SESSION_FORKED`
- `XLLM_SESSION_EVENT_SESSION_RECOVERED`
- `XLLM_SESSION_EVENT_SESSION_TRUNCATED`
- `XLLM_SESSION_EVENT_TURN_BEGIN`
- `XLLM_SESSION_EVENT_TURN_END`
- `XLLM_SESSION_FEATURE_XLLM_SESSION`
- `XLLM_SESSION_PERSISTENCE_SCHEMA_VERSION`
- `XLLM_SESSION_PRESSURE_COMPACT`
- `XLLM_SESSION_PRESSURE_NONE`
- `XLLM_SESSION_PRESSURE_OVERFLOW`
- `XLLM_SESSION_PRESSURE_PRUNE`
- `XLLM_SESSION_RESTORE_ABI_VERSION`
- `XLLM_SESSION_STATE_COMPACTION_COMMITTING`
- `XLLM_SESSION_STATE_COMPACTION_IN_FLIGHT`
- `XLLM_SESSION_STATE_COMPACTION_PREPARED`
- `XLLM_SESSION_STATE_CORRUPT`
- `XLLM_SESSION_STATE_IDLE`
- `XLLM_SESSION_STATE_MODEL_IN_FLIGHT`
- `XLLM_SESSION_STATE_RECOVERY_REQUIRED`
- `XLLM_SESSION_STATE_TOOLS_PARTIAL`
- `XLLM_SESSION_STATE_TOOLS_PENDING`
- `XLLM_SESSION_STATE_TURN_COMPLETE`
- `XLLM_SESSION_STATE_USER_APPENDED`
- `XLLM_SESSION_VERSION_MAJOR`
- `XLLM_SESSION_VERSION_MINOR`
- `XLLM_SESSION_VERSION_PATCH`
- `XLLM_SESSION_VERSION_PRERELEASE`
- `XLLM_SESSION_VERSION_STRING`

### 类型 (38)

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
- `xllm_model_profile`
- `xllm_pending_tool_call`
- `xllm_render_action`
- `xllm_request`
- `xllm_response`
- `xllm_result`
- `xllm_role`
- `xllm_run_policy`
- `xllm_run_summary`
- `xllm_session`
- `xllm_session_budget`
- `xllm_session_config`
- `xllm_session_durability`
- `xllm_session_entry_view`
- `xllm_session_event`
- `xllm_session_event_type`
- `xllm_session_hooks`
- `xllm_session_pressure`
- `xllm_session_restore_options`
- `xllm_session_snapshot`
- `xllm_session_state`
- `xllm_session_stats`
- `xllm_session_summary`
- `xllm_session_tail`
- `xllm_stream_callbacks`
- `xllm_test_call_proc`
- `xllm_usage`
- `xllm_window_mode`
