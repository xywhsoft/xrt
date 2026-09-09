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
    int iImageUrlCount;
    int iDataUrlCount;
    int iTextValueCount;
} smoke_azure_openai_inline_image_server_state;

static uint32 smoke_azure_openai_inline_image_server_thread(ptr pParam)
{
    smoke_azure_openai_inline_image_server_state *pState = (smoke_azure_openai_inline_image_server_state *)pParam;
    SOCKET hClient = INVALID_SOCKET;
    char aBuffer[8192];
    const char *sBody =
        "{\"id\":\"chatcmpl-azure-probe-inline-image\","
        "\"object\":\"chat.completion\","
        "\"model\":\"Infrastructure-gpt54-azure-mm\","
        "\"choices\":[{\"index\":0,\"message\":{\"role\":\"assistant\",\"content\":\"probe azure openai inline image ok\"},\"finish_reason\":\"stop\"}],"
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
        if ( strstr(aBuffer, "\"type\":\"image_url\",\"image_url\":{\"url\":\"data:image/png;base64,") != NULL ) {
            ++pState->iImageUrlCount;
            ++pState->iDataUrlCount;
        }
        if ( strstr(aBuffer, "\"describe this azure inline probe image\"") != NULL ) {
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

static int smoke_azure_openai_inline_image_setenv_cstr(const char *sName, const char *sValue)
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
    smoke_azure_openai_inline_image_server_state tServer;
    struct sockaddr_in tAddr;
    socklen_t iAddrLen = sizeof(tAddr);
    xthread hServerThread = NULL;
    char sBaseUrl[512];
    const char *sImagePath = "build/azure_probe_inline_image.png";
    FILE *pImage = NULL;
    static const unsigned char aPngBytes[] = {
        0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A,
        0x00,0x00,0x00,0x0D,0x49,0x48,0x44,0x52,
        0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,
        0x08,0x04,0x00,0x00,0x00,0xB5,0x1C,0x0C,
        0x02,0x00,0x00,0x00,0x0B,0x49,0x44,0x41,
        0x54,0x78,0xDA,0x63,0xFC,0xFF,0x1F,0x00,
        0x03,0x03,0x01,0xFF,0xA5,0xF7,0xD1,0x5D,
        0x00,0x00,0x00,0x00,0x49,0x45,0x4E,0x44,
        0xAE,0x42,0x60,0x82
    };
    int iProbeExitCode = 0;
    int iExitCode = 0;

#if defined(_WIN32) || defined(_WIN64)
    if ( WSAStartup(MAKEWORD(2, 2), &tWsaData) != 0 ) {
        fprintf(stderr, "winsock startup failed\n");
        return 1;
    }
#endif

    pImage = fopen(sImagePath, "wb");
    if ( !pImage ) {
        fprintf(stderr, "create inline image failed\n");
        return 2;
    }
    if ( fwrite(aPngBytes, 1u, sizeof(aPngBytes), pImage) != sizeof(aPngBytes) ) {
        fclose(pImage);
        fprintf(stderr, "write inline image failed\n");
        return 3;
    }
    fclose(pImage);
    pImage = NULL;

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

    hServerThread = xrtThreadCreate((ptr)smoke_azure_openai_inline_image_server_thread, &tServer, 0);
    if ( !hServerThread ) {
        fprintf(stderr, "create server thread failed\n");
        iExitCode = 8;
        goto cleanup;
    }

    snprintf(sBaseUrl, sizeof(sBaseUrl), "http://127.0.0.1:%u/v1", (unsigned)tServer.uPort);
    if ( smoke_azure_openai_inline_image_setenv_cstr("XLLM_REAL_ADAPTER", "openai_compat") != 0 ||
         smoke_azure_openai_inline_image_setenv_cstr("AZURE_OPENAI_BASE_URL", sBaseUrl) != 0 ||
         smoke_azure_openai_inline_image_setenv_cstr("AZURE_OPENAI_DEPLOYMENT", "azure-gpt-test") != 0 ||
         smoke_azure_openai_inline_image_setenv_cstr("AZURE_OPENAI_MULTIMODAL_MODEL", "Infrastructure-gpt54-azure-mm") != 0 ||
         smoke_azure_openai_inline_image_setenv_cstr("AZURE_OPENAI_API_KEY", "alias-azure-openai-key") != 0 ||
         smoke_azure_openai_inline_image_setenv_cstr("XLLM_REAL_ENABLE_LOG", "0") != 0 ||
         smoke_azure_openai_inline_image_setenv_cstr("XLLM_REAL_ENABLE_TRACE", "0") != 0 ||
         smoke_azure_openai_inline_image_setenv_cstr("XLLM_REAL_EXPECT_TEXT_CONTAINS", "probe azure openai inline image ok") != 0 ||
         smoke_azure_openai_inline_image_setenv_cstr("AZURE_OPENAI_IMAGE_PATH", sImagePath) != 0 ||
         smoke_azure_openai_inline_image_setenv_cstr("AZURE_OPENAI_IMAGE_MIME", "image/png") != 0 ||
         smoke_azure_openai_inline_image_setenv_cstr("XLLM_REAL_PROMPT", "describe this azure inline probe image") != 0 ) {
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
         tServer.iImageUrlCount != 1 ||
         tServer.iDataUrlCount != 1 ||
         tServer.iTextValueCount < 1 ) {
        fprintf(
            stderr,
            "unexpected azure inline image probe counters: accept=%d image=%d data_url=%d text=%d\n",
            tServer.iAcceptCount,
            tServer.iImageUrlCount,
            tServer.iDataUrlCount,
            tServer.iTextValueCount
        );
        iExitCode = 11;
        goto cleanup;
    }

    printf("smoke_real_provider_probe_azure_openai_image_inline_bytes ok\n");

cleanup:
    smoke_azure_openai_inline_image_setenv_cstr("XLLM_REAL_ADAPTER", "");
    smoke_azure_openai_inline_image_setenv_cstr("AZURE_OPENAI_BASE_URL", "");
    smoke_azure_openai_inline_image_setenv_cstr("AZURE_OPENAI_DEPLOYMENT", "");
    smoke_azure_openai_inline_image_setenv_cstr("AZURE_OPENAI_MULTIMODAL_MODEL", "");
    smoke_azure_openai_inline_image_setenv_cstr("AZURE_OPENAI_API_KEY", "");
    smoke_azure_openai_inline_image_setenv_cstr("XLLM_REAL_ENABLE_LOG", "");
    smoke_azure_openai_inline_image_setenv_cstr("XLLM_REAL_ENABLE_TRACE", "");
    smoke_azure_openai_inline_image_setenv_cstr("XLLM_REAL_EXPECT_TEXT_CONTAINS", "");
    smoke_azure_openai_inline_image_setenv_cstr("AZURE_OPENAI_IMAGE_PATH", "");
    smoke_azure_openai_inline_image_setenv_cstr("AZURE_OPENAI_IMAGE_MIME", "");
    smoke_azure_openai_inline_image_setenv_cstr("XLLM_REAL_PROMPT", "");

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

    remove(sImagePath);

#if defined(_WIN32) || defined(_WIN64)
    WSACleanup();
#endif
    return iExitCode;
}
