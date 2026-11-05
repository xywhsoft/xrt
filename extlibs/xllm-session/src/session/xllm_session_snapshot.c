#include "../internal/xllm_session_internal.h"

struct xllm_session_snapshot
{
    xatomic32 tReferences;
    xllm_session_config tConfig;
    char *sSummaryStyle;
    char *sSnapshotPath;
    xllm_session_budget tBudget;
    xllm_session_state eState;
    uint64_t uCurrentTurn;
    xllm_session_tail tTail;
    xllm_session_stats tStats;
    xllm_session_summary tSummary;
    char *sSummary;
    xllm_pending_tool_call *pPendingCalls;
    size_t iPendingCallCount;
    char **psReadFiles;
    size_t iReadFileCount;
    char **psModifiedFiles;
    size_t iModifiedFileCount;
    xllm_request tRequest;
    bool bRequestAvailable;
    xllm_error tRequestError;
};

static void xllm_session__snapshot_string_list_unit(char **psItems, size_t iCount)
{
    size_t i;
    for (i = 0u; i < iCount; ++i)
    {
        free(psItems[i]);
    }
    free(psItems);
}

static bool xllm_session__snapshot_string_list(const char *const *psSource, size_t iCount,
                                               char ***ppsTarget)
{
    char **psTarget;
    size_t i;
    *ppsTarget = NULL;
    if (iCount == 0u)
    {
        return true;
    }
    if (!psSource || iCount > SIZE_MAX / sizeof(*psTarget))
    {
        return false;
    }
    psTarget = (char **)xllm_session__calloc(iCount, sizeof(*psTarget));
    if (!psTarget)
    {
        return false;
    }
    for (i = 0u; i < iCount; ++i)
    {
        psTarget[i] = xllm_session__strdup(psSource[i]);
        if (!psTarget[i])
        {
            xllm_session__snapshot_string_list_unit(psTarget, i);
            return false;
        }
    }
    *ppsTarget = psTarget;
    return true;
}

static void xllm_session__snapshot_pending_unit(xllm_pending_tool_call *pCalls, size_t iCount)
{
    size_t i;
    for (i = 0u; i < iCount; ++i)
    {
        free((char *)pCalls[i].sId);
        free((char *)pCalls[i].sName);
        free((char *)pCalls[i].sArgumentsJson);
    }
    free(pCalls);
}

static bool xllm_session__snapshot_pending(const xllm_session *pSession,
                                           xllm_session_snapshot *pSnapshot)
{
    size_t i;
    size_t iCount = xllmSessionPendingToolCallCount(pSession);
    if (iCount == 0u)
    {
        return true;
    }
    if (iCount > SIZE_MAX / sizeof(*pSnapshot->pPendingCalls))
    {
        return false;
    }
    pSnapshot->pPendingCalls =
        (xllm_pending_tool_call *)xllm_session__calloc(iCount, sizeof(*pSnapshot->pPendingCalls));
    if (!pSnapshot->pPendingCalls)
    {
        return false;
    }
    pSnapshot->iPendingCallCount = iCount;
    for (i = 0u; i < iCount; ++i)
    {
        xllm_pending_tool_call tSource;
        xllm_pending_tool_call *pTarget = &pSnapshot->pPendingCalls[i];
        if (!xllmSessionPendingToolCallAt(pSession, i, &tSource))
        {
            return false;
        }
        pTarget->uTurn = tSource.uTurn;
        pTarget->sId = xllm_session__strdup(tSource.sId);
        pTarget->sName = xllm_session__strdup(tSource.sName);
        pTarget->sArgumentsJson = xllm_session__strdup(tSource.sArgumentsJson);
        if (!pTarget->sId || !pTarget->sName || !pTarget->sArgumentsJson)
        {
            return false;
        }
    }
    return true;
}

