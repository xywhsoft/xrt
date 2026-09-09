#include "xllm_internal.h"

#define XLLM_MAX_ATTEMPTS 8u

static char* xllm__normalize_url(const char* sBaseUrl)
{
    static const char sSuffix[] = "/chat/completions";
    size_t iLen;
    size_t iSuffixLen = sizeof(sSuffix) - 1u;
    char* sUrl;
    if ( !sBaseUrl || !sBaseUrl[0] ) { return NULL; }
    if ( strstr(sBaseUrl, "/chat/completions") ) { return xllm__strdup(sBaseUrl); }
    iLen = strlen(sBaseUrl);
    while ( iLen && sBaseUrl[iLen - 1u] == '/' ) { --iLen; }
    sUrl = (char*)malloc(iLen + iSuffixLen + 1u);
    if ( !sUrl ) { return NULL; }
    memcpy(sUrl, sBaseUrl, iLen);
    memcpy(sUrl + iLen, sSuffix, iSuffixLen + 1u);
    return sUrl;
}

static uint32_t xllm__parse_retry_after_ms(const char* sValue, uint32_t uMultiplier)
{
    unsigned long long uValue;
    char* sEnd = NULL;
    if ( !sValue || !sValue[0] ) { return 0u; }
    uValue = strtoull(sValue, &sEnd, 10);
    if ( sEnd == sValue ) { return 0u; }
    while ( *sEnd && isspace((unsigned char)*sEnd) ) { ++sEnd; }
    if ( *sEnd || uValue > (unsigned long long)UINT32_MAX / uMultiplier ) { return 0u; }
    return (uint32_t)(uValue * uMultiplier);
}

void xllm__copy_view(char* sDst, size_t iCap, xstrview tValue)
{
    size_t iCopy;
    if ( !sDst || !iCap ) return;
    iCopy = tValue.Size < iCap - 1u ? tValue.Size : iCap - 1u;
    if ( iCopy ) memcpy(sDst, tValue.Data, iCopy);
    sDst[iCopy] = 0;
}

static const xhttpfield* xllm__header(const xhttp1head* pHead, const char* sName)
{
    return pHead ? xrtHttp1Field(pHead, (xstrview){ sName, strlen(sName) }) : NULL;
}

static void xllm__capture_diagnostics(xllm_call* pCall, xllm_diagnostics* pOut)
{
    const xllm_transport_diagnostics* pHttp;
    if ( !pOut ) { return; }
    memset(pOut, 0, sizeof(*pOut));
    if ( !pCall ) { return; }
    pHttp = &pCall->tHttpDiagnostics;
    pOut->uAttemptCount = pCall->uAttempt ? pCall->uAttempt : 1u;
    pOut->uMaxAttempts = pCall->pClient ? pCall->pClient->uMaxAttempts : 1u;
    pOut->uRetryAfterMs = pCall->uRetryAfterMs;
    pOut->bResponseStarted = pCall->uHttpStatus != 0u;
    pOut->bModelDataDelivered = pCall->bSawEvent || pCall->pResponse != NULL || pCall->bResponseTaken;
    pOut->bReusedConnection = pHttp->bReusedConnection;
    pOut->bContextAttached = pCall->bScopeAttached;
    pOut->iTransportStatus = (int32_t)pHttp->eResult;
    pOut->iSystemError = pHttp->iSystemError;
    pOut->uStartedMs = pHttp->uStartedMs;
    pOut->uConnectedMs = pHttp->uConnectedMs;
    pOut->uRequestSentMs = pHttp->uRequestSentMs;
    pOut->uFirstByteMs = pHttp->uFirstByteMs;
    pOut->uHeadersMs = pHttp->uHeadersMs;
    pOut->uCompletedMs = pHttp->uCompletedMs;
    pOut->uConnectDurationMs = pHttp->uConnectedMs > pHttp->uStartedMs ? pHttp->uConnectedMs - pHttp->uStartedMs : 0u;
    pOut->uTimeToFirstByteMs = pHttp->uFirstByteMs > pHttp->uStartedMs ? pHttp->uFirstByteMs - pHttp->uStartedMs : 0u;
    pOut->uTransferDurationMs = pHttp->uCompletedMs > pHttp->uHeadersMs ? pHttp->uCompletedMs - pHttp->uHeadersMs : 0u;
    pOut->uTotalDurationMs = pHttp->uCompletedMs > pHttp->uStartedMs ? pHttp->uCompletedMs - pHttp->uStartedMs : 0u;
    pOut->uRequestBytes = pHttp->uRequestBytes;
    pOut->uResponseBodyBytes = pHttp->uResponseBodyBytes;
    pOut->uContextDeadlineMs = pCall->uScopeDeadline == XRT_DEADLINE_NEVER ? 0u : pCall->uScopeDeadline / UINT64_C(1000);
    pOut->uEffectiveTimeoutMs = pHttp->uEffectiveTimeoutMs;
    xllm__copy_text(pOut->sTransportError, sizeof(pOut->sTransportError), pHttp->sError);
    if ( pCall->bScopeAttached && pHttp->eResult == XLLM_TRANSPORT_TIMEOUT ) {
        xllm__copy_text(pOut->sTransportError,
            sizeof(pOut->sTransportError), "deadline_exceeded");
    }
    xllm__copy_text(pOut->sTransportPhase, sizeof(pOut->sTransportPhase), pHttp->sPhase);
    xllm__copy_text(pOut->sContextStatus, sizeof(pOut->sContextStatus),
        !pCall->bScopeAttached ? "none" :
        (pHttp->eResult == XLLM_TRANSPORT_CANCELLED ? "cancelled" :
        (pHttp->eResult == XLLM_TRANSPORT_TIMEOUT ? "deadline_exceeded" : "active")));
}

