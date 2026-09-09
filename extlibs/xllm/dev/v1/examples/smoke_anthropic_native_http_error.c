#include "xllm-session.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#define closesocket close
#define SOCKET int
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#endif

typedef struct {
    SOCKET hListen;
    uint16 uPort;
    int iAuthedRequestCount;
} demo_http_error_server_state;

typedef struct {
    int iRequestTraceCount;
    int iFailedResponseTraceCount;
    int iRateLimitTraceCount;
    int iHttp429TraceCount;
    int iRequestIdTraceCount;
    int iRetryableTraceCount;
} demo_http_error_trace_state;

typedef struct {
    int iAnthropicLogCount;
    int iWarnLogCount;
} demo_http_error_log_state;

static uint32 demo_http_error_server_thread(ptr pParam)
{
    demo_http_error_server_state *pState = (demo_http_error_server_state *)pParam;
    SOCKET hClient = INVALID_SOCKET;
    char aBuffer[4096];
    const char *sBody =
        "{\"type\":\"error\",\"error\":{\"type\":\"rate_limit_error\",\"message\":\"rate limited\"}}";
    char sResponse[8192];
    int iRecv;
    int iRespLen;

    if ( !pState || pState->hListen == INVALID_SOCKET ) {
        return 1u;
    }

    hClient = accept(pState->hListen, NULL, NULL);
    if ( hClient == INVALID_SOCKET ) {
        closesocket(pState->hListen);
        pState->hListen = INVALID_SOCKET;
        return 2u;
    }

    iRecv = recv(hClient, aBuffer, (int)(sizeof(aBuffer) - 1u), 0);
    if ( iRecv > 0 ) {
        aBuffer[iRecv] = '\0';
        if ( strstr(aBuffer, "x-api-key: test-key") != NULL ) {
            ++pState->iAuthedRequestCount;
        }
    }

    iRespLen = snprintf(
        sResponse,
        sizeof(sResponse),
        "HTTP/1.1 429 Too Many Requests\r\n"
        "Content-Type: application/json\r\n"
        "request-id: req-anthropic-rate-limit-1\r\n"
        "Connection: close\r\n"
        "Content-Length: %u\r\n"
        "\r\n"
        "%s",
        (unsigned)strlen(sBody),
        sBody
    );

    if ( iRespLen > 0 ) {
        size_t iSent = 0u;
        while ( iSent < (size_t)iRespLen ) {
            int iNow = send(hClient, sResponse + iSent, (int)((size_t)iRespLen - iSent), 0);
            if ( iNow <= 0 ) {
                break;
            }
            iSent += (size_t)iNow;
        }
    }

    closesocket(hClient);
    closesocket(pState->hListen);
    pState->hListen = INVALID_SOCKET;
    return 0u;
}

static void demo_http_error_trace_callback(void *pCtx, xllm_trace_kind eKind, const xvalue *pPayload)
{
    demo_http_error_trace_state *pState = (demo_http_error_trace_state *)pCtx;
    const char *sPhase;
    const char *sErrorCode;
    const char *sRequestId;

    if ( !pState || !pPayload || !*pPayload ) {
        return;
    }

    sPhase = (const char *)xvoTableGetText(*pPayload, (str)"phase", 0u);
    if ( eKind == XLLM_TRACE_REQUEST && sPhase && strcmp(sPhase, "request") == 0 ) {
        ++pState->iRequestTraceCount;
        return;
    }

    if ( eKind != XLLM_TRACE_RESPONSE || !sPhase || strcmp(sPhase, "response") != 0 ) {
        return;
    }

    if ( !xvoTableGetBool(*pPayload, (str)"success", 0u) ) {
        ++pState->iFailedResponseTraceCount;
    }
    if ( xvoTableGetBool(*pPayload, (str)"retryable", 0u) ) {
        ++pState->iRetryableTraceCount;
    }

    sErrorCode = (const char *)xvoTableGetText(*pPayload, (str)"error_code", 0u);
    if ( sErrorCode && strcmp(sErrorCode, "rate_limit") == 0 ) {
        ++pState->iRateLimitTraceCount;
    }

    if ( (int)xvoTableGetInt(*pPayload, (str)"http_status", 0u) == 429 ) {
        ++pState->iHttp429TraceCount;
    }

    sRequestId = (const char *)xvoTableGetText(*pPayload, (str)"request_id", 0u);
    if ( sRequestId && strcmp(sRequestId, "req-anthropic-rate-limit-1") == 0 ) {
        ++pState->iRequestIdTraceCount;
    }
}

static void demo_http_error_log_callback(void *pCtx, xllm_log_level eLevel, const char *sComponent, const char *sMessage)
{
    demo_http_error_log_state *pState = (demo_http_error_log_state *)pCtx;

    (void)sMessage;

    if ( !pState || !sComponent ) {
        return;
    }

    if ( strcmp(sComponent, "xllm.anthropic_native") == 0 ) {
        ++pState->iAnthropicLogCount;
        if ( eLevel == XLLM_LOG_WARN ) {
            ++pState->iWarnLogCount;
        }
    }
}

