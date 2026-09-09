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
    int iStreamRequestCount;
    int iToolResultCount;
    int iToolResultTextCount;
    int iToolResultImageCount;
    int iToolResultFileCount;
} demo_server_state;

typedef struct {
    int iToolReadyCount;
    int iTextDeltaCount;
    int iEndCount;
} demo_event_state;

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
    int iRound;

    if ( !pState || pState->hListen == INVALID_SOCKET ) {
        return 1u;
    }

    for ( iRound = 0; iRound < 2; ++iRound ) {
        SOCKET hClient = INVALID_SOCKET;
        char aBuffer[16384];
        int iRecv;
        const char *sHeader =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/event-stream\r\n"
            "Cache-Control: no-cache\r\n"
            "Connection: close\r\n"
            "Transfer-Encoding: chunked\r\n";
        char sResponseHeader[512];
        int iHeaderLen;

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
            if ( strstr(aBuffer, "\"stream\":true") != NULL ) {
                ++pState->iStreamRequestCount;
            }
            if ( iRound == 1 ) {
                if ( strstr(aBuffer, "\"type\":\"tool_result\"") != NULL &&
                     strstr(aBuffer, "\"tool_use_id\":\"toolu_weather_stream_artifacts_1\"") != NULL &&
                     strstr(aBuffer, "\"content\":[") != NULL ) {
                    ++pState->iToolResultCount;
                }
                if ( strstr(aBuffer, "\"type\":\"text\",\"text\":\"sunny map attached\"") != NULL ) {
                    ++pState->iToolResultTextCount;
                }
                if ( strstr(aBuffer, "\"type\":\"image\",\"source\":{\"type\":\"url\",\"url\":\"https://example.invalid/weather-stream-map.png\"}}") != NULL ) {
                    ++pState->iToolResultImageCount;
                }
                if ( strstr(aBuffer, "\"type\":\"document\",\"source\":{\"type\":\"file\",\"file_id\":\"file_doc_stream_123\"}}") != NULL ) {
                    ++pState->iToolResultFileCount;
                }
            }
        }

        iHeaderLen = snprintf(
            sResponseHeader,
            sizeof(sResponseHeader),
            "%srequest-id: req_anthropic_stream_tool_artifacts_%d\r\n\r\n",
            sHeader,
            iRound + 1
        );
        if ( iHeaderLen <= 0 || !demo_send_all(hClient, sResponseHeader, (size_t)iHeaderLen) ) {
            closesocket(hClient);
            closesocket(pState->hListen);
            pState->hListen = INVALID_SOCKET;
            return 3u;
        }

        if ( iRound == 0 ) {
            const char *sEvent1 =
                "data: {\"type\":\"message_start\",\"message\":{\"id\":\"msg_tool_stream_artifacts\",\"type\":\"message\",\"role\":\"assistant\",\"model\":\"claude-mock\",\"usage\":{\"input_tokens\":10,\"output_tokens\":0}}}\n\n";
            const char *sEvent2 =
                "data: {\"type\":\"content_block_start\",\"index\":0,\"content_block\":{\"type\":\"tool_use\",\"id\":\"toolu_weather_stream_artifacts_1\",\"name\":\"get_weather\"}}\n\n";
            const char *sEvent3 =
                "data: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"input_json_delta\",\"partial_json\":\"{\\\"city\\\":\\\"Shang\"}}\n\n";
            const char *sEvent4 =
                "data: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"input_json_delta\",\"partial_json\":\"hai\\\"}\"}}\n\n";
            const char *sEvent5 =
                "data: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"tool_use\"},\"usage\":{\"output_tokens\":3}}\n\n";
            const char *sEvent6 = "data: {\"type\":\"message_stop\"}\n\n";

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
        } else {
            const char *sEvent1 =
                "data: {\"type\":\"message_start\",\"message\":{\"id\":\"msg_final_stream_artifacts\",\"type\":\"message\",\"role\":\"assistant\",\"model\":\"claude-mock\",\"usage\":{\"input_tokens\":20,\"output_tokens\":0}}}\n\n";
            const char *sEvent2 =
                "data: {\"type\":\"content_block_start\",\"index\":0,\"content_block\":{\"type\":\"text\",\"text\":\"weather result: \"}}\n\n";
            const char *sEvent3 =
                "data: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"artifact tool ok\"}}\n\n";
            const char *sEvent4 =
                "data: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"end_turn\"},\"usage\":{\"output_tokens\":4}}\n\n";
            const char *sEvent5 = "data: {\"type\":\"message_stop\"}\n\n";

            if ( !demo_send_chunk(hClient, sEvent1) ) return 10u;
            xrtSleep(20);
            if ( !demo_send_chunk(hClient, sEvent2) ) return 11u;
            xrtSleep(20);
            if ( !demo_send_chunk(hClient, sEvent3) ) return 12u;
            xrtSleep(20);
            if ( !demo_send_chunk(hClient, sEvent4) ) return 13u;
            xrtSleep(20);
            if ( !demo_send_chunk(hClient, sEvent5) ) return 14u;
        }

        if ( !demo_send_all(hClient, "0\r\n\r\n", 5u) ) {
            closesocket(hClient);
            closesocket(pState->hListen);
            pState->hListen = INVALID_SOCKET;
            return 15u;
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

    pResult->pParts = (xllm_content_part *)xrtCalloc(3u, sizeof(xllm_content_part));
    if ( !pResult->pParts ) {
        return XRT_NET_ERROR;
    }

    pResult->iPartCount = 3u;

    pResult->pParts[0].eKind = XLLM_PART_TEXT;
    pResult->pParts[0].as.tSource.eKind = XLLM_SOURCE_INLINE_TEXT;
    pResult->pParts[0].as.tSource.sMimeType = demo_dupstr("text/plain");
    pResult->pParts[0].as.tSource.as.sText = demo_dupstr("sunny map attached");

    pResult->pParts[1].eKind = XLLM_PART_IMAGE;
    pResult->pParts[1].as.tSource.eKind = XLLM_SOURCE_URL;
    pResult->pParts[1].as.tSource.sMimeType = demo_dupstr("image/png");
    pResult->pParts[1].as.tSource.as.sUrl = demo_dupstr("https://example.invalid/weather-stream-map.png");

    pResult->pParts[2].eKind = XLLM_PART_FILE;
    pResult->pParts[2].as.tSource.eKind = XLLM_SOURCE_PROVIDER_FILE_ID;
    pResult->pParts[2].as.tSource.sMimeType = demo_dupstr("application/pdf");
    pResult->pParts[2].as.tSource.sName = demo_dupstr("weather-stream-report.pdf");
    pResult->pParts[2].as.tSource.as.sFileId = demo_dupstr("file_doc_stream_123");

    if ( !pResult->pParts[0].as.tSource.sMimeType ||
         !pResult->pParts[0].as.tSource.as.sText ||
         !pResult->pParts[1].as.tSource.sMimeType ||
         !pResult->pParts[1].as.tSource.as.sUrl ||
         !pResult->pParts[2].as.tSource.sMimeType ||
         !pResult->pParts[2].as.tSource.sName ||
         !pResult->pParts[2].as.tSource.as.sFileId ) {
        xllm_tool_exec_result_free(pResult);
        return XRT_NET_ERROR;
    }

    return XRT_NET_OK;
}

