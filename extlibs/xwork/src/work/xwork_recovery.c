#include "../internal/xwork_internal.h"

/* Durable interruption recovery.
 *
 * Session journals can prove that a tool result is missing, but they cannot
 * prove that the external side effect did not happen before the process died.
 * Recovery therefore preflights the complete pending batch and retries only
 * current read-only descriptors unless the host names an explicit action.
 */

static void xwork__recovery_call_entry_unit(xwork_recovery_call_entry *pEntry)
{
    if (!pEntry)
        return;
    free(pEntry->sToolCallId);
    free(pEntry->sToolName);
    free(pEntry->sArgumentsJson);
    memset(pEntry, 0, sizeof(*pEntry));
}

static bool xwork__recovery_call_entry_copy(xwork_recovery_call_entry *pTarget,
                                            const xllm_pending_tool_call *pSource,
                                            const xwork_agent *pAgent)
{
    const xwork_tool_entry *pTool;
    if (!pTarget || !pSource || !pAgent)
        return false;
    memset(pTarget, 0, sizeof(*pTarget));
    pTarget->sToolCallId = xwork__strdup(pSource->sId ? pSource->sId : "");
    pTarget->sToolName = xwork__strdup(pSource->sName ? pSource->sName : "");
    pTarget->sArgumentsJson =
        xwork__strdup(pSource->sArgumentsJson ? pSource->sArgumentsJson : "{}");
    if (!pTarget->sToolCallId || !pTarget->sToolName || !pTarget->sArgumentsJson)
    {
        xwork__recovery_call_entry_unit(pTarget);
        return false;
    }
    pTool = xwork__find_tool(pAgent, pTarget->sToolName);
    pTarget->tInfo.uSize = (uint32_t)sizeof(pTarget->tInfo);
    pTarget->tInfo.uAbiVersion = XWORK_ABI_VERSION;
    pTarget->tInfo.uTurn = pSource->uTurn;
    pTarget->tInfo.uCatalogGeneration = pAgent->uToolRegistryGeneration;
    pTarget->tInfo.sToolCallId = pTarget->sToolCallId;
    pTarget->tInfo.sToolName = pTarget->sToolName;
    pTarget->tInfo.sArgumentsJson = pTarget->sArgumentsJson;
    pTarget->tInfo.uEffects = pTool ? pTool->uEffects : XWORK_TOOL_EFFECT_ALL;
    pTarget->tInfo.bToolAvailable = pTool != NULL;
    pTarget->tInfo.bAutomaticRetrySafe =
        pTool != NULL &&
        (pTool->bRetrySafeRead || (pTool->uEffects & ~XWORK_TOOL_EFFECT_READ) == 0u);
    return true;
}

static xwork_recovery_snapshot *xwork__recovery_snapshot_create_unlocked(xwork_agent *pAgent,
                                                                         xwork_error *pError)
{
    xwork_recovery_snapshot *pSnapshot;
    size_t iCount;
    size_t i;
    iCount = xllmSessionPendingToolCallCount(pAgent->pSession);
    if (iCount > SIZE_MAX / sizeof(*pSnapshot->pCalls))
    {
        xwork__set_error(pError, XWORK_ERROR_LIMIT,
                         "pending recovery call count exceeds addressable memory");
        return NULL;
    }
    pSnapshot = (xwork_recovery_snapshot *)calloc(1u, sizeof(*pSnapshot));
    if (!pSnapshot)
        goto oom;
    pSnapshot->iRefs = 1;
    if (iCount)
    {
        pSnapshot->pCalls = (xwork_recovery_call_entry *)calloc(iCount, sizeof(*pSnapshot->pCalls));
        if (!pSnapshot->pCalls)
            goto oom;
    }
    for (i = 0u; i < iCount; ++i)
    {
        xllm_pending_tool_call tPending;
        if (!xllmSessionPendingToolCallAt(pAgent->pSession, i, &tPending))
        {
            xwork__set_error(pError, XWORK_ERROR_CONTEXT,
                             "durable pending tool calls changed during recovery inspection");
            xworkRecoverySnapshotRelease(pSnapshot);
            return NULL;
        }
        if (!xwork__recovery_call_entry_copy(&pSnapshot->pCalls[i], &tPending, pAgent))
            goto oom;
        ++pSnapshot->iCallCount;
    }
    return pSnapshot;
oom:
    xworkRecoverySnapshotRelease(pSnapshot);
    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to capture durable recovery calls");
    return NULL;
}

