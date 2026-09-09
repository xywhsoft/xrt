#include "xllm_internal.h"

#define XLLM_HTTP_HEAD_LIMIT (64u * 1024u)
#define XLLM_HTTP_FIELD_LIMIT 100u
#define XLLM_HTTP_TRAILER_LIMIT 32u
#define XLLM_HTTP_IO_CHUNK (64u * 1024u)

struct xllm_connection {
    bool bTls;
    xnetstream* pTcp;
    xtlsstream* pTls;
};

typedef enum xllm_receive_result {
    XLLM_RECEIVE_DATA = 0,
    XLLM_RECEIVE_END,
    XLLM_RECEIVE_ERROR
} xllm_receive_result;

static xstrview xllm__sv(const char* sText)
{
    xstrview tView;
    tView.Data = sText ? sText : "";
    tView.Size = sText ? strlen(sText) : 0u;
    return tView;
}

static xbytesview xllm__bv(const void* pData, size_t iSize)
{
    xbytesview tView;
    tView.Data = (const uint8*)pData;
    tView.Size = iSize;
    return tView;
}

static uint64_t xllm__clock_ms(void)
{
    return xrtClock() / UINT64_C(1000);
}

static xdeadline xllm__min_deadline(xdeadline a, xdeadline b)
{
    if ( a == XRT_DEADLINE_NEVER ) return b;
    if ( b == XRT_DEADLINE_NEVER ) return a;
    return a < b ? a : b;
}

static xdeadline xllm__idle_deadline(const xllm_call* pCall)
{
    xdeadline tIdle = XRT_DEADLINE_NEVER;
    if ( pCall->pClient->uIdleTimeoutMs ) {
        tIdle = xrtDeadlineAfter((uint64)pCall->pClient->uIdleTimeoutMs * UINT64_C(1000));
    }
    return xllm__min_deadline(pCall->uDeadline, tIdle);
}

static void xllm__transport_fail(xllm_call* pCall, const char* sPhase,
    xllm_transport_result eResult, const char* sName, const xerror* pError)
{
    if ( !pCall ) return;
    pCall->tHttpDiagnostics.eResult = eResult;
    xllm__copy_text(pCall->tHttpDiagnostics.sPhase,
        sizeof(pCall->tHttpDiagnostics.sPhase), sPhase);
    xllm__copy_text(pCall->tHttpDiagnostics.sError,
        sizeof(pCall->tHttpDiagnostics.sError), sName);
    if ( pError ) {
        pCall->tHttpDiagnostics.iSystemError = xrtErrorSystemCode(pError);
    }
}

static xllm_transport_result xllm__transport_wait_result(xwaitresult eWait)
{
    if ( eWait == XWAIT_TIMEOUT ) return XLLM_TRANSPORT_TIMEOUT;
    if ( eWait == XWAIT_CANCELLED ) return XLLM_TRANSPORT_CANCELLED;
    return XLLM_TRANSPORT_ERROR;
}

static void xllm__connection_close(xllm_connection* pConnection)
{
    if ( !pConnection ) return;
    if ( pConnection->bTls && pConnection->pTls ) {
        xfuture* pClose;
        (void)xrtTlsStreamAbort(pConnection->pTls);
        pClose = xrtTlsStreamWaitAsync(pConnection->pTls, XTLS_STREAM_WAIT_CLOSE);
        if ( pClose ) {
            (void)xrtFutureWaitFor(pClose, UINT64_C(1000000));
            xrtFutureDestroy(pClose);
        }
        xrtTlsStreamDestroy(pConnection->pTls);
    } else if ( pConnection->pTcp ) {
        (void)xrtNetStreamAbort(pConnection->pTcp);
        (void)xrtNetStreamWait(pConnection->pTcp, XNET_STREAM_WAIT_CLOSE,
            xrtDeadlineAfter(UINT64_C(1000000)), NULL);
        xrtNetStreamDestroy(pConnection->pTcp);
    }
    free(pConnection);
}

static xllm_connection* xllm__connection_take(xllm_client* pClient)
{
    xllm_connection* pConnection = NULL;
    if ( !pClient || !pClient->pConnectionMutex ) return NULL;
    if ( xrtMutexLock(pClient->pConnectionMutex) ) {
        pConnection = pClient->pIdleConnection;
        pClient->pIdleConnection = NULL;
        (void)xrtMutexUnlock(pClient->pConnectionMutex);
    }
    return pConnection;
}

