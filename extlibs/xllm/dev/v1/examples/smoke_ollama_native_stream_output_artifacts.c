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
} demo_stream_server_state;

typedef struct {
    int iTextDeltaCount;
    int iArtifactBeginCount;
    int iArtifactChunkCount;
    int iArtifactReadyCount;
    int iOutputBeginCount;
    int iOutputEndCount;
    int iEndCount;
} demo_stream_event_state;

typedef struct {
    int iBeginCount;
    int iWriteCount;
    int iEndCount;
    size_t iTotalBytes;
} demo_sink_state;

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
        "{\"model\":\"llava-stream-output\",\"message\":{\"role\":\"assistant\",\"content\":\"ollama stream image\",\"images\":[\"iVBORw0KGgo=\"]},\"done\":false}\n"
        "{\"model\":\"llava-stream-output\",\"message\":{\"role\":\"assistant\",\"content\":\"\"},\"done\":true,\"done_reason\":\"stop\",\"prompt_eval_count\":11,\"eval_count\":4}\n";
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

    if ( !pState || !pEvent ) {
        return true;
    }

    switch ( pEvent->eType ) {
        case XLLM_EVENT_TEXT_DELTA:
            ++pState->iTextDeltaCount;
            break;
        case XLLM_EVENT_ARTIFACT_BEGIN:
            ++pState->iArtifactBeginCount;
            break;
        case XLLM_EVENT_ARTIFACT_CHUNK:
            ++pState->iArtifactChunkCount;
            break;
        case XLLM_EVENT_ARTIFACT_READY:
            ++pState->iArtifactReadyCount;
            break;
        case XLLM_EVENT_OUTPUT_BEGIN:
            ++pState->iOutputBeginCount;
            break;
        case XLLM_EVENT_OUTPUT_END:
            ++pState->iOutputEndCount;
            break;
        case XLLM_EVENT_END:
            ++pState->iEndCount;
            break;
        default:
            break;
    }

    return true;
}

static bool demo_sink_begin(void *pCtx, const xllm_artifact_info *pInfo)
{
    demo_sink_state *pState = (demo_sink_state *)pCtx;

    if ( !pState || !pInfo || !pInfo->sArtifactId ) {
        return false;
    }

    ++pState->iBeginCount;
    return true;
}

static bool demo_sink_write(void *pCtx, const char *sArtifactId, const void *pData, size_t iSize)
{
    demo_sink_state *pState = (demo_sink_state *)pCtx;

    if ( !pState || !sArtifactId || !pData || iSize == 0u ) {
        return false;
    }

    ++pState->iWriteCount;
    pState->iTotalBytes += iSize;
    return true;
}

static bool demo_sink_end(void *pCtx, const char *sArtifactId, bool bCompleted)
{
    demo_sink_state *pState = (demo_sink_state *)pCtx;

    if ( !pState || !sArtifactId || !bCompleted ) {
        return false;
    }

    ++pState->iEndCount;
    return true;
}

