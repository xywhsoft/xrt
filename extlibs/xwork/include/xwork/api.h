#ifndef XWORK_API_H
#define XWORK_API_H

#include <xwork/features.h>
#include <xllm-session/features.h>
#include <xllm/features.h>
#include <xhttp-client/features.h>
#include <xhttp-policy/features.h>
#include <xhttp/features.h>
#include <xrt.h>

#if defined(XWORK_FEATURE_XWORK)

#if !defined(XRT_FEATURE_FILE_ROOT)
#error "xwork requires file_root (XRT_FEATURE_FILE_ROOT)"
#endif
#if !defined(XRT_FEATURE_CRYPTO_SHA256)
#error "xwork requires crypto_sha256 (XRT_FEATURE_CRYPTO_SHA256)"
#endif
#if !defined(XHTTP_FEATURE_HTTP_CLIENT_FUTURE)
#error "xwork requires http_client_future (XHTTP_FEATURE_HTTP_CLIENT_FUTURE)"
#endif
#if !defined(XHTTP_FEATURE_HTTP_SSE)
#error "xwork requires http_sse (XHTTP_FEATURE_HTTP_SSE)"
#endif
#if !defined(XRT_FEATURE_ONCE)
#error "xwork requires once (XRT_FEATURE_ONCE)"
#endif
#if !defined(XRT_FEATURE_FILE_LOCK)
#error "xwork requires file_lock (XRT_FEATURE_FILE_LOCK)"
#endif
#if !defined(XRT_FEATURE_TIME_LOCAL)
#error "xwork requires time_local (XRT_FEATURE_TIME_LOCAL)"
#endif
#if !defined(XHTTP_FEATURE_HTTP_SSE_PARSER)
#error "xwork requires http_sse_parser (XHTTP_FEATURE_HTTP_SSE_PARSER)"
#endif

/* The selected product requires its complete declared dependency set. */
#if !defined(XLLM_SESSION_FEATURE_XLLM_SESSION)
#error "xwork requires xllm_session (XLLM_SESSION_FEATURE_XLLM_SESSION)"
#endif
#if !defined(XRT_FEATURE_JSON_WRITE)
#error "xwork requires json_write (XRT_FEATURE_JSON_WRITE)"
#endif
#if !defined(XRT_FEATURE_REGEX)
#error "xwork requires regex (XRT_FEATURE_REGEX)"
#endif
#if !defined(XRT_FEATURE_FILE_TEMP)
#error "xwork requires file_temp (XRT_FEATURE_FILE_TEMP)"
#endif
#if !defined(XRT_FEATURE_FILE_TREE)
#error "xwork requires file_tree (XRT_FEATURE_FILE_TREE)"
#endif
#if !defined(XRT_FEATURE_PROCESS_RUN)
#error "xwork requires process_run (XRT_FEATURE_PROCESS_RUN)"
#endif


/*
 * xwork v2: the agent/tool-loop boundary above xllm and xllm-session.
 *
 * xwork owns orchestration, workspace policy, tool execution, artifacts and
 * compaction scheduling. It does not own provider protocols or CLI rendering.
 */

#include <xllm.h>
#include <xllm/executor.h>
#include <xllm-session.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XWORK_VERSION_MAJOR 3
#define XWORK_VERSION_MINOR 11
#define XWORK_VERSION_PATCH 0
#define XWORK_VERSION_TEXT "3.11.0"
#define XWORK_ABI_VERSION 9u
#define XWORK_EVENT_SCHEMA_VERSION 3u
#define XWORK_EVENT_SNAPSHOT_MAX_RECORDS 1024u
#define XWORK_ARTIFACT_READ_DEFAULT_BYTES (64u * 1024u)
#define XWORK_ARTIFACT_READ_MAX_BYTES (1024u * 1024u)

typedef struct xwork_runtime xwork_runtime;
typedef struct xwork_artifact_store xwork_artifact_store;
/* Same opaque xrt directory type; this forward declaration imports no modules. */
typedef struct xroot_impl* xroot;
typedef struct xwork_agent_definition xwork_agent_definition;
typedef struct xwork_agent xwork_agent;
typedef struct xwork_run xwork_run;
typedef struct xwork_tool_catalog xwork_tool_catalog;
typedef struct xwork_task_snapshot xwork_task_snapshot;
typedef struct xwork_event_snapshot xwork_event_snapshot;
typedef struct xwork_recovery_snapshot xwork_recovery_snapshot;
typedef struct xwork_tool_batch_plan xwork_tool_batch_plan;
typedef struct xwork_subagent_catalog xwork_subagent_catalog;
typedef struct xwork_mcp_client xwork_mcp_client;
typedef struct xwork_permission_resource_writer xwork_permission_resource_writer;
typedef struct xwork_tool_result_writer xwork_tool_result_writer;

typedef enum xwork_result {
    XWORK_RESULT_OK = 0,
    XWORK_RESULT_ERROR = -1,
    XWORK_RESULT_CANCELLED = -2,
    XWORK_RESULT_LIMIT = -3,
    XWORK_RESULT_TIMEOUT = -4
} xwork_result;

typedef enum xwork_error_code {
    XWORK_ERROR_NONE = 0,
    XWORK_ERROR_INVALID_ARGUMENT,
    XWORK_ERROR_OUT_OF_MEMORY,
    XWORK_ERROR_MODEL,
    XWORK_ERROR_TOOL,
    XWORK_ERROR_POLICY,
    XWORK_ERROR_IO,
    XWORK_ERROR_CONTEXT,
    XWORK_ERROR_LOOP_GUARD,
    XWORK_ERROR_CANCELLED,
    XWORK_ERROR_TIMEOUT,
    XWORK_ERROR_LIMIT,
    /* A durable tool call has an unknown side-effect outcome and requires an
     * explicit host decision before it may be retried or resolved. */
    XWORK_ERROR_RECOVERY_REQUIRED
} xwork_error_code;

typedef struct xwork_error {
    xwork_error_code eCode;
    char sMessage[1024];
    xllm_error tModelError;
    /* Non-model transport failure status (currently MCP HTTP), or zero when
     * no failing HTTP response was received. Never inferred from provider text.
     * xworkErrorInit and successful public operations clear this field. Model
     * HTTP failures remain in tModelError.iHttpStatus. ABI 8 requires rebuilding
     * callers and the runtime together because this structure grew. */
    uint32_t uHttpStatus;
} xwork_error;

/* Tool effects are a bitset. A descriptor may declare every externally
 * visible effect that one invocation can exercise. Permission policy must
 * never reduce this trusted declaration from untrusted runtime metadata. */
typedef uint64_t xwork_tool_effects;
typedef xwork_tool_effects xwork_tool_effect; /* source-compatible type alias */
typedef enum xwork_tool_effect_flag {
    XWORK_TOOL_EFFECT_READ = 1u << 0,
    XWORK_TOOL_EFFECT_WORKSPACE_WRITE = 1u << 1,
    XWORK_TOOL_EFFECT_PROCESS = 1u << 2,
    XWORK_TOOL_EFFECT_NETWORK = 1u << 3,
    XWORK_TOOL_EFFECT_EXTERNAL_SERVICE = 1u << 4,
    XWORK_TOOL_EFFECT_SECRETS = 1u << 5,
    XWORK_TOOL_EFFECT_SCHEDULE = 1u << 6,
    XWORK_TOOL_EFFECT_AGENT_DELEGATION = 1u << 7
} xwork_tool_effect_flag;
#define XWORK_TOOL_EFFECT_READ_ONLY XWORK_TOOL_EFFECT_READ
#define XWORK_TOOL_EFFECT_ALL ((xwork_tool_effects)0xffu)

typedef enum xwork_approval_mode {
    /* Execute tools automatically inside the configured workspace sandbox. */
    XWORK_APPROVAL_AUTO = 0,
    /* Ask the host callback before workspace writes and process execution. */
    XWORK_APPROVAL_CALLBACK,
    /* Allow reads but reject all mutating tools. */
    XWORK_APPROVAL_READ_ONLY
} xwork_approval_mode;

/* Line-ending discipline for text tools. The model always works in LF
 * space; storage converts per policy. AUTO keeps each file's dominant
 * ending (new files LF), which heals mixed endings on first write-back. */
typedef enum xwork_eol_policy {
    XWORK_EOL_AUTO = 0,
    XWORK_EOL_FORCE_LF,
    XWORK_EOL_FORCE_CRLF,
    XWORK_EOL_PRESERVE   /* legacy strict mode: raw bytes, exact matching */
} xwork_eol_policy;

typedef enum xwork_permission_decision {
    XWORK_PERMISSION_DEFAULT = 0,
    XWORK_PERMISSION_ALLOW,
    XWORK_PERMISSION_DENY
} xwork_permission_decision;

typedef enum xwork_resource_kind {
    XWORK_RESOURCE_NONE = 0,
    XWORK_RESOURCE_PATH,
    XWORK_RESOURCE_COMMAND,
    XWORK_RESOURCE_PROCESS,
    XWORK_RESOURCE_NETWORK,
    XWORK_RESOURCE_EXTERNAL_SERVICE,
    XWORK_RESOURCE_SECRET,
    XWORK_RESOURCE_SCHEDULE,
    XWORK_RESOURCE_AGENT
} xwork_resource_kind;

typedef uint32_t xwork_resource_access;
typedef enum xwork_resource_access_flag {
    XWORK_RESOURCE_ACCESS_READ = 1u << 0,
    XWORK_RESOURCE_ACCESS_WRITE = 1u << 1,
    XWORK_RESOURCE_ACCESS_EXECUTE = 1u << 2,
    XWORK_RESOURCE_ACCESS_CONTROL = 1u << 3,
    XWORK_RESOURCE_ACCESS_CONNECT = 1u << 4,
    XWORK_RESOURCE_ACCESS_USE = 1u << 5
} xwork_resource_access_flag;

typedef struct xwork_permission_resource {
    xwork_resource_kind eKind;
    xwork_resource_access uAccess;
    const char* sResource; /* borrowed for the permission callback */
} xwork_permission_resource;

typedef enum xwork_risk_level {
    XWORK_RISK_LOW = 0,
    XWORK_RISK_MEDIUM,
    XWORK_RISK_HIGH
} xwork_risk_level;

typedef struct xwork_permission_request {
    uint64_t uRequestId;
    uint64_t uAgentId;
    uint64_t uRunId;
    uint64_t uCatalogGeneration;
    const char* sToolName;
    const char* sToolCallId;
    xwork_tool_effects uEffects;
    xwork_risk_level eRisk;
    const xwork_permission_resource* pResources;
    size_t iResourceCount;
    const char* sArgumentsJson;
    const char* sWorkspaceRoot;
    uint64_t uAgentTurn;
    xcancel* pCancel;       /* borrowed for the callback */
    uint64_t uDeadline;    /* absolute monotonic deadline; zero means none */
} xwork_permission_request;

typedef enum xwork_hook_phase {
    XWORK_HOOK_BEFORE_TOOL = 0,
    XWORK_HOOK_AFTER_TOOL
} xwork_hook_phase;

typedef enum xwork_hook_action {
    XWORK_HOOK_CONTINUE = 0,
    XWORK_HOOK_DENY,
    XWORK_HOOK_CANCEL
} xwork_hook_action;

