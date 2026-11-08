#include "../internal/xllm_internal.h"

#if defined(XLLM_TEST_ALLOCATOR)
static void *xllm__default_alloc(size_t iSize) { return malloc(iSize); }
static void *xllm__default_realloc(void *pMemory, size_t iSize) { return realloc(pMemory, iSize); }
static void xllm__default_free(void *pMemory) { free(pMemory); }

static xllm_allocator xllm__g_allocator = {xllm__default_alloc, xllm__default_realloc,
                                           xllm__default_free};
static volatile long xllm__g_allocations;
#endif

/* Strict UTF-8 scalar validation: rejects overlong forms, surrogates,
 * and code points beyond U+10FFFF. Message content rides into provider JSON
 * verbatim, so invalid UTF-8 must stop at the API boundary. */
bool xllm__utf8_valid(const char *sText)
{
    const unsigned char *p = (const unsigned char *)(sText ? sText : "");
    while (*p)
    {
        unsigned char c = *p;
        size_t iExtra;
        uint32_t cp;
        uint32_t uMinimum;
        if ( c < 0x80u ) { ++p; continue; }
        if ( c >= 0xC2u && c <= 0xDFu ) { iExtra = 1u; cp = c & 0x1Fu; uMinimum = 0x80u; }
        else if ( c >= 0xE0u && c <= 0xEFu ) { iExtra = 2u; cp = c & 0x0Fu; uMinimum = 0x800u; }
        else if ( c >= 0xF0u && c <= 0xF4u ) { iExtra = 3u; cp = c & 0x07u; uMinimum = 0x10000u; }
        else { return false; }
        ++p;
        while (iExtra--)
        {
            if ((*p & 0xC0u) != 0x80u)
            {
                return false;
            }
            cp = (cp << 6u) | (unsigned char)(*p & 0x3Fu);
            ++p;
        }
        if ( cp < uMinimum || (cp >= 0xD800u && cp <= 0xDFFFu) || cp > 0x10FFFFu ) { return false; }
    }
    return true;
}

#if defined(XLLM_TEST_ALLOCATOR)
void xllm__set_allocator(const xllm_allocator *pAllocator)
{
    if (pAllocator)
    {
        xllm__g_allocator = *pAllocator;
    }
    else
    {
        xllm__g_allocator.Alloc = xllm__default_alloc;
        xllm__g_allocator.Realloc = xllm__default_realloc;
        xllm__g_allocator.Free = xllm__default_free;
    }
    xllm__atomic_store(&xllm__g_allocations, 0);
}

size_t xllm__allocation_count(void) { return (size_t)xllm__atomic_load(&xllm__g_allocations); }
#endif

void *xllm__malloc(size_t iSize)
{
#if defined(XLLM_TEST_ALLOCATOR)
    (void)xllm__atomic_add(&xllm__g_allocations, 1);
    return xllm__g_allocator.Alloc(iSize);
#else
    return malloc(iSize);
#endif
}

void *xllm__calloc(size_t iCount, size_t iSize)
{
    void *pMemory;
    if (iCount && iSize > SIZE_MAX / iCount)
    {
        return NULL;
    }
#if defined(XLLM_TEST_ALLOCATOR)
    (void)xllm__atomic_add(&xllm__g_allocations, 1);
    pMemory = xllm__g_allocator.Alloc(iCount * iSize);
#else
    pMemory = malloc(iCount * iSize);
#endif
    if (pMemory)
    {
        memset(pMemory, 0, iCount * iSize);
    }
    return pMemory;
}

void *xllm__realloc(void *pMemory, size_t iSize)
{
#if defined(XLLM_TEST_ALLOCATOR)
    return xllm__g_allocator.Realloc(pMemory, iSize);
#else
    return realloc(pMemory, iSize);
#endif
}

void xllm__free(void *pMemory)
{
#if defined(XLLM_TEST_ALLOCATOR)
    xllm__g_allocator.Free(pMemory);
#else
    free(pMemory);
#endif
}

char *xllm__strdup(const char *sText)
{
    size_t iLen;
    char *sCopy;
    if (!sText)
    {
        return NULL;
    }
    iLen = strlen(sText);
    sCopy = (char *)xllm__malloc(iLen + 1u);
    if (!sCopy)
    {
        return NULL;
    }
    memcpy(sCopy, sText, iLen + 1u);
    return sCopy;
}