void xworkRecoveryCallInfoInit(xwork_recovery_call_info *pInfo)
{
    if (!pInfo)
        return;
    memset(pInfo, 0, sizeof(*pInfo));
    pInfo->uSize = (uint32_t)sizeof(*pInfo);
    pInfo->uAbiVersion = XWORK_ABI_VERSION;
}

void xworkRecoveryDecisionInit(xwork_recovery_decision *pDecision)
{
    if (!pDecision)
        return;
    memset(pDecision, 0, sizeof(*pDecision));
    pDecision->uSize = (uint32_t)sizeof(*pDecision);
    pDecision->uAbiVersion = XWORK_ABI_VERSION;
}

void xworkResumeOptionsInit(xwork_resume_options *pOptions)
{
    if (!pOptions)
        return;
    memset(pOptions, 0, sizeof(*pOptions));
    pOptions->uSize = (uint32_t)sizeof(*pOptions);
    pOptions->uAbiVersion = XWORK_ABI_VERSION;
}

void xwork__resume_options_unit(xwork_resume_options *pOptions,
                                xwork_recovery_decision *pOwnedDecisions)
{
    size_t i;
    if (pOptions && pOwnedDecisions)
    {
        for (i = 0u; i < pOptions->iDecisionCount; ++i)
        {
            free((char *)pOwnedDecisions[i].sToolCallId);
        }
    }
    free(pOwnedDecisions);
    if (pOptions)
        memset(pOptions, 0, sizeof(*pOptions));
}

bool xwork__resume_options_clone(xwork_resume_options *pTarget,
                                 xwork_recovery_decision **ppOwnedDecisions,
                                 const xwork_resume_options *pSource, xwork_error *pError)
{
    xwork_recovery_decision *pCopy = NULL;
    size_t i;
    size_t j;
    if (!pTarget || !ppOwnedDecisions || !pSource || pSource->uAbiVersion != XWORK_ABI_VERSION ||
        (size_t)pSource->uSize < sizeof(*pSource) ||
        (pSource->iDecisionCount && !pSource->pDecisions) || pSource->iDecisionCount > 4096u)
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
                         "resume options are invalid or exceed the decision limit");
        return false;
    }
    memset(pTarget, 0, sizeof(*pTarget));
    *ppOwnedDecisions = NULL;
    if (pSource->iDecisionCount)
    {
        pCopy = (xwork_recovery_decision *)calloc(pSource->iDecisionCount, sizeof(*pCopy));
        if (!pCopy)
            goto oom;
    }
    xworkResumeOptionsInit(pTarget);
    pTarget->pDecisions = pCopy;
    for (i = 0u; i < pSource->iDecisionCount; ++i)
    {
        const xwork_recovery_decision *pDecision = &pSource->pDecisions[i];
        if (pDecision->uAbiVersion != XWORK_ABI_VERSION ||
            (size_t)pDecision->uSize < sizeof(*pDecision) || !pDecision->sToolCallId ||
            !pDecision->sToolCallId[0] || pDecision->eAction < XWORK_RECOVERY_DEFAULT ||
            pDecision->eAction > XWORK_RECOVERY_RECORD_UNCERTAIN)
        {
            xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
                             "a recovery decision is invalid");
            goto fail;
        }
        for (j = 0u; j < i; ++j)
        {
            if (strcmp(pDecision->sToolCallId, pSource->pDecisions[j].sToolCallId) == 0)
            {
                xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
                                 "recovery decisions contain a duplicate tool-call ID");
                goto fail;
            }
        }
        xworkRecoveryDecisionInit(&pCopy[i]);
        pCopy[i].sToolCallId = xwork__strdup(pDecision->sToolCallId);
        pCopy[i].eAction = pDecision->eAction;
        if (!pCopy[i].sToolCallId)
            goto oom;
        ++pTarget->iDecisionCount;
    }
    *ppOwnedDecisions = pCopy;
    return true;
oom:
    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to copy resume decisions");
fail:
    xwork__resume_options_unit(pTarget, pCopy);
    return false;
}

xwork_recovery_snapshot *xworkAgentRecoverySnapshot(xwork_agent *pAgent, xwork_error *pError)
{
    xwork_recovery_snapshot *pSnapshot;
    xworkErrorInit(pError);
    if (!pAgent || !pAgent->pStateLock || !xrtMutexLock(pAgent->pStateLock))
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "agent is null or unavailable");
        return NULL;
    }
    if (pAgent->bRunning || pAgent->bClosing)
    {
        (void)xrtMutexUnlock(pAgent->pStateLock);
        xwork__set_error(pError, XWORK_ERROR_CONTEXT,
                         "recovery calls can be inspected only while the agent is idle");
        return NULL;
    }
    pSnapshot = xwork__recovery_snapshot_create_unlocked(pAgent, pError);
    (void)xrtMutexUnlock(pAgent->pStateLock);
    return pSnapshot;
}

