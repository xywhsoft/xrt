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
    int iAuthCount;
    int iPathCount;
    int iImagePartCount;
    int iImageUriValueCount;
    int iTextValueCount;
} smoke_vertex_gemini_image_fileid_server_state;

static bool smoke_vertex_gemini_image_fileid_send_all(SOCKET hSocket, const char *sData, size_t iLen)
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

static uint32 smoke_vertex_gemini_image_fileid_server_thread(ptr pParam)
{
    smoke_vertex_gemini_image_fileid_server_state *pState = (smoke_vertex_gemini_image_fileid_server_state *)pParam;
    SOCKET hClient = INVALID_SOCKET;
    char aBuffer[16384];
    const char *sBody =
        "{\"candidates\":[{\"finishReason\":\"STOP\",\"content\":{\"role\":\"model\",\"parts\":[{\"text\":\"probe vertex gemini image file_id alias ok\"}]}}],"
        "\"usageMetadata\":{\"promptTokenCount\":4,\"candidatesTokenCount\":3,\"totalTokenCount\":7}}";
    char sResponse[24576];
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
        if ( strstr(aBuffer, "x-goog-api-key: alias-vertex-gemini-image-fileid-key\r\n") != NULL ) {
            ++pState->iAuthCount;
        }
        if ( strstr(aBuffer, "POST /v1/projects/demo/locations/us-central1/publishers/google/models/vertex-gemini-test-image-fileid:generateContent HTTP/1.1\r\n") != NULL ) {
            ++pState->iPathCount;
        }
        if ( strstr(aBuffer, "\"fileData\":{\"mimeType\":\"image/png\",\"fileUri\":\"vertex-gemini-image-file-alias-123\"}") != NULL ) {
            ++pState->iImagePartCount;
        }
        if ( strstr(aBuffer, "vertex-gemini-image-file-alias-123") != NULL ) {
            ++pState->iImageUriValueCount;
        }
        if ( strstr(aBuffer, "\"text\":\"describe this vertex gemini alias image file id\"") != NULL ) {
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

    if ( iRespLen > 0 && !smoke_vertex_gemini_image_fileid_send_all(hClient, sResponse, (size_t)iRespLen) ) {
        closesocket(hClient);
        closesocket(pState->hListen);
        pState->hListen = INVALID_SOCKET;
        return 3u;
    }

    closesocket(hClient);
    closesocket(pState->hListen);
    pState->hListen = INVALID_SOCKET;
    return 0u;
}

static int smoke_vertex_gemini_image_fileid_setenv_cstr(const char *sName, const char *sValue)
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
    smoke_vertex_gemini_image_fileid_server_state tServer;
    struct sockaddr_in tAddr;
    socklen_t iAddrLen = sizeof(tAddr);
    xthread hServerThread = NULL;
    char sBaseUrl[512];
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

    hServerThread = xrtThreadCreate((ptr)smoke_vertex_gemini_image_fileid_server_thread, &tServer, 0);
    if ( !hServerThread ) {
        fprintf(stderr, "create server thread failed\n");
        iExitCode = 6;
        goto cleanup;
    }

    snprintf(
        sBaseUrl,
        sizeof(sBaseUrl),
        "http://127.0.0.1:%u/v1/projects/demo/locations/us-central1/publishers/google/models",
        (unsigned)tServer.uPort
    );
    if ( smoke_vertex_gemini_image_fileid_setenv_cstr("XLLM_REAL_ADAPTER", "vertex_gemini_native") != 0 ||
         smoke_vertex_gemini_image_fileid_setenv_cstr("VERTEX_GEMINI_BASE_URL", sBaseUrl) != 0 ||
         smoke_vertex_gemini_image_fileid_setenv_cstr("VERTEX_GEMINI_MODEL", "vertex-gemini-test-image-fileid") != 0 ||
         smoke_vertex_gemini_image_fileid_setenv_cstr("VERTEX_GEMINI_API_KEY", "alias-vertex-gemini-image-fileid-key") != 0 ||
         smoke_vertex_gemini_image_fileid_setenv_cstr("VERTEX_GEMINI_IMAGE_FILE_ID", "vertex-gemini-image-file-alias-123") != 0 ||
         smoke_vertex_gemini_image_fileid_setenv_cstr("VERTEX_GEMINI_IMAGE_MIME", "image/png") != 0 ||
         smoke_vertex_gemini_image_fileid_setenv_cstr("XLLM_REAL_ENABLE_LOG", "0") != 0 ||
         smoke_vertex_gemini_image_fileid_setenv_cstr("XLLM_REAL_ENABLE_TRACE", "0") != 0 ||
         smoke_vertex_gemini_image_fileid_setenv_cstr("XLLM_REAL_PROMPT", "describe this vertex gemini alias image file id") != 0 ||
         smoke_vertex_gemini_image_fileid_setenv_cstr("XLLM_REAL_EXPECT_TEXT_CONTAINS", "probe vertex gemini image file_id alias ok") != 0 ) {
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
         tServer.iAuthCount != 1 ||
         tServer.iPathCount != 1 ||
         tServer.iImagePartCount != 1 ||
         tServer.iImageUriValueCount != 1 ||
         tServer.iTextValueCount < 1 ) {
        fprintf(
            stderr,
            "unexpected vertex gemini image file_id alias counters: accept=%d auth=%d path=%d image_part=%d image_uri=%d text=%d\n",
            tServer.iAcceptCount,
            tServer.iAuthCount,
            tServer.iPathCount,
            tServer.iImagePartCount,
            tServer.iImageUriValueCount,
            tServer.iTextValueCount
        );
        iExitCode = 9;
        goto cleanup;
    }

    printf("smoke_real_provider_probe_vertex_gemini_image_file_id_alias_envs ok\n");

cleanup:
    smoke_vertex_gemini_image_fileid_setenv_cstr("XLLM_REAL_ADAPTER", "");
    smoke_vertex_gemini_image_fileid_setenv_cstr("VERTEX_GEMINI_BASE_URL", "");
    smoke_vertex_gemini_image_fileid_setenv_cstr("VERTEX_GEMINI_MODEL", "");
    smoke_vertex_gemini_image_fileid_setenv_cstr("VERTEX_GEMINI_API_KEY", "");
    smoke_vertex_gemini_image_fileid_setenv_cstr("VERTEX_GEMINI_IMAGE_FILE_ID", "");
    smoke_vertex_gemini_image_fileid_setenv_cstr("VERTEX_GEMINI_IMAGE_MIME", "");
    smoke_vertex_gemini_image_fileid_setenv_cstr("XLLM_REAL_ENABLE_LOG", "");
    smoke_vertex_gemini_image_fileid_setenv_cstr("XLLM_REAL_ENABLE_TRACE", "");
    smoke_vertex_gemini_image_fileid_setenv_cstr("XLLM_REAL_PROMPT", "");
    smoke_vertex_gemini_image_fileid_setenv_cstr("XLLM_REAL_EXPECT_TEXT_CONTAINS", "");

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
