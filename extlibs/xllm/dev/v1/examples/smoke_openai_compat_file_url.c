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
    int iFilePathCount;
} demo_file_server_state;

typedef struct {
    SOCKET hListen;
    uint16 uPort;
    int iRequestCount;
    int iContentArrayCount;
    int iFilePartCount;
    int iFilenameCount;
    int iMimeTypeCount;
    int iFileDataCount;
    int iFileIdCount;
} demo_chat_server_state;

static bool demo_send_all(SOCKET hSocket, const void *pData, size_t iLen)
{
    const char *sData = (const char *)pData;
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

static bool demo_read_http_message(SOCKET hSocket, char *sBuffer, size_t iCapacity)
{
    size_t iUsed = 0u;
    size_t iRequired = 0u;
    bool bHeaderParsed = false;

    if ( !sBuffer || iCapacity < 8u ) {
        return false;
    }

    while ( iUsed + 1u < iCapacity ) {
        char *sHeaderEnd;
        int iRecv = recv(hSocket, sBuffer + iUsed, (int)(iCapacity - iUsed - 1u), 0);
        if ( iRecv <= 0 ) {
            return false;
        }
        iUsed += (size_t)iRecv;
        sBuffer[iUsed] = '\0';

        if ( !bHeaderParsed ) {
            sHeaderEnd = strstr(sBuffer, "\r\n\r\n");
            if ( sHeaderEnd != NULL ) {
                const char *sContentLength = strstr(sBuffer, "Content-Length:");
                size_t iHeaderLen = (size_t)((sHeaderEnd + 4) - sBuffer);
                size_t iContentLength = 0u;

                if ( sContentLength != NULL && sContentLength < sHeaderEnd ) {
                    sContentLength += strlen("Content-Length:");
                    while ( *sContentLength == ' ' ) {
                        ++sContentLength;
                    }
                    iContentLength = (size_t)strtoul(sContentLength, NULL, 10);
                }
                iRequired = iHeaderLen + iContentLength;
                bHeaderParsed = true;
                if ( iUsed >= iRequired ) {
                    break;
                }
            }
        } else if ( iUsed >= iRequired ) {
            break;
        }
    }

    return bHeaderParsed;
}

static uint32 demo_file_server_thread(ptr pParam)
{
    static const char sFileBody[] = "hello file\n";
    demo_file_server_state *pState = (demo_file_server_state *)pParam;
    SOCKET hClient = INVALID_SOCKET;
    char aBuffer[4096];
    char sResponse[1024];
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

    ++pState->iAcceptCount;
    if ( demo_read_http_message(hClient, aBuffer, sizeof(aBuffer)) ) {
        if ( strstr(aBuffer, "GET /doc.txt HTTP/1.1\r\n") != NULL ) {
            ++pState->iFilePathCount;
        }
    }

    iRespLen = snprintf(
        sResponse,
        sizeof(sResponse),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Connection: close\r\n"
        "Content-Length: %u\r\n"
        "\r\n",
        (unsigned)(sizeof(sFileBody) - 1u)
    );
    if ( iRespLen > 0 ) {
        (void)demo_send_all(hClient, sResponse, (size_t)iRespLen);
        (void)demo_send_all(hClient, sFileBody, sizeof(sFileBody) - 1u);
    }

    closesocket(hClient);
    closesocket(pState->hListen);
    pState->hListen = INVALID_SOCKET;
    return 0u;
}

static uint32 demo_chat_server_thread(ptr pParam)
{
    demo_chat_server_state *pState = (demo_chat_server_state *)pParam;
    SOCKET hClient = INVALID_SOCKET;
    char aBuffer[8192];
    const char *sBody =
        "{\"id\":\"chatcmpl-file-url\","
        "\"object\":\"chat.completion\","
        "\"model\":\"gpt-mock-mm\","
        "\"choices\":[{\"index\":0,\"message\":{\"role\":\"assistant\",\"content\":\"url file ok\"},\"finish_reason\":\"stop\"}],"
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
        if ( strstr(aBuffer, "\"filename\":\"upload.bin\"") != NULL ) {
            ++pState->iFilenameCount;
        }
        if ( strstr(aBuffer, "\"mime_type\":\"text/plain\"") != NULL ) {
            ++pState->iMimeTypeCount;
        }
        if ( strstr(aBuffer, "\"file_data\":\"aGVsbG8gZmlsZQo=\"") != NULL ) {
            ++pState->iFileDataCount;
        }
        if ( strstr(aBuffer, "\"file_id\":") != NULL ) {
            ++pState->iFileIdCount;
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
        (void)demo_send_all(hClient, sResponse, (size_t)iRespLen);
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
    demo_file_server_state tFileServer;
    demo_chat_server_state tChatServer;
    struct sockaddr_in tAddr;
    socklen_t iAddrLen = sizeof(tAddr);
    xthread hFileThread = NULL;
    xthread hChatThread = NULL;
    xllm_runtime *pRuntime = NULL;
    xllm *pLlm = NULL;
    xllm_response *pResponse = NULL;
    xllm_profile tProfile;
    xllm_create_options tCreate;
    xllm_turn tTurn;
    char sBaseUrl[128];
    char sFileUrl[160];
    const char *sText;
    int iStatus = 0;

#if defined(_WIN32) || defined(_WIN64)
    if ( WSAStartup(MAKEWORD(2, 2), &tWsaData) != 0 ) {
        fprintf(stderr, "winsock startup failed\n");
        return 1;
    }
#endif

    memset(&tFileServer, 0, sizeof(tFileServer));
    memset(&tChatServer, 0, sizeof(tChatServer));
    memset(&tAddr, 0, sizeof(tAddr));
    memset(&tCreate, 0, sizeof(tCreate));

    tFileServer.hListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if ( tFileServer.hListen == INVALID_SOCKET ) {
        fprintf(stderr, "create file listen socket failed\n");
        return 2;
    }
    tAddr.sin_family = AF_INET;
    tAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    tAddr.sin_port = htons(0);
    if ( bind(tFileServer.hListen, (struct sockaddr *)&tAddr, sizeof(tAddr)) == SOCKET_ERROR ) {
        fprintf(stderr, "bind file listen socket failed\n");
        return 3;
    }
    if ( listen(tFileServer.hListen, 1) == SOCKET_ERROR ) {
        fprintf(stderr, "listen file socket failed\n");
        return 4;
    }
    if ( getsockname(tFileServer.hListen, (struct sockaddr *)&tAddr, &iAddrLen) == SOCKET_ERROR ) {
        fprintf(stderr, "getsockname file socket failed\n");
        return 5;
    }
    tFileServer.uPort = ntohs(tAddr.sin_port);

    tChatServer.hListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if ( tChatServer.hListen == INVALID_SOCKET ) {
        fprintf(stderr, "create chat listen socket failed\n");
        return 6;
    }
    tAddr.sin_family = AF_INET;
    tAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    tAddr.sin_port = htons(0);
    if ( bind(tChatServer.hListen, (struct sockaddr *)&tAddr, sizeof(tAddr)) == SOCKET_ERROR ) {
        fprintf(stderr, "bind chat listen socket failed\n");
        return 7;
    }
    if ( listen(tChatServer.hListen, 1) == SOCKET_ERROR ) {
        fprintf(stderr, "listen chat socket failed\n");
        return 8;
    }
    if ( getsockname(tChatServer.hListen, (struct sockaddr *)&tAddr, &iAddrLen) == SOCKET_ERROR ) {
        fprintf(stderr, "getsockname chat socket failed\n");
        return 9;
    }
    tChatServer.uPort = ntohs(tAddr.sin_port);

    hFileThread = xrtThreadCreate((ptr)demo_file_server_thread, &tFileServer, 0);
    if ( !hFileThread ) {
        fprintf(stderr, "create file server thread failed\n");
        return 10;
    }
    hChatThread = xrtThreadCreate((ptr)demo_chat_server_thread, &tChatServer, 0);
    if ( !hChatThread ) {
        fprintf(stderr, "create chat server thread failed\n");
        return 11;
    }

    if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || !pRuntime ) {
        fprintf(stderr, "create runtime failed\n");
        return 12;
    }
    if ( xllm_register_openai_compat_adapter(pRuntime) != XRT_NET_OK ) {
        fprintf(stderr, "register adapter failed\n");
        return 13;
    }

    (void)snprintf(sBaseUrl, sizeof(sBaseUrl), "http://127.0.0.1:%u/v1", (unsigned)tChatServer.uPort);
    (void)snprintf(sFileUrl, sizeof(sFileUrl), "http://127.0.0.1:%u/doc.txt", (unsigned)tFileServer.uPort);

    xllm_profile_init(&tProfile);
    tProfile.sId = "openai-file-url";
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
        return 14;
    }

    tCreate.sInitialProfileId = "openai-file-url";
    tCreate.sSystemPrompt = "you are a url file mock";
    pLlm = xllm_create(pRuntime, &tCreate);
    if ( !pLlm ) {
        fprintf(stderr, "create llm failed\n");
        return 15;
    }

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "summarize this url file") != XRT_NET_OK ) {
        fprintf(stderr, "turn add text failed\n");
        return 16;
    }
    if ( xllm_turn_add_file_url(&tTurn, sFileUrl, "text/plain") != XRT_NET_OK ) {
        fprintf(stderr, "turn add file url failed\n");
        return 17;
    }

    iStatus = xllm_send(pLlm, &tTurn, NULL, &pResponse);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "send failed: %d\n", iStatus);
        return 18;
    }

    xrtThreadWait(hFileThread);
    xrtThreadDestroy(hFileThread);
    hFileThread = NULL;
    xrtThreadWait(hChatThread);
    xrtThreadDestroy(hChatThread);
    hChatThread = NULL;

    sText = xllm_response_get_text(pResponse);
    if ( !sText || strcmp(sText, "url file ok") != 0 ) {
        fprintf(stderr, "unexpected response text: %s\n", sText ? sText : "(null)");
        return 19;
    }
    if ( tFileServer.iAcceptCount != 1 || tFileServer.iFilePathCount != 1 ) {
        fprintf(stderr, "unexpected file server counters: accept=%d path=%d\n", tFileServer.iAcceptCount, tFileServer.iFilePathCount);
        return 20;
    }
    if ( tChatServer.iRequestCount != 1 ||
         tChatServer.iContentArrayCount != 1 ||
         tChatServer.iFilePartCount != 1 ||
         tChatServer.iFilenameCount != 1 ||
         tChatServer.iMimeTypeCount != 1 ||
         tChatServer.iFileDataCount != 1 ||
         tChatServer.iFileIdCount != 0 ) {
        fprintf(
            stderr,
            "unexpected chat counters: req=%d content=%d file_part=%d filename=%d mime=%d file_data=%d file_id=%d\n",
            tChatServer.iRequestCount,
            tChatServer.iContentArrayCount,
            tChatServer.iFilePartCount,
            tChatServer.iFilenameCount,
            tChatServer.iMimeTypeCount,
            tChatServer.iFileDataCount,
            tChatServer.iFileIdCount
        );
        return 21;
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
