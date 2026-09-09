#include "../xllm.c"

typedef struct test_events {
    int iStarts;
    int iText;
    int iReasoning;
    int iTool;
    int iUsage;
    int iDone;
} test_events;

typedef struct test_server_ctx {
    xnetengine* pEngine;
    xnetlistener* pListener;
    xthread* pThread;
    xcancel* pCancel;
    uint16_t uPort;
    int iRequests;
    int iTransientRequests;
    int iSlowRequests;
    bool bSawAuth;
    bool bSawModel;
    bool bSawTools;
} test_server_ctx;

static int g_iFailures = 0;

#define CHECK(expr, name) do { \
    bool xllm_test_ok__ = !!(expr); \
    printf("  %-54s %s\n", (name), xllm_test_ok__ ? "PASS" : "FAIL"); \
    if ( !xllm_test_ok__ ) { ++g_iFailures; } \
} while (0)

static void test_model_profiles(void)
{
    const xllm_model_profile* pProfile = xllmModelProfileBuiltin("glm-5.2");
    xllm_model_profile tSnapshot;
    xllm_model_profile tInvalid;
    xllm_client_config tClientConfig;
    xllm_client* pClient;
    xllm_request tRequest;
    xllm_error tError;
    CHECK(pProfile && pProfile == xllmModelProfileBuiltin("glm-5.2-coding") &&
        pProfile->uContextWindowTokens == 1000000ull &&
        pProfile->uMaxOutputTokens == 131072u,
        "built-in GLM-5.2 capability profile resolves");
    CHECK(pProfile && xllmModelProfileValidate(pProfile, &tError) &&
        xllmModelProfileSupports(pProfile, XLLM_CAP_TOOL_CALL_OUT | XLLM_CAP_REASONING_CONTROL),
        "built-in model profile validates agent capabilities");
    xllmRequestInit(&tRequest);
    tRequest.bParallelToolCalls = true;
    tRequest.uMaxOutputTokens = 131072u;
    CHECK(xllmRequestSetReasoningEffort(&tRequest, "max") &&
        xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "Inspect this project.") &&
        xllmRequestAddTool(&tRequest, "read_file", "Read a file", "{\"type\":\"object\"}", true) &&
        xllmModelProfileValidateRequest(pProfile, &tRequest, &tError),
        "profile accepts a declared coding-agent request");
    tRequest.uMaxOutputTokens = 131073u;
    CHECK(!xllmModelProfileValidateRequest(pProfile, &tRequest, &tError) &&
        tError.eCode == XLLM_ERROR_INVALID_ARGUMENT,
        "profile rejects output beyond the declared model limit");
    xllmRequestUnit(&tRequest);

    xllmModelProfileInit(&tInvalid);
    tInvalid.sId = "invalid";
    tInvalid.sModel = "invalid";
    tInvalid.uContextWindowTokens = 1000u;
    tInvalid.uMaxInputTokens = 1001u;
    tInvalid.uMaxOutputTokens = 200u;
    CHECK(!xllmModelProfileValidate(&tInvalid, &tError),
        "invalid shared-window profile is rejected");

    xllmClientConfigInit(&tClientConfig);
    tClientConfig.sBaseUrl = "http://127.0.0.1:1/v1/chat/completions";
    tClientConfig.sModel = NULL;
    tClientConfig.pModelProfile = pProfile;
    tClientConfig.uMaxOutputTokens = 131072u;
    tClientConfig.bVerifyPeer = false;
    pClient = xllmClientCreate(&tClientConfig, &tError);
    CHECK(pClient && xllmClientGetModelProfile(pClient, &tSnapshot) &&
        strcmp(tSnapshot.sId, "glm-5.2-coding") == 0 &&
        strcmp(tSnapshot.sModel, "glm-5.2") == 0,
        "client retains an owned model-profile snapshot");
    xllmClientDestroy(pClient);
}

static bool test_on_event(void* pUserData, const xllm_event* pEvent)
{
    test_events* pEvents = (test_events*)pUserData;
    if ( !pEvents || !pEvent ) { return false; }
    switch ( pEvent->eKind ) {
        case XLLM_EVENT_RESPONSE_START: ++pEvents->iStarts; break;
        case XLLM_EVENT_TEXT_DELTA: ++pEvents->iText; break;
        case XLLM_EVENT_REASONING_DELTA: ++pEvents->iReasoning; break;
        case XLLM_EVENT_TOOL_CALL_DELTA: ++pEvents->iTool; break;
        case XLLM_EVENT_USAGE: ++pEvents->iUsage; break;
        case XLLM_EVENT_RESPONSE_DONE: ++pEvents->iDone; break;
        default: return false;
    }
    return true;
}

static bool test_view_equal(xstrview tView, const char* sText)
{
    size_t iLength = strlen(sText);
    return tView.Size == iLength && memcmp(tView.Data, sText, iLength) == 0;
}

