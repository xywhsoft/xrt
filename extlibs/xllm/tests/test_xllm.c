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
    int iKeepaliveRequests;
    bool bLastKeepAlive;
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
        default: break;
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
    const char* sRetryAfter, bool bChunked, bool bKeepAlive)
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
    tFields[iFieldCount++] = (xhttpfield){ xllm__sv("Connection"),
        xllm__sv(bKeepAlive ? "keep-alive" : "close") };
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
    pCtx->bLastKeepAlive = false;
    ++pCtx->iRequests;
    pAuth = xrtHttp1Field(pRequest, xllm__sv("Authorization"));
    if ( pAuth && test_view_equal(pAuth->Value, "Bearer test-key") ) pCtx->bSawAuth = true;
    if ( test_view_contains(tBody, "\"model\":\"glm-test\"") ) pCtx->bSawModel = true;
    if ( test_view_contains(tBody, "\"tools\":[") ) pCtx->bSawTools = true;
    if ( test_view_contains(pRequest->Target, "/transient/") ) {
        ++pCtx->iTransientRequests;
        if ( pCtx->iTransientRequests == 1 ) {
            return test_server_write_response(pStream, 503u, "Service Unavailable",
                "application/json", sError, "1", false, false);
        }
        return test_server_write_response(pStream, 200u, "OK",
            "application/json", sJson, NULL, false, false);
    }
    if ( test_view_contains(pRequest->Target, "/slow/") ) {
        ++pCtx->iSlowRequests;
        xrtSleep(250u);
        return test_server_write_response(pStream, 200u, "OK",
            "application/json", sJson, NULL, false, false);
    }
    if ( test_view_contains(pRequest->Target, "/error/") ) {
        return test_server_write_response(pStream, 429u, "Too Many Requests",
            "application/json", sError, "1", false, false);
    }
    if ( test_view_contains(pRequest->Target, "/policy/") ) {
        return test_server_write_response(pStream, 429u, "Too Many Requests",
            "application/json", sPolicyError, NULL, false, false);
    }
    if ( test_view_contains(pRequest->Target, "/json/") ) {
        return test_server_write_response(pStream, 200u, "OK",
            "application/json", sJson, NULL, false, false);
    }
    if ( test_view_contains(pRequest->Target, "/keepalive/") ) {
        ++pCtx->iKeepaliveRequests;
        pCtx->bLastKeepAlive = true;
        return test_server_write_response(pStream, 200u, "OK",
            "application/json", sJson, NULL, false, true);
    }
    return test_server_write_response(pStream, 200u, "OK",
        "text/event-stream", sStream, NULL, true, false);
}

