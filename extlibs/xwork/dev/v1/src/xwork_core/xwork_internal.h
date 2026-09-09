#ifndef XWORK_INTERNAL_H
#define XWORK_INTERNAL_H

#include "../../xwork.h"
#include "../../lib/xrt.h"

#include <stdlib.h>
#include <string.h>

typedef struct xwork_tool_record xwork_tool_record;
typedef struct xwork_event_record xwork_event_record;
typedef struct xwork_checkpoint_record xwork_checkpoint_record;
typedef struct xwork_artifact_record xwork_artifact_record;

struct xwork_event_record {
    xwork_event tEvent;
    char *sEventId;
    char *sToolId;
    char *sApprovalRequestId;
    char *sCheckpointId;
    char *sSummary;
};

struct xwork_checkpoint_record {
    xwork_checkpoint tCheckpoint;
    char *sCheckpointId;
    char *sPendingStep;
    char *sSessionStateRef;
    char *sSessionStateData;
    char *sToolOutputsRef;
    char *sWorkspaceSnapshotRef;
    char *sArtifactRefs;
    char *sLastOutputText;
    bool bHasMemoryContext;
    char *sMemoryContextText;
    size_t iMemoryWorkspaceCount;
    bool bHasToolCall;
    char *sToolCallId;
    char *sToolId;
    char *sToolArgumentsJson;
    bool bHasToolResult;
    char *sToolResultText;
    char *sToolVisibleSummary;
    bool bHasApprovalRequest;
    char *sApprovalRequestId;
    char *sApprovalToolId;
    char *sApprovalReason;
    char *sApprovalScope;
    char *sApprovalActionSummary;
    xwork_risk_level eApprovalRiskLevel;
    xwork_approval_state eApprovalState;
    size_t iApprovalSequence;
};

struct xwork_artifact_record {
    xwork_artifact tArtifact;
    char *sArtifactId;
    char *sOutputRole;
    char *sReportSubjectRef;
    char *sName;
    char *sMimeType;
    char *sStorageRef;
    char *sSummary;
    char *sContentText;
    char *sPatchApplyResultJson;
    char *sPatchFileSummaryJson;
    char *sCommandText;
    xwork_artifact_output_class eOutputClass;
    xwork_artifact_report_class eReportClass;
    bool bHasContentStats;
    size_t iContentByteCount;
    size_t iContentLineCount;
    bool bHasPatchStats;
    size_t iPatchFileCount;
    size_t iPatchHunkCount;
    size_t iPatchAddedLineCount;
    size_t iPatchDeletedLineCount;
    bool bHasCommandIoStats;
    size_t iStdoutByteCount;
    size_t iStderrByteCount;
    bool bStdoutTruncated;
    bool bStderrTruncated;
    bool bHasExitCode;
    int iExitCode;
};

struct xwork_runtime {
    xllm_runtime *pLlmRuntime;
    bool bOwnLlmRuntime;
    xwork_replay_engine *pReplayEngine;
    char *sLastReplayHostOutputText;
    char *sLastReplayHostVisibleSummary;
    xwork_host_services tHostServices;
    xwork_persistence_backend tPersistenceBackend;
    xwork_policy_options tPolicy;
    void *pUserData;
    xwork_workspace *pWorkspaces;
    xwork_tool_record *pTools;
    xwork_run *pRuns;
};

struct xwork_workspace {
    xwork_runtime *pRuntime;
    char *sWorkspaceId;
    char *sRootPath;
    bool bEnableMemory;
    xllm_memory *pMemory;
    char *sMemorySyncAllowedExtensions;
    char *sMemorySyncIgnoredDirectories;
    char *sMemorySyncIgnoredExtensions;
    char *sMemorySyncIgnoredPathPatterns;
    char *sMemorySyncIgnoredFiles;
    size_t iMemorySyncMaxFileBytes;
    xwork_workspace *pNext;
};

struct xwork_tool_record {
    xwork_tool_def tDef;
    char *sToolId;
    char *sDisplayName;
    char *sDescription;
    xwork_tool_record *pNext;
};

