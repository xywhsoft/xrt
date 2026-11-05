#ifndef XLLM_PROXY_FUTURE_H
#define XLLM_PROXY_FUTURE_H
#include <xrt/future_bridge.h>
/* TLS Dial Future 桥接一个受管 TLS Dial 与一个公开 Future。 */
typedef struct xllm_proxy_future
{
    xfuturebridge Bridge;
    volatile int32 References;
    xtlsdial *Dial;
} xllm_proxy_future;

/* Future 成功值只持有一个 TLS Stream 调用方引用。 */
static void xllm__proxy_future_Free(ptr pValue, ptr pData)
{
    (void)pData;
    xrtTlsStreamDestroy((xtlsstream *)pValue);
}

/* Future 的协作取消请求转发给 DNS、TCP 或 TLS 当前有效阶段。 */
static void xllm__proxy_future_Cancel(ptr pData)
{
    xllm_proxy_future *pContext = (xllm_proxy_future *)pData;

    (void)xrtTlsDialCancel(pContext->Dial);
}

/* 把不可能出现的空成功结果转换为结构化 TLS 内部错误。 */
static xerror *xllm__proxy_future_InvalidError(void)
{
    xerrordesc d;
    memset(&d, 0, sizeof(d));
    d.Kind = XERR_INTERNAL;
    d.Domain = "xllm.proxy";
    d.Operation = "complete-tls-proxy-dial";
    d.Message = "TLS proxy dial reported success without a stream";
    return xrtErrorBuild(&d);
}

/* 把 TLS Dial 唯一终态转发到 Promise，并处理装配失败竞态。 */
static void xllm__proxy_future_Done(xtlsdial *pDial, xnetresult Result, xtlsstream *pStream,
                                    const xerror *pError, ptr pData)
{
    xllm_proxy_future *pContext = (xllm_proxy_future *)pData;
    xpromise *pPromise = xrtFutureBridgePromise(&pContext->Bridge);
    xtlsdial *pHeld;
    xerror *pFailure = NULL;
    bool bReady;

    (void)pDial;
    bReady = xrtFutureBridgeWait(&pContext->Bridge);
    /* The submitter publishes Dial before releasing the setup gate. */
    pHeld = pContext->Dial;
    xrtFutureBridgeUnwatch(&pContext->Bridge);
    if ((Result != XNET_RESULT_OK) && (pError != NULL))
    {
        pFailure = xrtErrorRef(pError);
    }
    else if ((Result == XNET_RESULT_OK) && (pStream == NULL))
    {
        pFailure = xllm__proxy_future_InvalidError();
    }
    xrtTlsDialDestroy(pHeld);
    if (xrtRefRelease(&pContext->References) == 0)
    {
        xrtFree(pContext);
    }
    if (!bReady)
    {
        if (pStream != NULL)
        {
            (void)xrtTlsStreamAbort(pStream);
            xrtTlsStreamDestroy(pStream);
        }
    }
    else if ((Result == XNET_RESULT_OK) && (pStream != NULL))
    {
        if (!xrtPromiseResolveOwned(pPromise, pStream, xllm__proxy_future_Free, NULL))
        {
            (void)xrtTlsStreamAbort(pStream);
            xrtTlsStreamDestroy(pStream);
        }
    }
    else if (Result == XNET_RESULT_CANCELLED)
    {
        (void)xrtPromiseCancel(pPromise);
    }
    else if (pFailure != NULL)
    {
        (void)xrtPromiseReject(pPromise, pFailure);
    }
    else
    {
        (void)xrtPromiseClose(pPromise);
    }
    xrtErrorFree(pFailure);
    xrtPromiseDestroy(pPromise);
}

/* 创建受管 TLS Dial、Promise 和双向取消桥接。 */
static xfuture *xllm__proxy_dial_async(xnetengine *pEngine, xnetresolver *pResolver,
                                       const xnetproxy *pProxy, cstr sHost, uint16 iPort,
                                       const xtlsclientconfig *pTls, const xtlsdialconfig *pConfig,
                                       const xtlsstreamevents *pStreamEvents, ptr pStreamData)
{
    xllm_proxy_future *pContext;
    xfuture *pFuture;
    xerror *pError;

    pContext = (xllm_proxy_future *)xrtCalloc(1, sizeof(*pContext));
    if (pContext == NULL)
    {
        return NULL;
    }
    pFuture = xrtFutureBridgeCreate(&pContext->Bridge, NULL);
    if (pFuture == NULL)
    {
        xrtFree(pContext);
        return NULL;
    }
    pContext->References = 2;
    pContext->Dial = xrtTlsDialProxy(pEngine, pResolver, pProxy, sHost, iPort, pTls, pConfig,
                                     pStreamEvents, pStreamData, xllm__proxy_future_Done, pContext);
    if (pContext->Dial == NULL)
    {
        pError = xrtTakeError();
        xrtFutureDestroy(pFuture);
        xrtPromiseDestroy(xrtFutureBridgePromise(&pContext->Bridge));
        xrtFree(pContext);
        if (pError != NULL)
        {
            xrtSetError(pError);
            xrtErrorFree(pError);
        }
        return NULL;
    }
    if (!xrtFutureBridgeWatch(&pContext->Bridge, xllm__proxy_future_Cancel, pContext))
    {
        pError = xrtTakeError();
        (void)xrtFutureBridgeFail(&pContext->Bridge);
        (void)xrtTlsDialCancel(pContext->Dial);
        xrtFutureDestroy(pFuture);
        if (pError != NULL)
        {
            xrtSetError(pError);
            xrtErrorFree(pError);
        }
        if (xrtRefRelease(&pContext->References) == 0)
        {
            xrtFree(pContext);
        }
        return NULL;
    }
    (void)xrtFutureBridgeReady(&pContext->Bridge);
    if (xrtRefRelease(&pContext->References) == 0)
    {
        xrtFree(pContext);
    }
    return pFuture;
}

#endif
