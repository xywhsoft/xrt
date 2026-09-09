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
    int iNamedChoiceCount;
    int iToolDefCount;
    int iToolResultCount;
    int iToolResultTextCount;
} smoke_gemini_server_state;

static int smoke_gemini_validate_mock_body(void)
{
    static const char sBody[] =
        "{\"candidates\":[{\"finishReason\":\"STOP\",\"content\":{\"role\":\"model\",\"parts\":[{\"functionCall\":{\"name\":\"get_probe_value\",\"args\":{\"input\":\"probe\"}}}]}}],"
        "\"usageMetadata\":{\"promptTokenCount\":8,\"candidatesTokenCount\":2,\"totalTokenCount\":10}}";
    xvalue tRoot = xrtParseJSON((str)sBody, strlen(sBody));
    xvalue tCandidates = NULL;
    xvalue tCandidate = NULL;
    xvalue tContent = NULL;
    xvalue tParts = NULL;
    xvalue tPart0 = NULL;
    xvalue tFunctionCall = NULL;
    int iStatus = 0;

    if ( !tRoot ) {
        fprintf(stderr, "gemini mock self-parse failed: invalid json\n");
        return 1;
    }
    tCandidates = xllm__json_table_get(tRoot, "candidates");
    if ( !tCandidates || xvoType(tCandidates) != XVO_DT_ARRAY || xvoArrayItemCount(tCandidates) == 0u ) {
        fprintf(
            stderr,
            "gemini mock self-parse failed: candidates missing (root_type=%d candidates=%p candidates_type=%d candidates_count=%u)\n",
            tRoot ? (int)xvoType(tRoot) : -1,
            (void *)tCandidates,
            tCandidates ? (int)xvoType(tCandidates) : -1,
            tCandidates ? (unsigned)xvoArrayItemCount(tCandidates) : 0u
        );
        iStatus = 2;
        goto done;
    }
    tCandidate = xvoArrayGetValue(tCandidates, 0u);
    tContent = xllm__json_table_get(tCandidate, "content");
    tParts = xllm__json_table_get(tContent, "parts");
    if ( !tParts || xvoType(tParts) != XVO_DT_ARRAY || xvoArrayItemCount(tParts) == 0u ) {
        fprintf(
            stderr,
            "gemini mock self-parse failed: parts missing (content_type=%d parts=%p parts_type=%d parts_count=%u)\n",
            tContent ? (int)xvoType(tContent) : -1,
            (void *)tParts,
            tParts ? (int)xvoType(tParts) : -1,
            tParts ? (unsigned)xvoArrayItemCount(tParts) : 0u
        );
        iStatus = 3;
        goto done;
    }
    tPart0 = xvoArrayGetValue(tParts, 0u);
    tFunctionCall = xllm__json_table_get(tPart0, "functionCall");
    if ( !tFunctionCall || xvoType(tFunctionCall) != XVO_DT_TABLE ) {
        fprintf(stderr, "gemini mock self-parse failed: functionCall missing\n");
        iStatus = 4;
        goto done;
    }

done:
    if ( tRoot ) {
        xvoUnref(tRoot);
    }
    return iStatus;
}

static bool smoke_gemini_send_all(SOCKET hSocket, const char *sData, size_t iLen)
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