static void xllm__connection_release(xllm_client* pClient,
    xllm_connection* pConnection, bool bReusable)
{
    if ( !pConnection ) return;
    if ( bReusable && pClient && pClient->pConnectionMutex &&
         xrtMutexLock(pClient->pConnectionMutex) ) {
        if ( !pClient->pIdleConnection ) {
            pClient->pIdleConnection = pConnection;
            pConnection = NULL;
        }
        (void)xrtMutexUnlock(pClient->pConnectionMutex);
    }
    xllm__connection_close(pConnection);
}

static xllm_connection* xllm__connection_open(xllm_call* pCall)
{
    xllm_client* pClient = pCall->pClient;
    xllm_connection* pConnection;
    xnetdialconfig tDial;

    pConnection = (xllm_connection*)calloc(1u, sizeof(*pConnection));
    if ( !pConnection ) {
        xllm__transport_fail(pCall, "connect", XLLM_TRANSPORT_ERROR,
            "out_of_memory", NULL);
        return NULL;
    }
    pConnection->bTls = pClient->bTls;
    if ( pClient->bTls ) {
        xtlsclientconfig tTls;
        xtlsdialconfig tTlsDial;
        xfuture* pFuture;
        xwaitresult eWait;
        xrtTlsClientConfigInit(&tTls);
        tTls.ServerName = xllm__sv(pClient->sHost);
        tTls.VerifyName = xllm__sv(pClient->sHost);
        tTls.Verifier = pClient->pVerifier;
        xrtTlsDialConfigInit(&tTlsDial);
        if ( pCall->uDeadline != XRT_DEADLINE_NEVER ) {
            tTlsDial.Timeout = xrtDeadlineRemaining(pCall->uDeadline);
        }
        pFuture = xrtTlsDialAsync(pClient->pNetEngine, pClient->pResolver,
            pClient->sHost, pClient->uPort, &tTls, &tTlsDial, NULL, NULL);
        if ( !pFuture ) {
            xllm__transport_fail(pCall, "connect", XLLM_TRANSPORT_ERROR,
                "tls_connect", xrtGetError());
            free(pConnection);
            return NULL;
        }
        eWait = xrtFutureWaitUntilCancel(pFuture, pCall->uDeadline, pCall->pCancel);
        if ( eWait != XWAIT_OK || xrtFutureState(pFuture) != XFUTURE_RESOLVED ) {
            xllm_transport_result eResult = eWait == XWAIT_OK
                ? XLLM_TRANSPORT_ERROR : xllm__transport_wait_result(eWait);
            const xerror* pError = xrtFutureError(pFuture);
            (void)xrtFutureCancel(pFuture);
            xllm__transport_fail(pCall, "connect", eResult,
                eResult == XLLM_TRANSPORT_TIMEOUT ? "timeout" :
                (eResult == XLLM_TRANSPORT_CANCELLED ? "cancelled" : "tls_connect"), pError);
            xrtFutureDestroy(pFuture);
            free(pConnection);
            return NULL;
        }
        pConnection->pTls = xrtTlsStreamRef((xtlsstream*)xrtFutureValue(pFuture));
        xrtFutureDestroy(pFuture);
        if ( !pConnection->pTls ) {
            xllm__transport_fail(pCall, "connect", XLLM_TRANSPORT_ERROR,
                "tls_connect", xrtGetError());
            free(pConnection);
            return NULL;
        }
    } else {
        xrtNetDialConfigInit(&tDial);
        pConnection->pTcp = xrtNetConnect(pClient->pNetEngine, pClient->pResolver,
            pClient->sHost, pClient->uPort, &tDial, NULL, NULL,
            pCall->uDeadline, pCall->pCancel);
        if ( !pConnection->pTcp ) {
            const xerror* pError = xrtGetError();
            xllm_transport_result eResult = XLLM_TRANSPORT_ERROR;
            if ( pError && xrtErrorKind(pError) == XERR_TIMEOUT ) eResult = XLLM_TRANSPORT_TIMEOUT;
            else if ( pError && xrtErrorKind(pError) == XERR_CANCELLED ) eResult = XLLM_TRANSPORT_CANCELLED;
            xllm__transport_fail(pCall, "connect", eResult,
                eResult == XLLM_TRANSPORT_TIMEOUT ? "timeout" :
                (eResult == XLLM_TRANSPORT_CANCELLED ? "cancelled" : "connect"), pError);
            free(pConnection);
            return NULL;
        }
    }
    pCall->tHttpDiagnostics.uConnectedMs = xllm__clock_ms();
    return pConnection;
}

