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
    for ( i = 0u; i < pSrc->iPartCount; ++i ) {
        if ( !xllmMessageAddPart(pDst, &pSrc->pParts[i]) ) goto fail;
    }
    if ( pSrc->sNative && !xllmMessageSetNative(pDst, pSrc->sNative) ) goto fail;
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
    pConfig->uCompactionRequiredSections = 0u; /* 0 = all sections of the active style */
    pConfig->fPruneTrigger = 0.75;
    pConfig->fCompactTrigger = 0.95;
    /* v3 defaults; Create clamps keep-recent and the summary cap to the
     * window so small-window sessions stay structurally compactable (D6). */
    pConfig->uKeepRecentTokens = 20000u;
    pConfig->uSummaryMaxBytes = 32768u;
    pConfig->uToolResultCapBytes = 2000u;
    pConfig->uJournalMaxBytes = 64u * 1024u * 1024u;
    pConfig->sSummaryStyle = NULL; /* "coding" (Pi) */
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
         (tConfig.uCompactionRequiredSections & ~(XLLM_COMPACTION_SECTION_ALL | XLLM_COMPACTION_SECTION_PI_ALL)) != 0u ) {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT, "invalid compaction quality policy");
        return NULL;
    }
    /* v3 derived caps: keep-recent and the summary budget shrink to the
     * window quarter; explicit D6 violation of the static feasibility
     * check (keep-recent + reserves + summary > window) rejects creation. */
    if ( tConfig.uKeepRecentTokens == 0u ) {
        tConfig.uKeepRecentTokens = (uint32_t)(tConfig.uContextWindowTokens / 4u);
        if ( tConfig.uKeepRecentTokens > 20000u ) { tConfig.uKeepRecentTokens = 20000u; }
    }
    if ( tConfig.uSummaryMaxBytes == 0u ) { tConfig.uSummaryMaxBytes = 32768u; }
    {
        uint64_t uWindowQuarter = tConfig.uContextWindowTokens / 4u;
        uint64_t uSummaryTokenCap = ((uint64_t)tConfig.uSummaryMaxBytes + 3u) / 4u;
        if ( (uint64_t)tConfig.uKeepRecentTokens > uWindowQuarter ) {
            tConfig.uKeepRecentTokens = (uint32_t)uWindowQuarter;
        }
        if ( uSummaryTokenCap > uWindowQuarter ) {
            tConfig.uSummaryMaxBytes = (uint32_t)(uWindowQuarter * 4u);
        }
        if ( (uint64_t)tConfig.uKeepRecentTokens +
             (uint64_t)tConfig.uOutputReserveTokens + tConfig.uSafetyReserveTokens +
             ((uint64_t)tConfig.uSummaryMaxBytes + 3u) / 4u >= tConfig.uContextWindowTokens ) {
            xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT,
                "keep-recent plus reserves plus the summary budget do not fit the context window");
            return NULL;
        }
    }
    if ( tConfig.sSummaryStyle && tConfig.sSummaryStyle[0] &&
         strcmp(tConfig.sSummaryStyle, "coding") != 0 && strcmp(tConfig.sSummaryStyle, "general") != 0 &&
         strcmp(tConfig.sSummaryStyle, "durable") != 0 ) {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT,
            "summary style must be coding, general, or durable");
        return NULL;
    }
    if ( tConfig.uJournalMaxBytes == 0u ) { tConfig.uJournalMaxBytes = 64u * 1024u * 1024u; }
    pSession = (xllm_session*)calloc(1u, sizeof(*pSession));
    if ( !pSession ) {
        xllm_session__error(pError, XLLM_ERROR_OUT_OF_MEMORY, "failed to allocate session");
        return NULL;
    }
    pSession->tConfig = tConfig;
    pSession->uNextSequence = 1u;
    pSession->uFillExact = 0u;
    pSession->bFillExactValid = false;
    pSession->bFillSeen = false;
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
    /* v3 inherited state: strategy/hooks/client are borrowed pointers; the
     * exact fill invalidates on fork (design §4.2) until the next real call. */
    pFork->pOps = pSession->pOps;
    pFork->pHooks = pSession->pHooks;
    pFork->pClient = pSession->pClient;
    pFork->pTestCall = pSession->pTestCall;
    pFork->pTestCallData = pSession->pTestCallData;
    pFork->uSummaryGeneration = pSession->uSummaryGeneration;
    pFork->uSummaryPromptAtBirth = pSession->uSummaryPromptAtBirth;
    pFork->uSummaryOutputAtBirth = pSession->uSummaryOutputAtBirth;
    pFork->uTailFloor = pSession->uTailFloor;
    pFork->bFillSeen = pSession->bFillSeen;
    pFork->bFillExactValid = false;
    pFork->uLastUserSequence = pSession->uLastUserSequence;
    xllm_session__event(pFork, XLLM_SESSION_EVENT_SESSION_FORKED, 0u, 0u, NULL);
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
    free(pSession->sStyleStorage);
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
    if ( pSession->bInHook ) { return 0u; }
    uTurn = pSession->uCurrentTurn + 1u;
    if ( !xllm_session__journal_append_turn(pSession, uTurn) ) { return 0u; }
    if ( pSession->uCurrentTurn != 0u ) {
        xllm_session__event(pSession, XLLM_SESSION_EVENT_TURN_END,
            pSession->uCurrentTurn, 0u, NULL);
    }
    pSession->uCurrentTurn = uTurn;
    xllm_session__event(pSession, XLLM_SESSION_EVENT_TURN_BEGIN, uTurn, 0u, NULL);
    return uTurn;
}

