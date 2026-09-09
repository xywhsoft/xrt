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
    int iToolDefCount;
    int iAssistantToolCallCount;
    int iToolResultCount;
} demo_server_state;

typedef struct {
    int iToolDeltaCount;
    int iToolReadyCount;
    int iTextDeltaCount;
    int iEndCount;
} demo_event_state;

typedef struct {
    int iRequestTraceCount;
    int iResponseTraceCount;
    int iStreamPayloadTraceCount;
    int iStreamFinalizeTraceCount;
    int iToolLoopRoundResponseCount;
    int iToolLoopToolResultCount;
    int iToolLoopStopFinalCount;
} demo_trace_state;

typedef struct {
    int iOllamaLogCount;
    int iToolLoopLogCount;
} demo_log_state;

static char *demo_dupstr(const char *sText)
{
    size_t iLen;
    char *sCopy;

    if ( !sText ) {
        return NULL;
    }

    iLen = strlen(sText);
    sCopy = (char *)xrtCalloc(iLen + 1u, sizeof(char));
    if ( !sCopy ) {
        return NULL;
    }

    memcpy(sCopy, sText, iLen);
    sCopy[iLen] = '\0';
    return sCopy;
}

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

static uint32 demo_server_thread(ptr pParam)
{
    demo_server_state *pState = (demo_server_state *)pParam;
    int iRound;

    if ( !pState || pState->hListen == INVALID_SOCKET ) {
        return 1u;
    }

    for ( iRound = 0; iRound < 2; ++iRound ) {
        SOCKET hClient = accept(pState->hListen, NULL, NULL);
        char aBuffer[8192];
        int iRecv;
        const char *sBody;
        char sResponse[16384];
        int iRespLen;

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
            if ( iRound == 0 ) {
                if ( strstr(aBuffer, "\"tools\":[{\"type\":\"function\",\"function\":{\"name\":\"get_weather\"") != NULL ) {
                    ++pState->iToolDefCount;
                }
            } else {
                if ( strstr(aBuffer, "\"tool_calls\":[{\"function\":{\"name\":\"get_weather\",\"arguments\":{\"city\":\"Shanghai\"}}}]") != NULL ) {
                    ++pState->iAssistantToolCallCount;
                }
                if ( strstr(aBuffer, "\"role\":\"tool\",\"tool_name\":\"get_weather\",\"content\":\"sunny\"") != NULL ) {
                    ++pState->iToolResultCount;
                }
            }
        }

        if ( iRound == 0 ) {
            sBody =
                "{\"model\":\"llama-mock\",\"message\":{\"role\":\"assistant\",\"tool_calls\":[{\"function\":{\"name\":\"get_weather\",\"arguments\":{\"city\":\"Shanghai\"}}}]},\"done\":false}\n"
                "{\"model\":\"llama-mock\",\"message\":{\"role\":\"assistant\",\"content\":\"\"},\"done\":true,\"done_reason\":\"stop\",\"prompt_eval_count\":12,\"eval_count\":3}\n";
        } else {
            sBody =
                "{\"model\":\"llama-mock\",\"message\":{\"role\":\"assistant\",\"content\":\"weather result: \"},\"done\":false}\n"
                "{\"model\":\"llama-mock\",\"message\":{\"role\":\"assistant\",\"content\":\"sunny\"},\"done\":false}\n"
                "{\"model\":\"llama-mock\",\"message\":{\"role\":\"assistant\",\"content\":\"\"},\"done\":true,\"done_reason\":\"stop\",\"prompt_eval_count\":18,\"eval_count\":4}\n";
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

        if ( iRespLen > 0 && !demo_send_all(hClient, sResponse, (size_t)iRespLen) ) {
            closesocket(hClient);
            closesocket(pState->hListen);
            pState->hListen = INVALID_SOCKET;
            return 3u;
        }

        closesocket(hClient);
    }

    closesocket(pState->hListen);
    pState->hListen = INVALID_SOCKET;
    return 0u;
}

static int32 demo_tool_execute(
    void *pCtx,
    const xllm_tool_exec_request *pRequest,
    xllm_tool_exec_result *pResult,
    xllm_error *pError
)
{
    (void)pCtx;
    (void)pError;

    if ( !pRequest || !pResult || !pRequest->sToolId ||
         strcmp(pRequest->sToolId, "app.weather.get_current") != 0 ) {
        return XRT_NET_ERROR;
    }

    pResult->pParts = (xllm_content_part *)xrtCalloc(1u, sizeof(xllm_content_part));
    if ( !pResult->pParts ) {
        return XRT_NET_ERROR;
    }

    pResult->iPartCount = 1u;
    pResult->pParts[0].eKind = XLLM_PART_TEXT;
    pResult->pParts[0].as.tSource.eKind = XLLM_SOURCE_INLINE_TEXT;
    pResult->pParts[0].as.tSource.sMimeType = demo_dupstr("text/plain");
    pResult->pParts[0].as.tSource.as.sText = demo_dupstr("sunny");
    if ( !pResult->pParts[0].as.tSource.sMimeType ||
         !pResult->pParts[0].as.tSource.as.sText ) {
        xllm_tool_exec_result_free(pResult);
        return XRT_NET_ERROR;
    }

    return XRT_NET_OK;
}