static xllm_result xllm__scope_result(xcancel* pCancel, uint64_t uDeadline, xllm_error* pError)
{
    if ( pError && (pCancel || uDeadline != XRT_DEADLINE_NEVER) ) {
        pError->tDiagnostics.bContextAttached = true;
        pError->tDiagnostics.uContextDeadlineMs = uDeadline == XRT_DEADLINE_NEVER ? 0u : uDeadline / UINT64_C(1000);
        xllm__copy_text(pError->tDiagnostics.sTransportPhase,
            sizeof(pError->tDiagnostics.sTransportPhase), "prepare");
    }
    if ( pCancel && xrtCancelRequested(pCancel) ) {
        xllm__error_set(pError, XLLM_ERROR_CANCELLED, "model operation was cancelled");
        if ( pError ) {
            xllm__copy_text(pError->tDiagnostics.sContextStatus,
                sizeof(pError->tDiagnostics.sContextStatus), "cancelled");
            xllm__copy_text(pError->tDiagnostics.sTransportError,
                sizeof(pError->tDiagnostics.sTransportError), "cancelled");
        }
        return XLLM_RESULT_CANCELLED;
    }
    if ( uDeadline != XRT_DEADLINE_NEVER && xrtDeadlineExpired(uDeadline) ) {
        xllm__error_set(pError, XLLM_ERROR_TIMEOUT, "model operation deadline was exceeded");
        if ( pError ) {
            xllm__copy_text(pError->tDiagnostics.sContextStatus,
                sizeof(pError->tDiagnostics.sContextStatus), "deadline_exceeded");
            xllm__copy_text(pError->tDiagnostics.sTransportError,
                sizeof(pError->tDiagnostics.sTransportError), "deadline_exceeded");
        }
        return XLLM_RESULT_TIMEOUT;
    }
    return XLLM_RESULT_OK;
}

static bool xllm__scope_sleep(xcancel* pCancel, uint64_t uDeadline, uint32_t uDelayMs)
{
    uint64_t uEnd = xrtClock() + (uint64_t)uDelayMs * UINT64_C(1000);
    if ( uDeadline != XRT_DEADLINE_NEVER && uEnd > uDeadline ) uEnd = uDeadline;
    for ( ;; ) {
        uint64_t uNow;
        uint64_t uRemaining;
        uint32_t uSlice;
        if ( pCancel && xrtCancelRequested(pCancel) ) return false;
        uNow = xrtClock();
        if ( uNow >= uEnd ) { return true; }
        uRemaining = uEnd - uNow;
        uSlice = uRemaining > UINT64_C(20000) ? 20u : (uint32_t)((uRemaining + 999u) / 1000u);
        if ( !uSlice ) { uSlice = 1u; }
        xrtSleep(uSlice);
    }
}

