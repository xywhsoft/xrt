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
    int iAuthedRequestCount;
    int iStreamRequestCount;
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
    char aBuffer[4096];
    const char *sHeader =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: close\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n";
    const char *sEvent1 =
        "data: {\"id\":\"chatcmpl-stream-artifacts\",\"object\":\"chat.completion.chunk\",\"model\":\"gpt-mock\","
        "\"choices\":[{\"index\":0,\"delta\":{\"role\":\"assistant\",\"content\":[{\"type\":\"output_text\",\"text\":\"Here is the generated output.\"}]},\"finish_reason\":null}]}\n\n";
    const char *sEvent2 =
        "data: {\"id\":\"chatcmpl-stream-artifacts\",\"object\":\"chat.completion.chunk\",\"model\":\"gpt-mock\","
        "\"choices\":[{\"index\":0,\"delta\":{\"content\":[{\"type\":\"image_url\",\"image_url\":{\"url\":\"https://example.invalid/generated.png\",\"mime_type\":\"image/png\",\"name\":\"generated.png\"}}]},\"finish_reason\":null}]}\n\n";
    const char *sEvent3 =
        "data: {\"id\":\"chatcmpl-stream-artifacts\",\"object\":\"chat.completion.chunk\",\"model\":\"gpt-mock\","
        "\"choices\":[{\"index\":0,\"delta\":{\"content\":[{\"type\":\"file\",\"file\":{\"file_data\":\"cmVwb3J0LWJ5dGVz\",\"mime_type\":\"application/pdf\",\"filename\":\"report.pdf\"}}]},\"finish_reason\":null}]}\n\n";
    const char *sEvent4 =
        "data: {\"id\":\"chatcmpl-stream-artifacts\",\"object\":\"chat.completion.chunk\",\"model\":\"gpt-mock\","
        "\"choices\":[{\"index\":0,\"delta\":{},\"finish_reason\":\"stop\"}]}\n\n";
    const char *sEvent5 =
        "data: {\"id\":\"chatcmpl-stream-artifacts\",\"object\":\"chat.completion.chunk\",\"model\":\"gpt-mock\","
        "\"choices\":[],\"usage\":{\"prompt_tokens\":10,\"completion_tokens\":6}}\n\n";
    const char *sDone = "data: [DONE]\n\n";
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
        if ( strstr(aBuffer, "Authorization: Bearer test-key") != NULL ) {
            ++pState->iAuthedRequestCount;
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
    if ( !demo_send_chunk(hClient, sDone) ) return 9u;
    if ( !demo_send_all(hClient, "0\r\n\r\n", 5u) ) return 10u;

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
    if ( xllm_register_openai_compat_adapter(pRuntime) != XRT_NET_OK ) {
        fprintf(stderr, "register adapter failed\n");
        return 8;
    }

    (void)snprintf(sBaseUrl, sizeof(sBaseUrl), "http://127.0.0.1:%u/v1", (unsigned)tServer.uPort);
    xllm_profile_init(&tProfile);
    tProfile.sId = "openai-stream-output-artifacts";
    tProfile.sProvider = "openai";
    tProfile.sAdapter = XLLM_ADAPTER_OPENAI_COMPAT;
    tProfile.sBaseUrl = sBaseUrl;
    tProfile.tAuth.eKind = XLLM_AUTH_BEARER;
    tProfile.tAuth.sSecret = "test-key";
    tProfile.tModels.tText.sModelId = "gpt-mock";
    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "register profile failed\n");
        return 9;
    }

    tCreate.sInitialProfileId = "openai-stream-output-artifacts";
    pLlm = xllm_create(pRuntime, &tCreate);
    if ( !pLlm ) {
        fprintf(stderr, "create llm failed\n");
        return 10;
    }

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "show me the generated assets") != XRT_NET_OK ) {
        fprintf(stderr, "turn add text failed\n");
        return 11;
    }

    xllm_call_options_init(&tCall);
    tCall.eStreamMode = XLLM_STREAM_REQUIRE;
    tCall.pfnOnEvent = demo_stream_on_event;
    tCall.pUserData = &tEvents;
    tArtifactSink.pCtx = &tSink;
    tArtifactSink.pfnBegin = demo_sink_begin;
    tArtifactSink.pfnWrite = demo_sink_write;
    tArtifactSink.pfnEnd = demo_sink_end;
    tCall.pArtifactSink = &tArtifactSink;

    iStatus = xllm_send(pLlm, &tTurn, &tCall, &pResponse);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "send failed: %d\n", iStatus);
        return 12;
    }

    xrtThreadWait(hServerThread);
    xrtThreadDestroy(hServerThread);
    hServerThread = NULL;

    if ( tServer.iAuthedRequestCount != 1 || tServer.iStreamRequestCount != 1 ) {
        fprintf(stderr, "unexpected request counters: auth=%d stream=%d\n", tServer.iAuthedRequestCount, tServer.iStreamRequestCount);
        return 13;
    }

    sText = xllm_response_get_text(pResponse);
    if ( !sText || strcmp(sText, "Here is the generated output.") != 0 ) {
        fprintf(stderr, "unexpected response text: %s\n", sText ? sText : "(null)");
        return 14;
    }

    if ( pResponse->tUsage.uInputTokens != 10u || pResponse->tUsage.uOutputTokens != 6u ) {
        fprintf(stderr, "unexpected usage: in=%u out=%u\n", pResponse->tUsage.uInputTokens, pResponse->tUsage.uOutputTokens);
        return 15;
    }

    if ( xllm_response_get_output_count(pResponse) != 1u ) {
        fprintf(stderr, "unexpected output count: %u\n", (unsigned)xllm_response_get_output_count(pResponse));
        return 16;
    }

    pOutput = xllm_response_get_output(pResponse, 0u);
    if ( !pOutput || pOutput->eKind != XLLM_OUTPUT_MESSAGE || pOutput->as.tMessage.iPartCount != 3u ) {
        fprintf(stderr, "unexpected message output shape\n");
        return 17;
    }

    if ( pOutput->as.tMessage.pParts[0].eKind != XLLM_PART_TEXT ||
         pOutput->as.tMessage.pParts[0].as.tSource.eKind != XLLM_SOURCE_INLINE_TEXT ||
         !pOutput->as.tMessage.pParts[0].as.tSource.as.sText ||
         strcmp(pOutput->as.tMessage.pParts[0].as.tSource.as.sText, "Here is the generated output.") != 0 ) {
        fprintf(stderr, "unexpected text part\n");
        return 18;
    }

    if ( pOutput->as.tMessage.pParts[1].eKind != XLLM_PART_IMAGE ||
         pOutput->as.tMessage.pParts[1].as.tSource.eKind != XLLM_SOURCE_URL ||
         !pOutput->as.tMessage.pParts[1].as.tSource.as.sUrl ||
         strcmp(pOutput->as.tMessage.pParts[1].as.tSource.as.sUrl, "https://example.invalid/generated.png") != 0 ) {
        fprintf(stderr, "unexpected image part\n");
        return 19;
    }

    if ( pOutput->as.tMessage.pParts[2].eKind != XLLM_PART_FILE ||
         pOutput->as.tMessage.pParts[2].as.tSource.eKind != XLLM_SOURCE_INLINE_BYTES ||
         !pOutput->as.tMessage.pParts[2].as.tSource.as.tBytes.pData ||
         pOutput->as.tMessage.pParts[2].as.tSource.as.tBytes.iSize != 12u ) {
        fprintf(stderr, "unexpected file part\n");
        return 20;
    }

    if ( tEvents.iTextDeltaCount != 1 ||
         tEvents.iArtifactBeginCount != 2 ||
         tEvents.iArtifactChunkCount != 1 ||
         tEvents.iArtifactReadyCount != 2 ||
         tEvents.iOutputBeginCount != 1 ||
         tEvents.iOutputEndCount != 1 ||
         tEvents.iEndCount != 1 ) {
        fprintf(
            stderr,
            "unexpected event counts text=%d artifact_begin=%d artifact_chunk=%d artifact_ready=%d output_begin=%d output_end=%d end=%d\n",
            tEvents.iTextDeltaCount,
            tEvents.iArtifactBeginCount,
            tEvents.iArtifactChunkCount,
            tEvents.iArtifactReadyCount,
            tEvents.iOutputBeginCount,
            tEvents.iOutputEndCount,
            tEvents.iEndCount
        );
        return 21;
    }

    if ( tSink.iBeginCount != 2 || tSink.iWriteCount != 1 || tSink.iEndCount != 2 || tSink.iTotalBytes != 12u ) {
        fprintf(
            stderr,
            "unexpected sink counters begin=%d write=%d end=%d bytes=%u\n",
            tSink.iBeginCount,
            tSink.iWriteCount,
            tSink.iEndCount,
            (unsigned)tSink.iTotalBytes
        );
        return 22;
    }

    printf("smoke_openai_compat_stream_output_artifacts ok\n");

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
