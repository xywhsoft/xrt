#include "xllm_session_internal.h"

/* ------------------------------------------------------------------ */
/* Bound sessions and the default meta call                            */
/* ------------------------------------------------------------------ */

static xllm_result xllm_session__dispatch_call(xllm_session* pSession, const xllm_request* pRequest,
    const xllm_stream_callbacks* pCallbacks, xllm_response** ppResponse, xllm_error* pError)
{
    xllm_result eResult;
    if ( pSession->pTestCall ) {
        if ( !xllm_session__hook_enter(pSession, pError, "test_call") ) { return XLLM_RESULT_ERROR; }
        eResult = pSession->pTestCall(pSession->pTestCallData, pRequest, pCallbacks, ppResponse, pError);
        xllm_session__hook_leave(pSession);
        return eResult;
    }
    return xllmClientComplete(pSession->pClient, pRequest, pCallbacks, ppResponse, pError);
}

/* Default pSummarize stage: one non-streaming call on the bound client (or
 * the test seam) with the summary output budget. */
bool xllm_session__client_summarize(xllm_session* pSession, const char* sPrompt,
    char** psSummary, xllm_usage* pUsage, xllm_error* pError)
{
    xllm_request tRequest;
    xllm_response* pResponse = NULL;
    xllm_header tHeader;
    xllm_result eResult;
    bool bOk = false;
    if ( pError ) { xllmErrorInit(pError); }
    if ( !pSession || !sPrompt || !psSummary ) { return false; }
    if ( !pSession->pClient && !pSession->pTestCall ) {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT,
            "summarization requires a bound client or a test call");
        return false;
    }
    xllmRequestInit(&tRequest);
    tRequest.bStream = false;
    tRequest.uMaxOutputTokens = pSession->tConfig.uSummaryMaxTokens;
    /* One-off routing namespace (pi's fresh routing session id): each meta
     * call carries a fresh UUID so routing-style backends keep it out of
     * the conversation's affinity slot. store:false rides the shared wire
     * path since GAP-CACHE-HINT v2; no per-call body work here. */
    {
        unsigned char aSeed[16];
        if ( xrtSecureRandom(aSeed, sizeof(aSeed)) ) {
            char sKey[40];
            aSeed[6] = (unsigned char)((aSeed[6] & 0x0fu) | 0x40u); /* v4 */
            aSeed[8] = (unsigned char)((aSeed[8] & 0x3fu) | 0x80u); /* variant */
            (void)snprintf(sKey, sizeof(sKey),
                "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                aSeed[0], aSeed[1], aSeed[2], aSeed[3], aSeed[4], aSeed[5],
                aSeed[6], aSeed[7], aSeed[8], aSeed[9], aSeed[10], aSeed[11],
                aSeed[12], aSeed[13], aSeed[14], aSeed[15]);
            tHeader.sName = "xllm-routing-key";
            tHeader.sValue = sKey;
            tRequest.pExtraHeaders = &tHeader;
            tRequest.iExtraHeaderCount = 1u;
        }
    }
    if ( !xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, sPrompt) ) {
        xllm_session__error(pError, XLLM_ERROR_OUT_OF_MEMORY, "failed to build the summary request");
        goto done;
    }
    eResult = xllm_session__dispatch_call(pSession, &tRequest, NULL, &pResponse, pError);
    if ( eResult != XLLM_RESULT_OK || !pResponse ) { goto done; }
    if ( !pResponse->sContent || !pResponse->sContent[0] ) {
        xllm_session__error(pError, XLLM_ERROR_UPSTREAM, "the summary call returned no content");
        goto done;
    }
    *psSummary = xllm_session__strdup(pResponse->sContent);
    if ( !*psSummary ) {
        xllm_session__error(pError, XLLM_ERROR_OUT_OF_MEMORY, "failed to store the summary");
        goto done;
    }
    if ( pUsage ) { *pUsage = pResponse->tUsage; }
    bOk = true;
done:
    xllmResponseDestroy(pResponse);
    xllmRequestUnit(&tRequest);
    return bOk;
}

xllm_session* xllmSessionCreateBound(const xllm_session_config* pConfig,
    xllm_client* pClient, xllm_error* pError)
{
    xllm_session_config tConfig;
    xllm_model_profile tProfile;
    xllm_session* pSession;
    if ( pError ) { xllmErrorInit(pError); }
    if ( !pClient ) {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT, "a bound session requires a client");
        return NULL;
    }
    if ( pConfig ) { tConfig = *pConfig; } else { xllmSessionConfigInit(&tConfig); }
    /* D8 window provenance: an explicit zero opts into the bound client's
     * profile (window, output ceiling, recommended reserves); the profile
     * wins over ConfigInit's generic defaults. */
    if ( tConfig.uContextWindowTokens == 0u && xllmClientGetModelProfile(pClient, &tProfile) ) {
        tConfig.uContextWindowTokens = tProfile.uContextWindowTokens;
        if ( tConfig.uMaxOutputTokens == 0u && tProfile.uMaxOutputTokens ) {
            tConfig.uMaxOutputTokens = tProfile.uMaxOutputTokens;
        }
        if ( tConfig.uOutputReserveTokens == 0u && tProfile.uRecommendedOutputReserveTokens ) {
            tConfig.uOutputReserveTokens = tProfile.uRecommendedOutputReserveTokens;
        }
        if ( tConfig.uSummaryMaxTokens == 32768u && tProfile.uRecommendedSummaryTokens ) {
            tConfig.uSummaryMaxTokens = tProfile.uRecommendedSummaryTokens;
        }
    }
    pSession = xllmSessionCreate(&tConfig, pError);
    if ( !pSession ) { return NULL; }
    pSession->pClient = pClient;
    return pSession;
}