static uint32_t xllm__retry_delay_ms(const xllm_client* pClient, uint32_t uAttempt, uint32_t uRetryAfterMs)
{
    uint64_t uDelay;
    uint32_t i;
    if ( !pClient ) { return 0u; }
    uDelay = pClient->uRetryBaseDelayMs;
    for ( i = 1u; i < uAttempt && uDelay < pClient->uRetryMaxDelayMs; ++i ) {
        uDelay *= 2u;
    }
    if ( uDelay < uRetryAfterMs ) { uDelay = uRetryAfterMs; }
    if ( pClient->uRetryMaxDelayMs > 0u && uDelay > pClient->uRetryMaxDelayMs ) {
        uDelay = pClient->uRetryMaxDelayMs;
    }
    return uDelay > UINT32_MAX ? UINT32_MAX : (uint32_t)uDelay;
}

void xllmClientConfigInit(xllm_client_config* pConfig)
{
    if ( !pConfig ) { return; }
    memset(pConfig, 0, sizeof(*pConfig));
    pConfig->sReasoningEffort = "max";
    pConfig->sUserAgent = "xllm/2.0";
    pConfig->uMaxOutputTokens = 65536u;
    pConfig->uTimeoutMs = 30u * 60u * 1000u;
    pConfig->uIdleTimeoutMs = 5u * 60u * 1000u;
    pConfig->uMaxAttempts = 3u;
    pConfig->uRetryBaseDelayMs = 250u;
    pConfig->uRetryMaxDelayMs = 30000u;
    pConfig->bVerifyPeer = true;
    pConfig->eProvider = XLLM_PROVIDER_OPENAI_COMPAT;
}

xllm_client* xllmClientCreate(const xllm_client_config* pConfig, xllm_error* pError)
{
    xllm_client* pClient = NULL;
    const xllm_model_profile* pProfile;
    const char* sModel;
    if ( pError ) { xllmErrorInit(pError); }
    pProfile = pConfig ? pConfig->pModelProfile : NULL;
    sModel = pConfig && pConfig->sModel && pConfig->sModel[0]
        ? pConfig->sModel : (pProfile ? pProfile->sModel : NULL);
    if ( !pConfig || !pConfig->sBaseUrl || !pConfig->sBaseUrl[0] || !sModel || !sModel[0] ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "base URL and model are required");
        return NULL;
    }
    if ( pProfile ) {
        if ( !xllmModelProfileValidate(pProfile, pError) ) return NULL;
        if ( pConfig->sModel && pConfig->sModel[0] &&
             strcmp(pConfig->sModel, pProfile->sModel) != 0 ) {
            xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "client model does not match the model profile");
            return NULL;
        }
        if ( pConfig->uMaxOutputTokens > pProfile->uMaxOutputTokens ) {
            xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "client output limit exceeds the model profile");
            return NULL;
        }
    }
    pClient = (xllm_client*)calloc(1u, sizeof(*pClient));
    if ( !pClient ) goto oom;
    pClient->sBaseUrl = xllm__normalize_url(pConfig->sBaseUrl);
    pClient->sApiKey = xllm__strdup(pConfig->sApiKey ? pConfig->sApiKey : "");
    pClient->sModel = xllm__strdup(sModel);
    pClient->sReasoningEffort = xllm__strdup(pConfig->sReasoningEffort ? pConfig->sReasoningEffort : "");
    pClient->sUserAgent = xllm__strdup(pConfig->sUserAgent ? pConfig->sUserAgent : "xllm/2.0");
    pClient->uMaxOutputTokens = pConfig->uMaxOutputTokens
        ? pConfig->uMaxOutputTokens : (pProfile ? pProfile->uMaxOutputTokens : 65536u);
    pClient->uTimeoutMs = pConfig->uTimeoutMs;
    pClient->uIdleTimeoutMs = pConfig->uIdleTimeoutMs;
    pClient->uMaxAttempts = pConfig->uMaxAttempts ? pConfig->uMaxAttempts : 1u;
    if ( pClient->uMaxAttempts > XLLM_MAX_ATTEMPTS ) { pClient->uMaxAttempts = XLLM_MAX_ATTEMPTS; }
    pClient->uRetryBaseDelayMs = pConfig->uRetryBaseDelayMs;
    pClient->uRetryMaxDelayMs = pConfig->uRetryMaxDelayMs;
    pClient->bVerifyPeer = pConfig->bVerifyPeer;
    pClient->eProvider = pProfile ? pProfile->eProvider : pConfig->eProvider;
    if ( pProfile ) {
        pClient->sProfileId = xllm__strdup(pProfile->sId);
        pClient->tModelProfile = *pProfile;
        pClient->tModelProfile.sId = pClient->sProfileId;
        pClient->tModelProfile.sModel = pClient->sModel;
        pClient->bHasModelProfile = true;
    }
    if ( !pClient->sBaseUrl || !pClient->sApiKey || !pClient->sModel ||
         !pClient->sReasoningEffort || !pClient->sUserAgent || (pProfile && !pClient->sProfileId) ) goto oom;
    if ( !xllm__transport_client_init(pClient, pError) ) {
        xllmClientDestroy(pClient);
        return NULL;
    }
    return pClient;
