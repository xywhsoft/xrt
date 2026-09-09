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
    int iApiKeyCount;
    int iVersionCount;
    int iNamedChoiceCount;
    int iToolDefCount;
    int iToolResultCount;
    int iToolResultTextCount;
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
        const char *sBody;
        char sResponse[24576];
        int iRespLen;

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
            if ( strstr(aBuffer, "x-api-key: alias-anthropic-key\r\n") != NULL ) {
                ++pState->iApiKeyCount;
            }
            if ( strstr(aBuffer, "anthropic-version: 2023-06-01\r\n") != NULL ) {
                ++pState->iVersionCount;
            }
            if ( iRound == 0 ) {
                if ( strstr(aBuffer, "\"tool_choice\":{\"type\":\"tool\",\"name\":\"get_probe_value\"") != NULL ) {
                    ++pState->iNamedChoiceCount;
                }
                if ( strstr(aBuffer, "\"tools\":[{\"name\":\"get_probe_value\"") != NULL ) {
                    ++pState->iToolDefCount;
                }
            } else {
                if ( strstr(aBuffer, "\"type\":\"tool_result\"") != NULL &&
                     strstr(aBuffer, "\"tool_use_id\":\"toolu_probe_alias_1\"") != NULL ) {
                    ++pState->iToolResultCount;
                }
                if ( strstr(aBuffer, "\"content\":\"alias-tool-value\"") != NULL ||
                     strstr(aBuffer, "\"type\":\"text\",\"text\":\"alias-tool-value\"") != NULL ) {
                    ++pState->iToolResultTextCount;
                }
            }
        }

        if ( iRound == 0 ) {
            sBody =
                "{\"id\":\"msg_probe_tool_alias_1\","
                "\"type\":\"message\","
                "\"role\":\"assistant\","
                "\"model\":\"claude-probe-alias\","
                "\"content\":[{\"type\":\"tool_use\",\"id\":\"toolu_probe_alias_1\",\"name\":\"get_probe_value\",\"input\":{\"input\":\"probe\"}}],"
                "\"stop_reason\":\"tool_use\","
                "\"usage\":{\"input_tokens\":9,\"output_tokens\":2}}";
        } else {
            sBody =
                "{\"id\":\"msg_probe_tool_alias_2\","
                "\"type\":\"message\","
                "\"role\":\"assistant\","
                "\"model\":\"claude-probe-alias\","
                "\"content\":[{\"type\":\"text\",\"text\":\"probe anthropic tool alias ok\"}],"
                "\"stop_reason\":\"end_turn\","
                "\"usage\":{\"input_tokens\":14,\"output_tokens\":4}}";
        }

        iRespLen = snprintf(
            sResponse,
            sizeof(sResponse),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "request-id: req_probe_anthropic_tool_alias_%d\r\n"
            "Connection: close\r\n"
            "Content-Length: %u\r\n"
            "\r\n"
            "%s",
            iRound + 1,
            (unsigned)strlen(sBody),
            sBody
        );

        if ( iRespLen > 0 && !smoke_send_all(hClient, sResponse, (size_t)iRespLen) ) {
            closesocket(hClient);
            closesocket(pState->hListen);
            pState->hListen = INVALID_SOCKET;
            return 3u;
        }

        closesocket(hClient);
    }

    closesocket(pState->hListen);
    pState->hListen = INVALID_SOCKET;
    return 0u;
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
    const char *sSummaryPath = "build\\probe_anthropic_tool_alias_envs.summary.json";
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
    if ( smoke_setenv_cstr("XLLM_REAL_ADAPTER", XLLM_ADAPTER_ANTHROPIC_NATIVE) != 0 ||
         smoke_setenv_cstr("ANTHROPIC_BASE_URL", sBaseUrl) != 0 ||
         smoke_setenv_cstr("ANTHROPIC_MODEL", "claude-probe-alias") != 0 ||
         smoke_setenv_cstr("ANTHROPIC_API_KEY", "alias-anthropic-key") != 0 ||
         smoke_setenv_cstr("ANTHROPIC_ENABLE_TOOL", "1") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_TOOL_ID", "app.probe.get_value") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_TOOL_WIRE_NAME", "get_probe_value") != 0 ||
         smoke_setenv_cstr("ANTHROPIC_TOOL_CHOICE", "named") != 0 ||
         smoke_setenv_cstr("ANTHROPIC_TOOL_CHOICE_NAME", "get_probe_value") != 0 ||
         smoke_setenv_cstr("ANTHROPIC_TOOL_RESULT_TEXT", "alias-tool-value") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_ENABLE_LOG", "0") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_ENABLE_TRACE", "0") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_EXPECT_STATUS", "completed") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_EXPECT_TEXT_CONTAINS", "probe anthropic tool alias ok") != 0 ||
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
         tServer.iApiKeyCount != 2 ||
         tServer.iVersionCount != 2 ||
         tServer.iNamedChoiceCount != 1 ||
         tServer.iToolDefCount < 1 ||
         tServer.iToolResultCount != 1 ||
         tServer.iToolResultTextCount != 1 ) {
        fprintf(
            stderr,
            "unexpected counters: accept=%d auth=%d version=%d named=%d defs=%d result=%d text=%d\n",
            tServer.iAcceptCount,
            tServer.iApiKeyCount,
            tServer.iVersionCount,
            tServer.iNamedChoiceCount,
            tServer.iToolDefCount,
            tServer.iToolResultCount,
            tServer.iToolResultTextCount
        );
        iExitCode = 9;
        goto cleanup;
    }

    if ( !smoke_file_contains(sSummaryPath, "\"status\":\"completed\"") ||
         !smoke_file_contains(sSummaryPath, "\"visible_text\":\"probe anthropic tool alias ok\"") ) {
        fprintf(stderr, "unexpected summary file contents\n");
        iExitCode = 10;
        goto cleanup;
    }

    printf("smoke_real_provider_probe_anthropic_tool_alias_envs ok\n");

cleanup:
    smoke_setenv_cstr("XLLM_REAL_ADAPTER", "");
    smoke_setenv_cstr("ANTHROPIC_BASE_URL", "");
    smoke_setenv_cstr("ANTHROPIC_MODEL", "");
    smoke_setenv_cstr("ANTHROPIC_API_KEY", "");
    smoke_setenv_cstr("ANTHROPIC_ENABLE_TOOL", "");
    smoke_setenv_cstr("XLLM_REAL_TOOL_ID", "");
    smoke_setenv_cstr("XLLM_REAL_TOOL_WIRE_NAME", "");
    smoke_setenv_cstr("ANTHROPIC_TOOL_CHOICE", "");
    smoke_setenv_cstr("ANTHROPIC_TOOL_CHOICE_NAME", "");
    smoke_setenv_cstr("ANTHROPIC_TOOL_RESULT_TEXT", "");
    smoke_setenv_cstr("XLLM_REAL_ENABLE_LOG", "");
    smoke_setenv_cstr("XLLM_REAL_ENABLE_TRACE", "");
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
    return iExitCode;
}
