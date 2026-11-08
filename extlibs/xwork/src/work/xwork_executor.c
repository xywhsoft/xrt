#include "../internal/xwork_internal.h"

/* Executor adapter: the agent's tool machinery behind the xllm contract.
 *
 * Slice note: the binding currently wraps an xwork_agent because the registry,
 * permission gate, hooks and spill path live on it. The planned toolset /
 * process-table extraction rehomes that machinery into standalone objects;
 * this adapter then keeps its signature while the agent dependency shrinks
 * to the toolset. */

struct xwork_executor_state
{
    xwork_agent *pAgent;
    char *sLastResult;         /* rolling storage: freed on the next execute or unbind */
    unsigned char *pLastImage; /* rolling image payload, same lifetime */
    size_t iLastImageSize;
    char sLastImageMime[32];
    /* Tool-table cache (改造 B): owned xllm_tool copies rebuilt only when
     * the registry generation changes; requests borrow it as a view. */
    xllm_tool *pToolCache;
    xwork_tool_catalog *pToolCacheCatalog; /* retains strings borrowed by tool views */
    size_t iToolCacheCount;
    uint64_t uToolCacheGeneration;
};

static bool xwork__executor_rebuild_tool_cache(xwork_executor_state *pState)
{
    size_t i;
    xllm_tool *pNew;
    xwork_tool_catalog *pCatalog;
    xwork_tool_catalog *pOld;
    if (!xrtMutexLock(pState->pAgent->pStateLock))
        return false;
    pCatalog = xworkToolCatalogRef(pState->pAgent->pToolCatalog);
    (void)xrtMutexUnlock(pState->pAgent->pStateLock);
    if (!pCatalog)
        return false;
    if (pCatalog->iToolCount != pState->iToolCacheCount)
    {
        pNew = (xllm_tool *)realloc(pState->pToolCache, pCatalog->iToolCount * sizeof(*pNew));
        if (!pNew && pCatalog->iToolCount)
        {
            xworkToolCatalogRelease(pCatalog);
            return false;
        }
        pState->pToolCache = pNew;
    }
    for (i = 0u; i < pCatalog->iToolCount; ++i)
    {
        const xwork_tool_entry *pTool = &pCatalog->pTools[i];
        pState->pToolCache[i].sName = pTool->sName;
        pState->pToolCache[i].sDescription = pTool->sDescription;
        pState->pToolCache[i].sParametersJson = pTool->sParametersJson;
        pState->pToolCache[i].bStrict = pTool->bStrict;
    }
    pState->iToolCacheCount = pCatalog->iToolCount;
    pState->uToolCacheGeneration = pCatalog->uGeneration;
    pOld = pState->pToolCacheCatalog;
    pState->pToolCacheCatalog = pCatalog;
    xworkToolCatalogRelease(pOld);
    return true;
}

static bool xwork__executor_list(void *pUserData, xllm_request *pRequest)
{
    xwork_executor_state *pState = (xwork_executor_state *)pUserData;
    if (!pState || !pState->pAgent || !pRequest)
    {
        return false;
    }
    if (pState->uToolCacheGeneration != pState->pAgent->uToolRegistryGeneration)
    {
        if (!xwork__executor_rebuild_tool_cache(pState))
        {
            return false;
        }
    }
    if (pState->iToolCacheCount == 0u)
    {
        return true;
    }
    return xllmRequestSetToolsView(pRequest, pState->pToolCache, pState->iToolCacheCount);
}

static bool xwork__executor_execute(void *pUserData, const xllm_tool_call *pCall,
                                    const xllm_executor_ctx *pCtx, xllm_executor_result *pResult)
{
    xwork_executor_state *pState = (xwork_executor_state *)pUserData;
    xwork_error tError;
    char *sContent = NULL;
    bool bSuccess = false;
    bool bEffectApplied = false;
    xwork_result eResult;
    if (pResult)
    {
        memset(pResult, 0, sizeof(*pResult));
    }
    if (!pState || !pState->pAgent || !pCall || !pResult)
    {
        return false;
    }
    if (pCtx)
    {
        if (pCtx->pCancel && xrtCancelRequested(pCtx->pCancel))
        {
            xworkErrorInit(&tError);
            xwork__set_error(&tError, XWORK_ERROR_CANCELLED,
                             "executor refused a tool call after cancellation");
            return false;
        }
        if (pCtx->uDeadline != 0u && pCtx->uDeadline != XLLM_DEADLINE_NEVER &&
            xllmDeadlineExpired(pCtx->uDeadline))
        {
            xworkErrorInit(&tError);
            xwork__set_error(&tError, XWORK_ERROR_TIMEOUT,
                             "executor refused a tool call after the operation deadline");
            return false;
        }
    }
    xworkErrorInit(&tError);
    {
        unsigned char *pImage = NULL;
        size_t iImageSize = 0u;
        char sMime[32];
        memset(sMime, 0, sizeof(sMime));
        eResult = xwork__execute_tool(
            pState->pAgent, pCall, pCtx ? pCtx->uTurn : 0u, pCtx ? pCtx->pCancel : NULL,
            pCtx ? pCtx->uDeadline : XLLM_DEADLINE_NEVER, &sContent, &bSuccess, &bEffectApplied,
            &pImage, &iImageSize, sMime, &tError);
        if (eResult != XWORK_RESULT_OK)
        {
            free(sContent);
            free(pImage);
            return false; /* infrastructure failure: run aborts */
        }
        free(pState->sLastResult);
        pState->sLastResult = sContent;
        free(pState->pLastImage);
        pState->pLastImage = pImage;
        pState->iLastImageSize = iImageSize;
        memcpy(pState->sLastImageMime, sMime, sizeof(pState->sLastImageMime));
    }
    pResult->sContent = sContent;
    pResult->bSuccess = bSuccess;
    pResult->pImageBytes = pState->pLastImage;
    pResult->iImageSize = pState->iLastImageSize;
    pResult->sImageMime = pState->sLastImageMime[0] ? pState->sLastImageMime : NULL;
    if (bEffectApplied)
        pResult->uFlags |= XLLM_EXECUTOR_RESULT_EFFECT_APPLIED;
    return true;
}