oom:
    xllm__error_set(pError, XLLM_ERROR_OUT_OF_MEMORY, "failed to allocate model client");
    xllmClientDestroy(pClient);
    return NULL;
}

void xllmClientDestroy(xllm_client* pClient)
{
    size_t i;
    if ( !pClient ) { return; }
    xllm__transport_client_unit(pClient);
    free(pClient->sBaseUrl);
    if ( pClient->sApiKey ) {
        volatile char* pSecret = (volatile char*)pClient->sApiKey;
        i = strlen(pClient->sApiKey);
        while ( i-- ) { pSecret[i] = 0; }
    }
    free(pClient->sApiKey);
    free(pClient->sModel);
    free(pClient->sReasoningEffort);
    free(pClient->sUserAgent);
    free(pClient->sProfileId);
    free(pClient->sHost);
    free(pClient->sTarget);
    free(pClient->sHostHeader);
    free(pClient);
}

bool xllmClientGetModelProfile(const xllm_client* pClient, xllm_model_profile* pProfile)
{
    if ( !pClient || !pProfile || !pClient->bHasModelProfile ) return false;
    *pProfile = pClient->tModelProfile;
    return true;
}

static bool xllm__call_enter(xllm_call* pCall)
{
    if ( !pCall ) { return false; }
    (void)xllm__atomic_add(&pCall->iCallbackActive, 1);
    if ( xllm__atomic_load(&pCall->iClosing) != 0 ) {
        (void)xllm__atomic_add(&pCall->iCallbackActive, -1);
        return false;
    }
    return true;
}

static void xllm__call_leave(xllm_call* pCall)
{
    (void)xllm__atomic_add(&pCall->iCallbackActive, -1);
}

bool xllm__transport_headers(xllm_call* pCall, const xhttp1head* pHead)
{
    const xhttpfield* pHeader;
    char sRetryAfter[64];
    xllm_event tEvent;
    bool bOk = true;
    if ( !xllm__call_enter(pCall) ) { return false; }
    pCall->uHttpStatus = pHead ? pHead->Status : 0u;
    pHeader = xllm__header(pHead, "x-request-id");
    if ( !pHeader ) pHeader = xllm__header(pHead, "request-id");
    if ( pHeader ) xllm__copy_view(pCall->sRequestId, sizeof(pCall->sRequestId), pHeader->Value);
    pHeader = xllm__header(pHead, "content-type");
    if ( pHeader ) xllm__copy_view(pCall->sContentType, sizeof(pCall->sContentType), pHeader->Value);
    pHeader = xllm__header(pHead, "retry-after-ms");
    sRetryAfter[0] = 0;
    if ( pHeader ) xllm__copy_view(sRetryAfter, sizeof(sRetryAfter), pHeader->Value);
    pCall->uRetryAfterMs = xllm__parse_retry_after_ms(sRetryAfter, 1u);
    if ( pCall->uRetryAfterMs == 0u ) {
        pHeader = xllm__header(pHead, "retry-after");
        sRetryAfter[0] = 0;
        if ( pHeader ) xllm__copy_view(sRetryAfter, sizeof(sRetryAfter), pHeader->Value);
        pCall->uRetryAfterMs = xllm__parse_retry_after_ms(sRetryAfter, 1000u);
    }
    pCall->bSse = pCall->uHttpStatus >= 200u && pCall->uHttpStatus < 300u &&
        xllm__contains_ci(pCall->sContentType, "text/event-stream");
    memset(&tEvent, 0, sizeof(tEvent));
    tEvent.eKind = XLLM_EVENT_RESPONSE_START;
    tEvent.as.tResponse.uHttpStatus = pCall->uHttpStatus;
    tEvent.as.tResponse.sRequestId = pCall->sRequestId;
    bOk = xllm__emit(pCall, &tEvent);
    xllm__call_leave(pCall);
    return bOk;
}

