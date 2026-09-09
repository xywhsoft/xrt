#include "xwork_internal.h"
#include "../../lib/xllm.h"

#include <stdarg.h>
#include <stdio.h>

const char *xwork_version(void)
{
    return "0.1.0";
}

const char *xwork_status_cstr(xwork_status eStatus)
{
    switch ( eStatus ) {
        case XWORK_OK:
            return "XWORK_OK";
        case XWORK_ERROR_INVALID_ARGUMENT:
            return "XWORK_ERROR_INVALID_ARGUMENT";
        case XWORK_ERROR_NO_MEMORY:
            return "XWORK_ERROR_NO_MEMORY";
        case XWORK_ERROR_NOT_FOUND:
            return "XWORK_ERROR_NOT_FOUND";
        case XWORK_ERROR_ALREADY_EXISTS:
            return "XWORK_ERROR_ALREADY_EXISTS";
        case XWORK_ERROR_INVALID_STATE:
            return "XWORK_ERROR_INVALID_STATE";
        case XWORK_ERROR_EXTERNAL_FAILURE:
            return "XWORK_ERROR_EXTERNAL_FAILURE";
        case XWORK_ERROR_UNSUPPORTED:
            return "XWORK_ERROR_UNSUPPORTED";
        case XWORK_ERROR_CANCELLED:
            return "XWORK_ERROR_CANCELLED";
        case XWORK_ERROR_PAUSED:
            return "XWORK_ERROR_PAUSED";
        default:
            return "XWORK_STATUS_UNKNOWN";
    }
}

char *xwork__dup_cstr(const char *sText)
{
    char *sCopy;
    size_t iLen;

    if ( !sText ) {
        return NULL;
    }

    iLen = strlen(sText);
    sCopy = (char *)calloc(iLen + 1u, sizeof(char));
    if ( !sCopy ) {
        return NULL;
    }

    memcpy(sCopy, sText, iLen);
    sCopy[iLen] = '\0';
    return sCopy;
}

char *xwork__dup_printf(const char *sFormat, ...)
{
    va_list tArgs;
    va_list tArgsCopy;
    int iRequired;
    char *sText;

    if ( !sFormat ) {
        return NULL;
    }

    va_start(tArgs, sFormat);
    va_copy(tArgsCopy, tArgs);
    iRequired = vsnprintf(NULL, 0, sFormat, tArgsCopy);
    va_end(tArgsCopy);
    if ( iRequired < 0 ) {
        va_end(tArgs);
        return NULL;
    }

    sText = (char *)calloc((size_t)iRequired + 1u, sizeof(char));
    if ( !sText ) {
        va_end(tArgs);
        return NULL;
    }

    (void)vsnprintf(sText, (size_t)iRequired + 1u, sFormat, tArgs);
    va_end(tArgs);
    return sText;
}

char *xwork__dup_scoped_id(const char *sRunId, const char *sKind, size_t iSequence)
{
    if ( !sRunId || !sKind || !sKind[0] ) {
        return NULL;
    }
    return xwork__dup_printf("%s:%s:%lu", sRunId, sKind, (unsigned long)iSequence);
}

void xwork__free_cstr(char **psText)
{
    if ( psText && *psText ) {
        free(*psText);
        *psText = NULL;
    }
}

void xwork__free_str_array(char ***ppsItems, size_t *piItemCount)
{
    size_t i;

    if ( !ppsItems || !*ppsItems ) {
        if ( piItemCount ) {
            *piItemCount = 0u;
        }
        return;
    }

    if ( piItemCount ) {
        for ( i = 0u; i < *piItemCount; ++i ) {
            free((*ppsItems)[i]);
        }
        *piItemCount = 0u;
    }
    free(*ppsItems);
    *ppsItems = NULL;
}