static void test_server_connection(test_server_ctx* pCtx, xnetstream* pStream)
{
    xllm_buf tWire = {0};
    xhttpfield tFields[64];
    xhttp1head tHead;
    xhttp1limits tLimits;
    xhttp1bodyplan tPlan;
    xhttp1errorinfo tError;
    xrtHttp1LimitsInit(&tLimits);
    tLimits.MaxFields = 64u;
    tLimits.MaxHead = 64u * 1024u;
    for ( ;; ) {
    size_t iMessageSize = 0u;
    bool bReady = false;
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
    if ( !bReady || !pCtx->bLastKeepAlive ) break;
    tWire.iLen = 0u;
    if ( tWire.pData ) tWire.pData[0] = 0;
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
    CHECK(sJson && strstr(sJson, "\"reasoning_content\":\"continue prior thought\""), "GLM assistant history replays reasoning_content");
    xllmFree(sJson);
    sJson = xllmClientBuildRequestJson(pOpenAIClient, &tRequest, &tError);
    CHECK(sJson && strstr(sJson, "\"strict\":true") && strstr(sJson, "\"parallel_tool_calls\":true"), "OpenAI-compatible strict and parallel tool fields retained");
    CHECK(sJson && strstr(sJson, "tool_stream") == NULL, "OpenAI-compatible request omits GLM tool_stream field");
    CHECK(sJson && strstr(sJson, "reasoning_content") == NULL, "OpenAI-compatible history omits GLM reasoning_content field");
    CHECK(sJson && strstr(sJson, "\"max_tokens\":131072") && strstr(sJson, "\"role\":\"system\""), "legacy OpenAI-compatible dialect keeps max_tokens and system role");
    xllmFree(sJson);
    {
        xllm_model_profile tProfile;
        xllm_client_config tConfig;
        xllm_client* pNewApiClient;
        xllmModelProfileInit(&tProfile);
        tProfile.sId = "gpt-test";
        tProfile.sModel = "gpt-test";
        tProfile.eProvider = XLLM_PROVIDER_OPENAI_COMPAT;
        tProfile.uCapabilities = XLLM_CAP_TEXT_IN | XLLM_CAP_TOOL_RESULT_IN |
            XLLM_CAP_TEXT_OUT | XLLM_CAP_JSON_OUT | XLLM_CAP_TOOL_CALL_OUT |
            XLLM_CAP_STREAM | XLLM_CAP_REASONING_CONTROL | XLLM_CAP_PARALLEL_TOOL_CALL |
            XLLM_CAP_MAX_COMPLETION_TOKENS | XLLM_CAP_DEVELOPER_ROLE;
        tProfile.uContextWindowTokens = 400000ull;
        tProfile.uMaxInputTokens = 272000ull;
        tProfile.uMaxOutputTokens = 128000u;
        tProfile.uRecommendedOutputReserveTokens = 65536u;
        tProfile.uRecommendedSummaryTokens = 32768u;
        xllmClientConfigInit(&tConfig);
        tConfig.sBaseUrl = "http://127.0.0.1:1/v1";
        tConfig.sApiKey = "test-key";
        tConfig.sModel = "gpt-test";
        tConfig.uMaxOutputTokens = 0u;
        tConfig.pModelProfile = &tProfile;
        tConfig.bVerifyPeer = false;
        pNewApiClient = xllmClientCreate(&tConfig, &tError);
        CHECK(pNewApiClient != NULL, "new-model profile client created");
        sJson = pNewApiClient ? xllmClientBuildRequestJson(pNewApiClient, &tRequest, &tError) : NULL;
        CHECK(sJson != NULL && xrtJsonValid(xllm__sv(sJson)), "new-model request JSON is valid");
        CHECK(sJson && strstr(sJson, "\"max_completion_tokens\":128000") && strstr(sJson, "\"max_tokens\":") == NULL, "new-model dialect serializes max_completion_tokens only");
        CHECK(sJson && strstr(sJson, "\"role\":\"developer\"") && strstr(sJson, "\"role\":\"system\"") == NULL, "new-model dialect maps system to the developer role");
        CHECK(sJson && strstr(sJson, "reasoning_content") == NULL && strstr(sJson, "tool_stream") == NULL, "new-model dialect omits GLM-only history and tool fields");
        xllmFree(sJson);
        xllmClientDestroy(pNewApiClient);
    }
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

static void test_v3_model(void)
{
    xllm_client* pClient = test_make_client("http://127.0.0.1:1/v1", XLLM_PROVIDER_OPENAI_COMPAT);
    xllm_request tRequest;
    xllm_message tVision;
    xllm_part tPart;
    xllm_error tError;
    char* sJson;
    CHECK(pClient != NULL, "v3 model client created");
    xllmRequestInit(&tRequest);
    xllmRequestSetStop(&tRequest, "END");
    tRequest.fTopP = 0.5;
    tRequest.bHasTopP = true;
    tRequest.eJsonMode = XLLM_JSON_OBJECT;
    tRequest.bStream = false;
    CHECK(xllmRequestSetExtraBody(&tRequest,
            "{\"vendor_beta\":true,\"chat_template_kwargs\":{\"enable_thinking\":false}}") &&
        xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "Describe this image."), "v3 request inputs");
    xllmMessageInit(&tVision, XLLM_ROLE_USER);
    xllmPartInit(&tPart, XLLM_PART_IMAGE);
    CHECK(xllmPartSetImageData(&tPart, "\x89PNG", 4u, "image/png") &&
        xllmMessageAddPart(&tVision, &tPart) &&
        xllmRequestAddMessage(&tRequest, &tVision), "vision message with image part");
    sJson = xllmClientBuildRequestJson(pClient, &tRequest, &tError);
    CHECK(sJson != NULL && xrtJsonValid(xllm__sv(sJson)), "v3 request JSON is valid");
    CHECK(sJson && strstr(sJson, "\"type\":\"image_url\"") && strstr(sJson, "data:image/png;base64,") != NULL,
        "image part serialized as a base64 image_url element");
    CHECK(sJson && strstr(sJson, "\"top_p\":0.5") && strstr(sJson, "\"stop\":\"END\"") &&
        strstr(sJson, "\"response_format\":{\"type\":\"json_object\"}"), "v3 sampling and JSON-mode fields serialized");
    CHECK(sJson && strstr(sJson, "\"stream\":") == NULL, "non-streaming request omits the stream field");
    CHECK(sJson && strstr(sJson, "\"vendor_beta\":true") &&
        strstr(sJson, "\"chat_template_kwargs\":{\"enable_thinking\":false}") &&
        strstr(sJson, "\"vendor_beta\":true,\"chat_template_kwargs\""),
        "extra body JSON shallow-merged (nested object intact)");
    xllmFree(sJson);
    xllm__part_unit(&tPart);
    xllmMessageUnit(&tVision);
    xllmRequestUnit(&tRequest);
    xllmClientDestroy(pClient);
}

static void test_v3_blocks_and_refusal(void)
{
    static const char sSse[] =
        "data: {\"choices\":[{\"index\":0,\"delta\":{\"content\":\"Hello\"},\"finish_reason\":null}]}\n\n"
        "data: {\"choices\":[{\"index\":0,\"delta\":{\"reasoning\":\"thinking\"},\"finish_reason\":null}]}\n\n"
        "data: {\"choices\":[{\"index\":0,\"delta\":{\"content\":\" world\",\"refusal\":null},\"finish_reason\":\"stop\"}]}\n\n"
        "data: {\"choices\":[],\"usage\":{\"prompt_tokens\":3,\"completion_tokens\":2,\"total_tokens\":5}}\n\n"
        "data: [DONE]\n\n";
    static const char sRefusal[] =
        "data: {\"choices\":[{\"index\":0,\"delta\":{\"refusal\":\"cannot help\"},\"finish_reason\":\"stop\"}]}\n\n"
        "data: [DONE]\n\n";
    xllm_call* pCall = (xllm_call*)calloc(1u, sizeof(*pCall));
    CHECK(pCall != NULL, "blocks parser call allocation");
    if ( pCall ) {
        pCall->pDialect = xllm__dialect_completions();
        pCall->uHttpStatus = 200u;
        pCall->bSse = true;
        pCall->bStreamWanted = true;
        pCall->sSelectedModel = xllm__strdup("gpt-test");
        CHECK(xllm__sse_feed(pCall, sSse, sizeof(sSse) - 1u) && xllm__sse_finish(pCall) &&
            xllm__assemble_finalize(pCall), "interleaved stream decoded");
        CHECK(pCall->pResponse && pCall->pResponse->iBlockCount == 3u &&
            pCall->pResponse->pBlocks[0].eKind == XLLM_BLOCK_TEXT &&
            pCall->pResponse->pBlocks[1].eKind == XLLM_BLOCK_REASONING &&
            pCall->pResponse->pBlocks[2].eKind == XLLM_BLOCK_TEXT, "arrival-order blocks recorded");
        CHECK(pCall->pResponse && strcmp(pCall->pResponse->sContent, "Hello world") == 0 &&
            strcmp(pCall->pResponse->sReasoningContent, "thinking") == 0, "convenience fields joined");
        CHECK(pCall->pResponse && pCall->pResponse->eFinish == XLLM_FINISH_STOP &&
            pCall->pResponse->tUsage.uTotalTokens == 5u, "finish and usage normalized");
        xllmCallDestroy(pCall);
    }
    pCall = (xllm_call*)calloc(1u, sizeof(*pCall));
    CHECK(pCall != NULL, "refusal parser call allocation");
    if ( pCall ) {
        pCall->pDialect = xllm__dialect_completions();
        pCall->uHttpStatus = 200u;
        pCall->bSse = true;
        pCall->bStreamWanted = true;
        CHECK(xllm__sse_feed(pCall, sRefusal, sizeof(sRefusal) - 1u) && xllm__sse_finish(pCall) &&
            xllm__assemble_finalize(pCall), "refusal stream decoded");
        CHECK(pCall->pResponse && pCall->pResponse->sRefusal &&
            strcmp(pCall->pResponse->sRefusal, "cannot help") == 0 &&
            pCall->pResponse->eFinish == XLLM_FINISH_REFUSAL, "provider refusal surfaced");
        xllmCallDestroy(pCall);
    }
}

static void test_dialect_anthropic(void)
{
    xllm_client* pClient = test_make_client("http://127.0.0.1:1", XLLM_PROVIDER_ANTHROPIC);
    xllm_request tRequest;
    xllm_message tAssistant;
    xllm_error tError;
    char* sJson;
    static const char sSse[] =
        "event: message_start\n"
        "data: {\"type\":\"message_start\",\"message\":{\"usage\":{\"input_tokens\":11,\"cache_read_input_tokens\":3}}}\n\n"
        "event: content_block_start\n"
        "data: {\"type\":\"content_block_start\",\"index\":0,\"content_block\":{\"type\":\"thinking\"}}\n\n"
        "event: content_block_delta\n"
        "data: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"thinking_delta\",\"thinking\":\"why\"}}\n\n"
        "event: content_block_delta\n"
        "data: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"signature_delta\",\"signature\":\"sig0\"}}\n\n"
        "event: content_block_start\n"
        "data: {\"type\":\"content_block_start\",\"index\":1,\"content_block\":{\"type\":\"text\"}}\n\n"
        "event: content_block_delta\n"
        "data: {\"type\":\"content_block_delta\",\"index\":1,\"delta\":{\"type\":\"text_delta\",\"text\":\"Hi\"}}\n\n"
        "event: content_block_start\n"
        "data: {\"type\":\"content_block_start\",\"index\":2,\"content_block\":{\"type\":\"tool_use\",\"id\":\"tu_1\",\"name\":\"read_file\"}}\n\n"
        "event: content_block_delta\n"
        "data: {\"type\":\"content_block_delta\",\"index\":2,\"delta\":{\"type\":\"input_json_delta\",\"partial_json\":\"{\\\"pa\"}}\n\n"
        "event: content_block_delta\n"
        "data: {\"type\":\"content_block_delta\",\"index\":2,\"delta\":{\"type\":\"input_json_delta\",\"partial_json\":\"th\\\":1}\"}}\n\n"
        "event: message_delta\n"
        "data: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"tool_use\"},\"usage\":{\"output_tokens\":4}}\n\n"
        "event: message_stop\n"
        "data: {\"type\":\"message_stop\"}\n\n";
    CHECK(pClient != NULL && strcmp(pClient->sTarget, "/v1/messages") == 0, "anthropic endpoint path resolves");
    xllmRequestInit(&tRequest);
    xllmMessageInit(&tAssistant, XLLM_ROLE_ASSISTANT);
    CHECK(xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_SYSTEM, "Be terse.") &&
        xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "Read the file.") &&
        xllmMessageAddToolCall(&tAssistant, "tu_0", "read_file", "{\"path\":\"a.c\"}") &&
        xllmRequestAddMessage(&tRequest, &tAssistant) &&
        xllmRequestAddToolResult(&tRequest, "tu_0", "file body") &&
        xllmRequestAddTool(&tRequest, "read_file", "Read one file", "{\"type\":\"object\"}", false) &&
        xllmRequestSetStop(&tRequest, "END"), "anthropic request inputs");
    sJson = xllmClientBuildRequestJson(pClient, &tRequest, &tError);
    CHECK(sJson != NULL && xrtJsonValid(xllm__sv(sJson)), "anthropic request JSON is valid");
    CHECK(sJson && strstr(sJson, "\"system\":\"Be terse.\"") && strstr(sJson, "\"max_tokens\":131072"),
        "anthropic system lift and required max_tokens");
    CHECK(sJson && strstr(sJson, "\"tool_use\",\"id\":\"tu_0\"") && strstr(sJson, "\"input\":{\"path\":\"a.c\"}"),
        "assistant tool calls map to tool_use blocks");
    CHECK(sJson && strstr(sJson, "{\"type\":\"tool_result\",\"tool_use_id\":\"tu_0\",\"content\":\"file body\"}"),
        "tool results map to tool_result blocks in a user message");
    CHECK(sJson && strstr(sJson, "\"input_schema\"") && strstr(sJson, "\"tool_choice\":{\"type\":\"auto\"}") &&
        strstr(sJson, "\"stop_sequences\":[\"END\"]"), "anthropic tool schema, choice, and stop sequences");
    xllmFree(sJson);
    xllmMessageUnit(&tAssistant);
    xllmRequestUnit(&tRequest);
    {
        xllm_call* pCall = (xllm_call*)calloc(1u, sizeof(*pCall));
        CHECK(pCall != NULL, "anthropic parser call allocation");
        if ( pCall ) {
            pCall->pDialect = xllm__dialect_anthropic();
            pCall->uHttpStatus = 200u;
            pCall->bSse = true;
            pCall->bStreamWanted = true;
            pCall->sSelectedModel = xllm__strdup("claude-test");
            CHECK(xllm__sse_feed(pCall, sSse, sizeof(sSse) - 1u) && xllm__sse_finish(pCall) &&
                xllm__assemble_finalize(pCall), "anthropic typed event stream decoded");
            CHECK(pCall->pResponse && strcmp(pCall->pResponse->sContent, "Hi") == 0 &&
                pCall->pResponse->iToolCallCount == 1u &&
                strcmp(pCall->pResponse->pToolCalls[0].sArgumentsJson, "{\"path\":1}") == 0,
                "anthropic text and tool arguments assembled");
            CHECK(pCall->pResponse && pCall->pResponse->iBlockCount == 3u &&
                pCall->pResponse->pBlocks[0].eKind == XLLM_BLOCK_REASONING &&
                pCall->pResponse->pBlocks[1].eKind == XLLM_BLOCK_TEXT &&
                pCall->pResponse->pBlocks[2].eKind == XLLM_BLOCK_TOOL_CALL, "anthropic block order recorded");
            CHECK(pCall->pResponse && pCall->pResponse->pBlocks[0].sNative &&
                strcmp(pCall->pResponse->pBlocks[0].sNative, "sig0") == 0 &&
                pCall->pResponse->sReasoningContent && strcmp(pCall->pResponse->sReasoningContent, "why") == 0,
                "thinking signature captured into block metadata");
            {
                xllm_message tReplay;
                xllm_request tReplayRequest;
                char* sReplayJson;
                CHECK(xllmMessageFromResponse(pCall->pResponse, &tReplay) &&
                    tReplay.iPartCount == 1u && tReplay.pParts[0].eKind == XLLM_PART_NATIVE &&
                    strcmp(tReplay.pParts[0].sNativeType, "thinking_signature") == 0,
                    "response converts signed reasoning into a replayable part");
                xllmRequestInit(&tReplayRequest);
                xllmRequestAddMessage(&tReplayRequest, &tReplay);
                sReplayJson = xllmClientBuildRequestJson(pClient, &tReplayRequest, &tError);
                CHECK(sReplayJson && strstr(sReplayJson, "{\"type\":\"thinking\",\"thinking\":\"why\",\"signature\":\"sig0\"}"),
                    "anthropic replays signed thinking blocks verbatim");
                xllmFree(sReplayJson);
                xllmRequestUnit(&tReplayRequest);
                xllmMessageUnit(&tReplay);
            }
            CHECK(pCall->pResponse && pCall->pResponse->eFinish == XLLM_FINISH_TOOL_CALLS &&
                pCall->pResponse->tUsage.uInputTokens == 11u &&
                pCall->pResponse->tUsage.uOutputTokens == 4u &&
                pCall->pResponse->tUsage.uCachedInputTokens == 3u &&
                pCall->pResponse->tUsage.uTotalTokens == 15u, "anthropic usage and finish normalized");
            xllmCallDestroy(pCall);
        }
    }
    xllmClientDestroy(pClient);
}