typedef struct xwork_hook_event {
    xwork_hook_phase ePhase;
    uint64_t uAgentTurn;
    const char* sToolName;
    xwork_tool_effects uEffects;
    const char* sArgumentsJson;
    const char* sOutput;
    bool bSuccess;
} xwork_hook_event;

typedef struct xwork_tool_context {
    xwork_agent* pAgent;
    const char* sWorkspaceRoot;
    const char* sToolName;
    const char* sToolCallId;
    xwork_tool_effects uEffects;
    uint64_t uAgentTurn;
    uint64_t uAgentId;
    uint64_t uRunId;
    uint64_t uCatalogGeneration;
    xcancel* pCancel;       /* borrowed for the callback */
    uint64_t uDeadline;    /* absolute monotonic deadline; zero means none */
} xwork_tool_context;

typedef struct xwork_tool_output {
    char* sContent;
    bool bSuccess;
    /* Image passthrough (read): owned bytes + mime; a text summary rides
     * sContent. Images bypass the text truncation/spill path. */
    unsigned char* pImageBytes;   /* owned */
    size_t iImageSize;
    char sImageMime[32];          /* "image/png" etc.; empty when no image */
} xwork_tool_output;

typedef xwork_result (*xwork_tool_execute_fn)(
    void* pUserData,
    const xwork_tool_context* pContext,
    const char* sArgumentsJson,
    xwork_tool_output* pOutput,
    xwork_error* pError
);

/* vNext callbacks never transfer module-allocated memory to the host.
 * Writers copy every supplied byte during the callback and are valid only
 * for that callback. Permission discovery must be side-effect free. */
typedef xwork_result (*xwork_tool_describe_permissions_fn)(
    void* pUserData,
    const xwork_tool_context* pContext,
    const char* sArgumentsJson,
    xwork_permission_resource_writer* pWriter,
    xwork_error* pError
);

typedef xwork_result (*xwork_tool_execute_v2_fn)(
    void* pUserData,
    const xwork_tool_context* pContext,
    const char* sArgumentsJson,
    xwork_tool_result_writer* pWriter,
    xwork_error* pError
);

/* Optional lifetime pin for callbacks and user data owned by a dynamic code
 * generation. Catalog construction retains once per copied tool entry and
 * catalog destruction releases exactly once. Both callbacks must be supplied
 * together. They must be thread-safe, non-blocking, and must not re-enter
 * xwork. The retain callback leaves ownership unchanged when it returns false. */
typedef bool (*xwork_tool_owner_retain_fn)(void* pUserData);
typedef void (*xwork_tool_owner_release_fn)(void* pUserData);

typedef struct xwork_tool_definition {
    const char* sName;
    const char* sDescription;
    const char* sParametersJson;
    bool bStrict;
    xwork_tool_effects uEffects;
    xwork_tool_execute_fn OnExecute;
    void* pUserData;
    /* Stable owner namespace used for discovery and bulk replacement. When
     * omitted the registry records "application". Catalogs copy descriptor
     * strings. Callbacks and pUserData are borrowed unless the paired owner
     * callbacks below retain their code/data generation. */
    const char* sSource;
    xwork_tool_describe_permissions_fn OnDescribePermissions;
    xwork_tool_execute_v2_fn OnExecuteV2;
    size_t iMaxResultBytes; /* 0 selects the agent's captured-output limit. */
    xwork_tool_owner_retain_fn OnOwnerRetain;
    xwork_tool_owner_release_fn OnOwnerRelease;
    bool bParallelSafe;
    /* NULL selects the source for serial tools and no group for parallel-safe
     * tools. Supplying a group keeps matching tools mutually exclusive. */
    const char* sSerialGroup;
} xwork_tool_definition;

typedef struct xwork_tool_info {
    const char* sName;
    const char* sDescription;
    const char* sParametersJson;
    const char* sSource;
    bool bStrict;
    xwork_tool_effects uEffects;
    uint64_t uCatalogGeneration;
    bool bDescribesPermissions;
    bool bUsesResultWriter;
    size_t iMaxResultBytes;
    bool bParallelSafe;
    const char* sSerialGroup;
} xwork_tool_info;

/* Scheduling metadata is separate from the legacy tool descriptor so its
 * layout remains stable. Tools are batch-serial by default. Parallel-safe
 * tools may overlap only when their declared resources do not conflict. */
typedef struct xwork_tool_concurrency_config {
    uint32_t uSize;
    uint32_t uAbiVersion;
    const char* sToolName;
    bool bParallelSafe;
    const char* sSerialGroup; /* NULL selects the source as the group name. */
} xwork_tool_concurrency_config;

typedef struct xwork_tool_batch_item_info {
    uint32_t uSize;
    uint32_t uAbiVersion;
    size_t iSequence;
    uint32_t uWave;
    const char* sToolName;
    xwork_tool_effects uEffects;
    bool bParallelSafe;
    const char* sSerialGroup;
    const xwork_permission_resource* pResources;
    size_t iResourceCount;
} xwork_tool_batch_item_info;

typedef enum xwork_event_kind {
    XWORK_EVENT_AGENT_START = 0,
    XWORK_EVENT_MODEL_START,
    XWORK_EVENT_MODEL_TEXT_DELTA,
    XWORK_EVENT_MODEL_REASONING_DELTA,
    XWORK_EVENT_MODEL_DONE,
    XWORK_EVENT_TOOL_START,
    XWORK_EVENT_TOOL_DONE,
    XWORK_EVENT_COMPACTION_START,
    XWORK_EVENT_COMPACTION_REJECTED,
    XWORK_EVENT_COMPACTION_DONE,
    XWORK_EVENT_AGENT_DONE,
    XWORK_EVENT_ERROR,
    XWORK_EVENT_ARTIFACT_CREATED,
    XWORK_EVENT_TASK_UPDATED,
    XWORK_EVENT_RECOVERY_REQUIRED,
    XWORK_EVENT_RECOVERY_RESOLVED
} xwork_event_kind;

typedef enum xwork_recovery_action {
    /* Retry only when the current registered descriptor has no effect other
     * than READ. An unresolved effectful or unknown tool stops recovery. */
    XWORK_RECOVERY_DEFAULT = 0,
    /* Explicitly accept at-least-once execution for this tool-call ID. */
    XWORK_RECOVERY_RETRY,
    /* Do not execute the tool. Append a durable uncertainty result so the
     * model can continue and ask the user to inspect external state. */
    XWORK_RECOVERY_RECORD_UNCERTAIN
} xwork_recovery_action;

typedef struct xwork_event {
    xwork_event_kind eKind;
    uint64_t uAgentTurn;
    uint32_t uAgentDepth;
    uint64_t uDelegationId;
    uint64_t uParentAgentTurn;
    const char* sText;
    size_t iTextLength;
    const char* sToolName;
    const char* sToolCallId;
    const char* sArtifactPath;
    const char* sModel;
    const char* sProviderRequestId;
    const char* sProviderCode;
    const char* sProviderMessage;
    const char* sFinishReason;
    const char* sRequestFingerprint;
    size_t iMessageCount;
    size_t iToolDefinitionCount;
    size_t iResponseToolCallCount;
    uint32_t uMaxOutputTokens;
    uint32_t uHttpStatus;
    xllm_error_code eModelErrorCode;
    bool bSuccess;
    uint32_t uCompactionAttempt;
    xllm_compaction_quality tCompactionQuality;
    xllm_usage tUsage;
    xllm_diagnostics tDiagnostics;
    xllm_session_stats tSessionStats;
    uint64_t uParentRunId;
    /* Runtime-assigned replay/audit identity. Schema version 1 introduced the
     * audit suffix; version 2 adds explicit recovery fields at its tail. */
    uint32_t uSchemaVersion;
    uint64_t uEventId;
    int64_t iOccurredAtUs;
    uint64_t uAgentId;
    uint64_t uRunId;
    uint64_t uTaskId;
    uint64_t uArtifactId;
    xwork_tool_effects uEffects;
    bool bEffectApplied;
    bool bTextTruncated;
    bool bMetadataTruncated;
    size_t iOriginalTextLength;
    uint32_t eTaskEvent; /* xwork_task_event_kind when kind is TASK_UPDATED */
    uint32_t eTaskKind;  /* xwork_task_kind */
    uint32_t eTaskState; /* xwork_task_state */
    uint64_t uTaskRevision;
    uint32_t eRecoveryAction; /* xwork_recovery_action */
    bool bRecoveryAutomatic;
    bool bRecoveryUncertain;
    /* Sequence of the durable user message added for AGENT_START. Zero for
     * resumed runs and all other event kinds. This is a ledger boundary,
     * distinct from the agent turn and event IDs. */
    uint64_t uUserMessageSequence;
} xwork_event;

/* Immutable view of one unresolved durable tool call. Strings are owned by
 * the recovery snapshot and remain valid until it is released. Effects and
 * catalog generation describe the agent's current immutable tool catalog;
 * they do not prove whether an earlier process completed the call. */
typedef struct xwork_recovery_call_info {
    uint32_t uSize;
    uint32_t uAbiVersion;
    uint64_t uTurn;
    uint64_t uCatalogGeneration;
    const char* sToolCallId;
    const char* sToolName;
    const char* sArgumentsJson;
    xwork_tool_effects uEffects;
    bool bToolAvailable;
    bool bAutomaticRetrySafe;
} xwork_recovery_call_info;

typedef struct xwork_recovery_decision {
    uint32_t uSize;
    uint32_t uAbiVersion;
    const char* sToolCallId;
    xwork_recovery_action eAction;
} xwork_recovery_decision;

typedef struct xwork_resume_options {
    uint32_t uSize;
    uint32_t uAbiVersion;
    const xwork_recovery_decision* pDecisions;
    size_t iDecisionCount;
} xwork_resume_options;

/* Return false to request cooperative cancellation. Callbacks may be invoked
 * from a background delegation thread (agent tool, background=true): hosts
 * must treat these callbacks as thread-safe and must not mutate the agent
 * or its session inside them. The child's cancellation does not propagate
 * to the parent agent. */
typedef bool (*xwork_event_fn)(void* pUserData, const xwork_event* pEvent);

typedef enum xwork_artifact_kind {
    XWORK_ARTIFACT_TOOL_OUTPUT = 0
} xwork_artifact_kind;

/* Borrowed runtime-owned metadata. Paths are workspace-relative and remain
 * valid while the caller holds a live runtime reference. */
typedef struct xwork_artifact_info {
    uint32_t uSize;
    uint32_t uAbiVersion;
    uint64_t uArtifactId;
    xwork_artifact_kind eKind;
    int64_t iCreatedAtUs;
    uint64_t uAgentId;
    uint64_t uRunId;
    uint64_t uTaskId;
    const char* sSource;
    const char* sPath;
    const char* sMediaType;
    uint64_t uSizeBytes;
    char sSha256[65];
} xwork_artifact_info;

/* Owned bounded read result. Unit releases pData. */
typedef struct xwork_artifact_chunk {
    uint32_t uSize;
    uint32_t uAbiVersion;
    unsigned char* pData;
    size_t iSize;
    uint64_t uOffset;
    uint64_t uTotalSize;
    bool bEof;
} xwork_artifact_chunk;

/* Return true to approve the requested side effect. */
typedef bool (*xwork_approval_fn)(
    void* pUserData,
    const char* sToolName,
    xwork_tool_effects uEffects,
    const char* sArgumentsJson
);

