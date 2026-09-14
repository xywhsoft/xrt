#include "xllm_session_internal.h"

static bool xllm_session__buf_double(xllm_session_buf* pBuf, double fValue)
{
    char sValue[64];
    (void)snprintf(sValue, sizeof(sValue), "%.17g", fValue);
    return xllm_session__buf_cstr(pBuf, sValue);
}

static bool xllm_session__write_tool_call(xllm_session_buf* pJson, const xllm_tool_call* pCall)
{
    return xllm_session__buf_cstr(pJson, "{\"id\":") &&
        xllm_session__json_string(pJson, pCall->sId) &&
        xllm_session__buf_cstr(pJson, ",\"name\":") &&
        xllm_session__json_string(pJson, pCall->sName) &&
        xllm_session__buf_cstr(pJson, ",\"arguments\":") &&
        xllm_session__json_string(pJson, pCall->sArgumentsJson) &&
        xllm_session__buf_char(pJson, '}');
}

bool xllm_session__write_entry(xllm_session_buf* pJson, const xllm_session_entry* pEntry)
{
    size_t i;
    if ( !xllm_session__buf_cstr(pJson, "{\"sequence\":") ||
         !xllm_session__buf_u64(pJson, pEntry->uSequence) ||
         !xllm_session__buf_cstr(pJson, ",\"turn\":") ||
         !xllm_session__buf_u64(pJson, pEntry->uTurn) ||
         !xllm_session__buf_cstr(pJson, ",\"flags\":") ||
         !xllm_session__buf_u64(pJson, pEntry->uFlags) ||
         !xllm_session__buf_cstr(pJson, ",\"role\":") ||
         !xllm_session__buf_u64(pJson, (uint64_t)pEntry->tMessage.eRole) ||
         !xllm_session__buf_cstr(pJson, ",\"content\":") ) return false;
    if ( pEntry->tMessage.sContent ) {
        if ( !xllm_session__json_string(pJson, pEntry->tMessage.sContent) ) return false;
    } else if ( !xllm_session__buf_cstr(pJson, "null") ) return false;
    if ( !xllm_session__buf_cstr(pJson, ",\"reasoning\":") ) return false;
    if ( pEntry->tMessage.sReasoningContent ) {
        if ( !xllm_session__json_string(pJson, pEntry->tMessage.sReasoningContent) ) return false;
    } else if ( !xllm_session__buf_cstr(pJson, "null") ) return false;
    if ( !xllm_session__buf_cstr(pJson, ",\"tool_call_id\":") ) return false;
    if ( pEntry->tMessage.sToolCallId ) {
        if ( !xllm_session__json_string(pJson, pEntry->tMessage.sToolCallId) ) return false;
    } else if ( !xllm_session__buf_cstr(pJson, "null") ) return false;
    if ( !xllm_session__buf_cstr(pJson, ",\"tool_calls\":[") ) return false;
    for ( i = 0u; i < pEntry->tMessage.iToolCallCount; ++i ) {
        if ( i && !xllm_session__buf_char(pJson, ',') ) return false;
        if ( !xllm_session__write_tool_call(pJson, &pEntry->tMessage.pToolCalls[i]) ) return false;
    }
    return xllm_session__buf_cstr(pJson, "]}");
}

