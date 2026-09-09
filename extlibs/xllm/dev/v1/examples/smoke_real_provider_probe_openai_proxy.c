#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#include <direct.h>
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
    int iAuthHeaderCount;
    int iRequestPathCount;
} smoke_upstream_server_state;

typedef struct {
    SOCKET hListen;
    uint16 uPort;
    uint16 uTargetPort;
    int iAcceptCount;
    int iConnectLineCount;
    int iHostHeaderCount;
    int iProxyAuthCount;
    int iTunnelRequestCount;
} smoke_proxy_server_state;

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

static bool smoke_read_http_message(SOCKET hSocket, char *sBuffer, size_t iCapacity, size_t *piLen)
{
    size_t iUsed = 0u;
    size_t iRequired = 0u;
    bool bHeaderParsed = false;

    if ( sBuffer == NULL || iCapacity < 8u ) {
        return false;
    }
    if ( piLen != NULL ) {
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

    if ( piLen != NULL ) {
        *piLen = iUsed;
    }
    return bHeaderParsed;
}

static SOCKET smoke_create_listen_socket(uint16 *puPort)
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

    if ( puPort != NULL ) {
        *puPort = ntohs(tAddr.sin_port);
    }
    return hListen;
}

static SOCKET smoke_connect_loopback(uint16 uPort)
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

static uint32 smoke_upstream_server_thread(ptr pParam)
{
    smoke_upstream_server_state *pState = (smoke_upstream_server_state *)pParam;
    SOCKET hClient = INVALID_SOCKET;
    char sRequest[16384];
    size_t iRequestLen = 0u;
    const char *sBody =
        "{\"id\":\"chatcmpl-probe-proxy\","
        "\"object\":\"chat.completion\","
        "\"model\":\"gpt-test\","
        "\"choices\":[{\"index\":0,\"message\":{\"role\":\"assistant\",\"content\":\"pong via proxy\"},\"finish_reason\":\"stop\"}],"
        "\"usage\":{\"prompt_tokens\":1,\"completion_tokens\":2,\"total_tokens\":3}}";
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
    if ( smoke_read_http_message(hClient, sRequest, sizeof(sRequest), &iRequestLen) ) {
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
        (void)smoke_send_all(hClient, sResponse, (size_t)iRespLen);
    }

    closesocket(hClient);
    closesocket(pState->hListen);
    pState->hListen = INVALID_SOCKET;
    return 0u;
}