/* Structured per-call policy. DEFAULT falls back to eApprovalMode/OnApproval. */
typedef xwork_permission_decision (*xwork_permission_fn)(
    void* pUserData,
    const xwork_permission_request* pRequest
);

/* DENY is a tool-level rejection before execution. After execution it marks
 * the tool result failed because an already-completed side effect cannot be undone. */
typedef xwork_hook_action (*xwork_hook_fn)(void* pUserData, const xwork_hook_event* pEvent);

/* Injectable model boundary used by tests and offline hosts. */
typedef xllm_result (*xwork_model_complete_fn)(
    void* pUserData,
    const xllm_request* pRequest,
    const xllm_stream_callbacks* pCallbacks,
    xllm_response** ppResponse,
    xllm_error* pError
);

/* Optional lifetime pin for every borrowed agent dependency: session, model
 * client, callback user data, and host policy state.  The retain callback is
 * invoked once before an agent becomes observable; final agent destruction
 * invokes the paired release.  This matters when a run, schedule, or
 * background subagent outlives the host's original agent reference. */
typedef bool (*xwork_agent_owner_retain_fn)(void* pUserData);
typedef void (*xwork_agent_owner_release_fn)(void* pUserData);

/* Host-owned role hooks. Values are borrowed only for the callback. The
 * callback owner is pinned by the normal Agent owner callbacks. */
typedef enum xwork_agent_extension_phase {
    XWORK_AGENT_EXTENSION_RUN = 1,
    XWORK_AGENT_EXTENSION_REQUEST,
    XWORK_AGENT_EXTENSION_RESPONSE,
    XWORK_AGENT_EXTENSION_CHILD,
    XWORK_AGENT_EXTENSION_BEFORE_TOOL,
    XWORK_AGENT_EXTENSION_AFTER_TOOL,
    XWORK_AGENT_EXTENSION_CONTEXT,
    XWORK_AGENT_EXTENSION_RELEASE,
    XWORK_AGENT_EXTENSION_CHILD_READY,
    XWORK_AGENT_EXTENSION_MODEL_DISPATCH
} xwork_agent_extension_phase;
struct xwork_run_result;
typedef xwork_result (*xwork_agent_default_run_fn)(xwork_agent*,const char*,
    const xllm_message*,bool,const xwork_resume_options*,struct xwork_run_result*,xwork_error*);
typedef struct xwork_agent_extension_context {
    xwork_agent_extension_phase ePhase;
    xwork_agent* pAgent;
    xwork_agent* pParent;
    const xllm_session_snapshot* pParentSnapshot;
    char** psDelegationPrompt; /* replacement uses malloc/free ownership */
    const char* sDefinitionId;
    xllm_session* pSession;
    xcancel* pCancel;
    uint64_t uDeadline;
    xllm_request* pRequest;
    xllm_response* pResponse;
    const xwork_hook_event* pToolEvent;
    xwork_tool_output* pToolOutput;
    const char* sPrompt;
    const xllm_message* pUserMessage;
    bool bResume;
    const xwork_resume_options* pResumeOptions;
    struct xwork_run_result* pResult;
    xwork_agent_default_run_fn DefaultRun;
    bool bHandled;
    bool bCompactAndRetry; /* REQUEST: one bounded rebuild before dispatch */
    const xwork_tool_catalog* pDelegationTools;
    const void* pDelegationToolOwner;
    bool bOverrideDelegationApproval;
    xwork_approval_mode eDelegationApproval;
} xwork_agent_extension_context;
typedef xwork_result (*xwork_agent_extension_fn)(void*,xwork_agent_extension_context*,xwork_error*);

typedef struct xwork_agent_config {
    /* Borrowed dependencies; they must outlive the agent. */
    xllm_client* pClient;
    xllm_session* pSession;

    const char* sWorkspaceRoot;
    const char* sSystemPrompt;
    const char* sSessionPath;
    const char* sArtifactDirectory;
    const char* sModel;
    const char* sReasoningEffort;
    xcancel* pCancel;
    uint64_t uDeadline;                 /* Absolute xrt deadline; zero means none. */

    xwork_approval_mode eApprovalMode;
    xwork_approval_fn OnApproval;
    void* pApprovalUserData;
    xwork_permission_fn OnPermission;
    void* pPermissionUserData;
    xwork_hook_fn OnHook;
    void* pHookUserData;
    xwork_eol_policy eEolPolicy;

    xwork_event_fn OnEvent;
    void* pEventUserData;

    xwork_model_complete_fn OnModelComplete;
    void* pModelUserData;
    xwork_agent_extension_fn OnExtension;
    void* pExtensionUserData;

    uint32_t uCommandTimeoutMs;
    uint32_t uMaxAgentTurns;          /* 0 means unlimited. */
    uint32_t uRepeatedToolBatchLimit;
    uint32_t uConsecutiveFailureLimit;
    uint32_t uMaxManagedProcesses;
    uint32_t uCompletionVerificationRetries; /* Premature final answers after a write; default 2. */
    uint32_t uCompactionQualityRetries;       /* Retries after a structurally rejected summary. */
    size_t iMaxInlineToolBytes;
    size_t iMaxCapturedCommandBytes;
    bool bRegisterBuiltinTools;
    bool bAutoSaveSession;
    bool bAllowArtifactWrites;
    bool bRequireVerificationAfterWrite;      /* Require successful exec after latest write. */
    /* Opt-in identity injection: when true and the session is empty, creation
     * pins sSystemPrompt as the system message. The host owns identity by
     * default (see xllmSessionSetSystemPrompt); only loop-style hosts that
     * want xwork's default persona enable this. */
    bool bInjectSystemPrompt;
    uint32_t uMaxParallelTools;       /* 0 selects 4; hard limit 64. */
    uint32_t uMaxSubagentDepth;       /* 0 selects 1; hard limit 16. */
    uint32_t uMaxConcurrentSubagents; /* 0 selects 4; hard limit 64. */
    void* pOwnerUserData;
    xwork_agent_owner_retain_fn OnOwnerRetain;
    xwork_agent_owner_release_fn OnOwnerRelease;
    xwork_artifact_store* pArtifactStore; /* retained; overrides sArtifactDirectory */
} xwork_agent_config;

typedef struct xwork_run_result {
    char* sFinalText;
    uint64_t uAgentTurns;
    uint64_t uModelCalls;
    uint64_t uToolCalls;
    uint64_t uCompactions;
    uint64_t uRejectedCompactionSummaries;
    uint32_t uAgentDepth;
    uint64_t uDelegationId;
    xllm_usage tLastUsage;
    xllm_session_stats tFinalSessionStats;
} xwork_run_result;

/* Process-wide owner for task, tool-catalog, MCP, scheduler, permission,
 * artifact, and audit state.  Extensible public records start with
 * uSize/uAbiVersion; callers initialize them before overriding fields. */
typedef struct xwork_runtime_config {
    uint32_t uSize;
    uint32_t uAbiVersion;
    uint32_t uMaxTasks;             /* 0 selects the bounded default (64). */
    uint32_t uMaxCatalogTools;      /* 0 selects the bounded default (4096). */
    size_t iMaxRetainedTaskBytes;   /* 0 selects 8 MiB per task. */
    uint32_t uMaxSchedules;         /* 0 selects the bounded default (256). */
    uint32_t uMaxArtifacts;         /* lifetime records; 0 selects 4096. */
    uint32_t uMaxEvents;            /* replay window; 0 selects 4096. */
    size_t iMaxRetainedEventBytes;  /* 0 selects 8 MiB. */
    size_t iMaxEventTextBytes;      /* per event; 0 selects 64 KiB. */
} xwork_runtime_config;

/* Immutable policy generation shared by one or more session agents. */
typedef struct xwork_agent_definition_config {
    uint32_t uSize;
    uint32_t uAbiVersion;
    const char* sId;
    const char* sSystemPrompt;
    const char* sModel;
    const char* sReasoningEffort;
    xwork_approval_mode eApprovalMode;
    xwork_eol_policy eEolPolicy;
    uint32_t uCommandTimeoutMs;
    uint32_t uMaxAgentTurns;
    uint32_t uRepeatedToolBatchLimit;
    uint32_t uConsecutiveFailureLimit;
    uint32_t uMaxManagedTasks;
    uint32_t uCompletionVerificationRetries;
    uint32_t uCompactionQualityRetries;
    size_t iMaxInlineToolBytes;
    size_t iMaxCapturedCommandBytes;
    bool bRegisterBuiltinTools;
    bool bAutoSaveSession;
    bool bAllowArtifactWrites;
    bool bRequireVerificationAfterWrite;
    bool bInjectSystemPrompt;
    uint32_t uMaxParallelTools;       /* 0 selects 4; hard limit 64. */
    uint32_t uMaxSubagentDepth;       /* 0 selects 1; hard limit 16. */
    uint32_t uMaxConcurrentSubagents; /* 0 selects 4; hard limit 64. */
} xwork_agent_definition_config;

/* Session-specific bindings. Dependencies and callback user data are
 * borrowed. They must outlive the agent and its active runs unless the
 * optional owner callbacks below pin their aggregate lifetime. */
typedef struct xwork_agent_options {
    uint32_t uSize;
    uint32_t uAbiVersion;
    xllm_client* pClient;
    xllm_session* pSession;
    const char* sWorkspaceRoot;
    const char* sSessionPath;
    const char* sArtifactDirectory;
    xcancel* pCancel;
    uint64_t uDeadline;                 /* Absolute xrt deadline; zero means none. */
    xwork_approval_fn OnApproval;
    void* pApprovalUserData;
    xwork_permission_fn OnPermission;
    void* pPermissionUserData;
    xwork_hook_fn OnHook;
    void* pHookUserData;
    xwork_event_fn OnEvent;
    void* pEventUserData;
    xwork_model_complete_fn OnModelComplete;
    void* pModelUserData;
    xwork_agent_extension_fn OnExtension;
    void* pExtensionUserData;
    void* pOwnerUserData;
    xwork_agent_owner_retain_fn OnOwnerRetain;
    xwork_agent_owner_release_fn OnOwnerRelease;
    xwork_artifact_store* pArtifactStore; /* retained; overrides sArtifactDirectory */
} xwork_agent_options;

typedef enum xwork_run_state {
    XWORK_RUN_CREATED = 0,
    XWORK_RUN_RUNNING,
    XWORK_RUN_SUCCEEDED,
    XWORK_RUN_FAILED,
    XWORK_RUN_CANCELLED,
    XWORK_RUN_TIMED_OUT
} xwork_run_state;

typedef struct xwork_run_config {
    uint32_t uSize;
    uint32_t uAbiVersion;
    const char* sPrompt;
    bool bResume;
    xcancel* pCancel;
    uint64_t uDeadline;
    xwork_event_fn OnEvent;          /* NULL inherits the agent callback. */
    void* pEventUserData;
    /* Optional only with bResume. RunCreate deep-copies every decision and
     * call ID, so caller storage may be released immediately afterwards. */
    const xwork_resume_options* pResumeOptions;
} xwork_run_config;

typedef struct xwork_run_info {
    uint32_t uSize;
    uint32_t uAbiVersion;
    uint64_t uRunId;
    xwork_run_state eState;
    xwork_result eResult;
    uint64_t uCreatedUs;
    uint64_t uStartedUs;
    uint64_t uEndedUs;
    bool bResume;
} xwork_run_info;