static void test_dialect_responses(void)
{
    xllm_client* pClient = test_make_client("http://127.0.0.1:1/v1", XLLM_PROVIDER_OPENAI_RESPONSES);
    xllm_request tRequest;
    xllm_error tError;
    char* sJson;
    static const char sSse[] =
        "data: {\"type\":\"response.output_text.delta\",\"delta\":\"Hel\"}\n\n"
        "data: {\"type\":\"response.output_text.delta\",\"delta\":\"lo\"}\n\n"
        "data: {\"type\":\"response.reasoning_summary_text.delta\",\"delta\":\"ponder\"}\n\n"
        "data: {\"type\":\"response.output_item.added\",\"item\":{\"type\":\"function_call\",\"id\":\"fc_1\",\"call_id\":\"call_1\",\"name\":\"read_file\"}}\n\n"
        "data: {\"type\":\"response.function_call_arguments.delta\",\"item_id\":\"fc_1\",\"delta\":\"{\\\"x\\\":\"}\n\n"
        "data: {\"type\":\"response.function_call_arguments.delta\",\"item_id\":\"fc_1\",\"delta\":\"1}\"}\n\n"
        "data: {\"type\":\"response.completed\",\"response\":{\"usage\":{\"input_tokens\":7,\"output_tokens\":9,\"total_tokens\":16,\"input_tokens_details\":{\"cached_tokens\":2}}}}\n\n";
    CHECK(pClient != NULL && strcmp(pClient->sTarget, "/v1/responses") == 0, "responses endpoint path resolves");
    xllmRequestInit(&tRequest);
    CHECK(xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_SYSTEM, "Be terse.") &&
        xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "Hi") &&
        xllmRequestAddTool(&tRequest, "read_file", "Read one file", "{\"type\":\"object\"}", true) &&
        xllmRequestSetReasoningEffort(&tRequest, "low"), "responses request inputs");
    sJson = xllmClientBuildRequestJson(pClient, &tRequest, &tError);
    CHECK(sJson != NULL && xrtJsonValid(xllm__sv(sJson)), "responses request JSON is valid");
    CHECK(sJson && strstr(sJson, "\"instructions\":\"Be terse.\"") &&
        strstr(sJson, "\"max_output_tokens\":131072"), "responses instructions and max_output_tokens");
    CHECK(sJson && strstr(sJson, "\"input\":[") &&
        strstr(sJson, "{\"type\":\"message\",\"role\":\"user\",\"content\":\"Hi\"}"), "responses typed input items");
    CHECK(sJson && strstr(sJson, "\"reasoning\":{\"effort\":\"low\"}") &&
        strstr(sJson, "{\"type\":\"function\",\"name\":\"read_file\"") && strstr(sJson, "\"strict\":true"),
        "responses reasoning effort and function tools");
    xllmFree(sJson);
    xllmRequestUnit(&tRequest);
    {
        xllm_call* pCall = (xllm_call*)calloc(1u, sizeof(*pCall));
        CHECK(pCall != NULL, "responses parser call allocation");
        if ( pCall ) {
            pCall->pDialect = xllm__dialect_responses();
            pCall->uHttpStatus = 200u;
            pCall->bSse = true;
            pCall->bStreamWanted = true;
            pCall->sSelectedModel = xllm__strdup("gpt-test");
            CHECK(xllm__sse_feed(pCall, sSse, sizeof(sSse) - 1u) && xllm__sse_finish(pCall) &&
                xllm__assemble_finalize(pCall), "responses typed event stream decoded");
            CHECK(pCall->pResponse && strcmp(pCall->pResponse->sContent, "Hello") == 0 &&
                strcmp(pCall->pResponse->sReasoningContent, "ponder") == 0, "responses text and reasoning assembled");
            CHECK(pCall->pResponse && pCall->pResponse->iToolCallCount == 1u &&
                strcmp(pCall->pResponse->pToolCalls[0].sArgumentsJson, "{\"x\":1}") == 0 &&
                strcmp(pCall->pResponse->pToolCalls[0].sName, "read_file") == 0,
                "responses function_call assembled from item deltas");
            CHECK(pCall->pResponse && pCall->pResponse->tUsage.uTotalTokens == 16u &&
                pCall->pResponse->tUsage.uCachedInputTokens == 2u &&
                pCall->bDone, "responses usage normalized and stream completed");
            xllmCallDestroy(pCall);
        }
    }
    xllmClientDestroy(pClient);
}

