#include "xllm_session_internal.h"

char* xllm_session__strdup(const char* sText)
{
    size_t iLen;
    char* sCopy;
    if ( !sText ) { return NULL; }
    iLen = strlen(sText);
    sCopy = (char*)malloc(iLen + 1u);
    if ( !sCopy ) { return NULL; }
    memcpy(sCopy, sText, iLen + 1u);
    return sCopy;
}

void xllm_session__error(xllm_error* pError, xllm_error_code eCode, const char* sMessage)
{
    if ( !pError ) { return; }
    xllmErrorInit(pError);
    pError->eCode = eCode;
    if ( sMessage ) {
        size_t iLen = strlen(sMessage);
        if ( iLen >= sizeof(pError->sMessage) ) { iLen = sizeof(pError->sMessage) - 1u; }
        memcpy(pError->sMessage, sMessage, iLen);
        pError->sMessage[iLen] = '\0';
    }
}

bool xllm_session__buf_append(xllm_session_buf* pBuf, const void* pData, size_t iLen)
{
    size_t iNeed;
    size_t iCap;
    char* pNew;
    if ( !pBuf || (!pData && iLen) || pBuf->iLen > SIZE_MAX - iLen - 1u ) { return false; }
    iNeed = pBuf->iLen + iLen + 1u;
    if ( iNeed > pBuf->iCap ) {
        iCap = pBuf->iCap ? pBuf->iCap : 512u;
        while ( iCap < iNeed ) {
            if ( iCap > SIZE_MAX / 2u ) { iCap = iNeed; break; }
            iCap *= 2u;
        }
        pNew = (char*)realloc(pBuf->pData, iCap);
        if ( !pNew ) { return false; }
        pBuf->pData = pNew;
        pBuf->iCap = iCap;
    }
    if ( iLen ) { memcpy(pBuf->pData + pBuf->iLen, pData, iLen); }
    pBuf->iLen += iLen;
    pBuf->pData[pBuf->iLen] = '\0';
    return true;
}

bool xllm_session__buf_cstr(xllm_session_buf* pBuf, const char* sText)
{
    return xllm_session__buf_append(pBuf, sText ? sText : "", sText ? strlen(sText) : 0u);
}

bool xllm_session__buf_char(xllm_session_buf* pBuf, char ch)
{
    return xllm_session__buf_append(pBuf, &ch, 1u);
}

bool xllm_session__buf_u64(xllm_session_buf* pBuf, uint64_t uValue)
{
    char sValue[32];
    (void)snprintf(sValue, sizeof(sValue), "%llu", (unsigned long long)uValue);
    return xllm_session__buf_cstr(pBuf, sValue);
}

bool xllm_session__json_string(xllm_session_buf* pBuf, const char* sText)
{
    const unsigned char* p = (const unsigned char*)(sText ? sText : "");
    char sEscape[7];
    if ( !xllm_session__buf_char(pBuf, '"') ) { return false; }
    while ( *p ) {
        switch ( *p ) {
            case '"': if ( !xllm_session__buf_cstr(pBuf, "\\\"") ) return false; break;
            case '\\': if ( !xllm_session__buf_cstr(pBuf, "\\\\") ) return false; break;
            case '\b': if ( !xllm_session__buf_cstr(pBuf, "\\b") ) return false; break;
            case '\f': if ( !xllm_session__buf_cstr(pBuf, "\\f") ) return false; break;
            case '\n': if ( !xllm_session__buf_cstr(pBuf, "\\n") ) return false; break;
            case '\r': if ( !xllm_session__buf_cstr(pBuf, "\\r") ) return false; break;
            case '\t': if ( !xllm_session__buf_cstr(pBuf, "\\t") ) return false; break;
            default:
                if ( *p < 0x20u ) {
                    (void)snprintf(sEscape, sizeof(sEscape), "\\u%04x", (unsigned)*p);
                    if ( !xllm_session__buf_cstr(pBuf, sEscape) ) return false;
                } else if ( !xllm_session__buf_append(pBuf, p, 1u) ) {
                    return false;
                }
                break;
        }
        ++p;
    }
    return xllm_session__buf_char(pBuf, '"');
}

char* xllm_session__buf_detach(xllm_session_buf* pBuf)
{
    char* pData;
    if ( !pBuf ) { return NULL; }
    if ( !pBuf->pData ) {
        pBuf->pData = (char*)calloc(1u, 1u);
        if ( !pBuf->pData ) { return NULL; }
    }
    pData = pBuf->pData;
    memset(pBuf, 0, sizeof(*pBuf));
    return pData;
}

void xllm_session__buf_unit(xllm_session_buf* pBuf)
{
    if ( !pBuf ) { return; }
    free(pBuf->pData);
    memset(pBuf, 0, sizeof(*pBuf));
}

bool xllm_session__message_clone(xllm_message* pDst, const xllm_message* pSrc)
{
    size_t i;
    if ( !pDst || !pSrc ) { return false; }
    xllmMessageInit(pDst, pSrc->eRole);
    if ( pSrc->sContent && !xllmMessageSetContent(pDst, pSrc->sContent) ) goto fail;
    if ( pSrc->sReasoningContent && !xllmMessageSetReasoning(pDst, pSrc->sReasoningContent) ) goto fail;
    if ( pSrc->sToolCallId && !xllmMessageSetToolCallId(pDst, pSrc->sToolCallId) ) goto fail;
    for ( i = 0u; i < pSrc->iToolCallCount; ++i ) {
        const xllm_tool_call* pCall = &pSrc->pToolCalls[i];
        if ( !xllmMessageAddToolCall(pDst, pCall->sId, pCall->sName, pCall->sArgumentsJson) ) goto fail;
    }
    return true;
fail:
    xllmMessageUnit(pDst);
    return false;
}

uint64_t xllmSessionComputeSafetyReserve(uint64_t uContextWindowTokens)
{
    uint64_t uReserve = (uContextWindowTokens * 3u) / 100u;
    if ( uReserve < 8000u ) { uReserve = 8000u; }
    if ( uReserve > 32000u ) { uReserve = 32000u; }
    return uReserve;
}

