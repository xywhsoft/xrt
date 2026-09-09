#include "xllm-session.h"

#include <stdio.h>
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

typedef struct {
    SOCKET hListen;
    uint16 uPort;
    int iApiKeyCount;
    int iVersionCount;
    int iToolDefCount;
    int iToolChoiceNoneCount;
    int iDisableParallelCount;
    int iThinkingTypeCount;
    int iThinkingBudgetCount;
    int iThinkingDisplayCount;
    int iTextCount;
} demo_server_state;

static uint32 demo_server_thread(ptr pParam)
{
    demo_server_state *pState = (demo_server_state *)pParam;
    SOCKET hClient = INVALID_SOCKET;
    char aBuffer[8192];
    const char *sBody =
        "{\"id\":\"msg_none\","
        "\"type\":\"message\","
        "\"role\":\"assistant\","
        "\"model\":\"claude-mock\","
        "\"content\":[{\"type\":\"text\",\"text\":\"tool choice none ok\"}],"
        "\"stop_reason\":\"end_turn\","
        "\"usage\":{\"input_tokens\":10,\"output_tokens\":5}}";
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

    iRecv = recv(hClient, aBuffer, (int)(sizeof(aBuffer) - 1u), 0);
    if ( iRecv > 0 ) {
        aBuffer[iRecv] = '\0';
        if ( strstr(aBuffer, "x-api-key: test-key") != NULL ) {
            ++pState->iApiKeyCount;
        }
        if ( strstr(aBuffer, "anthropic-version: 2023-06-01") != NULL ) {
            ++pState->iVersionCount;
        }
        if ( strstr(aBuffer, "\"tools\":[{\"name\":\"get_weather\"") != NULL ) {
            ++pState->iToolDefCount;
        }
        if ( strstr(aBuffer, "\"tool_choice\":{\"type\":\"none\"") != NULL ) {
            ++pState->iToolChoiceNoneCount;
        }
        if ( strstr(aBuffer, "\"disable_parallel_tool_use\":true") != NULL ) {
            ++pState->iDisableParallelCount;
        }
        if ( strstr(aBuffer, "\"thinking\":{\"type\":\"enabled\"") != NULL ) {
            ++pState->iThinkingTypeCount;
        }
        if ( strstr(aBuffer, "\"budget_tokens\":1024") != NULL ) {
            ++pState->iThinkingBudgetCount;
        }
        if ( strstr(aBuffer, "\"display\":\"summarized\"") != NULL ) {
            ++pState->iThinkingDisplayCount;
        }
        if ( strstr(aBuffer, "\"tool choice none with thinking\"") != NULL ) {
            ++pState->iTextCount;
        }
    }

    iRespLen = snprintf(
        sResponse,
        sizeof(sResponse),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "request-id: req_anthropic_none_1\r\n"
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

int main(void)
{
#if defined(_WIN32) || defined(_WIN64)
    WSADATA tWsaData;
#endif
    demo_server_state tServer;
    struct sockaddr_in tAddr;
    socklen_t iAddrLen = sizeof(tAddr);
    xthread hServerThread = NULL;
    xllm_runtime *pRuntime = NULL;
    xllm *pLlm = NULL;
    xllm_response *pResponse = NULL;
    xllm_profile tProfile;
    xllm_create_options tCreate;
    xllm_turn tTurn;
    xllm_tool_def tTool;
    char sBaseUrl[128];
    xvalue tSchema = NULL;
    const char *sText;
    int iStatus = 0;

#if defined(_WIN32) || defined(_WIN64)
    if ( WSAStartup(MAKEWORD(2, 2), &tWsaData) != 0 ) {
        fprintf(stderr, "winsock startup failed\n");
        return 1;
    }
#endif

    memset(&tServer, 0, sizeof(tServer));
    memset(&tAddr, 0, sizeof(tAddr));
    memset(&tCreate, 0, sizeof(tCreate));
    memset(&tTool, 0, sizeof(tTool));

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
    if ( listen(tServer.hListen, 1) == SOCKET_ERROR ) {
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

    if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || !pRuntime ) {
        fprintf(stderr, "create runtime failed\n");
        return 7;
    }

    if ( xllm_register_anthropic_native_adapter(pRuntime) != XRT_NET_OK ) {
        fprintf(stderr, "register built-in adapter failed\n");
        return 8;
    }

    (void)snprintf(sBaseUrl, sizeof(sBaseUrl), "http://127.0.0.1:%u/v1", (unsigned)tServer.uPort);
    xllm_profile_init(&tProfile);
    tProfile.sId = "anthropic-none";
    tProfile.sProvider = "anthropic";
    tProfile.sAdapter = XLLM_ADAPTER_ANTHROPIC_NATIVE;
    tProfile.sBaseUrl = sBaseUrl;
    tProfile.tAuth.eKind = XLLM_AUTH_API_KEY_HEADER;
    tProfile.tAuth.sSecret = "test-key";
    tProfile.tAuth.sHeaderName = "x-api-key";
    tProfile.tModels.tText.sModelId = "claude-mock";
    tProfile.tModels.tText.eCapMode = XLLM_CAP_MODE_EXACT;
    tProfile.tModels.tText.tCaps.uFlags =
        XLLM_CAP_TEXT_IN |
        XLLM_CAP_TEXT_OUT |
        XLLM_CAP_TOOL_CALL_OUT |
        XLLM_CAP_REASONING_CONTROL |
        XLLM_CAP_THINKING_SUMMARY_OUT;
    tProfile.tDefaults.tGeneration.tMaxOutputTokens.bSet = true;
    tProfile.tDefaults.tGeneration.tMaxOutputTokens.iValue = 2048u;
    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "register profile failed\n");
        return 9;
    }

    tCreate.sInitialProfileId = "anthropic-none";
    pLlm = xllm_create(pRuntime, &tCreate);
    if ( !pLlm ) {
        fprintf(stderr, "create llm failed\n");
        return 10;
    }

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "tool choice none with thinking") != XRT_NET_OK ) {
        fprintf(stderr, "turn add text failed\n");
        return 11;
    }

    tSchema = xrtParseJSON((str)"{\"type\":\"object\",\"properties\":{\"city\":{\"type\":\"string\"}},\"required\":[\"city\"]}",
                           strlen("{\"type\":\"object\",\"properties\":{\"city\":{\"type\":\"string\"}},\"required\":[\"city\"]}"));
    if ( !tSchema ) {
        fprintf(stderr, "schema parse failed\n");
        return 12;
    }
    tTool.sToolId = "app.weather.get_current";
    tTool.sWireName = "get_weather";
    tTool.sDescription = "Get weather";
    tTool.tInputSchema = tSchema;
    if ( xllm_turn_add_tool(&tTurn, &tTool) != XRT_NET_OK ) {
        xvoUnref(tSchema);
        fprintf(stderr, "turn add tool failed\n");
        return 13;
    }
    xvoUnref(tSchema);
    tSchema = NULL;

    if ( xllm_turn_set_tool_choice(&tTurn, XLLM_TOOL_CHOICE_NONE, NULL, false) != XRT_NET_OK ) {
        fprintf(stderr, "set tool choice failed\n");
        return 14;
    }
    tTurn.tReasoning.tEnabled.bSet = true;
    tTurn.tReasoning.tEnabled.bValue = true;
    tTurn.tReasoning.tBudgetTokens.bSet = true;
    tTurn.tReasoning.tBudgetTokens.iValue = 1024u;
    tTurn.tReasoning.tExposeThinking.bSet = true;
    tTurn.tReasoning.tExposeThinking.bValue = true;

    iStatus = xllm_send(pLlm, &tTurn, NULL, &pResponse);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "send failed: %d\n", iStatus);
        return 14;
    }

    xrtThreadWait(hServerThread);
    xrtThreadDestroy(hServerThread);
    hServerThread = NULL;

    sText = xllm_response_get_text(pResponse);
    if ( !sText || strcmp(sText, "tool choice none ok") != 0 ) {
        fprintf(stderr, "unexpected response text: %s\n", sText ? sText : "(null)");
        return 15;
    }

    if ( tServer.iApiKeyCount != 1 ||
         tServer.iVersionCount != 1 ||
         tServer.iToolDefCount != 1 ||
         tServer.iToolChoiceNoneCount != 1 ||
         tServer.iDisableParallelCount != 1 ||
         tServer.iThinkingTypeCount != 1 ||
         tServer.iThinkingBudgetCount != 1 ||
         tServer.iThinkingDisplayCount != 1 ||
         tServer.iTextCount != 1 ) {
        fprintf(
            stderr,
            "unexpected request counters: key=%d version=%d tools=%d choice=%d parallel=%d thinking=%d budget=%d display=%d text=%d\n",
            tServer.iApiKeyCount,
            tServer.iVersionCount,
            tServer.iToolDefCount,
            tServer.iToolChoiceNoneCount,
            tServer.iDisableParallelCount,
            tServer.iThinkingTypeCount,
            tServer.iThinkingBudgetCount,
            tServer.iThinkingDisplayCount,
            tServer.iTextCount
        );
        return 16;
    }

    printf("ok: %s\n", sText);

    xllm_response_free(pResponse);
    xllm_turn_reset(&tTurn);
    xllm_destroy(pLlm);
    xllm_runtime_destroy(pRuntime);
    if ( tServer.hListen != INVALID_SOCKET ) {
        closesocket(tServer.hListen);
    }
#if defined(_WIN32) || defined(_WIN64)
    WSACleanup();
#endif
    return 0;
}