struct xwork_run {
    xwork_runtime *pRuntime;
    char *sRunId;
    char *sParentRunId;
    char *sAgentId;
    char *sTaskId;
    char *sInstruction;
    char *sLlmProfileId;
    char *sSessionProfileId;
    char *sSessionStateData;
    xllm_session *pSession;
    char **psWorkspaceIds;
    size_t iWorkspaceCount;
    xwork_autonomy_mode eAutonomy;
    xwork_session_policy tSessionPolicy;
    xwork_run_state eState;
    xmutex_struct tExecutionLock;
    bool bExecutionLockInitialized;
    bool bExecuting;
    char *sLastOutputText;
    bool bHasLastMemoryContext;
    char *sLastMemoryContextText;
    size_t iLastMemoryWorkspaceCount;
    bool bHasLastToolCall;
    char *sLastToolCallId;
    char *sLastToolId;
    char *sLastToolArgumentsJson;
    bool bHasLastToolResult;
    char *sLastToolResultText;
    char *sLastToolVisibleSummary;
    char *sLastEventId;
    char *sLastEventToolId;
    char *sLastEventApprovalRequestId;
    char *sLastEventCheckpointId;
    char *sLastEventSummary;
    xwork_event_kind eLastEventKind;
    xwork_run_state eLastEventRunState;
    size_t iLastEventSequence;
    size_t iNextEventSequence;
    bool bHasLastApprovalRequest;
    char *sLastApprovalRequestId;
    char *sLastApprovalToolId;
    char *sLastApprovalReason;
    char *sLastApprovalScope;
    char *sLastApprovalActionSummary;
    xwork_risk_level eLastApprovalRiskLevel;
    xwork_approval_state eLastApprovalState;
    size_t iLastApprovalSequence;
    size_t iNextApprovalSequence;
    xwork_event_record *pEventLog;
    size_t iEventCount;
    size_t iEventCapacity;
    bool bHasLastCheckpoint;
    char *sLastCheckpointId;
    char *sLastCheckpointPendingStep;
    char *sLastCheckpointSessionStateRef;
    char *sLastCheckpointToolOutputsRef;
    char *sLastCheckpointWorkspaceSnapshotRef;
    char *sLastCheckpointArtifactRefs;
    xwork_checkpoint_kind eLastCheckpointKind;
    xwork_run_state eLastCheckpointRunState;
    size_t iLastCheckpointSequence;
    size_t iNextCheckpointSequence;
    xwork_checkpoint_record *pCheckpointLog;
    size_t iCheckpointCount;
    size_t iCheckpointCapacity;
    xwork_artifact_record *pArtifactLog;
    size_t iArtifactCount;
    size_t iArtifactCapacity;
    size_t iNextArtifactSequence;
    xwork_run *pNext;
};

