#include "../internal/xwork_internal.h"

/* Deterministic resource-aware tool batching.
 *
 * Planning is single-threaded and side-effect free: it snapshots the catalog,
 * asks each tool for permission resources exactly once, normalizes paths, and
 * assigns the earliest wave that follows every conflicting predecessor.
 * Execution uses bounded wave barriers. Results remain indexed by the model's
 * original sequence even when workers finish out of order. */

typedef struct xwork_batch_worker
{
    xwork_agent *pAgent;
    const xllm_tool_call *pCall;
    const xllm_executor_ctx *pContext;
    const xwork_tool_batch_plan_entry *pPlan;
    xwork_tool_batch_output *pOutput;
    xwork_result eResult;
    xwork_error tError;
} xwork_batch_worker;

static const xwork_tool_entry *xwork__catalog_find_tool(const xwork_tool_catalog *pCatalog,
                                                        const char *sName)
{
    size_t i;
    if (!pCatalog || !sName)
        return NULL;
    sName = xwork__canonical_tool_name(sName);
    for (i = 0u; i < pCatalog->iToolCount; ++i)
    {
        if (strcmp(pCatalog->pTools[i].sName, sName) == 0)
        {
            return &pCatalog->pTools[i];
        }
    }
    return NULL;
}

static bool xwork__schedule_path_char_equal(char a, char b)
{
#if defined(_WIN32)
    return tolower((unsigned char)a) == tolower((unsigned char)b);
#else
    return a == b;
#endif
}

static bool xwork__path_prefix(const char *sParent, const char *sChild)
{
    size_t i = 0u;
    if (!sParent || !sChild)
        return false;
    while (sParent[i] && sChild[i] && xwork__schedule_path_char_equal(sParent[i], sChild[i]))
        ++i;
    if (sParent[i] != '\0')
        return false;
    if (sChild[i] == '\0')
        return true;
    if (i != 0u && (sParent[i - 1u] == '/' || sParent[i - 1u] == '\\'))
    {
        return true;
    }
    return sChild[i] == '/' || sChild[i] == '\\';
}

static bool xwork__resource_mutates(const xwork_permission_resource *pResource)
{
    return pResource &&
           (pResource->uAccess & ~((xwork_resource_access)XWORK_RESOURCE_ACCESS_READ)) != 0u;
}

static bool xwork__resource_conflict(const xwork_tool_batch_plan_entry *pLeft, size_t iLeft,
                                     const xwork_tool_batch_plan_entry *pRight, size_t iRight)
{
    const xwork_permission_resource *pA = &pLeft->tResources.pResources[iLeft];
    const xwork_permission_resource *pB = &pRight->tResources.pResources[iRight];
    const char *sA =
        pLeft->psNormalizedResources ? pLeft->psNormalizedResources[iLeft] : pA->sResource;
    const char *sB =
        pRight->psNormalizedResources ? pRight->psNormalizedResources[iRight] : pB->sResource;
    if (pA->eKind != pB->eKind || (!xwork__resource_mutates(pA) && !xwork__resource_mutates(pB)))
    {
        return false;
    }
    if (pA->eKind == XWORK_RESOURCE_PATH)
    {
        return xwork__path_prefix(sA, sB) || xwork__path_prefix(sB, sA);
    }
#if defined(_WIN32)
    return _stricmp(sA ? sA : "", sB ? sB : "") == 0;
#else
    return strcmp(sA ? sA : "", sB ? sB : "") == 0;
#endif
}