uint32_t xllmSessionComputeOutputReserve(uint64_t uContextWindowTokens, uint32_t uMaxOutputTokens)
{
    uint64_t uReserve = uContextWindowTokens / 6u;
    if ( uReserve < 4096u ) { uReserve = 4096u; }
    if ( uReserve > 32768u ) { uReserve = 32768u; }
    if ( uReserve > uMaxOutputTokens ) { uReserve = uMaxOutputTokens; }
    return (uint32_t)uReserve;
}

void xllmSessionConfigInit(xllm_session_config* pConfig)
{
    if ( !pConfig ) { return; }
    memset(pConfig, 0, sizeof(*pConfig));
    pConfig->uContextWindowTokens = XLLM_SESSION_DEFAULT_CONTEXT_WINDOW_TOKENS;
    pConfig->uMaxOutputTokens = XLLM_SESSION_DEFAULT_MAX_OUTPUT_TOKENS;
    pConfig->uRecentTurnsToKeep = 4u;
    pConfig->uToolPruneBytes = 64u * 1024u;
    pConfig->uSummaryMaxTokens = 32768u;
    pConfig->uSummaryMinTokens = 64u;
    pConfig->uCompactionRequiredSections = XLLM_COMPACTION_SECTION_ALL;
    pConfig->fPruneTrigger = 0.75;
    pConfig->fCompactTrigger = 0.95;
}

uint64_t xllmEstimateTextTokens(const char* sText)
{
    const unsigned char* p = (const unsigned char*)sText;
    uint64_t uAscii = 0u;
    uint64_t uNonAscii = 0u;
    if ( !p ) { return 0u; }
    while ( *p ) {
        if ( *p < 0x80u ) {
            ++uAscii;
            ++p;
        } else {
            ++uNonAscii;
            if ( (*p & 0xE0u) == 0xC0u && p[1] ) { p += 2; }
            else if ( (*p & 0xF0u) == 0xE0u && p[1] && p[2] ) { p += 3; }
            else if ( (*p & 0xF8u) == 0xF0u && p[1] && p[2] && p[3] ) { p += 4; }
            else { ++p; }
        }
    }
    return (uAscii + 3u) / 4u + uNonAscii;
}

uint64_t xllmEstimateMessageTokens(const xllm_message* pMessage)
{
    uint64_t uTokens = 12u;
    size_t i;
    if ( !pMessage ) { return 0u; }
    uTokens += xllmEstimateTextTokens(pMessage->sContent);
    uTokens += xllmEstimateTextTokens(pMessage->sReasoningContent);
    uTokens += xllmEstimateTextTokens(pMessage->sToolCallId);
    for ( i = 0u; i < pMessage->iToolCallCount; ++i ) {
        const xllm_tool_call* pCall = &pMessage->pToolCalls[i];
        uTokens += 16u + xllmEstimateTextTokens(pCall->sId) +
            xllmEstimateTextTokens(pCall->sName) + xllmEstimateTextTokens(pCall->sArgumentsJson);
    }
    return uTokens;
}

uint64_t xllm_session__input_budget(const xllm_session* pSession)
{
    uint64_t uReserved;
    if ( !pSession ) { return 0u; }
    uReserved = (uint64_t)pSession->tConfig.uOutputReserveTokens + pSession->tConfig.uSafetyReserveTokens;
    return pSession->tConfig.uContextWindowTokens > uReserved
        ? pSession->tConfig.uContextWindowTokens - uReserved : 0u;
}

xllm_session* xllmSessionCreate(const xllm_session_config* pConfig, xllm_error* pError)
{
    xllm_session_config tConfig;
    xllm_session* pSession;
    if ( pError ) { xllmErrorInit(pError); }
    if ( pConfig ) { tConfig = *pConfig; }
    else { xllmSessionConfigInit(&tConfig); }
    if ( tConfig.uContextWindowTokens == 0u || tConfig.uMaxOutputTokens == 0u ) {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT, "context window and max output tokens must be non-zero");
        return NULL;
    }
    if ( tConfig.uSafetyReserveTokens == 0u ) {
        tConfig.uSafetyReserveTokens = (uint32_t)xllmSessionComputeSafetyReserve(tConfig.uContextWindowTokens);
    }
    if ( tConfig.uOutputReserveTokens == 0u ) {
        tConfig.uOutputReserveTokens = xllmSessionComputeOutputReserve(tConfig.uContextWindowTokens, tConfig.uMaxOutputTokens);
    }
    if ( (uint64_t)tConfig.uMaxOutputTokens + tConfig.uSafetyReserveTokens >= tConfig.uContextWindowTokens ||
         tConfig.uOutputReserveTokens > tConfig.uMaxOutputTokens ||
         (uint64_t)tConfig.uOutputReserveTokens + tConfig.uSafetyReserveTokens >= tConfig.uContextWindowTokens ||
         tConfig.fPruneTrigger <= 0.0 || tConfig.fPruneTrigger >= 1.0 ||
         tConfig.fCompactTrigger <= tConfig.fPruneTrigger || tConfig.fCompactTrigger > 1.0 ) {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT, "invalid session budget or pressure thresholds");
        return NULL;
    }
    if ( tConfig.uRecentTurnsToKeep == 0u ) { tConfig.uRecentTurnsToKeep = 1u; }
    if ( tConfig.uToolPruneBytes < 256u ) { tConfig.uToolPruneBytes = 256u; }
    if ( tConfig.uSummaryMaxTokens == 0u ) { tConfig.uSummaryMaxTokens = 32768u; }
    if ( tConfig.uSummaryMinTokens == 0u ) { tConfig.uSummaryMinTokens = 64u; }
    if ( tConfig.uSummaryMinTokens > tConfig.uSummaryMaxTokens ||
         (tConfig.uCompactionRequiredSections & ~XLLM_COMPACTION_SECTION_ALL) != 0u ) {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT, "invalid compaction quality policy");
        return NULL;
    }
    pSession = (xllm_session*)calloc(1u, sizeof(*pSession));
    if ( !pSession ) {
        xllm_session__error(pError, XLLM_ERROR_OUT_OF_MEMORY, "failed to allocate session");
        return NULL;
    }
    pSession->tConfig = tConfig;
    pSession->uNextSequence = 1u;
    return pSession;
}

