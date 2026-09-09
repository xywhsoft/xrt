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

static bool demo_send_json_response(SOCKET hSocket, const char *sBody)
{
    char sResponse[16384];
    int iRespLen;

    iRespLen = snprintf(
        sResponse,
        sizeof(sResponse),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "request-id: req_openai_session_stream_artifacts\r\n"
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
            const char *sHeader =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/event-stream\r\n"
                "request-id: req_openai_session_stream_artifacts_1\r\n"
                "Cache-Control: no-cache\r\n"
                "Connection: close\r\n"
                "Transfer-Encoding: chunked\r\n"
                "\r\n";
            const char *sEvent1 =
                "data: {\"id\":\"chatcmpl-session-stream-artifacts\",\"object\":\"chat.completion.chunk\",\"model\":\"gpt-session-mm\","
                "\"choices\":[{\"index\":0,\"delta\":{\"role\":\"assistant\",\"content\":[{\"type\":\"output_text\",\"text\":\"first streamed artifact reply\"}]},\"finish_reason\":null}]}\n\n";
            const char *sEvent2 =
                "data: {\"id\":\"chatcmpl-session-stream-artifacts\",\"object\":\"chat.completion.chunk\",\"model\":\"gpt-session-mm\","
                "\"choices\":[{\"index\":0,\"delta\":{\"content\":[{\"type\":\"image_url\",\"image_url\":{\"url\":\"https://example.invalid/session-stream.png\",\"mime_type\":\"image/png\",\"name\":\"session-stream.png\"}}]},\"finish_reason\":null}]}\n\n";
            const char *sEvent3 =
                "data: {\"id\":\"chatcmpl-session-stream-artifacts\",\"object\":\"chat.completion.chunk\",\"model\":\"gpt-session-mm\","
                "\"choices\":[{\"index\":0,\"delta\":{\"content\":[{\"type\":\"file\",\"file\":{\"file_id\":\"file-session-stream-1\",\"mime_type\":\"application/pdf\",\"filename\":\"session-stream.pdf\"}}]},\"finish_reason\":null}]}\n\n";
            const char *sEvent4 =
                "data: {\"id\":\"chatcmpl-session-stream-artifacts\",\"object\":\"chat.completion.chunk\",\"model\":\"gpt-session-mm\","
                "\"choices\":[{\"index\":0,\"delta\":{},\"finish_reason\":\"stop\"}]}\n\n";
            const char *sEvent5 =
                "data: {\"id\":\"chatcmpl-session-stream-artifacts\",\"object\":\"chat.completion.chunk\",\"model\":\"gpt-session-mm\","
                "\"choices\":[],\"usage\":{\"prompt_tokens\":4,\"completion_tokens\":6}}\n\n";
            const char *sDone = "data: [DONE]\n\n";

            if ( strstr(aBuffer, "\"stream\":true") != NULL &&
                 strstr(aBuffer, "\"first turn\"") != NULL ) {
                pState->iFirstRequestOk = 1;
            }
            if ( !demo_send_all(hClient, sHeader, strlen(sHeader)) ) {
                closesocket(hClient);
                closesocket(pState->hListen);
                pState->hListen = INVALID_SOCKET;
                return 4u;
            }
            if ( !demo_send_chunk(hClient, sEvent1) ) return 5u;
            xrtSleep(10);
            if ( !demo_send_chunk(hClient, sEvent2) ) return 6u;
            xrtSleep(10);
            if ( !demo_send_chunk(hClient, sEvent3) ) return 7u;
            xrtSleep(10);
            if ( !demo_send_chunk(hClient, sEvent4) ) return 8u;
            xrtSleep(10);
            if ( !demo_send_chunk(hClient, sEvent5) ) return 9u;
            xrtSleep(10);
            if ( !demo_send_chunk(hClient, sDone) ) return 10u;
            if ( !demo_send_all(hClient, "0\r\n\r\n", 5u) ) return 11u;
        } else {
            const char *sBody =
                "{\"id\":\"chatcmpl-session-stream-artifacts-2\","
                "\"object\":\"chat.completion\","
                "\"model\":\"gpt-session-mm\","
                "\"choices\":[{\"index\":0,\"message\":{\"role\":\"assistant\",\"content\":\"stream follow-up ok\"},\"finish_reason\":\"stop\"}],"
                "\"usage\":{\"prompt_tokens\":6,\"completion_tokens\":2,\"total_tokens\":8}}";

            if ( strstr(aBuffer, "\"role\":\"assistant\",\"content\":[") != NULL &&
                 strstr(aBuffer, "\"type\":\"text\",\"text\":\"first streamed artifact reply\"") != NULL &&
                 strstr(aBuffer, "\"type\":\"image_url\",\"image_url\":{\"url\":\"https://example.invalid/session-stream.png\"}}") != NULL &&
                 strstr(aBuffer, "\"type\":\"file\",\"file\":{\"file_id\":\"file-session-stream-1\"}}") != NULL &&
                 strstr(aBuffer, "\"role\":\"user\",\"content\":\"second turn\"") != NULL ) {
                pState->iSecondRequestOk = 1;
            }
            if ( !demo_send_json_response(hClient, sBody) ) {
                closesocket(hClient);
                closesocket(pState->hListen);
                pState->hListen = INVALID_SOCKET;
                return 12u;
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
    xllm_call_options tCall;
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
    memset(&tCall, 0, sizeof(tCall));

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
    if ( xllm_register_openai_compat_adapter(pRuntime) != XRT_NET_OK ) {
        fprintf(stderr, "register openai-compatible adapter failed\n");
        return 8;
    }

    snprintf(sBaseUrl, sizeof(sBaseUrl), "http://127.0.0.1:%u/v1", (unsigned)tServer.uPort);
    xllm_profile_init(&tProfile);
    tProfile.sId = "openai-session-stream-mm";
    tProfile.sProvider = "openai";
    tProfile.sAdapter = XLLM_ADAPTER_OPENAI_COMPAT;
    tProfile.sBaseUrl = sBaseUrl;
    tProfile.tAuth.eKind = XLLM_AUTH_NONE;
    tProfile.tModels.tText.sModelId = "gpt-session-text";
    tProfile.tModels.tText.tCaps.uFlags = XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT | XLLM_CAP_STREAM;
    tProfile.tModels.tMultimodal.sModelId = "gpt-session-mm";
    tProfile.tModels.tMultimodal.tCaps.uFlags =
        XLLM_CAP_TEXT_IN | XLLM_CAP_IMAGE_IN | XLLM_CAP_FILE_IN | XLLM_CAP_TEXT_OUT | XLLM_CAP_STREAM;
    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "register profile failed\n");
        return 9;
    }

    xllm_session_options_init(&tSessionOptions);
    tSessionOptions.sProfileId = "openai-session-stream-mm";
    if ( xllm_session_create(pRuntime, &tSessionOptions, &pSession) != XRT_NET_OK || !pSession ) {
        fprintf(stderr, "create session failed\n");
        return 10;
    }

    tCall.eStreamMode = XLLM_STREAM_REQUIRE;

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "first turn") != XRT_NET_OK ) {
        fprintf(stderr, "first turn add text failed\n");
        return 11;
    }
    iStatus = xllm_session_chat(pSession, &tTurn, &tCall, &pResponse);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "first session chat failed: %d\n", iStatus);
        return 12;
    }
    if ( !xllm_response_get_text(pResponse) ||
         strcmp(xllm_response_get_text(pResponse), "first streamed artifact reply") != 0 ) {
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
         strcmp(xllm_response_get_text(pResponse), "stream follow-up ok") != 0 ) {
        fprintf(stderr, "unexpected second response text: %s\n", xllm_response_get_text(pResponse));
        return 16;
    }
    xllm_response_free(pResponse);
    pResponse = NULL;
    xllm_turn_reset(&tTurn);

    if ( hServerThread ) {
        xrtThreadWait(hServerThread);
        xrtThreadDestroy(hServerThread);
        hServerThread = NULL;
    }

    if ( tServer.iAcceptCount != 2 || !tServer.iFirstRequestOk || !tServer.iSecondRequestOk ) {
        fprintf(
            stderr,
            "unexpected session request counters: accept=%d first_ok=%d second_ok=%d\n",
            tServer.iAcceptCount,
            tServer.iFirstRequestOk,
            tServer.iSecondRequestOk
        );
        return 17;
    }

    printf("smoke_openai_compat_session_stream_output_artifacts ok\n");

    xllm_session_destroy(pSession);
    xllm_runtime_destroy(pRuntime);

#if defined(_WIN32) || defined(_WIN64)
    WSACleanup();
#endif
    return 0;
}
