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
    int iStreamRequestCount;
    int iToolResultCount;
    int iToolResultTextCount;
    int iToolResultImageCount;
    int iToolResultFileCount;
} smoke_server_state;

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

static bool smoke_send_chunk(SOCKET hSocket, const char *sBody)
{
    char sHeader[64];
    int iHeaderLen;
    size_t iBodyLen = sBody ? strlen(sBody) : 0u;

    iHeaderLen = snprintf(sHeader, sizeof(sHeader), "%x\r\n", (unsigned)iBodyLen);
    if ( iHeaderLen <= 0 ) {
        return false;
    }

    if ( !smoke_send_all(hSocket, sHeader, (size_t)iHeaderLen) ) {
        return false;
    }
    if ( iBodyLen > 0u && !smoke_send_all(hSocket, sBody, iBodyLen) ) {
        return false;
    }
    return smoke_send_all(hSocket, "\r\n", 2u);
}

static uint32 smoke_server_thread(ptr pParam)
{
    smoke_server_state *pState = (smoke_server_state *)pParam;
    int iRound;

    if ( !pState || pState->hListen == INVALID_SOCKET ) {
        return 1u;
    }

    for ( iRound = 0; iRound < 2; ++iRound ) {
        SOCKET hClient = INVALID_SOCKET;
        char aBuffer[16384];
        int iRecv;
        const char *sHeader =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/event-stream\r\n"
            "request-id: req-probe-anthropic-stream-tool-artifacts\r\n"
            "Cache-Control: no-cache\r\n"
            "Connection: close\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n";

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
            if ( strstr(aBuffer, "\"stream\":true") != NULL ) {
                ++pState->iStreamRequestCount;
            }
            if ( iRound == 1 ) {
                if ( strstr(aBuffer, "\"type\":\"tool_result\"") != NULL &&
                     strstr(aBuffer, "\"tool_use_id\":\"toolu_probe_stream_1\"") != NULL &&
                     strstr(aBuffer, "\"content\":[") != NULL ) {
                    ++pState->iToolResultCount;
                }
                if ( strstr(aBuffer, "\"type\":\"text\",\"text\":\"probe-value\"") != NULL ) {
                    ++pState->iToolResultTextCount;
                }
                if ( strstr(aBuffer, "\"type\":\"image\",\"source\":{\"type\":\"url\",\"url\":\"https://example.invalid/probe-stream-tool-image.png\"}}") != NULL ) {
                    ++pState->iToolResultImageCount;
                }
                if ( strstr(aBuffer, "\"type\":\"document\",\"source\":{\"type\":\"file\",\"file_id\":\"file_doc_probe_stream_tool\"}}") != NULL ) {
                    ++pState->iToolResultFileCount;
                }
            }
        }

        if ( !smoke_send_all(hClient, sHeader, strlen(sHeader)) ) {
            closesocket(hClient);
            closesocket(pState->hListen);
            pState->hListen = INVALID_SOCKET;
            return 3u;
        }

        if ( iRound == 0 ) {
            const char *sEvent1 =
                "data: {\"type\":\"message_start\",\"message\":{\"id\":\"msg_probe_tool_stream_1\",\"type\":\"message\",\"role\":\"assistant\",\"model\":\"claude-probe-stream-tool\",\"usage\":{\"input_tokens\":8,\"output_tokens\":0}}}\n\n";
            const char *sEvent2 =
                "data: {\"type\":\"content_block_start\",\"index\":0,\"content_block\":{\"type\":\"tool_use\",\"id\":\"toolu_probe_stream_1\",\"name\":\"get_probe_value\"}}\n\n";
            const char *sEvent3 =
                "data: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"input_json_delta\",\"partial_json\":\"{\\\"input\\\":\\\"pro\"}}\n\n";
            const char *sEvent4 =
                "data: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"input_json_delta\",\"partial_json\":\"be\\\"}\"}}\n\n";
            const char *sEvent5 =
                "data: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"tool_use\"},\"usage\":{\"output_tokens\":2}}\n\n";
            const char *sEvent6 = "data: {\"type\":\"message_stop\"}\n\n";

            if ( !smoke_send_chunk(hClient, sEvent1) ) return 4u;
            xrtSleep(10);
            if ( !smoke_send_chunk(hClient, sEvent2) ) return 5u;
            xrtSleep(10);
            if ( !smoke_send_chunk(hClient, sEvent3) ) return 6u;
            xrtSleep(10);
            if ( !smoke_send_chunk(hClient, sEvent4) ) return 7u;
            xrtSleep(10);
            if ( !smoke_send_chunk(hClient, sEvent5) ) return 8u;
            xrtSleep(10);
            if ( !smoke_send_chunk(hClient, sEvent6) ) return 9u;
        } else {
            const char *sEvent1 =
                "data: {\"type\":\"message_start\",\"message\":{\"id\":\"msg_probe_tool_stream_2\",\"type\":\"message\",\"role\":\"assistant\",\"model\":\"claude-probe-stream-tool\",\"usage\":{\"input_tokens\":13,\"output_tokens\":0}}}\n\n";
            const char *sEvent2 =
                "data: {\"type\":\"content_block_start\",\"index\":0,\"content_block\":{\"type\":\"text\",\"text\":\"probe anthropic stream tool artifacts ok\"}}\n\n";
            const char *sEvent3 =
                "data: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"end_turn\"},\"usage\":{\"output_tokens\":4}}\n\n";
            const char *sEvent4 = "data: {\"type\":\"message_stop\"}\n\n";

            if ( !smoke_send_chunk(hClient, sEvent1) ) return 10u;
            xrtSleep(10);
            if ( !smoke_send_chunk(hClient, sEvent2) ) return 11u;
            xrtSleep(10);
            if ( !smoke_send_chunk(hClient, sEvent3) ) return 12u;
            xrtSleep(10);
            if ( !smoke_send_chunk(hClient, sEvent4) ) return 13u;
        }

        if ( !smoke_send_all(hClient, "0\r\n\r\n", 5u) ) {
            closesocket(hClient);
            closesocket(pState->hListen);
            pState->hListen = INVALID_SOCKET;
            return 14u;
        }

        closesocket(hClient);
    }

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

