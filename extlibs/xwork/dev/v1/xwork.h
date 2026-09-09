#ifndef XWORK_H
#define XWORK_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef XWORK_API
#define XWORK_API
#endif

#define XWORK_VERSION_MAJOR 0
#define XWORK_VERSION_MINOR 1
#define XWORK_VERSION_PATCH 0
#define XWORK_PERSISTENCE_FORMAT_VERSION 14
#define XWORK_REMOTE_PROTOCOL_VERSION_CURRENT 1

#define XWORK_PROFILE_XCODE "xcode"
#define XWORK_PROFILE_XCLAW "xclaw"
#define XWORK_XLLM_ADAPTER_OPENAI_COMPAT "openai_compat"
#define XWORK_XLLM_ADAPTER_GLM_NATIVE "glm_native"
#define XWORK_XLLM_ADAPTER_MINIMAX_NATIVE "minimax_native"
#define XWORK_XLLM_ADAPTER_KIMI_NATIVE "kimi_native"
#define XWORK_XLLM_ADAPTER_GEMINI_NATIVE "gemini_native"
#define XWORK_XLLM_ADAPTER_VERTEX_GEMINI_NATIVE "vertex_gemini_native"
#define XWORK_XLLM_ADAPTER_QWEN_NATIVE "qwen_native"
#define XWORK_XLLM_ADAPTER_DOUBAO_NATIVE "doubao_native"
#define XWORK_XLLM_ADAPTER_ANTHROPIC_NATIVE "anthropic_native"
#define XWORK_XLLM_ADAPTER_OLLAMA_NATIVE "ollama_native"
#define XWORK_TOOL_FILESYSTEM_READ_TEXT "filesystem.read_text"
#define XWORK_TOOL_FILESYSTEM_WRITE_TEXT "filesystem.write_text"
#define XWORK_TOOL_FILESYSTEM_LIST "filesystem.list"
#define XWORK_TOOL_FILESYSTEM_STAT "filesystem.stat"
#define XWORK_TOOL_FILESYSTEM_GLOB "filesystem.glob"
#define XWORK_TOOL_FILESYSTEM_MKDIR "filesystem.mkdir"
#define XWORK_TOOL_FILESYSTEM_MOVE "filesystem.move"
#define XWORK_TOOL_FILESYSTEM_DELETE "filesystem.delete"
#define XWORK_TOOL_FILESYSTEM_APPLY_PATCH "filesystem.apply_patch"
#define XWORK_TOOL_PROCESS_EXEC "process.exec"
#define XWORK_TOOL_PROCESS_START_TERMINAL "process.start_terminal"
#define XWORK_TOOL_PROCESS_LIST_TERMINALS "process.list_terminals"
#define XWORK_TOOL_PROCESS_TERMINAL_READ "process.terminal_read"
#define XWORK_TOOL_PROCESS_TERMINAL_WRITE "process.terminal_write"
#define XWORK_TOOL_PROCESS_TERMINAL_RESIZE "process.terminal_resize"
#define XWORK_TOOL_PROCESS_TERMINAL_STOP "process.terminal_stop"
#define XWORK_TOOL_VCS_STATUS "vcs.status"
#define XWORK_TOOL_VCS_DIFF "vcs.diff"
#define XWORK_TOOL_VCS_LOG "vcs.log"
#define XWORK_TOOL_VCS_BRANCH "vcs.branch"
#define XWORK_TOOL_EDITOR_OPEN_BUFFER "editor.open_buffer"
#define XWORK_TOOL_EDITOR_APPLY_EDIT "editor.apply_edit"
#define XWORK_HOST_FILESYSTEM_READ_TEXT "read_text"
#define XWORK_HOST_FILESYSTEM_WRITE_TEXT "write_text"
#define XWORK_HOST_FILESYSTEM_LIST "list"
#define XWORK_HOST_FILESYSTEM_STAT "stat"
#define XWORK_HOST_FILESYSTEM_GLOB "glob"
#define XWORK_HOST_FILESYSTEM_MKDIR "mkdir"
#define XWORK_HOST_FILESYSTEM_MOVE "move"
#define XWORK_HOST_FILESYSTEM_DELETE "delete"
#define XWORK_HOST_FILESYSTEM_APPLY_PATCH "apply_patch"
#define XWORK_HOST_PROCESS_EXEC "exec"
#define XWORK_HOST_PROCESS_START_TERMINAL "start_terminal"
#define XWORK_HOST_PROCESS_LIST_TERMINALS "list_terminals"
#define XWORK_HOST_PROCESS_TERMINAL_READ "terminal_read"
#define XWORK_HOST_PROCESS_TERMINAL_WRITE "terminal_write"
#define XWORK_HOST_PROCESS_TERMINAL_RESIZE "terminal_resize"
#define XWORK_HOST_PROCESS_TERMINAL_STOP "terminal_stop"
#define XWORK_HOST_VCS_STATUS "status"
#define XWORK_HOST_VCS_DIFF "diff"
#define XWORK_HOST_VCS_LOG "log"
#define XWORK_HOST_VCS_BRANCH "branch"
#define XWORK_HOST_DIAGNOSTICS_FROM_PROCESS "from_process_output"
#define XWORK_HOST_EDITOR_OPEN_BUFFER "open_buffer"
#define XWORK_HOST_EDITOR_APPLY_EDIT "apply_edit"
#define XWORK_REPLAY_FILESYSTEM_SNAPSHOT_REF "filesystem.snapshot_ref"
#define XWORK_REPORT_SCHEMA_V1 "xwork.report.v1"
#define XWORK_DIAGNOSTICS_SCHEMA_V1 "xwork.diagnostics.v1"
#define XWORK_PATCH_APPLY_RESULT_SCHEMA_V1 "xwork.patch_apply_result.v1"
#define XWORK_PATCH_FILE_SUMMARY_SCHEMA_V1 "xwork.patch_file_summary.v1"
#define XWORK_TERMINAL_STATE_SCHEMA_V1 "xwork.terminal_state.v1"
#define XWORK_TERMINAL_INVENTORY_SCHEMA_V1 "xwork.terminal_inventory.v1"

typedef struct xllm_runtime xllm_runtime;
typedef struct xllm_session xllm_session;
typedef struct xllm_memory xllm_memory;
typedef struct xllm_cancel_token xllm_cancel_token;

typedef struct xwork_runtime xwork_runtime;
typedef struct xwork_workspace xwork_workspace;
typedef struct xwork_run xwork_run;
typedef struct xwork_run_async xwork_run_async;
typedef struct xwork_agent xwork_agent;
typedef struct xwork_agent_pool xwork_agent_pool;
typedef struct xwork_task_graph xwork_task_graph;
typedef struct xwork_control_plane xwork_control_plane;
typedef struct xwork_worker xwork_worker;
typedef struct xwork_replay_engine xwork_replay_engine;
typedef struct xwork_event xwork_event;
typedef struct xwork_run_step xwork_run_step;
typedef struct xwork_run_step_list xwork_run_step_list;
typedef struct xwork_run_step_query xwork_run_step_query;
typedef struct xwork_approval_request xwork_approval_request;
typedef struct xwork_checkpoint xwork_checkpoint;
typedef struct xwork_artifact xwork_artifact;
typedef struct xwork_artifact_summary xwork_artifact_summary;
typedef struct xwork_profile xwork_profile;
typedef struct xwork_run_snapshot xwork_run_snapshot;
typedef struct xwork_persistence_backend xwork_persistence_backend;
typedef struct xwork_run_index_list xwork_run_index_list;
typedef struct xwork_run_index_query xwork_run_index_query;
typedef struct xwork_xllm_profile_options xwork_xllm_profile_options;
typedef struct xwork_xllm_bootstrap_options xwork_xllm_bootstrap_options;
typedef struct xwork_xllm_transport_options xwork_xllm_transport_options;
typedef struct xwork_host_invoke_context xwork_host_invoke_context;
typedef struct xwork_tool_exec_context xwork_tool_exec_context;

/*
 * Public API contract
 *
 * Object ownership:
 * - xwork_runtime_create() returns an owned runtime. Destroy it with
 *   xwork_runtime_destroy(). Destroying a runtime also destroys workspaces,
 *   registered tools, and runs that are still attached to that runtime.
 * - If xwork_runtime_options::pLlmBootstrap is used, xwork owns the created
 *   xllm_runtime and destroys it with the xwork_runtime. If pLlmRuntime is
 *   used instead, it is borrowed and must outlive the xwork_runtime.
 *   xwork_runtime_options::pReplayEngine is also borrowed and must outlive
 *   any runtime host-service call that records or replays through it.
 * - xwork_runtime_add_workspace() returns an owned workspace attached to the
 *   runtime. xwork copies workspace id/root strings but borrows pMemory.
 *   pMemory must outlive the workspace.
 * - xwork_run_create() and xwork_runtime_recover_run() return owned runs
 *   attached to the runtime. They copy public run strings and workspace ids.
 *   Destroy runs with xwork_run_destroy(), or let xwork_runtime_destroy()
 *   destroy still-attached runs.
 * - xwork_run_execute_async() returns an owned async handle. Destroy it with
 *   xwork_run_async_destroy(). The handle may own an internal cancel token;
 *   caller-provided cancel tokens are borrowed.
 * - xwork_agent_pool_create() returns an owned in-process agent pool. Destroy
 *   it with xwork_agent_pool_destroy(). Agents returned from
 *   xwork_agent_pool_add_agent() are owned by the pool and borrowed by callers.
 * - xwork_task_graph_create() returns an owned task graph. Destroy it with
 *   xwork_task_graph_destroy(). A graph borrows its agent pool, copies task
 *   strings/workspace ids, and maps each executing task to a child xwork_run.
 * - xwork_file_persistence_configure_backend() and
 *   xwork_local_host_configure_services() initialize caller-owned helper
 *   structs. Reset them with xwork_file_persistence_reset() and
 *   xwork_local_host_reset().
 *
 * Borrowed callback state:
 * - Host service callbacks, persistence backend callbacks, callback user data,
 *   pUserData fields, caller-owned xllm objects, and strings explicitly noted
 *   as borrowed must remain valid for the lifetime of the runtime, workspace,
 *   run, async handle, or callback invocation that references them.
 * - xwork_runtime_register_tool() deep-copies sToolId/sDisplayName/sDescription
 *   but borrows other pointer fields such as sOperationId.
 *
 * Output structs:
 * - *_init() zeroes caller-owned structs.
 * - Functions that fill result structs deep-copy string/list/object contents
 *   unless the function explicitly returns a borrowed pointer. Release filled
 *   results with the matching *_reset() function.
 * - Getter functions returning const char * or const xwork_tool_def * return
 *   borrowed pointers valid until the owning object is mutated or destroyed.
 *
 * Thread-safety:
 * - xwork objects are not generally thread-safe for concurrent mutation.
 *   Serialize access to a runtime/workspace/run unless an API explicitly says
 *   otherwise.
 * - xwork_run_async_* APIs synchronize access to the async handle status. They
 *   do not make arbitrary concurrent xwork_run mutation safe.
 * - While a run is executing synchronously or through an async handle, callers
 *   must not execute, destroy, or directly mutate the same run concurrently.
 *   A second execute entry on the same run returns XWORK_ERROR_INVALID_STATE.
 *
 * Recovery boundary:
 * - File persistence stores events, artifacts, checkpoint snapshots, and the
 *   latest run snapshot. xwork_runtime_recover_run_from_persistence() restores
 *   that latest snapshot, including pending tool calls and approval decisions.
 *   An approved pending tool can be resumed and executed again from stored
 *   arguments.
 * - OS process handles and local interactive terminal sessions are not
 *   rehydrated across process restarts. Persisted process/terminal artifacts
 *   are durable audit/output records; callers must rediscover or restart live
 *   host sessions after recovery.
 */

/*
 * Return status rules:
 * - XWORK_OK means the requested operation completed. For wait-with-timeout
 *   APIs, inspect their completion output parameter as documented.
 * - XWORK_ERROR_INVALID_ARGUMENT is used for bad caller input and invalid
 *   option combinations before the operation starts.
 * - XWORK_ERROR_INVALID_STATE is used when the input object is valid but its
 *   lifecycle state cannot accept the operation.
 * - XWORK_ERROR_NOT_FOUND and XWORK_ERROR_ALREADY_EXISTS are used for stable
 *   object identity or persistence identity lookups.
 * - XWORK_ERROR_UNSUPPORTED is used for disabled or unavailable capabilities.
 * - XWORK_ERROR_EXTERNAL_FAILURE preserves failures from xrt/xllm, host
 *   services, providers, filesystem/process/persistence, and callback errors
 *   that are not cancellation.
 * - XWORK_ERROR_CANCELLED means the operation observed cooperative
 *   cancellation and the run should be treated as cancelled, not failed.
 * - XWORK_ERROR_PAUSED means cooperative execution stopped at a resumable
 *   scheduler boundary and can continue after an explicit resume call.
 */
typedef enum {
    XWORK_OK = 0,

    /* A required pointer, id, enum value, or option combination is invalid. */
    XWORK_ERROR_INVALID_ARGUMENT,

    /* Allocation failed before the requested operation could complete. */
    XWORK_ERROR_NO_MEMORY,

    /* A uniquely identified runtime object already exists. */
    XWORK_ERROR_ALREADY_EXISTS,

    /* The requested object or persisted record does not exist. */
    XWORK_ERROR_NOT_FOUND,

    /* The object exists, but its current lifecycle state rejects the action. */
    XWORK_ERROR_INVALID_STATE,

    /* An external dependency failed: model provider, host service, file I/O, persistence, or xrt/xllm operation. */
    XWORK_ERROR_EXTERNAL_FAILURE,

    /* The requested capability is valid but not implemented or not enabled. */
    XWORK_ERROR_UNSUPPORTED,

    /* Execution was cancelled by a cancel token, interrupt check, timeout stop, or async cancellation. */
    XWORK_ERROR_CANCELLED,

    /* Execution paused cooperatively and can be resumed by the caller. */
    XWORK_ERROR_PAUSED
} xwork_status;

/*
 * Returns a stable string literal for an xwork_status value.
 * Unknown numeric values return "XWORK_STATUS_UNKNOWN".
 */
XWORK_API const char *xwork_version(void);
XWORK_API const char *xwork_status_cstr(xwork_status eStatus);

typedef enum {
    XWORK_AUTONOMY_MANUAL = 0,
    XWORK_AUTONOMY_SEMI_AUTO,
    XWORK_AUTONOMY_AUTO
} xwork_autonomy_mode;

typedef enum {
    XWORK_AGENT_ROLE_CUSTOM = 0,
    XWORK_AGENT_ROLE_PLANNER,
    XWORK_AGENT_ROLE_CODER,
    XWORK_AGENT_ROLE_REVIEWER,
    XWORK_AGENT_ROLE_TESTER,
    XWORK_AGENT_ROLE_RESEARCHER
} xwork_agent_role;

typedef enum {
    XWORK_TASK_PENDING = 0,
    XWORK_TASK_READY,
    XWORK_TASK_RUNNING,
    XWORK_TASK_BLOCKED,
    XWORK_TASK_COMPLETED,
    XWORK_TASK_FAILED,
    XWORK_TASK_CANCELLED,
    XWORK_TASK_SKIPPED
} xwork_task_state;

typedef enum {
    XWORK_TASK_FAILURE_FAIL_FAST = 0,
    XWORK_TASK_FAILURE_BEST_EFFORT,
    XWORK_TASK_FAILURE_REQUIRE_ALL
} xwork_task_failure_policy;

typedef enum {
    XWORK_HANDOFF_PENDING = 0,
    XWORK_HANDOFF_ACCEPTED,
    XWORK_HANDOFF_REJECTED,
    XWORK_HANDOFF_COMPLETED
} xwork_handoff_state;

typedef enum {
    XWORK_WORKER_REGISTERED = 0,
    XWORK_WORKER_ONLINE,
    XWORK_WORKER_STALE,
    XWORK_WORKER_OFFLINE,
    XWORK_WORKER_UNREGISTERED
} xwork_worker_state;

