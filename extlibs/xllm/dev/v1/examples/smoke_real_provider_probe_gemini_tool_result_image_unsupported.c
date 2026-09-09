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
    int iToolDefCount;
} smoke_gemini_tool_result_image_unsupported_server_state;

static bool smoke_gemini_tool_result_image_unsupported_send_all(SOCKET hSocket, const char *sData, size_t iLen)
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

static uint32 smoke_gemini_tool_result_image_unsupported_server_thread(ptr pParam)
{
    smoke_gemini_tool_result_image_unsupported_server_state *pState = (smoke_gemini_tool_result_image_unsupported_server_state *)pParam;
    SOCKET hClient = INVALID_SOCKET;
    char aBuffer[16384];
    int iRecv;
    const char *sBody =
        "{\"candidates\":[{\"finishReason\":\"STOP\",\"content\":{\"role\":\"model\",\"parts\":[{\"functionCall\":{\"name\":\"get_probe_value\",\"args\":{\"input\":\"probe\"}}}]}}],"
        "\"usageMetadata\":{\"promptTokenCount\":8,\"candidatesTokenCount\":2,\"totalTokenCount\":10}}";
    char sResponse[24576];
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
        if ( strstr(aBuffer, "x-goog-api-key: alias-gemini-tool-result-image-unsupported-key\r\n") != NULL ) {
            ++pState->iAuthCount;
        }
        if ( strstr(aBuffer, "POST /v1beta/models/gemini-test-tool-result-image-unsupported:generateContent HTTP/1.1\r\n") != NULL ) {
            ++pState->iPathCount;
        }
        if ( strstr(aBuffer, "\"functionDeclarations\":[{\"name\":\"get_probe_value\"") != NULL ) {
            ++pState->iToolDefCount;
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
        (void)smoke_gemini_tool_result_image_unsupported_send_all(hClient, sResponse, (size_t)iRespLen);
    }

    closesocket(hClient);
    closesocket(pState->hListen);
    pState->hListen = INVALID_SOCKET;
    return 0u;
}

static int smoke_gemini_tool_result_image_unsupported_setenv_cstr(const char *sName, const char *sValue)
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

static int smoke_gemini_tool_result_image_unsupported_file_contains(const char *sPath, const char *sNeedle)
{
    FILE *pFile = fopen(sPath, "rb");
    long iSize;
    char *sBuffer;
    int iFound;

    if ( !pFile ) {
        return 0;
    }
    if ( fseek(pFile, 0, SEEK_END) != 0 ) {
        fclose(pFile);
        return 0;
    }
    iSize = ftell(pFile);
    if ( iSize < 0 || fseek(pFile, 0, SEEK_SET) != 0 ) {
        fclose(pFile);
        return 0;
    }
    sBuffer = (char *)calloc((size_t)iSize + 1u, 1u);
    if ( !sBuffer ) {
        fclose(pFile);
        return 0;
    }
    if ( iSize > 0 ) {
        (void)fread(sBuffer, 1u, (size_t)iSize, pFile);
    }
    fclose(pFile);
    iFound = (strstr(sBuffer, sNeedle) != NULL) ? 1 : 0;
    free(sBuffer);
    return iFound;
}

