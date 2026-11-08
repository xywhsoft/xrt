#ifndef XWORK_INTERNAL_H
#define XWORK_INTERNAL_H
#define XWORK_PERMISSION_WRITER_MAGIC UINT64_C(0x58575045524d5632)
#define XWORK_RESULT_WRITER_MAGIC UINT64_C(0x5857524553555632)

#include <xwork.h>
#include <xhttp-client.h>

#include "../../../xllm/src/internal/xllm_time.h"
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <windows.h>
#if defined(_MSC_VER)
#include <intrin.h>
#endif
#else
#include <dirent.h>
#include <strings.h>
#include <sys/stat.h>
#endif

typedef struct xwork_buf
{
    char *pData;
    size_t iLen;
    size_t iCap;
} xwork_buf;

struct xwork_permission_resource_writer
{
    uint64_t uMagic;
    xwork_permission_resource *pResources;
    size_t iResourceCount;
    size_t iResourceCap;
    size_t iTextBytes;
    bool bFailed;
    xwork_error_code eFailure;
    char sFailure[160];
};

struct xwork_tool_result_writer
{
    uint64_t uMagic;
    xwork_agent *pAgent; /* callback-bound; never owned */
    const xwork_tool_context *pContext;
    const struct xwork_tool_entry *pTool;
    const xllm_tool_call *pCall;
    char *sArtifactPath;
    uint64_t uArtifactId;
    xwork_tool_output *pOutput;
    xwork_buf tText;
    size_t iMaxBytes;
    size_t iImageBytes;
    bool bStatusSet;
    bool bFailed;
    xwork_error_code eFailure;
    char sFailure[160];
};

typedef struct xwork_tool_entry
{
    char *sName;
    char *sDescription;
    char *sParametersJson;
    char *sSource;
    bool bStrict;
    xwork_tool_effects uEffects;
    bool bRetrySafeRead; /* explicit host declaration, independent of effects */
    xwork_tool_execute_fn OnExecute;
    xwork_tool_describe_permissions_fn OnDescribePermissions;
    xwork_tool_execute_v2_fn OnExecuteV2;
    void *pUserData;
    xwork_tool_owner_retain_fn OnOwnerRetain;
    xwork_tool_owner_release_fn OnOwnerRelease;
    bool bOwnerRetained;
    size_t iMaxResultBytes;
    bool bParallelSafe;
    char *sSerialGroup;
} xwork_tool_entry;

struct xwork_tool_catalog
{
    volatile long iRefs;
    uint64_t uGeneration;
    xwork_tool_entry *pTools;
    size_t iToolCount;
};

typedef struct xwork_subagent_entry
{
    char *sName;
    char *sDescription;
    char *sSystemPrompt;
    char **psTools;
    size_t iToolCount;
    char **psSkills;
    size_t iSkillCount;
    char *sModel;
    char *sReasoningEffort;
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
    bool bExplicitTools;
    bool bRequireVerificationAfterWrite;
} xwork_subagent_entry;

struct xwork_subagent_catalog
{
    volatile long iRefs;
    uint64_t uGeneration;
    xwork_subagent_entry *pEntries;
    size_t iEntryCount;
};

typedef struct xwork_delegation_scope
{
    volatile long iRefs;
    xmutex *pLock;
    uint32_t uActive;
    uint32_t uMaxActive;
    uint32_t uMaxDepth;
    volatile uint64_t uNextDelegationId;
} xwork_delegation_scope;

/* Unified task kinds live in xwork.h (xwork_task_kind): one table, one id
 * space, one tool family shared by processes and subagents. */

