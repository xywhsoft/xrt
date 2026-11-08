#include "../internal/xllm_internal.h"
#include "../internal/xllm_proxy_future.h"
#include <stdio.h>

/* Engine-driven transport.
 *
 * The exchange runs as a watch-chained state machine on the client engine's
 * workers: dial -> send (header, body) -> read (head + body pumped through
 * the incremental HTTP/1.1 parser). Every asynchronous operation future gets
 * the call's embedded watch; its notify callback advances the machine. A
 * per-call engine timer enforces the deadline and a cancel watch aborts the
 * streams, so no thread is ever dedicated to a call. The call promise
 * reaches its terminal state exactly once from xllm__transport_finish. */

struct xllm_connection
{
    bool bTls;
    xnetstream *pTcp;
    xtlsstream *pTls;
    xfuture *pIdleRead;
    uint64_t uPooledAtMs;
};

/* Sentinel returned by step helpers: continue to the next step inline
 * instead of arming a watch on a new pending future. */
#define XLLM_STEP_CONTINUE ((xfuture *)1)

static xstrview xllm__sv(const char *sText)
{
    xstrview tView;
    tView.Data = sText ? sText : "";
    tView.Size = sText ? strlen(sText) : 0u;
    return tView;
}

static xbytesview xllm__bv(const void *pData, size_t iSize)
{
    xbytesview tView;
    tView.Data = (const uint8 *)pData;
    tView.Size = iSize;
    return tView;
}

static uint64_t xllm__clock_ms(void) { return xllmClockUs() / UINT64_C(1000); }

static xllm_error_stage xllm__transport_stage(const char *sPhase, const char *sName)
{
    if ((sName && strstr(sName, "proxy")) || (sPhase && strstr(sPhase, "proxy")))
        return XLLM_ERROR_STAGE_PROXY;
    if ((sName && strstr(sName, "tls")) || (sPhase && strstr(sPhase, "tls")))
        return XLLM_ERROR_STAGE_TLS;
    if (sPhase && (strcmp(sPhase, "resolve") == 0 || strcmp(sPhase, "dns") == 0))
        return XLLM_ERROR_STAGE_RESOLVE;
    if (sPhase && strcmp(sPhase, "connect") == 0)
        return XLLM_ERROR_STAGE_CONNECT;
    if (sPhase && (strcmp(sPhase, "send") == 0 || strcmp(sPhase, "request") == 0))
        return XLLM_ERROR_STAGE_WRITE;
    if (sPhase && (strcmp(sPhase, "receive") == 0 || strcmp(sPhase, "body") == 0))
        return XLLM_ERROR_STAGE_READ;
    if (sPhase && strcmp(sPhase, "headers") == 0)
        return XLLM_ERROR_STAGE_HTTP;
    return XLLM_ERROR_STAGE_WAIT;
}

static void xllm__transport_fail(xllm_call *pCall, const char *sPhase,
                                 xllm_transport_result eResult, const char *sName,
                                 const xerror *pError)
{
    const char *sOperation;
    const char *sMessage;
    if (!pCall)
        return;
    pCall->tHttpDiagnostics.eResult = eResult;
    xllm__copy_text(pCall->tHttpDiagnostics.sPhase, sizeof(pCall->tHttpDiagnostics.sPhase), sPhase);
    xllm__copy_text(pCall->tHttpDiagnostics.sError, sizeof(pCall->tHttpDiagnostics.sError), sName);
    if (pError)
    {
        pCall->tHttpDiagnostics.iSystemError = xrtErrorSystemCode(pError);
    }
    /* A parser/hook/limit failure may make the body consumer stop the
     * transport. Preserve that primary cause instead of replacing it with
     * the secondary callback cancellation. */
    if (pCall->tError.tCause.eDomain != XLLM_ERROR_DOMAIN_NONE &&
        pCall->tError.tCause.eDomain != XLLM_ERROR_DOMAIN_TRANSPORT)
    {
        return;
    }
    sOperation = pError ? xrtErrorOperation(pError) : NULL;
    if (!sOperation || !sOperation[0])
    {
        sOperation = sPhase;
    }
    sMessage = pError ? xrtErrorMessage(pError) : NULL;
    if (!sMessage || !sMessage[0])
    {
        sMessage = sName;
    }
    xllm__error_cause_set(
        &pCall->tError, XLLM_ERROR_DOMAIN_TRANSPORT, xllm__transport_stage(sPhase, sName),
        pError ? (int32_t)xrtErrorKind(pError) : 0, pError ? xrtErrorCode(pError) : 0,
        pError ? xrtErrorSystemCode(pError) : 0, sOperation, sMessage);
}

static void xllm__connection_close(xllm_connection *pConnection)
{
    if (!pConnection)
        return;
    if (pConnection->pIdleRead)
    {
        (void)xrtFutureCancel(pConnection->pIdleRead);
        xrtFutureDestroy(pConnection->pIdleRead);
        pConnection->pIdleRead = NULL;
    }
    if (pConnection->bTls && pConnection->pTls)
    {
        xfuture *pClose;
        (void)xrtTlsStreamAbort(pConnection->pTls);
        pClose = xrtTlsStreamWaitAsync(pConnection->pTls, XTLS_STREAM_WAIT_CLOSE);
        if (pClose)
        {
            (void)xrtFutureWaitFor(pClose, 1000);
            xrtFutureDestroy(pClose);
        }
        xrtTlsStreamDestroy(pConnection->pTls);
    }
    else if (pConnection->pTcp)
    {
        (void)xrtNetStreamAbort(pConnection->pTcp);
        (void)xrtNetStreamWait(pConnection->pTcp, XNET_STREAM_WAIT_CLOSE, 1000, NULL);
        xrtNetStreamDestroy(pConnection->pTcp);
    }
    xllm__free(pConnection);
}

static bool xllm__connection_open(const xllm_connection *pConnection)
{
    if (!pConnection)
        return false;
    if (pConnection->bTls)
    {
        return pConnection->pTls && xrtTlsStreamState(pConnection->pTls) == XTLS_STREAM_OPEN;
    }
    return pConnection->pTcp && xrtNetStreamState(pConnection->pTcp) == XNET_STREAM_OPEN;
}

static bool xllm__connection_idle_expired(const xllm_client *pClient,
                                          const xllm_connection *pConnection, uint64_t uNowMs)
{
    return pClient->uIdleTimeoutMs != 0u && pConnection->uPooledAtMs != 0u &&
           uNowMs >= pConnection->uPooledAtMs &&
           uNowMs - pConnection->uPooledAtMs >= pClient->uIdleTimeoutMs;
}

static bool xllm__connection_idle_arm(xllm_connection *pConnection)
{
    if (!xllm__connection_open(pConnection) || pConnection->pIdleRead)
    {
        return false;
    }
    pConnection->pIdleRead = pConnection->bTls
                                 ? xrtTlsStreamWaitAsync(pConnection->pTls, XTLS_STREAM_WAIT_READ)
                                 : xrtNetStreamWaitAsync(pConnection->pTcp, XNET_STREAM_WAIT_READ);
    return pConnection->pIdleRead != NULL;
}

/* Stop the idle read sentinel before handing the stream to a request. If a
 * peer-close/read event won the race, the connection is contaminated and is
 * discarded. Cancellation normally resolves inline; the bounded wait covers
 * a worker that is finishing publication. */
