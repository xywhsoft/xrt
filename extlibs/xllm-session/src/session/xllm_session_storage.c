#include "../internal/xllm_session_internal.h"

/* Persistence is routed through this narrow layer so the session code can
 * snapshot an XRT cause before cleanup changes the thread-local error.  The
 * production build has no mutable selector; deterministic faults exist only
 * in the explicitly instrumented test translation unit. */

#if defined(XLLM_SESSION_TEST_FAULTS)
static size_t xllm_session__test_alloc_index;
static size_t xllm_session__test_alloc_fail_at;
static xllm_session_test_storage_fault xllm_session__test_storage_fault_state;

void xllm_session__test_alloc_fail(size_t iAllocation)
{
    xllm_session__test_alloc_index = 0u;
    xllm_session__test_alloc_fail_at = iAllocation;
}

void xllm_session__test_alloc_reset(void)
{
    xllm_session__test_alloc_index = 0u;
    xllm_session__test_alloc_fail_at = 0u;
}

size_t xllm_session__test_alloc_count(void) { return xllm_session__test_alloc_index; }

static bool xllm_session__test_alloc_should_fail(void)
{
    ++xllm_session__test_alloc_index;
    return xllm_session__test_alloc_fail_at != 0u &&
           xllm_session__test_alloc_index == xllm_session__test_alloc_fail_at;
}

void xllm_session__test_storage_fail(xllm_session_test_storage_operation eOperation,
                                     xllm_session_test_storage_mode eMode, size_t iOccurrence)
{
    memset(&xllm_session__test_storage_fault_state, 0,
           sizeof(xllm_session__test_storage_fault_state));
    xllm_session__test_storage_fault_state.eOperation = eOperation;
    xllm_session__test_storage_fault_state.eMode = eMode;
    xllm_session__test_storage_fault_state.iOccurrence = iOccurrence ? iOccurrence : 1u;
}

void xllm_session__test_storage_reset(void)
{
    memset(&xllm_session__test_storage_fault_state, 0,
           sizeof(xllm_session__test_storage_fault_state));
}

static bool xllm_session__test_storage_should_fail(xllm_session_test_storage_operation eOperation,
                                                   xllm_session_test_storage_mode *peMode)
{
    xllm_session_test_storage_fault *pFault = &xllm_session__test_storage_fault_state;
    if (pFault->eOperation != eOperation || pFault->eMode == XLLM_SESSION_TEST_STORAGE_MODE_NONE)
    {
        return false;
    }
    ++pFault->iSeen;
    if (pFault->iSeen != pFault->iOccurrence)
    {
        return false;
    }
    *peMode = pFault->eMode;
    return true;
}

static void xllm_session__test_storage_error(xllm_session_test_storage_operation eOperation)
{
    xerrordesc tDesc;
    xerror *pError;
    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.Kind = XERR_IO;
    tDesc.Code = 10800 + (int32_t)eOperation;
    tDesc.SystemCode = 28; /* ENOSPC: stable injected disk-full marker */
    tDesc.Domain = "xllm.session.test";
    tDesc.Operation = "fault-injection";
    tDesc.Message = "injected storage failure";
    pError = xrtErrorBuild(&tDesc);
    if (pError)
    {
        xrtSetErrorTake(pError);
    }
    else
    {
        xrtSetErrorKind(XERR_IO);
    }
}
#else
static bool xllm_session__test_alloc_should_fail(void) { return false; }
#endif

void *xllm_session__malloc(size_t iSize)
{
    void *pMemory = xllm_session__test_alloc_should_fail() ? NULL : malloc(iSize);
    if (!pMemory)
    {
        xrtSetErrorKind(XERR_MEMORY);
    }
    return pMemory;
}

void *xllm_session__calloc(size_t iCount, size_t iSize)
{
    void *pMemory;
    if (iCount && iSize > SIZE_MAX / iCount)
    {
        xrtSetErrorKind(XERR_RANGE);
        return NULL;
    }
    pMemory = xllm_session__test_alloc_should_fail() ? NULL : calloc(iCount, iSize);
    if (!pMemory)
    {
        xrtSetErrorKind(XERR_MEMORY);
    }
    return pMemory;
}

void *xllm_session__realloc(void *pMemory, size_t iSize)
{
    void *pResult = xllm_session__test_alloc_should_fail() ? NULL : realloc(pMemory, iSize);
    if (!pResult)
    {
        xrtSetErrorKind(XERR_MEMORY);
    }
    return pResult;
}

