#include "xllm-session.h"

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

typedef struct {
    SOCKET hListen;
    uint16 uPort;
    int iAcceptCount;
    int iAuthHeaderCount;
    int iRequestPathCount;
} demo_upstream_server_state;

typedef struct {
    SOCKET hListen;
    uint16 uPort;
    uint16 uTargetPort;
    int iAcceptCount;
    int iGreetingCount;
    int iAuthMethodCount;
    int iUserPassAuthCount;
    int iConnectIpv4Count;
    int iConnectDomainCount;
    int iConnectPortCount;
    int iTunnelRequestCount;
} demo_socks5_server_state;

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

static bool demo_recv_exact(SOCKET hSocket, void *pData, size_t iLen)
{
    char *sData = (char *)pData;
    size_t iRead = 0u;

    while ( iRead < iLen ) {
        int iNow = recv(hSocket, sData + iRead, (int)(iLen - iRead), 0);
        if ( iNow <= 0 ) {
            return false;
        }
        iRead += (size_t)iNow;
    }
    return true;
}

static bool demo_read_http_message(SOCKET hSocket, char *sBuffer, size_t iCapacity, size_t *piLen)
{
    size_t iUsed = 0u;
    size_t iRequired = 0u;
    bool bHeaderParsed = false;

    if ( sBuffer == NULL || iCapacity < 8u ) {
        return false;
    }
    if ( piLen ) {
        *piLen = 0u;
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

    if ( piLen ) {
        *piLen = iUsed;
    }
    return bHeaderParsed;
}

static SOCKET demo_create_listen_socket(uint16 *puPort)
{
    SOCKET hListen = INVALID_SOCKET;
    struct sockaddr_in tAddr;
    socklen_t iAddrLen = sizeof(tAddr);

    hListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if ( hListen == INVALID_SOCKET ) {
        return INVALID_SOCKET;
    }

    memset(&tAddr, 0, sizeof(tAddr));
    tAddr.sin_family = AF_INET;
    tAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    tAddr.sin_port = htons(0);
    if ( bind(hListen, (struct sockaddr *)&tAddr, sizeof(tAddr)) == SOCKET_ERROR ) {
        closesocket(hListen);
        return INVALID_SOCKET;
    }
    if ( listen(hListen, 1) == SOCKET_ERROR ) {
        closesocket(hListen);
        return INVALID_SOCKET;
    }
    if ( getsockname(hListen, (struct sockaddr *)&tAddr, &iAddrLen) == SOCKET_ERROR ) {
        closesocket(hListen);
        return INVALID_SOCKET;
    }

    if ( puPort ) {
        *puPort = ntohs(tAddr.sin_port);
    }
    return hListen;
}

static SOCKET demo_connect_loopback(uint16 uPort)
{
    SOCKET hSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in tAddr;

    if ( hSocket == INVALID_SOCKET ) {
        return INVALID_SOCKET;
    }

    memset(&tAddr, 0, sizeof(tAddr));
    tAddr.sin_family = AF_INET;
    tAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    tAddr.sin_port = htons(uPort);
    if ( connect(hSocket, (struct sockaddr *)&tAddr, sizeof(tAddr)) == SOCKET_ERROR ) {
        closesocket(hSocket);
        return INVALID_SOCKET;
    }
    return hSocket;
}

static uint32 demo_upstream_server_thread(ptr pParam)
{
    demo_upstream_server_state *pState = (demo_upstream_server_state *)pParam;
    SOCKET hClient = INVALID_SOCKET;
    char sRequest[16384];
    size_t iRequestLen = 0u;
    const char *sBody =
        "{\"id\":\"chatcmpl-socks5\","
        "\"object\":\"chat.completion\","
        "\"model\":\"gpt-mock\","
        "\"choices\":[{\"index\":0,\"message\":{\"role\":\"assistant\",\"content\":\"echo: socks5 ok\"},\"finish_reason\":\"stop\"}],"
        "\"usage\":{\"prompt_tokens\":6,\"completion_tokens\":3}}";
    char sResponse[12288];
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
    if ( demo_read_http_message(hClient, sRequest, sizeof(sRequest), &iRequestLen) ) {
        (void)iRequestLen;
        if ( strstr(sRequest, "Authorization: Bearer test-key\r\n") != NULL ) {
            ++pState->iAuthHeaderCount;
        }
        if ( strstr(sRequest, "POST /v1/chat/completions HTTP/1.1\r\n") != NULL ) {
            ++pState->iRequestPathCount;
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

static uint32 demo_socks5_server_thread(ptr pParam)
{
    demo_socks5_server_state *pState = (demo_socks5_server_state *)pParam;
    SOCKET hClient = INVALID_SOCKET;
    SOCKET hTarget = INVALID_SOCKET;
    char aGreeting[4];
    unsigned char aAuthHeader[2];
    unsigned char aAuthUserLen = 0u;
    unsigned char aAuthPassLen = 0u;
    char aAuthData[256];
    unsigned char aConnectHeader[4];
    unsigned char aAddrBuf[260];
    unsigned char aPortBuf[2];
    char sTunnelRequest[16384];
    char sForwardBuffer[8192];
    size_t iTunnelLen = 0u;
    uint16 uConnectPort;

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

    if ( !demo_recv_exact(hClient, aGreeting, 4u) ) {
        goto fail;
    }
    if ( (unsigned char)aGreeting[0] == 0x05 &&
         (unsigned char)aGreeting[1] == 0x02 &&
         (unsigned char)aGreeting[2] == 0x00 &&
         (unsigned char)aGreeting[3] == 0x02 ) {
        ++pState->iGreetingCount;
        ++pState->iAuthMethodCount;
    }
    if ( !demo_send_all(hClient, "\x05\x02", 2u) ) {
        goto fail;
    }

    if ( !demo_recv_exact(hClient, aAuthHeader, 2u) ) {
        goto fail;
    }
    aAuthUserLen = aAuthHeader[1];
    if ( (size_t)aAuthUserLen + 1u >= sizeof(aAuthData) || !demo_recv_exact(hClient, aAuthData, aAuthUserLen + 1u) ) {
        goto fail;
    }
    aAuthPassLen = (unsigned char)aAuthData[aAuthUserLen];
    if ( (size_t)aAuthUserLen + 1u + (size_t)aAuthPassLen > sizeof(aAuthData) ||
         !demo_recv_exact(hClient, aAuthData + aAuthUserLen + 1u, aAuthPassLen) ) {
        goto fail;
    }
    if ( aAuthHeader[0] == 0x01 &&
         aAuthUserLen == 4u &&
         memcmp(aAuthData, "demo", 4u) == 0 &&
         aAuthPassLen == 6u &&
         memcmp(aAuthData + aAuthUserLen + 1u, "secret", 6u) == 0 ) {
        ++pState->iUserPassAuthCount;
    }
    if ( !demo_send_all(hClient, "\x01\x00", 2u) ) {
        goto fail;
    }

    if ( !demo_recv_exact(hClient, aConnectHeader, 4u) ) {
        goto fail;
    }
    if ( aConnectHeader[0] != 0x05 || aConnectHeader[1] != 0x01 || aConnectHeader[2] != 0x00 ) {
        goto fail;
    }
    if ( aConnectHeader[3] == 0x01 ) {
        if ( !demo_recv_exact(hClient, aAddrBuf, 4u) ) {
            goto fail;
        }
        if ( aAddrBuf[0] == 127u && aAddrBuf[1] == 0u && aAddrBuf[2] == 0u && aAddrBuf[3] == 1u ) {
            ++pState->iConnectIpv4Count;
        }
    } else if ( aConnectHeader[3] == 0x03 ) {
        unsigned char iHostLen;

        if ( !demo_recv_exact(hClient, aAddrBuf, 1u) ) {
            goto fail;
        }
        iHostLen = aAddrBuf[0];
        if ( iHostLen == 0u || (size_t)iHostLen > sizeof(aAddrBuf) || !demo_recv_exact(hClient, aAddrBuf, iHostLen) ) {
            goto fail;
        }
        if ( iHostLen == strlen("127.0.0.1") && memcmp(aAddrBuf, "127.0.0.1", iHostLen) == 0 ) {
            ++pState->iConnectDomainCount;
        }
    } else {
        goto fail;
    }
    if ( !demo_recv_exact(hClient, aPortBuf, 2u) ) {
        goto fail;
    }
    uConnectPort = ((uint16)aPortBuf[0] << 8) | (uint16)aPortBuf[1];
    if ( uConnectPort == pState->uTargetPort ) {
        ++pState->iConnectPortCount;
    }

    hTarget = demo_connect_loopback(pState->uTargetPort);
    if ( hTarget == INVALID_SOCKET ) {
        goto fail;
    }
    {
        unsigned char aConnectResp[] = {0x05, 0x00, 0x00, 0x01, 127u, 0u, 0u, 1u, aPortBuf[0], aPortBuf[1]};
        if ( !demo_send_all(hClient, aConnectResp, sizeof(aConnectResp)) ) {
            goto fail;
        }
    }

    if ( !demo_read_http_message(hClient, sTunnelRequest, sizeof(sTunnelRequest), &iTunnelLen) ) {
        goto fail;
    }
    if ( strstr(sTunnelRequest, "POST /v1/chat/completions HTTP/1.1\r\n") != NULL ) {
        ++pState->iTunnelRequestCount;
    }
    if ( !demo_send_all(hTarget, sTunnelRequest, iTunnelLen) ) {
        goto fail;
    }

    for ( ; ; ) {
        int iRecv = recv(hTarget, sForwardBuffer, (int)sizeof(sForwardBuffer), 0);
        if ( iRecv == 0 ) {
            break;
        }
        if ( iRecv < 0 ) {
            goto fail;
        }
        if ( !demo_send_all(hClient, sForwardBuffer, (size_t)iRecv) ) {
            goto fail;
        }
    }

    closesocket(hTarget);
    closesocket(hClient);
    closesocket(pState->hListen);
    pState->hListen = INVALID_SOCKET;
    return 0u;

fail:
    if ( hTarget != INVALID_SOCKET ) {
        closesocket(hTarget);
    }
    if ( hClient != INVALID_SOCKET ) {
        closesocket(hClient);
    }
    closesocket(pState->hListen);
    pState->hListen = INVALID_SOCKET;
    return 3u;
}

int main(void)
{
#if defined(_WIN32) || defined(_WIN64)
    WSADATA tWsaData;
#endif
    demo_upstream_server_state tUpstream;
    demo_socks5_server_state tProxy;
    xthread hUpstreamThread = NULL;
    xthread hProxyThread = NULL;
    xllm_runtime *pRuntime = NULL;
    xllm *pLlm = NULL;
    xllm_response *pResponse = NULL;
    xllm_profile tProfile;
    xllm_create_options tCreate;
    xllm_turn tTurn;
    char sBaseUrl[128];
    const char *sText;
    int iExitCode = 0;

#if defined(_WIN32) || defined(_WIN64)
    if ( WSAStartup(MAKEWORD(2, 2), &tWsaData) != 0 ) {
        fprintf(stderr, "winsock startup failed\n");
        return 1;
    }
#endif

    memset(&tUpstream, 0, sizeof(tUpstream));
    memset(&tProxy, 0, sizeof(tProxy));
    memset(&tCreate, 0, sizeof(tCreate));

    tUpstream.hListen = demo_create_listen_socket(&tUpstream.uPort);
    if ( tUpstream.hListen == INVALID_SOCKET ) {
        fprintf(stderr, "create upstream listen socket failed\n");
        iExitCode = 2;
        goto cleanup;
    }
    tProxy.hListen = demo_create_listen_socket(&tProxy.uPort);
    if ( tProxy.hListen == INVALID_SOCKET ) {
        fprintf(stderr, "create socks5 listen socket failed\n");
        iExitCode = 3;
        goto cleanup;
    }
    tProxy.uTargetPort = tUpstream.uPort;

    hUpstreamThread = xrtThreadCreate((ptr)demo_upstream_server_thread, &tUpstream, 0);
    if ( !hUpstreamThread ) {
        fprintf(stderr, "create upstream thread failed\n");
        iExitCode = 4;
        goto cleanup;
    }
    hProxyThread = xrtThreadCreate((ptr)demo_socks5_server_thread, &tProxy, 0);
    if ( !hProxyThread ) {
        fprintf(stderr, "create socks5 thread failed\n");
        iExitCode = 5;
        goto cleanup;
    }

    if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || pRuntime == NULL ) {
        fprintf(stderr, "create runtime failed\n");
        iExitCode = 6;
        goto cleanup;
    }
    if ( xllm_register_openai_compat_adapter(pRuntime) != XRT_NET_OK ) {
        fprintf(stderr, "register openai-compatible adapter failed\n");
        iExitCode = 7;
        goto cleanup;
    }

    (void)snprintf(sBaseUrl, sizeof(sBaseUrl), "http://127.0.0.1:%u/v1", (unsigned)tUpstream.uPort);
    xllm_profile_init(&tProfile);
    tProfile.sId = "openai-proxy-socks5";
    tProfile.sProvider = "openai";
    tProfile.sAdapter = XLLM_ADAPTER_OPENAI_COMPAT;
    tProfile.sBaseUrl = sBaseUrl;
    tProfile.tAuth.eKind = XLLM_AUTH_BEARER;
    tProfile.tAuth.sSecret = "test-key";
    tProfile.tModels.tText.sModelId = "gpt-mock";
    tProfile.tTransport.eProxyKind = XLLM_PROXY_SOCKS5;
    tProfile.tTransport.sProxyHost = "127.0.0.1";
    tProfile.tTransport.tProxyPort.bSet = true;
    tProfile.tTransport.tProxyPort.iValue = tProxy.uPort;
    tProfile.tTransport.sProxyUser = "demo";
    tProfile.tTransport.sProxyPass = "secret";
    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "register profile failed\n");
        iExitCode = 8;
        goto cleanup;
    }

    tCreate.sInitialProfileId = "openai-proxy-socks5";
    pLlm = xllm_create(pRuntime, &tCreate);
    if ( pLlm == NULL ) {
        fprintf(stderr, "create llm failed\n");
        iExitCode = 9;
        goto cleanup;
    }

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "hello through socks5") != XRT_NET_OK ) {
        fprintf(stderr, "turn add text failed\n");
        iExitCode = 10;
        goto cleanup;
    }

    if ( xllm_send(pLlm, &tTurn, NULL, &pResponse) != XRT_NET_OK ) {
        fprintf(stderr, "send failed\n");
        iExitCode = 11;
        goto cleanup;
    }

    xrtThreadWait(hProxyThread);
    xrtThreadDestroy(hProxyThread);
    hProxyThread = NULL;
    xrtThreadWait(hUpstreamThread);
    xrtThreadDestroy(hUpstreamThread);
    hUpstreamThread = NULL;

    sText = xllm_response_get_text(pResponse);
    if ( sText == NULL || strcmp(sText, "echo: socks5 ok") != 0 ) {
        fprintf(stderr, "unexpected response text: %s\n", sText ? sText : "(null)");
        iExitCode = 12;
        goto cleanup;
    }

    if ( tProxy.iAcceptCount != 1 ||
         tProxy.iGreetingCount != 1 ||
         tProxy.iAuthMethodCount != 1 ||
         tProxy.iUserPassAuthCount != 1 ||
         (tProxy.iConnectIpv4Count + tProxy.iConnectDomainCount) != 1 ||
         tProxy.iConnectPortCount != 1 ||
         tProxy.iTunnelRequestCount != 1 ||
         tUpstream.iAcceptCount != 1 ||
         tUpstream.iAuthHeaderCount != 1 ||
         tUpstream.iRequestPathCount != 1 ) {
        fprintf(
            stderr,
            "unexpected socks5 counters: proxy_accept=%d greeting=%d auth_method=%d userpass=%d connect_ipv4=%d connect_domain=%d connect_port=%d tunnel=%d upstream_accept=%d upstream_auth=%d upstream_path=%d\n",
            tProxy.iAcceptCount,
            tProxy.iGreetingCount,
            tProxy.iAuthMethodCount,
            tProxy.iUserPassAuthCount,
            tProxy.iConnectIpv4Count,
            tProxy.iConnectDomainCount,
            tProxy.iConnectPortCount,
            tProxy.iTunnelRequestCount,
            tUpstream.iAcceptCount,
            tUpstream.iAuthHeaderCount,
            tUpstream.iRequestPathCount
        );
        iExitCode = 13;
        goto cleanup;
    }

    printf("ok: %s\n", sText);

