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
    int iVersionCount;
    int iUnexpectedResponseFormatCount;
} demo_server_state;

static uint32 demo_server_thread(ptr pParam)
{
    demo_server_state *pState = (demo_server_state *)pParam;
    SOCKET hClient = INVALID_SOCKET;
    char aBuffer[4096];
    const char *sBody =
        "{\"id\":\"msg_mock_json\","
        "\"type\":\"message\","
        "\"role\":\"assistant\","
        "\"model\":\"claude-mock\","
        "\"content\":[{\"type\":\"text\",\"text\":\"Result:\\n```json\\n{\\\"value\\\":42}\\n```\"}],"
        "\"stop_reason\":\"end_turn\","
        "\"usage\":{\"input_tokens\":9,\"output_tokens\":6}}";
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
        if ( strstr(aBuffer, "x-api-key: test-key") != NULL ) {
            ++pState->iRequestCount;
        }
        if ( strstr(aBuffer, "anthropic-version: 2023-06-01") != NULL ) {
            ++pState->iVersionCount;
        }
        if ( strstr(aBuffer, "\"response_format\":") != NULL ) {
            ++pState->iUnexpectedResponseFormatCount;
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
    xllm *pLlm = NULL;
    xllm_response *pResponse = NULL;
    xllm_profile tProfile;
    xllm_create_options tCreate;
    xllm_turn tTurn;
    xllm_call_options tCall;
    char sBaseUrl[128];
    const xvalue *pJson = NULL;
    char *sJson = NULL;
    const char *sText;
    int iStatus = 0;

#if defined(_WIN32) || defined(_WIN64)
    if ( WSAStartup(MAKEWORD(2, 2), &tWsaData) != 0 ) {
        fprintf(stderr, "winsock startup failed\n");
        return 1;
    }
#endif

    memset(&tServer, 0, sizeof(tServer));
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

    if ( xllm_register_anthropic_native_adapter(pRuntime) != XRT_NET_OK ) {
        fprintf(stderr, "register built-in adapter failed\n");
        return 8;
    }

    (void)snprintf(sBaseUrl, sizeof(sBaseUrl), "http://127.0.0.1:%u/v1", (unsigned)tServer.uPort);
    xllm_profile_init(&tProfile);
    tProfile.sId = "anthropic-local-json";
    tProfile.sProvider = "anthropic";
    tProfile.sAdapter = XLLM_ADAPTER_ANTHROPIC_NATIVE;
    tProfile.sBaseUrl = sBaseUrl;
    tProfile.tAuth.eKind = XLLM_AUTH_API_KEY_HEADER;
    tProfile.tAuth.sSecret = "test-key";
    tProfile.tModels.tText.sModelId = "claude-mock";
    tProfile.tModels.tText.eCapMode = XLLM_CAP_MODE_EXACT;
    tProfile.tModels.tText.tCaps.uFlags = XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT;
    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "register profile failed\n");
        return 9;
    }

    tCreate.sInitialProfileId = "anthropic-local-json";
    tCreate.sSystemPrompt = "extract best effort json";
    pLlm = xllm_create(pRuntime, &tCreate);
    if ( !pLlm ) {
        fprintf(stderr, "create llm failed\n");
        return 10;
    }

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "return wrapped json") != XRT_NET_OK ) {
        fprintf(stderr, "turn add text failed\n");
        return 11;
    }
    tTurn.tResponseFormat.eKind = XLLM_RESPONSE_JSON;

    xllm_call_options_init(&tCall);
    tCall.bBestEffortStructuredOutput = true;

    iStatus = xllm_send(pLlm, &tTurn, &tCall, &pResponse);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "send failed: %d\n", iStatus);
        return 12;
    }

    xrtThreadWait(hServerThread);
    xrtThreadDestroy(hServerThread);
    hServerThread = NULL;

    pJson = xllm_response_get_json(pResponse, 0u, 0u);
    if ( !pJson ) {
        fprintf(stderr, "missing json output\n");
        return 13;
    }

    sJson = (char *)xrtStringifyJSON(*pJson, 0, NULL);
    if ( !sJson || strcmp(sJson, "{\"value\":42}") != 0 ) {
        fprintf(stderr, "unexpected normalized json: %s\n", sJson ? sJson : "(null)");
        return 14;
    }

    sText = xllm_response_get_text(pResponse);
    if ( !sText || strcmp(sText, "{\"value\":42}") != 0 ) {
        fprintf(stderr, "unexpected visible text: %s\n", sText ? sText : "(null)");
        return 15;
    }

    if ( tServer.iRequestCount != 1 || tServer.iVersionCount != 1 || tServer.iUnexpectedResponseFormatCount != 0 ) {
        fprintf(
            stderr,
            "unexpected request counters: request=%d version=%d response_format=%d\n",
            tServer.iRequestCount,
            tServer.iVersionCount,
            tServer.iUnexpectedResponseFormatCount
        );
        return 16;
    }

    printf("ok: %s\n", sJson);

    xrtFree(sJson);
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