bool xllmSessionSave(const xllm_session* pSession, const char* sPath, xllm_error* pError)
{
    xllm_session_buf tJson = {0};
    char* sJson = NULL;
    size_t i;
    bool bOk = false;
    if ( pError ) { xllmErrorInit(pError); }
    if ( !pSession || !sPath || !sPath[0] ) {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT, "session path is required");
        return false;
    }
    if ( !xllm_session__buf_cstr(&tJson, "{\"format\":\"xllm-session\",\"version\":2,\"config\":{\"context_window_tokens\":") ||
         !xllm_session__buf_u64(&tJson, pSession->tConfig.uContextWindowTokens) ||
         !xllm_session__buf_cstr(&tJson, ",\"max_output_tokens\":") ||
         !xllm_session__buf_u64(&tJson, pSession->tConfig.uMaxOutputTokens) ||
         !xllm_session__buf_cstr(&tJson, ",\"output_reserve_tokens\":") ||
         !xllm_session__buf_u64(&tJson, pSession->tConfig.uOutputReserveTokens) ||
         !xllm_session__buf_cstr(&tJson, ",\"safety_reserve_tokens\":") ||
         !xllm_session__buf_u64(&tJson, pSession->tConfig.uSafetyReserveTokens) ||
         !xllm_session__buf_cstr(&tJson, ",\"recent_turns_to_keep\":") ||
         !xllm_session__buf_u64(&tJson, pSession->tConfig.uRecentTurnsToKeep) ||
         !xllm_session__buf_cstr(&tJson, ",\"tool_prune_bytes\":") ||
         !xllm_session__buf_u64(&tJson, pSession->tConfig.uToolPruneBytes) ||
         !xllm_session__buf_cstr(&tJson, ",\"summary_max_tokens\":") ||
         !xllm_session__buf_u64(&tJson, pSession->tConfig.uSummaryMaxTokens) ||
         !xllm_session__buf_cstr(&tJson, ",\"summary_min_tokens\":") ||
         !xllm_session__buf_u64(&tJson, pSession->tConfig.uSummaryMinTokens) ||
         !xllm_session__buf_cstr(&tJson, ",\"compaction_required_sections\":") ||
         !xllm_session__buf_u64(&tJson, pSession->tConfig.uCompactionRequiredSections) ||
         !xllm_session__buf_cstr(&tJson, ",\"prune_trigger\":") ||
         !xllm_session__buf_double(&tJson, pSession->tConfig.fPruneTrigger) ||
         !xllm_session__buf_cstr(&tJson, ",\"compact_trigger\":") ||
         !xllm_session__buf_double(&tJson, pSession->tConfig.fCompactTrigger) ||
         !xllm_session__buf_cstr(&tJson, ",\"keep_recent_tokens\":") ||
         !xllm_session__buf_u64(&tJson, pSession->tConfig.uKeepRecentTokens) ||
         !xllm_session__buf_cstr(&tJson, ",\"summary_max_bytes\":") ||
         !xllm_session__buf_u64(&tJson, pSession->tConfig.uSummaryMaxBytes) ||
         !xllm_session__buf_cstr(&tJson, ",\"user_message_cap_bytes\":") ||
         !xllm_session__buf_u64(&tJson, pSession->tConfig.uUserMessageCapBytes) ||
         !xllm_session__buf_cstr(&tJson, ",\"tool_result_cap_bytes\":") ||
         !xllm_session__buf_u64(&tJson, pSession->tConfig.uToolResultCapBytes) ||
         !xllm_session__buf_cstr(&tJson, ",\"tool_result_total_cap_bytes\":") ||
         !xllm_session__buf_u64(&tJson, pSession->tConfig.uToolResultTotalCapBytes) ||
         !xllm_session__buf_cstr(&tJson, ",\"journal_max_bytes\":") ||
         !xllm_session__buf_u64(&tJson, pSession->tConfig.uJournalMaxBytes) ||
         !xllm_session__buf_cstr(&tJson, ",\"summary_style\":") ) goto oom;
    if ( pSession->tConfig.sSummaryStyle ) {
        if ( !xllm_session__json_string(&tJson, pSession->tConfig.sSummaryStyle) ) goto oom;
    } else if ( !xllm_session__buf_cstr(&tJson, "null") ) goto oom;
    if ( !xllm_session__buf_cstr(&tJson, "},\"next_sequence\":") ||
         !xllm_session__buf_u64(&tJson, pSession->uNextSequence) ||
         !xllm_session__buf_cstr(&tJson, ",\"journal_sequence\":") ||
         !xllm_session__buf_u64(&tJson, pSession->uJournalSequence) ||
         !xllm_session__buf_cstr(&tJson, ",\"current_turn\":") ||
         !xllm_session__buf_u64(&tJson, pSession->uCurrentTurn) ||
         !xllm_session__buf_cstr(&tJson, ",\"compacted_through\":") ||
         !xllm_session__buf_u64(&tJson, pSession->uCompactedThrough) ||
         !xllm_session__buf_cstr(&tJson, ",\"compaction_count\":") ||
         !xllm_session__buf_u64(&tJson, pSession->uCompactionCount) ||
         !xllm_session__buf_cstr(&tJson, ",\"summary_generation\":") ||
         !xllm_session__buf_u64(&tJson, pSession->uSummaryGeneration) ||
         !xllm_session__buf_cstr(&tJson, ",\"summary_prompt_at_birth\":") ||
         !xllm_session__buf_u64(&tJson, pSession->uSummaryPromptAtBirth) ||
         !xllm_session__buf_cstr(&tJson, ",\"summary_output_at_birth\":") ||
         !xllm_session__buf_u64(&tJson, pSession->uSummaryOutputAtBirth) ||
         !xllm_session__buf_cstr(&tJson, ",\"tail_floor\":") ||
         !xllm_session__buf_u64(&tJson, pSession->uTailFloor) ||
         !xllm_session__buf_cstr(&tJson, ",\"fill_seen\":") ||
         !xllm_session__buf_u64(&tJson, pSession->bFillSeen ? 1u : 0u) ||
         !xllm_session__buf_cstr(&tJson, ",\"summary\":") ) goto oom;
    if ( pSession->sSummary ) {
        if ( !xllm_session__json_string(&tJson, pSession->sSummary) ) goto oom;
    } else if ( !xllm_session__buf_cstr(&tJson, "null") ) goto oom;
    if ( !xllm_session__buf_cstr(&tJson, ",\"entries\":[") ) goto oom;
    for ( i = 0u; i < pSession->iEntryCount; ++i ) {
        if ( i && !xllm_session__buf_char(&tJson, ',') ) goto oom;
        if ( !xllm_session__write_entry(&tJson, &pSession->pEntries[i]) ) goto oom;
    }
    if ( !xllm_session__buf_cstr(&tJson, "]}") ) goto oom;
    sJson = xllm_session__buf_detach(&tJson);
    if ( !sJson ) goto oom;
    bOk = xrtFileWriteAtomic(sPath,
        (xbytesview){ (const uint8*)sJson, strlen(sJson) });
    if ( !bOk ) { xllm_session__error(pError, XLLM_ERROR_NETWORK, "failed to atomically write session state"); }
    free(sJson);
    return bOk;
