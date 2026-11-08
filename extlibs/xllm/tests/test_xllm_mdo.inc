typedef struct test_events
{
    int iStarts;
    int iText;
    int iReasoning;
    int iTool;
    int iUsage;
    int iDone;
} test_events;

typedef struct test_server_ctx
{
    xnetengine *pEngine;
    xnetlistener *pListener;
    xthread *pThread;
    xcancel *pCancel;
    uint16_t uPort;
    xatomic32 iRequests;
    xatomic32 iTransientRequests;
    xatomic32 iUnsafeDropRequests;
    xatomic32 iIdempotentDropRequests;
    xatomic32 iStaleRequests;
    xatomic32 iSlowRequests;
    xatomic32 iKeepaliveRequests;
    bool bLastKeepAlive;
    bool bCloseAfterResponse;
    xatomic32 bSawAuth;
    xatomic32 bSawModel;
    xatomic32 bSawTools;
    xatomic32 bSawIdempotencyKey;
    xatomic32 bStaleConnectionClosed;
} test_server_ctx;

static int g_iFailures = 0;

#define CHECK(expr, name)                                                                          \
    do                                                                                             \
    {                                                                                              \
        bool xllm_test_ok__ = !!(expr);                                                            \
        printf("  %-54s %s\n", (name), xllm_test_ok__ ? "PASS" : "FAIL");                          \
        if (!xllm_test_ok__)                                                                       \
        {                                                                                          \
            ++g_iFailures;                                                                         \
        }                                                                                          \
    } while (0)

static void test_model_profiles(void)
{
    const xllm_model_profile *pProfile = xllmModelProfileBuiltin("glm-5.2");
    xllm_model_profile tSnapshot;
    xllm_model_profile tInvalid;
    xllm_client_config tClientConfig;
    xllm_client *pClient;
    xllm_request tRequest;
    xllm_error tError;
    CHECK(pProfile && pProfile == xllmModelProfileBuiltin("glm-5.2-coding") &&
              pProfile->uContextWindowTokens == 1000000ull && pProfile->uMaxOutputTokens == 131072u,
          "built-in GLM-5.2 capability profile resolves");
    CHECK(
        pProfile && xllmModelProfileValidate(pProfile, &tError) &&
            xllmModelProfileSupports(pProfile, XLLM_CAP_TOOL_CALL_OUT | XLLM_CAP_REASONING_CONTROL),
        "built-in model profile validates agent capabilities");
    xllmRequestInit(&tRequest);
    tRequest.bParallelToolCalls = true;
    tRequest.uMaxOutputTokens = 131072u;
    CHECK(xllmRequestSetReasoningEffort(&tRequest, "max") &&
              xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "Inspect this project.") &&
              xllmRequestAddTool(&tRequest, "read_file", "Read a file", "{\"type\":\"object\"}",
                                 true) &&
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
    CHECK(tClientConfig.uMaxHttpHeadBytes == 64u * 1024u &&
              tClientConfig.uMaxResponseBodyBytes == 64u * 1024u * 1024u &&
              tClientConfig.uMaxSseLineBytes == 1024u * 1024u &&
              tClientConfig.uMaxSseEventBytes == 16u * 1024u * 1024u,
          "client parser limits have explicit bounded defaults");
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

    /* Post-create replacement: custom self-hosted endpoints. */
    xllmClientConfigInit(&tClientConfig);
    tClientConfig.sBaseUrl = "http://127.0.0.1:1/v1/chat/completions";
    tClientConfig.sModel = "ornith-35b";
    tClientConfig.bVerifyPeer = false;
    pClient = xllmClientCreate(&tClientConfig, &tError);
    CHECK(pClient != NULL && !xllmClientGetModelProfile(pClient, &tSnapshot),
          "unknown models start without a profile");
    if (pClient)
    {
        xllm_model_profile tCustom;
        xllmModelProfileInit(&tCustom);
        tCustom.sId = "ornith-35b-local";
        tCustom.sModel = "ornith-35b";
        tCustom.eWindowMode = XLLM_WINDOW_SHARED_CONTEXT;
        tCustom.uCapabilities =
            XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT | XLLM_CAP_STREAM | XLLM_CAP_TOOL_CALL_OUT;
        tCustom.uContextWindowTokens = 131072ull;
        tCustom.uMaxInputTokens = 131072ull;
        tCustom.uMaxOutputTokens = 8192u;
        CHECK(xllmClientSetModelProfile(pClient, &tCustom, &tError) &&
                  xllmClientGetModelProfile(pClient, &tSnapshot) &&
                  tSnapshot.uContextWindowTokens == 131072ull &&
                  strcmp(tSnapshot.sId, "ornith-35b-local") == 0 &&
                  strcmp(tSnapshot.sModel, "ornith-35b") == 0,
              "post-create custom profile installs (wire model stays client config)");
        tCustom.uRecommendedSummaryTokens = 1024u;
        CHECK(!xllmClientSetModelProfile(pClient, &tCustom, &tError) &&
                  tError.eCode == XLLM_ERROR_INVALID_ARGUMENT,
              "successful profile inspection seals client capabilities");
        tCustom.uContextWindowTokens = 0u;
        CHECK(!xllmClientSetModelProfile(pClient, &tCustom, &tError) &&
                  tError.eCode == XLLM_ERROR_INVALID_ARGUMENT,
              "invalid replacement profile is rejected");
    }
    xllmClientDestroy(pClient);
    tClientConfig.uMaxHttpHeadBytes = UINT32_MAX;
    pClient = xllmClientCreate(&tClientConfig, &tError);
    CHECK(pClient == NULL && tError.eCode == XLLM_ERROR_INVALID_ARGUMENT,
          "client rejects parser limits above the hard ceiling");
    xllmClientDestroy(pClient);
}

static bool test_on_event(void *pUserData, const xllm_event *pEvent)
{
    test_events *pEvents = (test_events *)pUserData;
    if (!pEvents || !pEvent)
    {
        return false;
    }
    switch (pEvent->eKind)
    {
    case XLLM_EVENT_RESPONSE_START:
        ++pEvents->iStarts;
        break;
    case XLLM_EVENT_TEXT_DELTA:
        ++pEvents->iText;
        break;
    case XLLM_EVENT_REASONING_DELTA:
        ++pEvents->iReasoning;
        break;
    case XLLM_EVENT_TOOL_CALL_DELTA:
        ++pEvents->iTool;
        break;
    case XLLM_EVENT_USAGE:
        ++pEvents->iUsage;
        break;
    case XLLM_EVENT_RESPONSE_DONE:
        ++pEvents->iDone;
        break;
    default:
        break;
    }
    return true;
}

static bool test_view_equal(xstrview tView, const char *sText)
{
    size_t iLength = strlen(sText);
    return tView.Size == iLength && memcmp(tView.Data, sText, iLength) == 0;
}

static bool test_view_contains(xstrview tView, const char *sText)
{
    size_t i;
    size_t iLength = strlen(sText);
    if (iLength > tView.Size)
        return false;
    for (i = 0u; i + iLength <= tView.Size; ++i)
    {
        if (memcmp(tView.Data + i, sText, iLength) == 0)
            return true;
    }
    return false;
}

static char *test_read_text_file(const char *sPath)
{
    FILE *pFile = fopen(sPath, "rb");
    char *sData;
    long iSize;
    if (!pFile)
    {
        return NULL;
    }
    if (fseek(pFile, 0, SEEK_END) != 0 || (iSize = ftell(pFile)) < 0)
    {
        fclose(pFile);
        return NULL;
    }
    rewind(pFile);
    sData = (char *)malloc((size_t)iSize + 1u);
    if (!sData || fread(sData, 1u, (size_t)iSize, pFile) != (size_t)iSize)
    {
        free(sData);
        fclose(pFile);
        return NULL;
    }
    fclose(pFile);
    sData[iSize] = '\0';
    return sData;
}

static void test_release_metadata(void)
{
    char* version = test_read_text_file("extlibs/xllm/VERSION");
    char* manifest = test_read_text_file("extlibs/xllm/config/modules.json");
    char* readme = test_read_text_file("extlibs/xllm/README.md");
    char* migration = test_read_text_file("extlibs/xllm/docs/MIGRATION-3.2.md");
    char expected[40];
    snprintf(expected, sizeof(expected), "%d.%d.%d", XLLM_VERSION_MAJOR, XLLM_VERSION_MINOR, XLLM_VERSION_PATCH);
    CHECK(version && strncmp(version,expected,strlen(expected))==0 &&
          strspn(version+strlen(expected),"\r\n")==strlen(version+strlen(expected)),
          "VERSION matches the public header");
    CHECK(manifest && xrtJsonValid(xllm__sv(manifest)) && strstr(manifest,"xllm") && strstr(manifest,"future_bridge"), "modular manifest declares native transport dependencies");
    CHECK(readme && strstr(readme,"xllm v3.2") && strstr(readme,"xllmRequestSetTimeout"), "README documents current native budget compatibility");
    CHECK(migration && strstr(migration,"xllm-session") && strstr(migration,"rebuild"), "migration guide records the consumer rebuild requirement");
    free(migration);free(readme);free(manifest);free(version);
}

static bool test_server_send_all(xnetstream *pStream, const void *pData, size_t iSize)
{
    const uint8_t *pBytes = (const uint8_t *)pData;
    size_t iOffset = 0u;
    while (iOffset < iSize)
    {
        size_t iChunk = iSize - iOffset;
        xnetresult eResult;
        if (iChunk > 64u * 1024u)
            iChunk = 64u * 1024u;
        eResult = xrtNetStreamSend(pStream, pBytes + iOffset, iChunk);
        if (eResult == XNET_RESULT_AGAIN)
        {
            if (!mdo_test_xrtNetStreamWait(pStream, XNET_STREAM_WAIT_WRITE,
                                           xllmDeadlineAfterUs(UINT64_C(1000000)), NULL))
                return false;
            continue;
        }
        if (eResult != XNET_RESULT_OK)
            return false;
        iOffset += iChunk;
    }
    return true;
}

static bool test_server_write_response(xnetstream *pStream, uint16_t uStatus, const char *sReason,
                                       const char *sContentType, const char *sBody,
                                       const char *sRetryAfter, bool bChunked, bool bKeepAlive)
{
    xhttpfield tFields[5];
    size_t iFieldCount = 0u;
    size_t iHeaderSize = 0u;
    size_t iBodySize = strlen(sBody);
    char sLength[32];
    uint8_t *pHeader = NULL;
    bool bOk = false;
    (void)snprintf(sLength, sizeof(sLength), "%llu", (unsigned long long)iBodySize);
    tFields[iFieldCount++] = (xhttpfield){xllm__sv("Content-Type"), xllm__sv(sContentType)};
    tFields[iFieldCount++] = (xhttpfield){xllm__sv("X-Request-Id"), xllm__sv("request-local-1")};
    tFields[iFieldCount++] =
        (xhttpfield){xllm__sv("Connection"), xllm__sv(bKeepAlive ? "keep-alive" : "close")};
    if (bChunked)
    {
        tFields[iFieldCount++] = (xhttpfield){xllm__sv("Transfer-Encoding"), xllm__sv("chunked")};
    }
    else
    {
        tFields[iFieldCount++] = (xhttpfield){xllm__sv("Content-Length"), xllm__sv(sLength)};
    }
    if (sRetryAfter)
    {
        tFields[iFieldCount++] = (xhttpfield){xllm__sv("Retry-After-Ms"), xllm__sv(sRetryAfter)};
    }
    if (!xrtHttp1ResponseWrite(XHTTP_VERSION_1_1, uStatus, xllm__sv(sReason), tFields, iFieldCount,
                               NULL, 0u, &iHeaderSize))
        goto done;
    pHeader = (uint8_t *)malloc(iHeaderSize);
    if (!pHeader ||
        !xrtHttp1ResponseWrite(XHTTP_VERSION_1_1, uStatus, xllm__sv(sReason), tFields, iFieldCount,
                               pHeader, iHeaderSize, &iHeaderSize) ||
        !test_server_send_all(pStream, pHeader, iHeaderSize))
        goto done;
    if (bChunked)
    {
        size_t iOffset = 0u;
        while (iOffset < iBodySize)
        {
            uint8_t sChunk[257];
            size_t iPayload = iBodySize - iOffset;
            size_t iChunkSize = 0u;
            if (iPayload > 239u)
                iPayload = 239u;
            if (!xrtHttp1ChunkWrite(xllm__bv(sBody + iOffset, iPayload), sChunk, sizeof(sChunk),
                                    &iChunkSize) ||
                !test_server_send_all(pStream, sChunk, iChunkSize))
                goto done;
            iOffset += iPayload;
        }
        {
            uint8_t sEnd[16];
            size_t iEndSize = 0u;
            if (!xrtHttp1ChunkEndWrite(NULL, 0u, sEnd, sizeof(sEnd), &iEndSize) ||
                !test_server_send_all(pStream, sEnd, iEndSize))
                goto done;
        }
    }
    else if (iBodySize && !test_server_send_all(pStream, sBody, iBodySize))
    {
        goto done;
    }
    bOk = true;
done:
    free(pHeader);
    return bOk;
}

static bool test_server_route(test_server_ctx *pCtx, xnetstream *pStream,
                              const xhttp1head *pRequest, xstrview tBody)
{
    static const char sStream[] =
        "data: "
        "{\"id\":\"resp_live\",\"model\":\"glm-test\",\"choices\":[{\"index\":0,\"delta\":{"
        "\"reasoning_content\":\"inspect \"},\"finish_reason\":null}]}\n\n"
        "data: "
        "{\"choices\":[{\"index\":0,\"delta\":{\"content\":\"working\",\"tool_calls\":[{\"index\":"
        "0,\"id\":\"call_a\",\"function\":{\"name\":\"read_file\",\"arguments\":\"{\\\"pa\"}},{"
        "\"index\":1,\"id\":\"call_b\",\"function\":{\"name\":\"search\",\"arguments\":\"{"
        "\\\"q\\\":\"}}]},\"finish_reason\":null}]}\n\n"
        "data: "
        "{\"choices\":[{\"index\":0,\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{"
        "\"arguments\":\"th\\\":\\\"main.c\\\"}\"}},{\"index\":1,\"function\":{\"arguments\":"
        "\"\\\"agent\\\"}\"}}]},\"finish_reason\":\"tool_calls\"}]}\n\n"
        "data: "
        "{\"choices\":[],\"usage\":{\"prompt_tokens\":120,\"completion_tokens\":30,\"total_"
        "tokens\":150,\"completion_tokens_details\":{\"reasoning_tokens\":9}}}\n\n"
        "data: [DONE]\n\n";
    static const char sJson[] =
        "{\"id\":\"resp_json\",\"model\":\"glm-test\",\"choices\":[{\"index\":0,\"message\":{"
        "\"role\":\"assistant\",\"content\":\"fallback\"},\"finish_reason\":\"stop\"}],\"usage\":{"
        "\"prompt_tokens\":10,\"completion_tokens\":2,\"total_tokens\":12}}";
    static const char sError[] = "{\"error\":{\"code\":\"rate_limit\",\"message\":\"slow down\"}}";
    static const char sPolicyError[] =
        "{\"error\":{\"code\":1313,\"message\":\"account policy restricted\"}}";
    const xhttpfield *pAuth;
    if (!pCtx || !pStream || !pRequest)
        return false;
    pCtx->bLastKeepAlive = false;
    (void)xrtAtomic32FetchAdd(&pCtx->iRequests, 1u, XMEMORY_RELAXED);
    pAuth = xrtHttp1Field(pRequest, xllm__sv("Authorization"));
    if (pAuth && test_view_equal(pAuth->Value, "Bearer test-key"))
        xrtAtomic32Store(&pCtx->bSawAuth, 1u, XMEMORY_RELAXED);
    if (test_view_contains(tBody, "\"model\":\"glm-test\""))
        xrtAtomic32Store(&pCtx->bSawModel, 1u, XMEMORY_RELAXED);
    if (test_view_contains(tBody, "\"tools\":["))
        xrtAtomic32Store(&pCtx->bSawTools, 1u, XMEMORY_RELAXED);
    if (test_view_contains(pRequest->Target, "/unsafe-drop/"))
    {
        uint32 iAttempt = xrtAtomic32FetchAdd(&pCtx->iUnsafeDropRequests, 1u, XMEMORY_RELAXED) + 1u;
        if (iAttempt == 1u)
        {
            return false;
        }
        return test_server_write_response(pStream, 200u, "OK", "application/json", sJson, NULL,
                                          false, false);
    }
    if (test_view_contains(pRequest->Target, "/idempotent-drop/"))
    {
        const xhttpfield *pIdempotency = xrtHttp1Field(pRequest, xllm__sv("Idempotency-Key"));
        uint32 iAttempt =
            xrtAtomic32FetchAdd(&pCtx->iIdempotentDropRequests, 1u, XMEMORY_RELAXED) + 1u;
        if (pIdempotency && test_view_equal(pIdempotency->Value, "operation-1"))
        {
            xrtAtomic32Store(&pCtx->bSawIdempotencyKey, 1u, XMEMORY_RELAXED);
        }
        if (iAttempt == 1u)
        {
            return false;
        }
        return test_server_write_response(pStream, 200u, "OK", "application/json", sJson, NULL,
                                          false, false);
    }
    if (test_view_contains(pRequest->Target, "/stale/"))
    {
        (void)xrtAtomic32FetchAdd(&pCtx->iStaleRequests, 1u, XMEMORY_RELAXED);
        /* Advertise keep-alive, then close after this response. The client
         * must reject the closed pooled stream before its next POST. */
        pCtx->bCloseAfterResponse = true;
        return test_server_write_response(pStream, 200u, "OK", "application/json", sJson, NULL,
                                          false, true);
    }
    if (test_view_contains(pRequest->Target, "/transient/"))
    {
        uint32 iAttempt = xrtAtomic32FetchAdd(&pCtx->iTransientRequests, 1u, XMEMORY_RELAXED) + 1u;
        if (iAttempt == 1u)
        {
            return test_server_write_response(pStream, 503u, "Service Unavailable",
                                              "application/json", sError, "1", false, false);
        }
        return test_server_write_response(pStream, 200u, "OK", "application/json", sJson, NULL,
                                          false, false);
    }
    if (test_view_contains(pRequest->Target, "/slow/"))
    {
        (void)xrtAtomic32FetchAdd(&pCtx->iSlowRequests, 1u, XMEMORY_RELAXED);
        mdo_test_sleep_ms(250u);
        return test_server_write_response(pStream, 200u, "OK", "application/json", sJson, NULL,
                                          false, false);
    }
    if (test_view_contains(pRequest->Target, "/error/"))
    {
        return test_server_write_response(pStream, 429u, "Too Many Requests", "application/json",
                                          sError, "1", false, false);
    }
    if (test_view_contains(pRequest->Target, "/policy/"))
    {
        return test_server_write_response(pStream, 429u, "Too Many Requests", "application/json",
                                          sPolicyError, NULL, false, false);
    }
    if (test_view_contains(pRequest->Target, "/json/"))
    {
        return test_server_write_response(pStream, 200u, "OK", "application/json", sJson, NULL,
                                          false, false);
    }
    if (test_view_contains(pRequest->Target, "/keepalive/"))
    {
        (void)xrtAtomic32FetchAdd(&pCtx->iKeepaliveRequests, 1u, XMEMORY_RELAXED);
        pCtx->bLastKeepAlive = true;
        return test_server_write_response(pStream, 200u, "OK", "application/json", sJson, NULL,
                                          false, true);
    }
    return test_server_write_response(pStream, 200u, "OK", "text/event-stream", sStream, NULL, true,
                                      false);
}

static void test_server_connection(test_server_ctx *pCtx, xnetstream *pStream)
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
    for (;;)
    {
        size_t iMessageSize = 0u;
        bool bReady = false;
        while (!xrtCancelRequested(pCtx->pCancel))
        {
            xnetbytes *pBytes = xrtNetStreamRecv(
                pStream, 64u * 1024u, xllmDeadlineAfterUs(UINT64_C(2000000)), pCtx->pCancel);
            xbytesview tBytes;
            xhttp1status eStatus;
            if (!pBytes)
                break;
            tBytes = xrtNetBytesView(pBytes);
            if (!xllm__buf_append(&tWire, tBytes.Data, tBytes.Size))
            {
                xrtNetBytesDestroy(pBytes);
                break;
            }
            xrtNetBytesDestroy(pBytes);
            xrtHttp1HeadInit(&tHead, tFields, 64u);
            memset(&tError, 0, sizeof(tError));
            eStatus =
                xrtHttp1RequestParse(xllm__bv(tWire.pData, tWire.iLen), &tHead, &tLimits, &tError);
            if (eStatus == XHTTP1_READY)
            {
                if (!xrtHttp1RequestBodyPlan(&tHead, &tPlan) || tPlan.Mode != XHTTP1_BODY_FIXED ||
                    tPlan.Length > UINT64_C(1024) * UINT64_C(1024))
                    break;
                iMessageSize = tHead.Bytes + (size_t)tPlan.Length;
                if (tWire.iLen >= iMessageSize)
                {
                    bReady = true;
                    break;
                }
            }
            else if (eStatus != XHTTP1_MORE)
            {
                break;
            }
        }
        if (bReady)
        {
            xrtHttp1HeadInit(&tHead, tFields, 64u);
            memset(&tError, 0, sizeof(tError));
            if (xrtHttp1RequestParse(xllm__bv(tWire.pData, iMessageSize), &tHead, &tLimits,
                                     &tError) == XHTTP1_READY)
            {
                (void)test_server_route(pCtx, pStream, &tHead,
                                        (xstrview){(const char *)tWire.pData + tHead.Bytes,
                                                   iMessageSize - tHead.Bytes});
            }
        }
        if (!bReady || !pCtx->bLastKeepAlive)
            break;
        tWire.iLen = 0u;
        if (tWire.pData)
            tWire.pData[0] = 0;
    }
    (void)xrtNetStreamClose(pStream);
    (void)mdo_test_xrtNetStreamWait(pStream, XNET_STREAM_WAIT_CLOSE,
                                    xllmDeadlineAfterUs(UINT64_C(1000000)), NULL);
    xrtNetStreamDestroy(pStream);
    if (pCtx->bCloseAfterResponse)
    {
        pCtx->bCloseAfterResponse = false;
        xrtAtomic32Store(&pCtx->bStaleConnectionClosed, 1u, XMEMORY_RELEASE);
    }
    xllm__buf_reset(&tWire);
}

static int32 test_server_thread(ptr pData)
{
    test_server_ctx *pCtx = (test_server_ctx *)pData;
    while (!xrtCancelRequested(pCtx->pCancel))
    {
        xnetstream *pStream =
            mdo_test_xrtNetListenerAcceptWait(pCtx->pListener, XLLM_DEADLINE_NEVER, pCtx->pCancel);
        if (!pStream)
            break;
        test_server_connection(pCtx, pStream);
    }
    return 0;
}

