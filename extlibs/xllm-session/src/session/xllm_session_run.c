#include "../internal/xllm_session_internal.h"

/* ------------------------------------------------------------------ */
/* Bounded tool round-trips: the loop as a library function.           */
/*                                                                     */
/* Design invariants:                                                  */
/*  - every step is ledgered before the next one starts (crash = the   */
/*    journal tail shows exactly how far the run got);                 */
/*  - pending tool calls drain before any new model round, so an       */
/*    interrupted run resumes without appending another user prompt;   */
/*  - the executor contract keeps this layer free of tool semantics:   */
/*    infra failure aborts the run, tool failure is content.           */
/* ------------------------------------------------------------------ */

#define XLLM_SESSION_RUN_DEFAULT_ROUNDS 32u

static bool xllm_session__executor_has_batch(const xllm_executor *pExecutor)
{
    return pExecutor && pExecutor->uAbiVersion == XLLM_EXECUTOR_ABI_VERSION &&
           pExecutor->uStructSize >=
               offsetof(xllm_executor, pBatchResultsUnit) + sizeof(pExecutor->pBatchResultsUnit) &&
           pExecutor->pExecuteBatch && pExecutor->pBatchResultsUnit;
}

static bool xllm_session__record_executor_result(xllm_session *pSession, uint64_t uTurn,
                                                 const xllm_tool_call *pCall,
                                                 const xllm_executor_result *pOut)
{
    if (pOut->pImageBytes && pOut->iImageSize && pOut->sImageMime)
    {
        return xllmSessionAddToolResultWithImage(pSession, uTurn, pCall->sId, pOut->sContent,
                                                 pOut->pImageBytes, pOut->iImageSize,
                                                 pOut->sImageMime);
    }
    return xllmSessionAddToolResult(pSession, uTurn, pCall->sId, pOut->sContent);
}

static xllm_result xllm_session__execute_tool_batch(xllm_session *pSession,
                                                    const xllm_executor *pExecutor,
                                                    const xllm_tool_call *pCalls, size_t iCallCount,
                                                    const xllm_executor_ctx *pCtx,
                                                    xllm_run_summary *pSummary, xllm_error *pError)
{
    size_t i;
    if (iCallCount == 0u)
        return XLLM_RESULT_OK;
    if (xllm_session__executor_has_batch(pExecutor) && iCallCount > 1u)
    {
        xllm_executor_result *pResults =
            (xllm_executor_result *)calloc(iCallCount, sizeof(*pResults));
        bool bExecuted;
        if (!pResults)
        {
            xllm_session__error(pError, XLLM_ERROR_OUT_OF_MEMORY,
                                "failed to allocate ordered tool batch results");
            return XLLM_RESULT_ERROR;
        }
        bExecuted =
            pExecutor->pExecuteBatch(pExecutor->pUserData, pCalls, iCallCount, pCtx, pResults);
        if (!bExecuted)
        {
            pExecutor->pBatchResultsUnit(pExecutor->pUserData, pResults, iCallCount);
            free(pResults);
            xllm_session__error(pError, XLLM_ERROR_UPSTREAM,
                                "executor infrastructure failure while running a tool batch");
            return XLLM_RESULT_ERROR;
        }
        for (i = 0u; i < iCallCount; ++i)
        {
            if (!pResults[i].sContent || !xllm_session__record_executor_result(
                                             pSession, pCtx->uTurn, &pCalls[i], &pResults[i]))
            {
                pExecutor->pBatchResultsUnit(pExecutor->pUserData, pResults, iCallCount);
                free(pResults);
                xllm_session__error(pError, XLLM_ERROR_UPSTREAM,
                                    "failed to record an ordered tool batch result");
                return XLLM_RESULT_ERROR;
            }
            ++pSummary->uToolCalls;
        }
        pExecutor->pBatchResultsUnit(pExecutor->pUserData, pResults, iCallCount);
        free(pResults);
        return XLLM_RESULT_OK;
    }
    for (i = 0u; i < iCallCount; ++i)
    {
        xllm_executor_result tOut;
        memset(&tOut, 0, sizeof(tOut));
        if (!pExecutor->pExecute(pExecutor->pUserData, &pCalls[i], pCtx, &tOut) || !tOut.sContent)
        {
            xllm_session__error(pError, XLLM_ERROR_UPSTREAM,
                                "executor infrastructure failure while running a tool call");
            return XLLM_RESULT_ERROR;
        }
        if (!xllm_session__record_executor_result(pSession, pCtx->uTurn, &pCalls[i], &tOut))
        {
            xllm_session__error(pError, XLLM_ERROR_UPSTREAM, "failed to record a tool result");
            return XLLM_RESULT_ERROR;
        }
        ++pSummary->uToolCalls;
    }
    return XLLM_RESULT_OK;
}