static bool test_view_contains(xstrview tView, const char* sText)
{
    size_t i;
    size_t iLength = strlen(sText);
    if ( iLength > tView.Size ) return false;
    for ( i = 0u; i + iLength <= tView.Size; ++i ) {
        if ( memcmp(tView.Data + i, sText, iLength) == 0 ) return true;
    }
    return false;
}

static bool test_server_send_all(xnetstream* pStream, const void* pData, size_t iSize)
{
    const uint8_t* pBytes = (const uint8_t*)pData;
    size_t iOffset = 0u;
    while ( iOffset < iSize ) {
        size_t iChunk = iSize - iOffset;
        xnetresult eResult;
        if ( iChunk > 64u * 1024u ) iChunk = 64u * 1024u;
        eResult = xrtNetStreamSend(pStream, pBytes + iOffset, iChunk);
        if ( eResult == XNET_RESULT_AGAIN ) {
            if ( !xrtNetStreamWait(pStream, XNET_STREAM_WAIT_WRITE,
                    xrtDeadlineAfter(UINT64_C(1000000)), NULL) ) return false;
            continue;
        }
        if ( eResult != XNET_RESULT_OK ) return false;
        iOffset += iChunk;
    }
    return true;
}

static bool test_server_write_response(xnetstream* pStream, uint16_t uStatus,
    const char* sReason, const char* sContentType, const char* sBody,
    const char* sRetryAfter, bool bChunked)
{
    xhttpfield tFields[5];
    size_t iFieldCount = 0u;
    size_t iHeaderSize = 0u;
    size_t iBodySize = strlen(sBody);
    char sLength[32];
    uint8_t* pHeader = NULL;
    bool bOk = false;
    (void)snprintf(sLength, sizeof(sLength), "%llu", (unsigned long long)iBodySize);
    tFields[iFieldCount++] = (xhttpfield){ xllm__sv("Content-Type"), xllm__sv(sContentType) };
    tFields[iFieldCount++] = (xhttpfield){ xllm__sv("X-Request-Id"), xllm__sv("request-local-1") };
    tFields[iFieldCount++] = (xhttpfield){ xllm__sv("Connection"), xllm__sv("close") };
    if ( bChunked ) {
        tFields[iFieldCount++] = (xhttpfield){ xllm__sv("Transfer-Encoding"), xllm__sv("chunked") };
    } else {
        tFields[iFieldCount++] = (xhttpfield){ xllm__sv("Content-Length"), xllm__sv(sLength) };
    }
    if ( sRetryAfter ) {
        tFields[iFieldCount++] = (xhttpfield){ xllm__sv("Retry-After-Ms"), xllm__sv(sRetryAfter) };
    }
    if ( !xrtHttp1ResponseWrite(XHTTP_VERSION_1_1, uStatus, xllm__sv(sReason),
            tFields, iFieldCount, NULL, 0u, &iHeaderSize) ) goto done;
    pHeader = (uint8_t*)malloc(iHeaderSize);
    if ( !pHeader || !xrtHttp1ResponseWrite(XHTTP_VERSION_1_1, uStatus,
            xllm__sv(sReason), tFields, iFieldCount, pHeader, iHeaderSize,
            &iHeaderSize) || !test_server_send_all(pStream, pHeader, iHeaderSize) ) goto done;
    if ( bChunked ) {
        size_t iOffset = 0u;
        while ( iOffset < iBodySize ) {
            uint8_t sChunk[257];
            size_t iPayload = iBodySize - iOffset;
            size_t iChunkSize = 0u;
            if ( iPayload > 239u ) iPayload = 239u;
            if ( !xrtHttp1ChunkWrite(xllm__bv(sBody + iOffset, iPayload),
                    sChunk, sizeof(sChunk), &iChunkSize) ||
                 !test_server_send_all(pStream, sChunk, iChunkSize) ) goto done;
            iOffset += iPayload;
        }
        {
            uint8_t sEnd[16];
            size_t iEndSize = 0u;
            if ( !xrtHttp1ChunkEndWrite(NULL, 0u, sEnd, sizeof(sEnd), &iEndSize) ||
                 !test_server_send_all(pStream, sEnd, iEndSize) ) goto done;
        }
    } else if ( iBodySize && !test_server_send_all(pStream, sBody, iBodySize) ) {
        goto done;
    }
    bOk = true;
done:
    free(pHeader);
    return bOk;
}