static bool test_server_start(test_server_ctx *pCtx)
{
    xnetengineconfig tEngine;
    xnetlistenconfig tListen;
    xnetaddr tLocal;
    memset(pCtx, 0, sizeof(*pCtx));
    pCtx->pCancel = xrtCancelCreate();
    xrtNetEngineConfigInit(&tEngine);
    pCtx->pEngine = xrtNetEngineCreate(&tEngine);
    if (!pCtx->pCancel || !pCtx->pEngine || !xrtNetEngineStart(pCtx->pEngine))
        return false;
    xrtNetListenConfigInit(&tListen);
    if (!xrtNetAddrParse(&tListen.Address, "127.0.0.1", 0u))
        return false;
    pCtx->pListener = xrtNetListen(pCtx->pEngine, &tListen, NULL, NULL, NULL);
    if (!pCtx->pListener || !xrtNetListenerLocal(pCtx->pListener, &tLocal))
        return false;
    pCtx->uPort = tLocal.Port;
    pCtx->pThread = xrtThreadCreate(test_server_thread, pCtx, 0u);
    return pCtx->pThread != NULL;
}

static void test_server_stop(test_server_ctx *pCtx)
{
    if (!pCtx)
        return;
    if (pCtx->pCancel)
        (void)xrtCancelRequest(pCtx->pCancel);
    if (pCtx->pListener)
        (void)xrtNetListenerClose(pCtx->pListener);
    if (pCtx->pThread)
    {
        (void)xrtThreadWait(pCtx->pThread);
        xrtThreadDestroy(pCtx->pThread);
    }
    if (pCtx->pListener)
        xrtNetListenerDestroy(pCtx->pListener);
    if (pCtx->pEngine)
    {
        (void)xrtNetEngineStop(pCtx->pEngine);
        (void)xrtNetEngineDestroy(pCtx->pEngine);
    }
    xrtCancelDestroy(pCtx->pCancel);
}

static xllm_client *test_make_client(const char *sBaseUrl, xllm_provider eProvider)
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

static void test_provider_golden(void)
{
    size_t i;
    for (i = 0u; i < sizeof(XLLM_TEST_PROVIDER_GOLDENS) / sizeof(XLLM_TEST_PROVIDER_GOLDENS[0]);
         ++i)
    {
        const xllm_test_provider_golden *pGolden = &XLLM_TEST_PROVIDER_GOLDENS[i];
        xllm_client_config tConfig;
        xllm_client *pClient;
        xllm_request tRequest;
        xllm_error tError;
        char *sRequestJson;
        xllm_call *pCall;
        bool bDecoded;
        xllmClientConfigInit(&tConfig);
        tConfig.sBaseUrl = "http://127.0.0.1:1";
        tConfig.sApiKey = "fixture-key";
        tConfig.sModel = "golden-model";
        tConfig.uMaxOutputTokens = 32u;
        tConfig.eProvider = pGolden->eProvider;
        tConfig.bVerifyPeer = false;
        pClient = xllmClientCreate(&tConfig, &tError);
        CHECK(pClient != NULL && pClient->pDialect != NULL &&
                  strcmp(pClient->pDialect->sName, pGolden->sDialect) == 0 &&
                  strcmp(pClient->pDialect->sPathSuffix, pGolden->sPathSuffix) == 0,
              pGolden->sName);
        if (!pClient)
        {
            continue;
        }

        xllmRequestInit(&tRequest);
        tRequest.bStream = false;
        CHECK(xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "ping"), "golden request input");
        sRequestJson = xllmClientBuildRequestJson(pClient, &tRequest, &tError);
        if (!sRequestJson || strcmp(sRequestJson, pGolden->sRequestJson) != 0)
        {
            printf("    %s request actual: %s\n", pGolden->sName,
                   sRequestJson ? sRequestJson : "<null>");
        }
        CHECK(sRequestJson != NULL && strcmp(sRequestJson, pGolden->sRequestJson) == 0,
              "provider request matches golden bytes");
        xllmFree(sRequestJson);
        xllmRequestUnit(&tRequest);

        pCall = (xllm_call *)calloc(1u, sizeof(*pCall));
        CHECK(pCall != NULL, "golden response call allocation");
        if (pCall)
        {
            pCall->pDialect = pClient->pDialect;
            pCall->uHttpStatus = 200u;
            pCall->sSelectedModel = xllm__strdup("golden-model");
            bDecoded = pCall->pDialect->DecodeJsonBody(pCall, xllm__sv(pGolden->sResponseJson));
            CHECK(bDecoded && xllm__assemble_finalize(pCall), "provider response golden decodes");
            CHECK(pCall->pResponse && pCall->pResponse->sContent &&
                      strcmp(pCall->pResponse->sContent, "pong") == 0 &&
                      pCall->pResponse->eFinish == XLLM_FINISH_STOP &&
                      pCall->pResponse->tUsage.uInputTokens == 2u &&
                      pCall->pResponse->tUsage.uOutputTokens == 1u &&
                      pCall->pResponse->tUsage.uTotalTokens == 3u,
                  "provider response matches normalized golden");
            xllmCallDestroy(pCall);
        }
        xllmClientDestroy(pClient);
    }
}

static bool test_feed_stream_pattern(xllm_call *pCall, const char *sWire, size_t iWire,
                                     unsigned uPattern)
{
    static const size_t aSteps[] = {1u, 2u, 3u, 5u, 8u, 13u};
    size_t iOffset = 0u;
    size_t iStep = 0u;
    if (uPattern == 0u)
    {
        return xllm__sse_feed(pCall, sWire, iWire);
    }
    while (iOffset < iWire)
    {
        size_t iChunk = uPattern == 1u ? 1u : aSteps[iStep % (sizeof(aSteps) / sizeof(aSteps[0]))];
        if (iChunk > iWire - iOffset)
        {
            iChunk = iWire - iOffset;
        }
        if (!xllm__sse_feed(pCall, sWire + iOffset, iChunk))
        {
            return false;
        }
        iOffset += iChunk;
        ++iStep;
    }
    return true;
}

static void test_parser_corpus(void)
{
    size_t i;
    for (i = 0u; i < sizeof(XLLM_TEST_STREAM_GOLDENS) / sizeof(XLLM_TEST_STREAM_GOLDENS[0]); ++i)
    {
        const xllm_test_stream_golden *pGolden = &XLLM_TEST_STREAM_GOLDENS[i];
        unsigned uPattern;
        for (uPattern = 0u; uPattern < 3u; ++uPattern)
        {
            xllm_call *pCall = (xllm_call *)calloc(1u, sizeof(*pCall));
            bool bOk;
            CHECK(pCall != NULL, "parser corpus call allocation");
            if (!pCall)
            {
                continue;
            }
            pCall->pDialect = xllm__dialect_ops_for(pGolden->eProvider);
            pCall->uHttpStatus = 200u;
            pCall->bSse = true;
            pCall->bStreamWanted = true;
            pCall->sSelectedModel = xllm__strdup("golden-model");
            bOk =
                test_feed_stream_pattern(pCall, pGolden->sWire, strlen(pGolden->sWire), uPattern) &&
                xllm__sse_finish(pCall) && xllm__assemble_finalize(pCall);
            if (!bOk)
            {
                printf("    %s pattern %u: %s\n", pGolden->sName, uPattern, pCall->tError.sMessage);
            }
            CHECK(bOk && pCall->pResponse && pCall->pResponse->sContent &&
                      strcmp(pCall->pResponse->sContent, pGolden->sContent) == 0 &&
                      pCall->pResponse->eFinish == XLLM_FINISH_STOP &&
                      pCall->pResponse->tUsage.uInputTokens == 2u &&
                      pCall->pResponse->tUsage.uOutputTokens == 1u &&
                      pCall->pResponse->tUsage.uTotalTokens == 3u,
                  "parser corpus is invariant across fixed fragmentation");
            xllmCallDestroy(pCall);
        }
    }
    {
        static const char sTruncatedUtf8[] =
            "data: {\"choices\":[{\"index\":0,\"delta\":{\"content\":\"\xE7\xAD\"}}]}\n\n";
        xllm_call *pCall = (xllm_call *)calloc(1u, sizeof(*pCall));
        CHECK(pCall != NULL, "truncated UTF-8 corpus call allocation");
        if (pCall)
        {
            pCall->pDialect = xllm__dialect_completions();
            pCall->uHttpStatus = 200u;
            pCall->bSse = true;
            pCall->bStreamWanted = true;
            CHECK(!xllm__sse_feed(pCall, sTruncatedUtf8, sizeof(sTruncatedUtf8) - 1u) &&
                      pCall->tError.eCode == XLLM_ERROR_PARSE && pCall->pResponse == NULL,
                  "truncated UTF-8 is rejected before partial delivery");
            xllmCallDestroy(pCall);
        }
    }
}

static void test_build_json(void)
{
    xllm_client *pClient = test_make_client("http://127.0.0.1:1/v1", XLLM_PROVIDER_GLM);
    xllm_client *pOpenAIClient =
        test_make_client("http://127.0.0.1:1/v1", XLLM_PROVIDER_OPENAI_COMPAT);
    xllm_request tRequest;
    xllm_message tAssistant;
    xllm_error tError;
    char *sJson;
    xllmRequestInit(&tRequest);
    xllmMessageInit(&tAssistant, XLLM_ROLE_ASSISTANT);
    CHECK(pClient != NULL && pOpenAIClient != NULL, "provider client configurations");
    CHECK(xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_SYSTEM, "You are an agent."),
          "add system message");
    CHECK(xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "Inspect the project."),
          "add user message");
    CHECK(xllmMessageSetReasoning(&tAssistant, "continue prior thought"),
          "set assistant reasoning continuity");
    CHECK(xllmMessageAddToolCall(&tAssistant, "call_old", "read_file", "{\"path\":\"old.c\"}"),
          "add assistant tool call");
    CHECK(xllmRequestAddMessage(&tRequest, &tAssistant), "add assistant tool-call message");
    CHECK(xllmRequestAddToolResult(&tRequest, "call_old", "file contents"), "add tool result");
    CHECK(xllmRequestAddTool(&tRequest, "read_file", "Read one file",
                             "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},"
                             "\"required\":[\"path\"]}",
                             true),
          "add strict tool schema");
    sJson = xllmClientBuildRequestJson(pClient, &tRequest, &tError);
    CHECK(sJson != NULL && xrtJsonValid(xllm__sv(sJson)), "request JSON is valid");
    CHECK(sJson && strstr(sJson, "\"max_tokens\":131072"),
          "configured output token budget serialized");
    CHECK(sJson && strstr(sJson, "\"thinking\":{\"type\":\"enabled\",\"clear_thinking\":false}"),
          "GLM preserved thinking control serialized");
    CHECK(sJson && strstr(sJson, "\"tool_stream\":true"), "GLM streaming function calls enabled");
    CHECK(sJson && strstr(sJson, "\"strict\":true") == NULL &&
              strstr(sJson, "parallel_tool_calls") == NULL,
          "GLM request omits undocumented OpenAI-only tool fields");
    CHECK(sJson && strstr(sJson, "\"tool_call_id\":\"call_old\""),
          "tool result correlation serialized");
    CHECK(sJson && strstr(sJson, "\"reasoning_content\":\"continue prior thought\""),
          "GLM assistant history replays reasoning_content");
    xllmFree(sJson);
    sJson = xllmClientBuildRequestJson(pOpenAIClient, &tRequest, &tError);
    CHECK(sJson && strstr(sJson, "\"strict\":true") &&
              strstr(sJson, "\"parallel_tool_calls\":true"),
          "OpenAI-compatible strict and parallel tool fields retained");
    CHECK(sJson && strstr(sJson, "tool_stream") == NULL,
          "OpenAI-compatible request omits GLM tool_stream field");
    CHECK(sJson && strstr(sJson, "reasoning_content") == NULL,
          "OpenAI-compatible history omits GLM reasoning_content field");
    CHECK(sJson && strstr(sJson, "\"max_tokens\":131072") && strstr(sJson, "\"role\":\"system\""),
          "legacy OpenAI-compatible dialect keeps max_tokens and system role");
    xllmFree(sJson);
    {
        xllm_model_profile tProfile;
        xllm_client_config tConfig;
        xllm_client *pNewApiClient;
        xllmModelProfileInit(&tProfile);
        tProfile.sId = "gpt-test";
        tProfile.sModel = "gpt-test";
        tProfile.eProvider = XLLM_PROVIDER_OPENAI_COMPAT;
        tProfile.uCapabilities = XLLM_CAP_TEXT_IN | XLLM_CAP_TOOL_RESULT_IN | XLLM_CAP_TEXT_OUT |
                                 XLLM_CAP_JSON_OUT | XLLM_CAP_TOOL_CALL_OUT | XLLM_CAP_STREAM |
                                 XLLM_CAP_REASONING_CONTROL | XLLM_CAP_PARALLEL_TOOL_CALL |
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
        sJson =
            pNewApiClient ? xllmClientBuildRequestJson(pNewApiClient, &tRequest, &tError) : NULL;
        CHECK(sJson != NULL && xrtJsonValid(xllm__sv(sJson)), "new-model request JSON is valid");
        CHECK(sJson && strstr(sJson, "\"max_completion_tokens\":128000") &&
                  strstr(sJson, "\"max_tokens\":") == NULL,
              "new-model dialect serializes max_completion_tokens only");
        CHECK(sJson && strstr(sJson, "\"role\":\"developer\"") &&
                  strstr(sJson, "\"role\":\"system\"") == NULL,
              "new-model dialect maps system to the developer role");
        CHECK(sJson && strstr(sJson, "reasoning_content") == NULL &&
                  strstr(sJson, "tool_stream") == NULL,
              "new-model dialect omits GLM-only history and tool fields");
        xllmFree(sJson);
        xllmClientDestroy(pNewApiClient);
    }
    xllmMessageUnit(&tAssistant);
    xllmRequestUnit(&tRequest);
    xllmClientDestroy(pClient);
    xllmClientDestroy(pOpenAIClient);
}

static void test_v3_model(void)
{
    xllm_client *pClient = test_make_client("http://127.0.0.1:1/v1", XLLM_PROVIDER_OPENAI_COMPAT);
    xllm_request tRequest;
    xllm_message tVision;
    xllm_part tPart;
    xllm_error tError;
    char *sJson;
    CHECK(pClient != NULL, "v3 model client created");
    xllmRequestInit(&tRequest);
    xllmRequestSetStop(&tRequest, "END");
    tRequest.fTopP = 0.5;
    tRequest.bHasTopP = true;
    tRequest.eJsonMode = XLLM_JSON_OBJECT;
    tRequest.bStream = false;
    CHECK(xllmRequestSetExtraBody(
              &tRequest,
              "{\"vendor_beta\":true,\"chat_template_kwargs\":{\"enable_thinking\":false}}") &&
              xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "Describe this image."),
          "v3 request inputs");
    xllmMessageInit(&tVision, XLLM_ROLE_USER);
    xllmPartInit(&tPart, XLLM_PART_IMAGE);
    CHECK(xllmPartSetImageData(&tPart, "\x89PNG", 4u, "image/png") &&
              xllmMessageAddPart(&tVision, &tPart) && xllmRequestAddMessage(&tRequest, &tVision),
          "vision message with image part");
    sJson = xllmClientBuildRequestJson(pClient, &tRequest, &tError);
    CHECK(sJson != NULL && xrtJsonValid(xllm__sv(sJson)), "v3 request JSON is valid");
    CHECK(sJson && strstr(sJson, "\"type\":\"image_url\"") &&
              strstr(sJson, "data:image/png;base64,") != NULL,
          "image part serialized as a base64 image_url element");
    CHECK(sJson && strstr(sJson, "\"top_p\":0.5") && strstr(sJson, "\"stop\":\"END\"") &&
              strstr(sJson, "\"response_format\":{\"type\":\"json_object\"}"),
          "v3 sampling and JSON-mode fields serialized");
    CHECK(sJson && strstr(sJson, "\"stream\":") == NULL,
          "non-streaming request omits the stream field");
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
        "data: "
        "{\"choices\":[{\"index\":0,\"delta\":{\"content\":\"Hello\"},\"finish_reason\":null}]}\n\n"
        "data: "
        "{\"choices\":[{\"index\":0,\"delta\":{\"reasoning\":\"thinking\"},\"finish_reason\":null}]"
        "}\n\n"
        "data: {\"choices\":[{\"index\":0,\"delta\":{\"content\":\" "
        "world\",\"refusal\":null},\"finish_reason\":\"stop\"}]}\n\n"
        "data: "
        "{\"choices\":[],\"usage\":{\"prompt_tokens\":3,\"completion_tokens\":2,\"total_tokens\":5}"
        "}\n\n"
        "data: [DONE]\n\n";
    static const char sRefusal[] =
        "data: {\"choices\":[{\"index\":0,\"delta\":{\"refusal\":\"cannot "
        "help\"},\"finish_reason\":\"stop\"}]}\n\n"
        "data: [DONE]\n\n";
    xllm_call *pCall = (xllm_call *)calloc(1u, sizeof(*pCall));
    CHECK(pCall != NULL, "blocks parser call allocation");
    if (pCall)
    {
        pCall->pDialect = xllm__dialect_completions();
        pCall->uHttpStatus = 200u;
        pCall->bSse = true;
        pCall->bStreamWanted = true;
        pCall->sSelectedModel = xllm__strdup("gpt-test");
        CHECK(xllm__sse_feed(pCall, sSse, sizeof(sSse) - 1u) && xllm__sse_finish(pCall) &&
                  xllm__assemble_finalize(pCall),
              "interleaved stream decoded");
        CHECK(pCall->pResponse && pCall->pResponse->iBlockCount == 3u &&
                  pCall->pResponse->pBlocks[0].eKind == XLLM_BLOCK_TEXT &&
                  pCall->pResponse->pBlocks[1].eKind == XLLM_BLOCK_REASONING &&
                  pCall->pResponse->pBlocks[2].eKind == XLLM_BLOCK_TEXT,
              "arrival-order blocks recorded");
        CHECK(pCall->pResponse && strcmp(pCall->pResponse->sContent, "Hello world") == 0 &&
                  strcmp(pCall->pResponse->sReasoningContent, "thinking") == 0,
              "convenience fields joined");
        CHECK(pCall->pResponse && pCall->pResponse->eFinish == XLLM_FINISH_STOP &&
                  pCall->pResponse->tUsage.uTotalTokens == 5u,
              "finish and usage normalized");
        xllmCallDestroy(pCall);
    }
    pCall = (xllm_call *)calloc(1u, sizeof(*pCall));
    CHECK(pCall != NULL, "refusal parser call allocation");
    if (pCall)
    {
        pCall->pDialect = xllm__dialect_completions();
        pCall->uHttpStatus = 200u;
        pCall->bSse = true;
        pCall->bStreamWanted = true;
        CHECK(xllm__sse_feed(pCall, sRefusal, sizeof(sRefusal) - 1u) && xllm__sse_finish(pCall) &&
                  xllm__assemble_finalize(pCall),
              "refusal stream decoded");
        CHECK(pCall->pResponse && pCall->pResponse->sRefusal &&
                  strcmp(pCall->pResponse->sRefusal, "cannot help") == 0 &&
                  pCall->pResponse->eFinish == XLLM_FINISH_REFUSAL,
              "provider refusal surfaced");
        xllmCallDestroy(pCall);
    }
}