int main(void)
{
#if defined(_WIN32) || defined(_WIN64)
    WSADATA tWsaData;
#endif
    demo_stream_server_state tServer;
    demo_stream_event_state tEvents;
    demo_sink_state tSink;
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
    xllm_artifact_sink tArtifactSink;
    char sBaseUrl[128];
    const xllm_output_item *pOutput;
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
    memset(&tSink, 0, sizeof(tSink));
    memset(&tAddr, 0, sizeof(tAddr));
    memset(&tCreate, 0, sizeof(tCreate));
    memset(&tArtifactSink, 0, sizeof(tArtifactSink));

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
        fprintf(stderr, "register adapter failed\n");
        return 8;
    }

    (void)snprintf(sBaseUrl, sizeof(sBaseUrl), "http://127.0.0.1:%u", (unsigned)tServer.uPort);
    xllm_profile_init(&tProfile);
    tProfile.sId = "ollama-stream-output-artifacts";
    tProfile.sProvider = "ollama";
    tProfile.sAdapter = XLLM_ADAPTER_OLLAMA_NATIVE;
    tProfile.sBaseUrl = sBaseUrl;
    tProfile.tModels.tText.sModelId = "llama-stream-output";
    tProfile.tModels.tText.tCaps.uFlags = XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT | XLLM_CAP_STREAM;
    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "register profile failed\n");
        return 9;
    }

    tCreate.sInitialProfileId = "ollama-stream-output-artifacts";
    pLlm = xllm_create(pRuntime, &tCreate);
    if ( !pLlm ) {
        fprintf(stderr, "create llm failed\n");
        return 10;
    }

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "show ollama stream output image") != XRT_NET_OK ) {
        fprintf(stderr, "turn add text failed\n");
        return 11;
    }

    xllm_call_options_init(&tCall);
    tCall.eStreamMode = XLLM_STREAM_REQUIRE;
    tCall.pfnOnEvent = demo_stream_on_event;
    tCall.pUserData = &tEvents;
    tCall.eArtifactPolicy = XLLM_ARTIFACT_INLINE_SMALL;
    tArtifactSink.pfnBegin = demo_sink_begin;
    tArtifactSink.pfnWrite = demo_sink_write;
    tArtifactSink.pfnEnd = demo_sink_end;
    tArtifactSink.pCtx = &tSink;
    tCall.pArtifactSink = &tArtifactSink;

    iStatus = xllm_send(pLlm, &tTurn, &tCall, &pResponse);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "send failed: %d\n", iStatus);
        return 12;
    }

    xrtThreadWait(hServerThread);
    xrtThreadDestroy(hServerThread);
    hServerThread = NULL;

    sText = xllm_response_get_text(pResponse);
    if ( !sText || strcmp(sText, "ollama stream image") != 0 ) {
        fprintf(stderr, "unexpected visible text: %s\n", sText ? sText : "(null)");
        return 13;
    }

    pOutput = xllm_response_get_output(pResponse, 0u);
    if ( !pOutput || pOutput->eKind != XLLM_OUTPUT_MESSAGE || pOutput->as.tMessage.iPartCount != 2u ) {
        fprintf(stderr, "unexpected streamed message output\n");
        return 14;
    }

    if ( pOutput->as.tMessage.pParts[0].eKind != XLLM_PART_TEXT ||
         pOutput->as.tMessage.pParts[1].eKind != XLLM_PART_IMAGE ||
         pOutput->as.tMessage.pParts[1].as.tSource.eKind != XLLM_SOURCE_INLINE_BYTES ||
         pOutput->as.tMessage.pParts[1].as.tSource.as.tBytes.iSize != 8u ) {
        fprintf(stderr, "unexpected streamed output part layout\n");
        return 15;
    }

    if ( tServer.iApiChatCount != 1 || tServer.iStreamTrueCount != 1 ) {
        fprintf(stderr, "unexpected request counters api=%d stream=%d\n", tServer.iApiChatCount, tServer.iStreamTrueCount);
        return 16;
    }

    if ( tEvents.iTextDeltaCount != 1 ||
         tEvents.iArtifactBeginCount != 1 ||
         tEvents.iArtifactChunkCount != 1 ||
         tEvents.iArtifactReadyCount != 1 ||
         tEvents.iOutputBeginCount != 1 ||
         tEvents.iOutputEndCount != 1 ||
         tEvents.iEndCount != 1 ) {
        fprintf(
            stderr,
            "unexpected stream event counters text=%d artifact_begin=%d artifact_chunk=%d artifact_ready=%d output_begin=%d output_end=%d end=%d\n",
            tEvents.iTextDeltaCount,
            tEvents.iArtifactBeginCount,
            tEvents.iArtifactChunkCount,
            tEvents.iArtifactReadyCount,
            tEvents.iOutputBeginCount,
            tEvents.iOutputEndCount,
            tEvents.iEndCount
        );
        return 17;
    }

    if ( tSink.iBeginCount != 1 || tSink.iWriteCount != 1 || tSink.iEndCount != 1 || tSink.iTotalBytes != 8u ) {
        fprintf(
            stderr,
            "unexpected sink counters begin=%d write=%d end=%d bytes=%u\n",
            tSink.iBeginCount,
            tSink.iWriteCount,
            tSink.iEndCount,
            (unsigned)tSink.iTotalBytes
        );
        return 18;
    }

    printf("ok: %s\n", sText);

    xllm_response_free(pResponse);
    xllm_turn_reset(&tTurn);
    xllm_destroy(pLlm);
    xllm_runtime_destroy(pRuntime);
#if defined(_WIN32) || defined(_WIN64)
    WSACleanup();
#endif
    return 0;
}