static bool xllm__connection_idle_stop(xllm_connection *pConnection)
{
    xfuture *pIdle;
    xfuturestate eState;
    bool bQuiet;
    if (!pConnection || !(pIdle = pConnection->pIdleRead))
        return false;
    pConnection->pIdleRead = NULL;
    if (xrtFutureState(pIdle) == XFUTURE_PENDING)
    {
        (void)xrtFutureCancel(pIdle);
        (void)xrtFutureWaitFor(pIdle, 1000);
    }
    eState = xrtFutureState(pIdle);
    bQuiet = eState == XFUTURE_CANCELLED;
    xrtFutureDestroy(pIdle);
    return bQuiet;
}

static xllm_connection *xllm__connection_take(xllm_client *pClient)
{
    xllm_connection *pConnection;
    uint64_t uNowMs = xllm__clock_ms();
    if (!pClient || !pClient->pConnectionMutex)
        return NULL;
    for (;;)
    {
        pConnection = NULL;
        if (!xrtMutexLock(pClient->pConnectionMutex))
            return NULL;
        if (pClient->uIdleConnectionCount)
        {
            pConnection = pClient->pIdleConnections[--pClient->uIdleConnectionCount];
            pClient->pIdleConnections[pClient->uIdleConnectionCount] = NULL;
        }
        (void)xrtMutexUnlock(pClient->pConnectionMutex);
        if (!pConnection)
            return NULL;
        if (xllm__connection_open(pConnection) &&
            !xllm__connection_idle_expired(pClient, pConnection, uNowMs) &&
            xllm__connection_idle_stop(pConnection))
        {
            return pConnection;
        }
        xllm__connection_close(pConnection);
    }
}

static void xllm__connection_release(xllm_client *pClient, xllm_connection *pConnection,
                                     bool bReusable)
{
    if (!pConnection)
        return;
    if (bReusable && pClient && pClient->pConnectionMutex && pClient->uMaxIdleConnections &&
        xllm__connection_idle_arm(pConnection) && xrtMutexLock(pClient->pConnectionMutex))
    {
        if (pClient->uIdleConnectionCount < pClient->uMaxIdleConnections)
        {
            pConnection->uPooledAtMs = xllm__clock_ms();
            pClient->pIdleConnections[pClient->uIdleConnectionCount++] = pConnection;
            pConnection = NULL;
        }
        (void)xrtMutexUnlock(pClient->pConnectionMutex);
    }
    xllm__connection_close(pConnection);
}

/* ------------------------------------------------------------------ */
/* Request header assembly (once per call, on the starting thread)     */
/* ------------------------------------------------------------------ */

static bool xllm__transport_build_header(xllm_call *pCall)
{
    xllm_auth_header tAuth[XLLM_MAX_AUTH_HEADERS];
    size_t iAuth = 0u;
    size_t iFieldCount = 0u;
    size_t iHeaderSize = 0u;
    size_t iBodySize = strlen(pCall->sRequestBody);
    size_t i;
    char sLength[32];
    xhttpfield *tFields;
    bool bOk = false;
    (void)snprintf(sLength, sizeof(sLength), "%llu", (unsigned long long)iBodySize);
    if (pCall->pClient->sApiKey[0])
    {
        iAuth = pCall->pDialect->BuildAuth(pCall->pClient, tAuth, XLLM_MAX_AUTH_HEADERS);
    }
    tFields =
        (xhttpfield *)xllm__malloc(sizeof(*tFields) * (6u + iAuth + pCall->iExtraHeaderCount));
    if (!tFields)
        goto done;
    tFields[iFieldCount++] = (xhttpfield){xllm__sv("Host"), xllm__sv(pCall->pClient->sHostHeader)};
    tFields[iFieldCount++] =
        (xhttpfield){xllm__sv("Accept"),
                     xllm__sv(pCall->bStreamWanted ? "text/event-stream" : "application/json")};
    tFields[iFieldCount++] = (xhttpfield){xllm__sv("Content-Type"), xllm__sv("application/json")};
    tFields[iFieldCount++] = (xhttpfield){xllm__sv("Content-Length"), xllm__sv(sLength)};
    tFields[iFieldCount++] =
        (xhttpfield){xllm__sv("User-Agent"), xllm__sv(pCall->pClient->sUserAgent)};
    tFields[iFieldCount++] = (xhttpfield){xllm__sv("Connection"), xllm__sv("keep-alive")};
    for (i = 0u; i < iAuth; ++i)
    {
        tFields[iFieldCount++] = (xhttpfield){xllm__sv(tAuth[i].sName), xllm__sv(tAuth[i].sValue)};
    }
    for (i = 0u; i < pCall->iExtraHeaderCount; ++i)
    {
        tFields[iFieldCount++] = (xhttpfield){xllm__sv(pCall->pExtraHeaderNames[i]),
                                              xllm__sv(pCall->pExtraHeaderValues[i])};
    }
    if (!xrtHttp1RequestWrite(xllm__sv("POST"), xllm__sv(pCall->pClient->sTarget),
                              XHTTP_VERSION_1_1, tFields, iFieldCount, NULL, 0u, &iHeaderSize))
        goto done;
    pCall->sRequestHeader = (char *)xllm__malloc(iHeaderSize);
    if (!pCall->sRequestHeader ||
        !xrtHttp1RequestWrite(xllm__sv("POST"), xllm__sv(pCall->pClient->sTarget),
                              XHTTP_VERSION_1_1, tFields, iFieldCount,
                              (uint8_t *)pCall->sRequestHeader, iHeaderSize, &iHeaderSize))
        goto done;
    pCall->iRequestHeaderSize = iHeaderSize;
    bOk = true;
done:
    xllm__free(tFields);
    for (i = 0u; i < iAuth; ++i)
    {
        if (tAuth[i].sValue)
        {
            volatile char *pSecret = (volatile char *)tAuth[i].sValue;
            size_t iLen = strlen(tAuth[i].sValue);
            while (iLen--)
            {
                pSecret[iLen] = 0;
            }
            xllm__free(tAuth[i].sValue);
        }
    }
    return bOk;
}

/* ------------------------------------------------------------------ */
/* URL parsing and client runtime                                      */
/* ------------------------------------------------------------------ */