static bool xwork__executor_execute_batch(void *pUserData, const xllm_tool_call *pCalls,
                                          size_t iCallCount, const xllm_executor_ctx *pCtx,
                                          xllm_executor_result *pResults)
{
    xwork_executor_state *pState = (xwork_executor_state *)pUserData;
    xwork_tool_batch_output *pOutputs;
    xwork_error tError;
    size_t i;
    bool bOk;
    if (!pState || !pState->pAgent || (!pCalls && iCallCount) || (!pResults && iCallCount))
        return false;
    pOutputs = (xwork_tool_batch_output *)calloc(iCallCount, sizeof(*pOutputs));
    if (!pOutputs && iCallCount)
        return false;
    xworkErrorInit(&tError);
    bOk = xwork__execute_tool_batch(pState->pAgent, pCalls, iCallCount, pCtx, pOutputs, &tError);
    for (i = 0u; i < iCallCount; ++i)
    {
        pResults[i].sContent = pOutputs[i].sContent;
        pResults[i].bSuccess = pOutputs[i].bSuccess;
        pResults[i].pImageBytes = pOutputs[i].pImageBytes;
        pResults[i].iImageSize = pOutputs[i].iImageSize;
        pResults[i].sImageMime = pOutputs[i].sImageMime[0] ? pOutputs[i].sImageMime : NULL;
        if (pOutputs[i].bEffectApplied)
        {
            pResults[i].uFlags |= XLLM_EXECUTOR_RESULT_EFFECT_APPLIED;
        }
        pOutputs[i].sContent = NULL;
        pOutputs[i].pImageBytes = NULL;
    }
    xwork__tool_batch_outputs_unit(pOutputs, iCallCount);
    free(pOutputs);
    return bOk;
}

static void xwork__executor_batch_results_unit(void *pUserData, xllm_executor_result *pResults,
                                               size_t iResultCount)
{
    size_t i;
    (void)pUserData;
    for (i = 0u; pResults && i < iResultCount; ++i)
    {
        free(pResults[i].sContent);
        free((void *)pResults[i].pImageBytes);
        memset(&pResults[i], 0, sizeof(pResults[i]));
    }
}

bool xworkExecutorBind(xllm_executor *pOut, xwork_agent *pAgent, xwork_error *pError)
{
    xwork_executor_state *pState;
    if (pError)
    {
        xworkErrorInit(pError);
    }
    if (!pOut || !pAgent)
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
                         "executor binding requires an output executor and an agent");
        return false;
    }
    pState = (xwork_executor_state *)calloc(1u, sizeof(*pState));
    if (!pState)
    {
        xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY,
                         "failed to allocate the executor binding");
        return false;
    }
    if (!xrtMutexLock(pAgent->pStateLock))
    {
        free(pState);
        xwork__set_error(pError, XWORK_ERROR_CONTEXT, "cannot seal file mounts");
        return false;
    }
    pAgent->bFileMountsSealed = true;
    (void)xrtMutexUnlock(pAgent->pStateLock);
    pState->pAgent = pAgent;
    xllmExecutorInit(pOut);
    pOut->pListTools = xwork__executor_list;
    pOut->pExecute = xwork__executor_execute;
    pOut->pUserData = pState;
    pOut->pExecuteBatch = xwork__executor_execute_batch;
    pOut->pBatchResultsUnit = xwork__executor_batch_results_unit;
    return true;
}

void xworkExecutorUnbind(xllm_executor *pExecutor)
{
    xwork_executor_state *pState = NULL;
    if (!pExecutor)
    {
        return;
    }
    if (pExecutor->pListTools == xwork__executor_list)
    {
        pState = (xwork_executor_state *)pExecutor->pUserData;
    }
    if (pState)
    {
        free(pState->sLastResult);
        free(pState->pLastImage);
        free(pState->pToolCache);
        xworkToolCatalogRelease(pState->pToolCacheCatalog);
        free(pState);
    }
    memset(pExecutor, 0, sizeof(*pExecutor));
}