static bool xwork__plan_entries_conflict(const xwork_tool_batch_plan_entry *pLeft,
                                         const xwork_tool_batch_plan_entry *pRight)
{
    size_t i;
    size_t j;
    const xwork_tool_effects uGlobal =
        XWORK_TOOL_EFFECT_PROCESS | XWORK_TOOL_EFFECT_SCHEDULE | XWORK_TOOL_EFFECT_AGENT_DELEGATION;
    if (!pLeft->pTool || !pRight->pTool || pLeft->bDescriptorFailed || pRight->bDescriptorFailed)
        return true;
    if ((pLeft->pTool->uEffects & pRight->pTool->uEffects & uGlobal) != 0u)
    {
        return true;
    }
    if (pLeft->pTool->sSerialGroup && pRight->pTool->sSerialGroup &&
        strcmp(pLeft->pTool->sSerialGroup, pRight->pTool->sSerialGroup) == 0)
        return true;
    if (!pLeft->pTool->bParallelSafe || !pRight->pTool->bParallelSafe)
    {
        return true;
    }
    for (i = 0u; i < pLeft->tResources.iResourceCount; ++i)
    {
        for (j = 0u; j < pRight->tResources.iResourceCount; ++j)
        {
            if (xwork__resource_conflict(pLeft, i, pRight, j))
                return true;
        }
    }
    return false;
}

static void xwork__plan_entry_unit(xwork_tool_batch_plan_entry *pEntry)
{
    size_t i;
    if (!pEntry)
        return;
    if (pEntry->psNormalizedResources)
    {
        for (i = 0u; i < pEntry->tResources.iResourceCount; ++i)
        {
            free(pEntry->psNormalizedResources[i]);
        }
    }
    free(pEntry->psNormalizedResources);
    free(pEntry->sToolName);
    xwork__permission_writer_unit(&pEntry->tResources);
    memset(pEntry, 0, sizeof(*pEntry));
}

static bool xwork__plan_entry_prepare(xwork_tool_batch_plan *pPlan,
                                      xwork_tool_batch_plan_entry *pEntry,
                                      const xllm_tool_call *pCall, size_t iSequence,
                                      const xllm_executor_ctx *pContext)
{
    xwork_error tDescriptorError;
    size_t i;
    memset(pEntry, 0, sizeof(*pEntry));
    pEntry->iSequence = iSequence;
    pEntry->sToolName = xwork__strdup(xwork__canonical_tool_name(pCall->sName ? pCall->sName : ""));
    if (!pEntry->sToolName)
        return false;
    pEntry->pTool = xwork__catalog_find_tool(pPlan->pCatalog, pEntry->sToolName);
    memset(&pEntry->tContext, 0, sizeof(pEntry->tContext));
    pEntry->tContext.pAgent = pPlan->pAgent;
    pEntry->tContext.sWorkspaceRoot = pPlan->pAgent->sWorkspaceRoot;
    pEntry->tContext.sToolName = pEntry->sToolName;
    pEntry->tContext.sToolCallId = pCall->sId;
    pEntry->tContext.uEffects = pEntry->pTool ? pEntry->pTool->uEffects : 0u;
    pEntry->tContext.uAgentTurn = pContext ? pContext->uTurn : 0u;
    pEntry->tContext.uAgentId = pPlan->pAgent->uAgentId;
    pEntry->tContext.uRunId = pPlan->uRunId;
    pEntry->tContext.uCatalogGeneration = pPlan->pCatalog->uGeneration;
    pEntry->tContext.pCancel = pContext ? pContext->pCancel : NULL;
    pEntry->tContext.uDeadline = pContext ? pContext->uDeadline : 0u;
    xwork__permission_writer_init(&pEntry->tResources);
    if (!pEntry->pTool)
    {
        pEntry->bDescriptorFailed = true;
        return true;
    }
    xworkErrorInit(&tDescriptorError);
    if (pEntry->pTool->OnDescribePermissions &&
        (pEntry->pTool->OnDescribePermissions(pEntry->pTool->pUserData, &pEntry->tContext,
                                              pCall->sArgumentsJson ? pCall->sArgumentsJson : "{}",
                                              &pEntry->tResources,
                                              &tDescriptorError) != XWORK_RESULT_OK ||
         pEntry->tResources.bFailed))
    {
        pEntry->bDescriptorFailed = true;
        if (!pEntry->tResources.bFailed)
        {
            pEntry->tResources.bFailed = true;
            pEntry->tResources.eFailure = tDescriptorError.eCode != XWORK_ERROR_NONE
                                              ? tDescriptorError.eCode
                                              : XWORK_ERROR_CONTEXT;
            (void)snprintf(pEntry->tResources.sFailure, sizeof(pEntry->tResources.sFailure), "%.*s",
                           (int)sizeof(pEntry->tResources.sFailure) - 1,
                           tDescriptorError.sMessage[0]
                               ? tDescriptorError.sMessage
                               : "permission descriptor failed without an error reason");
        }
        return true;
    }
    if (pEntry->tResources.iResourceCount)
    {
        pEntry->psNormalizedResources = (char **)calloc(pEntry->tResources.iResourceCount,
                                                        sizeof(*pEntry->psNormalizedResources));
        if (!pEntry->psNormalizedResources)
            return false;
    }
    for (i = 0u; i < pEntry->tResources.iResourceCount; ++i)
    {
        const xwork_permission_resource *pResource = &pEntry->tResources.pResources[i];
        if (pResource->eKind == XWORK_RESOURCE_PATH)
        {
            xwork_error tPathError;
            xworkErrorInit(&tPathError);
            /* A read-only result is confined by its anchored store, not by
             * the workspace mount table. Retain the canonical logical key for
             * batch conflict detection; writes never receive this exception. */
            pEntry->psNormalizedResources[i] =
                pResource->uAccess == XWORK_RESOURCE_ACCESS_READ &&
                        !strncmp(pResource->sResource, "@results", 8u) &&
                        (pResource->sResource[8] == '/' || !pResource->sResource[8])
                    ? xwork__strdup(pResource->sResource)
                    : xwork__resolve_path(pPlan->pAgent, pResource->sResource, &tPathError);
            if (!pEntry->psNormalizedResources[i])
            {
                pEntry->bDescriptorFailed = true;
                return true;
            }
        }
        else
        {
            pEntry->psNormalizedResources[i] = xwork__strdup(pResource->sResource);
            if (!pEntry->psNormalizedResources[i])
                return false;
        }
    }
    return true;
}