typedef struct test_concurrent_ctx {
    xllm_client* pClient;
    xllm_result eResult;
    char sContent[32];
} test_concurrent_ctx;

static int32 test_concurrent_thread(ptr pData)
{
    test_concurrent_ctx* pCtx = (test_concurrent_ctx*)pData;
    xllm_request tRequest;
    xllm_response* pResponse = NULL;
    xllm_error tError;
    xllmRequestInit(&tRequest);
    if ( !xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "Ping") ) { return 1; }
    pCtx->eResult = xllmClientComplete(pCtx->pClient, &tRequest, NULL, &pResponse, &tError);
    if ( pCtx->eResult == XLLM_RESULT_OK && pResponse ) {
        xllm__copy_text(pCtx->sContent, sizeof(pCtx->sContent),
            pResponse->sContent ? pResponse->sContent : "");
    }
    xllmResponseDestroy(pResponse);
    xllmRequestUnit(&tRequest);
    return 0;
}

static void test_keepalive_and_concurrency(void)
{
    test_server_ctx tServerCtx = {0};
    char sKeepAliveUrl[256];
    char sJsonUrl[256];
    xllm_client* pKeepAliveClient = NULL;
    xllm_client* pSharedClient = NULL;
    xllm_request tRequest;
    xllm_response* pResponse = NULL;
    xllm_error tError;
    test_concurrent_ctx tWorkers[2];
    xthread* pThreads[2];
    CHECK(test_server_start(&tServerCtx), "reuse test server starts");
    if ( !tServerCtx.pListener ) goto cleanup;
    (void)snprintf(sKeepAliveUrl, sizeof(sKeepAliveUrl), "http://127.0.0.1:%u/keepalive/v1", (unsigned)tServerCtx.uPort);
    (void)snprintf(sJsonUrl, sizeof(sJsonUrl), "http://127.0.0.1:%u/json/v1", (unsigned)tServerCtx.uPort);
    pKeepAliveClient = test_make_client(sKeepAliveUrl, XLLM_PROVIDER_OPENAI_COMPAT);
    CHECK(pKeepAliveClient != NULL, "keep-alive client created");
    xllmRequestInit(&tRequest);
    CHECK(xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "Ping"), "reuse request built");
    pResponse = NULL;
    CHECK(xllmClientComplete(pKeepAliveClient, &tRequest, NULL, &pResponse, &tError) == XLLM_RESULT_OK &&
        pResponse != NULL && !pResponse->tDiagnostics.bReusedConnection,
        "first keep-alive exchange opens a fresh connection");
    xllmResponseDestroy(pResponse);
    pResponse = NULL;
    CHECK(xllmClientComplete(pKeepAliveClient, &tRequest, NULL, &pResponse, &tError) == XLLM_RESULT_OK &&
        pResponse != NULL && pResponse->tDiagnostics.bReusedConnection,
        "second keep-alive exchange reuses the pooled connection");
    xllmResponseDestroy(pResponse);
    pResponse = NULL;
    CHECK(tServerCtx.iKeepaliveRequests == 2, "server served both requests on the kept connection");
    xllmRequestUnit(&tRequest);

    pSharedClient = test_make_client(sJsonUrl, XLLM_PROVIDER_OPENAI_COMPAT);
    CHECK(pSharedClient != NULL, "shared concurrent client created");
    memset(tWorkers, 0, sizeof(tWorkers));
    tWorkers[0].pClient = pSharedClient;
    tWorkers[1].pClient = pSharedClient;
    pThreads[0] = xrtThreadCreate(test_concurrent_thread, &tWorkers[0], 0u);
    pThreads[1] = xrtThreadCreate(test_concurrent_thread, &tWorkers[1], 0u);
    CHECK(pThreads[0] != NULL && pThreads[1] != NULL, "concurrent worker threads started");
    if ( pThreads[0] ) { (void)xrtThreadWait(pThreads[0]); xrtThreadDestroy(pThreads[0]); }
    if ( pThreads[1] ) { (void)xrtThreadWait(pThreads[1]); xrtThreadDestroy(pThreads[1]); }
    CHECK(tWorkers[0].eResult == XLLM_RESULT_OK && tWorkers[1].eResult == XLLM_RESULT_OK &&
        strcmp(tWorkers[0].sContent, "fallback") == 0 && strcmp(tWorkers[1].sContent, "fallback") == 0,
        "two concurrent calls on one client both complete");
cleanup:
    xllmClientDestroy(pKeepAliveClient);
    xllmClientDestroy(pSharedClient);
    test_server_stop(&tServerCtx);
}

static long g_iFailAt = -1;
static size_t g_iTestAllocs;
static bool g_bSawOomError;

static void* test_fault_alloc(size_t iSize)
{
    ++g_iTestAllocs;
    if ( (long)g_iTestAllocs == g_iFailAt ) { return NULL; }
    return malloc(iSize);
}
static void* test_fault_realloc(void* pMemory, size_t iSize)
{
    ++g_iTestAllocs;
    if ( (long)g_iTestAllocs == g_iFailAt ) { return NULL; }
    return realloc(pMemory, iSize);
}
static void test_fault_free(void* pMemory) { free(pMemory); }

