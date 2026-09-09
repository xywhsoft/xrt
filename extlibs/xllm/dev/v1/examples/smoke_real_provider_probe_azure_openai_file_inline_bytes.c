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
    int iFilePartCount;
    int iFilenameCount;
    int iMimeTypeCount;
    int iFileDataCount;
} smoke_azure_openai_inline_file_server_state;

static uint32 smoke_azure_openai_inline_file_server_thread(ptr pParam)
{
    smoke_azure_openai_inline_file_server_state *pState = (smoke_azure_openai_inline_file_server_state *)pParam;
    SOCKET hClient = INVALID_SOCKET;
    char aBuffer[8192];
    const char *sBody =
        "{\"id\":\"chatcmpl-azure-probe-inline-file\","
        "\"object\":\"chat.completion\","
        "\"model\":\"Infrastructure-gpt54-azure-mm\","
        "\"choices\":[{\"index\":0,\"message\":{\"role\":\"assistant\",\"content\":\"probe azure openai inline file ok\"},\"finish_reason\":\"stop\"}],"
        "\"usage\":{\"prompt_tokens\":1,\"completion_tokens\":1,\"total_tokens\":2}}";
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

    ++pState->iAcceptCount;
    iRecv = recv(hClient, aBuffer, (int)(sizeof(aBuffer) - 1u), 0);
    if ( iRecv > 0 ) {
        aBuffer[iRecv] = '\0';
        if ( strstr(aBuffer, "\"type\":\"file\",\"file\":{") != NULL ) {
            ++pState->iFilePartCount;
        }
        if ( strstr(aBuffer, "\"filename\":\"azure-probe-inline.txt\"") != NULL ) {
            ++pState->iFilenameCount;
        }
        if ( strstr(aBuffer, "\"mime_type\":\"text/plain\"") != NULL ) {
            ++pState->iMimeTypeCount;
        }
        if ( strstr(aBuffer, "\"file_data\":\"aGVsbG8gYXp1cmUgcHJvYmUgZmlsZQo=\"") != NULL ) {
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

static int smoke_azure_openai_inline_file_setenv_cstr(const char *sName, const char *sValue)
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
    smoke_azure_openai_inline_file_server_state tServer;
    struct sockaddr_in tAddr;
    socklen_t iAddrLen = sizeof(tAddr);
    xthread hServerThread = NULL;
    char sBaseUrl[512];
    const char *sUploadPath = "build/azure_probe_inline_file.txt";
    FILE *pUpload = NULL;
    int iProbeExitCode = 0;
    int iExitCode = 0;

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
    if ( fwrite("hello azure probe file\n", 1u, 23u, pUpload) != 23u ) {
        fclose(pUpload);
        fprintf(stderr, "write upload file failed\n");
        return 3;
    }
    fclose(pUpload);
    pUpload = NULL;

    memset(&tServer, 0, sizeof(tServer));
    memset(&tAddr, 0, sizeof(tAddr));
    tServer.hListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if ( tServer.hListen == INVALID_SOCKET ) {
        fprintf(stderr, "create listen socket failed\n");
        iExitCode = 4;
        goto cleanup;
    }

    tAddr.sin_family = AF_INET;
    tAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    tAddr.sin_port = htons(0);
    if ( bind(tServer.hListen, (struct sockaddr *)&tAddr, sizeof(tAddr)) == SOCKET_ERROR ) {
        fprintf(stderr, "bind listen socket failed\n");
        iExitCode = 5;
        goto cleanup;
    }
    if ( listen(tServer.hListen, 1) == SOCKET_ERROR ) {
        fprintf(stderr, "listen failed\n");
        iExitCode = 6;
        goto cleanup;
    }
    if ( getsockname(tServer.hListen, (struct sockaddr *)&tAddr, &iAddrLen) == SOCKET_ERROR ) {
        fprintf(stderr, "getsockname failed\n");
        iExitCode = 7;
        goto cleanup;
    }
    tServer.uPort = ntohs(tAddr.sin_port);

    hServerThread = xrtThreadCreate((ptr)smoke_azure_openai_inline_file_server_thread, &tServer, 0);
    if ( !hServerThread ) {
        fprintf(stderr, "create server thread failed\n");
        iExitCode = 8;
        goto cleanup;
    }

    snprintf(sBaseUrl, sizeof(sBaseUrl), "http://127.0.0.1:%u/v1", (unsigned)tServer.uPort);
    if ( smoke_azure_openai_inline_file_setenv_cstr("XLLM_REAL_ADAPTER", "openai_compat") != 0 ||
         smoke_azure_openai_inline_file_setenv_cstr("AZURE_OPENAI_BASE_URL", sBaseUrl) != 0 ||
         smoke_azure_openai_inline_file_setenv_cstr("AZURE_OPENAI_DEPLOYMENT", "azure-gpt-test") != 0 ||
         smoke_azure_openai_inline_file_setenv_cstr("AZURE_OPENAI_MULTIMODAL_MODEL", "Infrastructure-gpt54-azure-mm") != 0 ||
         smoke_azure_openai_inline_file_setenv_cstr("AZURE_OPENAI_API_KEY", "alias-azure-openai-key") != 0 ||
         smoke_azure_openai_inline_file_setenv_cstr("XLLM_REAL_ENABLE_LOG", "0") != 0 ||
         smoke_azure_openai_inline_file_setenv_cstr("XLLM_REAL_ENABLE_TRACE", "0") != 0 ||
         smoke_azure_openai_inline_file_setenv_cstr("XLLM_REAL_PROMPT", "summarize this inline azure probe file") != 0 ||
         smoke_azure_openai_inline_file_setenv_cstr("XLLM_REAL_EXPECT_TEXT_CONTAINS", "probe azure openai inline file ok") != 0 ||
         smoke_azure_openai_inline_file_setenv_cstr("AZURE_OPENAI_FILE_PATH", sUploadPath) != 0 ||
         smoke_azure_openai_inline_file_setenv_cstr("AZURE_OPENAI_FILE_NAME", "azure-probe-inline.txt") != 0 ||
         smoke_azure_openai_inline_file_setenv_cstr("AZURE_OPENAI_FILE_MIME", "text/plain") != 0 ) {
        fprintf(stderr, "set environment variables failed\n");
        iExitCode = 9;
        goto cleanup;
    }

    iProbeExitCode = smoke_real_provider_probe_entry();
    if ( iProbeExitCode != 0 ) {
        fprintf(stderr, "probe failed unexpectedly: %d\n", iProbeExitCode);
        iExitCode = 10;
        goto cleanup;
    }

    if ( tServer.hListen != INVALID_SOCKET ) {
        closesocket(tServer.hListen);
        tServer.hListen = INVALID_SOCKET;
    }
    xrtThreadWait(hServerThread);
    xrtThreadDestroy(hServerThread);
    hServerThread = NULL;

    if ( tServer.iAcceptCount != 1 ||
         tServer.iFilePartCount != 1 ||
         tServer.iFilenameCount != 1 ||
         tServer.iMimeTypeCount != 1 ||
         tServer.iFileDataCount != 1 ) {
        fprintf(
            stderr,
            "unexpected azure inline file probe counters: accept=%d file_part=%d filename=%d mime=%d file_data=%d\n",
            tServer.iAcceptCount,
            tServer.iFilePartCount,
            tServer.iFilenameCount,
            tServer.iMimeTypeCount,
            tServer.iFileDataCount
        );
        iExitCode = 11;
        goto cleanup;
    }

    printf("smoke_real_provider_probe_azure_openai_file_inline_bytes ok\n");

cleanup:
    smoke_azure_openai_inline_file_setenv_cstr("XLLM_REAL_ADAPTER", "");
    smoke_azure_openai_inline_file_setenv_cstr("AZURE_OPENAI_BASE_URL", "");
    smoke_azure_openai_inline_file_setenv_cstr("AZURE_OPENAI_DEPLOYMENT", "");
    smoke_azure_openai_inline_file_setenv_cstr("AZURE_OPENAI_MULTIMODAL_MODEL", "");
    smoke_azure_openai_inline_file_setenv_cstr("AZURE_OPENAI_API_KEY", "");
    smoke_azure_openai_inline_file_setenv_cstr("XLLM_REAL_ENABLE_LOG", "");
    smoke_azure_openai_inline_file_setenv_cstr("XLLM_REAL_ENABLE_TRACE", "");
    smoke_azure_openai_inline_file_setenv_cstr("XLLM_REAL_PROMPT", "");
    smoke_azure_openai_inline_file_setenv_cstr("XLLM_REAL_EXPECT_TEXT_CONTAINS", "");
    smoke_azure_openai_inline_file_setenv_cstr("AZURE_OPENAI_FILE_PATH", "");
    smoke_azure_openai_inline_file_setenv_cstr("AZURE_OPENAI_FILE_NAME", "");
    smoke_azure_openai_inline_file_setenv_cstr("AZURE_OPENAI_FILE_MIME", "");

    if ( hServerThread ) {
        if ( tServer.hListen != INVALID_SOCKET ) {
            closesocket(tServer.hListen);
            tServer.hListen = INVALID_SOCKET;
        }
        xrtThreadWait(hServerThread);
        xrtThreadDestroy(hServerThread);
        hServerThread = NULL;
    } else if ( tServer.hListen != INVALID_SOCKET ) {
        closesocket(tServer.hListen);
        tServer.hListen = INVALID_SOCKET;
    }

    remove(sUploadPath);

#if defined(_WIN32) || defined(_WIN64)
    WSACleanup();
#endif
    return iExitCode;
}