xwork_tool_batch_plan *xworkAgentPlanToolBatch(xwork_agent *pAgent, const xllm_tool_call *pCalls,
                                               size_t iCallCount, const xllm_executor_ctx *pContext,
                                               xwork_error *pError)
{
    xwork_tool_batch_plan *pPlan = NULL;
    size_t i;
    xworkErrorInit(pError);
    if (!pAgent || (!pCalls && iCallCount) || iCallCount > 4096u)
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
                         "invalid tool batch planning arguments");
        return NULL;
    }
    pPlan = (xwork_tool_batch_plan *)calloc(1u, sizeof(*pPlan));
    if (!pPlan)
        goto oom;
    pPlan->iRefs = 1;
    pPlan->pAgent = xwork__agent_ref(pAgent);
    if (!pPlan->pAgent || !pPlan->pAgent->pStateLock || !xrtMutexLock(pPlan->pAgent->pStateLock))
        goto context;
    pPlan->uRunId = pPlan->pAgent->pActiveRun ? pPlan->pAgent->pActiveRun->uRunId : 0u;
    (void)xrtMutexUnlock(pPlan->pAgent->pStateLock);
    pPlan->pCatalog = xworkAgentToolCatalogSnapshot(pAgent);
    pPlan->iEntryCount = iCallCount;
    if (iCallCount)
    {
        pPlan->pEntries =
            (xwork_tool_batch_plan_entry *)calloc(iCallCount, sizeof(*pPlan->pEntries));
    }
    if (!pPlan->pAgent || !pPlan->pCatalog || (iCallCount && !pPlan->pEntries))
        goto oom;
    for (i = 0u; i < iCallCount; ++i)
    {
        size_t j;
        uint32_t uWave = 0u;
        if (!xwork__plan_entry_prepare(pPlan, &pPlan->pEntries[i], &pCalls[i], i, pContext))
            goto oom;
        for (j = 0u; j < i; ++j)
        {
            if (xwork__plan_entries_conflict(&pPlan->pEntries[j], &pPlan->pEntries[i]) &&
                uWave <= pPlan->pEntries[j].uWave)
            {
                uWave = pPlan->pEntries[j].uWave + 1u;
            }
        }
        pPlan->pEntries[i].uWave = uWave;
        if (pPlan->uWaveCount <= uWave)
            pPlan->uWaveCount = uWave + 1u;
    }
    return pPlan;
