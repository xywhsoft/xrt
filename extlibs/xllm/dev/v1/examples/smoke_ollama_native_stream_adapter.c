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
    int iApiChatCount;
    int iStreamTrueCount;
    int iSystemCount;
    int iMessageCount;
    int iTemperatureCount;
} demo_stream_server_state;

typedef struct {
    int iEventCount;
    int iTextDeltaCount;
    int iEndCount;
} demo_stream_event_state;

typedef struct {
    int iRequestTraceCount;
    int iResponseTraceCount;
    int iStreamPayloadTraceCount;
    int iStreamFinalizeTraceCount;
} demo_stream_trace_state;

typedef struct {
    int iOllamaLogCount;
} demo_stream_log_state;

static bool demo_send_all(SOCKET hSocket, const char *sData, size_t iLen)
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

static uint32 demo_stream_server_thread(ptr pParam)
{
    demo_stream_server_state *pState = (demo_stream_server_state *)pParam;
    SOCKET hClient = INVALID_SOCKET;
    char aBuffer[8192];
    const char *sBody =
        "{\"model\":\"llama-mock\",\"message\":{\"role\":\"assistant\",\"content\":\"echo: \"},\"done\":false}\n"
        "{\"model\":\"llama-mock\",\"message\":{\"role\":\"assistant\",\"content\":\"hello \"},\"done\":false}\n"
        "{\"model\":\"llama-mock\",\"message\":{\"role\":\"assistant\",\"content\":\"ollama stream\"},\"done\":false}\n"
        "{\"model\":\"llama-mock\",\"message\":{\"role\":\"assistant\",\"content\":\"\"},\"done\":true,\"done_reason\":\"stop\",\"prompt_eval_count\":8,\"eval_count\":4}\n";
    char sResponse[16384];
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
        if ( strstr(aBuffer, "POST /api/chat HTTP/1.1") != NULL ) {
            ++pState->iApiChatCount;
        }
        if ( strstr(aBuffer, "\"stream\":true") != NULL ) {
            ++pState->iStreamTrueCount;
        }
        if ( strstr(aBuffer, "\"role\":\"system\",\"content\":\"You are ollama stream test\"") != NULL ) {
            ++pState->iSystemCount;
        }
        if ( strstr(aBuffer, "\"role\":\"user\",\"content\":\"hello ollama stream\"") != NULL ) {
            ++pState->iMessageCount;
        }
        if ( strstr(aBuffer, "\"temperature\":0.4") != NULL ) {
            ++pState->iTemperatureCount;
        }
    }

    iRespLen = snprintf(
        sResponse,
        sizeof(sResponse),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/x-ndjson\r\n"
        "Connection: close\r\n"
        "Content-Length: %u\r\n"
        "\r\n"
        "%s",
        (unsigned)strlen(sBody),
        sBody
    );

    if ( iRespLen > 0 ) {
        (void)demo_send_all(hClient, sResponse, (size_t)iRespLen);
    }

    closesocket(hClient);
    closesocket(pState->hListen);
    pState->hListen = INVALID_SOCKET;
    return 0u;
}

static bool demo_stream_on_event(const xllm_event *pEvent, void *pUserData)
{
    demo_stream_event_state *pState = (demo_stream_event_state *)pUserData;

    if ( pState ) {
        ++pState->iEventCount;
    }

    if ( pEvent && pEvent->eType == XLLM_EVENT_TEXT_DELTA ) {
        if ( pState ) {
            ++pState->iTextDeltaCount;
        }
        printf("stream: %s\n", pEvent->as.tTextDelta.sText ? pEvent->as.tTextDelta.sText : "");
    }
    if ( pEvent && pEvent->eType == XLLM_EVENT_END && pState ) {
        ++pState->iEndCount;
    }

    return true;
}

static void demo_stream_trace_callback(void *pCtx, xllm_trace_kind eKind, const xvalue *pPayload)
{
    demo_stream_trace_state *pState = (demo_stream_trace_state *)pCtx;
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
    } else if ( eKind == XLLM_TRACE_STREAM && sPhase && strcmp(sPhase, "payload") == 0 ) {
        ++pState->iStreamPayloadTraceCount;
    } else if ( eKind == XLLM_TRACE_STREAM && sPhase && strcmp(sPhase, "finalize") == 0 ) {
        ++pState->iStreamFinalizeTraceCount;
    }
}

static void demo_stream_log_callback(void *pCtx, xllm_log_level eLevel, const char *sComponent, const char *sMessage)
{
    demo_stream_log_state *pState = (demo_stream_log_state *)pCtx;

    (void)eLevel;
    (void)sMessage;

    if ( !pState || !sComponent ) {
        return;
    }

    if ( strcmp(sComponent, "xllm.ollama_native") == 0 ) {
        ++pState->iOllamaLogCount;
    }
}