xllm_session* xllmSessionFork(const xllm_session* pSession, xllm_error* pError)
{
    xllm_session* pFork;
    size_t i;
    if ( pError ) { xllmErrorInit(pError); }
    if ( !pSession ) {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT, "source session is required");
        return NULL;
    }
    pFork = xllmSessionCreate(&pSession->tConfig, pError);
    if ( !pFork ) { return NULL; }
    pFork->uCurrentTurn = pSession->uCurrentTurn;
    if ( pSession->sSummary ) {
        pFork->sSummary = xllm_session__strdup(pSession->sSummary);
        if ( !pFork->sSummary ) { goto oom; }
    }
    for ( i = 0u; i < pSession->iEntryCount; ++i ) {
        const xllm_session_entry* pSourceEntry = &pSession->pEntries[i];
        xllm_session_entry* pForkEntry;
        if ( !xllmSessionAddMessage(pFork, pSourceEntry->uTurn, &pSourceEntry->tMessage, pSourceEntry->uFlags) ) {
            goto oom;
        }
        pForkEntry = &pFork->pEntries[pFork->iEntryCount - 1u];
        pForkEntry->uSequence = pSourceEntry->uSequence;
        pForkEntry->uEstimatedTokens = pSourceEntry->uEstimatedTokens;
    }
    pFork->uNextSequence = pSession->uNextSequence;
    pFork->uCompactedThrough = pSession->uCompactedThrough;
    pFork->uCompactionCount = pSession->uCompactionCount;
    pFork->uJournalSequence = pSession->uJournalSequence;
    return pFork;
oom:
    xllmSessionDestroy(pFork);
    xllm_session__error(pError, XLLM_ERROR_OUT_OF_MEMORY, "failed to fork session");
    return NULL;
}

void xllmSessionDestroy(xllm_session* pSession)
{
    size_t i;
    if ( !pSession ) { return; }
    for ( i = 0u; i < pSession->iEntryCount; ++i ) { xllmMessageUnit(&pSession->pEntries[i].tMessage); }
    free(pSession->pEntries);
    free(pSession->sSummary);
    free(pSession->sJournalPath);
    free(pSession);
}

bool xllmSessionGetConfig(const xllm_session* pSession, xllm_session_config* pConfig)
{
    if ( !pSession || !pConfig ) { return false; }
    *pConfig = pSession->tConfig;
    return true;
}

uint64_t xllmSessionBeginTurn(xllm_session* pSession)
{
    uint64_t uTurn;
    if ( !pSession || pSession->uCurrentTurn == UINT64_MAX ) { return 0u; }
    uTurn = pSession->uCurrentTurn + 1u;
    if ( !xllm_session__journal_append_turn(pSession, uTurn) ) { return 0u; }
    pSession->uCurrentTurn = uTurn;
    return uTurn;
}

uint64_t xllmSessionCurrentTurn(const xllm_session* pSession)
{
    return pSession ? pSession->uCurrentTurn : 0u;
}

bool xllmSessionAddMessage(xllm_session* pSession, uint64_t uTurn, const xllm_message* pMessage, uint32_t uFlags)
{
    xllm_session_entry* pNew;
    xllm_session_entry* pEntry;
    size_t iCap;
    if ( !pSession || !pMessage || uTurn > pSession->uCurrentTurn || pSession->uNextSequence == UINT64_MAX ) { return false; }
    if ( pSession->iEntryCount == pSession->iEntryCap ) {
        iCap = pSession->iEntryCap ? pSession->iEntryCap * 2u : 32u;
        pNew = (xllm_session_entry*)realloc(pSession->pEntries, sizeof(*pNew) * iCap);
        if ( !pNew ) { return false; }
        memset(pNew + pSession->iEntryCap, 0, sizeof(*pNew) * (iCap - pSession->iEntryCap));
        pSession->pEntries = pNew;
        pSession->iEntryCap = iCap;
    }
    pEntry = &pSession->pEntries[pSession->iEntryCount];
    memset(pEntry, 0, sizeof(*pEntry));
    if ( !xllm_session__message_clone(&pEntry->tMessage, pMessage) ) { return false; }
    pEntry->uSequence = pSession->uNextSequence;
    pEntry->uTurn = uTurn;
    pEntry->uFlags = uFlags;
    pEntry->uEstimatedTokens = xllmEstimateMessageTokens(&pEntry->tMessage);
    if ( !xllm_session__journal_append_entry(pSession, pEntry) ) {
        xllmMessageUnit(&pEntry->tMessage);
        memset(pEntry, 0, sizeof(*pEntry));
        return false;
    }
    ++pSession->uNextSequence;
    ++pSession->iEntryCount;
    return true;
}

bool xllmSessionAddText(xllm_session* pSession, uint64_t uTurn, xllm_role eRole, const char* sContent, uint32_t uFlags)
{
    xllm_message tMessage;
    bool bOk;
    if ( !pSession ) { return false; }
    xllmMessageInit(&tMessage, eRole);
    bOk = xllmMessageSetContent(&tMessage, sContent ? sContent : "") &&
        xllmSessionAddMessage(pSession, uTurn, &tMessage, uFlags);
    xllmMessageUnit(&tMessage);
    return bOk;
}

bool xllmSessionAddAssistantResponse(xllm_session* pSession, uint64_t uTurn, const xllm_response* pResponse)
{
    xllm_message tMessage;
    size_t i;
    bool bOk = false;
    if ( !pSession || !pResponse ) { return false; }
    xllmMessageInit(&tMessage, XLLM_ROLE_ASSISTANT);
    if ( !xllmMessageSetContent(&tMessage, pResponse->sContent ? pResponse->sContent : "") ) goto done;
    if ( pResponse->sReasoningContent && !xllmMessageSetReasoning(&tMessage, pResponse->sReasoningContent) ) goto done;
    for ( i = 0u; i < pResponse->iToolCallCount; ++i ) {
        const xllm_tool_call* pCall = &pResponse->pToolCalls[i];
        if ( !xllmMessageAddToolCall(&tMessage, pCall->sId, pCall->sName, pCall->sArgumentsJson) ) goto done;
    }
    bOk = xllmSessionAddMessage(pSession, uTurn, &tMessage, 0u);
done:
    xllmMessageUnit(&tMessage);
    return bOk;
}