static bool xllm__parse_url(xllm_client *pClient, xllm_error *pError)
{
    const char *sUrl = pClient->sBaseUrl;
    const char *sAuthority;
    const char *sPath;
    const char *sHostBegin;
    const char *sHostEnd;
    const char *sPort = NULL;
    size_t iHostLen;
    size_t iHeaderLen;
    unsigned long uPort;
    char *sEnd;
    if (strncmp(sUrl, "https://", 8u) == 0)
    {
        pClient->bTls = true;
        sAuthority = sUrl + 8u;
        pClient->uPort = 443u;
    }
    else if (strncmp(sUrl, "http://", 7u) == 0)
    {
        pClient->bTls = false;
        sAuthority = sUrl + 7u;
        pClient->uPort = 80u;
    }
    else
    {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "base URL must use http or https");
        return false;
    }
    sPath = strchr(sAuthority, '/');
    if (!sPath)
        sPath = sAuthority + strlen(sAuthority);
    if (sPath == sAuthority || memchr(sAuthority, '@', (size_t)(sPath - sAuthority)) ||
        strchr(sPath, '#'))
    {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT,
                        "base URL contains an invalid authority or fragment");
        return false;
    }
    sHostBegin = sAuthority;
    if (*sHostBegin == '[')
    {
        sHostEnd = memchr(sHostBegin, ']', (size_t)(sPath - sHostBegin));
        if (!sHostEnd || sHostEnd == sHostBegin + 1)
            goto invalid;
        ++sHostBegin;
        if (sHostEnd + 1 < sPath)
        {
            if (sHostEnd[1] != ':')
                goto invalid;
            sPort = sHostEnd + 2;
        }
    }
    else
    {
        sHostEnd = memchr(sHostBegin, ':', (size_t)(sPath - sHostBegin));
        if (!sHostEnd)
            sHostEnd = sPath;
        else
            sPort = sHostEnd + 1;
    }
    iHostLen = (size_t)(sHostEnd - sHostBegin);
    if (!iHostLen)
        goto invalid;
    if (sPort)
    {
        if (sPort >= sPath)
            goto invalid;
        uPort = strtoul(sPort, &sEnd, 10);
        if (sEnd != sPath || !uPort || uPort > 65535u)
            goto invalid;
        pClient->uPort = (uint16_t)uPort;
    }
    pClient->sHost = (char *)xllm__malloc(iHostLen + 1u);
    pClient->sTarget = xllm__strdup(*sPath ? sPath : "/");
    iHeaderLen = (size_t)(sPath - sAuthority);
    pClient->sHostHeader = (char *)xllm__malloc(iHeaderLen + 1u);
    if (!pClient->sHost || !pClient->sTarget || !pClient->sHostHeader)
        goto oom;
    memcpy(pClient->sHost, sHostBegin, iHostLen);
    pClient->sHost[iHostLen] = 0;
    memcpy(pClient->sHostHeader, sAuthority, iHeaderLen);
    pClient->sHostHeader[iHeaderLen] = 0;
    return true;
invalid:
    xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT,
                    "base URL contains an invalid host or port");
    return false;
oom:
    xllm__error_set(pError, XLLM_ERROR_OUT_OF_MEMORY, "failed to parse base URL");
    return false;
}

/* IP-literal endpoints must not receive a (protocol-illegal) IP SNI:
 * per the XRT TLS contract only VerifyName carries the identity then. */
static bool xllm__host_is_ip_literal(const char *sHost)
{
    const char *p;
    unsigned uGroups = 1u;
    unsigned uValue = 0u;
    unsigned uDigits = 0u;
    if (!sHost || !sHost[0])
    {
        return false;
    }
    if (strchr(sHost, ':'))
    {
        return true;
    } /* bare IPv6 (brackets stripped) */
    for (p = sHost; *p; ++p)
    {
        if (*p >= '0' && *p <= '9')
        {
            uValue = uValue * 10u + (unsigned)(*p - '0');
            if (++uDigits > 3u)
            {
                return false;
            }
        }
        else if (*p == '.')
        {
            if (uDigits == 0u || uValue > 255u)
            {
                return false;
            }
            uValue = 0u;
            uDigits = 0u;
            ++uGroups;
        }
        else
        {
            return false;
        }
    }
    return uGroups == 4u && uDigits != 0u && uValue <= 255u;
}

static xtlsverifydecision xllm__tls_accept(const xtlspeer *pPeer, ptr pData)
{
    (void)pPeer;
    (void)pData;
    return XTLS_VERIFY_ACCEPT;
}

bool xllm__transport_client_init(xllm_client *pClient, xllm_error *pError)
{
    xnetengineconfig tEngine;
    if (!xllm__parse_url(pClient, pError))
        return false;
    pClient->pConnectionMutex = xrtMutexCreate();
    if (!pClient->pConnectionMutex)
        goto network_error;
    if (pClient->pNetEngine == NULL)
    {
        xrtNetEngineConfigInit(&tEngine);
        pClient->pNetEngine = xrtNetEngineCreate(&tEngine);
        if (!pClient->pNetEngine)
            goto network_error;
        pClient->bEngineOwned = true;
        if (!xrtNetEngineStart(pClient->pNetEngine))
            goto network_error;
    }
    pClient->pResolver = xrtNetResolverCreate(NULL);
    if (!pClient->pResolver)
        goto network_error;
    if (pClient->bTls)
    {
        xtlsverifierconfig tVerify;
        xx509store *pStore = NULL;
        xrtTlsVerifierConfigInit(&tVerify);
        if (pClient->pX509Store)
        {
            /* borrowed store wins; the verifier clones the anchors */
            tVerify.Store = pClient->pX509Store;
        }
        else if (pClient->sCaPem && pClient->sCaPem[0])
        {
            size_t iAdded = 0u;
            pStore = xrtX509StoreCreate();
            if (!pStore ||
                !xrtX509StoreAddPem(pStore, pClient->sCaPem, strlen(pClient->sCaPem), &iAdded) ||
                iAdded == 0u)
            {
                xrtX509StoreFree(pStore);
                xllm__error_set(pError, XLLM_ERROR_NETWORK,
                                "failed to load the configured CA PEM trust store");
                xllm__transport_client_unit(pClient);
                return false;
            }
            tVerify.Store = pStore;
        }
        else if (pClient->bVerifyPeer)
        {
            pStore = xrtX509StoreSystem();
            if (!pStore)
                goto network_error;
            tVerify.Store = pStore;
        }
        else
        {
            tVerify.Verify = xllm__tls_accept;
        }
        pClient->pVerifier = xrtTlsVerifierCreate(&tVerify);
        /* pStore is owned on the PEM and system paths (System() builds a
         * fresh store per call); the verifier cloned what it needs. */
        if (pStore)
        {
            xrtX509StoreFree(pStore);
        }
        if (!pClient->pVerifier)
            goto network_error;
    }
    return true;
network_error:
    xllm__error_set(pError, XLLM_ERROR_NETWORK, "failed to initialize XRT HTTP transport");
    xllm__transport_client_unit(pClient);
    return false;
}

void xllm__transport_client_unit(xllm_client *pClient)
{
    xllm_connection *pIdle[XLLM_MAX_IDLE_CONNECTIONS] = {0};
    uint32_t uIdleCount = 0u;
    uint32_t i;
    if (!pClient)
        return;
    if (pClient->pConnectionMutex && xrtMutexLock(pClient->pConnectionMutex))
    {
        uIdleCount = pClient->uIdleConnectionCount;
        pClient->uIdleConnectionCount = 0u;
        for (i = 0u; i < uIdleCount; ++i)
        {
            pIdle[i] = pClient->pIdleConnections[i];
            pClient->pIdleConnections[i] = NULL;
        }
        (void)xrtMutexUnlock(pClient->pConnectionMutex);
    }
    for (i = 0u; i < uIdleCount; ++i)
    {
        xllm__connection_close(pIdle[i]);
    }
    if (pClient->pResolver)
    {
        (void)xrtNetResolverDestroy(pClient->pResolver);
        pClient->pResolver = NULL;
    }
    if (pClient->bEngineOwned && pClient->pNetEngine)
    {
        (void)xrtNetEngineStop(pClient->pNetEngine);
        (void)xrtNetEngineDestroy(pClient->pNetEngine);
    }
    pClient->pNetEngine = NULL;
    pClient->bEngineOwned = false;
    xrtTlsVerifierRelease(pClient->pVerifier);
    pClient->pVerifier = NULL;
    if (pClient->pConnectionMutex)
    {
        (void)xrtMutexDestroy(pClient->pConnectionMutex);
        pClient->pConnectionMutex = NULL;
    }
}