typedef enum {
    XWORK_REMOTE_TASK_QUEUED = 0,
    XWORK_REMOTE_TASK_ASSIGNED,
    XWORK_REMOTE_TASK_RUNNING,
    XWORK_REMOTE_TASK_COMPLETED,
    XWORK_REMOTE_TASK_FAILED,
    XWORK_REMOTE_TASK_CANCELLED,
    XWORK_REMOTE_TASK_ORPHANED
} xwork_remote_task_state;

typedef enum {
    XWORK_REMOTE_TRANSPORT_IN_PROCESS = 0,
    XWORK_REMOTE_TRANSPORT_HTTP_BOUNDARY
} xwork_remote_transport_kind;

typedef enum {
    XWORK_REMOTE_TASK_HOST_TOOL = 0,
    XWORK_REMOTE_TASK_PROCESS_EXEC
} xwork_remote_task_kind;

typedef enum {
    XWORK_REMOTE_OUTPUT_STDOUT = 0,
    XWORK_REMOTE_OUTPUT_STDERR
} xwork_remote_output_stream;

typedef enum {
    XWORK_REPLAY_MODE_RECORD = 0,
    XWORK_REPLAY_MODE_STRICT,
    XWORK_REPLAY_MODE_AUDIT
} xwork_replay_mode;

typedef enum {
    XWORK_REPLAY_ENTRY_MODEL = 0,
    XWORK_REPLAY_ENTRY_TOOL,
    XWORK_REPLAY_ENTRY_HOST_TOOL,
    XWORK_REPLAY_ENTRY_FILESYSTEM,
    XWORK_REPLAY_ENTRY_PROCESS,
    XWORK_REPLAY_ENTRY_TERMINAL,
    XWORK_REPLAY_ENTRY_ARTIFACT,
    XWORK_REPLAY_ENTRY_CHECKPOINT
} xwork_replay_entry_kind;

typedef enum {
    XWORK_REPLAY_DIVERGENCE_NONE = 0,
    XWORK_REPLAY_DIVERGENCE_MISSING_ENTRY,
    XWORK_REPLAY_DIVERGENCE_UNEXPECTED_ENTRY,
    XWORK_REPLAY_DIVERGENCE_KIND_MISMATCH,
    XWORK_REPLAY_DIVERGENCE_KEY_MISMATCH,
    XWORK_REPLAY_DIVERGENCE_REQUEST_MISMATCH,
    XWORK_REPLAY_DIVERGENCE_RESPONSE_MISMATCH,
    XWORK_REPLAY_DIVERGENCE_STATUS_MISMATCH,
    XWORK_REPLAY_DIVERGENCE_CONTENT_MISMATCH
} xwork_replay_divergence_kind;

typedef enum {
    XWORK_REPLAY_EVENT_GENERIC = 0,
    XWORK_REPLAY_EVENT_MODEL_STREAM,
    XWORK_REPLAY_EVENT_RUN_EVENT,
    XWORK_REPLAY_EVENT_TOOL_EVENT,
    XWORK_REPLAY_EVENT_TERMINAL_INTERACTION
} xwork_replay_event_kind;

typedef enum {
    XWORK_RUN_CREATED = 0,
    XWORK_RUN_READY,
    XWORK_RUN_RUNNING,
    XWORK_RUN_WAITING_APPROVAL,
    XWORK_RUN_WAITING_TOOL,
    XWORK_RUN_CHECKPOINTING,
    XWORK_RUN_PAUSED,
    XWORK_RUN_COMPLETED,
    XWORK_RUN_CANCELLED,
    XWORK_RUN_FAILED
} xwork_run_state;

typedef enum {
    XWORK_TOOL_HOST_SERVICE = 0,
    XWORK_TOOL_VIRTUAL,
    XWORK_TOOL_SUBTASK
} xwork_tool_kind;

typedef enum {
    XWORK_SIDE_EFFECT_READ_ONLY = 0,
    XWORK_SIDE_EFFECT_WORKSPACE_WRITE,
    XWORK_SIDE_EFFECT_PROCESS_EXEC,
    XWORK_SIDE_EFFECT_NETWORK_ACCESS,
    XWORK_SIDE_EFFECT_EXTERNAL_MUTATION
} xwork_tool_side_effect;

typedef enum {
    XWORK_APPROVAL_DEFAULT = 0,
    XWORK_APPROVAL_NEVER,
    XWORK_APPROVAL_ALWAYS,
    XWORK_APPROVAL_ON_DEMAND
} xwork_approval_mode;

typedef enum {
    XWORK_RISK_LOW = 0,
    XWORK_RISK_MEDIUM,
    XWORK_RISK_HIGH,
    XWORK_RISK_CRITICAL
} xwork_risk_level;

typedef enum {
    XWORK_APPROVAL_PENDING = 0,
    XWORK_APPROVAL_APPROVED,
    XWORK_APPROVAL_REJECTED,
    XWORK_APPROVAL_CANCELLED
} xwork_approval_state;

typedef enum {
    XWORK_CHECKPOINT_MANUAL = 0,
    XWORK_CHECKPOINT_MODEL_TURN,
    XWORK_CHECKPOINT_SESSION_COMPACTED,
    XWORK_CHECKPOINT_BEFORE_TOOL,
    XWORK_CHECKPOINT_AFTER_TOOL,
    XWORK_CHECKPOINT_TASK_GRAPH,
    XWORK_CHECKPOINT_COMPLETION
} xwork_checkpoint_kind;

typedef enum {
    XWORK_ARTIFACT_PATCH = 0,
    XWORK_ARTIFACT_REPORT,
    XWORK_ARTIFACT_COMMAND,
    XWORK_ARTIFACT_OUTPUT
} xwork_artifact_kind;

typedef enum {
    XWORK_ARTIFACT_OUTPUT_UNSPECIFIED = 0,
    XWORK_ARTIFACT_OUTPUT_TEXT,
    XWORK_ARTIFACT_OUTPUT_JSON,
    XWORK_ARTIFACT_OUTPUT_FILE_CONTENT,
    XWORK_ARTIFACT_OUTPUT_FILE_CHANGE,
    XWORK_ARTIFACT_OUTPUT_TERMINAL_STATE,
    XWORK_ARTIFACT_OUTPUT_TERMINAL_INVENTORY
} xwork_artifact_output_class;

typedef enum {
    XWORK_ARTIFACT_REPORT_UNSPECIFIED = 0,
    XWORK_ARTIFACT_REPORT_DOCUMENT,
    XWORK_ARTIFACT_REPORT_SUMMARY,
    XWORK_ARTIFACT_REPORT_PLAN,
    XWORK_ARTIFACT_REPORT_REVIEW,
    XWORK_ARTIFACT_REPORT_DIAGNOSTICS,
    XWORK_ARTIFACT_REPORT_PROGRESS,
    XWORK_ARTIFACT_REPORT_FINAL
} xwork_artifact_report_class;

#define XWORK_ARTIFACT_KIND_MASK(kind_) (1u << (unsigned int)(kind_))
#define XWORK_ARTIFACT_OUTPUT_CLASS_MASK(class_) (1u << (unsigned int)(class_))
#define XWORK_ARTIFACT_REPORT_CLASS_MASK(class_) (1u << (unsigned int)(class_))

typedef enum {
    XWORK_PLANNER_OFF = 0,
    XWORK_PLANNER_BOUNDARY
} xwork_planner_mode;

typedef enum {
    XWORK_TOOL_CHOICE_AUTO = 0,
    XWORK_TOOL_CHOICE_NONE,
    XWORK_TOOL_CHOICE_REQUIRED,
    XWORK_TOOL_CHOICE_NAMED
} xwork_tool_choice_mode;

typedef enum {
    XWORK_DIAGNOSTIC_NOTE = 0,
    XWORK_DIAGNOSTIC_WARNING,
    XWORK_DIAGNOSTIC_ERROR
} xwork_diagnostic_severity;

typedef enum {
    XWORK_EVENT_NONE = 0,
    XWORK_EVENT_RUN_CREATED,
    XWORK_EVENT_RUN_STARTED,
    XWORK_EVENT_MEMORY_CONTEXT_ATTACHED,
    XWORK_EVENT_MEMORY_RECORD_INGESTED,
    XWORK_EVENT_MODEL_TURN_STARTED,
    XWORK_EVENT_MODEL_TURN_COMPLETED,
    XWORK_EVENT_SESSION_COMPACTED,
    XWORK_EVENT_TOOL_CALL_REQUESTED,
    XWORK_EVENT_APPROVAL_REQUESTED,
    XWORK_EVENT_APPROVAL_RESOLVED,
    XWORK_EVENT_TOOL_EXEC_STARTED,
    XWORK_EVENT_TOOL_EXEC_COMPLETED,
    XWORK_EVENT_ARTIFACT_EMITTED,
    XWORK_EVENT_CHECKPOINT_SAVED,
    XWORK_EVENT_CHECKPOINT_LOADED,
    XWORK_EVENT_RETRY_SCHEDULED,
    XWORK_EVENT_AGENT_SPAWNED,
    XWORK_EVENT_AGENT_STARTED,
    XWORK_EVENT_AGENT_PAUSED,
    XWORK_EVENT_AGENT_COMPLETED,
    XWORK_EVENT_AGENT_FAILED,
    XWORK_EVENT_AGENT_CANCELLED,
    XWORK_EVENT_TASK_SCHEDULED,
    XWORK_EVENT_TASK_STARTED,
    XWORK_EVENT_TASK_JOINED,
    XWORK_EVENT_TASK_BLOCKED,
    XWORK_EVENT_TASK_UNBLOCKED,
    XWORK_EVENT_TASK_COMPLETED,
    XWORK_EVENT_TASK_FAILED,
    XWORK_EVENT_TASK_CANCELLED,
    XWORK_EVENT_HANDOFF_REQUESTED,
    XWORK_EVENT_HANDOFF_ACCEPTED,
    XWORK_EVENT_HANDOFF_REJECTED,
    XWORK_EVENT_HANDOFF_COMPLETED,
    XWORK_EVENT_RUN_PAUSED,
    XWORK_EVENT_RUN_COMPLETED,
    XWORK_EVENT_RUN_CANCELLED,
    XWORK_EVENT_RUN_FAILED
} xwork_event_kind;

typedef enum {
    XWORK_RUN_STEP_RUN_STATE = 0,
    XWORK_RUN_STEP_MEMORY,
    XWORK_RUN_STEP_MODEL_TURN,
    XWORK_RUN_STEP_TOOL_CALL,
    XWORK_RUN_STEP_APPROVAL,
    XWORK_RUN_STEP_TOOL_EXEC,
    XWORK_RUN_STEP_ARTIFACT,
    XWORK_RUN_STEP_RETRY,
    XWORK_RUN_STEP_CHECKPOINT
} xwork_run_step_kind;

typedef enum {
    XWORK_SESSION_COMPACT_TRUNCATE = 0,
    XWORK_SESSION_COMPACT_SUMMARIZE,
    XWORK_SESSION_COMPACT_CUSTOM
} xwork_session_compact_strategy;

typedef struct {
    const char *sCallId;
    const char *sToolId;
    const char *sArgumentsJson;
} xwork_tool_call;

typedef struct {
    const char *sOutputText;
    const char *sVisibleSummary;
    bool bRetryable;
} xwork_tool_result;

typedef enum {
    XWORK_HOST_NONE = 0,
    XWORK_HOST_FILESYSTEM,
    XWORK_HOST_PROCESS,
    XWORK_HOST_VCS,
    XWORK_HOST_NETWORK,
    XWORK_HOST_DIAGNOSTICS,
    XWORK_HOST_EDITOR
} xwork_host_service_kind;

typedef bool (*xwork_interrupt_check_fn)(
    const xwork_run *pRun,
    const char *sPhase,
    void *pUserData
);

struct xwork_host_invoke_context {
    const xwork_run *pRun;
    xllm_cancel_token *pCancelToken;
    xwork_interrupt_check_fn pfnShouldInterrupt;
    void *pInterruptUserData;
    const char *sPhase;
};

struct xwork_tool_exec_context {
    xllm_cancel_token *pCancelToken;
    xwork_interrupt_check_fn pfnShouldInterrupt;
    void *pInterruptUserData;
    const char *sPhase;
};

typedef xwork_status (*xwork_host_invoke_fn)(
    const char *sOperationId,
    const char *sRequestJson,
    xwork_tool_result *pResult,
    void *pUserData
);

typedef xwork_status (*xwork_host_invoke_ex_fn)(
    const char *sOperationId,
    const char *sRequestJson,
    const xwork_host_invoke_context *pContext,
    xwork_tool_result *pResult,
    void *pUserData
);

typedef struct {
    xwork_host_invoke_fn pfnInvoke;
    xwork_host_invoke_ex_fn pfnInvokeEx;
    void *pUserData;
} xwork_host_service;

typedef struct {
    xwork_host_service tFilesystem;
    xwork_host_service tProcess;
    xwork_host_service tVcs;
    xwork_host_service tNetwork;
    xwork_host_service tDiagnostics;
    xwork_host_service tEditor;
} xwork_host_services;

typedef struct {
    xwork_risk_level eAutoApproveRiskLimit;
    const char **psNetworkAllowHostPatterns;
    size_t iNetworkAllowHostPatternCount;
    const char **psNetworkDenyHostPatterns;
    size_t iNetworkDenyHostPatternCount;
    bool bDenyNetworkByDefault;
} xwork_policy_options;

typedef struct {
    xwork_autonomy_mode eAutonomy;
    xwork_approval_mode eApprovalMode;
    xwork_tool_side_effect eSideEffect;
    bool bAutoApproveRequested;
    bool bHasRiskLevelOverride;
    xwork_risk_level eRiskLevelOverride;
    const char *sRiskScopeOverride;
    const char *sRiskReasonOverride;
} xwork_approval_eval_input;

typedef struct {
    bool bRequiresApproval;
    bool bAutoApproved;
    xwork_risk_level eRiskLevel;
    const char *sScope;
    const char *sReason;
} xwork_approval_decision;

typedef struct {
    const char *sUrl;
    const char *sHost;
    bool bNetworkAccessRequested;
} xwork_network_policy_eval_input;

typedef struct {
    bool bAllowed;
    xwork_risk_level eRiskLevel;
    const char *sScope;
    const char *sReason;
} xwork_network_policy_decision;

typedef struct {
    const char *sText;
    size_t iWorkspaceCount;
} xwork_memory_context;

typedef struct {
    bool bEnableAutoCompact;
    double fCompactTriggerRatio;
    size_t iCompactTriggerTurns;
    size_t iReserveOutputTokens;
    size_t iKeepRecentTurns;
    bool bKeepActiveToolChain;
    xwork_session_compact_strategy eCompactStrategy;
} xwork_session_policy;

typedef enum {
    XWORK_XLLM_PROXY_UNSPECIFIED = 0,
    XWORK_XLLM_PROXY_NONE,
    XWORK_XLLM_PROXY_SOCKS5,
    XWORK_XLLM_PROXY_HTTP_CONNECT
} xwork_xllm_proxy_kind;

typedef enum {
    XWORK_XLLM_DEBUG_NONE = 0,
    XWORK_XLLM_DEBUG_HEADERS,
    XWORK_XLLM_DEBUG_BODY,
    XWORK_XLLM_DEBUG_WIRE
} xwork_xllm_debug_mode;

typedef enum {
    XWORK_XLLM_REDACT_DEFAULT = 0,
    XWORK_XLLM_REDACT_OFF,
    XWORK_XLLM_REDACT_STRICT
} xwork_xllm_redact_mode;

struct xwork_xllm_profile_options {
    const char *sProfileId;
    const char *sDisplayName;
    const char *sProvider;
    const char *sAdapter;
    const char *sBaseUrl;
    const char *sModelId;
    const char *sApiKey;
    const char *sOpenAIOrganizationId;
    const char *sOpenAIProjectId;
    const char *sAnthropicApiVersion;
    const char **psAnthropicBetaHeaders;
    size_t iAnthropicBetaHeaderCount;
    size_t iMaxOutputTokens;
};

