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
    int iRequestCount;
    int iFixedTemperatureCount;
    int iUnexpectedTopPCount;
    int iStopCount;
} demo_server_state;

typedef struct {
    int iRequestTraceCount;
    int iResponseTraceCount;
} demo_trace_state;

typedef struct {
    int iOpenAILogCount;
} demo_log_state;

static uint32 demo_server_thread(ptr pParam)
{
    demo_server_state *pState = (demo_server_state *)pParam;
    SOCKET hClient = INVALID_SOCKET;
    char aBuffer[4096];
    const char *sBody =
        "{\"id\":\"chatcmpl-mock\","
        "\"object\":\"chat.completion\","
        "\"model\":\"gpt-mock\","
        "\"choices\":[{\"index\":0,\"message\":{\"role\":\"assistant\",\"content\":\"echo: hello openai\"},\"finish_reason\":\"stop\"}],"
        "\"usage\":{\"prompt_tokens\":8,\"completion_tokens\":4}}";
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
        if ( strstr(aBuffer, "Authorization: Bearer test-key") != NULL ) {
            ++pState->iRequestCount;
        }
        if ( strstr(aBuffer, "\"temperature\":1") != NULL ) {
            ++pState->iFixedTemperatureCount;
        }
        if ( strstr(aBuffer, "\"top_p\":") != NULL ) {
            ++pState->iUnexpectedTopPCount;
        }
        if ( strstr(aBuffer, "\"stop\":\"<END>\"") != NULL ) {
            ++pState->iStopCount;
        }
    }

    iRespLen = snprintf(
        sResponse,
        sizeof(sResponse),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
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

static bool demo_on_event(const xllm_event *pEvent, void *pUserData)
{
    int *piEventCount = (int *)pUserData;

    if ( piEventCount ) {
        ++(*piEventCount);
    }

    if ( pEvent && pEvent->eType == XLLM_EVENT_TEXT_DELTA ) {
        printf("stream: %s\n", pEvent->as.tTextDelta.sText ? pEvent->as.tTextDelta.sText : "");
    }

    return true;
}

static void demo_trace_callback(void *pCtx, xllm_trace_kind eKind, const xvalue *pPayload)
{
    demo_trace_state *pState = (demo_trace_state *)pCtx;
    const char *sPhase;
    const char *sResponseStatus;

    if ( !pState || !pPayload || !*pPayload ) {
        return;
    }

    sPhase = (const char *)xvoTableGetText(*pPayload, (str)"phase", 0u);
    if ( eKind == XLLM_TRACE_REQUEST && sPhase && strcmp(sPhase, "request") == 0 ) {
        ++pState->iRequestTraceCount;
    } else if ( eKind == XLLM_TRACE_RESPONSE && sPhase && strcmp(sPhase, "response") == 0 ) {
        sResponseStatus = (const char *)xvoTableGetText(*pPayload, (str)"response_status", 0u);
        if ( sResponseStatus && strcmp(sResponseStatus, "completed") == 0 ) {
            ++pState->iResponseTraceCount;
        }
    }
}

static void demo_log_callback(void *pCtx, xllm_log_level eLevel, const char *sComponent, const char *sMessage)
{
    demo_log_state *pState = (demo_log_state *)pCtx;

    (void)eLevel;
    (void)sMessage;

    if ( !pState || !sComponent ) {
        return;
    }

    if ( strcmp(sComponent, "xllm.openai_compat") == 0 ) {
        ++pState->iOpenAILogCount;
    }
}

int main(void)
{
#if defined(_WIN32) || defined(_WIN64)
    WSADATA tWsaData;
#endif
    demo_server_state tServer;
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
    demo_trace_state tTrace;
    demo_log_state tLog;
    char sBaseUrl[128];
    const char *arrStop[1];
    const char *sText;
    int iEventCount = 0;
    int iStatus = 0;

#if defined(_WIN32) || defined(_WIN64)
    if ( WSAStartup(MAKEWORD(2, 2), &tWsaData) != 0 ) {
        fprintf(stderr, "winsock startup failed\n");
        return 1;
    }
#endif

    memset(&tServer, 0, sizeof(tServer));
    memset(&tAddr, 0, sizeof(tAddr));
    memset(&tCreate, 0, sizeof(tCreate));
    memset(&tTrace, 0, sizeof(tTrace));
    memset(&tLog, 0, sizeof(tLog));

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

    hServerThread = xrtThreadCreate((ptr)demo_server_thread, &tServer, 0);
    if ( !hServerThread ) {
        fprintf(stderr, "create server thread failed\n");
        return 6;
    }

    if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || !pRuntime ) {
        fprintf(stderr, "create runtime failed\n");
        return 7;
    }
    if ( xllm_runtime_set_trace_callback(pRuntime, demo_trace_callback, &tTrace) != XRT_NET_OK ) {
        fprintf(stderr, "set trace callback failed\n");
        return 8;
    }
    if ( xllm_runtime_set_log_callback(pRuntime, demo_log_callback, &tLog) != XRT_NET_OK ) {
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
    tProfile.tModels.tText.tCaps.tTemperatureRule.eKind = XLLM_PARAM_RULE_FIXED;
    tProfile.tModels.tText.tCaps.tTemperatureRule.fFixed = 1.0;
    tProfile.tModels.tText.tCaps.tTopPRule.eKind = XLLM_PARAM_RULE_UNSUPPORTED;
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
    if ( xllm_turn_add_user_text(&tTurn, "hello openai") != XRT_NET_OK ) {
        fprintf(stderr, "turn add text failed\n");
        return 13;
    }
    tTurn.tGeneration.tTemperature.bSet = true;
    tTurn.tGeneration.tTemperature.fValue = 0.2;
    tTurn.tGeneration.tTopP.bSet = true;
    tTurn.tGeneration.tTopP.fValue = 0.3;
    arrStop[0] = "<END>";
    if ( xllm_turn_set_stop_sequences(&tTurn, arrStop, 1u) != XRT_NET_OK ) {
        fprintf(stderr, "turn set stop failed\n");
        return 14;
    }

    xllm_call_options_init(&tCall);
    tCall.pfnOnEvent = demo_on_event;
    tCall.pUserData = &iEventCount;

    iStatus = xllm_send(pLlm, &tTurn, &tCall, &pResponse);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "send failed: %d\n", iStatus);
        return 15;
    }

    xrtThreadWait(hServerThread);
    xrtThreadDestroy(hServerThread);
    hServerThread = NULL;

    sText = xllm_response_get_text(pResponse);
    if ( !sText || strcmp(sText, "echo: hello openai") != 0 ) {
        fprintf(stderr, "unexpected response text: %s\n", sText ? sText : "(null)");
        return 16;
    }
    if ( iEventCount < 5 ) {
        fprintf(stderr, "unexpected event count: %d\n", iEventCount);
        return 17;
    }
    if ( tServer.iRequestCount != 1 ) {
        fprintf(stderr, "unexpected http request count: %d\n", tServer.iRequestCount);
        return 18;
    }
    if ( tServer.iFixedTemperatureCount != 1 || tServer.iUnexpectedTopPCount != 0 || tServer.iStopCount != 1 ) {
        fprintf(
            stderr,
            "unexpected normalized request body counters: temp=%d top_p=%d stop=%d\n",
            tServer.iFixedTemperatureCount,
            tServer.iUnexpectedTopPCount,
            tServer.iStopCount
        );
        return 19;
    }
    if ( !pResponse->tEffectiveParams.tGeneration.tTemperature.bSet ||
         pResponse->tEffectiveParams.tGeneration.tTemperature.fValue != 1.0 ||
         pResponse->tEffectiveParams.tGeneration.tTopP.bSet ) {
        fprintf(stderr, "unexpected effective params normalization result\n");
        return 20;
    }
    if ( tTrace.iRequestTraceCount < 1 || tTrace.iResponseTraceCount < 1 ) {
        fprintf(stderr, "unexpected trace counts: request=%d response=%d\n", tTrace.iRequestTraceCount, tTrace.iResponseTraceCount);
        return 21;
    }
    if ( tLog.iOpenAILogCount < 2 ) {
        fprintf(stderr, "unexpected openai log count: %d\n", tLog.iOpenAILogCount);
        return 22;
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