/* ------------------------------------------------------------------ */
/* State machine                                                       */
/* ------------------------------------------------------------------ */

static xfuture *xllm__step_advance(xllm_call *pCall, xfuture *pDone);

/* One heap node per armed watch: a watch may never be re-initialized while
 * its notification is unwinding, so the embedded-storage reuse pattern is
 * forbidden here. The release callback frees the node exactly once. */
struct xllm_op_node
{
    xfuturewatch Watch;
    xllm_call *pCall;
    xfuture *pFuture;
    volatile long iRefs;
};

static void xllm__op_notify(ptr pData);
static void xllm__op_node_release(xllm_op_node *pNode)
{
    if (xllm__atomic_add(&pNode->iRefs, -1) == 0)
    {
        xrtFutureDestroy(pNode->pFuture);
        xllm__free(pNode);
    }
}

static void xllm__op_release(ptr pData) { xllm__op_node_release((xllm_op_node *)pData); }

typedef enum xllm_op_arm_result
{
    XLLM_OP_ARM_PENDING = 0,
    XLLM_OP_ARM_TERMINAL, /* future already complete: advance inline */
    XLLM_OP_ARM_MEMORY,   /* watch node allocation failed */
    XLLM_OP_ARM_ERROR     /* mutex/watch registration failed */
} xllm_op_arm_result;

/* Arm a watch and transfer the pending Future to its release callback.
 * Publication and WatchAdd share the slot lock. A concurrent completion may
 * run as soon as that lock is released, so the pending path must not touch
 * the node afterwards. */
static xllm_op_arm_result xllm__op_arm(xllm_call *pCall, xfuture *pFuture)
{
    xllm_op_node *pNode = (xllm_op_node *)xllm__calloc(1u, sizeof(*pNode));
    xfuturewatchresult eWatch;
    if (!pNode)
    {
        return XLLM_OP_ARM_MEMORY;
    }
    pNode->pCall = pCall;
    pNode->pFuture = pFuture;
    /* One reference belongs to the published slot and one to the Watch.
     * A completion can run before WatchAdd returns, so both must exist before
     * registration. Non-pending registration frees the unowned node directly. */
    pNode->iRefs = 2;
    if (!xrtFutureWatchInit(&pNode->Watch, xllm__op_notify, xllm__op_release, pNode) ||
        !pCall->bOpMutexInit || !xrtMutexLock(&pCall->OpMutex))
    {
        xllm__free(pNode);
        return XLLM_OP_ARM_ERROR;
    }
    if (pCall->pOpNode != NULL)
    {
        (void)xrtMutexUnlock(&pCall->OpMutex);
        xllm__free(pNode);
        return XLLM_OP_ARM_ERROR;
    }
    pCall->pOpNode = pNode;
    eWatch = xrtFutureWatchAdd(pFuture, &pNode->Watch);
    if (eWatch == XFUTURE_WATCH_PENDING)
    {
        (void)xrtMutexUnlock(&pCall->OpMutex);
        return XLLM_OP_ARM_PENDING;
    }
    pCall->pOpNode = NULL;
    (void)xrtMutexUnlock(&pCall->OpMutex);
    /* WatchAdd on a terminal future does not take the watch and runs no
     * release: free the node ourselves. */
    xllm__free(pNode);
    return eWatch == XFUTURE_WATCH_READY ? XLLM_OP_ARM_TERMINAL : XLLM_OP_ARM_ERROR;
}

/* Remove the currently published operation. Clearing the slot under the
 * mutex prevents its callback from advancing the state machine. Remove then
 * joins an already-running callback and invokes the node release exactly
 * once; it must run outside the mutex because the callback also takes it. */
void xllm__op_disarm(xllm_call *pCall)
{
    xllm_op_node *pNode = NULL;
    if (!pCall || !pCall->bOpMutexInit || !xrtMutexLock(&pCall->OpMutex))
    {
        return;
    }
    pNode = pCall->pOpNode;
    pCall->pOpNode = NULL;
    (void)xrtMutexUnlock(&pCall->OpMutex);
    if (pNode)
    {
        xrtFutureWatchRemove(pNode->pFuture, &pNode->Watch);
        xllm__op_node_release(pNode);
    }
}

/* Borrow the current operation as a real Future reference for cancellation.
 * The slot mutex keeps the node-owned reference alive until Ref succeeds. */
static xfuture *xllm__op_future_ref(xllm_call *pCall)
{
    xllm_op_node *pNode;
    xfuture *pFuture = NULL;
    if (!pCall || !pCall->bOpMutexInit || !xrtMutexLock(&pCall->OpMutex))
    {
        return NULL;
    }
    pNode = pCall->pOpNode;
    if (pNode)
    {
        pFuture = xrtFutureRef(pNode->pFuture);
    }
    (void)xrtMutexUnlock(&pCall->OpMutex);
    return pFuture;
}

static void xllm__op_notify(ptr pData)
{
    xllm_op_node *pNode = (xllm_op_node *)pData;
    xllm_call *pCall = pNode->pCall;
    xfuture *pFuture = pNode->pFuture;
    bool bCurrent = false;
    (void)xllm__atomic_add(&pCall->iTransportActive, 1);
    if (xrtMutexLock(&pCall->OpMutex))
    {
        if (pCall->pOpNode == pNode)
        {
            pCall->pOpNode = NULL;
            bCurrent = true;
        }
        (void)xrtMutexUnlock(&pCall->OpMutex);
    }
    if (bCurrent)
    {
        xfuture *pNext = NULL;
        if (pCall->bTransportMutexInit && xrtMutexLock(&pCall->TransportMutex))
        {
            if (xllm__atomic_load(&pCall->iClosing) == 0 &&
                xllm__atomic_load(&pCall->iTerminal) == 0)
            {
                pNext = xllm__step_advance(pCall, pFuture);
            }
            while (pNext)
            {
                xfuture *pCurrent;
                xllm_op_arm_result eArm;
                if (xllm__atomic_load(&pCall->iClosing) != 0 ||
                    xllm__atomic_load(&pCall->iTerminal) != 0)
                {
                    xrtFutureDestroy(pNext);
                    pNext = NULL;
                    break;
                }
                pCurrent = pNext;
                eArm = xllm__op_arm(pCall, pCurrent);
                if (eArm == XLLM_OP_ARM_PENDING)
                {
                    break;
                }
                if (eArm == XLLM_OP_ARM_MEMORY || eArm == XLLM_OP_ARM_ERROR)
                {
                    xrtFutureDestroy(pCurrent);
                    xllm__transport_fail(pCall, "submit", XLLM_TRANSPORT_ERROR,
                                         eArm == XLLM_OP_ARM_MEMORY ? "out_of_memory" : "watch",
                                         xrtGetError());
                    xllm__transport_finish(pCall, XLLM_TRANSPORT_ERROR);
                    pNext = NULL;
                    break;
                }
                pNext = xllm__step_advance(pCall, pCurrent);
                xrtFutureDestroy(pCurrent);
            }
            (void)xrtMutexUnlock(&pCall->TransportMutex);
        }
    }
    (void)xllm__atomic_add(&pCall->iTransportActive, -1);
    if (bCurrent)
    {
        xllm__op_node_release(pNode);
    }
}