static void test_oom_injection(void)
{
    xllm_client* pClient = test_make_client("http://127.0.0.1:1/v1", XLLM_PROVIDER_OPENAI_COMPAT);
    xllm_request tRequest;
    xllm_message tMessage;
    xllm_error tError;
    xllm_allocator tFault = { test_fault_alloc, test_fault_realloc, test_fault_free };
    char* sJson;
    long k;
    long iSucceeded = 0;
    long iFailed = 0;
    CHECK(pClient != NULL, "oom test client created");
    xllmRequestInit(&tRequest);
    xllmMessageInit(&tMessage, XLLM_ROLE_ASSISTANT);
    CHECK(xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_SYSTEM, "Be terse.") &&
        xllmMessageSetContent(&tMessage, "prior answer") &&
        xllmMessageAddToolCall(&tMessage, "call_1", "read_file", "{}") &&
        xllmRequestAddMessage(&tRequest, &tMessage) &&
        xllmRequestAddToolResult(&tRequest, "call_1", "result") &&
        xllmRequestAddTool(&tRequest, "read_file", "Read", "{\"type\":\"object\"}", true) &&
        xllmRequestSetExtraBody(&tRequest, "{\"x\":1}"), "oom request inputs");
    xllm__set_allocator(&tFault);
    for ( k = 1; k <= 96; ++k ) {
        g_iFailAt = k;
        g_iTestAllocs = 0u;
        g_bSawOomError = false;
        sJson = xllmClientBuildRequestJson(pClient, &tRequest, &tError);
        if ( sJson ) {
            ++iSucceeded;
            CHECK(xrtJsonValid(xllm__sv(sJson)) || k == 0, "oom survival keeps JSON valid");
            xllmFree(sJson);
        } else {
            ++iFailed;
            if ( tError.eCode != XLLM_ERROR_OUT_OF_MEMORY ) { g_bSawOomError = true; }
        }
    }
    g_iFailAt = -1;
    xllm__set_allocator(NULL);
    CHECK(iFailed > 0 && iSucceeded > 0, "allocation faults observed across the build path");
    CHECK(!g_bSawOomError, "every injected failure maps to an out-of-memory error");
    sJson = xllmClientBuildRequestJson(pClient, &tRequest, &tError);
    CHECK(sJson != NULL && xrtJsonValid(xllm__sv(sJson)), "default allocator restored after injection");
    xllmFree(sJson);
    xllmMessageUnit(&tMessage);
    xllmRequestUnit(&tRequest);
    xllmClientDestroy(pClient);
}

#include "fixtures/xllm_tls_identity.h"
#include "fixtures/xllm_tls_ca_identity.h"

typedef struct test_tls_server_ctx {
    xnetengine* pEngine;
    xtlslistener* pListener;
    xthread* pThread;
    xcancel* pCancel;
    uint16_t uPort;
    int iRequests;
} test_tls_server_ctx;

static bool test_tls_send_all(xtlsstream* pStream, const void* pData, size_t iSize)
{
    const uint8_t* p = (const uint8_t*)pData;
    size_t iOffset = 0u;
    while ( iOffset < iSize ) {
        size_t iChunk = iSize - iOffset;
        xfuture* pFuture;
        xwaitresult eWait;
        if ( iChunk > 16384u ) iChunk = 16384u;
        pFuture = xrtTlsStreamSendAsync(pStream, p + iOffset, iChunk);
        if ( !pFuture ) return false;
        eWait = xrtFutureWaitFor(pFuture, xrtDeadlineAfter(UINT64_C(5000000)));
        xrtFutureDestroy(pFuture);
        if ( eWait != XWAIT_OK ) return false;
        iOffset += iChunk;
    }
    return true;
}

static void test_tls_serve_one(test_tls_server_ctx* pCtx, xtlsstream* pStream)
{
    static const char sBody[] =
        "{\"id\":\"tls_resp\",\"model\":\"tls-model\",\"choices\":[{\"index\":0,"
        "\"message\":{\"role\":\"assistant\",\"content\":\"secure hello\"},"
        "\"finish_reason\":\"stop\"}],\"usage\":{\"prompt_tokens\":3,"
        "\"completion_tokens\":2,\"total_tokens\":5}}";
    char sHeader[512];
    xllm_buf tIn = {0};
    size_t iQuiet = 0u;
    /* Read until quiescent: two consecutive short-deadline reads with no
     * data. The loopback request always lands within that window. */
    while ( iQuiet < 2u && !xrtCancelRequested(pCtx->pCancel) ) {
        xfuture* pFuture = xrtTlsStreamRecvAsync(pStream, 16384u);
        xwaitresult eWait = pFuture ?
            xrtFutureWaitFor(pFuture, UINT64_C(300000)) : XWAIT_ERROR;
        xnetbytes* pBytes = ( eWait == XWAIT_OK ) ? (xnetbytes*)xrtFutureValue(pFuture) : NULL;
        xbytesview tView = pBytes ? xrtNetBytesView(pBytes) : (xbytesview){0};
        if ( pBytes ) xrtNetBytesDestroy(pBytes);
        if ( pFuture ) xrtFutureDestroy(pFuture);
        if ( !pBytes ) { ++iQuiet; continue; }
        iQuiet = 0u;
        if ( tView.Size && !xllm__buf_append(&tIn, tView.Data, tView.Size) ) break;
    }
    if ( tIn.iLen >= 4u && memcmp(tIn.pData, "POST", 4u) == 0 ) {
        ++pCtx->iRequests;
        (void)snprintf(sHeader, sizeof(sHeader),
            "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
            "Content-Length: %llu\r\nConnection: close\r\n\r\n%s",
            (unsigned long long)(sizeof(sBody) - 1u), sBody);
        (void)test_tls_send_all(pStream, sHeader, strlen(sHeader));
    }
    (void)xrtTlsStreamClose(pStream);
    xllm__buf_reset(&tIn);
}

static int32 test_tls_server_thread(ptr pData)
{
    test_tls_server_ctx* pCtx = (test_tls_server_ctx*)pData;
    while ( !xrtCancelRequested(pCtx->pCancel) && pCtx->iRequests < 2 ) {
        xtlsstream* pStream = xrtTlsListenerAcceptWait(pCtx->pListener,
            xrtDeadlineAfter(UINT64_C(500000)), pCtx->pCancel);
        if ( !pStream ) continue;
        test_tls_serve_one(pCtx, pStream);
        xrtTlsStreamDestroy(pStream);
    }
    return 0;
}

