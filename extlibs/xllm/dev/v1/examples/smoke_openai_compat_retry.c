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
    int iAttemptCount;
} demo_retry_server_state;

typedef struct {
    int iRequestTraceCount;
    int iFailedResponseTraceCount;
    int iCompletedResponseTraceCount;
    int iRetryableTraceCount;
    int iAttemptOneTraceCount;
    int iAttemptTwoTraceCount;
} demo_retry_trace_state;

typedef struct {
    int iOpenAILogCount;
    int iWarnLogCount;
    int iInfoLogCount;
} demo_retry_log_state;

static bool demo_retry_send_all(SOCKET hSocket, const char *sData, size_t iLen)
{
    size_t iSent = 0u;

    while ( iSent < iLen ) {
        int iNow = send(hSocket, sData + iSent, (int)(iLen - iSent), 0);
        if ( iNow <= 0 ) {
            return false;
        }
        iSent += (size_t)iNow;
    }

    return true;
}

static uint32 demo_retry_server_thread(ptr pParam)
{
    demo_retry_server_state *pState = (demo_retry_server_state *)pParam;
    int iAttempt;

    if ( !pState || pState->hListen == INVALID_SOCKET ) {
        return 1u;
    }

    for ( iAttempt = 0; iAttempt < 2; ++iAttempt ) {
        SOCKET hClient = INVALID_SOCKET;
        char aBuffer[4096];
        int iRecv;

        hClient = accept(pState->hListen, NULL, NULL);
        if ( hClient == INVALID_SOCKET ) {
            closesocket(pState->hListen);
            pState->hListen = INVALID_SOCKET;
            return (uint32)(2 + iAttempt);
        }

        iRecv = recv(hClient, aBuffer, (int)(sizeof(aBuffer) - 1u), 0);
        if ( iRecv > 0 ) {
            aBuffer[iRecv] = '\0';
            if ( strstr(aBuffer, "Authorization: Bearer test-key") != NULL ) {
                ++pState->iAuthedRequestCount;
            }
        }
        ++pState->iAttemptCount;

        if ( iAttempt == 0 ) {
            const char *sBody = "{\"error\":{\"message\":\"rate limited\",\"code\":\"rate_limit\"}}";
            char sResponse[4096];
            int iRespLen = snprintf(
                sResponse,
                sizeof(sResponse),
                "HTTP/1.1 429 Too Many Requests\r\n"
                "Content-Type: application/json\r\n"
                "x-request-id: req-retry-1\r\n"
                "Connection: close\r\n"
                "Content-Length: %u\r\n"
                "\r\n"
                "%s",
                (unsigned)strlen(sBody),
                sBody
            );
            if ( iRespLen > 0 ) {
                (void)demo_retry_send_all(hClient, sResponse, (size_t)iRespLen);
            }
        } else {
            const char *sBody =
                "{\"id\":\"chatcmpl-retry\","
                "\"object\":\"chat.completion\","
                "\"model\":\"gpt-mock\","
                "\"choices\":[{\"index\":0,\"message\":{\"role\":\"assistant\",\"content\":\"echo: after retry\"},\"finish_reason\":\"stop\"}],"
                "\"usage\":{\"prompt_tokens\":9,\"completion_tokens\":5}}";
            char sResponse[8192];
            int iRespLen = snprintf(
                sResponse,
                sizeof(sResponse),
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/json\r\n"
                "x-request-id: req-retry-2\r\n"
                "Connection: close\r\n"
                "Content-Length: %u\r\n"
                "\r\n"
                "%s",
                (unsigned)strlen(sBody),
                sBody
            );
            if ( iRespLen > 0 ) {
                (void)demo_retry_send_all(hClient, sResponse, (size_t)iRespLen);
            }
        }

        closesocket(hClient);
    }

    closesocket(pState->hListen);
    pState->hListen = INVALID_SOCKET;
    return 0u;
}