static void test_dialect_anthropic(void)
{
    xllm_client *pClient = test_make_client("http://127.0.0.1:1", XLLM_PROVIDER_ANTHROPIC);
    xllm_request tRequest;
    xllm_message tAssistant;
    xllm_error tError;
    char *sJson;
    static const char sSse[] =
        "event: message_start\n"
        "data: "
        "{\"type\":\"message_start\",\"message\":{\"usage\":{\"input_tokens\":11,\"cache_read_"
        "input_tokens\":3}}}\n\n"
        "event: content_block_start\n"
        "data: "
        "{\"type\":\"content_block_start\",\"index\":0,\"content_block\":{\"type\":\"thinking\"}}"
        "\n\n"
        "event: content_block_delta\n"
        "data: "
        "{\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"thinking_delta\","
        "\"thinking\":\"why\"}}\n\n"
        "event: content_block_delta\n"
        "data: "
        "{\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"signature_delta\","
        "\"signature\":\"sig0\"}}\n\n"
        "event: content_block_start\n"
        "data: "
        "{\"type\":\"content_block_start\",\"index\":1,\"content_block\":{\"type\":\"text\"}}\n\n"
        "event: content_block_delta\n"
        "data: "
        "{\"type\":\"content_block_delta\",\"index\":1,\"delta\":{\"type\":\"text_delta\",\"text\":"
        "\"Hi\"}}\n\n"
        "event: content_block_start\n"
        "data: "
        "{\"type\":\"content_block_start\",\"index\":2,\"content_block\":{\"type\":\"tool_use\","
        "\"id\":\"tu_1\",\"name\":\"read_file\"}}\n\n"
        "event: content_block_delta\n"
        "data: "
        "{\"type\":\"content_block_delta\",\"index\":2,\"delta\":{\"type\":\"input_json_delta\","
        "\"partial_json\":\"{\\\"pa\"}}\n\n"
        "event: content_block_delta\n"
        "data: "
        "{\"type\":\"content_block_delta\",\"index\":2,\"delta\":{\"type\":\"input_json_delta\","
        "\"partial_json\":\"th\\\":1}\"}}\n\n"
        "event: message_delta\n"
        "data: "
        "{\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"tool_use\"},\"usage\":{\"output_"
        "tokens\":4}}\n\n"
        "event: message_stop\n"
        "data: {\"type\":\"message_stop\"}\n\n";
    CHECK(pClient != NULL && strcmp(pClient->sTarget, "/v1/messages") == 0,
          "anthropic endpoint path resolves");
    xllmRequestInit(&tRequest);
    xllmMessageInit(&tAssistant, XLLM_ROLE_ASSISTANT);
    CHECK(xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_SYSTEM, "Be terse.") &&
              xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "Read the file.") &&
              xllmMessageAddToolCall(&tAssistant, "tu_0", "read_file", "{\"path\":\"a.c\"}") &&
              xllmRequestAddMessage(&tRequest, &tAssistant) &&
              xllmRequestAddToolResult(&tRequest, "tu_0", "file body") &&
              xllmRequestAddTool(&tRequest, "read_file", "Read one file", "{\"type\":\"object\"}",
                                 false) &&
              xllmRequestSetStop(&tRequest, "END"),
          "anthropic request inputs");
    sJson = xllmClientBuildRequestJson(pClient, &tRequest, &tError);
    CHECK(sJson != NULL && xrtJsonValid(xllm__sv(sJson)), "anthropic request JSON is valid");
    CHECK(sJson && strstr(sJson, "\"system\":\"Be terse.\"") &&
              strstr(sJson, "\"max_tokens\":131072"),
          "anthropic system lift and required max_tokens");
    CHECK(sJson && strstr(sJson, "\"tool_use\",\"id\":\"tu_0\"") &&
              strstr(sJson, "\"input\":{\"path\":\"a.c\"}"),
          "assistant tool calls map to tool_use blocks");
    CHECK(
        sJson &&
            strstr(sJson,
                   "{\"type\":\"tool_result\",\"tool_use_id\":\"tu_0\",\"content\":\"file body\"}"),
        "tool results map to tool_result blocks in a user message");
    CHECK(sJson && strstr(sJson, "\"input_schema\"") &&
              strstr(sJson, "\"tool_choice\":{\"type\":\"auto\"}") &&
              strstr(sJson, "\"stop_sequences\":[\"END\"]"),
          "anthropic tool schema, choice, and stop sequences");
    xllmFree(sJson);
    xllmMessageUnit(&tAssistant);
    xllmRequestUnit(&tRequest);
    {
        xllm_call *pCall = (xllm_call *)calloc(1u, sizeof(*pCall));
        CHECK(pCall != NULL, "anthropic parser call allocation");
        if (pCall)
        {
            pCall->pDialect = xllm__dialect_anthropic();
            pCall->uHttpStatus = 200u;
            pCall->bSse = true;
            pCall->bStreamWanted = true;
            pCall->sSelectedModel = xllm__strdup("claude-test");
            CHECK(xllm__sse_feed(pCall, sSse, sizeof(sSse) - 1u) && xllm__sse_finish(pCall) &&
                      xllm__assemble_finalize(pCall),
                  "anthropic typed event stream decoded");
            CHECK(pCall->pResponse && strcmp(pCall->pResponse->sContent, "Hi") == 0 &&
                      pCall->pResponse->iToolCallCount == 1u &&
                      strcmp(pCall->pResponse->pToolCalls[0].sArgumentsJson, "{\"path\":1}") == 0,
                  "anthropic text and tool arguments assembled");
            CHECK(pCall->pResponse && pCall->pResponse->iBlockCount == 3u &&
                      pCall->pResponse->pBlocks[0].eKind == XLLM_BLOCK_REASONING &&
                      pCall->pResponse->pBlocks[1].eKind == XLLM_BLOCK_TEXT &&
                      pCall->pResponse->pBlocks[2].eKind == XLLM_BLOCK_TOOL_CALL,
                  "anthropic block order recorded");
            CHECK(pCall->pResponse && pCall->pResponse->pBlocks[0].sNative &&
                      strcmp(pCall->pResponse->pBlocks[0].sNative, "sig0") == 0 &&
                      pCall->pResponse->sReasoningContent &&
                      strcmp(pCall->pResponse->sReasoningContent, "why") == 0,
                  "thinking signature captured into block metadata");
            {
                xllm_message tReplay;
                xllm_request tReplayRequest;
                char *sReplayJson;
                CHECK(xllmMessageFromResponse(pCall->pResponse, &tReplay) &&
                          tReplay.iPartCount == 3u &&
                          tReplay.pParts[0].eKind == XLLM_PART_REASONING &&
                          tReplay.pParts[1].eKind == XLLM_PART_NATIVE &&
                          strcmp(tReplay.pParts[1].sNativeType, "thinking_signature") == 0 &&
                          tReplay.pParts[2].eKind == XLLM_PART_TEXT &&
                          strcmp(tReplay.pParts[2].sText, "Hi") == 0,
                      "response preserves signed reasoning and visible text together");
                xllmRequestInit(&tReplayRequest);
                xllmRequestAddMessage(&tReplayRequest, &tReplay);
                sReplayJson = xllmClientBuildRequestJson(pClient, &tReplayRequest, &tError);
                CHECK(sReplayJson && xrtJsonValid(xllm__sv(sReplayJson)) &&
                          strstr(sReplayJson, "{\"type\":\"thinking\",\"thinking\":\"why\","
                                              "\"signature\":\"sig0\"}") &&
                          strstr(sReplayJson, "{\"type\":\"text\",\"text\":\"Hi\"}") &&
                          strstr(sReplayJson, "\"tool_use\",\"id\":\"tu_1\""),
                      "anthropic replays signed thinking, text and tool calls");
                xllmFree(sReplayJson);
                xllmRequestUnit(&tReplayRequest);
                xllmMessageUnit(&tReplay);
            }
            CHECK(pCall->pResponse && pCall->pResponse->eFinish == XLLM_FINISH_TOOL_CALLS &&
                      pCall->pResponse->tUsage.uInputTokens == 11u &&
                      pCall->pResponse->tUsage.uOutputTokens == 4u &&
                      pCall->pResponse->tUsage.uCachedInputTokens == 3u &&
                      pCall->pResponse->tUsage.uTotalTokens == 15u,
                  "anthropic usage and finish normalized");
            xllmCallDestroy(pCall);
        }
    }
    xllmClientDestroy(pClient);
}

static void test_dialect_responses(void)
{
    xllm_client *pClient =
        test_make_client("http://127.0.0.1:1/v1", XLLM_PROVIDER_OPENAI_RESPONSES);
    xllm_request tRequest;
    xllm_error tError;
    char *sJson;
    static const char sSse[] =
        "data: {\"type\":\"response.output_text.delta\",\"delta\":\"Hel\"}\n\n"
        "data: {\"type\":\"response.output_text.delta\",\"delta\":\"lo\"}\n\n"
        "data: {\"type\":\"response.reasoning_summary_text.delta\",\"delta\":\"ponder\"}\n\n"
        "data: "
        "{\"type\":\"response.output_item.added\",\"item\":{\"type\":\"function_call\",\"id\":\"fc_"
        "1\",\"call_id\":\"call_1\",\"name\":\"read_file\"}}\n\n"
        "data: "
        "{\"type\":\"response.function_call_arguments.delta\",\"item_id\":\"fc_1\",\"delta\":\"{"
        "\\\"x\\\":\"}\n\n"
        "data: "
        "{\"type\":\"response.function_call_arguments.delta\",\"item_id\":\"fc_1\",\"delta\":\"1}"
        "\"}\n\n"
        "data: "
        "{\"type\":\"response.completed\",\"response\":{\"usage\":{\"input_tokens\":7,\"output_"
        "tokens\":9,\"total_tokens\":16,\"input_tokens_details\":{\"cached_tokens\":2}}}}\n\n";
    CHECK(pClient != NULL && strcmp(pClient->sTarget, "/v1/responses") == 0,
          "responses endpoint path resolves");
    xllmRequestInit(&tRequest);
    CHECK(xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_SYSTEM, "Be terse.") &&
              xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "Hi") &&
              xllmRequestAddTool(&tRequest, "read_file", "Read one file", "{\"type\":\"object\"}",
                                 true) &&
              xllmRequestSetReasoningEffort(&tRequest, "low"),
          "responses request inputs");
    sJson = xllmClientBuildRequestJson(pClient, &tRequest, &tError);
    CHECK(sJson != NULL && xrtJsonValid(xllm__sv(sJson)), "responses request JSON is valid");
    CHECK(sJson && strstr(sJson, "\"instructions\":\"Be terse.\"") &&
              strstr(sJson, "\"max_output_tokens\":131072"),
          "responses instructions and max_output_tokens");
    CHECK(sJson && strstr(sJson, "\"input\":[") &&
              strstr(sJson, "{\"type\":\"message\",\"role\":\"user\",\"content\":\"Hi\"}"),
          "responses typed input items");
    CHECK(sJson && strstr(sJson, "\"reasoning\":{\"effort\":\"low\"}") &&
              strstr(sJson, "{\"type\":\"function\",\"name\":\"read_file\"") &&
              strstr(sJson, "\"strict\":true"),
          "responses reasoning effort and function tools");
    xllmFree(sJson);
    xllmRequestUnit(&tRequest);
    {
        xllm_call *pCall = (xllm_call *)calloc(1u, sizeof(*pCall));
        CHECK(pCall != NULL, "responses parser call allocation");
        if (pCall)
        {
            pCall->pDialect = xllm__dialect_responses();
            pCall->uHttpStatus = 200u;
            pCall->bSse = true;
            pCall->bStreamWanted = true;
            pCall->sSelectedModel = xllm__strdup("gpt-test");
            CHECK(xllm__sse_feed(pCall, sSse, sizeof(sSse) - 1u) && xllm__sse_finish(pCall) &&
                      xllm__assemble_finalize(pCall),
                  "responses typed event stream decoded");
            CHECK(pCall->pResponse && strcmp(pCall->pResponse->sContent, "Hello") == 0 &&
                      strcmp(pCall->pResponse->sReasoningContent, "ponder") == 0,
                  "responses text and reasoning assembled");
            CHECK(pCall->pResponse && pCall->pResponse->iToolCallCount == 1u &&
                      strcmp(pCall->pResponse->pToolCalls[0].sArgumentsJson, "{\"x\":1}") == 0 &&
                      strcmp(pCall->pResponse->pToolCalls[0].sName, "read_file") == 0,
                  "responses function_call assembled from item deltas");
            CHECK(pCall->pResponse && pCall->pResponse->tUsage.uTotalTokens == 16u &&
                      pCall->pResponse->tUsage.uCachedInputTokens == 2u && pCall->bDone,
                  "responses usage normalized and stream completed");
            xllmCallDestroy(pCall);
        }
    }
    xllmClientDestroy(pClient);
}

typedef struct test_concurrent_ctx
{
    xllm_client *pClient;
    xllm_result eResult;
    char sContent[32];
} test_concurrent_ctx;

static int32 test_concurrent_thread(ptr pData)
{
    test_concurrent_ctx *pCtx = (test_concurrent_ctx *)pData;
    xllm_request tRequest;
    xllm_response *pResponse = NULL;
    xllm_error tError;
    xllmRequestInit(&tRequest);
    if (!xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "Ping"))
    {
        return 1;
    }
    tRequest.pStablePrefixOwner = pCtx->pClient;
    tRequest.uStablePrefixStamp = 1u;
    tRequest.iStableMessages = 1u;
    pCtx->eResult = xllmClientComplete(pCtx->pClient, &tRequest, NULL, &pResponse, &tError);
    if (pCtx->eResult == XLLM_RESULT_OK && pResponse)
    {
        xllm__copy_text(pCtx->sContent, sizeof(pCtx->sContent),
                        pResponse->sContent ? pResponse->sContent : "");
    }
    xllmResponseDestroy(pResponse);
    xllmRequestUnit(&tRequest);
    return 0;
}

static bool test_wait_for_stale_pool_entry(test_server_ctx *pServer, xllm_client *pClient)
{
    uint64_t uDeadline = xllmDeadlineAfterUs(UINT64_C(500000));
    while (!xllmDeadlineExpired(uDeadline))
    {
        bool bClosed = false;
        if (xrtAtomic32Load(&pServer->bStaleConnectionClosed, XMEMORY_ACQUIRE) != 0u && pClient &&
            pClient->pConnectionMutex && xrtMutexLock(pClient->pConnectionMutex))
        {
            if (pClient->uIdleConnectionCount != 0u)
            {
                xllm_connection *pConnection =
                    pClient->pIdleConnections[pClient->uIdleConnectionCount - 1u];
                bClosed = pConnection->pIdleRead && xrtFutureDone(pConnection->pIdleRead);
            }
            (void)xrtMutexUnlock(pClient->pConnectionMutex);
        }
        if (bClosed)
            return true;
        mdo_test_sleep_ms(1u);
    }
    return false;
}

static void test_keepalive_and_concurrency(void)
{
    test_server_ctx tServerCtx = {0};
    char sKeepAliveUrl[256];
    char sJsonUrl[256];
    char sStaleUrl[256];
    xllm_client *pKeepAliveClient = NULL;
    xllm_client *pSharedClient = NULL;
    xllm_client *pStaleClient = NULL;
    xllm_request tRequest;
    xllm_response *pResponse = NULL;
    xllm_error tError;
    test_concurrent_ctx tWorkers[2];
    xthread *pThreads[2];
    CHECK(test_server_start(&tServerCtx), "reuse test server starts");
    if (!tServerCtx.pListener)
        goto cleanup;
    (void)snprintf(sKeepAliveUrl, sizeof(sKeepAliveUrl), "http://127.0.0.1:%u/keepalive/v1",
                   (unsigned)tServerCtx.uPort);
    (void)snprintf(sJsonUrl, sizeof(sJsonUrl), "http://127.0.0.1:%u/json/v1",
                   (unsigned)tServerCtx.uPort);
    (void)snprintf(sStaleUrl, sizeof(sStaleUrl), "http://127.0.0.1:%u/stale/v1",
                   (unsigned)tServerCtx.uPort);
    pKeepAliveClient = test_make_client(sKeepAliveUrl, XLLM_PROVIDER_OPENAI_COMPAT);
    CHECK(pKeepAliveClient != NULL, "keep-alive client created");
    xllmRequestInit(&tRequest);
    CHECK(xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "Ping"), "reuse request built");
    pResponse = NULL;
    CHECK(xllmClientComplete(pKeepAliveClient, &tRequest, NULL, &pResponse, &tError) ==
                  XLLM_RESULT_OK &&
              pResponse != NULL && !pResponse->tDiagnostics.bReusedConnection,
          "first keep-alive exchange opens a fresh connection");
    xllmResponseDestroy(pResponse);
    pResponse = NULL;
    CHECK(xllmClientComplete(pKeepAliveClient, &tRequest, NULL, &pResponse, &tError) ==
                  XLLM_RESULT_OK &&
              pResponse != NULL && pResponse->tDiagnostics.bReusedConnection,
          "second keep-alive exchange reuses the pooled connection");
    xllmResponseDestroy(pResponse);
    pResponse = NULL;
    pKeepAliveClient->uIdleTimeoutMs = 1u;
    mdo_test_sleep_ms(5u);
    CHECK(xllmClientComplete(pKeepAliveClient, &tRequest, NULL, &pResponse, &tError) ==
                  XLLM_RESULT_OK &&
              pResponse != NULL && !pResponse->tDiagnostics.bReusedConnection &&
              pResponse->tDiagnostics.uAttemptCount == 1u,
          "expired pooled connection is replaced before the next request");
    xllmResponseDestroy(pResponse);
    pResponse = NULL;
    CHECK(xrtAtomic32Load(&tServerCtx.iKeepaliveRequests, XMEMORY_RELAXED) == 3u,
          "server observed reuse followed by bounded idle expiry");
    xllmRequestUnit(&tRequest);
    xllmClientDestroy(pKeepAliveClient);
    pKeepAliveClient = NULL;

    pStaleClient = test_make_client(sStaleUrl, XLLM_PROVIDER_OPENAI_COMPAT);
    CHECK(pStaleClient != NULL, "stale recovery client created");
    xllmRequestInit(&tRequest);
    tRequest.bStream = false;
    CHECK(xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "Ping"),
          "stale recovery request built");
    CHECK(xllmClientComplete(pStaleClient, &tRequest, NULL, &pResponse, &tError) ==
                  XLLM_RESULT_OK &&
              pResponse != NULL && !pResponse->tDiagnostics.bReusedConnection,
          "first stale-route response enters the idle pool");
    xllmResponseDestroy(pResponse);
    pResponse = NULL;
    CHECK(test_wait_for_stale_pool_entry(&tServerCtx, pStaleClient),
          "server close becomes visible on the pooled stream");
    CHECK(xllmClientComplete(pStaleClient, &tRequest, NULL, &pResponse, &tError) ==
                  XLLM_RESULT_OK &&
              pResponse != NULL && !pResponse->tDiagnostics.bReusedConnection &&
              pResponse->tDiagnostics.uAttemptCount == 1u &&
              xrtAtomic32Load(&tServerCtx.iStaleRequests, XMEMORY_RELAXED) == 2u,
          "closed pooled stream is discarded before the next POST");
    xllmResponseDestroy(pResponse);
    pResponse = NULL;
    xllmRequestUnit(&tRequest);

    pSharedClient = test_make_client(sJsonUrl, XLLM_PROVIDER_OPENAI_COMPAT);
    CHECK(pSharedClient != NULL, "shared concurrent client created");
    memset(tWorkers, 0, sizeof(tWorkers));
    tWorkers[0].pClient = pSharedClient;
    tWorkers[1].pClient = pSharedClient;
    pThreads[0] = xrtThreadCreate(test_concurrent_thread, &tWorkers[0], 0u);
    pThreads[1] = xrtThreadCreate(test_concurrent_thread, &tWorkers[1], 0u);
    CHECK(pThreads[0] != NULL && pThreads[1] != NULL, "concurrent worker threads started");
    if (pThreads[0])
    {
        (void)xrtThreadWait(pThreads[0]);
        xrtThreadDestroy(pThreads[0]);
    }
    if (pThreads[1])
    {
        (void)xrtThreadWait(pThreads[1]);
        xrtThreadDestroy(pThreads[1]);
    }
    CHECK(tWorkers[0].eResult == XLLM_RESULT_OK && tWorkers[1].eResult == XLLM_RESULT_OK &&
              strcmp(tWorkers[0].sContent, "fallback") == 0 &&
              strcmp(tWorkers[1].sContent, "fallback") == 0,
          "two concurrent calls on one client both complete");
    {
        xcancel *pPreCancelled = xrtCancelCreate();
        xllm_call *pCall = NULL;
        xllm_result eResult = XLLM_RESULT_ERROR;
        uint32_t uRequestsBefore = xrtAtomic32Load(&tServerCtx.iRequests, XMEMORY_RELAXED);
        xllmRequestInit(&tRequest);
        (void)xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "Ping");
        xllmRequestSetCancel(&tRequest, pPreCancelled);
        CHECK(pPreCancelled && xrtCancelRequest(pPreCancelled),
              "pre-cancel token created for Start path");
        pCall = xllmClientStart(pSharedClient, &tRequest, NULL, &tError);
        if (pCall)
        {
            eResult = xllmCallWait(pCall, &pResponse, &tError);
        }
        CHECK(pCall && eResult == XLLM_RESULT_CANCELLED && pResponse == NULL &&
                  xrtAtomic32Load(&tServerCtx.iRequests, XMEMORY_RELAXED) == uRequestsBefore,
              "synchronous cancel-watch notification cannot deadlock or dial");
        xllmCallDestroy(pCall);
        xllmRequestUnit(&tRequest);
        xrtCancelDestroy(pPreCancelled);
    }
cleanup:
    xllmResponseDestroy(pResponse);
    xllmClientDestroy(pKeepAliveClient);
    xllmClientDestroy(pSharedClient);
    xllmClientDestroy(pStaleClient);
    test_server_stop(&tServerCtx);
}

/* ------------------------------------------------------------------ */
/* GAP-CACHE-HINT v2: store:false on the shared wire path (completions */
/* and Responses); Anthropic has no such field; extraBody override.     */
/* ------------------------------------------------------------------ */

static void test_wire_store_flags(void)
{
    static const char *sUrl = "http://127.0.0.1:9/v1";
    xllm_client_config tConfig;
    xllm_request tRequest;
    xllm_error tError;
    char *sJson;
    const xllm_provider aeProviders[3] = {XLLM_PROVIDER_OPENAI_COMPAT,
                                          XLLM_PROVIDER_OPENAI_RESPONSES, XLLM_PROVIDER_ANTHROPIC};
    size_t i;
    for (i = 0u; i < 3u; ++i)
    {
        xllm_client *pClient;
        xllmClientConfigInit(&tConfig);
        tConfig.sBaseUrl = sUrl;
        tConfig.sApiKey = "test-key";
        tConfig.sModel = "wire-model";
        tConfig.eProvider = aeProviders[i];
        pClient = xllmClientCreate(&tConfig, &tError);
        CHECK(pClient != NULL, "wire store client created");
        if (!pClient)
        {
            continue;
        }
        xllmRequestInit(&tRequest);
        tRequest.bStream = false;
        (void)xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "ping");
        sJson = xllmClientBuildRequestJson(pClient, &tRequest, &tError);
        if (aeProviders[i] == XLLM_PROVIDER_ANTHROPIC)
        {
            CHECK(sJson != NULL && strstr(sJson, "\"store\"") == NULL,
                  "anthropic body never carries a store field");
        }
        else
        {
            CHECK(sJson != NULL && strstr(sJson, "\"store\":false") != NULL,
                  "openai-compat and Responses bodies carry store:false");
        }
        xllmFree(sJson);
        /* caller override wins and must not duplicate the key */
        (void)xllmRequestSetExtraBody(&tRequest, "{\"store\":true,\"x\":1}");
        sJson = xllmClientBuildRequestJson(pClient, &tRequest, &tError);
        CHECK(sJson != NULL && strstr(sJson, "\"store\":true") != NULL &&
                  strstr(sJson, "\"store\":false") == NULL && strstr(sJson, "\"x\":1") != NULL,
              "extraBody store key overrides the wire default without duplication");
        xllmFree(sJson);
        xllmRequestUnit(&tRequest);
        xllmClientDestroy(pClient);
    }
}

/* ------------------------------------------------------------------ */
/* Lifecycle hooks: six mutable seams around one model call           */
/* ------------------------------------------------------------------ */

typedef struct
{
    int iRequestFires;
    int iBodyFires;
    int iRawFires;
    int iToolFires[2];
    int iResponseFires;
    int iRetryFires;
    bool bRejectRequest;
    bool bDropSecondTool;
    const char *sMarker;     /* appended as a system message */
    char *sBodySeen;         /* owned request body snapshot */
    const char *sRawReplace; /* canned replacement for the response body */
    const char *sRawPre;     /* canned body injected before send (offline) */
    const char *sVetoRetry;
} test_hook_state;

static bool test_hook_on_request(xllm_client *pClient, xllm_request *pRequest, void *pUserData)
{
    test_hook_state *pState = (test_hook_state *)pUserData;
    (void)pClient;
    ++pState->iRequestFires;
    if (pState->bRejectRequest)
    {
        return false;
    }
    if (pState->sMarker)
    {
        return xllmRequestAddTextMessage(pRequest, XLLM_ROLE_SYSTEM, pState->sMarker);
    }
    return true;
}

static bool test_hook_on_body(xllm_client *pClient, xllm_wire *pWire, void *pUserData)
{
    test_hook_state *pState = (test_hook_state *)pUserData;
    (void)pClient;
    ++pState->iBodyFires;
    if (!pState->sBodySeen)
    {
        pState->sBodySeen = xllm__strdup(pWire->sBody);
        if (!pState->sBodySeen)
        {
            return false;
        }
    }
    return true;
}

static bool test_hook_on_retry(xllm_client *pClient, const xllm_diagnostics *pDiagnostics,
                               uint32_t uNextAttempt, void *pUserData)
{
    test_hook_state *pState = (test_hook_state *)pUserData;
    (void)pClient;
    (void)pDiagnostics;
    (void)uNextAttempt;
    ++pState->iRetryFires;
    return pState->sVetoRetry == NULL;
}