bool xllm__transport_body(xllm_call* pCall, const void* pData, size_t iLen)
{
    bool bOk = true;
    if ( !xllm__call_enter(pCall) ) { return false; }
    if ( pCall->bSse ) {
        bOk = xllm__sse_feed(pCall, pData, iLen);
    } else {
        if ( pCall->tRawBody.iLen > XLLM_MAX_FALLBACK_BODY || iLen > XLLM_MAX_FALLBACK_BODY - pCall->tRawBody.iLen ) {
            xllm__error_set(&pCall->tError, XLLM_ERROR_PROTOCOL, "provider response exceeds the fallback body limit");
            bOk = false;
        } else {
            bOk = xllm__buf_append(&pCall->tRawBody, pData, iLen);
            if ( !bOk ) {
                xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY, "failed to buffer provider response");
            } else if ( pCall->uHttpStatus >= 200u && pCall->uHttpStatus < 300u &&
                        pCall->tRawBody.iLen >= 5u && memcmp(pCall->tRawBody.pData, "data:", 5u) == 0 ) {
                pCall->bSse = true;
                bOk = xllm__sse_feed(pCall, pCall->tRawBody.pData, pCall->tRawBody.iLen);
                xllm__buf_reset(&pCall->tRawBody);
            }
        }
    }
    xllm__call_leave(pCall);
    return bOk;
}

static void xllm__map_http_error(xllm_call* pCall)
{
    xllm_error_code eCode = XLLM_ERROR_UPSTREAM;
    const char* sMessage = "provider returned an HTTP error";
    if ( pCall->uHttpStatus == 401u || pCall->uHttpStatus == 403u ) {
        eCode = XLLM_ERROR_AUTH;
        sMessage = "provider authentication failed";
    } else if ( pCall->uHttpStatus == 404u ) {
        eCode = XLLM_ERROR_MODEL_NOT_FOUND;
        sMessage = "provider endpoint or model was not found";
    } else if ( pCall->uHttpStatus == 429u ) {
        eCode = XLLM_ERROR_RATE_LIMIT;
        sMessage = "provider rate limit exceeded";
    }
    xllm__error_set(&pCall->tError, eCode, sMessage);
    pCall->tError.iHttpStatus = (int32_t)pCall->uHttpStatus;
    xllm__copy_text(pCall->tError.sRequestId, sizeof(pCall->tError.sRequestId), pCall->sRequestId);
    xllm__parse_error_body(pCall, pCall->tRawBody.pData, pCall->tRawBody.iLen);
    pCall->tError.eCode = eCode;
    if ( !pCall->tError.sMessage[0] ) { xllm__copy_text(pCall->tError.sMessage, sizeof(pCall->tError.sMessage), sMessage); }
}

static bool xllm__finalize_response(xllm_call* pCall)
{
    xllm_response* pResponse = xllm__ensure_response(pCall);
    xllm_event tEvent;
    size_t i;
    char sGeneratedId[64];
    if ( !pResponse ) { return false; }
    pResponse->uHttpStatus = pCall->uHttpStatus;
    if ( !pResponse->sRequestId && pCall->sRequestId[0] ) {
        pResponse->sRequestId = xllm__strdup(pCall->sRequestId);
        if ( !pResponse->sRequestId ) goto oom;
    }
    if ( !pResponse->sModel && pCall->sSelectedModel ) {
        pResponse->sModel = xllm__strdup(pCall->sSelectedModel);
        if ( !pResponse->sModel ) goto oom;
    }
    if ( !pResponse->sContent ) {
        pResponse->sContent = xllm__strdup("");
        if ( !pResponse->sContent ) goto oom;
    }
    if ( !pResponse->sFinishReason ) {
        pResponse->sFinishReason = xllm__strdup(pResponse->iToolCallCount ? "tool_calls" : "stop");
        if ( !pResponse->sFinishReason ) goto oom;
    }
    for ( i = 0u; i < pResponse->iToolCallCount; ++i ) {
        xllm_tool_call* pTool = &pResponse->pToolCalls[i];
        if ( !pTool->sId || !pTool->sId[0] ) {
            (void)snprintf(sGeneratedId, sizeof(sGeneratedId), "call_%u", (unsigned)i);
            if ( !xllm__replace(&pTool->sId, sGeneratedId) ) goto oom;
        }
        if ( !pTool->sName || !pTool->sName[0] ) {
            xllm__error_set(&pCall->tError, XLLM_ERROR_PROTOCOL, "provider returned a tool call without a function name");
            return false;
        }
        if ( !pTool->sArgumentsJson || !pTool->sArgumentsJson[0] ) {
            if ( !xllm__replace(&pTool->sArgumentsJson, "{}") ) goto oom;
        }
        if ( !xrtJsonValid((xstrview){ pTool->sArgumentsJson, strlen(pTool->sArgumentsJson) }) ) {
            xllm__error_set(&pCall->tError, XLLM_ERROR_PARSE, "provider returned invalid tool-call arguments JSON");
            return false;
        }
    }
    memset(&tEvent, 0, sizeof(tEvent));
    tEvent.eKind = XLLM_EVENT_RESPONSE_DONE;
    tEvent.as.tResponse.uHttpStatus = pCall->uHttpStatus;
    tEvent.as.tResponse.sRequestId = pCall->sRequestId;
    return xllm__emit(pCall, &tEvent);
oom:
    xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY, "failed to finalize model response");
    return false;
}