bool xllmSessionAddToolResult(xllm_session* pSession, uint64_t uTurn, const char* sToolCallId, const char* sContent)
{
    xllm_message tMessage;
    bool bOk;
    if ( !pSession || !sToolCallId || !sToolCallId[0] ) { return false; }
    xllmMessageInit(&tMessage, XLLM_ROLE_TOOL);
    bOk = xllmMessageSetToolCallId(&tMessage, sToolCallId) &&
        xllmMessageSetContent(&tMessage, sContent ? sContent : "") &&
        xllmSessionAddMessage(pSession, uTurn, &tMessage, 0u);
    xllmMessageUnit(&tMessage);
    return bOk;
}

bool xllmSessionGetTail(const xllm_session* pSession, xllm_session_tail* pTail)
{
    size_t i;
    if ( !pSession || !pTail ) { return false; }
    memset(pTail, 0, sizeof(*pTail));
    for ( i = pSession->iEntryCount; i > 0u; --i ) {
        const xllm_session_entry* pEntry = &pSession->pEntries[i - 1u];
        if ( !xllm_session__entry_is_active(pSession, pEntry) ) continue;
        pTail->uTurn = pEntry->uTurn;
        pTail->eRole = pEntry->tMessage.eRole;
        pTail->bHasMessage = true;
        return true;
    }
    return true;
}

bool xllm_session__entry_is_active(const xllm_session* pSession, const xllm_session_entry* pEntry)
{
    if ( !pSession || !pEntry ) { return false; }
    return (pEntry->uFlags & XLLM_SESSION_ENTRY_PINNED) != 0u || pEntry->uSequence > pSession->uCompactedThrough;
}

bool xllm_session__should_prune_tool(const xllm_session* pSession, const xllm_session_entry* pEntry)
{
    size_t iLen;
    if ( !pSession || !pEntry || pEntry->tMessage.eRole != XLLM_ROLE_TOOL || !pEntry->tMessage.sContent ) { return false; }
    iLen = strlen(pEntry->tMessage.sContent);
    return iLen > pSession->tConfig.uToolPruneBytes &&
        pEntry->uTurn + pSession->tConfig.uRecentTurnsToKeep < pSession->uCurrentTurn;
}

static bool xllm_session__tool_resolved(const xllm_session* pSession, size_t iAssistantEntry, const char* sCallId)
{
    size_t i;
    uint64_t uTurn;
    if ( !pSession || !sCallId ) { return false; }
    uTurn = pSession->pEntries[iAssistantEntry].uTurn;
    for ( i = iAssistantEntry + 1u; i < pSession->iEntryCount; ++i ) {
        const xllm_session_entry* pEntry = &pSession->pEntries[i];
        if ( pEntry->uTurn != uTurn ) {
            if ( pEntry->uTurn > uTurn ) { break; }
            continue;
        }
        if ( pEntry->tMessage.eRole == XLLM_ROLE_TOOL && pEntry->tMessage.sToolCallId &&
             strcmp(pEntry->tMessage.sToolCallId, sCallId) == 0 ) return true;
    }
    return false;
}

uint32_t xllm_session__pending_tool_calls(const xllm_session* pSession)
{
    size_t iPending = xllmSessionPendingToolCallCount(pSession);
    return iPending > UINT32_MAX ? UINT32_MAX : (uint32_t)iPending;
}

size_t xllmSessionPendingToolCallCount(const xllm_session* pSession)
{
    size_t iPending = 0u;
    size_t i;
    if ( !pSession ) { return 0u; }
    for ( i = 0u; i < pSession->iEntryCount; ++i ) {
        const xllm_session_entry* pEntry = &pSession->pEntries[i];
        size_t j;
        if ( !xllm_session__entry_is_active(pSession, pEntry) || pEntry->tMessage.eRole != XLLM_ROLE_ASSISTANT ) continue;
        for ( j = 0u; j < pEntry->tMessage.iToolCallCount; ++j ) {
            if ( !xllm_session__tool_resolved(pSession, i, pEntry->tMessage.pToolCalls[j].sId) ) { ++iPending; }
        }
    }
    return iPending;
}

bool xllmSessionPendingToolCallAt(const xllm_session* pSession, size_t iIndex, xllm_pending_tool_call* pCall)
{
    size_t iPending = 0u;
    size_t i;
    if ( !pSession || !pCall ) { return false; }
    memset(pCall, 0, sizeof(*pCall));
    for ( i = 0u; i < pSession->iEntryCount; ++i ) {
        const xllm_session_entry* pEntry = &pSession->pEntries[i];
        size_t j;
        if ( !xllm_session__entry_is_active(pSession, pEntry) || pEntry->tMessage.eRole != XLLM_ROLE_ASSISTANT ) continue;
        for ( j = 0u; j < pEntry->tMessage.iToolCallCount; ++j ) {
            const xllm_tool_call* pToolCall = &pEntry->tMessage.pToolCalls[j];
            if ( xllm_session__tool_resolved(pSession, i, pToolCall->sId) ) continue;
            if ( iPending++ != iIndex ) continue;
            pCall->uTurn = pEntry->uTurn;
            pCall->sId = pToolCall->sId;
            pCall->sName = pToolCall->sName;
            pCall->sArgumentsJson = pToolCall->sArgumentsJson;
            return true;
        }
    }
    return false;
}

