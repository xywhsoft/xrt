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
    int iApiKeyCount;
    int iVersionCount;
    int iThinkingTypeCount;
    int iThinkingBudgetCount;
    int iThinkingDisplayCount;
    int iStreamRequestCount;
} demo_server_state;

typedef struct {
    int iThinkingDeltaCount;
    int iTextDeltaCount;
    int iEndCount;
} demo_event_state;

typedef struct {
    int iRequestTraceCount;
    int iResponseTraceCount;
    int iStreamPayloadTraceCount;
    int iStreamFinalizeTraceCount;
} demo_trace_state;

typedef struct {
    int iAnthropicLogCount;
} demo_log_state;

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

static bool demo_send_chunk(SOCKET hSocket, const char *sBody)
{
    char sHeader[64];
    int iHeaderLen;
    size_t iBodyLen = sBody ? strlen(sBody) : 0u;

    iHeaderLen = snprintf(sHeader, sizeof(sHeader), "%x\r\n", (unsigned)iBodyLen);
    if ( iHeaderLen <= 0 ) {
        return false;
    }

    if ( !demo_send_all(hSocket, sHeader, (size_t)iHeaderLen) ) {
        return false;
    }
    if ( iBodyLen > 0u && !demo_send_all(hSocket, sBody, iBodyLen) ) {
        return false;
    }
    return demo_send_all(hSocket, "\r\n", 2u);
}

static uint32 demo_server_thread(ptr pParam)
{
    demo_server_state *pState = (demo_server_state *)pParam;
    SOCKET hClient = INVALID_SOCKET;
    char aBuffer[8192];
    const char *sHeader =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream\r\n"
        "request-id: req_anthropic_thinking_stream_1\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: close\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n";
    const char *sEvent1 =
        "data: {\"type\":\"message_start\",\"message\":{\"id\":\"msg_stream_thinking\",\"type\":\"message\",\"role\":\"assistant\",\"model\":\"claude-thinking-mock\",\"usage\":{\"input_tokens\":10,\"output_tokens\":0}}}\n\n";
    const char *sEvent2 =
        "data: {\"type\":\"content_block_start\",\"index\":0,\"content_block\":{\"type\":\"thinking\",\"thinking\":\"\"}}\n\n";
    const char *sEvent3 =
        "data: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"thinking_delta\",\"thinking\":\"Plan \"}}\n\n";
    const char *sEvent4 =
        "data: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"signature_delta\",\"signature\":\"sig_mock\"}}\n\n";
    const char *sEvent5 =
        "data: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"thinking_delta\",\"thinking\":\"done.\"}}\n\n";
    const char *sEvent6 =
        "data: {\"type\":\"content_block_start\",\"index\":1,\"content_block\":{\"type\":\"text\",\"text\":\"answer \"}}\n\n";
    const char *sEvent7 =
        "data: {\"type\":\"content_block_delta\",\"index\":1,\"delta\":{\"type\":\"text_delta\",\"text\":\"ok\"}}\n\n";
    const char *sEvent8 =
        "data: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"end_turn\"},\"usage\":{\"output_tokens\":3}}\n\n";
    const char *sEvent9 = "data: {\"type\":\"message_stop\"}\n\n";
    int iRecv;

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
            ++pState->iApiKeyCount;
        }
        if ( strstr(aBuffer, "anthropic-version: 2023-06-01") != NULL ) {
            ++pState->iVersionCount;
        }
        if ( strstr(aBuffer, "\"thinking\":{\"type\":\"enabled\"") != NULL ) {
            ++pState->iThinkingTypeCount;
        }
        if ( strstr(aBuffer, "\"budget_tokens\":2048") != NULL ) {
            ++pState->iThinkingBudgetCount;
        }
        if ( strstr(aBuffer, "\"display\":\"summarized\"") != NULL ) {
            ++pState->iThinkingDisplayCount;
        }
        if ( strstr(aBuffer, "\"stream\":true") != NULL ) {
            ++pState->iStreamRequestCount;
        }
    }

    if ( !demo_send_all(hClient, sHeader, strlen(sHeader)) ) {
        closesocket(hClient);
        closesocket(pState->hListen);
        pState->hListen = INVALID_SOCKET;
        return 3u;
    }

    if ( !demo_send_chunk(hClient, sEvent1) ) return 4u;
    xrtSleep(20);
    if ( !demo_send_chunk(hClient, sEvent2) ) return 5u;
    xrtSleep(20);
    if ( !demo_send_chunk(hClient, sEvent3) ) return 6u;
    xrtSleep(20);
    if ( !demo_send_chunk(hClient, sEvent4) ) return 7u;
    xrtSleep(20);
    if ( !demo_send_chunk(hClient, sEvent5) ) return 8u;
    xrtSleep(20);
    if ( !demo_send_chunk(hClient, sEvent6) ) return 9u;
    xrtSleep(20);
    if ( !demo_send_chunk(hClient, sEvent7) ) return 10u;
    xrtSleep(20);
    if ( !demo_send_chunk(hClient, sEvent8) ) return 11u;
    xrtSleep(20);
    if ( !demo_send_chunk(hClient, sEvent9) ) return 12u;
    if ( !demo_send_all(hClient, "0\r\n\r\n", 5u) ) return 13u;

    closesocket(hClient);
    closesocket(pState->hListen);
    pState->hListen = INVALID_SOCKET;
    return 0u;
}