static bool test_hook_on_raw(xllm_client *pClient, xllm_wire *pWire, void *pUserData)
{
    test_hook_state *pState = (test_hook_state *)pUserData;
    (void)pClient;
    if (pWire->sBody == NULL)
    {
        /* pre-send probe: assigning here replays offline (no dial) */
        if (pState->sRawPre)
        {
            size_t iLen = strlen(pState->sRawPre);
            pWire->sBody = xllm__strdup(pState->sRawPre);
            pWire->iBodySize = pWire->sBody ? iLen : 0u;
            return pWire->sBody != NULL;
        }
        return true;
    }
    ++pState->iRawFires;
    if (pState->sRawReplace)
    {
        /* contract: the library frees the old buffer when adopting */
        size_t iLen = strlen(pState->sRawReplace);
        char *sNew = xllm__strdup(pState->sRawReplace);
        if (!sNew)
        {
            return false;
        }
        pWire->sBody = sNew;
        pWire->iBodySize = iLen;
    }
    return true;
}

static bool test_hook_on_tool(xllm_client *pClient, xllm_tool_call *pCall, uint32_t uIndex,
                              void *pUserData)
{
    test_hook_state *pState = (test_hook_state *)pUserData;
    (void)pClient;
    if (uIndex < 2u)
    {
        ++pState->iToolFires[uIndex];
    }
    if (uIndex == 1u && pState->bDropSecondTool)
    {
        return false;
    }
    if (uIndex == 0u)
    {
        return xllm__replace(&pCall->sArgumentsJson, "{\"path\":\"hooked.c\"}");
    }
    return true;
}

static bool test_hook_on_response(xllm_client *pClient, xllm_response *pResponse, void *pUserData)
{
    test_hook_state *pState = (test_hook_state *)pUserData;
    (void)pClient;
    ++pState->iResponseFires;
    if (pResponse->sContent && strcmp(pResponse->sContent, "fallback") == 0)
    {
        return xllm__replace(&pResponse->sContent, "rewritten-by-hook");
    }
    return true;
}

static void test_lifecycle_hooks(void)
{
    test_server_ctx tServerCtx = {0};
    char sJsonUrl[256];
    char sSseUrl[256];
    char sTransientUrl[256];
    xllm_client *pClient = NULL;
    xllm_request tRequest;
    xllm_response *pResponse = NULL;
    xllm_error tError;
    xllm_hooks tHooks;
    test_hook_state tState = {0};
    CHECK(test_server_start(&tServerCtx), "hooks test server starts");
    if (!tServerCtx.pListener)
        goto cleanup;
    (void)snprintf(sJsonUrl, sizeof(sJsonUrl), "http://127.0.0.1:%u/json/v1",
                   (unsigned)tServerCtx.uPort);
    (void)snprintf(sSseUrl, sizeof(sSseUrl), "http://127.0.0.1:%u/v1", (unsigned)tServerCtx.uPort);
    (void)snprintf(sTransientUrl, sizeof(sTransientUrl), "http://127.0.0.1:%u/transient/v1",
                   (unsigned)tServerCtx.uPort);
    pClient = test_make_client(sJsonUrl, XLLM_PROVIDER_OPENAI_COMPAT);
    CHECK(pClient != NULL, "hooks client created");

    memset(&tState, 0, sizeof(tState));
    tState.sMarker = "you are modou";
    memset(&tHooks, 0, sizeof(tHooks));
    tHooks.pOnRequest = test_hook_on_request;
    tHooks.pOnRequestBody = test_hook_on_body;
    tHooks.pOnResponse = test_hook_on_response;
    tHooks.pUserData = &tState;
    {
        xllm_hooks tInstalled = tHooks;
        xllmClientSetHooks(pClient, &tInstalled);
        memset(&tInstalled, 0, sizeof(tInstalled));
    }

    /* 1) request mutation flows to the wire; response seam rewrites */
    xllmRequestInit(&tRequest);
    tRequest.bStream = false;
    (void)xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "ping");
    pResponse = NULL;
    CHECK(xllmClientComplete(pClient, &tRequest, NULL, &pResponse, &tError) == XLLM_RESULT_OK &&
              pResponse && strcmp(pResponse->sContent, "rewritten-by-hook") == 0,
          "onResponse rewrites the delivered content");
    xllmResponseDestroy(pResponse);
    pResponse = NULL;
    CHECK(tState.iRequestFires == 1 && tState.iBodyFires == 1 && tState.iResponseFires == 1 &&
              tState.sBodySeen && strstr(tState.sBodySeen, "you are modou") != NULL &&
              strstr(tState.sBodySeen, "\"store\":false") != NULL,
          "onRequest mutation reaches the serialized wire body");
    {
        int iResponseBefore = tState.iResponseFires;
        xllm_call *pCall = xllmClientStart(pClient, &tRequest, NULL, &tError);
        xllmClientSetHooks(pClient, NULL);
        pResponse = NULL;
        CHECK(pCall && xllmCallWait(pCall, &pResponse, &tError) == XLLM_RESULT_OK && pResponse &&
                  strcmp(pResponse->sContent, "rewritten-by-hook") == 0 &&
                  tState.iResponseFires == iResponseBefore + 1,
              "active call owns its hook snapshot across client updates");
        xllmResponseDestroy(pResponse);
        pResponse = NULL;
        xllmCallDestroy(pCall);
        xllmClientSetHooks(pClient, &tHooks);
    }
    xllmRequestUnit(&tRequest);

    /* 2) request rejection aborts with the hook error */
    tState.bRejectRequest = true;
    xllmRequestInit(&tRequest);
    tRequest.bStream = false;
    (void)xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "ping");
    pResponse = NULL;
    CHECK(xllmClientComplete(pClient, &tRequest, NULL, &pResponse, &tError) != XLLM_RESULT_OK &&
              pResponse == NULL && tError.eCode == XLLM_ERROR_HOOK,
          "onRequest rejection aborts the call");
    xllmRequestUnit(&tRequest);
    tState.bRejectRequest = false;

    /* 3) per-request hooks replace the client-level set */
    {
        xllm_hooks tOwn;
        memset(&tOwn, 0, sizeof(tOwn));
        tOwn.pOnRequest = test_hook_on_request;
        tOwn.pUserData = &tState;
        xllmRequestInit(&tRequest);
        tRequest.bStream = false;
        tRequest.pHooks = &tOwn;
        (void)xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "ping");
        {
            int iBeforeBody = tState.iBodyFires;
            int iBeforeResponse = tState.iResponseFires;
            pResponse = NULL;
            CHECK(xllmClientComplete(pClient, &tRequest, NULL, &pResponse, &tError) ==
                      XLLM_RESULT_OK,
                  "request-level hooks call succeeds");
            xllmResponseDestroy(pResponse);
            pResponse = NULL;
            CHECK(tState.iBodyFires == iBeforeBody && tState.iResponseFires == iBeforeResponse,
                  "request-level hooks replace the client set for that call");
        }
        xllmRequestUnit(&tRequest);
    }

    /* 4) response-body replacement on a non-streaming call (replay) */
    tHooks.pOnResponseBody = test_hook_on_raw;
    xllmClientSetHooks(pClient, &tHooks);
    tState.sRawReplace =
        "{\"id\":\"r\",\"model\":\"m\",\"choices\":[{\"index\":0,\"message\":"
        "{\"role\":\"assistant\",\"content\":\"replayed-body\"},\"finish_reason\":\"stop\"}],"
        "\"usage\":{\"prompt_tokens\":1,\"completion_tokens\":1,\"total_tokens\":2}}";
    xllmRequestInit(&tRequest);
    tRequest.bStream = false;
    (void)xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "ping");
    pResponse = NULL;
    CHECK(xllmClientComplete(pClient, &tRequest, NULL, &pResponse, &tError) == XLLM_RESULT_OK &&
              pResponse && strcmp(pResponse->sContent, "replayed-body") == 0 &&
              tState.iRawFires == 1,
          "response-body replacement rewrites the parsed result");
    xllmResponseDestroy(pResponse);
    pResponse = NULL;
    tState.sRawReplace = NULL;
    xllmRequestUnit(&tRequest);

    /* 5) streaming: armed body seam defers parsing; replacement replays SSE */
    {
        xllm_client *pSseClient = test_make_client(sSseUrl, XLLM_PROVIDER_OPENAI_COMPAT);
        tHooks.pOnRequest = NULL;
        tHooks.pOnResponse = NULL;
        xllmClientSetHooks(pSseClient, &tHooks);
        tState.sRawPre =
            "data: "
            "{\"choices\":[{\"index\":0,\"delta\":{\"content\":\"replayed-sse\"},\"finish_reason\":"
            "null}]}\n\n"
            "data: "
            "{\"choices\":[{\"index\":0,\"delta\":{},\"finish_reason\":\"stop\"}],\"usage\":{"
            "\"prompt_tokens\":2,\"completion_tokens\":2,\"total_tokens\":4}}\n\n"
            "data: [DONE]\n\n";
        xllmRequestInit(&tRequest);
        (void)xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "ping");
        pResponse = NULL;
        CHECK(pSseClient &&
                  xllmClientComplete(pSseClient, &tRequest, NULL, &pResponse, &tError) ==
                      XLLM_RESULT_OK &&
                  pResponse && strcmp(pResponse->sContent, "replayed-sse") == 0,
              "offline pre-send injection replays canned SSE without a dial");
        if (pSseClient)
        {
            xllmResponseDestroy(pResponse);
            pResponse = NULL;
            xllmRequestUnit(&tRequest);
            xllmClientDestroy(pSseClient);
        }
        else
        {
            xllmRequestUnit(&tRequest);
        }
        tState.sRawPre = NULL;
    }

    /* 6) tool seams on the interleaved SSE route: rewrite args, drop #2 */
    {
        xllm_client *pSseClient = test_make_client(sSseUrl, XLLM_PROVIDER_OPENAI_COMPAT);
        tHooks.pOnRequest = NULL;
        tHooks.pOnResponseBody = NULL;
        tHooks.pOnToolCall = test_hook_on_tool;
        xllmClientSetHooks(pSseClient, &tHooks);
        tState.bDropSecondTool = true;
        xllmRequestInit(&tRequest);
        (void)xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "ping");
        pResponse = NULL;
        CHECK(pSseClient &&
                  xllmClientComplete(pSseClient, &tRequest, NULL, &pResponse, &tError) ==
                      XLLM_RESULT_OK &&
                  pResponse && pResponse->iToolCallCount == 1 &&
                  strcmp(pResponse->pToolCalls[0].sArgumentsJson, "{\"path\":\"hooked.c\"}") == 0 &&
                  strcmp(pResponse->pToolCalls[0].sName, "read_file") == 0 &&
                  pResponse->tDiagnostics.bToolCallDropped,
              "tool seam rewrites arguments and drops the second call");
        if (pSseClient)
        {
            xllmResponseDestroy(pResponse);
            pResponse = NULL;
            xllmRequestUnit(&tRequest);
            xllmClientDestroy(pSseClient);
        }
        else
        {
            xllmRequestUnit(&tRequest);
        }
        tState.bDropSecondTool = false;
    }

    /* 7) retry veto on the transient route: one attempt only */
    {
        xllm_client *pTransient = test_make_client(sTransientUrl, XLLM_PROVIDER_OPENAI_COMPAT);
        tHooks.pOnToolCall = NULL;
        tHooks.pOnRetry = test_hook_on_retry;
        xllmClientSetHooks(pTransient, &tHooks);
        tState.sVetoRetry = "yes";
        xllmRequestInit(&tRequest);
        tRequest.bStream = false;
        (void)xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "ping");
        pResponse = NULL;
        CHECK(pTransient &&
                  xllmClientComplete(pTransient, &tRequest, NULL, &pResponse, &tError) !=
                      XLLM_RESULT_OK &&
                  pResponse == NULL && tState.iRetryFires == 1 &&
                  xrtAtomic32Load(&tServerCtx.iTransientRequests, XMEMORY_RELAXED) == 1u,
              "retry veto stops after the first failed attempt");
        if (pTransient)
        {
            xllmRequestUnit(&tRequest);
            xllmClientDestroy(pTransient);
        }
        else
        {
            xllmRequestUnit(&tRequest);
        }
        tState.sVetoRetry = NULL;
    }

cleanup:
    xllmClientDestroy(pClient);
    test_server_stop(&tServerCtx);
    xllm__free(tState.sBodySeen);
}

/* ------------------------------------------------------------------ */
/* History container                                                   */
/* ------------------------------------------------------------------ */

static void test_history_container(void)
{
    xllm_history *pHistory = xllmHistoryCreate();
    xllm_request tRequest;
    xllm_response tResponse;
    xllm_tool_call tCall;
    char *sOwned;
    CHECK(pHistory != NULL, "history creates");
    CHECK(xllmHistoryAddText(pHistory, XLLM_ROLE_USER, "hello agent"), "history add text");
    sOwned = xllm__strdup("assistant says");
    CHECK(sOwned && xllmHistoryAddText(pHistory, XLLM_ROLE_ASSISTANT, sOwned),
          "history add owned text");
    xllm__free(sOwned);
    CHECK(strcmp(xllmHistoryAt(pHistory, 1u)->sContent, "assistant says") == 0,
          "history deep-copies its messages");

    memset(&tCall, 0, sizeof(tCall));
    tCall.sId = (char *)"call_hist";
    tCall.sName = (char *)"read_file";
    tCall.sArgumentsJson = (char *)"{\"path\":\"x.c\"}";
    memset(&tResponse, 0, sizeof(tResponse));
    tResponse.sContent = (char *)"reading";
    tResponse.pToolCalls = &tCall;
    tResponse.iToolCallCount = 1u;
    CHECK(xllmHistoryAddFromResponse(pHistory, &tResponse), "history add from response");
    CHECK(xllmHistoryAddToolResult(pHistory, "call_hist", "file bytes"), "history add tool result");
    CHECK(xllmHistoryCount(pHistory) == 4u && xllmHistoryAt(pHistory, 2u)->iToolCallCount == 1u &&
              xllmHistoryAt(pHistory, 3u)->eRole == XLLM_ROLE_TOOL,
          "history keeps the tool pair shape");

    xllmRequestInit(&tRequest);
    CHECK(xllmHistoryAppendInto(pHistory, &tRequest) && tRequest.iMessageCount == 4u &&
              strcmp(tRequest.pMessages[1].sContent, "assistant says") == 0,
          "history appends into a request");
    xllmRequestUnit(&tRequest);

    CHECK(xllmHistoryRemove(pHistory, 0u, 2u) && xllmHistoryCount(pHistory) == 2u,
          "history remove range");
    CHECK(!xllmHistoryRemove(pHistory, 5u, 1u), "history remove out of range rejected");
    xllmHistoryDestroy(pHistory);
    printf("  history container ok\n");
}

static long g_iFailAt = -1;
static size_t g_iTestAllocs;
static size_t g_iTestLiveAllocs;
static bool g_bSawOomError;
static bool g_bTestAllocatorUnderflow;

static void *test_fault_alloc(size_t iSize)
{
    void *pMemory;
    ++g_iTestAllocs;
    if ((long)g_iTestAllocs == g_iFailAt)
    {
        return NULL;
    }
    pMemory = malloc(iSize);
    if (pMemory)
        ++g_iTestLiveAllocs;
    return pMemory;
}
static void *test_fault_realloc(void *pMemory, size_t iSize)
{
    void *pNew;
    ++g_iTestAllocs;
    if ((long)g_iTestAllocs == g_iFailAt)
    {
        return NULL;
    }
    pNew = realloc(pMemory, iSize);
    if (pNew && !pMemory)
        ++g_iTestLiveAllocs;
    return pNew;
}
static void test_fault_free(void *pMemory)
{
    if (pMemory)
    {
        if (g_iTestLiveAllocs)
            --g_iTestLiveAllocs;
        else
            g_bTestAllocatorUnderflow = true;
    }
    free(pMemory);
}

static void test_signed_response(void)
{
    char sText[] = "Answer \"one\"\n", sSig[] = "opaque/one+=";
    const char *sTail = "\xe4\xba\x8c\\\t\x01";
    xllm_block tBlocks[5] = {0};
    xllm_tool_call tTool = {"call-1", "read", "{\"path\":\"a.c\"}"};
    xllm_response tResponse = {0};
    xllm_message tMessage;
    xllm_request tRequest;
    xllm_history *pHistory = xllmHistoryCreate();
    xllm_error tError;
    size_t i;
    bool bConverted;
    tBlocks[0].eKind = XLLM_BLOCK_REASONING;
    tBlocks[0].sText = "first";
    tBlocks[0].sNative = sSig;
    tBlocks[1].eKind = XLLM_BLOCK_TEXT;
    tBlocks[1].sText = sText;
    tBlocks[2].eKind = XLLM_BLOCK_REASONING;
    tBlocks[2].sText = "second";
    tBlocks[2].sNative = "opaque/two+=";
    tBlocks[3].eKind = XLLM_BLOCK_TEXT;
    tBlocks[3].sText = (char *)sTail;
    tBlocks[4].eKind = XLLM_BLOCK_REASONING;
    tBlocks[4].sText = "";
    tBlocks[4].sNative = "opaque/empty+=";
    tResponse.sContent = "Answer \"one\"\n\xe4\xba\x8c\\\t\x01";
    tResponse.sReasoningContent = "firstsecond";
    tResponse.pBlocks = tBlocks;
    tResponse.iBlockCount = 5u;
    tResponse.pToolCalls = &tTool;
    tResponse.iToolCallCount = 1u;
    bConverted = xllmMessageFromResponse(&tResponse, &tMessage);
    CHECK(bConverted && !tMessage.sContent && tMessage.iPartCount == 8u &&
              strcmp(tMessage.pParts[0].sText, "first") == 0 &&
              strcmp(tMessage.pParts[1].sText, sSig) == 0 &&
              strcmp(tMessage.pParts[2].sText, sText) == 0 &&
              strcmp(tMessage.pParts[3].sText, "second") == 0 &&
              strcmp(tMessage.pParts[4].sText, "opaque/two+=") == 0 &&
              strcmp(tMessage.pParts[5].sText, sTail) == 0 && !tMessage.pParts[6].sText[0] &&
              strcmp(tMessage.pParts[7].sText, "opaque/empty+=") == 0 &&
              tMessage.iToolCallCount == 1u && strcmp(tMessage.pToolCalls[0].sId, "call-1") == 0,
          "signed response owns separate thinking/text blocks, empty thinking and tools");
    CHECK(pHistory && xllmHistoryAddFromResponse(pHistory, &tResponse),
          "history uses the same signed response conversion");
    sSig[0] = 'X';
    sText[0] = 'X';
    CHECK(bConverted && strcmp(tMessage.pParts[1].sText, "opaque/one+=") == 0 && pHistory &&
              xllmHistoryCount(pHistory) == 1u &&
              strcmp(xllmHistoryAt(pHistory, 0u)->pParts[2].sText, "Answer \"one\"\n") == 0,
          "converted messages do not borrow mutable response buffers");
    xllmRequestInit(&tRequest);
    CHECK(bConverted && xllmRequestAddMessage(&tRequest, &tMessage),
          "signed response becomes an owned request");
    for (i = 0u; i < 4u; ++i)
    {
        static const xllm_provider tProviders[] = {XLLM_PROVIDER_ANTHROPIC,
                                                   XLLM_PROVIDER_OPENAI_COMPAT, XLLM_PROVIDER_GLM,
                                                   XLLM_PROVIDER_OPENAI_RESPONSES};
        xllm_client *pClient = test_make_client("http://127.0.0.1:1/v1", tProviders[i]);
        char *sJson = pClient ? xllmClientBuildRequestJson(pClient, &tRequest, &tError) : NULL;
        CHECK(sJson && xrtJsonValid(xllm__sv(sJson)),
              "signed history serializes valid JSON on every dialect");
        if (i == 0u)
        {
            const char *pFirst = sJson ? strstr(sJson, "{\"type\":\"thinking\",\"thinking\":"
                                                       "\"first\",\"signature\":\"opaque/one+=\"}")
                                       : NULL,
                       *pText =
                           sJson ? strstr(sJson,
                                          "{\"type\":\"text\",\"text\":\"Answer \\\"one\\\"\\n\"}")
                                 : NULL,
                       *pSecond = sJson
                                      ? strstr(sJson, "{\"type\":\"thinking\",\"thinking\":"
                                                      "\"second\",\"signature\":\"opaque/two+=\"}")
                                      : NULL;
            CHECK(pFirst && pText && pSecond && pFirst < pText && pText < pSecond &&
                      strstr(sJson, "{\"type\":\"thinking\",\"thinking\":\"\",\"signature\":"
                                    "\"opaque/empty+=\"}") &&
                      strstr(sJson, "\"tool_use\",\"id\":\"call-1\""),
                  "thinking signatures retain original text, emptiness and block order");
        }
        else
        {
            CHECK(sJson && !strstr(sJson, "opaque/") && strstr(sJson, "Answer \\\"one\\\"\\n") &&
                      strstr(sJson, "\\\\\\t\\u0001"),
                  "switching dialect preserves escaped answer and excludes foreign signatures");
            if (i == 2u)
                CHECK(sJson && strstr(sJson, "\"reasoning_content\":\"firstsecond\"") &&
                          strstr(sJson, "\"content\":\"Answer \\\"one\\\"\\n"),
                      "GLM keeps reasoning separate from visible answer");
        }
        xllmFree(sJson);
        xllmClientDestroy(pClient);
    }
    xllmRequestUnit(&tRequest);
    xllmMessageUnit(&tMessage);
    xllmHistoryDestroy(pHistory);
    tResponse.pBlocks = NULL;
    tResponse.iBlockCount = 0u;
    tResponse.iToolCallCount = 0u;
    CHECK(xllmMessageFromResponse(&tResponse, &tMessage) && !tMessage.iPartCount &&
              strcmp(tMessage.sContent, tResponse.sContent) == 0,
          "unsigned response keeps its compact text representation");
    {
        xllm_client *pClient = test_make_client("http://127.0.0.1:1/v1", XLLM_PROVIDER_ANTHROPIC);
        char *sJson;
        xllmRequestInit(&tRequest);
        (void)xllmRequestAddMessage(&tRequest, &tMessage);
        sJson = pClient ? xllmClientBuildRequestJson(pClient, &tRequest, &tError) : NULL;
        CHECK(sJson && xrtJsonValid(xllm__sv(sJson)),
              "plain assistant text closes its Anthropic content object");
        xllmFree(sJson);
        xllmClientDestroy(pClient);
        xllmRequestUnit(&tRequest);
    }
    xllmMessageUnit(&tMessage);
    tResponse.pBlocks = tBlocks;
    tResponse.iBlockCount = 1u;
    CHECK(xllmMessageFromResponse(&tResponse, &tMessage) && tMessage.iPartCount == 3u &&
              strcmp(tMessage.pParts[2].sText, tResponse.sContent) == 0,
          "signed custom response without text blocks retains its joined answer");
    xllmMessageUnit(&tMessage);
    {
        xllm_client *pClient =
            test_make_client("http://127.0.0.1:1/v1", XLLM_PROVIDER_OPENAI_RESPONSES);
        char *sJson;
        tResponse.sContent = NULL;
        (void)xllmMessageFromResponse(&tResponse, &tMessage);
        xllmRequestInit(&tRequest);
        (void)xllmRequestAddMessage(&tRequest, &tMessage);
        sJson = pClient ? xllmClientBuildRequestJson(pClient, &tRequest, &tError) : NULL;
        CHECK(sJson && xrtJsonValid(xllm__sv(sJson)) && strstr(sJson, "\"content\":\"\"") &&
                  !strstr(sJson, "opaque/"),
              "signature-only history maps to empty text on another dialect");
        xllmFree(sJson);
        xllmClientDestroy(pClient);
        xllmRequestUnit(&tRequest);
        xllmMessageUnit(&tMessage);
        tResponse.sContent = "Answer \"one\"\n\xe4\xba\x8c\\\t\x01";
    }
    tResponse.pBlocks = NULL;
    xllmMessageInit(&tMessage, XLLM_ROLE_USER);
    CHECK(!xllmMessageFromResponse(&tResponse, &tMessage) && tMessage.eRole == XLLM_ROLE_USER &&
              !tMessage.pParts,
          "missing response block array fails without touching output");
    tResponse.iBlockCount = 0u;
    tResponse.pToolCalls = NULL;
    tResponse.iToolCallCount = 1u;
    CHECK(!xllmMessageFromResponse(&tResponse, &tMessage) && !tMessage.pToolCalls,
          "missing response tool array fails safely");
    xllmMessageUnit(&tMessage);
    tResponse.pToolCalls = &tTool;
    /* Every allocation in the converter must unwind its owned output. */
    {
        xllm_allocator tFault = {test_fault_alloc, test_fault_realloc, test_fault_free};
        size_t iAllocations;
        bool bClean = true;
        tResponse.pBlocks = tBlocks;
        tResponse.iBlockCount = 5u;
        tResponse.iToolCallCount = 1u;
        g_iFailAt = -1;
        g_iTestAllocs = g_iTestLiveAllocs = 0u;
        g_bTestAllocatorUnderflow = false;
        xllm__set_allocator(&tFault);
        bClean = xllmMessageFromResponse(&tResponse, &tMessage);
        iAllocations = g_iTestAllocs;
        xllmMessageUnit(&tMessage);
        for (i = 1u; i <= iAllocations; ++i)
        {
            g_iTestAllocs = 0u;
            g_iFailAt = (long)i;
            if (xllmMessageFromResponse(&tResponse, &tMessage) || tMessage.pParts ||
                tMessage.sContent || tMessage.sReasoningContent || tMessage.pToolCalls ||
                tMessage.iPartCount || tMessage.iToolCallCount || g_iTestLiveAllocs)
                bClean = false;
            g_iFailAt = -1;
            xllmMessageUnit(&tMessage);
        }
        xllm__set_allocator(NULL);
        CHECK(bClean && iAllocations && !g_iTestLiveAllocs && !g_bTestAllocatorUnderflow,
              "every signed response allocation failure leaves an empty owned output");
    }
}