typedef struct xwork_process_entry
{
    xwork_process_owner_release_fn OnProcessRelease;
    void *pProcessOwnerData, *pProcessOwnerLease;
    bool bProcessOwnerHeld;
    volatile long iRefs;
    volatile long iReady;    /* published only after initialization */
    xwork_runtime *pRuntime; /* borrowed from the owning task table */
    uint64_t uId;
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
    char *sOwnerSession;
    char *sLabel;
    char *sScheduleId;
    uint64_t uScheduleGeneration;
    xwork_task_event arrEvents[16];
    size_t iEventStart;
    size_t iEventCount;
    xprocess *pProcess;
    struct xwork_process_log_capture *LogCapture;
    xthread *ProcessMonitor;
    xwork_process_output *Output;
    char *OutputNotice;
    struct xwork_process_capture *pCapture;
    char *sCommand; /* argv preview for status display */
    bool bStdinClosed;
    /* Model-driven timing and notifications. */
    char *sNotify;           /* message delivered with the completion notice */
    uint64_t uRemindAfterMs; /* model-set soft deadline; 0 = none */
    uint64_t uStartedUs;     /* xllmClockUs() at start */
    uint64_t uExitedUs;      /* first observed exit; 0 while running */
    bool bNoticeTaken;       /* completion notice consumed by the host */
    bool bNudged;            /* uncollected-notice nudge already sent */
    /* Agent-task fields (eKind == XWORK_TASK_AGENT). The delegate thread
     * owns its child objects; the entry owns the cancel token and result. */
    xcancel *pChildCancel;
    xthread *pThread;
    xmutex *pStateLock; /* guards state, result, cursors and flags */
    char *sResult;      /* final report text (locked by pStateLock) */
    uint64_t uResultBaseOffset;
    bool bResultDropped;
    bool bDone;          /* thread finished (locked by pStateLock) */
    bool bSuccess;       /* run result (locked by pStateLock) */
    bool bStopRequested; /* cooperative stop asked */
} xwork_process_entry;

typedef struct xwork_schedule_entry
{
    bool bRegistered;
    xwork_schedule_config tConfig;
    char *sScheduleId;
    char *sLabel;
    char *sNotify;
    char *sOwnerSession;
    char *sAgentProfile;
    char *sInput;
    uint64_t uGeneration;
    int64_t iNextOccurrenceAtUs;
    int64_t iLastClaimedAtUs;
    uint64_t uClaimCount;
    uint64_t uMisfireCount;
    uint64_t arrActiveTaskIds[16];
    size_t iActiveTaskCount;
    uint32_t uCatchUpRemaining;
    bool bCatchingUp;
} xwork_schedule_entry;

typedef struct xwork_artifact_entry
{
    xwork_artifact_info tInfo;
    char *sSource;
    char *sPath;
    char *sMediaType;
    char *sAbsolutePath;
    xwork_artifact_store *pStore;
    char *sStoreRelativePath;
} xwork_artifact_entry;

struct xwork_artifact_store
{
    volatile long iRefs;
    xroot pDirectory;
    char *sDirectory;
};

typedef struct xwork_event_entry
{
    volatile long iRefs;
    xwork_event tEvent;
    size_t iRetainedBytes;
} xwork_event_entry;

typedef struct xwork_process_capture_stream
{
    struct xwork_process_capture *pOwner;
    xprocessstream eStream;
    xthread *pThread;
    xwork_buf tData;
    uint64_t uBaseOffset;
    size_t iLimit;
    bool bDone;
} xwork_process_capture_stream;

typedef struct xwork_process_capture
{
    xprocess *pProcess;
    xmutex *pLock;
    xwork_process_capture_stream tStdout;
    xwork_process_capture_stream tStderr;
} xwork_process_capture;

struct xwork_runtime
{
    volatile long iRefs;
    xmutex *pLock;
    xwork_process_entry **ppTasks; /* stable, individually allocated entries */
    size_t iTaskCount;
    size_t iTaskCap;
    uint64_t uNextTaskId;
    uint64_t uNextAgentId;
    uint64_t uNextRunId;
    volatile uint64_t uNextPermissionRequestId;
    uint32_t uMaxTasks;
    uint32_t uMaxCatalogTools;
    size_t iMaxRetainedTaskBytes;
    volatile uint64_t uNextCatalogGeneration;
    volatile uint64_t uNextSubagentCatalogGeneration;
    xwork_tool_catalog *pToolCatalog;
    /* Entries are stable until runtime destruction. Unregistering a server
     * disables its entry so retained agent tool generations never dangle. */
    xwork_mcp_client **ppMcpServers;
    size_t iMcpServerCount;
    size_t iMcpServerCap;
    xnetengine *pMcpNetEngine;
    xhttpclient *pMcpHttpClient;
    xwork_mcp_authorization_fn OnMcpAuthorize;
    void *pMcpAuthorizationData;
    xmutex *pSchedulerLock;
    xwork_schedule_entry **ppSchedules; /* stable until runtime destruction */
    size_t iScheduleCount;
    size_t iScheduleCap;
    uint32_t uMaxSchedules;
    uint64_t uNextScheduleGeneration;
    xmutex *pObservabilityLock;
    xwork_artifact_entry **ppArtifacts;
    size_t iArtifactCount;
    size_t iArtifactReservations;
    uint32_t uMaxArtifacts;
    uint64_t uNextArtifactId;
    xwork_event_entry **ppEvents;
    size_t iEventStart;
    size_t iEventCount;
    uint32_t uMaxEvents;
    size_t iRetainedEventBytes;
    size_t iMaxRetainedEventBytes;
    size_t iMaxEventTextBytes;
    uint64_t uNextEventId;
    uint64_t uDroppedEvents;
};