static bool test_server_route(test_server_ctx* pCtx, xnetstream* pStream,
    const xhttp1head* pRequest, xstrview tBody)
{
    static const char sStream[] =
        "data: {\"id\":\"resp_live\",\"model\":\"glm-test\",\"choices\":[{\"index\":0,\"delta\":{\"reasoning_content\":\"inspect \"},\"finish_reason\":null}]}\n\n"
        "data: {\"choices\":[{\"index\":0,\"delta\":{\"content\":\"working\",\"tool_calls\":[{\"index\":0,\"id\":\"call_a\",\"function\":{\"name\":\"read_file\",\"arguments\":\"{\\\"pa\"}},{\"index\":1,\"id\":\"call_b\",\"function\":{\"name\":\"search\",\"arguments\":\"{\\\"q\\\":\"}}]},\"finish_reason\":null}]}\n\n"
        "data: {\"choices\":[{\"index\":0,\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"arguments\":\"th\\\":\\\"main.c\\\"}\"}},{\"index\":1,\"function\":{\"arguments\":\"\\\"agent\\\"}\"}}]},\"finish_reason\":\"tool_calls\"}]}\n\n"
        "data: {\"choices\":[],\"usage\":{\"prompt_tokens\":120,\"completion_tokens\":30,\"total_tokens\":150,\"completion_tokens_details\":{\"reasoning_tokens\":9}}}\n\n"
        "data: [DONE]\n\n";
    static const char sJson[] =
        "{\"id\":\"resp_json\",\"model\":\"glm-test\",\"choices\":[{\"index\":0,\"message\":{\"role\":\"assistant\",\"content\":\"fallback\"},\"finish_reason\":\"stop\"}],\"usage\":{\"prompt_tokens\":10,\"completion_tokens\":2,\"total_tokens\":12}}";
    static const char sError[] = "{\"error\":{\"code\":\"rate_limit\",\"message\":\"slow down\"}}";
    static const char sPolicyError[] = "{\"error\":{\"code\":1313,\"message\":\"account policy restricted\"}}";
    const xhttpfield* pAuth;
    if ( !pCtx || !pStream || !pRequest ) return false;
    ++pCtx->iRequests;
    pAuth = xrtHttp1Field(pRequest, xllm__sv("Authorization"));
    if ( pAuth && test_view_equal(pAuth->Value, "Bearer test-key") ) pCtx->bSawAuth = true;
    if ( test_view_contains(tBody, "\"model\":\"glm-test\"") ) pCtx->bSawModel = true;
    if ( test_view_contains(tBody, "\"tools\":[") ) pCtx->bSawTools = true;
    if ( test_view_contains(pRequest->Target, "/transient/") ) {
        ++pCtx->iTransientRequests;
        if ( pCtx->iTransientRequests == 1 ) {
            return test_server_write_response(pStream, 503u, "Service Unavailable",
                "application/json", sError, "1", false);
        }
        return test_server_write_response(pStream, 200u, "OK",
            "application/json", sJson, NULL, false);
    }
    if ( test_view_contains(pRequest->Target, "/slow/") ) {
        ++pCtx->iSlowRequests;
        xrtSleep(250u);
        return test_server_write_response(pStream, 200u, "OK",
            "application/json", sJson, NULL, false);
    }
    if ( test_view_contains(pRequest->Target, "/error/") ) {
        return test_server_write_response(pStream, 429u, "Too Many Requests",
            "application/json", sError, "1", false);
    }
    if ( test_view_contains(pRequest->Target, "/policy/") ) {
        return test_server_write_response(pStream, 429u, "Too Many Requests",
            "application/json", sPolicyError, NULL, false);
    }
    if ( test_view_contains(pRequest->Target, "/json/") ) {
        return test_server_write_response(pStream, 200u, "OK",
            "application/json", sJson, NULL, false);
    }
    return test_server_write_response(pStream, 200u, "OK",
        "text/event-stream", sStream, NULL, true);
}

static void test_server_connection(test_server_ctx* pCtx, xnetstream* pStream)
{
    xllm_buf tWire = {0};
    xhttpfield tFields[64];
    xhttp1head tHead;
    xhttp1limits tLimits;
    xhttp1bodyplan tPlan;
    xhttp1errorinfo tError;
    size_t iMessageSize = 0u;
    bool bReady = false;
    xrtHttp1LimitsInit(&tLimits);
    tLimits.MaxFields = 64u;
    tLimits.MaxHead = 64u * 1024u;
    while ( !xrtCancelRequested(pCtx->pCancel) ) {
        xnetbytes* pBytes = xrtNetStreamRecv(pStream, 64u * 1024u,
            xrtDeadlineAfter(UINT64_C(2000000)), pCtx->pCancel);
        xbytesview tBytes;
        xhttp1status eStatus;
        if ( !pBytes ) break;
        tBytes = xrtNetBytesView(pBytes);
        if ( !xllm__buf_append(&tWire, tBytes.Data, tBytes.Size) ) {
            xrtNetBytesDestroy(pBytes);
            break;
        }
        xrtNetBytesDestroy(pBytes);
        xrtHttp1HeadInit(&tHead, tFields, 64u);
        memset(&tError, 0, sizeof(tError));
        eStatus = xrtHttp1RequestParse(xllm__bv(tWire.pData, tWire.iLen),
            &tHead, &tLimits, &tError);
        if ( eStatus == XHTTP1_READY ) {
            if ( !xrtHttp1RequestBodyPlan(&tHead, &tPlan) ||
                 tPlan.Mode != XHTTP1_BODY_FIXED ||
                 tPlan.Length > UINT64_C(1024) * UINT64_C(1024) ) break;
            iMessageSize = tHead.Bytes + (size_t)tPlan.Length;
            if ( tWire.iLen >= iMessageSize ) { bReady = true; break; }
        } else if ( eStatus != XHTTP1_MORE ) {
            break;
        }
    }
    if ( bReady ) {
        xrtHttp1HeadInit(&tHead, tFields, 64u);
        memset(&tError, 0, sizeof(tError));
        if ( xrtHttp1RequestParse(xllm__bv(tWire.pData, iMessageSize),
                &tHead, &tLimits, &tError) == XHTTP1_READY ) {
            (void)test_server_route(pCtx, pStream, &tHead,
                (xstrview){ (const char*)tWire.pData + tHead.Bytes,
                    iMessageSize - tHead.Bytes });
        }
    }
    (void)xrtNetStreamClose(pStream);
    (void)xrtNetStreamWait(pStream, XNET_STREAM_WAIT_CLOSE,
        xrtDeadlineAfter(UINT64_C(1000000)), NULL);
    xrtNetStreamDestroy(pStream);
    xllm__buf_reset(&tWire);
}

