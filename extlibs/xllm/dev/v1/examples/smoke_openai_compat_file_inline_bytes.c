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
    int iContentArrayCount;
    int iFilePartCount;
    int iFilenameCount;
    int iMimeTypeCount;
    int iFileDataCount;
} demo_server_state;

static uint32 demo_server_thread(ptr pParam)
{
    demo_server_state *pState = (demo_server_state *)pParam;
    SOCKET hClient = INVALID_SOCKET;
    char aBuffer[8192];
    const char *sBody =
        "{\"id\":\"chatcmpl-file-inline\","
        "\"object\":\"chat.completion\","
        "\"model\":\"gpt-mock-mm\","
        "\"choices\":[{\"index\":0,\"message\":{\"role\":\"assistant\",\"content\":\"inline file ok\"},\"finish_reason\":\"stop\"}],"
        "\"usage\":{\"prompt_tokens\":12,\"completion_tokens\":2}}";
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
        if ( strstr(aBuffer, "Authorization: Bearer test-key") != NULL ) {
            ++pState->iRequestCount;
        }
        if ( strstr(aBuffer, "\"content\":[") != NULL ) {
            ++pState->iContentArrayCount;
        }
        if ( strstr(aBuffer, "\"type\":\"file\",\"file\":{") != NULL ) {
            ++pState->iFilePartCount;
        }
        if ( strstr(aBuffer, "\"filename\":\"openai_inline_file.txt\"") != NULL ) {
            ++pState->iFilenameCount;
        }
        if ( strstr(aBuffer, "\"mime_type\":\"text/plain\"") != NULL ) {
            ++pState->iMimeTypeCount;
        }
        if ( strstr(aBuffer, "\"file_data\":\"aGVsbG8gZmlsZQo=\"") != NULL ) {
            ++pState->iFileDataCount;
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
    char sBaseUrl[128];
    const char *sText;
    const char *sUploadPath = "build/openai_inline_file.txt";
    FILE *pUpload = NULL;
    int iStatus = 0;

#if defined(_WIN32) || defined(_WIN64)
    if ( WSAStartup(MAKEWORD(2, 2), &tWsaData) != 0 ) {
        fprintf(stderr, "winsock startup failed\n");
        return 1;
    }
#endif

    pUpload = fopen(sUploadPath, "wb");
    if ( !pUpload ) {
        fprintf(stderr, "create upload file failed\n");
        return 2;
    }
    if ( fwrite("hello file\n", 1u, 11u, pUpload) != 11u ) {
        fclose(pUpload);
        fprintf(stderr, "write upload file failed\n");
        return 3;
    }
    fclose(pUpload);
    pUpload = NULL;

    memset(&tServer, 0, sizeof(tServer));
    memset(&tAddr, 0, sizeof(tAddr));
    memset(&tCreate, 0, sizeof(tCreate));

    tServer.hListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if ( tServer.hListen == INVALID_SOCKET ) {
        fprintf(stderr, "create listen socket failed\n");
        return 4;
    }

    tAddr.sin_family = AF_INET;
    tAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    tAddr.sin_port = htons(0);
    if ( bind(tServer.hListen, (struct sockaddr *)&tAddr, sizeof(tAddr)) == SOCKET_ERROR ) {
        fprintf(stderr, "bind listen socket failed\n");
        return 5;
    }
    if ( listen(tServer.hListen, 1) == SOCKET_ERROR ) {
        fprintf(stderr, "listen failed\n");
        return 6;
    }
    if ( getsockname(tServer.hListen, (struct sockaddr *)&tAddr, &iAddrLen) == SOCKET_ERROR ) {
        fprintf(stderr, "getsockname failed\n");
        return 7;
    }
    tServer.uPort = ntohs(tAddr.sin_port);

    hServerThread = xrtThreadCreate((ptr)demo_server_thread, &tServer, 0);
    if ( !hServerThread ) {
        fprintf(stderr, "create server thread failed\n");
        return 8;
    }

    if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || !pRuntime ) {
        fprintf(stderr, "create runtime failed\n");
        return 9;
    }

    if ( xllm_register_openai_compat_adapter(pRuntime) != XRT_NET_OK ) {
        fprintf(stderr, "register built-in adapter failed\n");
        return 10;
    }

    (void)snprintf(sBaseUrl, sizeof(sBaseUrl), "http://127.0.0.1:%u/v1", (unsigned)tServer.uPort);
    xllm_profile_init(&tProfile);
    tProfile.sId = "openai-file-inline";
    tProfile.sProvider = "openai";
    tProfile.sAdapter = XLLM_ADAPTER_OPENAI_COMPAT;
    tProfile.sBaseUrl = sBaseUrl;
    tProfile.tAuth.eKind = XLLM_AUTH_BEARER;
    tProfile.tAuth.sSecret = "test-key";
    tProfile.tModels.tText.sModelId = "gpt-mock";
    tProfile.tModels.tText.tCaps.uFlags = XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT;
    tProfile.tModels.tMultimodal.sModelId = "gpt-mock-mm";
    tProfile.tModels.tMultimodal.tCaps.uFlags = XLLM_CAP_TEXT_IN | XLLM_CAP_FILE_IN | XLLM_CAP_TEXT_OUT;
    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "register profile failed\n");
        return 11;
    }

    tCreate.sInitialProfileId = "openai-file-inline";
    tCreate.sSystemPrompt = "you are an inline file mock";
    pLlm = xllm_create(pRuntime, &tCreate);
    if ( !pLlm ) {
        fprintf(stderr, "create llm failed\n");
        return 12;
    }

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "summarize this inline file") != XRT_NET_OK ) {
        fprintf(stderr, "turn add text failed\n");
        return 13;
    }
    if ( xllm_turn_add_file(&tTurn, sUploadPath, "text/plain") != XRT_NET_OK ) {
        fprintf(stderr, "turn add inline file failed\n");
        return 14;
    }

    iStatus = xllm_send(pLlm, &tTurn, NULL, &pResponse);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "send failed: %d\n", iStatus);
        return 15;
    }

    xrtThreadWait(hServerThread);
    xrtThreadDestroy(hServerThread);
    hServerThread = NULL;

    sText = xllm_response_get_text(pResponse);
    if ( !sText || strcmp(sText, "inline file ok") != 0 ) {
        fprintf(stderr, "unexpected response text: %s\n", sText ? sText : "(null)");
        return 16;
    }
    if ( tServer.iRequestCount != 1 ||
         tServer.iContentArrayCount != 1 ||
         tServer.iFilePartCount != 1 ||
         tServer.iFilenameCount != 1 ||
         tServer.iMimeTypeCount != 1 ||
         tServer.iFileDataCount != 1 ) {
        fprintf(
            stderr,
            "unexpected inline file counters: req=%d content=%d file_part=%d filename=%d mime=%d file_data=%d\n",
            tServer.iRequestCount,
            tServer.iContentArrayCount,
            tServer.iFilePartCount,
            tServer.iFilenameCount,
            tServer.iMimeTypeCount,
            tServer.iFileDataCount
        );
        return 17;
    }

    printf("ok: %s\n", sText);

    xllm_response_free(pResponse);
    xllm_turn_reset(&tTurn);
    xllm_destroy(pLlm);
    xllm_runtime_destroy(pRuntime);
    if ( tServer.hListen != INVALID_SOCKET ) {
        closesocket(tServer.hListen);
    }
    remove(sUploadPath);
#if defined(_WIN32) || defined(_WIN64)
    WSACleanup();
#endif
    return 0;
}