bool xllm__replace(char **ppDst, const char *sText)
{
    char *sCopy = sText ? xllm__strdup(sText) : NULL;
    if (sText && !sCopy)
    {
        return false;
    }
    xllm__free(*ppDst);
    *ppDst = sCopy;
    return true;
}

bool xllm__buf_reserve(xllm_buf *pBuf, size_t iNeed)
{
    size_t iCap;
    char *pNew;
    if (!pBuf)
    {
        return false;
    }
    if (iNeed <= pBuf->iCap)
    {
        return true;
    }
    iCap = pBuf->iCap ? pBuf->iCap : 256u;
    while (iCap < iNeed)
    {
        if (iCap > SIZE_MAX / 2u)
        {
            iCap = iNeed;
            break;
        }
        iCap *= 2u;
    }
    if (iCap < iNeed)
    {
        return false;
    }
    pNew = (char *)xllm__realloc(pBuf->pData, iCap);
    if (!pNew)
    {
        return false;
    }
    pBuf->pData = pNew;
    pBuf->iCap = iCap;
    return true;
}

bool xllm__buf_append(xllm_buf *pBuf, const void *pData, size_t iLen)
{
    if (!pBuf || (!pData && iLen) || pBuf->iLen == SIZE_MAX || iLen > SIZE_MAX - pBuf->iLen - 1u)
    {
        return false;
    }
    if (!xllm__buf_reserve(pBuf, pBuf->iLen + iLen + 1u))
    {
        return false;
    }
    if (iLen)
    {
        memcpy(pBuf->pData + pBuf->iLen, pData, iLen);
    }
    pBuf->iLen += iLen;
    pBuf->pData[pBuf->iLen] = '\0';
    return true;
}

bool xllm__buf_append_cstr(xllm_buf *pBuf, const char *sText)
{
    return xllm__buf_append(pBuf, sText ? sText : "", sText ? strlen(sText) : 0u);
}

bool xllm__buf_append_char(xllm_buf *pBuf, char ch) { return xllm__buf_append(pBuf, &ch, 1u); }

void xllm__buf_reset(xllm_buf *pBuf)
{
    if (!pBuf)
    {
        return;
    }
    xllm__free(pBuf->pData);
    memset(pBuf, 0, sizeof(*pBuf));
}

char *xllm__buf_detach(xllm_buf *pBuf)
{
    char *pData;
    if (!pBuf)
    {
        return NULL;
    }
    if (!pBuf->pData)
    {
        pBuf->pData = (char *)xllm__calloc(1u, 1u);
        if (!pBuf->pData)
        {
            return NULL;
        }
    }
    pData = pBuf->pData;
    pBuf->pData = NULL;
    pBuf->iLen = 0u;
    pBuf->iCap = 0u;
    return pData;
}

bool xllm__json_string_content(xllm_buf *pBuf, const char *sText)
{
    const unsigned char *p = (const unsigned char *)(sText ? sText : "");
    char sEscape[7];
    while (*p)
    {
        switch (*p)
        {
        case '"':
            if (!xllm__buf_append_cstr(pBuf, "\\\""))
                return false;
            break;
        case '\\':
            if (!xllm__buf_append_cstr(pBuf, "\\\\"))
                return false;
            break;
        case '\b':
            if (!xllm__buf_append_cstr(pBuf, "\\b"))
                return false;
            break;
        case '\f':
            if (!xllm__buf_append_cstr(pBuf, "\\f"))
                return false;
            break;
        case '\n':
            if (!xllm__buf_append_cstr(pBuf, "\\n"))
                return false;
            break;
        case '\r':
            if (!xllm__buf_append_cstr(pBuf, "\\r"))
                return false;
            break;
        case '\t':
            if (!xllm__buf_append_cstr(pBuf, "\\t"))
                return false;
            break;
        default:
            if (*p < 0x20u)
            {
                (void)snprintf(sEscape, sizeof(sEscape), "\\u%04x", (unsigned)*p);
                if (!xllm__buf_append_cstr(pBuf, sEscape))
                    return false;
            }
            else if (!xllm__buf_append(pBuf, p, 1u))
            {
                return false;
            }
            break;
        }
        ++p;
    }
    return true;
}