static int smoke_file_contains(const char *sPath, const char *sNeedle)
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
    smoke_server_state tServer;
    struct sockaddr_in tAddr;
    socklen_t iAddrLen = sizeof(tAddr);
    xthread hServerThread = NULL;
    char sBaseUrl[256];
    const char *sSummaryPath = "build\\probe_anthropic_stream_tool_result_artifacts.summary.json";
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
    if ( listen(tServer.hListen, 2) == SOCKET_ERROR ) {
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
    if ( smoke_setenv_cstr("XLLM_REAL_ADAPTER", "anthropic_native") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_BASE_URL", sBaseUrl) != 0 ||
         smoke_setenv_cstr("XLLM_REAL_MODEL", "claude-probe-stream-tool") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_MULTIMODAL_MODEL", "claude-probe-stream-tool") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_API_KEY", "test-key") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_AUTH_KIND", "api_key_header") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_AUTH_HEADER", "x-api-key") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_STREAM", "require") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_ENABLE_TOOL", "1") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_TOOL_ID", "app.probe.get_value") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_TOOL_WIRE_NAME", "get_probe_value") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_TOOL_RESULT_TEXT", "probe-value") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_TOOL_RESULT_IMAGE_URL", "https://example.invalid/probe-stream-tool-image.png") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_TOOL_RESULT_IMAGE_MIME", "image/png") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_TOOL_RESULT_FILE_FILE_ID", "file_doc_probe_stream_tool") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_TOOL_RESULT_FILE_MIME", "application/pdf") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_TOOL_RESULT_FILE_NAME", "probe-stream-tool.pdf") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_EXPECT_STATUS", "completed") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_EXPECT_TEXT_CONTAINS", "probe anthropic stream tool artifacts ok") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_SUMMARY_PATH", sSummaryPath) != 0 ) {
        fprintf(stderr, "set environment variables failed\n");
        iExitCode = 7;
        goto cleanup;
    }

    remove(sSummaryPath);
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

    if ( tServer.iAcceptCount != 2 || tServer.iStreamRequestCount != 2 ) {
        fprintf(stderr, "unexpected request counters: accept=%d stream=%d\n", tServer.iAcceptCount, tServer.iStreamRequestCount);
        iExitCode = 9;
        goto cleanup;
    }
    if ( tServer.iToolResultCount != 1 ||
         tServer.iToolResultTextCount != 1 ||
         tServer.iToolResultImageCount != 1 ||
         tServer.iToolResultFileCount != 1 ) {
        fprintf(
            stderr,
            "unexpected tool result counters: result=%d text=%d image=%d file=%d\n",
            tServer.iToolResultCount,
            tServer.iToolResultTextCount,
            tServer.iToolResultImageCount,
            tServer.iToolResultFileCount
        );
        iExitCode = 10;
        goto cleanup;
    }

    if ( !smoke_file_contains(sSummaryPath, "\"status\":\"completed\"") ||
         !smoke_file_contains(sSummaryPath, "\"visible_text\":\"probe anthropic stream tool artifacts ok\"") ) {
        fprintf(stderr, "unexpected summary file contents\n");
        iExitCode = 11;
        goto cleanup;
    }

cleanup:
    if ( hServerThread ) {
        xrtThreadWait(hServerThread);
        xrtThreadDestroy(hServerThread);
    }
    if ( tServer.hListen != INVALID_SOCKET ) {
        closesocket(tServer.hListen);
    }
#if defined(_WIN32) || defined(_WIN64)
    WSACleanup();
#endif
    if ( iExitCode == 0 ) {
        printf("smoke_real_provider_probe_anthropic_stream_tool_result_artifacts ok\n");
    }
    return iExitCode;
}
