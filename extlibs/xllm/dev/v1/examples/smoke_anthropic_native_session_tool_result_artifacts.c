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
    int iAcceptCount;
    int iFirstRequestOk;
    int iToolRoundRequestOk;
    int iFollowupRequestOk;
    char aLastFollowupRequest[32768];
} demo_server_state;

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

static bool demo_send_json_response(SOCKET hSocket, const char *sRequestId, const char *sBody)
{
    char sResponse[32768];
    int iRespLen;

    iRespLen = snprintf(
        sResponse,
        sizeof(sResponse),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "request-id: %s\r\n"
        "Connection: close\r\n"
        "Content-Length: %u\r\n"
        "\r\n"
        "%s",
        sRequestId ? sRequestId : "req_anthropic_session_tool_artifacts",
        (unsigned)strlen(sBody),
        sBody
    );
    if ( iRespLen <= 0 ) {
        return false;
    }
    return demo_send_all(hSocket, sResponse, (size_t)iRespLen);
}

static uint32 demo_server_thread(ptr pParam)
{
    demo_server_state *pState = (demo_server_state *)pParam;
    int iHandled = 0;

    if ( !pState || pState->hListen == INVALID_SOCKET ) {
        return 1u;
    }

    while ( iHandled < 3 ) {
        SOCKET hClient = accept(pState->hListen, NULL, NULL);
        char aBuffer[32768];
        int iRecv;

        if ( hClient == INVALID_SOCKET ) {
            closesocket(pState->hListen);
            pState->hListen = INVALID_SOCKET;
            return 2u;
        }

        iRecv = recv(hClient, aBuffer, (int)(sizeof(aBuffer) - 1u), 0);
        if ( iRecv <= 0 ) {
            closesocket(hClient);
            closesocket(pState->hListen);
            pState->hListen = INVALID_SOCKET;
            return 3u;
        }
        aBuffer[iRecv] = '\0';
        ++pState->iAcceptCount;

        if ( iHandled == 0 ) {
            const char *sBody =
                "{\"id\":\"msg_session_tool_artifacts_1\","
                "\"type\":\"message\","
                "\"role\":\"assistant\","
                "\"model\":\"claude-session-tool-mm\","
                "\"content\":[{\"type\":\"tool_use\",\"id\":\"toolu_session_tool_artifacts_1\",\"name\":\"get_weather\",\"input\":{\"city\":\"Shanghai\"}}],"
                "\"stop_reason\":\"tool_use\","
                "\"usage\":{\"input_tokens\":8,\"output_tokens\":2}}";

            if ( strstr(aBuffer, "\"hello anthropic session tool artifacts\"") != NULL &&
                 strstr(aBuffer, "\"tools\":[{\"name\":\"get_weather\"") != NULL ) {
                pState->iFirstRequestOk = 1;
            }
            if ( !demo_send_json_response(hClient, "req_anthropic_session_tool_artifacts_1", sBody) ) {
                closesocket(hClient);
                closesocket(pState->hListen);
                pState->hListen = INVALID_SOCKET;
                return 4u;
            }
        } else if ( iHandled == 1 ) {
            const char *sBody =
                "{\"id\":\"msg_session_tool_artifacts_2\","
                "\"type\":\"message\","
                "\"role\":\"assistant\","
                "\"model\":\"claude-session-tool-mm\","
                "\"content\":[{\"type\":\"text\",\"text\":\"anthropic session tool artifacts first ok\"}],"
                "\"stop_reason\":\"end_turn\","
                "\"usage\":{\"input_tokens\":14,\"output_tokens\":4}}";

            if ( strstr(aBuffer, "\"type\":\"tool_use\"") != NULL &&
                 strstr(aBuffer, "\"id\":\"toolu_session_tool_artifacts_1\"") != NULL &&
                 strstr(aBuffer, "\"name\":\"get_weather\"") != NULL &&
                 strstr(aBuffer, "\"type\":\"tool_result\"") != NULL &&
                 strstr(aBuffer, "\"tool_use_id\":\"toolu_session_tool_artifacts_1\"") != NULL &&
                 strstr(aBuffer, "\"type\":\"text\",\"text\":\"sunny map attached\"") != NULL &&
                 strstr(aBuffer, "\"type\":\"image\",\"source\":{\"type\":\"url\",\"url\":\"https://example.invalid/anthropic-session-tool-map.png\"}}") != NULL &&
                 strstr(aBuffer, "\"type\":\"document\",\"source\":{\"type\":\"file\",\"file_id\":\"file_anthropic_session_tool_doc_1\"}}") != NULL ) {
                pState->iToolRoundRequestOk = 1;
            }
            if ( !demo_send_json_response(hClient, "req_anthropic_session_tool_artifacts_2", sBody) ) {
                closesocket(hClient);
                closesocket(pState->hListen);
                pState->hListen = INVALID_SOCKET;
                return 5u;
            }
        } else {
            const char *sBody =
                "{\"id\":\"msg_session_tool_artifacts_3\","
                "\"type\":\"message\","
                "\"role\":\"assistant\","
                "\"model\":\"claude-session-tool-mm\","
                "\"content\":[{\"type\":\"text\",\"text\":\"anthropic session tool artifacts follow-up ok\"}],"
                "\"stop_reason\":\"end_turn\","
                "\"usage\":{\"input_tokens\":19,\"output_tokens\":5}}";

            memcpy(pState->aLastFollowupRequest, aBuffer, (size_t)iRecv + 1u);

            if ( strstr(aBuffer, "\"type\":\"tool_use\"") != NULL &&
                 strstr(aBuffer, "\"id\":\"toolu_session_tool_artifacts_1\"") != NULL &&
                 strstr(aBuffer, "\"type\":\"tool_result\"") != NULL &&
                 strstr(aBuffer, "\"tool_use_id\":\"toolu_session_tool_artifacts_1\"") != NULL &&
                 strstr(aBuffer, "\"type\":\"image\",\"source\":{\"type\":\"url\",\"url\":\"https://example.invalid/anthropic-session-tool-map.png\"}}") != NULL &&
                 strstr(aBuffer, "\"type\":\"document\",\"source\":{\"type\":\"file\",\"file_id\":\"file_anthropic_session_tool_doc_1\"}}") != NULL &&
                 strstr(aBuffer, "\"role\":\"assistant\",\"content\":\"anthropic session tool artifacts first ok\"") != NULL &&
                 strstr(aBuffer, "\"role\":\"user\",\"content\":\"follow-up turn\"") != NULL ) {
                pState->iFollowupRequestOk = 1;
            }
            if ( !demo_send_json_response(hClient, "req_anthropic_session_tool_artifacts_3", sBody) ) {
                closesocket(hClient);
                closesocket(pState->hListen);
                pState->hListen = INVALID_SOCKET;
                return 6u;
            }
        }

        closesocket(hClient);
        ++iHandled;
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
    pResult->pParts[1].as.tSource.as.sUrl = demo_dupstr("https://example.invalid/anthropic-session-tool-map.png");

    pResult->pParts[2].eKind = XLLM_PART_FILE;
    pResult->pParts[2].as.tSource.eKind = XLLM_SOURCE_PROVIDER_FILE_ID;
    pResult->pParts[2].as.tSource.sMimeType = demo_dupstr("application/pdf");
    pResult->pParts[2].as.tSource.sName = demo_dupstr("anthropic-session-tool-report.pdf");
    pResult->pParts[2].as.tSource.as.sFileId = demo_dupstr("file_anthropic_session_tool_doc_1");

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
    xllm_session *pSession = NULL;
    xllm_response *pResponse = NULL;
    xllm_profile tProfile;
    xllm_session_options tSessionOptions;
    xllm_turn tTurn;
    xllm_tool_def tTool;
    xllm_tool_executor tExecutor;
    char sBaseUrl[128];
    int iStatus = 0;

#if defined(_WIN32) || defined(_WIN64)
    if ( WSAStartup(MAKEWORD(2, 2), &tWsaData) != 0 ) {
        fprintf(stderr, "winsock startup failed\n");
        return 1;
    }
#endif

    memset(&tServer, 0, sizeof(tServer));
    memset(&tAddr, 0, sizeof(tAddr));
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
    if ( listen(tServer.hListen, 3) == SOCKET_ERROR ) {
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
        fprintf(stderr, "register anthropic-native adapter failed\n");
        return 8;
    }

    snprintf(sBaseUrl, sizeof(sBaseUrl), "http://127.0.0.1:%u/v1", (unsigned)tServer.uPort);
    xllm_profile_init(&tProfile);
    tProfile.sId = "anthropic-session-tool-mm";
    tProfile.sProvider = "anthropic";
    tProfile.sAdapter = XLLM_ADAPTER_ANTHROPIC_NATIVE;
    tProfile.sBaseUrl = sBaseUrl;
    tProfile.tAuth.eKind = XLLM_AUTH_API_KEY_HEADER;
    tProfile.tAuth.sSecret = "test-key";
    tProfile.tAuth.sHeaderName = "x-api-key";
    tProfile.tModels.tText.sModelId = "claude-session-tool-text";
    tProfile.tModels.tText.tCaps.uFlags =
        XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT | XLLM_CAP_TOOL_CALL_OUT | XLLM_CAP_TOOL_RESULT_IN;
    tProfile.tModels.tMultimodal.sModelId = "claude-session-tool-mm";
    tProfile.tModels.tMultimodal.tCaps.uFlags =
        XLLM_CAP_TEXT_IN | XLLM_CAP_IMAGE_IN | XLLM_CAP_FILE_IN | XLLM_CAP_TEXT_OUT |
        XLLM_CAP_TOOL_CALL_OUT | XLLM_CAP_TOOL_RESULT_IN;
    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "register profile failed\n");
        return 9;
    }

    xllm_session_options_init(&tSessionOptions);
    tSessionOptions.sProfileId = "anthropic-session-tool-mm";
    if ( xllm_session_create(pRuntime, &tSessionOptions, &pSession) != XRT_NET_OK || !pSession ) {
        fprintf(stderr, "create session failed\n");
        return 10;
    }

    tExecutor.pfnExecute = demo_tool_execute;
    if ( xllm_session_set_tool_executor(pSession, &tExecutor) != XRT_NET_OK ) {
        fprintf(stderr, "set session tool executor failed\n");
        return 11;
    }

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "hello anthropic session tool artifacts") != XRT_NET_OK ) {
        fprintf(stderr, "first turn add text failed\n");
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

    iStatus = xllm_session_chat(pSession, &tTurn, NULL, &pResponse);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "first session chat failed: %d\n", iStatus);
        return 14;
    }
    if ( !xllm_response_get_text(pResponse) ||
         strcmp(xllm_response_get_text(pResponse), "anthropic session tool artifacts first ok") != 0 ) {
        fprintf(stderr, "unexpected first response text: %s\n", xllm_response_get_text(pResponse));
        return 15;
    }
    xllm_response_free(pResponse);
    pResponse = NULL;
    xllm_turn_reset(&tTurn);

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "follow-up turn") != XRT_NET_OK ) {
        fprintf(stderr, "follow-up turn add text failed\n");
        return 16;
    }
    iStatus = xllm_session_chat(pSession, &tTurn, NULL, &pResponse);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "follow-up session chat failed: %d\n", iStatus);
        return 17;
    }
    if ( !xllm_response_get_text(pResponse) ||
         strcmp(xllm_response_get_text(pResponse), "anthropic session tool artifacts follow-up ok") != 0 ) {
        fprintf(stderr, "unexpected follow-up response text: %s\n", xllm_response_get_text(pResponse));
        return 18;
    }

    xllm_response_free(pResponse);
    pResponse = NULL;
    xllm_turn_reset(&tTurn);
    xllm_session_destroy(pSession);
    pSession = NULL;
    xllm_runtime_destroy(pRuntime);
    pRuntime = NULL;

    if ( hServerThread ) {
        xrtThreadWait(hServerThread);
        xrtThreadDestroy(hServerThread);
        hServerThread = NULL;
    }

    if ( tServer.iAcceptCount != 3 ||
         !tServer.iFirstRequestOk ||
         !tServer.iToolRoundRequestOk ||
         !tServer.iFollowupRequestOk ) {
        fprintf(
            stderr,
            "unexpected server state accept=%d first=%d tool=%d follow=%d\n",
            tServer.iAcceptCount,
            tServer.iFirstRequestOk,
            tServer.iToolRoundRequestOk,
            tServer.iFollowupRequestOk
        );
        if ( !tServer.iFollowupRequestOk ) {
            fprintf(stderr, "followup request body:\n%s\n", tServer.aLastFollowupRequest);
        }
        return 19;
    }

#if defined(_WIN32) || defined(_WIN64)
    WSACleanup();
#endif
    return 0;
}