static void xllm_session__copy_text(char *sTarget, size_t iCapacity, const char *sText)
{
    size_t iLength;
    if (!sTarget || iCapacity == 0u)
    {
        return;
    }
    if (!sText)
    {
        sTarget[0] = '\0';
        return;
    }
    iLength = strlen(sText);
    if (iLength >= iCapacity)
    {
        iLength = iCapacity - 1u;
    }
    if (iLength)
    {
        memcpy(sTarget, sText, iLength);
    }
    sTarget[iLength] = '\0';
}

void xllm_session__storage_error(xllm_error *pError, xllm_error_stage eStage,
                                 const char *sOperation, const char *sPath, const char *sFallback)
{
    const xerror *pCause = xrtGetError();
    const char *sCause = pCause ? xrtErrorMessage(pCause) : NULL;
    xllm_error_code eCode = pCause && xrtErrorKind(pCause) == XERR_MEMORY ? XLLM_ERROR_OUT_OF_MEMORY
                                                                          : XLLM_ERROR_NETWORK;
    char sMessage[sizeof(((xllm_error *)0)->sMessage)];
    if (sPath && sPath[0])
    {
        (void)snprintf(sMessage, sizeof(sMessage), "%s: %s (%s)",
                       sFallback ? sFallback : "session storage operation failed", sPath,
                       sCause && sCause[0] ? sCause : "no XRT cause");
    }
    else
    {
        (void)snprintf(sMessage, sizeof(sMessage), "%s (%s)",
                       sFallback ? sFallback : "session storage operation failed",
                       sCause && sCause[0] ? sCause : "no XRT cause");
    }
    xllmErrorInit(pError);
    if (!pError)
    {
        return;
    }
    pError->eCode = eCode;
    xllm_session__copy_text(pError->sMessage, sizeof(pError->sMessage), sMessage);
    pError->tCause.eDomain = XLLM_ERROR_DOMAIN_RESOURCE;
    pError->tCause.eStage = eStage;
    pError->tCause.iRuntimeKind = pCause ? (int32_t)xrtErrorKind(pCause) : 0;
    pError->tCause.iRuntimeCode = pCause ? xrtErrorCode(pCause) : 0;
    pError->tCause.iSystemCode = pCause ? xrtErrorSystemCode(pCause) : 0;
    xllm_session__copy_text(pError->tCause.sOperation, sizeof(pError->tCause.sOperation),
                            sOperation);
    xllm_session__copy_text(pError->tCause.sMessage, sizeof(pError->tCause.sMessage), sMessage);
}

void xllm_session__storage_error_remember(xllm_session *pSession, xllm_error_stage eStage,
                                          const char *sOperation, const char *sPath,
                                          const char *sFallback)
{
    if (!pSession)
    {
        return;
    }
    xllm_session__storage_error(&pSession->tLastPersistenceError, eStage, sOperation, sPath,
                                sFallback);
    pSession->bHasLastPersistenceError = true;
}

void xllm_session__storage_error_copy(const xllm_session *pSession, xllm_error *pError,
                                      xllm_error_code eFallbackCode, const char *sFallback)
{
    if (pError && pSession && pSession->bHasLastPersistenceError)
    {
        memcpy(pError, &pSession->tLastPersistenceError, sizeof(*pError));
    }
    else
    {
        xllm_session__error(pError, eFallbackCode, sFallback);
    }
}

bool xllm_session__storage_path_info(const char *sPath, bool *pbExists, uint64_t *pSize)
{
    xfileinfo tInfo;
#if defined(XLLM_SESSION_TEST_FAULTS)
    xllm_session_test_storage_mode eMode;
    if (xllm_session__test_storage_should_fail(XLLM_SESSION_TEST_STORAGE_STAT, &eMode))
    {
        (void)eMode;
        xllm_session__test_storage_error(XLLM_SESSION_TEST_STORAGE_STAT);
        return false;
    }
#endif
    xrtClearError();
    if (xrtPathStat(sPath, true, &tInfo))
    {
        if (pbExists)
        {
            *pbExists = true;
        }
        if (pSize)
        {
            *pSize = tInfo.Size;
        }
        return true;
    }
    if (xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_NOT_FOUND)
    {
        xrtClearError();
        if (pbExists)
        {
            *pbExists = false;
        }
        if (pSize)
        {
            *pSize = 0u;
        }
        return true;
    }
    return false;
}

xfile xllm_session__storage_open(const char *sPath, uint32_t uFlags)
{
#if defined(XLLM_SESSION_TEST_FAULTS)
    xllm_session_test_storage_mode eMode;
    if (xllm_session__test_storage_should_fail(XLLM_SESSION_TEST_STORAGE_OPEN, &eMode))
    {
        (void)eMode;
        xllm_session__test_storage_error(XLLM_SESSION_TEST_STORAGE_OPEN);
        return NULL;
    }
#endif
    xrtClearError();
    return xrtOpen(sPath, uFlags);
}