static int32 test_server_thread(ptr pData)
{
    test_server_ctx* pCtx = (test_server_ctx*)pData;
    while ( !xrtCancelRequested(pCtx->pCancel) ) {
        xnetstream* pStream = xrtNetListenerAcceptWait(pCtx->pListener,
            XRT_DEADLINE_NEVER, pCtx->pCancel);
        if ( !pStream ) break;
        test_server_connection(pCtx, pStream);
    }
    return 0;
}

static bool test_server_start(test_server_ctx* pCtx)
{
    xnetengineconfig tEngine;
    xnetlistenconfig tListen;
    xnetaddr tLocal;
    memset(pCtx, 0, sizeof(*pCtx));
    pCtx->pCancel = xrtCancelCreate();
    xrtNetEngineConfigInit(&tEngine);
    pCtx->pEngine = xrtNetEngineCreate(&tEngine);
    if ( !pCtx->pCancel || !pCtx->pEngine || !xrtNetEngineStart(pCtx->pEngine) ) return false;
    xrtNetListenConfigInit(&tListen);
    if ( !xrtNetAddrParse(&tListen.Address, "127.0.0.1", 0u) ) return false;
    pCtx->pListener = xrtNetListen(pCtx->pEngine, &tListen, NULL, NULL, NULL);
    if ( !pCtx->pListener || !xrtNetListenerLocal(pCtx->pListener, &tLocal) ) return false;
    pCtx->uPort = tLocal.Port;
    pCtx->pThread = xrtThreadCreate(test_server_thread, pCtx, 0u);
    return pCtx->pThread != NULL;
}

static void test_server_stop(test_server_ctx* pCtx)
{
    if ( !pCtx ) return;
    if ( pCtx->pCancel ) (void)xrtCancelRequest(pCtx->pCancel);
    if ( pCtx->pListener ) (void)xrtNetListenerClose(pCtx->pListener);
    if ( pCtx->pThread ) {
        (void)xrtThreadWait(pCtx->pThread);
        xrtThreadDestroy(pCtx->pThread);
    }
    if ( pCtx->pListener ) xrtNetListenerDestroy(pCtx->pListener);
    if ( pCtx->pEngine ) {
        (void)xrtNetEngineStop(pCtx->pEngine);
        (void)xrtNetEngineDestroy(pCtx->pEngine);
    }
    xrtCancelDestroy(pCtx->pCancel);
}

static xllm_client* test_make_client(const char* sBaseUrl, xllm_provider eProvider)
{
    xllm_client_config tConfig;
    xllm_error tError;
    xllmClientConfigInit(&tConfig);
    tConfig.sBaseUrl = sBaseUrl;
    tConfig.sApiKey = "test-key";
    tConfig.sModel = "glm-test";
    tConfig.eProvider = eProvider;
    tConfig.uMaxOutputTokens = 131072u;
    tConfig.uTimeoutMs = 5000u;
    tConfig.uIdleTimeoutMs = 2000u;
    tConfig.uMaxAttempts = 3u;
    tConfig.uRetryBaseDelayMs = 1u;
    tConfig.uRetryMaxDelayMs = 5u;
    tConfig.bVerifyPeer = false;
    return xllmClientCreate(&tConfig, &tError);
}

