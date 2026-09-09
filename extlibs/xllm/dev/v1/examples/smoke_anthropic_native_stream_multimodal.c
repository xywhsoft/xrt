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
    int iMultimodalModelCount;
    int iImageBlockCount;
    int iBase64Count;
    int iTextValueCount;
} demo_stream_server_state;

typedef struct {
    int iTextDeltaCount;
    int iEndCount;
} demo_stream_event_state;

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

static uint32 demo_stream_server_thread(ptr pParam)
{
    demo_stream_server_state *pState = (demo_stream_server_state *)pParam;
    SOCKET hClient = INVALID_SOCKET;
    char aBuffer[8192];
    const char *sHeader =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream\r\n"
        "request-id: req_anthropic_mm_stream_123\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: close\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n";
    const char *sEvent1 =
        "data: {\"type\":\"message_start\",\"message\":{\"id\":\"msg_stream_mm\",\"type\":\"message\",\"role\":\"assistant\",\"model\":\"claude-mock-mm\",\"usage\":{\"input_tokens\":14,\"output_tokens\":0}}}\n\n";
    const char *sEvent2 =
        "data: {\"type\":\"content_block_start\",\"index\":0,\"content_block\":{\"type\":\"text\",\"text\":\"anthropic \"}}\n\n";
    const char *sEvent3 =
        "data: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"stream \"}}\n\n";
    const char *sEvent4 =
        "data: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"image\"}}\n\n";
    const char *sEvent5 =
        "data: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"end_turn\"},\"usage\":{\"output_tokens\":3}}\n\n";
    const char *sEvent6 = "data: {\"type\":\"message_stop\"}\n\n";
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
        if ( strstr(aBuffer, "\"stream\":true") != NULL ) {
            ++pState->iStreamRequestCount;
        }
        if ( strstr(aBuffer, "\"model\":\"claude-mock-mm\"") != NULL ) {
            ++pState->iMultimodalModelCount;
        }
        if ( strstr(aBuffer, "\"type\":\"image\"") != NULL ) {
            ++pState->iImageBlockCount;
        }
        if ( strstr(aBuffer, "\"data\":\"iVBORw0KGgo=\"") != NULL ) {
            ++pState->iBase64Count;
        }
        if ( strstr(aBuffer, "\"describe anthropic stream image\"") != NULL ) {
            ++pState->iTextValueCount;
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
    if ( !demo_send_all(hClient, "0\r\n\r\n", 5u) ) return 10u;

    closesocket(hClient);
    closesocket(pState->hListen);
    pState->hListen = INVALID_SOCKET;
    return 0u;
}

static bool demo_stream_on_event(const xllm_event *pEvent, void *pUserData)
{
    demo_stream_event_state *pState = (demo_stream_event_state *)pUserData;

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

static int demo_write_image_file(const char *sPath)
{
    static const unsigned char aPngSig[8] = {0x89u, 'P', 'N', 'G', '\r', '\n', 0x1Au, '\n'};
    FILE *pFile;

    pFile = fopen(sPath, "wb");
    if ( !pFile ) {
        return XRT_NET_ERROR;
    }

    if ( fwrite(aPngSig, 1u, sizeof(aPngSig), pFile) != sizeof(aPngSig) ) {
        fclose(pFile);
        return XRT_NET_ERROR;
    }

    fclose(pFile);
    return XRT_NET_OK;
}

int main(void)
{
#if defined(_WIN32) || defined(_WIN64)
    WSADATA tWsaData;
#endif
    demo_stream_server_state tServer;
    demo_stream_event_state tEvents;
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
    char sImagePath[260];
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
    if ( xllm_register_anthropic_native_adapter(pRuntime) != XRT_NET_OK ) {
        fprintf(stderr, "register built-in adapter failed\n");
        return 8;
    }

    (void)snprintf(sBaseUrl, sizeof(sBaseUrl), "http://127.0.0.1:%u/v1", (unsigned)tServer.uPort);
    xllm_profile_init(&tProfile);
    tProfile.sId = "anthropic-mm-stream";
    tProfile.sProvider = "anthropic";
    tProfile.sAdapter = XLLM_ADAPTER_ANTHROPIC_NATIVE;
    tProfile.sBaseUrl = sBaseUrl;
    tProfile.tAuth.eKind = XLLM_AUTH_API_KEY_HEADER;
    tProfile.tAuth.sSecret = "test-key";
    tProfile.tAuth.sHeaderName = "x-api-key";
    tProfile.tModels.tText.sModelId = "claude-mock";
    tProfile.tModels.tText.tCaps.uFlags = XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT | XLLM_CAP_STREAM;
    tProfile.tModels.tMultimodal.sModelId = "claude-mock-mm";
    tProfile.tModels.tMultimodal.tCaps.uFlags = XLLM_CAP_TEXT_IN | XLLM_CAP_IMAGE_IN | XLLM_CAP_TEXT_OUT | XLLM_CAP_STREAM;
    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "register profile failed\n");
        return 9;
    }

    tCreate.sInitialProfileId = "anthropic-mm-stream";
    pLlm = xllm_create(pRuntime, &tCreate);
    if ( !pLlm ) {
        fprintf(stderr, "create llm failed\n");
        return 10;
    }

    (void)snprintf(sImagePath, sizeof(sImagePath), "build\\smoke_anthropic_stream_mm_image.bin");
    if ( demo_write_image_file(sImagePath) != XRT_NET_OK ) {
        fprintf(stderr, "write image file failed\n");
        return 11;
    }

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "describe anthropic stream image") != XRT_NET_OK ) {
        fprintf(stderr, "turn add text failed\n");
        return 12;
    }
    if ( xllm_turn_add_image_file(&tTurn, sImagePath, "image/png") != XRT_NET_OK ) {
        fprintf(stderr, "turn add image failed\n");
        return 13;
    }

    xllm_call_options_init(&tCall);
    tCall.eStreamMode = XLLM_STREAM_REQUIRE;
    tCall.pfnOnEvent = demo_stream_on_event;
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
    if ( !sText || strcmp(sText, "anthropic stream image") != 0 ) {
        fprintf(stderr, "unexpected response text: %s\n", sText ? sText : "(null)");
        return 15;
    }
    if ( tEvents.iTextDeltaCount != 3 || tEvents.iEndCount != 1 ) {
        fprintf(stderr, "unexpected event counts: text=%d end=%d\n", tEvents.iTextDeltaCount, tEvents.iEndCount);
        return 16;
    }
    if ( tServer.iApiKeyCount != 1 ||
         tServer.iVersionCount != 1 ||
         tServer.iStreamRequestCount != 1 ||
         tServer.iMultimodalModelCount != 1 ||
         tServer.iImageBlockCount != 1 ||
         tServer.iBase64Count != 1 ||
         tServer.iTextValueCount < 1 ) {
        fprintf(
            stderr,
            "unexpected request counters: api=%d version=%d stream=%d model=%d image=%d base64=%d text=%d\n",
            tServer.iApiKeyCount,
            tServer.iVersionCount,
            tServer.iStreamRequestCount,
            tServer.iMultimodalModelCount,
            tServer.iImageBlockCount,
            tServer.iBase64Count,
            tServer.iTextValueCount
        );
        return 17;
    }
    if ( pResponse->tUsage.uInputTokens != 14u || pResponse->tUsage.uOutputTokens != 3u ) {
        fprintf(stderr, "unexpected usage: in=%u out=%u\n", pResponse->tUsage.uInputTokens, pResponse->tUsage.uOutputTokens);
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