struct xwork_xllm_transport_options {
    bool bSetConnectTimeoutMs;
    size_t iConnectTimeoutMs;
    bool bSetReadTimeoutMs;
    size_t iReadTimeoutMs;
    bool bSetVerifyPeer;
    bool bVerifyPeer;
    xwork_xllm_proxy_kind eProxyKind;
    const char *sProxyHost;
    bool bSetProxyPort;
    size_t iProxyPort;
    const char *sProxyUser;
    const char *sProxyPass;
    const char *sCaBundlePath;
    const char *sClientCertPath;
    const char *sClientKeyPath;
};

struct xwork_xllm_bootstrap_options {
    const xwork_xllm_profile_options *pProfiles;
    size_t iProfileCount;
    xwork_xllm_debug_mode eDebugMode;
    xwork_xllm_redact_mode eRedactMode;
    xwork_xllm_transport_options tTransportDefaults;
};

struct xwork_profile {
    const char *sProfileId;
    const char *sDisplayName;
    const char *sDescription;
    const char *sDefaultLlmProfileId;
    const char *sDefaultSessionProfileId;
    xwork_autonomy_mode eAutonomy;
    xwork_policy_options tPolicy;
    xwork_session_policy tSessionPolicy;
    size_t iDefaultMaxTurns;
    bool bDefaultAutoApprove;
    bool bEnableWorkspaceMemory;
    xwork_planner_mode ePlannerMode;
};

typedef struct {
    /*
     * pLlmRuntime is borrowed. pLlmBootstrap is consumed during create and
     * cannot be combined with pLlmRuntime.
     *
     * pReplayEngine is borrowed. When present, xwork_runtime_invoke_host_service*()
     * records host service results in record mode and serves them from the
     * cassette in strict/audit replay modes.
     *
     * pHostServices and pPersistenceBackend are copied by value; their
     * callback pointers and pUserData remain borrowed and must outlive the
     * runtime.
     */
    xllm_runtime *pLlmRuntime;
    const xwork_xllm_bootstrap_options *pLlmBootstrap;
    xwork_replay_engine *pReplayEngine;
    const xwork_host_services *pHostServices;
    const xwork_persistence_backend *pPersistenceBackend;
    xwork_policy_options tPolicy;
    void *pUserData;
} xwork_runtime_options;

typedef xwork_status (*xwork_tool_exec_fn)(
    xwork_run *pRun,
    const xwork_tool_call *pCall,
    xwork_tool_result *pResult,
    void *pUserData
);

typedef xwork_status (*xwork_tool_exec_ex_fn)(
    xwork_run *pRun,
    const xwork_tool_call *pCall,
    const xwork_tool_exec_context *pContext,
    xwork_tool_result *pResult,
    void *pUserData
);

typedef xwork_status (*xwork_memory_resolve_fn)(
    const xwork_run *pRun,
    xwork_memory_context *pContext,
    void *pUserData
);

typedef enum {
    XWORK_MODEL_STREAM_AUTO = 0,
    XWORK_MODEL_STREAM_OFF,
    XWORK_MODEL_STREAM_PREFER,
    XWORK_MODEL_STREAM_REQUIRE
} xwork_model_stream_mode;

/*
 * Model stream event passed to xwork_model_event_fn.
 *
 * eType is the underlying xllm_event_type value. xwork forwards the event
 * ordering produced by xllm and exposes stable v1 payload fields for common
 * agent UI cases:
 * - START: sResponseId, sModel
 * - TEXT_DELTA / THINKING_DELTA / REFUSAL / ERROR: sText
 * - THINKING_DELTA: sFormat
 * - TOOL_CALL_DELTA / TOOL_CALL_READY: sToolCallId, sToolId, sToolName,
 *   sArgumentsDelta
 * - ARTIFACT_*: sArtifactId, plus pArtifactData/iArtifactSize for chunks
 *
 * Other event kinds still pass through via eType so callers can count progress
 * events such as OUTPUT_BEGIN, USAGE, OUTPUT_END, and END without depending on
 * private xwork internals.
 */
typedef struct {
    int eType;
    bool bSynthetic;
    size_t iOutputIndex;
    const char *sText;
    const char *sFormat;
    const char *sResponseId;
    const char *sModel;
    const char *sToolCallId;
    const char *sToolId;
    const char *sToolName;
    const char *sArgumentsDelta;
    const char *sArtifactId;
    const void *pArtifactData;
    size_t iArtifactSize;
} xwork_model_event;

typedef bool (*xwork_model_event_fn)(
    xwork_run *pRun,
    const xwork_model_event *pEvent,
    void *pUserData
);

typedef struct {
    /*
     * sWorkspaceId, sRootPath and memory sync policy strings are copied.
     * pMemory is borrowed.
     *
     * Memory sync policy strings use xllm's delimiter convention:
     * comma, semicolon, pipe or whitespace separated tokens.
     * - sMemorySyncAllowedExtensions is an include list, e.g. ".c,.h,.md".
     * - sMemorySyncIgnoredDirectories excludes directory names.
     * - sMemorySyncIgnoredExtensions excludes extensions.
     * - sMemorySyncIgnoredPathPatterns excludes relative path substrings/patterns.
     * - sMemorySyncIgnoredFiles is forwarded to workspace sync for exact file ignores.
     * - iMemorySyncMaxFileBytes skips files above the limit when non-zero.
     *
     * Binary files are not ingested by xllm memory; callers should combine an
     * extension include list with max-file limits for deterministic agent memory
     * behavior.
     */
    const char *sWorkspaceId;
    const char *sRootPath;
    bool bEnableMemory;
    xllm_memory *pMemory;
    const char *sMemorySyncAllowedExtensions;
    const char *sMemorySyncIgnoredDirectories;
    const char *sMemorySyncIgnoredExtensions;
    const char *sMemorySyncIgnoredPathPatterns;
    const char *sMemorySyncIgnoredFiles;
    size_t iMemorySyncMaxFileBytes;
} xwork_workspace_options;

typedef struct {
    size_t iVisitedFileCount;
    size_t iIngestedFileCount;
    size_t iCreatedRecordCount;
    size_t iUpdatedRecordCount;
    size_t iSkippedFileCount;
    size_t iFailedFileCount;
    size_t iExaminedRecordCount;
    size_t iRemovedRecordCount;
} xwork_workspace_memory_sync_summary;

typedef struct {
    size_t iChangeCount;
    size_t iCreatedCount;
    size_t iUpdatedCount;
    size_t iRemovedCount;
    size_t iSkippedCount;
    size_t iFailedCount;
} xwork_workspace_memory_file_sync_summary;

typedef struct {
    /*
     * xwork_runtime_register_tool() copies sToolId/sDisplayName/sDescription.
     * sOperationId is borrowed and must remain valid while the tool is
     * registered. Builtin tool definitions use static storage.
     */
    const char *sToolId;
    const char *sDisplayName;
    const char *sDescription;
    xwork_tool_kind eKind;
    xwork_host_service_kind eHostService;
    const char *sOperationId;
    xwork_tool_side_effect eSideEffect;
    xwork_approval_mode eApprovalMode;
    bool bSupportsStreaming;
} xwork_tool_def;

typedef struct {
    /*
     * xwork_run_create() copies run ids, instruction, profile ids, and each
     * workspace id. Referenced workspaces must already be registered on the
     * runtime and remain alive for the run lifetime.
     */
    const char *sRunId;
    const char *sParentRunId;
    const char *sAgentId;
    const char *sTaskId;
    const char *sInstruction;
    const char *sLlmProfileId;
    const char *sSessionProfileId;
    const char **psWorkspaceIds;
    size_t iWorkspaceCount;
    xwork_autonomy_mode eAutonomy;
    xwork_session_policy tSessionPolicy;
} xwork_run_options;

typedef struct {
    const char *sPoolId;
    xwork_runtime *pRuntime;
} xwork_agent_pool_options;

typedef struct {
    const char *sAgentId;
    const char *sDisplayName;
    const char *sDescription;
    xwork_agent_role eRole;
    const char *sLlmProfileId;
    const char *sSessionProfileId;
    xwork_autonomy_mode eAutonomy;
    size_t iMaxTurns;
    size_t iTimeoutMs;
    size_t iMaxRetries;
} xwork_agent_options;

typedef struct {
    const char *sAgentId;
    const char *sDisplayName;
    const char *sDescription;
    xwork_agent_role eRole;
    const char *sLlmProfileId;
    const char *sSessionProfileId;
    xwork_autonomy_mode eAutonomy;
    size_t iMaxTurns;
    size_t iTimeoutMs;
    size_t iMaxRetries;
} xwork_agent_snapshot;

typedef struct {
    xwork_agent_snapshot *pItems;
    size_t iCount;
} xwork_agent_snapshot_list;

typedef struct {
    const char *sPoolId;
    xwork_agent_snapshot_list tAgents;
} xwork_agent_pool_snapshot;

typedef struct {
    const char *sTaskId;
    const char *sAgentId;
    const char *sRunId;
    const char *sParentRunId;
    const char *sInstruction;
    const char *sLlmProfileId;
    const char *sSessionProfileId;
    const char **psWorkspaceIds;
    size_t iWorkspaceCount;
    xwork_autonomy_mode eAutonomy;
    xwork_session_policy tSessionPolicy;
    void *pUserData;
} xwork_task_node_options;

typedef struct {
    const char *sTaskId;
    const char *sAgentId;
    const char *sRunId;
    const char *sParentRunId;
    const char *sInstruction;
    xwork_task_state eState;
    xwork_status iStatus;
    size_t iDependencyCount;
    size_t iAttemptCount;
    size_t iMaxTurns;
    size_t iTimeoutMs;
    size_t iMaxRetries;
    void *pUserData;
} xwork_task_node_summary;

typedef struct {
    xwork_task_node_summary *pItems;
    size_t iCount;
} xwork_task_node_summary_list;

typedef struct {
    const char *sTaskId;
    const char *sAgentId;
    const char *sRunId;
    const char *sParentRunId;
    const char *sInstruction;
    const char *sLlmProfileId;
    const char *sSessionProfileId;
    const char **psWorkspaceIds;
    size_t iWorkspaceCount;
    const char **psDependencyTaskIds;
    size_t iDependencyCount;
    xwork_autonomy_mode eAutonomy;
    xwork_session_policy tSessionPolicy;
    xwork_task_state eState;
    xwork_status iStatus;
    size_t iAttemptCount;
    size_t iMaxTurns;
    size_t iTimeoutMs;
    size_t iMaxRetries;
} xwork_task_node_snapshot;

typedef struct {
    xwork_task_node_snapshot *pItems;
    size_t iCount;
} xwork_task_node_snapshot_list;

typedef xwork_status (*xwork_task_execute_fn)(
    xwork_run *pRun,
    const xwork_task_node_summary *pNode,
    void *pUserData
);

typedef struct {
    const char *sGraphId;
    xwork_agent_pool *pAgentPool;
    size_t iMaxConcurrency;
    xwork_task_failure_policy eFailurePolicy;
    xllm_cancel_token *pCancelToken;
    xwork_task_execute_fn pfnExecute;
    void *pUserData;
} xwork_task_graph_options;

typedef struct {
    xwork_status iStatus;
    size_t iTotalCount;
    size_t iCompletedCount;
    size_t iFailedCount;
    size_t iCancelledCount;
    size_t iSkippedCount;
} xwork_task_graph_result;

typedef struct {
    const char *sHandoffId;
    const char *sFromTaskId;
    const char *sToTaskId;
    const char *sReason;
    const char **psArtifactRefs;
    size_t iArtifactRefCount;
    const char **psMemoryContextRefs;
    size_t iMemoryContextRefCount;
    const char **psSharedWorkspaceIds;
    size_t iSharedWorkspaceCount;
    bool bReadOnlySharedContext;
    bool bWritableWorkspace;
} xwork_handoff_request_options;

typedef struct {
    const char *sHandoffId;
    xwork_handoff_state eState;
    xwork_status iStatus;
    const char *sMessage;
} xwork_handoff_result_options;

typedef struct {
    const char *sHandoffId;
    const char *sFromTaskId;
    const char *sToTaskId;
    const char *sReason;
    xwork_handoff_state eState;
    xwork_status iStatus;
    const char *sMessage;
    const char **psArtifactRefs;
    size_t iArtifactRefCount;
    const char **psMemoryContextRefs;
    size_t iMemoryContextRefCount;
    const char **psSharedWorkspaceIds;
    size_t iSharedWorkspaceCount;
    bool bReadOnlySharedContext;
    bool bWritableWorkspace;
} xwork_handoff_summary;

typedef struct {
    xwork_handoff_summary *pItems;
    size_t iCount;
} xwork_handoff_summary_list;

typedef struct {
    const char *sGraphId;
    size_t iMaxConcurrency;
    xwork_task_failure_policy eFailurePolicy;
    bool bCancelRequested;
    const char *sCancelReason;
    bool bPauseRequested;
    const char *sPauseReason;
    xwork_task_graph_result tResult;
    xwork_task_node_snapshot_list tNodes;
    xwork_handoff_summary_list tHandoffs;
} xwork_task_graph_snapshot;

/*
 * Remote worker/control-plane contract:
 * - xwork_control_plane_create() and create_from_snapshot() return an owned
 *   plane. Destroy it with xwork_control_plane_destroy().
 * - Registered workers are owned by the control plane. A returned
 *   xwork_worker* is borrowed and becomes invalid after unregister/destroy.
 * - Option/input structs are copied during the API call except borrowed
 *   xwork_runtime* pointers, which must outlive the plane/worker using them.
 * - Summary/list/snapshot outputs own their copied strings and nested arrays;
 *   release them with the matching *_reset() function before reuse.
 * - Control planes and workers are not safe for concurrent mutation. Serialize
 *   start/stop/register/heartbeat/enqueue/claim/complete/fail/cancel/upload
 *   calls and serialize queries against mutation when a coherent view matters.
 * - stop() stops scheduling state only. It does not kill OS processes or live
 *   terminal sessions owned by borrowed worker runtimes. Snapshot recovery
 *   never rehydrates live process/terminal handles; assigned/running work is
 *   recovered as orphaned durable state.
 * - XWORK_REMOTE_TRANSPORT_IN_PROCESS uses shared memory. HTTP transport uses
 *   the same decoded control-plane APIs after the caller authenticates,
 *   decodes, and validates the wire JSON envelope.
 */
typedef struct {
    const char *sPlaneId;
    xwork_runtime *pRuntime;
    xwork_remote_transport_kind eTransport;
    size_t iProtocolVersion;
    size_t iDefaultLeaseTimeoutMs;
    size_t iNowMs;
    const char **psAllowedCapabilities;
    size_t iAllowedCapabilityCount;
    bool bEnforceCapabilityAllowlist;
    xwork_autonomy_mode eAutonomy;
    xwork_approval_mode eApprovalMode;
    bool bAutoApproveTasks;
    bool bEnforceTaskPolicy;
    bool bEnforceNetworkPolicy;
    bool bRedactTaskSecrets;
} xwork_control_plane_options;

typedef struct {
    const char *sWorkerId;
    const char *sDisplayName;
    const char *sEndpoint;
    size_t iProtocolVersion;
    const char **psCapabilities;
    size_t iCapabilityCount;
    const char **psLabels;
    size_t iLabelCount;
    size_t iLeaseTimeoutMs;
    xwork_runtime *pRuntime;
} xwork_worker_options;

typedef struct {
    const char *sWorkerId;
    const char *sDisplayName;
    const char *sEndpoint;
    xwork_worker_state eState;
    size_t iProtocolVersion;
    size_t iCapabilityCount;
    size_t iLabelCount;
    size_t iLastHeartbeatMs;
    size_t iLeaseExpiresMs;
    size_t iClaimedCount;
    size_t iCompletedCount;
    size_t iFailedCount;
} xwork_worker_summary;

typedef struct {
    xwork_worker_summary *pItems;
    size_t iCount;
} xwork_worker_summary_list;