static void test_tls_transport(void)
{
    test_tls_server_ctx tServer = {0};
    xtlscontext* pContext = NULL;
    xtlsidentity* pIdentity = NULL;
    xnetaddr tLocal;
    xllm_client* pClient = NULL;
    xllm_request tRequest;
    xllm_response* pResponse = NULL;
    xllm_error tError;
    char sUrl[256];
    xtlspolicy tPolicy;
    xtlscontextconfig tTlsConfig;
    xtlslistenerconfig tListenConfig;
    xnetengineconfig tEngineConfig;
    xbytesview tCert = { xllm_tls_cert, sizeof(xllm_tls_cert) };
    xbytesview tKey = { xllm_tls_key, sizeof(xllm_tls_key) };
    pIdentity = xrtTlsIdentityRsa(&tCert, 1u, tKey);
    CHECK(pIdentity != NULL, "loopback TLS identity loads");
    xrtTlsPolicyInit(&tPolicy);
    xrtTlsContextConfigInit(&tTlsConfig);
    tTlsConfig.Policy = &tPolicy;
    pContext = xrtTlsContextCreate(&tTlsConfig);
    CHECK(pContext != NULL, "loopback TLS context created");
    tServer.pCancel = xrtCancelCreate();
    xrtNetEngineConfigInit(&tEngineConfig);
    tServer.pEngine = xrtNetEngineCreate(&tEngineConfig);
    CHECK(tServer.pCancel && tServer.pEngine && xrtNetEngineStart(tServer.pEngine), "loopback TLS engine started");
    xrtTlsListenerConfigInit(&tListenConfig);
    CHECK(xrtNetAddrParse(&tListenConfig.Listen.Address, "127.0.0.1", 0u), "loopback TLS address parsed");
    tListenConfig.Tls.Context = pContext;
    tListenConfig.Tls.Identity = pIdentity;
    tServer.pListener = xrtTlsListenerStart(tServer.pEngine, &tListenConfig, NULL, NULL, NULL);
    CHECK(tServer.pListener != NULL && xrtTlsListenerLocal(tServer.pListener, &tLocal), "loopback TLS listener started");
    if ( !tServer.pListener ) goto cleanup;
    tServer.uPort = tLocal.Port;
    tServer.pThread = xrtThreadCreate(test_tls_server_thread, &tServer, 0u);
    CHECK(tServer.pThread != NULL, "loopback TLS server thread started");
    (void)snprintf(sUrl, sizeof(sUrl), "https://127.0.0.1:%u/v1", (unsigned)tServer.uPort);
    {
        xllm_client_config tConfig;
        xllmClientConfigInit(&tConfig);
        tConfig.sBaseUrl = sUrl;
        tConfig.sApiKey = "test-key";
        tConfig.sModel = "tls-model";
        tConfig.eProvider = XLLM_PROVIDER_OPENAI_COMPAT;
        tConfig.uMaxOutputTokens = 64u;
        tConfig.uTimeoutMs = 15000u;
        tConfig.bVerifyPeer = false; /* self-signed loopback identity */
        pClient = xllmClientCreate(&tConfig, &tError);
    }
    CHECK(pClient != NULL, "https client created");
    if ( pClient ) {
        xllmRequestInit(&tRequest);
        tRequest.bStream = false;
        CHECK(xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "ping"), "TLS request built");
        CHECK(xllmClientComplete(pClient, &tRequest, NULL, &pResponse, &tError) == XLLM_RESULT_OK &&
            pResponse && pResponse->sContent && strcmp(pResponse->sContent, "secure hello") == 0,
            "TLS round trip delivers the model payload");
        if ( !pResponse ) {
            printf("    tls error: %s | phase=%s err=%s\n",
                tError.sMessage, tError.tDiagnostics.sTransportPhase,
                tError.tDiagnostics.sTransportError);
        }
        xllmResponseDestroy(pResponse);
        xllmRequestUnit(&tRequest);
    }
cleanup:
    xllmClientDestroy(pClient);
    if ( tServer.pCancel ) (void)xrtCancelRequest(tServer.pCancel);
    if ( tServer.pThread ) { (void)xrtThreadWait(tServer.pThread); xrtThreadDestroy(tServer.pThread); }
    if ( tServer.pListener ) { (void)xrtTlsListenerClose(tServer.pListener); xrtTlsListenerDestroy(tServer.pListener); }
    if ( tServer.pEngine ) { (void)xrtNetEngineStop(tServer.pEngine); (void)xrtNetEngineDestroy(tServer.pEngine); }
    xrtCancelDestroy(tServer.pCancel);
    xrtTlsContextRelease(pContext);
    xrtTlsIdentityRelease(pIdentity);
}


/* ------------------------------------------------------------------ */
/* GAP-TLS: private-CA trust (PEM store, borrowed store, wrong-CA      */
/* rejection) plus the IP-literal no-SNI VerifyName dial path.         */
/* ------------------------------------------------------------------ */