void xllmRunPolicyInit(xllm_run_policy *pPolicy)
{
    if (!pPolicy)
    {
        return;
    }
    memset(pPolicy, 0, sizeof(*pPolicy));
}

void xllmRunSummaryUnit(xllm_run_summary *pSummary)
{
    if (!pSummary)
    {
        return;
    }
    free(pSummary->sFinalText);
    memset(pSummary, 0, sizeof(*pSummary));
}

static xllm_result xllm_session__run_drain_pending(xllm_session *pSession,
                                                   const xllm_executor *pExecutor,
                                                   const xllm_run_policy *pPolicy,
                                                   xllm_run_summary *pSummary, xllm_error *pError)
{
    while (xllmSessionPendingToolCallCount(pSession) != 0u)
    {
        size_t iPendingBefore = xllmSessionPendingToolCallCount(pSession);
        xllm_pending_tool_call tFirst;
        xllm_tool_call *pCalls = NULL;
        size_t iCallCount = 0u;
        size_t i;
        xllm_executor_ctx tCtx;
        xllm_result eResult;
        if (!xllmSessionPendingToolCallAt(pSession, 0u, &tFirst))
        {
            xllm_session__error(pError, XLLM_ERROR_UPSTREAM, "failed to read a pending tool call");
            return XLLM_RESULT_ERROR;
        }
        for (i = 0u; i < iPendingBefore; ++i)
        {
            xllm_pending_tool_call tCall;
            if (!xllmSessionPendingToolCallAt(pSession, i, &tCall) || tCall.uTurn != tFirst.uTurn)
                break;
            ++iCallCount;
        }
        pCalls = (xllm_tool_call *)calloc(iCallCount, sizeof(*pCalls));
        if (!pCalls)
        {
            xllm_session__error(pError, XLLM_ERROR_OUT_OF_MEMORY,
                                "failed to allocate recovered tool batch");
            return XLLM_RESULT_ERROR;
        }
        for (i = 0u; i < iCallCount; ++i)
        {
            xllm_pending_tool_call tCall;
            if (!xllmSessionPendingToolCallAt(pSession, i, &tCall))
            {
                free(pCalls);
                xllm_session__error(pError, XLLM_ERROR_UPSTREAM,
                                    "failed to read a recovered tool batch");
                return XLLM_RESULT_ERROR;
            }
            pCalls[i].sId = (char *)tCall.sId;
            pCalls[i].sName = (char *)tCall.sName;
            pCalls[i].sArgumentsJson = (char *)tCall.sArgumentsJson;
        }
        memset(&tCtx, 0, sizeof(tCtx));
        tCtx.uRound = pSummary->uRounds + 1u;
        tCtx.uTurn = tFirst.uTurn;
        tCtx.pCancel = pPolicy ? pPolicy->pCancel : NULL;
        tCtx.uDeadline = (pPolicy && pPolicy->uDeadline) ? pPolicy->uDeadline : 0u;
        eResult = xllm_session__execute_tool_batch(pSession, pExecutor, pCalls, iCallCount, &tCtx,
                                                   pSummary, pError);
        free(pCalls);
        if (eResult != XLLM_RESULT_OK)
            return eResult;
        /* 防御闸：入账成功后 pending 必须减少，否则说明解析/匹配有缺陷——
         * 宁可报错终止，也不能无限重执行一个有副作用的工具。 */
        if (xllmSessionPendingToolCallCount(pSession) >= iPendingBefore)
        {
            xllm_session__error(pError, XLLM_ERROR_UPSTREAM,
                                "pending tool call did not resolve after recording its result");
            return XLLM_RESULT_ERROR;
        }
    }
    return XLLM_RESULT_OK;
}