typedef struct {
    const char *sWorkerId;
    const char *sDisplayName;
    const char *sEndpoint;
    size_t iProtocolVersion;
    const char **psCapabilities;
    size_t iCapabilityCount;
    const char **psLabels;
    size_t iLabelCount;
    size_t iLeaseTimeoutMs;
    size_t iLastHeartbeatMs;
    size_t iLeaseExpiresMs;
    size_t iClaimedCount;
    size_t iCompletedCount;
    size_t iFailedCount;
    xwork_worker_state eState;
} xwork_worker_snapshot;

typedef struct {
    xwork_worker_snapshot *pItems;
    size_t iCount;
} xwork_worker_snapshot_list;

typedef struct {
    const char *sTaskId;
    xwork_remote_task_kind eKind;
    xwork_host_service_kind eHostService;
    const char *sOperationId;
    const char *sRequestJson;
    const char *sRequiredCapability;
    bool bRetryable;
    size_t iTimeoutMs;
    void *pUserData;
} xwork_remote_task_options;

typedef struct {
    xwork_remote_output_stream eStream;
    size_t iChunkIndex;
    size_t iOffsetBytes;
    size_t iByteCount;
    bool bFinalChunk;
    const char *sContentHash;
    const char *sText;
} xwork_remote_output_chunk_summary;

typedef struct {
    xwork_remote_output_chunk_summary *pItems;
    size_t iCount;
} xwork_remote_output_chunk_summary_list;

typedef struct {
    const char *sTaskId;
    const char *sAssignmentId;
    const char *sWorkerId;
    const char *sArtifactId;
    const char *sBlobRef;
    const char *sContentHash;
    size_t iChunkIndex;
    size_t iChunkCount;
    size_t iOffsetBytes;
    const void *pChunkData;
    size_t iChunkSize;
    bool bFinalChunk;
} xwork_remote_blob_chunk_summary;

typedef struct {
    xwork_remote_blob_chunk_summary *pItems;
    size_t iCount;
} xwork_remote_blob_chunk_summary_list;

typedef struct {
    const char *sTaskId;
    const char *sAssignmentId;
    const char *sWorkerId;
    xwork_remote_task_kind eKind;
    xwork_remote_task_state eState;
    xwork_host_service_kind eHostService;
    const char *sOperationId;
    const char *sRequestJson;
    const char *sRequiredCapability;
    size_t iAttemptCount;
    size_t iAssignedAtMs;
    size_t iCompletedAtMs;
    xwork_status iStatus;
    bool bRetryable;
    const char *sOutputText;
    const char *sVisibleSummary;
    const char *sErrorKind;
    const char *sErrorMessage;
    size_t iProtocolVersion;
    xwork_artifact_summary *pArtifacts;
    size_t iArtifactCount;
    xwork_remote_output_chunk_summary *pOutputChunks;
    size_t iOutputChunkCount;
} xwork_remote_task_summary;

typedef struct {
    xwork_remote_task_summary *pItems;
    size_t iCount;
} xwork_remote_task_summary_list;

typedef struct {
    const char *sTaskId;
    const char *sAssignmentId;
    const char *sWorkerId;
    xwork_remote_task_kind eKind;
    xwork_remote_task_state eState;
    xwork_host_service_kind eHostService;
    const char *sOperationId;
    const char *sRequestJson;
    const char *sRequiredCapability;
    size_t iAttemptCount;
    size_t iAssignedAtMs;
    size_t iCompletedAtMs;
    size_t iTimeoutMs;
    xwork_status iStatus;
    bool bRetryable;
    const char *sOutputText;
    const char *sVisibleSummary;
    const char *sErrorKind;
    const char *sErrorMessage;
    size_t iProtocolVersion;
    xwork_artifact_summary *pArtifacts;
    size_t iArtifactCount;
    xwork_remote_output_chunk_summary *pOutputChunks;
    size_t iOutputChunkCount;
} xwork_remote_task_snapshot;

typedef struct {
    xwork_remote_task_snapshot *pItems;
    size_t iCount;
} xwork_remote_task_snapshot_list;

typedef struct {
    const char *sTaskId;
    const char *sAssignmentId;
    const char *sWorkerId;
    xwork_remote_task_kind eKind;
    xwork_host_service_kind eHostService;
    const char *sOperationId;
    const char *sRequestJson;
    const char *sRequiredCapability;
    size_t iProtocolVersion;
    size_t iAttemptCount;
    bool bRetryable;
} xwork_remote_task_assignment;

typedef struct {
    xwork_status iStatus;
    const char *sOutputText;
    const char *sVisibleSummary;
    const char *sErrorKind;
    const char *sErrorMessage;
    size_t iProtocolVersion;
    bool bRetryable;
    const xwork_artifact_summary *pArtifacts;
    size_t iArtifactCount;
} xwork_remote_task_result;

typedef struct {
    const char *sTaskId;
    const char *sAssignmentId;
    const char *sWorkerId;
    size_t iProtocolVersion;
    xwork_remote_output_stream eStream;
    size_t iChunkIndex;
    size_t iOffsetBytes;
    const char *sText;
    size_t iByteCount;
    const char *sContentHash;
    bool bFinalChunk;
} xwork_remote_output_chunk;

typedef struct {
    const char *sTaskId;
    const char *sAssignmentId;
    const char *sWorkerId;
    size_t iProtocolVersion;
    const xwork_artifact_summary *pArtifact;
    const char *sBlobRef;
    const char *sContentHash;
    size_t iChunkIndex;
    size_t iChunkCount;
    size_t iOffsetBytes;
    const void *pChunkData;
    size_t iChunkSize;
    bool bFinalChunk;
} xwork_remote_artifact_upload;

typedef struct {
    const char *sPlaneId;
    xwork_remote_transport_kind eTransport;
    size_t iProtocolVersion;
    size_t iDefaultLeaseTimeoutMs;
    size_t iNowMs;
    bool bStarted;
    size_t iNextAssignmentSequence;
    xwork_worker_snapshot_list tWorkers;
    xwork_remote_task_snapshot_list tTasks;
    xwork_remote_blob_chunk_summary_list tBlobChunks;
} xwork_control_plane_snapshot;

typedef struct {
    const char *sReplayId;
    xwork_replay_mode eMode;
    bool bReadonlyFilesystem;
    bool bBlockSideEffects;
    size_t iMaxDivergences;
} xwork_replay_options;

typedef struct {
    const char *sManifestId;
    const char *sReplayId;
    const char *sSourceRunId;
    const char *sCreatedAtText;
    const char *sContentHashAlgorithm;
    size_t iEntryCount;
} xwork_replay_manifest;

typedef struct {
    xwork_replay_entry_kind eKind;
    const char *sKey;
    const char *sOperationId;
    const char *sRequestJson;
    const char *sResponseJson;
    const char *sArgumentsJson;
    const char *sResultJson;
    const char *sRequestHash;
    const char *sResponseHash;
    const char *sArgumentsHash;
    const char *sResultHash;
    const char *sContentHash;
    xwork_status iStatus;
} xwork_replay_entry_options;

typedef struct {
    size_t iSequence;
    xwork_replay_entry_kind eKind;
    const char *sKey;
    const char *sOperationId;
    const char *sRequestJson;
    const char *sResponseJson;
    const char *sArgumentsJson;
    const char *sResultJson;
    const char *sRequestHash;
    const char *sResponseHash;
    const char *sArgumentsHash;
    const char *sResultHash;
    const char *sContentHash;
    xwork_status iStatus;
} xwork_replay_entry_summary;

typedef struct {
    xwork_replay_entry_summary *pItems;
    size_t iCount;
} xwork_replay_entry_summary_list;

typedef struct {
    const char *sRefId;
    const char *sPath;
    const char *sMetadataJson;
    const char *sContentHash;
    xwork_status iStatus;
} xwork_replay_filesystem_ref_options;

typedef struct {
    size_t iSequence;
    const char *sRefId;
    const char *sPath;
    const char *sMetadataJson;
    const char *sContentHash;
    xwork_status iStatus;
} xwork_replay_filesystem_ref_summary;

typedef struct {
    xwork_replay_filesystem_ref_summary *pItems;
    size_t iCount;
} xwork_replay_filesystem_ref_summary_list;

typedef struct {
    xwork_replay_event_kind eKind;
    const char *sKey;
    const char *sName;
    int iType;
    const char *sPayloadJson;
    const char *sContentText;
    const char *sPayloadHash;
    const char *sContentHash;
    xwork_status iStatus;
} xwork_replay_event_options;

typedef struct {
    size_t iSequence;
    xwork_replay_event_kind eKind;
    const char *sKey;
    const char *sName;
    int iType;
    const char *sPayloadHash;
    const char *sContentHash;
    xwork_status iStatus;
} xwork_replay_event_summary;

typedef struct {
    xwork_replay_event_summary *pItems;
    size_t iCount;
} xwork_replay_event_summary_list;

typedef struct {
    xwork_replay_divergence_kind eKind;
    size_t iSequence;
    const char *sExpectedKey;
    const char *sActualKey;
    xwork_replay_entry_kind eExpectedEntryKind;
    xwork_replay_entry_kind eActualEntryKind;
    const char *sExpectedHash;
    const char *sActualHash;
    const char *sMessage;
} xwork_replay_divergence;

typedef struct {
    xwork_status iStatus;
    size_t iRecordedCount;
    size_t iReplayedCount;
    size_t iDivergenceCount;
    bool bDiverged;
    xwork_replay_divergence tFirstDivergence;
} xwork_replay_result;

typedef struct {
    const char *sArtifactId;
    xwork_artifact_kind eKind;
    xwork_artifact_output_class eOutputClass;
    const char *sOutputRole;
    xwork_artifact_report_class eReportClass;
    const char *sReportSubjectRef;
    const char *sName;
    const char *sMimeType;
    const char *sStorageRef;
    const char *sSummary;
    const char *sContentText;
    bool bHasContentStats;
    size_t iContentByteCount;
    size_t iContentLineCount;
    bool bHasPatchStats;
    size_t iPatchFileCount;
    size_t iPatchHunkCount;
    size_t iPatchAddedLineCount;
    size_t iPatchDeletedLineCount;
    const char *sPatchApplyResultJson;
    const char *sPatchFileSummaryJson;
    const char *sCommandText;
    bool bHasCommandIoStats;
    size_t iStdoutByteCount;
    size_t iStderrByteCount;
    bool bStdoutTruncated;
    bool bStderrTruncated;
    bool bHasExitCode;
    int iExitCode;
} xwork_artifact_options;

typedef struct {
    const char *sArtifactId;
    const char *sName;
    const char *sTargetRef;
    const char *sSummary;
    const char *sPatchText;
    const char *sApplyResultJson;
    const char *sFileSummaryJson;
} xwork_patch_artifact_options;

typedef struct {
    const char *sArtifactId;
    const char *sName;
    const char *sMimeType;
    const char *sStorageRef;
    const char *sSummary;
    xwork_artifact_output_class eOutputClass;
    const char *sOutputRole;
    xwork_artifact_report_class eReportClass;
    const char *sReportSubjectRef;
    const char *sReportText;
} xwork_report_artifact_options;

typedef struct {
    const char *sArtifactId;
    const char *sName;
    const char *sMimeType;
    const char *sStorageRef;
    const char *sSummary;
    xwork_artifact_output_class eOutputClass;
    const char *sOutputRole;
    const char *sOutputText;
} xwork_output_artifact_options;

typedef struct {
    const char *sArtifactId;
    const char *sName;
    const char *sMimeType;
    const char *sStorageRef;
    const char *sSummary;
    const char *sCommandText;
    const char *sOutputText;
    bool bHasCommandIoStats;
    size_t iStdoutByteCount;
    size_t iStderrByteCount;
    bool bStdoutTruncated;
    bool bStderrTruncated;
    bool bHasExitCode;
    int iExitCode;
} xwork_command_artifact_options;

typedef struct {
    const char *sRunId;
    const char *sParentRunId;
    const char *sAgentId;
    const char *sTaskId;
    const char *sInstruction;
    xwork_autonomy_mode eAutonomy;
    xwork_run_state eState;
    size_t iWorkspaceCount;
} xwork_run_summary;

typedef struct {
    const xwork_run_summary *pItems;
    size_t iCount;
} xwork_run_summary_list;

struct xwork_artifact_summary {
    const char *sArtifactId;
    xwork_artifact_kind eKind;
    xwork_artifact_output_class eOutputClass;
    const char *sOutputRole;
    xwork_artifact_report_class eReportClass;
    const char *sReportSubjectRef;
    const char *sName;
    const char *sMimeType;
    const char *sStorageRef;
    const char *sSummary;
    bool bHasContentStats;
    size_t iContentByteCount;
    size_t iContentLineCount;
    bool bHasPatchStats;
    size_t iPatchFileCount;
    size_t iPatchHunkCount;
    size_t iPatchAddedLineCount;
    size_t iPatchDeletedLineCount;
    const char *sPatchApplyResultJson;
    const char *sPatchFileSummaryJson;
    bool bHasCommandIoStats;
    size_t iStdoutByteCount;
    size_t iStderrByteCount;
    bool bStdoutTruncated;
    bool bStderrTruncated;
    bool bHasExitCode;
    int iExitCode;
    size_t iSequence;
};

typedef struct {
    const xwork_artifact_summary *pItems;
    size_t iCount;
    bool bHasMore;
    size_t iNextAfterSequence;
} xwork_artifact_summary_list;

typedef struct {
    bool bHasKind;
    xwork_artifact_kind eKind;
    bool bHasOutputClass;
    xwork_artifact_output_class eOutputClass;
    const char *sOutputRole;
    const char *sOutputRolePrefix;
    bool bHasReportClass;
    xwork_artifact_report_class eReportClass;
    const char *sReportSubjectRef;
    const char *sReportSubjectRefPrefix;
    const char *sArtifactName;
    const char *sNamePrefix;
    const char *sMimeType;
    const char *sMimeTypePrefix;
    const char *sStorageRef;
    const char *sStorageRefPrefix;
    bool bRequireExitCode;
    bool bHasExitCodeValue;
    int iExitCode;
    bool bHasAfterSequence;
    size_t iAfterSequence;
    bool bHasMinSequence;
    size_t iMinSequence;
    bool bHasMaxSequence;
    size_t iMaxSequence;
    size_t iLimit;
} xwork_artifact_summary_query;

struct xwork_run_snapshot {
    const char *sRunId;
    const char *sParentRunId;
    const char *sAgentId;
    const char *sTaskId;
    const char *sInstruction;
    const char *sLlmProfileId;
    const char *sSessionProfileId;
    const char *sSessionStateData;
    const char **psWorkspaceIds;
    size_t iWorkspaceCount;
    xwork_autonomy_mode eAutonomy;
    xwork_session_policy tSessionPolicy;
    xwork_run_state eState;
    const char *sLastOutputText;
    bool bHasMemoryContext;
    const char *sLastMemoryContextText;
    size_t iLastMemoryWorkspaceCount;
    bool bHasToolCall;
    const char *sLastToolCallId;
    const char *sLastToolId;
    const char *sLastToolArgumentsJson;
    bool bHasToolResult;
    const char *sLastToolResultText;
    const char *sLastToolVisibleSummary;
    bool bHasApprovalRequest;
    const char *sLastApprovalRequestId;
    const char *sLastApprovalToolId;
    const char *sLastApprovalReason;
    const char *sLastApprovalScope;
    const char *sLastApprovalActionSummary;
    xwork_risk_level eLastApprovalRiskLevel;
    xwork_approval_state eLastApprovalState;
    size_t iLastApprovalSequence;
    bool bHasCheckpoint;
    const char *sLastCheckpointId;
    const char *sLastCheckpointPendingStep;
    const char *sLastCheckpointSessionStateRef;
    const char *sLastCheckpointToolOutputsRef;
    const char *sLastCheckpointWorkspaceSnapshotRef;
    const char *sLastCheckpointArtifactRefs;
    xwork_checkpoint_kind eLastCheckpointKind;
    xwork_run_state eLastCheckpointRunState;
    size_t iLastCheckpointSequence;
    const xwork_artifact *pArtifacts;
    size_t iArtifactCount;
    size_t iNextEventSequence;
    size_t iNextArtifactSequence;
    size_t iNextApprovalSequence;
    size_t iNextCheckpointSequence;
};

