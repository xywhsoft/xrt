#include <stdio.h>
#include <stdlib.h>
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

#define main smoke_real_provider_probe_entry
#include "real_provider_probe.c"
#undef main

typedef struct {
    SOCKET hListen;
    uint16 uPort;
    int iAcceptCount;
    int iFilePathCount;
} smoke_file_server_state;

typedef struct {
    SOCKET hListen;
    uint16 uPort;
    int iAcceptCount;
    int iAuthHeaderCount;
    int iFilePartCount;
    int iFilenameCount;
    int iMimeTypeCount;
    int iFileDataCount;
    int iFileIdCount;
} smoke_chat_server_state;

static bool smoke_send_all(SOCKET hSocket, const char *sData, size_t iLen)
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

static bool smoke_read_http_message(SOCKET hSocket, char *sBuffer, size_t iCapacity)
{
    size_t iUsed = 0u;
    size_t iRequired = 0u;
    bool bHeaderParsed = false;

    if ( sBuffer == NULL || iCapacity < 8u ) {
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

static SOCKET smoke_create_listen_socket(uint16 *puPort)
{
    SOCKET hListen = INVALID_SOCKET;
    struct sockaddr_in tAddr;
    socklen_t iAddrLen = sizeof(tAddr);

    hListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if ( hListen == INVALID_SOCKET ) {
        return INVALID_SOCKET;
    }

    memset(&tAddr, 0, sizeof(tAddr));
    tAddr.sin_family = AF_INET;
    tAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    tAddr.sin_port = htons(0);
    if ( bind(hListen, (struct sockaddr *)&tAddr, sizeof(tAddr)) == SOCKET_ERROR ) {
        closesocket(hListen);
        return INVALID_SOCKET;
    }
    if ( listen(hListen, 1) == SOCKET_ERROR ) {
        closesocket(hListen);
        return INVALID_SOCKET;
    }
    if ( getsockname(hListen, (struct sockaddr *)&tAddr, &iAddrLen) == SOCKET_ERROR ) {
        closesocket(hListen);
        return INVALID_SOCKET;
    }

    if ( puPort != NULL ) {
        *puPort = ntohs(tAddr.sin_port);
    }
    return hListen;
}

static uint32 smoke_file_server_thread(ptr pParam)
{
    static const char sFileBody[] = "hello file\n";
    smoke_file_server_state *pState = (smoke_file_server_state *)pParam;
    SOCKET hClient = INVALID_SOCKET;
    char sRequest[4096];
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
    if ( smoke_read_http_message(hClient, sRequest, sizeof(sRequest)) ) {
        if ( strstr(sRequest, "GET /doc.txt HTTP/1.1\r\n") != NULL ) {
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
        (void)smoke_send_all(hClient, sResponse, (size_t)iRespLen);
        (void)smoke_send_all(hClient, sFileBody, sizeof(sFileBody) - 1u);
    }

    closesocket(hClient);
    closesocket(pState->hListen);
    pState->hListen = INVALID_SOCKET;
    return 0u;
}

static uint32 smoke_chat_server_thread(ptr pParam)
{
    smoke_chat_server_state *pState = (smoke_chat_server_state *)pParam;
    SOCKET hClient = INVALID_SOCKET;
    char sRequest[16384];
    const char *sBody =
        "{\"id\":\"chatcmpl-probe-file-url\","
        "\"object\":\"chat.completion\","
        "\"model\":\"gpt-test-mm\","
        "\"choices\":[{\"index\":0,\"message\":{\"role\":\"assistant\",\"content\":\"probe openai file url ok\"},\"finish_reason\":\"stop\"}],"
        "\"usage\":{\"prompt_tokens\":1,\"completion_tokens\":2,\"total_tokens\":3}}";
    char sResponse[12288];
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
    if ( smoke_read_http_message(hClient, sRequest, sizeof(sRequest)) ) {
        if ( strstr(sRequest, "Authorization: Bearer test-key\r\n") != NULL ) {
            ++pState->iAuthHeaderCount;
        }
        if ( strstr(sRequest, "\"type\":\"file\",\"file\":{") != NULL ) {
            ++pState->iFilePartCount;
        }
        if ( strstr(sRequest, "\"filename\":\"probe-url.txt\"") != NULL ) {
            ++pState->iFilenameCount;
        }
        if ( strstr(sRequest, "\"mime_type\":\"text/plain\"") != NULL ) {
            ++pState->iMimeTypeCount;
        }
        if ( strstr(sRequest, "\"file_data\":\"aGVsbG8gZmlsZQo=\"") != NULL ) {
            ++pState->iFileDataCount;
        }
        if ( strstr(sRequest, "\"file_id\":") != NULL ) {
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
        (void)smoke_send_all(hClient, sResponse, (size_t)iRespLen);
    }

    closesocket(hClient);
    closesocket(pState->hListen);
    pState->hListen = INVALID_SOCKET;
    return 0u;
}

static int smoke_setenv_cstr(const char *sName, const char *sValue)
{
#if defined(_WIN32) || defined(_WIN64)
    return _putenv_s(sName, sValue ? sValue : "");
#else
    if ( sValue == NULL || sValue[0] == '\0' ) {
        return unsetenv(sName);
    }
    return setenv(sName, sValue, 1);
#endif
}

int main(void)
{
#if defined(_WIN32) || defined(_WIN64)
    WSADATA tWsaData;
#endif
    smoke_file_server_state tFileServer;
    smoke_chat_server_state tChatServer;
    xthread hFileThread = NULL;
    xthread hChatThread = NULL;
    char sBaseUrl[256];
    char sFileUrl[256];
    int iProbeExitCode = 0;
    int iExitCode = 0;

#if defined(_WIN32) || defined(_WIN64)
    if ( WSAStartup(MAKEWORD(2, 2), &tWsaData) != 0 ) {
        fprintf(stderr, "winsock startup failed\n");
        return 1;
    }
#endif

    memset(&tFileServer, 0, sizeof(tFileServer));
    memset(&tChatServer, 0, sizeof(tChatServer));

    tFileServer.hListen = smoke_create_listen_socket(&tFileServer.uPort);
    if ( tFileServer.hListen == INVALID_SOCKET ) {
        fprintf(stderr, "create file listen socket failed\n");
        iExitCode = 2;
        goto cleanup;
    }
    tChatServer.hListen = smoke_create_listen_socket(&tChatServer.uPort);
    if ( tChatServer.hListen == INVALID_SOCKET ) {
        fprintf(stderr, "create chat listen socket failed\n");
        iExitCode = 3;
        goto cleanup;
    }

    hFileThread = xrtThreadCreate((ptr)smoke_file_server_thread, &tFileServer, 0);
    if ( !hFileThread ) {
        fprintf(stderr, "create file server thread failed\n");
        iExitCode = 4;
        goto cleanup;
    }
    hChatThread = xrtThreadCreate((ptr)smoke_chat_server_thread, &tChatServer, 0);
    if ( !hChatThread ) {
        fprintf(stderr, "create chat server thread failed\n");
        iExitCode = 5;
        goto cleanup;
    }

    snprintf(sBaseUrl, sizeof(sBaseUrl), "http://127.0.0.1:%u/v1", (unsigned)tChatServer.uPort);
    snprintf(sFileUrl, sizeof(sFileUrl), "http://127.0.0.1:%u/doc.txt", (unsigned)tFileServer.uPort);

    if ( smoke_setenv_cstr("XLLM_REAL_ADAPTER", "openai_compat") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_BASE_URL", sBaseUrl) != 0 ||
         smoke_setenv_cstr("XLLM_REAL_MODEL", "gpt-test") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_MULTIMODAL_MODEL", "gpt-test-mm") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_API_KEY", "test-key") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_AUTH_KIND", "bearer") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_ENABLE_LOG", "0") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_ENABLE_TRACE", "0") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_EXPECT_TEXT_CONTAINS", "probe openai file url ok") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_FILE_URL", sFileUrl) != 0 ||
         smoke_setenv_cstr("XLLM_REAL_FILE_MIME", "text/plain") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_FILE_NAME", "probe-url.txt") != 0 ) {
        fprintf(stderr, "set environment variables failed\n");
        iExitCode = 6;
        goto cleanup;
    }

    iProbeExitCode = smoke_real_provider_probe_entry();
    if ( iProbeExitCode != 0 ) {
        fprintf(stderr, "probe failed unexpectedly: %d\n", iProbeExitCode);
        iExitCode = 7;
        goto cleanup;
    }

    if ( tFileServer.hListen != INVALID_SOCKET ) {
        closesocket(tFileServer.hListen);
        tFileServer.hListen = INVALID_SOCKET;
    }
    if ( tChatServer.hListen != INVALID_SOCKET ) {
        closesocket(tChatServer.hListen);
        tChatServer.hListen = INVALID_SOCKET;
    }
    xrtThreadWait(hFileThread);
    xrtThreadDestroy(hFileThread);
    hFileThread = NULL;
    xrtThreadWait(hChatThread);
    xrtThreadDestroy(hChatThread);
    hChatThread = NULL;

    if ( tFileServer.iAcceptCount != 1 ||
         tFileServer.iFilePathCount != 1 ||
         tChatServer.iAcceptCount != 1 ||
         tChatServer.iAuthHeaderCount != 1 ||
         tChatServer.iFilePartCount != 1 ||
         tChatServer.iFilenameCount != 1 ||
         tChatServer.iMimeTypeCount != 1 ||
         tChatServer.iFileDataCount != 1 ||
         tChatServer.iFileIdCount != 0 ) {
        fprintf(
            stderr,
            "unexpected counters: file_accept=%d file_path=%d chat_accept=%d auth=%d file_part=%d filename=%d mime=%d file_data=%d file_id=%d\n",
            tFileServer.iAcceptCount,
            tFileServer.iFilePathCount,
            tChatServer.iAcceptCount,
            tChatServer.iAuthHeaderCount,
            tChatServer.iFilePartCount,
            tChatServer.iFilenameCount,
            tChatServer.iMimeTypeCount,
            tChatServer.iFileDataCount,
            tChatServer.iFileIdCount
        );
        iExitCode = 8;
        goto cleanup;
    }

    puts("smoke_real_provider_probe_openai_file_url ok");

cleanup:
    smoke_setenv_cstr("XLLM_REAL_ADAPTER", "");
    smoke_setenv_cstr("XLLM_REAL_BASE_URL", "");
    smoke_setenv_cstr("XLLM_REAL_MODEL", "");
    smoke_setenv_cstr("XLLM_REAL_MULTIMODAL_MODEL", "");
    smoke_setenv_cstr("XLLM_REAL_API_KEY", "");
    smoke_setenv_cstr("XLLM_REAL_AUTH_KIND", "");
    smoke_setenv_cstr("XLLM_REAL_ENABLE_LOG", "");
    smoke_setenv_cstr("XLLM_REAL_ENABLE_TRACE", "");
    smoke_setenv_cstr("XLLM_REAL_EXPECT_TEXT_CONTAINS", "");
    smoke_setenv_cstr("XLLM_REAL_FILE_URL", "");
    smoke_setenv_cstr("XLLM_REAL_FILE_MIME", "");
    smoke_setenv_cstr("XLLM_REAL_FILE_NAME", "");

    if ( hFileThread ) {
        if ( tFileServer.hListen != INVALID_SOCKET ) {
            closesocket(tFileServer.hListen);
            tFileServer.hListen = INVALID_SOCKET;
        }
        xrtThreadWait(hFileThread);
        xrtThreadDestroy(hFileThread);
    } else if ( tFileServer.hListen != INVALID_SOCKET ) {
        closesocket(tFileServer.hListen);
    }
    if ( hChatThread ) {
        if ( tChatServer.hListen != INVALID_SOCKET ) {
            closesocket(tChatServer.hListen);
            tChatServer.hListen = INVALID_SOCKET;
        }
        xrtThreadWait(hChatThread);
        xrtThreadDestroy(hChatThread);
    } else if ( tChatServer.hListen != INVALID_SOCKET ) {
        closesocket(tChatServer.hListen);
    }

#if defined(_WIN32) || defined(_WIN64)
    WSACleanup();
#endif
    return iExitCode;
}