static int32 xllm__call_thread(ptr pData)
{
    xllm_call* pCall = (xllm_call*)pData;
    pCall->eTransportResult = xllm__transport_execute(pCall);
    return (int32)pCall->eTransportResult;
}

xllm_call* xllmClientStart(
    xllm_client* pClient,
    const xllm_request* pRequest,
    const xllm_stream_callbacks* pCallbacks,
    xllm_error* pError
)
{
    xllm_call* pCall = NULL;
    char* sBody = NULL;
    const char* sModel;
    xdeadline tTimeout = XRT_DEADLINE_NEVER;
    if ( pError ) { xllmErrorInit(pError); }
    if ( !pClient || !pRequest ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "client and request are required");
        return NULL;
    }
    if ( pClient->bHasModelProfile &&
         !xllmModelProfileValidateRequest(&pClient->tModelProfile, pRequest, pError) ) return NULL;
    sBody = xllm__build_request_json(pClient, pRequest, pError);
    if ( !sBody ) { return NULL; }
    pCall = (xllm_call*)calloc(1u, sizeof(*pCall));
    if ( !pCall ) goto oom;
    pCall->pClient = pClient;
    pCall->uAttempt = 1u;
    if ( pCallbacks ) { pCall->tCallbacks = *pCallbacks; }
    xllmErrorInit(&pCall->tError);
    sModel = (pRequest->sModel && pRequest->sModel[0]) ? pRequest->sModel : pClient->sModel;
    pCall->sSelectedModel = xllm__strdup(sModel);
    pCall->sRequestBody = sBody;
    sBody = NULL;
    pCall->uScopeDeadline = pRequest->uDeadline;
    pCall->bScopeAttached = pRequest->pCancel != NULL ||
        pRequest->uDeadline != XRT_DEADLINE_NEVER;
    pCall->pCancel = xrtCancelChild(pRequest->pCancel);
    if ( pClient->uTimeoutMs ) {
        tTimeout = xrtDeadlineAfter((uint64)pClient->uTimeoutMs * UINT64_C(1000));
    }
    pCall->uDeadline = pRequest->uDeadline;
    if ( tTimeout != XRT_DEADLINE_NEVER &&
         (pCall->uDeadline == XRT_DEADLINE_NEVER || tTimeout < pCall->uDeadline) ) {
        pCall->uDeadline = tTimeout;
    }
    if ( pCall->uDeadline != XRT_DEADLINE_NEVER ) {
        pCall->tHttpDiagnostics.uEffectiveTimeoutMs = xrtDeadlineRemaining(pCall->uDeadline) / UINT64_C(1000);
    }
    if ( !pCall->sSelectedModel || !pCall->pCancel ) goto oom;
    pCall->pThread = xrtThreadCreate(xllm__call_thread, pCall, 0u);
    if ( !pCall->pThread ) {
        xllm__error_set(pError, XLLM_ERROR_NETWORK, "failed to start HTTP request");
        goto fail;
    }
    free(sBody);
    return pCall;
oom:
    xllm__error_set(pError, XLLM_ERROR_OUT_OF_MEMORY, "failed to allocate model call");
fail:
    free(sBody);
    xllmCallDestroy(pCall);
    return NULL;
}

