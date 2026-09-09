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
    int iBetaHeaderCount;
    int iMultimodalModelCount;
    int iContentArrayCount;
    int iDocumentBlockCount;
    int iFileSourceCount;
    int iFileIdCount;
    int iTextBlockCount;
    int iTextValueCount;
} demo_server_state;

static uint32 demo_server_thread(ptr pParam)
{
    demo_server_state *pState = (demo_server_state *)pParam;
    SOCKET hClient = INVALID_SOCKET;
    char aBuffer[8192];
    const char *sBody =
        "{\"id\":\"msg_mock_doc_file_id\","
        "\"type\":\"message\","
        "\"role\":\"assistant\","
        "\"model\":\"claude-mock-mm\","
        "\"content\":[{\"type\":\"text\",\"text\":\"anthropic document file id ok\"}],"
        "\"stop_reason\":\"end_turn\","
        "\"usage\":{\"input_tokens\":12,\"output_tokens\":6}}";
    char sResponse[12288];
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
            ++pState->iApiKeyCount;
        }
        if ( strstr(aBuffer, "anthropic-version: 2023-06-01") != NULL ) {
            ++pState->iVersionCount;
        }
        if ( strstr(aBuffer, "anthropic-beta: files-api-2025-04-14") != NULL ) {
            ++pState->iBetaHeaderCount;
        }
        if ( strstr(aBuffer, "\"model\":\"claude-mock-mm\"") != NULL ) {
            ++pState->iMultimodalModelCount;
        }
        if ( strstr(aBuffer, "\"content\":[") != NULL ) {
            ++pState->iContentArrayCount;
        }
        if ( strstr(aBuffer, "\"type\":\"document\"") != NULL ) {
            ++pState->iDocumentBlockCount;
        }
        if ( strstr(aBuffer, "\"source\":{\"type\":\"file\",\"file_id\":\"file_doc_123\"}") != NULL ) {
            ++pState->iFileSourceCount;
        }
        if ( strstr(aBuffer, "\"file_id\":\"file_doc_123\"") != NULL ) {
            ++pState->iFileIdCount;
        }
        if ( strstr(aBuffer, "\"type\":\"text\"") != NULL ) {
            ++pState->iTextBlockCount;
        }
        if ( strstr(aBuffer, "\"summarize anthropic file id document\"") != NULL ) {
            ++pState->iTextValueCount;
        }
    }

    iRespLen = snprintf(
        sResponse,
        sizeof(sResponse),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "request-id: req_anthropic_doc_file_id_1\r\n"
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
    const char *arrBetaHeaders[1];
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
    memset(&tAddr, 0, sizeof(tAddr));
    memset(&tCreate, 0, sizeof(tCreate));
    memset(&tTurn, 0, sizeof(tTurn));

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

    arrBetaHeaders[0] = "files-api-2025-04-14";
    (void)snprintf(sBaseUrl, sizeof(sBaseUrl), "http://127.0.0.1:%u/v1", (unsigned)tServer.uPort);
    xllm_profile_init(&tProfile);
    tProfile.sId = "anthropic-doc-file-id";
    tProfile.sProvider = "anthropic";
    tProfile.sAdapter = XLLM_ADAPTER_ANTHROPIC_NATIVE;
    tProfile.sBaseUrl = sBaseUrl;
    tProfile.tAuth.eKind = XLLM_AUTH_API_KEY_HEADER;
    tProfile.tAuth.sSecret = "test-key";
    tProfile.tAuth.sHeaderName = "x-api-key";
    tProfile.tProviderOptions.psAnthropicBetaHeaders = arrBetaHeaders;
    tProfile.tProviderOptions.iAnthropicBetaHeaderCount = 1u;
    tProfile.tModels.tText.sModelId = "claude-mock";
    tProfile.tModels.tText.tCaps.uFlags = XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT;
    tProfile.tModels.tMultimodal.sModelId = "claude-mock-mm";
    tProfile.tModels.tMultimodal.tCaps.uFlags = XLLM_CAP_TEXT_IN | XLLM_CAP_FILE_IN | XLLM_CAP_TEXT_OUT;
    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "register profile failed\n");
        return 9;
    }

    tCreate.sInitialProfileId = "anthropic-doc-file-id";
    tCreate.sSystemPrompt = "you are an anthropic file id document mock";
    pLlm = xllm_create(pRuntime, &tCreate);
    if ( !pLlm ) {
        fprintf(stderr, "create llm failed\n");
        return 10;
    }

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "summarize anthropic file id document") != XRT_NET_OK ) {
        fprintf(stderr, "turn add text failed\n");
        return 11;
    }
    if ( xllm_turn_add_file_file_id(&tTurn, "file_doc_123", "application/pdf") != XRT_NET_OK ) {
        fprintf(stderr, "turn add file file_id failed\n");
        return 12;
    }

    iStatus = xllm_send(pLlm, &tTurn, NULL, &pResponse);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "send failed: %d\n", iStatus);
        return 13;
    }

    xrtThreadWait(hServerThread);
    xrtThreadDestroy(hServerThread);
    hServerThread = NULL;

    sText = xllm_response_get_text(pResponse);
    if ( !sText || strcmp(sText, "anthropic document file id ok") != 0 ) {
        fprintf(stderr, "unexpected response text: %s\n", sText ? sText : "(null)");
        return 14;
    }

    if ( tServer.iApiKeyCount != 1 ||
         tServer.iVersionCount != 1 ||
         tServer.iBetaHeaderCount != 1 ||
         tServer.iMultimodalModelCount != 1 ||
         tServer.iContentArrayCount != 1 ||
         tServer.iDocumentBlockCount != 1 ||
         tServer.iFileSourceCount != 1 ||
         tServer.iFileIdCount != 1 ||
         tServer.iTextBlockCount < 1 ||
         tServer.iTextValueCount != 1 ) {
        fprintf(
            stderr,
            "unexpected request counters api=%d version=%d beta=%d model=%d content=%d doc=%d file_source=%d file_id=%d text_block=%d text_value=%d\n",
            tServer.iApiKeyCount,
            tServer.iVersionCount,
            tServer.iBetaHeaderCount,
            tServer.iMultimodalModelCount,
            tServer.iContentArrayCount,
            tServer.iDocumentBlockCount,
            tServer.iFileSourceCount,
            tServer.iFileIdCount,
            tServer.iTextBlockCount,
            tServer.iTextValueCount
        );
        return 15;
    }

    xllm_response_free(pResponse);
    xllm_turn_reset(&tTurn);
    xllm_destroy(pLlm);
    xllm_runtime_destroy(pRuntime);
#if defined(_WIN32) || defined(_WIN64)
    WSACleanup();
#endif
    printf("ok: anthropic document file id ok\n");
    return 0;
}