static uint64_t xllm_session__pruned_entry_tokens(const xllm_session* pSession, const xllm_session_entry* pEntry)
{
    uint64_t uTokens;
    size_t iLen;
    size_t iKeep;
    if ( !xllm_session__should_prune_tool(pSession, pEntry) ) { return pEntry->uEstimatedTokens; }
    iLen = strlen(pEntry->tMessage.sContent);
    iKeep = pSession->tConfig.uToolPruneBytes;
    if ( iKeep > iLen ) { iKeep = iLen; }
    uTokens = 32u + (iKeep + 3u) / 4u + xllmEstimateTextTokens(pEntry->tMessage.sToolCallId);
    return uTokens;
}

bool xllmSessionGetStats(const xllm_session* pSession, xllm_session_stats* pStats)
{
    uint64_t uInputBudget;
    uint64_t uRaw = 0u;
    uint64_t uRendered = 0u;
    uint64_t uSummaryTokens = 0u;
    size_t i;
    bool bPrune;
    if ( !pSession || !pStats ) { return false; }
    memset(pStats, 0, sizeof(*pStats));
    uInputBudget = xllm_session__input_budget(pSession);
    if ( pSession->sSummary ) { uSummaryTokens = 24u + xllmEstimateTextTokens(pSession->sSummary); }
    uRaw += uSummaryTokens;
    for ( i = 0u; i < pSession->iEntryCount; ++i ) {
        if ( xllm_session__entry_is_active(pSession, &pSession->pEntries[i]) ) {
            uRaw += pSession->pEntries[i].uEstimatedTokens;
        }
    }
    pStats->uPruneThresholdTokens = (uint64_t)((double)uInputBudget * pSession->tConfig.fPruneTrigger);
    pStats->uCompactThresholdTokens = (uint64_t)((double)uInputBudget * pSession->tConfig.fCompactTrigger);
    bPrune = uRaw >= pStats->uPruneThresholdTokens;
    uRendered += uSummaryTokens;
    for ( i = 0u; i < pSession->iEntryCount; ++i ) {
        if ( !xllm_session__entry_is_active(pSession, &pSession->pEntries[i]) ) continue;
        uRendered += bPrune ? xllm_session__pruned_entry_tokens(pSession, &pSession->pEntries[i])
            : pSession->pEntries[i].uEstimatedTokens;
    }
    pStats->uContextWindowTokens = pSession->tConfig.uContextWindowTokens;
    pStats->uInputBudgetTokens = uInputBudget;
    pStats->uOutputReserveTokens = pSession->tConfig.uOutputReserveTokens;
    pStats->uRawActiveTokens = uRaw;
    pStats->uRenderedActiveTokens = uRendered;
    pStats->uCompactedThroughSequence = pSession->uCompactedThrough;
    pStats->uCurrentTurn = pSession->uCurrentTurn;
    pStats->uEntryCount = pSession->iEntryCount;
    pStats->uCompactionCount = pSession->uCompactionCount;
    pStats->uJournalSequence = pSession->uJournalSequence;
    pStats->bJournalEnabled = pSession->sJournalPath != NULL;
    if ( pSession->tConfig.uContextWindowTokens > uRendered + pSession->tConfig.uSafetyReserveTokens ) {
        uint64_t uAvailable = pSession->tConfig.uContextWindowTokens - uRendered - pSession->tConfig.uSafetyReserveTokens;
        pStats->uNextMaxOutputTokens = uAvailable < pSession->tConfig.uMaxOutputTokens
            ? (uint32_t)uAvailable : pSession->tConfig.uMaxOutputTokens;
    }
    pStats->uPendingToolCalls = xllm_session__pending_tool_calls(pSession);
    if ( uRendered > uInputBudget ) { pStats->ePressure = XLLM_SESSION_PRESSURE_OVERFLOW; }
    else if ( uRendered >= pStats->uCompactThresholdTokens ) { pStats->ePressure = XLLM_SESSION_PRESSURE_COMPACT; }
    else if ( bPrune ) { pStats->ePressure = XLLM_SESSION_PRESSURE_PRUNE; }
    else { pStats->ePressure = XLLM_SESSION_PRESSURE_NONE; }
    return true;
}

static char* xllm_session__pruned_content(const xllm_session* pSession, const xllm_session_entry* pEntry)
{
    static const char sMarker[] = "\n\n[... older tool output pruned from active context; full output remains in the persisted session ledger ...]\n\n";
    const char* sContent = pEntry->tMessage.sContent ? pEntry->tMessage.sContent : "";
    size_t iLen = strlen(sContent);
    size_t iKeep = pSession->tConfig.uToolPruneBytes;
    size_t iHead;
    size_t iTail;
    char* sOut;
    if ( iLen <= iKeep ) { return xllm_session__strdup(sContent); }
    iHead = (iKeep * 3u) / 4u;
    iTail = iKeep - iHead;
    while ( iHead && ((unsigned char)sContent[iHead] & 0xC0u) == 0x80u ) { --iHead; }
    while ( iTail < iLen && ((unsigned char)sContent[iLen - iTail] & 0xC0u) == 0x80u ) { ++iTail; }
    sOut = (char*)malloc(iHead + sizeof(sMarker) - 1u + iTail + 1u);
    if ( !sOut ) { return NULL; }
    memcpy(sOut, sContent, iHead);
    memcpy(sOut + iHead, sMarker, sizeof(sMarker) - 1u);
    memcpy(sOut + iHead + sizeof(sMarker) - 1u, sContent + iLen - iTail, iTail);
    sOut[iHead + sizeof(sMarker) - 1u + iTail] = '\0';
    return sOut;
}