typedef struct xwork_task_snapshot_entry
{
    xwork_task_info tInfo;
    bool bStopRequested;
} xwork_task_snapshot_entry;

struct xwork_task_snapshot
{
    volatile long iRefs;
    xwork_task_snapshot_entry *pTasks;
    size_t iTaskCount;
};

struct xwork_event_snapshot
{
    volatile long iRefs;
    xwork_event_entry **ppEvents;
    size_t iEventCount;
    uint64_t uNextCursor;
    uint64_t uLatestId;
    uint64_t uDroppedCount;
    bool bHistoryLost;
};

typedef struct xwork_recovery_call_entry
{
    xwork_recovery_call_info tInfo;
    char *sToolCallId;
    char *sToolName;
    char *sArgumentsJson;
} xwork_recovery_call_entry;

struct xwork_recovery_snapshot
{
    volatile long iRefs;
    xwork_recovery_call_entry *pCalls;
    size_t iCallCount;
};

typedef struct xwork_recovery_plan_entry
{
    xwork_recovery_call_entry tCall;
    xwork_recovery_action eAction;
    bool bAutomatic;
} xwork_recovery_plan_entry;

typedef struct xwork_recovery_plan
{
    xwork_recovery_plan_entry *pEntries;
    size_t iEntryCount;
} xwork_recovery_plan;

typedef struct xwork_tool_batch_plan_entry
{
    size_t iSequence;
    uint32_t uWave;
    char *sToolName;
    const xwork_tool_entry *pTool; /* retained catalog */
    xwork_tool_context tContext;
    xwork_permission_resource_writer tResources;
    char **psNormalizedResources;
    bool bDescriptorFailed;
} xwork_tool_batch_plan_entry;

struct xwork_tool_batch_plan
{
    volatile long iRefs;
    xwork_agent *pAgent;
    xwork_tool_catalog *pCatalog;
    uint64_t uRunId;
    xwork_tool_batch_plan_entry *pEntries;
    size_t iEntryCount;
    uint32_t uWaveCount;
};

typedef struct xwork_tool_batch_output
{
    char *sContent;
    bool bSuccess;
    bool bEffectApplied;
    unsigned char *pImageBytes;
    size_t iImageSize;
    char sImageMime[32];
} xwork_tool_batch_output;

struct xwork_agent_definition
{
    volatile long iRefs;
    char *sId;
    char *sSystemPrompt;
    char *sModel;
    char *sReasoningEffort;
    xwork_approval_mode eApprovalMode;
    xwork_eol_policy eEolPolicy;
    uint32_t uCommandTimeoutMs;
    uint32_t uMaxAgentTurns;
    uint32_t uRepeatedToolBatchLimit;
    uint32_t uConsecutiveFailureLimit;
    uint32_t uMaxManagedTasks;
    uint32_t uMaxParallelTools;
    uint32_t uMaxSubagentDepth;
    uint32_t uMaxConcurrentSubagents;
    uint32_t uCompletionVerificationRetries;
    uint32_t uCompactionQualityRetries;
    size_t iMaxInlineToolBytes;
    size_t iMaxCapturedCommandBytes;
    bool bRegisterBuiltinTools;
    bool bAutoSaveSession;
    bool bAllowArtifactWrites;
    bool bRequireVerificationAfterWrite;
    bool bInjectSystemPrompt;
};

typedef enum xwork_operation_status
{
    XWORK_OPERATION_ACTIVE = 0,
    XWORK_OPERATION_CANCELLED,
    XWORK_OPERATION_TIMED_OUT
} xwork_operation_status;

bool xwork__external_paths(const xwork_agent *pAgent);