typedef struct {
    const char *sRootPath;
} xwork_file_persistence_options;

typedef struct {
    char *sRootPath;
} xwork_file_persistence;

typedef struct {
    const char *sDefaultWorkingDirectory;
    bool bEnforceFilesystemRoot;
    const char **psFilesystemAllowPathPrefixes;
    size_t iFilesystemAllowPathPrefixCount;
    const char **psFilesystemDenyPathPrefixes;
    size_t iFilesystemDenyPathPrefixCount;
    const char **psCommandAllowPatterns;
    size_t iCommandAllowPatternCount;
    const char **psCommandDenyPatterns;
    size_t iCommandDenyPatternCount;
    bool bDenyDestructiveCommands;
    size_t iMaxReadBytes;
    size_t iMaxProcessInputBytes;
    size_t iMaxProcessEnvEntries;
    size_t iMaxProcessOutputBytes;
    bool bEnableFilesystemReadText;
    bool bEnableFilesystemWriteText;
    bool bEnableProcessExec;
    bool bEnableVcsStatus;
    bool bEnableVcsDiff;
    bool bEnableVcsLog;
    bool bEnableVcsBranch;
    bool bEnableEditorBuffers;
} xwork_local_host_options;

typedef struct {
    char *sDefaultWorkingDirectory;
    bool bEnforceFilesystemRoot;
    char **psFilesystemAllowPathPrefixes;
    size_t iFilesystemAllowPathPrefixCount;
    char **psFilesystemDenyPathPrefixes;
    size_t iFilesystemDenyPathPrefixCount;
    char **psCommandAllowPatterns;
    size_t iCommandAllowPatternCount;
    char **psCommandDenyPatterns;
    size_t iCommandDenyPatternCount;
    bool bDenyDestructiveCommands;
    size_t iMaxReadBytes;
    size_t iMaxProcessInputBytes;
    size_t iMaxProcessEnvEntries;
    size_t iMaxProcessOutputBytes;
    bool bEnableFilesystemReadText;
    bool bEnableFilesystemWriteText;
    bool bEnableProcessExec;
    bool bEnableVcsStatus;
    bool bEnableVcsDiff;
    bool bEnableVcsLog;
    bool bEnableVcsBranch;
    bool bEnableEditorBuffers;
    char *sLastOutputText;
    char *sLastVisibleSummary;
    void *pEditorBuffers;
    size_t iNextEditorBufferId;
    void *pTerminalSessions;
    size_t iNextTerminalSessionId;
} xwork_local_host;

typedef struct {
    const char **psItems;
    size_t iCount;
} xwork_string_list;

typedef xwork_status (*xwork_persistence_store_event_fn)(
    const xwork_event *pEvent,
    void *pUserData
);

typedef xwork_status (*xwork_persistence_store_checkpoint_fn)(
    const xwork_checkpoint *pCheckpoint,
    const xwork_run_snapshot *pSnapshot,
    void *pUserData
);

typedef xwork_status (*xwork_persistence_store_run_snapshot_fn)(
    const xwork_run_snapshot *pSnapshot,
    void *pUserData
);

typedef xwork_status (*xwork_persistence_store_artifact_fn)(
    const xwork_artifact *pArtifact,
    void *pUserData
);

typedef xwork_status (*xwork_persistence_load_run_snapshot_fn)(
    const char *sRunId,
    xwork_run_snapshot *pSnapshot,
    void *pUserData
);

typedef xwork_status (*xwork_persistence_load_checkpoint_snapshot_fn)(
    const char *sRunId,
    const char *sCheckpointId,
    xwork_run_snapshot *pSnapshot,
    void *pUserData
);

typedef xwork_status (*xwork_persistence_list_runs_fn)(
    xwork_string_list *pList,
    void *pUserData
);

typedef xwork_status (*xwork_persistence_list_run_items_fn)(
    const char *sRunId,
    xwork_string_list *pList,
    void *pUserData
);

typedef xwork_status (*xwork_persistence_query_run_index_fn)(
    const xwork_run_index_query *pQuery,
    xwork_run_index_list *pList,
    void *pUserData
);

typedef xwork_status (*xwork_persistence_load_run_summary_fn)(
    const char *sRunId,
    xwork_run_summary *pSummary,
    void *pUserData
);

typedef xwork_status (*xwork_persistence_load_last_event_fn)(
    const char *sRunId,
    xwork_event *pEvent,
    void *pUserData
);

typedef xwork_status (*xwork_persistence_load_last_approval_request_fn)(
    const char *sRunId,
    xwork_approval_request *pRequest,
    void *pUserData
);

typedef xwork_status (*xwork_persistence_load_last_checkpoint_fn)(
    const char *sRunId,
    xwork_checkpoint *pCheckpoint,
    void *pUserData
);

typedef xwork_status (*xwork_persistence_load_last_artifact_fn)(
    const char *sRunId,
    xwork_artifact *pArtifact,
    void *pUserData
);

typedef xwork_status (*xwork_persistence_load_event_fn)(
    const char *sRunId,
    const char *sEventId,
    xwork_event *pEvent,
    void *pUserData
);

typedef xwork_status (*xwork_persistence_load_checkpoint_fn)(
    const char *sRunId,
    const char *sCheckpointId,
    xwork_checkpoint *pCheckpoint,
    void *pUserData
);

typedef xwork_status (*xwork_persistence_load_artifact_fn)(
    const char *sRunId,
    const char *sArtifactId,
    xwork_artifact *pArtifact,
    void *pUserData
);

struct xwork_persistence_backend {
    xwork_persistence_store_event_fn pfnStoreEvent;
    xwork_persistence_store_checkpoint_fn pfnStoreCheckpoint;
    xwork_persistence_store_run_snapshot_fn pfnStoreRunSnapshot;
    xwork_persistence_store_artifact_fn pfnStoreArtifact;
    xwork_persistence_load_run_snapshot_fn pfnLoadRunSnapshot;
    xwork_persistence_load_checkpoint_snapshot_fn pfnLoadCheckpointSnapshot;
    xwork_persistence_list_runs_fn pfnListRuns;
    xwork_persistence_list_run_items_fn pfnListCheckpoints;
    xwork_persistence_list_run_items_fn pfnListEvents;
    xwork_persistence_list_run_items_fn pfnListArtifacts;
    xwork_persistence_query_run_index_fn pfnQueryRunIndex;
    xwork_persistence_load_run_summary_fn pfnLoadRunSummary;
    xwork_persistence_load_last_event_fn pfnLoadLastEvent;
    xwork_persistence_load_last_approval_request_fn pfnLoadLastApprovalRequest;
    xwork_persistence_load_last_checkpoint_fn pfnLoadLastCheckpoint;
    xwork_persistence_load_last_artifact_fn pfnLoadLastArtifact;
    xwork_persistence_load_event_fn pfnLoadEvent;
    xwork_persistence_load_checkpoint_fn pfnLoadCheckpoint;
    xwork_persistence_load_artifact_fn pfnLoadArtifact;
    void *pUserData;
};

struct xwork_event {
    const char *sEventId;
    const char *sRunId;
    const char *sToolId;
    const char *sApprovalRequestId;
    const char *sCheckpointId;
    const char *sSummary;
    xwork_event_kind eKind;
    xwork_run_state eRunState;
    size_t iSequence;
};

struct xwork_run_step {
    const char *sStepId;
    const char *sRunId;
    const char *sEventId;
    const char *sToolId;
    const char *sApprovalRequestId;
    const char *sCheckpointId;
    const char *sSummary;
    xwork_run_step_kind eKind;
    xwork_event_kind eEventKind;
    xwork_checkpoint_kind eCheckpointKind;
    xwork_run_state eRunState;
    size_t iSequence;
};

struct xwork_run_step_list {
    const xwork_run_step *pItems;
    size_t iCount;
    bool bHasMore;
    size_t iNextAfterSequence;
};

struct xwork_run_step_query {
    bool bFilterKind;
    xwork_run_step_kind eKind;
    bool bFilterEventKind;
    xwork_event_kind eEventKind;
    bool bFilterCheckpointKind;
    xwork_checkpoint_kind eCheckpointKind;
    bool bHasAfterSequence;
    size_t iAfterSequence;
    bool bHasMinSequence;
    size_t iMinSequence;
    bool bHasMaxSequence;
    size_t iMaxSequence;
    size_t iLimit;
};

struct xwork_approval_request {
    const char *sRequestId;
    const char *sRunId;
    const char *sToolId;
    const char *sReason;
    const char *sScope;
    const char *sActionSummary;
    xwork_risk_level eRiskLevel;
    xwork_approval_state eState;
};

struct xwork_checkpoint {
    const char *sCheckpointId;
    const char *sRunId;
    const char *sPendingStep;
    const char *sSessionStateRef;
    const char *sToolOutputsRef;
    const char *sWorkspaceSnapshotRef;
    const char *sArtifactRefs;
    xwork_checkpoint_kind eKind;
    xwork_run_state eRunState;
    size_t iSequence;
};

struct xwork_artifact {
    const char *sArtifactId;
    const char *sRunId;
    xwork_artifact_kind eKind;
    xwork_artifact_output_class eOutputClass;
    const char *sOutputRole;
    xwork_artifact_report_class eReportClass;
    const char *sReportSubjectRef;
    const char *sName;
    const char *sMimeType;
    const char *sStorageRef;
    const char *sSummary;
    const char *sContentText;
    bool bHasContentStats;
    size_t iContentByteCount;
    size_t iContentLineCount;
    bool bHasPatchStats;
    size_t iPatchFileCount;
    size_t iPatchHunkCount;
    size_t iPatchAddedLineCount;
    size_t iPatchDeletedLineCount;
    const char *sPatchApplyResultJson;
    const char *sPatchFileSummaryJson;
    const char *sCommandText;
    bool bHasCommandIoStats;
    size_t iStdoutByteCount;
    size_t iStderrByteCount;
    bool bStdoutTruncated;
    bool bStderrTruncated;
    bool bHasExitCode;
    int iExitCode;
    size_t iSequence;
};

typedef struct xwork_run_index_entry {
    xwork_run_summary tSummary;
    bool bHasLastApprovalRequest;
    xwork_approval_request tLastApprovalRequest;
    size_t iEventCount;
    bool bHasLastEvent;
    xwork_event tLastEvent;
    size_t iCheckpointCount;
    bool bHasLastCheckpoint;
    xwork_checkpoint tLastCheckpoint;
    size_t iArtifactCount;
    bool bHasLastArtifact;
    xwork_artifact tLastArtifact;
} xwork_run_index_entry;

struct xwork_run_index_list {
    const xwork_run_index_entry *pItems;
    size_t iCount;
    bool bHasMore;
    const char *sNextAfterRunId;
};

typedef enum {
    XWORK_RUN_INDEX_SORT_RUN_ID_ASC = 0,
    XWORK_RUN_INDEX_SORT_RUN_ID_DESC,
    XWORK_RUN_INDEX_SORT_LAST_EVENT_SEQUENCE_DESC,
    XWORK_RUN_INDEX_SORT_LAST_CHECKPOINT_SEQUENCE_DESC,
    XWORK_RUN_INDEX_SORT_EVENT_COUNT_DESC,
    XWORK_RUN_INDEX_SORT_CHECKPOINT_COUNT_DESC,
    XWORK_RUN_INDEX_SORT_ARTIFACT_COUNT_DESC
} xwork_run_index_sort;

struct xwork_run_index_query {
    bool bFilterState;
    xwork_run_state eState;
    bool bFilterAutonomy;
    xwork_autonomy_mode eAutonomy;
    bool bRequireParentRunId;
    const char *sParentRunId;
    const char *sParentRunIdPrefix;
    const char *sAgentId;
    const char *sTaskId;
    bool bFilterLastApprovalState;
    xwork_approval_state eLastApprovalState;
    bool bRequireLastEvent;
    bool bFilterLastEventKind;
    xwork_event_kind eLastEventKind;
    bool bRequireLastApprovalRequest;
    bool bRequireLastCheckpoint;
    bool bFilterLastCheckpointKind;
    xwork_checkpoint_kind eLastCheckpointKind;
    bool bRequireArtifacts;
    bool bFilterMinEventCount;
    size_t iMinEventCount;
    bool bFilterMaxEventCount;
    size_t iMaxEventCount;
    bool bFilterMinCheckpointCount;
    size_t iMinCheckpointCount;
    bool bFilterMaxCheckpointCount;
    size_t iMaxCheckpointCount;
    bool bFilterMinArtifactCount;
    size_t iMinArtifactCount;
    bool bFilterMaxArtifactCount;
    size_t iMaxArtifactCount;
    bool bFilterMinLastEventSequence;
    size_t iMinLastEventSequence;
    bool bFilterMaxLastEventSequence;
    size_t iMaxLastEventSequence;
    bool bFilterMinLastCheckpointSequence;
    size_t iMinLastCheckpointSequence;
    bool bFilterMaxLastCheckpointSequence;
    size_t iMaxLastCheckpointSequence;
    const char *sAfterRunId;
    size_t iLimit;
    xwork_run_index_sort eSort;
};

typedef struct {
    /*
     * Callbacks and user data are borrowed for the duration of
     * xwork_run_execute(). For xwork_run_execute_async(), they must remain
     * valid until the async handle completes or is destroyed.
     *
     * Model event cancellation order is deterministic: the orchestrator checks
     * pfnShouldInterrupt/pCancelToken before forwarding an event to
     * pfnModelEvent. If pfnModelEvent returns false, xwork marks the model turn
     * cancelled, cancels pCancelToken when one is available, and the run
     * completes the cancellation path with XWORK_ERROR_CANCELLED.
     *
     * Planner boundary:
     * - XWORK_PLANNER_BOUNDARY does not run a planner. It lets callers attach
     *   planner output as model-turn system context through sPlannerContextText
     *   or sPlannerPlanJson.
     * - eToolChoiceMode maps to xllm tool choice for the next model turn.
     *   XWORK_TOOL_CHOICE_NAMED requires sToolChoiceToolId.
     */
    xwork_tool_exec_fn pfnToolExec;
    xwork_tool_exec_ex_fn pfnToolExecEx;
    void *pUserData;
    xwork_memory_resolve_fn pfnResolveMemoryContext;
    void *pMemoryUserData;
    xwork_model_stream_mode eModelStreamMode;
    xllm_cancel_token *pCancelToken;
    xwork_model_event_fn pfnModelEvent;
    void *pModelEventUserData;
    xwork_interrupt_check_fn pfnShouldInterrupt;
    void *pInterruptUserData;
    xwork_planner_mode ePlannerMode;
    const char *sPlannerContextText;
    const char *sPlannerPlanJson;
    xwork_tool_choice_mode eToolChoiceMode;
    const char *sToolChoiceToolId;
    bool bAllowParallelToolCalls;
    /*
     * Default workspace memory retrieval policy used when
     * pfnResolveMemoryContext is NULL.
     *
     * - iMemorySearchMaxHits maps to xllm_memory_search_options.uMaxHits.
     * - iMemoryContextMaxBlocks limits how many workspace memory blocks are
     *   searched and merged for a model turn. Zero means no xwork-side limit.
     * - iMemoryContextMaxCharsPerHit and iMemoryContextMaxTotalChars map to
     *   xllm_memory_context_options character budgets. They are stable
     *   token-budget proxies until xllm exposes tokenizer-backed limits here.
     * - iMemoryContextPriority and bMemoryContextPinned are forwarded to the
     *   final model-turn context block.
     */
    size_t iMemorySearchMaxHits;
    size_t iMemoryContextMaxBlocks;
    size_t iMemoryContextMaxCharsPerHit;
    size_t iMemoryContextMaxTotalChars;
    int iMemoryContextPriority;
    bool bMemoryContextPinned;
    bool bIngestToolResultsToMemory;
    bool bIngestArtifactsToMemory;
    /*
     * Artifact memory ingest policy used when bIngestArtifactsToMemory is true.
     * A zero mask means "no filter" for compatibility.
     *
     * Sensitive-looking artifact content is skipped by default. Set
     * bIngestSensitiveArtifactsToMemory only when the caller has already
     * redacted or explicitly opted into retaining sensitive material in memory.
     */
    unsigned int uArtifactMemoryIngestKindMask;
    unsigned int uArtifactMemoryIngestOutputClassMask;
    unsigned int uArtifactMemoryIngestReportClassMask;
    bool bIngestSensitiveArtifactsToMemory;
    size_t iMaxTurns;
    size_t iMaxRetries;
    size_t iRetryBackoffMs;
    bool bAutoApprove;
} xwork_orchestrator_options;

