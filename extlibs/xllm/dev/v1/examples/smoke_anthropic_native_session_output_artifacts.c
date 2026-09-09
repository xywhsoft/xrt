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
    int iSecondRequestOk;
} demo_server_state;

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

static bool demo_send_json_response(SOCKET hSocket, const char *sBody)
{
    char sResponse[16384];
    int iRespLen;

    iRespLen = snprintf(
        sResponse,
        sizeof(sResponse),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "request-id: req_anthropic_session_artifacts\r\n"
        "Connection: close\r\n"
        "Content-Length: %u\r\n"
        "\r\n"
        "%s",
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

    while ( iHandled < 2 ) {
        SOCKET hClient = accept(pState->hListen, NULL, NULL);
        char aBuffer[16384];
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
                "{\"id\":\"msg_session_artifacts_1\","
                "\"type\":\"message\","
                "\"role\":\"assistant\","
                "\"model\":\"claude-session-mm\","
                "\"content\":["
                "{\"type\":\"text\",\"text\":\"first anthropic artifact reply\"},"
                "{\"type\":\"image\",\"name\":\"session.png\",\"source\":{\"type\":\"url\",\"url\":\"https://example.invalid/anthropic-session.png\",\"media_type\":\"image/png\"}},"
                "{\"type\":\"document\",\"name\":\"session.pdf\",\"source\":{\"type\":\"file\",\"file_id\":\"file-anthropic-session-1\",\"media_type\":\"application/pdf\"}}"
                "],"
                "\"stop_reason\":\"end_turn\","
                "\"usage\":{\"input_tokens\":3,\"output_tokens\":6}}";

            if ( strstr(aBuffer, "first turn") != NULL ) {
                pState->iFirstRequestOk = 1;
            }
            if ( !demo_send_json_response(hClient, sBody) ) {
                closesocket(hClient);
                closesocket(pState->hListen);
                pState->hListen = INVALID_SOCKET;
                return 4u;
            }
        } else {
            const char *sBody =
                "{\"id\":\"msg_session_artifacts_2\","
                "\"type\":\"message\","
                "\"role\":\"assistant\","
                "\"model\":\"claude-session-mm\","
                "\"content\":[{\"type\":\"text\",\"text\":\"anthropic follow-up ok\"}],"
                "\"stop_reason\":\"end_turn\","
                "\"usage\":{\"input_tokens\":6,\"output_tokens\":3}}";

            if ( strstr(aBuffer, "\"role\":\"assistant\",\"content\":[") != NULL &&
                 strstr(aBuffer, "\"type\":\"text\",\"text\":\"first anthropic artifact reply\"") != NULL &&
                 strstr(aBuffer, "\"type\":\"image\",\"source\":{\"type\":\"url\",\"url\":\"https://example.invalid/anthropic-session.png\"}}") != NULL &&
                 strstr(aBuffer, "\"type\":\"document\",\"source\":{\"type\":\"file\",\"file_id\":\"file-anthropic-session-1\"") != NULL &&
                 strstr(aBuffer, "\"role\":\"user\",\"content\":\"second turn\"") != NULL ) {
                pState->iSecondRequestOk = 1;
            }
            if ( !demo_send_json_response(hClient, sBody) ) {
                closesocket(hClient);
                closesocket(pState->hListen);
                pState->hListen = INVALID_SOCKET;
                return 5u;
            }
        }

        closesocket(hClient);
        ++iHandled;
    }

    closesocket(pState->hListen);
    pState->hListen = INVALID_SOCKET;
    return 0u;
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
        fprintf(stderr, "register anthropic-native adapter failed\n");
        return 8;
    }

    snprintf(sBaseUrl, sizeof(sBaseUrl), "http://127.0.0.1:%u/v1", (unsigned)tServer.uPort);
    xllm_profile_init(&tProfile);
    tProfile.sId = "anthropic-session-mm";
    tProfile.sProvider = "anthropic";
    tProfile.sAdapter = XLLM_ADAPTER_ANTHROPIC_NATIVE;
    tProfile.sBaseUrl = sBaseUrl;
    tProfile.tAuth.eKind = XLLM_AUTH_API_KEY_HEADER;
    tProfile.tAuth.sSecret = "test-key";
    tProfile.tAuth.sHeaderName = "x-api-key";
    tProfile.tModels.tText.sModelId = "claude-session-text";
    tProfile.tModels.tText.tCaps.uFlags = XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT;
    tProfile.tModels.tMultimodal.sModelId = "claude-session-mm";
    tProfile.tModels.tMultimodal.tCaps.uFlags =
        XLLM_CAP_TEXT_IN | XLLM_CAP_IMAGE_IN | XLLM_CAP_FILE_IN | XLLM_CAP_TEXT_OUT;
    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "register profile failed\n");
        return 9;
    }

    xllm_session_options_init(&tSessionOptions);
    tSessionOptions.sProfileId = "anthropic-session-mm";
    if ( xllm_session_create(pRuntime, &tSessionOptions, &pSession) != XRT_NET_OK || !pSession ) {
        fprintf(stderr, "create session failed\n");
        return 10;
    }

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "first turn") != XRT_NET_OK ) {
        fprintf(stderr, "first turn add text failed\n");
        return 11;
    }
    iStatus = xllm_session_chat(pSession, &tTurn, NULL, &pResponse);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "first session chat failed: %d\n", iStatus);
        return 12;
    }
    if ( !xllm_response_get_text(pResponse) ||
         strcmp(xllm_response_get_text(pResponse), "first anthropic artifact reply") != 0 ) {
        fprintf(stderr, "unexpected first response text: %s\n", xllm_response_get_text(pResponse));
        return 13;
    }
    xllm_response_free(pResponse);
    pResponse = NULL;
    xllm_turn_reset(&tTurn);

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "second turn") != XRT_NET_OK ) {
        fprintf(stderr, "second turn add text failed\n");
        return 14;
    }
    iStatus = xllm_session_chat(pSession, &tTurn, NULL, &pResponse);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "second session chat failed: %d\n", iStatus);
        return 15;
    }
    if ( !xllm_response_get_text(pResponse) ||
         strcmp(xllm_response_get_text(pResponse), "anthropic follow-up ok") != 0 ) {
        fprintf(stderr, "unexpected second response text: %s\n", xllm_response_get_text(pResponse));
        return 16;
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

    if ( tServer.iAcceptCount != 2 ||
         !tServer.iFirstRequestOk ||
         !tServer.iSecondRequestOk ) {
        fprintf(
            stderr,
            "unexpected server state accept=%d first=%d second=%d\n",
            tServer.iAcceptCount,
            tServer.iFirstRequestOk,
            tServer.iSecondRequestOk
        );
        return 17;
    }

#if defined(_WIN32) || defined(_WIN64)
    WSACleanup();
#endif
    return 0;
}