struct xwork_agent
{
    xwork_process_owner_acquire_fn OnProcessAcquire;
    xwork_process_owner_release_fn OnProcessRelease;
    void *pProcessOwnerData;
    bool bEventSinkFailed;
    volatile long iRefs;
    xmutex *pStateLock;
    xmutex *pCallbackLock;
    xwork_runtime *pRuntime;
    xwork_agent_definition *pDefinition;
    struct xwork_run *pActiveRun; /* borrowed while the state lock says running */
    uint64_t uAgentId;
    bool bClosing;
    xllm_client *pClient;
    xllm_session *pSession;
    char *sWorkspaceRoot;
    bool bMcpServerScope;
    char **psMcpServerScope;
    size_t iMcpServerScopeCount;
    struct
    {
        char *sAlias;
        char *sDirectory;
    } PathMounts[8];
    size_t iPathMountCount;
    bool bFileMountsSealed;
    bool bAllowExternalPaths; /* immutable fallback when no shared scope */
    xwork_file_scope *pFileScope;
    char *sSystemPrompt;
    char *sSessionPath;
    char *sArtifactDirectory;
    char *sProcessOutputDirectory; /* absolute native root; public artifact paths stay relative */
    xwork_artifact_store *pArtifactStore;
    char *sModel;
    char *sReasoningEffort;
    xcancel *pCancel;
    uint64_t uDeadline;

    xwork_approval_mode eApprovalMode;
    xwork_approval_fn OnApproval;
    void *pApprovalUserData;
    xwork_permission_fn OnPermission;
    void *pPermissionUserData;
    xwork_hook_fn OnHook;
    void *pHookUserData;
    xwork_eol_policy eEolPolicy;
    xwork_event_fn OnEvent;
    void *pEventUserData;
    xwork_model_complete_fn OnModelComplete;
    void *pModelUserData;
    xwork_agent_extension_fn OnExtension;
    void *pExtensionUserData;
    void *pOwnerUserData;
    xwork_agent_owner_retain_fn OnOwnerRetain;
    xwork_agent_owner_release_fn OnOwnerRelease;
    bool bOwnerRetained;

    uint32_t uCommandTimeoutMs;
    uint32_t uMaxAgentTurns;
    uint32_t uRepeatedToolBatchLimit;
    uint32_t uConsecutiveFailureLimit;
    uint32_t uMaxManagedProcesses;
    uint32_t uMaxParallelTools;
    uint32_t uMaxSubagentDepth;
    uint32_t uMaxConcurrentSubagents;
    uint32_t uCompletionVerificationRetries;
    uint32_t uCompactionQualityRetries;
    size_t iMaxInlineToolBytes;
    size_t iMaxCapturedCommandBytes;
    bool bAutoSaveSession;
    bool bAllowArtifactWrites;
    bool bRequireVerificationAfterWrite;
    bool bRegisterPythonTool;
    char sPythonPath[280];
    volatile long iCancelled;
    bool bRunning;

    xwork_tool_entry *pTools;
    size_t iToolCount;
    uint64_t uToolRegistryGeneration;
    xwork_tool_catalog *pToolCatalog;
    xwork_subagent_catalog *pSubagentCatalog;
    xwork_delegation_scope *pDelegationScope;
    bool bAllowSubagentDelegation;
    uint64_t uRunSequence;
    uint32_t uAgentDepth;
    uint64_t uDelegationId;
    uint64_t uParentRunId;
    uint64_t uParentTaskId;
    uint64_t uParentAgentTurn;
    uint64_t uSubagentSequence;
    /* python persistent REPL (python tool; state lives for the agent's life) */
    xprocess *pPyProc;
    xmutex *pPyLock;
    xcond *pPyCond;
    xthread *pPyReader;
    char *pPyBuf; /* accumulated stdout (tail-capped) */
    size_t iPyLen;
    size_t iPyCap;
    bool bPyEof;     /* reader saw EOF: interpreter exited */
    uint32_t uPySeq; /* sentinel sequence counter */
};

struct xwork_run
{
    volatile long iRefs;
    xwork_agent *pAgent;
    xmutex *pLock;
    xcond *pDone;
    xthread *pThread;
    xcancel *pCancel;
    xcancelwatch *pExternalWatch;
    char *sPrompt;
    xllm_message tUserMessage;
    bool bHasUserMessage;
    xwork_event_fn OnEvent;
    void *pEventUserData;
    uint64_t uRunId;
    uint64_t uDeadline;
    uint64_t uCreatedUs;
    uint64_t uStartedUs;
    uint64_t uEndedUs;
    xwork_run_state eState;
    xwork_result eResult;
    xwork_run_result tResult;
    xwork_error tError;
    xwork_resume_options tResumeOptions;
    xwork_recovery_decision *pRecoveryDecisions;
    bool bHasResumeOptions;
    bool bResume;
};