bool xllmSessionBuildRequest(const xllm_session* pSession, xllm_request* pRequest, xllm_error* pError)
{
    xllm_session_stats tStats;
    size_t i;
    bool bPrune;
    if ( pError ) { xllmErrorInit(pError); }
    if ( !pSession || !pRequest || !xllmSessionGetStats(pSession, &tStats) ) {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT, "session and initialized request are required");
        return false;
    }
    if ( tStats.ePressure == XLLM_SESSION_PRESSURE_OVERFLOW ) {
        xllm_session__error(pError, XLLM_ERROR_PROTOCOL, "active context exceeds the model input budget and must be compacted");
        return false;
    }
    if ( pRequest->uMaxOutputTokens == 0u || pRequest->uMaxOutputTokens > tStats.uNextMaxOutputTokens ) {
        pRequest->uMaxOutputTokens = tStats.uNextMaxOutputTokens;
    }
    if ( pRequest->uMaxOutputTokens == 0u ) {
        xllm_session__error(pError, XLLM_ERROR_PROTOCOL, "active context leaves no room for model output");
        return false;
    }
    bPrune = tStats.uRawActiveTokens >= tStats.uPruneThresholdTokens;
    for ( i = 0u; i < pSession->iEntryCount; ++i ) {
        const xllm_session_entry* pEntry = &pSession->pEntries[i];
        if ( (pEntry->uFlags & XLLM_SESSION_ENTRY_PINNED) == 0u ) continue;
        if ( !xllmRequestAddMessage(pRequest, &pEntry->tMessage) ) goto oom;
    }
    if ( pSession->sSummary && pSession->sSummary[0] ) {
        xllm_session_buf tSummary = {0};
        xllm_message tMessage;
        bool bOk;
        /* The summary is a synthetic continuation turn, not a second system
         * message. Keeping it as user content also gives providers a valid
         * user bridge when the retained suffix begins with assistant/tool
         * messages from an in-progress agent loop. */
        xllmMessageInit(&tMessage, XLLM_ROLE_USER);
        bOk = xllm_session__buf_cstr(&tSummary, "Compacted session state. Treat this as authoritative continuity for history through sequence ") &&
            xllm_session__buf_u64(&tSummary, pSession->uCompactedThrough) &&
            xllm_session__buf_cstr(&tSummary, ":\n\n") && xllm_session__buf_cstr(&tSummary, pSession->sSummary) &&
            xllmMessageSetContent(&tMessage, tSummary.pData) && xllmRequestAddMessage(pRequest, &tMessage);
        xllmMessageUnit(&tMessage);
        xllm_session__buf_unit(&tSummary);
        if ( !bOk ) goto oom;
    }
    for ( i = 0u; i < pSession->iEntryCount; ++i ) {
        const xllm_session_entry* pEntry = &pSession->pEntries[i];
        if ( (pEntry->uFlags & XLLM_SESSION_ENTRY_PINNED) != 0u || pEntry->uSequence <= pSession->uCompactedThrough ) continue;
        if ( bPrune && xllm_session__should_prune_tool(pSession, pEntry) ) {
            xllm_message tMessage;
            char* sPruned = xllm_session__pruned_content(pSession, pEntry);
            bool bOk;
            if ( !sPruned ) goto oom;
            if ( !xllm_session__message_clone(&tMessage, &pEntry->tMessage) ) { free(sPruned); goto oom; }
            bOk = xllmMessageSetContent(&tMessage, sPruned) && xllmRequestAddMessage(pRequest, &tMessage);
            free(sPruned);
            xllmMessageUnit(&tMessage);
            if ( !bOk ) goto oom;
        } else if ( !xllmRequestAddMessage(pRequest, &pEntry->tMessage) ) {
            goto oom;
        }
    }
    return true;
oom:
    xllm_session__error(pError, XLLM_ERROR_OUT_OF_MEMORY, "failed to render session request");
    return false;
}

static bool xllm_session__turn_is_safe(const xllm_session* pSession, uint64_t uTurn)
{
    size_t i;
    for ( i = 0u; i < pSession->iEntryCount; ++i ) {
        const xllm_session_entry* pEntry = &pSession->pEntries[i];
        size_t j;
        if ( pEntry->uTurn != uTurn || pEntry->tMessage.eRole != XLLM_ROLE_ASSISTANT ) continue;
        for ( j = 0u; j < pEntry->tMessage.iToolCallCount; ++j ) {
            if ( !xllm_session__tool_resolved(pSession, i, pEntry->tMessage.pToolCalls[j].sId) ) return false;
        }
    }
    return true;
}

static uint64_t xllm_session__compaction_candidate(const xllm_session* pSession)
{
    uint64_t uMaxTurn;
    uint64_t uCandidate = pSession ? pSession->uCompactedThrough : 0u;
    uint64_t uLastTurn = 0u;
    size_t i;
    if ( !pSession || pSession->uCurrentTurn <= pSession->tConfig.uRecentTurnsToKeep ) return uCandidate;
    uMaxTurn = pSession->uCurrentTurn - pSession->tConfig.uRecentTurnsToKeep;
    for ( i = 0u; i < pSession->iEntryCount; ++i ) {
        const xllm_session_entry* pEntry = &pSession->pEntries[i];
        if ( pEntry->uSequence <= pSession->uCompactedThrough || pEntry->uTurn == 0u || pEntry->uTurn > uMaxTurn ) continue;
        if ( pEntry->uTurn != uLastTurn ) {
            if ( !xllm_session__turn_is_safe(pSession, pEntry->uTurn) ) break;
            uLastTurn = pEntry->uTurn;
        }
        if ( (pEntry->uFlags & XLLM_SESSION_ENTRY_PINNED) == 0u && pEntry->uSequence > uCandidate ) {
            uCandidate = pEntry->uSequence;
        }
    }
    return uCandidate;
}

static const char* xllm_session__role_name(xllm_role eRole)
{
    switch ( eRole ) {
        case XLLM_ROLE_SYSTEM: return "system";
        case XLLM_ROLE_USER: return "user";
        case XLLM_ROLE_ASSISTANT: return "assistant";
        case XLLM_ROLE_TOOL: return "tool";
        default: return "unknown";
    }
}