typedef bool (*test_part_data_setter)(xllm_part *, const void *, size_t, const char *);

static bool test_part_setter_oom_case(test_part_data_setter pSetter)
{
    static const unsigned char tData[] = {1u, 2u, 3u, 4u};
    long k;
    for (k = 1; k <= 2; ++k)
    {
        xllm_part tPart;
        size_t iBaseline;
        bool bOk;
        g_iFailAt = -1;
        xllmPartInit(&tPart, XLLM_PART_TEXT);
        if (!xllmPartSetText(&tPart, "stable"))
            return false;
        iBaseline = g_iTestLiveAllocs;
        g_iTestAllocs = 0u;
        g_iFailAt = k;
        bOk = pSetter(&tPart, tData, sizeof(tData), "application/octet-stream");
        if (bOk || tPart.eKind != XLLM_PART_TEXT || !tPart.sText ||
            strcmp(tPart.sText, "stable") != 0 || g_iTestLiveAllocs != iBaseline)
        {
            g_iFailAt = -1;
            xllmPartUnit(&tPart);
            return false;
        }
        g_iFailAt = -1;
        xllmPartUnit(&tPart);
        if (g_iTestLiveAllocs != 0u)
            return false;
    }
    return true;
}

static void test_public_mutator_oom(void)
{
    xllm_allocator tFault = {test_fault_alloc, test_fault_realloc, test_fault_free};
    bool bPartAtomic = true;
    bool bMessageAtomic = true;
    bool bToolsAtomic = true;
    bool bHistoryAtomic = true;
    long k;
    g_iFailAt = -1;
    g_iTestAllocs = 0u;
    g_iTestLiveAllocs = 0u;
    g_bTestAllocatorUnderflow = false;
    xllm__set_allocator(&tFault);

    bPartAtomic = test_part_setter_oom_case(xllmPartSetImageData) &&
                  test_part_setter_oom_case(xllmPartSetAudioData) &&
                  test_part_setter_oom_case(xllmPartSetFileData);
    for (k = 1; k <= 2 && bPartAtomic; ++k)
    {
        xllm_part tPart;
        size_t iBaseline;
        bool bOk;
        xllmPartInit(&tPart, XLLM_PART_TEXT);
        bPartAtomic = xllmPartSetText(&tPart, "stable");
        iBaseline = g_iTestLiveAllocs;
        g_iTestAllocs = 0u;
        g_iFailAt = k;
        bOk = xllmPartSetImageUrl(&tPart, "https://example.invalid/a.png", "image/png");
        bPartAtomic = bPartAtomic && !bOk && tPart.eKind == XLLM_PART_TEXT && tPart.sText &&
                      strcmp(tPart.sText, "stable") == 0 && g_iTestLiveAllocs == iBaseline;
        g_iFailAt = -1;
        xllmPartUnit(&tPart);
        bPartAtomic = bPartAtomic && g_iTestLiveAllocs == 0u;
    }
    for (k = 1; k <= 2 && bPartAtomic; ++k)
    {
        xllm_part tPart;
        size_t iBaseline;
        bool bOk;
        xllmPartInit(&tPart, XLLM_PART_TEXT);
        bPartAtomic = xllmPartSetText(&tPart, "stable");
        iBaseline = g_iTestLiveAllocs;
        g_iTestAllocs = 0u;
        g_iFailAt = k;
        bOk = xllmPartSetNative(&tPart, "provider_block", "{\"value\":1}");
        bPartAtomic = bPartAtomic && !bOk && tPart.eKind == XLLM_PART_TEXT && tPart.sText &&
                      strcmp(tPart.sText, "stable") == 0 && g_iTestLiveAllocs == iBaseline;
        g_iFailAt = -1;
        xllmPartUnit(&tPart);
        bPartAtomic = bPartAtomic && g_iTestLiveAllocs == 0u;
    }
    {
        xllm_part tPart;
        size_t iBaseline;
        xllmPartInit(&tPart, XLLM_PART_NATIVE);
        bPartAtomic = bPartAtomic && xllmPartSetNative(&tPart, "provider_block", "{\"old\":true}");
        iBaseline = g_iTestLiveAllocs;
        g_iTestAllocs = 0u;
        g_iFailAt = 1;
        bPartAtomic = bPartAtomic && !xllmPartSetText(&tPart, "replacement") &&
                      tPart.eKind == XLLM_PART_NATIVE && tPart.sText &&
                      strcmp(tPart.sText, "{\"old\":true}") == 0 && g_iTestLiveAllocs == iBaseline;
        g_iFailAt = -1;
        xllmPartUnit(&tPart);
        bPartAtomic = bPartAtomic && g_iTestLiveAllocs == 0u;
    }
    {
        static const unsigned char tData[] = {7u, 8u};
        xllm_part tPart;
        xllmPartInit(&tPart, XLLM_PART_IMAGE);
        bPartAtomic = bPartAtomic &&
                      xllmPartSetImageData(&tPart, tData, sizeof(tData), "image/png") &&
                      xllmPartSetFileData(&tPart, NULL, 0u, "application/octet-stream") &&
                      tPart.eKind == XLLM_PART_FILE && tPart.pData == NULL && tPart.iDataSize == 0u;
        xllmPartUnit(&tPart);
        bPartAtomic = bPartAtomic && g_iTestLiveAllocs == 0u;
    }

    {
        xllm_part tPart;
        xllm_message tMessage;
        size_t iBaseline;
        xllmPartInit(&tPart, XLLM_PART_TEXT);
        xllmMessageInit(&tMessage, XLLM_ROLE_USER);
        bMessageAtomic = xllmPartSetText(&tPart, "part") && xllmMessageAddPart(&tMessage, &tPart);
        xllmPartUnit(&tPart);
        iBaseline = g_iTestLiveAllocs;
        g_iTestAllocs = 0u;
        g_iFailAt = 1;
        bMessageAtomic =
            bMessageAtomic && !xllmMessageSetContent(&tMessage, "replacement") &&
            tMessage.iPartCount == 1u && tMessage.sContent == NULL && tMessage.pParts[0].sText &&
            strcmp(tMessage.pParts[0].sText, "part") == 0 && g_iTestLiveAllocs == iBaseline;
        g_iFailAt = -1;
        xllmMessageUnit(&tMessage);
        bMessageAtomic = bMessageAtomic && g_iTestLiveAllocs == 0u;
    }

    for (k = 1; k <= 2 && bToolsAtomic; ++k)
    {
        xllm_request tRequest;
        xllm_tool tBorrowed;
        size_t iBaseline;
        bool bOk;
        memset(&tBorrowed, 0, sizeof(tBorrowed));
        tBorrowed.sName = (char *)"borrowed";
        tBorrowed.sDescription = (char *)"borrowed description";
        tBorrowed.sParametersJson = (char *)"{\"type\":\"object\"}";
        xllmRequestInit(&tRequest);
        bToolsAtomic = xllmRequestAddTool(&tRequest, "owned", "old", "{\"type\":\"object\"}", true);
        iBaseline = g_iTestLiveAllocs;
        g_iTestAllocs = 0u;
        g_iFailAt = k;
        bOk = xllmRequestSetToolsView(&tRequest, &tBorrowed, 1u);
        bToolsAtomic = bToolsAtomic && !bOk && tRequest.iToolCount == 1u &&
                       tRequest.pTools[0].sName && strcmp(tRequest.pTools[0].sName, "owned") == 0 &&
                       g_iTestLiveAllocs == iBaseline;
        g_iFailAt = -1;
        xllmRequestUnit(&tRequest);
        bToolsAtomic = bToolsAtomic && g_iTestLiveAllocs == 0u;
    }

    for (k = 1; k <= 2 && bHistoryAtomic; ++k)
    {
        xllm_history *pHistory;
        xllm_request tRequest;
        size_t iBaseline;
        bool bOk;
        pHistory = xllmHistoryCreate();
        xllmRequestInit(&tRequest);
        bHistoryAtomic = pHistory && xllmHistoryAddText(pHistory, XLLM_ROLE_USER, "one") &&
                         xllmHistoryAddText(pHistory, XLLM_ROLE_ASSISTANT, "two") &&
                         xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_SYSTEM, "seed");
        iBaseline = g_iTestLiveAllocs;
        g_iTestAllocs = 0u;
        g_iFailAt = k;
        bOk = xllmHistoryAppendInto(pHistory, &tRequest);
        bHistoryAtomic = bHistoryAtomic && !bOk && tRequest.iMessageCount == 1u &&
                         tRequest.pMessages[0].sContent &&
                         strcmp(tRequest.pMessages[0].sContent, "seed") == 0 &&
                         g_iTestLiveAllocs == iBaseline;
        g_iFailAt = -1;
        xllmRequestUnit(&tRequest);
        xllmHistoryDestroy(pHistory);
        bHistoryAtomic = bHistoryAtomic && g_iTestLiveAllocs == 0u;
    }

    g_iFailAt = -1;
    xllm__set_allocator(NULL);
    CHECK(bPartAtomic, "part setters preserve prior value at every allocation failure");
    CHECK(bMessageAtomic, "message content replacement is failure-atomic");
    CHECK(bToolsAtomic, "borrowed tool-table replacement is failure-atomic");
    CHECK(bHistoryAtomic, "history append rolls back partial request copies");
    CHECK(!g_bTestAllocatorUnderflow && g_iTestLiveAllocs == 0u,
          "public mutator fault cases release every test allocation once");
}

static bool test_hook_reject_body(xllm_client *pClient, xllm_wire *pWire, void *pUserData)
{
    (void)pClient;
    (void)pWire;
    (void)pUserData;
    return false;
}

static void test_public_client_oom(void)
{
    static const char sOfflineSse[] =
        "data: "
        "{\"choices\":[{\"index\":0,\"delta\":{\"content\":\"offline\"},\"finish_reason\":null}]}"
        "\n\n"
        "data: "
        "{\"choices\":[{\"index\":0,\"delta\":{},\"finish_reason\":\"stop\"}],\"usage\":{\"prompt_"
        "tokens\":1,\"completion_tokens\":1,\"total_tokens\":2}}\n\n"
        "data: [DONE]\n\n";
    xllm_allocator tFault = {test_fault_alloc, test_fault_realloc, test_fault_free};
    xnetengineconfig tEngineConfig;
    xnetengine *pEngine = NULL;
    xllm_client_config tConfig;
    xllm_error tError;
    size_t iCreateAllocs = 0u;
    bool bCreateSafe = true;
    bool bProfileSafe = true;
    bool bStartSafe = true;
    bool bWaitSafe = true;
    bool bHookRejectSafe = true;
    long k;

    g_iFailAt = -1;
    g_iTestAllocs = 0u;
    g_iTestLiveAllocs = 0u;
    g_bTestAllocatorUnderflow = false;
    xllm__set_allocator(&tFault);
    xrtNetEngineConfigInit(&tEngineConfig);
    pEngine = xrtNetEngineCreate(&tEngineConfig);
    if (!pEngine || !xrtNetEngineStart(pEngine))
    {
        bCreateSafe = bProfileSafe = bStartSafe = bWaitSafe = bHookRejectSafe = false;
        goto done;
    }

    xllmClientConfigInit(&tConfig);
    tConfig.sBaseUrl = "https://model.example.invalid/v1";
    tConfig.sModel = "oom-model";
    tConfig.eProvider = XLLM_PROVIDER_OPENAI_COMPAT;
    tConfig.pNetEngine = pEngine;
    tConfig.eProxyKind = XLLM_PROXY_HTTP_CONNECT;
    tConfig.sProxyHost = "127.0.0.1";
    tConfig.uProxyPort = 8080u;
    tConfig.sProxyUser = "oom-user";
    tConfig.sProxyPass = "oom-pass";
    tConfig.sProxyBypass = "localhost;*.internal";
    g_iTestAllocs = 0u;
    {
        xllm_client *pClient = xllmClientCreate(&tConfig, &tError);
        iCreateAllocs = g_iTestAllocs;
        bCreateSafe = pClient != NULL && iCreateAllocs != 0u;
        xllmClientDestroy(pClient);
        bCreateSafe = bCreateSafe && g_iTestLiveAllocs == 0u;
    }
    for (k = 1; k <= (long)iCreateAllocs && bCreateSafe; ++k)
    {
        xllm_client *pClient;
        g_iTestAllocs = 0u;
        g_iFailAt = k;
        pClient = xllmClientCreate(&tConfig, &tError);
        bCreateSafe =
            pClient == NULL && tError.eCode == XLLM_ERROR_OUT_OF_MEMORY && g_iTestLiveAllocs == 0u;
        g_iFailAt = -1;
        xllmClientDestroy(pClient);
    }

    if (bProfileSafe)
    {
        xllm_client *pClient;
        xllm_model_profile tProfile;
        xllm_model_profile tSnapshot;
        size_t iBaseline;
        g_iFailAt = -1;
        pClient = xllmClientCreate(&tConfig, &tError);
        xllmModelProfileInit(&tProfile);
        tProfile.sId = "oom-profile";
        tProfile.sModel = "oom-model";
        tProfile.eProvider = XLLM_PROVIDER_OPENAI_COMPAT;
        tProfile.eWindowMode = XLLM_WINDOW_SHARED_CONTEXT;
        tProfile.uCapabilities = XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT | XLLM_CAP_STREAM;
        tProfile.uContextWindowTokens = 8192u;
        tProfile.uMaxInputTokens = 8192u;
        tProfile.uMaxOutputTokens = 1024u;
        iBaseline = g_iTestLiveAllocs;
        g_iTestAllocs = 0u;
        g_iFailAt = 1;
        bProfileSafe = pClient && !xllmClientSetModelProfile(pClient, &tProfile, &tError) &&
                       tError.eCode == XLLM_ERROR_OUT_OF_MEMORY &&
                       !xllmClientGetModelProfile(pClient, &tSnapshot) &&
                       g_iTestLiveAllocs == iBaseline;
        if (!bProfileSafe)
        {
            printf("    profile failure error=%s live=%llu baseline=%llu\n",
                   xllmErrorCodeName(tError.eCode), (unsigned long long)g_iTestLiveAllocs,
                   (unsigned long long)iBaseline);
        }
        g_iFailAt = -1;
        xllmClientDestroy(pClient);
        bProfileSafe = bProfileSafe && g_iTestLiveAllocs == 0u;
    }

    if (bStartSafe)
    {
        xllm_client *pClient;
        xllm_request tRequest;
        xllm_hooks tHooks;
        test_hook_state tHookState;
        xcancel *pCancel;
        size_t iStartAllocs;
        size_t iBaseline;
        xllm_call *pCall;
        xllm_response *pResponse = NULL;
        xllm_result eResult;
        memset(&tHooks, 0, sizeof(tHooks));
        memset(&tHookState, 0, sizeof(tHookState));
        tHooks.pOnRequest = test_hook_on_request;
        tHooks.pUserData = &tHookState;
        g_iFailAt = -1;
        pClient = xllmClientCreate(&tConfig, &tError);
        pCancel = xrtCancelCreate();
        xllmRequestInit(&tRequest);
        bStartSafe = pClient && pCancel && xrtCancelRequest(pCancel) &&
                     xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "start") &&
                     xllmRequestAddTool(&tRequest, "read_file", "Read a file",
                                        "{\"type\":\"object\"}", true);
        xllmRequestSetCancel(&tRequest, pCancel);
        tRequest.pHooks = &tHooks;
        iBaseline = g_iTestLiveAllocs;
        g_iTestAllocs = 0u;
        pCall = bStartSafe ? xllmClientStart(pClient, &tRequest, NULL, &tError) : NULL;
        iStartAllocs = g_iTestAllocs;
        eResult = pCall ? xllmCallWait(pCall, &pResponse, &tError) : XLLM_RESULT_ERROR;
        bStartSafe = bStartSafe && pCall && iStartAllocs != 0u &&
                     eResult == XLLM_RESULT_CANCELLED && pResponse == NULL;
        if (!bStartSafe)
        {
            printf("    start baseline call=%p allocs=%llu result=%d error=%s live=%llu "
                   "baseline=%llu\n",
                   (void *)pCall, (unsigned long long)iStartAllocs, (int)eResult,
                   xllmErrorCodeName(tError.eCode), (unsigned long long)g_iTestLiveAllocs,
                   (unsigned long long)iBaseline);
        }
        xllmCallDestroy(pCall);
        bStartSafe = bStartSafe && g_iTestLiveAllocs == iBaseline;
        for (k = 1; k <= (long)iStartAllocs && bStartSafe; ++k)
        {
            g_iTestAllocs = 0u;
            g_iFailAt = k;
            pCall = xllmClientStart(pClient, &tRequest, NULL, &tError);
            bStartSafe = pCall == NULL && tError.eCode == XLLM_ERROR_OUT_OF_MEMORY;
            if (!bStartSafe)
            {
                printf("    start failpoint=%ld call=%p error=%s live=%llu baseline=%llu\n", k,
                       (void *)pCall, xllmErrorCodeName(tError.eCode),
                       (unsigned long long)g_iTestLiveAllocs, (unsigned long long)iBaseline);
            }
            g_iFailAt = -1;
            xllmCallDestroy(pCall);
            bStartSafe = bStartSafe && g_iTestLiveAllocs == iBaseline;
            if (!bStartSafe)
            {
                printf("    start cleanup failpoint=%ld live=%llu baseline=%llu\n", k,
                       (unsigned long long)g_iTestLiveAllocs, (unsigned long long)iBaseline);
            }
        }
        xllmRequestUnit(&tRequest);
        xrtCancelDestroy(pCancel);
        xllmClientDestroy(pClient);
        bStartSafe = bStartSafe && g_iTestLiveAllocs == 0u;
    }

    if (bWaitSafe)
    {
        xllm_client *pClient;
        xllm_request tRequest;
        xllm_hooks tHooks;
        test_hook_state tHookState;
        size_t iWaitAllocs;
        size_t iBaseline;
        xllm_call *pCall;
        xllm_response *pResponse = NULL;
        xllm_result eResult;
        memset(&tHooks, 0, sizeof(tHooks));
        memset(&tHookState, 0, sizeof(tHookState));
        tHookState.sRawPre = sOfflineSse;
        tHooks.pOnResponseBody = test_hook_on_raw;
        tHooks.pUserData = &tHookState;
        g_iFailAt = -1;
        pClient = xllmClientCreate(&tConfig, &tError);
        xllmRequestInit(&tRequest);
        tRequest.pHooks = &tHooks;
        bWaitSafe = pClient && xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "wait");
        iBaseline = g_iTestLiveAllocs;
        pCall = bWaitSafe ? xllmClientStart(pClient, &tRequest, NULL, &tError) : NULL;
        g_iTestAllocs = 0u;
        eResult = pCall ? xllmCallWait(pCall, &pResponse, &tError) : XLLM_RESULT_ERROR;
        iWaitAllocs = g_iTestAllocs;
        bWaitSafe = bWaitSafe && pCall && eResult == XLLM_RESULT_OK && pResponse &&
                    pResponse->sContent && strcmp(pResponse->sContent, "offline") == 0 &&
                    iWaitAllocs != 0u;
        if (!bWaitSafe)
        {
            printf("    wait baseline call=%p allocs=%llu result=%d error=%s response=%p live=%llu "
                   "baseline=%llu\n",
                   (void *)pCall, (unsigned long long)iWaitAllocs, (int)eResult,
                   xllmErrorCodeName(tError.eCode), (void *)pResponse,
                   (unsigned long long)g_iTestLiveAllocs, (unsigned long long)iBaseline);
        }
        xllmResponseDestroy(pResponse);
        pResponse = NULL;
        xllmCallDestroy(pCall);
        bWaitSafe = bWaitSafe && g_iTestLiveAllocs == iBaseline;
        for (k = 1; k <= (long)iWaitAllocs && bWaitSafe; ++k)
        {
            g_iFailAt = -1;
            pCall = xllmClientStart(pClient, &tRequest, NULL, &tError);
            g_iTestAllocs = 0u;
            g_iFailAt = k;
            eResult = pCall ? xllmCallWait(pCall, &pResponse, &tError) : XLLM_RESULT_ERROR;
            bWaitSafe = pCall && eResult == XLLM_RESULT_ERROR && pResponse == NULL &&
                        tError.eCode == XLLM_ERROR_OUT_OF_MEMORY;
            if (!bWaitSafe)
            {
                printf("    wait failpoint=%ld call=%p result=%d error=%s response=%p live=%llu "
                       "baseline=%llu\n",
                       k, (void *)pCall, (int)eResult, xllmErrorCodeName(tError.eCode),
                       (void *)pResponse, (unsigned long long)g_iTestLiveAllocs,
                       (unsigned long long)iBaseline);
            }
            g_iFailAt = -1;
            xllmResponseDestroy(pResponse);
            pResponse = NULL;
            xllmCallDestroy(pCall);
            bWaitSafe = bWaitSafe && g_iTestLiveAllocs == iBaseline;
            if (!bWaitSafe)
            {
                printf("    wait cleanup failpoint=%ld live=%llu baseline=%llu\n", k,
                       (unsigned long long)g_iTestLiveAllocs, (unsigned long long)iBaseline);
            }
        }
        xllmRequestUnit(&tRequest);
        xllmClientDestroy(pClient);
        bWaitSafe = bWaitSafe && g_iTestLiveAllocs == 0u;
    }

    if (bHookRejectSafe)
    {
        xllm_client *pClient;
        xllm_request tRequest;
        xllm_hooks tHooks;
        size_t iBaseline;
        xllm_call *pCall;
        memset(&tHooks, 0, sizeof(tHooks));
        tHooks.pOnRequestBody = test_hook_reject_body;
        g_iFailAt = -1;
        pClient = xllmClientCreate(&tConfig, &tError);
        xllmRequestInit(&tRequest);
        tRequest.pHooks = &tHooks;
        bHookRejectSafe = pClient && xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "reject");
        iBaseline = g_iTestLiveAllocs;
        pCall = bHookRejectSafe ? xllmClientStart(pClient, &tRequest, NULL, &tError) : NULL;
        bHookRejectSafe = bHookRejectSafe && !pCall && tError.eCode == XLLM_ERROR_HOOK &&
                          g_iTestLiveAllocs == iBaseline;
        if (!bHookRejectSafe)
        {
            printf("    hook reject call=%p error=%s live=%llu baseline=%llu\n", (void *)pCall,
                   xllmErrorCodeName(tError.eCode), (unsigned long long)g_iTestLiveAllocs,
                   (unsigned long long)iBaseline);
        }
        xllmCallDestroy(pCall);
        xllmRequestUnit(&tRequest);
        xllmClientDestroy(pClient);
        bHookRejectSafe = bHookRejectSafe && g_iTestLiveAllocs == 0u;
    }