XWORK_API void xwork_runtime_options_init(xwork_runtime_options *pOptions);
XWORK_API void xwork_workspace_options_init(xwork_workspace_options *pOptions);
XWORK_API void xwork_workspace_memory_sync_summary_init(
    xwork_workspace_memory_sync_summary *pSummary
);
XWORK_API void xwork_workspace_memory_file_sync_summary_init(
    xwork_workspace_memory_file_sync_summary *pSummary
);
XWORK_API void xwork_tool_def_init(xwork_tool_def *pDef);
XWORK_API void xwork_run_options_init(xwork_run_options *pOptions);
XWORK_API void xwork_agent_pool_options_init(xwork_agent_pool_options *pOptions);
XWORK_API void xwork_agent_options_init(xwork_agent_options *pOptions);
XWORK_API void xwork_agent_snapshot_init(xwork_agent_snapshot *pSnapshot);
XWORK_API void xwork_agent_snapshot_reset(xwork_agent_snapshot *pSnapshot);
XWORK_API void xwork_agent_snapshot_list_init(xwork_agent_snapshot_list *pList);
XWORK_API void xwork_agent_snapshot_list_reset(xwork_agent_snapshot_list *pList);
XWORK_API void xwork_agent_pool_snapshot_init(xwork_agent_pool_snapshot *pSnapshot);
XWORK_API void xwork_agent_pool_snapshot_reset(xwork_agent_pool_snapshot *pSnapshot);
XWORK_API void xwork_task_node_options_init(xwork_task_node_options *pOptions);
XWORK_API void xwork_task_graph_options_init(xwork_task_graph_options *pOptions);
XWORK_API void xwork_task_node_summary_init(xwork_task_node_summary *pSummary);
XWORK_API void xwork_task_node_summary_reset(xwork_task_node_summary *pSummary);
XWORK_API void xwork_task_node_summary_list_init(xwork_task_node_summary_list *pList);
XWORK_API void xwork_task_node_summary_list_reset(xwork_task_node_summary_list *pList);
XWORK_API void xwork_task_node_snapshot_init(xwork_task_node_snapshot *pSnapshot);
XWORK_API void xwork_task_node_snapshot_reset(xwork_task_node_snapshot *pSnapshot);
XWORK_API void xwork_task_node_snapshot_list_init(xwork_task_node_snapshot_list *pList);
XWORK_API void xwork_task_node_snapshot_list_reset(xwork_task_node_snapshot_list *pList);
XWORK_API void xwork_task_graph_result_init(xwork_task_graph_result *pResult);
XWORK_API void xwork_task_graph_snapshot_init(xwork_task_graph_snapshot *pSnapshot);
XWORK_API void xwork_task_graph_snapshot_reset(xwork_task_graph_snapshot *pSnapshot);
XWORK_API void xwork_handoff_request_options_init(
    xwork_handoff_request_options *pOptions
);
XWORK_API void xwork_handoff_result_options_init(
    xwork_handoff_result_options *pOptions
);
XWORK_API void xwork_handoff_summary_init(xwork_handoff_summary *pSummary);
XWORK_API void xwork_handoff_summary_reset(xwork_handoff_summary *pSummary);
XWORK_API void xwork_handoff_summary_list_init(xwork_handoff_summary_list *pList);
XWORK_API void xwork_handoff_summary_list_reset(xwork_handoff_summary_list *pList);
XWORK_API void xwork_control_plane_options_init(xwork_control_plane_options *pOptions);
XWORK_API void xwork_worker_options_init(xwork_worker_options *pOptions);
XWORK_API void xwork_worker_summary_init(xwork_worker_summary *pSummary);
XWORK_API void xwork_worker_summary_reset(xwork_worker_summary *pSummary);
XWORK_API void xwork_worker_summary_list_init(xwork_worker_summary_list *pList);
XWORK_API void xwork_worker_summary_list_reset(xwork_worker_summary_list *pList);
XWORK_API void xwork_worker_snapshot_init(xwork_worker_snapshot *pSnapshot);
XWORK_API void xwork_worker_snapshot_reset(xwork_worker_snapshot *pSnapshot);
XWORK_API void xwork_worker_snapshot_list_init(xwork_worker_snapshot_list *pList);
XWORK_API void xwork_worker_snapshot_list_reset(xwork_worker_snapshot_list *pList);
XWORK_API void xwork_remote_task_options_init(xwork_remote_task_options *pOptions);
XWORK_API void xwork_remote_task_summary_init(xwork_remote_task_summary *pSummary);
XWORK_API void xwork_remote_task_summary_reset(xwork_remote_task_summary *pSummary);
XWORK_API void xwork_remote_task_summary_list_init(xwork_remote_task_summary_list *pList);
XWORK_API void xwork_remote_task_summary_list_reset(xwork_remote_task_summary_list *pList);
XWORK_API void xwork_remote_task_snapshot_init(xwork_remote_task_snapshot *pSnapshot);
XWORK_API void xwork_remote_task_snapshot_reset(xwork_remote_task_snapshot *pSnapshot);
XWORK_API void xwork_remote_task_snapshot_list_init(xwork_remote_task_snapshot_list *pList);
XWORK_API void xwork_remote_task_snapshot_list_reset(xwork_remote_task_snapshot_list *pList);
XWORK_API void xwork_remote_task_assignment_init(xwork_remote_task_assignment *pAssignment);
XWORK_API void xwork_remote_task_assignment_reset(xwork_remote_task_assignment *pAssignment);
XWORK_API void xwork_remote_task_result_init(xwork_remote_task_result *pResult);
XWORK_API void xwork_remote_output_chunk_init(xwork_remote_output_chunk *pChunk);
XWORK_API void xwork_remote_output_chunk_summary_init(
    xwork_remote_output_chunk_summary *pSummary
);
XWORK_API void xwork_remote_output_chunk_summary_reset(
    xwork_remote_output_chunk_summary *pSummary
);
XWORK_API void xwork_remote_output_chunk_summary_list_init(
    xwork_remote_output_chunk_summary_list *pList
);
XWORK_API void xwork_remote_output_chunk_summary_list_reset(
    xwork_remote_output_chunk_summary_list *pList
);
XWORK_API void xwork_remote_blob_chunk_summary_init(
    xwork_remote_blob_chunk_summary *pSummary
);
XWORK_API void xwork_remote_blob_chunk_summary_reset(
    xwork_remote_blob_chunk_summary *pSummary
);
XWORK_API void xwork_remote_blob_chunk_summary_list_init(
    xwork_remote_blob_chunk_summary_list *pList
);
XWORK_API void xwork_remote_blob_chunk_summary_list_reset(
    xwork_remote_blob_chunk_summary_list *pList
);
XWORK_API void xwork_remote_artifact_upload_init(xwork_remote_artifact_upload *pUpload);
XWORK_API void xwork_control_plane_snapshot_init(xwork_control_plane_snapshot *pSnapshot);
XWORK_API void xwork_control_plane_snapshot_reset(xwork_control_plane_snapshot *pSnapshot);
XWORK_API void xwork_replay_options_init(xwork_replay_options *pOptions);
XWORK_API void xwork_replay_manifest_init(xwork_replay_manifest *pManifest);
XWORK_API void xwork_replay_manifest_reset(xwork_replay_manifest *pManifest);
XWORK_API void xwork_replay_entry_options_init(xwork_replay_entry_options *pOptions);
XWORK_API void xwork_replay_entry_summary_init(xwork_replay_entry_summary *pSummary);
XWORK_API void xwork_replay_entry_summary_reset(xwork_replay_entry_summary *pSummary);
XWORK_API void xwork_replay_entry_summary_list_init(xwork_replay_entry_summary_list *pList);
XWORK_API void xwork_replay_entry_summary_list_reset(xwork_replay_entry_summary_list *pList);
XWORK_API void xwork_replay_filesystem_ref_options_init(
    xwork_replay_filesystem_ref_options *pOptions
);
XWORK_API void xwork_replay_filesystem_ref_summary_init(
    xwork_replay_filesystem_ref_summary *pSummary
);
XWORK_API void xwork_replay_filesystem_ref_summary_reset(
    xwork_replay_filesystem_ref_summary *pSummary
);
XWORK_API void xwork_replay_filesystem_ref_summary_list_init(
    xwork_replay_filesystem_ref_summary_list *pList
);
XWORK_API void xwork_replay_filesystem_ref_summary_list_reset(
    xwork_replay_filesystem_ref_summary_list *pList
);
XWORK_API void xwork_replay_event_options_init(xwork_replay_event_options *pOptions);
XWORK_API void xwork_replay_event_options_from_model_event(
    const xwork_model_event *pEvent,
    xwork_replay_event_options *pOptions
);
XWORK_API void xwork_replay_event_summary_init(xwork_replay_event_summary *pSummary);
XWORK_API void xwork_replay_event_summary_reset(xwork_replay_event_summary *pSummary);
XWORK_API void xwork_replay_event_summary_list_init(xwork_replay_event_summary_list *pList);
XWORK_API void xwork_replay_event_summary_list_reset(xwork_replay_event_summary_list *pList);
XWORK_API void xwork_replay_divergence_init(xwork_replay_divergence *pDivergence);
XWORK_API void xwork_replay_divergence_reset(xwork_replay_divergence *pDivergence);
XWORK_API void xwork_replay_result_init(xwork_replay_result *pResult);
XWORK_API void xwork_replay_result_reset(xwork_replay_result *pResult);
XWORK_API void xwork_run_summary_init(xwork_run_summary *pSummary);
XWORK_API void xwork_run_summary_reset(xwork_run_summary *pSummary);
XWORK_API void xwork_run_summary_list_init(xwork_run_summary_list *pList);
XWORK_API void xwork_run_summary_list_reset(xwork_run_summary_list *pList);
XWORK_API void xwork_artifact_summary_init(xwork_artifact_summary *pSummary);
XWORK_API void xwork_artifact_summary_reset(xwork_artifact_summary *pSummary);
XWORK_API void xwork_artifact_summary_list_init(xwork_artifact_summary_list *pList);
XWORK_API void xwork_artifact_summary_list_reset(xwork_artifact_summary_list *pList);
XWORK_API void xwork_artifact_summary_query_init(xwork_artifact_summary_query *pQuery);
XWORK_API void xwork_run_index_entry_init(xwork_run_index_entry *pEntry);
XWORK_API void xwork_run_index_entry_reset(xwork_run_index_entry *pEntry);
XWORK_API void xwork_run_index_list_init(xwork_run_index_list *pList);
XWORK_API void xwork_run_index_list_reset(xwork_run_index_list *pList);
XWORK_API void xwork_run_index_query_init(xwork_run_index_query *pQuery);
XWORK_API void xwork_host_service_init(xwork_host_service *pService);
XWORK_API void xwork_host_services_init(xwork_host_services *pServices);
XWORK_API void xwork_policy_options_init(xwork_policy_options *pOptions);
XWORK_API void xwork_approval_eval_input_init(xwork_approval_eval_input *pInput);
XWORK_API void xwork_approval_decision_init(xwork_approval_decision *pDecision);
XWORK_API void xwork_network_policy_eval_input_init(
    xwork_network_policy_eval_input *pInput
);
XWORK_API void xwork_network_policy_decision_init(
    xwork_network_policy_decision *pDecision
);
XWORK_API void xwork_memory_context_init(xwork_memory_context *pContext);
XWORK_API void xwork_memory_context_reset(xwork_memory_context *pContext);
XWORK_API void xwork_session_policy_init(xwork_session_policy *pPolicy);
XWORK_API void xwork_xllm_transport_options_init(xwork_xllm_transport_options *pOptions);
XWORK_API void xwork_xllm_profile_options_init(xwork_xllm_profile_options *pOptions);
XWORK_API void xwork_xllm_bootstrap_options_init(xwork_xllm_bootstrap_options *pOptions);
XWORK_API void xwork_profile_init(xwork_profile *pProfile);
XWORK_API void xwork_persistence_backend_init(xwork_persistence_backend *pBackend);
XWORK_API void xwork_file_persistence_options_init(
    xwork_file_persistence_options *pOptions
);
XWORK_API void xwork_file_persistence_init(xwork_file_persistence *pStore);
XWORK_API void xwork_file_persistence_reset(xwork_file_persistence *pStore);
XWORK_API void xwork_local_host_options_init(xwork_local_host_options *pOptions);
XWORK_API void xwork_local_host_init(xwork_local_host *pHost);
XWORK_API void xwork_local_host_reset(xwork_local_host *pHost);
XWORK_API void xwork_string_list_init(xwork_string_list *pList);
XWORK_API void xwork_string_list_reset(xwork_string_list *pList);
XWORK_API void xwork_event_init(xwork_event *pEvent);
XWORK_API void xwork_event_reset(xwork_event *pEvent);
XWORK_API void xwork_run_step_init(xwork_run_step *pStep);
XWORK_API void xwork_run_step_reset(xwork_run_step *pStep);
XWORK_API void xwork_run_step_list_init(xwork_run_step_list *pList);
XWORK_API void xwork_run_step_list_reset(xwork_run_step_list *pList);
XWORK_API void xwork_run_step_query_init(xwork_run_step_query *pQuery);
XWORK_API void xwork_approval_request_init(xwork_approval_request *pRequest);
XWORK_API void xwork_approval_request_reset(xwork_approval_request *pRequest);
XWORK_API void xwork_checkpoint_init(xwork_checkpoint *pCheckpoint);
XWORK_API void xwork_checkpoint_reset(xwork_checkpoint *pCheckpoint);
XWORK_API void xwork_artifact_options_init(xwork_artifact_options *pOptions);
XWORK_API void xwork_patch_artifact_options_init(xwork_patch_artifact_options *pOptions);
XWORK_API void xwork_report_artifact_options_init(xwork_report_artifact_options *pOptions);
XWORK_API void xwork_output_artifact_options_init(xwork_output_artifact_options *pOptions);
XWORK_API void xwork_command_artifact_options_init(xwork_command_artifact_options *pOptions);
XWORK_API void xwork_artifact_init(xwork_artifact *pArtifact);
XWORK_API void xwork_artifact_reset(xwork_artifact *pArtifact);
XWORK_API void xwork_run_snapshot_init(xwork_run_snapshot *pSnapshot);
XWORK_API void xwork_run_snapshot_reset(xwork_run_snapshot *pSnapshot);
XWORK_API void xwork_tool_result_init(xwork_tool_result *pResult);
XWORK_API void xwork_orchestrator_options_init(xwork_orchestrator_options *pOptions);
XWORK_API xwork_status xwork_profile_get_builtin(
    const char *sProfileId,
    xwork_profile *pProfile
);
XWORK_API xwork_status xwork_profile_apply_runtime_options(
    const xwork_profile *pProfile,
    xwork_runtime_options *pOptions
);
XWORK_API xwork_status xwork_profile_apply_xllm_profile_options(
    const xwork_profile *pProfile,
    xwork_xllm_profile_options *pOptions
);
XWORK_API xwork_status xwork_profile_apply_xllm_bootstrap_options(
    const xwork_profile *pProfile,
    xwork_xllm_profile_options *pProfileOptions,
    xwork_xllm_bootstrap_options *pBootstrapOptions
);
XWORK_API xwork_status xwork_profile_apply_workspace_options(
    const xwork_profile *pProfile,
    xwork_workspace_options *pOptions
);
XWORK_API xwork_status xwork_profile_apply_run_options(
    const xwork_profile *pProfile,
    xwork_run_options *pOptions
);
XWORK_API xwork_status xwork_profile_apply_orchestrator_options(
    const xwork_profile *pProfile,
    xwork_orchestrator_options *pOptions
);
XWORK_API xwork_status xwork_file_persistence_configure_backend(
    xwork_file_persistence *pStore,
    const xwork_file_persistence_options *pOptions,
    xwork_persistence_backend *pBackend
);
XWORK_API xwork_status xwork_local_host_configure_services(
    xwork_local_host *pHost,
    const xwork_local_host_options *pOptions,
    xwork_host_services *pServices
);
XWORK_API xwork_status xwork_file_persistence_list_runs(
    const xwork_file_persistence *pStore,
    xwork_string_list *pList
);
XWORK_API xwork_status xwork_file_persistence_list_run_summaries(
    const xwork_file_persistence *pStore,
    xwork_run_summary_list *pList
);
XWORK_API xwork_status xwork_file_persistence_list_run_index(
    const xwork_file_persistence *pStore,
    xwork_run_index_list *pList
);
XWORK_API xwork_status xwork_file_persistence_query_run_index(
    const xwork_file_persistence *pStore,
    const xwork_run_index_query *pQuery,
    xwork_run_index_list *pList
);
XWORK_API xwork_status xwork_file_persistence_list_checkpoints(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    xwork_string_list *pList
);
XWORK_API xwork_status xwork_file_persistence_list_events(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    xwork_string_list *pList
);
XWORK_API xwork_status xwork_file_persistence_list_artifacts(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    xwork_string_list *pList
);
XWORK_API xwork_status xwork_file_persistence_list_artifact_summaries(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    xwork_artifact_summary_list *pList
);
XWORK_API xwork_status xwork_file_persistence_query_artifact_summaries(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    const xwork_artifact_summary_query *pQuery,
    xwork_artifact_summary_list *pList
);
XWORK_API xwork_status xwork_file_persistence_query_run_steps(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    const xwork_run_step_query *pQuery,
    xwork_run_step_list *pList
);
XWORK_API xwork_status xwork_file_persistence_load_event(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    const char *sEventId,
    xwork_event *pEvent
);
XWORK_API xwork_status xwork_file_persistence_load_last_event(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    xwork_event *pEvent
);
XWORK_API xwork_status xwork_file_persistence_load_run_snapshot(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    xwork_run_snapshot *pSnapshot
);
XWORK_API xwork_status xwork_file_persistence_load_checkpoint_snapshot(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    const char *sCheckpointId,
    xwork_run_snapshot *pSnapshot
);
XWORK_API xwork_status xwork_file_persistence_store_task_graph_snapshot(
    const xwork_file_persistence *pStore,
    const xwork_task_graph_snapshot *pSnapshot
);
XWORK_API xwork_status xwork_file_persistence_load_task_graph_snapshot(
    const xwork_file_persistence *pStore,
    const char *sGraphId,
    xwork_task_graph_snapshot *pSnapshot
);
XWORK_API xwork_status xwork_file_persistence_store_agent_pool_snapshot(
    const xwork_file_persistence *pStore,
    const xwork_agent_pool_snapshot *pSnapshot
);
XWORK_API xwork_status xwork_file_persistence_load_agent_pool_snapshot(
    const xwork_file_persistence *pStore,
    const char *sPoolId,
    xwork_agent_pool_snapshot *pSnapshot
);
XWORK_API xwork_status xwork_file_persistence_store_control_plane_snapshot(
    const xwork_file_persistence *pStore,
    const xwork_control_plane_snapshot *pSnapshot
);
XWORK_API xwork_status xwork_file_persistence_load_control_plane_snapshot(
    const xwork_file_persistence *pStore,
    const char *sPlaneId,
    xwork_control_plane_snapshot *pSnapshot
);
XWORK_API xwork_status xwork_file_persistence_store_replay(
    const xwork_file_persistence *pStore,
    const xwork_replay_engine *pEngine
);
XWORK_API xwork_status xwork_file_persistence_list_replays(
    const xwork_file_persistence *pStore,
    xwork_string_list *pList
);
XWORK_API xwork_status xwork_file_persistence_load_replay_manifest(
    const xwork_file_persistence *pStore,
    const char *sReplayId,
    xwork_replay_manifest *pManifest
);
XWORK_API xwork_status xwork_file_persistence_load_replay_entries(
    const xwork_file_persistence *pStore,
    const char *sReplayId,
    xwork_replay_entry_summary_list *pList
);
XWORK_API xwork_status xwork_file_persistence_load_replay_result(
    const xwork_file_persistence *pStore,
    const char *sReplayId,
    xwork_replay_result *pResult
);
XWORK_API xwork_status xwork_file_persistence_load_replay_engine(
    const xwork_file_persistence *pStore,
    const char *sReplayId,
    const xwork_replay_options *pOptions,
    xwork_replay_engine **ppEngine
);
XWORK_API xwork_status xwork_file_persistence_recover_task_graph(
    const xwork_file_persistence *pStore,
    xwork_runtime *pRuntime,
    const char *sPoolId,
    const char *sGraphId,
    const xwork_task_graph_options *pExecutionOptions,
    xwork_agent_pool **ppPool,
    xwork_task_graph **ppGraph
);
XWORK_API xwork_status xwork_file_persistence_recover_control_plane(
    const xwork_file_persistence *pStore,
    xwork_runtime *pRuntime,
    const char *sPlaneId,
    const xwork_control_plane_options *pOptions,
    xwork_control_plane **ppPlane
);
XWORK_API xwork_status xwork_file_persistence_load_last_approval_request(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    xwork_approval_request *pRequest
);
XWORK_API xwork_status xwork_file_persistence_load_run_summary(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    xwork_run_summary *pSummary
);
XWORK_API xwork_status xwork_file_persistence_load_checkpoint(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    const char *sCheckpointId,
    xwork_checkpoint *pCheckpoint
);
XWORK_API xwork_status xwork_file_persistence_load_last_checkpoint(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    xwork_checkpoint *pCheckpoint
);
XWORK_API xwork_status xwork_file_persistence_load_artifact(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    const char *sArtifactId,
    xwork_artifact *pArtifact
);
XWORK_API xwork_status xwork_file_persistence_load_last_artifact(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    xwork_artifact *pArtifact
);
XWORK_API xwork_status xwork_file_persistence_find_artifact_by_name(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    const char *sArtifactName,
    xwork_artifact *pArtifact
);

