#ifndef XLLM_INTERNAL_H
#define XLLM_INTERNAL_H

#include "../xllm.h"
#include "../xllm-xrt.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_MSC_VER)
#include <intrin.h>
#endif

#define XLLM_MAX_FALLBACK_BODY (64u * 1024u * 1024u)

typedef struct xllm_buf {
    char* pData;
    size_t iLen;
    size_t iCap;
} xllm_buf;

typedef enum xllm_transport_result {
    XLLM_TRANSPORT_OK = 0,
    XLLM_TRANSPORT_ERROR,
    XLLM_TRANSPORT_TIMEOUT,
    XLLM_TRANSPORT_CANCELLED
} xllm_transport_result;

typedef struct xllm_transport_diagnostics {
    xllm_transport_result eResult;
    int32_t iSystemError;
    uint64_t uStartedMs;
    uint64_t uConnectedMs;
    uint64_t uRequestSentMs;
    uint64_t uFirstByteMs;
    uint64_t uHeadersMs;
    uint64_t uCompletedMs;
    uint64_t uRequestBytes;
    uint64_t uResponseBodyBytes;
    uint64_t uEffectiveTimeoutMs;
    bool bReusedConnection;
    char sError[32];
    char sPhase[32];
} xllm_transport_diagnostics;

typedef struct xllm_connection xllm_connection;

struct xllm_client {
    char* sBaseUrl;
    char* sApiKey;
    char* sModel;
    char* sReasoningEffort;
    char* sUserAgent;
    uint32_t uMaxOutputTokens;
    uint32_t uTimeoutMs;
    uint32_t uIdleTimeoutMs;
    uint32_t uMaxAttempts;
    uint32_t uRetryBaseDelayMs;
    uint32_t uRetryMaxDelayMs;
    bool bVerifyPeer;
    xllm_provider eProvider;
    xllm_model_profile tModelProfile;
    char* sProfileId;
    bool bHasModelProfile;
    bool bTls;
    char* sHost;
    char* sTarget;
    char* sHostHeader;
    uint16_t uPort;
    xnetengine* pNetEngine;
    xnetresolver* pResolver;
    xtlsverifier* pVerifier;
    xmutex* pConnectionMutex;
    xllm_connection* pIdleConnection;
};

struct xllm_call {
    xllm_client* pClient;
    xthread* pThread;
    xcancel* pCancel;
    char* sRequestBody;
    uint64_t uDeadline;
    uint64_t uScopeDeadline;
    bool bScopeAttached;
    xllm_transport_result eTransportResult;
    xllm_response* pResponse;
    xllm_stream_callbacks tCallbacks;
    xllm_error tError;
    xllm_transport_diagnostics tHttpDiagnostics;
    xllm_buf tLine;
    xllm_buf tEventData;
    xllm_buf tRawBody;
    uint32_t uHttpStatus;
    uint32_t uAttempt;
    uint32_t uRetryAfterMs;
    char sRequestId[160];
    char sContentType[160];
    char* sSelectedModel;
    volatile long iCallbackActive;
    volatile long iClosing;
    bool bSse;
    bool bDone;
    bool bSawEvent;
    bool bCallbackCancelled;
    bool bWaited;
    bool bResponseTaken;
};

char* xllm__strdup(const char* sText);
bool xllm__replace(char** ppDst, const char* sText);
bool xllm__buf_reserve(xllm_buf* pBuf, size_t iNeed);
bool xllm__buf_append(xllm_buf* pBuf, const void* pData, size_t iLen);
bool xllm__buf_append_cstr(xllm_buf* pBuf, const char* sText);
bool xllm__buf_append_char(xllm_buf* pBuf, char ch);
void xllm__buf_reset(xllm_buf* pBuf);
char* xllm__buf_detach(xllm_buf* pBuf);
bool xllm__json_string(xllm_buf* pBuf, const char* sText);
void xllm__error_set(xllm_error* pError, xllm_error_code eCode, const char* sMessage);
void xllm__error_copy(xllm_error* pDst, const xllm_error* pSrc);
void xllm__copy_text(char* sDst, size_t iCap, const char* sSrc);
void xllm__copy_view(char* sDst, size_t iCap, xstrview tValue);
bool xllm__contains_ci(const char* sText, const char* sNeedle);

bool xllm__tool_call_clone(xllm_tool_call* pDst, const xllm_tool_call* pSrc);
void xllm__tool_call_unit(xllm_tool_call* pCall);
bool xllm__message_clone(xllm_message* pDst, const xllm_message* pSrc);
void xllm__tool_unit(xllm_tool* pTool);

bool xllm__response_ensure_tool(xllm_response* pResponse, size_t iIndex);
bool xllm__response_append_text(char** ppText, const char* sDelta, size_t iLen);
xllm_response* xllm__ensure_response(xllm_call* pCall);

char* xllm__build_request_json(xllm_client* pClient, const xllm_request* pRequest, xllm_error* pError);
bool xllm__parse_stream_event(xllm_call* pCall, const char* sData, size_t iLen);
bool xllm__sse_feed(xllm_call* pCall, const void* pData, size_t iLen);
bool xllm__sse_finish(xllm_call* pCall);
bool xllm__parse_json_response(xllm_call* pCall, const char* sData, size_t iLen);
void xllm__parse_error_body(xllm_call* pCall, const char* sData, size_t iLen);

bool xllm__emit(xllm_call* pCall, const xllm_event* pEvent);
bool xllm__transport_headers(xllm_call* pCall, const xhttp1head* pHead);
bool xllm__transport_body(xllm_call* pCall, const void* pData, size_t iLen);
bool xllm__transport_client_init(xllm_client* pClient, xllm_error* pError);
void xllm__transport_client_unit(xllm_client* pClient);
xllm_transport_result xllm__transport_execute(xllm_call* pCall);

static inline long xllm__atomic_add(volatile long* pValue, long iDelta)
{
#if defined(_MSC_VER)
    return _InterlockedExchangeAdd(pValue, iDelta) + iDelta;
#elif defined(__GNUC__) || defined(__clang__)
    return __atomic_add_fetch(pValue, iDelta, __ATOMIC_SEQ_CST);
#else
    *pValue += iDelta;
    return *pValue;
#endif
}

static inline long xllm__atomic_load(volatile long* pValue)
{
#if defined(_MSC_VER)
    return _InterlockedCompareExchange(pValue, 0, 0);
#elif defined(__GNUC__) || defined(__clang__)
    return __atomic_load_n(pValue, __ATOMIC_SEQ_CST);
#else
    return *pValue;
#endif
}

static inline void xllm__atomic_store(volatile long* pValue, long iValue)
{
#if defined(_MSC_VER)
    (void)_InterlockedExchange(pValue, iValue);
#elif defined(__GNUC__) || defined(__clang__)
    __atomic_store_n(pValue, iValue, __ATOMIC_SEQ_CST);
#else
    *pValue = iValue;
#endif
}

#endif