typedef struct xwork_readonly_subagent_config {
    const char* sSystemPrompt;
    xwork_permission_fn OnPermission;
    void* pPermissionUserData;
    xwork_event_fn OnEvent;
    void* pEventUserData;
    uint64_t uParentAgentTurn;
    uint32_t uTimeoutMs;
    uint32_t uMaxAgentTurns;
    uint32_t uMaxOutputTokens;
    size_t iMaxFinalBytes;
} xwork_readonly_subagent_config;

XRT_API void xworkErrorInit(xwork_error* pError);
XRT_API const char* xworkErrorCodeName(xwork_error_code eCode);
XRT_API void xworkRecoveryCallInfoInit(xwork_recovery_call_info* pInfo);
XRT_API void xworkRecoveryDecisionInit(xwork_recovery_decision* pDecision);
XRT_API void xworkResumeOptionsInit(xwork_resume_options* pOptions);
/* Shared process output storage. Generated into the locked xwork public header.
 * Options.Directory is an anchored, caller-owned artifact root. Explicit paths
 * must be resolved and authorized by the caller before Create. Returned values
 * are owned xvalues. No result read consumes or advances a shared cursor. */
typedef struct xwork_process_output xwork_process_output;
typedef struct xvalue xvalue;
typedef struct xwork_process_output_options {
    uint32_t Size;
    xroot Directory;
    const char* StdoutPath;
    const char* StderrPath;
    size_t InlineBytes;
    uint64_t MaxBytes;
    bool FilesFromStart;
    bool MergeStderr;
} xwork_process_output_options;
XRT_API void xworkProcessOutputOptionsInit(xwork_process_output_options* Options);
XRT_API xwork_process_output* xworkProcessOutputCreate(
    const xwork_process_output_options* Options, xwork_error* Error);
XRT_API bool xworkProcessOutputWrite(xwork_process_output* Output, bool Stderr,
    const void* Bytes, size_t Size);
XRT_API void xworkProcessOutputFinish(xwork_process_output* Output);
XRT_API xvalue* xworkProcessOutputSnapshot(xwork_process_output* Output, bool IncludeText);
XRT_API void xworkProcessOutputDestroy(xwork_process_output* Output);
/* Removes only recognized, unlocked process-output directories; never follows
 * links or removes explicit output destinations. Age is measured in seconds. */
XRT_API bool xworkProcessOutputCollect(xroot Directory, uint64_t MaxBytes, uint64_t MaxAgeSeconds);

XRT_API void xworkToolOutputInit(xwork_tool_output* pOutput);
XRT_API void xworkToolOutputUnit(xwork_tool_output* pOutput);
XRT_API bool xworkToolOutputSet(xwork_tool_output* pOutput, bool bSuccess, const char* sContent);
/* Attach an image payload (copies); the text summary should already be in
 * sContent via xworkToolOutputSet. */
XRT_API bool xworkToolOutputSetImage(xwork_tool_output* pOutput,
    const unsigned char* pData, size_t iSize, const char* sMime);

/* Writer APIs for dynamically compiled tools. Inputs are copied before the
 * function returns; callers retain ownership. A result must set success once
 * and write at least one text byte. Writers reject overflow and use-after-
 * callback without exposing allocator details across the module ABI. */
XRT_API bool xworkPermissionResourceWriterAdd(
    xwork_permission_resource_writer* pWriter,
    xwork_resource_kind eKind,
    xwork_resource_access uAccess,
    const char* sResource
);
XRT_API bool xworkToolResultWriterSetSuccess(
    xwork_tool_result_writer* pWriter, bool bSuccess);
XRT_API bool xworkToolResultWriterWrite(
    xwork_tool_result_writer* pWriter, const char* pText, size_t iLength);
XRT_API bool xworkToolResultWriterWriteText(
    xwork_tool_result_writer* pWriter, const char* sText);
XRT_API bool xworkToolResultWriterSetImage(
    xwork_tool_result_writer* pWriter,
    const unsigned char* pData, size_t iSize, const char* sMime);

XRT_API void xworkAgentConfigInit(xwork_agent_config* pConfig);
XRT_API void xworkReadOnlySubagentConfigInit(xwork_readonly_subagent_config* pConfig);
XRT_API xwork_agent* xworkAgentCreate(const xwork_agent_config* pConfig, xwork_error* pError);
XRT_API void xworkAgentDestroy(xwork_agent* pAgent);

/* v3 object model. A runtime and definition are reference-counted; Release
 * drops one owned reference. An agent retains both objects. A run retains its
 * agent, so releasing the host's agent reference cannot invalidate an active
 * run. Runtime-owned background tasks are unaffected when a completed run is
 * destroyed. */
XRT_API void xworkRuntimeConfigInit(xwork_runtime_config* pConfig);
XRT_API xwork_runtime* xworkRuntimeCreate(const xwork_runtime_config* pConfig, xwork_error* pError);
XRT_API xwork_runtime* xworkRuntimeRef(xwork_runtime* pRuntime);
XRT_API void xworkRuntimeRelease(xwork_runtime* pRuntime);
XRT_API size_t xworkRuntimeTaskCount(const xwork_runtime* pRuntime);

/* Host-owned artifact storage, separate from model workspace permissions.
 * Directory must already exist. Create clones its anchored xrt handle; the
 * caller may close Directory immediately afterwards. Agents, descendants and
 * registry entries retain Store. No user/tool path is accepted by this API.
 * xrtRootPath is normalized against the current directory for display only;
 * it may become stale after a move. All storage IO uses the owned handle.
 * Ordinary file tools stay confined to their workspace. A caller must own a
 * live reference while calling Ref/Release or binding a store to an agent. */
XRT_API xwork_artifact_store* xworkArtifactStoreCreate(xroot Directory, xwork_error* pError);
XRT_API xwork_artifact_store* xworkArtifactStoreRef(xwork_artifact_store* Store);
XRT_API void xworkArtifactStoreRelease(xwork_artifact_store* Store);

typedef struct xwork_tool_request_token {
    uint32_t uSize;
    uint32_t uAbiVersion;
    char sId[33];                   /* opaque 128-bit lower-case hex ID */
    int64_t iCreatedAt;              /* Unix seconds; caller enforces service retention */
    bool bPersistent;
    bool bRecovered;                /* an existing token, including a publication race */
} xwork_tool_request_token;

/* Prepare before submitting an externally idempotent request. A host-declared
 * recoverable read with writable session storage retains one immutable token
 * per exact source/tool/call/arguments/turn. Scope (bounded UTF-8, no controls)
 * binds the service and account; changing it or damaging the record fails
 * without generating a replacement. No payload or credential is stored.
 * Other callbacks, nested tasks and ephemeral sessions receive a fresh token
 * with bPersistent=false. This does not make an arbitrary POST retry-safe.
 * Writer and Context must belong to this live v2 callback. */
XRT_API void xworkToolRequestTokenInit(xwork_tool_request_token* Token);
XRT_API bool xworkToolResultWriterPrepareRequest(xwork_tool_result_writer* Writer,
    const xwork_tool_context* Context, const char* Scope,
    xwork_tool_request_token* Token, xwork_error* Error);

/* Publish a full UTF-8 result file from a live v2 callback and attach it to the
 * completion event. The writer and Context must belong to this callback.
 * Metadata is borrowed from the runtime registry. Files use the existing
 * host-owned store and non-replacing publication; ordinary workspace writes
 * are not enabled. Model file reads use @results/<run-directory>/<filename>.
 * No artifact is published when persistence is disabled. */
XRT_API bool xworkToolResultWriterSaveArtifact(xwork_tool_result_writer* Writer,
    const xwork_tool_context* Context, const char* Text, size_t Size,
    const char* MediaType, xwork_artifact_info* Info, xwork_error* Error);

/* Explicitly declare a read operation safe to recover. Publication creates a
 * new immutable catalog generation. This never changes access permissions and
 * rejects descriptors with write/process/schedule/delegation effects.
 * Completed results in a host store are reused before invoking the callback.
 * Default descriptors retain the conservative effect-derived policy. */
XRT_API bool xworkRuntimeConfigureToolRecovery(xwork_runtime* Runtime,
    const char* ToolName, bool RetrySafeRead, xwork_error* Error);

/* Runtime artifact registry and bounded reads. Artifact IDs never alias within
 * one runtime. The registry owns metadata, while files remain ordinary host
 * files below the creating agent's workspace. */
XRT_API void xworkArtifactInfoInit(xwork_artifact_info* pInfo);
XRT_API void xworkArtifactChunkInit(xwork_artifact_chunk* pChunk);
XRT_API void xworkArtifactChunkUnit(xwork_artifact_chunk* pChunk);
XRT_API size_t xworkRuntimeArtifactCount(xwork_runtime* pRuntime);
XRT_API bool xworkRuntimeArtifactGetInfo(xwork_runtime* pRuntime,
    uint64_t uArtifactId, xwork_artifact_info* pInfo);
XRT_API bool xworkRuntimeArtifactAt(xwork_runtime* pRuntime,
    size_t iIndex, xwork_artifact_info* pInfo);
/* A zero byte limit selects XWORK_ARTIFACT_READ_DEFAULT_BYTES; larger than
 * XWORK_ARTIFACT_READ_MAX_BYTES is rejected. Reusing a chunk is supported. */
XRT_API bool xworkRuntimeReadArtifact(xwork_runtime* pRuntime,
    uint64_t uArtifactId, uint64_t uOffset, size_t iMaxBytes,
    xwork_artifact_chunk* pChunk, xwork_error* pError);

/* Immutable replay page over the runtime event/audit ledger. Events and every
 * referenced string remain valid until the snapshot is released. A true
 * history-loss flag means the requested cursor predates retained history or an
 * event could not be retained. A zero limit selects 256 records; the public
 * XWORK_EVENT_SNAPSHOT_MAX_RECORDS ceiling always applies. */
XRT_API xwork_event_snapshot* xworkRuntimeEventSnapshot(xwork_runtime* pRuntime,
    uint64_t uAfterEventId, size_t iLimit, xwork_error* pError);
XRT_API xwork_event_snapshot* xworkEventSnapshotRef(xwork_event_snapshot* pSnapshot);
XRT_API void xworkEventSnapshotRelease(xwork_event_snapshot* pSnapshot);
XRT_API size_t xworkEventSnapshotCount(const xwork_event_snapshot* pSnapshot);
XRT_API bool xworkEventSnapshotAt(const xwork_event_snapshot* pSnapshot,
    size_t iIndex, xwork_event* pEvent);
XRT_API uint64_t xworkEventSnapshotNextCursor(const xwork_event_snapshot* pSnapshot);
XRT_API uint64_t xworkEventSnapshotLatestId(const xwork_event_snapshot* pSnapshot);
XRT_API uint64_t xworkEventSnapshotDroppedCount(const xwork_event_snapshot* pSnapshot);
XRT_API bool xworkEventSnapshotHistoryLost(const xwork_event_snapshot* pSnapshot);

/* Immutable tool-catalog generations. Snapshot functions return one owned
 * reference. ToolAt strings remain valid until that snapshot is released.
 * Runtime mutation prepares a complete replacement and publishes it once;
 * existing agent generations change only through explicit adoption. */