xllm_session* xllmSessionCreateForTest(const xllm_session_config* pConfig,
    xllm_test_call_proc pCall, void* pUserData, xllm_error* pError)
{
    xllm_session* pSession;
    if ( pError ) { xllmErrorInit(pError); }
    if ( !pCall ) {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT, "a test session requires a call proc");
        return NULL;
    }
    pSession = xllmSessionCreate(pConfig, pError);
    if ( !pSession ) { return NULL; }
    pSession->pTestCall = pCall;
    pSession->pTestCallData = pUserData;
    return pSession;
}

/* ------------------------------------------------------------------ */
/* Convenience turns                                                    */
/* ------------------------------------------------------------------ */

xllm_result xllmSessionComplete(xllm_session* pSession, const xllm_stream_callbacks* pCallbacks,
    xllm_response** ppResponse, xllm_error* pError)
{
    xllm_request tRequest;
    xllm_result eResult;
    if ( pError ) { xllmErrorInit(pError); }
    if ( !pSession || !ppResponse ) {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT, "session and response slot are required");
        return XLLM_RESULT_ERROR;
    }
    *ppResponse = NULL;
    if ( !pSession->pClient && !pSession->pTestCall ) {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT,
            "no bound client; use the core-layer APIs and drive the call yourself");
        return XLLM_RESULT_ERROR;
    }
    xllmRequestInit(&tRequest);
    if ( !xllmSessionBuildRequest(pSession, &tRequest, pError) ) {
        xllmRequestUnit(&tRequest);
        return XLLM_RESULT_ERROR;
    }
    eResult = xllm_session__dispatch_call(pSession, &tRequest, pCallbacks, ppResponse, pError);
    xllmRequestUnit(&tRequest);
    return eResult;
}

xllm_result xllmSessionSend(xllm_session* pSession, const char* sUserText,
    const xllm_stream_callbacks* pCallbacks, xllm_response** ppResponse, xllm_error* pError)
{
    uint64_t uTurn;
    xllm_result eResult;
    if ( pError ) { xllmErrorInit(pError); }
    if ( !pSession || !sUserText || !ppResponse ) {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT,
            "session, user text, and response slot are required");
        return XLLM_RESULT_ERROR;
    }
    *ppResponse = NULL;
    if ( pSession->bInHook ) {
        xllm_session__error(pError, XLLM_ERROR_HOOK, "session hook re-entered a mutating API");
        return XLLM_RESULT_ERROR;
    }
    uTurn = xllmSessionBeginTurn(pSession);
    if ( uTurn == 0u ) {
        xllm_session__error(pError, XLLM_ERROR_UPSTREAM, "failed to begin the session turn");
        return XLLM_RESULT_ERROR;
    }
    if ( !xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, sUserText, 0u) ) {
        xllm_session__error(pError, XLLM_ERROR_UPSTREAM, "failed to record the user message");
        return XLLM_RESULT_ERROR;
    }
    eResult = xllmSessionComplete(pSession, pCallbacks, ppResponse, pError);
    if ( eResult != XLLM_RESULT_OK ) { return eResult; }
    /* AddAssistantResponse records usage, which refreshes governance and may
     * raise the compaction pressure. */
    if ( !xllmSessionAddAssistantResponse(pSession, uTurn, *ppResponse) ) {
        xllmResponseDestroy(*ppResponse);
        *ppResponse = NULL;
        xllm_session__error(pError, XLLM_ERROR_UPSTREAM, "failed to record the assistant response");
        return XLLM_RESULT_ERROR;
    }
    {
        bool bCompact = false;
        if ( !xllmSessionMaybeCompact(pSession, &bCompact, pError) ) {
            /* The turn is durable; surface the compaction failure but keep
             * the response: the host can ladder or retry compaction. */
            if ( pError && pError->eCode == XLLM_ERROR_NONE ) {
                pError->eCode = XLLM_ERROR_UPSTREAM;
            }
            return XLLM_RESULT_OK;
        }
    }
    return XLLM_RESULT_OK;
}

bool xllmSessionSetHooks(xllm_session* pSession, const xllm_session_hooks* pHooks)
{
    if ( !pSession ) { return false; }
    pSession->pHooks = pHooks;
    return true;
}