int main(void)
{
#if defined(_WIN32) || defined(_WIN64)
    WSADATA tWsaData;
#endif
    demo_stream_server_state tServer;
    demo_stream_event_state tEvents;
    demo_stream_trace_state tTrace;
    demo_stream_log_state tLog;
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
    memset(&tEvents, 0, sizeof(tEvents));
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

    hServerThread = xrtThreadCreate((ptr)demo_stream_server_thread, &tServer, 0);
    if ( !hServerThread ) {
        fprintf(stderr, "create server thread failed\n");
        return 6;
    }

    if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || !pRuntime ) {
        fprintf(stderr, "create runtime failed\n");
        return 7;
    }
    if ( xllm_runtime_set_trace_callback(pRuntime, demo_stream_trace_callback, &tTrace) != XRT_NET_OK ) {
        fprintf(stderr, "set trace callback failed\n");
        return 8;
    }
    if ( xllm_runtime_set_log_callback(pRuntime, demo_stream_log_callback, &tLog) != XRT_NET_OK ) {
        fprintf(stderr, "set log callback failed\n");
        return 9;
    }
    if ( xllm_register_ollama_native_adapter(pRuntime) != XRT_NET_OK ) {
        fprintf(stderr, "register built-in adapter failed\n");
        return 10;
    }

    (void)snprintf(sBaseUrl, sizeof(sBaseUrl), "http://127.0.0.1:%u", (unsigned)tServer.uPort);
    xllm_profile_init(&tProfile);
    tProfile.sId = "ollama-stream";
    tProfile.sProvider = "ollama";
    tProfile.sAdapter = XLLM_ADAPTER_OLLAMA_NATIVE;
    tProfile.sBaseUrl = sBaseUrl;
    tProfile.tAuth.eKind = XLLM_AUTH_NONE;
    tProfile.tModels.tText.sModelId = "llama-mock";
    tProfile.tModels.tText.tCaps.uFlags = XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT | XLLM_CAP_STREAM;
    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "register profile failed\n");
        return 11;
    }

    tCreate.sInitialProfileId = "ollama-stream";
    tCreate.sSystemPrompt = "You are ollama stream test";
    pLlm = xllm_create(pRuntime, &tCreate);
    if ( !pLlm ) {
        fprintf(stderr, "create llm failed\n");
        return 12;
    }

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "hello ollama stream") != XRT_NET_OK ) {
        fprintf(stderr, "turn add text failed\n");
        return 13;
    }
    tTurn.tGeneration.tTemperature.bSet = true;
    tTurn.tGeneration.tTemperature.fValue = 0.4;

    xllm_call_options_init(&tCall);
    tCall.eStreamMode = XLLM_STREAM_REQUIRE;
    tCall.pfnOnEvent = demo_stream_on_event;
    tCall.pUserData = &tEvents;

    if ( xllm_send(pLlm, &tTurn, &tCall, &pResponse) != XRT_NET_OK || !pResponse ) {
        fprintf(stderr, "xllm_send failed\n");
        return 14;
    }

    sText = xllm_response_get_text(pResponse);
    printf("ok: %s\n", sText ? sText : "(null)");

    if ( !sText || strcmp(sText, "echo: hello ollama stream") != 0 ) {
        fprintf(stderr, "unexpected response text\n");
        iStatus = 15;
    } else if ( pResponse->tUsage.uInputTokens != 8u || pResponse->tUsage.uOutputTokens != 4u ) {
        fprintf(stderr, "unexpected usage counts\n");
        iStatus = 16;
    } else if ( tServer.iApiChatCount != 1 || tServer.iStreamTrueCount != 1 ) {
        fprintf(stderr, "unexpected stream request counts\n");
        iStatus = 17;
    } else if ( tServer.iSystemCount != 1 || tServer.iMessageCount != 1 ) {
        fprintf(stderr, "request body missing expected messages\n");
        iStatus = 18;
    } else if ( tServer.iTemperatureCount != 1 ) {
        fprintf(stderr, "request body missing expected temperature\n");
        iStatus = 19;
    } else if ( tEvents.iTextDeltaCount != 3 || tEvents.iEndCount != 1 ) {
        fprintf(stderr, "unexpected event counts\n");
        iStatus = 20;
    } else if ( tTrace.iRequestTraceCount < 1 || tTrace.iResponseTraceCount < 1 ) {
        fprintf(stderr, "missing request/response trace\n");
        iStatus = 21;
    } else if ( tTrace.iStreamPayloadTraceCount < 3 || tTrace.iStreamFinalizeTraceCount < 1 ) {
        fprintf(stderr, "missing stream trace\n");
        iStatus = 22;
    } else if ( tLog.iOllamaLogCount < 2 ) {
        fprintf(stderr, "missing adapter logs\n");
        iStatus = 23;
    }

    xllm_response_free(pResponse);
    xllm_destroy(pLlm);
    xllm_runtime_destroy(pRuntime);
    xllm_turn_reset(&tTurn);

    xrtThreadWait(hServerThread);

#if defined(_WIN32) || defined(_WIN64)
    WSACleanup();
#endif

    return iStatus;
}
