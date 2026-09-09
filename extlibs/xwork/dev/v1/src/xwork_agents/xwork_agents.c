#include "../xwork_core/xwork_internal.h"
#include "../../lib/xllm-session.h"

#include <stdio.h>

struct xwork_agent {
    char *sAgentId;
    char *sDisplayName;
    char *sDescription;
    char *sLlmProfileId;
    char *sSessionProfileId;
    xwork_agent_role eRole;
    xwork_autonomy_mode eAutonomy;
    size_t iMaxTurns;
    size_t iTimeoutMs;
    size_t iMaxRetries;
    xwork_agent *pNext;
};

struct xwork_agent_pool {
    xwork_runtime *pRuntime;
    char *sPoolId;
    xwork_agent *pAgents;
    size_t iAgentCount;
};

typedef struct xwork_task_node_record xwork_task_node_record;
typedef struct xwork_handoff_record xwork_handoff_record;

struct xwork_task_node_record {
    char *sTaskId;
    char *sAgentId;
    char *sRunId;
    char *sParentRunId;
    char *sInstruction;
    char *sLlmProfileId;
    char *sSessionProfileId;
    char **psWorkspaceIds;
    size_t iWorkspaceCount;
    char **psDependencies;
    size_t iDependencyCount;
    size_t iDependencyCapacity;
    xwork_autonomy_mode eAutonomy;
    xwork_session_policy tSessionPolicy;
    void *pUserData;
    size_t iMaxTurns;
    size_t iTimeoutMs;
    size_t iMaxRetries;
    size_t iAttemptCount;
    xwork_task_state eState;
    xwork_status iStatus;
    xwork_run *pRun;
    xthread pThread;
    bool bThreadStarted;
};

struct xwork_handoff_record {
    char *sHandoffId;
    char *sFromTaskId;
    char *sToTaskId;
    char *sReason;
    xwork_handoff_state eState;
    xwork_status iStatus;
    char *sMessage;
    char **psArtifactRefs;
    size_t iArtifactRefCount;
    char **psMemoryContextRefs;
    size_t iMemoryContextRefCount;
    char **psSharedWorkspaceIds;
    size_t iSharedWorkspaceCount;
    bool bReadOnlySharedContext;
    bool bWritableWorkspace;
};

struct xwork_task_graph {
    char *sGraphId;
    xwork_agent_pool *pAgentPool;
    size_t iMaxConcurrency;
    xwork_task_failure_policy eFailurePolicy;
    xllm_cancel_token *pCancelToken;
    xwork_task_execute_fn pfnExecute;
    void *pUserData;
    xwork_task_node_record *pNodes;
    size_t iNodeCount;
    size_t iNodeCapacity;
    xwork_handoff_record *pHandoffs;
    size_t iHandoffCount;
    size_t iHandoffCapacity;
    xmutex_struct tLock;
    bool bLockInitialized;
    bool bCancelRequested;
    char *sCancelReason;
    bool bPauseRequested;
    char *sPauseReason;
    bool bExecuting;
};

static void xwork__task_graph_count_result(
    const xwork_task_graph *pGraph,
    xwork_task_graph_result *pResult
);

static void xwork__agent_free(xwork_agent *pAgent)
{
    if ( !pAgent ) {
        return;
    }
    xwork__free_cstr(&pAgent->sAgentId);
    xwork__free_cstr(&pAgent->sDisplayName);
    xwork__free_cstr(&pAgent->sDescription);
    xwork__free_cstr(&pAgent->sLlmProfileId);
    xwork__free_cstr(&pAgent->sSessionProfileId);
    free(pAgent);
}