xllm_result xllmCallWait(xllm_call* pCall, xllm_response** ppResponse, xllm_error* pError)
{
    xllm_result eResult = XLLM_RESULT_ERROR;
    bool bParsed = false;
    if ( pError ) { xllmErrorInit(pError); }
    if ( ppResponse ) { *ppResponse = NULL; }
    if ( !pCall || !ppResponse || !pCall->pThread || pCall->bWaited ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "call can only be waited once");
        return XLLM_RESULT_ERROR;
    }
    (void)xrtThreadWait(pCall->pThread);
    xrtThreadDestroy(pCall->pThread);
    pCall->pThread = NULL;
    pCall->bWaited = true;

    if ( pCall->bCallbackCancelled ) {
        eResult = XLLM_RESULT_CANCELLED;
        goto done;
    }
    if ( pCall->tError.eCode != XLLM_ERROR_NONE ) {
        eResult = pCall->tError.eCode == XLLM_ERROR_CANCELLED ? XLLM_RESULT_CANCELLED : XLLM_RESULT_ERROR;
        goto done;
    }
    if ( pCall->eTransportResult != XLLM_TRANSPORT_OK ) {
        pCall->tError.iTransportStatus = (int32_t)pCall->eTransportResult;
        if ( pCall->eTransportResult == XLLM_TRANSPORT_TIMEOUT ) {
            xllm__error_set(&pCall->tError, XLLM_ERROR_TIMEOUT, "model request timed out");
            eResult = XLLM_RESULT_TIMEOUT;
        } else if ( pCall->eTransportResult == XLLM_TRANSPORT_CANCELLED ) {
            xllm__error_set(&pCall->tError, XLLM_ERROR_CANCELLED, "model request was cancelled");
            eResult = XLLM_RESULT_CANCELLED;
        } else {
            xllm__error_set(&pCall->tError, XLLM_ERROR_NETWORK, "model request failed");
        }
        goto done;
    }
    if ( pCall->uHttpStatus < 200u || pCall->uHttpStatus >= 300u ) {
        xllm__map_http_error(pCall);
        goto done;
    }
    if ( pCall->bSse ) {
        bParsed = xllm__sse_finish(pCall) && (pCall->bSawEvent || pCall->bDone || pCall->pResponse != NULL);
    } else if ( pCall->tRawBody.iLen ) {
        bParsed = xllm__parse_json_response(pCall, pCall->tRawBody.pData, pCall->tRawBody.iLen);
    }
    if ( !bParsed ) {
        if ( pCall->tError.eCode == XLLM_ERROR_NONE ) {
            xllm__error_set(&pCall->tError, XLLM_ERROR_PROTOCOL, "provider returned no usable response payload");
        }
        goto done;
    }
    if ( !xllm__finalize_response(pCall) ) {
        eResult = pCall->bCallbackCancelled ? XLLM_RESULT_CANCELLED : XLLM_RESULT_ERROR;
        goto done;
    }
    *ppResponse = pCall->pResponse;
    pCall->pResponse = NULL;
    pCall->bResponseTaken = true;
    eResult = XLLM_RESULT_OK;

done:
    xllm__capture_diagnostics(pCall, &pCall->tError.tDiagnostics);
    pCall->tError.iTransportStatus = pCall->tError.tDiagnostics.iTransportStatus;
    pCall->tError.tDiagnostics.bRetryable = xllmErrorRetryable(&pCall->tError);
    if ( ppResponse && *ppResponse ) {
        memcpy(&(*ppResponse)->tDiagnostics, &pCall->tError.tDiagnostics, sizeof(xllm_diagnostics));
    }
    xllm__error_copy(pError, &pCall->tError);
    return eResult;
}

bool xllmCallCancel(xllm_call* pCall)
{
    if ( !pCall || !pCall->pCancel || !pCall->pThread ) { return false; }
    return xrtCancelRequest(pCall->pCancel);
}

void xllmCallDestroy(xllm_call* pCall)
{
    if ( !pCall ) { return; }
    xllm__atomic_store(&pCall->iClosing, 1);
    if ( pCall->pThread ) {
        (void)xrtCancelRequest(pCall->pCancel);
        (void)xrtThreadWait(pCall->pThread);
        xrtThreadDestroy(pCall->pThread);
        pCall->pThread = NULL;
    }
    while ( xllm__atomic_load(&pCall->iCallbackActive) != 0 ) { xrtSleep(1u); }
    xllmResponseDestroy(pCall->pResponse);
    xllm__buf_reset(&pCall->tLine);
    xllm__buf_reset(&pCall->tEventData);
    xllm__buf_reset(&pCall->tRawBody);
    xrtCancelDestroy(pCall->pCancel);
    free(pCall->sRequestBody);
    free(pCall->sSelectedModel);
    free(pCall);
}