static bool xllm__future_resolved(xllm_call* pCall, xfuture* pFuture,
    xdeadline iDeadline, const char* sPhase)
{
    xwaitresult eWait;
    if ( !pFuture ) {
        xllm__transport_fail(pCall, sPhase, XLLM_TRANSPORT_ERROR, "submit", xrtGetError());
        return false;
    }
    eWait = xrtFutureWaitUntilCancel(pFuture, iDeadline, pCall->pCancel);
    if ( eWait == XWAIT_OK && xrtFutureState(pFuture) == XFUTURE_RESOLVED ) return true;
    (void)xrtFutureCancel(pFuture);
    if ( eWait == XWAIT_TIMEOUT ) {
        xllm__transport_fail(pCall, sPhase, XLLM_TRANSPORT_TIMEOUT, "timeout", xrtFutureError(pFuture));
    } else if ( eWait == XWAIT_CANCELLED ) {
        xllm__transport_fail(pCall, sPhase, XLLM_TRANSPORT_CANCELLED, "cancelled", xrtFutureError(pFuture));
    } else {
        xllm__transport_fail(pCall, sPhase, XLLM_TRANSPORT_ERROR, "io", xrtFutureError(pFuture));
    }
    return false;
}

static bool xllm__connection_send(xllm_call* pCall,
    xllm_connection* pConnection, const void* pData, size_t iSize)
{
    const uint8_t* p = (const uint8_t*)pData;
    size_t iOffset = 0u;
    while ( iOffset < iSize ) {
        size_t iChunk = iSize - iOffset;
        if ( iChunk > XLLM_HTTP_IO_CHUNK ) iChunk = XLLM_HTTP_IO_CHUNK;
        if ( pConnection->bTls ) {
            xfuture* pFuture = xrtTlsStreamSendAsync(pConnection->pTls, p + iOffset, iChunk);
            bool bOk = xllm__future_resolved(pCall, pFuture, pCall->uDeadline, "send");
            xrtFutureDestroy(pFuture);
            if ( !bOk ) return false;
        } else {
            for ( ;; ) {
                xnetresult eResult = xrtNetStreamSend(pConnection->pTcp, p + iOffset, iChunk);
                if ( eResult == XNET_RESULT_OK ) break;
                if ( eResult != XNET_RESULT_AGAIN ||
                     !xrtNetStreamWait(pConnection->pTcp, XNET_STREAM_WAIT_WRITE,
                        pCall->uDeadline, pCall->pCancel) ) {
                    const xerror* pError = xrtGetError();
                    xllm_transport_result eTransport = XLLM_TRANSPORT_ERROR;
                    if ( pError && xrtErrorKind(pError) == XERR_TIMEOUT ) eTransport = XLLM_TRANSPORT_TIMEOUT;
                    else if ( pError && xrtErrorKind(pError) == XERR_CANCELLED ) eTransport = XLLM_TRANSPORT_CANCELLED;
                    xllm__transport_fail(pCall, "send", eTransport,
                        eTransport == XLLM_TRANSPORT_TIMEOUT ? "timeout" :
                        (eTransport == XLLM_TRANSPORT_CANCELLED ? "cancelled" : "send"), pError);
                    return false;
                }
            }
        }
        iOffset += iChunk;
    }
    pCall->tHttpDiagnostics.uRequestBytes += iSize;
    return true;
}