char *xwork__strdup(const char *sText);
char *xwork__strndup(const char *sText, size_t iLen);
bool xwork__replace(char **ppDst, const char *sText);
void xwork__set_error(xwork_error *pError, xwork_error_code eCode, const char *sMessage);
xwork_result xwork__tool_fail(xwork_tool_output *pOutput, const char *sMessage);
bool xwork__task_running(xwork_process_entry *pEntry); /* unified task state refresh */
xwork_process_entry *xwork__task_add(xwork_agent *pAgent, xwork_task_kind eKind,
                                     const xwork_tool_context *pContext);
xwork_process_entry *xwork__process_add(xwork_agent *pAgent, const xwork_tool_context *pContext,
                                        xwork_task_kind eKind);
void xwork__task_entry_ref(xwork_process_entry *pEntry);
void xwork__task_entry_release(xwork_process_entry *pEntry);
void xwork__process_remove_entry(xwork_agent *pAgent, xwork_process_entry *pEntry);
void xwork__runtime_tasks_unit(xwork_runtime *pRuntime);
void xwork__task_set_state_locked(xwork_process_entry *pEntry, xwork_task_state eState);
void xwork__task_record_event_locked(xwork_process_entry *pEntry, xwork_task_event_kind eKind);
xwork_process_entry *xwork__runtime_task_find(xwork_runtime *pRuntime, uint64_t uId);
void xwork__subagent_entry_unit(xwork_subagent_entry *pEntry);
xwork_subagent_catalog *xwork__subagent_catalog_empty(xwork_runtime *pRuntime);
xwork_delegation_scope *xwork__delegation_scope_create(uint32_t uMaxDepth, uint32_t uMaxActive);
xwork_delegation_scope *xwork__delegation_scope_ref(xwork_delegation_scope *pScope);
void xwork__delegation_scope_release(xwork_delegation_scope *pScope);
bool xwork__delegation_scope_enter(xwork_delegation_scope *pScope);
void xwork__delegation_scope_leave(xwork_delegation_scope *pScope);
void xwork__copy_model_error(xwork_error *pError, const xllm_error *pModelError);
bool xwork__buf_reserve(xwork_buf *pBuf, size_t iNeed);
bool xwork__buf_append(xwork_buf *pBuf, const void *pData, size_t iLen);
bool xwork__buf_append_cstr(xwork_buf *pBuf, const char *sText);
bool xwork__buf_append_char(xwork_buf *pBuf, char ch);
bool xwork__buf_appendf(xwork_buf *pBuf, const char *sFormat, ...);
char *xwork__buf_detach(xwork_buf *pBuf);
void xwork__buf_unit(xwork_buf *pBuf);
bool xwork__json_string(xwork_buf *pBuf, const char *sText);
void xwork__permission_writer_init(xwork_permission_resource_writer *pWriter);
void xwork__permission_writer_unit(xwork_permission_resource_writer *pWriter);
void xwork__result_writer_init(xwork_tool_result_writer *pWriter, xwork_tool_output *pOutput,
                               size_t iMaxBytes);
bool xwork__result_writer_finish(xwork_tool_result_writer *pWriter, xwork_error *pError);
void xwork__result_writer_unit(xwork_tool_result_writer *pWriter);

xvalue *xwork__json_parse_object(const char *sJson);
xvalue *xwork__json_get(xvalue *pObject, const char *sKey);
const char *xwork__json_text(xvalue *pObject, const char *sKey);
bool xwork__json_bool(xvalue *pObject, const char *sKey, bool bDefault, bool *pValid);
uint64_t xwork__json_u64(xvalue *pObject, const char *sKey, uint64_t uDefault, bool *pValid);