static bool xllm_session__snapshot_request_clone(xllm_request *pTarget, const xllm_request *pSource)
{
    xllm_request tCopy;
    size_t i;
    xllmRequestInit(&tCopy);
    tCopy.uReasoningBudgetTokens = pSource->uReasoningBudgetTokens;
    tCopy.uMaxOutputTokens = pSource->uMaxOutputTokens;
    tCopy.fTemperature = pSource->fTemperature;
    tCopy.bHasTemperature = pSource->bHasTemperature;
    tCopy.fTopP = pSource->fTopP;
    tCopy.bHasTopP = pSource->bHasTopP;
    tCopy.bParallelToolCalls = pSource->bParallelToolCalls;
    tCopy.eJsonMode = pSource->eJsonMode;
    tCopy.bStream = pSource->bStream;
    if ((pSource->sModel && !xllmRequestSetModel(&tCopy, pSource->sModel)) ||
        (pSource->sReasoningEffort &&
         !xllmRequestSetReasoningEffort(&tCopy, pSource->sReasoningEffort)) ||
        (pSource->sStop && !xllmRequestSetStop(&tCopy, pSource->sStop)) ||
        (pSource->sExtraBodyJson && !xllmRequestSetExtraBody(&tCopy, pSource->sExtraBodyJson)) ||
        !xllmRequestSetToolChoice(&tCopy, pSource->eToolChoice, pSource->sNamedTool))
    {
        goto fail;
    }
    for (i = 0u; i < pSource->iMessageCount; ++i)
    {
        if (!xllmRequestAddMessage(&tCopy, &pSource->pMessages[i]))
        {
            goto fail;
        }
    }
    for (i = 0u; i < pSource->iToolCount; ++i)
    {
        const xllm_tool *pTool = &pSource->pTools[i];
        if (!xllmRequestAddTool(&tCopy, pTool->sName, pTool->sDescription, pTool->sParametersJson,
                                pTool->bStrict))
        {
            goto fail;
        }
    }
    /* Intentionally absent: extra headers, cancel/deadline, call hooks and
     * prefix-cache owner. They are runtime borrows, not immutable state. */
    xllmRequestUnit(pTarget);
    *pTarget = tCopy;
    return true;
fail:
    xllmRequestUnit(&tCopy);
    return false;
}

static void xllm_session__snapshot_destroy(xllm_session_snapshot *pSnapshot)
{
    if (!pSnapshot)
    {
        return;
    }
    free(pSnapshot->sSummaryStyle);
    free(pSnapshot->sSnapshotPath);
    free(pSnapshot->sSummary);
    xllm_session__snapshot_pending_unit(pSnapshot->pPendingCalls, pSnapshot->iPendingCallCount);
    xllm_session__snapshot_string_list_unit(pSnapshot->psReadFiles, pSnapshot->iReadFileCount);
    xllm_session__snapshot_string_list_unit(pSnapshot->psModifiedFiles,
                                            pSnapshot->iModifiedFileCount);
    xllmRequestUnit(&pSnapshot->tRequest);
    free(pSnapshot);
}