xwork_recovery_snapshot *xworkRecoverySnapshotRef(xwork_recovery_snapshot *pSnapshot)
{
    if (pSnapshot)
        (void)xwork__atomic_inc(&pSnapshot->iRefs);
    return pSnapshot;
}

void xworkRecoverySnapshotRelease(xwork_recovery_snapshot *pSnapshot)
{
    size_t i;
    if (!pSnapshot || xwork__atomic_dec(&pSnapshot->iRefs) != 0)
        return;
    for (i = 0u; i < pSnapshot->iCallCount; ++i)
    {
        xwork__recovery_call_entry_unit(&pSnapshot->pCalls[i]);
    }
    free(pSnapshot->pCalls);
    free(pSnapshot);
}

size_t xworkRecoverySnapshotCount(const xwork_recovery_snapshot *pSnapshot)
{
    return pSnapshot ? pSnapshot->iCallCount : 0u;
}

bool xworkRecoverySnapshotAt(const xwork_recovery_snapshot *pSnapshot, size_t iIndex,
                             xwork_recovery_call_info *pInfo)
{
    if (!pSnapshot || !pInfo || pInfo->uAbiVersion != XWORK_ABI_VERSION ||
        (size_t)pInfo->uSize < sizeof(*pInfo) || iIndex >= pSnapshot->iCallCount)
        return false;
    *pInfo = pSnapshot->pCalls[iIndex].tInfo;
    return true;
}

static bool xwork__recovery_decisions_valid(const xwork_resume_options *pOptions,
                                            const xwork_recovery_snapshot *pSnapshot,
                                            xwork_error *pError)
{
    size_t i;
    size_t j;
    if (!pOptions)
        return true;
    if (pOptions->uAbiVersion != XWORK_ABI_VERSION || (size_t)pOptions->uSize < sizeof(*pOptions) ||
        (pOptions->iDecisionCount && !pOptions->pDecisions) || pOptions->iDecisionCount > 4096u)
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
                         "resume options are invalid or exceed the decision limit");
        return false;
    }
    for (i = 0u; i < pOptions->iDecisionCount; ++i)
    {
        const xwork_recovery_decision *pDecision = &pOptions->pDecisions[i];
        bool bMatched = false;
        if (pDecision->uAbiVersion != XWORK_ABI_VERSION ||
            (size_t)pDecision->uSize < sizeof(*pDecision) || !pDecision->sToolCallId ||
            !pDecision->sToolCallId[0] || pDecision->eAction < XWORK_RECOVERY_DEFAULT ||
            pDecision->eAction > XWORK_RECOVERY_RECORD_UNCERTAIN)
        {
            xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
                             "a recovery decision is invalid");
            return false;
        }
        for (j = 0u; j < i; ++j)
        {
            if (strcmp(pDecision->sToolCallId, pOptions->pDecisions[j].sToolCallId) == 0)
            {
                xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
                                 "recovery decisions contain a duplicate tool-call ID");
                return false;
            }
        }
        for (j = 0u; j < pSnapshot->iCallCount; ++j)
        {
            if (strcmp(pDecision->sToolCallId, pSnapshot->pCalls[j].sToolCallId) == 0)
            {
                bMatched = true;
                break;
            }
        }
        if (!bMatched)
        {
            xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
                             "a recovery decision does not match a pending tool-call ID");
            return false;
        }
    }
    return true;
}

static xwork_recovery_action xwork__recovery_decision_for(const xwork_resume_options *pOptions,
                                                          const char *sToolCallId)
{
    size_t i;
    if (!pOptions)
        return XWORK_RECOVERY_DEFAULT;
    for (i = 0u; i < pOptions->iDecisionCount; ++i)
    {
        if (strcmp(pOptions->pDecisions[i].sToolCallId, sToolCallId) == 0)
        {
            return pOptions->pDecisions[i].eAction;
        }
    }
    return XWORK_RECOVERY_DEFAULT;
}

