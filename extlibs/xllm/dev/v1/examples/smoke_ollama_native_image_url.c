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
    int iAcceptCount;
    int iImagePathCount;
} demo_image_server_state;

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
} demo_chat_server_state;

static bool demo_send_all(SOCKET hSocket, const void *pData, size_t iLen)
{
    const char *sData = (const char *)pData;
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

static bool demo_read_http_message(SOCKET hSocket, char *sBuffer, size_t iCapacity)
{
    size_t iUsed = 0u;
    size_t iRequired = 0u;
    bool bHeaderParsed = false;

    if ( !sBuffer || iCapacity < 8u ) {
        return false;
    }

    while ( iUsed + 1u < iCapacity ) {
        char *sHeaderEnd;
        int iRecv = recv(hSocket, sBuffer + iUsed, (int)(iCapacity - iUsed - 1u), 0);
        if ( iRecv <= 0 ) {
            return false;
        }
        iUsed += (size_t)iRecv;
        sBuffer[iUsed] = '\0';

        if ( !bHeaderParsed ) {
            sHeaderEnd = strstr(sBuffer, "\r\n\r\n");
            if ( sHeaderEnd != NULL ) {
                const char *sContentLength = strstr(sBuffer, "Content-Length:");
                size_t iHeaderLen = (size_t)((sHeaderEnd + 4) - sBuffer);
                size_t iContentLength = 0u;

                if ( sContentLength != NULL && sContentLength < sHeaderEnd ) {
                    sContentLength += strlen("Content-Length:");
                    while ( *sContentLength == ' ' ) {
                        ++sContentLength;
                    }
                    iContentLength = (size_t)strtoul(sContentLength, NULL, 10);
                }
                iRequired = iHeaderLen + iContentLength;
                bHeaderParsed = true;
                if ( iUsed >= iRequired ) {
                    break;
                }
            }
        } else if ( iUsed >= iRequired ) {
            break;
        }
    }

    return bHeaderParsed;
}

static uint32 demo_image_server_thread(ptr pParam)
{
    static const unsigned char aPngSig[8] = {0x89u, 'P', 'N', 'G', '\r', '\n', 0x1Au, '\n'};
    demo_image_server_state *pState = (demo_image_server_state *)pParam;
    SOCKET hClient = INVALID_SOCKET;
    char aBuffer[4096];
    char sResponse[1024];
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
    if ( demo_read_http_message(hClient, aBuffer, sizeof(aBuffer)) ) {
        if ( strstr(aBuffer, "GET /image.png HTTP/1.1\r\n") != NULL ) {
            ++pState->iImagePathCount;
        }
    }

    iRespLen = snprintf(
        sResponse,
        sizeof(sResponse),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: image/png\r\n"
        "Connection: close\r\n"
        "Content-Length: %u\r\n"
        "\r\n",
        (unsigned)sizeof(aPngSig)
    );
    if ( iRespLen > 0 ) {
        (void)demo_send_all(hClient, sResponse, (size_t)iRespLen);
        (void)demo_send_all(hClient, aPngSig, sizeof(aPngSig));
    }

    closesocket(hClient);
    closesocket(pState->hListen);
    pState->hListen = INVALID_SOCKET;
    return 0u;
}

static uint32 demo_chat_server_thread(ptr pParam)
{
    demo_chat_server_state *pState = (demo_chat_server_state *)pParam;
    SOCKET hClient = INVALID_SOCKET;
    char aBuffer[8192];
    const char *sBody =
        "{"
        "\"model\":\"llava-mock\","
        "\"created_at\":\"2026-03-20T12:00:00Z\","
        "\"message\":{\"role\":\"assistant\",\"content\":\"ollama image url ok\"},"
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
        if ( strstr(aBuffer, "\"role\":\"system\",\"content\":\"You are ollama url test\"") != NULL ) {
            ++pState->iSystemCount;
        }
        if ( strstr(aBuffer, "\"model\":\"llava-mock\"") != NULL ) {
            ++pState->iMultimodalModelCount;
        }
        if ( strstr(aBuffer, "\"content\":\"describe ollama image url\"") != NULL ) {
            ++pState->iContentCount;
        }
        if ( strstr(aBuffer, "\"images\":[\"iVBORw0KGgo=\"]") != NULL ) {
            ++pState->iImageArrayCount;
        }
        if ( strstr(aBuffer, "iVBORw0KGgo=") != NULL ) {
            ++pState->iBase64Count;
        }
        if ( strstr(aBuffer, "describe ollama image url") != NULL ) {
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
        (void)demo_send_all(hClient, sResponse, (size_t)iRespLen);
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
    demo_image_server_state tImageServer;
    demo_chat_server_state tChatServer;
    struct sockaddr_in tAddr;
    socklen_t iAddrLen = sizeof(tAddr);
    xthread hImageThread = NULL;
    xthread hChatThread = NULL;
    xllm_runtime *pRuntime = NULL;
    xllm *pLlm = NULL;
    xllm_response *pResponse = NULL;
    xllm_profile tProfile;
    xllm_create_options tCreate;
    xllm_turn tTurn;
    char sBaseUrl[128];
    char sImageUrl[160];
    const char *sText;
    int iStatus = 0;

#if defined(_WIN32) || defined(_WIN64)
    if ( WSAStartup(MAKEWORD(2, 2), &tWsaData) != 0 ) {
        fprintf(stderr, "winsock startup failed\n");
        return 1;
    }
#endif

    memset(&tImageServer, 0, sizeof(tImageServer));
    memset(&tChatServer, 0, sizeof(tChatServer));
    memset(&tAddr, 0, sizeof(tAddr));
    memset(&tCreate, 0, sizeof(tCreate));

    tImageServer.hListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if ( tImageServer.hListen == INVALID_SOCKET ) {
        fprintf(stderr, "create image listen socket failed\n");
        return 2;
    }
    tAddr.sin_family = AF_INET;
    tAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    tAddr.sin_port = htons(0);
    if ( bind(tImageServer.hListen, (struct sockaddr *)&tAddr, sizeof(tAddr)) == SOCKET_ERROR ) {
        fprintf(stderr, "bind image listen socket failed\n");
        return 3;
    }
    if ( listen(tImageServer.hListen, 1) == SOCKET_ERROR ) {
        fprintf(stderr, "listen image failed\n");
        return 4;
    }
    if ( getsockname(tImageServer.hListen, (struct sockaddr *)&tAddr, &iAddrLen) == SOCKET_ERROR ) {
        fprintf(stderr, "getsockname image failed\n");
        return 5;
    }
    tImageServer.uPort = ntohs(tAddr.sin_port);

    tChatServer.hListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if ( tChatServer.hListen == INVALID_SOCKET ) {
        fprintf(stderr, "create chat listen socket failed\n");
        return 6;
    }
    tAddr.sin_port = htons(0);
    if ( bind(tChatServer.hListen, (struct sockaddr *)&tAddr, sizeof(tAddr)) == SOCKET_ERROR ) {
        fprintf(stderr, "bind chat listen socket failed\n");
        return 7;
    }
    if ( listen(tChatServer.hListen, 1) == SOCKET_ERROR ) {
        fprintf(stderr, "listen chat failed\n");
        return 8;
    }
    if ( getsockname(tChatServer.hListen, (struct sockaddr *)&tAddr, &iAddrLen) == SOCKET_ERROR ) {
        fprintf(stderr, "getsockname chat failed\n");
        return 9;
    }
    tChatServer.uPort = ntohs(tAddr.sin_port);

    hImageThread = xrtThreadCreate((ptr)demo_image_server_thread, &tImageServer, 0);
    if ( !hImageThread ) {
        fprintf(stderr, "create image server thread failed\n");
        return 10;
    }
    hChatThread = xrtThreadCreate((ptr)demo_chat_server_thread, &tChatServer, 0);
    if ( !hChatThread ) {
        fprintf(stderr, "create chat server thread failed\n");
        return 11;
    }

    if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || !pRuntime ) {
        fprintf(stderr, "create runtime failed\n");
        return 12;
    }

    if ( xllm_register_ollama_native_adapter(pRuntime) != XRT_NET_OK ) {
        fprintf(stderr, "register built-in adapter failed\n");
        return 13;
    }

    (void)snprintf(sBaseUrl, sizeof(sBaseUrl), "http://127.0.0.1:%u", (unsigned)tChatServer.uPort);
    (void)snprintf(sImageUrl, sizeof(sImageUrl), "http://127.0.0.1:%u/image.png", (unsigned)tImageServer.uPort);
    xllm_profile_init(&tProfile);
    tProfile.sId = "ollama-image-url";
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
        return 14;
    }

    tCreate.sInitialProfileId = "ollama-image-url";
    tCreate.sSystemPrompt = "You are ollama url test";
    pLlm = xllm_create(pRuntime, &tCreate);
    if ( !pLlm ) {
        fprintf(stderr, "create llm failed\n");
        return 15;
    }

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "describe ollama image url") != XRT_NET_OK ) {
        fprintf(stderr, "turn add text failed\n");
        return 16;
    }
    if ( xllm_turn_add_image_url(&tTurn, sImageUrl, "image/png") != XRT_NET_OK ) {
        fprintf(stderr, "turn add image url failed\n");
        return 17;
    }

    iStatus = xllm_send(pLlm, &tTurn, NULL, &pResponse);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "send failed: %d\n", iStatus);
        return 18;
    }

    xrtThreadWait(hImageThread);
    xrtThreadDestroy(hImageThread);
    hImageThread = NULL;
    xrtThreadWait(hChatThread);
    xrtThreadDestroy(hChatThread);
    hChatThread = NULL;

    sText = xllm_response_get_text(pResponse);
    if ( !sText || strcmp(sText, "ollama image url ok") != 0 ) {
        fprintf(stderr, "unexpected response text: %s\n", sText ? sText : "(null)");
        return 19;
    }
    if ( tImageServer.iAcceptCount != 1 ||
         tImageServer.iImagePathCount != 1 ||
         tChatServer.iApiChatCount != 1 ||
         tChatServer.iStreamFalseCount != 1 ||
         tChatServer.iSystemCount != 1 ||
         tChatServer.iMultimodalModelCount != 1 ||
         tChatServer.iContentCount != 1 ||
         tChatServer.iImageArrayCount != 1 ||
         tChatServer.iBase64Count < 1 ||
         tChatServer.iTextValueCount < 1 ) {
        fprintf(
            stderr,
            "unexpected counters image_accept=%d image_path=%d api=%d stream=%d system=%d model=%d content=%d images=%d base64=%d text=%d\n",
            tImageServer.iAcceptCount,
            tImageServer.iImagePathCount,
            tChatServer.iApiChatCount,
            tChatServer.iStreamFalseCount,
            tChatServer.iSystemCount,
            tChatServer.iMultimodalModelCount,
            tChatServer.iContentCount,
            tChatServer.iImageArrayCount,
            tChatServer.iBase64Count,
            tChatServer.iTextValueCount
        );
        return 20;
    }

    printf("ok: %s\n", sText);

    xllm_response_free(pResponse);
    xllm_turn_reset(&tTurn);
    xllm_destroy(pLlm);
    xllm_runtime_destroy(pRuntime);
    if ( tImageServer.hListen != INVALID_SOCKET ) {
        closesocket(tImageServer.hListen);
    }
    if ( tChatServer.hListen != INVALID_SOCKET ) {
        closesocket(tChatServer.hListen);
    }
#if defined(_WIN32) || defined(_WIN64)
    WSACleanup();
#endif
    return 0;
}
