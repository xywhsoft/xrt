#include "xllm-session.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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
    int iToolUseCount;
    int iToolResultCount;
    int iToolResultTextCount;
    int iToolResultImageCount;
    int iToolResultFileCount;
} demo_server_state;

static bool demo_send_all(SOCKET hSocket, const char *sData, size_t iLen)
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

static uint32 demo_server_thread(ptr pParam)
{
    demo_server_state *pState = (demo_server_state *)pParam;
    int iRound;

    if ( !pState || pState->hListen == INVALID_SOCKET ) {
        return 1u;
    }

    for ( iRound = 0; iRound < 2; ++iRound ) {
        SOCKET hClient = accept(pState->hListen, NULL, NULL);
        char aBuffer[16384];
        int iRecv;
        const char *sBody;
        char sResponse[20480];
        int iRespLen;

        if ( hClient == INVALID_SOCKET ) {
            closesocket(pState->hListen);
            pState->hListen = INVALID_SOCKET;
            return 2u;
        }

        iRecv = recv(hClient, aBuffer, (int)(sizeof(aBuffer) - 1u), 0);
        if ( iRecv > 0 ) {
            aBuffer[iRecv] = '\0';
            if ( iRound == 1 ) {
                if ( strstr(aBuffer, "\"type\":\"tool_use\"") != NULL &&
                     strstr(aBuffer, "\"id\":\"toolu_probe_1\"") != NULL &&
                     strstr(aBuffer, "\"name\":\"get_probe_value\"") != NULL ) {
                    ++pState->iToolUseCount;
                }
                if ( strstr(aBuffer, "\"type\":\"tool_result\"") != NULL &&
                     strstr(aBuffer, "\"tool_use_id\":\"toolu_probe_1\"") != NULL &&
                     strstr(aBuffer, "\"content\":[") != NULL ) {
                    ++pState->iToolResultCount;
                }
                if ( strstr(aBuffer, "\"type\":\"text\",\"text\":\"probe-value\"") != NULL ) {
                    ++pState->iToolResultTextCount;
                }
                if ( strstr(aBuffer, "\"type\":\"image\",\"source\":{\"type\":\"url\",\"url\":\"https://example.invalid/probe-tool-image.png\"}}") != NULL ) {
                    ++pState->iToolResultImageCount;
                }
                if ( strstr(aBuffer, "\"type\":\"document\",\"source\":{\"type\":\"file\",\"file_id\":\"file_doc_probe_tool\"}}") != NULL ) {
                    ++pState->iToolResultFileCount;
                }
            }
        }

        if ( iRound == 0 ) {
            sBody =
                "{\"id\":\"msg_probe_tool_1\","
                "\"type\":\"message\","
                "\"role\":\"assistant\","
                "\"model\":\"claude-probe\","
                "\"content\":[{\"type\":\"tool_use\",\"id\":\"toolu_probe_1\",\"name\":\"get_probe_value\",\"input\":{\"input\":\"probe\"}}],"
                "\"stop_reason\":\"tool_use\","
                "\"usage\":{\"input_tokens\":9,\"output_tokens\":2}}";
        } else {
            sBody =
                "{\"id\":\"msg_probe_tool_2\","
                "\"type\":\"message\","
                "\"role\":\"assistant\","
                "\"model\":\"claude-probe\","
                "\"content\":[{\"type\":\"text\",\"text\":\"tool artifacts probe ok\"}],"
                "\"stop_reason\":\"end_turn\","
                "\"usage\":{\"input_tokens\":14,\"output_tokens\":4}}";
        }

        iRespLen = snprintf(
            sResponse,
            sizeof(sResponse),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "request-id: req_probe_tool_artifacts_%d\r\n"
            "Connection: close\r\n"
            "Content-Length: %u\r\n"
            "\r\n"
            "%s",
            iRound + 1,
            (unsigned)strlen(sBody),
            sBody
        );

        if ( iRespLen > 0 && !demo_send_all(hClient, sResponse, (size_t)iRespLen) ) {
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

static void demo_setenv_required(const char *sName, const char *sValue)
{
#if defined(_WIN32) || defined(_WIN64)
    _putenv_s(sName, sValue ? sValue : "");
#else
    setenv(sName, sValue ? sValue : "", 1);
#endif
}

int main(void)
{
#if defined(_WIN32) || defined(_WIN64)
    WSADATA tWsaData;
#endif
    demo_server_state tServer;
    struct sockaddr_in tAddr;
    socklen_t iAddrLen = sizeof(tAddr);
    xthread hServerThread = NULL;
    char sBaseUrl[128];
    char sSummaryPath[260];
    int iExitCode;
    char *sSummary = NULL;

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
        return 2;
    }

    tAddr.sin_family = AF_INET;
    tAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    tAddr.sin_port = htons(0);
    if ( bind(tServer.hListen, (struct sockaddr *)&tAddr, sizeof(tAddr)) == SOCKET_ERROR ) {
        fprintf(stderr, "bind listen socket failed\n");
        return 3;
    }
    if ( listen(tServer.hListen, 2) == SOCKET_ERROR ) {
        fprintf(stderr, "listen failed\n");
        return 4;
    }
    if ( getsockname(tServer.hListen, (struct sockaddr *)&tAddr, &iAddrLen) == SOCKET_ERROR ) {
        fprintf(stderr, "getsockname failed\n");
        return 5;
    }
    tServer.uPort = ntohs(tAddr.sin_port);

    hServerThread = xrtThreadCreate((ptr)demo_server_thread, &tServer, 0);
    if ( !hServerThread ) {
        fprintf(stderr, "create server thread failed\n");
        return 6;
    }

    snprintf(sBaseUrl, sizeof(sBaseUrl), "http://127.0.0.1:%u/v1", (unsigned)tServer.uPort);
    snprintf(sSummaryPath, sizeof(sSummaryPath), "build\\probe_anthropic_tool_result_artifacts.summary.json");

    demo_setenv_required("XLLM_REAL_ADAPTER", XLLM_ADAPTER_ANTHROPIC_NATIVE);
    demo_setenv_required("XLLM_REAL_BASE_URL", sBaseUrl);
    demo_setenv_required("XLLM_REAL_MODEL", "claude-probe");
    demo_setenv_required("XLLM_REAL_MULTIMODAL_MODEL", "claude-probe");
    demo_setenv_required("XLLM_REAL_API_KEY", "test-key");
    demo_setenv_required("XLLM_REAL_ENABLE_TOOL", "1");
    demo_setenv_required("XLLM_REAL_TOOL_ID", "app.probe.get_value");
    demo_setenv_required("XLLM_REAL_TOOL_WIRE_NAME", "get_probe_value");
    demo_setenv_required("XLLM_REAL_TOOL_RESULT_TEXT", "probe-value");
    demo_setenv_required("XLLM_REAL_TOOL_RESULT_IMAGE_URL", "https://example.invalid/probe-tool-image.png");
    demo_setenv_required("XLLM_REAL_TOOL_RESULT_IMAGE_MIME", "image/png");
    demo_setenv_required("XLLM_REAL_TOOL_RESULT_FILE_FILE_ID", "file_doc_probe_tool");
    demo_setenv_required("XLLM_REAL_TOOL_RESULT_FILE_NAME", "probe-tool.pdf");
    demo_setenv_required("XLLM_REAL_TOOL_RESULT_FILE_MIME", "application/pdf");
    demo_setenv_required("XLLM_REAL_EXPECT_STATUS", "completed");
    demo_setenv_required("XLLM_REAL_EXPECT_TEXT_CONTAINS", "tool artifacts probe ok");
    demo_setenv_required("XLLM_REAL_SUMMARY_PATH", sSummaryPath);

    iExitCode = smoke_real_provider_probe_entry();

    xrtThreadWait(hServerThread);
    xrtThreadDestroy(hServerThread);
    hServerThread = NULL;

    if ( iExitCode != 0 ) {
        fprintf(stderr, "probe exited with %d\n", iExitCode);
        return 7;
    }
    if ( tServer.iToolUseCount != 1 ||
         tServer.iToolResultCount != 1 ||
         tServer.iToolResultTextCount != 1 ||
         tServer.iToolResultImageCount != 1 ||
         tServer.iToolResultFileCount != 1 ) {
        fprintf(
            stderr,
            "unexpected tool result counters: use=%d result=%d text=%d image=%d file=%d\n",
            tServer.iToolUseCount,
            tServer.iToolResultCount,
            tServer.iToolResultTextCount,
            tServer.iToolResultImageCount,
            tServer.iToolResultFileCount
        );
        return 8;
    }

    sSummary = (char *)xrtFileReadAll((str)sSummaryPath, XBUF_ANSI, NULL);
    if ( sSummary == NULL ||
         strstr(sSummary, "\"status\":\"completed\"") == NULL ||
         strstr(sSummary, "\"visible_text\":\"tool artifacts probe ok\"") == NULL ) {
        fprintf(stderr, "unexpected summary json\n");
        xrtFree(sSummary);
        return 9;
    }
    xrtFree(sSummary);

    printf("smoke_real_provider_probe_anthropic_tool_result_artifacts ok\n");

#if defined(_WIN32) || defined(_WIN64)
    WSACleanup();
#endif
    return 0;
}
