#include "xllm_session_internal.h"

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

void xllmRunPolicyInit(xllm_run_policy* pPolicy)
{
    if ( !pPolicy ) { return; }
    memset(pPolicy, 0, sizeof(*pPolicy));
}

void xllmRunSummaryUnit(xllm_run_summary* pSummary)
{
    if ( !pSummary ) { return; }
    free(pSummary->sFinalText);
    memset(pSummary, 0, sizeof(*pSummary));
}

static xllm_result xllm_session__run_drain_pending(xllm_session* pSession,
    const xllm_executor* pExecutor, const xllm_run_policy* pPolicy,
    xllm_run_summary* pSummary, xllm_error* pError)
{
    while ( xllmSessionPendingToolCallCount(pSession) != 0u ) {
        xllm_pending_tool_call tCall;
        xllm_tool_call tCallView;
        xllm_executor_result tOut;
        xllm_executor_ctx tCtx;
        if ( !xllmSessionPendingToolCallAt(pSession, 0u, &tCall) ) {
            xllm_session__error(pError, XLLM_ERROR_UPSTREAM, "failed to read a pending tool call");
            return XLLM_RESULT_ERROR;
        }
        memset(&tCallView, 0, sizeof(tCallView));
        tCallView.sId = (char*)tCall.sId;                       /* borrowed */
        tCallView.sName = (char*)tCall.sName;                   /* borrowed */
        tCallView.sArgumentsJson = (char*)tCall.sArgumentsJson; /* borrowed */
        memset(&tOut, 0, sizeof(tOut));
        memset(&tCtx, 0, sizeof(tCtx));
        tCtx.uRound = pSummary->uRounds + 1u;
        tCtx.uTurn = tCall.uTurn;
        tCtx.pCancel = pPolicy ? pPolicy->pCancel : NULL;
        tCtx.uDeadline = (pPolicy && pPolicy->uDeadline) ? pPolicy->uDeadline : 0u;
        if ( !pExecutor->pExecute(pExecutor->pUserData, &tCallView, &tCtx, &tOut) ||
             !tOut.sContent ) {
            xllm_session__error(pError, XLLM_ERROR_UPSTREAM,
                "executor failed while recovering a pending tool call");
            return XLLM_RESULT_ERROR;
        }
        if ( !xllmSessionAddToolResult(pSession, tCall.uTurn, tCall.sId, tOut.sContent) ) {
            xllm_session__error(pError, XLLM_ERROR_UPSTREAM,
                "failed to record a recovered tool result");
            return XLLM_RESULT_ERROR;
        }
        ++pSummary->uToolCalls;
    }
    return XLLM_RESULT_OK;
}