static void demo_retry_trace_callback(void *pCtx, xllm_trace_kind eKind, const xvalue *pPayload)
{
    demo_retry_trace_state *pState = (demo_retry_trace_state *)pCtx;
    const char *sPhase;
    const char *sStatus;
    int iAttempt;

    if ( !pState || !pPayload || !*pPayload ) {
        return;
    }

    sPhase = (const char *)xvoTableGetText(*pPayload, (str)"phase", 0u);
    iAttempt = (int)xvoTableGetInt(*pPayload, (str)"attempt", 0u);
    if ( eKind == XLLM_TRACE_REQUEST && sPhase && strcmp(sPhase, "request") == 0 ) {
        ++pState->iRequestTraceCount;
        if ( iAttempt == 1 ) {
            ++pState->iAttemptOneTraceCount;
        } else if ( iAttempt == 2 ) {
            ++pState->iAttemptTwoTraceCount;
        }
        return;
    }

    if ( eKind != XLLM_TRACE_RESPONSE || !sPhase || strcmp(sPhase, "response") != 0 ) {
        return;
    }

    if ( xvoTableGetBool(*pPayload, (str)"retryable", 0u) ) {
        ++pState->iRetryableTraceCount;
    }

    sStatus = (const char *)xvoTableGetText(*pPayload, (str)"response_status", 0u);
    if ( sStatus && strcmp(sStatus, "errored") == 0 ) {
        ++pState->iFailedResponseTraceCount;
    } else if ( sStatus && strcmp(sStatus, "completed") == 0 ) {
        ++pState->iCompletedResponseTraceCount;
    }

    if ( iAttempt == 1 ) {
        ++pState->iAttemptOneTraceCount;
    } else if ( iAttempt == 2 ) {
        ++pState->iAttemptTwoTraceCount;
    }
}

static void demo_retry_log_callback(void *pCtx, xllm_log_level eLevel, const char *sComponent, const char *sMessage)
{
    demo_retry_log_state *pState = (demo_retry_log_state *)pCtx;

    (void)sMessage;

    if ( !pState || !sComponent ) {
        return;
    }

    if ( strcmp(sComponent, "xllm.openai_compat") == 0 ) {
        ++pState->iOpenAILogCount;
        if ( eLevel == XLLM_LOG_WARN ) {
            ++pState->iWarnLogCount;
        } else if ( eLevel == XLLM_LOG_INFO ) {
            ++pState->iInfoLogCount;
        }
    }
}