XWORK_API xwork_status xwork_runtime_create(
    const xwork_runtime_options *pOptions,
    xwork_runtime **ppRuntime
);
XWORK_API void xwork_runtime_destroy(xwork_runtime *pRuntime);

XWORK_API xllm_runtime *xwork_runtime_get_llm_runtime(const xwork_runtime *pRuntime);
XWORK_API const xwork_host_services *xwork_runtime_get_host_services(const xwork_runtime *pRuntime);
XWORK_API const xwork_persistence_backend *xwork_runtime_get_persistence_backend(
    const xwork_runtime *pRuntime
);
XWORK_API xwork_status xwork_runtime_list_persisted_runs(
    const xwork_runtime *pRuntime,
    xwork_string_list *pList
);
XWORK_API xwork_status xwork_runtime_list_persisted_checkpoints(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_string_list *pList
);
XWORK_API xwork_status xwork_runtime_list_persisted_events(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_string_list *pList
);
XWORK_API xwork_status xwork_runtime_list_persisted_artifacts(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_string_list *pList
);
XWORK_API xwork_status xwork_runtime_list_persisted_artifact_summaries(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_artifact_summary_list *pList
);
XWORK_API xwork_status xwork_runtime_query_persisted_artifact_summaries(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    const xwork_artifact_summary_query *pQuery,
    xwork_artifact_summary_list *pList
);
XWORK_API xwork_status xwork_runtime_query_persisted_run_steps(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    const xwork_run_step_query *pQuery,
    xwork_run_step_list *pList
);
XWORK_API xwork_status xwork_runtime_list_persisted_run_summaries(
    const xwork_runtime *pRuntime,
    xwork_run_summary_list *pList
);
XWORK_API xwork_status xwork_runtime_list_persisted_run_index(
    const xwork_runtime *pRuntime,
    xwork_run_index_list *pList
);
XWORK_API xwork_status xwork_runtime_query_persisted_run_index(
    const xwork_runtime *pRuntime,
    const xwork_run_index_query *pQuery,
    xwork_run_index_list *pList
);
XWORK_API xwork_status xwork_runtime_load_persisted_run_summary(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_run_summary *pSummary
);
XWORK_API xwork_status xwork_runtime_load_persisted_last_event(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_event *pEvent
);
XWORK_API xwork_status xwork_runtime_load_persisted_last_approval_request(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_approval_request *pRequest
);
XWORK_API xwork_status xwork_runtime_load_persisted_last_checkpoint(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_checkpoint *pCheckpoint
);
XWORK_API xwork_status xwork_runtime_load_persisted_last_artifact(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_artifact *pArtifact
);
XWORK_API xwork_status xwork_runtime_load_persisted_event(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    const char *sEventId,
    xwork_event *pEvent
);
XWORK_API xwork_status xwork_runtime_load_persisted_checkpoint(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    const char *sCheckpointId,
    xwork_checkpoint *pCheckpoint
);
XWORK_API xwork_status xwork_runtime_load_persisted_artifact(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    const char *sArtifactId,
    xwork_artifact *pArtifact
);
XWORK_API xwork_status xwork_runtime_find_persisted_artifact_by_name(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    const char *sArtifactName,
    xwork_artifact *pArtifact
);
XWORK_API xwork_status xwork_runtime_get_policy_options(
    const xwork_runtime *pRuntime,
    xwork_policy_options *pOptions
);
XWORK_API size_t xwork_runtime_get_workspace_count(const xwork_runtime *pRuntime);
XWORK_API size_t xwork_runtime_get_tool_count(const xwork_runtime *pRuntime);
XWORK_API size_t xwork_runtime_get_run_count(const xwork_runtime *pRuntime);
XWORK_API xwork_status xwork_runtime_invoke_host_service(
    const xwork_runtime *pRuntime,
    xwork_host_service_kind eKind,
    const char *sOperationId,
    const char *sRequestJson,
    xwork_tool_result *pResult
);
XWORK_API xwork_status xwork_runtime_invoke_host_service_ex(
    const xwork_runtime *pRuntime,
    xwork_host_service_kind eKind,
    const char *sOperationId,
    const char *sRequestJson,
    const xwork_host_invoke_context *pContext,
    xwork_tool_result *pResult
);
XWORK_API bool xwork_host_invoke_context_should_cancel(
    const xwork_host_invoke_context *pContext,
    const char *sPhase
);
XWORK_API bool xwork_tool_exec_context_should_cancel(
    const xwork_run *pRun,
    const xwork_tool_exec_context *pContext,
    const char *sPhase
);

XWORK_API xwork_status xwork_runtime_add_workspace(
    xwork_runtime *pRuntime,
    const xwork_workspace_options *pOptions,
    xwork_workspace **ppWorkspace
);
XWORK_API xwork_workspace *xwork_runtime_find_workspace(
    const xwork_runtime *pRuntime,
    const char *sWorkspaceId
);
XWORK_API void xwork_workspace_destroy(xwork_workspace *pWorkspace);
XWORK_API const char *xwork_workspace_get_id(const xwork_workspace *pWorkspace);
XWORK_API const char *xwork_workspace_get_root_path(const xwork_workspace *pWorkspace);
XWORK_API bool xwork_workspace_is_memory_enabled(const xwork_workspace *pWorkspace);
XWORK_API xllm_memory *xwork_workspace_get_memory(const xwork_workspace *pWorkspace);
XWORK_API xwork_status xwork_workspace_sync_memory(
    xwork_workspace *pWorkspace,
    xwork_workspace_memory_sync_summary *pSummary
);
XWORK_API xwork_status xwork_workspace_sync_memory_file(
    xwork_workspace *pWorkspace,
    const char *sPath,
    xwork_workspace_memory_file_sync_summary *pSummary
);

XWORK_API xwork_status xwork_runtime_register_tool(
    xwork_runtime *pRuntime,
    const xwork_tool_def *pDef
);
XWORK_API const xwork_tool_def *xwork_get_builtin_tool_def(const char *sToolId);
XWORK_API xwork_status xwork_runtime_register_builtin_tool(
    xwork_runtime *pRuntime,
    const char *sToolId
);
XWORK_API const xwork_tool_def *xwork_runtime_find_tool(
    const xwork_runtime *pRuntime,
    const char *sToolId
);
XWORK_API xwork_status xwork_runtime_recover_run(
    xwork_runtime *pRuntime,
    const xwork_run_snapshot *pSnapshot,
    xwork_run **ppRun
);
XWORK_API xwork_status xwork_runtime_recover_run_from_persistence(
    xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_run **ppRun
);

XWORK_API xwork_status xwork_agent_pool_create(
    const xwork_agent_pool_options *pOptions,
    xwork_agent_pool **ppPool
);
XWORK_API xwork_status xwork_agent_pool_create_from_snapshot(
    xwork_runtime *pRuntime,
    const xwork_agent_pool_snapshot *pSnapshot,
    xwork_agent_pool **ppPool
);
XWORK_API void xwork_agent_pool_destroy(xwork_agent_pool *pPool);
XWORK_API xwork_status xwork_agent_pool_add_agent(
    xwork_agent_pool *pPool,
    const xwork_agent_options *pOptions,
    xwork_agent **ppAgent
);
XWORK_API size_t xwork_agent_pool_get_agent_count(const xwork_agent_pool *pPool);
XWORK_API xwork_agent *xwork_agent_pool_find_agent(
    const xwork_agent_pool *pPool,
    const char *sAgentId
);
XWORK_API xwork_status xwork_agent_pool_get_snapshot(
    const xwork_agent_pool *pPool,
    xwork_agent_pool_snapshot *pSnapshot
);
XWORK_API const char *xwork_agent_get_id(const xwork_agent *pAgent);
XWORK_API xwork_agent_role xwork_agent_get_role(const xwork_agent *pAgent);

XWORK_API xwork_status xwork_task_graph_create(
    const xwork_task_graph_options *pOptions,
    xwork_task_graph **ppGraph
);
XWORK_API xwork_status xwork_task_graph_create_from_snapshot(
    const xwork_task_graph_options *pOptions,
    const xwork_task_graph_snapshot *pSnapshot,
    xwork_task_graph **ppGraph
);
XWORK_API void xwork_task_graph_destroy(xwork_task_graph *pGraph);
XWORK_API xwork_status xwork_task_graph_add_node(
    xwork_task_graph *pGraph,
    const xwork_task_node_options *pOptions
);
XWORK_API xwork_status xwork_task_graph_add_dependency(
    xwork_task_graph *pGraph,
    const char *sBeforeTaskId,
    const char *sAfterTaskId
);
XWORK_API size_t xwork_task_graph_get_node_count(const xwork_task_graph *pGraph);
XWORK_API xwork_status xwork_task_graph_get_node_summary(
    const xwork_task_graph *pGraph,
    const char *sTaskId,
    xwork_task_node_summary *pSummary
);
XWORK_API xwork_status xwork_task_graph_list_node_summaries(
    const xwork_task_graph *pGraph,
    xwork_task_node_summary_list *pList
);
XWORK_API xwork_run *xwork_task_graph_get_node_run(
    const xwork_task_graph *pGraph,
    const char *sTaskId
);
XWORK_API xwork_status xwork_task_graph_get_snapshot(
    const xwork_task_graph *pGraph,
    xwork_task_graph_snapshot *pSnapshot
);
XWORK_API xwork_status xwork_task_graph_request_handoff(
    xwork_task_graph *pGraph,
    const xwork_handoff_request_options *pOptions,
    xwork_handoff_summary *pSummary
);
XWORK_API xwork_status xwork_task_graph_resolve_handoff(
    xwork_task_graph *pGraph,
    const xwork_handoff_result_options *pOptions,
    xwork_handoff_summary *pSummary
);
XWORK_API xwork_status xwork_task_graph_list_handoffs(
    const xwork_task_graph *pGraph,
    xwork_handoff_summary_list *pList
);
XWORK_API xwork_status xwork_task_graph_emit_agent_result_report(
    const xwork_task_graph *pGraph,
    const char *sTaskId,
    const char *sArtifactId,
    xwork_artifact *pArtifact
);
XWORK_API xwork_status xwork_task_graph_emit_aggregate_report(
    const xwork_task_graph *pGraph,
    xwork_run *pRun,
    const char *sArtifactId,
    xwork_artifact *pArtifact
);
XWORK_API xwork_status xwork_task_graph_execute(
    xwork_task_graph *pGraph,
    xwork_task_graph_result *pResult
);
XWORK_API xwork_status xwork_task_graph_cancel(
    xwork_task_graph *pGraph,
    const char *sReason
);
XWORK_API bool xwork_task_graph_is_cancelled(const xwork_task_graph *pGraph);
XWORK_API xwork_status xwork_task_graph_pause(
    xwork_task_graph *pGraph,
    const char *sReason
);
XWORK_API xwork_status xwork_task_graph_resume(xwork_task_graph *pGraph);
XWORK_API bool xwork_task_graph_is_paused(const xwork_task_graph *pGraph);