/* Submit an operation future and advance inline until a pending watch arms. */
static void xllm__op_submit_chain(xllm_call *pCall, xfuture *pFuture)
{
    if (!pFuture)
    {
        if (xllm__atomic_load(&pCall->iTerminal) != 0)
        {
            return;
        }
        if (pCall->tHttpDiagnostics.eResult == XLLM_TRANSPORT_OK)
        {
            xllm__transport_fail(pCall, pCall->ePhase == XLLM_ASYNC_DIAL ? "connect" : "submit",
                                 XLLM_TRANSPORT_ERROR, "submit", xrtGetError());
        }
        xllm__transport_finish(pCall, XLLM_TRANSPORT_ERROR);
        return;
    }
    for (;;)
    {
        xllm_op_arm_result eArm;
        eArm = xllm__op_arm(pCall, pFuture);
        if (eArm == XLLM_OP_ARM_PENDING)
        {
            return;
        }
        if (eArm == XLLM_OP_ARM_MEMORY || eArm == XLLM_OP_ARM_ERROR)
        {
            xllm__transport_fail(pCall, "submit", XLLM_TRANSPORT_ERROR,
                                 eArm == XLLM_OP_ARM_MEMORY ? "out_of_memory" : "watch",
                                 xrtGetError());
            xllm__transport_finish(pCall, XLLM_TRANSPORT_ERROR);
            xrtFutureDestroy(pFuture);
            return;
        }
        {
            xfuture *pNext;
            if (xllm__atomic_load(&pCall->iClosing) != 0 ||
                xllm__atomic_load(&pCall->iTerminal) != 0)
            {
                xrtFutureDestroy(pFuture);
                return;
            }
            pNext = xllm__step_advance(pCall, pFuture);
            xrtFutureDestroy(pFuture);
            if (!pNext)
            {
                return;
            }
            pFuture = pNext;
        }
    }
}

static xllm_transport_result xllm__wait_error_result(xfuture *pFuture)
{
    const xerror *pError = xrtFutureError(pFuture);
    if (pError)
    {
        if (xrtErrorKind(pError) == XERR_TIMEOUT)
            return XLLM_TRANSPORT_TIMEOUT;
        if (xrtErrorKind(pError) == XERR_CANCELLED)
            return XLLM_TRANSPORT_CANCELLED;
    }
    return XLLM_TRANSPORT_ERROR;
}

static const char *xllm__error_name(xllm_transport_result eResult, const char *sFallback)
{
    if (eResult == XLLM_TRANSPORT_TIMEOUT)
        return "timeout";
    if (eResult == XLLM_TRANSPORT_CANCELLED)
        return "cancelled";
    return sFallback;
}

/* Dial completion: adopt the stream and continue to the send phase. */
static xfuture *xllm__step_dial(xllm_call *pCall, xfuture *pFuture)
{
    xllm_client *pClient = pCall->pClient;
    if (xrtFutureState(pFuture) != XFUTURE_RESOLVED)
    {
        xllm_transport_result eResult = xllm__wait_error_result(pFuture);
        xllm__transport_fail(pCall, "connect", eResult,
                             xllm__error_name(eResult, pClient->bTls ? "tls_connect" : "connect"),
                             xrtFutureError(pFuture));
        xllm__transport_finish(pCall, eResult);
        return NULL;
    }
    pCall->pConnection = (xllm_connection *)xllm__calloc(1u, sizeof(*pCall->pConnection));
    if (!pCall->pConnection)
    {
        xllm__transport_fail(pCall, "connect", XLLM_TRANSPORT_ERROR, "out_of_memory", NULL);
        xllm__transport_finish(pCall, XLLM_TRANSPORT_ERROR);
        return NULL;
    }
    pCall->pConnection->bTls = pClient->bTls;
    if (pClient->bTls)
    {
        pCall->pConnection->pTls = xrtTlsStreamRef((xtlsstream *)xrtFutureValue(pFuture));
    }
    else
    {
        pCall->pConnection->pTcp = xrtNetStreamRef((xnetstream *)xrtFutureValue(pFuture));
    }
    pCall->tHttpDiagnostics.uConnectedMs = xllm__clock_ms();
    pCall->ePhase = XLLM_ASYNC_SEND;
    pCall->iSendOffset = 0u;
    return XLLM_STEP_CONTINUE;
}

/* Submit the next send chunk; CONTINUE when written inline, NULL when done. */
static xfuture *xllm__send_submit(xllm_call *pCall)
{
    size_t iTotal = pCall->iRequestHeaderSize + strlen(pCall->sRequestBody);
    if (pCall->iSendOffset >= iTotal)
    {
        return NULL;
    }
    {
        size_t iLocal = pCall->iSendOffset < pCall->iRequestHeaderSize
                            ? pCall->iSendOffset
                            : pCall->iSendOffset - pCall->iRequestHeaderSize;
        size_t iSegmentEnd =
            pCall->iSendOffset < pCall->iRequestHeaderSize ? pCall->iRequestHeaderSize : iTotal;
        const char *sBase = pCall->iSendOffset < pCall->iRequestHeaderSize ? pCall->sRequestHeader
                                                                           : pCall->sRequestBody;
        size_t iChunk = iSegmentEnd - pCall->iSendOffset;
        const void *pData = sBase + iLocal;
        if (iChunk > XLLM_HTTP_IO_CHUNK)
            iChunk = XLLM_HTTP_IO_CHUNK;
        if (pCall->pConnection->bTls)
        {
            pCall->iSendPending = iChunk;
            return xrtTlsStreamSendAsync(pCall->pConnection->pTls, pData, iChunk);
        }
        {
            xnetresult eResult = xrtNetStreamSend(pCall->pConnection->pTcp, pData, iChunk);
            if (eResult == XNET_RESULT_OK)
            {
                pCall->iSendOffset += iChunk;
                pCall->tHttpDiagnostics.uRequestBytes += iChunk;
                return XLLM_STEP_CONTINUE;
            }
            if (eResult == XNET_RESULT_AGAIN)
            {
                return xrtNetStreamWaitAsync(pCall->pConnection->pTcp, XNET_STREAM_WAIT_WRITE);
            }
            xllm__transport_fail(pCall, "send", XLLM_TRANSPORT_ERROR, "send", xrtGetError());
            xllm__transport_finish(pCall, XLLM_TRANSPORT_ERROR);
            return NULL;
        }
    }
}