oom:
    free(sJson);
    xllm_session__buf_unit(&tJson);
    xllm_session__error(pError, XLLM_ERROR_OUT_OF_MEMORY, "failed to serialize session state");
    return false;
}

xvalue* xllm_session__json_get(xvalue* pObject, const char* sKey)
{
    xvalue* pValue = pObject && xrtValueIs(pObject, XVALUE_OBJECT) ?
        xrtValueObjectGet(pObject, (xstrview){ sKey, strlen(sKey) }) : NULL;
    return pValue && !xrtValueIs(pValue, XVALUE_NULL) ? pValue : NULL;
}

const char* xllm_session__json_text(xvalue* pObject, const char* sKey)
{
    xvalue* pValue = xllm_session__json_get(pObject, sKey);
    xstrview tText;
    return pValue && xrtValueGetString(pValue, &tText) ? tText.Data : NULL;
}

uint64_t xllm_session__json_u64(xvalue* pObject, const char* sKey, uint64_t uDefault)
{
    xvalue* pValue = xllm_session__json_get(pObject, sKey);
    int64 iValue;
    return pValue && xrtValueGetInt(pValue, &iValue) && iValue >= 0 ?
        (uint64_t)iValue : uDefault;
}

double xllm_session__json_double(xvalue* pObject, const char* sKey, double fDefault)
{
    xvalue* pValue = xllm_session__json_get(pObject, sKey);
    double fValue;
    int64 iValue;
    if ( pValue && xrtValueGetFloat(pValue, &fValue) ) return fValue;
    return pValue && xrtValueGetInt(pValue, &iValue) ? (double)iValue : fDefault;
}

bool xllm_session__load_message(xllm_message* pMessage, xvalue* pEntry)
{
    xvalue* pCalls;
    size_t i;
    int64_t iRole = (int64_t)xllm_session__json_u64(pEntry, "role", UINT64_MAX);
    const char* sText;
    if ( iRole < XLLM_ROLE_SYSTEM || iRole > XLLM_ROLE_TOOL ) { return false; }
    xllmMessageInit(pMessage, (xllm_role)iRole);
    sText = xllm_session__json_text(pEntry, "content");
    if ( sText && !xllmMessageSetContent(pMessage, sText) ) goto fail;
    sText = xllm_session__json_text(pEntry, "reasoning");
    if ( sText && !xllmMessageSetReasoning(pMessage, sText) ) goto fail;
    sText = xllm_session__json_text(pEntry, "tool_call_id");
    if ( sText && !xllmMessageSetToolCallId(pMessage, sText) ) goto fail;
    pCalls = xllm_session__json_get(pEntry, "tool_calls");
    if ( pCalls && xrtValueIs(pCalls, XVALUE_ARRAY) ) {
        size_t uCount = xrtValueCount(pCalls);
        for ( i = 0u; i < uCount; ++i ) {
            xvalue* pCall = xrtValueArrayGet(pCalls, i);
            const char* sId = xllm_session__json_text(pCall, "id");
            const char* sName = xllm_session__json_text(pCall, "name");
            const char* sArguments = xllm_session__json_text(pCall, "arguments");
            if ( !sName || !xllmMessageAddToolCall(pMessage, sId, sName, sArguments) ) goto fail;
        }
    }
    return true;
fail:
    xllmMessageUnit(pMessage);
    return false;
}