static void test_tls_private_ca(void)
{
    test_tls_server_ctx tServer = {0};
    xtlscontext* pContext = NULL;
    xtlsidentity* pIdentity = NULL;
    xnetaddr tLocal;
    xnetengineconfig tEngineConfig;
    xtlspolicy tPolicy;
    xtlscontextconfig tTlsConfig;
    xtlslistenerconfig tListenConfig;
    xbytesview tCert = { xllm_ca_srv_cert_der, sizeof(xllm_ca_srv_cert_der) };
    xbytesview tKey = { xllm_ca_srv_key_der, sizeof(xllm_ca_srv_key_der) };
    char sUrl[256];
    pIdentity = xrtTlsIdentityRsa(&tCert, 1u, tKey);
    CHECK(pIdentity != NULL, "private-CA loopback identity loads");
    xrtTlsPolicyInit(&tPolicy);
    xrtTlsContextConfigInit(&tTlsConfig);
    tTlsConfig.Policy = &tPolicy;
    pContext = xrtTlsContextCreate(&tTlsConfig);
    tServer.pCancel = xrtCancelCreate();
    xrtNetEngineConfigInit(&tEngineConfig);
    tServer.pEngine = xrtNetEngineCreate(&tEngineConfig);
    CHECK(tServer.pCancel && tServer.pEngine && xrtNetEngineStart(tServer.pEngine),
        "private-CA loopback engine started");
    xrtTlsListenerConfigInit(&tListenConfig);
    CHECK(xrtNetAddrParse(&tListenConfig.Listen.Address, "127.0.0.1", 0u),
        "private-CA loopback address parsed");
    tListenConfig.Tls.Context = pContext;
    tListenConfig.Tls.Identity = pIdentity;
    tServer.pListener = xrtTlsListenerStart(tServer.pEngine, &tListenConfig, NULL, NULL, NULL);
    CHECK(tServer.pListener != NULL && xrtTlsListenerLocal(tServer.pListener, &tLocal),
        "private-CA loopback listener started");
    if ( !tServer.pListener ) goto cleanup;
    tServer.uPort = tLocal.Port;
    tServer.pThread = xrtThreadCreate(test_tls_server_thread, &tServer, 0u);
    CHECK(tServer.pThread != NULL, "private-CA server thread started");
    (void)snprintf(sUrl, sizeof(sUrl), "https://127.0.0.1:%u/v1", (unsigned)tServer.uPort);

    {   /* sCaPem: full chain verification over an IP-literal host (no SNI). */
        xllm_client_config tConfig;
        xllm_client* pClient = NULL;
        xllm_request tRequest;
        xllm_response* pResponse = NULL;
        xllm_error tError;
        xllmClientConfigInit(&tConfig);
        tConfig.sBaseUrl = sUrl;
        tConfig.sApiKey = "test-key";
        tConfig.sModel = "tls-model";
        tConfig.eProvider = XLLM_PROVIDER_OPENAI_COMPAT;
        tConfig.uMaxOutputTokens = 64u;
        tConfig.uTimeoutMs = 15000u;
        tConfig.sCaPem = xllm_ca_pem;
        pClient = xllmClientCreate(&tConfig, &tError);
        CHECK(pClient != NULL, "private-CA client created");
        if ( pClient ) {
            xllmRequestInit(&tRequest);
            tRequest.bStream = false;
            (void)xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "ping");
            CHECK(xllmClientComplete(pClient, &tRequest, NULL, &pResponse, &tError) == XLLM_RESULT_OK &&
                pResponse && pResponse->sContent && strcmp(pResponse->sContent, "secure hello") == 0,
                "CA PEM store verifies the chain and matches the IP SAN");
            if ( !pResponse ) {
                printf("    ca pem error: %s | phase=%s err=%s\n",
                    tError.sMessage, tError.tDiagnostics.sTransportPhase,
                    tError.tDiagnostics.sTransportError);
            }
            xllmResponseDestroy(pResponse);
            pResponse = NULL;
            xllmRequestUnit(&tRequest);
            xllmClientDestroy(pClient);
        }
    }
    {   /* wrong CA: the handshake must fail verification, no silent fallback */
        xllm_client_config tConfig;
        xllm_client* pClient = NULL;
        xllm_request tRequest;
        xllm_response* pResponse = NULL;
        xllm_error tError;
        xllmClientConfigInit(&tConfig);
        tConfig.sBaseUrl = sUrl;
        tConfig.sApiKey = "test-key";
        tConfig.sModel = "tls-model";
        tConfig.eProvider = XLLM_PROVIDER_OPENAI_COMPAT;
        tConfig.uMaxOutputTokens = 64u;
        tConfig.uTimeoutMs = 8000u;
        tConfig.uMaxAttempts = 1u;
        tConfig.sCaPem = xllm_wrong_ca_pem;
        pClient = xllmClientCreate(&tConfig, &tError);
        CHECK(pClient != NULL, "wrong-CA client created");
        if ( pClient ) {
            xllm_result eResult;
            xllmRequestInit(&tRequest);
            tRequest.bStream = false;
            (void)xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "ping");
            eResult = xllmClientComplete(pClient, &tRequest, NULL, &pResponse, &tError);
            CHECK(eResult != XLLM_RESULT_OK && pResponse == NULL,
                "wrong CA fails verification instead of silently degrading");
            if ( eResult == XLLM_RESULT_OK ) { xllmResponseDestroy(pResponse); }
            xllmRequestUnit(&tRequest);
            xllmClientDestroy(pClient);
        }
    }
    {   /* borrowed pX509Store outranks sCaPem */
        xllm_client_config tConfig;
        xllm_client* pClient = NULL;
        xllm_request tRequest;
        xllm_response* pResponse = NULL;
        xllm_error tError;
        xx509store* pStore = xrtX509StoreCreate();
        size_t iAdded = 0u;
        bool bLoaded = pStore && xrtX509StoreAddPem(pStore, xllm_ca_pem,
            strlen(xllm_ca_pem), &iAdded) && iAdded == 1u;
        CHECK(bLoaded, "test-side borrowed store loads the CA PEM");
        xllmClientConfigInit(&tConfig);
        tConfig.sBaseUrl = sUrl;
        tConfig.sApiKey = "test-key";
        tConfig.sModel = "tls-model";
        tConfig.eProvider = XLLM_PROVIDER_OPENAI_COMPAT;
        tConfig.uMaxOutputTokens = 64u;
        tConfig.uTimeoutMs = 15000u;
        tConfig.sCaPem = xllm_wrong_ca_pem; /* must lose against the store */
        tConfig.pX509Store = pStore;
        pClient = bLoaded ? xllmClientCreate(&tConfig, &tError) : NULL;
        CHECK(pClient != NULL, "borrowed-store client created");
        if ( pClient ) {
            xllmRequestInit(&tRequest);
            tRequest.bStream = false;
            (void)xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "ping");
            CHECK(xllmClientComplete(pClient, &tRequest, NULL, &pResponse, &tError) == XLLM_RESULT_OK &&
                pResponse && pResponse->sContent && strcmp(pResponse->sContent, "secure hello") == 0,
                "borrowed store outranks the (wrong) CA PEM field");
            xllmResponseDestroy(pResponse);
            xllmRequestUnit(&tRequest);
            xllmClientDestroy(pClient);
        }
        xrtX509StoreFree(pStore);
    }

cleanup:
    if ( tServer.pCancel ) { xrtCancelRequest(tServer.pCancel); }
    if ( tServer.pThread ) { (void)xrtThreadWait(tServer.pThread); xrtThreadDestroy(tServer.pThread); }
    if ( tServer.pListener ) { xrtTlsListenerDestroy(tServer.pListener); }
    if ( tServer.pEngine ) { xrtNetEngineStop(tServer.pEngine); xrtNetEngineDestroy(tServer.pEngine); }
    if ( tServer.pCancel ) { xrtCancelDestroy(tServer.pCancel); }
    if ( pIdentity ) { xrtTlsIdentityRelease(pIdentity); }
    if ( pContext ) { xrtTlsContextRelease(pContext); }
}

static void test_async_engine(void)
{
    test_server_ctx tServerCtx = {0};
    char sJsonUrl[256];
    xllm_client* pClient = NULL;
    xllm_request tRequest;
    xllm_error tError;
    xllm_call* tCalls[6];
    xllm_response* pResponse = NULL;
    int i;
    int iOk = 0;
    printf("async engine transport\n");
    CHECK(test_server_start(&tServerCtx), "async test server starts");
    if ( !tServerCtx.pListener ) goto cleanup;
    (void)snprintf(sJsonUrl, sizeof(sJsonUrl), "http://127.0.0.1:%u/json/v1", (unsigned)tServerCtx.uPort);
    pClient = test_make_client(sJsonUrl, XLLM_PROVIDER_OPENAI_COMPAT);
    CHECK(pClient != NULL, "async client created");
    xllmRequestInit(&tRequest);
    CHECK(xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "Ping"), "async request built");
    /* Submit every call before waiting on any: Start must return while the
     * transports run on the engine workers. */
    for ( i = 0; i < 6; ++i ) {
        tCalls[i] = xllmClientStart(pClient, &tRequest, NULL, &tError);
        if ( !tCalls[i] ) { break; }
    }
    CHECK(i == 6 && xllmCallFuture(tCalls[0]) != NULL,
        "six calls started concurrently with futures");
    for ( ; i > 0; ) {
        --i;
        pResponse = NULL;
        if ( xllmCallWait(tCalls[i], &pResponse, &tError) == XLLM_RESULT_OK &&
             pResponse && pResponse->sContent &&
             strcmp(pResponse->sContent, "fallback") == 0 ) {
            ++iOk;
        }
        xllmResponseDestroy(pResponse);
        xllmCallDestroy(tCalls[i]);
    }
    CHECK(iOk == 6, "all async calls complete through engine workers");
    xllmRequestUnit(&tRequest);

    /* Shared engine: two clients borrow one engine; destroying a client
     * must not stop it. */
    {
        xnetengineconfig tEngineConfig;
        xnetengine* pEngine = NULL;
        xllm_client* pShared1 = NULL;
        xllm_client* pShared2 = NULL;
        xrtNetEngineConfigInit(&tEngineConfig);
        pEngine = xrtNetEngineCreate(&tEngineConfig);
        CHECK(pEngine != NULL && xrtNetEngineStart(pEngine), "shared engine started");
        {
            xllm_client_config tConfig;
            xllmClientConfigInit(&tConfig);
            tConfig.sBaseUrl = sJsonUrl;
            tConfig.sApiKey = "test-key";
            tConfig.sModel = "glm-test";
            tConfig.eProvider = XLLM_PROVIDER_OPENAI_COMPAT;
            tConfig.uMaxOutputTokens = 131072u;
            tConfig.uTimeoutMs = 15000u;
            tConfig.bVerifyPeer = false;
            tConfig.pNetEngine = pEngine;
            pShared1 = xllmClientCreate(&tConfig, &tError);
            pShared2 = xllmClientCreate(&tConfig, &tError);
        }
        CHECK(pShared1 != NULL && pShared2 != NULL, "clients borrow the shared engine");
        xllmRequestInit(&tRequest);
        xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "Ping");
        pResponse = NULL;
        CHECK(xllmClientComplete(pShared1, &tRequest, NULL, &pResponse, &tError) == XLLM_RESULT_OK &&
            pResponse && pResponse->sContent && strcmp(pResponse->sContent, "fallback") == 0,
            "shared-engine client 1 completes");
        xllmResponseDestroy(pResponse);
        pResponse = NULL;
        CHECK(xllmClientComplete(pShared2, &tRequest, NULL, &pResponse, &tError) == XLLM_RESULT_OK &&
            pResponse && pResponse->sContent && strcmp(pResponse->sContent, "fallback") == 0,
            "shared-engine client 2 completes");
        xllmResponseDestroy(pResponse);
        xllmRequestUnit(&tRequest);
        xllmClientDestroy(pShared1);
        xllmClientDestroy(pShared2);
        pResponse = NULL;
        {
            xllm_client_config tConfig;
            xllmClientConfigInit(&tConfig);
            tConfig.sBaseUrl = sJsonUrl;
            tConfig.sApiKey = "test-key";
            tConfig.sModel = "glm-test";
            tConfig.eProvider = XLLM_PROVIDER_OPENAI_COMPAT;
            tConfig.uMaxOutputTokens = 131072u;
            tConfig.uTimeoutMs = 15000u;
            tConfig.bVerifyPeer = false;
            tConfig.pNetEngine = pEngine;
            pShared1 = xllmClientCreate(&tConfig, &tError);
        }
        xllmRequestInit(&tRequest);
        xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "Ping");
        CHECK(pShared1 != NULL &&
            xllmClientComplete(pShared1, &tRequest, NULL, &pResponse, &tError) == XLLM_RESULT_OK &&
            pResponse && pResponse->sContent,
            "shared engine survives client destruction");
        xllmResponseDestroy(pResponse);
        xllmRequestUnit(&tRequest);
        xllmClientDestroy(pShared1);
        (void)xrtNetEngineStop(pEngine);
        (void)xrtNetEngineDestroy(pEngine);
    }