static bool demo_on_event(const xllm_event *pEvent, void *pUserData)
{
    demo_event_state *pState = (demo_event_state *)pUserData;

    if ( !pState || !pEvent ) {
        return true;
    }

    switch ( pEvent->eType ) {
        case XLLM_EVENT_TOOL_CALL_READY:
            ++pState->iToolReadyCount;
            break;
        case XLLM_EVENT_TEXT_DELTA:
            ++pState->iTextDeltaCount;
            break;
        case XLLM_EVENT_END:
            ++pState->iEndCount;
            break;
        default:
            break;
    }

    return true;
}

int main(void)
{
#if defined(_WIN32) || defined(_WIN64)
    WSADATA tWsaData;
#endif
    demo_server_state tServer;
    demo_event_state tEvents;
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
    if ( xllm_register_anthropic_native_adapter(pRuntime) != XRT_NET_OK ) {
        fprintf(stderr, "register adapter failed\n");
        return 8;
    }

    (void)snprintf(sBaseUrl, sizeof(sBaseUrl), "http://127.0.0.1:%u/v1", (unsigned)tServer.uPort);
    xllm_profile_init(&tProfile);
    tProfile.sId = "anthropic-stream-tool-artifacts";
    tProfile.sProvider = "anthropic";
    tProfile.sAdapter = XLLM_ADAPTER_ANTHROPIC_NATIVE;
    tProfile.sBaseUrl = sBaseUrl;
    tProfile.tAuth.eKind = XLLM_AUTH_API_KEY_HEADER;
    tProfile.tAuth.sSecret = "test-key";
    tProfile.tAuth.sHeaderName = "x-api-key";
    tProfile.tModels.tText.sModelId = "claude-mock";
    tProfile.tModels.tMultimodal.sModelId = "claude-mock";
    tProfile.tModels.tText.tCaps.uFlags =
        XLLM_CAP_TEXT_IN |
        XLLM_CAP_TEXT_OUT |
        XLLM_CAP_STREAM |
        XLLM_CAP_TOOL_CALL_OUT |
        XLLM_CAP_TOOL_RESULT_IN;
    tProfile.tModels.tMultimodal.tCaps.uFlags =
        XLLM_CAP_TEXT_IN |
        XLLM_CAP_IMAGE_IN |
        XLLM_CAP_FILE_IN |
        XLLM_CAP_TEXT_OUT |
        XLLM_CAP_STREAM |
        XLLM_CAP_TOOL_CALL_OUT |
        XLLM_CAP_TOOL_RESULT_IN;
    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "register profile failed\n");
        return 9;
    }

    tCreate.sInitialProfileId = "anthropic-stream-tool-artifacts";
    pLlm = xllm_create(pRuntime, &tCreate);
    if ( !pLlm ) {
        fprintf(stderr, "create llm failed\n");
        return 10;
    }

    tExecutor.pfnExecute = demo_tool_execute;
    if ( xllm_set_tool_executor(pLlm, &tExecutor) != XRT_NET_OK ) {
        fprintf(stderr, "set tool executor failed\n");
        return 11;
    }

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "hello anthropic streamed tool artifacts") != XRT_NET_OK ) {
        fprintf(stderr, "turn add text failed\n");
        return 12;
    }
    tTurn.tToolPolicy.eMode = XLLM_TOOL_CHOICE_AUTO;
    tTool.sToolId = "app.weather.get_current";
    tTool.sWireName = "get_weather";
    tTool.sDescription = "Get current weather";
    if ( xllm_turn_add_tool(&tTurn, &tTool) != XRT_NET_OK ) {
        fprintf(stderr, "add tool failed\n");
        return 13;
    }

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
    if ( pResponse->eStatus != XLLM_STATUS_COMPLETED ||
         !sText ||
         strcmp(sText, "weather result: artifact tool ok") != 0 ) {
        fprintf(stderr, "unexpected response text: %s\n", sText ? sText : "(null)");
        return 15;
    }
    if ( tEvents.iToolReadyCount != 1 || tEvents.iTextDeltaCount != 2 || tEvents.iEndCount != 2 ) {
        fprintf(
            stderr,
            "unexpected event counts: ready=%d text=%d end=%d\n",
            tEvents.iToolReadyCount,
            tEvents.iTextDeltaCount,
            tEvents.iEndCount
        );
        return 16;
    }
    if ( tServer.iApiKeyCount != 2 || tServer.iVersionCount != 2 || tServer.iStreamRequestCount != 2 ) {
        fprintf(
            stderr,
            "unexpected request counters: api=%d version=%d stream=%d\n",
            tServer.iApiKeyCount,
            tServer.iVersionCount,
            tServer.iStreamRequestCount
        );
        return 17;
    }
    if ( tServer.iToolResultCount != 1 ||
         tServer.iToolResultTextCount != 1 ||
         tServer.iToolResultImageCount != 1 ||
         tServer.iToolResultFileCount != 1 ) {
        fprintf(
            stderr,
            "unexpected tool result counters: result=%d text=%d image=%d file=%d\n",
            tServer.iToolResultCount,
            tServer.iToolResultTextCount,
            tServer.iToolResultImageCount,
            tServer.iToolResultFileCount
        );
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