bool xllm__json_string(xllm_buf *pBuf, const char *sText)
{
    return xllm__buf_append_char(pBuf, '"') && xllm__json_string_content(pBuf, sText) &&
           xllm__buf_append_char(pBuf, '"');
}

void xllm__copy_text(char *sDst, size_t iCap, const char *sSrc)
{
    size_t iLen;
    if (!sDst || iCap == 0u)
    {
        return;
    }
    if (!sSrc)
    {
        sDst[0] = '\0';
        return;
    }
    iLen = strlen(sSrc);
    if (iLen >= iCap)
    {
        iLen = iCap - 1u;
    }
    memcpy(sDst, sSrc, iLen);
    sDst[iLen] = '\0';
}

void xllm__copy_view(char *sDst, size_t iCap, xstrview tValue)
{
    size_t iCopy;
    if (!sDst || !iCap)
        return;
    iCopy = tValue.Size < iCap - 1u ? tValue.Size : iCap - 1u;
    if (iCopy)
        memcpy(sDst, tValue.Data, iCopy);
    sDst[iCopy] = 0;
}

void xllmErrorInit(xllm_error *pError)
{
    if (pError)
    {
        memset(pError, 0, sizeof(*pError));
        pError->tCause.Size = (uint32_t)sizeof(pError->tCause);
        pError->tCause.Version = XLLM_ERROR_CAUSE_VERSION;
    }
}

static xllm_error_domain xllm__error_default_domain(xllm_error_code eCode)
{
    switch (eCode)
    {
    case XLLM_ERROR_INVALID_ARGUMENT:
        return XLLM_ERROR_DOMAIN_API;
    case XLLM_ERROR_OUT_OF_MEMORY:
        return XLLM_ERROR_DOMAIN_MEMORY;
    case XLLM_ERROR_NETWORK:
    case XLLM_ERROR_TIMEOUT:
    case XLLM_ERROR_CANCELLED:
        return XLLM_ERROR_DOMAIN_TRANSPORT;
    case XLLM_ERROR_AUTH:
    case XLLM_ERROR_RATE_LIMIT:
    case XLLM_ERROR_MODEL_NOT_FOUND:
    case XLLM_ERROR_UPSTREAM:
        return XLLM_ERROR_DOMAIN_PROVIDER;
    case XLLM_ERROR_INCOMPLETE_RESPONSE:
    case XLLM_ERROR_OUTPUT_LIMIT:
    case XLLM_ERROR_PROTOCOL:
    case XLLM_ERROR_PARSE:
        return XLLM_ERROR_DOMAIN_PARSER;
    case XLLM_ERROR_HOOK:
        return XLLM_ERROR_DOMAIN_HOOK;
    case XLLM_ERROR_LIMIT:
        return XLLM_ERROR_DOMAIN_RESOURCE;
    case XLLM_ERROR_NONE:
    default:
        return XLLM_ERROR_DOMAIN_NONE;
    }
}

static xllm_error_stage xllm__error_default_stage(xllm_error_code eCode)
{
    switch (eCode)
    {
    case XLLM_ERROR_INVALID_ARGUMENT:
        return XLLM_ERROR_STAGE_VALIDATE;
    case XLLM_ERROR_OUT_OF_MEMORY:
        return XLLM_ERROR_STAGE_NONE;
    case XLLM_ERROR_NETWORK:
        return XLLM_ERROR_STAGE_CONNECT;
    case XLLM_ERROR_TIMEOUT:
    case XLLM_ERROR_CANCELLED:
        return XLLM_ERROR_STAGE_WAIT;
    case XLLM_ERROR_AUTH:
    case XLLM_ERROR_RATE_LIMIT:
    case XLLM_ERROR_MODEL_NOT_FOUND:
    case XLLM_ERROR_UPSTREAM:
        return XLLM_ERROR_STAGE_PROVIDER;
    case XLLM_ERROR_INCOMPLETE_RESPONSE:
    case XLLM_ERROR_OUTPUT_LIMIT:
    case XLLM_ERROR_PROTOCOL:
    case XLLM_ERROR_PARSE:
        return XLLM_ERROR_STAGE_PARSE;
    case XLLM_ERROR_HOOK:
        return XLLM_ERROR_STAGE_HOOK;
    case XLLM_ERROR_LIMIT:
        return XLLM_ERROR_STAGE_LIMIT;
    case XLLM_ERROR_NONE:
    default:
        return XLLM_ERROR_STAGE_NONE;
    }
}