char *xwork__resolve_path(const xwork_agent *pAgent, const char *sPath, xwork_error *pError);
char *xwork__read_recover_path(xwork_agent *pAgent, const char *sPath);
const char *xwork__file_root_for_path(const xwork_agent *pAgent, const char *sPath);
char *xwork__relative_path(const xwork_agent *pAgent, const char *sPath);
bool xwork__ensure_parent(const char *sPath);
bool xwork__parent_exists(const char *sPath);
bool xwork__emit(xwork_agent *pAgent, const xwork_event *pEvent);
void xwork__runtime_record_event(xwork_runtime *pRuntime, xwork_event *pEvent);
xroot xwork__artifact_open_directory(const xwork_agent *, const char *, const char *,
                                            xwork_error *);
bool xwork__runtime_write_artifact(xwork_agent *pAgent, const char *sSource,
                                          const void *pData, size_t iSize, const char *sMediaType,
                                          uint64_t *puArtifactId, char **ppRelativePath,
                                          xwork_error *pError);
xroot xwork__artifact_open_directory(const xwork_agent *Agent, const char *Absolute,
                                            const char *Relative, xwork_error *Error);
xfile xwork__artifact_temp(xroot Directory, char Name[64]);
void xwork__runtime_observability_unit(xwork_runtime *pRuntime);
bool xwork__save(xwork_agent *pAgent, xwork_error *pError);
bool xwork__recover(xwork_agent*,const char*,xwork_error*,uint32_t,bool);
const xwork_tool_entry *xwork__find_tool(const xwork_agent *pAgent, const char *sName);
void xwork__tool_entry_unit(xwork_tool_entry *pTool);
xwork_tool_catalog *xwork__tool_catalog_empty(uint64_t uGeneration);
xwork_tool_catalog *xwork__tool_catalog_add(const xwork_tool_catalog *pBase, uint64_t uGeneration,
                                            const xwork_tool_definition *pDefinition,
                                            xwork_error *pError);
xwork_tool_catalog *xwork__tool_catalog_remove_name(const xwork_tool_catalog *pBase,
                                                    uint64_t uGeneration, const char *sName,
                                                    xwork_error *pError);
xwork_tool_catalog *xwork__tool_catalog_replace_source(const xwork_tool_catalog *pBase,
                                                       uint64_t uGeneration, const char *sSource,
                                                       const xwork_tool_definition *pDefinitions,
                                                       size_t iCount, size_t *piRemoved,
                                                       xwork_error *pError);
xwork_tool_catalog *
xwork__tool_catalog_configure_concurrency(const xwork_tool_catalog *pBase, uint64_t uGeneration,
                                          const xwork_tool_concurrency_config *pConfig,
                                          xwork_error *pError);
xwork_tool_catalog *
xwork__tool_catalog_filter_subagent(const xwork_tool_catalog *pBase, uint64_t uGeneration,
                                    const char *const *psNames, size_t iNameCount,
                                    xwork_tool_effects uAllowedEffects, const void *pOldUserData,
                                    void *pNewUserData, xwork_error *pError);
uint64_t xwork__runtime_next_catalog_generation(xwork_runtime *pRuntime);
void xwork__runtime_tasks_unit(xwork_runtime *pRuntime);
void xwork__python_unit(xwork_agent *pAgent); /* python REPL teardown (python tool) */
bool xwork__register_explore_tools(xwork_agent *pAgent, xwork_error *pError);
bool xwork__register_python_tool(xwork_agent *pAgent, xwork_error *pError);
xwork_result xwork__describe_builtin_permissions(void *pUserData,
                                                 const xwork_tool_context *pContext,
                                                 const char *sArgumentsJson,
                                                 xwork_permission_resource_writer *pWriter,
                                                 xwork_error *pError);
bool xwork__list_directory(const char *sDir, bool bLong, bool bAll, xwork_buf *pOut);

xwork_result xwork__execute_tool(xwork_agent *pAgent, const xllm_tool_call *pCall, uint64_t uTurn,
                                 xcancel *pOperationCancel, uint64_t uOperationDeadline,
                                 char **ppSessionContent, bool *pbSuccess, bool *pbEffectApplied,
                                 unsigned char **ppImageBytes, size_t *piImageSize,
                                 char *psImageMime, xwork_error *pError);

void xwork__runtime_mcp_unit(xwork_runtime *pRuntime);
size_t xwork__runtime_mcp_enabled_count(xwork_runtime *pRuntime);
void xwork__runtime_scheduler_unit(xwork_runtime *pRuntime);
xwork_result xwork__execute_tool_prepared(
    xwork_agent *pAgent, const xllm_tool_call *pCall, uint64_t uTurn, xcancel *pOperationCancel,
    uint64_t uOperationDeadline, const xwork_permission_resource_writer *pPreparedResources,
    char **ppSessionContent, bool *pbSuccess, bool *pbEffectApplied, unsigned char **ppImageBytes,
    size_t *piImageSize, char *psImageMime, xwork_error *pError);