int main(void)
{
#if defined(_WIN32) || defined(_WIN64)
    WSADATA tWsaData;
#endif
    demo_retry_server_state tServer;
    demo_retry_trace_state tTrace;
    demo_retry_log_state tLog;
    struct sockaddr_in tAddr;
    socklen_t iAddrLen = sizeof(tAddr);
    xthread hServerThread = NULL;
    xllm_runtime *pRuntime = NULL;
    xllm *pLlm = NULL;
    xllm_response *pResponse = NULL;
    xllm_profile tProfile;
    xllm_create_options tCreate;
    xllm_turn tTurn;
    xllm_call_options tCall;
    char sBaseUrl[128];
    const char *sText;
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
    if ( listen(tServer.hListen, 2) == SOCKET_ERROR ) {
        fprintf(stderr, "listen failed\n");
        return 4;
    }
    if ( getsockname(tServer.hListen, (struct sockaddr *)&tAddr, &iAddrLen) == SOCKET_ERROR ) {
        fprintf(stderr, "getsockname failed\n");
        return 5;
    }
    tServer.uPort = ntohs(tAddr.sin_port);

    hServerThread = xrtThreadCreate((ptr)demo_retry_server_thread, &tServer, 0);
    if ( !hServerThread ) {
        fprintf(stderr, "create server thread failed\n");
        return 6;
    }

    if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || !pRuntime ) {
        fprintf(stderr, "create runtime failed\n");
        return 7;
    }
    if ( xllm_runtime_set_trace_callback(pRuntime, demo_retry_trace_callback, &tTrace) != XRT_NET_OK ) {
        fprintf(stderr, "set trace callback failed\n");
        return 8;
    }
    if ( xllm_runtime_set_log_callback(pRuntime, demo_retry_log_callback, &tLog) != XRT_NET_OK ) {
        fprintf(stderr, "set log callback failed\n");
        return 9;
    }
    if ( xllm_register_openai_compat_adapter(pRuntime) != XRT_NET_OK ) {
        fprintf(stderr, "register built-in adapter failed\n");
        return 10;
    }

    (void)snprintf(sBaseUrl, sizeof(sBaseUrl), "http://127.0.0.1:%u/v1", (unsigned)tServer.uPort);
    xllm_profile_init(&tProfile);
    tProfile.sId = "openai-local";
    tProfile.sProvider = "openai";
    tProfile.sAdapter = XLLM_ADAPTER_OPENAI_COMPAT;
    tProfile.sBaseUrl = sBaseUrl;
    tProfile.tAuth.eKind = XLLM_AUTH_BEARER;
    tProfile.tAuth.sSecret = "test-key";
    tProfile.tModels.tText.sModelId = "gpt-mock";
    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "register profile failed\n");
        return 11;
    }

    tCreate.sInitialProfileId = "openai-local";
    tCreate.sSystemPrompt = "you are a mock openai server";
    pLlm = xllm_create(pRuntime, &tCreate);
    if ( !pLlm ) {
        fprintf(stderr, "create llm failed\n");
        return 12;
    }

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "retry please") != XRT_NET_OK ) {
        fprintf(stderr, "turn add text failed\n");
        return 13;
    }

    xllm_call_options_init(&tCall);
    tCall.uMaxRetries = 1u;
    tCall.uRetryBackoffBaseMs = 1u;
    tCall.uRetryBackoffMaxMs = 1u;

    iStatus = xllm_send(pLlm, &tTurn, &tCall, &pResponse);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "send failed: %d\n", iStatus);
        return 14;
    }

    xrtThreadWait(hServerThread);
    xrtThreadDestroy(hServerThread);
    hServerThread = NULL;

    sText = xllm_response_get_text(pResponse);
    if ( !sText || strcmp(sText, "echo: after retry") != 0 ) {
        fprintf(stderr, "unexpected response text: %s\n", sText ? sText : "(null)");
        return 15;
    }
    if ( tServer.iAuthedRequestCount != 2 || tServer.iAttemptCount != 2 ) {
        fprintf(stderr, "unexpected request counters: auth=%d attempts=%d\n", tServer.iAuthedRequestCount, tServer.iAttemptCount);
        return 16;
    }
    if ( tTrace.iRequestTraceCount < 2 ||
         tTrace.iFailedResponseTraceCount < 1 ||
         tTrace.iCompletedResponseTraceCount < 1 ||
         tTrace.iRetryableTraceCount < 1 ||
         tTrace.iAttemptOneTraceCount < 2 ||
         tTrace.iAttemptTwoTraceCount < 2 ) {
        fprintf(
            stderr,
            "unexpected retry trace counts: req=%d fail=%d ok=%d retryable=%d a1=%d a2=%d\n",
            tTrace.iRequestTraceCount,
            tTrace.iFailedResponseTraceCount,
            tTrace.iCompletedResponseTraceCount,
            tTrace.iRetryableTraceCount,
            tTrace.iAttemptOneTraceCount,
            tTrace.iAttemptTwoTraceCount
        );
        return 17;
    }
    if ( tLog.iOpenAILogCount < 4 || tLog.iWarnLogCount < 1 || tLog.iInfoLogCount < 1 ) {
        fprintf(stderr, "unexpected log counts: total=%d warn=%d info=%d\n", tLog.iOpenAILogCount, tLog.iWarnLogCount, tLog.iInfoLogCount);
        return 18;
    }

    printf("ok: %s\n", sText);

    xllm_response_free(pResponse);
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
