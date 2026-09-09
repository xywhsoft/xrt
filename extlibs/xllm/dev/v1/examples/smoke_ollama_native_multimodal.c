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
    int iApiChatCount;
    int iStreamFalseCount;
    int iSystemCount;
    int iMultimodalModelCount;
    int iContentCount;
    int iImageArrayCount;
    int iBase64Count;
    int iTextValueCount;
} demo_server_state;

static uint32 demo_server_thread(ptr pParam)
{
    demo_server_state *pState = (demo_server_state *)pParam;
    SOCKET hClient = INVALID_SOCKET;
    char aBuffer[8192];
    const char *sBody =
        "{"
        "\"model\":\"llava-mock\","
        "\"created_at\":\"2026-03-20T12:00:00Z\","
        "\"message\":{\"role\":\"assistant\",\"content\":\"ollama image ok\"},"
        "\"done\":true,"
        "\"done_reason\":\"stop\","
        "\"prompt_eval_count\":10,"
        "\"eval_count\":3"
        "}";
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
        if ( strstr(aBuffer, "POST /api/chat HTTP/1.1") != NULL ) {
            ++pState->iApiChatCount;
        }
        if ( strstr(aBuffer, "\"stream\":false") != NULL ) {
            ++pState->iStreamFalseCount;
        }
        if ( strstr(aBuffer, "\"role\":\"system\",\"content\":\"You are ollama multimodal test\"") != NULL ) {
            ++pState->iSystemCount;
        }
        if ( strstr(aBuffer, "\"model\":\"llava-mock\"") != NULL ) {
            ++pState->iMultimodalModelCount;
        }
        if ( strstr(aBuffer, "\"content\":\"describe ollama image\"") != NULL ) {
            ++pState->iContentCount;
        }
        if ( strstr(aBuffer, "\"images\":[\"iVBORw0KGgo=\"]") != NULL ) {
            ++pState->iImageArrayCount;
        }
        if ( strstr(aBuffer, "iVBORw0KGgo=") != NULL ) {
            ++pState->iBase64Count;
        }
        if ( strstr(aBuffer, "describe ollama image") != NULL ) {
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

static int demo_write_image_file(const char *sPath)
{
    static const unsigned char aPngSig[8] = {0x89u, 'P', 'N', 'G', '\r', '\n', 0x1Au, '\n'};
    FILE *pFile;

    pFile = fopen(sPath, "wb");
    if ( !pFile ) {
        return XRT_NET_ERROR;
    }

    if ( fwrite(aPngSig, 1u, sizeof(aPngSig), pFile) != sizeof(aPngSig) ) {
        fclose(pFile);
        return XRT_NET_ERROR;
    }

    fclose(pFile);
    return XRT_NET_OK;
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
    char sBaseUrl[128];
    char sImagePath[260];
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

    if ( xllm_register_ollama_native_adapter(pRuntime) != XRT_NET_OK ) {
        fprintf(stderr, "register built-in adapter failed\n");
        return 8;
    }

    (void)snprintf(sBaseUrl, sizeof(sBaseUrl), "http://127.0.0.1:%u", (unsigned)tServer.uPort);
    xllm_profile_init(&tProfile);
    tProfile.sId = "ollama-mm";
    tProfile.sProvider = "ollama";
    tProfile.sAdapter = XLLM_ADAPTER_OLLAMA_NATIVE;
    tProfile.sBaseUrl = sBaseUrl;
    tProfile.tAuth.eKind = XLLM_AUTH_NONE;
    tProfile.tModels.tText.sModelId = "llama-mock";
    tProfile.tModels.tText.tCaps.uFlags = XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT;
    tProfile.tModels.tMultimodal.sModelId = "llava-mock";
    tProfile.tModels.tMultimodal.tCaps.uFlags = XLLM_CAP_TEXT_IN | XLLM_CAP_IMAGE_IN | XLLM_CAP_TEXT_OUT;
    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "register profile failed\n");
        return 9;
    }

    tCreate.sInitialProfileId = "ollama-mm";
    tCreate.sSystemPrompt = "You are ollama multimodal test";
    pLlm = xllm_create(pRuntime, &tCreate);
    if ( !pLlm ) {
        fprintf(stderr, "create llm failed\n");
        return 10;
    }

    (void)snprintf(sImagePath, sizeof(sImagePath), "build\\smoke_ollama_mm_image.bin");
    if ( demo_write_image_file(sImagePath) != XRT_NET_OK ) {
        fprintf(stderr, "write image file failed\n");
        return 11;
    }

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "describe ollama image") != XRT_NET_OK ) {
        fprintf(stderr, "turn add text failed\n");
        return 12;
    }
    if ( xllm_turn_add_image_file(&tTurn, sImagePath, "image/png") != XRT_NET_OK ) {
        fprintf(stderr, "turn add image failed\n");
        return 13;
    }

    iStatus = xllm_send(pLlm, &tTurn, NULL, &pResponse);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "send failed: %d\n", iStatus);
        return 14;
    }

    xrtThreadWait(hServerThread);
    xrtThreadDestroy(hServerThread);
    hServerThread = NULL;

    sText = xllm_response_get_text(pResponse);
    if ( !sText || strcmp(sText, "ollama image ok") != 0 ) {
        fprintf(stderr, "unexpected response text: %s\n", sText ? sText : "(null)");
        return 15;
    }
    if ( tServer.iApiChatCount != 1 ||
         tServer.iStreamFalseCount != 1 ||
         tServer.iSystemCount != 1 ||
         tServer.iMultimodalModelCount != 1 ||
         tServer.iContentCount != 1 ||
         tServer.iImageArrayCount != 1 ||
         tServer.iBase64Count < 1 ||
         tServer.iTextValueCount < 1 ) {
        fprintf(
            stderr,
            "unexpected multimodal counters: api=%d stream=%d system=%d model=%d content=%d images=%d base64=%d text=%d\n",
            tServer.iApiChatCount,
            tServer.iStreamFalseCount,
            tServer.iSystemCount,
            tServer.iMultimodalModelCount,
            tServer.iContentCount,
            tServer.iImageArrayCount,
            tServer.iBase64Count,
            tServer.iTextValueCount
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