static xllm_receive_result xllm__connection_receive(xllm_call* pCall,
    xllm_connection* pConnection, xllm_buf* pBuffer)
{
    xdeadline iDeadline = xllm__idle_deadline(pCall);
    xbytesview tView;
    if ( pConnection->bTls ) {
        xfuture* pFuture = xrtTlsStreamRecvAsync(pConnection->pTls, XLLM_HTTP_IO_CHUNK);
        xwaitresult eWait;
        xfuturestate eState;
        if ( !pFuture ) {
            xllm__transport_fail(pCall, "receive", XLLM_TRANSPORT_ERROR, "receive", xrtGetError());
            return XLLM_RECEIVE_ERROR;
        }
        eWait = xrtFutureWaitUntilCancel(pFuture, iDeadline, pCall->pCancel);
        eState = xrtFutureState(pFuture);
        if ( eWait == XWAIT_OK && eState == XFUTURE_RESOLVED ) {
            xnetbytes* pBytes = (xnetbytes*)xrtFutureValue(pFuture);
            tView = xrtNetBytesView(pBytes);
            if ( tView.Size && !xllm__buf_append(pBuffer, tView.Data, tView.Size) ) {
                xllm__transport_fail(pCall, "receive", XLLM_TRANSPORT_ERROR, "out_of_memory", NULL);
                xrtFutureDestroy(pFuture);
                return XLLM_RECEIVE_ERROR;
            }
            xrtFutureDestroy(pFuture);
            return tView.Size ? XLLM_RECEIVE_DATA : XLLM_RECEIVE_END;
        }
        if ( eWait == XWAIT_OK && eState == XFUTURE_CLOSED ) {
            xrtFutureDestroy(pFuture);
            return XLLM_RECEIVE_END;
        }
        (void)xrtFutureCancel(pFuture);
        if ( eWait == XWAIT_TIMEOUT ) xllm__transport_fail(pCall, "receive", XLLM_TRANSPORT_TIMEOUT, "timeout", xrtFutureError(pFuture));
        else if ( eWait == XWAIT_CANCELLED ) xllm__transport_fail(pCall, "receive", XLLM_TRANSPORT_CANCELLED, "cancelled", xrtFutureError(pFuture));
        else xllm__transport_fail(pCall, "receive", XLLM_TRANSPORT_ERROR, "receive", xrtFutureError(pFuture));
        xrtFutureDestroy(pFuture);
        return XLLM_RECEIVE_ERROR;
    }
    {
        xnetbytes* pBytes = xrtNetStreamRecv(pConnection->pTcp, XLLM_HTTP_IO_CHUNK,
            iDeadline, pCall->pCancel);
        if ( !pBytes ) {
            const xerror* pError = xrtGetError();
            if ( xrtNetStreamState(pConnection->pTcp) == XNET_STREAM_CLOSED ) return XLLM_RECEIVE_END;
            if ( pError && xrtErrorKind(pError) == XERR_TIMEOUT )
                xllm__transport_fail(pCall, "receive", XLLM_TRANSPORT_TIMEOUT, "timeout", pError);
            else if ( pError && xrtErrorKind(pError) == XERR_CANCELLED )
                xllm__transport_fail(pCall, "receive", XLLM_TRANSPORT_CANCELLED, "cancelled", pError);
            else xllm__transport_fail(pCall, "receive", XLLM_TRANSPORT_ERROR, "receive", pError);
            return XLLM_RECEIVE_ERROR;
        }
        tView = xrtNetBytesView(pBytes);
        if ( tView.Size && !xllm__buf_append(pBuffer, tView.Data, tView.Size) ) {
            xrtNetBytesDestroy(pBytes);
            xllm__transport_fail(pCall, "receive", XLLM_TRANSPORT_ERROR, "out_of_memory", NULL);
            return XLLM_RECEIVE_ERROR;
        }
        xrtNetBytesDestroy(pBytes);
        return tView.Size ? XLLM_RECEIVE_DATA : XLLM_RECEIVE_END;
    }
}