int main(void)
{
#if defined(_WIN32) || defined(_WIN64)
    WSADATA tWsaData;
#endif
    demo_http_error_server_state tServer;
    demo_http_error_trace_state tTrace;
    demo_http_error_log_state tLog;
    struct sockaddr_in tAddr;
    socklen_t iAddrLen = sizeof(tAddr);
    xthread hServerThread = NULL;
    xllm_runtime *pRuntime = NULL;
    xllm *pLlm = NULL;
    xllm_response *pResponse = NULL;
    xllm_profile tProfile;
    xllm_create_options tCreate;
    xllm_turn tTurn;
    char sBaseUrl[128];
    int iStatus = 0;

#if defined(_WIN32) || defined(_WIN64)
    if ( WSAStartup(MAKEWORD(2, 2), &tWsaData) != 0 ) {
        fprintf(stderr, "winsock startup failed\n");
        return 1;
    }
#endif

    memset(&tServer, 0, sizeof(tServer));
    memset(&tTrace, 0, sizeof(tTrace));
    memset(&tLog, 0, sizeof(tLog));
    memset(&tAddr, 0, sizeof(tAddr));
    memset(&tCreate, 0, sizeof(tCreate));

    tServer.hListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if ( tServer.hListen == INVALID_SOCKET ) {
        fprintf(stderr, "create listen socket failed\n");
        return 2;
    }

    tAddr.sin_family = AF_INET;
    tAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    tAddr.sin_port = htons(0);
    if ( bind(tServer.hListen, (struct sockaddr *)&tAddr, sizeof(tAddr)) == SOCKET_ERROR ) {
        fprintf(stderr, "bind listen socket failed\n");
        return 3;
    }
    if ( listen(tServer.hListen, 1) == SOCKET_ERROR ) {
        fprintf(stderr, "listen failed\n");
        return 4;
    }
    if ( getsockname(tServer.hListen, (struct sockaddr *)&tAddr, &iAddrLen) == SOCKET_ERROR ) {
        fprintf(stderr, "getsockname failed\n");
        return 5;
    }
    tServer.uPort = ntohs(tAddr.sin_port);

    hServerThread = xrtThreadCreate((ptr)demo_http_error_server_thread, &tServer, 0);
    if ( !hServerThread ) {
        fprintf(stderr, "create server thread failed\n");
        return 6;
    }

    if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || !pRuntime ) {
        fprintf(stderr, "create runtime failed\n");
        return 7;
    }
    if ( xllm_runtime_set_trace_callback(pRuntime, demo_http_error_trace_callback, &tTrace) != XRT_NET_OK ) {
        fprintf(stderr, "set trace callback failed\n");
        return 8;
    }
    if ( xllm_runtime_set_log_callback(pRuntime, demo_http_error_log_callback, &tLog) != XRT_NET_OK ) {
        fprintf(stderr, "set log callback failed\n");
        return 9;
    }

    if ( xllm_register_anthropic_native_adapter(pRuntime) != XRT_NET_OK ) {
        fprintf(stderr, "register built-in adapter failed\n");
        return 10;
    }

    (void)snprintf(sBaseUrl, sizeof(sBaseUrl), "http://127.0.0.1:%u/v1", (unsigned)tServer.uPort);
    xllm_profile_init(&tProfile);
    tProfile.sId = "anthropic-local";
    tProfile.sProvider = "anthropic";
    tProfile.sAdapter = XLLM_ADAPTER_ANTHROPIC_NATIVE;
    tProfile.sBaseUrl = sBaseUrl;
    tProfile.tAuth.eKind = XLLM_AUTH_API_KEY_HEADER;
    tProfile.tAuth.sSecret = "test-key";
    tProfile.tModels.tText.sModelId = "claude-mock";
    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "register profile failed\n");
        return 11;
    }

    tCreate.sInitialProfileId = "anthropic-local";
    tCreate.sSystemPrompt = "you are a mock anthropic server";
    pLlm = xllm_create(pRuntime, &tCreate);
    if ( !pLlm ) {
        fprintf(stderr, "create llm failed\n");
        return 12;
    }

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "hello anthropic") != XRT_NET_OK ) {
        fprintf(stderr, "turn add text failed\n");
        return 13;
    }

    iStatus = xllm_send(pLlm, &tTurn, NULL, &pResponse);
    if ( iStatus == XRT_NET_OK ) {
        fprintf(stderr, "send unexpectedly succeeded\n");
        return 14;
    }
    if ( pResponse ) {
        fprintf(stderr, "unexpected response object on failure\n");
        return 15;
    }

    xrtThreadWait(hServerThread);
    xrtThreadDestroy(hServerThread);
    hServerThread = NULL;

    if ( tServer.iAuthedRequestCount != 1 ) {
        fprintf(stderr, "unexpected auth request count: %d\n", tServer.iAuthedRequestCount);
        return 16;
    }
    if ( tTrace.iRequestTraceCount < 1 ||
         tTrace.iFailedResponseTraceCount < 1 ||
         tTrace.iRateLimitTraceCount < 1 ||
         tTrace.iHttp429TraceCount < 1 ||
         tTrace.iRequestIdTraceCount < 1 ) {
        fprintf(
            stderr,
            "unexpected trace counters: request=%d failed=%d rate_limit=%d http429=%d reqid=%d\n",
            tTrace.iRequestTraceCount,
            tTrace.iFailedResponseTraceCount,
            tTrace.iRateLimitTraceCount,
            tTrace.iHttp429TraceCount,
            tTrace.iRequestIdTraceCount
        );
        return 17;
    }
    if ( tTrace.iRetryableTraceCount < 1 ) {
        fprintf(stderr, "unexpected retryable trace count: %d\n", tTrace.iRetryableTraceCount);
        return 18;
    }
    if ( tLog.iAnthropicLogCount < 1 || tLog.iWarnLogCount < 1 ) {
        fprintf(stderr, "unexpected log counters: total=%d warn=%d\n", tLog.iAnthropicLogCount, tLog.iWarnLogCount);
        return 19;
    }

    printf("ok: anthropic http error traced\n");

    xllm_turn_reset(&tTurn);
    xllm_destroy(pLlm);
    xllm_runtime_destroy(pRuntime);
    if ( tServer.hListen != INVALID_SOCKET ) {
        closesocket(tServer.hListen);
    }
#if defined(_WIN32) || defined(_WIN64)
    WSACleanup();
#endif
    return 0;
}