XRT_API xwork_tool_catalog* xworkToolCatalogRef(xwork_tool_catalog* pCatalog);
XRT_API void xworkToolCatalogRelease(xwork_tool_catalog* pCatalog);
XRT_API uint64_t xworkToolCatalogGeneration(const xwork_tool_catalog* pCatalog);
XRT_API size_t xworkToolCatalogCount(const xwork_tool_catalog* pCatalog);
XRT_API bool xworkToolCatalogToolAt(
    const xwork_tool_catalog* pCatalog, size_t iIndex, xwork_tool_info* pInfo);
XRT_API xwork_tool_catalog* xworkRuntimeToolCatalogSnapshot(xwork_runtime* pRuntime);
XRT_API bool xworkRuntimeRegisterTool(
    xwork_runtime* pRuntime, const xwork_tool_definition* pDefinition,
    xwork_error* pError);
XRT_API bool xworkRuntimeUnregisterTool(
    xwork_runtime* pRuntime, const char* sName, xwork_error* pError);
XRT_API bool xworkRuntimeReplaceToolsBySource(
    xwork_runtime* pRuntime, const char* sSource,
    const xwork_tool_definition* pDefinitions, size_t iCount,
    size_t* piRemoved, xwork_error* pError);
XRT_API void xworkToolConcurrencyConfigInit(xwork_tool_concurrency_config* pConfig);
XRT_API bool xworkRuntimeConfigureToolConcurrency(xwork_runtime* pRuntime,
    const xwork_tool_concurrency_config* pConfig, xwork_error* pError);

XRT_API void xworkAgentDefinitionConfigInit(xwork_agent_definition_config* pConfig);
XRT_API xwork_agent_definition* xworkAgentDefinitionCreate(
    const xwork_agent_definition_config* pConfig, xwork_error* pError);
XRT_API xwork_agent_definition* xworkAgentDefinitionRef(xwork_agent_definition* pDefinition);
XRT_API void xworkAgentDefinitionRelease(xwork_agent_definition* pDefinition);

XRT_API void xworkAgentOptionsInit(xwork_agent_options* pOptions);
XRT_API xwork_agent* xworkAgentCreateWithRuntime(
    xwork_runtime* pRuntime,
    xwork_agent_definition* pDefinition,
    const xwork_agent_options* pOptions,
    xwork_error* pError
);

XRT_API void xworkRunConfigInit(xwork_run_config* pConfig);
XRT_API void xworkRunInfoInit(xwork_run_info* pInfo);
XRT_API xwork_run* xworkRunCreate(
    xwork_agent* pAgent, const xwork_run_config* pConfig, xwork_error* pError);
/* Appends an owned USER message with TEXT followed by bounded IMAGE data
 * parts. sPrompt must match the text part and is used for the event stream;
 * it may be empty for an image-only message. Caller storage is borrowed only
 * for this call. */
XRT_API xwork_run* xworkRunCreateWithUserMessage(xwork_agent* pAgent,
    const xwork_run_config* pConfig, const xllm_message* pUserMessage,
    xwork_error* pError);
XRT_API bool xworkRunStart(xwork_run* pRun, xwork_error* pError);
XRT_API xwork_result xworkRunWait(
    xwork_run* pRun, uint64_t uDeadline,
    xwork_run_result* pResult, xwork_error* pError);
/* Idempotently request cooperative cancellation. Wait, Cancel, and GetInfo
 * may run concurrently while the caller keeps the run alive. The host must
 * serialize Create/Start/Destroy lifecycle operations. */
XRT_API bool xworkRunCancel(xwork_run* pRun);
XRT_API bool xworkRunGetInfo(const xwork_run* pRun, xwork_run_info* pInfo);
XRT_API void xworkRunDestroy(xwork_run* pRun);
XRT_API xwork_tool_catalog* xworkAgentToolCatalogSnapshot(xwork_agent* pAgent);
/* Replace the complete agent catalog with the runtime's current generation.
 * Agent-private overlays must be reapplied explicitly after adoption. */
XRT_API bool xworkAgentAdoptRuntimeToolCatalog(xwork_agent* pAgent, xwork_error* pError);
XRT_API bool xworkAgentReplaceToolsBySource(
    xwork_agent* pAgent, const char* sSource,
    const xwork_tool_definition* pDefinitions, size_t iCount,
    size_t* piRemoved, xwork_error* pError);
/* Legacy agent-local mutation/enumeration adapters. Mutation is failure-atomic
 * and rejected during a run. ToolCount/ToolAt return a borrowed view for the
 * owning thread; cross-thread readers use xworkAgentToolCatalogSnapshot. */
/* Configure before running this owned agent. Children inherit these callbacks.
 * Acquire is called before exec/spawn; release follows synchronous process-tree
 * cleanup or managed-task release. Completed task records can retain native
 * process resources: release them explicitly. Callbacks own no xwork memory,
 * are paired, thread-safe, and their code/data must outlive the agent/tasks. */
typedef bool (*xwork_process_owner_acquire_fn)(void* User,void** Lease);
typedef void (*xwork_process_owner_release_fn)(void* User,void* Lease);
XRT_API bool xworkAgentSetProcessOwner(xwork_agent* Agent,
    xwork_process_owner_acquire_fn Acquire,xwork_process_owner_release_fn Release,
    void* User,xwork_error* Error);
XRT_API bool xworkAgentRegisterTool(xwork_agent* pAgent, const xwork_tool_definition* pDefinition, xwork_error* pError);
XRT_API bool xworkAgentConfigureToolConcurrency(xwork_agent* pAgent,
    const xwork_tool_concurrency_config* pConfig, xwork_error* pError);
XRT_API bool xworkAgentUnregisterTool(xwork_agent* pAgent, const char* sName, xwork_error* pError);
XRT_API bool xworkAgentUnregisterToolsBySource(
    xwork_agent* pAgent,
    const char* sSource,
    size_t* piRemoved,
    xwork_error* pError
);
XRT_API size_t xworkAgentToolCount(const xwork_agent* pAgent);
XRT_API bool xworkAgentToolAt(const xwork_agent* pAgent, size_t iIndex, xwork_tool_info* pInfo);
XRT_API uint64_t xworkAgentToolRegistryGeneration(const xwork_agent* pAgent);

/* Build an immutable schedule without executing tools or asking for approval.
 * Permission resource discovery runs once and is retained by the plan.
 * Sequence is the original model order. Equal waves may run concurrently;
 * increasing waves are deterministic barriers. */
XRT_API xwork_tool_batch_plan* xworkAgentPlanToolBatch(xwork_agent* pAgent,
    const xllm_tool_call* pCalls, size_t iCallCount,
    const xllm_executor_ctx* pContext, xwork_error* pError);
XRT_API xwork_tool_batch_plan* xworkToolBatchPlanRef(xwork_tool_batch_plan* pPlan);
XRT_API void xworkToolBatchPlanRelease(xwork_tool_batch_plan* pPlan);
XRT_API void xworkToolBatchItemInfoInit(xwork_tool_batch_item_info* pInfo);
XRT_API size_t xworkToolBatchPlanCount(const xwork_tool_batch_plan* pPlan);
XRT_API uint32_t xworkToolBatchPlanWaveCount(const xwork_tool_batch_plan* pPlan);
XRT_API bool xworkToolBatchPlanItemAt(const xwork_tool_batch_plan* pPlan,
    size_t iIndex, xwork_tool_batch_item_info* pInfo);

/* Compatibility MCP stdio client (2025-11-25 by default). The client owns the subprocess and discovered proxy
 * definitions. It must outlive every agent whose registry refers to those
 * proxies and every retained catalog snapshot containing them. Transport
 * messages use newline-delimited UTF-8 JSON-RPC. New hosts should prefer the
 * runtime-owned server APIs below for cold registration and lazy schemas. */
typedef struct xwork_mcp_stdio_config {
    const char* sServerName;
    const char* sProgram;
    const char* const* psArguments;
    size_t iArgumentCount;
    const char* sWorkingDirectory;
    const char* sProtocolVersion;
    uint32_t uStartupTimeoutMs;
    uint32_t uRequestTimeoutMs;
    size_t iMaxMessageBytes;
    size_t iMaxTools;
    xwork_tool_effects uDefaultToolEffects;
    bool bTrustReadOnlyAnnotations;
    xcancel* pCancel;
    uint64_t uDeadline;
} xwork_mcp_stdio_config;

typedef struct xwork_mcp_info {
    const char* sServerName;
    const char* sProtocolVersion;
    const char* sToolSource;
    size_t iToolCount;
    uint64_t uRequestsCompleted;
    bool bConnected;
    bool bServerSupportsToolListChanges;
} xwork_mcp_info;

typedef enum xwork_mcp_server_state {
    XWORK_MCP_SERVER_DISCONNECTED = 0,
    XWORK_MCP_SERVER_READY,
    XWORK_MCP_SERVER_FAILED,
    XWORK_MCP_SERVER_DISABLED
} xwork_mcp_server_state;

typedef struct xwork_mcp_environment {
    const char* sName;
    const char* sValue; /* NULL removes the inherited variable. */
} xwork_mcp_environment;

typedef enum xwork_mcp_transport {
    XWORK_MCP_TRANSPORT_STDIO = 0,
    XWORK_MCP_TRANSPORT_STREAMABLE_HTTP = 1
} xwork_mcp_transport;

typedef struct xwork_mcp_http_header {
    const char* sName;
    const char* sValue;
} xwork_mcp_http_header;

/* Runtime-owned MCP server definition. Init selects the current 2026-07-28
 * codec over stdio; callers may select Streamable HTTP or explicitly select
 * 2025-11-25 for legacy servers.
 * Registration copies every string,
 * argv/environment entry, and allow/deny list but does not start the server.
 * The first server-specific search, load, refresh, or call connects it. */
typedef struct xwork_mcp_server_config {
    uint32_t uSize;
    uint32_t uAbiVersion;
    const char* sServerId;
    const char* sSummary;
    const char* sProgram;
    const char* const* psArguments;
    size_t iArgumentCount;
    const char* sWorkingDirectory;
    const xwork_mcp_environment* pEnvironment;
    size_t iEnvironmentCount;
    bool bInheritEnvironment;
    const char* sProtocolVersion;
    uint32_t uStartupTimeoutMs;
    uint32_t uRequestTimeoutMs;
    size_t iMaxMessageBytes;
    size_t iMaxTools;
    xwork_tool_effects uDefaultToolEffects;
    bool bTrustReadOnlyAnnotations;
    const char* const* psAllowedTools;
    size_t iAllowedToolCount;
    const char* const* psDeniedTools;
    size_t iDeniedToolCount;
    bool bEnabled;
    bool bAutoReconnect;
    xcancel* pCancel;       /* borrowed; optional process-wide ceiling */
    uint64_t uDeadline;    /* absolute process-wide ceiling; zero means none */
    xwork_mcp_transport eTransport;
    const char* sEndpoint; /* absolute HTTPS URL, or loopback HTTP URL */
    const xwork_mcp_http_header* pHttpHeaders;
    size_t iHttpHeaderCount;
} xwork_mcp_server_config;

/* Borrowed diagnostic view. Strings remain valid until the runtime is
 * destroyed; scalar fields are copied under the runtime/client locks. */
typedef struct xwork_mcp_server_info {
    uint32_t uSize;
    uint32_t uAbiVersion;
    const char* sServerId;
    const char* sSummary;
    const char* sProtocolVersion;
    const char* sToolSource;
    xwork_mcp_server_state eState;
    uint64_t uSchemaGeneration;
    uint64_t uSchemaExpiresUs; /* monotonic; XLLM_DEADLINE_NEVER for legacy */
    size_t iDiscoveredToolCount;
    uint64_t uRequestsCompleted;
    bool bEnabled;
    bool bConnected;
    bool bToolsDiscovered;
    bool bServerSupportsToolListChanges;
    xwork_mcp_transport eTransport;
    const char* sEndpoint;
} xwork_mcp_server_info;