static xwork_status xwork__agent_fill_snapshot(
    const xwork_agent *pAgent,
    xwork_agent_snapshot *pSnapshot
)
{
    if ( !pAgent || !pSnapshot ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    xwork_agent_snapshot_init(pSnapshot);
    pSnapshot->sAgentId = xwork__dup_cstr(pAgent->sAgentId);
    pSnapshot->sDisplayName = xwork__dup_cstr(pAgent->sDisplayName);
    pSnapshot->sDescription = xwork__dup_cstr(pAgent->sDescription);
    pSnapshot->sLlmProfileId = xwork__dup_cstr(pAgent->sLlmProfileId);
    pSnapshot->sSessionProfileId = xwork__dup_cstr(pAgent->sSessionProfileId);
    if ( !pSnapshot->sAgentId ||
         (pAgent->sDisplayName && !pSnapshot->sDisplayName) ||
         (pAgent->sDescription && !pSnapshot->sDescription) ||
         (pAgent->sLlmProfileId && !pSnapshot->sLlmProfileId) ||
         (pAgent->sSessionProfileId && !pSnapshot->sSessionProfileId) ) {
        xwork_agent_snapshot_reset(pSnapshot);
        return XWORK_ERROR_NO_MEMORY;
    }
    pSnapshot->eRole = pAgent->eRole;
    pSnapshot->eAutonomy = pAgent->eAutonomy;
    pSnapshot->iMaxTurns = pAgent->iMaxTurns;
    pSnapshot->iTimeoutMs = pAgent->iTimeoutMs;
    pSnapshot->iMaxRetries = pAgent->iMaxRetries;
    return XWORK_OK;
}

static void xwork__task_node_reset(xwork_task_node_record *pNode)
{
    if ( !pNode ) {
        return;
    }
    if ( pNode->pThread ) {
        xrtThreadWait(pNode->pThread);
        xrtThreadDestroy(pNode->pThread);
        pNode->pThread = NULL;
    }
    xwork__free_cstr(&pNode->sTaskId);
    xwork__free_cstr(&pNode->sAgentId);
    xwork__free_cstr(&pNode->sRunId);
    xwork__free_cstr(&pNode->sParentRunId);
    xwork__free_cstr(&pNode->sInstruction);
    xwork__free_cstr(&pNode->sLlmProfileId);
    xwork__free_cstr(&pNode->sSessionProfileId);
    xwork__free_str_array(&pNode->psWorkspaceIds, &pNode->iWorkspaceCount);
    xwork__free_str_array(&pNode->psDependencies, &pNode->iDependencyCount);
    pNode->iDependencyCapacity = 0u;
    memset(pNode, 0, sizeof(*pNode));
}

static xwork_status xwork__copy_string_array(
    const char **psSource,
    size_t iCount,
    char ***ppsTarget
)
{
    char **psCopy;
    size_t i;

    if ( !ppsTarget ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    *ppsTarget = NULL;
    if ( iCount == 0u ) {
        return XWORK_OK;
    }
    if ( !psSource ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    psCopy = (char **)calloc(iCount, sizeof(char *));
    if ( !psCopy ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    for ( i = 0u; i < iCount; ++i ) {
        if ( !psSource[i] || !psSource[i][0] ) {
            xwork__free_str_array(&psCopy, &i);
            return XWORK_ERROR_INVALID_ARGUMENT;
        }
        psCopy[i] = xwork__dup_cstr(psSource[i]);
        if ( !psCopy[i] ) {
            xwork__free_str_array(&psCopy, &i);
            return XWORK_ERROR_NO_MEMORY;
        }
    }
    *ppsTarget = psCopy;
    return XWORK_OK;
}

static void xwork__handoff_record_reset(xwork_handoff_record *pHandoff)
{
    if ( !pHandoff ) {
        return;
    }
    xwork__free_cstr(&pHandoff->sHandoffId);
    xwork__free_cstr(&pHandoff->sFromTaskId);
    xwork__free_cstr(&pHandoff->sToTaskId);
    xwork__free_cstr(&pHandoff->sReason);
    xwork__free_cstr(&pHandoff->sMessage);
    xwork__free_str_array(&pHandoff->psArtifactRefs, &pHandoff->iArtifactRefCount);
    xwork__free_str_array(&pHandoff->psMemoryContextRefs, &pHandoff->iMemoryContextRefCount);
    xwork__free_str_array(&pHandoff->psSharedWorkspaceIds, &pHandoff->iSharedWorkspaceCount);
    memset(pHandoff, 0, sizeof(*pHandoff));
}

static xwork_status xwork__handoff_fill_summary(
    const xwork_handoff_record *pHandoff,
    xwork_handoff_summary *pSummary
)
{
    xwork_status iStatus;

    if ( !pHandoff || !pSummary ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    xwork_handoff_summary_init(pSummary);
    pSummary->sHandoffId = xwork__dup_cstr(pHandoff->sHandoffId);
    pSummary->sFromTaskId = xwork__dup_cstr(pHandoff->sFromTaskId);
    pSummary->sToTaskId = xwork__dup_cstr(pHandoff->sToTaskId);
    pSummary->sReason = xwork__dup_cstr(pHandoff->sReason);
    pSummary->sMessage = xwork__dup_cstr(pHandoff->sMessage);
    if ( !pSummary->sHandoffId ||
         !pSummary->sFromTaskId ||
         !pSummary->sToTaskId ||
         (pHandoff->sReason && !pSummary->sReason) ||
         (pHandoff->sMessage && !pSummary->sMessage) ) {
        xwork_handoff_summary_reset(pSummary);
        return XWORK_ERROR_NO_MEMORY;
    }
    pSummary->eState = pHandoff->eState;
    pSummary->iStatus = pHandoff->iStatus;
    pSummary->bReadOnlySharedContext = pHandoff->bReadOnlySharedContext;
    pSummary->bWritableWorkspace = pHandoff->bWritableWorkspace;

    iStatus = xwork__copy_string_array(
        (const char **)pHandoff->psArtifactRefs,
        pHandoff->iArtifactRefCount,
        (char ***)&pSummary->psArtifactRefs
    );
    if ( iStatus != XWORK_OK ) {
        xwork_handoff_summary_reset(pSummary);
        return iStatus;
    }
    pSummary->iArtifactRefCount = pHandoff->iArtifactRefCount;
    iStatus = xwork__copy_string_array(
        (const char **)pHandoff->psMemoryContextRefs,
        pHandoff->iMemoryContextRefCount,
        (char ***)&pSummary->psMemoryContextRefs
    );
    if ( iStatus != XWORK_OK ) {
        xwork_handoff_summary_reset(pSummary);
        return iStatus;
    }
    pSummary->iMemoryContextRefCount = pHandoff->iMemoryContextRefCount;
    iStatus = xwork__copy_string_array(
        (const char **)pHandoff->psSharedWorkspaceIds,
        pHandoff->iSharedWorkspaceCount,
        (char ***)&pSummary->psSharedWorkspaceIds
    );
    if ( iStatus != XWORK_OK ) {
        xwork_handoff_summary_reset(pSummary);
        return iStatus;
    }
    pSummary->iSharedWorkspaceCount = pHandoff->iSharedWorkspaceCount;
    return XWORK_OK;
}

static xwork_status xwork__task_node_fill_summary(
    const xwork_task_node_record *pNode,
    xwork_task_node_summary *pSummary,
    bool bCopyStrings
)
{
    if ( !pNode || !pSummary ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    xwork_task_node_summary_init(pSummary);
    pSummary->eState = pNode->eState;
    pSummary->iStatus = pNode->iStatus;
    pSummary->iDependencyCount = pNode->iDependencyCount;
    pSummary->iAttemptCount = pNode->iAttemptCount;
    pSummary->iMaxTurns = pNode->iMaxTurns;
    pSummary->iTimeoutMs = pNode->iTimeoutMs;
    pSummary->iMaxRetries = pNode->iMaxRetries;
    pSummary->pUserData = pNode->pUserData;

    if ( !bCopyStrings ) {
        pSummary->sTaskId = pNode->sTaskId;
        pSummary->sAgentId = pNode->sAgentId;
        pSummary->sRunId = pNode->sRunId;
        pSummary->sParentRunId = pNode->sParentRunId;
        pSummary->sInstruction = pNode->sInstruction;
        return XWORK_OK;
    }

    pSummary->sTaskId = xwork__dup_cstr(pNode->sTaskId);
    pSummary->sAgentId = xwork__dup_cstr(pNode->sAgentId);
    pSummary->sRunId = xwork__dup_cstr(pNode->sRunId);
    pSummary->sParentRunId = xwork__dup_cstr(pNode->sParentRunId);
    pSummary->sInstruction = xwork__dup_cstr(pNode->sInstruction);
    if ( !pSummary->sTaskId ||
         (pNode->sAgentId && !pSummary->sAgentId) ||
         (pNode->sRunId && !pSummary->sRunId) ||
         (pNode->sParentRunId && !pSummary->sParentRunId) ||
         (pNode->sInstruction && !pSummary->sInstruction) ) {
        xwork_task_node_summary_reset(pSummary);
        return XWORK_ERROR_NO_MEMORY;
    }
    return XWORK_OK;
}

static xwork_status xwork__task_node_fill_snapshot(
    const xwork_task_node_record *pNode,
    xwork_task_node_snapshot *pSnapshot
)
{
    xwork_status iStatus;

    if ( !pNode || !pSnapshot ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    xwork_task_node_snapshot_init(pSnapshot);
    pSnapshot->sTaskId = xwork__dup_cstr(pNode->sTaskId);
    pSnapshot->sAgentId = xwork__dup_cstr(pNode->sAgentId);
    pSnapshot->sRunId = xwork__dup_cstr(pNode->sRunId);
    pSnapshot->sParentRunId = xwork__dup_cstr(pNode->sParentRunId);
    pSnapshot->sInstruction = xwork__dup_cstr(pNode->sInstruction);
    pSnapshot->sLlmProfileId = xwork__dup_cstr(pNode->sLlmProfileId);
    pSnapshot->sSessionProfileId = xwork__dup_cstr(pNode->sSessionProfileId);
    if ( !pSnapshot->sTaskId ||
         (pNode->sAgentId && !pSnapshot->sAgentId) ||
         (pNode->sRunId && !pSnapshot->sRunId) ||
         (pNode->sParentRunId && !pSnapshot->sParentRunId) ||
         (pNode->sInstruction && !pSnapshot->sInstruction) ||
         (pNode->sLlmProfileId && !pSnapshot->sLlmProfileId) ||
         (pNode->sSessionProfileId && !pSnapshot->sSessionProfileId) ) {
        xwork_task_node_snapshot_reset(pSnapshot);
        return XWORK_ERROR_NO_MEMORY;
    }

    iStatus = xwork__copy_string_array(
        (const char **)pNode->psWorkspaceIds,
        pNode->iWorkspaceCount,
        (char ***)&pSnapshot->psWorkspaceIds
    );
    if ( iStatus != XWORK_OK ) {
        xwork_task_node_snapshot_reset(pSnapshot);
        return iStatus;
    }
    pSnapshot->iWorkspaceCount = pNode->iWorkspaceCount;

    iStatus = xwork__copy_string_array(
        (const char **)pNode->psDependencies,
        pNode->iDependencyCount,
        (char ***)&pSnapshot->psDependencyTaskIds
    );
    if ( iStatus != XWORK_OK ) {
        xwork_task_node_snapshot_reset(pSnapshot);
        return iStatus;
    }
    pSnapshot->iDependencyCount = pNode->iDependencyCount;
    pSnapshot->eAutonomy = pNode->eAutonomy;
    pSnapshot->tSessionPolicy = pNode->tSessionPolicy;
    pSnapshot->eState = pNode->eState;
    pSnapshot->iStatus = pNode->iStatus;
    pSnapshot->iAttemptCount = pNode->iAttemptCount;
    pSnapshot->iMaxTurns = pNode->iMaxTurns;
    pSnapshot->iTimeoutMs = pNode->iTimeoutMs;
    pSnapshot->iMaxRetries = pNode->iMaxRetries;
    return XWORK_OK;
}

static xwork_agent *xwork__agent_pool_find_agent_mutable(
    const xwork_agent_pool *pPool,
    const char *sAgentId
)
{
    xwork_agent *pAgent;

    if ( !pPool || !sAgentId || !sAgentId[0] ) {
        return NULL;
    }
    for ( pAgent = pPool->pAgents; pAgent; pAgent = pAgent->pNext ) {
        if ( pAgent->sAgentId && strcmp(pAgent->sAgentId, sAgentId) == 0 ) {
            return pAgent;
        }
    }
    return NULL;
}

static xwork_task_node_record *xwork__task_graph_find_node(
    const xwork_task_graph *pGraph,
    const char *sTaskId
)
{
    size_t i;

    if ( !pGraph || !sTaskId || !sTaskId[0] ) {
        return NULL;
    }
    for ( i = 0u; i < pGraph->iNodeCount; ++i ) {
        if ( pGraph->pNodes[i].sTaskId &&
             strcmp(pGraph->pNodes[i].sTaskId, sTaskId) == 0 ) {
            return &pGraph->pNodes[i];
        }
    }
    return NULL;
}

static xwork_handoff_record *xwork__task_graph_find_handoff(
    const xwork_task_graph *pGraph,
    const char *sHandoffId
)
{
    size_t i;

    if ( !pGraph || !sHandoffId || !sHandoffId[0] ) {
        return NULL;
    }
    for ( i = 0u; i < pGraph->iHandoffCount; ++i ) {
        if ( pGraph->pHandoffs[i].sHandoffId &&
             strcmp(pGraph->pHandoffs[i].sHandoffId, sHandoffId) == 0 ) {
            return &pGraph->pHandoffs[i];
        }
    }
    return NULL;
}

static bool xwork__task_state_is_terminal(xwork_task_state eState)
{
    return eState == XWORK_TASK_COMPLETED ||
           eState == XWORK_TASK_FAILED ||
           eState == XWORK_TASK_CANCELLED ||
           eState == XWORK_TASK_SKIPPED;
}

static bool xwork__task_state_is_failed_terminal(xwork_task_state eState)
{
    return eState == XWORK_TASK_FAILED ||
           eState == XWORK_TASK_CANCELLED ||
           eState == XWORK_TASK_SKIPPED;
}

static char *xwork__json_escape_dup(const char *sText)
{
    char *sEscaped;
    size_t iLen = 0u;
    size_t iOut = 0u;
    size_t i;

    if ( !sText ) {
        return xwork__dup_cstr("");
    }
    for ( i = 0u; sText[i]; ++i ) {
        switch ( sText[i] ) {
            case '"':
            case '\\':
            case '\b':
            case '\f':
            case '\n':
            case '\r':
            case '\t':
                iLen += 2u;
                break;
            default:
                iLen += ((unsigned char)sText[i] < 0x20u) ? 6u : 1u;
                break;
        }
    }
    sEscaped = (char *)malloc(iLen + 1u);
    if ( !sEscaped ) {
        return NULL;
    }
    for ( i = 0u; sText[i]; ++i ) {
        switch ( sText[i] ) {
            case '"': sEscaped[iOut++] = '\\'; sEscaped[iOut++] = '"'; break;
            case '\\': sEscaped[iOut++] = '\\'; sEscaped[iOut++] = '\\'; break;
            case '\b': sEscaped[iOut++] = '\\'; sEscaped[iOut++] = 'b'; break;
            case '\f': sEscaped[iOut++] = '\\'; sEscaped[iOut++] = 'f'; break;
            case '\n': sEscaped[iOut++] = '\\'; sEscaped[iOut++] = 'n'; break;
            case '\r': sEscaped[iOut++] = '\\'; sEscaped[iOut++] = 'r'; break;
            case '\t': sEscaped[iOut++] = '\\'; sEscaped[iOut++] = 't'; break;
            default:
                if ( (unsigned char)sText[i] < 0x20u ) {
                    snprintf(&sEscaped[iOut], 7u, "\\u%04x", (unsigned int)(unsigned char)sText[i]);
                    iOut += 6u;
                } else {
                    sEscaped[iOut++] = sText[i];
                }
                break;
        }
    }
    sEscaped[iOut] = '\0';
    return sEscaped;
}

static const char *xwork__task_state_cstr(xwork_task_state eState)
{
    switch ( eState ) {
        case XWORK_TASK_PENDING: return "pending";
        case XWORK_TASK_READY: return "ready";
        case XWORK_TASK_RUNNING: return "running";
        case XWORK_TASK_BLOCKED: return "blocked";
        case XWORK_TASK_COMPLETED: return "completed";
        case XWORK_TASK_FAILED: return "failed";
        case XWORK_TASK_CANCELLED: return "cancelled";
        case XWORK_TASK_SKIPPED: return "skipped";
        default: return "unknown";
    }
}

static bool xwork__task_graph_cancel_requested(const xwork_task_graph *pGraph)
{
    xwork_task_graph *pMutableGraph;
    bool bCancelled;

    if ( !pGraph ) {
        return false;
    }
    if ( pGraph->pCancelToken && xllm_cancel_token_is_cancelled(pGraph->pCancelToken) ) {
        return true;
    }

    pMutableGraph = (xwork_task_graph *)pGraph;
    if ( pMutableGraph->bLockInitialized ) {
        xrtMutexLock(&pMutableGraph->tLock);
        bCancelled = pMutableGraph->bCancelRequested;
        xrtMutexUnlock(&pMutableGraph->tLock);
        return bCancelled;
    }
    return pMutableGraph->bCancelRequested;
}

static bool xwork__task_graph_pause_requested(const xwork_task_graph *pGraph)
{
    xwork_task_graph *pMutableGraph;
    bool bPaused;

    if ( !pGraph ) {
        return false;
    }

    pMutableGraph = (xwork_task_graph *)pGraph;
    if ( pMutableGraph->bLockInitialized ) {
        xrtMutexLock(&pMutableGraph->tLock);
        bPaused = pMutableGraph->bPauseRequested;
        xrtMutexUnlock(&pMutableGraph->tLock);
        return bPaused;
    }
    return pMutableGraph->bPauseRequested;
}

void xwork_agent_pool_options_init(xwork_agent_pool_options *pOptions)
{
    if ( pOptions ) {
        memset(pOptions, 0, sizeof(*pOptions));
    }
}

void xwork_agent_options_init(xwork_agent_options *pOptions)
{
    if ( pOptions ) {
        memset(pOptions, 0, sizeof(*pOptions));
        pOptions->eRole = XWORK_AGENT_ROLE_CUSTOM;
        pOptions->eAutonomy = XWORK_AUTONOMY_SEMI_AUTO;
    }
}

void xwork_agent_snapshot_init(xwork_agent_snapshot *pSnapshot)
{
    if ( pSnapshot ) {
        memset(pSnapshot, 0, sizeof(*pSnapshot));
        pSnapshot->eRole = XWORK_AGENT_ROLE_CUSTOM;
        pSnapshot->eAutonomy = XWORK_AUTONOMY_SEMI_AUTO;
    }
}

void xwork_agent_snapshot_reset(xwork_agent_snapshot *pSnapshot)
{
    if ( !pSnapshot ) {
        return;
    }
    xwork__free_cstr((char **)&pSnapshot->sAgentId);
    xwork__free_cstr((char **)&pSnapshot->sDisplayName);
    xwork__free_cstr((char **)&pSnapshot->sDescription);
    xwork__free_cstr((char **)&pSnapshot->sLlmProfileId);
    xwork__free_cstr((char **)&pSnapshot->sSessionProfileId);
    xwork_agent_snapshot_init(pSnapshot);
}

void xwork_agent_snapshot_list_init(xwork_agent_snapshot_list *pList)
{
    if ( pList ) {
        memset(pList, 0, sizeof(*pList));
    }
}

void xwork_agent_snapshot_list_reset(xwork_agent_snapshot_list *pList)
{
    size_t i;

    if ( !pList ) {
        return;
    }
    for ( i = 0u; i < pList->iCount; ++i ) {
        xwork_agent_snapshot_reset(&pList->pItems[i]);
    }
    free(pList->pItems);
    xwork_agent_snapshot_list_init(pList);
}

void xwork_agent_pool_snapshot_init(xwork_agent_pool_snapshot *pSnapshot)
{
    if ( pSnapshot ) {
        memset(pSnapshot, 0, sizeof(*pSnapshot));
        xwork_agent_snapshot_list_init(&pSnapshot->tAgents);
    }
}

void xwork_agent_pool_snapshot_reset(xwork_agent_pool_snapshot *pSnapshot)
{
    if ( !pSnapshot ) {
        return;
    }
    xwork__free_cstr((char **)&pSnapshot->sPoolId);
    xwork_agent_snapshot_list_reset(&pSnapshot->tAgents);
    xwork_agent_pool_snapshot_init(pSnapshot);
}

void xwork_task_node_options_init(xwork_task_node_options *pOptions)
{
    if ( pOptions ) {
        memset(pOptions, 0, sizeof(*pOptions));
        pOptions->eAutonomy = XWORK_AUTONOMY_SEMI_AUTO;
        xwork_session_policy_init(&pOptions->tSessionPolicy);
    }
}

void xwork_task_graph_options_init(xwork_task_graph_options *pOptions)
{
    if ( pOptions ) {
        memset(pOptions, 0, sizeof(*pOptions));
        pOptions->iMaxConcurrency = 1u;
        pOptions->eFailurePolicy = XWORK_TASK_FAILURE_FAIL_FAST;
    }
}

void xwork_task_node_summary_init(xwork_task_node_summary *pSummary)
{
    if ( pSummary ) {
        memset(pSummary, 0, sizeof(*pSummary));
        pSummary->eState = XWORK_TASK_PENDING;
        pSummary->iStatus = XWORK_OK;
    }
}

void xwork_task_node_summary_reset(xwork_task_node_summary *pSummary)
{
    if ( !pSummary ) {
        return;
    }
    xwork__free_cstr((char **)&pSummary->sTaskId);
    xwork__free_cstr((char **)&pSummary->sAgentId);
    xwork__free_cstr((char **)&pSummary->sRunId);
    xwork__free_cstr((char **)&pSummary->sParentRunId);
    xwork__free_cstr((char **)&pSummary->sInstruction);
    xwork_task_node_summary_init(pSummary);
}

void xwork_task_node_summary_list_init(xwork_task_node_summary_list *pList)
{
    if ( pList ) {
        memset(pList, 0, sizeof(*pList));
    }
}

void xwork_task_node_summary_list_reset(xwork_task_node_summary_list *pList)
{
    size_t i;

    if ( !pList ) {
        return;
    }
    for ( i = 0u; i < pList->iCount; ++i ) {
        xwork_task_node_summary_reset(&pList->pItems[i]);
    }
    free(pList->pItems);
    xwork_task_node_summary_list_init(pList);
}

void xwork_task_node_snapshot_init(xwork_task_node_snapshot *pSnapshot)
{
    if ( pSnapshot ) {
        memset(pSnapshot, 0, sizeof(*pSnapshot));
        pSnapshot->eState = XWORK_TASK_PENDING;
        pSnapshot->iStatus = XWORK_OK;
        xwork_session_policy_init(&pSnapshot->tSessionPolicy);
    }
}

void xwork_task_node_snapshot_reset(xwork_task_node_snapshot *pSnapshot)
{
    if ( !pSnapshot ) {
        return;
    }
    xwork__free_cstr((char **)&pSnapshot->sTaskId);
    xwork__free_cstr((char **)&pSnapshot->sAgentId);
    xwork__free_cstr((char **)&pSnapshot->sRunId);
    xwork__free_cstr((char **)&pSnapshot->sParentRunId);
    xwork__free_cstr((char **)&pSnapshot->sInstruction);
    xwork__free_cstr((char **)&pSnapshot->sLlmProfileId);
    xwork__free_cstr((char **)&pSnapshot->sSessionProfileId);
    xwork__free_str_array((char ***)&pSnapshot->psWorkspaceIds, &pSnapshot->iWorkspaceCount);
    xwork__free_str_array((char ***)&pSnapshot->psDependencyTaskIds, &pSnapshot->iDependencyCount);
    xwork_task_node_snapshot_init(pSnapshot);
}

void xwork_task_node_snapshot_list_init(xwork_task_node_snapshot_list *pList)
{
    if ( pList ) {
        memset(pList, 0, sizeof(*pList));
    }
}

void xwork_task_node_snapshot_list_reset(xwork_task_node_snapshot_list *pList)
{
    size_t i;

    if ( !pList ) {
        return;
    }
    for ( i = 0u; i < pList->iCount; ++i ) {
        xwork_task_node_snapshot_reset(&pList->pItems[i]);
    }
    free(pList->pItems);
    xwork_task_node_snapshot_list_init(pList);
}

void xwork_task_graph_result_init(xwork_task_graph_result *pResult)
{
    if ( pResult ) {
        memset(pResult, 0, sizeof(*pResult));
        pResult->iStatus = XWORK_OK;
    }
}

void xwork_task_graph_snapshot_init(xwork_task_graph_snapshot *pSnapshot)
{
    if ( pSnapshot ) {
        memset(pSnapshot, 0, sizeof(*pSnapshot));
        xwork_task_graph_result_init(&pSnapshot->tResult);
        xwork_task_node_snapshot_list_init(&pSnapshot->tNodes);
        xwork_handoff_summary_list_init(&pSnapshot->tHandoffs);
    }
}

void xwork_task_graph_snapshot_reset(xwork_task_graph_snapshot *pSnapshot)
{
    if ( !pSnapshot ) {
        return;
    }
    xwork__free_cstr((char **)&pSnapshot->sGraphId);
    xwork__free_cstr((char **)&pSnapshot->sCancelReason);
    xwork__free_cstr((char **)&pSnapshot->sPauseReason);
    xwork_task_node_snapshot_list_reset(&pSnapshot->tNodes);
    xwork_handoff_summary_list_reset(&pSnapshot->tHandoffs);
    xwork_task_graph_snapshot_init(pSnapshot);
}

void xwork_handoff_request_options_init(xwork_handoff_request_options *pOptions)
{
    if ( pOptions ) {
        memset(pOptions, 0, sizeof(*pOptions));
        pOptions->bReadOnlySharedContext = true;
    }
}

void xwork_handoff_result_options_init(xwork_handoff_result_options *pOptions)
{
    if ( pOptions ) {
        memset(pOptions, 0, sizeof(*pOptions));
        pOptions->eState = XWORK_HANDOFF_COMPLETED;
        pOptions->iStatus = XWORK_OK;
    }
}

void xwork_handoff_summary_init(xwork_handoff_summary *pSummary)
{
    if ( pSummary ) {
        memset(pSummary, 0, sizeof(*pSummary));
        pSummary->eState = XWORK_HANDOFF_PENDING;
        pSummary->iStatus = XWORK_OK;
        pSummary->bReadOnlySharedContext = true;
    }
}

void xwork_handoff_summary_reset(xwork_handoff_summary *pSummary)
{
    if ( !pSummary ) {
        return;
    }
    xwork__free_cstr((char **)&pSummary->sHandoffId);
    xwork__free_cstr((char **)&pSummary->sFromTaskId);
    xwork__free_cstr((char **)&pSummary->sToTaskId);
    xwork__free_cstr((char **)&pSummary->sReason);
    xwork__free_cstr((char **)&pSummary->sMessage);
    xwork__free_str_array((char ***)&pSummary->psArtifactRefs, &pSummary->iArtifactRefCount);
    xwork__free_str_array((char ***)&pSummary->psMemoryContextRefs, &pSummary->iMemoryContextRefCount);
    xwork__free_str_array((char ***)&pSummary->psSharedWorkspaceIds, &pSummary->iSharedWorkspaceCount);
    xwork_handoff_summary_init(pSummary);
}

void xwork_handoff_summary_list_init(xwork_handoff_summary_list *pList)
{
    if ( pList ) {
        memset(pList, 0, sizeof(*pList));
    }
}

void xwork_handoff_summary_list_reset(xwork_handoff_summary_list *pList)
{
    size_t i;

    if ( !pList ) {
        return;
    }
    for ( i = 0u; i < pList->iCount; ++i ) {
        xwork_handoff_summary_reset(&pList->pItems[i]);
    }
    free(pList->pItems);
    xwork_handoff_summary_list_init(pList);
}

xwork_status xwork_agent_pool_create(
    const xwork_agent_pool_options *pOptions,
    xwork_agent_pool **ppPool
)
{
    xwork_agent_pool *pPool;

    if ( !pOptions || !pOptions->pRuntime || !ppPool ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    *ppPool = NULL;
    pPool = (xwork_agent_pool *)calloc(1u, sizeof(*pPool));
    if ( !pPool ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    pPool->pRuntime = pOptions->pRuntime;
    pPool->sPoolId = xwork__dup_cstr(
        pOptions->sPoolId && pOptions->sPoolId[0] ? pOptions->sPoolId : "default"
    );
    if ( !pPool->sPoolId ) {
        xwork_agent_pool_destroy(pPool);
        return XWORK_ERROR_NO_MEMORY;
    }
    *ppPool = pPool;
    return XWORK_OK;
}

xwork_status xwork_agent_pool_create_from_snapshot(
    xwork_runtime *pRuntime,
    const xwork_agent_pool_snapshot *pSnapshot,
    xwork_agent_pool **ppPool
)
{
    xwork_agent_pool_options tPoolOptions;
    xwork_agent_options tAgentOptions;
    xwork_agent_pool *pPool;
    xwork_status iStatus;
    size_t i;

    if ( !pRuntime || !pSnapshot || !pSnapshot->sPoolId || !ppPool ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    *ppPool = NULL;
    xwork_agent_pool_options_init(&tPoolOptions);
    tPoolOptions.sPoolId = pSnapshot->sPoolId;
    tPoolOptions.pRuntime = pRuntime;
    iStatus = xwork_agent_pool_create(&tPoolOptions, &pPool);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    for ( i = 0u; i < pSnapshot->tAgents.iCount; ++i ) {
        const xwork_agent_snapshot *pAgent = &pSnapshot->tAgents.pItems[i];

        xwork_agent_options_init(&tAgentOptions);
        tAgentOptions.sAgentId = pAgent->sAgentId;
        tAgentOptions.sDisplayName = pAgent->sDisplayName;
        tAgentOptions.sDescription = pAgent->sDescription;
        tAgentOptions.eRole = pAgent->eRole;
        tAgentOptions.sLlmProfileId = pAgent->sLlmProfileId;
        tAgentOptions.sSessionProfileId = pAgent->sSessionProfileId;
        tAgentOptions.eAutonomy = pAgent->eAutonomy;
        tAgentOptions.iMaxTurns = pAgent->iMaxTurns;
        tAgentOptions.iTimeoutMs = pAgent->iTimeoutMs;
        tAgentOptions.iMaxRetries = pAgent->iMaxRetries;
        iStatus = xwork_agent_pool_add_agent(pPool, &tAgentOptions, NULL);
        if ( iStatus != XWORK_OK ) {
            xwork_agent_pool_destroy(pPool);
            return iStatus;
        }
    }

    *ppPool = pPool;
    return XWORK_OK;
}

void xwork_agent_pool_destroy(xwork_agent_pool *pPool)
{
    xwork_agent *pAgent;
    xwork_agent *pNext;

    if ( !pPool ) {
        return;
    }
    pAgent = pPool->pAgents;
    while ( pAgent ) {
        pNext = pAgent->pNext;
        xwork__agent_free(pAgent);
        pAgent = pNext;
    }
    xwork__free_cstr(&pPool->sPoolId);
    free(pPool);
}

xwork_status xwork_agent_pool_add_agent(
    xwork_agent_pool *pPool,
    const xwork_agent_options *pOptions,
    xwork_agent **ppAgent
)
{
    xwork_agent *pAgent;

    if ( ppAgent ) {
        *ppAgent = NULL;
    }
    if ( !pPool || !pOptions || !pOptions->sAgentId || !pOptions->sAgentId[0] ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( xwork__agent_pool_find_agent_mutable(pPool, pOptions->sAgentId) ) {
        return XWORK_ERROR_ALREADY_EXISTS;
    }

    pAgent = (xwork_agent *)calloc(1u, sizeof(*pAgent));
    if ( !pAgent ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    pAgent->sAgentId = xwork__dup_cstr(pOptions->sAgentId);
    pAgent->sDisplayName = xwork__dup_cstr(pOptions->sDisplayName);
    pAgent->sDescription = xwork__dup_cstr(pOptions->sDescription);
    pAgent->sLlmProfileId = xwork__dup_cstr(pOptions->sLlmProfileId);
    pAgent->sSessionProfileId = xwork__dup_cstr(pOptions->sSessionProfileId);
    pAgent->eRole = pOptions->eRole;
    pAgent->eAutonomy = pOptions->eAutonomy;
    pAgent->iMaxTurns = pOptions->iMaxTurns;
    pAgent->iTimeoutMs = pOptions->iTimeoutMs;
    pAgent->iMaxRetries = pOptions->iMaxRetries;
    if ( !pAgent->sAgentId ||
         (pOptions->sDisplayName && !pAgent->sDisplayName) ||
         (pOptions->sDescription && !pAgent->sDescription) ||
         (pOptions->sLlmProfileId && !pAgent->sLlmProfileId) ||
         (pOptions->sSessionProfileId && !pAgent->sSessionProfileId) ) {
        xwork__agent_free(pAgent);
        return XWORK_ERROR_NO_MEMORY;
    }

    pAgent->pNext = pPool->pAgents;
    pPool->pAgents = pAgent;
    ++pPool->iAgentCount;
    if ( ppAgent ) {
        *ppAgent = pAgent;
    }
    return XWORK_OK;
}

size_t xwork_agent_pool_get_agent_count(const xwork_agent_pool *pPool)
{
    return pPool ? pPool->iAgentCount : 0u;
}

xwork_agent *xwork_agent_pool_find_agent(
    const xwork_agent_pool *pPool,
    const char *sAgentId
)
{
    return xwork__agent_pool_find_agent_mutable(pPool, sAgentId);
}

xwork_status xwork_agent_pool_get_snapshot(
    const xwork_agent_pool *pPool,
    xwork_agent_pool_snapshot *pSnapshot
)
{
    const xwork_agent *pAgent;
    size_t i;
    xwork_status iStatus;

    if ( !pPool || !pSnapshot ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_agent_pool_snapshot_reset(pSnapshot);
    pSnapshot->sPoolId = xwork__dup_cstr(pPool->sPoolId);
    if ( pPool->sPoolId && !pSnapshot->sPoolId ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    if ( pPool->iAgentCount == 0u ) {
        return XWORK_OK;
    }
    pSnapshot->tAgents.pItems = (xwork_agent_snapshot *)calloc(
        pPool->iAgentCount,
        sizeof(*pSnapshot->tAgents.pItems)
    );
    if ( !pSnapshot->tAgents.pItems ) {
        xwork_agent_pool_snapshot_reset(pSnapshot);
        return XWORK_ERROR_NO_MEMORY;
    }
    pSnapshot->tAgents.iCount = pPool->iAgentCount;
    for ( pAgent = pPool->pAgents, i = 0u; pAgent; pAgent = pAgent->pNext, ++i ) {
        iStatus = xwork__agent_fill_snapshot(pAgent, &pSnapshot->tAgents.pItems[i]);
        if ( iStatus != XWORK_OK ) {
            xwork_agent_pool_snapshot_reset(pSnapshot);
            return iStatus;
        }
    }
    return XWORK_OK;
}

const char *xwork_agent_get_id(const xwork_agent *pAgent)
{
    return pAgent ? pAgent->sAgentId : NULL;
}

xwork_agent_role xwork_agent_get_role(const xwork_agent *pAgent)
{
    return pAgent ? pAgent->eRole : XWORK_AGENT_ROLE_CUSTOM;
}

xwork_status xwork_task_graph_create(
    const xwork_task_graph_options *pOptions,
    xwork_task_graph **ppGraph
)
{
    xwork_task_graph *pGraph;

    if ( !pOptions || !pOptions->pAgentPool || !ppGraph ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    *ppGraph = NULL;
    pGraph = (xwork_task_graph *)calloc(1u, sizeof(*pGraph));
    if ( !pGraph ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    pGraph->sGraphId = xwork__dup_cstr(
        pOptions->sGraphId && pOptions->sGraphId[0] ? pOptions->sGraphId : "graph"
    );
    if ( !pGraph->sGraphId ) {
        xwork_task_graph_destroy(pGraph);
        return XWORK_ERROR_NO_MEMORY;
    }
    pGraph->pAgentPool = pOptions->pAgentPool;
    pGraph->iMaxConcurrency = pOptions->iMaxConcurrency ? pOptions->iMaxConcurrency : 1u;
    pGraph->eFailurePolicy = pOptions->eFailurePolicy;
    pGraph->pCancelToken = pOptions->pCancelToken;
    pGraph->pfnExecute = pOptions->pfnExecute;
    pGraph->pUserData = pOptions->pUserData;
    xrtMutexInit(&pGraph->tLock);
    pGraph->bLockInitialized = true;
    *ppGraph = pGraph;
    return XWORK_OK;
}

xwork_status xwork_task_graph_create_from_snapshot(
    const xwork_task_graph_options *pOptions,
    const xwork_task_graph_snapshot *pSnapshot,
    xwork_task_graph **ppGraph
)
{
    xwork_task_graph_options tOptions;
    xwork_task_node_options tNodeOptions;
    xwork_task_graph *pGraph;
    xwork_status iStatus;
    size_t i;
    size_t j;
    bool bOverrideGraphId;

    if ( !pOptions || !pSnapshot || !ppGraph ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    *ppGraph = NULL;
    tOptions = *pOptions;
    bOverrideGraphId = tOptions.sGraphId && tOptions.sGraphId[0] &&
        (!pSnapshot->sGraphId || strcmp(tOptions.sGraphId, pSnapshot->sGraphId) != 0);
    if ( !tOptions.sGraphId || !tOptions.sGraphId[0] ) {
        tOptions.sGraphId = pSnapshot->sGraphId;
    }
    iStatus = xwork_task_graph_create(&tOptions, &pGraph);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    for ( i = 0u; i < pSnapshot->tNodes.iCount; ++i ) {
        const xwork_task_node_snapshot *pNode = &pSnapshot->tNodes.pItems[i];

        xwork_task_node_options_init(&tNodeOptions);
        tNodeOptions.sTaskId = pNode->sTaskId;
        tNodeOptions.sAgentId = pNode->sAgentId;
        tNodeOptions.sRunId = bOverrideGraphId ? NULL : pNode->sRunId;
        tNodeOptions.sParentRunId = pNode->sParentRunId;
        tNodeOptions.sInstruction = pNode->sInstruction;
        tNodeOptions.sLlmProfileId = pNode->sLlmProfileId;
        tNodeOptions.sSessionProfileId = pNode->sSessionProfileId;
        tNodeOptions.psWorkspaceIds = pNode->psWorkspaceIds;
        tNodeOptions.iWorkspaceCount = pNode->iWorkspaceCount;
        tNodeOptions.eAutonomy = pNode->eAutonomy;
        tNodeOptions.tSessionPolicy = pNode->tSessionPolicy;
        iStatus = xwork_task_graph_add_node(pGraph, &tNodeOptions);
        if ( iStatus != XWORK_OK ) {
            xwork_task_graph_destroy(pGraph);
            return iStatus;
        }
        pGraph->pNodes[pGraph->iNodeCount - 1u].eState = pNode->eState;
        pGraph->pNodes[pGraph->iNodeCount - 1u].iStatus = pNode->iStatus;
        pGraph->pNodes[pGraph->iNodeCount - 1u].iAttemptCount = pNode->iAttemptCount;
        if ( pNode->iMaxTurns || pNode->iTimeoutMs ) {
            pGraph->pNodes[pGraph->iNodeCount - 1u].iMaxTurns = pNode->iMaxTurns;
            pGraph->pNodes[pGraph->iNodeCount - 1u].iTimeoutMs = pNode->iTimeoutMs;
        }
        pGraph->pNodes[pGraph->iNodeCount - 1u].iMaxRetries = pNode->iMaxRetries;
    }

    for ( i = 0u; i < pSnapshot->tNodes.iCount; ++i ) {
        const xwork_task_node_snapshot *pNode = &pSnapshot->tNodes.pItems[i];

        for ( j = 0u; j < pNode->iDependencyCount; ++j ) {
            iStatus = xwork_task_graph_add_dependency(
                pGraph,
                pNode->psDependencyTaskIds[j],
                pNode->sTaskId
            );
            if ( iStatus != XWORK_OK ) {
                xwork_task_graph_destroy(pGraph);
                return iStatus;
            }
        }
    }

    for ( i = 0u; i < pSnapshot->tHandoffs.iCount; ++i ) {
        const xwork_handoff_summary *pHandoff = &pSnapshot->tHandoffs.pItems[i];
        xwork_handoff_request_options tHandoffOptions;
        xwork_handoff_result_options tHandoffResult;

        xwork_handoff_request_options_init(&tHandoffOptions);
        tHandoffOptions.sHandoffId = pHandoff->sHandoffId;
        tHandoffOptions.sFromTaskId = pHandoff->sFromTaskId;
        tHandoffOptions.sToTaskId = pHandoff->sToTaskId;
        tHandoffOptions.sReason = pHandoff->sReason;
        tHandoffOptions.psArtifactRefs = pHandoff->psArtifactRefs;
        tHandoffOptions.iArtifactRefCount = pHandoff->iArtifactRefCount;
        tHandoffOptions.psMemoryContextRefs = pHandoff->psMemoryContextRefs;
        tHandoffOptions.iMemoryContextRefCount = pHandoff->iMemoryContextRefCount;
        tHandoffOptions.psSharedWorkspaceIds = pHandoff->psSharedWorkspaceIds;
        tHandoffOptions.iSharedWorkspaceCount = pHandoff->iSharedWorkspaceCount;
        tHandoffOptions.bReadOnlySharedContext = pHandoff->bReadOnlySharedContext;
        tHandoffOptions.bWritableWorkspace = pHandoff->bWritableWorkspace;
        iStatus = xwork_task_graph_request_handoff(pGraph, &tHandoffOptions, NULL);
        if ( iStatus != XWORK_OK ) {
            xwork_task_graph_destroy(pGraph);
            return iStatus;
        }
        if ( pHandoff->eState != XWORK_HANDOFF_PENDING ) {
            xwork_handoff_result_options_init(&tHandoffResult);
            tHandoffResult.sHandoffId = pHandoff->sHandoffId;
            tHandoffResult.eState = pHandoff->eState;
            tHandoffResult.iStatus = pHandoff->iStatus;
            tHandoffResult.sMessage = pHandoff->sMessage;
            iStatus = xwork_task_graph_resolve_handoff(pGraph, &tHandoffResult, NULL);
            if ( iStatus != XWORK_OK ) {
                xwork_task_graph_destroy(pGraph);
                return iStatus;
            }
        }
    }

    if ( pSnapshot->bCancelRequested ) {
        iStatus = xwork_task_graph_cancel(pGraph, pSnapshot->sCancelReason);
        if ( iStatus != XWORK_OK ) {
            xwork_task_graph_destroy(pGraph);
            return iStatus;
        }
    }
    if ( pSnapshot->bPauseRequested ) {
        iStatus = xwork_task_graph_pause(pGraph, pSnapshot->sPauseReason);
        if ( iStatus != XWORK_OK ) {
            xwork_task_graph_destroy(pGraph);
            return iStatus;
        }
    }
    *ppGraph = pGraph;
    return XWORK_OK;
}

void xwork_task_graph_destroy(xwork_task_graph *pGraph)
{
    size_t i;

    if ( !pGraph ) {
        return;
    }
    for ( i = 0u; i < pGraph->iNodeCount; ++i ) {
        xwork__task_node_reset(&pGraph->pNodes[i]);
    }
    free(pGraph->pNodes);
    for ( i = 0u; i < pGraph->iHandoffCount; ++i ) {
        xwork__handoff_record_reset(&pGraph->pHandoffs[i]);
    }
    free(pGraph->pHandoffs);
    if ( pGraph->bLockInitialized ) {
        xrtMutexUnit(&pGraph->tLock);
    }
    xwork__free_cstr(&pGraph->sCancelReason);
    xwork__free_cstr(&pGraph->sPauseReason);
    xwork__free_cstr(&pGraph->sGraphId);
    free(pGraph);
}

static xwork_status xwork__task_graph_reserve_node(xwork_task_graph *pGraph)
{
    xwork_task_node_record *pNewNodes;
    size_t iNewCapacity;

    if ( pGraph->iNodeCount < pGraph->iNodeCapacity ) {
        return XWORK_OK;
    }
    iNewCapacity = pGraph->iNodeCapacity ? pGraph->iNodeCapacity * 2u : 4u;
    pNewNodes = (xwork_task_node_record *)realloc(
        pGraph->pNodes,
        iNewCapacity * sizeof(*pNewNodes)
    );
    if ( !pNewNodes ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    memset(
        pNewNodes + pGraph->iNodeCapacity,
        0,
        (iNewCapacity - pGraph->iNodeCapacity) * sizeof(*pNewNodes)
    );
    pGraph->pNodes = pNewNodes;
    pGraph->iNodeCapacity = iNewCapacity;
    return XWORK_OK;
}

static xwork_status xwork__task_graph_reserve_handoff(xwork_task_graph *pGraph)
{
    xwork_handoff_record *pNewHandoffs;
    size_t iNewCapacity;

    if ( pGraph->iHandoffCount < pGraph->iHandoffCapacity ) {
        return XWORK_OK;
    }
    iNewCapacity = pGraph->iHandoffCapacity ? pGraph->iHandoffCapacity * 2u : 4u;
    pNewHandoffs = (xwork_handoff_record *)realloc(
        pGraph->pHandoffs,
        iNewCapacity * sizeof(*pNewHandoffs)
    );
    if ( !pNewHandoffs ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    memset(
        pNewHandoffs + pGraph->iHandoffCapacity,
        0,
        (iNewCapacity - pGraph->iHandoffCapacity) * sizeof(*pNewHandoffs)
    );
    pGraph->pHandoffs = pNewHandoffs;
    pGraph->iHandoffCapacity = iNewCapacity;
    return XWORK_OK;
}

xwork_status xwork_task_graph_add_node(
    xwork_task_graph *pGraph,
    const xwork_task_node_options *pOptions
)
{
    xwork_task_node_record *pNode;
    xwork_agent *pAgent;
    xwork_status iStatus;

    if ( !pGraph ||
         !pOptions ||
         !pOptions->sTaskId ||
         !pOptions->sTaskId[0] ||
         !pOptions->sInstruction ||
         !pOptions->sInstruction[0] ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( pGraph->bExecuting ) {
        return XWORK_ERROR_INVALID_STATE;
    }
    if ( xwork__task_graph_find_node(pGraph, pOptions->sTaskId) ) {
        return XWORK_ERROR_ALREADY_EXISTS;
    }
    if ( pOptions->sAgentId && pOptions->sAgentId[0] &&
         !xwork__agent_pool_find_agent_mutable(pGraph->pAgentPool, pOptions->sAgentId) ) {
        return XWORK_ERROR_NOT_FOUND;
    }

    iStatus = xwork__task_graph_reserve_node(pGraph);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    pNode = &pGraph->pNodes[pGraph->iNodeCount];
    memset(pNode, 0, sizeof(*pNode));

    pAgent = xwork__agent_pool_find_agent_mutable(pGraph->pAgentPool, pOptions->sAgentId);
    pNode->sTaskId = xwork__dup_cstr(pOptions->sTaskId);
    pNode->sAgentId = xwork__dup_cstr(pOptions->sAgentId);
    pNode->sRunId = pOptions->sRunId && pOptions->sRunId[0]
        ? xwork__dup_cstr(pOptions->sRunId)
        : xwork__dup_printf("%s:%s", pGraph->sGraphId, pOptions->sTaskId);
    pNode->sParentRunId = xwork__dup_cstr(pOptions->sParentRunId);
    pNode->sInstruction = xwork__dup_cstr(pOptions->sInstruction);
    pNode->sLlmProfileId = xwork__dup_cstr(
        pOptions->sLlmProfileId ? pOptions->sLlmProfileId :
        (pAgent ? pAgent->sLlmProfileId : NULL)
    );
    pNode->sSessionProfileId = xwork__dup_cstr(
        pOptions->sSessionProfileId ? pOptions->sSessionProfileId :
        (pAgent ? pAgent->sSessionProfileId : NULL)
    );
    pNode->eAutonomy = pOptions->eAutonomy;
    pNode->tSessionPolicy = pOptions->tSessionPolicy;
    pNode->pUserData = pOptions->pUserData;
    pNode->iMaxTurns = pAgent ? pAgent->iMaxTurns : 0u;
    pNode->iTimeoutMs = pAgent ? pAgent->iTimeoutMs : 0u;
    pNode->iMaxRetries = pAgent ? pAgent->iMaxRetries : 0u;
    pNode->eState = XWORK_TASK_PENDING;
    pNode->iStatus = XWORK_OK;

    iStatus = xwork__copy_string_array(
        pOptions->psWorkspaceIds,
        pOptions->iWorkspaceCount,
        &pNode->psWorkspaceIds
    );
    if ( iStatus != XWORK_OK ||
         !pNode->sTaskId ||
         !pNode->sRunId ||
         !pNode->sInstruction ||
         (pOptions->sAgentId && !pNode->sAgentId) ||
         (pOptions->sParentRunId && !pNode->sParentRunId) ||
         ((pOptions->sLlmProfileId || (pAgent && pAgent->sLlmProfileId)) && !pNode->sLlmProfileId) ||
         ((pOptions->sSessionProfileId || (pAgent && pAgent->sSessionProfileId)) && !pNode->sSessionProfileId) ) {
        if ( iStatus == XWORK_OK ) {
            iStatus = XWORK_ERROR_NO_MEMORY;
        }
        xwork__task_node_reset(pNode);
        return iStatus;
    }
    pNode->iWorkspaceCount = pOptions->iWorkspaceCount;
    ++pGraph->iNodeCount;
    return XWORK_OK;
}

xwork_status xwork_task_graph_add_dependency(
    xwork_task_graph *pGraph,
    const char *sBeforeTaskId,
    const char *sAfterTaskId
)
{
    xwork_task_node_record *pAfter;
    char **psNewDependencies;
    size_t i;
    size_t iNewCapacity;

    if ( !pGraph ||
         !sBeforeTaskId ||
         !sBeforeTaskId[0] ||
         !sAfterTaskId ||
         !sAfterTaskId[0] ||
         strcmp(sBeforeTaskId, sAfterTaskId) == 0 ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( pGraph->bExecuting ) {
        return XWORK_ERROR_INVALID_STATE;
    }
    if ( !xwork__task_graph_find_node(pGraph, sBeforeTaskId) ) {
        return XWORK_ERROR_NOT_FOUND;
    }
    pAfter = xwork__task_graph_find_node(pGraph, sAfterTaskId);
    if ( !pAfter ) {
        return XWORK_ERROR_NOT_FOUND;
    }
    for ( i = 0u; i < pAfter->iDependencyCount; ++i ) {
        if ( strcmp(pAfter->psDependencies[i], sBeforeTaskId) == 0 ) {
            return XWORK_ERROR_ALREADY_EXISTS;
        }
    }
    if ( pAfter->iDependencyCount == pAfter->iDependencyCapacity ) {
        iNewCapacity = pAfter->iDependencyCapacity ? pAfter->iDependencyCapacity * 2u : 2u;
        psNewDependencies = (char **)realloc(
            pAfter->psDependencies,
            iNewCapacity * sizeof(*psNewDependencies)
        );
        if ( !psNewDependencies ) {
            return XWORK_ERROR_NO_MEMORY;
        }
        pAfter->psDependencies = psNewDependencies;
        pAfter->iDependencyCapacity = iNewCapacity;
    }
    pAfter->psDependencies[pAfter->iDependencyCount] = xwork__dup_cstr(sBeforeTaskId);
    if ( !pAfter->psDependencies[pAfter->iDependencyCount] ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    ++pAfter->iDependencyCount;
    return XWORK_OK;
}

size_t xwork_task_graph_get_node_count(const xwork_task_graph *pGraph)
{
    return pGraph ? pGraph->iNodeCount : 0u;
}

xwork_status xwork_task_graph_get_node_summary(
    const xwork_task_graph *pGraph,
    const char *sTaskId,
    xwork_task_node_summary *pSummary
)
{
    xwork_task_node_record *pNode;

    if ( !pSummary ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    pNode = xwork__task_graph_find_node(pGraph, sTaskId);
    if ( !pNode ) {
        return XWORK_ERROR_NOT_FOUND;
    }
    return xwork__task_node_fill_summary(pNode, pSummary, true);
}

xwork_status xwork_task_graph_list_node_summaries(
    const xwork_task_graph *pGraph,
    xwork_task_node_summary_list *pList
)
{
    size_t i;
    xwork_status iStatus;

    if ( !pGraph || !pList ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    xwork_task_node_summary_list_reset(pList);
    if ( pGraph->iNodeCount == 0u ) {
        return XWORK_OK;
    }
    pList->pItems = (xwork_task_node_summary *)calloc(
        pGraph->iNodeCount,
        sizeof(*pList->pItems)
    );
    if ( !pList->pItems ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    pList->iCount = pGraph->iNodeCount;
    for ( i = 0u; i < pGraph->iNodeCount; ++i ) {
        iStatus = xwork__task_node_fill_summary(
            &pGraph->pNodes[i],
            &pList->pItems[i],
            true
        );
        if ( iStatus != XWORK_OK ) {
            xwork_task_node_summary_list_reset(pList);
            return iStatus;
        }
    }
    return XWORK_OK;
}

xwork_run *xwork_task_graph_get_node_run(
    const xwork_task_graph *pGraph,
    const char *sTaskId
)
{
    xwork_task_node_record *pNode = xwork__task_graph_find_node(pGraph, sTaskId);
    return pNode ? pNode->pRun : NULL;
}

xwork_status xwork_task_graph_get_snapshot(
    const xwork_task_graph *pGraph,
    xwork_task_graph_snapshot *pSnapshot
)
{
    xwork_task_graph *pMutableGraph;
    xwork_status iStatus;
    bool bCancelRequested;
    const char *sCancelReason;
    bool bPauseRequested;
    const char *sPauseReason;
    size_t i;

    if ( !pGraph || !pSnapshot ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_task_graph_snapshot_reset(pSnapshot);
    pSnapshot->sGraphId = xwork__dup_cstr(pGraph->sGraphId);
    if ( pGraph->sGraphId && !pSnapshot->sGraphId ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    pSnapshot->iMaxConcurrency = pGraph->iMaxConcurrency;
    pSnapshot->eFailurePolicy = pGraph->eFailurePolicy;

    pMutableGraph = (xwork_task_graph *)pGraph;
    if ( pMutableGraph->bLockInitialized ) {
        xrtMutexLock(&pMutableGraph->tLock);
    }
    bCancelRequested = pGraph->bCancelRequested;
    sCancelReason = pGraph->sCancelReason;
    bPauseRequested = pGraph->bPauseRequested;
    sPauseReason = pGraph->sPauseReason;
    if ( pGraph->pCancelToken && xllm_cancel_token_is_cancelled(pGraph->pCancelToken) ) {
        bCancelRequested = true;
    }
    pSnapshot->bCancelRequested = bCancelRequested;
    pSnapshot->sCancelReason = xwork__dup_cstr(sCancelReason);
    pSnapshot->bPauseRequested = bPauseRequested;
    pSnapshot->sPauseReason = xwork__dup_cstr(sPauseReason);
    if ( pMutableGraph->bLockInitialized ) {
        xrtMutexUnlock(&pMutableGraph->tLock);
    }
    if ( (sCancelReason && !pSnapshot->sCancelReason) ||
         (sPauseReason && !pSnapshot->sPauseReason) ) {
        xwork_task_graph_snapshot_reset(pSnapshot);
        return XWORK_ERROR_NO_MEMORY;
    }

    xwork__task_graph_count_result(pGraph, &pSnapshot->tResult);
    if ( pGraph->iNodeCount > 0u ) {
        pSnapshot->tNodes.pItems = (xwork_task_node_snapshot *)calloc(
            pGraph->iNodeCount,
            sizeof(*pSnapshot->tNodes.pItems)
        );
        if ( !pSnapshot->tNodes.pItems ) {
            xwork_task_graph_snapshot_reset(pSnapshot);
            return XWORK_ERROR_NO_MEMORY;
        }
        pSnapshot->tNodes.iCount = pGraph->iNodeCount;
        for ( i = 0u; i < pGraph->iNodeCount; ++i ) {
            iStatus = xwork__task_node_fill_snapshot(
                &pGraph->pNodes[i],
                &pSnapshot->tNodes.pItems[i]
            );
            if ( iStatus != XWORK_OK ) {
                xwork_task_graph_snapshot_reset(pSnapshot);
                return iStatus;
            }
        }
    }
    if ( pGraph->iHandoffCount > 0u ) {
        pSnapshot->tHandoffs.pItems = (xwork_handoff_summary *)calloc(
            pGraph->iHandoffCount,
            sizeof(*pSnapshot->tHandoffs.pItems)
        );
        if ( !pSnapshot->tHandoffs.pItems ) {
            xwork_task_graph_snapshot_reset(pSnapshot);
            return XWORK_ERROR_NO_MEMORY;
        }
        pSnapshot->tHandoffs.iCount = pGraph->iHandoffCount;
        for ( i = 0u; i < pGraph->iHandoffCount; ++i ) {
            iStatus = xwork__handoff_fill_summary(
                &pGraph->pHandoffs[i],
                &pSnapshot->tHandoffs.pItems[i]
            );
            if ( iStatus != XWORK_OK ) {
                xwork_task_graph_snapshot_reset(pSnapshot);
                return iStatus;
            }
        }
    }
    return XWORK_OK;
}

static void xwork__task_node_record_event(
    xwork_task_node_record *pNode,
    xwork_event_kind eKind,
    const char *sSummary
)
{
    if ( !pNode || !pNode->pRun ) {
        return;
    }
    (void)xwork__run_record_event(
        pNode->pRun,
        eKind,
        pNode->sAgentId,
        NULL,
        NULL,
        sSummary
    );
}

static xwork_status xwork__task_node_ensure_run(
    xwork_task_graph *pGraph,
    xwork_task_node_record *pNode
)
{
    xwork_run_options tRunOptions;
    xwork_status iStatus;

    if ( !pGraph || !pGraph->pAgentPool || !pGraph->pAgentPool->pRuntime || !pNode ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( pNode->pRun ) {
        return XWORK_OK;
    }

    xwork_run_options_init(&tRunOptions);
    tRunOptions.sRunId = pNode->sRunId;
    tRunOptions.sParentRunId = pNode->sParentRunId;
    tRunOptions.sAgentId = pNode->sAgentId;
    tRunOptions.sTaskId = pNode->sTaskId;
    tRunOptions.sInstruction = pNode->sInstruction;
    tRunOptions.sLlmProfileId = pNode->sLlmProfileId;
    tRunOptions.sSessionProfileId = pNode->sSessionProfileId;
    tRunOptions.psWorkspaceIds = (const char **)pNode->psWorkspaceIds;
    tRunOptions.iWorkspaceCount = pNode->iWorkspaceCount;
    tRunOptions.eAutonomy = pNode->eAutonomy;
    tRunOptions.tSessionPolicy = pNode->tSessionPolicy;

    iStatus = xwork_run_create(pGraph->pAgentPool->pRuntime, &tRunOptions, &pNode->pRun);
    if ( iStatus != XWORK_OK ) {
        pNode->eState = XWORK_TASK_FAILED;
        pNode->iStatus = iStatus;
    }
    return iStatus;
}

static xwork_status xwork__task_node_execute_default(
    xwork_task_graph *pGraph,
    xwork_task_node_record *pNode
)
{
    xwork_orchestrator_options tOptions;
    xwork_run_async *pAsync = NULL;
    xwork_status iStatus;
    bool bCompleted = false;

    if ( !pGraph || !pNode || !pNode->pRun ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_orchestrator_options_init(&tOptions);
    if ( pNode->iMaxTurns != 0u ) {
        tOptions.iMaxTurns = pNode->iMaxTurns;
    }
    tOptions.iMaxRetries = pNode->iMaxRetries;
    tOptions.pCancelToken = pGraph->pCancelToken;

    if ( pNode->iTimeoutMs == 0u ) {
        return xwork_run_execute(pNode->pRun, &tOptions);
    }

    iStatus = xwork_run_execute_async(pNode->pRun, &tOptions, &pAsync);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    iStatus = xwork_run_async_wait_timeout(pAsync, pNode->iTimeoutMs, &bCompleted);
    if ( iStatus == XWORK_OK && !bCompleted ) {
        (void)xwork_run_async_cancel(pAsync, "Agent task timeout.");
        (void)xwork_run_async_wait(pAsync);
        iStatus = XWORK_ERROR_CANCELLED;
    }
    xwork_run_async_destroy(pAsync);
    return iStatus;
}

xwork_status xwork_task_graph_request_handoff(
    xwork_task_graph *pGraph,
    const xwork_handoff_request_options *pOptions,
    xwork_handoff_summary *pSummary
)
{
    xwork_handoff_record *pHandoff;
    xwork_task_node_record *pFrom;
    xwork_task_node_record *pTo;
    xwork_status iStatus;

    if ( pSummary ) {
        xwork_handoff_summary_reset(pSummary);
    }
    if ( !pGraph ||
         !pOptions ||
         !pOptions->sHandoffId ||
         !pOptions->sHandoffId[0] ||
         !pOptions->sFromTaskId ||
         !pOptions->sFromTaskId[0] ||
         !pOptions->sToTaskId ||
         !pOptions->sToTaskId[0] ||
         strcmp(pOptions->sFromTaskId, pOptions->sToTaskId) == 0 ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    if ( pGraph->bLockInitialized ) {
        xrtMutexLock(&pGraph->tLock);
    }
    pFrom = xwork__task_graph_find_node(pGraph, pOptions->sFromTaskId);
    pTo = xwork__task_graph_find_node(pGraph, pOptions->sToTaskId);
    if ( !pFrom || !pTo ) {
        if ( pGraph->bLockInitialized ) {
            xrtMutexUnlock(&pGraph->tLock);
        }
        return XWORK_ERROR_NOT_FOUND;
    }
    if ( xwork__task_graph_find_handoff(pGraph, pOptions->sHandoffId) ) {
        if ( pGraph->bLockInitialized ) {
            xrtMutexUnlock(&pGraph->tLock);
        }
        return XWORK_ERROR_ALREADY_EXISTS;
    }
    iStatus = xwork__task_graph_reserve_handoff(pGraph);
    if ( iStatus != XWORK_OK ) {
        if ( pGraph->bLockInitialized ) {
            xrtMutexUnlock(&pGraph->tLock);
        }
        return iStatus;
    }
    pHandoff = &pGraph->pHandoffs[pGraph->iHandoffCount];
    memset(pHandoff, 0, sizeof(*pHandoff));
    pHandoff->sHandoffId = xwork__dup_cstr(pOptions->sHandoffId);
    pHandoff->sFromTaskId = xwork__dup_cstr(pOptions->sFromTaskId);
    pHandoff->sToTaskId = xwork__dup_cstr(pOptions->sToTaskId);
    pHandoff->sReason = xwork__dup_cstr(pOptions->sReason);
    pHandoff->eState = XWORK_HANDOFF_PENDING;
    pHandoff->iStatus = XWORK_OK;
    pHandoff->bReadOnlySharedContext = pOptions->bReadOnlySharedContext;
    pHandoff->bWritableWorkspace = pOptions->bWritableWorkspace;
    if ( !pHandoff->sHandoffId ||
         !pHandoff->sFromTaskId ||
         !pHandoff->sToTaskId ||
         (pOptions->sReason && !pHandoff->sReason) ) {
        xwork__handoff_record_reset(pHandoff);
        if ( pGraph->bLockInitialized ) {
            xrtMutexUnlock(&pGraph->tLock);
        }
        return XWORK_ERROR_NO_MEMORY;
    }
    iStatus = xwork__copy_string_array(
        pOptions->psArtifactRefs,
        pOptions->iArtifactRefCount,
        &pHandoff->psArtifactRefs
    );
    if ( iStatus == XWORK_OK ) {
        pHandoff->iArtifactRefCount = pOptions->iArtifactRefCount;
        iStatus = xwork__copy_string_array(
            pOptions->psMemoryContextRefs,
            pOptions->iMemoryContextRefCount,
            &pHandoff->psMemoryContextRefs
        );
    }
    if ( iStatus == XWORK_OK ) {
        pHandoff->iMemoryContextRefCount = pOptions->iMemoryContextRefCount;
        iStatus = xwork__copy_string_array(
            pOptions->psSharedWorkspaceIds,
            pOptions->iSharedWorkspaceCount,
            &pHandoff->psSharedWorkspaceIds
        );
    }
    if ( iStatus != XWORK_OK ) {
        xwork__handoff_record_reset(pHandoff);
        if ( pGraph->bLockInitialized ) {
            xrtMutexUnlock(&pGraph->tLock);
        }
        return iStatus;
    }
    pHandoff->iSharedWorkspaceCount = pOptions->iSharedWorkspaceCount;
    ++pGraph->iHandoffCount;
    if ( pSummary ) {
        iStatus = xwork__handoff_fill_summary(pHandoff, pSummary);
        if ( iStatus != XWORK_OK ) {
            --pGraph->iHandoffCount;
            xwork__handoff_record_reset(pHandoff);
            if ( pGraph->bLockInitialized ) {
                xrtMutexUnlock(&pGraph->tLock);
            }
            return iStatus;
        }
    }
    if ( pGraph->bLockInitialized ) {
        xrtMutexUnlock(&pGraph->tLock);
    }

    xwork__task_node_record_event(pFrom, XWORK_EVENT_HANDOFF_REQUESTED, "Handoff requested.");
    xwork__task_node_record_event(pTo, XWORK_EVENT_HANDOFF_REQUESTED, "Handoff requested.");
    return XWORK_OK;
}

xwork_status xwork_task_graph_resolve_handoff(
    xwork_task_graph *pGraph,
    const xwork_handoff_result_options *pOptions,
    xwork_handoff_summary *pSummary
)
{
    xwork_handoff_record *pHandoff;
    xwork_task_node_record *pFrom;
    xwork_task_node_record *pTo;
    xwork_event_kind eEventKind;
    xwork_status iStatus;

    if ( pSummary ) {
        xwork_handoff_summary_reset(pSummary);
    }
    if ( !pGraph ||
         !pOptions ||
         !pOptions->sHandoffId ||
         !pOptions->sHandoffId[0] ||
         pOptions->eState == XWORK_HANDOFF_PENDING ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    if ( pGraph->bLockInitialized ) {
        xrtMutexLock(&pGraph->tLock);
    }
    pHandoff = xwork__task_graph_find_handoff(pGraph, pOptions->sHandoffId);
    if ( !pHandoff ) {
        if ( pGraph->bLockInitialized ) {
            xrtMutexUnlock(&pGraph->tLock);
        }
        return XWORK_ERROR_NOT_FOUND;
    }
    pFrom = xwork__task_graph_find_node(pGraph, pHandoff->sFromTaskId);
    pTo = xwork__task_graph_find_node(pGraph, pHandoff->sToTaskId);
    pHandoff->eState = pOptions->eState;
    pHandoff->iStatus = pOptions->iStatus;
    xwork__free_cstr(&pHandoff->sMessage);
    pHandoff->sMessage = xwork__dup_cstr(pOptions->sMessage);
    if ( pOptions->sMessage && !pHandoff->sMessage ) {
        if ( pGraph->bLockInitialized ) {
            xrtMutexUnlock(&pGraph->tLock);
        }
        return XWORK_ERROR_NO_MEMORY;
    }
    if ( pSummary ) {
        iStatus = xwork__handoff_fill_summary(pHandoff, pSummary);
        if ( iStatus != XWORK_OK ) {
            if ( pGraph->bLockInitialized ) {
                xrtMutexUnlock(&pGraph->tLock);
            }
            return iStatus;
        }
    }
    if ( pGraph->bLockInitialized ) {
        xrtMutexUnlock(&pGraph->tLock);
    }

    eEventKind = XWORK_EVENT_HANDOFF_COMPLETED;
    if ( pOptions->eState == XWORK_HANDOFF_ACCEPTED ) {
        eEventKind = XWORK_EVENT_HANDOFF_ACCEPTED;
    } else if ( pOptions->eState == XWORK_HANDOFF_REJECTED ) {
        eEventKind = XWORK_EVENT_HANDOFF_REJECTED;
    }
    xwork__task_node_record_event(pFrom, eEventKind, "Handoff resolved.");
    xwork__task_node_record_event(pTo, eEventKind, "Handoff resolved.");
    return XWORK_OK;
}

xwork_status xwork_task_graph_list_handoffs(
    const xwork_task_graph *pGraph,
    xwork_handoff_summary_list *pList
)
{
    xwork_task_graph *pMutableGraph;
    xwork_status iStatus;
    size_t i;

    if ( !pGraph || !pList ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    xwork_handoff_summary_list_reset(pList);
    pMutableGraph = (xwork_task_graph *)pGraph;
    if ( pMutableGraph->bLockInitialized ) {
        xrtMutexLock(&pMutableGraph->tLock);
    }
    if ( pGraph->iHandoffCount == 0u ) {
        if ( pMutableGraph->bLockInitialized ) {
            xrtMutexUnlock(&pMutableGraph->tLock);
        }
        return XWORK_OK;
    }
    pList->pItems = (xwork_handoff_summary *)calloc(
        pGraph->iHandoffCount,
        sizeof(*pList->pItems)
    );
    if ( !pList->pItems ) {
        if ( pMutableGraph->bLockInitialized ) {
            xrtMutexUnlock(&pMutableGraph->tLock);
        }
        return XWORK_ERROR_NO_MEMORY;
    }
    pList->iCount = pGraph->iHandoffCount;
    for ( i = 0u; i < pGraph->iHandoffCount; ++i ) {
        iStatus = xwork__handoff_fill_summary(&pGraph->pHandoffs[i], &pList->pItems[i]);
        if ( iStatus != XWORK_OK ) {
            if ( pMutableGraph->bLockInitialized ) {
                xrtMutexUnlock(&pMutableGraph->tLock);
            }
            xwork_handoff_summary_list_reset(pList);
            return iStatus;
        }
    }
    if ( pMutableGraph->bLockInitialized ) {
        xrtMutexUnlock(&pMutableGraph->tLock);
    }
    return XWORK_OK;
}

xwork_status xwork_task_graph_emit_agent_result_report(
    const xwork_task_graph *pGraph,
    const char *sTaskId,
    const char *sArtifactId,
    xwork_artifact *pArtifact
)
{
    xwork_task_graph *pMutableGraph;
    xwork_task_node_record *pNode;
    xwork_report_artifact_options tOptions;
    char *sReportText;
    char *sSubjectRef;
    const char *sAgentId;
    xwork_task_state eState;
    xwork_status iTaskStatus;
    size_t iAttemptCount;
    size_t iMaxRetries;
    xwork_run *pRun;
    char *sTaskIdJson;
    char *sAgentIdJson;
    char *sSubjectRefJson;
    xwork_status iStatus;

    if ( !pGraph || !sTaskId || !sTaskId[0] ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    pMutableGraph = (xwork_task_graph *)pGraph;
    if ( pMutableGraph->bLockInitialized ) {
        xrtMutexLock(&pMutableGraph->tLock);
    }
    pNode = xwork__task_graph_find_node(pGraph, sTaskId);
    if ( !pNode ) {
        if ( pMutableGraph->bLockInitialized ) {
            xrtMutexUnlock(&pMutableGraph->tLock);
        }
        return XWORK_ERROR_NOT_FOUND;
    }
    pRun = pNode->pRun;
    sAgentId = pNode->sAgentId ? pNode->sAgentId : "";
    eState = pNode->eState;
    iTaskStatus = pNode->iStatus;
    iAttemptCount = pNode->iAttemptCount;
    iMaxRetries = pNode->iMaxRetries;
    if ( pMutableGraph->bLockInitialized ) {
        xrtMutexUnlock(&pMutableGraph->tLock);
    }
    if ( !pRun ) {
        return XWORK_ERROR_INVALID_STATE;
    }

    sSubjectRef = xwork__dup_printf("task://%s", sTaskId);
    sTaskIdJson = xwork__json_escape_dup(sTaskId);
    sAgentIdJson = xwork__json_escape_dup(sAgentId);
    sSubjectRefJson = xwork__json_escape_dup(sSubjectRef);
    sReportText = xwork__dup_printf(
        "{"
        "\"schema\":\"%s\","
        "\"report_kind\":\"agent_result\","
        "\"status\":\"%s\","
        "\"subject_ref\":\"%s\","
        "\"title\":\"Agent task result: %s\","
        "\"summary\":\"Task %s finished with state %s\","
        "\"task_id\":\"%s\","
        "\"agent_id\":\"%s\","
        "\"task_state\":\"%s\","
        "\"task_status\":\"%s\","
        "\"attempt_count\":%lu,"
        "\"max_retries\":%lu"
        "}",
        XWORK_REPORT_SCHEMA_V1,
        iTaskStatus == XWORK_OK ? "ok" : "error",
        sSubjectRefJson ? sSubjectRefJson : "",
        sTaskIdJson ? sTaskIdJson : "",
        sTaskIdJson ? sTaskIdJson : "",
        xwork__task_state_cstr(eState),
        sTaskIdJson ? sTaskIdJson : "",
        sAgentIdJson ? sAgentIdJson : "",
        xwork__task_state_cstr(eState),
        xwork_status_cstr(iTaskStatus),
        (unsigned long)iAttemptCount,
        (unsigned long)iMaxRetries
    );
    if ( !sSubjectRef || !sTaskIdJson || !sAgentIdJson || !sSubjectRefJson || !sReportText ) {
        free(sSubjectRef);
        free(sTaskIdJson);
        free(sAgentIdJson);
        free(sSubjectRefJson);
        free(sReportText);
        return XWORK_ERROR_NO_MEMORY;
    }

    xwork_report_artifact_options_init(&tOptions);
    tOptions.sArtifactId = sArtifactId && sArtifactId[0] ? sArtifactId : NULL;
    tOptions.sName = "agent-result-report.json";
    tOptions.sMimeType = "application/json";
    tOptions.sSummary = "Agent task result report.";
    tOptions.eOutputClass = XWORK_ARTIFACT_OUTPUT_JSON;
    tOptions.sOutputRole = "agent.result.report";
    tOptions.eReportClass = XWORK_ARTIFACT_REPORT_SUMMARY;
    tOptions.sReportSubjectRef = sSubjectRef;
    tOptions.sReportText = sReportText;
    iStatus = xwork_run_emit_report_artifact(pRun, &tOptions, pArtifact);
    free(sSubjectRef);
    free(sTaskIdJson);
    free(sAgentIdJson);
    free(sSubjectRefJson);
    free(sReportText);
    return iStatus;
}

xwork_status xwork_task_graph_emit_aggregate_report(
    const xwork_task_graph *pGraph,
    xwork_run *pRun,
    const char *sArtifactId,
    xwork_artifact *pArtifact
)
{
    xwork_task_graph_result tResult;
    xwork_report_artifact_options tOptions;
    char *sReportText;
    char *sSubjectRef;
    char *sGraphIdJson;
    char *sSubjectRefJson;
    xwork_status iStatus;

    if ( !pGraph || !pRun ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork__task_graph_count_result(pGraph, &tResult);
    sSubjectRef = xwork__dup_printf("task-graph://%s", pGraph->sGraphId);
    sGraphIdJson = xwork__json_escape_dup(pGraph->sGraphId);
    sSubjectRefJson = xwork__json_escape_dup(sSubjectRef);
    sReportText = xwork__dup_printf(
        "{"
        "\"schema\":\"%s\","
        "\"report_kind\":\"multi_agent_aggregate\","
        "\"status\":\"%s\","
        "\"subject_ref\":\"%s\","
        "\"title\":\"Multi-agent aggregate report: %s\","
        "\"summary\":\"%lu/%lu tasks completed\","
        "\"graph_id\":\"%s\","
        "\"task_status\":\"%s\","
        "\"total_count\":%lu,"
        "\"completed_count\":%lu,"
        "\"failed_count\":%lu,"
        "\"cancelled_count\":%lu,"
        "\"skipped_count\":%lu"
        "}",
        XWORK_REPORT_SCHEMA_V1,
        tResult.iStatus == XWORK_OK ? "ok" : "error",
        sSubjectRefJson ? sSubjectRefJson : "",
        sGraphIdJson ? sGraphIdJson : "",
        (unsigned long)tResult.iCompletedCount,
        (unsigned long)tResult.iTotalCount,
        sGraphIdJson ? sGraphIdJson : "",
        xwork_status_cstr(tResult.iStatus),
        (unsigned long)tResult.iTotalCount,
        (unsigned long)tResult.iCompletedCount,
        (unsigned long)tResult.iFailedCount,
        (unsigned long)tResult.iCancelledCount,
        (unsigned long)tResult.iSkippedCount
    );
    if ( !sSubjectRef || !sGraphIdJson || !sSubjectRefJson || !sReportText ) {
        free(sSubjectRef);
        free(sGraphIdJson);
        free(sSubjectRefJson);
        free(sReportText);
        return XWORK_ERROR_NO_MEMORY;
    }

    xwork_report_artifact_options_init(&tOptions);
    tOptions.sArtifactId = sArtifactId && sArtifactId[0] ? sArtifactId : NULL;
    tOptions.sName = "multi-agent-aggregate-report.json";
    tOptions.sMimeType = "application/json";
    tOptions.sSummary = "Multi-agent aggregate report.";
    tOptions.eOutputClass = XWORK_ARTIFACT_OUTPUT_JSON;
    tOptions.sOutputRole = "agent.aggregate.report";
    tOptions.eReportClass = XWORK_ARTIFACT_REPORT_FINAL;
    tOptions.sReportSubjectRef = sSubjectRef;
    tOptions.sReportText = sReportText;
    iStatus = xwork_run_emit_report_artifact(pRun, &tOptions, pArtifact);
    free(sSubjectRef);
    free(sGraphIdJson);
    free(sSubjectRefJson);
    free(sReportText);
    return iStatus;
}

static xwork_status xwork__task_node_store_run_snapshot(xwork_task_node_record *pNode)
{
    xwork_run_snapshot tSnapshot;
    xwork_status iStatus;

    if ( !pNode || !pNode->pRun ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_run_snapshot_init(&tSnapshot);
    iStatus = xwork_run_get_snapshot(pNode->pRun, &tSnapshot);
    if ( iStatus == XWORK_OK ) {
        iStatus = xwork__runtime_store_run_snapshot(pNode->pRun->pRuntime, &tSnapshot);
    }
    xwork_run_snapshot_reset(&tSnapshot);
    return iStatus;
}

static bool xwork__task_graph_dependencies_ready(
    const xwork_task_graph *pGraph,
    const xwork_task_node_record *pNode,
    bool *pbImpossible
)
{
    size_t i;
    xwork_task_node_record *pDep;

    *pbImpossible = false;
    for ( i = 0u; i < pNode->iDependencyCount; ++i ) {
        pDep = xwork__task_graph_find_node(pGraph, pNode->psDependencies[i]);
        if ( !pDep ) {
            *pbImpossible = true;
            return false;
        }
        if ( !xwork__task_state_is_terminal(pDep->eState) ) {
            return false;
        }
        if ( xwork__task_state_is_failed_terminal(pDep->eState) &&
             pGraph->eFailurePolicy != XWORK_TASK_FAILURE_BEST_EFFORT ) {
            *pbImpossible = true;
            return false;
        }
    }
    return true;
}

typedef struct {
    xwork_task_graph *pGraph;
    xwork_task_node_record *pNode;
} xwork_task_thread_context;

static ptr xwork__task_thread_proc_ptr(uint32 (*pProc)(ptr))
{
    ptr pValue = NULL;

    if ( sizeof(pValue) < sizeof(pProc) ) {
        return NULL;
    }
    memcpy(&pValue, &pProc, sizeof(pProc));
    return pValue;
}

static uint32 xwork__task_thread_proc(ptr pParam)
{
    xwork_task_thread_context *pContext;
    xwork_task_graph *pGraph;
    xwork_task_node_record *pNode;
    xwork_task_node_summary tSummary;
    xwork_status iCallbackStatus;
    xwork_status iTransitionStatus;
    size_t iAttempt;
    size_t iMaxAttempts;

    pContext = (xwork_task_thread_context *)pParam;
    if ( !pContext || !pContext->pGraph || !pContext->pNode ) {
        free(pContext);
        return 1u;
    }
    pGraph = pContext->pGraph;
    pNode = pContext->pNode;
    free(pContext);

    xwork__task_node_record_event(pNode, XWORK_EVENT_AGENT_STARTED, "Agent task started.");
    xwork__task_node_record_event(pNode, XWORK_EVENT_TASK_STARTED, "Task execution started.");

    if ( xwork__task_graph_cancel_requested(pGraph) ) {
        iCallbackStatus = XWORK_ERROR_CANCELLED;
    } else {
        iCallbackStatus = xwork_run_start(pNode->pRun);
    }

    iMaxAttempts = pNode->iMaxRetries + 1u;
    for ( iAttempt = 1u;
          iCallbackStatus == XWORK_OK && iAttempt <= iMaxAttempts;
          ++iAttempt ) {
        pNode->iAttemptCount = iAttempt;
        if ( xwork__task_graph_cancel_requested(pGraph) ) {
            iCallbackStatus = XWORK_ERROR_CANCELLED;
            break;
        }
        if ( pGraph->pfnExecute ) {
            (void)xwork__task_node_fill_summary(pNode, &tSummary, false);
            iCallbackStatus = pGraph->pfnExecute(
                pNode->pRun,
                &tSummary,
                pGraph->pUserData
            );
        } else {
            iCallbackStatus = xwork__task_node_execute_default(pGraph, pNode);
        }
        if ( iCallbackStatus == XWORK_OK ||
             iCallbackStatus == XWORK_ERROR_CANCELLED ||
             iAttempt == iMaxAttempts ) {
            break;
        }
        xwork__task_node_record_event(
            pNode,
            XWORK_EVENT_RETRY_SCHEDULED,
            "Task retry scheduled."
        );
        iCallbackStatus = XWORK_OK;
    }
    if ( iCallbackStatus == XWORK_OK && xwork__task_graph_cancel_requested(pGraph) ) {
        iCallbackStatus = XWORK_ERROR_CANCELLED;
    }

    if ( iCallbackStatus == XWORK_OK ) {
        iTransitionStatus = xwork_run_complete(pNode->pRun);
        pNode->eState = iTransitionStatus == XWORK_OK ? XWORK_TASK_COMPLETED : XWORK_TASK_FAILED;
        pNode->iStatus = iTransitionStatus;
        xwork__task_node_record_event(pNode, XWORK_EVENT_TASK_COMPLETED, "Task execution completed.");
        xwork__task_node_record_event(pNode, XWORK_EVENT_AGENT_COMPLETED, "Agent task completed.");
    } else if ( iCallbackStatus == XWORK_ERROR_CANCELLED ) {
        (void)xwork_run_cancel(pNode->pRun);
        pNode->eState = XWORK_TASK_CANCELLED;
        pNode->iStatus = iCallbackStatus;
        xwork__task_node_record_event(pNode, XWORK_EVENT_TASK_CANCELLED, "Task execution cancelled.");
        xwork__task_node_record_event(pNode, XWORK_EVENT_AGENT_CANCELLED, "Agent task cancelled.");
    } else {
        (void)xwork_run_fail(pNode->pRun);
        pNode->eState = XWORK_TASK_FAILED;
        pNode->iStatus = iCallbackStatus;
        xwork__task_node_record_event(pNode, XWORK_EVENT_TASK_FAILED, "Task execution failed.");
        xwork__task_node_record_event(pNode, XWORK_EVENT_AGENT_FAILED, "Agent task failed.");
    }
    iTransitionStatus = xwork__task_node_store_run_snapshot(pNode);
    if ( iTransitionStatus != XWORK_OK ) {
        pNode->eState = XWORK_TASK_FAILED;
        pNode->iStatus = iTransitionStatus;
    }
    return pNode->iStatus == XWORK_OK ? 0u : 1u;
}

static xwork_status xwork__task_graph_launch_node(
    xwork_task_graph *pGraph,
    xwork_task_node_record *pNode
)
{
    xwork_task_thread_context *pContext;
    ptr pThreadProc;
    xwork_status iStatus;

    iStatus = xwork__task_node_ensure_run(pGraph, pNode);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    xwork__task_node_record_event(pNode, XWORK_EVENT_AGENT_SPAWNED, "Agent task spawned.");
    xwork__task_node_record_event(pNode, XWORK_EVENT_TASK_SCHEDULED, "Task scheduled.");

    pContext = (xwork_task_thread_context *)calloc(1u, sizeof(*pContext));
    if ( !pContext ) {
        pNode->eState = XWORK_TASK_FAILED;
        pNode->iStatus = XWORK_ERROR_NO_MEMORY;
        return XWORK_ERROR_NO_MEMORY;
    }
    pContext->pGraph = pGraph;
    pContext->pNode = pNode;

    pNode->eState = XWORK_TASK_RUNNING;
    pNode->iStatus = XWORK_OK;
    pThreadProc = xwork__task_thread_proc_ptr(xwork__task_thread_proc);
    if ( !pThreadProc ) {
        free(pContext);
        pNode->eState = XWORK_TASK_FAILED;
        pNode->iStatus = XWORK_ERROR_UNSUPPORTED;
        return XWORK_ERROR_UNSUPPORTED;
    }
    pNode->pThread = xrtThreadCreate(pThreadProc, pContext, 0u);
    if ( !pNode->pThread ) {
        free(pContext);
        pNode->eState = XWORK_TASK_FAILED;
        pNode->iStatus = XWORK_ERROR_EXTERNAL_FAILURE;
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }
    pNode->bThreadStarted = true;
    return XWORK_OK;
}

static void xwork__task_graph_join_running(xwork_task_graph *pGraph)
{
    size_t i;
    xwork_task_node_record *pNode;

    for ( i = 0u; i < pGraph->iNodeCount; ++i ) {
        pNode = &pGraph->pNodes[i];
        if ( pNode->pThread ) {
            xrtThreadWait(pNode->pThread);
            xrtThreadDestroy(pNode->pThread);
            pNode->pThread = NULL;
            xwork__task_node_record_event(pNode, XWORK_EVENT_TASK_JOINED, "Task worker joined.");
        }
    }
}

static void xwork__task_graph_mark_remaining_skipped(xwork_task_graph *pGraph)
{
    size_t i;

    for ( i = 0u; i < pGraph->iNodeCount; ++i ) {
        if ( !xwork__task_state_is_terminal(pGraph->pNodes[i].eState) ) {
            pGraph->pNodes[i].eState = XWORK_TASK_SKIPPED;
            pGraph->pNodes[i].iStatus = XWORK_ERROR_CANCELLED;
        }
    }
}

static void xwork__task_graph_mark_remaining_cancelled(xwork_task_graph *pGraph)
{
    size_t i;

    for ( i = 0u; i < pGraph->iNodeCount; ++i ) {
        if ( !xwork__task_state_is_terminal(pGraph->pNodes[i].eState) ) {
            pGraph->pNodes[i].eState = XWORK_TASK_CANCELLED;
            pGraph->pNodes[i].iStatus = XWORK_ERROR_CANCELLED;
        }
    }
}

static void xwork__task_graph_count_result(
    const xwork_task_graph *pGraph,
    xwork_task_graph_result *pResult
)
{
    size_t i;
    size_t iTerminalCount = 0u;

    xwork_task_graph_result_init(pResult);
    pResult->iTotalCount = pGraph->iNodeCount;
    for ( i = 0u; i < pGraph->iNodeCount; ++i ) {
        switch ( pGraph->pNodes[i].eState ) {
            case XWORK_TASK_COMPLETED:
                ++iTerminalCount;
                ++pResult->iCompletedCount;
                break;
            case XWORK_TASK_FAILED:
                ++iTerminalCount;
                ++pResult->iFailedCount;
                break;
            case XWORK_TASK_CANCELLED:
                ++iTerminalCount;
                ++pResult->iCancelledCount;
                break;
            case XWORK_TASK_SKIPPED:
                ++iTerminalCount;
                ++pResult->iSkippedCount;
                break;
            default:
                break;
        }
    }
    if ( xwork__task_graph_pause_requested(pGraph) && iTerminalCount < pGraph->iNodeCount ) {
        pResult->iStatus = XWORK_ERROR_PAUSED;
    } else if ( pResult->iCancelledCount ) {
        pResult->iStatus = XWORK_ERROR_CANCELLED;
    } else if ( pResult->iFailedCount || pResult->iSkippedCount ) {
        pResult->iStatus = XWORK_ERROR_EXTERNAL_FAILURE;
    }
}

xwork_status xwork_task_graph_cancel(
    xwork_task_graph *pGraph,
    const char *sReason
)
{
    char *sReasonCopy = NULL;

    if ( !pGraph ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( sReason ) {
        sReasonCopy = xwork__dup_cstr(sReason);
        if ( !sReasonCopy ) {
            return XWORK_ERROR_NO_MEMORY;
        }
    }
    if ( pGraph->pCancelToken ) {
        xllm_cancel_token_cancel(pGraph->pCancelToken, sReason ? sReason : "task graph cancelled");
    }
    if ( pGraph->bLockInitialized ) {
        xrtMutexLock(&pGraph->tLock);
    }
    pGraph->bCancelRequested = true;
    xwork__free_cstr(&pGraph->sCancelReason);
    pGraph->sCancelReason = sReasonCopy;
    if ( pGraph->bLockInitialized ) {
        xrtMutexUnlock(&pGraph->tLock);
    }
    return XWORK_OK;
}

bool xwork_task_graph_is_cancelled(const xwork_task_graph *pGraph)
{
    return xwork__task_graph_cancel_requested(pGraph);
}

xwork_status xwork_task_graph_pause(
    xwork_task_graph *pGraph,
    const char *sReason
)
{
    char *sReasonCopy = NULL;
    size_t i;

    if ( !pGraph ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( sReason ) {
        sReasonCopy = xwork__dup_cstr(sReason);
        if ( !sReasonCopy ) {
            return XWORK_ERROR_NO_MEMORY;
        }
    }
    if ( pGraph->bLockInitialized ) {
        xrtMutexLock(&pGraph->tLock);
    }
    pGraph->bPauseRequested = true;
    xwork__free_cstr(&pGraph->sPauseReason);
    pGraph->sPauseReason = sReasonCopy;
    if ( pGraph->bLockInitialized ) {
        xrtMutexUnlock(&pGraph->tLock);
    }

    for ( i = 0u; i < pGraph->iNodeCount; ++i ) {
        if ( pGraph->pNodes[i].eState == XWORK_TASK_RUNNING && pGraph->pNodes[i].pRun ) {
            xwork__task_node_record_event(
                &pGraph->pNodes[i],
                XWORK_EVENT_RUN_PAUSED,
                sReason ? sReason : "Task graph paused."
            );
            xwork__task_node_record_event(
                &pGraph->pNodes[i],
                XWORK_EVENT_AGENT_PAUSED,
                "Agent task paused by scheduler."
            );
        }
    }
    return XWORK_OK;
}

xwork_status xwork_task_graph_resume(xwork_task_graph *pGraph)
{
    if ( !pGraph ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( pGraph->bLockInitialized ) {
        xrtMutexLock(&pGraph->tLock);
    }
    pGraph->bPauseRequested = false;
    xwork__free_cstr(&pGraph->sPauseReason);
    if ( pGraph->bLockInitialized ) {
        xrtMutexUnlock(&pGraph->tLock);
    }
    return XWORK_OK;
}

bool xwork_task_graph_is_paused(const xwork_task_graph *pGraph)
{
    return xwork__task_graph_pause_requested(pGraph);
}

xwork_status xwork_task_graph_execute(
    xwork_task_graph *pGraph,
    xwork_task_graph_result *pResult
)
{
    size_t i;
    size_t iLaunched;
    size_t iTerminalCount;
    bool bImpossible;
    bool bHasFailure;
    xwork_status iLaunchStatus;
    xwork_task_graph_result tLocalResult;

    if ( !pGraph ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( pGraph->bExecuting ) {
        return XWORK_ERROR_INVALID_STATE;
    }
    if ( xwork__task_graph_cancel_requested(pGraph) ) {
        return XWORK_ERROR_CANCELLED;
    }
    if ( xwork__task_graph_pause_requested(pGraph) ) {
        xwork__task_graph_count_result(pGraph, &tLocalResult);
        if ( pResult ) {
            *pResult = tLocalResult;
        }
        return tLocalResult.iStatus;
    }
    pGraph->bExecuting = true;

    for ( i = 0u; i < pGraph->iNodeCount; ++i ) {
        if ( pGraph->pNodes[i].eState != XWORK_TASK_PENDING &&
             pGraph->pNodes[i].eState != XWORK_TASK_BLOCKED &&
             !xwork__task_state_is_terminal(pGraph->pNodes[i].eState) ) {
            pGraph->bExecuting = false;
            return XWORK_ERROR_INVALID_STATE;
        }
    }

    while ( true ) {
        if ( xwork__task_graph_cancel_requested(pGraph) ) {
            xwork__task_graph_mark_remaining_cancelled(pGraph);
            break;
        }
        if ( xwork__task_graph_pause_requested(pGraph) ) {
            break;
        }

        iTerminalCount = 0u;
        bHasFailure = false;
        for ( i = 0u; i < pGraph->iNodeCount; ++i ) {
            if ( xwork__task_state_is_terminal(pGraph->pNodes[i].eState) ) {
                ++iTerminalCount;
                if ( xwork__task_state_is_failed_terminal(pGraph->pNodes[i].eState) ) {
                    bHasFailure = true;
                }
            }
        }
        if ( iTerminalCount == pGraph->iNodeCount ) {
            break;
        }
        if ( bHasFailure && pGraph->eFailurePolicy == XWORK_TASK_FAILURE_FAIL_FAST ) {
            xwork__task_graph_mark_remaining_skipped(pGraph);
            break;
        }

        iLaunched = 0u;
        for ( i = 0u; i < pGraph->iNodeCount && iLaunched < pGraph->iMaxConcurrency; ++i ) {
            if ( xwork__task_graph_cancel_requested(pGraph) ) {
                xwork__task_graph_mark_remaining_cancelled(pGraph);
                break;
            }
            if ( pGraph->pNodes[i].eState != XWORK_TASK_PENDING &&
                 pGraph->pNodes[i].eState != XWORK_TASK_BLOCKED ) {
                continue;
            }
            if ( !xwork__task_graph_dependencies_ready(pGraph, &pGraph->pNodes[i], &bImpossible) ) {
                if ( bImpossible ) {
                    pGraph->pNodes[i].eState = XWORK_TASK_SKIPPED;
                    pGraph->pNodes[i].iStatus = XWORK_ERROR_CANCELLED;
                } else {
                    if ( pGraph->pNodes[i].eState != XWORK_TASK_BLOCKED &&
                         xwork__task_node_ensure_run(pGraph, &pGraph->pNodes[i]) == XWORK_OK ) {
                        xwork__task_node_record_event(
                            &pGraph->pNodes[i],
                            XWORK_EVENT_TASK_BLOCKED,
                            "Task blocked waiting for dependencies."
                        );
                    }
                    if ( pGraph->pNodes[i].eState != XWORK_TASK_FAILED ) {
                        pGraph->pNodes[i].eState = XWORK_TASK_BLOCKED;
                    }
                }
                continue;
            }
            if ( pGraph->pNodes[i].eState == XWORK_TASK_BLOCKED ) {
                xwork__task_node_record_event(
                    &pGraph->pNodes[i],
                    XWORK_EVENT_TASK_UNBLOCKED,
                    "Task dependencies satisfied."
                );
            }
            pGraph->pNodes[i].eState = XWORK_TASK_READY;
            iLaunchStatus = xwork__task_graph_launch_node(pGraph, &pGraph->pNodes[i]);
            if ( iLaunchStatus != XWORK_OK &&
                 pGraph->eFailurePolicy == XWORK_TASK_FAILURE_FAIL_FAST ) {
                xwork__task_graph_mark_remaining_skipped(pGraph);
                break;
            }
            ++iLaunched;
        }

        if ( iLaunched == 0u ) {
            xwork__task_graph_mark_remaining_skipped(pGraph);
            break;
        }
        xwork__task_graph_join_running(pGraph);
    }

    xwork__task_graph_count_result(pGraph, &tLocalResult);
    if ( pResult ) {
        *pResult = tLocalResult;
    }
    pGraph->bExecuting = false;
    return tLocalResult.iStatus;
}