static void test_build_json(void)
{
    xllm_client* pClient = test_make_client("http://127.0.0.1:1/v1", XLLM_PROVIDER_GLM);
    xllm_client* pOpenAIClient = test_make_client("http://127.0.0.1:1/v1", XLLM_PROVIDER_OPENAI_COMPAT);
    xllm_request tRequest;
    xllm_message tAssistant;
    xllm_error tError;
    char* sJson;
    xllmRequestInit(&tRequest);
    xllmMessageInit(&tAssistant, XLLM_ROLE_ASSISTANT);
    CHECK(pClient != NULL && pOpenAIClient != NULL, "provider client configurations");
    CHECK(xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_SYSTEM, "You are an agent."), "add system message");
    CHECK(xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "Inspect the project."), "add user message");
    CHECK(xllmMessageSetReasoning(&tAssistant, "continue prior thought"), "set assistant reasoning continuity");
    CHECK(xllmMessageAddToolCall(&tAssistant, "call_old", "read_file", "{\"path\":\"old.c\"}"), "add assistant tool call");
    CHECK(xllmRequestAddMessage(&tRequest, &tAssistant), "add assistant tool-call message");
    CHECK(xllmRequestAddToolResult(&tRequest, "call_old", "file contents"), "add tool result");
    CHECK(xllmRequestAddTool(&tRequest, "read_file", "Read one file", "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"]}", true), "add strict tool schema");
    sJson = xllmClientBuildRequestJson(pClient, &tRequest, &tError);
    CHECK(sJson != NULL && xrtJsonValid(xllm__sv(sJson)), "request JSON is valid");
    CHECK(sJson && strstr(sJson, "\"max_tokens\":131072"), "configured output token budget serialized");
    CHECK(sJson && strstr(sJson, "\"thinking\":{\"type\":\"enabled\",\"clear_thinking\":false}"), "GLM preserved thinking control serialized");
    CHECK(sJson && strstr(sJson, "\"tool_stream\":true"), "GLM streaming function calls enabled");
    CHECK(sJson && strstr(sJson, "\"strict\":true") == NULL && strstr(sJson, "parallel_tool_calls") == NULL, "GLM request omits undocumented OpenAI-only tool fields");
    CHECK(sJson && strstr(sJson, "\"tool_call_id\":\"call_old\""), "tool result correlation serialized");
    xllmFree(sJson);
    sJson = xllmClientBuildRequestJson(pOpenAIClient, &tRequest, &tError);
    CHECK(sJson && strstr(sJson, "\"strict\":true") && strstr(sJson, "\"parallel_tool_calls\":true"), "OpenAI-compatible strict and parallel tool fields retained");
    CHECK(sJson && strstr(sJson, "tool_stream") == NULL, "OpenAI-compatible request omits GLM tool_stream field");
    xllmFree(sJson);
    xllmErrorInit(&tError);
    tError.eCode = XLLM_ERROR_NETWORK;
    CHECK(xllmErrorRetryable(&tError), "network failure without model data is retryable");
    tError.tDiagnostics.bModelDataDelivered = true;
    CHECK(!xllmErrorRetryable(&tError), "delivered model data suppresses automatic retry");
    xllmMessageUnit(&tAssistant);
    xllmRequestUnit(&tRequest);
    xllmClientDestroy(pClient);
    xllmClientDestroy(pOpenAIClient);
}

static void test_fragmented_parser(void)
{
    static const char sSse[] =
        "data: {\"id\":\"resp_frag\",\"model\":\"glm-test\",\"choices\":[{\"index\":0,\"delta\":{\"reasoning_content\":\"think\"},\"finish_reason\":null}]}\r\n\r\n"
        "data: {\"choices\":[{\"index\":0,\"delta\":{\"content\":\"done\",\"tool_calls\":[{\"index\":0,\"id\":\"a\",\"function\":{\"name\":\"read_file\",\"arguments\":\"{\\\"p\"}},{\"index\":1,\"id\":\"b\",\"function\":{\"name\":\"search\",\"arguments\":\"{\\\"q\\\":\"}}]},\"finish_reason\":null}]}\n\n"
        "data: {\"choices\":[{\"index\":0,\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"arguments\":\"ath\\\":\\\"x.c\\\"}\"}},{\"index\":1,\"function\":{\"arguments\":\"\\\"needle\\\"}\"}}]},\"finish_reason\":\"tool_calls\"}],\"usage\":{\"prompt_tokens\":5,\"completion_tokens\":7,\"total_tokens\":12}}\n\n"
        "data: [DONE]\n\n";
    xllm_call* pCall = (xllm_call*)calloc(1u, sizeof(*pCall));
    test_events tEvents = {0};
    size_t i = 0u;
    size_t iStep = 1u;
    bool bFeedOk = true;
    CHECK(pCall != NULL, "fragmented parser call allocation");
    if ( !pCall ) { return; }
    pCall->uHttpStatus = 200u;
    pCall->bSse = true;
    pCall->sSelectedModel = xllm__strdup("glm-test");
    pCall->tCallbacks.pUserData = &tEvents;
    pCall->tCallbacks.OnEvent = test_on_event;
    while ( i < sizeof(sSse) - 1u ) {
        size_t iChunk = iStep;
        if ( iChunk > sizeof(sSse) - 1u - i ) { iChunk = sizeof(sSse) - 1u - i; }
        if ( !xllm__sse_feed(pCall, sSse + i, iChunk) ) { bFeedOk = false; break; }
        i += iChunk;
        iStep = iStep == 13u ? 1u : iStep + 1u;
    }
    CHECK(bFeedOk, "arbitrarily fragmented stream accepted");
    if ( !bFeedOk ) {
        printf("    parser error: %s\n", pCall->tError.sMessage);
    }
    CHECK(xllm__sse_finish(pCall), "fragmented SSE finalization");
    {
        bool bFinal = xllm__finalize_response(pCall);
        CHECK(bFinal, "fragmented response finalization");
        if ( !bFinal ) {
            printf("    finalize error: %s\n", pCall->tError.sMessage);
            if ( pCall->pResponse && pCall->pResponse->iToolCallCount > 1u ) {
                printf("    tool[1] arguments: %s\n", pCall->pResponse->pToolCalls[1].sArgumentsJson);
            }
        }
    }
    CHECK(pCall->pResponse && strcmp(pCall->pResponse->sContent, "done") == 0, "text deltas assembled");
    CHECK(pCall->pResponse && pCall->pResponse->sReasoningContent && strcmp(pCall->pResponse->sReasoningContent, "think") == 0, "reasoning deltas assembled");
    CHECK(pCall->pResponse && pCall->pResponse->iToolCallCount == 2u, "interleaved multi-tool calls assembled");
    CHECK(pCall->pResponse && pCall->pResponse->iToolCallCount > 0u && strcmp(pCall->pResponse->pToolCalls[0].sArgumentsJson, "{\"path\":\"x.c\"}") == 0, "tool arguments fragments assembled");
    CHECK(pCall->pResponse && pCall->pResponse->iToolCallCount > 1u && strcmp(pCall->pResponse->pToolCalls[1].sArgumentsJson, "{\"q\":\"needle\"}") == 0, "second tool arguments assembled");
    CHECK(pCall->pResponse && pCall->pResponse->tUsage.uTotalTokens == 12u, "stream usage parsed");
    CHECK(tEvents.iReasoning == 1 && tEvents.iText == 1 && tEvents.iTool == 4 && tEvents.iUsage == 1 && tEvents.iDone == 1, "normalized stream events emitted");
    xllmCallDestroy(pCall);
}