static const char *xllm__error_default_operation(xllm_error_code eCode)
{
    switch (eCode)
    {
    case XLLM_ERROR_INVALID_ARGUMENT:
        return "validate";
    case XLLM_ERROR_OUT_OF_MEMORY:
        return "allocate";
    case XLLM_ERROR_NETWORK:
        return "transport";
    case XLLM_ERROR_TIMEOUT:
    case XLLM_ERROR_CANCELLED:
        return "wait";
    case XLLM_ERROR_AUTH:
    case XLLM_ERROR_RATE_LIMIT:
    case XLLM_ERROR_MODEL_NOT_FOUND:
    case XLLM_ERROR_UPSTREAM:
        return "provider-response";
    case XLLM_ERROR_INCOMPLETE_RESPONSE:
    case XLLM_ERROR_OUTPUT_LIMIT:
    case XLLM_ERROR_PROTOCOL:
    case XLLM_ERROR_PARSE:
        return "parse-response";
    case XLLM_ERROR_HOOK:
        return "host-hook";
    case XLLM_ERROR_LIMIT:
        return "enforce-limit";
    case XLLM_ERROR_NONE:
    default:
        return "";
    }
}

void xllm__error_cause_set(xllm_error *pError, xllm_error_domain eDomain, xllm_error_stage eStage,
                           int32_t iRuntimeKind, int32_t iRuntimeCode, int32_t iSystemCode,
                           const char *sOperation, const char *sMessage)
{
    if (!pError)
    {
        return;
    }
    memset(&pError->tCause, 0, sizeof(pError->tCause));
    pError->tCause.Size = (uint32_t)sizeof(pError->tCause);
    pError->tCause.Version = XLLM_ERROR_CAUSE_VERSION;
    pError->tCause.eDomain = eDomain;
    pError->tCause.eStage = eStage;
    pError->tCause.iRuntimeKind = iRuntimeKind;
    pError->tCause.iRuntimeCode = iRuntimeCode;
    pError->tCause.iSystemCode = iSystemCode;
    xllm__copy_text(pError->tCause.sOperation, sizeof(pError->tCause.sOperation), sOperation);
    xllm__copy_text(pError->tCause.sMessage, sizeof(pError->tCause.sMessage), sMessage);
}

void xllm__error_set(xllm_error *pError, xllm_error_code eCode, const char *sMessage)
{
    bool bKeepRuntimeCause;
    if (!pError)
    {
        return;
    }
    bKeepRuntimeCause = pError->tCause.Size == sizeof(pError->tCause) &&
                        pError->tCause.Version == XLLM_ERROR_CAUSE_VERSION &&
                        pError->tCause.eDomain == XLLM_ERROR_DOMAIN_TRANSPORT &&
                        (eCode == XLLM_ERROR_NETWORK || eCode == XLLM_ERROR_TIMEOUT ||
                         eCode == XLLM_ERROR_CANCELLED);
    pError->eCode = eCode;
    xllm__copy_text(pError->sMessage, sizeof(pError->sMessage), sMessage);
    if (!bKeepRuntimeCause)
    {
        xllm__error_cause_set(pError, xllm__error_default_domain(eCode),
                              xllm__error_default_stage(eCode), 0, 0, 0,
                              xllm__error_default_operation(eCode), sMessage);
    }
}

void xllm__error_copy(xllm_error *pDst, const xllm_error *pSrc)
{
    if (pDst)
    {
        if (pSrc)
        {
            memcpy(pDst, pSrc, sizeof(*pDst));
        }
        else
        {
            xllmErrorInit(pDst);
        }
    }
}