static bool demo_on_event(const xllm_event *pEvent, void *pUserData)
{
    demo_event_state *pState = (demo_event_state *)pUserData;

    if ( !pEvent ) {
        return true;
    }

    if ( pEvent->eType == XLLM_EVENT_THINKING_DELTA ) {
        if ( pState ) {
            ++pState->iThinkingDeltaCount;
        }
    } else if ( pEvent->eType == XLLM_EVENT_TEXT_DELTA ) {
        if ( pState ) {
            ++pState->iTextDeltaCount;
        }
        printf("stream: %s\n", pEvent->as.tTextDelta.sText ? pEvent->as.tTextDelta.sText : "");
    } else if ( pEvent->eType == XLLM_EVENT_END ) {
        if ( pState ) {
            ++pState->iEndCount;
        }
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
    } else if ( eKind == XLLM_TRACE_STREAM && sPhase && strcmp(sPhase, "payload") == 0 ) {
        ++pState->iStreamPayloadTraceCount;
    } else if ( eKind == XLLM_TRACE_STREAM && sPhase && strcmp(sPhase, "finalize") == 0 ) {
        ++pState->iStreamFinalizeTraceCount;
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

    if ( strcmp(sComponent, "xllm.anthropic_native") == 0 ) {
        ++pState->iAnthropicLogCount;
    }
}

int main(void)
{
#if defined(_WIN32) || defined(_WIN64)
    WSADATA tWsaData;
#endif
    demo_server_state tServer;
    demo_event_state tEvents;
    demo_trace_state tTrace;
    demo_log_state tLog;
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
    if ( xllm_register_anthropic_native_adapter(pRuntime) != XRT_NET_OK ) {
        fprintf(stderr, "register built-in adapter failed\n");
        return 10;
    }

    (void)snprintf(sBaseUrl, sizeof(sBaseUrl), "http://127.0.0.1:%u/v1", (unsigned)tServer.uPort);
    xllm_profile_init(&tProfile);
    tProfile.sId = "anthropic-thinking-stream";
    tProfile.sProvider = "anthropic";
    tProfile.sAdapter = XLLM_ADAPTER_ANTHROPIC_NATIVE;
    tProfile.sBaseUrl = sBaseUrl;
    tProfile.tAuth.eKind = XLLM_AUTH_API_KEY_HEADER;
    tProfile.tAuth.sSecret = "test-key";
    tProfile.tModels.tText.sModelId = "claude-thinking-mock";
    tProfile.tModels.tText.eCapMode = XLLM_CAP_MODE_EXACT;
    tProfile.tModels.tText.tCaps.uFlags =
        XLLM_CAP_TEXT_IN |
        XLLM_CAP_TEXT_OUT |
        XLLM_CAP_STREAM |
        XLLM_CAP_REASONING_CONTROL |
        XLLM_CAP_THINKING_SUMMARY_OUT;
    tProfile.tDefaults.tGeneration.tMaxOutputTokens.bSet = true;
    tProfile.tDefaults.tGeneration.tMaxOutputTokens.iValue = 4096u;
    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "register profile failed\n");
        return 11;
    }

    tCreate.sInitialProfileId = "anthropic-thinking-stream";
    pLlm = xllm_create(pRuntime, &tCreate);
    if ( !pLlm ) {
        fprintf(stderr, "create llm failed\n");
        return 12;
    }

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "stream your thinking") != XRT_NET_OK ) {
        fprintf(stderr, "turn add text failed\n");
        return 13;
    }
    tTurn.tReasoning.tEnabled.bSet = true;
    tTurn.tReasoning.tEnabled.bValue = true;
    tTurn.tReasoning.tBudgetTokens.bSet = true;
    tTurn.tReasoning.tBudgetTokens.iValue = 2048u;
    tTurn.tReasoning.tExposeThinking.bSet = true;
    tTurn.tReasoning.tExposeThinking.bValue = true;

    xllm_call_options_init(&tCall);
    tCall.eStreamMode = XLLM_STREAM_REQUIRE;
    tCall.pfnOnEvent = demo_on_event;
    tCall.pUserData = &tEvents;

    iStatus = xllm_send(pLlm, &tTurn, &tCall, &pResponse);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "send failed: %d\n", iStatus);
        return 14;
    }

    xrtThreadWait(hServerThread);
    xrtThreadDestroy(hServerThread);
    hServerThread = NULL;

    sText = xllm_response_get_text(pResponse);
    if ( !sText || strcmp(sText, "answer ok") != 0 ) {
        fprintf(stderr, "unexpected response text: %s\n", sText ? sText : "(null)");
        return 15;
    }
    if ( pResponse->iOutputCount < 2 ||
         pResponse->pOutputs[0].eKind != XLLM_OUTPUT_THINKING ||
         !pResponse->pOutputs[0].as.tThinking.sText ||
         strcmp(pResponse->pOutputs[0].as.tThinking.sText, "Plan done.") != 0 ) {
        fprintf(stderr, "unexpected thinking output\n");
        return 16;
    }
    if ( tServer.iApiKeyCount != 1 ||
         tServer.iVersionCount != 1 ||
         tServer.iThinkingTypeCount != 1 ||
         tServer.iThinkingBudgetCount != 1 ||
         tServer.iThinkingDisplayCount != 1 ||
         tServer.iStreamRequestCount != 1 ) {
        fprintf(
            stderr,
            "unexpected request counters: key=%d version=%d type=%d budget=%d display=%d stream=%d\n",
            tServer.iApiKeyCount,
            tServer.iVersionCount,
            tServer.iThinkingTypeCount,
            tServer.iThinkingBudgetCount,
            tServer.iThinkingDisplayCount,
            tServer.iStreamRequestCount
        );
        return 17;
    }
    if ( tEvents.iThinkingDeltaCount != 2 || tEvents.iTextDeltaCount != 2 || tEvents.iEndCount != 1 ) {
        fprintf(
            stderr,
            "unexpected event counts: thinking=%d text=%d end=%d\n",
            tEvents.iThinkingDeltaCount,
            tEvents.iTextDeltaCount,
            tEvents.iEndCount
        );
        return 18;
    }
    if ( pResponse->tUsage.uInputTokens != 10u || pResponse->tUsage.uOutputTokens != 3u ) {
        fprintf(stderr, "unexpected usage: in=%u out=%u\n", pResponse->tUsage.uInputTokens, pResponse->tUsage.uOutputTokens);
        return 19;
    }
    if ( tTrace.iRequestTraceCount < 1 || tTrace.iResponseTraceCount < 1 ) {
        fprintf(stderr, "missing request/response trace\n");
        return 20;
    }
    if ( tTrace.iStreamPayloadTraceCount < 7 || tTrace.iStreamFinalizeTraceCount < 1 ) {
        fprintf(stderr, "missing stream trace\n");
        return 21;
    }
    if ( tLog.iAnthropicLogCount < 2 ) {
        fprintf(stderr, "missing anthropic adapter logs\n");
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