static bool xllm_session__append_compaction_entry(
    xllm_session_buf* pPrompt,
    const xllm_session* pSession,
    const xllm_session_entry* pEntry
)
{
    const char* sContent = pEntry->tMessage.sContent ? pEntry->tMessage.sContent : "";
    char* sPruned = NULL;
    size_t i;
    if ( xllm_session__should_prune_tool(pSession, pEntry) ) {
        sPruned = xllm_session__pruned_content(pSession, pEntry);
        if ( !sPruned ) return false;
        sContent = sPruned;
    }
    if ( !xllm_session__buf_cstr(pPrompt, "\n<message sequence=\"") ||
         !xllm_session__buf_u64(pPrompt, pEntry->uSequence) ||
         !xllm_session__buf_cstr(pPrompt, "\" turn=\"") ||
         !xllm_session__buf_u64(pPrompt, pEntry->uTurn) ||
         !xllm_session__buf_cstr(pPrompt, "\" role=\"") ||
         !xllm_session__buf_cstr(pPrompt, xllm_session__role_name(pEntry->tMessage.eRole)) ||
         !xllm_session__buf_cstr(pPrompt, "\">\n") ||
         !xllm_session__buf_cstr(pPrompt, sContent) ) goto fail;
    if ( pEntry->tMessage.sReasoningContent && pEntry->tMessage.sReasoningContent[0] ) {
        if ( !xllm_session__buf_cstr(pPrompt, "\n<reasoning>\n") ||
             !xllm_session__buf_cstr(pPrompt, pEntry->tMessage.sReasoningContent) ||
             !xllm_session__buf_cstr(pPrompt, "\n</reasoning>") ) goto fail;
    }
    if ( pEntry->tMessage.sToolCallId ) {
        if ( !xllm_session__buf_cstr(pPrompt, "\n<tool_call_id>") ||
             !xllm_session__buf_cstr(pPrompt, pEntry->tMessage.sToolCallId) ||
             !xllm_session__buf_cstr(pPrompt, "</tool_call_id>") ) goto fail;
    }
    for ( i = 0u; i < pEntry->tMessage.iToolCallCount; ++i ) {
        const xllm_tool_call* pCall = &pEntry->tMessage.pToolCalls[i];
        if ( !xllm_session__buf_cstr(pPrompt, "\n<tool_call id=\"") ||
             !xllm_session__buf_cstr(pPrompt, pCall->sId) ||
             !xllm_session__buf_cstr(pPrompt, "\" name=\"") ||
             !xllm_session__buf_cstr(pPrompt, pCall->sName) ||
             !xllm_session__buf_cstr(pPrompt, "\">\n") ||
             !xllm_session__buf_cstr(pPrompt, pCall->sArgumentsJson) ||
             !xllm_session__buf_cstr(pPrompt, "\n</tool_call>") ) goto fail;
    }
    if ( !xllm_session__buf_cstr(pPrompt, "\n</message>\n") ) goto fail;
    free(sPruned);
    return true;
fail:
    free(sPruned);
    return false;
}

xllm_compaction* xllmSessionPrepareCompaction(xllm_session* pSession, bool bForce, xllm_error* pError)
{
    static const char sInstruction[] =
        "You are compacting the durable state of a long-running code-agent session.\n"
        "Produce a dense, factual continuation summary. Preserve the objective, constraints, architecture decisions, files changed, commands and test evidence, unresolved failures, active hypotheses, exact next steps, and every identifier or path needed to continue. Preserve tool-call outcomes, not conversational filler. Do not claim unfinished work is complete.\n\n"
        "Use these headings: Objective; Constraints; Architecture and decisions; Completed work; Current repository state; Verification evidence; Open issues and risks; Exact next actions.\n\n";
    xllm_session_stats tStats;
    xllm_session_buf tPrompt = {0};
    xllm_compaction* pCompaction = NULL;
    uint64_t uCandidate;
    size_t i;
    if ( pError ) { xllmErrorInit(pError); }
    if ( !pSession || !xllmSessionGetStats(pSession, &tStats) ) {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT, "session is required");
        return NULL;
    }
    if ( !bForce && tStats.ePressure < XLLM_SESSION_PRESSURE_COMPACT ) {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT, "compaction threshold has not been reached");
        return NULL;
    }
    uCandidate = xllm_session__compaction_candidate(pSession);
    if ( uCandidate <= pSession->uCompactedThrough ) {
        xllm_session__error(pError, XLLM_ERROR_PROTOCOL, "no completed prefix is safe to compact yet");
        return NULL;
    }
    if ( !xllm_session__buf_cstr(&tPrompt, sInstruction) ) goto oom;
    if ( pSession->sSummary && pSession->sSummary[0] ) {
        if ( !xllm_session__buf_cstr(&tPrompt, "<previous_summary>\n") ||
             !xllm_session__buf_cstr(&tPrompt, pSession->sSummary) ||
             !xllm_session__buf_cstr(&tPrompt, "\n</previous_summary>\n\n") ) goto oom;
    }
    if ( !xllm_session__buf_cstr(&tPrompt, "<conversation_prefix>\n") ) goto oom;
    for ( i = 0u; i < pSession->iEntryCount; ++i ) {
        const xllm_session_entry* pEntry = &pSession->pEntries[i];
        if ( pEntry->uSequence <= pSession->uCompactedThrough || pEntry->uSequence > uCandidate ||
             (pEntry->uFlags & XLLM_SESSION_ENTRY_PINNED) != 0u ) continue;
        if ( !xllm_session__append_compaction_entry(&tPrompt, pSession, pEntry) ) goto oom;
    }
    if ( !xllm_session__buf_cstr(&tPrompt, "</conversation_prefix>\n") ) goto oom;
    pCompaction = (xllm_compaction*)calloc(1u, sizeof(*pCompaction));
    if ( !pCompaction ) goto oom;
    pCompaction->pSession = pSession;
    pCompaction->uBaseCompactedThrough = pSession->uCompactedThrough;
    pCompaction->uThroughSequence = uCandidate;
    pCompaction->sPrompt = xllm_session__buf_detach(&tPrompt);
    if ( !pCompaction->sPrompt ) goto oom;
    pCompaction->uEstimatedTokens = xllmEstimateTextTokens(pCompaction->sPrompt);
    return pCompaction;
oom:
    xllm_session__buf_unit(&tPrompt);
    xllmCompactionDestroy(pCompaction);
    xllm_session__error(pError, XLLM_ERROR_OUT_OF_MEMORY, "failed to build compaction transaction");
    return NULL;
}