const char *xllmErrorCodeName(xllm_error_code eCode)
{
    switch (eCode)
    {
    case XLLM_ERROR_NONE:
        return "none";
    case XLLM_ERROR_INVALID_ARGUMENT:
        return "invalid_argument";
    case XLLM_ERROR_OUT_OF_MEMORY:
        return "out_of_memory";
    case XLLM_ERROR_NETWORK:
        return "network";
    case XLLM_ERROR_TIMEOUT:
        return "timeout";
    case XLLM_ERROR_CANCELLED:
        return "cancelled";
    case XLLM_ERROR_AUTH:
        return "auth";
    case XLLM_ERROR_RATE_LIMIT:
        return "rate_limit";
    case XLLM_ERROR_MODEL_NOT_FOUND:
        return "model_not_found";
    case XLLM_ERROR_UPSTREAM:
        return "upstream";
    case XLLM_ERROR_PROTOCOL:
        return "protocol";
    case XLLM_ERROR_INCOMPLETE_RESPONSE:
        return "incomplete_response";
    case XLLM_ERROR_OUTPUT_LIMIT:
        return "output_limit";
    case XLLM_ERROR_PARSE:
        return "parse";
    case XLLM_ERROR_LIMIT:
        return "limit";
    case XLLM_ERROR_HOOK:
        return "hook";
    default:
        return "unknown";
    }
}

const char *xllmErrorDomainName(xllm_error_domain eDomain)
{
    switch (eDomain)
    {
    case XLLM_ERROR_DOMAIN_NONE:
        return "none";
    case XLLM_ERROR_DOMAIN_API:
        return "xllm.api";
    case XLLM_ERROR_DOMAIN_MEMORY:
        return "xllm.memory";
    case XLLM_ERROR_DOMAIN_TRANSPORT:
        return "xllm.transport";
    case XLLM_ERROR_DOMAIN_PROVIDER:
        return "xllm.provider";
    case XLLM_ERROR_DOMAIN_PARSER:
        return "xllm.parser";
    case XLLM_ERROR_DOMAIN_HOOK:
        return "xllm.hook";
    case XLLM_ERROR_DOMAIN_RESOURCE:
        return "xllm.resource";
    default:
        return "unknown";
    }
}

const char *xllmErrorStageName(xllm_error_stage eStage)
{
    switch (eStage)
    {
    case XLLM_ERROR_STAGE_NONE:
        return "none";
    case XLLM_ERROR_STAGE_VALIDATE:
        return "validate";
    case XLLM_ERROR_STAGE_BUILD_REQUEST:
        return "build_request";
    case XLLM_ERROR_STAGE_RESOLVE:
        return "resolve";
    case XLLM_ERROR_STAGE_CONNECT:
        return "connect";
    case XLLM_ERROR_STAGE_PROXY:
        return "proxy";
    case XLLM_ERROR_STAGE_TLS:
        return "tls";
    case XLLM_ERROR_STAGE_WRITE:
        return "write";
    case XLLM_ERROR_STAGE_READ:
        return "read";
    case XLLM_ERROR_STAGE_HTTP:
        return "http";
    case XLLM_ERROR_STAGE_PARSE:
        return "parse";
    case XLLM_ERROR_STAGE_PROVIDER:
        return "provider";
    case XLLM_ERROR_STAGE_HOOK:
        return "hook";
    case XLLM_ERROR_STAGE_LIMIT:
        return "limit";
    case XLLM_ERROR_STAGE_WAIT:
        return "wait";
    default:
        return "unknown";
    }
}

const char *xllmFinishReasonName(xllm_finish eFinish)
{
    switch (eFinish)
    {
    case XLLM_FINISH_STOP:
        return "stop";
    case XLLM_FINISH_LENGTH:
        return "length";
    case XLLM_FINISH_TOOL_CALLS:
        return "tool_calls";
    case XLLM_FINISH_CONTENT_FILTER:
        return "content_filter";
    case XLLM_FINISH_REFUSAL:
        return "refusal";
    case XLLM_FINISH_OTHER:
        return "other";
    default:
        return "unknown";
    }
}