xllm_session_snapshot *xllmSessionSnapshotCreate(const xllm_session *pSession, xllm_error *pError)
{
    xllm_session_snapshot *pSnapshot;
    xllm_file_ledger tLedger;
    xllm_session_summary tSummary;
    xllm_request tRequest;
    xllm_error tRenderError;
    if (pError)
    {
        xllmErrorInit(pError);
    }
    if (!pSession)
    {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT, "source session is required");
        return NULL;
    }
    if (pSession->bInHook)
    {
        xllm_session__error(pError, XLLM_ERROR_HOOK, "session hook re-entered snapshot creation");
        return NULL;
    }
    if (!xllm_session__state_is_stable(pSession->eState))
    {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT,
                            "a snapshot can be created only from a stable writer state");
        return NULL;
    }
    pSnapshot = (xllm_session_snapshot *)xllm_session__calloc(1u, sizeof(*pSnapshot));
    if (!pSnapshot)
    {
        xllm_session__error(pError, XLLM_ERROR_OUT_OF_MEMORY,
                            "failed to allocate session snapshot");
        return NULL;
    }
    xrtAtomic32Init(&pSnapshot->tReferences, 1u);
    xllmRequestInit(&pSnapshot->tRequest);
    xllmErrorInit(&pSnapshot->tRequestError);
    pSnapshot->tConfig = pSession->tConfig;
    if (pSession->tConfig.sSummaryStyle)
    {
        pSnapshot->sSummaryStyle = xllm_session__strdup(pSession->tConfig.sSummaryStyle);
        if (!pSnapshot->sSummaryStyle)
        {
            goto oom;
        }
        pSnapshot->tConfig.sSummaryStyle = pSnapshot->sSummaryStyle;
    }
    if (pSession->tConfig.sSnapshotPath)
    {
        pSnapshot->sSnapshotPath = xllm_session__strdup(pSession->tConfig.sSnapshotPath);
        if (!pSnapshot->sSnapshotPath)
        {
            goto oom;
        }
        pSnapshot->tConfig.sSnapshotPath = pSnapshot->sSnapshotPath;
    }
    pSnapshot->eState = pSession->eState;
    pSnapshot->uCurrentTurn = pSession->uCurrentTurn;
    if (!xllmSessionGetStats(pSession, &pSnapshot->tStats) ||
        !xllmSessionGetTail(pSession, &pSnapshot->tTail))
    {
        goto invalid;
    }
    xllmSessionBudgetInit(&pSnapshot->tBudget);
    if (!xllmSessionGetBudget(pSession, &pSnapshot->tBudget))
    {
        goto invalid;
    }
    if (!xllmSessionGetSummary(pSession, &tSummary))
    {
        goto invalid;
    }
    pSnapshot->tSummary = tSummary;
    if (tSummary.sText)
    {
        pSnapshot->sSummary = xllm_session__strdup(tSummary.sText);
        if (!pSnapshot->sSummary)
        {
            goto oom;
        }
        pSnapshot->tSummary.sText = pSnapshot->sSummary;
    }
    if (!xllm_session__snapshot_pending(pSession, pSnapshot))
    {
        goto oom;
    }
    if (!xllmSessionGetFileLedger(pSession, &tLedger))
    {
        goto invalid;
    }
    if (!xllm_session__snapshot_string_list(tLedger.psReadFiles, tLedger.iReadFileCount,
                                            &pSnapshot->psReadFiles))
    {
        goto oom;
    }
    pSnapshot->iReadFileCount = tLedger.iReadFileCount;
    if (!xllm_session__snapshot_string_list(tLedger.psModifiedFiles, tLedger.iModifiedFileCount,
                                            &pSnapshot->psModifiedFiles))
    {
        goto oom;
    }
    pSnapshot->iModifiedFileCount = tLedger.iModifiedFileCount;

    if (pSnapshot->tStats.ePressure == XLLM_SESSION_PRESSURE_OVERFLOW ||
        pSnapshot->tStats.uNextMaxOutputTokens == 0u)
    {
        xllm_session__error(&pSnapshot->tRequestError, XLLM_ERROR_PROTOCOL,
                            "snapshot source has no renderable model-call budget");
        return pSnapshot;
    }
    xllmRequestInit(&tRequest);
    xllmErrorInit(&tRenderError);
    if (!xllmSessionBuildRequest(pSession, &tRequest, &tRenderError))
    {
        xllmRequestUnit(&tRequest);
        if (pError)
        {
            *pError = tRenderError;
        }
        xllm_session__snapshot_destroy(pSnapshot);
        return NULL;
    }
    /* The owned render result is detached from every runtime borrow before
     * publication. Its messages/tools/strings already belong to tRequest. */
    tRequest.pExtraHeaders = NULL;
    tRequest.iExtraHeaderCount = 0u;
    tRequest.pCancel = NULL;
    tRequest.uDeadline = UINT64_MAX;
    tRequest.pHooks = NULL;
    tRequest.pStablePrefixOwner = NULL;
    tRequest.uStablePrefixStamp = 0u;
    tRequest.iStableMessages = 0u;
    pSnapshot->tRequest = tRequest;
    pSnapshot->bRequestAvailable = true;
    return pSnapshot;

invalid:
    xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT, "failed to read the source session");
    xllm_session__snapshot_destroy(pSnapshot);
    return NULL;
oom:
    xllm_session__error(pError, XLLM_ERROR_OUT_OF_MEMORY, "failed to copy immutable session state");
    xllm_session__snapshot_destroy(pSnapshot);
    return NULL;
}

xllm_session_snapshot *xllmSessionSnapshotRetain(xllm_session_snapshot *pSnapshot)
{
    uint32 uExpected;
    if (!pSnapshot)
    {
        return NULL;
    }
    uExpected = xrtAtomic32Load(&pSnapshot->tReferences, XMEMORY_ACQUIRE);
    while (uExpected != 0u && uExpected != UINT32_MAX)
    {
        if (xrtAtomic32CompareExchange(&pSnapshot->tReferences, &uExpected, uExpected + 1u,
                                       XMEMORY_ACQ_REL, XMEMORY_ACQUIRE))
        {
            return pSnapshot;
        }
    }
    return NULL;
}