static bool xllm__parse_url(xllm_client* pClient, xllm_error* pError)
{
    const char* sUrl = pClient->sBaseUrl;
    const char* sAuthority;
    const char* sPath;
    const char* sHostBegin;
    const char* sHostEnd;
    const char* sPort = NULL;
    size_t iHostLen;
    size_t iHeaderLen;
    unsigned long uPort;
    char* sEnd;
    if ( strncmp(sUrl, "https://", 8u) == 0 ) {
        pClient->bTls = true;
        sAuthority = sUrl + 8u;
        pClient->uPort = 443u;
    } else if ( strncmp(sUrl, "http://", 7u) == 0 ) {
        pClient->bTls = false;
        sAuthority = sUrl + 7u;
        pClient->uPort = 80u;
    } else {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "base URL must use http or https");
        return false;
    }
    sPath = strchr(sAuthority, '/');
    if ( !sPath ) sPath = sAuthority + strlen(sAuthority);
    if ( sPath == sAuthority || memchr(sAuthority, '@', (size_t)(sPath - sAuthority)) || strchr(sPath, '#') ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "base URL contains an invalid authority or fragment");
        return false;
    }
    sHostBegin = sAuthority;
    if ( *sHostBegin == '[' ) {
        sHostEnd = memchr(sHostBegin, ']', (size_t)(sPath - sHostBegin));
        if ( !sHostEnd || sHostEnd == sHostBegin + 1 ) goto invalid;
        ++sHostBegin;
        if ( sHostEnd + 1 < sPath ) {
            if ( sHostEnd[1] != ':' ) goto invalid;
            sPort = sHostEnd + 2;
        }
    } else {
        sHostEnd = memchr(sHostBegin, ':', (size_t)(sPath - sHostBegin));
        if ( !sHostEnd ) sHostEnd = sPath;
        else sPort = sHostEnd + 1;
    }
    iHostLen = (size_t)(sHostEnd - sHostBegin);
    if ( !iHostLen ) goto invalid;
    if ( sPort ) {
        if ( sPort >= sPath ) goto invalid;
        uPort = strtoul(sPort, &sEnd, 10);
        if ( sEnd != sPath || !uPort || uPort > 65535u ) goto invalid;
        pClient->uPort = (uint16_t)uPort;
    }
    pClient->sHost = (char*)malloc(iHostLen + 1u);
    pClient->sTarget = xllm__strdup(*sPath ? sPath : "/");
    iHeaderLen = (size_t)(sPath - sAuthority);
    pClient->sHostHeader = (char*)malloc(iHeaderLen + 1u);
    if ( !pClient->sHost || !pClient->sTarget || !pClient->sHostHeader ) goto oom;
    memcpy(pClient->sHost, sHostBegin, iHostLen);
    pClient->sHost[iHostLen] = 0;
    memcpy(pClient->sHostHeader, sAuthority, iHeaderLen);
    pClient->sHostHeader[iHeaderLen] = 0;
    return true;
invalid:
    xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "base URL contains an invalid host or port");
    return false;
oom:
    xllm__error_set(pError, XLLM_ERROR_OUT_OF_MEMORY, "failed to parse base URL");
    return false;
}

static xtlsverifydecision xllm__tls_accept(const xtlspeer* pPeer, ptr pData)
{
    (void)pPeer;
    (void)pData;
    return XTLS_VERIFY_ACCEPT;
}

bool xllm__transport_client_init(xllm_client* pClient, xllm_error* pError)
{
    xnetengineconfig tEngine;
    if ( !xllm__parse_url(pClient, pError) ) return false;
    pClient->pConnectionMutex = xrtMutexCreate();
    xrtNetEngineConfigInit(&tEngine);
    pClient->pNetEngine = xrtNetEngineCreate(&tEngine);
    if ( !pClient->pConnectionMutex || !pClient->pNetEngine ||
         !xrtNetEngineStart(pClient->pNetEngine) ) goto network_error;
    pClient->pResolver = xrtNetResolverCreate(NULL);
    if ( !pClient->pResolver ) goto network_error;
    if ( pClient->bTls ) {
        xtlsverifierconfig tVerify;
        xx509store* pStore = NULL;
        xrtTlsVerifierConfigInit(&tVerify);
        if ( pClient->bVerifyPeer ) {
            pStore = xrtX509StoreSystem();
            if ( !pStore ) goto network_error;
            tVerify.Store = pStore;
        } else {
            tVerify.Verify = xllm__tls_accept;
        }
        pClient->pVerifier = xrtTlsVerifierCreate(&tVerify);
        xrtX509StoreFree(pStore);
        if ( !pClient->pVerifier ) goto network_error;
    }
    return true;
network_error:
    xllm__error_set(pError, XLLM_ERROR_NETWORK, "failed to initialize XRT HTTP transport");
    xllm__transport_client_unit(pClient);
    return false;
}