cleanup:
    xllm_response_free(pResponse);
    xllm_turn_reset(&tTurn);
    xllm_destroy(pLlm);
    xllm_runtime_destroy(pRuntime);

    if ( hProxyThread ) {
        if ( tProxy.hListen != INVALID_SOCKET ) {
            closesocket(tProxy.hListen);
            tProxy.hListen = INVALID_SOCKET;
        }
        xrtThreadWait(hProxyThread);
        xrtThreadDestroy(hProxyThread);
    } else if ( tProxy.hListen != INVALID_SOCKET ) {
        closesocket(tProxy.hListen);
        tProxy.hListen = INVALID_SOCKET;
    }

    if ( hUpstreamThread ) {
        if ( tUpstream.hListen != INVALID_SOCKET ) {
            closesocket(tUpstream.hListen);
            tUpstream.hListen = INVALID_SOCKET;
        }
        xrtThreadWait(hUpstreamThread);
        xrtThreadDestroy(hUpstreamThread);
    } else if ( tUpstream.hListen != INVALID_SOCKET ) {
        closesocket(tUpstream.hListen);
        tUpstream.hListen = INVALID_SOCKET;
    }

#if defined(_WIN32) || defined(_WIN64)
    WSACleanup();
#endif
    return iExitCode;
}