static uint32 smoke_gemini_server_thread(ptr pParam)
{
    smoke_gemini_server_state *pState = (smoke_gemini_server_state *)pParam;
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
            if ( strstr(aBuffer, "x-goog-api-key: alias-gemini-key\r\n") != NULL ) {
                ++pState->iAuthCount;
            }
            if ( strstr(aBuffer, "POST /v1beta/models/gemini-test-tool:generateContent HTTP/1.1\r\n") != NULL ) {
                ++pState->iPathCount;
            }
            if ( iRound == 0 ) {
                if ( strstr(aBuffer, "\"allowedFunctionNames\":[\"get_probe_value\"]") != NULL ) {
                    ++pState->iNamedChoiceCount;
                }
                if ( strstr(aBuffer, "\"functionDeclarations\":[{\"name\":\"get_probe_value\"") != NULL ) {
                    ++pState->iToolDefCount;
                }
            } else {
                if ( strstr(aBuffer, "\"functionResponse\":{\"name\":\"get_probe_value\"") != NULL ) {
                    ++pState->iToolResultCount;
                }
                if ( strstr(aBuffer, "\"response\":{\"text\":\"alias-gemini-tool-value\"}") != NULL ) {
                    ++pState->iToolResultTextCount;
                }
            }
        }

        if ( iRound == 0 ) {
            sBody =
                "{\"candidates\":[{\"finishReason\":\"STOP\",\"content\":{\"role\":\"model\",\"parts\":[{\"functionCall\":{\"name\":\"get_probe_value\",\"args\":{\"input\":\"probe\"}}}]}}],"
                "\"usageMetadata\":{\"promptTokenCount\":8,\"candidatesTokenCount\":2,\"totalTokenCount\":10}}";
        } else {
            sBody =
                "{\"candidates\":[{\"finishReason\":\"STOP\",\"content\":{\"role\":\"model\",\"parts\":[{\"text\":\"probe gemini tool alias ok\"}]}}],"
                "\"usageMetadata\":{\"promptTokenCount\":14,\"candidatesTokenCount\":4,\"totalTokenCount\":18}}";
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

        if ( iRespLen > 0 && !smoke_gemini_send_all(hClient, sResponse, (size_t)iRespLen) ) {
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

static int smoke_gemini_setenv_cstr(const char *sName, const char *sValue)
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
    smoke_gemini_server_state tServer;
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
    iExitCode = smoke_gemini_validate_mock_body();
    if ( iExitCode != 0 ) {
        goto cleanup;
    }

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

    hServerThread = xrtThreadCreate((ptr)smoke_gemini_server_thread, &tServer, 0);
    if ( !hServerThread ) {
        fprintf(stderr, "create server thread failed\n");
        iExitCode = 6;
        goto cleanup;
    }

    snprintf(sBaseUrl, sizeof(sBaseUrl), "http://127.0.0.1:%u/v1beta/models", (unsigned)tServer.uPort);
    if ( smoke_gemini_setenv_cstr("XLLM_REAL_ADAPTER", "gemini_native") != 0 ||
         smoke_gemini_setenv_cstr("GEMINI_BASE_URL", sBaseUrl) != 0 ||
         smoke_gemini_setenv_cstr("GEMINI_MODEL", "gemini-test-tool") != 0 ||
         smoke_gemini_setenv_cstr("GEMINI_API_KEY", "alias-gemini-key") != 0 ||
         smoke_gemini_setenv_cstr("GEMINI_TOOL_CHOICE", "named") != 0 ||
         smoke_gemini_setenv_cstr("GEMINI_TOOL_CHOICE_NAME", "get_probe_value") != 0 ||
         smoke_gemini_setenv_cstr("GEMINI_TOOL_RESULT_TEXT", "alias-gemini-tool-value") != 0 ||
         smoke_gemini_setenv_cstr("XLLM_REAL_ENABLE_LOG", "1") != 0 ||
         smoke_gemini_setenv_cstr("XLLM_REAL_ENABLE_TRACE", "1") != 0 ||
         smoke_gemini_setenv_cstr("XLLM_REAL_EXPECT_STATUS", "completed") != 0 ||
         smoke_gemini_setenv_cstr("XLLM_REAL_EXPECT_TEXT_CONTAINS", "probe gemini tool alias ok") != 0 ) {
        fprintf(stderr, "set environment variables failed\n");
        iExitCode = 7;
        goto cleanup;
    }

    iProbeExitCode = smoke_real_provider_probe_entry();
    if ( iProbeExitCode != 0 ) {
        fprintf(
            stderr,
            "probe failed unexpectedly: %d (accept=%d auth=%d path=%d named=%d defs=%d result=%d text=%d)\n",
            iProbeExitCode,
            tServer.iAcceptCount,
            tServer.iAuthCount,
            tServer.iPathCount,
            tServer.iNamedChoiceCount,
            tServer.iToolDefCount,
            tServer.iToolResultCount,
            tServer.iToolResultTextCount
        );
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
         tServer.iAuthCount != 2 ||
         tServer.iPathCount != 2 ||
         tServer.iNamedChoiceCount != 1 ||
         tServer.iToolDefCount != 1 ||
         tServer.iToolResultCount != 1 ||
         tServer.iToolResultTextCount != 1 ) {
        fprintf(
            stderr,
            "unexpected gemini tool alias counters: accept=%d auth=%d path=%d named=%d defs=%d result=%d text=%d\n",
            tServer.iAcceptCount,
            tServer.iAuthCount,
            tServer.iPathCount,
            tServer.iNamedChoiceCount,
            tServer.iToolDefCount,
            tServer.iToolResultCount,
            tServer.iToolResultTextCount
        );
        iExitCode = 9;
        goto cleanup;
    }

    printf("smoke_real_provider_probe_gemini_tool_alias_envs ok\n");

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