void xllmSessionSnapshotRelease(xllm_session_snapshot *pSnapshot)
{
    if (!pSnapshot)
    {
        return;
    }
    if (xrtAtomic32FetchSub(&pSnapshot->tReferences, 1u, XMEMORY_ACQ_REL) == 1u)
    {
        xllm_session__snapshot_destroy(pSnapshot);
    }
}

bool xllmSessionSnapshotGetConfig(const xllm_session_snapshot *pSnapshot,
                                  xllm_session_config *pConfig)
{
    if (!pSnapshot || !pConfig)
    {
        return false;
    }
    *pConfig = pSnapshot->tConfig;
    return true;
}

bool xllmSessionSnapshotGetBudget(const xllm_session_snapshot *pSnapshot,
                                  xllm_session_budget *pBudget)
{
    if (!pSnapshot || !pBudget || pBudget->uAbiVersion != XLLM_SESSION_BUDGET_ABI_VERSION ||
        pBudget->uSize < sizeof(*pBudget))
    {
        return false;
    }
    *pBudget = pSnapshot->tBudget;
    return true;
}

xllm_session_state xllmSessionSnapshotGetState(const xllm_session_snapshot *pSnapshot)
{
    return pSnapshot ? pSnapshot->eState : XLLM_SESSION_STATE_CORRUPT;
}

uint64_t xllmSessionSnapshotCurrentTurn(const xllm_session_snapshot *pSnapshot)
{
    return pSnapshot ? pSnapshot->uCurrentTurn : 0u;
}

bool xllmSessionSnapshotGetTail(const xllm_session_snapshot *pSnapshot, xllm_session_tail *pTail)
{
    if (!pSnapshot || !pTail)
    {
        return false;
    }
    *pTail = pSnapshot->tTail;
    return true;
}

size_t xllmSessionSnapshotPendingToolCallCount(const xllm_session_snapshot *pSnapshot)
{
    return pSnapshot ? pSnapshot->iPendingCallCount : 0u;
}

bool xllmSessionSnapshotPendingToolCallAt(const xllm_session_snapshot *pSnapshot, size_t iIndex,
                                          xllm_pending_tool_call *pCall)
{
    if (!pSnapshot || !pCall || iIndex >= pSnapshot->iPendingCallCount)
    {
        return false;
    }
    *pCall = pSnapshot->pPendingCalls[iIndex];
    return true;
}

bool xllmSessionSnapshotGetStats(const xllm_session_snapshot *pSnapshot, xllm_session_stats *pStats)
{
    if (!pSnapshot || !pStats)
    {
        return false;
    }
    *pStats = pSnapshot->tStats;
    return true;
}

bool xllmSessionSnapshotGetSummary(const xllm_session_snapshot *pSnapshot,
                                   xllm_session_summary *pSummary)
{
    if (!pSnapshot || !pSummary)
    {
        return false;
    }
    *pSummary = pSnapshot->tSummary;
    return true;
}

bool xllmSessionSnapshotGetFileLedger(const xllm_session_snapshot *pSnapshot,
                                      xllm_file_ledger *pLedger)
{
    if (!pSnapshot || !pLedger)
    {
        return false;
    }
    pLedger->psReadFiles = (const char *const *)pSnapshot->psReadFiles;
    pLedger->iReadFileCount = pSnapshot->iReadFileCount;
    pLedger->psModifiedFiles = (const char *const *)pSnapshot->psModifiedFiles;
    pLedger->iModifiedFileCount = pSnapshot->iModifiedFileCount;
    return true;
}

bool xllmSessionSnapshotBuildRequest(const xllm_session_snapshot *pSnapshot, xllm_request *pRequest,
                                     xllm_error *pError)
{
    if (pError)
    {
        xllmErrorInit(pError);
    }
    if (!pSnapshot || !pRequest)
    {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT,
                            "snapshot and initialized request are required");
        return false;
    }
    if (!pSnapshot->bRequestAvailable)
    {
        if (pError)
        {
            *pError = pSnapshot->tRequestError;
        }
        return false;
    }
    if (!xllm_session__snapshot_request_clone(pRequest, &pSnapshot->tRequest))
    {
        xllm_session__error(pError, XLLM_ERROR_OUT_OF_MEMORY,
                            "failed to clone the immutable session request");
        return false;
    }
    return true;
}