/* Send-phase completion: account the chunk, continue or move to read. */
static xfuture *xllm__step_send(xllm_call *pCall, xfuture *pFuture)
{
    if (pFuture)
    {
        if (xrtFutureState(pFuture) != XFUTURE_RESOLVED)
        {
            xllm_transport_result eResult = xllm__wait_error_result(pFuture);
            xllm__transport_fail(pCall, "send", eResult, xllm__error_name(eResult, "send"),
                                 xrtFutureError(pFuture));
            xllm__transport_finish(pCall, eResult);
            return NULL;
        }
        if (pCall->pConnection->bTls)
        {
            /* Account exactly the chunk that was submitted: the segment
             * layout may cap a chunk below the 64 KiO io slice, and the
             * server waits for every Content-Length byte. */
            pCall->iSendOffset += pCall->iSendPending;
            pCall->tHttpDiagnostics.uRequestBytes += pCall->iSendPending;
            pCall->iSendPending = 0u;
        }
        /* Plain TCP WAIT_WRITE fired: retry the send inline below. */
    }
    for (;;)
    {
        xfuture *pNext = xllm__send_submit(pCall);
        if (pNext == NULL)
        {
            pCall->tHttpDiagnostics.uRequestSentMs = xllm__clock_ms();
            pCall->ePhase = XLLM_ASYNC_READ;
            return XLLM_STEP_CONTINUE;
        }
        if (pNext != XLLM_STEP_CONTINUE)
        {
            return pNext;
        }
    }
}

static xfuture *xllm__recv_submit(xllm_call *pCall)
{
    if (pCall->pConnection->bTls)
    {
        return xrtTlsStreamRecvAsync(pCall->pConnection->pTls, XLLM_HTTP_IO_CHUNK);
    }
    return xrtNetStreamRecvAsync(pCall->pConnection->pTcp, XLLM_HTTP_IO_CHUNK);
}

static bool xllm__response_reusable(const xhttp1head *pHead, const xhttp1bodyplan *pPlan)
{
    if (pPlan->Mode == XHTTP1_BODY_CLOSE || (pHead->Flags & XHTTP1_CONNECTION_CLOSE))
        return false;
    if (pHead->Version == XHTTP_VERSION_1_0 && !(pHead->Flags & XHTTP1_KEEP_ALIVE))
        return false;
    return true;
}

static void xllm__wire_compact(xllm_call *pCall)
{
    if (pCall->iWireOffset)
    {
        if (pCall->iWireOffset < pCall->tWire.iLen)
        {
            memmove(pCall->tWire.pData, pCall->tWire.pData + pCall->iWireOffset,
                    pCall->tWire.iLen - pCall->iWireOffset);
        }
        pCall->tWire.iLen -= pCall->iWireOffset;
        if (pCall->tWire.pData)
        {
            pCall->tWire.pData[pCall->tWire.iLen] = 0;
        }
        pCall->iWireOffset = 0u;
    }
}

static void xllm__pump_fail(xllm_call *pCall, const char *sPhase, const char *sName)
{
    xllm__transport_fail(pCall, sPhase, XLLM_TRANSPORT_ERROR, sName, NULL);
    xllm__transport_finish(pCall, XLLM_TRANSPORT_ERROR);
}

/* Pump buffered wire bytes through the incremental head/body parser. */
static xfuture *xllm__step_pump(xllm_call *pCall)
{
    for (;;)
    {
        if (!pCall->bHeadReady)
        {
            xhttp1status eStatus =
                xrtHttp1ResponseParse(xllm__bv(pCall->tWire.pData, pCall->tWire.iLen),
                                      &pCall->tHead, &pCall->tHeadLimits, &pCall->tProtoErr);
            if (eStatus == XHTTP1_READY)
            {
                pCall->bHeadReady = true;
                pCall->iWireOffset = pCall->tHead.Bytes;
                pCall->tHttpDiagnostics.uHeadersMs = xllm__clock_ms();
                if (!xllm__transport_headers(pCall, &pCall->tHead))
                {
                    xllm__transport_fail(pCall, "headers", XLLM_TRANSPORT_CANCELLED,
                                         "callback_cancelled", NULL);
                    xllm__transport_finish(pCall, XLLM_TRANSPORT_CANCELLED);
                    return NULL;
                }
                if (!xrtHttp1ResponseBodyPlan(&pCall->tHead, xllm__sv("POST"), &pCall->tPlan))
                {
                    xllm__pump_fail(pCall, "headers", "http_framing");
                    return NULL;
                }
                xrtHttp1BodyLimitsInit(&pCall->tBodyLimits);
                pCall->tBodyLimits.MaxBody = pCall->uMaxResponseBodyBytes
                                                 ? pCall->uMaxResponseBodyBytes
                                                 : XLLM_MAX_FALLBACK_BODY;
                if (!xrtHttp1BodyInit(&pCall->tBody, &pCall->tPlan, pCall->tTrailers,
                                      XLLM_HTTP_TRAILER_LIMIT, &pCall->tBodyLimits))
                {
                    xllm__pump_fail(pCall, "body", "http_framing");
                    return NULL;
                }
                continue;
            }
            if (eStatus == XHTTP1_ERROR || eStatus == XHTTP1_FIELDS ||
                pCall->tWire.iLen >=
                    (pCall->uMaxHttpHeadBytes ? pCall->uMaxHttpHeadBytes : XLLM_HTTP_HEAD_LIMIT))
            {
                xllm__pump_fail(pCall, "headers", "http_protocol");
                return NULL;
            }
            if (pCall->bWireEnd)
            {
                xllm__pump_fail(pCall, "headers", "unexpected_eof");
                return NULL;
            }
            return xllm__recv_submit(pCall);
        }
        {
            xbytesview tInput =
                xllm__bv(pCall->tWire.pData ? pCall->tWire.pData + pCall->iWireOffset : NULL,
                         pCall->tWire.iLen - pCall->iWireOffset);
            size_t iConsumed = 0u;
            xbytesview tData = {0};
            xhttp1bodystatus eBody = xrtHttp1BodyRead(&pCall->tBody, tInput, pCall->bWireEnd,
                                                      &iConsumed, &tData, &pCall->tProtoErr);
            pCall->iWireOffset += iConsumed;
            if (eBody == XHTTP1_BODY_DATA)
            {
                pCall->tHttpDiagnostics.uResponseBodyBytes += tData.Size;
                if (!xllm__transport_body(pCall, tData.Data, tData.Size))
                {
                    xllm__transport_fail(pCall, "body", XLLM_TRANSPORT_CANCELLED,
                                         "callback_cancelled", NULL);
                    xllm__transport_finish(pCall, XLLM_TRANSPORT_CANCELLED);
                    return NULL;
                }
                continue;
            }
            if (eBody == XHTTP1_BODY_DONE)
            {
                pCall->bReusable = xllm__response_reusable(&pCall->tHead, &pCall->tPlan) &&
                                   pCall->iWireOffset == pCall->tWire.iLen;
                xllm__transport_finish(pCall, XLLM_TRANSPORT_OK);
                return NULL;
            }
            if (eBody == XHTTP1_BODY_ERROR || eBody == XHTTP1_BODY_FIELDS)
            {
                xllm__pump_fail(pCall, "body", "http_protocol");
                return NULL;
            }
            /* A decoded SSE terminal event completes the model protocol.
             * Do not wait for a provider's delayed final chunk/EOF. Prefer
             * the BODY_DONE path above when framing is already available;
             * otherwise retire this undrained connection, never pool it.
             * A response-body hook deliberately defers decoding until EOF. */
            if (pCall->bSse && !pCall->bDeferParse && pCall->bDone)
            {
                pCall->bReusable = false;
                xllm__transport_finish(pCall, XLLM_TRANSPORT_OK);
                return NULL;
            }
            if (pCall->bWireEnd)
            {
                xllm__pump_fail(pCall, "body", "unexpected_eof");
                return NULL;
            }
            xllm__wire_compact(pCall);
            return xllm__recv_submit(pCall);
        }
    }
}