done:
    g_iFailAt = -1;
    if (pEngine)
    {
        (void)xrtNetEngineStop(pEngine);
        (void)xrtNetEngineDestroy(pEngine);
    }
    xllm__set_allocator(NULL);
    if (g_bTestAllocatorUnderflow || g_iTestLiveAllocs != 0u)
    {
        printf("    allocator final live=%llu underflow=%d\n",
               (unsigned long long)g_iTestLiveAllocs, g_bTestAllocatorUnderflow ? 1 : 0);
    }
    CHECK(bCreateSafe, "client create safely unwinds every xllm allocation failure");
    CHECK(bProfileSafe, "profile replacement preserves client state on allocation failure");
    CHECK(bStartSafe, "call start safely unwinds request snapshots and call state");
    CHECK(bWaitSafe, "offline parse safely unwinds every response allocation failure");
    CHECK(bHookRejectSafe, "request-body hook rejection releases the serialized body");
    CHECK(!g_bTestAllocatorUnderflow && g_iTestLiveAllocs == 0u,
          "public client fault cases release every test allocation once");
}

typedef struct
{
    const char *sName;
    const char *sBaseUrl;
    xllm_provider eProvider;
    const char *sFixture;
    bool bStream;
} test_adapter_oom_case;

static bool test_adapter_response_ok(xllm_provider eProvider, const xllm_response *pResponse)
{
    if (!pResponse || !pResponse->sContent || !pResponse->sReasoningContent ||
        pResponse->iToolCallCount != 1u || !pResponse->pToolCalls[0].sName ||
        !pResponse->pToolCalls[0].sArgumentsJson)
    {
        return false;
    }
    if (strcmp(pResponse->pToolCalls[0].sName, "read_file") != 0)
    {
        return false;
    }
    switch (eProvider)
    {
    case XLLM_PROVIDER_ANTHROPIC:
        return strcmp(pResponse->sContent, "Hi") == 0 &&
               strcmp(pResponse->sReasoningContent, "why") == 0 &&
               strcmp(pResponse->pToolCalls[0].sArgumentsJson, "{\"path\":1}") == 0 &&
               pResponse->iBlockCount == 3u &&
               pResponse->pBlocks[0].eKind == XLLM_BLOCK_REASONING &&
               pResponse->pBlocks[0].sNative && strcmp(pResponse->pBlocks[0].sNative, "sig0") == 0;
    case XLLM_PROVIDER_OPENAI_RESPONSES:
        return strcmp(pResponse->sContent, "Hello") == 0 &&
               strcmp(pResponse->sReasoningContent, "ponder") == 0 &&
               strcmp(pResponse->pToolCalls[0].sArgumentsJson, "{\"x\":1}") == 0 &&
               pResponse->iBlockCount == 3u;
    default:
        return strcmp(pResponse->sContent, "Hello") == 0 &&
               strcmp(pResponse->sReasoningContent, "think") == 0 &&
               strcmp(pResponse->pToolCalls[0].sArgumentsJson, "{\"path\":1}") == 0 &&
               pResponse->iBlockCount == 3u;
    }
}

static bool test_adapter_oom_one(xnetengine *pEngine, const test_adapter_oom_case *pCase)
{
    xllm_client_config tConfig;
    xllm_client *pClient = NULL;
    xllm_request tRequest;
    xllm_hooks tHooks;
    test_hook_state tHookState;
    xllm_error tError;
    xllm_call *pCall = NULL;
    xllm_response *pResponse = NULL;
    xllm_result eResult;
    size_t iBaseline = 0u;
    size_t iWaitAllocs = 0u;
    long k;
    bool bOk = true;

    memset(&tHooks, 0, sizeof(tHooks));
    memset(&tHookState, 0, sizeof(tHookState));
    tHookState.sRawPre = pCase->sFixture;
    tHooks.pOnResponseBody = test_hook_on_raw;
    tHooks.pUserData = &tHookState;
    xllmClientConfigInit(&tConfig);
    tConfig.sBaseUrl = pCase->sBaseUrl;
    tConfig.sModel = "oom-adapter-model";
    tConfig.eProvider = pCase->eProvider;
    tConfig.pNetEngine = pEngine;
    tConfig.uTimeoutMs = 250u;
    tConfig.uMaxAttempts = 1u;
    g_iFailAt = -1;
    pClient = xllmClientCreate(&tConfig, &tError);
    xllmRequestInit(&tRequest);
    tRequest.bStream = pCase->bStream;
    tRequest.pHooks = &tHooks;
    bOk = pClient && xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "decode complex fixture");
    iBaseline = g_iTestLiveAllocs;

    pCall = bOk ? xllmClientStart(pClient, &tRequest, NULL, &tError) : NULL;
    bOk = bOk && pCall && xllm__atomic_load(&pCall->iTerminal) == 1;
    g_iTestAllocs = 0u;
    eResult = pCall ? xllmCallWait(pCall, &pResponse, &tError) : XLLM_RESULT_ERROR;
    iWaitAllocs = g_iTestAllocs;
    bOk = bOk && pCall && eResult == XLLM_RESULT_OK &&
          test_adapter_response_ok(pCase->eProvider, pResponse) && iWaitAllocs != 0u;
    if (!bOk)
    {
        printf("    %s baseline call=%p allocs=%llu result=%d error=%s response=%p\n", pCase->sName,
               (void *)pCall, (unsigned long long)iWaitAllocs, (int)eResult,
               xllmErrorCodeName(tError.eCode), (void *)pResponse);
    }
    xllmResponseDestroy(pResponse);
    pResponse = NULL;
    xllmCallDestroy(pCall);
    pCall = NULL;
    bOk = bOk && g_iTestLiveAllocs == iBaseline;

    for (k = 1; k <= (long)iWaitAllocs && bOk; ++k)
    {
        g_iFailAt = -1;
        pCall = xllmClientStart(pClient, &tRequest, NULL, &tError);
        g_iTestAllocs = 0u;
        g_iFailAt = k;
        eResult = pCall ? xllmCallWait(pCall, &pResponse, &tError) : XLLM_RESULT_ERROR;
        bOk = pCall && eResult == XLLM_RESULT_ERROR && pResponse == NULL &&
              tError.eCode == XLLM_ERROR_OUT_OF_MEMORY;
        if (!bOk)
        {
            printf("    %s failpoint=%ld call=%p result=%d error=%s response=%p live=%llu "
                   "baseline=%llu\n",
                   pCase->sName, k, (void *)pCall, (int)eResult, xllmErrorCodeName(tError.eCode),
                   (void *)pResponse, (unsigned long long)g_iTestLiveAllocs,
                   (unsigned long long)iBaseline);
        }
        g_iFailAt = -1;
        xllmResponseDestroy(pResponse);
        pResponse = NULL;
        xllmCallDestroy(pCall);
        pCall = NULL;
        bOk = bOk && g_iTestLiveAllocs == iBaseline;
        if (!bOk)
        {
            printf("    %s cleanup failpoint=%ld live=%llu baseline=%llu\n", pCase->sName, k,
                   (unsigned long long)g_iTestLiveAllocs, (unsigned long long)iBaseline);
        }
    }

    g_iFailAt = -1;
    xllmResponseDestroy(pResponse);
    xllmCallDestroy(pCall);
    xllmRequestUnit(&tRequest);
    xllmClientDestroy(pClient);
    return bOk && g_iTestLiveAllocs == 0u;
}

static void test_adapter_assembly_oom(void)
{
    static const char sCompletions[] =
        "data: "
        "{\"choices\":[{\"index\":0,\"delta\":{\"content\":\"Hello\"},\"finish_reason\":null}]}\n\n"
        "data: "
        "{\"choices\":[{\"index\":0,\"delta\":{\"reasoning\":\"think\"},\"finish_reason\":null}]}"
        "\n\n"
        "data: "
        "{\"choices\":[{\"index\":0,\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"tc_1\","
        "\"function\":{\"name\":\"read_file\",\"arguments\":\"{\\\"pa\"}}]},\"finish_reason\":null}"
        "]}\n\n"
        "data: "
        "{\"choices\":[{\"index\":0,\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{"
        "\"arguments\":\"th\\\":1}\"}}]},\"finish_reason\":\"tool_calls\"}],\"usage\":{\"prompt_"
        "tokens\":3,\"completion_tokens\":4,\"total_tokens\":7}}\n\n"
        "data: [DONE]\n\n";
    static const char sAnthropic[] =
        "data: "
        "{\"type\":\"message_start\",\"message\":{\"usage\":{\"input_tokens\":11,\"cache_read_"
        "input_tokens\":3}}}\n\n"
        "data: "
        "{\"type\":\"content_block_start\",\"index\":0,\"content_block\":{\"type\":\"thinking\"}}"
        "\n\n"
        "data: "
        "{\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"thinking_delta\","
        "\"thinking\":\"why\"}}\n\n"
        "data: "
        "{\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"signature_delta\","
        "\"signature\":\"sig0\"}}\n\n"
        "data: "
        "{\"type\":\"content_block_start\",\"index\":1,\"content_block\":{\"type\":\"text\"}}\n\n"
        "data: "
        "{\"type\":\"content_block_delta\",\"index\":1,\"delta\":{\"type\":\"text_delta\",\"text\":"
        "\"Hi\"}}\n\n"
        "data: "
        "{\"type\":\"content_block_start\",\"index\":2,\"content_block\":{\"type\":\"tool_use\","
        "\"id\":\"tu_1\",\"name\":\"read_file\"}}\n\n"
        "data: "
        "{\"type\":\"content_block_delta\",\"index\":2,\"delta\":{\"type\":\"input_json_delta\","
        "\"partial_json\":\"{\\\"pa\"}}\n\n"
        "data: "
        "{\"type\":\"content_block_delta\",\"index\":2,\"delta\":{\"type\":\"input_json_delta\","
        "\"partial_json\":\"th\\\":1}\"}}\n\n"
        "data: "
        "{\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"tool_use\"},\"usage\":{\"output_"
        "tokens\":4}}\n\n"
        "data: {\"type\":\"message_stop\"}\n\n";
    static const char sResponses[] =
        "data: {\"type\":\"response.output_text.delta\",\"delta\":\"Hel\"}\n\n"
        "data: {\"type\":\"response.output_text.delta\",\"delta\":\"lo\"}\n\n"
        "data: {\"type\":\"response.reasoning_summary_text.delta\",\"delta\":\"ponder\"}\n\n"
        "data: "
        "{\"type\":\"response.output_item.added\",\"item\":{\"type\":\"function_call\",\"id\":\"fc_"
        "1\",\"call_id\":\"call_1\",\"name\":\"read_file\"}}\n\n"
        "data: "
        "{\"type\":\"response.function_call_arguments.delta\",\"item_id\":\"fc_1\",\"delta\":\"{"
        "\\\"x\\\":\"}\n\n"
        "data: "
        "{\"type\":\"response.function_call_arguments.delta\",\"item_id\":\"fc_1\",\"delta\":\"1}"
        "\"}\n\n"
        "data: "
        "{\"type\":\"response.completed\",\"response\":{\"usage\":{\"input_tokens\":7,\"output_"
        "tokens\":9,\"total_tokens\":16,\"input_tokens_details\":{\"cached_tokens\":2}}}}\n\n";
    static const char sCompletionsJson[] =
        "{\"id\":\"chat_1\",\"model\":\"fixture-model\",\"choices\":[{\"index\":0,"
        "\"message\":{\"content\":\"Hello\",\"reasoning\":\"think\",\"tool_calls\":[{"
        "\"id\":\"tc_1\",\"function\":{\"name\":\"read_file\",\"arguments\":\"{\\\"path\\\":1}\"}}]"
        "},"
        "\"finish_reason\":\"tool_calls\"}],\"usage\":{\"prompt_tokens\":3,"
        "\"completion_tokens\":4,\"total_tokens\":7}}";
    static const char sAnthropicJson[] =
        "{\"id\":\"msg_1\",\"model\":\"fixture-model\",\"content\":["
        "{\"type\":\"thinking\",\"thinking\":\"why\",\"signature\":\"sig0\"},"
        "{\"type\":\"text\",\"text\":\"Hi\"},{\"type\":\"tool_use\","
        "\"id\":\"tu_1\",\"name\":\"read_file\",\"input\":{\"path\":1}}],"
        "\"stop_reason\":\"tool_use\",\"usage\":{\"input_tokens\":11,"
        "\"output_tokens\":4,\"cache_read_input_tokens\":3}}";
    static const char sResponsesJson[] =
        "{\"id\":\"resp_1\",\"model\":\"fixture-model\",\"status\":\"completed\","
        "\"output\":[{\"type\":\"message\",\"content\":[{\"type\":\"output_text\","
        "\"text\":\"Hello\"}]},{\"type\":\"reasoning\",\"summary\":[{"
        "\"type\":\"summary_text\",\"text\":\"ponder\"}]},{\"type\":\"function_call\","
        "\"call_id\":\"call_1\",\"name\":\"read_file\",\"arguments\":\"{\\\"x\\\":1}\"}],"
        "\"usage\":{\"input_tokens\":7,\"output_tokens\":9,\"total_tokens\":16,"
        "\"input_tokens_details\":{\"cached_tokens\":2}}}";
    static const test_adapter_oom_case atCases[] = {
        {"completions SSE", "https://model.example.invalid/v1", XLLM_PROVIDER_OPENAI_COMPAT,
         sCompletions, true},
        {"anthropic SSE", "https://model.example.invalid", XLLM_PROVIDER_ANTHROPIC, sAnthropic,
         true},
        {"responses SSE", "https://model.example.invalid/v1", XLLM_PROVIDER_OPENAI_RESPONSES,
         sResponses, true},
        {"completions JSON", "https://model.example.invalid/v1", XLLM_PROVIDER_OPENAI_COMPAT,
         sCompletionsJson, false},
        {"anthropic JSON", "https://model.example.invalid", XLLM_PROVIDER_ANTHROPIC, sAnthropicJson,
         false},
        {"responses JSON", "https://model.example.invalid/v1", XLLM_PROVIDER_OPENAI_RESPONSES,
         sResponsesJson, false}};
    xllm_allocator tFault = {test_fault_alloc, test_fault_realloc, test_fault_free};
    xnetengineconfig tEngineConfig;
    xnetengine *pEngine;
    bool abOk[sizeof(atCases) / sizeof(atCases[0])] = {false};
    bool bBounds;
    size_t i;

    g_iFailAt = -1;
    g_iTestAllocs = 0u;
    g_iTestLiveAllocs = 0u;
    g_bTestAllocatorUnderflow = false;
    xllm__set_allocator(&tFault);
    {
        xllm_buf tBuf = {0};
        xllm_call tCall;
        char *sText = NULL;
        size_t iTextLen = SIZE_MAX;
        memset(&tCall, 0, sizeof(tCall));
        xllmErrorInit(&tCall.tError);
        tBuf.iLen = SIZE_MAX;
        bBounds = !xllm__buf_append(&tBuf, "", 0u) &&
                  !xllm__append_tracked(&sText, &iTextLen, "", 0u) && sText == NULL &&
                  iTextLen == SIZE_MAX && !xllm__assemble_set_block_tool(&tCall, SIZE_MAX, 0u) &&
                  tCall.tError.eCode == XLLM_ERROR_OUT_OF_MEMORY && tCall.pBlockMap == NULL;
    }
    xrtNetEngineConfigInit(&tEngineConfig);
    pEngine = xrtNetEngineCreate(&tEngineConfig);
    if (pEngine && xrtNetEngineStart(pEngine))
    {
        for (i = 0u; i < sizeof(atCases) / sizeof(atCases[0]); ++i)
        {
            abOk[i] = test_adapter_oom_one(pEngine, &atCases[i]);
        }
    }
    g_iFailAt = -1;
    if (pEngine)
    {
        (void)xrtNetEngineStop(pEngine);
        (void)xrtNetEngineDestroy(pEngine);
    }
    xllm__set_allocator(NULL);
    CHECK(abOk[0], "completions complex assembly propagates and unwinds every OOM");
    CHECK(abOk[1], "anthropic complex assembly propagates and unwinds every OOM");
    CHECK(abOk[2], "responses complex assembly propagates and unwinds every OOM");
    CHECK(abOk[3], "completions JSON assembly propagates and unwinds every OOM");
    CHECK(abOk[4], "anthropic JSON assembly propagates and unwinds every OOM");
    CHECK(abOk[5], "responses JSON assembly propagates and unwinds every OOM");
    CHECK(bBounds, "assembly growth rejects size overflow before allocation");
    CHECK(!g_bTestAllocatorUnderflow && g_iTestLiveAllocs == 0u,
          "adapter assembly fault cases release every test allocation once");
}

static void test_oom_injection(void)
{
    xllm_client *pClient = test_make_client("http://127.0.0.1:1/v1", XLLM_PROVIDER_OPENAI_COMPAT);
    xllm_request tRequest;
    xllm_message tMessage;
    xllm_error tError;
    xllm_allocator tFault = {test_fault_alloc, test_fault_realloc, test_fault_free};
    char *sJson;
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
              xllmRequestSetExtraBody(&tRequest, "{\"x\":1}"),
          "oom request inputs");
    xllm__set_allocator(&tFault);
    for (k = 1; k <= 96; ++k)
    {
        g_iFailAt = k;
        g_iTestAllocs = 0u;
        g_bSawOomError = false;
        sJson = xllmClientBuildRequestJson(pClient, &tRequest, &tError);
        if (sJson)
        {
            ++iSucceeded;
            CHECK(xrtJsonValid(xllm__sv(sJson)) || k == 0, "oom survival keeps JSON valid");
            xllmFree(sJson);
        }
        else
        {
            ++iFailed;
            if (tError.eCode != XLLM_ERROR_OUT_OF_MEMORY)
            {
                g_bSawOomError = true;
            }
        }
    }
    g_iFailAt = -1;
    xllm__set_allocator(NULL);
    CHECK(iFailed > 0 && iSucceeded > 0, "allocation faults observed across the build path");
    CHECK(!g_bSawOomError, "every injected failure maps to an out-of-memory error");
    sJson = xllmClientBuildRequestJson(pClient, &tRequest, &tError);
    CHECK(sJson != NULL && xrtJsonValid(xllm__sv(sJson)),
          "default allocator restored after injection");
    xllmFree(sJson);
    xllmMessageUnit(&tMessage);
    xllmRequestUnit(&tRequest);
    xllmClientDestroy(pClient);
}

#include "fixtures/xllm_tls_ca_identity.h"
#include "fixtures/xllm_tls_identity.h"

typedef struct test_tls_server_ctx
{
    xnetengine *pEngine;
    xtlslistener *pListener;
    xthread *pThread;
    xcancel *pCancel;
    uint16_t uPort;
    int iRequests;
} test_tls_server_ctx;

static bool test_tls_send_all(xtlsstream *pStream, const void *pData, size_t iSize)
{
    const uint8_t *p = (const uint8_t *)pData;
    size_t iOffset = 0u;
    while (iOffset < iSize)
    {
        size_t iChunk = iSize - iOffset;
        xfuture *pFuture;
        xwaitresult eWait;
        if (iChunk > 16384u)
            iChunk = 16384u;
        pFuture = xrtTlsStreamSendAsync(pStream, p + iOffset, iChunk);
        if (!pFuture)
            return false;
        eWait = mdo_test_future_wait_us(pFuture, xllmDeadlineAfterUs(UINT64_C(5000000)));
        xrtFutureDestroy(pFuture);
        if (eWait != XWAIT_OK)
            return false;
        iOffset += iChunk;
    }
    return true;
}

static void test_tls_serve_one(test_tls_server_ctx *pCtx, xtlsstream *pStream)
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
    while (iQuiet < 2u && !xrtCancelRequested(pCtx->pCancel))
    {
        xfuture *pFuture = xrtTlsStreamRecvAsync(pStream, 16384u);
        xwaitresult eWait =
            pFuture ? mdo_test_future_wait_us(pFuture, UINT64_C(300000)) : XWAIT_ERROR;
        xnetbytes *pBytes = (eWait == XWAIT_OK) ? (xnetbytes *)xrtFutureValue(pFuture) : NULL;
        xbytesview tView = pBytes ? xrtNetBytesView(pBytes) : (xbytesview){0};
        bool received = pBytes != NULL;
        bool appended = !tView.Size || xllm__buf_append(&tIn, tView.Data, tView.Size);
        if (pFuture) xrtFutureDestroy(pFuture);
        if (!received) { ++iQuiet; continue; }
        iQuiet = 0u;
        if (!appended) break;
    }
    if (tIn.iLen >= 4u && memcmp(tIn.pData, "POST", 4u) == 0)
    {
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
    test_tls_server_ctx *pCtx = (test_tls_server_ctx *)pData;
    while (!xrtCancelRequested(pCtx->pCancel) && pCtx->iRequests < 2)
    {
        xtlsstream *pStream = mdo_test_xrtTlsListenerAcceptWait(
            pCtx->pListener, xllmDeadlineAfterUs(UINT64_C(500000)), pCtx->pCancel);
        if (!pStream)
            continue;
        test_tls_serve_one(pCtx, pStream);
        xrtTlsStreamDestroy(pStream);
    }
    return 0;
}

