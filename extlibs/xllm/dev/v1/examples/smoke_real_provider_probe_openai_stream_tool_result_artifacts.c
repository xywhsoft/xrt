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
            "Cache-Control: no-cache\r\n"
            "Connection: close\r\n"
            "Transfer-Encoding: chunked\r\n"
            "x-request-id: req_probe_openai_stream_tool_artifacts\r\n"
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
                if ( strstr(aBuffer, "\"role\":\"tool\"") != NULL &&
                     strstr(aBuffer, "\"tool_call_id\":\"call_probe_tool_stream_1\"") != NULL &&
                     strstr(aBuffer, "\"content\":[") != NULL ) {
                    ++pState->iToolResultCount;
                }
                if ( strstr(aBuffer, "\"type\":\"text\",\"text\":\"probe-value\"") != NULL ) {
                    ++pState->iToolResultTextCount;
                }
                if ( strstr(aBuffer, "\"type\":\"image_url\",\"image_url\":{\"url\":\"https://example.invalid/probe-openai-stream-tool-image.png\"}}") != NULL ) {
                    ++pState->iToolResultImageCount;
                }
                if ( strstr(aBuffer, "\"type\":\"file\",\"file\":{\"file_id\":\"file_doc_probe_openai_stream_tool\"}}") != NULL ) {
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
                "data: {\"id\":\"chatcmpl-probe-stream-tool-1\",\"object\":\"chat.completion.chunk\",\"model\":\"gpt-probe-stream-tool\","
                "\"choices\":[{\"index\":0,\"delta\":{\"role\":\"assistant\",\"tool_calls\":[{\"index\":0,\"id\":\"call_probe_tool_stream_1\",\"type\":\"function\",\"function\":{\"name\":\"get_probe_value\",\"arguments\":\"\"}}]},\"finish_reason\":null}]}\n\n";
            const char *sEvent2 =
                "data: {\"id\":\"chatcmpl-probe-stream-tool-1\",\"object\":\"chat.completion.chunk\",\"model\":\"gpt-probe-stream-tool\","
                "\"choices\":[{\"index\":0,\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"arguments\":\"{\\\"input\\\":\\\"pro\"}}]},\"finish_reason\":null}]}\n\n";
            const char *sEvent3 =
                "data: {\"id\":\"chatcmpl-probe-stream-tool-1\",\"object\":\"chat.completion.chunk\",\"model\":\"gpt-probe-stream-tool\","
                "\"choices\":[{\"index\":0,\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"arguments\":\"be\\\"}\"}}]},\"finish_reason\":null}]}\n\n";
            const char *sEvent4 =
                "data: {\"id\":\"chatcmpl-probe-stream-tool-1\",\"object\":\"chat.completion.chunk\",\"model\":\"gpt-probe-stream-tool\","
                "\"choices\":[{\"index\":0,\"delta\":{},\"finish_reason\":\"tool_calls\"}]}\n\n";
            const char *sDone = "data: [DONE]\n\n";

            if ( !smoke_send_chunk(hClient, sEvent1) ) return 4u;
            xrtSleep(10);
            if ( !smoke_send_chunk(hClient, sEvent2) ) return 5u;
            xrtSleep(10);
            if ( !smoke_send_chunk(hClient, sEvent3) ) return 6u;
            xrtSleep(10);
            if ( !smoke_send_chunk(hClient, sEvent4) ) return 7u;
            xrtSleep(10);
            if ( !smoke_send_chunk(hClient, sDone) ) return 8u;
        } else {
            const char *sEvent1 =
                "data: {\"id\":\"chatcmpl-probe-stream-tool-2\",\"object\":\"chat.completion.chunk\",\"model\":\"gpt-probe-stream-tool\","
                "\"choices\":[{\"index\":0,\"delta\":{\"role\":\"assistant\",\"content\":\"probe openai stream \"},\"finish_reason\":null}]}\n\n";
            const char *sEvent2 =
                "data: {\"id\":\"chatcmpl-probe-stream-tool-2\",\"object\":\"chat.completion.chunk\",\"model\":\"gpt-probe-stream-tool\","
                "\"choices\":[{\"index\":0,\"delta\":{\"content\":\"tool artifacts ok\"},\"finish_reason\":null}]}\n\n";
            const char *sEvent3 =
                "data: {\"id\":\"chatcmpl-probe-stream-tool-2\",\"object\":\"chat.completion.chunk\",\"model\":\"gpt-probe-stream-tool\","
                "\"choices\":[{\"index\":0,\"delta\":{},\"finish_reason\":\"stop\"}]}\n\n";
            const char *sDone = "data: [DONE]\n\n";

            if ( !smoke_send_chunk(hClient, sEvent1) ) return 9u;
            xrtSleep(10);
            if ( !smoke_send_chunk(hClient, sEvent2) ) return 10u;
            xrtSleep(10);
            if ( !smoke_send_chunk(hClient, sEvent3) ) return 11u;
            xrtSleep(10);
            if ( !smoke_send_chunk(hClient, sDone) ) return 12u;
        }

        if ( !smoke_send_all(hClient, "0\r\n\r\n", 5u) ) {
            closesocket(hClient);
            closesocket(pState->hListen);
            pState->hListen = INVALID_SOCKET;
            return 13u;
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
    const char *sSummaryPath = "build\\probe_openai_stream_tool_result_artifacts.summary.json";
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
    if ( smoke_setenv_cstr("XLLM_REAL_ADAPTER", "openai_compat") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_BASE_URL", sBaseUrl) != 0 ||
         smoke_setenv_cstr("XLLM_REAL_MODEL", "gpt-probe-stream-tool") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_MULTIMODAL_MODEL", "gpt-probe-stream-tool") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_AUTH_KIND", "none") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_ENABLE_LOG", "0") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_ENABLE_TRACE", "0") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_STREAM", "require") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_ENABLE_TOOL", "1") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_TOOL_ID", "app.probe.get_value") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_TOOL_WIRE_NAME", "get_probe_value") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_TOOL_RESULT_TEXT", "probe-value") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_TOOL_RESULT_IMAGE_URL", "https://example.invalid/probe-openai-stream-tool-image.png") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_TOOL_RESULT_IMAGE_MIME", "image/png") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_TOOL_RESULT_FILE_FILE_ID", "file_doc_probe_openai_stream_tool") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_TOOL_RESULT_FILE_MIME", "application/pdf") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_TOOL_RESULT_FILE_NAME", "probe-openai-stream-tool.pdf") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_EXPECT_STATUS", "completed") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_EXPECT_TEXT_CONTAINS", "probe openai stream tool artifacts ok") != 0 ||
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

    if ( tServer.iAcceptCount != 2 ||
         tServer.iStreamRequestCount != 2 ||
         tServer.iToolResultCount != 1 ||
         tServer.iToolResultTextCount != 1 ||
         tServer.iToolResultImageCount != 1 ||
         tServer.iToolResultFileCount != 1 ) {
        fprintf(
            stderr,
            "unexpected counters: accept=%d stream=%d result=%d text=%d image=%d file=%d\n",
            tServer.iAcceptCount,
            tServer.iStreamRequestCount,
            tServer.iToolResultCount,
            tServer.iToolResultTextCount,
            tServer.iToolResultImageCount,
            tServer.iToolResultFileCount
        );
        iExitCode = 9;
        goto cleanup;
    }

    if ( !smoke_file_contains(sSummaryPath, "\"status\":\"completed\"") ||
         !smoke_file_contains(sSummaryPath, "\"visible_text\":\"probe openai stream tool artifacts ok\"") ||
         !smoke_file_contains(sSummaryPath, "\"stream_mode\":3") ) {
        fprintf(stderr, "unexpected summary file contents\n");
        iExitCode = 10;
        goto cleanup;
    }