bool xwork__execute_tool_batch(xwork_agent *pAgent, const xllm_tool_call *pCalls, size_t iCallCount,
                               const xllm_executor_ctx *pContext, xwork_tool_batch_output *pOutputs,
                               xwork_error *pError);
void xwork__tool_batch_outputs_unit(xwork_tool_batch_output *pOutputs, size_t iOutputCount);

xllm_result xwork__model_complete(xwork_agent *pAgent, const xllm_request *pRequest,
                                  const xllm_stream_callbacks *pCallbacks,
                                  xllm_response **ppResponse, xllm_error *pError);

xwork_agent *xwork__agent_create_bound(const xwork_agent_config *pConfig, xwork_runtime *pRuntime,
                                       xwork_agent_definition *pDefinition, xwork_error *pError);
xwork_agent *xwork__agent_ref(xwork_agent *pAgent);
void xwork__agent_release(xwork_agent *pAgent);
bool xwork__agent_run_claim(xwork_agent *pAgent, xwork_run *pRun, xwork_error *pError);
void xwork__agent_run_release(xwork_agent *pAgent, xwork_run *pRun);
xwork_result xwork__agent_run_claimed(xwork_agent *pAgent, const char *sPrompt,
                                      const xllm_message *pUserMessage, bool bResume,
                                      const xwork_resume_options *pResumeOptions,
                                      xwork_run_result *pResult, xwork_error *pError);
xwork_recovery_plan *xwork__recovery_plan_create(xwork_agent *pAgent,
                                                 const xwork_resume_options *pOptions,
                                                 xwork_error *pError);
void xwork__recovery_plan_destroy(xwork_recovery_plan *pPlan);
bool xwork__resume_options_clone(xwork_resume_options *pTarget,
                                 xwork_recovery_decision **ppOwnedDecisions,
                                 const xwork_resume_options *pSource, xwork_error *pError);
void xwork__resume_options_unit(xwork_resume_options *pOptions,
                                xwork_recovery_decision *pOwnedDecisions);
xcancel *xwork__active_cancel(const xwork_agent *pAgent);
uint64_t xwork__active_deadline(const xwork_agent *pAgent);

static inline uint64_t xwork__atomic_add_u64(volatile uint64_t *pValue, uint64_t uAdd)
{
#if defined(_MSC_VER)
    return (uint64_t)_InterlockedExchangeAdd64((volatile LONG64 *)pValue, (LONG64)uAdd) + uAdd;
#elif defined(__GNUC__) || defined(__clang__)
    return __atomic_add_fetch(pValue, uAdd, __ATOMIC_SEQ_CST);
#else
    *pValue += uAdd; /* best effort on unknown compilers */
    return *pValue;
#endif
}

static inline long xwork__atomic_load(volatile long *pValue)
{
#if defined(_MSC_VER)
    return _InterlockedCompareExchange(pValue, 0, 0);
#elif defined(__GNUC__) || defined(__clang__)
    return __atomic_load_n(pValue, __ATOMIC_SEQ_CST);
#else
    return *pValue;
#endif
}

static inline void xwork__atomic_store(volatile long *pValue, long iValue)
{
#if defined(_MSC_VER)
    (void)_InterlockedExchange(pValue, iValue);
#elif defined(__GNUC__) || defined(__clang__)
    __atomic_store_n(pValue, iValue, __ATOMIC_SEQ_CST);
#else
    *pValue = iValue;
#endif
}

static inline long xwork__atomic_inc(volatile long *pValue)
{
#if defined(_MSC_VER)
    return _InterlockedIncrement(pValue);
#elif defined(__GNUC__) || defined(__clang__)
    return __atomic_add_fetch(pValue, 1, __ATOMIC_SEQ_CST);
#else
    return ++*pValue;
#endif
}

static inline long xwork__atomic_dec(volatile long *pValue)
{
#if defined(_MSC_VER)
    return _InterlockedDecrement(pValue);
#elif defined(__GNUC__) || defined(__clang__)
    return __atomic_sub_fetch(pValue, 1, __ATOMIC_SEQ_CST);
#else
    return --*pValue;
#endif
}