XRT_API void xworkMcpStdioConfigInit(xwork_mcp_stdio_config* pConfig);
XRT_API xwork_mcp_client* xworkMcpClientCreate(const xwork_mcp_stdio_config* pConfig, xwork_error* pError);
XRT_API bool xworkMcpClientConnect(xwork_mcp_client* pClient, xwork_error* pError);
XRT_API bool xworkMcpClientRefreshTools(xwork_mcp_client* pClient, xwork_agent* pAgent, xwork_error* pError);
XRT_API xwork_result xworkMcpClientCallTool(
    xwork_mcp_client* pClient,
    const char* sRemoteToolName,
    const char* sArgumentsJson,
    xcancel* pCancel,
    uint64_t uDeadline,
    xwork_tool_output* pOutput,
    xwork_error* pError
);
XRT_API bool xworkMcpClientGetInfo(const xwork_mcp_client* pClient, xwork_mcp_info* pInfo);
XRT_API void xworkMcpClientDestroy(xwork_mcp_client* pClient);
/* Restrict lazy discovery and calls to this immutable run's effective servers.
 * An empty list disables MCP. Subagents inherit the parent's restriction. */
XRT_API bool xworkAgentSetMcpServers(xwork_agent* pAgent,
    const char* const* psServerIds, size_t iCount, xwork_error* pError);
/* Host-owned credentials are resolved for every HTTP request. Returning an
 * empty header leaves ordinary configured headers unchanged. A 401 may request
 * one refresh before a single replay; transport errors never replay a call. */
typedef bool (*xwork_mcp_authorization_fn)(void* pData,
    const char* sServerId, const char* sEndpoint, bool bRefresh,
    xcancel* pCancel, uint64_t uDeadline, char* sHeader, size_t iCapacity,
    xwork_error* pError);
XRT_API bool xworkRuntimeSetMcpAuthorizationProvider(xwork_runtime* pRuntime,
    xwork_mcp_authorization_fn OnAuthorize, void* pData);

XRT_API void xworkMcpServerConfigInit(xwork_mcp_server_config* pConfig);
XRT_API void xworkMcpServerInfoInit(xwork_mcp_server_info* pInfo);
XRT_API bool xworkRuntimeRegisterMcpServer(xwork_runtime* pRuntime,
    const xwork_mcp_server_config* pConfig, xwork_error* pError);
/* Atomically replaces every registered server owned by sSource. Unchanged
 * definitions retain their live connection and schema cache. The runtime
 * deep-copies source and configuration data. A zero-count batch removes the
 * source. Retired clients remain fail-closed until runtime destruction so
 * tool catalogs that still reference their callbacks cannot dangle.
 * piReplaced receives the number of previous source definitions retired. */
XRT_API bool xworkRuntimeReplaceMcpServersBySource(xwork_runtime* pRuntime,
    const char* sSource, const xwork_mcp_server_config* pConfigs,
    size_t iConfigCount, size_t* piReplaced, xwork_error* pError);
XRT_API bool xworkRuntimeSetMcpServerEnabled(xwork_runtime* pRuntime,
    const char* sServerId, bool bEnabled, xwork_error* pError);
/* Closes the current transport without changing the configured enabled bit.
 * The next server-specific operation reconnects according to normal policy. */
XRT_API bool xworkRuntimeDisconnectMcpServer(xwork_runtime* pRuntime,
    const char* sServerId, xwork_error* pError);
XRT_API bool xworkRuntimeUnregisterMcpServer(xwork_runtime* pRuntime,
    const char* sServerId, xwork_error* pError);
XRT_API bool xworkRuntimeMcpServerGetInfo(xwork_runtime* pRuntime,
    const char* sServerId, xwork_mcp_server_info* pInfo);
XRT_API size_t xworkRuntimeMcpServerCount(xwork_runtime* pRuntime);
XRT_API bool xworkRuntimeRefreshMcpServer(xwork_runtime* pRuntime,
    const char* sServerId, xcancel* pCancel, uint64_t uDeadline,
    xwork_error* pError);
/* Adds only the compact discovery pair. Remote schemas enter the agent
 * catalog individually through load or the explicit host API. */
/* Bounded context-only methods; result JSON is caller-owned (xrtFree).
 * Context operations never issue tools/call or replay a failed POST. */
XRT_API bool xworkRuntimeMcpRead(xwork_runtime* Runtime,const char* Server,const char* Method,
    const char* Params,xcancel* Cancel,uint64_t Deadline,char** Result,xwork_error* Error);
XRT_API char* xworkRuntimeMcpDescribe(xwork_runtime* Runtime,const char* Server,xwork_error* Error);
XRT_API bool xworkAgentRegisterMcpDiscoveryTools(
    xwork_agent* pAgent, xwork_error* pError);
XRT_API bool xworkAgentLoadMcpTool(xwork_agent* pAgent,
    const char* sServerId, const char* sRemoteToolName,
    xwork_error* pError);
XRT_API bool xworkAgentCancel(xwork_agent* pAgent);
XRT_API const char* xworkAgentWorkspaceRoot(const xwork_agent* pAgent);
/* Host-selected filesystem scope. Defaults to workspace and explicit mounts.
 * Allowing external paths removes the workspace boundary for ordinary native
 * paths, including file tools and process cwd; OS permissions and tool effect
 * policies still apply. Configure before executor binding or the first run.
 * Subagents inherit this scope, subject to their own tool effect ceilings. */
/* A live, shared file scope. Attach before executor binding/first run. Child
 * agents retain the same scope, including background descendants. Set is
 * thread-safe and affects subsequent path checks; already executing operations
 * retain their admitted resources. Mounted directories remain confined. */
typedef struct xwork_file_scope xwork_file_scope;
XRT_API xwork_file_scope* xworkFileScopeCreate(bool bAllowExternal);
XRT_API xwork_file_scope* xworkFileScopeRef(xwork_file_scope* pScope);
XRT_API void xworkFileScopeRelease(xwork_file_scope* pScope);
XRT_API void xworkFileScopeSetExternal(xwork_file_scope* pScope, bool bAllow);
XRT_API bool xworkFileScopeExternal(const xwork_file_scope* pScope);
XRT_API bool xworkAgentSetFileScope(xwork_agent* pAgent, xwork_file_scope* pScope,
    xwork_error* pError);

XRT_API bool xworkAgentAllowExternalPaths(xwork_agent* pAgent, bool bAllow,
    xwork_error* pError);
/* Mount a host-selected, existing directory for the ordinary file tools.
 * Paths use @alias/relative-path and always stay within that mount. Ordinary
 * external paths require the host opt-in above. Mounts are copied,
 * inherited by subagents, and immutable after binding.
 * This is a tool path boundary, not an OS process sandbox. Max eight mounts. */
XRT_API bool xworkAgentMountDirectory(xwork_agent* pAgent, const char* sAlias,
    const char* sDirectory, xwork_error* pError);

/* True when any path component is an internal control directory (.git,
 * .xcode), compared case-insensitively across both separators. Hosts use it
 * inside permission callbacks to keep internal state off-limits; the
 * readonly subagent enforces it by default. */
XRT_API bool xworkPathIsProtected(const char* sPath);

XRT_API xwork_result xworkAgentRun(xwork_agent* pAgent, const char* sPrompt, xwork_run_result* pResult, xwork_error* pError);
/* Declare an externally driven run window: while open, registry mutation and
 * every other run entry (built-in loop, compact, another window) are rejected
 * — the same rule the built-in loop enforces on itself. Pair with RunEnd;
 * hosts driving xllmSessionRunWithTools over an xwork executor wrap the call
 * in this pair so the tool registry stays stable for the whole run. */
XRT_API bool xworkAgentRunBegin(xwork_agent* pAgent, xwork_error* pError);
XRT_API void xworkAgentRunEnd(xwork_agent* pAgent);

/* ------------------------------------------------------------------ */
/* Unified task table: notices and the model-clock watchdog.           */
/*                                                                     */
/* Task completion is pushed at turn boundaries: TakeTaskNotices       */
/* returns each finished, not-yet-consumed task (with the model's      */
/* notify message, if any) and marks it consumed; the host injects     */
/* the notices into the session as synthetic entries. The watchdog     */
/* digest reports model-scheduled reminders (spawn remind_after_ms)    */
/* and a one-shot uncollected-notice nudge — the harness executes the  */
/* clocks the model set; it never invents its own schedule.            */
/* ------------------------------------------------------------------ */

typedef enum xwork_task_kind {
    XWORK_TASK_PROCESS = 0,
    XWORK_TASK_AGENT,
    XWORK_TASK_SCHEDULED
} xwork_task_kind;

typedef enum xwork_task_state {
    XWORK_TASK_PENDING = 0,
    XWORK_TASK_RUNNING,
    XWORK_TASK_SUCCEEDED,
    XWORK_TASK_FAILED,
    XWORK_TASK_CANCELLED,
    XWORK_TASK_TIMED_OUT,
    XWORK_TASK_LOST
} xwork_task_state;

typedef enum xwork_task_wait_mode {
    XWORK_TASK_WAIT_ANY = 0,
    XWORK_TASK_WAIT_ALL
} xwork_task_wait_mode;

typedef enum xwork_task_event_kind {
    XWORK_TASK_EVENT_CREATED = 0,
    XWORK_TASK_EVENT_STATE_CHANGED,
    XWORK_TASK_EVENT_CANCEL_REQUESTED,
    XWORK_TASK_EVENT_RESTORED,
    XWORK_TASK_EVENT_NOTICE_TAKEN
} xwork_task_event_kind;

typedef struct xwork_task_event {
    uint64_t uTaskId;
    uint64_t uRevision;
    xwork_task_event_kind eKind;
    xwork_task_state eState;
    int64_t iTimestampUs;
} xwork_task_event;

/* Immutable task metadata. String members returned by a snapshot are
 * borrowed from that snapshot and remain valid until it is released. Unix
 * timestamps are epoch microseconds; zero means the transition has not
 * happened. A restored task keeps its stable id and metadata, but live OS
 * handles are deliberately never reconstructed. */
typedef struct xwork_task_info {
    uint32_t uSize;
    uint32_t uAbiVersion;
    uint64_t uTaskId;
    uint64_t uOwnerAgentId;
    uint64_t uOwnerRunId;
    uint64_t uParentTaskId;
    xwork_task_kind eKind;
    xwork_task_state eState;
    uint64_t uRevision;
    int64_t iCreatedAtUs;
    int64_t iStartedAtUs;
    int64_t iEndedAtUs;
    int64_t iScheduledAtUs;
    bool bExitStatusValid;
    int32_t iExitCode;
    int32_t iExitSignal;
    int32_t iStopReason;
    bool bNoticeTaken;
    const char* sOwnerSession;
    const char* sLabel;
    const char* sNotify;
    const char* sScheduleId;
    uint64_t uScheduleGeneration;
} xwork_task_info;