xllm_result xllmClientComplete(
    xllm_client* pClient,
    const xllm_request* pRequest,
    const xllm_stream_callbacks* pCallbacks,
    xllm_response** ppResponse,
    xllm_error* pError
)
{
    xllm_call* pCall;
    xllm_error tAttemptError;
    xllm_result eResult = XLLM_RESULT_ERROR;
    uint32_t uAttempt;
    uint32_t uMaxAttempts;
    if ( ppResponse ) { *ppResponse = NULL; }
    if ( pError ) { xllmErrorInit(pError); }
    if ( !pClient || !pRequest || !ppResponse ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "client, request, and response output are required");
        return XLLM_RESULT_ERROR;
    }
    uMaxAttempts = pClient->uMaxAttempts ? pClient->uMaxAttempts : 1u;
    for ( uAttempt = 1u; uAttempt <= uMaxAttempts; ++uAttempt ) {
        uint32_t uDelayMs;
        bool bCanRetry;
        bool bWillRetry;
        xllmErrorInit(&tAttemptError);
        if ( xllm__scope_result(pRequest->pCancel, pRequest->uDeadline, &tAttemptError) != XLLM_RESULT_OK ) {
            xllm__error_copy(pError, &tAttemptError);
            return tAttemptError.eCode == XLLM_ERROR_CANCELLED ? XLLM_RESULT_CANCELLED : XLLM_RESULT_TIMEOUT;
        }
        pCall = xllmClientStart(pClient, pRequest, pCallbacks, &tAttemptError);
        if ( !pCall ) {
            xllm__error_copy(pError, &tAttemptError);
            return XLLM_RESULT_ERROR;
        }
        pCall->uAttempt = uAttempt;
        eResult = xllmCallWait(pCall, ppResponse, &tAttemptError);
        xllmCallDestroy(pCall);
        if ( eResult == XLLM_RESULT_OK ) {
            xllm__error_copy(pError, &tAttemptError);
            return eResult;
        }
        if ( (pRequest->pCancel && xrtCancelRequested(pRequest->pCancel)) ||
             (pRequest->uDeadline != XRT_DEADLINE_NEVER &&
              xrtDeadlineExpired(pRequest->uDeadline)) ) {
            tAttemptError.tDiagnostics.bContextAttached = true;
            tAttemptError.tDiagnostics.uContextDeadlineMs =
                pRequest->uDeadline == XRT_DEADLINE_NEVER ? 0u :
                pRequest->uDeadline / UINT64_C(1000);
            xllm__copy_text(tAttemptError.tDiagnostics.sContextStatus,
                sizeof(tAttemptError.tDiagnostics.sContextStatus),
                pRequest->pCancel && xrtCancelRequested(pRequest->pCancel) ?
                    "cancelled" : "deadline_exceeded");
            xllm__error_copy(pError, &tAttemptError);
            return pRequest->pCancel && xrtCancelRequested(pRequest->pCancel) ?
                XLLM_RESULT_CANCELLED : XLLM_RESULT_TIMEOUT;
        }
        bCanRetry = xllmErrorRetryable(&tAttemptError);
        bWillRetry = bCanRetry && uAttempt < uMaxAttempts;
        tAttemptError.tDiagnostics.bRetryable = bWillRetry;
        tAttemptError.tDiagnostics.bRetryExhausted = bCanRetry && !bWillRetry && uAttempt >= uMaxAttempts;
        if ( !bWillRetry ) {
            xllm__error_copy(pError, &tAttemptError);
            return eResult;
        }
        uDelayMs = xllm__retry_delay_ms(pClient, uAttempt, tAttemptError.tDiagnostics.uRetryAfterMs);
        if ( uDelayMs > 0u && !xllm__scope_sleep(pRequest->pCancel, pRequest->uDeadline, uDelayMs) ) {
            eResult = xllm__scope_result(pRequest->pCancel, pRequest->uDeadline, &tAttemptError);
            xllm__error_copy(pError, &tAttemptError);
            return eResult;
        }
    }
    xllm__error_copy(pError, &tAttemptError);
    return eResult;
}