/* Read-phase completion: absorb the received bytes and pump. */
static xfuture *xllm__step_read(xllm_call *pCall, xfuture *pFuture)
{
    xfuturestate eState = xrtFutureState(pFuture);
    if (eState == XFUTURE_RESOLVED)
    {
        xnetbytes *pBytes = (xnetbytes *)xrtFutureValue(pFuture);
        xbytesview tView = xrtNetBytesView(pBytes);
        if (tView.Size)
        {
            if (!xllm__buf_append(&pCall->tWire, tView.Data, tView.Size))
            {
                xllm__pump_fail(pCall, "receive", "out_of_memory");
                return NULL;
            }
            if (!pCall->tHttpDiagnostics.uFirstByteMs)
            {
                pCall->tHttpDiagnostics.uFirstByteMs = xllm__clock_ms();
            }
        }
        return xllm__step_pump(pCall);
    }
    if (eState == XFUTURE_CLOSED)
    {
        pCall->bWireEnd = true;
        return xllm__step_pump(pCall);
    }
    {
        xllm_transport_result eResult = xllm__wait_error_result(pFuture);
        if (eResult == XLLM_TRANSPORT_ERROR && pCall->pConnection && !pCall->pConnection->bTls &&
            xrtNetStreamState(pCall->pConnection->pTcp) == XNET_STREAM_CLOSED)
        {
            pCall->bWireEnd = true;
            return xllm__step_pump(pCall);
        }
        xllm__transport_fail(pCall, "receive", eResult, xllm__error_name(eResult, "receive"),
                             xrtFutureError(pFuture));
        xllm__transport_finish(pCall, eResult);
        return NULL;
    }
}

/* Process one completed operation future; return the next operation future,
 * the CONTINUE sentinel, or NULL at a terminal. */
static xfuture *xllm__step_advance(xllm_call *pCall, xfuture *pFuture)
{
    xfuture *pNext = NULL;
    switch (pCall->ePhase)
    {
    case XLLM_ASYNC_DIAL:
        pNext = xllm__step_dial(pCall, pFuture);
        if (pNext == XLLM_STEP_CONTINUE)
        {
            pNext = xllm__step_send(pCall, NULL);
            if (pNext == XLLM_STEP_CONTINUE)
            {
                pNext = xllm__recv_submit(pCall);
            }
        }
        break;
    case XLLM_ASYNC_SEND:
        pNext = xllm__step_send(pCall, pFuture);
        if (pNext == XLLM_STEP_CONTINUE)
        {
            pNext = xllm__recv_submit(pCall);
        }
        break;
    case XLLM_ASYNC_READ:
        pNext = xllm__step_read(pCall, pFuture);
        break;
    default:
        xllm__transport_finish(pCall, XLLM_TRANSPORT_ERROR);
        break;
    }
    return pNext;
}

void xllm__transport_finish(xllm_call *pCall, xllm_transport_result eResult)
{
    if (xllm__atomic_add(&pCall->iTerminal, 1) != 1)
    {
        return;
    }
    pCall->ePhase = XLLM_ASYNC_DONE;
    pCall->eTransportResult = eResult;
    pCall->tHttpDiagnostics.uCompletedMs = xllm__clock_ms();
    if (pCall->tHttpDiagnostics.eResult == XLLM_TRANSPORT_OK && eResult != XLLM_TRANSPORT_OK)
    {
        pCall->tHttpDiagnostics.eResult = eResult;
    }
    if (eResult == XLLM_TRANSPORT_OK)
    {
        xllm__copy_text(pCall->tHttpDiagnostics.sPhase, sizeof(pCall->tHttpDiagnostics.sPhase),
                        "complete");
        xllm__copy_text(pCall->tHttpDiagnostics.sError, sizeof(pCall->tHttpDiagnostics.sError),
                        "none");
    }
    if (pCall->uTimerId)
    {
        (void)xrtNetEngineTimerCancel(pCall->pClient->pNetEngine, pCall->uTimerId);
        pCall->uTimerId = 0u;
    }
    if (pCall->pConnection)
    {
        xllm__connection_release(pCall->pClient, pCall->pConnection,
                                 eResult == XLLM_TRANSPORT_OK && pCall->bReusable);
        pCall->pConnection = NULL;
    }
    if (pCall->pPromise)
    {
        (void)xrtPromiseResolve(pCall->pPromise, NULL);
    }
}

/* TransportMutex protects publication and retirement of pConnection. Stream
 * Abort is non-blocking and may run while an operation Future owns another
 * stream reference, so it is safe to issue while the mutex is held. */
static void xllm__abort_connection_locked(xllm_call *pCall)
{
    if (pCall->pConnection)
    {
        if (pCall->pConnection->bTls)
        {
            (void)xrtTlsStreamAbort(pCall->pConnection->pTls);
        }
        else
        {
            (void)xrtNetStreamAbort(pCall->pConnection->pTcp);
        }
    }
}

static void xllm__watchdog(xnetworker *pWorker, uint64 uId, xnetresult eResult, ptr pData)
{
    xllm_call *pCall = (xllm_call *)pData;
    (void)pWorker;
    (void)uId;
    /* Exactly-once callback: release the timer lifetime reference no matter
     * which terminal result this is (fired, cancelled, stopped, failed). */
    if (eResult != XNET_RESULT_OK || xllm__atomic_load(&pCall->iTerminal) != 0)
    {
        (void)xllm__atomic_add(&pCall->iTimerRefs, -1);
        return;
    }
    if (!pCall->bTransportMutexInit || !xrtMutexLock(&pCall->TransportMutex))
    {
        (void)xllm__atomic_add(&pCall->iTimerRefs, -1);
        return;
    }
    if (xllm__atomic_load(&pCall->iTerminal) == 0)
    {
        xllm__abort_connection_locked(pCall);
        xllm__transport_fail(pCall,
                             pCall->ePhase == XLLM_ASYNC_DIAL
                                 ? "connect"
                                 : (pCall->ePhase == XLLM_ASYNC_SEND ? "send" : "receive"),
                             XLLM_TRANSPORT_TIMEOUT, "timeout", NULL);
        xllm__transport_finish(pCall, XLLM_TRANSPORT_TIMEOUT);
    }
    (void)xrtMutexUnlock(&pCall->TransportMutex);
    (void)xllm__atomic_add(&pCall->iTimerRefs, -1);
}

static void xllm__cancel_abort(ptr pData)
{
    xllm_call *pCall = (xllm_call *)pData;
    if (pCall->bTransportMutexInit && xrtMutexLock(&pCall->TransportMutex))
    {
        xllm__abort_connection_locked(pCall);
        (void)xrtMutexUnlock(&pCall->TransportMutex);
    }
}

bool xllm__transport_arm_cancel(xllm_call *pCall)
{
    if (!pCall || !pCall->pCancel || pCall->pCancelWatch)
        return false;
    /* CancelWatch may call back synchronously for an already-cancelled parent.
     * Register before transport_begin takes TransportMutex. */
    pCall->pCancelWatch = xrtCancelWatch(pCall->pCancel, xllm__cancel_abort, pCall);
    return pCall->pCancelWatch != NULL;
}

