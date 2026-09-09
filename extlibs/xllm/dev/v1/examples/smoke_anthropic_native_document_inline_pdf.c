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
    int iMultimodalModelCount;
    int iContentArrayCount;
    int iDocumentBlockCount;
    int iBase64SourceCount;
    int iMediaTypeCount;
    int iDataCount;
    int iTextValueCount;
} demo_server_state;

static uint32 demo_server_thread(ptr pParam)
{
    demo_server_state *pState = (demo_server_state *)pParam;
    SOCKET hClient = INVALID_SOCKET;
    char aBuffer[8192];
    const char *sBody =
        "{\"id\":\"msg_mock_doc_inline\","
        "\"type\":\"message\","
        "\"role\":\"assistant\","
        "\"model\":\"claude-mock-mm\","
        "\"content\":[{\"type\":\"text\",\"text\":\"anthropic inline pdf ok\"}],"
        "\"stop_reason\":\"end_turn\","
        "\"usage\":{\"input_tokens\":11,\"output_tokens\":4}}";
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
        if ( strstr(aBuffer, "\"model\":\"claude-mock-mm\"") != NULL ) {
            ++pState->iMultimodalModelCount;
        }
        if ( strstr(aBuffer, "\"content\":[") != NULL ) {
            ++pState->iContentArrayCount;
        }
        if ( strstr(aBuffer, "\"type\":\"document\"") != NULL ) {
            ++pState->iDocumentBlockCount;
        }
        if ( strstr(aBuffer, "\"source\":{\"type\":\"base64\"") != NULL ) {
            ++pState->iBase64SourceCount;
        }
        if ( strstr(aBuffer, "\"media_type\":\"application/pdf\"") != NULL ) {
            ++pState->iMediaTypeCount;
        }
        if ( strstr(aBuffer, "\"data\":\"JVBERi0xLjQKJSVFT0YK\"") != NULL ) {
            ++pState->iDataCount;
        }
        if ( strstr(aBuffer, "\"summarize inline anthropic pdf\"") != NULL ) {
            ++pState->iTextValueCount;
        }
    }

    iRespLen = snprintf(
        sResponse,
        sizeof(sResponse),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "request-id: req_anthropic_doc_inline_1\r\n"
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

static int demo_write_pdf_file(const char *sPath)
{
    static const unsigned char aPdf[] = "%PDF-1.4\n%%EOF\n";
    FILE *pFile;

    pFile = fopen(sPath, "wb");
    if ( !pFile ) {
        return XRT_NET_ERROR;
    }

    if ( fwrite(aPdf, 1u, sizeof(aPdf) - 1u, pFile) != (sizeof(aPdf) - 1u) ) {
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
    char sBaseUrl[128];
    char sPdfPath[260];
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
    tProfile.sId = "anthropic-doc-inline";
    tProfile.sProvider = "anthropic";
    tProfile.sAdapter = XLLM_ADAPTER_ANTHROPIC_NATIVE;
    tProfile.sBaseUrl = sBaseUrl;
    tProfile.tAuth.eKind = XLLM_AUTH_API_KEY_HEADER;
    tProfile.tAuth.sSecret = "test-key";
    tProfile.tAuth.sHeaderName = "x-api-key";
    tProfile.tModels.tText.sModelId = "claude-mock";
    tProfile.tModels.tText.tCaps.uFlags = XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT;
    tProfile.tModels.tMultimodal.sModelId = "claude-mock-mm";
    tProfile.tModels.tMultimodal.tCaps.uFlags = XLLM_CAP_TEXT_IN | XLLM_CAP_FILE_IN | XLLM_CAP_TEXT_OUT;
    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "register profile failed\n");
        return 9;
    }

    tCreate.sInitialProfileId = "anthropic-doc-inline";
    tCreate.sSystemPrompt = "you are an anthropic inline pdf mock";
    pLlm = xllm_create(pRuntime, &tCreate);
    if ( !pLlm ) {
        fprintf(stderr, "create llm failed\n");
        return 10;
    }

    (void)snprintf(sPdfPath, sizeof(sPdfPath), "build\\smoke_anthropic_inline.pdf");
    if ( demo_write_pdf_file(sPdfPath) != XRT_NET_OK ) {
        fprintf(stderr, "write pdf file failed\n");
        return 11;
    }

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "summarize inline anthropic pdf") != XRT_NET_OK ) {
        fprintf(stderr, "turn add text failed\n");
        return 12;
    }
    if ( xllm_turn_add_file(&tTurn, sPdfPath, "application/pdf") != XRT_NET_OK ) {
        fprintf(stderr, "turn add file failed\n");
        return 13;
    }

    iStatus = xllm_send(pLlm, &tTurn, NULL, &pResponse);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "send failed: %d\n", iStatus);
        return 14;
    }

    xrtThreadWait(hServerThread);
    xrtThreadDestroy(hServerThread);
    hServerThread = NULL;

    sText = xllm_response_get_text(pResponse);
    if ( !sText || strcmp(sText, "anthropic inline pdf ok") != 0 ) {
        fprintf(stderr, "unexpected response text: %s\n", sText ? sText : "(null)");
        return 15;
    }

    if ( tServer.iApiKeyCount != 1 ||
         tServer.iVersionCount != 1 ||
         tServer.iMultimodalModelCount != 1 ||
         tServer.iContentArrayCount != 1 ||
         tServer.iDocumentBlockCount != 1 ||
         tServer.iBase64SourceCount != 1 ||
         tServer.iMediaTypeCount != 1 ||
         tServer.iDataCount != 1 ||
         tServer.iTextValueCount != 1 ) {
        fprintf(
            stderr,
            "unexpected request counters api=%d version=%d model=%d content=%d doc=%d base64=%d media=%d data=%d text=%d\n",
            tServer.iApiKeyCount,
            tServer.iVersionCount,
            tServer.iMultimodalModelCount,
            tServer.iContentArrayCount,
            tServer.iDocumentBlockCount,
            tServer.iBase64SourceCount,
            tServer.iMediaTypeCount,
            tServer.iDataCount,
            tServer.iTextValueCount
        );
        return 16;
    }

    xllm_turn_reset(&tTurn);
    xllm_response_free(pResponse);
    xllm_destroy(pLlm);
    xllm_runtime_destroy(pRuntime);
#if defined(_WIN32) || defined(_WIN64)
    WSACleanup();
#endif
    printf("ok: anthropic inline pdf ok\n");
    return 0;
}