cleanup:
    smoke_setenv_cstr("XLLM_REAL_ADAPTER", "");
    smoke_setenv_cstr("XLLM_REAL_BASE_URL", "");
    smoke_setenv_cstr("XLLM_REAL_MODEL", "");
    smoke_setenv_cstr("XLLM_REAL_MULTIMODAL_MODEL", "");
    smoke_setenv_cstr("XLLM_REAL_AUTH_KIND", "");
    smoke_setenv_cstr("XLLM_REAL_ENABLE_LOG", "");
    smoke_setenv_cstr("XLLM_REAL_ENABLE_TRACE", "");
    smoke_setenv_cstr("XLLM_REAL_STREAM", "");
    smoke_setenv_cstr("XLLM_REAL_ENABLE_TOOL", "");
    smoke_setenv_cstr("XLLM_REAL_TOOL_ID", "");
    smoke_setenv_cstr("XLLM_REAL_TOOL_WIRE_NAME", "");
    smoke_setenv_cstr("XLLM_REAL_TOOL_RESULT_TEXT", "");
    smoke_setenv_cstr("XLLM_REAL_TOOL_RESULT_IMAGE_URL", "");
    smoke_setenv_cstr("XLLM_REAL_TOOL_RESULT_IMAGE_MIME", "");
    smoke_setenv_cstr("XLLM_REAL_TOOL_RESULT_FILE_FILE_ID", "");
    smoke_setenv_cstr("XLLM_REAL_TOOL_RESULT_FILE_MIME", "");
    smoke_setenv_cstr("XLLM_REAL_TOOL_RESULT_FILE_NAME", "");
    smoke_setenv_cstr("XLLM_REAL_EXPECT_STATUS", "");
    smoke_setenv_cstr("XLLM_REAL_EXPECT_TEXT_CONTAINS", "");
    smoke_setenv_cstr("XLLM_REAL_SUMMARY_PATH", "");

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
    if ( iExitCode == 0 ) {
        printf("smoke_real_provider_probe_openai_stream_tool_result_artifacts ok\n");
    }
    return iExitCode;
}