void xllm__transport_client_unit(xllm_client* pClient)
{
    xllm_connection* pIdle = NULL;
    if ( !pClient ) return;
    if ( pClient->pConnectionMutex && xrtMutexLock(pClient->pConnectionMutex) ) {
        pIdle = pClient->pIdleConnection;
        pClient->pIdleConnection = NULL;
        (void)xrtMutexUnlock(pClient->pConnectionMutex);
    }
    xllm__connection_close(pIdle);
    if ( pClient->pResolver ) {
        (void)xrtNetResolverDestroy(pClient->pResolver);
        pClient->pResolver = NULL;
    }
    if ( pClient->pNetEngine ) {
        (void)xrtNetEngineStop(pClient->pNetEngine);
        (void)xrtNetEngineDestroy(pClient->pNetEngine);
        pClient->pNetEngine = NULL;
    }
    xrtTlsVerifierRelease(pClient->pVerifier);
    pClient->pVerifier = NULL;
    if ( pClient->pConnectionMutex ) {
        (void)xrtMutexDestroy(pClient->pConnectionMutex);
        pClient->pConnectionMutex = NULL;
    }
}

static bool xllm__write_request(xllm_call* pCall, xllm_connection* pConnection)
{
    xhttpfield tFields[7];
    size_t iFieldCount = 0u;
    size_t iHeaderSize = 0u;
    size_t iBodySize = strlen(pCall->sRequestBody);
    char sLength[32];
    char* sAuth = NULL;
    uint8_t* pHeader = NULL;
    bool bOk = false;
    (void)snprintf(sLength, sizeof(sLength), "%llu", (unsigned long long)iBodySize);
    tFields[iFieldCount++] = (xhttpfield){ xllm__sv("Host"), xllm__sv(pCall->pClient->sHostHeader) };
    tFields[iFieldCount++] = (xhttpfield){ xllm__sv("Accept"), xllm__sv("text/event-stream") };
    tFields[iFieldCount++] = (xhttpfield){ xllm__sv("Content-Type"), xllm__sv("application/json") };
    tFields[iFieldCount++] = (xhttpfield){ xllm__sv("Content-Length"), xllm__sv(sLength) };
    tFields[iFieldCount++] = (xhttpfield){ xllm__sv("User-Agent"), xllm__sv(pCall->pClient->sUserAgent) };
    tFields[iFieldCount++] = (xhttpfield){ xllm__sv("Connection"), xllm__sv("keep-alive") };
    if ( pCall->pClient->sApiKey[0] ) {
        size_t iKeyLen = strlen(pCall->pClient->sApiKey);
        sAuth = (char*)malloc(iKeyLen + 8u);
        if ( !sAuth ) goto done;
        memcpy(sAuth, "Bearer ", 7u);
        memcpy(sAuth + 7u, pCall->pClient->sApiKey, iKeyLen + 1u);
        tFields[iFieldCount++] = (xhttpfield){ xllm__sv("Authorization"), xllm__sv(sAuth) };
    }
    if ( !xrtHttp1RequestWrite(xllm__sv("POST"), xllm__sv(pCall->pClient->sTarget),
            XHTTP_VERSION_1_1, tFields, iFieldCount, NULL, 0u, &iHeaderSize) ) goto done;
    pHeader = (uint8_t*)malloc(iHeaderSize);
    if ( !pHeader || !xrtHttp1RequestWrite(xllm__sv("POST"), xllm__sv(pCall->pClient->sTarget),
            XHTTP_VERSION_1_1, tFields, iFieldCount, pHeader, iHeaderSize, &iHeaderSize) ) goto done;
    if ( !xllm__connection_send(pCall, pConnection, pHeader, iHeaderSize) ||
         !xllm__connection_send(pCall, pConnection, pCall->sRequestBody, iBodySize) ) goto done;
    pCall->tHttpDiagnostics.uRequestSentMs = xllm__clock_ms();
    bOk = true;
done:
    if ( !bOk && pCall->tHttpDiagnostics.eResult == XLLM_TRANSPORT_OK ) {
        xllm__transport_fail(pCall, "request", XLLM_TRANSPORT_ERROR, "request", xrtGetError());
    }
    free(pHeader);
    if ( sAuth ) memset(sAuth, 0, strlen(sAuth));
    free(sAuth);
    return bOk;
}

