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
    int iRequestCount;
} demo_server_state;

typedef struct {
    int iRequestTraceCount;
    int iIncompleteTraceCount;
} demo_trace_state;

static uint32 demo_server_thread(ptr pParam)
{
    demo_server_state *pState = (demo_server_state *)pParam;
    SOCKET hClient = INVALID_SOCKET;
    char aBuffer[4096];
    const char *sBody =
        "{"
        "\"model\":\"llama-mock\","
        "\"message\":{\"role\":\"assistant\",\"content\":\"partial answer\"},"
        "\"done\":true,"
        "\"done_reason\":\"length\","
        "\"prompt_eval_count\":11,"
        "\"eval_count\":5"
        "}";
    char sResponse[8192];
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
            ++pState->iRequestCount;
        }
    }

    iRespLen = snprintf(
        sResponse,
        sizeof(sResponse),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Connection: close\r\n"
        "Content-Length: %u\r\n"
        "\r\n"
        "%s",
        (unsigned)strlen(sBody),
        sBody
    );

    if ( iRespLen > 0 ) {
        size_t iSent = 0u;
        while ( iSent < (size_t)iRespLen ) {
            int iNow = send(hClient, sResponse + iSent, (int)((size_t)iRespLen - iSent), 0);
            if ( iNow <= 0 ) {
                break;
            }
            iSent += (size_t)iNow;
        }
    }

    closesocket(hClient);
    closesocket(pState->hListen);
    pState->hListen = INVALID_SOCKET;
    return 0u;
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
        if ( sResponseStatus && strcmp(sResponseStatus, "incomplete") == 0 ) {
            ++pState->iIncompleteTraceCount;
        }
    }
}

int main(void)
{
#if defined(_WIN32) || defined(_WIN64)
    WSADATA tWsaData;
#endif
    demo_server_state tServer;
    demo_trace_state tTrace;
    struct sockaddr_in tAddr;
    socklen_t iAddrLen = sizeof(tAddr);
    xthread hServerThread = NULL;
    xllm_runtime *pRuntime = NULL;
    xllm *pLlm = NULL;
    xllm_response *pResponse = NULL;
    xllm_profile tProfile;
    xllm_create_options tCreate;
    xllm_turn tTurn;
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
    memset(&tTrace, 0, sizeof(tTrace));
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
    if ( xllm_register_ollama_native_adapter(pRuntime) != XRT_NET_OK ) {
        fprintf(stderr, "register built-in adapter failed\n");
        return 9;
    }

    (void)snprintf(sBaseUrl, sizeof(sBaseUrl), "http://127.0.0.1:%u", (unsigned)tServer.uPort);
    xllm_profile_init(&tProfile);
    tProfile.sId = "ollama-length";
    tProfile.sProvider = "ollama";
    tProfile.sAdapter = XLLM_ADAPTER_OLLAMA_NATIVE;
    tProfile.sBaseUrl = sBaseUrl;
    tProfile.tAuth.eKind = XLLM_AUTH_NONE;
    tProfile.tModels.tText.sModelId = "llama-mock";
    tProfile.tModels.tText.tCaps.uFlags = XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT;
    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "register profile failed\n");
        return 10;
    }

    tCreate.sInitialProfileId = "ollama-length";
    tCreate.sSystemPrompt = "You are ollama length test";
    pLlm = xllm_create(pRuntime, &tCreate);
    if ( !pLlm ) {
        fprintf(stderr, "create llm failed\n");
        return 11;
    }

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "truncate please") != XRT_NET_OK ) {
        fprintf(stderr, "turn add text failed\n");
        return 12;
    }

    if ( xllm_send(pLlm, &tTurn, NULL, &pResponse) != XRT_NET_OK || !pResponse ) {
        fprintf(stderr, "xllm_send failed\n");
        return 13;
    }

    sText = xllm_response_get_text(pResponse);
    if ( pResponse->eStatus != XLLM_STATUS_INCOMPLETE ) {
        fprintf(stderr, "expected incomplete status\n");
        iStatus = 14;
    } else if ( !pResponse->sFinishReason || strcmp(pResponse->sFinishReason, "length") != 0 ) {
        fprintf(stderr, "unexpected finish reason\n");
        iStatus = 15;
    } else if ( !sText || strcmp(sText, "partial answer") != 0 ) {
        fprintf(stderr, "unexpected response text\n");
        iStatus = 16;
    } else if ( tServer.iRequestCount != 1 ) {
        fprintf(stderr, "server did not receive request\n");
        iStatus = 17;
    } else if ( tTrace.iRequestTraceCount != 1 || tTrace.iIncompleteTraceCount != 1 ) {
        fprintf(stderr, "trace counts mismatch\n");
        iStatus = 18;
    }

    xllm_response_free(pResponse);
    xllm_turn_reset(&tTurn);
    xllm_destroy(pLlm);
    xllm_runtime_destroy(pRuntime);
    if ( hServerThread ) {
        xrtThreadWait(hServerThread);
    }
#if defined(_WIN32) || defined(_WIN64)
    WSACleanup();
#endif
    return iStatus;
}