static void test_tls_transport(void)
{
    test_tls_server_ctx tServer = {0};
    xtlscontext *pContext = NULL;
    xtlsidentity *pIdentity = NULL;
    xnetaddr tLocal;
    xllm_client *pClient = NULL;
    xllm_request tRequest;
    xllm_response *pResponse = NULL;
    xllm_error tError;
    char sUrl[256];
    xtlspolicy tPolicy;
    xtlscontextconfig tTlsConfig;
    xtlslistenerconfig tListenConfig;
    xnetengineconfig tEngineConfig;
    xbytesview tCert = {xllm_tls_cert, sizeof(xllm_tls_cert)};
    xbytesview tKey = {xllm_tls_key, sizeof(xllm_tls_key)};
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
    CHECK(tServer.pCancel && tServer.pEngine && xrtNetEngineStart(tServer.pEngine),
          "loopback TLS engine started");
    xrtTlsListenerConfigInit(&tListenConfig);
    CHECK(xrtNetAddrParse(&tListenConfig.Listen.Address, "127.0.0.1", 0u),
          "loopback TLS address parsed");
    tListenConfig.Tls.Context = pContext;
    tListenConfig.Tls.Identity = pIdentity;
    tServer.pListener = xrtTlsListenerStart(tServer.pEngine, &tListenConfig, NULL, NULL, NULL);
    CHECK(tServer.pListener != NULL && xrtTlsListenerLocal(tServer.pListener, &tLocal),
          "loopback TLS listener started");
    if (!tServer.pListener)
        goto cleanup;
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
    if (pClient)
    {
        xllmRequestInit(&tRequest);
        tRequest.bStream = false;
        CHECK(xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "ping"), "TLS request built");
        CHECK(xllmClientComplete(pClient, &tRequest, NULL, &pResponse, &tError) == XLLM_RESULT_OK &&
                  pResponse && pResponse->sContent &&
                  strcmp(pResponse->sContent, "secure hello") == 0,
              "TLS round trip delivers the model payload");
        if (!pResponse)
        {
            printf("    tls error: %s | phase=%s err=%s\n", tError.sMessage,
                   tError.tDiagnostics.sTransportPhase, tError.tDiagnostics.sTransportError);
        }
        xllmResponseDestroy(pResponse);
        xllmRequestUnit(&tRequest);
    }
cleanup:
    xllmClientDestroy(pClient);
    if (tServer.pCancel)
        (void)xrtCancelRequest(tServer.pCancel);
    if (tServer.pThread)
    {
        (void)xrtThreadWait(tServer.pThread);
        xrtThreadDestroy(tServer.pThread);
    }
    if (tServer.pListener)
    {
        (void)xrtTlsListenerClose(tServer.pListener);
        xrtTlsListenerDestroy(tServer.pListener);
    }
    if (tServer.pEngine)
    {
        (void)xrtNetEngineStop(tServer.pEngine);
        (void)xrtNetEngineDestroy(tServer.pEngine);
    }
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
    xtlscontext *pContext = NULL;
    xtlsidentity *pIdentity = NULL;
    xnetaddr tLocal;
    xnetengineconfig tEngineConfig;
    xtlspolicy tPolicy;
    xtlscontextconfig tTlsConfig;
    xtlslistenerconfig tListenConfig;
    xbytesview tCert = {xllm_ca_srv_cert_der, sizeof(xllm_ca_srv_cert_der)};
    xbytesview tKey = {xllm_ca_srv_key_der, sizeof(xllm_ca_srv_key_der)};
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
    if (!tServer.pListener)
        goto cleanup;
    tServer.uPort = tLocal.Port;
    tServer.pThread = xrtThreadCreate(test_tls_server_thread, &tServer, 0u);
    CHECK(tServer.pThread != NULL, "private-CA server thread started");
    (void)snprintf(sUrl, sizeof(sUrl), "https://127.0.0.1:%u/v1", (unsigned)tServer.uPort);

    { /* sCaPem: full chain verification over an IP-literal host (no SNI). */
        xllm_client_config tConfig;
        xllm_client *pClient = NULL;
        xllm_request tRequest;
        xllm_response *pResponse = NULL;
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
        if (pClient)
        {
            xllmRequestInit(&tRequest);
            tRequest.bStream = false;
            (void)xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "ping");
            CHECK(xllmClientComplete(pClient, &tRequest, NULL, &pResponse, &tError) ==
                          XLLM_RESULT_OK &&
                      pResponse && pResponse->sContent &&
                      strcmp(pResponse->sContent, "secure hello") == 0,
                  "CA PEM store verifies the chain and matches the IP SAN");
            if (!pResponse)
            {
                printf("    ca pem error: %s | phase=%s err=%s\n", tError.sMessage,
                       tError.tDiagnostics.sTransportPhase, tError.tDiagnostics.sTransportError);
            }
            xllmResponseDestroy(pResponse);
            pResponse = NULL;
            xllmRequestUnit(&tRequest);
            xllmClientDestroy(pClient);
        }
    }
    { /* wrong CA: the handshake must fail verification, no silent fallback */
        xllm_client_config tConfig;
        xllm_client *pClient = NULL;
        xllm_request tRequest;
        xllm_response *pResponse = NULL;
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
        if (pClient)
        {
            xllm_result eResult;
            xllmRequestInit(&tRequest);
            tRequest.bStream = false;
            (void)xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "ping");
            eResult = xllmClientComplete(pClient, &tRequest, NULL, &pResponse, &tError);
            CHECK(eResult != XLLM_RESULT_OK && pResponse == NULL,
                  "wrong CA fails verification instead of silently degrading");
            CHECK(tError.tCause.eDomain == XLLM_ERROR_DOMAIN_TRANSPORT &&
                      (tError.tCause.eStage == XLLM_ERROR_STAGE_TLS ||
                       tError.tCause.eStage == XLLM_ERROR_STAGE_CONNECT) &&
                      tError.tCause.sOperation[0] != 0 && tError.tCause.sMessage[0] != 0 &&
                      strstr(tError.tCause.sMessage, "test-key") == NULL,
                  "TLS failure retains a sanitized runtime cause");
            if (eResult == XLLM_RESULT_OK)
            {
                xllmResponseDestroy(pResponse);
            }
            xllmRequestUnit(&tRequest);
            xllmClientDestroy(pClient);
        }
    }
    { /* borrowed pX509Store outranks sCaPem */
        xllm_client_config tConfig;
        xllm_client *pClient = NULL;
        xllm_request tRequest;
        xllm_response *pResponse = NULL;
        xllm_error tError;
        xx509store *pStore = xrtX509StoreCreate();
        size_t iAdded = 0u;
        bool bLoaded = pStore &&
                       xrtX509StoreAddPem(pStore, xllm_ca_pem, strlen(xllm_ca_pem), &iAdded) &&
                       iAdded == 1u;
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
        if (pClient)
        {
            xllmRequestInit(&tRequest);
            tRequest.bStream = false;
            (void)xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "ping");
            CHECK(xllmClientComplete(pClient, &tRequest, NULL, &pResponse, &tError) ==
                          XLLM_RESULT_OK &&
                      pResponse && pResponse->sContent &&
                      strcmp(pResponse->sContent, "secure hello") == 0,
                  "borrowed store outranks the (wrong) CA PEM field");
            xllmResponseDestroy(pResponse);
            xllmRequestUnit(&tRequest);
            xllmClientDestroy(pClient);
        }
        xrtX509StoreFree(pStore);
    }

cleanup:
    if (tServer.pCancel)
    {
        xrtCancelRequest(tServer.pCancel);
    }
    if (tServer.pThread)
    {
        (void)xrtThreadWait(tServer.pThread);
        xrtThreadDestroy(tServer.pThread);
    }
    if (tServer.pListener)
    {
        xrtTlsListenerDestroy(tServer.pListener);
    }
    if (tServer.pEngine)
    {
        xrtNetEngineStop(tServer.pEngine);
        xrtNetEngineDestroy(tServer.pEngine);
    }
    if (tServer.pCancel)
    {
        xrtCancelDestroy(tServer.pCancel);
    }
    if (pIdentity)
    {
        xrtTlsIdentityRelease(pIdentity);
    }
    if (pContext)
    {
        xrtTlsContextRelease(pContext);
    }
}

typedef struct test_op_slot_race_ctx
{
    volatile long iStart;
    xpromise *pPromise;
    bool bResolved;
} test_op_slot_race_ctx;

static int32 test_op_slot_resolve_thread(ptr pData)
{
    test_op_slot_race_ctx *pCtx = (test_op_slot_race_ctx *)pData;
    while (xllm__atomic_load(&pCtx->iStart) == 0)
    {
        xrtThreadYield();
    }
    pCtx->bResolved = xrtPromiseResolve(pCtx->pPromise, NULL);
    xrtPromiseDestroy(pCtx->pPromise);
    return 0;
}

/* Reproduces the packed-app teardown shape without a network dependency:
 * completion owns one side of the race while teardown removes the same
 * watch. The operation node must keep its Future alive through both tails. */
static void test_op_slot_completion_remove_race(void)
{
    xllm_call tCall;
    bool bOk = true;
    size_t i;
    memset(&tCall, 0, sizeof(tCall));
    tCall.bOpMutexInit = xrtMutexInit(&tCall.OpMutex);
    xllm__atomic_store(&tCall.iClosing, 1);
    if (!tCall.bOpMutexInit)
    {
        bOk = false;
    }
    for (i = 0u; bOk && i < 512u; ++i)
    {
        test_op_slot_race_ctx tRace;
        xpromise *pPromise;
        xfuture *pFuture = NULL;
        xthread *pThread;
        memset(&tRace, 0, sizeof(tRace));
        pPromise = xrtPromiseCreate(&pFuture, NULL);
        if (!pPromise || !pFuture || xllm__op_arm(&tCall, pFuture) != XLLM_OP_ARM_PENDING)
        {
            xrtPromiseDestroy(pPromise);
            xrtFutureDestroy(pFuture);
            bOk = false;
            break;
        }
        tRace.pPromise = pPromise;
        pThread = xrtThreadCreate(test_op_slot_resolve_thread, &tRace, 0u);
        if (!pThread)
        {
            xllm__op_disarm(&tCall);
            xrtPromiseDestroy(pPromise);
            bOk = false;
            break;
        }
        xllm__atomic_store(&tRace.iStart, 1);
        if ((i & 1u) != 0u)
        {
            xrtThreadYield();
        }
        if ((i & 3u) == 0u)
        {
            xfuture *pCancel = xllm__op_future_ref(&tCall);
            if (pCancel)
            {
                (void)xrtFutureCancel(pCancel);
                xrtFutureDestroy(pCancel);
            }
        }
        xllm__op_disarm(&tCall);
        (void)xrtThreadWait(pThread);
        xrtThreadDestroy(pThread);
        while (xllm__atomic_load(&tCall.iTransportActive) != 0)
        {
            xrtThreadYield();
        }
        if (tCall.pOpNode != NULL)
        {
            bOk = false;
        }
        /* Cancellation may win the terminal state; resolution is then
         * expected to report false while still releasing the Promise. */
        (void)tRace.bResolved;
    }
    xllm__op_disarm(&tCall);
    if (tCall.bOpMutexInit && !xrtMutexUnit(&tCall.OpMutex))
    {
        bOk = false;
    }
    CHECK(bOk, "operation completion/remove race keeps Future ownership valid");
}