char *xwork__dup_cstr(const char *sText);
char *xwork__dup_printf(const char *sFormat, ...);
char *xwork__dup_scoped_id(const char *sRunId, const char *sKind, size_t iSequence);
void xwork__free_cstr(char **psText);
void xwork__free_str_array(char ***ppsItems, size_t *piItemCount);
void xwork__run_reset_observability(xwork_run *pRun);
bool xwork__runtime_has_workspace(const xwork_runtime *pRuntime, const char *sWorkspaceId);
bool xwork__run_state_is_terminal(xwork_run_state eState);
xwork_status xwork__run_begin_execution(xwork_run *pRun);
void xwork__run_end_execution(xwork_run *pRun);
xwork_status xwork__replace_cstr(char **psTarget, const char *sText);
xwork_status xwork__run_step_from_event(
    xwork_run_step *pStep,
    const xwork_event *pEvent,
    const xwork_checkpoint *pCheckpoint
);
bool xwork__run_step_matches_query(
    const xwork_run_step *pStep,
    const xwork_run_step_query *pQuery
);
xwork_status xwork__run_step_list_append(
    xwork_run_step_list *pList,
    const xwork_run_step *pStep
);
xwork_status xwork__run_set_last_memory_context(
    xwork_run *pRun,
    const xwork_memory_context *pContext
);
xwork_status xwork__run_record_event(
    xwork_run *pRun,
    xwork_event_kind eKind,
    const char *sToolId,
    const char *sApprovalRequestId,
    const char *sCheckpointId,
    const char *sSummary
);
xwork_status xwork__run_record_approval_request(
    xwork_run *pRun,
    const char *sRequestId,
    const char *sToolId,
    const char *sReason,
    xwork_risk_level eRiskLevel,
    const char *sScope,
    const char *sActionSummary,
    xwork_approval_state eState
);
xwork_status xwork__run_record_checkpoint(
    xwork_run *pRun,
    xwork_checkpoint_kind eKind,
    const char *sPendingStep,
    const char *sSessionStateRef,
    const char *sToolOutputsRef,
    const char *sWorkspaceSnapshotRef,
    const char *sArtifactRefs
);
void xwork__run_reset_persistence(xwork_run *pRun);
void xwork__run_reset_artifacts(xwork_run *pRun);
xwork_status xwork__run_append_event_snapshot(xwork_run *pRun);
xwork_status xwork__run_append_checkpoint_snapshot(xwork_run *pRun);
xwork_status xwork__run_append_artifact_record(
    xwork_run *pRun,
    const xwork_artifact_options *pOptions,
    xwork_artifact *pArtifact
);
xwork_status xwork__run_snapshot_copy_artifacts(
    xwork_run_snapshot *pSnapshot,
    const xwork_run *pRun
);
void xwork__run_snapshot_reset_artifacts(xwork_run_snapshot *pSnapshot);
xwork_status xwork__run_apply_snapshot_artifacts(
    xwork_run *pRun,
    const xwork_run_snapshot *pSnapshot
);
char *xwork__run_build_artifact_refs(const xwork_run *pRun);
xwork_status xwork__runtime_store_event(
    const xwork_runtime *pRuntime,
    const xwork_event *pEvent
);
xwork_status xwork__runtime_store_checkpoint(
    const xwork_runtime *pRuntime,
    const xwork_checkpoint *pCheckpoint,
    const xwork_run_snapshot *pSnapshot
);
xwork_status xwork__runtime_store_run_snapshot(
    const xwork_runtime *pRuntime,
    const xwork_run_snapshot *pSnapshot
);
xwork_status xwork__runtime_store_artifact(
    const xwork_runtime *pRuntime,
    const xwork_artifact *pArtifact
);
xwork_status xwork__runtime_load_run_snapshot(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_run_snapshot *pSnapshot
);
xwork_status xwork__runtime_load_checkpoint_snapshot(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    const char *sCheckpointId,
    xwork_run_snapshot *pSnapshot
);
xwork_status xwork__runtime_list_runs(
    const xwork_runtime *pRuntime,
    xwork_string_list *pList
);
xwork_status xwork__runtime_list_checkpoints(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_string_list *pList
);
xwork_status xwork__runtime_list_events(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_string_list *pList
);
xwork_status xwork__runtime_list_artifacts(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_string_list *pList
);
xwork_status xwork__runtime_list_run_summaries(
    const xwork_runtime *pRuntime,
    xwork_run_summary_list *pList
);
xwork_status xwork__runtime_query_run_index(
    const xwork_runtime *pRuntime,
    const xwork_run_index_query *pQuery,
    xwork_run_index_list *pList
);
xwork_status xwork__runtime_load_run_summary(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_run_summary *pSummary
);
xwork_status xwork__runtime_load_last_event(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_event *pEvent
);
xwork_status xwork__runtime_load_last_approval_request(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_approval_request *pRequest
);
xwork_status xwork__runtime_load_last_checkpoint(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_checkpoint *pCheckpoint
);
xwork_status xwork__runtime_load_last_artifact(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_artifact *pArtifact
);
xwork_status xwork__runtime_load_event(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    const char *sEventId,
    xwork_event *pEvent
);
xwork_status xwork__runtime_load_checkpoint(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    const char *sCheckpointId,
    xwork_checkpoint *pCheckpoint
);
xwork_status xwork__runtime_load_artifact(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    const char *sArtifactId,
    xwork_artifact *pArtifact
);
xwork_status xwork__runtime_find_artifact_by_name(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    const char *sArtifactName,
    xwork_artifact *pArtifact
);
xwork_status xwork__run_restore_checkpoint_snapshot(
    xwork_run *pRun,
    const xwork_checkpoint_record *pRecord
);
void xwork__run_discard_session(xwork_run *pRun);
void xwork__run_reset_session_state(xwork_run *pRun);
xwork_status xwork__run_refresh_session_state(xwork_run *pRun);
xwork_status xwork__run_ensure_session(xwork_run *pRun);

#endif