const char* xllmCompactionPrompt(const xllm_compaction* pCompaction)
{
    return pCompaction ? pCompaction->sPrompt : NULL;
}

uint64_t xllmCompactionThroughSequence(const xllm_compaction* pCompaction)
{
    return pCompaction ? pCompaction->uThroughSequence : 0u;
}

uint64_t xllmCompactionEstimatedTokens(const xllm_compaction* pCompaction)
{
    return pCompaction ? pCompaction->uEstimatedTokens : 0u;
}

static bool xllm_session__heading_at_line(const char* sText, const char* sHeading)
{
    const char* p = sText;
    size_t iHeading = strlen(sHeading);
    while ( p && *p ) {
        const char* q = p;
        size_t i;
        while ( *q == ' ' || *q == '\t' || *q == '#' || *q == '*' ) ++q;
        for ( i = 0u; i < iHeading; ++i ) {
            if ( !q[i] || tolower((unsigned char)q[i]) != tolower((unsigned char)sHeading[i]) ) break;
        }
        if ( i == iHeading ) {
            q += iHeading;
            while ( *q == ' ' || *q == '\t' ) ++q;
            if ( *q == ':' || *q == ';' || *q == '\r' || *q == '\n' || *q == '\0' ) return true;
        }
        p = strchr(p, '\n');
        if ( p ) ++p;
    }
    return false;
}

bool xllmCompactionEvaluateSummary(const xllm_compaction* pCompaction, const char* sSummary,
    xllm_compaction_quality* pQuality, xllm_error* pError)
{
    static const struct { uint32_t uFlag; const char* sHeading; } aSections[] = {
        { XLLM_COMPACTION_SECTION_OBJECTIVE, "Objective" },
        { XLLM_COMPACTION_SECTION_CONSTRAINTS, "Constraints" },
        { XLLM_COMPACTION_SECTION_ARCHITECTURE, "Architecture and decisions" },
        { XLLM_COMPACTION_SECTION_COMPLETED, "Completed work" },
        { XLLM_COMPACTION_SECTION_REPOSITORY_STATE, "Current repository state" },
        { XLLM_COMPACTION_SECTION_VERIFICATION, "Verification evidence" },
        { XLLM_COMPACTION_SECTION_OPEN_ISSUES, "Open issues and risks" },
        { XLLM_COMPACTION_SECTION_NEXT_ACTIONS, "Exact next actions" }
    };
    xllm_compaction_quality tQuality;
    const xllm_session_config* pConfig;
    size_t i;
    if ( pError ) xllmErrorInit(pError);
    if ( !pCompaction || !pCompaction->pSession || !pQuality || !sSummary ) {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT, "compaction, summary, and quality report are required");
        return false;
    }
    memset(&tQuality, 0, sizeof(tQuality));
    pConfig = &pCompaction->pSession->tConfig;
    tQuality.uRequiredSections = pConfig->uCompactionRequiredSections;
    tQuality.uMinimumSummaryTokens = pConfig->uSummaryMinTokens;
    if ( pCompaction->uEstimatedTokens >= 16384u && tQuality.uMinimumSummaryTokens < 256u ) {
        tQuality.uMinimumSummaryTokens = 256u;
    } else if ( pCompaction->uEstimatedTokens >= 4096u && tQuality.uMinimumSummaryTokens < 128u ) {
        tQuality.uMinimumSummaryTokens = 128u;
    }
    tQuality.uMaximumSummaryTokens = pConfig->uSummaryMaxTokens;
    tQuality.uSourceTokens = pCompaction->uEstimatedTokens;
    tQuality.uSummaryTokens = xllmEstimateTextTokens(sSummary);
    for ( i = 0u; i < sizeof(aSections) / sizeof(aSections[0]); ++i ) {
        if ( xllm_session__heading_at_line(sSummary, aSections[i].sHeading) ) {
            tQuality.uPresentSections |= aSections[i].uFlag;
        }
    }
    tQuality.uMissingSections = tQuality.uRequiredSections & ~tQuality.uPresentSections;
    tQuality.bAccepted = sSummary[0] != '\0' && tQuality.uMissingSections == 0u &&
        tQuality.uSummaryTokens >= tQuality.uMinimumSummaryTokens &&
        tQuality.uSummaryTokens <= tQuality.uMaximumSummaryTokens;
    *pQuality = tQuality;
    return true;
}

bool xllmSessionCommitCompaction(xllm_session* pSession, xllm_compaction* pCompaction, const char* sSummary, xllm_error* pError)
{
    char* sCopy;
    xllm_compaction_quality tQuality;
    if ( pError ) { xllmErrorInit(pError); }
    if ( !pSession || !pCompaction || pCompaction->pSession != pSession || pCompaction->bCommitted ||
         !sSummary || !sSummary[0] || pSession->uCompactedThrough != pCompaction->uBaseCompactedThrough ) {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT, "invalid or stale compaction transaction");
        return false;
    }
    if ( !xllmCompactionEvaluateSummary(pCompaction, sSummary, &tQuality, pError) ) return false;
    if ( !tQuality.bAccepted ) {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT, "compaction summary failed the configured quality policy");
        return false;
    }
    sCopy = xllm_session__strdup(sSummary);
    if ( !sCopy ) {
        xllm_session__error(pError, XLLM_ERROR_OUT_OF_MEMORY, "failed to store compaction summary");
        return false;
    }
    if ( !xllm_session__journal_append_compaction(pSession, pCompaction->uThroughSequence,
            pSession->uCompactionCount + 1u, sSummary) ) {
        free(sCopy);
        xllm_session__error(pError, XLLM_ERROR_NETWORK, "failed to append compaction to the session journal");
        return false;
    }
    free(pSession->sSummary);
    pSession->sSummary = sCopy;
    pSession->uCompactedThrough = pCompaction->uThroughSequence;
    ++pSession->uCompactionCount;
    pCompaction->bCommitted = true;
    return true;
}

void xllmCompactionDestroy(xllm_compaction* pCompaction)
{
    if ( !pCompaction ) { return; }
    free(pCompaction->sPrompt);
    free(pCompaction);
}
