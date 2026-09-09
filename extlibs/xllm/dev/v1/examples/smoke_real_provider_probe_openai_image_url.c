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
    int iImageUrlPartCount;
    int iImageUrlValueCount;
    int iTextValueCount;
} smoke_server_state;

static uint32 smoke_server_thread(ptr pParam)
{
    smoke_server_state *pState = (smoke_server_state *)pParam;
    SOCKET hClient = INVALID_SOCKET;
    char aBuffer[8192];
    const char *sBody =
        "{\"id\":\"chatcmpl-openai-probe-image-url\","
        "\"object\":\"chat.completion\","
        "\"model\":\"gpt-test-mm\","
        "\"choices\":[{\"index\":0,\"message\":{\"role\":\"assistant\",\"content\":\"probe openai image url ok\"},\"finish_reason\":\"stop\"}],"
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
        if ( strstr(aBuffer, "\"type\":\"image_url\",\"image_url\":{\"url\":\"https://example.invalid/cat.png\"}}") != NULL ) {
            ++pState->iImageUrlPartCount;
        }
        if ( strstr(aBuffer, "https://example.invalid/cat.png") != NULL ) {
            ++pState->iImageUrlValueCount;
        }
        if ( strstr(aBuffer, "\"describe this probe image\"") != NULL ) {
            ++pState->iTextValueCount;
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
    smoke_server_state tServer;
    struct sockaddr_in tAddr;
    socklen_t iAddrLen = sizeof(tAddr);
    xthread hServerThread = NULL;
    char sBaseUrl[256];
    int iProbeExitCode = 0;
    int iExitCode = 0;

#if defined(_WIN32) || defined(_WIN64)
    if ( WSAStartup(MAKEWORD(2, 2), &tWsaData) != 0 ) {
        fprintf(stderr, "winsock startup failed\n");
        return 1;
    }
#endif

    memset(&tServer, 0, sizeof(tServer));
    memset(&tAddr, 0, sizeof(tAddr));
    tServer.hListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if ( tServer.hListen == INVALID_SOCKET ) {
        fprintf(stderr, "create listen socket failed\n");
        iExitCode = 2;
        goto cleanup;
    }

    tAddr.sin_family = AF_INET;
    tAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    tAddr.sin_port = htons(0);
    if ( bind(tServer.hListen, (struct sockaddr *)&tAddr, sizeof(tAddr)) == SOCKET_ERROR ) {
        fprintf(stderr, "bind listen socket failed\n");
        iExitCode = 3;
        goto cleanup;
    }
    if ( listen(tServer.hListen, 1) == SOCKET_ERROR ) {
        fprintf(stderr, "listen failed\n");
        iExitCode = 4;
        goto cleanup;
    }
    if ( getsockname(tServer.hListen, (struct sockaddr *)&tAddr, &iAddrLen) == SOCKET_ERROR ) {
        fprintf(stderr, "getsockname failed\n");
        iExitCode = 5;
        goto cleanup;
    }
    tServer.uPort = ntohs(tAddr.sin_port);

    hServerThread = xrtThreadCreate((ptr)smoke_server_thread, &tServer, 0);
    if ( !hServerThread ) {
        fprintf(stderr, "create server thread failed\n");
        iExitCode = 6;
        goto cleanup;
    }

    snprintf(sBaseUrl, sizeof(sBaseUrl), "http://127.0.0.1:%u/v1", (unsigned)tServer.uPort);
    if ( smoke_setenv_cstr("XLLM_REAL_ADAPTER", "openai_compat") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_BASE_URL", sBaseUrl) != 0 ||
         smoke_setenv_cstr("XLLM_REAL_MODEL", "gpt-test") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_MULTIMODAL_MODEL", "gpt-test-mm") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_AUTH_KIND", "none") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_ENABLE_LOG", "0") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_ENABLE_TRACE", "0") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_EXPECT_TEXT_CONTAINS", "probe openai image url ok") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_IMAGE_URL", "https://example.invalid/cat.png") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_IMAGE_MIME", "image/png") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_PROMPT", "describe this probe image") != 0 ) {
        fprintf(stderr, "set environment variables failed\n");
        iExitCode = 7;
        goto cleanup;
    }

    iProbeExitCode = smoke_real_provider_probe_entry();
    if ( iProbeExitCode != 0 ) {
        fprintf(stderr, "probe failed unexpectedly: %d\n", iProbeExitCode);
        iExitCode = 8;
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
         tServer.iImageUrlPartCount != 1 ||
         tServer.iImageUrlValueCount != 1 ||
         tServer.iTextValueCount < 1 ) {
        fprintf(
            stderr,
            "unexpected openai image-url probe counters: accept=%d image_part=%d image_value=%d text=%d\n",
            tServer.iAcceptCount,
            tServer.iImageUrlPartCount,
            tServer.iImageUrlValueCount,
            tServer.iTextValueCount
        );
        iExitCode = 9;
        goto cleanup;
    }

    printf("smoke_real_provider_probe_openai_image_url ok\n");

cleanup:
    smoke_setenv_cstr("XLLM_REAL_ADAPTER", "");
    smoke_setenv_cstr("XLLM_REAL_BASE_URL", "");
    smoke_setenv_cstr("XLLM_REAL_MODEL", "");
    smoke_setenv_cstr("XLLM_REAL_MULTIMODAL_MODEL", "");
    smoke_setenv_cstr("XLLM_REAL_AUTH_KIND", "");
    smoke_setenv_cstr("XLLM_REAL_ENABLE_LOG", "");
    smoke_setenv_cstr("XLLM_REAL_ENABLE_TRACE", "");
    smoke_setenv_cstr("XLLM_REAL_EXPECT_TEXT_CONTAINS", "");
    smoke_setenv_cstr("XLLM_REAL_IMAGE_URL", "");
    smoke_setenv_cstr("XLLM_REAL_IMAGE_MIME", "");
    smoke_setenv_cstr("XLLM_REAL_PROMPT", "");

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

#if defined(_WIN32) || defined(_WIN64)
    WSACleanup();
#endif
    return iExitCode;
}