xllm_session* xllmSessionLoad(const char* sPath, xllm_error* pError)
{
    char* sJson = NULL;
    size_t iJsonLen = 0u;
    xvalue* pRoot = NULL;
    xvalue* pConfig;
    xvalue* pEntries;
    xllm_session_config tSessionConfig;
    xllm_session* pSession = NULL;
    uint64_t uSavedNext;
    uint64_t uSavedJournal;
    uint64_t uSavedCompacted;
    uint64_t uSavedCompactions;
    const char* sSummary;
    size_t i;
    if ( pError ) { xllmErrorInit(pError); }
    if ( !sPath || !sPath[0] ) {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT, "session path is required");
        return NULL;
    }
    sJson = (char*)xrtFileReadAll(sPath, &iJsonLen);
    if ( !sJson ) {
        xllm_session__error(pError, XLLM_ERROR_NETWORK, "failed to read session state");
        return NULL;
    }
    pRoot = xrtJsonParse((xstrview){ sJson, iJsonLen });
    xrtFree(sJson);
    {
        uint64_t uVersion = pRoot ? xllm_session__json_u64(pRoot, "version", 0u) : 0u;
        if ( !pRoot || !xrtValueIs(pRoot, XVALUE_OBJECT) ||
             strcmp(xllm_session__json_text(pRoot, "format") ? xllm_session__json_text(pRoot, "format") : "", "xllm-session") != 0 ||
             (uVersion != 1u && uVersion != 2u) ) {
            xllm_session__error(pError, XLLM_ERROR_PARSE, "unsupported or invalid session state");
            xrtValueRelease(pRoot);
            return NULL;
        }
    }
    xllmSessionConfigInit(&tSessionConfig);
    pConfig = xllm_session__json_get(pRoot, "config");
    tSessionConfig.uContextWindowTokens = xllm_session__json_u64(pConfig, "context_window_tokens", tSessionConfig.uContextWindowTokens);
    tSessionConfig.uMaxOutputTokens = (uint32_t)xllm_session__json_u64(pConfig, "max_output_tokens", tSessionConfig.uMaxOutputTokens);
    tSessionConfig.uOutputReserveTokens = (uint32_t)xllm_session__json_u64(pConfig, "output_reserve_tokens", 0u);
    tSessionConfig.uSafetyReserveTokens = (uint32_t)xllm_session__json_u64(pConfig, "safety_reserve_tokens", 0u);
    tSessionConfig.uRecentTurnsToKeep = (uint32_t)xllm_session__json_u64(pConfig, "recent_turns_to_keep", tSessionConfig.uRecentTurnsToKeep);
    tSessionConfig.uToolPruneBytes = (uint32_t)xllm_session__json_u64(pConfig, "tool_prune_bytes", tSessionConfig.uToolPruneBytes);
    tSessionConfig.uSummaryMaxTokens = (uint32_t)xllm_session__json_u64(pConfig, "summary_max_tokens", tSessionConfig.uSummaryMaxTokens);
    tSessionConfig.uSummaryMinTokens = (uint32_t)xllm_session__json_u64(pConfig, "summary_min_tokens", tSessionConfig.uSummaryMinTokens);
    tSessionConfig.uCompactionRequiredSections = (uint32_t)xllm_session__json_u64(pConfig,
        "compaction_required_sections", tSessionConfig.uCompactionRequiredSections);
    tSessionConfig.fPruneTrigger = xllm_session__json_double(pConfig, "prune_trigger", tSessionConfig.fPruneTrigger);
    tSessionConfig.fCompactTrigger = xllm_session__json_double(pConfig, "compact_trigger", tSessionConfig.fCompactTrigger);
    /* v3 fields: zero keeps the Create-time derivation (v1 snapshots). */
    tSessionConfig.uKeepRecentTokens = (uint32_t)xllm_session__json_u64(pConfig, "keep_recent_tokens", 0u);
    tSessionConfig.uSummaryMaxBytes = (uint32_t)xllm_session__json_u64(pConfig, "summary_max_bytes", 0u);
    tSessionConfig.uUserMessageCapBytes = (uint32_t)xllm_session__json_u64(pConfig, "user_message_cap_bytes", 0u);
    tSessionConfig.uToolResultCapBytes = (uint32_t)xllm_session__json_u64(pConfig, "tool_result_cap_bytes", 0u);
    tSessionConfig.uToolResultTotalCapBytes = (uint32_t)xllm_session__json_u64(pConfig, "tool_result_total_cap_bytes", 0u);
    tSessionConfig.uJournalMaxBytes = xllm_session__json_u64(pConfig, "journal_max_bytes", 0u);
    tSessionConfig.sSummaryStyle = xllm_session__json_text(pConfig, "summary_style");
    pSession = xllmSessionCreate(&tSessionConfig, pError);
    tSessionConfig.sSummaryStyle = NULL; /* borrowed only for Create above */
    if ( !pSession ) { xrtValueRelease(pRoot); return NULL; }
    /* The session keeps an owned copy of the style so the parsed value's
     * lifetime ends with the DOM. */
    {
        const char* sStyle = xllm_session__json_text(pConfig, "summary_style");
        if ( sStyle && sStyle[0] ) {
            char* sCopy = xllm_session__strdup(sStyle);
            if ( !sCopy ) goto fail;
            pSession->sStyleStorage = sCopy;
            pSession->tConfig.sSummaryStyle = sCopy;
        }
    }
    pSession->uCurrentTurn = xllm_session__json_u64(pRoot, "current_turn", 0u);
    uSavedNext = xllm_session__json_u64(pRoot, "next_sequence", 1u);
    uSavedJournal = xllm_session__json_u64(pRoot, "journal_sequence", 0u);
    uSavedCompacted = xllm_session__json_u64(pRoot, "compacted_through", 0u);
    uSavedCompactions = xllm_session__json_u64(pRoot, "compaction_count", 0u);
    sSummary = xllm_session__json_text(pRoot, "summary");
    if ( sSummary ) {
        pSession->sSummary = xllm_session__strdup(sSummary);
        if ( !pSession->sSummary ) goto fail;
    }
    pEntries = xllm_session__json_get(pRoot, "entries");
    if ( !pEntries || !xrtValueIs(pEntries, XVALUE_ARRAY) ) goto fail;
    for ( i = 0u; i < xrtValueCount(pEntries); ++i ) {
        xvalue* pEntry = xrtValueArrayGet(pEntries, i);
        xllm_message tMessage;
        uint64_t uTurn = xllm_session__json_u64(pEntry, "turn", UINT64_MAX);
        uint64_t uSequence = xllm_session__json_u64(pEntry, "sequence", 0u);
        uint32_t uFlags = (uint32_t)xllm_session__json_u64(pEntry, "flags", 0u);
        if ( uTurn == UINT64_MAX || uTurn > pSession->uCurrentTurn || !xllm_session__load_message(&tMessage, pEntry) ) goto fail;
        if ( !xllmSessionAddMessage(pSession, uTurn, &tMessage, uFlags) ) {
            xllmMessageUnit(&tMessage);
            goto fail;
        }
        xllmMessageUnit(&tMessage);
        if ( uSequence ) { pSession->pEntries[pSession->iEntryCount - 1u].uSequence = uSequence; }
        if ( pSession->uNextSequence <= uSequence ) { pSession->uNextSequence = uSequence + 1u; }
    }
    if ( pSession->uNextSequence < uSavedNext ) { pSession->uNextSequence = uSavedNext; }
    pSession->uJournalSequence = uSavedJournal;
    pSession->uCompactedThrough = uSavedCompacted;
    pSession->uCompactionCount = uSavedCompactions;
    /* v3 state: governance restores as "seen but unknown" so the first real
     * call re-probes (design §4.2); the summary object carries its exact
     * birth usage and generation. */
    pSession->uSummaryGeneration = (uint32_t)xllm_session__json_u64(pRoot, "summary_generation", 0u);
    pSession->uSummaryPromptAtBirth = xllm_session__json_u64(pRoot, "summary_prompt_at_birth", 0u);
    pSession->uSummaryOutputAtBirth = xllm_session__json_u64(pRoot, "summary_output_at_birth", 0u);
    pSession->uTailFloor = xllm_session__json_u64(pRoot, "tail_floor", 0u);
    if ( pSession->uTailFloor <= pSession->uCompactedThrough ) {
        pSession->uTailFloor = 0u; /* only meaningful above the checkpoint */
    }
    pSession->bFillSeen = xllm_session__json_u64(pRoot, "fill_seen", 0u) != 0u;
    pSession->bFillExactValid = false;
    xrtValueRelease(pRoot);
    return pSession;
fail:
    xrtValueRelease(pRoot);
    xllmSessionDestroy(pSession);
    xllm_session__error(pError, XLLM_ERROR_PARSE, "invalid session entry data");
    return NULL;
}