static uint32 smoke_proxy_server_thread(ptr pParam)
{
    smoke_proxy_server_state *pState = (smoke_proxy_server_state *)pParam;
    SOCKET hClient = INVALID_SOCKET;
    SOCKET hTarget = INVALID_SOCKET;
    char sConnectRequest[8192];
    char sTunnelRequest[16384];
    char sForwardBuffer[8192];
    size_t iConnectLen = 0u;
    size_t iTunnelLen = 0u;
    char sExpectedTarget[64];
    char sExpectedConnectLine[96];
    char sExpectedHostLine[80];

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
    (void)snprintf(sExpectedTarget, sizeof(sExpectedTarget), "127.0.0.1:%u", (unsigned)pState->uTargetPort);
    (void)snprintf(sExpectedConnectLine, sizeof(sExpectedConnectLine), "CONNECT %s HTTP/1.1\r\n", sExpectedTarget);
    (void)snprintf(sExpectedHostLine, sizeof(sExpectedHostLine), "Host: %s\r\n", sExpectedTarget);

    if ( !smoke_read_http_message(hClient, sConnectRequest, sizeof(sConnectRequest), &iConnectLen) ) {
        closesocket(hClient);
        closesocket(pState->hListen);
        pState->hListen = INVALID_SOCKET;
        return 3u;
    }
    (void)iConnectLen;
    if ( strstr(sConnectRequest, sExpectedConnectLine) != NULL ) {
        ++pState->iConnectLineCount;
    }
    if ( strstr(sConnectRequest, sExpectedHostLine) != NULL ) {
        ++pState->iHostHeaderCount;
    }
    if ( strstr(sConnectRequest, "Proxy-Authorization: Basic ZGVtbzpzZWNyZXQ=\r\n") != NULL ) {
        ++pState->iProxyAuthCount;
    }

    hTarget = smoke_connect_loopback(pState->uTargetPort);
    if ( hTarget == INVALID_SOCKET ) {
        closesocket(hClient);
        closesocket(pState->hListen);
        pState->hListen = INVALID_SOCKET;
        return 4u;
    }

    if ( !smoke_send_all(hClient, "HTTP/1.1 200 Connection Established\r\n\r\n", 39u) ) {
        closesocket(hTarget);
        closesocket(hClient);
        closesocket(pState->hListen);
        pState->hListen = INVALID_SOCKET;
        return 5u;
    }

    if ( !smoke_read_http_message(hClient, sTunnelRequest, sizeof(sTunnelRequest), &iTunnelLen) ) {
        closesocket(hTarget);
        closesocket(hClient);
        closesocket(pState->hListen);
        pState->hListen = INVALID_SOCKET;
        return 6u;
    }
    if ( strstr(sTunnelRequest, "POST /v1/chat/completions HTTP/1.1\r\n") != NULL ) {
        ++pState->iTunnelRequestCount;
    }
    if ( !smoke_send_all(hTarget, sTunnelRequest, iTunnelLen) ) {
        closesocket(hTarget);
        closesocket(hClient);
        closesocket(pState->hListen);
        pState->hListen = INVALID_SOCKET;
        return 7u;
    }

    for ( ; ; ) {
        int iRecv = recv(hTarget, sForwardBuffer, (int)sizeof(sForwardBuffer), 0);
        if ( iRecv == 0 ) {
            break;
        }
        if ( iRecv < 0 ) {
            closesocket(hTarget);
            closesocket(hClient);
            closesocket(pState->hListen);
            pState->hListen = INVALID_SOCKET;
            return 8u;
        }
        if ( !smoke_send_all(hClient, sForwardBuffer, (size_t)iRecv) ) {
            closesocket(hTarget);
            closesocket(hClient);
            closesocket(pState->hListen);
            pState->hListen = INVALID_SOCKET;
            return 9u;
        }
    }

    closesocket(hTarget);
    closesocket(hClient);
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

int main(void)
{
#if defined(_WIN32) || defined(_WIN64)
    WSADATA tWsaData;
#endif
    smoke_upstream_server_state tUpstream;
    smoke_proxy_server_state tProxy;
    xthread hUpstreamThread = NULL;
    xthread hProxyThread = NULL;
    char sBaseUrl[256];
    char sProxyPort[16];
    int iProbeExitCode = 0;
    int iExitCode = 0;

#if defined(_WIN32) || defined(_WIN64)
    if ( WSAStartup(MAKEWORD(2, 2), &tWsaData) != 0 ) {
        fprintf(stderr, "winsock startup failed\n");
        return 1;
    }
#endif

    memset(&tUpstream, 0, sizeof(tUpstream));
    memset(&tProxy, 0, sizeof(tProxy));

    tUpstream.hListen = smoke_create_listen_socket(&tUpstream.uPort);
    if ( tUpstream.hListen == INVALID_SOCKET ) {
        fprintf(stderr, "create upstream listen socket failed\n");
        iExitCode = 2;
        goto cleanup;
    }
    tProxy.hListen = smoke_create_listen_socket(&tProxy.uPort);
    if ( tProxy.hListen == INVALID_SOCKET ) {
        fprintf(stderr, "create proxy listen socket failed\n");
        iExitCode = 3;
        goto cleanup;
    }
    tProxy.uTargetPort = tUpstream.uPort;

    hUpstreamThread = xrtThreadCreate((ptr)smoke_upstream_server_thread, &tUpstream, 0);
    if ( !hUpstreamThread ) {
        fprintf(stderr, "create upstream thread failed\n");
        iExitCode = 4;
        goto cleanup;
    }
    hProxyThread = xrtThreadCreate((ptr)smoke_proxy_server_thread, &tProxy, 0);
    if ( !hProxyThread ) {
        fprintf(stderr, "create proxy thread failed\n");
        iExitCode = 5;
        goto cleanup;
    }

    snprintf(sBaseUrl, sizeof(sBaseUrl), "http://127.0.0.1:%u/v1", (unsigned)tUpstream.uPort);
    snprintf(sProxyPort, sizeof(sProxyPort), "%u", (unsigned)tProxy.uPort);
    if ( smoke_setenv_cstr("XLLM_REAL_ADAPTER", "openai_compat") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_BASE_URL", sBaseUrl) != 0 ||
         smoke_setenv_cstr("XLLM_REAL_MODEL", "gpt-test") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_API_KEY", "test-key") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_AUTH_KIND", "bearer") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_ENABLE_LOG", "0") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_ENABLE_TRACE", "0") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_EXPECT_TEXT_CONTAINS", "pong via proxy") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_PROXY_KIND", "http_connect") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_PROXY_HOST", "127.0.0.1") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_PROXY_PORT", sProxyPort) != 0 ||
         smoke_setenv_cstr("XLLM_REAL_PROXY_USER", "demo") != 0 ||
         smoke_setenv_cstr("XLLM_REAL_PROXY_PASS", "secret") != 0 ) {
        fprintf(stderr, "set environment variables failed\n");
        iExitCode = 6;
        goto cleanup;
    }

    iProbeExitCode = smoke_real_provider_probe_entry();
    if ( iProbeExitCode != 0 ) {
        fprintf(stderr, "probe failed unexpectedly: %d\n", iProbeExitCode);
        iExitCode = 7;
        goto cleanup;
    }

    if ( hProxyThread ) {
        xrtThreadWait(hProxyThread);
        xrtThreadDestroy(hProxyThread);
        hProxyThread = NULL;
    }
    if ( hUpstreamThread ) {
        xrtThreadWait(hUpstreamThread);
        xrtThreadDestroy(hUpstreamThread);
        hUpstreamThread = NULL;
    }

    if ( tProxy.iAcceptCount != 1 ||
         tProxy.iConnectLineCount != 1 ||
         tProxy.iHostHeaderCount != 1 ||
         tProxy.iProxyAuthCount != 1 ||
         tProxy.iTunnelRequestCount != 1 ||
         tUpstream.iAcceptCount != 1 ||
         tUpstream.iAuthHeaderCount != 1 ||
         tUpstream.iRequestPathCount != 1 ) {
        fprintf(
            stderr,
            "unexpected proxy probe counters: proxy_accept=%d connect=%d host=%d proxy_auth=%d tunnel=%d upstream_accept=%d upstream_auth=%d upstream_path=%d\n",
            tProxy.iAcceptCount,
            tProxy.iConnectLineCount,
            tProxy.iHostHeaderCount,
            tProxy.iProxyAuthCount,
            tProxy.iTunnelRequestCount,
            tUpstream.iAcceptCount,
            tUpstream.iAuthHeaderCount,
            tUpstream.iRequestPathCount
        );
        iExitCode = 8;
        goto cleanup;
    }

    printf("smoke_real_provider_probe_openai_proxy ok\n");

cleanup:
    smoke_setenv_cstr("XLLM_REAL_ADAPTER", "");
    smoke_setenv_cstr("XLLM_REAL_BASE_URL", "");
    smoke_setenv_cstr("XLLM_REAL_MODEL", "");
    smoke_setenv_cstr("XLLM_REAL_API_KEY", "");
    smoke_setenv_cstr("XLLM_REAL_AUTH_KIND", "");
    smoke_setenv_cstr("XLLM_REAL_ENABLE_LOG", "");
    smoke_setenv_cstr("XLLM_REAL_ENABLE_TRACE", "");
    smoke_setenv_cstr("XLLM_REAL_EXPECT_TEXT_CONTAINS", "");
    smoke_setenv_cstr("XLLM_REAL_PROXY_KIND", "");
    smoke_setenv_cstr("XLLM_REAL_PROXY_HOST", "");
    smoke_setenv_cstr("XLLM_REAL_PROXY_PORT", "");
    smoke_setenv_cstr("XLLM_REAL_PROXY_USER", "");
    smoke_setenv_cstr("XLLM_REAL_PROXY_PASS", "");

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