oom:
    xworkToolBatchPlanRelease(pPlan);
    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to prepare tool batch schedule");
    return NULL;
context:
    xworkToolBatchPlanRelease(pPlan);
    xwork__set_error(pError, XWORK_ERROR_CONTEXT,
                     "failed to snapshot the active run for tool scheduling");
    return NULL;
}

xwork_tool_batch_plan *xworkToolBatchPlanRef(xwork_tool_batch_plan *pPlan)
{
    if (!pPlan || xwork__atomic_inc(&pPlan->iRefs) <= 1)
        return NULL;
    return pPlan;
}

void xworkToolBatchPlanRelease(xwork_tool_batch_plan *pPlan)
{
    size_t i;
    if (!pPlan || xwork__atomic_dec(&pPlan->iRefs) != 0)
        return;
    for (i = 0u; i < pPlan->iEntryCount; ++i)
    {
        xwork__plan_entry_unit(&pPlan->pEntries[i]);
    }
    free(pPlan->pEntries);
    xworkToolCatalogRelease(pPlan->pCatalog);
    xwork__agent_release(pPlan->pAgent);
    free(pPlan);
}

size_t xworkToolBatchPlanCount(const xwork_tool_batch_plan *pPlan)
{
    return pPlan ? pPlan->iEntryCount : 0u;
}

void xworkToolBatchItemInfoInit(xwork_tool_batch_item_info *pInfo)
{
    if (!pInfo)
        return;
    memset(pInfo, 0, sizeof(*pInfo));
    pInfo->uSize = (uint32_t)sizeof(*pInfo);
    pInfo->uAbiVersion = XWORK_ABI_VERSION;
}

uint32_t xworkToolBatchPlanWaveCount(const xwork_tool_batch_plan *pPlan)
{
    return pPlan ? pPlan->uWaveCount : 0u;
}

bool xworkToolBatchPlanItemAt(const xwork_tool_batch_plan *pPlan, size_t iIndex,
                              xwork_tool_batch_item_info *pInfo)
{
    const xwork_tool_batch_plan_entry *pEntry;
    uint32_t uSize;
    uint32_t uVersion;
    if (!pPlan || !pInfo || iIndex >= pPlan->iEntryCount ||
        pInfo->uAbiVersion != XWORK_ABI_VERSION || pInfo->uSize < sizeof(*pInfo))
        return false;
    pEntry = &pPlan->pEntries[iIndex];
    uSize = pInfo->uSize;
    uVersion = pInfo->uAbiVersion;
    memset(pInfo, 0, sizeof(*pInfo));
    pInfo->uSize = uSize;
    pInfo->uAbiVersion = uVersion;
    pInfo->iSequence = pEntry->iSequence;
    pInfo->uWave = pEntry->uWave;
    pInfo->sToolName = pEntry->sToolName;
    pInfo->uEffects = pEntry->pTool ? pEntry->pTool->uEffects : 0u;
    pInfo->bParallelSafe = pEntry->pTool && pEntry->pTool->bParallelSafe;
    pInfo->sSerialGroup = pEntry->pTool ? pEntry->pTool->sSerialGroup : NULL;
    pInfo->pResources = pEntry->tResources.pResources;
    pInfo->iResourceCount = pEntry->tResources.iResourceCount;
    return true;
}

static int32 xwork__batch_worker_proc(ptr pData)
{
    xwork_batch_worker *pWorker = (xwork_batch_worker *)pData;
    xworkErrorInit(&pWorker->tError);
    pWorker->eResult = xwork__execute_tool_prepared(
        pWorker->pAgent, pWorker->pCall, pWorker->pContext ? pWorker->pContext->uTurn : 0u,
        pWorker->pContext ? pWorker->pContext->pCancel : NULL,
        pWorker->pContext ? pWorker->pContext->uDeadline : XLLM_DEADLINE_NEVER,
        &pWorker->pPlan->tResources, &pWorker->pOutput->sContent, &pWorker->pOutput->bSuccess,
        &pWorker->pOutput->bEffectApplied, &pWorker->pOutput->pImageBytes,
        &pWorker->pOutput->iImageSize, pWorker->pOutput->sImageMime, &pWorker->tError);
    return 0;
}