typedef struct xwork_task_output {
    unsigned char* pStdout;
    size_t iStdoutSize;
    uint64_t uStdoutStart;
    uint64_t uStdoutNext;
    bool bStdoutDropped;
    unsigned char* pStderr;
    size_t iStderrSize;
    uint64_t uStderrStart;
    uint64_t uStderrNext;
    bool bStderrDropped;
    char* sResult;
    size_t iResultSize;
    uint64_t uResultStart;
    uint64_t uResultNext;
    bool bResultDropped;
    bool bComplete;
} xwork_task_output;

typedef struct xwork_scheduled_task_config {
    uint32_t uSize;
    uint32_t uAbiVersion;
    uint64_t uOwnerAgentId;
    uint64_t uOwnerRunId;
    uint64_t uParentTaskId;
    const char* sOwnerSession;
    const char* sLabel;
    const char* sNotify;
    int64_t iScheduledAtUs;
    const char* sScheduleId;       /* copied; optional for manual tasks */
    uint64_t uScheduleGeneration;
} xwork_scheduled_task_config;

typedef struct xwork_task_notice {
    uint64_t uTaskId;
    xwork_task_kind eKind;
    int32_t iExitCode;
    bool bExitedCleanly;
    const char* sNotify;    /* borrowed from the task entry */
    const char* sPreview;   /* borrowed command preview */
} xwork_task_notice;

typedef struct xwork_watchdog_digest {
    bool bShouldWake;
    uint64_t uNextWakeMs;         /* 0 = no timer needed */
    size_t iRunningTasks;
    size_t iStalledTasks;         /* past their remind_after_ms */
    size_t iUncollectedNotices;
} xwork_watchdog_digest;

/* Call from the thread that owns the agent (the same thread that drives
 * runs); the notice payloads borrow entry storage and are not safe to read
 * across a concurrent registry or task mutation. */
XRT_API size_t xworkAgentTakeTaskNotices(xwork_agent* pAgent,
    xwork_task_notice* pNotices, size_t iCapacity);
XRT_API bool xworkTaskWatchdog(xwork_agent* pAgent, xwork_watchdog_digest* pDigest);

/* Runtime task API. Snapshots isolate readers from concurrent state changes;
 * output reads use caller-owned cursors and never consume the model tool's
 * output cursor. Wait accepts an absolute monotonic xrt deadline. */
XRT_API void xworkTaskInfoInit(xwork_task_info* pInfo);
XRT_API void xworkTaskOutputInit(xwork_task_output* pOutput);
XRT_API void xworkTaskOutputUnit(xwork_task_output* pOutput);
XRT_API void xworkScheduledTaskConfigInit(xwork_scheduled_task_config* pConfig);
XRT_API xwork_task_snapshot* xworkRuntimeTaskSnapshot(
    xwork_runtime* pRuntime, uint64_t uOwnerAgentId, xwork_error* pError);
XRT_API xwork_task_snapshot* xworkTaskSnapshotRef(xwork_task_snapshot* pSnapshot);
XRT_API void xworkTaskSnapshotRelease(xwork_task_snapshot* pSnapshot);
XRT_API size_t xworkTaskSnapshotCount(const xwork_task_snapshot* pSnapshot);
XRT_API bool xworkTaskSnapshotTaskAt(const xwork_task_snapshot* pSnapshot,
    size_t iIndex, xwork_task_info* pInfo);
XRT_API bool xworkTaskSnapshotFind(const xwork_task_snapshot* pSnapshot,
    uint64_t uTaskId, xwork_task_info* pInfo);
/* Captured under the same task lock as its metadata; remains unchanged for
 * this snapshot. True means a stop request was accepted, not that work has
 * ended. Null snapshots, zero and absent ids return false. This additive
 * query leaves the ABI of xwork_task_info unchanged. */
XRT_API bool xworkTaskSnapshotStopRequested(const xwork_task_snapshot* pSnapshot,
    uint64_t uTaskId);
XRT_API bool xworkRuntimeReadTaskOutput(xwork_runtime* pRuntime, uint64_t uTaskId,
    uint64_t uStdoutOffset, uint64_t uStderrOffset, uint64_t uResultOffset,
    size_t iMaxBytes, xwork_task_output* pOutput, xwork_error* pError);
XRT_API bool xworkRuntimeReadTaskEvents(xwork_runtime* pRuntime, uint64_t uTaskId,
    uint64_t uAfterRevision, xwork_task_event* pEvents, size_t iCapacity,
    size_t* piCount, uint64_t* puNextRevision, bool* pbHistoryLost,
    xwork_error* pError);
XRT_API xwork_result xworkRuntimeWaitTasks(xwork_runtime* pRuntime,
    const uint64_t* puTaskIds, size_t iTaskCount, xwork_task_wait_mode eMode,
    uint64_t uDeadline, xcancel* pCancel, uint64_t* puCompletedTaskId,
    xwork_error* pError);
XRT_API bool xworkRuntimeCancelTask(xwork_runtime* pRuntime,
    uint64_t uTaskId, xwork_error* pError);
XRT_API bool xworkRuntimeReleaseTask(xwork_runtime* pRuntime,
    uint64_t uTaskId, xwork_error* pError);
XRT_API bool xworkRuntimeCreateScheduledTask(xwork_runtime* pRuntime,
    const xwork_scheduled_task_config* pConfig, uint64_t* puTaskId,
    xwork_error* pError);
XRT_API bool xworkRuntimeStartScheduledTask(xwork_runtime* pRuntime,
    uint64_t uTaskId, xwork_error* pError);
XRT_API bool xworkRuntimeFinishScheduledTask(xwork_runtime* pRuntime,
    uint64_t uTaskId, xwork_result eResult, const char* sResult,
    xwork_error* pError);
XRT_API bool xworkRuntimeRestoreTask(xwork_runtime* pRuntime,
    const xwork_task_info* pInfo, uint64_t* puTaskId, xwork_error* pError);

/* ------------------------------------------------------------------ */
/* Runtime scheduler.                                                  */
/*                                                                     */
/* The host drives the scheduler with explicit Unix microsecond time.  */
/* Claiming one due occurrence creates one RUNNING scheduled task in   */
/* the unified task table. The host runs its ordinary agent workflow   */
/* and completes that task with xworkRuntimeFinishScheduledTask().      */
/* Definitions are cold, copied, bounded, and recoverable.             */
/* ------------------------------------------------------------------ */

typedef enum xwork_schedule_frequency {
    XWORK_SCHEDULE_ONCE = 0,
    XWORK_SCHEDULE_MINUTELY,
    XWORK_SCHEDULE_HOURLY,
    XWORK_SCHEDULE_DAILY,
    XWORK_SCHEDULE_WEEKLY
} xwork_schedule_frequency;

typedef enum xwork_schedule_timezone {
    XWORK_SCHEDULE_TIMEZONE_UTC = 0,
    XWORK_SCHEDULE_TIMEZONE_FIXED_OFFSET,
    XWORK_SCHEDULE_TIMEZONE_SYSTEM_LOCAL
} xwork_schedule_timezone;

typedef enum xwork_schedule_fold_policy {
    XWORK_SCHEDULE_FOLD_EARLIER = 0,
    XWORK_SCHEDULE_FOLD_LATER
} xwork_schedule_fold_policy;

typedef enum xwork_schedule_misfire_policy {
    XWORK_SCHEDULE_MISFIRE_SKIP = 0,
    XWORK_SCHEDULE_MISFIRE_RUN_ONCE,
    XWORK_SCHEDULE_MISFIRE_CATCH_UP
} xwork_schedule_misfire_policy;

typedef enum xwork_schedule_overlap_policy {
    XWORK_SCHEDULE_OVERLAP_SKIP = 0,
    XWORK_SCHEDULE_OVERLAP_QUEUE_ONE
} xwork_schedule_overlap_policy;

/* DAILY and WEEKLY keep the local wall-clock fields of iStartAtUs in the
 * selected timezone. MINUTELY and HOURLY are absolute elapsed intervals.
 * Weekly mask bit 0 is Sunday through bit 6 Saturday; zero selects the
 * start timestamp's weekday. A system-local DST gap skips that occurrence;
 * folds use eFoldPolicy. */
typedef struct xwork_schedule_config {
    uint32_t uSize;
    uint32_t uAbiVersion;
    const char* sScheduleId;
    const char* sLabel;
    const char* sNotify;
    const char* sOwnerSession;
    const char* sAgentProfile;
    const char* sInput;
    uint64_t uOwnerAgentId;
    uint64_t uOwnerRunId;
    uint64_t uParentTaskId;
    xwork_schedule_frequency eFrequency;
    uint32_t uInterval;             /* 0 selects 1. */
    int64_t iStartAtUs;
    uint8_t uWeekdayMask;
    xwork_schedule_timezone eTimezone;
    int32_t iUtcOffsetSeconds;
    xwork_schedule_fold_policy eFoldPolicy;
    xwork_schedule_misfire_policy eMisfirePolicy;
    uint32_t uMisfireGraceSeconds;  /* 0 selects 60 seconds. */
    uint32_t uMaxCatchUp;           /* 0 selects 1; hard maximum 64. */
    xwork_schedule_overlap_policy eOverlapPolicy;
    uint32_t uMaxConcurrentRuns;    /* 0 selects 1; hard maximum 16. */
    bool bEnabled;
} xwork_schedule_config;

/* Borrowed diagnostic/persistence view. Every string remains valid until
 * the runtime is destroyed. Restore copies the complete record. */
typedef struct xwork_schedule_info {
    uint32_t uSize;
    uint32_t uAbiVersion;
    xwork_schedule_config tConfig;
    uint64_t uGeneration;
    int64_t iNextOccurrenceAtUs;    /* 0 when the recurrence is exhausted. */
    int64_t iLastClaimedAtUs;
    uint64_t uClaimCount;
    uint64_t uMisfireCount;
    size_t iActiveRuns;
} xwork_schedule_info;

/* Borrowed claim strings are backed by stable runtime storage. */
typedef struct xwork_schedule_claim {
    uint32_t uSize;
    uint32_t uAbiVersion;
    uint64_t uTaskId;
    uint64_t uScheduleGeneration;
    int64_t iOccurrenceAtUs;
    const char* sScheduleId;
    const char* sLabel;
    const char* sAgentProfile;
    const char* sInput;
} xwork_schedule_claim;

XRT_API void xworkScheduleConfigInit(xwork_schedule_config* pConfig);
XRT_API void xworkScheduleInfoInit(xwork_schedule_info* pInfo);
XRT_API void xworkScheduleClaimInit(xwork_schedule_claim* pClaim);
XRT_API bool xworkScheduleNextOccurrence(const xwork_schedule_config* pConfig,
    int64_t iAfterExclusiveUs, int64_t* piNextOccurrenceUs,
    bool* pbHasOccurrence, xwork_error* pError);
XRT_API bool xworkRuntimeRegisterSchedule(xwork_runtime* pRuntime,
    const xwork_schedule_config* pConfig, xwork_error* pError);
XRT_API bool xworkRuntimeRestoreSchedule(xwork_runtime* pRuntime,
    const xwork_schedule_info* pInfo, xwork_error* pError);
/* Atomically publishes a new definition under the same ID and resets its
 * recurrence cursor. Active scheduled work must finish first. Previously
 * borrowed definition strings remain valid until runtime destruction. */
XRT_API bool xworkRuntimeReplaceSchedule(xwork_runtime* pRuntime,
    const xwork_schedule_config* pConfig, xwork_error* pError);