static void test_malformed_parser(void)
{
    static const char sMalformed[] =
        "data: {\"choices\":[{\"unterminated\":\"value}\n\n";
    xllm_call* pCall = (xllm_call*)calloc(1u, sizeof(*pCall));
    bool bAccepted;
    CHECK(pCall != NULL, "malformed parser call allocation");
    if ( !pCall ) return;
    pCall->uHttpStatus = 200u;
    pCall->bSse = true;
    bAccepted = xllm__sse_feed(pCall, sMalformed, sizeof(sMalformed) - 1u);
    CHECK(!bAccepted && pCall->tError.eCode == XLLM_ERROR_PARSE &&
        strcmp(pCall->tError.sMessage,
            "invalid JSON in provider event stream") == 0,
        "malformed SSE reports the precise parse failure");
    CHECK(!pCall->bSawEvent && !pCall->bDone && pCall->pResponse == NULL,
        "malformed SSE delivers no partial model response");
    xllmCallDestroy(pCall);
}

static void test_transport(void)
{
    test_server_ctx tServerCtx = {0};
    char sBaseUrl[256];
    char sJsonUrl[256];
    char sErrorUrl[256];
    char sPolicyUrl[256];
    char sTransientUrl[256];
    char sSlowUrl[256];
    xllm_client* pClient = NULL;
    xllm_client* pJsonClient = NULL;
    xllm_client* pErrorClient = NULL;
    xllm_client* pPolicyClient = NULL;
    xllm_client* pTransientClient = NULL;
    xllm_client* pSlowClient = NULL;
    xcancel* pCancel = NULL;
    xllm_request tRequest;
    xllm_response* pResponse = NULL;
    xllm_error tError;
    xllm_stream_callbacks tCallbacks;
    test_events tEvents = {0};
    xllm_result eResult;

    CHECK(test_server_start(&tServerCtx), "local core HTTP/1 provider server starts");
    if ( !tServerCtx.pListener ) goto cleanup;
    (void)snprintf(sBaseUrl, sizeof(sBaseUrl), "http://127.0.0.1:%u/v1", (unsigned)tServerCtx.uPort);
    (void)snprintf(sJsonUrl, sizeof(sJsonUrl), "http://127.0.0.1:%u/json/v1", (unsigned)tServerCtx.uPort);
    (void)snprintf(sErrorUrl, sizeof(sErrorUrl), "http://127.0.0.1:%u/error/v1", (unsigned)tServerCtx.uPort);
    (void)snprintf(sPolicyUrl, sizeof(sPolicyUrl), "http://127.0.0.1:%u/policy/v1", (unsigned)tServerCtx.uPort);
    (void)snprintf(sTransientUrl, sizeof(sTransientUrl), "http://127.0.0.1:%u/transient/v1", (unsigned)tServerCtx.uPort);
    (void)snprintf(sSlowUrl, sizeof(sSlowUrl), "http://127.0.0.1:%u/slow/v1", (unsigned)tServerCtx.uPort);
    pClient = test_make_client(sBaseUrl, XLLM_PROVIDER_GLM);
    pJsonClient = test_make_client(sJsonUrl, XLLM_PROVIDER_OPENAI_COMPAT);
    pErrorClient = test_make_client(sErrorUrl, XLLM_PROVIDER_OPENAI_COMPAT);
    pPolicyClient = test_make_client(sPolicyUrl, XLLM_PROVIDER_OPENAI_COMPAT);
    pTransientClient = test_make_client(sTransientUrl, XLLM_PROVIDER_OPENAI_COMPAT);
    pSlowClient = test_make_client(sSlowUrl, XLLM_PROVIDER_OPENAI_COMPAT);
    CHECK(pClient && pJsonClient && pErrorClient && pPolicyClient && pTransientClient && pSlowClient, "transport clients created");

    xllmRequestInit(&tRequest);
    (void)xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "Use both tools.");
    (void)xllmRequestAddTool(&tRequest, "read_file", "Read a file", "{\"type\":\"object\",\"properties\":{}}", false);
    (void)xllmRequestAddTool(&tRequest, "search", "Search text", "{\"type\":\"object\",\"properties\":{}}", false);
    memset(&tCallbacks, 0, sizeof(tCallbacks));
    tCallbacks.pUserData = &tEvents;
    tCallbacks.OnEvent = test_on_event;
    eResult = xllmClientComplete(pClient, &tRequest, &tCallbacks, &pResponse, &tError);
    CHECK(eResult == XLLM_RESULT_OK, "streaming transport completes");
    if ( eResult != XLLM_RESULT_OK ) {
        printf("    transport error: %s (%s), provider=%s, http=%d\n", tError.sMessage, xllmErrorCodeName(tError.eCode), tError.sProviderMessage, (int)tError.iHttpStatus);
    }
    CHECK(pResponse && pResponse->iToolCallCount == 2u, "transport preserves multiple tool calls");
    CHECK(pResponse && strcmp(pResponse->pToolCalls[0].sArgumentsJson, "{\"path\":\"main.c\"}") == 0, "transport assembles split tool arguments");
    CHECK(pResponse && pResponse->tUsage.uReasoningTokens == 9u, "transport parses reasoning usage");
    CHECK(pResponse && pResponse->sRequestId && strcmp(pResponse->sRequestId, "request-local-1") == 0, "transport captures request id");
    CHECK(pResponse && pResponse->tDiagnostics.uAttemptCount == 1u &&
        strcmp(pResponse->tDiagnostics.sTransportError, "none") == 0 &&
        strcmp(pResponse->tDiagnostics.sTransportPhase, "complete") == 0,
        "transport success diagnostics captured");
    CHECK(tEvents.iStarts == 1 && tEvents.iDone == 1, "transport emits lifecycle events");
    xllmResponseDestroy(pResponse);
    pResponse = NULL;

    eResult = xllmClientComplete(pJsonClient, &tRequest, NULL, &pResponse, &tError);
    CHECK(eResult == XLLM_RESULT_OK && pResponse && strcmp(pResponse->sContent, "fallback") == 0, "non-SSE JSON fallback completes");
    xllmResponseDestroy(pResponse);
    pResponse = NULL;

    eResult = xllmClientComplete(pTransientClient, &tRequest, NULL, &pResponse, &tError);
    CHECK(eResult == XLLM_RESULT_OK && pResponse && strcmp(pResponse->sContent, "fallback") == 0,
        "transient provider failure retries to success");
    CHECK(pResponse && pResponse->tDiagnostics.uAttemptCount == 2u &&
        pResponse->tDiagnostics.uMaxAttempts == 3u && tServerCtx.iTransientRequests == 2,
        "successful retry reports final attempt metadata");
    xllmResponseDestroy(pResponse);
    pResponse = NULL;

    eResult = xllmClientComplete(pErrorClient, &tRequest, NULL, &pResponse, &tError);
    CHECK(eResult == XLLM_RESULT_ERROR && tError.eCode == XLLM_ERROR_RATE_LIMIT, "HTTP rate-limit error normalized");
    CHECK(strcmp(tError.sProviderMessage, "slow down") == 0, "provider error message preserved");
    CHECK(tError.tDiagnostics.uAttemptCount == 3u && tError.tDiagnostics.bRetryExhausted &&
        !tError.tDiagnostics.bRetryable && xllmErrorRetryable(&tError),
        "retry exhaustion remains observable and classifiable");
    CHECK(tError.tDiagnostics.iTransportStatus == XLLM_TRANSPORT_OK &&
        strcmp(tError.tDiagnostics.sTransportPhase, "complete") == 0 &&
        tError.tDiagnostics.uRetryAfterMs == 1u,
        "HTTP failure carries transport and retry-after diagnostics");

    eResult = xllmClientComplete(pPolicyClient, &tRequest, NULL, &pResponse, &tError);
    if ( eResult != XLLM_RESULT_ERROR || strcmp(tError.sProviderCode, "1313") != 0 ||
         strcmp(tError.sProviderMessage, "account policy restricted") != 0 ) {
        printf("    policy result=%d error=%s http=%d code='%s' provider='%s' attempts=%u\n",
            (int)eResult, xllmErrorCodeName(tError.eCode), (int)tError.iHttpStatus,
            tError.sProviderCode, tError.sProviderMessage,
            (unsigned)tError.tDiagnostics.uAttemptCount);
    }
    CHECK(eResult == XLLM_RESULT_ERROR && tError.eCode == XLLM_ERROR_RATE_LIMIT &&
        strcmp(tError.sProviderCode, "1313") == 0 &&
        strcmp(tError.sProviderMessage, "account policy restricted") == 0,
        "numeric provider policy code and message preserved");
    CHECK(tError.tDiagnostics.uAttemptCount == 1u &&
        !tError.tDiagnostics.bRetryable && !tError.tDiagnostics.bRetryExhausted &&
        !xllmErrorRetryable(&tError),
        "account policy restriction is not retried as a transient 429");

    pCancel = xrtCancelCreate();
    CHECK(pCancel && xrtCancelRequest(pCancel), "pre-cancelled model token created");
    xllmRequestSetCancel(&tRequest, pCancel);
    eResult = xllmClientComplete(pClient, &tRequest, NULL, &pResponse, &tError);
    CHECK(eResult == XLLM_RESULT_CANCELLED && tError.eCode == XLLM_ERROR_CANCELLED &&
        tError.tDiagnostics.bContextAttached &&
        strcmp(tError.tDiagnostics.sContextStatus, "cancelled") == 0 &&
        strcmp(tError.tDiagnostics.sTransportError, "cancelled") == 0,
        "pre-cancelled context stops before transport");
    xrtCancelDestroy(pCancel);
    pCancel = NULL;
    xllmRequestSetCancel(&tRequest, NULL);
    xllmRequestSetDeadline(&tRequest, xrtDeadlineAfter(UINT64_C(50000)));
    eResult = xllmClientComplete(pSlowClient, &tRequest, NULL, &pResponse, &tError);
    if ( !(eResult == XLLM_RESULT_TIMEOUT && tError.eCode == XLLM_ERROR_TIMEOUT &&
            tError.tDiagnostics.bContextAttached &&
            strcmp(tError.tDiagnostics.sContextStatus, "deadline_exceeded") == 0 &&
            strcmp(tError.tDiagnostics.sTransportError, "deadline_exceeded") == 0 &&
            tError.tDiagnostics.uEffectiveTimeoutMs > 0u) ) {
        printf("    deadline result=%d error=%s transport=%s phase=%s context=%s attached=%d effective=%llu\n",
            (int)eResult, xllmErrorCodeName(tError.eCode),
            tError.tDiagnostics.sTransportError, tError.tDiagnostics.sTransportPhase,
            tError.tDiagnostics.sContextStatus, tError.tDiagnostics.bContextAttached ? 1 : 0,
            (unsigned long long)tError.tDiagnostics.uEffectiveTimeoutMs);
    }
    CHECK(eResult == XLLM_RESULT_TIMEOUT && tError.eCode == XLLM_ERROR_TIMEOUT &&
        tError.tDiagnostics.bContextAttached &&
        strcmp(tError.tDiagnostics.sContextStatus, "deadline_exceeded") == 0 &&
        strcmp(tError.tDiagnostics.sTransportError, "deadline_exceeded") == 0 &&
        tError.tDiagnostics.uEffectiveTimeoutMs > 0u,
        "live model deadline propagates structured diagnostics");
    xllmRequestSetDeadline(&tRequest, XRT_DEADLINE_NEVER);
    CHECK(tServerCtx.iRequests == 9 && tServerCtx.iSlowRequests == 1 &&
        tServerCtx.bSawAuth && tServerCtx.bSawModel && tServerCtx.bSawTools,
        "server observed auth, model, tools, retries, and one deadline request");
    xllmRequestUnit(&tRequest);

cleanup:
    xrtCancelDestroy(pCancel);
    xllmResponseDestroy(pResponse);
    xllmClientDestroy(pSlowClient);
    xllmClientDestroy(pTransientClient);
    xllmClientDestroy(pPolicyClient);
    xllmClientDestroy(pErrorClient);
    xllmClientDestroy(pJsonClient);
    xllmClientDestroy(pClient);
    test_server_stop(&tServerCtx);
}

int main(void)
{
    printf("xllm v2 tests\n");
    test_model_profiles();
    test_build_json();
    test_fragmented_parser();
    test_malformed_parser();
    test_transport();
    printf("xllm v2: %s (%d failures)\n", g_iFailures ? "FAIL" : "PASS", g_iFailures);
    return g_iFailures ? 1 : 0;
}