void xwork__tool_batch_outputs_unit(xwork_tool_batch_output *pOutputs, size_t iOutputCount)
{
    size_t i;
    if (!pOutputs)
        return;
    for (i = 0u; i < iOutputCount; ++i)
    {
        free(pOutputs[i].sContent);
        free(pOutputs[i].pImageBytes);
        memset(&pOutputs[i], 0, sizeof(pOutputs[i]));
    }
}

bool xwork__execute_tool_batch(xwork_agent *pAgent, const xllm_tool_call *pCalls, size_t iCallCount,
                               const xllm_executor_ctx *pContext, xwork_tool_batch_output *pOutputs,
                               xwork_error *pError)
{
    xwork_tool_batch_plan *pPlan;
    uint32_t uWave;
    uint32_t uLimit;
    bool bOk = true;
    size_t i;
    xworkErrorInit(pError);
    if (!pAgent || (!pCalls && iCallCount) || (!pOutputs && iCallCount))
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
                         "invalid tool batch execution arguments");
        return false;
    }
    uint32_t PlanRecoveries=0;
    while(!(pPlan=xworkAgentPlanToolBatch(pAgent,pCalls,iCallCount,pContext,pError))){
        /* No worker or tool has started; this planning allocation is safe. */
        if(!pError||pError->eCode!=XWORK_ERROR_OUT_OF_MEMORY||
            !xwork__recover(pAgent,"resource",pError,++PlanRecoveries,true))return false;
        pAgent->uMaxParallelTools=1;
    }
    uLimit = pAgent->uMaxParallelTools ? pAgent->uMaxParallelTools : 1u;
    if (uLimit > 64u)
        uLimit = 64u;
    for (uWave = 0u; bOk && uWave < pPlan->uWaveCount; ++uWave)
    {
        size_t iStart = 0u;
        for (;;)
        {
            xwork_batch_worker arrWorkers[64];
            xthread *arrThreads[64];
            size_t iChunk = 0u;
            size_t j;
            memset(arrWorkers, 0, sizeof(arrWorkers));
            memset(arrThreads, 0, sizeof(arrThreads));
            for (i = iStart; i < iCallCount && iChunk < uLimit; ++i)
            {
                if (pPlan->pEntries[i].uWave != uWave)
                    continue;
                arrWorkers[iChunk].pAgent = pAgent;
                arrWorkers[iChunk].pCall = &pCalls[i];
                arrWorkers[iChunk].pContext = pContext;
                arrWorkers[iChunk].pPlan = &pPlan->pEntries[i];
                arrWorkers[iChunk].pOutput = &pOutputs[i];
                ++iChunk;
            }
            iStart = i;
            if (iChunk == 0u)
                break;
            if (iChunk == 1u)
            {
                (void)xwork__batch_worker_proc(&arrWorkers[0]);
            }
            else
            {
                for (j = 0u; j < iChunk; ++j)
                {
                    arrThreads[j] = xrtThreadCreate(xwork__batch_worker_proc, &arrWorkers[j], 0u);
                    if (!arrThreads[j])
                    {
                        (void)xwork__batch_worker_proc(&arrWorkers[j]);
                    }
                }
                for (j = 0u; j < iChunk; ++j)
                {
                    if (arrThreads[j])
                    {
                        (void)xrtThreadWait(arrThreads[j]);
                        xrtThreadDestroy(arrThreads[j]);
                    }
                }
            }
            for (j = 0u; j < iChunk; ++j)
            {
                if (arrWorkers[j].eResult != XWORK_RESULT_OK)
                {
                    if (bOk && pError)
                        *pError = arrWorkers[j].tError;
                    bOk = false;
                }
            }
            if (iStart >= iCallCount)
                break;
        }
    }
    xworkToolBatchPlanRelease(pPlan);
    return bOk;
}