bool xllmErrorRetryable(const xllm_error *pError)
{
    int32_t iHttpStatus;
    bool bSafeToRepeat;
    if (!pError || pError->eCode == XLLM_ERROR_NONE || pError->tDiagnostics.bModelDataDelivered)
    {
        return false;
    }
    /* Session persistence reuses the legacy NETWORK top-level code for ABI
     * compatibility, but its structured RESOURCE cause is local storage and
     * must never be retried as a provider request. */
    if (pError->tCause.eDomain == XLLM_ERROR_DOMAIN_RESOURCE &&
        (pError->tCause.eStage == XLLM_ERROR_STAGE_READ ||
         pError->tCause.eStage == XLLM_ERROR_STAGE_WRITE))
    {
        return false;
    }
    /* Provider policy code 1313 requires an explicit account-side review.
       Retrying it as a transient 429 only creates avoidable traffic. */
    if (strcmp(pError->sProviderCode, "1313") == 0)
    {
        return false;
    }
    iHttpStatus = pError->iHttpStatus;
    if (iHttpStatus == 408 || iHttpStatus == 409 || iHttpStatus == 425 || iHttpStatus == 429 ||
        iHttpStatus == 500 || iHttpStatus == 502 || iHttpStatus == 503 || iHttpStatus == 504)
    {
        return true;
    }
    if (pError->eCode == XLLM_ERROR_RATE_LIMIT)
    {
        return true;
    }
    /* Once any request byte reached the transport, a disconnect does not tell
     * us whether the provider accepted the operation. Only an explicit
     * caller-supplied idempotency key makes an automatic replay admissible. */
    bSafeToRepeat =
        pError->tDiagnostics.uRequestBytes == 0u || pError->tDiagnostics.bIdempotencyKeyPresent;
    if (pError->eCode == XLLM_ERROR_NETWORK)
    {
        return bSafeToRepeat;
    }
    if (pError->eCode == XLLM_ERROR_TIMEOUT)
    {
        return bSafeToRepeat &&
               strcmp(pError->tDiagnostics.sTransportError, "deadline_exceeded") != 0;
    }
    return false;
}

bool xllm__contains_ci(const char *sText, const char *sNeedle)
{
    size_t iNeedle;
    if (!sText || !sNeedle)
    {
        return false;
    }
    iNeedle = strlen(sNeedle);
    if (iNeedle == 0u)
    {
        return true;
    }
    for (; *sText; ++sText)
    {
        size_t i;
        for (i = 0u; i < iNeedle; ++i)
        {
            unsigned char a = (unsigned char)sText[i];
            unsigned char b = (unsigned char)sNeedle[i];
            if (!a || (unsigned char)tolower(a) != (unsigned char)tolower(b))
            {
                break;
            }
        }
        if (i == iNeedle)
        {
            return true;
        }
    }
    return false;
}

/* Length-tracked append: callers keep the current length so incremental
 * deltas never rescan the accumulated tail. */
bool xllm__append_tracked(char **ppText, size_t *piLen, const char *sDelta, size_t iDeltaLen)
{
    char *pNew;
    if (!ppText || !piLen || (!sDelta && iDeltaLen))
    {
        return false;
    }
    if (*piLen == SIZE_MAX || iDeltaLen > SIZE_MAX - *piLen - 1u)
    {
        return false;
    }
    pNew = (char *)xllm__realloc(*ppText, *piLen + iDeltaLen + 1u);
    if (!pNew)
    {
        return false;
    }
    if (iDeltaLen)
    {
        memcpy(pNew + *piLen, sDelta, iDeltaLen);
    }
    pNew[*piLen + iDeltaLen] = '\0';
    *ppText = pNew;
    *piLen += iDeltaLen;
    return true;
}

void xllm__tool_call_unit(xllm_tool_call *pCall)
{
    if (!pCall)
    {
        return;
    }
    xllm__free(pCall->sId);
    xllm__free(pCall->sName);
    xllm__free(pCall->sArgumentsJson);
    memset(pCall, 0, sizeof(*pCall));
}

bool xllm__tool_call_clone(xllm_tool_call *pDst, const xllm_tool_call *pSrc)
{
    if (!pDst || !pSrc)
    {
        return false;
    }
    memset(pDst, 0, sizeof(*pDst));
    if (pSrc->sId && !(pDst->sId = xllm__strdup(pSrc->sId)))
        goto fail;
    if (pSrc->sName && !(pDst->sName = xllm__strdup(pSrc->sName)))
        goto fail;
    if (pSrc->sArgumentsJson && !(pDst->sArgumentsJson = xllm__strdup(pSrc->sArgumentsJson)))
        goto fail;
    return true;
fail:
    xllm__tool_call_unit(pDst);
    return false;
}

void xllmFree(void *pMemory) { xllm__free(pMemory); }