static void xllm__transport_begin_locked(xllm_call *pCall)
{
    xllm_client *pClient = pCall->pClient;
    xllm_connection *pConnection = NULL;
    pCall->tHttpDiagnostics.uStartedMs = xllm__clock_ms();
    pCall->tHttpDiagnostics.eResult = XLLM_TRANSPORT_OK;
    xllm__copy_text(pCall->tHttpDiagnostics.sPhase, sizeof(pCall->tHttpDiagnostics.sPhase),
                    "connect");
    xrtHttp1LimitsInit(&pCall->tHeadLimits);
    pCall->tHeadLimits.MaxHead =
        pCall->uMaxHttpHeadBytes ? pCall->uMaxHttpHeadBytes : XLLM_HTTP_HEAD_LIMIT;
    pCall->tHeadLimits.MaxFields = XLLM_HTTP_FIELD_LIMIT;
    xrtHttp1HeadInit(&pCall->tHead, pCall->tHeadFields, XLLM_HTTP_FIELD_LIMIT);
    memset(&pCall->tProtoErr, 0, sizeof(pCall->tProtoErr));
    if (xrtCancelRequested(pCall->pCancel))
    {
        xllm__transport_fail(pCall, "cancel", XLLM_TRANSPORT_CANCELLED, "cancelled", NULL);
        xllm__transport_finish(pCall, XLLM_TRANSPORT_CANCELLED);
        return;
    }
    if (!xllm__transport_build_header(pCall))
    {
        xllm__transport_fail(pCall, "request", XLLM_TRANSPORT_ERROR, "request", xrtGetError());
        xllm__transport_finish(pCall, XLLM_TRANSPORT_ERROR);
        return;
    }
    if (pCall->uDeadline != XLLM_DEADLINE_NEVER)
    {
        (void)xllm__atomic_add(&pCall->iTimerRefs, 1);
        pCall->uTimerId = xrtNetEngineSchedule(
            pClient->pNetEngine, 0u, xllmDeadlineWaitMs(pCall->uDeadline), xllm__watchdog, pCall);
        if (pCall->uTimerId == 0u)
        {
            /* Scheduling failed: without the watchdog the deadline is only
             * enforced by a waiting consumer, which future-only callers are
             * not -- fail closed instead of running unbounded. */
            (void)xllm__atomic_add(&pCall->iTimerRefs, -1);
            xllm__transport_fail(pCall, "connect", XLLM_TRANSPORT_ERROR, "timer_unavailable", NULL);
            xllm__transport_finish(pCall, XLLM_TRANSPORT_ERROR);
            return;
        }
    }
    pConnection = xllm__connection_take(pClient);
    if (pConnection)
    {
        xfuture *pNext;
        pCall->tHttpDiagnostics.bReusedConnection = true;
        pCall->tHttpDiagnostics.uConnectedMs = pCall->tHttpDiagnostics.uStartedMs;
        pCall->pConnection = pConnection;
        pCall->ePhase = XLLM_ASYNC_SEND;
        pNext = xllm__step_send(pCall, NULL);
        if (pNext == XLLM_STEP_CONTINUE)
        {
            pNext = xllm__recv_submit(pCall);
        }
        xllm__op_submit_chain(pCall, pNext);
        return;
    }
    pCall->ePhase = XLLM_ASYNC_DIAL;
    if (pClient->bTls)
    {
        xtlsclientconfig tTls;
        xtlsdialconfig tDial;
        xfuture *pFuture;
        xrtTlsClientConfigInit(&tTls);
        if (xllm__host_is_ip_literal(pClient->sHost))
        {
            tTls.ServerName = xllm__sv(NULL);
            tTls.VerifyName = xllm__sv(pClient->sHost);
        }
        else
        {
            tTls.ServerName = xllm__sv(pClient->sHost);
            tTls.VerifyName = xllm__sv(pClient->sHost);
        }
        tTls.Verifier = pClient->pVerifier;
        xrtTlsDialConfigInit(&tDial);
        if (pCall->uDeadline != XLLM_DEADLINE_NEVER)
        {
            tDial.Timeout = xllmDeadlineWaitMs(pCall->uDeadline);
        }
        if (pClient->pProxy && !xllm__proxy_bypassed(pClient->sProxyBypass, pClient->sHost))
        {
            pFuture =
                xllm__proxy_dial_async(pClient->pNetEngine, pClient->pResolver, pClient->pProxy,
                                       pClient->sHost, pClient->uPort, &tTls, &tDial, NULL, NULL);
        }
        else
        {
            pFuture = xrtTlsDialAsync(pClient->pNetEngine, pClient->pResolver, pClient->sHost,
                                      pClient->uPort, &tTls, &tDial, NULL, NULL);
        }
        xllm__op_submit_chain(pCall, pFuture);
    }
    else
    {
        xnetdialconfig tDial;
        xfuture *pFuture;
        xrtNetDialConfigInit(&tDial);
        if (pCall->uDeadline != XLLM_DEADLINE_NEVER)
        {
            tDial.Timeout = xllmDeadlineWaitMs(pCall->uDeadline);
        }
        pFuture = xrtNetDialAsync(pClient->pNetEngine, pClient->pResolver, pClient->sHost,
                                  pClient->uPort, &tDial, NULL, NULL);
        xllm__op_submit_chain(pCall, pFuture);
    }
}

void xllm__transport_begin(xllm_call *pCall)
{
    if (!pCall || !pCall->bTransportMutexInit || !xrtMutexLock(&pCall->TransportMutex))
    {
        return;
    }
    xllm__transport_begin_locked(pCall);
    (void)xrtMutexUnlock(&pCall->TransportMutex);
}

void xllm__transport_abort(xllm_call *pCall)
{
    xfuture *pOpFuture;
    if (xllm__atomic_load(&pCall->iTerminal) != 0)
    {
        return;
    }
    xllm__cancel_abort((ptr)pCall);
    pOpFuture = xllm__op_future_ref(pCall);
    if (pOpFuture)
    {
        (void)xrtFutureCancel(pOpFuture);
        xrtFutureDestroy(pOpFuture);
    }
    if (pCall->pFuture)
    {
        /* xrtFutureWaitFor takes relative milliseconds. */
        uint64 uGiveUp = xllm__clock_ms() + 3000u;
        while (xllm__atomic_load(&pCall->iTerminal) == 0 && xllm__clock_ms() < uGiveUp)
        {
            (void)xrtFutureWaitFor(pCall->pFuture, 20);
        }
    }
    if (xllm__atomic_load(&pCall->iTerminal) == 0)
    {
        if (pCall->bTransportMutexInit && xrtMutexLock(&pCall->TransportMutex))
        {
            if (xllm__atomic_load(&pCall->iTerminal) == 0)
            {
                xllm__transport_fail(pCall, "abort", XLLM_TRANSPORT_ERROR, "aborted", NULL);
                xllm__transport_finish(pCall, XLLM_TRANSPORT_ERROR);
            }
            (void)xrtMutexUnlock(&pCall->TransportMutex);
        }
    }
}