xwork_recovery_plan *xwork__recovery_plan_create(xwork_agent *pAgent,
                                                 const xwork_resume_options *pOptions,
                                                 xwork_error *pError)
{
    xwork_recovery_snapshot *pSnapshot;
    xwork_recovery_plan *pPlan;
    size_t i;
    pSnapshot = xwork__recovery_snapshot_create_unlocked(pAgent, pError);
    if (!pSnapshot)
        return NULL;
    if (!xwork__recovery_decisions_valid(pOptions, pSnapshot, pError))
    {
        xworkRecoverySnapshotRelease(pSnapshot);
        return NULL;
    }
    for (i = 0u; i < pSnapshot->iCallCount; ++i)
    {
        size_t j;
        if (!pSnapshot->pCalls[i].sToolCallId[0])
        {
            xwork__set_error(pError, XWORK_ERROR_CONTEXT,
                             "a durable pending tool call has no stable ID");
            xworkRecoverySnapshotRelease(pSnapshot);
            return NULL;
        }
        for (j = 0u; j < i; ++j)
        {
            if (strcmp(pSnapshot->pCalls[i].sToolCallId, pSnapshot->pCalls[j].sToolCallId) == 0)
            {
                xwork__set_error(pError, XWORK_ERROR_CONTEXT,
                                 "durable pending tool calls contain a duplicate ID");
                xworkRecoverySnapshotRelease(pSnapshot);
                return NULL;
            }
        }
    }
    pPlan = (xwork_recovery_plan *)calloc(1u, sizeof(*pPlan));
    if (!pPlan)
        goto oom;
    if (pSnapshot->iCallCount)
    {
        pPlan->pEntries =
            (xwork_recovery_plan_entry *)calloc(pSnapshot->iCallCount, sizeof(*pPlan->pEntries));
        if (!pPlan->pEntries)
            goto oom;
    }
    for (i = 0u; i < pSnapshot->iCallCount; ++i)
    {
        xwork_recovery_plan_entry *pEntry = &pPlan->pEntries[i];
        xwork_recovery_action eAction =
            xwork__recovery_decision_for(pOptions, pSnapshot->pCalls[i].sToolCallId);
        xllm_pending_tool_call tPending;
        tPending.uTurn = pSnapshot->pCalls[i].tInfo.uTurn;
        tPending.sId = pSnapshot->pCalls[i].sToolCallId;
        tPending.sName = pSnapshot->pCalls[i].sToolName;
        tPending.sArgumentsJson = pSnapshot->pCalls[i].sArgumentsJson;
        if (!xwork__recovery_call_entry_copy(&pEntry->tCall, &tPending, pAgent))
            goto oom;
        ++pPlan->iEntryCount;
        if (eAction == XWORK_RECOVERY_DEFAULT && pEntry->tCall.tInfo.bAutomaticRetrySafe)
        {
            eAction = XWORK_RECOVERY_RETRY;
            pEntry->bAutomatic = true;
        }
        pEntry->eAction = eAction;
        if (eAction == XWORK_RECOVERY_DEFAULT)
        {
            xwork_event tEvent;
            char sMessage[512];
            (void)snprintf(sMessage, sizeof(sMessage),
                           "recovery decision required for pending tool '%s' (call '%s')",
                           pEntry->tCall.sToolName, pEntry->tCall.sToolCallId);
            xwork__set_error(pError, XWORK_ERROR_RECOVERY_REQUIRED, sMessage);
            memset(&tEvent, 0, sizeof(tEvent));
            tEvent.eKind = XWORK_EVENT_RECOVERY_REQUIRED;
            tEvent.uAgentTurn = pEntry->tCall.tInfo.uTurn;
            tEvent.sText = pError ? pError->sMessage : sMessage;
            tEvent.iTextLength = strlen(tEvent.sText);
            tEvent.sToolName = pEntry->tCall.sToolName;
            tEvent.sToolCallId = pEntry->tCall.sToolCallId;
            tEvent.uEffects = pEntry->tCall.tInfo.uEffects;
            tEvent.eRecoveryAction = XWORK_RECOVERY_DEFAULT;
            tEvent.bSuccess = false;
            (void)xwork__emit(pAgent, &tEvent);
            xworkRecoverySnapshotRelease(pSnapshot);
            xwork__recovery_plan_destroy(pPlan);
            return NULL;
        }
    }
    xworkRecoverySnapshotRelease(pSnapshot);
    return pPlan;
oom:
    xworkRecoverySnapshotRelease(pSnapshot);
    xwork__recovery_plan_destroy(pPlan);
    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY,
                     "failed to prepare the durable recovery plan");
    return NULL;
}

void xwork__recovery_plan_destroy(xwork_recovery_plan *pPlan)
{
    size_t i;
    if (!pPlan)
        return;
    for (i = 0u; i < pPlan->iEntryCount; ++i)
    {
        xwork__recovery_call_entry_unit(&pPlan->pEntries[i].tCall);
    }
    free(pPlan->pEntries);
    free(pPlan);
}