cleanup:
    xllmClientDestroy(pClient);
    test_server_stop(&tServerCtx);
}

static void test_audit_hardening(void)
{
    /* SSE line cap: a peer that never terminates a line must fail closed. */
    {
        xllm_call* pCall = (xllm_call*)calloc(1u, sizeof(*pCall));
        char* sFlood;
        static const char sHead[] = "data: ";
        CHECK(pCall != NULL, "sse cap call allocation");
        if ( pCall ) {
            pCall->pDialect = xllm__dialect_completions();
            pCall->uHttpStatus = 200u;
            pCall->bSse = true;
            pCall->bStreamWanted = true;
            sFlood = (char*)malloc(sizeof(sHead) - 1u + (1024u * 1024u) + 8u);
            CHECK(sFlood != NULL, "sse cap flood allocation");
            if ( sFlood ) {
                memcpy(sFlood, sHead, sizeof(sHead) - 1u);
                memset(sFlood + sizeof(sHead) - 1u, 'A', (1024u * 1024u) + 8u);
                CHECK(!xllm__sse_feed(pCall, sFlood,
                        sizeof(sHead) - 1u + (1024u * 1024u) + 8u) &&
                    pCall->tError.eCode == XLLM_ERROR_PROTOCOL,
                    "oversized SSE line rejected at the framing limit");
                free(sFlood);
            }
            xllmCallDestroy(pCall);
        }
    }
    /* UTF-8 gate: invalid content must stop at the API boundary. */
    {
        xllm_message tMessage;
        xllm_part tPart;
        xllmMessageInit(&tMessage, XLLM_ROLE_USER);
        CHECK(!xllmMessageSetContent(&tMessage, "bad\xF0\x28\x8C\x28utf8") &&
            xllmMessageSetContent(&tMessage, "good utf-8 \xE4\xBD\xA0\xE5\xA5\xBD"),
            "message content enforces strict UTF-8");
        CHECK(!xllmMessageSetReasoning(&tMessage, "\xC0\xAFoverlong"),
            "reasoning content rejects overlong encodings");
        xllmPartInit(&tPart, XLLM_PART_TEXT);
        CHECK(!xllmPartSetText(&tPart, "\xED\xA0\x80surrogate"),
            "part text rejects surrogate halves");
        xllmPartInit(&tPart, XLLM_PART_TEXT);
        CHECK(xllmPartSetText(&tPart, "\360\237\230\200emoji"),
            "part text accepts a valid 4-byte sequence");
        xllmMessageUnit(&tMessage);
    }
    /* Timer lifetime: a deadline call destroyed mid-flight must drain the
     * watchdog reference without hanging or crashing. */
    {
        test_server_ctx tServerCtx = {0};
        char sSlowUrl[256];
        xllm_client* pClient = NULL;
        xllm_request tRequest;
        xllm_error tError;
        xllm_call* pCall;
        CHECK(test_server_start(&tServerCtx), "timer drain server starts");
        if ( tServerCtx.pListener ) {
            (void)snprintf(sSlowUrl, sizeof(sSlowUrl), "http://127.0.0.1:%u/slow/v1",
                (unsigned)tServerCtx.uPort);
            pClient = test_make_client(sSlowUrl, XLLM_PROVIDER_OPENAI_COMPAT);
            xllmRequestInit(&tRequest);
            (void)xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "Ping");
            xllmRequestSetDeadline(&tRequest, xrtDeadlineAfter(UINT64_C(2000000)));
            pCall = xllmClientStart(pClient, &tRequest, NULL, &tError);
            CHECK(pCall != NULL && pCall->uTimerId != 0u,
                "deadline call arms the watchdog timer");
            xllmCallDestroy(pCall);
            CHECK(true, "mid-flight destroy drains the timer reference");
            xllmRequestUnit(&tRequest);
            xllmClientDestroy(pClient);
        }
        test_server_stop(&tServerCtx);
    }
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
    pCall->pDialect = xllm__dialect_completions();
    pCall->uHttpStatus = 200u;
    pCall->bSse = true;
    pCall->bStreamWanted = true;
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
        bool bFinal = xllm__assemble_finalize(pCall);
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
    pCall->pDialect = xllm__dialect_completions();
    pCall->uHttpStatus = 200u;
    pCall->bSse = true;
    pCall->bStreamWanted = true;
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
    xllmRequestSetDeadline(&tRequest, xrtDeadlineAfter(UINT64_C(150000)));
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
    test_v3_model();
    test_v3_blocks_and_refusal();
    test_dialect_anthropic();
    test_dialect_responses();
    test_keepalive_and_concurrency();
    test_async_engine();
    test_oom_injection();
    test_audit_hardening();
    test_tls_transport();
    test_tls_private_ca();
    test_fragmented_parser();
    test_malformed_parser();
    test_transport();
    printf("xllm v2: %s (%d failures)\n", g_iFailures ? "FAIL" : "PASS", g_iFailures);
    return g_iFailures ? 1 : 0;
}
