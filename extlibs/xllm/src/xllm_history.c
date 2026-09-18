#include "xllm_internal.h"

/* ------------------------------------------------------------------ */
/* History: deep-copying message ledger for hand-written agents        */
/*                                                                     */
/* Zero policy on purpose: no token counting, no compaction, no        */
/* journaling -- those live in xllm-session. This container only       */
/* removes the mechanical pain of owning a growing message array.      */
/* ------------------------------------------------------------------ */

struct xllm_history {
    xllm_message* pMessages;
    size_t iCount;
    size_t iCapacity;
};

XRT_API xllm_history* xllmHistoryCreate(void)
{
    return (xllm_history*)xllm__calloc(1u, sizeof(xllm_history));
}

XRT_API void xllmHistoryDestroy(xllm_history* pHistory)
{
    size_t i;
    if ( !pHistory ) { return; }
    for ( i = 0u; i < pHistory->iCount; ++i ) { xllmMessageUnit(&pHistory->pMessages[i]); }
    xllm__free(pHistory->pMessages);
    xllm__free(pHistory);
}

XRT_API size_t xllmHistoryCount(const xllm_history* pHistory)
{
    return pHistory ? pHistory->iCount : 0u;
}

XRT_API const xllm_message* xllmHistoryAt(const xllm_history* pHistory, size_t iIndex)
{
    if ( !pHistory || iIndex >= pHistory->iCount ) { return NULL; }
    return &pHistory->pMessages[iIndex];
}

static bool xllm__history_grow(xllm_history* pHistory, size_t iNeed)
{
    xllm_message* pNew;
    size_t iCapacity;
    if ( iNeed <= pHistory->iCapacity ) { return true; }
    iCapacity = pHistory->iCapacity ? pHistory->iCapacity : 16u;
    while ( iCapacity < iNeed ) {
        if ( iCapacity > SIZE_MAX / 2u ) { iCapacity = iNeed; break; }
        iCapacity *= 2u;
    }
    pNew = (xllm_message*)xllm__realloc(pHistory->pMessages, sizeof(*pNew) * iCapacity);
    if ( !pNew ) { return false; }
    memset(pNew + pHistory->iCapacity, 0,
        sizeof(*pNew) * (iCapacity - pHistory->iCapacity));
    pHistory->pMessages = pNew;
    pHistory->iCapacity = iCapacity;
    return true;
}

XRT_API bool xllmHistoryAdd(xllm_history* pHistory, const xllm_message* pMessage)
{
    xllm_message* pSlot;
    if ( !pHistory || !pMessage ) { return false; }
    if ( !xllm__history_grow(pHistory, pHistory->iCount + 1u) ) { return false; }
    pSlot = &pHistory->pMessages[pHistory->iCount];
    if ( !xllm__message_clone(pSlot, pMessage) ) { return false; }
    ++pHistory->iCount;
    return true;
}

XRT_API bool xllmHistoryAddText(xllm_history* pHistory, xllm_role eRole, const char* sContent)
{
    xllm_message tMessage;
    bool bOk;
    xllmMessageInit(&tMessage, eRole);
    bOk = xllmMessageSetContent(&tMessage, sContent ? sContent : "") &&
        xllmHistoryAdd(pHistory, &tMessage);
    xllmMessageUnit(&tMessage);
    return bOk;
}

XRT_API bool xllmHistoryAddFromResponse(xllm_history* pHistory, const xllm_response* pResponse)
{
    xllm_message tMessage;
    bool bOk;
    if ( !pHistory || !pResponse ) { return false; }
    if ( !xllmMessageFromResponse(pResponse, &tMessage) ) { return false; }
    bOk = xllmHistoryAdd(pHistory, &tMessage);
    xllmMessageUnit(&tMessage);
    return bOk;
}

XRT_API bool xllmHistoryAddToolResult(xllm_history* pHistory, const char* sCallId,
    const char* sContent)
{
    xllm_message tMessage;
    bool bOk;
    xllmMessageInit(&tMessage, XLLM_ROLE_TOOL);
    bOk = xllmMessageSetToolCallId(&tMessage, sCallId ? sCallId : "") &&
        xllmMessageSetContent(&tMessage, sContent ? sContent : "") &&
        xllmHistoryAdd(pHistory, &tMessage);
    xllmMessageUnit(&tMessage);
    return bOk;
}

XRT_API bool xllmHistoryRemove(xllm_history* pHistory, size_t iIndex, size_t iCount)
{
    size_t i;
    if ( !pHistory || iCount == 0u || iIndex >= pHistory->iCount ) { return false; }
    if ( iCount > pHistory->iCount - iIndex ) { iCount = pHistory->iCount - iIndex; }
    for ( i = 0u; i < iCount; ++i ) { xllmMessageUnit(&pHistory->pMessages[iIndex + i]); }
    memmove(&pHistory->pMessages[iIndex], &pHistory->pMessages[iIndex + iCount],
        sizeof(xllm_message) * (pHistory->iCount - iIndex - iCount));
    pHistory->iCount -= iCount;
    return true;
}

XRT_API bool xllmHistoryAppendInto(const xllm_history* pHistory, xllm_request* pRequest)
{
    size_t i;
    if ( !pHistory || !pRequest ) { return false; }
    for ( i = 0u; i < pHistory->iCount; ++i ) {
        if ( !xllmRequestAddMessage(pRequest, &pHistory->pMessages[i]) ) { return false; }
    }
    return true;
}