static inline bool xwork__is_cancelled(xwork_agent *pAgent)
{
    xcancel *pCancel = xwork__active_cancel(pAgent);
    uint64_t uDeadline = xwork__active_deadline(pAgent);
    return pAgent && (xwork__atomic_load(&pAgent->iCancelled) != 0 ||
                      (pCancel && xrtCancelRequested(pCancel)) ||
                      (uDeadline != XLLM_DEADLINE_NEVER && xllmDeadlineExpired(uDeadline)));
}

static inline xwork_operation_status xwork__operation_status(const xwork_agent *pAgent)
{
    xcancel *pCancel;
    uint64_t uDeadline;
    if (!pAgent)
        return XWORK_OPERATION_ACTIVE;
    pCancel = xwork__active_cancel(pAgent);
    uDeadline = xwork__active_deadline(pAgent);
    if (pCancel && xrtCancelRequested(pCancel))
    {
        return XWORK_OPERATION_CANCELLED;
    }
    if (uDeadline != XLLM_DEADLINE_NEVER && xllmDeadlineExpired(uDeadline))
    {
        return XWORK_OPERATION_TIMED_OUT;
    }
    return XWORK_OPERATION_ACTIVE;
}

/* Shared implementation entry points for independent translation units. */
bool xwork__tool_entry_copy(xwork_tool_entry *pTarget, const xwork_tool_entry *pSource,
                            xwork_error *pError);
xwork_tool_catalog *xwork__tool_catalog_allocate(uint64_t uGeneration, size_t iToolCount);
bool xwork__path_is_inside(const char *sRoot, const char *sPath);
const char *xwork__canonical_tool_name(const char *Name);
bool xwork__record_valid(uint32_t uSize, uint32_t uVersion, size_t iNeed);
bool xwork__buf_append_process_text(xwork_buf *pBuf, const void *pData, size_t iSize);
const char *xwork__result_file_relative(const char *Path);
xfile xwork__result_file_open(xwork_agent *Agent, const char *Path);
bool xwork__process_running(const xprocess *pProcess);
xwork_process_capture *xwork__process_capture_create(xprocess *pProcess, size_t iLimit,
                                                     bool bCaptureStderr);
void xwork__process_capture_destroy(xwork_process_capture *pCapture);
void *xwork__process_capture_since(xwork_process_capture *pCapture, bool bStderr, uint64_t uOffset,
                                   size_t iMaxBytes, size_t *pSize, uint64_t *pBaseOffset,
                                   uint64_t *pNextOffset);
void xwork__task_refresh_locked(xwork_process_entry *pEntry);
void xwork__task_mark_stop_locked(xwork_process_entry *pEntry);
bool xwork__task_stop_process_locked(xwork_process_entry *pEntry, const char *sMode);
xwork_process_entry *xwork__process_find(xwork_agent *pAgent, uint64_t uId, size_t *piIndex);
char *xwork__process_destination(xwork_agent *Agent, xvalue *Args, const char *Key);
bool xwork__process_output_read(xwork_process_output *O, bool Err, uint64_t Offset, size_t Max,
                                unsigned char **Bytes, size_t *Size, uint64_t *Next, bool *Dropped);
xwork_result xwork__tool_spawn(void *User, const xwork_tool_context *Context, const char *Arguments,
                               xwork_tool_output *Output, xwork_error *Error);
bool xwork__receipt_value(xvalue *Object, const char *Key, xvalue *Value);
bool xwork__receipt_key(const xwork_tool_entry *Tool, const xllm_tool_call *Call, uint64_t Turn,
                        char Key[65]);
char *xwork__result_locator(xwork_agent *Agent, const char *Path);
int xwork__receipt_load(xwork_agent *Agent, const xwork_tool_entry *Tool,
                        const xllm_tool_call *Call, uint64_t Turn, xwork_tool_output *Output,
                        char **Artifact, xwork_error *Error);
bool xwork__receipt_save(xwork_agent *Agent, const xwork_tool_entry *Tool,
                         const xllm_tool_call *Call, uint64_t Turn, const char *Text, bool Success,
                         const char *Artifact, xwork_error *Error);

#endif

xwork_result xwork__agent_extension(xwork_agent *, xwork_agent_extension_context *, xwork_error *);