xwork_status xwork__replace_cstr(char **psTarget, const char *sText)
{
    char *sCopy = NULL;

    if ( !psTarget ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    if ( sText ) {
        sCopy = xwork__dup_cstr(sText);
        if ( !sCopy ) {
            return XWORK_ERROR_NO_MEMORY;
        }
    }

    xwork__free_cstr(psTarget);
    *psTarget = sCopy;
    return XWORK_OK;
}

xwork_status xwork__run_set_last_memory_context(
    xwork_run *pRun,
    const xwork_memory_context *pContext
)
{
    xwork_status iStatus;

    if ( !pRun ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    if ( !pContext || !pContext->sText || !pContext->sText[0] ) {
        xwork__free_cstr(&pRun->sLastMemoryContextText);
        pRun->bHasLastMemoryContext = false;
        pRun->iLastMemoryWorkspaceCount = 0u;
        return XWORK_OK;
    }

    iStatus = xwork__replace_cstr(&pRun->sLastMemoryContextText, pContext->sText);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    pRun->bHasLastMemoryContext = true;
    pRun->iLastMemoryWorkspaceCount = pContext->iWorkspaceCount;
    return XWORK_OK;
}

bool xwork__runtime_has_workspace(const xwork_runtime *pRuntime, const char *sWorkspaceId)
{
    return xwork_runtime_find_workspace(pRuntime, sWorkspaceId) != NULL;
}

bool xwork__run_state_is_terminal(xwork_run_state eState)
{
    return eState == XWORK_RUN_COMPLETED ||
           eState == XWORK_RUN_CANCELLED ||
           eState == XWORK_RUN_FAILED;
}

xwork_status xwork__run_begin_execution(xwork_run *pRun)
{
    if ( !pRun || !pRun->bExecutionLockInitialized ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xrtMutexLock(&pRun->tExecutionLock);
    if ( pRun->bExecuting ) {
        xrtMutexUnlock(&pRun->tExecutionLock);
        return XWORK_ERROR_INVALID_STATE;
    }
    pRun->bExecuting = true;
    xrtMutexUnlock(&pRun->tExecutionLock);
    return XWORK_OK;
}

void xwork__run_end_execution(xwork_run *pRun)
{
    if ( !pRun || !pRun->bExecutionLockInitialized ) {
        return;
    }

    xrtMutexLock(&pRun->tExecutionLock);
    pRun->bExecuting = false;
    xrtMutexUnlock(&pRun->tExecutionLock);
}

static void xwork__free_tool_record(xwork_tool_record *pTool)
{
    if ( !pTool ) {
        return;
    }

    xwork__free_cstr(&pTool->sToolId);
    xwork__free_cstr(&pTool->sDisplayName);
    xwork__free_cstr(&pTool->sDescription);
    memset(&pTool->tDef, 0, sizeof(pTool->tDef));
    free(pTool);
}

static xwork_status xwork__run_snapshot_fill_from_run(
    const xwork_run *pRun,
    xwork_run_snapshot *pSnapshot
);

void xwork__run_reset_observability(xwork_run *pRun)
{
    if ( !pRun ) {
        return;
    }

    xwork__run_reset_session_state(pRun);
    xwork__free_cstr(&pRun->sLastOutputText);
    xwork__free_cstr(&pRun->sLastMemoryContextText);
    pRun->bHasLastMemoryContext = false;
    pRun->iLastMemoryWorkspaceCount = 0u;
    pRun->bHasLastToolCall = false;
    xwork__free_cstr(&pRun->sLastToolCallId);
    xwork__free_cstr(&pRun->sLastToolId);
    xwork__free_cstr(&pRun->sLastToolArgumentsJson);
    pRun->bHasLastToolResult = false;
    xwork__free_cstr(&pRun->sLastToolResultText);
    xwork__free_cstr(&pRun->sLastToolVisibleSummary);

    xwork__free_cstr(&pRun->sLastEventId);
    xwork__free_cstr(&pRun->sLastEventToolId);
    xwork__free_cstr(&pRun->sLastEventApprovalRequestId);
    xwork__free_cstr(&pRun->sLastEventCheckpointId);
    xwork__free_cstr(&pRun->sLastEventSummary);
    pRun->eLastEventKind = XWORK_EVENT_NONE;
    pRun->eLastEventRunState = XWORK_RUN_CREATED;
    pRun->iLastEventSequence = 0u;
    pRun->iNextEventSequence = 0u;

    xwork__free_cstr(&pRun->sLastApprovalRequestId);
    xwork__free_cstr(&pRun->sLastApprovalToolId);
    xwork__free_cstr(&pRun->sLastApprovalReason);
    xwork__free_cstr(&pRun->sLastApprovalScope);
    xwork__free_cstr(&pRun->sLastApprovalActionSummary);
    pRun->bHasLastApprovalRequest = false;
    pRun->eLastApprovalRiskLevel = XWORK_RISK_LOW;
    pRun->eLastApprovalState = XWORK_APPROVAL_PENDING;
    pRun->iLastApprovalSequence = 0u;
    pRun->iNextApprovalSequence = 0u;

    xwork__free_cstr(&pRun->sLastCheckpointId);
    xwork__free_cstr(&pRun->sLastCheckpointPendingStep);
    xwork__free_cstr(&pRun->sLastCheckpointSessionStateRef);
    xwork__free_cstr(&pRun->sLastCheckpointToolOutputsRef);
    xwork__free_cstr(&pRun->sLastCheckpointWorkspaceSnapshotRef);
    xwork__free_cstr(&pRun->sLastCheckpointArtifactRefs);
    pRun->bHasLastCheckpoint = false;
    pRun->eLastCheckpointKind = XWORK_CHECKPOINT_MANUAL;
    pRun->eLastCheckpointRunState = XWORK_RUN_CREATED;
    pRun->iLastCheckpointSequence = 0u;
    pRun->iNextCheckpointSequence = 0u;
}

xwork_status xwork__run_record_event(
    xwork_run *pRun,
    xwork_event_kind eKind,
    const char *sToolId,
    const char *sApprovalRequestId,
    const char *sCheckpointId,
    const char *sSummary
)
{
    char *sEventId;
    char *sToolIdCopy = NULL;
    char *sApprovalRequestIdCopy = NULL;
    char *sCheckpointIdCopy = NULL;
    char *sSummaryCopy = NULL;
    size_t iEventIndex;
    size_t iSequence;

    if ( !pRun || !pRun->sRunId ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    iSequence = pRun->iNextEventSequence + 1u;
    sEventId = xwork__dup_scoped_id(pRun->sRunId, "event", iSequence);
    if ( !sEventId ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    if ( sToolId ) {
        sToolIdCopy = xwork__dup_cstr(sToolId);
        if ( !sToolIdCopy ) {
            free(sEventId);
            return XWORK_ERROR_NO_MEMORY;
        }
    }
    if ( sApprovalRequestId ) {
        sApprovalRequestIdCopy = xwork__dup_cstr(sApprovalRequestId);
        if ( !sApprovalRequestIdCopy ) {
            free(sEventId);
            free(sToolIdCopy);
            return XWORK_ERROR_NO_MEMORY;
        }
    }
    if ( sCheckpointId ) {
        sCheckpointIdCopy = xwork__dup_cstr(sCheckpointId);
        if ( !sCheckpointIdCopy ) {
            free(sEventId);
            free(sToolIdCopy);
            free(sApprovalRequestIdCopy);
            return XWORK_ERROR_NO_MEMORY;
        }
    }
    if ( sSummary ) {
        sSummaryCopy = xwork__dup_cstr(sSummary);
        if ( !sSummaryCopy ) {
            free(sEventId);
            free(sToolIdCopy);
            free(sApprovalRequestIdCopy);
            free(sCheckpointIdCopy);
            return XWORK_ERROR_NO_MEMORY;
        }
    }

    xwork__free_cstr(&pRun->sLastEventId);
    xwork__free_cstr(&pRun->sLastEventToolId);
    xwork__free_cstr(&pRun->sLastEventApprovalRequestId);
    xwork__free_cstr(&pRun->sLastEventCheckpointId);
    xwork__free_cstr(&pRun->sLastEventSummary);

    pRun->sLastEventId = sEventId;
    pRun->sLastEventToolId = sToolIdCopy;
    pRun->sLastEventApprovalRequestId = sApprovalRequestIdCopy;
    pRun->sLastEventCheckpointId = sCheckpointIdCopy;
    pRun->sLastEventSummary = sSummaryCopy;
    pRun->eLastEventKind = eKind;
    pRun->eLastEventRunState = pRun->eState;
    pRun->iLastEventSequence = iSequence;
    pRun->iNextEventSequence = iSequence;
    iEventIndex = pRun->iEventCount;
    {
        xwork_status iStatus = xwork__run_append_event_snapshot(pRun);
        if ( iStatus != XWORK_OK ) {
            return iStatus;
        }
    }
    return xwork__runtime_store_event(pRun->pRuntime, &pRun->pEventLog[iEventIndex].tEvent);
}

xwork_status xwork__run_record_approval_request(
    xwork_run *pRun,
    const char *sRequestId,
    const char *sToolId,
    const char *sReason,
    xwork_risk_level eRiskLevel,
    const char *sScope,
    const char *sActionSummary,
    xwork_approval_state eState
)
{
    char *sRequestIdCopy = NULL;
    char *sToolIdCopy = NULL;
    char *sReasonCopy = NULL;
    char *sScopeCopy = NULL;
    char *sActionSummaryCopy = NULL;
    size_t iSequence;

    if ( !pRun || !pRun->sRunId ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    if ( sRequestId &&
         pRun->sLastApprovalRequestId &&
         strcmp(sRequestId, pRun->sLastApprovalRequestId) == 0 ) {
        iSequence = pRun->iLastApprovalSequence;
        sRequestIdCopy = xwork__dup_cstr(sRequestId);
    } else if ( sRequestId ) {
        iSequence = pRun->iNextApprovalSequence + 1u;
        sRequestIdCopy = xwork__dup_cstr(sRequestId);
    } else {
        iSequence = pRun->iNextApprovalSequence + 1u;
        sRequestIdCopy = xwork__dup_scoped_id(pRun->sRunId, "approval", iSequence);
    }
    if ( !sRequestIdCopy ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    if ( sToolId ) {
        sToolIdCopy = xwork__dup_cstr(sToolId);
        if ( !sToolIdCopy ) {
            free(sRequestIdCopy);
            return XWORK_ERROR_NO_MEMORY;
        }
    }
    if ( sReason ) {
        sReasonCopy = xwork__dup_cstr(sReason);
        if ( !sReasonCopy ) {
            free(sRequestIdCopy);
            free(sToolIdCopy);
            return XWORK_ERROR_NO_MEMORY;
        }
    }
    if ( sScope ) {
        sScopeCopy = xwork__dup_cstr(sScope);
        if ( !sScopeCopy ) {
            free(sRequestIdCopy);
            free(sToolIdCopy);
            free(sReasonCopy);
            return XWORK_ERROR_NO_MEMORY;
        }
    }
    if ( sActionSummary ) {
        sActionSummaryCopy = xwork__dup_cstr(sActionSummary);
        if ( !sActionSummaryCopy ) {
            free(sRequestIdCopy);
            free(sToolIdCopy);
            free(sReasonCopy);
            free(sScopeCopy);
            return XWORK_ERROR_NO_MEMORY;
        }
    }

    xwork__free_cstr(&pRun->sLastApprovalRequestId);
    xwork__free_cstr(&pRun->sLastApprovalToolId);
    xwork__free_cstr(&pRun->sLastApprovalReason);
    xwork__free_cstr(&pRun->sLastApprovalScope);
    xwork__free_cstr(&pRun->sLastApprovalActionSummary);

    pRun->sLastApprovalRequestId = sRequestIdCopy;
    pRun->sLastApprovalToolId = sToolIdCopy;
    pRun->sLastApprovalReason = sReasonCopy;
    pRun->sLastApprovalScope = sScopeCopy;
    pRun->sLastApprovalActionSummary = sActionSummaryCopy;
    pRun->eLastApprovalRiskLevel = eRiskLevel;
    pRun->eLastApprovalState = eState;
    pRun->bHasLastApprovalRequest = true;
    pRun->iLastApprovalSequence = iSequence;
    if ( iSequence > pRun->iNextApprovalSequence ) {
        pRun->iNextApprovalSequence = iSequence;
    }
    return XWORK_OK;
}

xwork_status xwork__run_record_checkpoint(
    xwork_run *pRun,
    xwork_checkpoint_kind eKind,
    const char *sPendingStep,
    const char *sSessionStateRef,
    const char *sToolOutputsRef,
    const char *sWorkspaceSnapshotRef,
    const char *sArtifactRefs
)
{
    char *sCheckpointId;
    char *sPendingStepCopy = NULL;
    char *sSessionStateRefCopy = NULL;
    char *sToolOutputsRefCopy = NULL;
    char *sWorkspaceSnapshotRefCopy = NULL;
    char *sArtifactRefsCopy = NULL;
    size_t iSequence;

    if ( !pRun || !pRun->sRunId ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    iSequence = pRun->iNextCheckpointSequence + 1u;
    sCheckpointId = xwork__dup_scoped_id(pRun->sRunId, "checkpoint", iSequence);
    if ( !sCheckpointId ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    if ( sPendingStep ) {
        sPendingStepCopy = xwork__dup_cstr(sPendingStep);
        if ( !sPendingStepCopy ) {
            free(sCheckpointId);
            return XWORK_ERROR_NO_MEMORY;
        }
    }
    if ( sSessionStateRef ) {
        sSessionStateRefCopy = xwork__dup_cstr(sSessionStateRef);
        if ( !sSessionStateRefCopy ) {
            free(sCheckpointId);
            free(sPendingStepCopy);
            return XWORK_ERROR_NO_MEMORY;
        }
    }
    if ( sToolOutputsRef ) {
        sToolOutputsRefCopy = xwork__dup_cstr(sToolOutputsRef);
        if ( !sToolOutputsRefCopy ) {
            free(sCheckpointId);
            free(sPendingStepCopy);
            free(sSessionStateRefCopy);
            return XWORK_ERROR_NO_MEMORY;
        }
    }
    if ( sWorkspaceSnapshotRef ) {
        sWorkspaceSnapshotRefCopy = xwork__dup_cstr(sWorkspaceSnapshotRef);
        if ( !sWorkspaceSnapshotRefCopy ) {
            free(sCheckpointId);
            free(sPendingStepCopy);
            free(sSessionStateRefCopy);
            free(sToolOutputsRefCopy);
            return XWORK_ERROR_NO_MEMORY;
        }
    }
    if ( sArtifactRefs ) {
        sArtifactRefsCopy = xwork__dup_cstr(sArtifactRefs);
        if ( !sArtifactRefsCopy ) {
            free(sCheckpointId);
            free(sPendingStepCopy);
            free(sSessionStateRefCopy);
            free(sToolOutputsRefCopy);
            free(sWorkspaceSnapshotRefCopy);
            return XWORK_ERROR_NO_MEMORY;
        }
    }

    xwork__free_cstr(&pRun->sLastCheckpointId);
    xwork__free_cstr(&pRun->sLastCheckpointPendingStep);
    xwork__free_cstr(&pRun->sLastCheckpointSessionStateRef);
    xwork__free_cstr(&pRun->sLastCheckpointToolOutputsRef);
    xwork__free_cstr(&pRun->sLastCheckpointWorkspaceSnapshotRef);
    xwork__free_cstr(&pRun->sLastCheckpointArtifactRefs);

    pRun->sLastCheckpointId = sCheckpointId;
    pRun->sLastCheckpointPendingStep = sPendingStepCopy;
    pRun->sLastCheckpointSessionStateRef = sSessionStateRefCopy;
    pRun->sLastCheckpointToolOutputsRef = sToolOutputsRefCopy;
    pRun->sLastCheckpointWorkspaceSnapshotRef = sWorkspaceSnapshotRefCopy;
    pRun->sLastCheckpointArtifactRefs = sArtifactRefsCopy;
    pRun->eLastCheckpointKind = eKind;
    pRun->eLastCheckpointRunState = pRun->eState;
    pRun->bHasLastCheckpoint = true;
    pRun->iLastCheckpointSequence = iSequence;
    pRun->iNextCheckpointSequence = iSequence;
    {
        xwork_run_snapshot tSnapshot;
        xwork_status iStatus;

        xwork_run_snapshot_init(&tSnapshot);
        iStatus = xwork__run_append_checkpoint_snapshot(pRun);
        if ( iStatus != XWORK_OK ) {
            return iStatus;
        }

        iStatus = xwork__run_snapshot_fill_from_run(pRun, &tSnapshot);
        if ( iStatus == XWORK_OK ) {
            iStatus = xwork__runtime_store_checkpoint(
                pRun->pRuntime,
                &pRun->pCheckpointLog[pRun->iCheckpointCount - 1u].tCheckpoint,
                &tSnapshot
            );
        }
        xwork_run_snapshot_reset(&tSnapshot);
        return iStatus;
    }
}

void xwork_runtime_options_init(xwork_runtime_options *pOptions)
{
    if ( pOptions ) {
        memset(pOptions, 0, sizeof(*pOptions));
        xwork_policy_options_init(&pOptions->tPolicy);
    }
}

void xwork_workspace_options_init(xwork_workspace_options *pOptions)
{
    if ( pOptions ) {
        memset(pOptions, 0, sizeof(*pOptions));
    }
}

void xwork_tool_def_init(xwork_tool_def *pDef)
{
    if ( pDef ) {
        memset(pDef, 0, sizeof(*pDef));
        pDef->eKind = XWORK_TOOL_HOST_SERVICE;
        pDef->eHostService = XWORK_HOST_NONE;
        pDef->eSideEffect = XWORK_SIDE_EFFECT_READ_ONLY;
        pDef->eApprovalMode = XWORK_APPROVAL_DEFAULT;
    }
}

void xwork_run_options_init(xwork_run_options *pOptions)
{
    if ( pOptions ) {
        memset(pOptions, 0, sizeof(*pOptions));
        pOptions->eAutonomy = XWORK_AUTONOMY_SEMI_AUTO;
        xwork_session_policy_init(&pOptions->tSessionPolicy);
    }
}

void xwork_run_summary_init(xwork_run_summary *pSummary)
{
    if ( pSummary ) {
        memset(pSummary, 0, sizeof(*pSummary));
        pSummary->eAutonomy = XWORK_AUTONOMY_SEMI_AUTO;
        pSummary->eState = XWORK_RUN_CREATED;
    }
}

void xwork_run_summary_reset(xwork_run_summary *pSummary)
{
    if ( !pSummary ) {
        return;
    }

    xwork__free_cstr((char **)&pSummary->sRunId);
    xwork__free_cstr((char **)&pSummary->sParentRunId);
    xwork__free_cstr((char **)&pSummary->sAgentId);
    xwork__free_cstr((char **)&pSummary->sTaskId);
    xwork__free_cstr((char **)&pSummary->sInstruction);
    xwork_run_summary_init(pSummary);
}

void xwork_run_summary_list_init(xwork_run_summary_list *pList)
{
    if ( pList ) {
        memset(pList, 0, sizeof(*pList));
    }
}

void xwork_run_summary_list_reset(xwork_run_summary_list *pList)
{
    xwork_run_summary *pItems;
    size_t i;

    if ( !pList ) {
        return;
    }

    pItems = (xwork_run_summary *)pList->pItems;
    if ( pItems ) {
        for ( i = 0u; i < pList->iCount; ++i ) {
            xwork_run_summary_reset(&pItems[i]);
        }
        free(pItems);
    }

    xwork_run_summary_list_init(pList);
}

void xwork_artifact_summary_init(xwork_artifact_summary *pSummary)
{
    if ( pSummary ) {
        memset(pSummary, 0, sizeof(*pSummary));
        pSummary->eKind = XWORK_ARTIFACT_OUTPUT;
    }
}

void xwork_artifact_summary_reset(xwork_artifact_summary *pSummary)
{
    if ( !pSummary ) {
        return;
    }

    xwork__free_cstr((char **)&pSummary->sArtifactId);
    xwork__free_cstr((char **)&pSummary->sName);
    xwork__free_cstr((char **)&pSummary->sMimeType);
    xwork__free_cstr((char **)&pSummary->sStorageRef);
    xwork__free_cstr((char **)&pSummary->sSummary);
    xwork__free_cstr((char **)&pSummary->sOutputRole);
    xwork__free_cstr((char **)&pSummary->sReportSubjectRef);
    xwork__free_cstr((char **)&pSummary->sPatchApplyResultJson);
    xwork__free_cstr((char **)&pSummary->sPatchFileSummaryJson);
    xwork_artifact_summary_init(pSummary);
}

void xwork_artifact_summary_list_init(xwork_artifact_summary_list *pList)
{
    if ( pList ) {
        memset(pList, 0, sizeof(*pList));
    }
}

void xwork_artifact_summary_list_reset(xwork_artifact_summary_list *pList)
{
    xwork_artifact_summary *pItems;
    size_t i;

    if ( !pList ) {
        return;
    }

    pItems = (xwork_artifact_summary *)pList->pItems;
    if ( pItems ) {
        for ( i = 0u; i < pList->iCount; ++i ) {
            xwork_artifact_summary_reset(&pItems[i]);
        }
        free(pItems);
    }

    xwork_artifact_summary_list_init(pList);
}

void xwork_artifact_summary_query_init(xwork_artifact_summary_query *pQuery)
{
    if ( pQuery ) {
        memset(pQuery, 0, sizeof(*pQuery));
    }
}

static bool xwork__artifact_summary_has_prefix(const char *sText, const char *sPrefix)
{
    size_t iPrefixLen;

    if ( !sPrefix || !sPrefix[0] ) {
        return true;
    }
    if ( !sText ) {
        return false;
    }
    iPrefixLen = strlen(sPrefix);
    return strncmp(sText, sPrefix, iPrefixLen) == 0;
}

static bool xwork__artifact_summary_matches_query(
    const xwork_artifact_summary *pSummary,
    const xwork_artifact_summary_query *pQuery
)
{
    if ( !pSummary || !pQuery ) {
        return true;
    }
    if ( pQuery->bHasKind && pSummary->eKind != pQuery->eKind ) {
        return false;
    }
    if ( pQuery->bHasOutputClass && pSummary->eOutputClass != pQuery->eOutputClass ) {
        return false;
    }
    if ( pQuery->sOutputRole && pQuery->sOutputRole[0] ) {
        if ( !pSummary->sOutputRole || strcmp(pSummary->sOutputRole, pQuery->sOutputRole) != 0 ) {
            return false;
        }
    }
    if ( !xwork__artifact_summary_has_prefix(pSummary->sOutputRole, pQuery->sOutputRolePrefix) ) {
        return false;
    }
    if ( pQuery->bHasReportClass && pSummary->eReportClass != pQuery->eReportClass ) {
        return false;
    }
    if ( pQuery->sReportSubjectRef && pQuery->sReportSubjectRef[0] ) {
        if ( !pSummary->sReportSubjectRef ||
             strcmp(pSummary->sReportSubjectRef, pQuery->sReportSubjectRef) != 0 ) {
            return false;
        }
    }
    if ( !xwork__artifact_summary_has_prefix(
             pSummary->sReportSubjectRef,
             pQuery->sReportSubjectRefPrefix
         ) ) {
        return false;
    }
    if ( pQuery->sArtifactName && pQuery->sArtifactName[0] ) {
        if ( !pSummary->sName || strcmp(pSummary->sName, pQuery->sArtifactName) != 0 ) {
            return false;
        }
    }
    if ( !xwork__artifact_summary_has_prefix(pSummary->sName, pQuery->sNamePrefix) ) {
        return false;
    }
    if ( pQuery->sMimeType && pQuery->sMimeType[0] ) {
        if ( !pSummary->sMimeType || strcmp(pSummary->sMimeType, pQuery->sMimeType) != 0 ) {
            return false;
        }
    }
    if ( !xwork__artifact_summary_has_prefix(pSummary->sMimeType, pQuery->sMimeTypePrefix) ) {
        return false;
    }
    if ( pQuery->sStorageRef && pQuery->sStorageRef[0] ) {
        if ( !pSummary->sStorageRef || strcmp(pSummary->sStorageRef, pQuery->sStorageRef) != 0 ) {
            return false;
        }
    }
    if ( !xwork__artifact_summary_has_prefix(pSummary->sStorageRef, pQuery->sStorageRefPrefix) ) {
        return false;
    }
    if ( pQuery->bRequireExitCode && !pSummary->bHasExitCode ) {
        return false;
    }
    if ( pQuery->bHasExitCodeValue ) {
        if ( !pSummary->bHasExitCode || pSummary->iExitCode != pQuery->iExitCode ) {
            return false;
        }
    }
    if ( pQuery->bHasAfterSequence && pSummary->iSequence <= pQuery->iAfterSequence ) {
        return false;
    }
    if ( pQuery->bHasMinSequence && pSummary->iSequence < pQuery->iMinSequence ) {
        return false;
    }
    if ( pQuery->bHasMaxSequence && pSummary->iSequence > pQuery->iMaxSequence ) {
        return false;
    }
    return true;
}

static xwork_status xwork__artifact_summary_copy(
    xwork_artifact_summary *pDst,
    const xwork_artifact_summary *pSrc
)
{
    xwork_status iStatus;

    if ( !pDst || !pSrc ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_artifact_summary_reset(pDst);
    iStatus = xwork__replace_cstr((char **)&pDst->sArtifactId, pSrc->sArtifactId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pDst->sName, pSrc->sName);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pDst->sMimeType, pSrc->sMimeType);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pDst->sStorageRef, pSrc->sStorageRef);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pDst->sSummary, pSrc->sSummary);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pDst->sOutputRole, pSrc->sOutputRole);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pDst->sReportSubjectRef, pSrc->sReportSubjectRef);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pDst->sPatchApplyResultJson, pSrc->sPatchApplyResultJson);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pDst->sPatchFileSummaryJson, pSrc->sPatchFileSummaryJson);
    if ( iStatus != XWORK_OK ) return iStatus;
    pDst->eKind = pSrc->eKind;
    pDst->eOutputClass = pSrc->eOutputClass;
    pDst->eReportClass = pSrc->eReportClass;
    pDst->bHasContentStats = pSrc->bHasContentStats;
    pDst->iContentByteCount = pSrc->iContentByteCount;
    pDst->iContentLineCount = pSrc->iContentLineCount;
    pDst->bHasPatchStats = pSrc->bHasPatchStats;
    pDst->iPatchFileCount = pSrc->iPatchFileCount;
    pDst->iPatchHunkCount = pSrc->iPatchHunkCount;
    pDst->iPatchAddedLineCount = pSrc->iPatchAddedLineCount;
    pDst->iPatchDeletedLineCount = pSrc->iPatchDeletedLineCount;
    pDst->bHasCommandIoStats = pSrc->bHasCommandIoStats;
    pDst->iStdoutByteCount = pSrc->iStdoutByteCount;
    pDst->iStderrByteCount = pSrc->iStderrByteCount;
    pDst->bStdoutTruncated = pSrc->bStdoutTruncated;
    pDst->bStderrTruncated = pSrc->bStderrTruncated;
    pDst->bHasExitCode = pSrc->bHasExitCode;
    pDst->iExitCode = pSrc->iExitCode;
    pDst->iSequence = pSrc->iSequence;
    return XWORK_OK;
}

static void xwork__artifact_summary_sort_by_sequence(
    xwork_artifact_summary *pItems,
    size_t iCount
)
{
    size_t i;

    if ( !pItems || iCount < 2u ) {
        return;
    }

    for ( i = 1u; i < iCount; ++i ) {
        size_t j = i;
        xwork_artifact_summary tValue = pItems[i];

        while ( j > 0u && tValue.iSequence < pItems[j - 1u].iSequence ) {
            pItems[j] = pItems[j - 1u];
            --j;
        }
        pItems[j] = tValue;
    }
}

void xwork_run_index_entry_init(xwork_run_index_entry *pEntry)
{
    if ( !pEntry ) {
        return;
    }

    memset(pEntry, 0, sizeof(*pEntry));
    xwork_run_summary_init(&pEntry->tSummary);
    xwork_approval_request_init(&pEntry->tLastApprovalRequest);
    xwork_event_init(&pEntry->tLastEvent);
    xwork_checkpoint_init(&pEntry->tLastCheckpoint);
    xwork_artifact_init(&pEntry->tLastArtifact);
}

void xwork_run_index_entry_reset(xwork_run_index_entry *pEntry)
{
    if ( !pEntry ) {
        return;
    }

    xwork_run_summary_reset(&pEntry->tSummary);
    xwork_approval_request_reset(&pEntry->tLastApprovalRequest);
    xwork_event_reset(&pEntry->tLastEvent);
    xwork_checkpoint_reset(&pEntry->tLastCheckpoint);
    xwork_artifact_reset(&pEntry->tLastArtifact);
    xwork_run_index_entry_init(pEntry);
}

void xwork_run_index_list_init(xwork_run_index_list *pList)
{
    if ( pList ) {
        memset(pList, 0, sizeof(*pList));
    }
}

void xwork_run_index_list_reset(xwork_run_index_list *pList)
{
    xwork_run_index_entry *pItems;
    size_t i;

    if ( !pList ) {
        return;
    }

    pItems = (xwork_run_index_entry *)pList->pItems;
    if ( pItems ) {
        for ( i = 0u; i < pList->iCount; ++i ) {
            xwork_run_index_entry_reset(&pItems[i]);
        }
        free(pItems);
    }
    free((char *)pList->sNextAfterRunId);

    xwork_run_index_list_init(pList);
}

void xwork_run_index_query_init(xwork_run_index_query *pQuery)
{
    if ( pQuery ) {
        memset(pQuery, 0, sizeof(*pQuery));
        pQuery->eState = XWORK_RUN_CREATED;
        pQuery->eAutonomy = XWORK_AUTONOMY_SEMI_AUTO;
        pQuery->eLastApprovalState = XWORK_APPROVAL_PENDING;
        pQuery->eLastEventKind = XWORK_EVENT_NONE;
        pQuery->eLastCheckpointKind = XWORK_CHECKPOINT_MANUAL;
        pQuery->eSort = XWORK_RUN_INDEX_SORT_RUN_ID_ASC;
    }
}

void xwork_memory_context_init(xwork_memory_context *pContext)
{
    if ( pContext ) {
        memset(pContext, 0, sizeof(*pContext));
    }
}

void xwork_memory_context_reset(xwork_memory_context *pContext)
{
    if ( !pContext ) {
        return;
    }

    xwork__free_cstr((char **)&pContext->sText);
    xwork_memory_context_init(pContext);
}

void xwork_session_policy_init(xwork_session_policy *pPolicy)
{
    if ( pPolicy ) {
        memset(pPolicy, 0, sizeof(*pPolicy));
        pPolicy->bEnableAutoCompact = true;
        pPolicy->fCompactTriggerRatio = 0.85;
        pPolicy->iCompactTriggerTurns = 16u;
        pPolicy->iKeepRecentTurns = 8u;
        pPolicy->bKeepActiveToolChain = true;
        pPolicy->eCompactStrategy = XWORK_SESSION_COMPACT_SUMMARIZE;
    }
}

void xwork_xllm_transport_options_init(xwork_xllm_transport_options *pOptions)
{
    if ( pOptions ) {
        memset(pOptions, 0, sizeof(*pOptions));
        pOptions->eProxyKind = XWORK_XLLM_PROXY_UNSPECIFIED;
    }
}

void xwork_xllm_profile_options_init(xwork_xllm_profile_options *pOptions)
{
    if ( pOptions ) {
        memset(pOptions, 0, sizeof(*pOptions));
    }
}

void xwork_xllm_bootstrap_options_init(xwork_xllm_bootstrap_options *pOptions)
{
    if ( pOptions ) {
        memset(pOptions, 0, sizeof(*pOptions));
        pOptions->eDebugMode = XWORK_XLLM_DEBUG_NONE;
        pOptions->eRedactMode = XWORK_XLLM_REDACT_DEFAULT;
        xwork_xllm_transport_options_init(&pOptions->tTransportDefaults);
    }
}

static bool xwork__same_cstr(const char *sLeft, const char *sRight)
{
    if ( sLeft == sRight ) {
        return true;
    }
    if ( !sLeft || !sRight ) {
        return false;
    }
    return strcmp(sLeft, sRight) == 0;
}

static int xwork__register_builtin_xllm_adapter(
    xllm_runtime *pLlmRuntime,
    const char *sAdapter
)
{
    if ( !pLlmRuntime || !sAdapter || !sAdapter[0] ) {
        return XRT_NET_ERROR;
    }

    if ( strcmp(sAdapter, XWORK_XLLM_ADAPTER_OPENAI_COMPAT) == 0 ) {
        return xllm_register_openai_compat_adapter(pLlmRuntime);
    }
    if ( strcmp(sAdapter, XWORK_XLLM_ADAPTER_GLM_NATIVE) == 0 ) {
        return xllm_register_glm_native_adapter(pLlmRuntime);
    }
    if ( strcmp(sAdapter, XWORK_XLLM_ADAPTER_MINIMAX_NATIVE) == 0 ) {
        return xllm_register_minimax_native_adapter(pLlmRuntime);
    }
    if ( strcmp(sAdapter, XWORK_XLLM_ADAPTER_KIMI_NATIVE) == 0 ) {
        return xllm_register_kimi_native_adapter(pLlmRuntime);
    }
    if ( strcmp(sAdapter, XWORK_XLLM_ADAPTER_GEMINI_NATIVE) == 0 ) {
        return xllm_register_gemini_native_adapter(pLlmRuntime);
    }
    if ( strcmp(sAdapter, XWORK_XLLM_ADAPTER_VERTEX_GEMINI_NATIVE) == 0 ) {
        return xllm_register_vertex_gemini_native_adapter(pLlmRuntime);
    }
    if ( strcmp(sAdapter, XWORK_XLLM_ADAPTER_QWEN_NATIVE) == 0 ) {
        return xllm_register_qwen_native_adapter(pLlmRuntime);
    }
    if ( strcmp(sAdapter, XWORK_XLLM_ADAPTER_DOUBAO_NATIVE) == 0 ) {
        return xllm_register_doubao_native_adapter(pLlmRuntime);
    }
    if ( strcmp(sAdapter, XWORK_XLLM_ADAPTER_ANTHROPIC_NATIVE) == 0 ) {
        return xllm_register_anthropic_native_adapter(pLlmRuntime);
    }
    if ( strcmp(sAdapter, XWORK_XLLM_ADAPTER_OLLAMA_NATIVE) == 0 ) {
        return xllm_register_ollama_native_adapter(pLlmRuntime);
    }

    return XRT_NET_ERROR;
}

static bool xwork__bootstrap_has_duplicate_profile_id(
    const xwork_xllm_bootstrap_options *pBootstrap,
    size_t iIndex
)
{
    size_t i;

    if ( !pBootstrap || !pBootstrap->pProfiles ||
         iIndex >= pBootstrap->iProfileCount ) {
        return false;
    }

    for ( i = 0u; i < iIndex; ++i ) {
        if ( xwork__same_cstr(
                 pBootstrap->pProfiles[i].sProfileId,
                 pBootstrap->pProfiles[iIndex].sProfileId
             ) ) {
            return true;
        }
    }

    return false;
}

static bool xwork__bootstrap_has_adapter_before(
    const xwork_xllm_bootstrap_options *pBootstrap,
    size_t iIndex
)
{
    size_t i;

    if ( !pBootstrap || !pBootstrap->pProfiles ||
         iIndex >= pBootstrap->iProfileCount ) {
        return false;
    }

    for ( i = 0u; i < iIndex; ++i ) {
        if ( xwork__same_cstr(
                 pBootstrap->pProfiles[i].sAdapter,
                 pBootstrap->pProfiles[iIndex].sAdapter
             ) ) {
            return true;
        }
    }

    return false;
}

static xwork_status xwork__fill_bootstrap_transport_defaults(
    const xwork_xllm_transport_options *pOptions,
    xllm_runtime_options *pRuntimeOptions
)
{
    if ( !pOptions || !pRuntimeOptions ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    if ( pOptions->bSetConnectTimeoutMs ) {
        pRuntimeOptions->tTransportDefaults.tConnectTimeoutMs.bSet = true;
        if ( pOptions->iConnectTimeoutMs > 0xFFFFFFFFu ) {
            pRuntimeOptions->tTransportDefaults.tConnectTimeoutMs.iValue = 0xFFFFFFFFu;
        } else {
            pRuntimeOptions->tTransportDefaults.tConnectTimeoutMs.iValue =
                (uint32)pOptions->iConnectTimeoutMs;
        }
    }
    if ( pOptions->bSetReadTimeoutMs ) {
        pRuntimeOptions->tTransportDefaults.tReadTimeoutMs.bSet = true;
        if ( pOptions->iReadTimeoutMs > 0xFFFFFFFFu ) {
            pRuntimeOptions->tTransportDefaults.tReadTimeoutMs.iValue = 0xFFFFFFFFu;
        } else {
            pRuntimeOptions->tTransportDefaults.tReadTimeoutMs.iValue =
                (uint32)pOptions->iReadTimeoutMs;
        }
    }
    if ( pOptions->bSetVerifyPeer ) {
        pRuntimeOptions->tTransportDefaults.tVerifyPeer.bSet = true;
        pRuntimeOptions->tTransportDefaults.tVerifyPeer.bValue = pOptions->bVerifyPeer;
    }

    switch ( pOptions->eProxyKind ) {
    case XWORK_XLLM_PROXY_UNSPECIFIED:
        pRuntimeOptions->tTransportDefaults.eProxyKind = XLLM_PROXY_UNSPECIFIED;
        break;
    case XWORK_XLLM_PROXY_NONE:
        pRuntimeOptions->tTransportDefaults.eProxyKind = XLLM_PROXY_NONE;
        break;
    case XWORK_XLLM_PROXY_SOCKS5:
        pRuntimeOptions->tTransportDefaults.eProxyKind = XLLM_PROXY_SOCKS5;
        break;
    case XWORK_XLLM_PROXY_HTTP_CONNECT:
        pRuntimeOptions->tTransportDefaults.eProxyKind = XLLM_PROXY_HTTP_CONNECT;
        break;
    default:
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    pRuntimeOptions->tTransportDefaults.sProxyHost = pOptions->sProxyHost;
    if ( pOptions->bSetProxyPort ) {
        pRuntimeOptions->tTransportDefaults.tProxyPort.bSet = true;
        if ( pOptions->iProxyPort > 0xFFFFFFFFu ) {
            pRuntimeOptions->tTransportDefaults.tProxyPort.iValue = 0xFFFFFFFFu;
        } else {
            pRuntimeOptions->tTransportDefaults.tProxyPort.iValue =
                (uint32)pOptions->iProxyPort;
        }
    }
    pRuntimeOptions->tTransportDefaults.sProxyUser = pOptions->sProxyUser;
    pRuntimeOptions->tTransportDefaults.sProxyPass = pOptions->sProxyPass;
    pRuntimeOptions->tTransportDefaults.sCaBundlePath = pOptions->sCaBundlePath;
    pRuntimeOptions->tTransportDefaults.sClientCertPath = pOptions->sClientCertPath;
    pRuntimeOptions->tTransportDefaults.sClientKeyPath = pOptions->sClientKeyPath;
    return XWORK_OK;
}

static xwork_status xwork__fill_bootstrap_runtime_defaults(
    const xwork_xllm_bootstrap_options *pBootstrap,
    xllm_runtime_options *pRuntimeOptions
)
{
    if ( !pBootstrap || !pRuntimeOptions ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    switch ( pBootstrap->eDebugMode ) {
    case XWORK_XLLM_DEBUG_NONE:
        pRuntimeOptions->eDebugMode = XLLM_DEBUG_NONE;
        break;
    case XWORK_XLLM_DEBUG_HEADERS:
        pRuntimeOptions->eDebugMode = XLLM_DEBUG_HEADERS;
        break;
    case XWORK_XLLM_DEBUG_BODY:
        pRuntimeOptions->eDebugMode = XLLM_DEBUG_BODY;
        break;
    case XWORK_XLLM_DEBUG_WIRE:
        pRuntimeOptions->eDebugMode = XLLM_DEBUG_WIRE;
        break;
    default:
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    switch ( pBootstrap->eRedactMode ) {
    case XWORK_XLLM_REDACT_DEFAULT:
        pRuntimeOptions->eRedactMode = XLLM_REDACT_DEFAULT;
        break;
    case XWORK_XLLM_REDACT_OFF:
        pRuntimeOptions->eRedactMode = XLLM_REDACT_OFF;
        break;
    case XWORK_XLLM_REDACT_STRICT:
        pRuntimeOptions->eRedactMode = XLLM_REDACT_STRICT;
        break;
    default:
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    return xwork__fill_bootstrap_transport_defaults(
        &pBootstrap->tTransportDefaults,
        pRuntimeOptions
    );
}

static void xwork__fill_bootstrap_auth(
    const xwork_xllm_profile_options *pOptions,
    xllm_profile *pProfile
)
{
    if ( !pOptions || !pProfile || !pOptions->sApiKey || !pOptions->sApiKey[0] ) {
        return;
    }

    if ( xwork__same_cstr(pOptions->sAdapter, XWORK_XLLM_ADAPTER_ANTHROPIC_NATIVE) ||
         xwork__same_cstr(pOptions->sAdapter, XWORK_XLLM_ADAPTER_GEMINI_NATIVE) ||
         xwork__same_cstr(pOptions->sAdapter, XWORK_XLLM_ADAPTER_VERTEX_GEMINI_NATIVE) ) {
        pProfile->tAuth.eKind = XLLM_AUTH_API_KEY_HEADER;
        pProfile->tAuth.sSecret = pOptions->sApiKey;
        return;
    }

    pProfile->tAuth.eKind = XLLM_AUTH_BEARER;
    pProfile->tAuth.sSecret = pOptions->sApiKey;
    pProfile->tAuth.sScheme = "Bearer";
}

static void xwork__fill_bootstrap_profile(
    const xwork_xllm_profile_options *pOptions,
    xllm_profile *pProfile
)
{
    xllm_profile_init(pProfile);
    pProfile->sId = pOptions->sProfileId;
    pProfile->sName = pOptions->sDisplayName ? pOptions->sDisplayName : pOptions->sProfileId;
    pProfile->sProvider = pOptions->sProvider ? pOptions->sProvider : pOptions->sAdapter;
    pProfile->sAdapter = pOptions->sAdapter;
    pProfile->sBaseUrl = pOptions->sBaseUrl;
    pProfile->tProviderOptions.sOpenAIOrganizationId = pOptions->sOpenAIOrganizationId;
    pProfile->tProviderOptions.sOpenAIProjectId = pOptions->sOpenAIProjectId;
    pProfile->tProviderOptions.sAnthropicApiVersion = pOptions->sAnthropicApiVersion;
    pProfile->tProviderOptions.psAnthropicBetaHeaders = pOptions->psAnthropicBetaHeaders;
    pProfile->tProviderOptions.iAnthropicBetaHeaderCount =
        pOptions->iAnthropicBetaHeaderCount;
    pProfile->tModels.tText.sModelId = pOptions->sModelId;
    pProfile->tModels.tText.eCapMode = XLLM_CAP_MODE_EXACT;
    pProfile->tModels.tText.tCaps.uFlags =
        XLLM_CAP_TEXT_IN |
        XLLM_CAP_TEXT_OUT |
        XLLM_CAP_TOOL_CALL_OUT |
        XLLM_CAP_TOOL_RESULT_IN;
    if ( pOptions->iMaxOutputTokens > 0u ) {
        pProfile->tDefaults.tGeneration.tMaxOutputTokens.bSet = true;
        if ( pOptions->iMaxOutputTokens > 0xFFFFFFFFu ) {
            pProfile->tDefaults.tGeneration.tMaxOutputTokens.iValue = 0xFFFFFFFFu;
        } else {
            pProfile->tDefaults.tGeneration.tMaxOutputTokens.iValue =
                (uint32)pOptions->iMaxOutputTokens;
        }
    }
    xwork__fill_bootstrap_auth(pOptions, pProfile);
}

static xwork_status xwork__bootstrap_llm_runtime(
    const xwork_xllm_bootstrap_options *pBootstrap,
    xllm_runtime **ppLlmRuntime
)
{
    xllm_runtime_options tLlmRuntimeOptions;
    xllm_runtime *pLlmRuntime = NULL;
    xwork_status iStatus;
    size_t i;

    if ( !pBootstrap || !ppLlmRuntime ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    *ppLlmRuntime = NULL;
    xllm_runtime_options_init(&tLlmRuntimeOptions);
    iStatus = xwork__fill_bootstrap_runtime_defaults(
        pBootstrap,
        &tLlmRuntimeOptions
    );
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    if ( xllm_runtime_create(&tLlmRuntimeOptions, &pLlmRuntime) != XRT_NET_OK ||
         !pLlmRuntime ) {
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }

    for ( i = 0u; i < pBootstrap->iProfileCount; ++i ) {
        const xwork_xllm_profile_options *pProfileOptions = &pBootstrap->pProfiles[i];
        xllm_profile tProfile;

        if ( !pProfileOptions->sProfileId || !pProfileOptions->sProfileId[0] ||
             !pProfileOptions->sAdapter || !pProfileOptions->sAdapter[0] ||
             !pProfileOptions->sModelId || !pProfileOptions->sModelId[0] ) {
            xllm_runtime_destroy(pLlmRuntime);
            return XWORK_ERROR_INVALID_ARGUMENT;
        }
        if ( xwork__bootstrap_has_duplicate_profile_id(pBootstrap, i) ) {
            xllm_runtime_destroy(pLlmRuntime);
            return XWORK_ERROR_ALREADY_EXISTS;
        }
        if ( !xwork__bootstrap_has_adapter_before(pBootstrap, i) &&
             xwork__register_builtin_xllm_adapter(
                 pLlmRuntime,
                 pProfileOptions->sAdapter
             ) != XRT_NET_OK ) {
            xllm_runtime_destroy(pLlmRuntime);
            return XWORK_ERROR_UNSUPPORTED;
        }

        xwork__fill_bootstrap_profile(pProfileOptions, &tProfile);
        if ( xllm_register_profile(pLlmRuntime, &tProfile) != XRT_NET_OK ) {
            xllm_runtime_destroy(pLlmRuntime);
            return XWORK_ERROR_EXTERNAL_FAILURE;
        }
    }

    *ppLlmRuntime = pLlmRuntime;
    return XWORK_OK;
}

void xwork_event_init(xwork_event *pEvent)
{
    if ( pEvent ) {
        memset(pEvent, 0, sizeof(*pEvent));
    }
}

void xwork_event_reset(xwork_event *pEvent)
{
    if ( !pEvent ) {
        return;
    }

    xwork__free_cstr((char **)&pEvent->sEventId);
    xwork__free_cstr((char **)&pEvent->sRunId);
    xwork__free_cstr((char **)&pEvent->sToolId);
    xwork__free_cstr((char **)&pEvent->sApprovalRequestId);
    xwork__free_cstr((char **)&pEvent->sCheckpointId);
    xwork__free_cstr((char **)&pEvent->sSummary);
    xwork_event_init(pEvent);
}

void xwork_run_step_init(xwork_run_step *pStep)
{
    if ( pStep ) {
        memset(pStep, 0, sizeof(*pStep));
        pStep->eKind = XWORK_RUN_STEP_RUN_STATE;
        pStep->eEventKind = XWORK_EVENT_NONE;
        pStep->eCheckpointKind = XWORK_CHECKPOINT_MANUAL;
        pStep->eRunState = XWORK_RUN_CREATED;
    }
}

void xwork_run_step_reset(xwork_run_step *pStep)
{
    if ( !pStep ) {
        return;
    }

    xwork__free_cstr((char **)&pStep->sStepId);
    xwork__free_cstr((char **)&pStep->sRunId);
    xwork__free_cstr((char **)&pStep->sEventId);
    xwork__free_cstr((char **)&pStep->sToolId);
    xwork__free_cstr((char **)&pStep->sApprovalRequestId);
    xwork__free_cstr((char **)&pStep->sCheckpointId);
    xwork__free_cstr((char **)&pStep->sSummary);
    xwork_run_step_init(pStep);
}

void xwork_run_step_list_init(xwork_run_step_list *pList)
{
    if ( pList ) {
        memset(pList, 0, sizeof(*pList));
    }
}

void xwork_run_step_list_reset(xwork_run_step_list *pList)
{
    xwork_run_step *pItems;
    size_t i;

    if ( !pList ) {
        return;
    }

    pItems = (xwork_run_step *)pList->pItems;
    if ( pItems ) {
        for ( i = 0u; i < pList->iCount; ++i ) {
            xwork_run_step_reset(&pItems[i]);
        }
        free(pItems);
    }

    xwork_run_step_list_init(pList);
}

void xwork_run_step_query_init(xwork_run_step_query *pQuery)
{
    if ( pQuery ) {
        memset(pQuery, 0, sizeof(*pQuery));
        pQuery->eKind = XWORK_RUN_STEP_RUN_STATE;
        pQuery->eEventKind = XWORK_EVENT_NONE;
        pQuery->eCheckpointKind = XWORK_CHECKPOINT_MANUAL;
    }
}

void xwork_approval_request_init(xwork_approval_request *pRequest)
{
    if ( pRequest ) {
        memset(pRequest, 0, sizeof(*pRequest));
        pRequest->eRiskLevel = XWORK_RISK_LOW;
        pRequest->eState = XWORK_APPROVAL_PENDING;
    }
}

void xwork_approval_request_reset(xwork_approval_request *pRequest)
{
    if ( !pRequest ) {
        return;
    }

    xwork__free_cstr((char **)&pRequest->sRequestId);
    xwork__free_cstr((char **)&pRequest->sRunId);
    xwork__free_cstr((char **)&pRequest->sToolId);
    xwork__free_cstr((char **)&pRequest->sReason);
    xwork__free_cstr((char **)&pRequest->sScope);
    xwork__free_cstr((char **)&pRequest->sActionSummary);
    xwork_approval_request_init(pRequest);
}

void xwork_checkpoint_init(xwork_checkpoint *pCheckpoint)
{
    if ( pCheckpoint ) {
        memset(pCheckpoint, 0, sizeof(*pCheckpoint));
        pCheckpoint->eKind = XWORK_CHECKPOINT_MANUAL;
    }
}

void xwork_checkpoint_reset(xwork_checkpoint *pCheckpoint)
{
    if ( !pCheckpoint ) {
        return;
    }

    xwork__free_cstr((char **)&pCheckpoint->sCheckpointId);
    xwork__free_cstr((char **)&pCheckpoint->sRunId);
    xwork__free_cstr((char **)&pCheckpoint->sPendingStep);
    xwork__free_cstr((char **)&pCheckpoint->sSessionStateRef);
    xwork__free_cstr((char **)&pCheckpoint->sToolOutputsRef);
    xwork__free_cstr((char **)&pCheckpoint->sWorkspaceSnapshotRef);
    xwork__free_cstr((char **)&pCheckpoint->sArtifactRefs);
    xwork_checkpoint_init(pCheckpoint);
}

void xwork_persistence_backend_init(xwork_persistence_backend *pBackend)
{
    if ( pBackend ) {
        memset(pBackend, 0, sizeof(*pBackend));
    }
}

static void xwork__run_snapshot_free_cstr(const char **psText)
{
    if ( psText && *psText ) {
        free((void *)*psText);
        *psText = NULL;
    }
}

static xwork_status xwork__run_snapshot_replace_cstr(const char **psTarget, const char *sText)
{
    char *sCopy = NULL;

    if ( !psTarget ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    if ( sText ) {
        sCopy = xwork__dup_cstr(sText);
        if ( !sCopy ) {
            return XWORK_ERROR_NO_MEMORY;
        }
    }

    xwork__run_snapshot_free_cstr(psTarget);
    *psTarget = sCopy;
    return XWORK_OK;
}

static xwork_status xwork__event_copy(
    xwork_event *pTarget,
    const xwork_event *pSource
)
{
    xwork_status iStatus;

    if ( !pTarget || !pSource ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_event_reset(pTarget);

    iStatus = xwork__replace_cstr((char **)&pTarget->sEventId, pSource->sEventId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pTarget->sRunId, pSource->sRunId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pTarget->sToolId, pSource->sToolId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr(
        (char **)&pTarget->sApprovalRequestId,
        pSource->sApprovalRequestId
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr(
        (char **)&pTarget->sCheckpointId,
        pSource->sCheckpointId
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pTarget->sSummary, pSource->sSummary);
    if ( iStatus != XWORK_OK ) return iStatus;

    pTarget->eKind = pSource->eKind;
    pTarget->eRunState = pSource->eRunState;
    pTarget->iSequence = pSource->iSequence;
    return XWORK_OK;
}

static xwork_status xwork__approval_request_copy(
    xwork_approval_request *pTarget,
    const xwork_approval_request *pSource
)
{
    xwork_status iStatus;

    if ( !pTarget || !pSource ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_approval_request_reset(pTarget);

    iStatus = xwork__replace_cstr((char **)&pTarget->sRequestId, pSource->sRequestId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pTarget->sRunId, pSource->sRunId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pTarget->sToolId, pSource->sToolId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pTarget->sReason, pSource->sReason);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pTarget->sScope, pSource->sScope);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr(
        (char **)&pTarget->sActionSummary,
        pSource->sActionSummary
    );
    if ( iStatus != XWORK_OK ) return iStatus;

    pTarget->eRiskLevel = pSource->eRiskLevel;
    pTarget->eState = pSource->eState;
    return XWORK_OK;
}

static xwork_status xwork__checkpoint_copy(
    xwork_checkpoint *pTarget,
    const xwork_checkpoint *pSource
)
{
    xwork_status iStatus;

    if ( !pTarget || !pSource ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_checkpoint_reset(pTarget);

    iStatus = xwork__replace_cstr(
        (char **)&pTarget->sCheckpointId,
        pSource->sCheckpointId
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pTarget->sRunId, pSource->sRunId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pTarget->sPendingStep, pSource->sPendingStep);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr(
        (char **)&pTarget->sSessionStateRef,
        pSource->sSessionStateRef
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr(
        (char **)&pTarget->sToolOutputsRef,
        pSource->sToolOutputsRef
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr(
        (char **)&pTarget->sWorkspaceSnapshotRef,
        pSource->sWorkspaceSnapshotRef
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pTarget->sArtifactRefs, pSource->sArtifactRefs);
    if ( iStatus != XWORK_OK ) return iStatus;

    pTarget->eKind = pSource->eKind;
    pTarget->eRunState = pSource->eRunState;
    pTarget->iSequence = pSource->iSequence;
    return XWORK_OK;
}

static xwork_run_step_kind xwork__run_step_kind_from_event(xwork_event_kind eKind)
{
    switch ( eKind ) {
        case XWORK_EVENT_MEMORY_CONTEXT_ATTACHED:
        case XWORK_EVENT_MEMORY_RECORD_INGESTED:
            return XWORK_RUN_STEP_MEMORY;
        case XWORK_EVENT_MODEL_TURN_STARTED:
        case XWORK_EVENT_MODEL_TURN_COMPLETED:
            return XWORK_RUN_STEP_MODEL_TURN;
        case XWORK_EVENT_SESSION_COMPACTED:
            return XWORK_RUN_STEP_CHECKPOINT;
        case XWORK_EVENT_TOOL_CALL_REQUESTED:
            return XWORK_RUN_STEP_TOOL_CALL;
        case XWORK_EVENT_APPROVAL_REQUESTED:
        case XWORK_EVENT_APPROVAL_RESOLVED:
            return XWORK_RUN_STEP_APPROVAL;
        case XWORK_EVENT_TOOL_EXEC_STARTED:
        case XWORK_EVENT_TOOL_EXEC_COMPLETED:
            return XWORK_RUN_STEP_TOOL_EXEC;
        case XWORK_EVENT_ARTIFACT_EMITTED:
            return XWORK_RUN_STEP_ARTIFACT;
        case XWORK_EVENT_RETRY_SCHEDULED:
            return XWORK_RUN_STEP_RETRY;
        case XWORK_EVENT_AGENT_SPAWNED:
        case XWORK_EVENT_AGENT_STARTED:
        case XWORK_EVENT_AGENT_PAUSED:
        case XWORK_EVENT_AGENT_COMPLETED:
        case XWORK_EVENT_AGENT_FAILED:
        case XWORK_EVENT_AGENT_CANCELLED:
        case XWORK_EVENT_TASK_SCHEDULED:
        case XWORK_EVENT_TASK_STARTED:
        case XWORK_EVENT_TASK_JOINED:
        case XWORK_EVENT_TASK_BLOCKED:
        case XWORK_EVENT_TASK_UNBLOCKED:
        case XWORK_EVENT_TASK_COMPLETED:
        case XWORK_EVENT_TASK_FAILED:
        case XWORK_EVENT_TASK_CANCELLED:
        case XWORK_EVENT_HANDOFF_REQUESTED:
        case XWORK_EVENT_HANDOFF_ACCEPTED:
        case XWORK_EVENT_HANDOFF_REJECTED:
        case XWORK_EVENT_HANDOFF_COMPLETED:
            return XWORK_RUN_STEP_RUN_STATE;
        case XWORK_EVENT_CHECKPOINT_SAVED:
        case XWORK_EVENT_CHECKPOINT_LOADED:
            return XWORK_RUN_STEP_CHECKPOINT;
        case XWORK_EVENT_RUN_CREATED:
        case XWORK_EVENT_RUN_STARTED:
        case XWORK_EVENT_RUN_PAUSED:
        case XWORK_EVENT_RUN_COMPLETED:
        case XWORK_EVENT_RUN_CANCELLED:
        case XWORK_EVENT_RUN_FAILED:
        case XWORK_EVENT_NONE:
        default:
            return XWORK_RUN_STEP_RUN_STATE;
    }
}

xwork_status xwork__run_step_from_event(
    xwork_run_step *pStep,
    const xwork_event *pEvent,
    const xwork_checkpoint *pCheckpoint
)
{
    xwork_status iStatus;

    if ( !pStep || !pEvent ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_run_step_reset(pStep);
    iStatus = xwork__replace_cstr((char **)&pStep->sStepId, pEvent->sEventId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pStep->sRunId, pEvent->sRunId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pStep->sEventId, pEvent->sEventId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pStep->sToolId, pEvent->sToolId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr(
        (char **)&pStep->sApprovalRequestId,
        pEvent->sApprovalRequestId
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr(
        (char **)&pStep->sCheckpointId,
        pEvent->sCheckpointId
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pStep->sSummary, pEvent->sSummary);
    if ( iStatus != XWORK_OK ) return iStatus;

    pStep->eKind = xwork__run_step_kind_from_event(pEvent->eKind);
    pStep->eEventKind = pEvent->eKind;
    pStep->eCheckpointKind = pCheckpoint ? pCheckpoint->eKind : XWORK_CHECKPOINT_MANUAL;
    pStep->eRunState = pEvent->eRunState;
    pStep->iSequence = pEvent->iSequence;
    return XWORK_OK;
}

bool xwork__run_step_matches_query(
    const xwork_run_step *pStep,
    const xwork_run_step_query *pQuery
)
{
    if ( !pStep || !pQuery ) {
        return true;
    }
    if ( pQuery->bFilterKind && pStep->eKind != pQuery->eKind ) {
        return false;
    }
    if ( pQuery->bFilterEventKind && pStep->eEventKind != pQuery->eEventKind ) {
        return false;
    }
    if ( pQuery->bFilterCheckpointKind ) {
        if ( pStep->eKind != XWORK_RUN_STEP_CHECKPOINT ||
             pStep->eCheckpointKind != pQuery->eCheckpointKind ) {
            return false;
        }
    }
    if ( pQuery->bHasAfterSequence && pStep->iSequence <= pQuery->iAfterSequence ) {
        return false;
    }
    if ( pQuery->bHasMinSequence && pStep->iSequence < pQuery->iMinSequence ) {
        return false;
    }
    if ( pQuery->bHasMaxSequence && pStep->iSequence > pQuery->iMaxSequence ) {
        return false;
    }
    return true;
}

static xwork_status xwork__run_step_copy(
    xwork_run_step *pTarget,
    const xwork_run_step *pSource
)
{
    xwork_status iStatus;

    if ( !pTarget || !pSource ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_run_step_reset(pTarget);
    iStatus = xwork__replace_cstr((char **)&pTarget->sStepId, pSource->sStepId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pTarget->sRunId, pSource->sRunId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pTarget->sEventId, pSource->sEventId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pTarget->sToolId, pSource->sToolId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr(
        (char **)&pTarget->sApprovalRequestId,
        pSource->sApprovalRequestId
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr(
        (char **)&pTarget->sCheckpointId,
        pSource->sCheckpointId
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pTarget->sSummary, pSource->sSummary);
    if ( iStatus != XWORK_OK ) return iStatus;

    pTarget->eKind = pSource->eKind;
    pTarget->eEventKind = pSource->eEventKind;
    pTarget->eCheckpointKind = pSource->eCheckpointKind;
    pTarget->eRunState = pSource->eRunState;
    pTarget->iSequence = pSource->iSequence;
    return XWORK_OK;
}

xwork_status xwork__run_step_list_append(
    xwork_run_step_list *pList,
    const xwork_run_step *pStep
)
{
    xwork_run_step *pItems;
    xwork_status iStatus;

    if ( !pList || !pStep ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    pItems = (xwork_run_step *)realloc(
        (void *)pList->pItems,
        (pList->iCount + 1u) * sizeof(*pItems)
    );
    if ( !pItems ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    pList->pItems = pItems;
    xwork_run_step_init(&pItems[pList->iCount]);
    iStatus = xwork__run_step_copy(&pItems[pList->iCount], pStep);
    if ( iStatus != XWORK_OK ) {
        xwork_run_step_reset(&pItems[pList->iCount]);
        return iStatus;
    }
    pList->iCount += 1u;
    pList->iNextAfterSequence = pStep->iSequence;
    return XWORK_OK;
}

static xwork_status xwork__run_summary_copy(
    xwork_run_summary *pTarget,
    const xwork_run_summary *pSource
)
{
    xwork_status iStatus;

    if ( !pTarget || !pSource ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_run_summary_reset(pTarget);

    iStatus = xwork__replace_cstr((char **)&pTarget->sRunId, pSource->sRunId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pTarget->sParentRunId, pSource->sParentRunId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pTarget->sAgentId, pSource->sAgentId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pTarget->sTaskId, pSource->sTaskId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pTarget->sInstruction, pSource->sInstruction);
    if ( iStatus != XWORK_OK ) return iStatus;

    pTarget->eAutonomy = pSource->eAutonomy;
    pTarget->eState = pSource->eState;
    pTarget->iWorkspaceCount = pSource->iWorkspaceCount;
    return XWORK_OK;
}

static xwork_status xwork__run_snapshot_copy_workspace_ids(
    xwork_run_snapshot *pSnapshot,
    const char *const *psWorkspaceIds,
    size_t iWorkspaceCount
)
{
    char **psItems;
    size_t i;

    if ( !pSnapshot ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !psWorkspaceIds || iWorkspaceCount == 0u ) {
        pSnapshot->psWorkspaceIds = NULL;
        pSnapshot->iWorkspaceCount = 0u;
        return XWORK_OK;
    }

    psItems = (char **)calloc(iWorkspaceCount, sizeof(*psItems));
    if ( !psItems ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    for ( i = 0u; i < iWorkspaceCount; ++i ) {
        psItems[i] = xwork__dup_cstr(psWorkspaceIds[i]);
        if ( !psItems[i] ) {
            while ( i > 0u ) {
                --i;
                free(psItems[i]);
            }
            free(psItems);
            return XWORK_ERROR_NO_MEMORY;
        }
    }

    pSnapshot->psWorkspaceIds = (const char **)psItems;
    pSnapshot->iWorkspaceCount = iWorkspaceCount;
    return XWORK_OK;
}

void xwork_run_snapshot_init(xwork_run_snapshot *pSnapshot)
{
    if ( pSnapshot ) {
        memset(pSnapshot, 0, sizeof(*pSnapshot));
        pSnapshot->eAutonomy = XWORK_AUTONOMY_SEMI_AUTO;
        xwork_session_policy_init(&pSnapshot->tSessionPolicy);
        pSnapshot->eState = XWORK_RUN_CREATED;
        pSnapshot->eLastApprovalRiskLevel = XWORK_RISK_LOW;
        pSnapshot->eLastApprovalState = XWORK_APPROVAL_PENDING;
        pSnapshot->eLastCheckpointKind = XWORK_CHECKPOINT_MANUAL;
        pSnapshot->eLastCheckpointRunState = XWORK_RUN_CREATED;
    }
}

void xwork_run_snapshot_reset(xwork_run_snapshot *pSnapshot)
{
    size_t i;
    char **psItems;

    if ( !pSnapshot ) {
        return;
    }

    xwork__run_snapshot_reset_artifacts(pSnapshot);
    xwork__run_snapshot_free_cstr(&pSnapshot->sRunId);
    xwork__run_snapshot_free_cstr(&pSnapshot->sParentRunId);
    xwork__run_snapshot_free_cstr(&pSnapshot->sAgentId);
    xwork__run_snapshot_free_cstr(&pSnapshot->sTaskId);
    xwork__run_snapshot_free_cstr(&pSnapshot->sInstruction);
    xwork__run_snapshot_free_cstr(&pSnapshot->sLlmProfileId);
    xwork__run_snapshot_free_cstr(&pSnapshot->sSessionProfileId);
    xwork__run_snapshot_free_cstr(&pSnapshot->sSessionStateData);
    psItems = (char **)pSnapshot->psWorkspaceIds;
    if ( psItems ) {
        for ( i = 0u; i < pSnapshot->iWorkspaceCount; ++i ) {
            free(psItems[i]);
        }
        free(psItems);
    }
    xwork__run_snapshot_free_cstr(&pSnapshot->sLastOutputText);
    xwork__run_snapshot_free_cstr(&pSnapshot->sLastMemoryContextText);
    xwork__run_snapshot_free_cstr(&pSnapshot->sLastToolCallId);
    xwork__run_snapshot_free_cstr(&pSnapshot->sLastToolId);
    xwork__run_snapshot_free_cstr(&pSnapshot->sLastToolArgumentsJson);
    xwork__run_snapshot_free_cstr(&pSnapshot->sLastToolResultText);
    xwork__run_snapshot_free_cstr(&pSnapshot->sLastToolVisibleSummary);
    xwork__run_snapshot_free_cstr(&pSnapshot->sLastApprovalRequestId);
    xwork__run_snapshot_free_cstr(&pSnapshot->sLastApprovalToolId);
    xwork__run_snapshot_free_cstr(&pSnapshot->sLastApprovalReason);
    xwork__run_snapshot_free_cstr(&pSnapshot->sLastApprovalScope);
    xwork__run_snapshot_free_cstr(&pSnapshot->sLastApprovalActionSummary);
    xwork__run_snapshot_free_cstr(&pSnapshot->sLastCheckpointId);
    xwork__run_snapshot_free_cstr(&pSnapshot->sLastCheckpointPendingStep);
    xwork__run_snapshot_free_cstr(&pSnapshot->sLastCheckpointSessionStateRef);
    xwork__run_snapshot_free_cstr(&pSnapshot->sLastCheckpointToolOutputsRef);
    xwork__run_snapshot_free_cstr(&pSnapshot->sLastCheckpointWorkspaceSnapshotRef);
    xwork__run_snapshot_free_cstr(&pSnapshot->sLastCheckpointArtifactRefs);
    xwork_run_snapshot_init(pSnapshot);
}

void xwork_tool_result_init(xwork_tool_result *pResult)
{
    if ( pResult ) {
        memset(pResult, 0, sizeof(*pResult));
    }
}

void xwork_orchestrator_options_init(xwork_orchestrator_options *pOptions)
{
    if ( pOptions ) {
        memset(pOptions, 0, sizeof(*pOptions));
        pOptions->eModelStreamMode = XWORK_MODEL_STREAM_AUTO;
        pOptions->ePlannerMode = XWORK_PLANNER_OFF;
        pOptions->eToolChoiceMode = XWORK_TOOL_CHOICE_AUTO;
        pOptions->bAllowParallelToolCalls = false;
        pOptions->iMemorySearchMaxHits = 0u;
        pOptions->iMemoryContextMaxBlocks = 0u;
        pOptions->iMemoryContextMaxCharsPerHit = 0u;
        pOptions->iMemoryContextMaxTotalChars = 0u;
        pOptions->iMemoryContextPriority = 0;
        pOptions->bMemoryContextPinned = false;
        pOptions->uArtifactMemoryIngestKindMask = 0u;
        pOptions->uArtifactMemoryIngestOutputClassMask = 0u;
        pOptions->uArtifactMemoryIngestReportClassMask = 0u;
        pOptions->bIngestSensitiveArtifactsToMemory = false;
        pOptions->iMaxTurns = 4u;
        pOptions->iMaxRetries = 0u;
        pOptions->iRetryBackoffMs = 0u;
        pOptions->bAutoApprove = true;
    }
}

xwork_status xwork_runtime_create(
    const xwork_runtime_options *pOptions,
    xwork_runtime **ppRuntime
)
{
    xwork_runtime *pRuntime;
    xwork_status iStatus;

    if ( !ppRuntime ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( pOptions && pOptions->pLlmRuntime && pOptions->pLlmBootstrap ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    *ppRuntime = NULL;
    pRuntime = (xwork_runtime *)calloc(1u, sizeof(*pRuntime));
    if ( !pRuntime ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    xwork_host_services_init(&pRuntime->tHostServices);
    xwork_persistence_backend_init(&pRuntime->tPersistenceBackend);
    xwork_policy_options_init(&pRuntime->tPolicy);

    if ( pOptions ) {
        if ( pOptions->pLlmBootstrap ) {
            iStatus = xwork__bootstrap_llm_runtime(
                pOptions->pLlmBootstrap,
                &pRuntime->pLlmRuntime
            );
            if ( iStatus != XWORK_OK ) {
                free(pRuntime);
                return iStatus;
            }
            pRuntime->bOwnLlmRuntime = true;
        } else {
            pRuntime->pLlmRuntime = pOptions->pLlmRuntime;
        }
        pRuntime->pReplayEngine = pOptions->pReplayEngine;
        if ( pOptions->pHostServices ) {
            pRuntime->tHostServices = *pOptions->pHostServices;
        }
        if ( pOptions->pPersistenceBackend ) {
            pRuntime->tPersistenceBackend = *pOptions->pPersistenceBackend;
        }
        pRuntime->tPolicy = pOptions->tPolicy;
        pRuntime->pUserData = pOptions->pUserData;
    }

    *ppRuntime = pRuntime;
    return XWORK_OK;
}

void xwork_runtime_destroy(xwork_runtime *pRuntime)
{
    xwork_workspace *pWorkspace;
    xwork_tool_record *pTool;
    xwork_run *pRun;

    if ( !pRuntime ) {
        return;
    }

    while ( pRuntime->pRuns ) {
        pRun = pRuntime->pRuns;
        pRuntime->pRuns = pRun->pNext;
        xwork_run_destroy(pRun);
    }

    while ( pRuntime->pWorkspaces ) {
        pWorkspace = pRuntime->pWorkspaces;
        pRuntime->pWorkspaces = pWorkspace->pNext;
        xwork_workspace_destroy(pWorkspace);
    }

    while ( pRuntime->pTools ) {
        pTool = pRuntime->pTools;
        pRuntime->pTools = pTool->pNext;
        xwork__free_tool_record(pTool);
    }

    if ( pRuntime->bOwnLlmRuntime && pRuntime->pLlmRuntime ) {
        xllm_runtime_destroy(pRuntime->pLlmRuntime);
        pRuntime->pLlmRuntime = NULL;
    }

    free(pRuntime->sLastReplayHostOutputText);
    free(pRuntime->sLastReplayHostVisibleSummary);
    free(pRuntime);
}

xllm_runtime *xwork_runtime_get_llm_runtime(const xwork_runtime *pRuntime)
{
    return pRuntime ? pRuntime->pLlmRuntime : NULL;
}

const xwork_host_services *xwork_runtime_get_host_services(const xwork_runtime *pRuntime)
{
    return pRuntime ? &pRuntime->tHostServices : NULL;
}

const xwork_persistence_backend *xwork_runtime_get_persistence_backend(
    const xwork_runtime *pRuntime
)
{
    return pRuntime ? &pRuntime->tPersistenceBackend : NULL;
}

xwork_status xwork_runtime_list_persisted_runs(
    const xwork_runtime *pRuntime,
    xwork_string_list *pList
)
{
    return xwork__runtime_list_runs(pRuntime, pList);
}

xwork_status xwork_runtime_list_persisted_checkpoints(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_string_list *pList
)
{
    return xwork__runtime_list_checkpoints(pRuntime, sRunId, pList);
}

xwork_status xwork_runtime_list_persisted_events(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_string_list *pList
)
{
    return xwork__runtime_list_events(pRuntime, sRunId, pList);
}

xwork_status xwork_runtime_list_persisted_artifacts(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_string_list *pList
)
{
    return xwork__runtime_list_artifacts(pRuntime, sRunId, pList);
}

xwork_status xwork_runtime_list_persisted_artifact_summaries(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_artifact_summary_list *pList
)
{
    xwork_string_list tArtifactIds;
    xwork_artifact_summary *pItems = NULL;
    xwork_artifact tArtifact;
    xwork_status iStatus = XWORK_OK;
    size_t i;

    if ( !pRuntime || !sRunId || !sRunId[0] || !pList ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_artifact_summary_list_reset(pList);
    xwork_string_list_init(&tArtifactIds);
    xwork_artifact_init(&tArtifact);

    iStatus = xwork__runtime_list_artifacts(pRuntime, sRunId, &tArtifactIds);
    if ( iStatus != XWORK_OK ) {
        goto done;
    }
    if ( tArtifactIds.iCount == 0u ) {
        goto done;
    }

    pItems = (xwork_artifact_summary *)calloc(tArtifactIds.iCount, sizeof(*pItems));
    if ( !pItems ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto done;
    }

    pList->pItems = pItems;
    pList->iCount = tArtifactIds.iCount;
    for ( i = 0u; i < pList->iCount; ++i ) {
        xwork_artifact_summary_init(&pItems[i]);
        iStatus = xwork__runtime_load_artifact(
            pRuntime,
            sRunId,
            tArtifactIds.psItems[i],
            &tArtifact
        );
        if ( iStatus != XWORK_OK ) {
            goto done;
        }

        iStatus = xwork__replace_cstr((char **)&pItems[i].sArtifactId, tArtifact.sArtifactId);
        if ( iStatus != XWORK_OK ) goto done;
        iStatus = xwork__replace_cstr((char **)&pItems[i].sName, tArtifact.sName);
        if ( iStatus != XWORK_OK ) goto done;
        iStatus = xwork__replace_cstr((char **)&pItems[i].sMimeType, tArtifact.sMimeType);
        if ( iStatus != XWORK_OK ) goto done;
        iStatus = xwork__replace_cstr((char **)&pItems[i].sStorageRef, tArtifact.sStorageRef);
        if ( iStatus != XWORK_OK ) goto done;
        iStatus = xwork__replace_cstr((char **)&pItems[i].sSummary, tArtifact.sSummary);
        if ( iStatus != XWORK_OK ) goto done;
        iStatus = xwork__replace_cstr((char **)&pItems[i].sOutputRole, tArtifact.sOutputRole);
        if ( iStatus != XWORK_OK ) goto done;
        iStatus = xwork__replace_cstr((char **)&pItems[i].sReportSubjectRef, tArtifact.sReportSubjectRef);
        if ( iStatus != XWORK_OK ) goto done;
        iStatus = xwork__replace_cstr(
            (char **)&pItems[i].sPatchApplyResultJson,
            tArtifact.sPatchApplyResultJson
        );
        if ( iStatus != XWORK_OK ) goto done;
        iStatus = xwork__replace_cstr(
            (char **)&pItems[i].sPatchFileSummaryJson,
            tArtifact.sPatchFileSummaryJson
        );
        if ( iStatus != XWORK_OK ) goto done;
        pItems[i].eKind = tArtifact.eKind;
        pItems[i].eOutputClass = tArtifact.eOutputClass;
        pItems[i].eReportClass = tArtifact.eReportClass;
        pItems[i].bHasContentStats = tArtifact.bHasContentStats;
        pItems[i].iContentByteCount = tArtifact.iContentByteCount;
        pItems[i].iContentLineCount = tArtifact.iContentLineCount;
        pItems[i].bHasPatchStats = tArtifact.bHasPatchStats;
        pItems[i].iPatchFileCount = tArtifact.iPatchFileCount;
        pItems[i].iPatchHunkCount = tArtifact.iPatchHunkCount;
        pItems[i].iPatchAddedLineCount = tArtifact.iPatchAddedLineCount;
        pItems[i].iPatchDeletedLineCount = tArtifact.iPatchDeletedLineCount;
        pItems[i].bHasCommandIoStats = tArtifact.bHasCommandIoStats;
        pItems[i].iStdoutByteCount = tArtifact.iStdoutByteCount;
        pItems[i].iStderrByteCount = tArtifact.iStderrByteCount;
        pItems[i].bStdoutTruncated = tArtifact.bStdoutTruncated;
        pItems[i].bStderrTruncated = tArtifact.bStderrTruncated;
        pItems[i].bHasExitCode = tArtifact.bHasExitCode;
        pItems[i].iExitCode = tArtifact.iExitCode;
        pItems[i].iSequence = tArtifact.iSequence;
        xwork_artifact_reset(&tArtifact);
    }

done:
    xwork_artifact_reset(&tArtifact);
    xwork_string_list_reset(&tArtifactIds);
    if ( iStatus != XWORK_OK ) {
        xwork_artifact_summary_list_reset(pList);
    }
    return iStatus;
}

xwork_status xwork_runtime_query_persisted_artifact_summaries(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    const xwork_artifact_summary_query *pQuery,
    xwork_artifact_summary_list *pList
)
{
    xwork_artifact_summary_list tAllSummaries;
    xwork_artifact_summary *pItems = NULL;
    xwork_status iStatus = XWORK_OK;
    size_t iMatchCount = 0u;
    size_t iReturnedCount = 0u;
    size_t iWriteIndex = 0u;
    size_t i;

    if ( !pRuntime || !sRunId || !sRunId[0] || !pList ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !pQuery ) {
        return xwork_runtime_list_persisted_artifact_summaries(pRuntime, sRunId, pList);
    }

    xwork_artifact_summary_list_init(&tAllSummaries);
    xwork_artifact_summary_list_reset(pList);

    iStatus = xwork_runtime_list_persisted_artifact_summaries(
        pRuntime,
        sRunId,
        &tAllSummaries
    );
    if ( iStatus != XWORK_OK ) {
        goto done;
    }
    xwork__artifact_summary_sort_by_sequence(
        (xwork_artifact_summary *)tAllSummaries.pItems,
        tAllSummaries.iCount
    );

    for ( i = 0u; i < tAllSummaries.iCount; ++i ) {
        if ( xwork__artifact_summary_matches_query(&tAllSummaries.pItems[i], pQuery) ) {
            iMatchCount++;
        }
    }
    if ( iMatchCount == 0u ) {
        goto done;
    }

    iReturnedCount = iMatchCount;
    if ( pQuery->iLimit > 0u && iReturnedCount > pQuery->iLimit ) {
        iReturnedCount = pQuery->iLimit;
        pList->bHasMore = true;
    }

    pItems = (xwork_artifact_summary *)calloc(iReturnedCount, sizeof(*pItems));
    if ( !pItems ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto done;
    }

    pList->pItems = pItems;
    pList->iCount = iReturnedCount;
    for ( i = 0u; i < pList->iCount; ++i ) {
        xwork_artifact_summary_init(&pItems[i]);
    }

    for ( i = 0u; i < tAllSummaries.iCount; ++i ) {
        if ( !xwork__artifact_summary_matches_query(&tAllSummaries.pItems[i], pQuery) ) {
            continue;
        }
        iStatus = xwork__artifact_summary_copy(
            &pItems[iWriteIndex],
            &tAllSummaries.pItems[i]
        );
        if ( iStatus != XWORK_OK ) {
            goto done;
        }
        iWriteIndex++;
        if ( iWriteIndex >= iReturnedCount ) {
            break;
        }
    }
    if ( pList->iCount > 0u ) {
        pList->iNextAfterSequence = pItems[pList->iCount - 1u].iSequence;
    }

done:
    xwork_artifact_summary_list_reset(&tAllSummaries);
    if ( iStatus != XWORK_OK ) {
        xwork_artifact_summary_list_reset(pList);
    }
    return iStatus;
}

xwork_status xwork_runtime_query_persisted_run_steps(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    const xwork_run_step_query *pQuery,
    xwork_run_step_list *pList
)
{
    xwork_string_list tEventIds;
    xwork_event tEvent;
    xwork_checkpoint tCheckpoint;
    xwork_run_step tStep;
    xwork_status iStatus = XWORK_OK;
    size_t i;

    if ( !pRuntime || !sRunId || !sRunId[0] || !pList ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_string_list_init(&tEventIds);
    xwork_event_init(&tEvent);
    xwork_checkpoint_init(&tCheckpoint);
    xwork_run_step_init(&tStep);
    xwork_run_step_list_reset(pList);

    iStatus = xwork__runtime_list_events(pRuntime, sRunId, &tEventIds);
    if ( iStatus != XWORK_OK ) {
        goto done;
    }

    for ( i = 0u; i < tEventIds.iCount; ++i ) {
        const xwork_checkpoint *pCheckpoint = NULL;

        xwork_event_reset(&tEvent);
        iStatus = xwork__runtime_load_event(
            pRuntime,
            sRunId,
            tEventIds.psItems[i],
            &tEvent
        );
        if ( iStatus != XWORK_OK ) {
            goto done;
        }
        if ( tEvent.sCheckpointId && tEvent.sCheckpointId[0] ) {
            xwork_checkpoint_reset(&tCheckpoint);
            if ( xwork__runtime_load_checkpoint(
                     pRuntime,
                     sRunId,
                     tEvent.sCheckpointId,
                     &tCheckpoint
                 ) == XWORK_OK ) {
                pCheckpoint = &tCheckpoint;
            }
        }

        xwork_run_step_reset(&tStep);
        iStatus = xwork__run_step_from_event(&tStep, &tEvent, pCheckpoint);
        if ( iStatus != XWORK_OK ) {
            goto done;
        }
        if ( !xwork__run_step_matches_query(&tStep, pQuery) ) {
            continue;
        }
        if ( pQuery && pQuery->iLimit > 0u && pList->iCount >= pQuery->iLimit ) {
            pList->bHasMore = true;
            break;
        }
        iStatus = xwork__run_step_list_append(pList, &tStep);
        if ( iStatus != XWORK_OK ) {
            goto done;
        }
    }

done:
    xwork_run_step_reset(&tStep);
    xwork_checkpoint_reset(&tCheckpoint);
    xwork_event_reset(&tEvent);
    xwork_string_list_reset(&tEventIds);
    if ( iStatus != XWORK_OK ) {
        xwork_run_step_list_reset(pList);
    }
    return iStatus;
}

xwork_status xwork_runtime_list_persisted_run_summaries(
    const xwork_runtime *pRuntime,
    xwork_run_summary_list *pList
)
{
    return xwork__runtime_list_run_summaries(pRuntime, pList);
}

xwork_status xwork_runtime_list_persisted_run_index(
    const xwork_runtime *pRuntime,
    xwork_run_index_list *pList
)
{
    return xwork__runtime_query_run_index(pRuntime, NULL, pList);
}

xwork_status xwork_runtime_query_persisted_run_index(
    const xwork_runtime *pRuntime,
    const xwork_run_index_query *pQuery,
    xwork_run_index_list *pList
)
{
    return xwork__runtime_query_run_index(pRuntime, pQuery, pList);
}

xwork_status xwork_runtime_load_persisted_run_summary(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_run_summary *pSummary
)
{
    return xwork__runtime_load_run_summary(pRuntime, sRunId, pSummary);
}

xwork_status xwork_runtime_load_persisted_last_event(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_event *pEvent
)
{
    return xwork__runtime_load_last_event(pRuntime, sRunId, pEvent);
}

xwork_status xwork_runtime_load_persisted_last_approval_request(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_approval_request *pRequest
)
{
    return xwork__runtime_load_last_approval_request(pRuntime, sRunId, pRequest);
}

xwork_status xwork_runtime_load_persisted_last_checkpoint(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_checkpoint *pCheckpoint
)
{
    return xwork__runtime_load_last_checkpoint(pRuntime, sRunId, pCheckpoint);
}

xwork_status xwork_runtime_load_persisted_last_artifact(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_artifact *pArtifact
)
{
    return xwork__runtime_load_last_artifact(pRuntime, sRunId, pArtifact);
}

xwork_status xwork_runtime_load_persisted_event(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    const char *sEventId,
    xwork_event *pEvent
)
{
    return xwork__runtime_load_event(pRuntime, sRunId, sEventId, pEvent);
}

xwork_status xwork_runtime_load_persisted_checkpoint(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    const char *sCheckpointId,
    xwork_checkpoint *pCheckpoint
)
{
    return xwork__runtime_load_checkpoint(
        pRuntime,
        sRunId,
        sCheckpointId,
        pCheckpoint
    );
}

xwork_status xwork_runtime_load_persisted_artifact(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    const char *sArtifactId,
    xwork_artifact *pArtifact
)
{
    return xwork__runtime_load_artifact(
        pRuntime,
        sRunId,
        sArtifactId,
        pArtifact
    );
}

xwork_status xwork_runtime_find_persisted_artifact_by_name(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    const char *sArtifactName,
    xwork_artifact *pArtifact
)
{
    return xwork__runtime_find_artifact_by_name(
        pRuntime,
        sRunId,
        sArtifactName,
        pArtifact
    );
}

xwork_status xwork_runtime_get_policy_options(
    const xwork_runtime *pRuntime,
    xwork_policy_options *pOptions
)
{
    if ( !pRuntime || !pOptions ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    *pOptions = pRuntime->tPolicy;
    return XWORK_OK;
}

size_t xwork_runtime_get_workspace_count(const xwork_runtime *pRuntime)
{
    size_t iCount = 0u;
    const xwork_workspace *pCursor;

    if ( !pRuntime ) {
        return 0u;
    }

    for ( pCursor = pRuntime->pWorkspaces; pCursor; pCursor = pCursor->pNext ) {
        ++iCount;
    }
    return iCount;
}

size_t xwork_runtime_get_tool_count(const xwork_runtime *pRuntime)
{
    size_t iCount = 0u;
    const xwork_tool_record *pCursor;

    if ( !pRuntime ) {
        return 0u;
    }

    for ( pCursor = pRuntime->pTools; pCursor; pCursor = pCursor->pNext ) {
        ++iCount;
    }
    return iCount;
}

size_t xwork_runtime_get_run_count(const xwork_runtime *pRuntime)
{
    size_t iCount = 0u;
    const xwork_run *pCursor;

    if ( !pRuntime ) {
        return 0u;
    }

    for ( pCursor = pRuntime->pRuns; pCursor; pCursor = pCursor->pNext ) {
        ++iCount;
    }
    return iCount;
}

static xwork_status xwork__copy_workspace_ids(
    const xwork_run_options *pOptions,
    char ***ppsWorkspaceIds,
    size_t *piWorkspaceCount
);

static xwork_status xwork__run_apply_snapshot(
    xwork_run *pRun,
    const xwork_run_snapshot *pSnapshot
);

xwork_status xwork_runtime_recover_run(
    xwork_runtime *pRuntime,
    const xwork_run_snapshot *pSnapshot,
    xwork_run **ppRun
)
{
    xwork_run *pRun;
    xwork_run_options tOptions;
    size_t i;
    xwork_status iStatus;

    if ( !pRuntime || !pSnapshot || !ppRun ||
         !pSnapshot->sRunId || !pSnapshot->sRunId[0] ||
         !pSnapshot->sInstruction || !pSnapshot->sInstruction[0] ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    *ppRun = NULL;

    for ( i = 0u; i < pSnapshot->iWorkspaceCount; ++i ) {
        const char *sWorkspaceId = pSnapshot->psWorkspaceIds ? pSnapshot->psWorkspaceIds[i] : NULL;
        if ( !sWorkspaceId || !xwork__runtime_has_workspace(pRuntime, sWorkspaceId) ) {
            return XWORK_ERROR_NOT_FOUND;
        }
    }

    for ( pRun = pRuntime->pRuns; pRun; pRun = pRun->pNext ) {
        if ( pRun->sRunId && strcmp(pRun->sRunId, pSnapshot->sRunId) == 0 ) {
            return XWORK_ERROR_ALREADY_EXISTS;
        }
    }

    pRun = (xwork_run *)calloc(1u, sizeof(*pRun));
    if ( !pRun ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    xrtMutexInit(&pRun->tExecutionLock);
    pRun->bExecutionLockInitialized = true;
    pRun->pRuntime = pRuntime;
    pRun->sRunId = xwork__dup_cstr(pSnapshot->sRunId);
    if ( !pRun->sRunId ) {
        xwork_run_destroy(pRun);
        return XWORK_ERROR_NO_MEMORY;
    }

    xwork_run_options_init(&tOptions);
    tOptions.psWorkspaceIds = pSnapshot->psWorkspaceIds;
    tOptions.iWorkspaceCount = pSnapshot->iWorkspaceCount;
    iStatus = xwork__copy_workspace_ids(&tOptions, &pRun->psWorkspaceIds, &pRun->iWorkspaceCount);
    if ( iStatus != XWORK_OK ) {
        xwork_run_destroy(pRun);
        return iStatus;
    }

    iStatus = xwork__run_apply_snapshot(pRun, pSnapshot);
    if ( iStatus != XWORK_OK ) {
        xwork_run_destroy(pRun);
        return iStatus;
    }

    pRun->pNext = pRuntime->pRuns;
    pRuntime->pRuns = pRun;

    iStatus = xwork__run_record_event(
        pRun,
        XWORK_EVENT_CHECKPOINT_LOADED,
        pRun->sLastToolId,
        pRun->sLastApprovalRequestId,
        pRun->sLastCheckpointId,
        "Run recovered from snapshot."
    );
    if ( iStatus != XWORK_OK ) {
        xwork_run_destroy(pRun);
        return iStatus;
    }

    *ppRun = pRun;
    return XWORK_OK;
}

xwork_status xwork_runtime_recover_run_from_persistence(
    xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_run **ppRun
)
{
    xwork_run_snapshot tSnapshot;
    xwork_status iStatus;

    if ( !pRuntime || !sRunId || !sRunId[0] || !ppRun ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    *ppRun = NULL;
    xwork_run_snapshot_init(&tSnapshot);

    iStatus = xwork__runtime_load_run_snapshot(pRuntime, sRunId, &tSnapshot);
    if ( iStatus != XWORK_OK ) {
        xwork_run_snapshot_reset(&tSnapshot);
        return iStatus;
    }

    if ( !tSnapshot.sRunId || strcmp(tSnapshot.sRunId, sRunId) != 0 ) {
        xwork_run_snapshot_reset(&tSnapshot);
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }

    iStatus = xwork_runtime_recover_run(pRuntime, &tSnapshot, ppRun);
    xwork_run_snapshot_reset(&tSnapshot);
    return iStatus;
}

static xwork_status xwork__copy_workspace_ids(
    const xwork_run_options *pOptions,
    char ***ppsWorkspaceIds,
    size_t *piWorkspaceCount
)
{
    size_t i;
    char **psItems = NULL;

    if ( !ppsWorkspaceIds || !piWorkspaceCount ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    *ppsWorkspaceIds = NULL;
    *piWorkspaceCount = 0u;

    if ( !pOptions || !pOptions->psWorkspaceIds || pOptions->iWorkspaceCount == 0u ) {
        return XWORK_OK;
    }

    psItems = (char **)calloc(pOptions->iWorkspaceCount, sizeof(char *));
    if ( !psItems ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    for ( i = 0u; i < pOptions->iWorkspaceCount; ++i ) {
        psItems[i] = xwork__dup_cstr(pOptions->psWorkspaceIds[i]);
        if ( !psItems[i] ) {
            *ppsWorkspaceIds = psItems;
            *piWorkspaceCount = i;
            xwork__free_str_array(ppsWorkspaceIds, piWorkspaceCount);
            return XWORK_ERROR_NO_MEMORY;
        }
    }

    *ppsWorkspaceIds = psItems;
    *piWorkspaceCount = pOptions->iWorkspaceCount;
    return XWORK_OK;
}

static xwork_status xwork__run_snapshot_fill_from_run(
    const xwork_run *pRun,
    xwork_run_snapshot *pSnapshot
)
{
    xwork_status iStatus;

    if ( !pRun || !pSnapshot ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_run_snapshot_reset(pSnapshot);

    iStatus = xwork__run_snapshot_replace_cstr(&pSnapshot->sRunId, pRun->sRunId);
    if ( iStatus != XWORK_OK ) {
        goto fail;
    }
    iStatus = xwork__run_snapshot_replace_cstr(&pSnapshot->sParentRunId, pRun->sParentRunId);
    if ( iStatus != XWORK_OK ) {
        goto fail;
    }
    iStatus = xwork__run_snapshot_replace_cstr(&pSnapshot->sAgentId, pRun->sAgentId);
    if ( iStatus != XWORK_OK ) {
        goto fail;
    }
    iStatus = xwork__run_snapshot_replace_cstr(&pSnapshot->sTaskId, pRun->sTaskId);
    if ( iStatus != XWORK_OK ) {
        goto fail;
    }
    iStatus = xwork__run_snapshot_replace_cstr(&pSnapshot->sInstruction, pRun->sInstruction);
    if ( iStatus != XWORK_OK ) {
        goto fail;
    }
    iStatus = xwork__run_snapshot_replace_cstr(&pSnapshot->sLlmProfileId, pRun->sLlmProfileId);
    if ( iStatus != XWORK_OK ) {
        goto fail;
    }
    iStatus = xwork__run_snapshot_replace_cstr(
        &pSnapshot->sSessionProfileId,
        pRun->sSessionProfileId
    );
    if ( iStatus != XWORK_OK ) {
        goto fail;
    }
    iStatus = xwork__run_snapshot_replace_cstr(
        &pSnapshot->sSessionStateData,
        pRun->sSessionStateData
    );
    if ( iStatus != XWORK_OK ) {
        goto fail;
    }
    iStatus = xwork__run_snapshot_copy_workspace_ids(
        pSnapshot,
        (const char *const *)pRun->psWorkspaceIds,
        pRun->iWorkspaceCount
    );
    if ( iStatus != XWORK_OK ) {
        goto fail;
    }
    iStatus = xwork__run_snapshot_replace_cstr(&pSnapshot->sLastOutputText, pRun->sLastOutputText);
    if ( iStatus != XWORK_OK ) {
        goto fail;
    }
    iStatus = xwork__run_snapshot_replace_cstr(
        &pSnapshot->sLastMemoryContextText,
        pRun->sLastMemoryContextText
    );
    if ( iStatus != XWORK_OK ) {
        goto fail;
    }
    iStatus = xwork__run_snapshot_replace_cstr(&pSnapshot->sLastToolCallId, pRun->sLastToolCallId);
    if ( iStatus != XWORK_OK ) {
        goto fail;
    }
    iStatus = xwork__run_snapshot_replace_cstr(&pSnapshot->sLastToolId, pRun->sLastToolId);
    if ( iStatus != XWORK_OK ) {
        goto fail;
    }
    iStatus = xwork__run_snapshot_replace_cstr(
        &pSnapshot->sLastToolArgumentsJson,
        pRun->sLastToolArgumentsJson
    );
    if ( iStatus != XWORK_OK ) {
        goto fail;
    }
    iStatus = xwork__run_snapshot_replace_cstr(
        &pSnapshot->sLastToolResultText,
        pRun->sLastToolResultText
    );
    if ( iStatus != XWORK_OK ) {
        goto fail;
    }
    iStatus = xwork__run_snapshot_replace_cstr(
        &pSnapshot->sLastToolVisibleSummary,
        pRun->sLastToolVisibleSummary
    );
    if ( iStatus != XWORK_OK ) {
        goto fail;
    }
    iStatus = xwork__run_snapshot_replace_cstr(
        &pSnapshot->sLastApprovalRequestId,
        pRun->sLastApprovalRequestId
    );
    if ( iStatus != XWORK_OK ) {
        goto fail;
    }
    iStatus = xwork__run_snapshot_replace_cstr(
        &pSnapshot->sLastApprovalToolId,
        pRun->sLastApprovalToolId
    );
    if ( iStatus != XWORK_OK ) {
        goto fail;
    }
    iStatus = xwork__run_snapshot_replace_cstr(
        &pSnapshot->sLastApprovalReason,
        pRun->sLastApprovalReason
    );
    if ( iStatus != XWORK_OK ) {
        goto fail;
    }
    iStatus = xwork__run_snapshot_replace_cstr(
        &pSnapshot->sLastApprovalScope,
        pRun->sLastApprovalScope
    );
    if ( iStatus != XWORK_OK ) {
        goto fail;
    }
    iStatus = xwork__run_snapshot_replace_cstr(
        &pSnapshot->sLastApprovalActionSummary,
        pRun->sLastApprovalActionSummary
    );
    if ( iStatus != XWORK_OK ) {
        goto fail;
    }
    iStatus = xwork__run_snapshot_replace_cstr(
        &pSnapshot->sLastCheckpointId,
        pRun->sLastCheckpointId
    );
    if ( iStatus != XWORK_OK ) {
        goto fail;
    }
    iStatus = xwork__run_snapshot_replace_cstr(
        &pSnapshot->sLastCheckpointPendingStep,
        pRun->sLastCheckpointPendingStep
    );
    if ( iStatus != XWORK_OK ) {
        goto fail;
    }
    iStatus = xwork__run_snapshot_replace_cstr(
        &pSnapshot->sLastCheckpointSessionStateRef,
        pRun->sLastCheckpointSessionStateRef
    );
    if ( iStatus != XWORK_OK ) {
        goto fail;
    }
    iStatus = xwork__run_snapshot_replace_cstr(
        &pSnapshot->sLastCheckpointToolOutputsRef,
        pRun->sLastCheckpointToolOutputsRef
    );
    if ( iStatus != XWORK_OK ) {
        goto fail;
    }
    iStatus = xwork__run_snapshot_replace_cstr(
        &pSnapshot->sLastCheckpointWorkspaceSnapshotRef,
        pRun->sLastCheckpointWorkspaceSnapshotRef
    );
    if ( iStatus != XWORK_OK ) {
        goto fail;
    }
    iStatus = xwork__run_snapshot_replace_cstr(
        &pSnapshot->sLastCheckpointArtifactRefs,
        pRun->sLastCheckpointArtifactRefs
    );
    if ( iStatus != XWORK_OK ) {
        goto fail;
    }
    iStatus = xwork__run_snapshot_copy_artifacts(pSnapshot, pRun);
    if ( iStatus != XWORK_OK ) {
        goto fail;
    }

    pSnapshot->eAutonomy = pRun->eAutonomy;
    pSnapshot->tSessionPolicy = pRun->tSessionPolicy;
    pSnapshot->eState = pRun->eState;
    pSnapshot->bHasMemoryContext = pRun->bHasLastMemoryContext;
    pSnapshot->iLastMemoryWorkspaceCount = pRun->iLastMemoryWorkspaceCount;
    pSnapshot->bHasToolCall = pRun->bHasLastToolCall;
    pSnapshot->bHasToolResult = pRun->bHasLastToolResult;
    pSnapshot->bHasApprovalRequest = pRun->bHasLastApprovalRequest;
    pSnapshot->eLastApprovalRiskLevel = pRun->eLastApprovalRiskLevel;
    pSnapshot->eLastApprovalState = pRun->eLastApprovalState;
    pSnapshot->iLastApprovalSequence = pRun->iLastApprovalSequence;
    pSnapshot->bHasCheckpoint = pRun->bHasLastCheckpoint;
    pSnapshot->eLastCheckpointKind = pRun->eLastCheckpointKind;
    pSnapshot->eLastCheckpointRunState = pRun->eLastCheckpointRunState;
    pSnapshot->iLastCheckpointSequence = pRun->iLastCheckpointSequence;
    pSnapshot->iNextEventSequence = pRun->iNextEventSequence;
    pSnapshot->iNextArtifactSequence = pRun->iNextArtifactSequence;
    pSnapshot->iNextApprovalSequence = pRun->iNextApprovalSequence;
    pSnapshot->iNextCheckpointSequence = pRun->iNextCheckpointSequence;
    return XWORK_OK;

fail:
    xwork_run_snapshot_reset(pSnapshot);
    return iStatus;
}

static xwork_status xwork__run_apply_snapshot(
    xwork_run *pRun,
    const xwork_run_snapshot *pSnapshot
)
{
    xwork_status iStatus;

    if ( !pRun || !pSnapshot ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    iStatus = xwork__replace_cstr(&pRun->sParentRunId, pSnapshot->sParentRunId);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__replace_cstr(&pRun->sAgentId, pSnapshot->sAgentId);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__replace_cstr(&pRun->sTaskId, pSnapshot->sTaskId);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__replace_cstr(&pRun->sInstruction, pSnapshot->sInstruction);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__replace_cstr(&pRun->sLlmProfileId, pSnapshot->sLlmProfileId);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__replace_cstr(&pRun->sSessionProfileId, pSnapshot->sSessionProfileId);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    pRun->tSessionPolicy = pSnapshot->tSessionPolicy;
    xwork__run_discard_session(pRun);
    iStatus = xwork__replace_cstr(&pRun->sSessionStateData, pSnapshot->sSessionStateData);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__replace_cstr(&pRun->sLastOutputText, pSnapshot->sLastOutputText);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__replace_cstr(
        &pRun->sLastMemoryContextText,
        pSnapshot->sLastMemoryContextText
    );
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__replace_cstr(&pRun->sLastToolCallId, pSnapshot->sLastToolCallId);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__replace_cstr(&pRun->sLastToolId, pSnapshot->sLastToolId);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__replace_cstr(&pRun->sLastToolArgumentsJson, pSnapshot->sLastToolArgumentsJson);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__replace_cstr(&pRun->sLastToolResultText, pSnapshot->sLastToolResultText);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__replace_cstr(
        &pRun->sLastToolVisibleSummary,
        pSnapshot->sLastToolVisibleSummary
    );
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__replace_cstr(
        &pRun->sLastApprovalRequestId,
        pSnapshot->sLastApprovalRequestId
    );
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__replace_cstr(&pRun->sLastApprovalToolId, pSnapshot->sLastApprovalToolId);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__replace_cstr(&pRun->sLastApprovalReason, pSnapshot->sLastApprovalReason);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__replace_cstr(&pRun->sLastApprovalScope, pSnapshot->sLastApprovalScope);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__replace_cstr(
        &pRun->sLastApprovalActionSummary,
        pSnapshot->sLastApprovalActionSummary
    );
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__replace_cstr(&pRun->sLastCheckpointId, pSnapshot->sLastCheckpointId);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__replace_cstr(
        &pRun->sLastCheckpointPendingStep,
        pSnapshot->sLastCheckpointPendingStep
    );
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__replace_cstr(
        &pRun->sLastCheckpointSessionStateRef,
        pSnapshot->sLastCheckpointSessionStateRef
    );
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__replace_cstr(
        &pRun->sLastCheckpointToolOutputsRef,
        pSnapshot->sLastCheckpointToolOutputsRef
    );
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__replace_cstr(
        &pRun->sLastCheckpointWorkspaceSnapshotRef,
        pSnapshot->sLastCheckpointWorkspaceSnapshotRef
    );
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__replace_cstr(
        &pRun->sLastCheckpointArtifactRefs,
        pSnapshot->sLastCheckpointArtifactRefs
    );
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__run_apply_snapshot_artifacts(pRun, pSnapshot);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    pRun->eAutonomy = pSnapshot->eAutonomy;
    pRun->eState = pSnapshot->eState;
    pRun->bHasLastMemoryContext = pSnapshot->bHasMemoryContext;
    pRun->iLastMemoryWorkspaceCount = pSnapshot->iLastMemoryWorkspaceCount;
    pRun->bHasLastToolCall = pSnapshot->bHasToolCall;
    pRun->bHasLastToolResult = pSnapshot->bHasToolResult;
    pRun->bHasLastApprovalRequest = pSnapshot->bHasApprovalRequest;
    pRun->eLastApprovalRiskLevel = pSnapshot->eLastApprovalRiskLevel;
    pRun->eLastApprovalState = pSnapshot->eLastApprovalState;
    pRun->iLastApprovalSequence = pSnapshot->iLastApprovalSequence;
    pRun->iNextApprovalSequence = pSnapshot->iNextApprovalSequence;
    pRun->bHasLastCheckpoint = pSnapshot->bHasCheckpoint;
    pRun->eLastCheckpointKind = pSnapshot->eLastCheckpointKind;
    pRun->eLastCheckpointRunState = pSnapshot->eLastCheckpointRunState;
    pRun->iLastCheckpointSequence = pSnapshot->iLastCheckpointSequence;
    pRun->iNextCheckpointSequence = pSnapshot->iNextCheckpointSequence;
    pRun->iNextEventSequence = pSnapshot->iNextEventSequence;
    pRun->iNextArtifactSequence = pSnapshot->iNextArtifactSequence;
    return XWORK_OK;
}

xwork_status xwork_run_create(
    xwork_runtime *pRuntime,
    const xwork_run_options *pOptions,
    xwork_run **ppRun
)
{
    xwork_run *pRun;
    size_t i;
    xwork_status iStatus;

    if ( !pRuntime || !pOptions || !ppRun ||
         !pOptions->sRunId || !pOptions->sRunId[0] ||
         !pOptions->sInstruction || !pOptions->sInstruction[0] ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    *ppRun = NULL;

    for ( i = 0u; i < pOptions->iWorkspaceCount; ++i ) {
        const char *sWorkspaceId = pOptions->psWorkspaceIds ? pOptions->psWorkspaceIds[i] : NULL;
        if ( !sWorkspaceId || !xwork__runtime_has_workspace(pRuntime, sWorkspaceId) ) {
            return XWORK_ERROR_NOT_FOUND;
        }
    }

    for ( pRun = pRuntime->pRuns; pRun; pRun = pRun->pNext ) {
        if ( pRun->sRunId && strcmp(pRun->sRunId, pOptions->sRunId) == 0 ) {
            return XWORK_ERROR_ALREADY_EXISTS;
        }
    }

    pRun = (xwork_run *)calloc(1u, sizeof(*pRun));
    if ( !pRun ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    xrtMutexInit(&pRun->tExecutionLock);
    pRun->bExecutionLockInitialized = true;
    pRun->pRuntime = pRuntime;
    pRun->sRunId = xwork__dup_cstr(pOptions->sRunId);
    pRun->sParentRunId = xwork__dup_cstr(pOptions->sParentRunId);
    pRun->sAgentId = xwork__dup_cstr(pOptions->sAgentId);
    pRun->sTaskId = xwork__dup_cstr(pOptions->sTaskId);
    pRun->sInstruction = xwork__dup_cstr(pOptions->sInstruction);
    pRun->sLlmProfileId = xwork__dup_cstr(pOptions->sLlmProfileId);
    pRun->sSessionProfileId = xwork__dup_cstr(pOptions->sSessionProfileId);
    pRun->eAutonomy = pOptions->eAutonomy;
    pRun->tSessionPolicy = pOptions->tSessionPolicy;
    pRun->eState = XWORK_RUN_CREATED;

    if ( !pRun->sRunId ||
         !pRun->sInstruction ||
         (pOptions->sParentRunId && !pRun->sParentRunId) ||
         (pOptions->sAgentId && !pRun->sAgentId) ||
         (pOptions->sTaskId && !pRun->sTaskId) ||
         (pOptions->sLlmProfileId && !pRun->sLlmProfileId) ||
         (pOptions->sSessionProfileId && !pRun->sSessionProfileId) ) {
        xwork_run_destroy(pRun);
        return XWORK_ERROR_NO_MEMORY;
    }

    iStatus = xwork__copy_workspace_ids(pOptions, &pRun->psWorkspaceIds, &pRun->iWorkspaceCount);
    if ( iStatus != XWORK_OK ) {
        xwork_run_destroy(pRun);
        return iStatus;
    }

    iStatus = xwork__run_record_event(
        pRun,
        XWORK_EVENT_RUN_CREATED,
        NULL,
        NULL,
        NULL,
        "Run created."
    );
    if ( iStatus != XWORK_OK ) {
        xwork_run_destroy(pRun);
        return iStatus;
    }

    pRun->pNext = pRuntime->pRuns;
    pRuntime->pRuns = pRun;
    *ppRun = pRun;
    return XWORK_OK;
}

void xwork_run_destroy(xwork_run *pRun)
{
    xwork_run **ppCursor;

    if ( !pRun ) {
        return;
    }

    if ( pRun->pRuntime ) {
        ppCursor = &pRun->pRuntime->pRuns;
        while ( *ppCursor ) {
            if ( *ppCursor == pRun ) {
                *ppCursor = pRun->pNext;
                break;
            }
            ppCursor = &(*ppCursor)->pNext;
        }
    }

    xwork__free_cstr(&pRun->sRunId);
    xwork__free_cstr(&pRun->sParentRunId);
    xwork__free_cstr(&pRun->sAgentId);
    xwork__free_cstr(&pRun->sTaskId);
    xwork__free_cstr(&pRun->sInstruction);
    xwork__free_cstr(&pRun->sLlmProfileId);
    xwork__free_cstr(&pRun->sSessionProfileId);
    xwork__free_str_array(&pRun->psWorkspaceIds, &pRun->iWorkspaceCount);
    xwork__run_reset_observability(pRun);
    xwork__run_reset_persistence(pRun);
    xwork__run_reset_artifacts(pRun);
    if ( pRun->bExecutionLockInitialized ) {
        xrtMutexUnit(&pRun->tExecutionLock);
    }
    free(pRun);
}

static xwork_status xwork__run_transition(xwork_run *pRun, xwork_run_state eNextState)
{
    if ( !pRun ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( xwork__run_state_is_terminal(pRun->eState) ) {
        return XWORK_ERROR_INVALID_STATE;
    }
    pRun->eState = eNextState;
    return XWORK_OK;
}

static bool xwork__approval_state_is_decision(xwork_approval_state eState)
{
    return eState == XWORK_APPROVAL_APPROVED ||
           eState == XWORK_APPROVAL_REJECTED ||
           eState == XWORK_APPROVAL_CANCELLED;
}

static bool xwork__run_has_pending_tool_call(const xwork_run *pRun)
{
    return pRun &&
           pRun->bHasLastToolCall &&
           !pRun->bHasLastToolResult &&
           pRun->sLastToolId &&
           pRun->sLastToolId[0];
}

static const xwork_checkpoint_record *xwork__run_find_checkpoint_record(
    const xwork_run *pRun,
    const char *sCheckpointId
)
{
    size_t i;

    if ( !pRun || !sCheckpointId || !sCheckpointId[0] ) {
        return NULL;
    }

    for ( i = 0u; i < pRun->iCheckpointCount; ++i ) {
        const xwork_checkpoint_record *pRecord = &pRun->pCheckpointLog[i];
        if ( pRecord->sCheckpointId &&
             strcmp(pRecord->sCheckpointId, sCheckpointId) == 0 ) {
            return pRecord;
        }
    }

    return NULL;
}

xwork_status xwork_run_start(xwork_run *pRun)
{
    if ( !pRun ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( pRun->eState != XWORK_RUN_CREATED &&
         pRun->eState != XWORK_RUN_READY &&
         pRun->eState != XWORK_RUN_PAUSED ) {
        return XWORK_ERROR_INVALID_STATE;
    }
    pRun->eState = XWORK_RUN_RUNNING;
    return xwork__run_record_event(
        pRun,
        XWORK_EVENT_RUN_STARTED,
        NULL,
        NULL,
        NULL,
        "Run started."
    );
}

xwork_status xwork_run_set_waiting_approval(xwork_run *pRun)
{
    return xwork__run_transition(pRun, XWORK_RUN_WAITING_APPROVAL);
}

xwork_status xwork_run_set_waiting_tool(xwork_run *pRun)
{
    return xwork__run_transition(pRun, XWORK_RUN_WAITING_TOOL);
}

xwork_status xwork_run_set_paused(xwork_run *pRun)
{
    xwork_status iStatus = xwork__run_transition(pRun, XWORK_RUN_PAUSED);

    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    return xwork__run_record_event(
        pRun,
        XWORK_EVENT_RUN_PAUSED,
        NULL,
        NULL,
        NULL,
        "Run paused."
    );
}

xwork_status xwork_run_submit_approval(
    xwork_run *pRun,
    xwork_approval_state eDecision
)
{
    const char *sSummary;
    xwork_status iStatus;

    if ( !pRun ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( pRun->eState != XWORK_RUN_WAITING_APPROVAL &&
         pRun->eState != XWORK_RUN_PAUSED ) {
        return XWORK_ERROR_INVALID_STATE;
    }
    if ( !pRun->bHasLastApprovalRequest || !pRun->sLastApprovalRequestId ) {
        return XWORK_ERROR_NOT_FOUND;
    }
    if ( pRun->eLastApprovalState != XWORK_APPROVAL_PENDING ) {
        return XWORK_ERROR_INVALID_STATE;
    }
    if ( !xwork__approval_state_is_decision(eDecision) ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    iStatus = xwork__run_record_approval_request(
        pRun,
        pRun->sLastApprovalRequestId,
        pRun->sLastApprovalToolId,
        pRun->sLastApprovalReason,
        pRun->eLastApprovalRiskLevel,
        pRun->sLastApprovalScope,
        pRun->sLastApprovalActionSummary,
        eDecision
    );
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    pRun->eState = XWORK_RUN_PAUSED;
    switch ( eDecision ) {
        case XWORK_APPROVAL_APPROVED:
            sSummary = "Approval approved; run is ready to resume.";
            break;
        case XWORK_APPROVAL_REJECTED:
            sSummary = "Approval rejected; run is paused.";
            break;
        case XWORK_APPROVAL_CANCELLED:
            sSummary = "Approval cancelled; run is paused.";
            break;
        default:
            return XWORK_ERROR_INVALID_ARGUMENT;
    }

    iStatus = xwork__run_record_event(
        pRun,
        XWORK_EVENT_APPROVAL_RESOLVED,
        pRun->sLastApprovalToolId,
        pRun->sLastApprovalRequestId,
        pRun->sLastCheckpointId,
        sSummary
    );
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    if ( pRun->bHasLastCheckpoint ) {
        xwork_run_snapshot tSnapshot;

        xwork_run_snapshot_init(&tSnapshot);
        iStatus = xwork__run_snapshot_fill_from_run(pRun, &tSnapshot);
        if ( iStatus == XWORK_OK ) {
            iStatus = xwork__runtime_store_run_snapshot(pRun->pRuntime, &tSnapshot);
        }
        xwork_run_snapshot_reset(&tSnapshot);
        if ( iStatus != XWORK_OK ) {
            return iStatus;
        }
    }

    return XWORK_OK;
}

xwork_status xwork_run_load_checkpoint(
    xwork_run *pRun,
    const char *sCheckpointId
)
{
    const xwork_checkpoint_record *pRecord;
    xwork_run_snapshot tSnapshot;
    xwork_status iStatus;

    if ( !pRun || !sCheckpointId || !sCheckpointId[0] ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    pRecord = xwork__run_find_checkpoint_record(pRun, sCheckpointId);
    if ( pRecord ) {
        iStatus = xwork__run_restore_checkpoint_snapshot(pRun, pRecord);
        if ( iStatus != XWORK_OK ) {
            return iStatus;
        }
    } else {
        xwork_run_snapshot_init(&tSnapshot);
        iStatus = xwork__runtime_load_checkpoint_snapshot(
            pRun->pRuntime,
            pRun->sRunId,
            sCheckpointId,
            &tSnapshot
        );
        if ( iStatus != XWORK_OK ) {
            xwork_run_snapshot_reset(&tSnapshot);
            return iStatus;
        }
        if ( !tSnapshot.sRunId ||
             strcmp(tSnapshot.sRunId, pRun->sRunId) != 0 ||
             !tSnapshot.sLastCheckpointId ||
             strcmp(tSnapshot.sLastCheckpointId, sCheckpointId) != 0 ) {
            xwork_run_snapshot_reset(&tSnapshot);
            return XWORK_ERROR_EXTERNAL_FAILURE;
        }

        iStatus = xwork__run_apply_snapshot(pRun, &tSnapshot);
        xwork_run_snapshot_reset(&tSnapshot);
        if ( iStatus != XWORK_OK ) {
            return iStatus;
        }
    }

    return xwork__run_record_event(
        pRun,
        XWORK_EVENT_CHECKPOINT_LOADED,
        pRun->sLastToolId,
        pRun->sLastApprovalRequestId,
        pRun->sLastCheckpointId,
        "Checkpoint loaded into run state."
    );
}

xwork_status xwork_run_resume(xwork_run *pRun)
{
    const char *sSummary = "Checkpoint loaded; run resumed.";
    bool bPendingToolApprovalLinked = false;

    if ( !pRun ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( xwork__run_state_is_terminal(pRun->eState) ) {
        return XWORK_ERROR_INVALID_STATE;
    }
    if ( !pRun->bHasLastCheckpoint || !pRun->sLastCheckpointId ) {
        return XWORK_ERROR_NOT_FOUND;
    }
    if ( pRun->eLastCheckpointRunState == XWORK_RUN_COMPLETED ||
         pRun->eLastCheckpointRunState == XWORK_RUN_CANCELLED ||
         pRun->eLastCheckpointRunState == XWORK_RUN_FAILED ) {
        return XWORK_ERROR_INVALID_STATE;
    }

    if ( xwork__run_has_pending_tool_call(pRun) ) {
        bPendingToolApprovalLinked =
            pRun->bHasLastApprovalRequest &&
            pRun->sLastApprovalRequestId &&
            pRun->sLastApprovalToolId &&
            strcmp(pRun->sLastApprovalToolId, pRun->sLastToolId) == 0;

        if ( bPendingToolApprovalLinked ) {
            if ( pRun->eLastApprovalState == XWORK_APPROVAL_PENDING ) {
                return XWORK_ERROR_INVALID_STATE;
            }
            if ( pRun->eLastApprovalState != XWORK_APPROVAL_APPROVED ) {
                return XWORK_ERROR_INVALID_STATE;
            }
        }

        if ( pRun->eLastCheckpointRunState == XWORK_RUN_WAITING_APPROVAL ) {
            if ( !bPendingToolApprovalLinked ) {
                return XWORK_ERROR_INVALID_STATE;
            }
            sSummary = "Checkpoint loaded; approved tool execution will resume.";
        } else {
            sSummary = "Checkpoint loaded; pending tool execution will resume.";
        }
    } else if ( pRun->eLastCheckpointRunState == XWORK_RUN_WAITING_APPROVAL ) {
        return XWORK_ERROR_INVALID_STATE;
    }

    pRun->eState = XWORK_RUN_RUNNING;
    return xwork__run_record_event(
        pRun,
        XWORK_EVENT_CHECKPOINT_LOADED,
        pRun->sLastToolId,
        pRun->sLastApprovalRequestId,
        pRun->sLastCheckpointId,
        sSummary
    );
}

xwork_status xwork_run_complete(xwork_run *pRun)
{
    xwork_status iStatus = xwork__run_transition(pRun, XWORK_RUN_COMPLETED);

    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    return xwork__run_record_event(
        pRun,
        XWORK_EVENT_RUN_COMPLETED,
        NULL,
        NULL,
        NULL,
        "Run completed."
    );
}

xwork_status xwork_run_cancel(xwork_run *pRun)
{
    xwork_status iStatus = xwork__run_transition(pRun, XWORK_RUN_CANCELLED);

    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    return xwork__run_record_event(
        pRun,
        XWORK_EVENT_RUN_CANCELLED,
        NULL,
        NULL,
        NULL,
        "Run cancelled."
    );
}

xwork_status xwork_run_fail(xwork_run *pRun)
{
    xwork_status iStatus = xwork__run_transition(pRun, XWORK_RUN_FAILED);

    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    return xwork__run_record_event(
        pRun,
        XWORK_EVENT_RUN_FAILED,
        NULL,
        NULL,
        NULL,
        "Run failed."
    );
}

const char *xwork_run_get_id(const xwork_run *pRun)
{
    return pRun ? pRun->sRunId : NULL;
}

const char *xwork_run_get_instruction(const xwork_run *pRun)
{
    return pRun ? pRun->sInstruction : NULL;
}

xwork_run_state xwork_run_get_state(const xwork_run *pRun)
{
    return pRun ? pRun->eState : XWORK_RUN_FAILED;
}

xwork_autonomy_mode xwork_run_get_autonomy(const xwork_run *pRun)
{
    return pRun ? pRun->eAutonomy : XWORK_AUTONOMY_MANUAL;
}

size_t xwork_run_get_workspace_count(const xwork_run *pRun)
{
    return pRun ? pRun->iWorkspaceCount : 0u;
}

const char *xwork_run_get_workspace_id(const xwork_run *pRun, size_t iIndex)
{
    if ( !pRun || iIndex >= pRun->iWorkspaceCount ) {
        return NULL;
    }
    return pRun->psWorkspaceIds ? pRun->psWorkspaceIds[iIndex] : NULL;
}

xwork_status xwork_run_get_summary(const xwork_run *pRun, xwork_run_summary *pSummary)
{
    xwork_run_summary tSummary;

    if ( !pRun || !pSummary ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_run_summary_init(&tSummary);
    tSummary.sRunId = pRun->sRunId;
    tSummary.sParentRunId = pRun->sParentRunId;
    tSummary.sAgentId = pRun->sAgentId;
    tSummary.sTaskId = pRun->sTaskId;
    tSummary.sInstruction = pRun->sInstruction;
    tSummary.eAutonomy = pRun->eAutonomy;
    tSummary.eState = pRun->eState;
    tSummary.iWorkspaceCount = pRun->iWorkspaceCount;
    return xwork__run_summary_copy(pSummary, &tSummary);
}

xwork_status xwork_run_get_snapshot(
    const xwork_run *pRun,
    xwork_run_snapshot *pSnapshot
)
{
    return xwork__run_snapshot_fill_from_run(pRun, pSnapshot);
}

xwork_status xwork_run_get_last_event(const xwork_run *pRun, xwork_event *pEvent)
{
    xwork_event tEvent;

    if ( !pRun || !pEvent ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !pRun->sLastEventId || pRun->iLastEventSequence == 0u ) {
        return XWORK_ERROR_NOT_FOUND;
    }

    xwork_event_init(&tEvent);
    tEvent.sEventId = pRun->sLastEventId;
    tEvent.sRunId = pRun->sRunId;
    tEvent.sToolId = pRun->sLastEventToolId;
    tEvent.sApprovalRequestId = pRun->sLastEventApprovalRequestId;
    tEvent.sCheckpointId = pRun->sLastEventCheckpointId;
    tEvent.sSummary = pRun->sLastEventSummary;
    tEvent.eKind = pRun->eLastEventKind;
    tEvent.eRunState = pRun->eLastEventRunState;
    tEvent.iSequence = pRun->iLastEventSequence;
    return xwork__event_copy(pEvent, &tEvent);
}

xwork_status xwork_run_get_last_approval_request(
    const xwork_run *pRun,
    xwork_approval_request *pRequest
)
{
    xwork_approval_request tRequest;

    if ( !pRun || !pRequest ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !pRun->bHasLastApprovalRequest || !pRun->sLastApprovalRequestId ) {
        return XWORK_ERROR_NOT_FOUND;
    }

    xwork_approval_request_init(&tRequest);
    tRequest.sRequestId = pRun->sLastApprovalRequestId;
    tRequest.sRunId = pRun->sRunId;
    tRequest.sToolId = pRun->sLastApprovalToolId;
    tRequest.sReason = pRun->sLastApprovalReason;
    tRequest.sScope = pRun->sLastApprovalScope;
    tRequest.sActionSummary = pRun->sLastApprovalActionSummary;
    tRequest.eRiskLevel = pRun->eLastApprovalRiskLevel;
    tRequest.eState = pRun->eLastApprovalState;
    return xwork__approval_request_copy(pRequest, &tRequest);
}

xwork_status xwork_run_get_last_checkpoint(
    const xwork_run *pRun,
    xwork_checkpoint *pCheckpoint
)
{
    xwork_checkpoint tCheckpoint;

    if ( !pRun || !pCheckpoint ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !pRun->bHasLastCheckpoint || !pRun->sLastCheckpointId ) {
        return XWORK_ERROR_NOT_FOUND;
    }

    xwork_checkpoint_init(&tCheckpoint);
    tCheckpoint.sCheckpointId = pRun->sLastCheckpointId;
    tCheckpoint.sRunId = pRun->sRunId;
    tCheckpoint.sPendingStep = pRun->sLastCheckpointPendingStep;
    tCheckpoint.sSessionStateRef = pRun->sLastCheckpointSessionStateRef;
    tCheckpoint.sToolOutputsRef = pRun->sLastCheckpointToolOutputsRef;
    tCheckpoint.sWorkspaceSnapshotRef = pRun->sLastCheckpointWorkspaceSnapshotRef;
    tCheckpoint.sArtifactRefs = pRun->sLastCheckpointArtifactRefs;
    tCheckpoint.eKind = pRun->eLastCheckpointKind;
    tCheckpoint.eRunState = pRun->eLastCheckpointRunState;
    tCheckpoint.iSequence = pRun->iLastCheckpointSequence;
    return xwork__checkpoint_copy(pCheckpoint, &tCheckpoint);
}

xwork_status xwork_run_get_last_memory_context(
    const xwork_run *pRun,
    xwork_memory_context *pContext
)
{
    xwork_status iStatus;

    if ( !pRun || !pContext ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !pRun->bHasLastMemoryContext || !pRun->sLastMemoryContextText ) {
        return XWORK_ERROR_NOT_FOUND;
    }

    xwork_memory_context_reset(pContext);
    iStatus = xwork__replace_cstr((char **)&pContext->sText, pRun->sLastMemoryContextText);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    pContext->iWorkspaceCount = pRun->iLastMemoryWorkspaceCount;
    return XWORK_OK;
}

size_t xwork_run_get_event_count(const xwork_run *pRun)
{
    return pRun ? pRun->iEventCount : 0u;
}

xwork_status xwork_run_get_event(
    const xwork_run *pRun,
    size_t iIndex,
    xwork_event *pEvent
)
{
    if ( !pRun || !pEvent ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( iIndex >= pRun->iEventCount ) {
        return XWORK_ERROR_NOT_FOUND;
    }

    return xwork__event_copy(pEvent, &pRun->pEventLog[iIndex].tEvent);
}

static const xwork_checkpoint *xwork__run_find_checkpoint_by_id(
    const xwork_run *pRun,
    const char *sCheckpointId
)
{
    size_t i;

    if ( !pRun || !sCheckpointId || !sCheckpointId[0] ) {
        return NULL;
    }

    for ( i = 0u; i < pRun->iCheckpointCount; ++i ) {
        const xwork_checkpoint *pCheckpoint = &pRun->pCheckpointLog[i].tCheckpoint;
        if ( pCheckpoint->sCheckpointId &&
             strcmp(pCheckpoint->sCheckpointId, sCheckpointId) == 0 ) {
            return pCheckpoint;
        }
    }
    return NULL;
}

size_t xwork_run_get_step_count(const xwork_run *pRun)
{
    return pRun ? pRun->iEventCount : 0u;
}

xwork_status xwork_run_get_step(
    const xwork_run *pRun,
    size_t iIndex,
    xwork_run_step *pStep
)
{
    const xwork_event *pEvent;
    const xwork_checkpoint *pCheckpoint;

    if ( !pRun || !pStep ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( iIndex >= pRun->iEventCount ) {
        return XWORK_ERROR_NOT_FOUND;
    }

    pEvent = &pRun->pEventLog[iIndex].tEvent;
    pCheckpoint = xwork__run_find_checkpoint_by_id(pRun, pEvent->sCheckpointId);
    return xwork__run_step_from_event(pStep, pEvent, pCheckpoint);
}

xwork_status xwork_run_query_steps(
    const xwork_run *pRun,
    const xwork_run_step_query *pQuery,
    xwork_run_step_list *pList
)
{
    xwork_run_step tStep;
    xwork_status iStatus = XWORK_OK;
    size_t i;

    if ( !pRun || !pList ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_run_step_init(&tStep);
    xwork_run_step_list_reset(pList);
    for ( i = 0u; i < pRun->iEventCount; ++i ) {
        const xwork_event *pEvent = &pRun->pEventLog[i].tEvent;
        const xwork_checkpoint *pCheckpoint =
            xwork__run_find_checkpoint_by_id(pRun, pEvent->sCheckpointId);

        xwork_run_step_reset(&tStep);
        iStatus = xwork__run_step_from_event(&tStep, pEvent, pCheckpoint);
        if ( iStatus != XWORK_OK ) {
            goto done;
        }
        if ( !xwork__run_step_matches_query(&tStep, pQuery) ) {
            continue;
        }
        if ( pQuery && pQuery->iLimit > 0u && pList->iCount >= pQuery->iLimit ) {
            pList->bHasMore = true;
            pList->iNextAfterSequence = tStep.iSequence;
            break;
        }
        iStatus = xwork__run_step_list_append(pList, &tStep);
        if ( iStatus != XWORK_OK ) {
            goto done;
        }
    }

done:
    xwork_run_step_reset(&tStep);
    if ( iStatus != XWORK_OK ) {
        xwork_run_step_list_reset(pList);
    }
    return iStatus;
}

size_t xwork_run_get_checkpoint_count(const xwork_run *pRun)
{
    return pRun ? pRun->iCheckpointCount : 0u;
}

xwork_status xwork_run_get_checkpoint(
    const xwork_run *pRun,
    size_t iIndex,
    xwork_checkpoint *pCheckpoint
)
{
    if ( !pRun || !pCheckpoint ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( iIndex >= pRun->iCheckpointCount ) {
        return XWORK_ERROR_NOT_FOUND;
    }

    return xwork__checkpoint_copy(pCheckpoint, &pRun->pCheckpointLog[iIndex].tCheckpoint);
}

const char *xwork_run_get_last_output_text(const xwork_run *pRun)
{
    return pRun ? pRun->sLastOutputText : NULL;
}