uint64_t xllmSessionCurrentTurn(const xllm_session* pSession)
{
    return pSession ? pSession->uCurrentTurn : 0u;
}

/* L3 preflight (design §7.1): a single message that can never fit is
 * rejected at accounting time, before it can ride into any render. */
static bool xllm_session__cap_ok(const xllm_session* pSession, xllm_role eRole, const char* sContent)
{
    uint32_t uCap = 0u; /* assistant/system content is not cap-checked */
    if ( eRole == XLLM_ROLE_USER ) {
        uCap = pSession->tConfig.uUserMessageCapBytes;
    } else if ( eRole == XLLM_ROLE_TOOL ) {
        uCap = pSession->tConfig.uToolResultCapBytes;
    }
    return uCap == 0u || !sContent || strlen(sContent) <= (size_t)uCap;
}

bool xllmSessionAddMessage(xllm_session* pSession, uint64_t uTurn, const xllm_message* pMessage, uint32_t uFlags)
{
    xllm_session_entry* pNew;
    xllm_session_entry* pEntry;
    size_t iCap;
    if ( !pSession || !pMessage || uTurn > pSession->uCurrentTurn || pSession->uNextSequence == UINT64_MAX ) { return false; }
    if ( pSession->bInHook ) { return false; }
    if ( !xllm_session__cap_ok(pSession, pMessage->eRole, pMessage->sContent) ) {
        return false;
    }
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
    xllm_session__event(pSession, XLLM_SESSION_EVENT_ENTRY_ADDED, pEntry->uSequence, uTurn, NULL);
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
    if ( bOk ) {
        /* The exact-feedback loop: the response usage refreshes governance. */
        xllm_session__record_usage(pSession, &pResponse->tUsage);
    }
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
    if ( (pEntry->uFlags & XLLM_SESSION_ENTRY_PINNED) != 0u ) { return true; }
    return pEntry->uSequence > pSession->uCompactedThrough && pEntry->uSequence > pSession->uTailFloor;
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
    /* Decision model (design §4): with usage feedback the ladder is exact
     * (fill + bounded increment vs thresholds); a session that has never
     * seen any feedback keeps the v2 offline estimate ladder so client-less
     * ledger workflows still function. Estimation never re-enters a live
     * session's decisions once feedback has arrived. */
    if ( pSession->bFillSeen && pSession->bFillExactValid ) {
        pStats->ePressure = xllm_session__pressure_exact(pSession);
    } else if ( pSession->bFillSeen ) {
        pStats->ePressure = XLLM_SESSION_PRESSURE_NONE; /* restored: unknown until the next real call */
    } else if ( uRendered > uInputBudget ) {
        pStats->ePressure = XLLM_SESSION_PRESSURE_OVERFLOW;
    } else if ( uRendered >= pStats->uCompactThresholdTokens ) {
        pStats->ePressure = XLLM_SESSION_PRESSURE_COMPACT;
    } else if ( bPrune ) {
        pStats->ePressure = XLLM_SESSION_PRESSURE_PRUNE;
    } else {
        pStats->ePressure = XLLM_SESSION_PRESSURE_NONE;
    }
    /* v3 observation fields */
    pStats->uFillExact = pSession->bFillExactValid ? pSession->uFillExact : UINT64_MAX;
    pStats->bFillExactValid = pSession->bFillExactValid;
    pStats->uIncrementMax = pSession->bFillExactValid ? pSession->uIncrementMax : 0u;
    pStats->uCachedInputTokens = pSession->uCachedInputTokens;
    pStats->uSummaryTokensExact = pSession->uSummaryOutputAtBirth;
    pStats->uSummaryGeneration = pSession->uSummaryGeneration;
    pStats->uAutoCompactStreak = pSession->uAutoCompactStreak;
    return true;
}

bool xllmSessionGetSummary(const xllm_session* pSession, xllm_session_summary* pSummary)
{
    if ( !pSession || !pSummary ) { return false; }
    pSummary->sText = pSession->sSummary;
    pSummary->uThroughSequence = pSession->uCompactedThrough;
    pSummary->uGeneration = pSession->uSummaryGeneration;
    pSummary->uPromptTokensAtBirth = pSession->uSummaryPromptAtBirth;
    pSummary->uOutputTokensAtBirth = pSession->uSummaryOutputAtBirth;
    return true;
}


uint64_t xllm_session__input_budget(const xllm_session* pSession)
{
    uint64_t uReserved;
    if ( !pSession ) { return 0u; }
    uReserved = (uint64_t)pSession->tConfig.uOutputReserveTokens + pSession->tConfig.uSafetyReserveTokens;
    return pSession->tConfig.uContextWindowTokens > uReserved
        ? pSession->tConfig.uContextWindowTokens - uReserved : 0u;
}