static bool xllm__response_reusable(const xhttp1head* pHead,
    const xhttp1bodyplan* pPlan)
{
    if ( pPlan->Mode == XHTTP1_BODY_CLOSE || (pHead->Flags & XHTTP1_CONNECTION_CLOSE) ) return false;
    if ( pHead->Version == XHTTP_VERSION_1_0 && !(pHead->Flags & XHTTP1_KEEP_ALIVE) ) return false;
    return true;
}

static xllm_transport_result xllm__read_response(xllm_call* pCall,
    xllm_connection* pConnection, bool* pReusable)
{
    xllm_buf tWire = {0};
    xhttpfield tFields[XLLM_HTTP_FIELD_LIMIT];
    xhttpfield tTrailers[XLLM_HTTP_TRAILER_LIMIT];
    xhttp1head tHead;
    xhttp1limits tHeadLimits;
    xhttp1bodyplan tPlan;
    xhttp1bodylimits tBodyLimits;
    xhttp1body tBody;
    xhttp1errorinfo tProtocolError;
    size_t iOffset = 0u;
    bool bEnd = false;
    bool bHeadReady = false;
    xllm_transport_result eResult = XLLM_TRANSPORT_ERROR;

    *pReusable = false;
    xrtHttp1LimitsInit(&tHeadLimits);
    tHeadLimits.MaxHead = XLLM_HTTP_HEAD_LIMIT;
    tHeadLimits.MaxFields = XLLM_HTTP_FIELD_LIMIT;
    xrtHttp1HeadInit(&tHead, tFields, XLLM_HTTP_FIELD_LIMIT);
    memset(&tProtocolError, 0, sizeof(tProtocolError));
    while ( !bHeadReady ) {
        xhttp1status eStatus = xrtHttp1ResponseParse(
            xllm__bv(tWire.pData, tWire.iLen), &tHead, &tHeadLimits, &tProtocolError);
        if ( eStatus == XHTTP1_READY ) {
            bHeadReady = true;
            break;
        }
        if ( eStatus == XHTTP1_ERROR || eStatus == XHTTP1_FIELDS || tWire.iLen >= XLLM_HTTP_HEAD_LIMIT ) {
            xllm__transport_fail(pCall, "headers", XLLM_TRANSPORT_ERROR, "http_protocol", NULL);
            goto done;
        }
        {
            xllm_receive_result eReceive = xllm__connection_receive(pCall, pConnection, &tWire);
            if ( eReceive == XLLM_RECEIVE_ERROR ) goto done;
            if ( eReceive == XLLM_RECEIVE_END ) {
                bEnd = true;
                xllm__transport_fail(pCall, "headers", XLLM_TRANSPORT_ERROR, "unexpected_eof", NULL);
                goto done;
            }
            if ( !pCall->tHttpDiagnostics.uFirstByteMs ) pCall->tHttpDiagnostics.uFirstByteMs = xllm__clock_ms();
        }
    }
    pCall->tHttpDiagnostics.uHeadersMs = xllm__clock_ms();
    if ( !xllm__transport_headers(pCall, &tHead) ) {
        xllm__transport_fail(pCall, "headers", XLLM_TRANSPORT_CANCELLED, "callback_cancelled", NULL);
        goto done;
    }
    if ( !xrtHttp1ResponseBodyPlan(&tHead, xllm__sv("POST"), &tPlan) ) {
        xllm__transport_fail(pCall, "headers", XLLM_TRANSPORT_ERROR, "http_framing", xrtGetError());
        goto done;
    }
    xrtHttp1BodyLimitsInit(&tBodyLimits);
    tBodyLimits.MaxBody = XLLM_MAX_FALLBACK_BODY;
    if ( !xrtHttp1BodyInit(&tBody, &tPlan, tTrailers,
            XLLM_HTTP_TRAILER_LIMIT, &tBodyLimits) ) {
        xllm__transport_fail(pCall, "body", XLLM_TRANSPORT_ERROR, "http_framing", xrtGetError());
        goto done;
    }
    iOffset = tHead.Bytes;
    for ( ;; ) {
        xbytesview tInput = xllm__bv(tWire.pData ? tWire.pData + iOffset : NULL,
            tWire.iLen - iOffset);
        size_t iConsumed = 0u;
        xbytesview tData = {0};
        xhttp1bodystatus eBody = xrtHttp1BodyRead(&tBody, tInput, bEnd,
            &iConsumed, &tData, &tProtocolError);
        iOffset += iConsumed;
        if ( eBody == XHTTP1_BODY_DATA ) {
            pCall->tHttpDiagnostics.uResponseBodyBytes += tData.Size;
            if ( !xllm__transport_body(pCall, tData.Data, tData.Size) ) {
                xllm__transport_fail(pCall, "body", XLLM_TRANSPORT_CANCELLED, "callback_cancelled", NULL);
                goto done;
            }
            continue;
        }
        if ( eBody == XHTTP1_BODY_DONE ) {
            *pReusable = xllm__response_reusable(&tHead, &tPlan) && iOffset == tWire.iLen;
            eResult = XLLM_TRANSPORT_OK;
            goto done;
        }
        if ( eBody == XHTTP1_BODY_ERROR || eBody == XHTTP1_BODY_FIELDS ) {
            xllm__transport_fail(pCall, "body", XLLM_TRANSPORT_ERROR, "http_protocol", NULL);
            goto done;
        }
        if ( iOffset ) {
            if ( iOffset < tWire.iLen ) memmove(tWire.pData, tWire.pData + iOffset, tWire.iLen - iOffset);
            tWire.iLen -= iOffset;
            if ( tWire.pData ) tWire.pData[tWire.iLen] = 0;
            iOffset = 0u;
        }
        {
            xllm_receive_result eReceive = xllm__connection_receive(pCall, pConnection, &tWire);
            if ( eReceive == XLLM_RECEIVE_ERROR ) goto done;
            if ( eReceive == XLLM_RECEIVE_END ) {
                if ( bEnd ) {
                    xllm__transport_fail(pCall, "body", XLLM_TRANSPORT_ERROR, "unexpected_eof", NULL);
                    goto done;
                }
                bEnd = true;
            }
        }
    }
done:
    xllm__buf_reset(&tWire);
    if ( eResult != XLLM_TRANSPORT_OK ) {
        eResult = pCall->tHttpDiagnostics.eResult;
    }
    return eResult;
}