xllm_result xllmSessionRunWithTools(xllm_session* pSession, const char* sPrompt,
    const xllm_executor* pExecutor, const xllm_stream_callbacks* pCallbacks,
    const xllm_run_policy* pPolicy, xllm_run_summary* pSummary, xllm_error* pError)
{
    const uint32_t uMaxRounds = (pPolicy && pPolicy->uMaxRounds) ? pPolicy->uMaxRounds
        : XLLM_SESSION_RUN_DEFAULT_ROUNDS;
    xllm_run_summary tLocal;
    xllm_session_tail tTail;
    xllm_result eResult = XLLM_RESULT_ERROR;
    uint64_t uTurn;
    if ( pError ) { xllmErrorInit(pError); }
    memset(&tLocal, 0, sizeof(tLocal));
    if ( !pSession || !pExecutor || !pExecutor->pListTools || !pExecutor->pExecute ) {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT,
            "session and a complete executor are required");
        goto done;
    }
    if ( !pSession->pClient && !pSession->pTestCall ) {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT,
            "RunWithTools requires a bound client or a test call");
        goto done;
    }
    if ( pSession->bInHook ) {
        xllm_session__error(pError, XLLM_ERROR_HOOK, "session hook re-entered a mutating API");
        goto done;
    }

    if ( sPrompt ) {
        uTurn = xllmSessionBeginTurn(pSession);
        if ( uTurn == 0u || !xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, sPrompt, 0u) ) {
            xllm_session__error(pError, XLLM_ERROR_UPSTREAM, "failed to open the user turn");
            goto done;
        }
    } else {
        if ( !xllmSessionGetTail(pSession, &tTail) || !tTail.bHasMessage ) {
            xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT,
                "resuming a run requires an existing session tail");
            goto done;
        }
        uTurn = tTail.uTurn;
    }

    /* Interrupted-run recovery: finish unresolved tool calls first. */
    eResult = xllm_session__run_drain_pending(pSession, pExecutor, pPolicy, &tLocal, pError);
    if ( eResult != XLLM_RESULT_OK ) { goto done; }

    while ( tLocal.uRounds < uMaxRounds ) {
        xllm_request tRequest;
        xllm_response* pResponse = NULL;
        /* Early-out checks: the test seam does not inspect the request token,
         * so the loop itself enforces the policy tree. */
        if ( pPolicy && pPolicy->pCancel && xrtCancelRequested(pPolicy->pCancel) ) {
            xllm_session__error(pError, XLLM_ERROR_CANCELLED, "run cancelled before a model round");
            eResult = XLLM_RESULT_CANCELLED;
            goto done;
        }
        if ( pPolicy && pPolicy->uDeadline && pPolicy->uDeadline != UINT64_MAX &&
             xrtDeadlineExpired(pPolicy->uDeadline) ) {
            xllm_session__error(pError, XLLM_ERROR_TIMEOUT, "run deadline expired before a model round");
            eResult = XLLM_RESULT_TIMEOUT;
            goto done;
        }
        xllmRequestInit(&tRequest);
        if ( !xllmSessionBuildRequest(pSession, &tRequest, pError) ) {
            xllmRequestUnit(&tRequest);
            goto done;
        }
        if ( !pExecutor->pListTools(pExecutor->pUserData, &tRequest) ) {
            xllmRequestUnit(&tRequest);
            xllm_session__error(pError, XLLM_ERROR_UPSTREAM,
                "executor failed to list tools for the model request");
            goto done;
        }
        if ( pPolicy && pPolicy->pCancel ) { xllmRequestSetCancel(&tRequest, pPolicy->pCancel); }
        if ( pPolicy && pPolicy->uDeadline ) { xllmRequestSetDeadline(&tRequest, pPolicy->uDeadline); }
        eResult = xllm_session__dispatch_call(pSession, &tRequest, pCallbacks, &pResponse, pError);
        xllmRequestUnit(&tRequest);
        if ( eResult != XLLM_RESULT_OK ) { goto done; }
        if ( !pResponse ) {
            xllm_session__error(pError, XLLM_ERROR_UPSTREAM, "the model call produced no response");
            goto done;
        }
        if ( !xllmSessionAddAssistantResponse(pSession, uTurn, pResponse) ) {
            xllmResponseDestroy(pResponse);
            xllm_session__error(pError, XLLM_ERROR_UPSTREAM, "failed to record the assistant response");
            goto done;
        }
        tLocal.tLastUsage = pResponse->tUsage;
        ++tLocal.uRounds;

        if ( pResponse->iToolCallCount == 0u ) {
            tLocal.sFinalText = xllm_session__strdup(
                pResponse->sContent ? pResponse->sContent : "");
            xllmResponseDestroy(pResponse);
            if ( !tLocal.sFinalText ) {
                xllm_session__error(pError, XLLM_ERROR_OUT_OF_MEMORY, "failed to store the final text");
                goto done;
            }
            eResult = XLLM_RESULT_OK;
            goto compact;
        }

        if ( pPolicy && pPolicy->pOnRound &&
             !pPolicy->pOnRound(pSession, tLocal.uRounds, pResponse,
                 pResponse->iToolCallCount, pPolicy->pUserData) ) {
            tLocal.bStoppedByPolicy = true;
            tLocal.sFinalText = xllm_session__strdup(
                pResponse->sContent ? pResponse->sContent : "");
            xllmResponseDestroy(pResponse);
            eResult = XLLM_RESULT_OK;
            goto compact;
        }

        {
            size_t i;
            for ( i = 0u; i < pResponse->iToolCallCount; ++i ) {
                const xllm_tool_call* pCall = &pResponse->pToolCalls[i];
                xllm_executor_result tOut;
                xllm_executor_ctx tCtx;
                memset(&tOut, 0, sizeof(tOut));
                memset(&tCtx, 0, sizeof(tCtx));
                tCtx.uRound = tLocal.uRounds;
                tCtx.uTurn = uTurn;
                tCtx.pCancel = pPolicy ? pPolicy->pCancel : NULL;
                tCtx.uDeadline = (pPolicy && pPolicy->uDeadline) ? pPolicy->uDeadline : 0u;
                if ( !pExecutor->pExecute(pExecutor->pUserData, pCall, &tCtx, &tOut) ||
                     !tOut.sContent ) {
                    xllmResponseDestroy(pResponse);
                    xllm_session__error(pError, XLLM_ERROR_UPSTREAM,
                        "executor infrastructure failure while running a tool call");
                    eResult = XLLM_RESULT_ERROR;
                    goto done;
                }
                if ( !xllmSessionAddToolResult(pSession, uTurn, pCall->sId, tOut.sContent) ) {
                    xllmResponseDestroy(pResponse);
                    xllm_session__error(pError, XLLM_ERROR_UPSTREAM,
                        "failed to record a tool result");
                    eResult = XLLM_RESULT_ERROR;
                    goto done;
                }
                ++tLocal.uToolCalls;
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
    if ( pSummary ) { *pSummary = tLocal; }
    else { xllmRunSummaryUnit(&tLocal); }
    return eResult;
}