static bool demo_on_event(const xllm_event *pEvent, void *pUserData)
{
    demo_event_state *pState = (demo_event_state *)pUserData;

    if ( !pEvent ) {
        return true;
    }

    if ( pEvent->eType == XLLM_EVENT_TOOL_CALL_DELTA ) {
        if ( pState ) {
            ++pState->iToolDeltaCount;
        }
    } else if ( pEvent->eType == XLLM_EVENT_TOOL_CALL_READY ) {
        if ( pState ) {
            ++pState->iToolReadyCount;
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
    const char *sReason;

    if ( !pState || !pPayload || !*pPayload ) {
        return;
    }

    sPhase = (const char *)xvoTableGetText(*pPayload, (str)"phase", 0u);
    if ( eKind == XLLM_TRACE_REQUEST && sPhase && strcmp(sPhase, "request") == 0 ) {
        ++pState->iRequestTraceCount;
    } else if ( eKind == XLLM_TRACE_RESPONSE && sPhase && strcmp(sPhase, "response") == 0 ) {
        sResponseStatus = (const char *)xvoTableGetText(*pPayload, (str)"response_status", 0u);
        if ( sResponseStatus &&
             (strcmp(sResponseStatus, "tool_call_required") == 0 ||
              strcmp(sResponseStatus, "completed") == 0) ) {
            ++pState->iResponseTraceCount;
        }
    } else if ( eKind == XLLM_TRACE_STREAM && sPhase && strcmp(sPhase, "payload") == 0 ) {
        ++pState->iStreamPayloadTraceCount;
    } else if ( eKind == XLLM_TRACE_STREAM && sPhase && strcmp(sPhase, "finalize") == 0 ) {
        ++pState->iStreamFinalizeTraceCount;
    } else if ( eKind == XLLM_TRACE_TOOL_LOOP && sPhase && strcmp(sPhase, "round_response") == 0 ) {
        sResponseStatus = (const char *)xvoTableGetText(*pPayload, (str)"response_status", 0u);
        if ( sResponseStatus && strcmp(sResponseStatus, "tool_call_required") == 0 ) {
            ++pState->iToolLoopRoundResponseCount;
        }
    } else if ( eKind == XLLM_TRACE_TOOL_LOOP && sPhase && strcmp(sPhase, "tool_result") == 0 ) {
        if ( (int)xvoTableGetInt(*pPayload, (str)"exec_status", 0u) == XRT_NET_OK ) {
            ++pState->iToolLoopToolResultCount;
        }
    } else if ( eKind == XLLM_TRACE_TOOL_LOOP && sPhase && strcmp(sPhase, "stop") == 0 ) {
        sReason = (const char *)xvoTableGetText(*pPayload, (str)"reason", 0u);
        if ( sReason && strcmp(sReason, "response_final") == 0 ) {
            ++pState->iToolLoopStopFinalCount;
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

    if ( strcmp(sComponent, "xllm.ollama_native") == 0 ) {
        ++pState->iOllamaLogCount;
    } else if ( strcmp(sComponent, "xllm.tool_loop") == 0 ) {
        ++pState->iToolLoopLogCount;
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
    xllm_tool_def tTool;
    xllm_tool_executor tExecutor;
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
    memset(&tTool, 0, sizeof(tTool));
    memset(&tExecutor, 0, sizeof(tExecutor));

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
    if ( xllm_register_ollama_native_adapter(pRuntime) != XRT_NET_OK ) {
        fprintf(stderr, "register built-in adapter failed\n");
        return 10;
    }

    (void)snprintf(sBaseUrl, sizeof(sBaseUrl), "http://127.0.0.1:%u", (unsigned)tServer.uPort);
    xllm_profile_init(&tProfile);
    tProfile.sId = "ollama-stream-tool-local";
    tProfile.sProvider = "ollama";
    tProfile.sAdapter = XLLM_ADAPTER_OLLAMA_NATIVE;
    tProfile.sBaseUrl = sBaseUrl;
    tProfile.tAuth.eKind = XLLM_AUTH_NONE;
    tProfile.tModels.tText.sModelId = "llama-mock";
    tProfile.tModels.tText.tCaps.uFlags =
        XLLM_CAP_TEXT_IN |
        XLLM_CAP_TEXT_OUT |
        XLLM_CAP_STREAM |
        XLLM_CAP_TOOL_CALL_OUT |
        XLLM_CAP_TOOL_RESULT_IN;
    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "register profile failed\n");
        return 11;
    }

    tCreate.sInitialProfileId = "ollama-stream-tool-local";
    pLlm = xllm_create(pRuntime, &tCreate);
    if ( !pLlm ) {
        fprintf(stderr, "create llm failed\n");
        return 12;
    }

    tExecutor.pfnExecute = demo_tool_execute;
    if ( xllm_set_tool_executor(pLlm, &tExecutor) != XRT_NET_OK ) {
        fprintf(stderr, "set tool executor failed\n");
        return 13;
    }

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "hello ollama streamed tool") != XRT_NET_OK ) {
        fprintf(stderr, "turn add text failed\n");
        return 14;
    }
    tTurn.tToolPolicy.eMode = XLLM_TOOL_CHOICE_AUTO;
    tTool.sToolId = "app.weather.get_current";
    tTool.sWireName = "get_weather";
    tTool.sDescription = "Get current weather";
    if ( xllm_turn_add_tool(&tTurn, &tTool) != XRT_NET_OK ) {
        fprintf(stderr, "add tool failed\n");
        return 15;
    }

    xllm_call_options_init(&tCall);
    tCall.eStreamMode = XLLM_STREAM_REQUIRE;
    tCall.pfnOnEvent = demo_on_event;
    tCall.pUserData = &tEvents;

    iStatus = xllm_send(pLlm, &tTurn, &tCall, &pResponse);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "send failed: %d\n", iStatus);
        return 16;
    }

    xrtThreadWait(hServerThread);
    xrtThreadDestroy(hServerThread);
    hServerThread = NULL;

    sText = xllm_response_get_text(pResponse);
    if ( pResponse->eStatus != XLLM_STATUS_COMPLETED ||
         !sText ||
         strcmp(sText, "weather result: sunny") != 0 ) {
        fprintf(stderr, "unexpected response text: %s\n", sText ? sText : "(null)");
        return 17;
    }
    if ( tEvents.iToolDeltaCount < 1 || tEvents.iToolReadyCount != 1 || tEvents.iTextDeltaCount != 2 ) {
        fprintf(
            stderr,
            "unexpected event counts: tool_delta=%d tool_ready=%d text_delta=%d\n",
            tEvents.iToolDeltaCount,
            tEvents.iToolReadyCount,
            tEvents.iTextDeltaCount
        );
        return 18;
    }
    if ( tEvents.iEndCount != 2 ) {
        fprintf(stderr, "unexpected end count: %d\n", tEvents.iEndCount);
        return 19;
    }
    if ( tServer.iApiChatCount != 2 || tServer.iStreamTrueCount != 2 ) {
        fprintf(stderr, "unexpected stream request counts: api=%d stream=%d\n", tServer.iApiChatCount, tServer.iStreamTrueCount);
        return 20;
    }
    if ( tServer.iToolDefCount != 1 ) {
        fprintf(stderr, "unexpected tool def count: %d\n", tServer.iToolDefCount);
        return 21;
    }
    if ( tServer.iAssistantToolCallCount != 1 || tServer.iToolResultCount != 1 ) {
        fprintf(
            stderr,
            "unexpected tool loop body counts: assistant=%d tool_result=%d\n",
            tServer.iAssistantToolCallCount,
            tServer.iToolResultCount
        );
        return 22;
    }
    if ( pResponse->tUsage.uInputTokens != 18u || pResponse->tUsage.uOutputTokens != 4u ) {
        fprintf(stderr, "unexpected usage: in=%u out=%u\n", pResponse->tUsage.uInputTokens, pResponse->tUsage.uOutputTokens);
        return 23;
    }
    if ( tTrace.iRequestTraceCount < 2 || tTrace.iResponseTraceCount < 2 ) {
        fprintf(stderr, "unexpected request/response trace counts: request=%d response=%d\n", tTrace.iRequestTraceCount, tTrace.iResponseTraceCount);
        return 24;
    }
    if ( tTrace.iStreamPayloadTraceCount < 3 || tTrace.iStreamFinalizeTraceCount < 2 ) {
        fprintf(
            stderr,
            "unexpected stream trace counts: payload=%d finalize=%d\n",
            tTrace.iStreamPayloadTraceCount,
            tTrace.iStreamFinalizeTraceCount
        );
        return 25;
    }
    if ( tTrace.iToolLoopRoundResponseCount < 1 ||
         tTrace.iToolLoopToolResultCount < 1 ||
         tTrace.iToolLoopStopFinalCount < 1 ) {
        fprintf(
            stderr,
            "unexpected tool loop trace counts: round=%d result=%d stop=%d\n",
            tTrace.iToolLoopRoundResponseCount,
            tTrace.iToolLoopToolResultCount,
            tTrace.iToolLoopStopFinalCount
        );
        return 26;
    }
    if ( tLog.iOllamaLogCount < 4 || tLog.iToolLoopLogCount < 3 ) {
        fprintf(stderr, "unexpected log counts: ollama=%d tool_loop=%d\n", tLog.iOllamaLogCount, tLog.iToolLoopLogCount);
        return 27;
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