xllm_transport_result xllm__transport_execute(xllm_call* pCall)
{
    xllm_connection* pConnection;
    bool bReusable = false;
    xllm_transport_result eResult;
    pCall->tHttpDiagnostics.uStartedMs = xllm__clock_ms();
    pCall->tHttpDiagnostics.eResult = XLLM_TRANSPORT_OK;
    xllm__copy_text(pCall->tHttpDiagnostics.sPhase,
        sizeof(pCall->tHttpDiagnostics.sPhase), "connect");
    pConnection = xllm__connection_take(pCall->pClient);
    if ( pConnection ) {
        pCall->tHttpDiagnostics.bReusedConnection = true;
        pCall->tHttpDiagnostics.uConnectedMs = pCall->tHttpDiagnostics.uStartedMs;
    } else {
        pConnection = xllm__connection_open(pCall);
    }
    if ( !pConnection ) return pCall->tHttpDiagnostics.eResult;
    if ( !xllm__write_request(pCall, pConnection) ) {
        eResult = pCall->tHttpDiagnostics.eResult;
        xllm__connection_release(pCall->pClient, pConnection, false);
        return eResult;
    }
    eResult = xllm__read_response(pCall, pConnection, &bReusable);
    pCall->tHttpDiagnostics.uCompletedMs = xllm__clock_ms();
    xllm__connection_release(pCall->pClient, pConnection,
        eResult == XLLM_TRANSPORT_OK && bReusable);
    pCall->tHttpDiagnostics.eResult = eResult;
    if ( eResult == XLLM_TRANSPORT_OK ) {
        xllm__copy_text(pCall->tHttpDiagnostics.sPhase,
            sizeof(pCall->tHttpDiagnostics.sPhase), "complete");
        xllm__copy_text(pCall->tHttpDiagnostics.sError,
            sizeof(pCall->tHttpDiagnostics.sError), "none");
    }
    return eResult;
}