bool xllm_session__storage_write_full(xfile File, const void *pData, size_t iSize)
{
#if defined(XLLM_SESSION_TEST_FAULTS)
    xllm_session_test_storage_mode eMode;
    if (xllm_session__test_storage_should_fail(XLLM_SESSION_TEST_STORAGE_WRITE, &eMode))
    {
        if (eMode == XLLM_SESSION_TEST_STORAGE_MODE_SHORT_WRITE && iSize)
        {
            size_t iDone = 0u;
            size_t iPrefix = iSize > 1u ? iSize / 2u : 0u;
            if (iPrefix && !xrtWrite(File, pData, iPrefix, &iDone))
            {
                return false;
            }
        }
        xllm_session__test_storage_error(XLLM_SESSION_TEST_STORAGE_WRITE);
        return false;
    }
#endif
    xrtClearError();
    return xrtWriteFull(File, pData, iSize, NULL);
}

bool xllm_session__storage_flush(xfile File)
{
    bool bOk;
    xrtClearError();
    bOk = xrtFlush(File);
    if (!bOk)
    {
        return false;
    }
#if defined(XLLM_SESSION_TEST_FAULTS)
    {
        xllm_session_test_storage_mode eMode;
        if (xllm_session__test_storage_should_fail(XLLM_SESSION_TEST_STORAGE_FLUSH, &eMode))
        {
            (void)eMode;
            xllm_session__test_storage_error(XLLM_SESSION_TEST_STORAGE_FLUSH);
            return false;
        }
    }
#endif
    return true;
}

bool xllm_session__storage_close(xfile File)
{
    bool bOk;
    xrtClearError();
    bOk = xrtClose(File);
    if (!bOk)
    {
        return false;
    }
#if defined(XLLM_SESSION_TEST_FAULTS)
    {
        xllm_session_test_storage_mode eMode;
        if (xllm_session__test_storage_should_fail(XLLM_SESSION_TEST_STORAGE_CLOSE, &eMode))
        {
            (void)eMode;
            xllm_session__test_storage_error(XLLM_SESSION_TEST_STORAGE_CLOSE);
            return false;
        }
    }
#endif
    return true;
}

bool xllm_session__storage_set_size(const char *sPath, uint64_t iSize)
{
#if defined(XLLM_SESSION_TEST_FAULTS)
    xllm_session_test_storage_mode eMode;
    if (xllm_session__test_storage_should_fail(XLLM_SESSION_TEST_STORAGE_SET_SIZE, &eMode))
    {
        (void)eMode;
        xllm_session__test_storage_error(XLLM_SESSION_TEST_STORAGE_SET_SIZE);
        return false;
    }
#endif
    xrtClearError();
    return xrtFileSetSize(sPath, iSize);
}

bool xllm_session__storage_delete(const char *sPath)
{
#if defined(XLLM_SESSION_TEST_FAULTS)
    xllm_session_test_storage_mode eMode;
    if (xllm_session__test_storage_should_fail(XLLM_SESSION_TEST_STORAGE_DELETE, &eMode))
    {
        (void)eMode;
        xllm_session__test_storage_error(XLLM_SESSION_TEST_STORAGE_DELETE);
        return false;
    }
#endif
    xrtClearError();
    return xrtFileDelete(sPath);
}

bool xllm_session__storage_atomic_write(const char *sPath, xbytesview tData)
{
#if defined(XLLM_SESSION_TEST_FAULTS)
    xllm_session_test_storage_mode eMode;
    if (xllm_session__test_storage_should_fail(XLLM_SESSION_TEST_STORAGE_ATOMIC_WRITE, &eMode))
    {
        (void)eMode;
        xllm_session__test_storage_error(XLLM_SESSION_TEST_STORAGE_ATOMIC_WRITE);
        return false;
    }
#endif
    xrtClearError();
    return xrtFileWriteAtomic(sPath, tData);
}

void *xllm_session__storage_read_all(const char *sPath, size_t *pSize,
                                     xllm_session_test_storage_operation eOperation)
{
#if defined(XLLM_SESSION_TEST_FAULTS)
    xllm_session_test_storage_mode eMode;
    if (xllm_session__test_storage_should_fail(eOperation, &eMode))
    {
        (void)eMode;
        xllm_session__test_storage_error(eOperation);
        return NULL;
    }
#else
    (void)eOperation;
#endif
    xrtClearError();
    return xrtFileReadAll(sPath, pSize);
}