int main(void)
{
#if defined(_WIN32) || defined(_WIN64)
    WSADATA tWsaData;
#endif
    smoke_gemini_tool_result_image_unsupported_server_state tServer;
    struct sockaddr_in tAddr;
    socklen_t iAddrLen = sizeof(tAddr);
    xthread hServerThread = NULL;
    char sBaseUrl[256];
    const char *sSummaryPath = "build\\probe_gemini_tool_result_image_unsupported.summary.json";
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

    hServerThread = xrtThreadCreate((ptr)smoke_gemini_tool_result_image_unsupported_server_thread, &tServer, 0);
    if ( !hServerThread ) {
        fprintf(stderr, "create server thread failed\n");
        iExitCode = 6;
        goto cleanup;
    }

    snprintf(sBaseUrl, sizeof(sBaseUrl), "http://127.0.0.1:%u/v1beta/models", (unsigned)tServer.uPort);
    remove(sSummaryPath);
    if ( smoke_gemini_tool_result_image_unsupported_setenv_cstr("XLLM_REAL_ADAPTER", "gemini_native") != 0 ||
         smoke_gemini_tool_result_image_unsupported_setenv_cstr("GEMINI_BASE_URL", sBaseUrl) != 0 ||
         smoke_gemini_tool_result_image_unsupported_setenv_cstr("GEMINI_MODEL", "gemini-test-tool-result-image-unsupported") != 0 ||
         smoke_gemini_tool_result_image_unsupported_setenv_cstr("GEMINI_API_KEY", "alias-gemini-tool-result-image-unsupported-key") != 0 ||
         smoke_gemini_tool_result_image_unsupported_setenv_cstr("XLLM_REAL_ENABLE_TOOL", "1") != 0 ||
         smoke_gemini_tool_result_image_unsupported_setenv_cstr("GEMINI_TOOL_CHOICE", "named") != 0 ||
         smoke_gemini_tool_result_image_unsupported_setenv_cstr("GEMINI_TOOL_CHOICE_NAME", "get_probe_value") != 0 ||
         smoke_gemini_tool_result_image_unsupported_setenv_cstr("GEMINI_TOOL_RESULT_IMAGE_URL", "https://example.com/tool-result.png") != 0 ||
         smoke_gemini_tool_result_image_unsupported_setenv_cstr("GEMINI_TOOL_RESULT_IMAGE_MIME", "image/png") != 0 ||
         smoke_gemini_tool_result_image_unsupported_setenv_cstr("XLLM_REAL_PROMPT", "use the tool and inspect its image result") != 0 ||
         smoke_gemini_tool_result_image_unsupported_setenv_cstr("XLLM_REAL_SUMMARY_PATH", sSummaryPath) != 0 ) {
        fprintf(stderr, "set environment variables failed\n");
        iExitCode = 7;
        goto cleanup;
    }

    iProbeExitCode = smoke_real_provider_probe_entry();
    if ( iProbeExitCode == 0 ) {
        fprintf(stderr, "probe unexpectedly succeeded\n");
        iExitCode = 8;
        goto cleanup;
    }

    if ( hServerThread ) {
        if ( tServer.hListen != INVALID_SOCKET ) {
            closesocket(tServer.hListen);
            tServer.hListen = INVALID_SOCKET;
        }
        xrtThreadWait(hServerThread);
        xrtThreadDestroy(hServerThread);
        hServerThread = NULL;
    }

    if ( tServer.iAcceptCount != 1 ||
         tServer.iAuthCount != 1 ||
         tServer.iPathCount != 1 ||
         tServer.iToolDefCount != 1 ) {
        fprintf(
            stderr,
            "unexpected gemini tool-result-image unsupported counters: accept=%d auth=%d path=%d defs=%d\n",
            tServer.iAcceptCount,
            tServer.iAuthCount,
            tServer.iPathCount,
            tServer.iToolDefCount
        );
        iExitCode = 9;
        goto cleanup;
    }

    if ( !smoke_gemini_tool_result_image_unsupported_file_contains(sSummaryPath, "\"error_code\":\"unsupported_input_type\"") ||
         !smoke_gemini_tool_result_image_unsupported_file_contains(sSummaryPath, "gemini tool result currently supports only text/json parts") ) {
        fprintf(stderr, "unexpected summary content for gemini tool result image unsupported case\n");
        iExitCode = 10;
        goto cleanup;
    }

    printf("smoke_real_provider_probe_gemini_tool_result_image_unsupported ok\n");

cleanup:
    if ( hServerThread ) {
        if ( tServer.hListen != INVALID_SOCKET ) {
            closesocket(tServer.hListen);
            tServer.hListen = INVALID_SOCKET;
        }
        xrtThreadWait(hServerThread);
        xrtThreadDestroy(hServerThread);
    } else if ( tServer.hListen != INVALID_SOCKET ) {
        closesocket(tServer.hListen);
    }
#if defined(_WIN32) || defined(_WIN64)
    WSACleanup();
#endif
    return iExitCode;
}