static void test_async_engine(void)
{
    test_server_ctx tServerCtx = {0};
    char sJsonUrl[256];
    xllm_client *pClient = NULL;
    xllm_request tRequest;
    xllm_error tError;
    xllm_call *tCalls[6];
    xllm_response *pResponse = NULL;
    int i;
    int iOk = 0;
    printf("async engine transport\n");
    CHECK(test_server_start(&tServerCtx), "async test server starts");
    if (!tServerCtx.pListener)
        goto cleanup;
    (void)snprintf(sJsonUrl, sizeof(sJsonUrl), "http://127.0.0.1:%u/json/v1",
                   (unsigned)tServerCtx.uPort);
    pClient = test_make_client(sJsonUrl, XLLM_PROVIDER_OPENAI_COMPAT);
    CHECK(pClient != NULL, "async client created");
    xllmRequestInit(&tRequest);
    CHECK(xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "Ping"), "async request built");
    /* Submit every call before waiting on any: Start must return while the
     * transports run on the engine workers. */
    for (i = 0; i < 6; ++i)
    {
        tCalls[i] = xllmClientStart(pClient, &tRequest, NULL, &tError);
        if (!tCalls[i])
        {
            break;
        }
    }
    CHECK(i == 6 && xllmCallFuture(tCalls[0]) != NULL,
          "six calls started concurrently with futures");
    for (; i > 0;)
    {
        --i;
        pResponse = NULL;
        if (xllmCallWait(tCalls[i], &pResponse, &tError) == XLLM_RESULT_OK && pResponse &&
            pResponse->sContent && strcmp(pResponse->sContent, "fallback") == 0)
        {
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
        xnetengine *pEngine = NULL;
        xllm_client *pShared1 = NULL;
        xllm_client *pShared2 = NULL;
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
        CHECK(xllmClientComplete(pShared1, &tRequest, NULL, &pResponse, &tError) ==
                      XLLM_RESULT_OK &&
                  pResponse && pResponse->sContent && strcmp(pResponse->sContent, "fallback") == 0,
              "shared-engine client 1 completes");
        xllmResponseDestroy(pResponse);
        pResponse = NULL;
        CHECK(xllmClientComplete(pShared2, &tRequest, NULL, &pResponse, &tError) ==
                      XLLM_RESULT_OK &&
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
                  xllmClientComplete(pShared1, &tRequest, NULL, &pResponse, &tError) ==
                      XLLM_RESULT_OK &&
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
        xllm_call *pCall = (xllm_call *)calloc(1u, sizeof(*pCall));
        char *sFlood;
        static const char sHead[] = "data: ";
        CHECK(pCall != NULL, "sse cap call allocation");
        if (pCall)
        {
            pCall->pDialect = xllm__dialect_completions();
            pCall->uHttpStatus = 200u;
            pCall->bSse = true;
            pCall->bStreamWanted = true;
            sFlood = (char *)malloc(sizeof(sHead) - 1u + (1024u * 1024u) + 8u);
            CHECK(sFlood != NULL, "sse cap flood allocation");
            if (sFlood)
            {
                memcpy(sFlood, sHead, sizeof(sHead) - 1u);
                memset(sFlood + sizeof(sHead) - 1u, 'A', (1024u * 1024u) + 8u);
                CHECK(!xllm__sse_feed(pCall, sFlood, sizeof(sHead) - 1u + (1024u * 1024u) + 8u) &&
                          pCall->tError.eCode == XLLM_ERROR_LIMIT,
                      "oversized SSE line rejected at the framing limit");
                free(sFlood);
            }
            xllmCallDestroy(pCall);
        }
    }
    {
        static const char sLongLine[] = "data: 123";
        static const char sLargeEvent[] = "data: 12345\n\n";
        xllm_call *pCall = (xllm_call *)calloc(1u, sizeof(*pCall));
        CHECK(pCall != NULL, "configured SSE cap call allocation");
        if (pCall)
        {
            pCall->pDialect = xllm__dialect_completions();
            pCall->uMaxSseLineBytes = 8u;
            CHECK(!xllm__sse_feed(pCall, sLongLine, sizeof(sLongLine) - 1u) &&
                      pCall->tError.eCode == XLLM_ERROR_LIMIT,
                  "configured SSE line limit fails before buffer growth");
            xllmCallDestroy(pCall);
        }
        pCall = (xllm_call *)calloc(1u, sizeof(*pCall));
        CHECK(pCall != NULL, "configured SSE event cap call allocation");
        if (pCall)
        {
            pCall->pDialect = xllm__dialect_completions();
            pCall->uMaxSseLineBytes = 64u;
            pCall->uMaxSseEventBytes = 4u;
            CHECK(!xllm__sse_feed(pCall, sLargeEvent, sizeof(sLargeEvent) - 1u) &&
                      pCall->tError.eCode == XLLM_ERROR_LIMIT,
                  "configured SSE event limit rejects one oversized field safely");
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
        CHECK(!xllm__utf8_valid("\xE0\x80\x80") && !xllm__utf8_valid("\xF0\x80\x80\x80") &&
                  xllm__utf8_valid("\xE0\xA0\x80") && xllm__utf8_valid("\xF0\x90\x80\x80") &&
                  xllm__utf8_valid("\xF4\x8F\xBF\xBF"),
              "strict UTF-8 rejects 3/4-byte overlong forms at scalar boundaries");
        xllmPartInit(&tPart, XLLM_PART_TEXT);
        CHECK(!xllmPartSetText(&tPart, "\xED\xA0\x80surrogate"),
              "part text rejects surrogate halves");
        xllmPartInit(&tPart, XLLM_PART_TEXT);
        CHECK(xllmPartSetText(&tPart, "\360\237\230\200emoji"),
              "part text accepts a valid 4-byte sequence");
        xllmPartUnit(&tPart);
        xllmMessageUnit(&tMessage);
    }
    {
        int iCode;
        bool bAllNamed = true;
        for (iCode = (int)XLLM_ERROR_NONE; iCode <= (int)XLLM_ERROR_HOOK; ++iCode)
        {
            if (strcmp(xllmErrorCodeName((xllm_error_code)iCode), "unknown") == 0)
            {
                bAllNamed = false;
            }
        }
        CHECK(bAllNamed && strcmp(xllmErrorCodeName(XLLM_ERROR_LIMIT), "limit") == 0 &&
                  strcmp(xllmErrorCodeName(XLLM_ERROR_HOOK), "hook") == 0,
              "every public error code has a stable diagnostic name");
    }
    {
        xllm_error tError;
        xllmErrorInit(&tError);
        CHECK(tError.tCause.Size == sizeof(tError.tCause) &&
                  tError.tCause.Version == XLLM_ERROR_CAUSE_VERSION &&
                  tError.tCause.eDomain == XLLM_ERROR_DOMAIN_NONE,
              "error cause has versioned inline storage");
        xllm__error_set(&tError, XLLM_ERROR_PARSE, "invalid JSON provider response");
        CHECK(tError.tCause.eDomain == XLLM_ERROR_DOMAIN_PARSER &&
                  tError.tCause.eStage == XLLM_ERROR_STAGE_PARSE &&
                  strcmp(tError.tCause.sOperation, "parse-response") == 0 &&
                  strcmp(xllmErrorDomainName(tError.tCause.eDomain), "xllm.parser") == 0 &&
                  strcmp(xllmErrorStageName(tError.tCause.eStage), "parse") == 0,
              "parser failure exposes a stable structured cause");
    }
    /* Timer lifetime: a deadline call destroyed mid-flight must drain the
     * watchdog reference without hanging or crashing. */
    {
        test_server_ctx tServerCtx = {0};
        char sSlowUrl[256];
        xllm_client *pClient = NULL;
        xllm_request tRequest;
        xllm_error tError;
        xllm_call *pCall;
        CHECK(test_server_start(&tServerCtx), "timer drain server starts");
        if (tServerCtx.pListener)
        {
            (void)snprintf(sSlowUrl, sizeof(sSlowUrl), "http://127.0.0.1:%u/slow/v1",
                           (unsigned)tServerCtx.uPort);
            pClient = test_make_client(sSlowUrl, XLLM_PROVIDER_OPENAI_COMPAT);
            xllmRequestInit(&tRequest);
            (void)xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "Ping");
            xllmRequestSetDeadline(&tRequest, xllmDeadlineAfterUs(UINT64_C(2000000)));
            pCall = xllmClientStart(pClient, &tRequest, NULL, &tError);
            CHECK(pCall != NULL && pCall->uTimerId != 0u, "deadline call arms the watchdog timer");
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
        "data: "
        "{\"id\":\"resp_frag\",\"model\":\"glm-test\",\"choices\":[{\"index\":0,\"delta\":{"
        "\"reasoning_content\":\"think\"},\"finish_reason\":null}]}\r\n\r\n"
        "data: "
        "{\"choices\":[{\"index\":0,\"delta\":{\"content\":\"done\",\"tool_calls\":[{\"index\":0,"
        "\"id\":\"a\",\"function\":{\"name\":\"read_file\",\"arguments\":\"{\\\"p\"}},{\"index\":1,"
        "\"id\":\"b\",\"function\":{\"name\":\"search\",\"arguments\":\"{\\\"q\\\":\"}}]},\"finish_"
        "reason\":null}]}\n\n"
        "data: "
        "{\"choices\":[{\"index\":0,\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{"
        "\"arguments\":\"ath\\\":\\\"x.c\\\"}\"}},{\"index\":1,\"function\":{\"arguments\":"
        "\"\\\"needle\\\"}\"}}]},\"finish_reason\":\"tool_calls\"}],\"usage\":{\"prompt_tokens\":5,"
        "\"completion_tokens\":7,\"total_tokens\":12}}\n\n"
        "data: [DONE]\n\n";
    xllm_call *pCall = (xllm_call *)calloc(1u, sizeof(*pCall));
    test_events tEvents = {0};
    size_t i = 0u;
    size_t iStep = 1u;
    bool bFeedOk = true;
    CHECK(pCall != NULL, "fragmented parser call allocation");
    if (!pCall)
    {
        return;
    }
    pCall->pDialect = xllm__dialect_completions();
    pCall->uHttpStatus = 200u;
    pCall->bSse = true;
    pCall->bStreamWanted = true;
    pCall->sSelectedModel = xllm__strdup("glm-test");
    pCall->tCallbacks.pUserData = &tEvents;
    pCall->tCallbacks.OnEvent = test_on_event;
    while (i < sizeof(sSse) - 1u)
    {
        size_t iChunk = iStep;
        if (iChunk > sizeof(sSse) - 1u - i)
        {
            iChunk = sizeof(sSse) - 1u - i;
        }
        if (!xllm__sse_feed(pCall, sSse + i, iChunk))
        {
            bFeedOk = false;
            break;
        }
        i += iChunk;
        iStep = iStep == 13u ? 1u : iStep + 1u;
    }
    CHECK(bFeedOk, "arbitrarily fragmented stream accepted");
    if (!bFeedOk)
    {
        printf("    parser error: %s\n", pCall->tError.sMessage);
    }
    CHECK(xllm__sse_finish(pCall), "fragmented SSE finalization");
    {
        bool bFinal = xllm__assemble_finalize(pCall);
        CHECK(bFinal, "fragmented response finalization");
        if (!bFinal)
        {
            printf("    finalize error: %s\n", pCall->tError.sMessage);
            if (pCall->pResponse && pCall->pResponse->iToolCallCount > 1u)
            {
                printf("    tool[1] arguments: %s\n",
                       pCall->pResponse->pToolCalls[1].sArgumentsJson);
            }
        }
    }
    CHECK(pCall->pResponse && strcmp(pCall->pResponse->sContent, "done") == 0,
          "text deltas assembled");
    CHECK(pCall->pResponse && pCall->pResponse->sReasoningContent &&
              strcmp(pCall->pResponse->sReasoningContent, "think") == 0,
          "reasoning deltas assembled");
    CHECK(pCall->pResponse && pCall->pResponse->iToolCallCount == 2u,
          "interleaved multi-tool calls assembled");
    CHECK(pCall->pResponse && pCall->pResponse->iToolCallCount > 0u &&
              strcmp(pCall->pResponse->pToolCalls[0].sArgumentsJson, "{\"path\":\"x.c\"}") == 0,
          "tool arguments fragments assembled");
    CHECK(pCall->pResponse && pCall->pResponse->iToolCallCount > 1u &&
              strcmp(pCall->pResponse->pToolCalls[1].sArgumentsJson, "{\"q\":\"needle\"}") == 0,
          "second tool arguments assembled");
    CHECK(pCall->pResponse && pCall->pResponse->tUsage.uTotalTokens == 12u, "stream usage parsed");
    CHECK(tEvents.iReasoning == 1 && tEvents.iText == 1 && tEvents.iTool == 4 &&
              tEvents.iUsage == 1 && tEvents.iDone == 1,
          "normalized stream events emitted");
    xllmCallDestroy(pCall);
}

static void test_malformed_parser(void)
{
    static const char sMalformed[] = "data: {\"choices\":[{\"unterminated\":\"value}\n\n";
    xllm_call *pCall = (xllm_call *)calloc(1u, sizeof(*pCall));
    bool bAccepted;
    CHECK(pCall != NULL, "malformed parser call allocation");
    if (!pCall)
        return;
    pCall->pDialect = xllm__dialect_completions();
    pCall->uHttpStatus = 200u;
    pCall->bSse = true;
    pCall->bStreamWanted = true;
    bAccepted = xllm__sse_feed(pCall, sMalformed, sizeof(sMalformed) - 1u);
    CHECK(!bAccepted && pCall->tError.eCode == XLLM_ERROR_PARSE &&
              strcmp(pCall->tError.sMessage, "invalid JSON in provider event stream") == 0,
          "malformed SSE reports the precise parse failure");
    CHECK(!pCall->bSawEvent && !pCall->bDone && pCall->pResponse == NULL,
          "malformed SSE delivers no partial model response");
    xllmCallDestroy(pCall);
}

static void test_retry_idempotency_boundary(void)
{
    test_server_ctx tServerCtx = {0};
    char sUnsafeUrl[256];
    char sIdempotentUrl[256];
    xllm_client *pUnsafeClient = NULL;
    xllm_client *pIdempotentClient = NULL;
    xllm_request tRequest;
    xllm_response *pResponse = NULL;
    xllm_error tError;
    xllm_result eResult;
    const xllm_header tIdempotencyHeader = {"iDeMpOtEnCy-KeY", "operation-1"};

    xllmErrorInit(&tError);
    tError.eCode = XLLM_ERROR_NETWORK;
    CHECK(xllmErrorRetryable(&tError), "network failure before write is retryable");
    tError.tDiagnostics.uRequestBytes = 1u;
    CHECK(!xllmErrorRetryable(&tError), "written network request requires idempotency key");
    tError.tDiagnostics.bIdempotencyKeyPresent = true;
    CHECK(xllmErrorRetryable(&tError), "idempotency key permits written network retry");
    tError.tDiagnostics.bModelDataDelivered = true;
    CHECK(!xllmErrorRetryable(&tError), "delivered model data suppresses automatic retry");
    xllmErrorInit(&tError);
    tError.eCode = XLLM_ERROR_NETWORK;
    tError.tCause.eDomain = XLLM_ERROR_DOMAIN_RESOURCE;
    tError.tCause.eStage = XLLM_ERROR_STAGE_WRITE;
    CHECK(!xllmErrorRetryable(&tError),
          "local storage failure is not retried as a provider request");

    CHECK(test_server_start(&tServerCtx), "retry boundary test server starts");
    if (!tServerCtx.pListener)
        goto cleanup;
    (void)snprintf(sUnsafeUrl, sizeof(sUnsafeUrl), "http://127.0.0.1:%u/unsafe-drop/v1",
                   (unsigned)tServerCtx.uPort);
    (void)snprintf(sIdempotentUrl, sizeof(sIdempotentUrl), "http://127.0.0.1:%u/idempotent-drop/v1",
                   (unsigned)tServerCtx.uPort);
    pUnsafeClient = test_make_client(sUnsafeUrl, XLLM_PROVIDER_OPENAI_COMPAT);
    pIdempotentClient = test_make_client(sIdempotentUrl, XLLM_PROVIDER_OPENAI_COMPAT);
    CHECK(pUnsafeClient && pIdempotentClient, "retry boundary clients created");

    xllmRequestInit(&tRequest);
    tRequest.bStream = false;
    (void)xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "ping");
    eResult = xllmClientComplete(pUnsafeClient, &tRequest, NULL, &pResponse, &tError);
    CHECK(eResult == XLLM_RESULT_ERROR && pResponse == NULL && tError.eCode == XLLM_ERROR_NETWORK &&
              tError.tDiagnostics.uAttemptCount == 1u && tError.tDiagnostics.uRequestBytes > 0u &&
              !tError.tDiagnostics.bIdempotencyKeyPresent && !xllmErrorRetryable(&tError) &&
              xrtAtomic32Load(&tServerCtx.iUnsafeDropRequests, XMEMORY_RELAXED) == 1u,
          "post-write disconnect is not replayed without idempotency key");
    xllmRequestUnit(&tRequest);

    xllmRequestInit(&tRequest);
    tRequest.bStream = false;
    tRequest.pExtraHeaders = &tIdempotencyHeader;
    tRequest.iExtraHeaderCount = 1u;
    (void)xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "ping");
    eResult = xllmClientComplete(pIdempotentClient, &tRequest, NULL, &pResponse, &tError);
    CHECK(eResult == XLLM_RESULT_OK && pResponse && strcmp(pResponse->sContent, "fallback") == 0 &&
              pResponse->tDiagnostics.uAttemptCount == 2u &&
              pResponse->tDiagnostics.bIdempotencyKeyPresent &&
              xrtAtomic32Load(&tServerCtx.iIdempotentDropRequests, XMEMORY_RELAXED) == 2u &&
              xrtAtomic32Load(&tServerCtx.bSawIdempotencyKey, XMEMORY_RELAXED) != 0u,
          "idempotency key permits one bounded post-write replay");
    xllmRequestUnit(&tRequest);

cleanup:
    xllmResponseDestroy(pResponse);
    xllmClientDestroy(pIdempotentClient);
    xllmClientDestroy(pUnsafeClient);
    test_server_stop(&tServerCtx);
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
    xllm_client *pClient = NULL;
    xllm_client *pJsonClient = NULL;
    xllm_client *pErrorClient = NULL;
    xllm_client *pPolicyClient = NULL;
    xllm_client *pTransientClient = NULL;
    xllm_client *pSlowClient = NULL;
    xcancel *pCancel = NULL;
    xllm_request tRequest;
    xllm_response *pResponse = NULL;
    xllm_error tError;
    xllm_stream_callbacks tCallbacks;
    test_events tEvents = {0};
    xllm_result eResult;

    CHECK(test_server_start(&tServerCtx), "local core HTTP/1 provider server starts");
    if (!tServerCtx.pListener)
        goto cleanup;
    (void)snprintf(sBaseUrl, sizeof(sBaseUrl), "http://127.0.0.1:%u/v1",
                   (unsigned)tServerCtx.uPort);
    (void)snprintf(sJsonUrl, sizeof(sJsonUrl), "http://127.0.0.1:%u/json/v1",
                   (unsigned)tServerCtx.uPort);
    (void)snprintf(sErrorUrl, sizeof(sErrorUrl), "http://127.0.0.1:%u/error/v1",
                   (unsigned)tServerCtx.uPort);
    (void)snprintf(sPolicyUrl, sizeof(sPolicyUrl), "http://127.0.0.1:%u/policy/v1",
                   (unsigned)tServerCtx.uPort);
    (void)snprintf(sTransientUrl, sizeof(sTransientUrl), "http://127.0.0.1:%u/transient/v1",
                   (unsigned)tServerCtx.uPort);
    (void)snprintf(sSlowUrl, sizeof(sSlowUrl), "http://127.0.0.1:%u/slow/v1",
                   (unsigned)tServerCtx.uPort);
    pClient = test_make_client(sBaseUrl, XLLM_PROVIDER_GLM);
    pJsonClient = test_make_client(sJsonUrl, XLLM_PROVIDER_OPENAI_COMPAT);
    pErrorClient = test_make_client(sErrorUrl, XLLM_PROVIDER_OPENAI_COMPAT);
    pPolicyClient = test_make_client(sPolicyUrl, XLLM_PROVIDER_OPENAI_COMPAT);
    pTransientClient = test_make_client(sTransientUrl, XLLM_PROVIDER_OPENAI_COMPAT);
    pSlowClient = test_make_client(sSlowUrl, XLLM_PROVIDER_OPENAI_COMPAT);
    CHECK(pClient && pJsonClient && pErrorClient && pPolicyClient && pTransientClient &&
              pSlowClient,
          "transport clients created");

    xllmRequestInit(&tRequest);
    (void)xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "Use both tools.");
    (void)xllmRequestAddTool(&tRequest, "read_file", "Read a file",
                             "{\"type\":\"object\",\"properties\":{}}", false);
    (void)xllmRequestAddTool(&tRequest, "search", "Search text",
                             "{\"type\":\"object\",\"properties\":{}}", false);
    memset(&tCallbacks, 0, sizeof(tCallbacks));
    tCallbacks.pUserData = &tEvents;
    tCallbacks.OnEvent = test_on_event;
    eResult = xllmClientComplete(pClient, &tRequest, &tCallbacks, &pResponse, &tError);
    CHECK(eResult == XLLM_RESULT_OK, "streaming transport completes");
    if (eResult != XLLM_RESULT_OK)
    {
        printf("    transport error: %s (%s), provider=%s, http=%d\n", tError.sMessage,
               xllmErrorCodeName(tError.eCode), tError.sProviderMessage, (int)tError.iHttpStatus);
    }
    CHECK(pResponse && pResponse->iToolCallCount == 2u, "transport preserves multiple tool calls");
    CHECK(pResponse &&
              strcmp(pResponse->pToolCalls[0].sArgumentsJson, "{\"path\":\"main.c\"}") == 0,
          "transport assembles split tool arguments");
    CHECK(pResponse && pResponse->tUsage.uReasoningTokens == 9u,
          "transport parses reasoning usage");
    CHECK(pResponse && pResponse->sRequestId &&
              strcmp(pResponse->sRequestId, "request-local-1") == 0,
          "transport captures request id");
    CHECK(pResponse && pResponse->tDiagnostics.uAttemptCount == 1u &&
              strcmp(pResponse->tDiagnostics.sTransportError, "none") == 0 &&
              strcmp(pResponse->tDiagnostics.sTransportPhase, "complete") == 0,
          "transport success diagnostics captured");
    CHECK(tEvents.iStarts == 1 && tEvents.iDone == 1, "transport emits lifecycle events");
    xllmResponseDestroy(pResponse);
    pResponse = NULL;

    eResult = xllmClientComplete(pJsonClient, &tRequest, NULL, &pResponse, &tError);
    CHECK(eResult == XLLM_RESULT_OK && pResponse && strcmp(pResponse->sContent, "fallback") == 0,
          "non-SSE JSON fallback completes");
    xllmResponseDestroy(pResponse);
    pResponse = NULL;

    eResult = xllmClientComplete(pTransientClient, &tRequest, NULL, &pResponse, &tError);
    CHECK(eResult == XLLM_RESULT_OK && pResponse && strcmp(pResponse->sContent, "fallback") == 0,
          "transient provider failure retries to success");
    CHECK(pResponse && pResponse->tDiagnostics.uAttemptCount == 2u &&
              pResponse->tDiagnostics.uMaxAttempts == 3u &&
              xrtAtomic32Load(&tServerCtx.iTransientRequests, XMEMORY_RELAXED) == 2u,
          "successful retry reports final attempt metadata");
    xllmResponseDestroy(pResponse);
    pResponse = NULL;

    eResult = xllmClientComplete(pErrorClient, &tRequest, NULL, &pResponse, &tError);
    CHECK(eResult == XLLM_RESULT_ERROR && tError.eCode == XLLM_ERROR_RATE_LIMIT,
          "HTTP rate-limit error normalized");
    CHECK(strcmp(tError.sProviderMessage, "slow down") == 0, "provider error message preserved");
    CHECK(tError.tCause.eDomain == XLLM_ERROR_DOMAIN_PROVIDER &&
              tError.tCause.eStage == XLLM_ERROR_STAGE_PROVIDER &&
              strcmp(tError.tCause.sOperation, "provider-response") == 0 &&
              strstr(tError.tCause.sMessage, "test-key") == NULL,
          "HTTP provider failure carries a sanitized structured cause");
    CHECK(tError.tDiagnostics.uAttemptCount == 3u && tError.tDiagnostics.bRetryExhausted &&
              !tError.tDiagnostics.bRetryable && xllmErrorRetryable(&tError),
          "retry exhaustion remains observable and classifiable");
    CHECK(tError.tDiagnostics.iTransportStatus == XLLM_TRANSPORT_OK &&
              strcmp(tError.tDiagnostics.sTransportPhase, "complete") == 0 &&
              tError.tDiagnostics.uRetryAfterMs == 1u,
          "HTTP failure carries transport and retry-after diagnostics");

    eResult = xllmClientComplete(pPolicyClient, &tRequest, NULL, &pResponse, &tError);
    if (eResult != XLLM_RESULT_ERROR || strcmp(tError.sProviderCode, "1313") != 0 ||
        strcmp(tError.sProviderMessage, "account policy restricted") != 0)
    {
        printf("    policy result=%d error=%s http=%d code='%s' provider='%s' attempts=%u\n",
               (int)eResult, xllmErrorCodeName(tError.eCode), (int)tError.iHttpStatus,
               tError.sProviderCode, tError.sProviderMessage,
               (unsigned)tError.tDiagnostics.uAttemptCount);
    }
    CHECK(eResult == XLLM_RESULT_ERROR && tError.eCode == XLLM_ERROR_RATE_LIMIT &&
              strcmp(tError.sProviderCode, "1313") == 0 &&
              strcmp(tError.sProviderMessage, "account policy restricted") == 0,
          "numeric provider policy code and message preserved");
    CHECK(tError.tDiagnostics.uAttemptCount == 1u && !tError.tDiagnostics.bRetryable &&
              !tError.tDiagnostics.bRetryExhausted && !xllmErrorRetryable(&tError),
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
    xllmRequestSetDeadline(&tRequest, xllmDeadlineAfterUs(UINT64_C(150000)));
    eResult = xllmClientComplete(pSlowClient, &tRequest, NULL, &pResponse, &tError);
    if (!(eResult == XLLM_RESULT_TIMEOUT && tError.eCode == XLLM_ERROR_TIMEOUT &&
          tError.tDiagnostics.bContextAttached &&
          strcmp(tError.tDiagnostics.sContextStatus, "deadline_exceeded") == 0 &&
          strcmp(tError.tDiagnostics.sTransportError, "deadline_exceeded") == 0 &&
          tError.tDiagnostics.uEffectiveTimeoutMs > 0u))
    {
        printf("    deadline result=%d error=%s transport=%s phase=%s context=%s attached=%d "
               "effective=%llu\n",
               (int)eResult, xllmErrorCodeName(tError.eCode), tError.tDiagnostics.sTransportError,
               tError.tDiagnostics.sTransportPhase, tError.tDiagnostics.sContextStatus,
               tError.tDiagnostics.bContextAttached ? 1 : 0,
               (unsigned long long)tError.tDiagnostics.uEffectiveTimeoutMs);
    }
    CHECK(eResult == XLLM_RESULT_TIMEOUT && tError.eCode == XLLM_ERROR_TIMEOUT &&
              tError.tDiagnostics.bContextAttached &&
              strcmp(tError.tDiagnostics.sContextStatus, "deadline_exceeded") == 0 &&
              strcmp(tError.tDiagnostics.sTransportError, "deadline_exceeded") == 0 &&
              tError.tDiagnostics.uEffectiveTimeoutMs > 0u,
          "live model deadline propagates structured diagnostics");
    xllmRequestSetDeadline(&tRequest, XLLM_DEADLINE_NEVER);
    CHECK(xrtAtomic32Load(&tServerCtx.iRequests, XMEMORY_RELAXED) == 9u &&
              xrtAtomic32Load(&tServerCtx.iSlowRequests, XMEMORY_RELAXED) == 1u &&
              xrtAtomic32Load(&tServerCtx.bSawAuth, XMEMORY_RELAXED) != 0u &&
              xrtAtomic32Load(&tServerCtx.bSawModel, XMEMORY_RELAXED) != 0u &&
              xrtAtomic32Load(&tServerCtx.bSawTools, XMEMORY_RELAXED) != 0u,
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

int main(int argc, char **argv)
{
    printf("xllm v%d.%d.%d tests\n", XLLM_VERSION_MAJOR, XLLM_VERSION_MINOR, XLLM_VERSION_PATCH);
    if (argc == 2 && strcmp(argv[1], "--signed-response") == 0)
    {
        test_signed_response();
        return g_iFailures ? 1 : 0;
    }
    if (argc == 2 && strcmp(argv[1], "--retry-idempotency") == 0)
    {
        test_retry_idempotency_boundary();
        printf("xllm v%d.%d.%d retry idempotency: %s (%d failures)\n", XLLM_VERSION_MAJOR,
               XLLM_VERSION_MINOR, XLLM_VERSION_PATCH, g_iFailures ? "FAIL" : "PASS", g_iFailures);
        return g_iFailures ? 1 : 0;
    }
    if (argc == 2 && strcmp(argv[1], "--connection-pool") == 0)
    {
        test_keepalive_and_concurrency();
        printf("xllm v%d.%d.%d connection pool: %s (%d failures)\n", XLLM_VERSION_MAJOR,
               XLLM_VERSION_MINOR, XLLM_VERSION_PATCH, g_iFailures ? "FAIL" : "PASS", g_iFailures);
        return g_iFailures ? 1 : 0;
    }
    if (argc == 2 && strcmp(argv[1], "--oom-public-mutators") == 0)
    {
        test_public_mutator_oom();
        printf("xllm v%d.%d.%d public mutator OOM: %s (%d failures)\n", XLLM_VERSION_MAJOR,
               XLLM_VERSION_MINOR, XLLM_VERSION_PATCH, g_iFailures ? "FAIL" : "PASS", g_iFailures);
        return g_iFailures ? 1 : 0;
    }
    if (argc == 2 && strcmp(argv[1], "--oom-public-client") == 0)
    {
        test_public_client_oom();
        printf("xllm v%d.%d.%d public client OOM: %s (%d failures)\n", XLLM_VERSION_MAJOR,
               XLLM_VERSION_MINOR, XLLM_VERSION_PATCH, g_iFailures ? "FAIL" : "PASS", g_iFailures);
        return g_iFailures ? 1 : 0;
    }
    if (argc == 2 && strcmp(argv[1], "--oom-adapters") == 0)
    {
        test_adapter_assembly_oom();
        printf("xllm v%d.%d.%d adapter assembly OOM: %s (%d failures)\n", XLLM_VERSION_MAJOR,
               XLLM_VERSION_MINOR, XLLM_VERSION_PATCH, g_iFailures ? "FAIL" : "PASS", g_iFailures);
        return g_iFailures ? 1 : 0;
    }
    if (argc == 2 && strcmp(argv[1], "--release-metadata") == 0)
    {
        test_release_metadata();
        printf("xllm v%d.%d.%d release metadata: %s (%d failures)\n", XLLM_VERSION_MAJOR,
               XLLM_VERSION_MINOR, XLLM_VERSION_PATCH, g_iFailures ? "FAIL" : "PASS", g_iFailures);
        return g_iFailures ? 1 : 0;
    }
    if (argc == 2 && strcmp(argv[1], "--offline-contracts") == 0)
    {
        test_release_metadata();
        test_model_profiles();
        test_provider_golden();
        test_parser_corpus();
        test_build_json();
        test_v3_model();
        test_v3_blocks_and_refusal();
        test_dialect_anthropic();
        test_signed_response();
        test_dialect_responses();
        test_audit_hardening();
        test_wire_store_flags();
        test_lifecycle_hooks();
        test_history_container();
        test_fragmented_parser();
        test_malformed_parser();
        printf("xllm v%d.%d.%d offline contracts: %s (%d failures)\n", XLLM_VERSION_MAJOR,
               XLLM_VERSION_MINOR, XLLM_VERSION_PATCH, g_iFailures ? "FAIL" : "PASS", g_iFailures);
        return g_iFailures ? 1 : 0;
    }
    if (argc == 2 && strcmp(argv[1], "--transport-contracts") == 0)
    {
        test_keepalive_and_concurrency();
        test_tls_transport();
        test_tls_private_ca();
        test_retry_idempotency_boundary();
        test_transport();
        printf("xllm v%d.%d.%d transport contracts: %s (%d failures)\n", XLLM_VERSION_MAJOR,
               XLLM_VERSION_MINOR, XLLM_VERSION_PATCH, g_iFailures ? "FAIL" : "PASS", g_iFailures);
        return g_iFailures ? 1 : 0;
    }
    test_model_profiles();
    test_provider_golden();
    test_parser_corpus();
    test_build_json();
    test_v3_model();
    test_v3_blocks_and_refusal();
    test_dialect_anthropic();
    test_signed_response();
    test_dialect_responses();
    test_keepalive_and_concurrency();
    test_op_slot_completion_remove_race();
    test_async_engine();
    test_oom_injection();
    test_public_mutator_oom();
    test_public_client_oom();
    test_adapter_assembly_oom();
    test_audit_hardening();
    test_wire_store_flags();
    test_lifecycle_hooks();
    test_history_container();
    test_tls_transport();
    test_tls_private_ca();
    test_fragmented_parser();
    test_malformed_parser();
    test_retry_idempotency_boundary();
    test_transport();
    printf("xllm v%d.%d.%d: %s (%d failures)\n", XLLM_VERSION_MAJOR, XLLM_VERSION_MINOR,
           XLLM_VERSION_PATCH, g_iFailures ? "FAIL" : "PASS", g_iFailures);
    return g_iFailures ? 1 : 0;
}
