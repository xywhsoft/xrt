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
    int iMultimodalModelCount;
    int iImageArrayCount;
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

static uint32 demo_stream_server_thread(ptr pParam)
{
    demo_stream_server_state *pState = (demo_stream_server_state *)pParam;
    SOCKET hClient = INVALID_SOCKET;
    char aBuffer[8192];
    const char *sBody =
        "{\"model\":\"llava-mock\",\"message\":{\"role\":\"assistant\",\"content\":\"ollama \"},\"done\":false}\n"
        "{\"model\":\"llava-mock\",\"message\":{\"role\":\"assistant\",\"content\":\"stream \"},\"done\":false}\n"
        "{\"model\":\"llava-mock\",\"message\":{\"role\":\"assistant\",\"content\":\"image\"},\"done\":false}\n"
        "{\"model\":\"llava-mock\",\"message\":{\"role\":\"assistant\",\"content\":\"\"},\"done\":true,\"done_reason\":\"stop\",\"prompt_eval_count\":12,\"eval_count\":3}\n";
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
        if ( strstr(aBuffer, "\"model\":\"llava-mock\"") != NULL ) {
            ++pState->iMultimodalModelCount;
        }
        if ( strstr(aBuffer, "\"images\":[\"iVBORw0KGgo=\"]") != NULL ) {
            ++pState->iImageArrayCount;
        }
        if ( strstr(aBuffer, "iVBORw0KGgo=") != NULL ) {
            ++pState->iBase64Count;
        }
        if ( strstr(aBuffer, "\"describe ollama stream image\"") != NULL ) {
            ++pState->iTextValueCount;
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
    if ( xllm_register_ollama_native_adapter(pRuntime) != XRT_NET_OK ) {
        fprintf(stderr, "register built-in adapter failed\n");
        return 8;
    }

    (void)snprintf(sBaseUrl, sizeof(sBaseUrl), "http://127.0.0.1:%u", (unsigned)tServer.uPort);
    xllm_profile_init(&tProfile);
    tProfile.sId = "ollama-mm-stream";
    tProfile.sProvider = "ollama";
    tProfile.sAdapter = XLLM_ADAPTER_OLLAMA_NATIVE;
    tProfile.sBaseUrl = sBaseUrl;
    tProfile.tAuth.eKind = XLLM_AUTH_NONE;
    tProfile.tModels.tText.sModelId = "llama-mock";
    tProfile.tModels.tText.tCaps.uFlags = XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT | XLLM_CAP_STREAM;
    tProfile.tModels.tMultimodal.sModelId = "llava-mock";
    tProfile.tModels.tMultimodal.tCaps.uFlags = XLLM_CAP_TEXT_IN | XLLM_CAP_IMAGE_IN | XLLM_CAP_TEXT_OUT | XLLM_CAP_STREAM;
    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "register profile failed\n");
        return 9;
    }

    tCreate.sInitialProfileId = "ollama-mm-stream";
    pLlm = xllm_create(pRuntime, &tCreate);
    if ( !pLlm ) {
        fprintf(stderr, "create llm failed\n");
        return 10;
    }

    (void)snprintf(sImagePath, sizeof(sImagePath), "build\\smoke_ollama_stream_mm_image.bin");
    if ( demo_write_image_file(sImagePath) != XRT_NET_OK ) {
        fprintf(stderr, "write image file failed\n");
        return 11;
    }

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "describe ollama stream image") != XRT_NET_OK ) {
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

    if ( xllm_send(pLlm, &tTurn, &tCall, &pResponse) != XRT_NET_OK || !pResponse ) {
        fprintf(stderr, "xllm_send failed\n");
        return 14;
    }

    sText = xllm_response_get_text(pResponse);
    if ( !sText || strcmp(sText, "ollama stream image") != 0 ) {
        fprintf(stderr, "unexpected response text: %s\n", sText ? sText : "(null)");
        return 15;
    }
    if ( tEvents.iTextDeltaCount != 3 || tEvents.iEndCount != 1 ) {
        fprintf(stderr, "unexpected event counts: text=%d end=%d\n", tEvents.iTextDeltaCount, tEvents.iEndCount);
        return 16;
    }
    if ( tServer.iApiChatCount != 1 ||
         tServer.iStreamTrueCount != 1 ||
         tServer.iMultimodalModelCount != 1 ||
         tServer.iImageArrayCount != 1 ||
         tServer.iBase64Count < 1 ||
         tServer.iTextValueCount < 1 ) {
        fprintf(
            stderr,
            "unexpected request counters: api=%d stream=%d model=%d images=%d base64=%d text=%d\n",
            tServer.iApiChatCount,
            tServer.iStreamTrueCount,
            tServer.iMultimodalModelCount,
            tServer.iImageArrayCount,
            tServer.iBase64Count,
            tServer.iTextValueCount
        );
        return 17;
    }
    if ( pResponse->tUsage.uInputTokens != 12u || pResponse->tUsage.uOutputTokens != 3u ) {
        fprintf(stderr, "unexpected usage: in=%u out=%u\n", pResponse->tUsage.uInputTokens, pResponse->tUsage.uOutputTokens);
        return 18;
    }

    printf("ok: %s\n", sText);

    xllm_response_free(pResponse);
    xllm_destroy(pLlm);
    xllm_runtime_destroy(pRuntime);
    xllm_turn_reset(&tTurn);

    xrtThreadWait(hServerThread);

#if defined(_WIN32) || defined(_WIN64)
    WSACleanup();
#endif

    return 0;
}