XWORK_API xwork_status xwork_control_plane_create(
    const xwork_control_plane_options *pOptions,
    xwork_control_plane **ppPlane
);
XWORK_API xwork_status xwork_control_plane_create_from_snapshot(
    const xwork_control_plane_options *pOptions,
    const xwork_control_plane_snapshot *pSnapshot,
    xwork_control_plane **ppPlane
);
XWORK_API void xwork_control_plane_destroy(xwork_control_plane *pPlane);
XWORK_API xwork_status xwork_control_plane_start(xwork_control_plane *pPlane);
XWORK_API xwork_status xwork_control_plane_stop(xwork_control_plane *pPlane);
XWORK_API xwork_status xwork_control_plane_set_time(
    xwork_control_plane *pPlane,
    size_t iNowMs
);
XWORK_API xwork_status xwork_control_plane_register_worker(
    xwork_control_plane *pPlane,
    const xwork_worker_options *pOptions,
    xwork_worker **ppWorker
);
XWORK_API xwork_status xwork_control_plane_unregister_worker(
    xwork_control_plane *pPlane,
    const char *sWorkerId
);
XWORK_API xwork_status xwork_control_plane_worker_heartbeat(
    xwork_control_plane *pPlane,
    const char *sWorkerId,
    size_t iNowMs
);
XWORK_API xwork_status xwork_control_plane_sweep_stale(
    xwork_control_plane *pPlane,
    size_t iNowMs,
    size_t *piOrphanedCount
);
XWORK_API xwork_status xwork_control_plane_list_workers(
    const xwork_control_plane *pPlane,
    xwork_worker_summary_list *pList
);
XWORK_API xwork_status xwork_control_plane_enqueue_task(
    xwork_control_plane *pPlane,
    const xwork_remote_task_options *pOptions
);
XWORK_API xwork_status xwork_control_plane_claim_task(
    xwork_control_plane *pPlane,
    const char *sWorkerId,
    xwork_remote_task_assignment *pAssignment
);
XWORK_API xwork_status xwork_control_plane_complete_task(
    xwork_control_plane *pPlane,
    const char *sAssignmentId,
    const xwork_remote_task_result *pResult
);
XWORK_API xwork_status xwork_control_plane_upload_artifact(
    xwork_control_plane *pPlane,
    const xwork_remote_artifact_upload *pUpload
);
XWORK_API xwork_status xwork_control_plane_upload_output_chunk(
    xwork_control_plane *pPlane,
    const xwork_remote_output_chunk *pChunk
);
XWORK_API xwork_status xwork_control_plane_list_artifact_blobs(
    const xwork_control_plane *pPlane,
    const char *sTaskId,
    const char *sArtifactId,
    xwork_remote_blob_chunk_summary_list *pList
);
XWORK_API xwork_status xwork_control_plane_fail_task(
    xwork_control_plane *pPlane,
    const char *sAssignmentId,
    const char *sErrorText,
    bool bRetryable
);
XWORK_API xwork_status xwork_control_plane_cancel_task(
    xwork_control_plane *pPlane,
    const char *sTaskId,
    const char *sReason
);
XWORK_API xwork_status xwork_control_plane_execute_next_local(
    xwork_control_plane *pPlane,
    const char *sWorkerId,
    xwork_remote_task_assignment *pAssignment
);
XWORK_API xwork_status xwork_control_plane_get_task_summary(
    const xwork_control_plane *pPlane,
    const char *sTaskId,
    xwork_remote_task_summary *pSummary
);
XWORK_API xwork_status xwork_control_plane_list_tasks(
    const xwork_control_plane *pPlane,
    xwork_remote_task_summary_list *pList
);
XWORK_API xwork_status xwork_control_plane_get_snapshot(
    const xwork_control_plane *pPlane,
    xwork_control_plane_snapshot *pSnapshot
);

XWORK_API xwork_status xwork_replay_engine_create(
    const xwork_replay_options *pOptions,
    xwork_replay_engine **ppEngine
);
XWORK_API void xwork_replay_engine_destroy(xwork_replay_engine *pEngine);
XWORK_API xwork_replay_mode xwork_replay_engine_get_mode(
    const xwork_replay_engine *pEngine
);
XWORK_API bool xwork_replay_engine_blocks_side_effects(
    const xwork_replay_engine *pEngine
);
XWORK_API xwork_status xwork_replay_engine_record_entry(
    xwork_replay_engine *pEngine,
    const xwork_replay_entry_options *pEntry
);
XWORK_API xwork_status xwork_replay_engine_load_entry(
    xwork_replay_engine *pEngine,
    const xwork_replay_entry_options *pEntry
);
XWORK_API xwork_status xwork_replay_engine_replay_entry(
    xwork_replay_engine *pEngine,
    const xwork_replay_entry_options *pExpected,
    xwork_replay_entry_summary *pActual
);
XWORK_API xwork_status xwork_replay_engine_record_filesystem_ref(
    xwork_replay_engine *pEngine,
    const xwork_replay_filesystem_ref_options *pRef
);
XWORK_API xwork_status xwork_replay_engine_load_filesystem_ref(
    xwork_replay_engine *pEngine,
    const xwork_replay_filesystem_ref_options *pRef
);
XWORK_API xwork_status xwork_replay_engine_replay_filesystem_ref(
    xwork_replay_engine *pEngine,
    const xwork_replay_filesystem_ref_options *pExpected,
    xwork_replay_filesystem_ref_summary *pActual
);
XWORK_API xwork_status xwork_replay_engine_list_filesystem_refs(
    const xwork_replay_engine *pEngine,
    xwork_replay_filesystem_ref_summary_list *pList
);
XWORK_API xwork_status xwork_replay_engine_record_event(
    xwork_replay_engine *pEngine,
    const xwork_replay_event_options *pEvent
);
XWORK_API xwork_status xwork_replay_engine_load_event(
    xwork_replay_engine *pEngine,
    const xwork_replay_event_options *pEvent
);
XWORK_API xwork_status xwork_replay_engine_replay_event(
    xwork_replay_engine *pEngine,
    const xwork_replay_event_options *pExpected,
    xwork_replay_event_summary *pActual
);
XWORK_API xwork_status xwork_replay_engine_seek_checkpoint(
    xwork_replay_engine *pEngine,
    const char *sCheckpointId
);
XWORK_API xwork_status xwork_replay_engine_emit_report_artifact(
    const xwork_replay_engine *pEngine,
    xwork_run *pRun,
    const char *sArtifactId,
    xwork_artifact *pArtifact
);
XWORK_API xwork_status xwork_replay_engine_cancel(
    xwork_replay_engine *pEngine,
    const char *sReason
);
XWORK_API xwork_status xwork_replay_engine_get_manifest(
    const xwork_replay_engine *pEngine,
    xwork_replay_manifest *pManifest
);
XWORK_API xwork_status xwork_replay_engine_get_result(
    const xwork_replay_engine *pEngine,
    xwork_replay_result *pResult
);
XWORK_API xwork_status xwork_replay_engine_get_first_divergence(
    const xwork_replay_engine *pEngine,
    xwork_replay_divergence *pDivergence
);
XWORK_API xwork_status xwork_replay_engine_list_entries(
    const xwork_replay_engine *pEngine,
    xwork_replay_entry_summary_list *pList
);
XWORK_API xwork_status xwork_replay_engine_list_events(
    const xwork_replay_engine *pEngine,
    xwork_replay_event_summary_list *pList
);
XWORK_API xwork_status xwork_replay_hash_text(
    const char *sText,
    char *sBuffer,
    size_t iBufferSize
);
/*
 * Hash a JSON payload after normalization. Object keys are sorted and
 * insignificant whitespace is ignored. Invalid JSON returns
 * XWORK_ERROR_INVALID_ARGUMENT. Replay entry JSON fields use this hash when
 * the payload parses as JSON and fall back to text hashing for non-JSON text.
 */
XWORK_API xwork_status xwork_replay_hash_json(
    const char *sJson,
    char *sBuffer,
    size_t iBufferSize
);

XWORK_API xwork_status xwork_run_create(
    xwork_runtime *pRuntime,
    const xwork_run_options *pOptions,
    xwork_run **ppRun
);
XWORK_API void xwork_run_destroy(xwork_run *pRun);

XWORK_API xwork_status xwork_run_start(xwork_run *pRun);
XWORK_API xwork_status xwork_run_set_waiting_approval(xwork_run *pRun);
XWORK_API xwork_status xwork_run_set_waiting_tool(xwork_run *pRun);
XWORK_API xwork_status xwork_run_set_paused(xwork_run *pRun);
XWORK_API xwork_status xwork_run_submit_approval(
    xwork_run *pRun,
    xwork_approval_state eDecision
);
XWORK_API xwork_status xwork_run_load_checkpoint(
    xwork_run *pRun,
    const char *sCheckpointId
);
XWORK_API xwork_status xwork_run_resume(xwork_run *pRun);
XWORK_API xwork_status xwork_run_emit_artifact(
    xwork_run *pRun,
    const xwork_artifact_options *pOptions,
    xwork_artifact *pArtifact
);
XWORK_API xwork_status xwork_run_emit_patch_artifact(
    xwork_run *pRun,
    const xwork_patch_artifact_options *pOptions,
    xwork_artifact *pArtifact
);
XWORK_API xwork_status xwork_run_emit_report_artifact(
    xwork_run *pRun,
    const xwork_report_artifact_options *pOptions,
    xwork_artifact *pArtifact
);
XWORK_API xwork_status xwork_run_emit_output_artifact(
    xwork_run *pRun,
    const xwork_output_artifact_options *pOptions,
    xwork_artifact *pArtifact
);
XWORK_API xwork_status xwork_run_emit_command_artifact(
    xwork_run *pRun,
    const xwork_command_artifact_options *pOptions,
    xwork_artifact *pArtifact
);
XWORK_API xwork_status xwork_run_complete(xwork_run *pRun);
XWORK_API xwork_status xwork_run_cancel(xwork_run *pRun);
XWORK_API xwork_status xwork_run_fail(xwork_run *pRun);

XWORK_API const char *xwork_run_get_id(const xwork_run *pRun);
XWORK_API const char *xwork_run_get_instruction(const xwork_run *pRun);
XWORK_API xwork_run_state xwork_run_get_state(const xwork_run *pRun);
XWORK_API xwork_autonomy_mode xwork_run_get_autonomy(const xwork_run *pRun);
XWORK_API size_t xwork_run_get_workspace_count(const xwork_run *pRun);
XWORK_API const char *xwork_run_get_workspace_id(const xwork_run *pRun, size_t iIndex);
XWORK_API xwork_status xwork_run_get_summary(const xwork_run *pRun, xwork_run_summary *pSummary);
XWORK_API xwork_status xwork_run_get_snapshot(
    const xwork_run *pRun,
    xwork_run_snapshot *pSnapshot
);
XWORK_API xwork_status xwork_run_get_last_event(const xwork_run *pRun, xwork_event *pEvent);
XWORK_API xwork_status xwork_run_get_last_approval_request(
    const xwork_run *pRun,
    xwork_approval_request *pRequest
);
XWORK_API xwork_status xwork_run_get_last_checkpoint(
    const xwork_run *pRun,
    xwork_checkpoint *pCheckpoint
);
XWORK_API xwork_status xwork_run_get_last_memory_context(
    const xwork_run *pRun,
    xwork_memory_context *pContext
);
XWORK_API size_t xwork_run_get_event_count(const xwork_run *pRun);
XWORK_API xwork_status xwork_run_get_event(
    const xwork_run *pRun,
    size_t iIndex,
    xwork_event *pEvent
);
XWORK_API size_t xwork_run_get_step_count(const xwork_run *pRun);
XWORK_API xwork_status xwork_run_get_step(
    const xwork_run *pRun,
    size_t iIndex,
    xwork_run_step *pStep
);
XWORK_API xwork_status xwork_run_query_steps(
    const xwork_run *pRun,
    const xwork_run_step_query *pQuery,
    xwork_run_step_list *pList
);
XWORK_API size_t xwork_run_get_checkpoint_count(const xwork_run *pRun);
XWORK_API xwork_status xwork_run_get_checkpoint(
    const xwork_run *pRun,
    size_t iIndex,
    xwork_checkpoint *pCheckpoint
);
XWORK_API size_t xwork_run_get_artifact_count(const xwork_run *pRun);
XWORK_API xwork_status xwork_run_get_artifact(
    const xwork_run *pRun,
    size_t iIndex,
    xwork_artifact *pArtifact
);
XWORK_API const char *xwork_run_get_last_output_text(const xwork_run *pRun);
XWORK_API xwork_status xwork_policy_evaluate_approval(
    const xwork_policy_options *pPolicy,
    const xwork_approval_eval_input *pInput,
    xwork_approval_decision *pDecision
);
XWORK_API xwork_status xwork_policy_evaluate_network_access(
    const xwork_policy_options *pPolicy,
    const xwork_network_policy_eval_input *pInput,
    xwork_network_policy_decision *pDecision
);
XWORK_API xwork_status xwork_run_execute(
    xwork_run *pRun,
    const xwork_orchestrator_options *pOptions
);

/*
 * Starts xwork_run_execute() on a background worker.
 *
 * The async handle shallow-copies pOptions. Any callback pointers, callback
 * user data, profile strings, and caller-owned cancel token referenced by
 * pOptions must remain valid until the handle completes or is destroyed.
 *
 * pRun and the runtime objects it references must also outlive the async
 * handle. While the async handle is active, callers should observe progress
 * through xwork_run_async_* and must not concurrently execute, mutate, or
 * destroy pRun directly. A second xwork_run_execute() entry for the same run
 * while one execution is already active returns XWORK_ERROR_INVALID_STATE.
 *
 * If pOptions does not provide pCancelToken, the async handle creates and owns
 * one. xwork_run_async_destroy() cancels and waits for unfinished work before
 * releasing the handle.
 */
XWORK_API xwork_status xwork_run_execute_async(
    xwork_run *pRun,
    const xwork_orchestrator_options *pOptions,
    xwork_run_async **ppAsync
);
XWORK_API xwork_status xwork_run_async_wait(xwork_run_async *pAsync);

/*
 * Waits up to iTimeoutMs. On timeout, returns XWORK_OK and sets
 * *pbCompleted=false. If the worker has completed, returns the final run
 * status and sets *pbCompleted=true.
 */
XWORK_API xwork_status xwork_run_async_wait_timeout(
    xwork_run_async *pAsync,
    size_t iTimeoutMs,
    bool *pbCompleted
);

/*
 * Reads the async handle status under its handle lock. pStatus is final only
 * when *pbCompleted is true.
 */
XWORK_API xwork_status xwork_run_async_get_status(
    const xwork_run_async *pAsync,
    xwork_status *pStatus,
    bool *pbCompleted
);
XWORK_API xwork_status xwork_run_async_cancel(
    xwork_run_async *pAsync,
    const char *sReason
);
XWORK_API void xwork_run_async_destroy(xwork_run_async *pAsync);

#ifdef __cplusplus
}
#endif

#endif