xllm_result xllmSessionRunWithTools(xllm_session *pSession, const char *sPrompt,
                                    const xllm_executor *pExecutor,
                                    const xllm_stream_callbacks *pCallbacks,
                                    const xllm_run_policy *pPolicy, xllm_run_summary *pSummary,
                                    xllm_error *pError)
{
    const uint32_t uMaxRounds =
        (pPolicy && pPolicy->uMaxRounds) ? pPolicy->uMaxRounds : XLLM_SESSION_RUN_DEFAULT_ROUNDS;
    xllm_run_summary tLocal;
    xllm_session_tail tTail;
    xllm_result eResult = XLLM_RESULT_ERROR;
    uint64_t uTurn;
    if (pError)
    {
        xllmErrorInit(pError);
    }
    memset(&tLocal, 0, sizeof(tLocal));
    if (!pSession || !pExecutor || !pExecutor->pListTools || !pExecutor->pExecute)
    {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT,
                            "session and a complete executor are required");
        goto done;
    }
    if (!pSession->pClient && !pSession->pTestCall)
    {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT,
                            "RunWithTools requires a bound client or a test call");
        goto done;
    }
    if (pSession->bInHook)
    {
        xllm_session__error(pError, XLLM_ERROR_HOOK, "session hook re-entered a mutating API");
        goto done;
    }

    if (sPrompt)
    {
        uTurn = xllmSessionBeginTurn(pSession);
        if (uTurn == 0u || !xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, sPrompt, 0u))
        {
            xllm_session__error(pError, XLLM_ERROR_UPSTREAM, "failed to open the user turn");
            goto done;
        }
    }
    else
    {
        if (!xllmSessionGetTail(pSession, &tTail) || !tTail.bHasMessage)
        {
            xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT,
                                "resuming a run requires an existing session tail");
            goto done;
        }
        uTurn = tTail.uTurn;
    }

    /* Interrupted-run recovery: finish unresolved tool calls first. */
    eResult = xllm_session__run_drain_pending(pSession, pExecutor, pPolicy, &tLocal, pError);
    if (eResult != XLLM_RESULT_OK)
    {
        goto done;
    }

    while (tLocal.uRounds < uMaxRounds)
    {
        xllm_request tRequest;
        xllm_response *pResponse = NULL;
        /* Early-out checks: the test seam does not inspect the request token,
         * so the loop itself enforces the policy tree. */
        if (pPolicy && pPolicy->pCancel && xrtCancelRequested(pPolicy->pCancel))
        {
            xllm_session__error(pError, XLLM_ERROR_CANCELLED, "run cancelled before a model round");
            eResult = XLLM_RESULT_CANCELLED;
            goto done;
        }
        if (pPolicy && pPolicy->uDeadline && pPolicy->uDeadline != UINT64_MAX &&
            xllmDeadlineExpired(pPolicy->uDeadline))
        {
            xllm_session__error(pError, XLLM_ERROR_TIMEOUT,
                                "run deadline expired before a model round");
            eResult = XLLM_RESULT_TIMEOUT;
            goto done;
        }
        xllmRequestInit(&tRequest);
        if (!xllmSessionBuildRequestView(pSession, &tRequest, pError))
        {
            xllmRequestUnit(&tRequest);
            goto done;
        }
        if (!pExecutor->pListTools(pExecutor->pUserData, &tRequest))
        {
            xllmRequestUnit(&tRequest);
            xllm_session__error(pError, XLLM_ERROR_UPSTREAM,
                                "executor failed to list tools for the model request");
            goto done;
        }
        if (pPolicy && pPolicy->pCancel)
        {
            xllmRequestSetCancel(&tRequest, pPolicy->pCancel);
        }
        if (pPolicy && pPolicy->uDeadline)
        {
            xllmRequestSetDeadline(&tRequest, pPolicy->uDeadline);
        }
        if (pPolicy && pPolicy->sModel && !xllmRequestSetModel(&tRequest, pPolicy->sModel))
        {
            xllmRequestUnit(&tRequest);
            xllm_session__error(pError, XLLM_ERROR_OUT_OF_MEMORY,
                                "failed to apply the per-run model override");
            goto done;
        }
        if (!xllmSessionBeginModelCall(pSession, pError))
        {
            xllmRequestUnit(&tRequest);
            goto done;
        }
        eResult = xllm_session__dispatch_call(pSession, &tRequest, pCallbacks, &pResponse, pError);
        xllmRequestUnit(&tRequest);
        if (eResult != XLLM_RESULT_OK)
        {
            (void)xllmSessionAbortModelCall(pSession, NULL);
            goto done;
        }
        if (!pResponse)
        {
            (void)xllmSessionAbortModelCall(pSession, NULL);
            xllm_session__error(pError, XLLM_ERROR_UPSTREAM, "the model call produced no response");
            goto done;
        }
        if (!xllmSessionAddAssistantResponse(pSession, uTurn, pResponse))
        {
            (void)xllmSessionAbortModelCall(pSession, NULL);
            xllmResponseDestroy(pResponse);
            xllm_session__error(pError, XLLM_ERROR_UPSTREAM,
                                "failed to record the assistant response");
            goto done;
        }
        tLocal.tLastUsage = pResponse->tUsage;
        ++tLocal.uRounds;

        if (pResponse->iToolCallCount == 0u)
        {
            tLocal.sFinalText =
                xllm_session__strdup(pResponse->sContent ? pResponse->sContent : "");
            xllmResponseDestroy(pResponse);
            if (!tLocal.sFinalText)
            {
                xllm_session__error(pError, XLLM_ERROR_OUT_OF_MEMORY,
                                    "failed to store the final text");
                goto done;
            }
            eResult = XLLM_RESULT_OK;
            goto compact;
        }

        if (pPolicy && pPolicy->pOnRound &&
            !pPolicy->pOnRound(pSession, tLocal.uRounds, pResponse, pResponse->iToolCallCount,
                               pPolicy->pUserData))
        {
            tLocal.bStoppedByPolicy = true;
            tLocal.sFinalText =
                xllm_session__strdup(pResponse->sContent ? pResponse->sContent : "");
            xllmResponseDestroy(pResponse);
            eResult = XLLM_RESULT_OK;
            goto compact;
        }

        {
            xllm_executor_ctx tCtx;
            memset(&tCtx, 0, sizeof(tCtx));
            tCtx.uRound = tLocal.uRounds;
            tCtx.uTurn = uTurn;
            tCtx.pCancel = pPolicy ? pPolicy->pCancel : NULL;
            tCtx.uDeadline = (pPolicy && pPolicy->uDeadline) ? pPolicy->uDeadline : 0u;
            eResult =
                xllm_session__execute_tool_batch(pSession, pExecutor, pResponse->pToolCalls,
                                                 pResponse->iToolCallCount, &tCtx, &tLocal, pError);
            if (eResult != XLLM_RESULT_OK)
            {
                xllmResponseDestroy(pResponse);
                goto done;
            }
        }
        xllmResponseDestroy(pResponse);
    }

    xllm_session__error(pError, XLLM_ERROR_LIMIT,
                        "run exceeded the maximum model rounds before a final answer");
    eResult = XLLM_RESULT_ERROR;

compact:
    /* Threshold compaction consult; a failure here does not unwind the run —
     * the turn is already durable, and xllmSessionSend keeps the same contract. */
    {
        bool bCompact = false;
        (void)xllmSessionMaybeCompact(pSession, &bCompact, NULL);
    }
done:
    if (pSummary)
    {
        *pSummary = tLocal;
    }
    else
    {
        xllmRunSummaryUnit(&tLocal);
    }
    return eResult;
}