/* Reverts an unclaimed replacement by exact generations. This is intended for
 * a host persistence transaction that failed after runtime publication. */
XRT_API bool xworkRuntimeRevertScheduleReplacement(xwork_runtime* pRuntime,
    const char* sScheduleId, uint64_t uCurrentGeneration,
    uint64_t uPreviousGeneration, xwork_error* pError);
XRT_API bool xworkRuntimeSetScheduleEnabled(xwork_runtime* pRuntime,
    const char* sScheduleId, bool bEnabled, xwork_error* pError);
XRT_API bool xworkRuntimeUnregisterSchedule(xwork_runtime* pRuntime,
    const char* sScheduleId, xwork_error* pError);
XRT_API size_t xworkRuntimeScheduleCount(xwork_runtime* pRuntime);
XRT_API bool xworkRuntimeScheduleGetInfo(xwork_runtime* pRuntime,
    const char* sScheduleId, xwork_schedule_info* pInfo);
XRT_API bool xworkRuntimeScheduleAt(xwork_runtime* pRuntime,
    size_t iIndex, xwork_schedule_info* pInfo);
/* Returns true for both a claim and an idle scheduler. pbClaimed tells them
 * apart. piNextWakeAtUs is zero when no time-based wake is required. */
XRT_API bool xworkRuntimeClaimDueSchedule(xwork_runtime* pRuntime, int64_t iNowUs,
    xwork_schedule_claim* pClaim, bool* pbClaimed,
    int64_t* piNextWakeAtUs, xwork_error* pError);
/* Starts a registered schedule explicitly at iNowUs, even when its recurrence
 * is paused or exhausted. This respects the concurrent-run limit but leaves
 * the recurrence cursor and catch-up state unchanged. The returned claim owns
 * a RUNNING task and uses borrowed definition strings, like a due claim. */
XRT_API bool xworkRuntimeTriggerSchedule(xwork_runtime* pRuntime,
    const char* sScheduleId, int64_t iNowUs,
    xwork_schedule_claim* pClaim, xwork_error* pError);

/* ------------------------------------------------------------------ */
/* Subagent delegation: the conditional `agent` tool.                  */
/*                                                                     */
/* A complete roster is published as one immutable catalog generation. */
/* Calls already in flight retain their old generation. Each child gets */
/* a fresh session, shares the process runtime, inherits the parent     */
/* cancel/deadline and permission chain, and may only tighten tools,    */
/* effects, token budgets, depth, and background execution.            */
/* ------------------------------------------------------------------ */

typedef struct xwork_subagent_definition_config {
    uint32_t uSize;
    uint32_t uAbiVersion;
    const char* sName;             /* Stable roster key; copied. */
    const char* sDescription;      /* One-line selection hint; copied. */
    const char* sSystemPrompt;     /* Child identity; copied. */
    const char* const* psTools;    /* NULL = every parent tool within the effect ceiling. */
    size_t iToolCount;
    /* Skill names are copied metadata for the product host. xwork does not
     * load skills; the host must enforce this ceiling when composing the
     * child's prompt and tool catalog. */
    const char* const* psSkills;
    size_t iSkillCount;
    const char* sModel;            /* NULL inherits the parent model. */
    const char* sReasoningEffort;  /* NULL inherits the parent setting. */
    uint64_t uContextWindowTokens; /* 0 inherits; host validates the selected model. */
    uint64_t uMaxInputTokens;      /* 0 inherits; host validates the selected model. */
    uint32_t uMaxTurns;            /* 0 means unlimited. */
    uint32_t uTimeoutMs;           /* 0 means no task lifetime deadline. */
    uint32_t uMaxOutputTokens;     /* 0 inherits the parent session cap. */
    size_t iMaxFinalBytes;         /* 0 selects 64 KiB. */
    xwork_tool_effects uAllowedEffects; /* Init selects READ. */
    uint32_t uMaxDepth;            /* 0 inherits the root definition ceiling. */
    bool bReadOnly;                /* Further clamps effects to READ. */
    bool bAllowBackground;
    bool bAllowDelegation;         /* Child may receive `agent` when depth permits. */
    bool bExplicitTools;
    bool bRequireVerificationAfterWrite;
} xwork_subagent_definition_config;

/* Every pointer below is a borrowed view into a retained catalog. Keep the
 * catalog alive while reading the record or its arrays. */
typedef struct xwork_subagent_definition_info {
    uint32_t uSize;
    uint32_t uAbiVersion;
    uint64_t uCatalogGeneration;
    const char* sName;             /* Borrowed from the retained catalog. */
    const char* sDescription;
    const char* sSystemPrompt;
    const char* const* psTools;
    size_t iToolCount;
    const char* const* psSkills;
    size_t iSkillCount;
    const char* sModel;
    const char* sReasoningEffort;
    uint64_t uContextWindowTokens;
    uint64_t uMaxInputTokens;
    uint32_t uMaxTurns;
    uint32_t uTimeoutMs;
    uint32_t uMaxOutputTokens;
    size_t iMaxFinalBytes;
    xwork_tool_effects uAllowedEffects;
    uint32_t uMaxDepth;
    bool bReadOnly;
    bool bAllowBackground;
    bool bAllowDelegation;
} xwork_subagent_definition_info;

XRT_API void xworkSubagentDefinitionConfigInit(
    xwork_subagent_definition_config* pConfig);
XRT_API void xworkSubagentDefinitionInfoInit(xwork_subagent_definition_info* pInfo);
/* Returns another owned reference; release it exactly once. */
XRT_API xwork_subagent_catalog* xworkSubagentCatalogRef(
    xwork_subagent_catalog* pCatalog);
XRT_API void xworkSubagentCatalogRelease(xwork_subagent_catalog* pCatalog);
XRT_API uint64_t xworkSubagentCatalogGeneration(
    const xwork_subagent_catalog* pCatalog);
XRT_API size_t xworkSubagentCatalogCount(const xwork_subagent_catalog* pCatalog);
XRT_API bool xworkSubagentCatalogDefinitionAt(
    const xwork_subagent_catalog* pCatalog, size_t iIndex,
    xwork_subagent_definition_info* pInfo);
/* Returns an owned immutable snapshot, or NULL. The caller releases it. */
XRT_API xwork_subagent_catalog* xworkAgentSubagentCatalogSnapshot(
    xwork_agent* pAgent);
/* Replaces the complete roster in one failure-atomic publication. Input
 * strings and arrays are copied before return. An active run pins the current
 * generation and rejects replacement. */
XRT_API bool xworkAgentReplaceSubagentDefinitions(xwork_agent* pAgent,
    const xwork_subagent_definition_config* pDefinitions, size_t iCount,
    xwork_error* pError);

/* Legacy one-at-a-time adapter. New hosts should publish a complete versioned
 * roster through xworkAgentReplaceSubagentDefinitions. */
typedef struct xwork_subagent_type {
    const char* sName;            /* roster key, e.g. "probe" */
    const char* sDescription;     /* one line: when to choose me */
    const char* sSystemPrompt;    /* identity injected into the child */
    const char* const* psTools;   /* tool-name whitelist; NULL = all parent tools */
    size_t iToolCount;
    const char* sModel;           /* optional lighter model override */
    uint32_t uMaxTurns;           /* 0 = 8 */
    uint32_t uTimeoutMs;          /* 0 = 120000 */
    uint32_t uMaxOutputTokens;    /* 0 = keep parent config */
    size_t iMaxFinalBytes;        /* 0 = 64 KiB */
    bool bReadOnly;               /* clamp approval to READ_ONLY (tighten only) */
} xwork_subagent_type;

/* Register an archetype (deep copy). Legacy types allow background work,
 * default to depth one, and do not allow their child to delegate again. */
XRT_API bool xworkAgentRegisterSubagentType(xwork_agent* pAgent,
    const xwork_subagent_type* pType, xwork_error* pError);
/* Remove one archetype by name (tool persists while others remain). */
XRT_API bool xworkAgentUnregisterSubagentType(xwork_agent* pAgent, const char* sName,
    xwork_error* pError);
XRT_API size_t xworkAgentSubagentTypeCount(const xwork_agent* pAgent);
XRT_API xwork_result xworkAgentRunReadOnlySubagent(
    xwork_agent* pParent,
    const xwork_readonly_subagent_config* pConfig,
    const char* sTask,
    xwork_run_result* pResult,
    xwork_error* pError
);
/* Capture unresolved durable tool calls. The agent must be idle. The returned
 * snapshot is independent of later session/catalog mutation and may be used to
 * render a recovery decision before starting a resume run. */
XRT_API xwork_recovery_snapshot* xworkAgentRecoverySnapshot(
    xwork_agent* pAgent, xwork_error* pError);
XRT_API xwork_recovery_snapshot* xworkRecoverySnapshotRef(
    xwork_recovery_snapshot* pSnapshot);
XRT_API void xworkRecoverySnapshotRelease(xwork_recovery_snapshot* pSnapshot);
XRT_API size_t xworkRecoverySnapshotCount(const xwork_recovery_snapshot* pSnapshot);
XRT_API bool xworkRecoverySnapshotAt(const xwork_recovery_snapshot* pSnapshot,
    size_t iIndex, xwork_recovery_call_info* pInfo);

/* Resume without appending another user prompt. Model-only interruptions are
 * retried normally. Pending tools are preflighted as one batch before any are
 * executed: DEFAULT automatically retries only known read-only tools; every
 * other call requires an explicit decision. Decisions match stable tool-call
 * IDs, are validated together, and are consumed only for this resume attempt. */
XRT_API xwork_result xworkAgentResumeWithOptions(xwork_agent* pAgent,
    const xwork_resume_options* pOptions,
    xwork_run_result* pResult, xwork_error* pError);
/* Safe-default adapter equivalent to initialized options with no decisions. */
XRT_API xwork_result xworkAgentResume(xwork_agent* pAgent, xwork_run_result* pResult, xwork_error* pError);
/* Force one safe-prefix summary compaction and persist the committed session. */
XRT_API xwork_result xworkAgentCompact(xwork_agent* pAgent, xwork_error* pError);
XRT_API void xworkRunResultUnit(xwork_run_result* pResult);

/* Registers filesystem, transactional edit, synchronous command, and managed process tools. */
XRT_API bool xworkAgentRegisterBuiltinTools(xwork_agent* pAgent, xwork_error* pError);
/* Registers only filesystem inspection tools: read, ls, glob, and grep. */
XRT_API bool xworkAgentRegisterBuiltinReadOnlyTools(xwork_agent* pAgent, xwork_error* pError);

/* ------------------------------------------------------------------ */
/* Executor adapter: expose an agent's tool machinery as xllm's hands.  */
/*                                                                     */
/* The binding serves the xllm_executor contract from the agent's       */
/* registry (list), and its full execution path (execute): permission   */
/* gate, hooks, executor, truncation and artifact spill — the same      */
/* path the built-in loop uses. Hosts driving their own loop (or        */
/* xllmSessionRunWithTools) consume this binding; the built-in          */
/* xworkAgentRun stays available as a convenience.                      */
/* ------------------------------------------------------------------ */

typedef struct xwork_executor_state xwork_executor_state;

XRT_API bool xworkExecutorBind(xllm_executor* pOut, xwork_agent* pAgent, xwork_error* pError);
XRT_API void xworkExecutorUnbind(xllm_executor* pExecutor);

#ifdef __cplusplus
}
#endif

#endif /* selected xwork */

#endif
