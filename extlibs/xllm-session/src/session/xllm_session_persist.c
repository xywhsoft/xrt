#include "../internal/xllm_session_internal.h"

void xllmSessionRestoreOptionsInit(xllm_session_restore_options *pOptions)
{
    if (!pOptions)
        return;
    memset(pOptions, 0, sizeof(*pOptions));
    pOptions->uSize = sizeof(*pOptions);
    pOptions->uAbiVersion = XLLM_SESSION_RESTORE_ABI_VERSION;
    pOptions->iMaxSnapshotBytes = 32u * 1024u * 1024u;
    pOptions->iMaxJournalBytes = 64u * 1024u * 1024u;
    pOptions->iMaxValues = 262144u;
    pOptions->iMaxRecords = 65536u;
    pOptions->uMaxDepth = 64u;
}

bool xllm_session__restore_check(const xllm_session_restore_options *pOptions, xllm_error *pError)
{
    if (!pOptions)
        return true;
    if (xrtCancelRequested(pOptions->pCancel))
    {
        xllm_session__error(pError, XLLM_ERROR_CANCELLED, "session restoration cancelled");
        if (pError)
        {
            pError->tCause.eDomain = XLLM_ERROR_DOMAIN_API;
            pError->tCause.eStage = XLLM_ERROR_STAGE_WAIT;
        }
        return false;
    }
    if (pOptions->uDeadline && xllmDeadlineExpired(pOptions->uDeadline))
    {
        xllm_session__error(pError, XLLM_ERROR_TIMEOUT, "session restoration deadline exceeded");
        if (pError)
        {
            pError->tCause.eDomain = XLLM_ERROR_DOMAIN_RESOURCE;
            pError->tCause.eStage = XLLM_ERROR_STAGE_WAIT;
        }
        return false;
    }
    return true;
}

/* Persisted fields are passed to the existing C-string mutation core. Reject
 * embedded NUL instead of silently shortening a value at that boundary. */
bool xllm_session__restore_strings(const xvalue *pValue,
                                   const xllm_session_restore_options *pOptions, xllm_error *pError)
{
    xvaluetype eType = xrtValueType(pValue);
    xstrview tText;
    size_t i;
    if (!xllm_session__restore_check(pOptions, pError))
        return false;
    if (eType == XVALUE_STRING)
    {
        if (!xrtValueGetString(pValue, &tText) || (tText.Size && memchr(tText.Data, 0, tText.Size)))
            goto invalid;
    }
    else if (eType == XVALUE_OBJECT || eType == XVALUE_ARRAY)
    {
        for (i = 0u; i < xrtValueCount(pValue); ++i)
        {
            const xvalue *pChild;
            if (eType == XVALUE_OBJECT)
            {
                pChild = xrtValueObjectAt(pValue, i, &tText);
                if (tText.Size && memchr(tText.Data, 0, tText.Size))
                    goto invalid;
            }
            else
                pChild = xrtValueArrayGet(pValue, i);
            if (!xllm_session__restore_strings(pChild, pOptions, pError))
                return false;
        }
    }
    return true;
invalid:
    xllm_session__error(pError, XLLM_ERROR_PARSE, "embedded NUL in persisted session text");
    return false;
}

/* CRC-32/ISO-HDLC. The checksum covers the exact canonical UTF-8 prefix
 * before the final checksum member, so verification never depends on DOM
 * key ordering or JSON re-serialization. */
static uint32_t xllm_session__checksum_update(uint32_t uCrc, const char *pData, size_t iLen)
{
    static const uint32_t tTable[16] = {0x00000000u, 0x1db71064u, 0x3b6e20c8u, 0x26d930acu,
                                        0x76dc4190u, 0x6b6b51f4u, 0x4db26158u, 0x5005713cu,
                                        0xedb88320u, 0xf00f9344u, 0xd6d6a3e8u, 0xcb61b38cu,
                                        0x9b64c2b0u, 0x86d3d2d4u, 0xa00ae278u, 0xbdbdf21cu};
    size_t i;
    for (i = 0u; i < iLen; ++i)
    {
        uCrc ^= (uint8_t)pData[i];
        uCrc = (uCrc >> 4u) ^ tTable[uCrc & 15u];
        uCrc = (uCrc >> 4u) ^ tTable[uCrc & 15u];
    }
    return uCrc;
}

bool xllm_session__checksum_append(xllm_session_buf *pJson)
{
    char sHex[9];
    uint32_t uCrc;
    if (!pJson || !pJson->pData)
    {
        return false;
    }
    uCrc = ~xllm_session__checksum_update(UINT32_MAX, pJson->pData, pJson->iLen);
    (void)snprintf(sHex, sizeof(sHex), "%08x", (unsigned)uCrc);
    return xllm_session__buf_cstr(pJson, ",\"checksum\":\"") &&
           xllm_session__buf_cstr(pJson, sHex) && xllm_session__buf_cstr(pJson, "\"}");
}

bool xllm_session__checksum_restore(const char *pData, size_t iLen,
                                    const xllm_session_restore_options *pOptions,
                                    xllm_error *pError)
{
    static const char sMarker[] = ",\"checksum\":\"";
    const size_t iTrailer = sizeof(sMarker) - 1u + 8u + 2u;
    const char *pHex;
    uint32_t uExpected = 0u, uCrc = UINT32_MAX;
    size_t i, iPrefix, iOffset = 0u;
    if (!pData || iLen < iTrailer)
        goto invalid;
    i = iLen - iTrailer;
    if (memcmp(pData + i, sMarker, sizeof(sMarker) - 1u) != 0 || pData[iLen - 2u] != '"' ||
        pData[iLen - 1u] != '}')
    {
        goto invalid;
    }
    pHex = pData + i + sizeof(sMarker) - 1u;
    for (size_t j = 0u; j < 8u; ++j)
    {
        unsigned uNibble;
        char ch = pHex[j];
        if (ch >= '0' && ch <= '9')
        {
            uNibble = (unsigned)(ch - '0');
        }
        else if (ch >= 'a' && ch <= 'f')
        {
            uNibble = (unsigned)(ch - 'a') + 10u;
        }
        else if (ch >= 'A' && ch <= 'F')
        {
            uNibble = (unsigned)(ch - 'A') + 10u;
        }
        else
            goto invalid;
        uExpected = (uExpected << 4u) | uNibble;
    }
    iPrefix = i;
    while (iOffset < iPrefix)
    {
        size_t iChunk = iPrefix - iOffset > 65536u ? 65536u : iPrefix - iOffset;
        if (!xllm_session__restore_check(pOptions, pError))
            return false;
        uCrc = xllm_session__checksum_update(uCrc, pData + iOffset, iChunk);
        iOffset += iChunk;
    }
    if (uExpected == ~uCrc)
        return true;
invalid:
    xllm_session__error(pError, XLLM_ERROR_PARSE, "session checksum mismatch");
    return false;
}

bool xllm_session__checksum_verify(const char *pData, size_t iLen)
{
    return xllm_session__checksum_restore(pData, iLen, NULL, NULL);
}

static bool xllm_session__buf_double(xllm_session_buf *pBuf, double fValue)
{
    char sValue[64];
    (void)snprintf(sValue, sizeof(sValue), "%.17g", fValue);
    return xllm_session__buf_cstr(pBuf, sValue);
}

static bool xllm_session__write_tool_call(xllm_session_buf *pJson, const xllm_tool_call *pCall)
{
    return xllm_session__buf_cstr(pJson, "{\"id\":") &&
           xllm_session__json_string(pJson, pCall->sId) &&
           xllm_session__buf_cstr(pJson, ",\"name\":") &&
           xllm_session__json_string(pJson, pCall->sName) &&
           xllm_session__buf_cstr(pJson, ",\"arguments\":") &&
           xllm_session__json_string(pJson, pCall->sArgumentsJson) &&
           xllm_session__buf_char(pJson, '}');
}

static bool xllm_session__write_string_array(xllm_session_buf *pJson, const char *sKey,
                                             char *const *psItems, size_t iCount)
{
    size_t i;
    if (!xllm_session__buf_cstr(pJson, ",\"") || !xllm_session__buf_cstr(pJson, sKey) ||
        !xllm_session__buf_cstr(pJson, "\":["))
    {
        return false;
    }
    for (i = 0u; i < iCount; ++i)
    {
        if ((i && !xllm_session__buf_char(pJson, ',')) ||
            !xllm_session__json_string(pJson, psItems[i]))
        {
            return false;
        }
    }
    return xllm_session__buf_char(pJson, ']');
}

static bool xllm_session__write_u64_array(xllm_session_buf *pJson, const char *sKey,
                                          const uint64_t *puItems, size_t iCount)
{
    size_t i;
    if (!xllm_session__buf_cstr(pJson, ",\"") || !xllm_session__buf_cstr(pJson, sKey) ||
        !xllm_session__buf_cstr(pJson, "\":["))
    {
        return false;
    }
    for (i = 0u; i < iCount; ++i)
    {
        if ((i && !xllm_session__buf_char(pJson, ',')) ||
            !xllm_session__buf_u64(pJson, puItems ? puItems[i] : 0u))
        {
            return false;
        }
    }
    return xllm_session__buf_char(pJson, ']');
}

bool xllm_session__write_entry(xllm_session_buf *pJson, const xllm_session_entry *pEntry)
{
    size_t i;
    if (!xllm_session__buf_cstr(pJson, "{\"sequence\":") ||
        !xllm_session__buf_u64(pJson, pEntry->uSequence) ||
        !xllm_session__buf_cstr(pJson, ",\"turn\":") ||
        !xllm_session__buf_u64(pJson, pEntry->uTurn) ||
        !xllm_session__buf_cstr(pJson, ",\"flags\":") ||
        !xllm_session__buf_u64(pJson, pEntry->uFlags) ||
        !xllm_session__buf_cstr(pJson, ",\"role\":") ||
        !xllm_session__buf_u64(pJson, (uint64_t)pEntry->tMessage.eRole) ||
        !xllm_session__buf_cstr(pJson, ",\"content\":"))
        return false;
    if (pEntry->tMessage.sContent)
    {
        if (!xllm_session__json_string(pJson, pEntry->tMessage.sContent))
            return false;
    }
    else if (!xllm_session__buf_cstr(pJson, "null"))
        return false;
    if (!xllm_session__buf_cstr(pJson, ",\"reasoning\":"))
        return false;
    if (pEntry->tMessage.sReasoningContent)
    {
        if (!xllm_session__json_string(pJson, pEntry->tMessage.sReasoningContent))
            return false;
    }
    else if (!xllm_session__buf_cstr(pJson, "null"))
        return false;
    if (!xllm_session__buf_cstr(pJson, ",\"tool_call_id\":"))
        return false;
    if (pEntry->tMessage.sToolCallId)
    {
        if (!xllm_session__json_string(pJson, pEntry->tMessage.sToolCallId))
            return false;
    }
    else if (!xllm_session__buf_cstr(pJson, "null"))
        return false;
    if (!xllm_session__buf_cstr(pJson, ",\"tool_calls\":["))
        return false;
    for (i = 0u; i < pEntry->tMessage.iToolCallCount; ++i)
    {
        if (i && !xllm_session__buf_char(pJson, ','))
            return false;
        if (!xllm_session__write_tool_call(pJson, &pEntry->tMessage.pToolCalls[i]))
            return false;
    }
    return xllm_session__buf_char(pJson, ']') &&
           xllm_session__write_parts(pJson, &pEntry->tMessage) &&
           xllm_session__buf_char(pJson, '}');
}

bool xllmSessionSave(const xllm_session *pSession, const char *sPath, xllm_error *pError)
{
    xllm_session_buf tJson = {0};
    char *sJson = NULL;
    size_t i;
    bool bOk = false;
    if (pError)
    {
        xllmErrorInit(pError);
    }
    if (!pSession || !sPath || !sPath[0])
    {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT, "session path is required");
        return false;
    }
    if (pSession->eState == XLLM_SESSION_STATE_MODEL_IN_FLIGHT ||
        xllm_session__state_is_compaction(pSession->eState) ||
        pSession->eState == XLLM_SESSION_STATE_RECOVERY_REQUIRED ||
        pSession->eState == XLLM_SESSION_STATE_CORRUPT)
    {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT,
                            "session state is transient and cannot be snapshotted");
        return false;
    }
    if (!xllm_session__buf_cstr(&tJson, "{\"format\":\"xllm-session\",\"version\":") ||
        !xllm_session__buf_u64(&tJson, XLLM_SESSION_PERSISTENCE_SCHEMA_VERSION) ||
        !xllm_session__buf_cstr(&tJson, ",\"config\":{\"context_window_tokens\":") ||
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
        !xllm_session__buf_cstr(&tJson, ",\"window_mode\":") ||
        !xllm_session__buf_u64(&tJson, pSession->tConfig.eWindowMode) ||
        !xllm_session__buf_cstr(&tJson, ",\"max_input_tokens\":") ||
        !xllm_session__buf_u64(&tJson, pSession->tConfig.uMaxInputTokens) ||
        !xllm_session__buf_cstr(&tJson, ",\"journal_durability\":") ||
        !xllm_session__buf_u64(&tJson, pSession->tConfig.eJournalDurability) ||
        !xllm_session__buf_cstr(&tJson, ",\"summary_style\":"))
        goto oom;
    if (pSession->tConfig.sSummaryStyle)
    {
        if (!xllm_session__json_string(&tJson, pSession->tConfig.sSummaryStyle))
            goto oom;
    }
    else if (!xllm_session__buf_cstr(&tJson, "null"))
        goto oom;
    if (!xllm_session__buf_cstr(&tJson, "},\"next_sequence\":") ||
        !xllm_session__buf_u64(&tJson, pSession->uNextSequence) ||
        !xllm_session__buf_cstr(&tJson, ",\"checkpoint_sequence\":") ||
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
        !xllm_session__buf_cstr(&tJson, ",\"context_usage\":"))
        goto oom;
    if (pSession->bFillExactValid)
    {
        if (!xllm_session__buf_cstr(&tJson, "{\"fill_tokens\":") ||
            !xllm_session__buf_u64(&tJson, pSession->uFillExact) ||
            !xllm_session__buf_cstr(&tJson, ",\"prefix_estimate\":") ||
            !xllm_session__buf_u64(&tJson, pSession->uFillEstimate) ||
            !xllm_session__buf_cstr(&tJson, ",\"through_sequence\":") ||
            !xllm_session__buf_u64(&tJson, pSession->uFillSequence) ||
            !xllm_session__buf_cstr(&tJson, ",\"cached_input_tokens\":") ||
            !xllm_session__buf_u64(&tJson, pSession->uCachedInputTokens) ||
            !xllm_session__buf_cstr(&tJson, ",\"profile_fingerprint\":") ||
            !xllm_session__buf_u64(&tJson, pSession->uProfileFingerprint) ||
            !xllm_session__buf_char(&tJson, '}'))
            goto oom;
    }
    else if (!xllm_session__buf_cstr(&tJson, "null"))
        goto oom;
    if (!xllm_session__buf_cstr(&tJson, ",\"summary\":"))
        goto oom;
    if (pSession->sSummary)
    {
        if (!xllm_session__json_string(&tJson, pSession->sSummary))
            goto oom;
    }
    else if (!xllm_session__buf_cstr(&tJson, "null"))
        goto oom;
    if (!xllm_session__write_string_array(&tJson, "read_files", pSession->psReadFiles,
                                          pSession->iReadFileCount) ||
        !xllm_session__write_u64_array(&tJson, "read_file_sequences", pSession->puReadFileSequences,
                                       pSession->iReadFileCount) ||
        !xllm_session__write_string_array(&tJson, "modified_files", pSession->psModifiedFiles,
                                          pSession->iModifiedFileCount) ||
        !xllm_session__write_u64_array(&tJson, "modified_file_sequences",
                                       pSession->puModifiedFileSequences,
                                       pSession->iModifiedFileCount))
        goto oom;
    if (!xllm_session__buf_cstr(&tJson, ",\"entries\":["))
        goto oom;
    for (i = 0u; i < pSession->iEntryCount; ++i)
    {
        if (i && !xllm_session__buf_char(&tJson, ','))
            goto oom;
        if (!xllm_session__write_entry(&tJson, &pSession->pEntries[i]))
            goto oom;
    }
    if (!xllm_session__buf_char(&tJson, ']') || !xllm_session__checksum_append(&tJson))
        goto oom;
    sJson = xllm_session__buf_detach(&tJson);
    if (!sJson)
        goto oom;
    bOk = xllm_session__storage_atomic_write(sPath,
                                             (xbytesview){(const uint8 *)sJson, strlen(sJson)});
    if (!bOk)
    {
        xllm_session__storage_error(pError, XLLM_ERROR_STAGE_WRITE, "snapshot-write", sPath,
                                    "failed to atomically write session state");
    }
    free(sJson);
    return bOk;
oom:
    free(sJson);
    xllm_session__buf_unit(&tJson);
    xllm_session__error(pError, XLLM_ERROR_OUT_OF_MEMORY, "failed to serialize session state");
    return false;
}

xvalue *xllm_session__json_get(xvalue *pObject, const char *sKey)
{
    xvalue *pValue = pObject && xrtValueIs(pObject, XVALUE_OBJECT)
                         ? xrtValueObjectGet(pObject, (xstrview){sKey, strlen(sKey)})
                         : NULL;
    return pValue && !xrtValueIs(pValue, XVALUE_NULL) ? pValue : NULL;
}

const char *xllm_session__json_text(xvalue *pObject, const char *sKey)
{
    xvalue *pValue = xllm_session__json_get(pObject, sKey);
    xstrview tText;
    return pValue && xrtValueGetString(pValue, &tText) ? tText.Data : NULL;
}

bool xllm_session__json_uint(xvalue* pObject, const char* sKey, uint64_t* puValue)
{
    xvalue* pValue = xllm_session__json_get(pObject, sKey);
    int64 iSigned;
    if ( !pValue ) return false;
    if ( xrtValueIs(pValue, XVALUE_UINT) ) return xrtValueGetUInt(pValue, puValue);
    if ( xrtValueIs(pValue, XVALUE_INT) && xrtValueGetInt(pValue, &iSigned) && iSigned >= 0 ) {
        *puValue = (uint64_t)iSigned;
        return true;
    }
    return false;
}

bool xllm_session__json_uint_valid(xvalue* pObject, const char* sKey, uint64_t uMax, bool bRequired)
{
    uint64_t uValue;
    xvalue* pValue;
    if ( !pObject || !xrtValueIs(pObject, XVALUE_OBJECT) ) return false;
    pValue = xrtValueObjectGet(pObject, xrtStrView(sKey));
    if ( !pValue ) return !bRequired;
    return xllm_session__json_uint(pObject, sKey, &uValue) && uValue <= uMax;
}

static bool xllm_session__load_text(xvalue* pObject, const char* sKey, const char** psText)
{
    xvalue* pValue;
    xstrview tText;
    *psText = NULL;
    if ( !pObject || !xrtValueIs(pObject, XVALUE_OBJECT) ) return false;
    pValue = xllm_session__json_get(pObject, sKey);
    if ( !pValue ) return true;
    if ( !xrtValueGetString(pValue, &tText) || memchr(tText.Data, '\0', tText.Size) ) return false;
    *psText = tText.Data;
    return true;
}

static bool xllm_session__numeric_fields_valid(xvalue* pObject,
    const char* const* psFields, size_t iCount, uint64_t uMax)
{
    size_t i;
    for ( i = 0u; i < iCount; ++i ) {
        if ( !xllm_session__json_uint_valid(pObject, psFields[i], uMax, false) ) return false;
    }
    return true;
}

static bool xllm_session__snapshot_fields_valid(xvalue* pRoot)
{
    static const char* const aRoot[] = {
        "current_turn", "next_sequence", "journal_sequence", "compacted_through", "compaction_count",
        "summary_prompt_at_birth", "summary_output_at_birth", "tail_floor"
    };
    static const char* const aConfig32[] = {
        "max_output_tokens", "output_reserve_tokens", "safety_reserve_tokens", "recent_turns_to_keep",
        "tool_prune_bytes", "summary_max_tokens", "summary_min_tokens", "compaction_required_sections",
        "keep_recent_tokens", "summary_max_bytes", "user_message_cap_bytes", "tool_result_cap_bytes",
        "tool_result_total_cap_bytes"
    };
    static const char* const aConfig64[] = { "context_window_tokens", "journal_max_bytes" };
    static const char* const aThresholds[] = { "prune_trigger", "compact_trigger" };
    xvalue* pConfig = xrtValueObjectGet(pRoot, xrtStrView("config"));
    const char* sText;
    size_t i;
    if ( !xllm_session__numeric_fields_valid(pRoot, aRoot, sizeof(aRoot) / sizeof(aRoot[0]), UINT64_MAX) ||
         !xllm_session__json_uint_valid(pRoot, "summary_generation", UINT32_MAX, false) ||
         !xllm_session__json_uint_valid(pRoot, "fill_seen", 1u, false) ||
         !xllm_session__load_text(pRoot, "summary", &sText) ) return false;
    if ( !pConfig ) return true; /* old snapshots may omit the optional config */
    if ( !xrtValueIs(pConfig, XVALUE_OBJECT) ||
         !xllm_session__numeric_fields_valid(pConfig, aConfig32, sizeof(aConfig32) / sizeof(aConfig32[0]), UINT32_MAX) ||
         !xllm_session__numeric_fields_valid(pConfig, aConfig64, sizeof(aConfig64) / sizeof(aConfig64[0]), UINT64_MAX) ||
         !xllm_session__load_text(pConfig, "summary_style", &sText) ) return false;
    for ( i = 0u; i < sizeof(aThresholds) / sizeof(aThresholds[0]); ++i ) {
        xvalue* pValue = xrtValueObjectGet(pConfig, xrtStrView(aThresholds[i]));
        if ( pValue && !xrtValueIs(pValue, XVALUE_FLOAT) &&
             !xrtValueIs(pValue, XVALUE_INT) && !xrtValueIs(pValue, XVALUE_UINT) ) return false;
    }
    return true;
}

uint64_t xllm_session__json_u64(xvalue* pObject, const char* sKey, uint64_t uDefault)
{
    uint64_t uValue;
    return xllm_session__json_uint(pObject, sKey, &uValue) ? uValue : uDefault;
}

double xllm_session__json_double(xvalue* pObject, const char* sKey, double fDefault)
{
    xvalue* pValue = xllm_session__json_get(pObject, sKey);
    double fValue;
    int64 iValue;
    uint64 uValue;
    if ( pValue && xrtValueIs(pValue, XVALUE_FLOAT) && xrtValueGetFloat(pValue, &fValue) ) return fValue;
    if ( pValue && xrtValueIs(pValue, XVALUE_INT) && xrtValueGetInt(pValue, &iValue) ) return (double)iValue;
    return pValue && xrtValueIs(pValue, XVALUE_UINT) && xrtValueGetUInt(pValue, &uValue) ? (double)uValue : fDefault;
}

bool xllm_session__load_message(xllm_message *pMessage, xvalue *pEntry, uint64_t uVersion,
                                const xllm_session_restore_options *pOptions, xllm_error *pError)
{
    xvalue *pCalls;
    size_t i;
    uint64_t iRole;
    const char *sText;
    if (!xllm_session__json_uint(pEntry, "role", &iRole) || iRole > XLLM_ROLE_TOOL)
    {
        return false;
    }
    xllmMessageInit(pMessage, (xllm_role)iRole);
    if (!xllm_session__load_text(pEntry, "content", &sText)) goto fail;
    if (sText && !xllmMessageSetContent(pMessage, sText))
        goto fail;
    if (!xllm_session__load_text(pEntry, "reasoning", &sText)) goto fail;
    if (sText && !xllmMessageSetReasoning(pMessage, sText))
        goto fail;
    if (!xllm_session__load_text(pEntry, "tool_call_id", &sText)) goto fail;
    if (sText && !xllmMessageSetToolCallId(pMessage, sText))
        goto fail;
    pCalls = xllm_session__json_get(pEntry, "tool_calls");
    if (pCalls && !xrtValueIs(pCalls, XVALUE_ARRAY)) goto fail;
    if (pCalls)
    {
        size_t uCount = xrtValueCount(pCalls);
        for (i = 0u; i < uCount; ++i)
        {
            xvalue *pCall = xrtValueArrayGet(pCalls, i);
            const char *sId = xllm_session__json_text(pCall, "id");
            const char *sName = xllm_session__json_text(pCall, "name");
            const char *sArguments = xllm_session__json_text(pCall, "arguments");
            if (!xllm_session__load_text(pCall, "id", &sId) ||
                !xllm_session__load_text(pCall, "name", &sName) ||
                !xllm_session__load_text(pCall, "arguments", &sArguments) ||
                !sName || !xllmMessageAddToolCall(pMessage, sId, sName, sArguments))
                goto fail;
        }
    }
    if (!xllm_session__load_parts(pMessage, pEntry, uVersion, pOptions, pError))
        goto fail;
    return true;
fail:
    xllmMessageUnit(pMessage);
    return false;
}

xllm_session *xllm_session__load_snapshot(const char *sJson, size_t iJsonLen,
                                          const xllm_session_restore_options *pOptions,
                                          xllm_error *pError)
{
    xvalue *pRoot = NULL;
    xvalue *pConfig;
    xvalue *pEntries;
    xllm_session_config tSessionConfig;
    xllm_session *pSession = NULL;
    uint64_t uSavedNext;
    uint64_t uSavedJournal;
    uint64_t uSavedCompacted;
    uint64_t uSavedCompactions;
    uint64_t uVersion;
    const char *sSummary;
    size_t i;
    if (pError)
    {
        xllmErrorInit(pError);
    }
    if (!xllm_session__restore_check(pOptions, pError))
        return NULL;
    if (pOptions)
    {
        xjsonreadconfig tRead;
        xrtJsonReadConfigInit(&tRead);
        tRead.MaxInputBytes = pOptions->iMaxSnapshotBytes;
        tRead.MaxValues = pOptions->iMaxValues;
        tRead.MaxDepth = pOptions->uMaxDepth;
        pRoot = xrtJsonRead((xstrview){sJson, iJsonLen}, &tRead);
    }
    else
        pRoot = xrtJsonParse((xstrview){sJson, iJsonLen});
    if (!pRoot && xrtGetError() &&
        (xrtErrorKind(xrtGetError()) == XERR_MEMORY ||
         (pOptions && xrtErrorCode(xrtGetError()) == XJSON_ERROR_LIMIT)))
    {
        xllm_session__error(pError,
                            xrtErrorKind(xrtGetError()) == XERR_MEMORY ? XLLM_ERROR_OUT_OF_MEMORY
                                                                       : XLLM_ERROR_LIMIT,
                            "cannot parse session state within budget");
        return NULL;
    }
    uVersion = pRoot ? xllm_session__json_u64(pRoot, "version", 0u) : 0u;
    if (!pRoot || !xrtValueIs(pRoot, XVALUE_OBJECT) ||
        strcmp(xllm_session__json_text(pRoot, "format") ? xllm_session__json_text(pRoot, "format")
                                                        : "",
               "xllm-session") != 0 ||
        (uVersion < 1u || uVersion > 4u))
    {
        xllm_session__error(pError, XLLM_ERROR_PARSE,
                            "unsupported, invalid, or checksum-damaged session state");
        xrtValueRelease(pRoot);
        return NULL;
    }
    if (!xllm_session__restore_check(pOptions, pError) ||
        (uVersion >= 3u && !xllm_session__checksum_restore(sJson, iJsonLen, pOptions, pError)) ||
        (pOptions && !xllm_session__restore_strings(pRoot, pOptions, pError)))
    {
        xrtValueRelease(pRoot);
        return NULL;
    }
    if ( !xllm_session__snapshot_fields_valid(pRoot) ) goto fail;
    xllmSessionConfigInit(&tSessionConfig);
    pConfig = xllm_session__json_get(pRoot, "config");
    tSessionConfig.uContextWindowTokens = xllm_session__json_u64(
        pConfig, "context_window_tokens", tSessionConfig.uContextWindowTokens);
    tSessionConfig.uMaxOutputTokens = (uint32_t)xllm_session__json_u64(
        pConfig, "max_output_tokens", tSessionConfig.uMaxOutputTokens);
    tSessionConfig.uOutputReserveTokens =
        (uint32_t)xllm_session__json_u64(pConfig, "output_reserve_tokens", 0u);
    tSessionConfig.uSafetyReserveTokens =
        (uint32_t)xllm_session__json_u64(pConfig, "safety_reserve_tokens", 0u);
    tSessionConfig.uRecentTurnsToKeep = (uint32_t)xllm_session__json_u64(
        pConfig, "recent_turns_to_keep", tSessionConfig.uRecentTurnsToKeep);
    tSessionConfig.uToolPruneBytes = (uint32_t)xllm_session__json_u64(
        pConfig, "tool_prune_bytes", tSessionConfig.uToolPruneBytes);
    tSessionConfig.uSummaryMaxTokens = (uint32_t)xllm_session__json_u64(
        pConfig, "summary_max_tokens", tSessionConfig.uSummaryMaxTokens);
    tSessionConfig.uSummaryMinTokens = (uint32_t)xllm_session__json_u64(
        pConfig, "summary_min_tokens", tSessionConfig.uSummaryMinTokens);
    tSessionConfig.uCompactionRequiredSections = (uint32_t)xllm_session__json_u64(
        pConfig, "compaction_required_sections", tSessionConfig.uCompactionRequiredSections);
    tSessionConfig.fPruneTrigger =
        xllm_session__json_double(pConfig, "prune_trigger", tSessionConfig.fPruneTrigger);
    tSessionConfig.fCompactTrigger =
        xllm_session__json_double(pConfig, "compact_trigger", tSessionConfig.fCompactTrigger);
    /* v3 fields: zero keeps the Create-time derivation (v1 snapshots). */
    tSessionConfig.uKeepRecentTokens =
        (uint32_t)xllm_session__json_u64(pConfig, "keep_recent_tokens", 0u);
    tSessionConfig.uSummaryMaxBytes =
        (uint32_t)xllm_session__json_u64(pConfig, "summary_max_bytes", 0u);
    tSessionConfig.uUserMessageCapBytes =
        (uint32_t)xllm_session__json_u64(pConfig, "user_message_cap_bytes", 0u);
    tSessionConfig.uToolResultCapBytes =
        (uint32_t)xllm_session__json_u64(pConfig, "tool_result_cap_bytes", 0u);
    tSessionConfig.uToolResultTotalCapBytes =
        (uint32_t)xllm_session__json_u64(pConfig, "tool_result_total_cap_bytes", 0u);
    tSessionConfig.uJournalMaxBytes = xllm_session__json_u64(pConfig, "journal_max_bytes", 0u);
    tSessionConfig.eWindowMode = (xllm_window_mode)xllm_session__json_u64(
        pConfig, "window_mode", XLLM_WINDOW_SHARED_CONTEXT);
    tSessionConfig.uMaxInputTokens = xllm_session__json_u64(pConfig, "max_input_tokens", 0u);
    tSessionConfig.eJournalDurability = (xllm_session_durability)xllm_session__json_u64(
        pConfig, "journal_durability", tSessionConfig.eJournalDurability);
    tSessionConfig.sSummaryStyle = xllm_session__json_text(pConfig, "summary_style");
    pSession = xllmSessionCreate(&tSessionConfig, pError);
    if (!pSession)
    {
        if (pError && pError->eCode == XLLM_ERROR_INVALID_ARGUMENT)
            xllm_session__error(pError, XLLM_ERROR_PARSE, "invalid saved session config");
        xrtValueRelease(pRoot);
        return NULL;
    }
    pSession->uCurrentTurn = xllm_session__json_u64(pRoot, "current_turn", 0u);
    pSession->bRestoring = true;
    uSavedNext = xllm_session__json_u64(pRoot, "next_sequence", 1u);
    uSavedJournal = xllm_session__json_u64(
        pRoot, uVersion >= 3u ? "checkpoint_sequence" : "journal_sequence", 0u);
    uSavedCompacted = xllm_session__json_u64(pRoot, "compacted_through", 0u);
    uSavedCompactions = xllm_session__json_u64(pRoot, "compaction_count", 0u);
    sSummary = xllm_session__json_text(pRoot, "summary");
    if (sSummary)
    {
        pSession->sSummary = xllm_session__strdup(sSummary);
        if (!pSession->sSummary)
            goto fail;
    }
    pEntries = xllm_session__json_get(pRoot, "entries");
    if (!pEntries || !xrtValueIs(pEntries, XVALUE_ARRAY))
        goto fail;
    for (i = 0u; i < xrtValueCount(pEntries); ++i)
    {
        if (!xllm_session__restore_check(pOptions, pError))
            goto fail;
        xvalue *pEntry = xrtValueArrayGet(pEntries, i);
        xllm_message tMessage;
        uint64_t uTurn;
        uint64_t uSequence = pSession->uNextSequence;
        uint32_t uFlags = (uint32_t)xllm_session__json_u64(pEntry, "flags", 0u);
        if (!xllm_session__json_uint(pEntry, "turn", &uTurn) ||
        !xllm_session__json_uint_valid(pEntry, "sequence", UINT64_MAX - 1u, false) ||
        !xllm_session__json_uint_valid(pEntry, "flags", UINT32_MAX, false) ||
        ((uSequence = xllm_session__json_u64(pEntry, "sequence", uSequence)) < pSession->uNextSequence) ||
        uTurn > pSession->uCurrentTurn ||
            !xllm_session__load_message(&tMessage, pEntry, uVersion, pOptions, pError))
            goto fail;
        if (!xllmSessionAddMessage(pSession, uTurn, &tMessage, uFlags))
        {
            xllmMessageUnit(&tMessage);
            goto fail;
        }
        xllmMessageUnit(&tMessage);
        if (uSequence)
        {
            pSession->pEntries[pSession->iEntryCount - 1u].uSequence = uSequence;
        }
        if (pSession->uNextSequence <= uSequence)
        {
            pSession->uNextSequence = uSequence + 1u;
        }
    }
    if (!xrtValueObjectGet(pRoot, xrtStrView("next_sequence"))) uSavedNext = pSession->uNextSequence;
    if (!uSavedNext || uSavedNext < pSession->uNextSequence ||
        uSavedCompacted >= uSavedNext ||
        xllm_session__json_u64(pRoot, "tail_floor", 0u) >= uSavedNext) goto fail;
    pSession->uNextSequence = uSavedNext;
    pSession->uJournalSequence = uSavedJournal;
    pSession->uCompactedThrough = uSavedCompacted;
    pSession->uCompactionCount = uSavedCompactions;
    /* Optional usage checkpoint is additive to schema 4. Old snapshots use
     * full-context estimation; new ones retain the exact prefix anchor. */
    pSession->uSummaryGeneration =
        (uint32_t)xllm_session__json_u64(pRoot, "summary_generation", 0u);
    pSession->uSummaryPromptAtBirth = xllm_session__json_u64(pRoot, "summary_prompt_at_birth", 0u);
    pSession->uSummaryOutputAtBirth = xllm_session__json_u64(pRoot, "summary_output_at_birth", 0u);
    pSession->uTailFloor = xllm_session__json_u64(pRoot, "tail_floor", 0u);
    if (pSession->uTailFloor <= pSession->uCompactedThrough)
    {
        pSession->uTailFloor = 0u; /* only meaningful above the checkpoint */
    }
    pSession->bFillSeen = xllm_session__json_u64(pRoot, "fill_seen", 0u) != 0u;
    pSession->bFillExactValid = false;
    if (!xllm_session__restore_usage(pSession, xllm_session__json_get(pRoot, "context_usage")))
        goto fail;
    { /* Sequence arrays are additive v3 fields; old snapshots use zero. */
        xvalue *pLedger;
        xvalue *pSequences;
        size_t n, iCount;
        pLedger = xllm_session__json_get(pRoot, "read_files");
        if (pLedger && !xrtValueIs(pLedger, XVALUE_ARRAY)) goto fail;
        pSequences = xllm_session__json_get(pRoot, "read_file_sequences");
        if (pSequences && (!xrtValueIs(pSequences, XVALUE_ARRAY) || !pLedger ||
                           xrtValueCount(pSequences) != xrtValueCount(pLedger)))
        {
            goto fail;
        }
        if (pLedger && xrtValueIs(pLedger, XVALUE_ARRAY))
        {
            iCount = xrtValueCount(pLedger);
            for (n = 0u; n < iCount; ++n)
            {
                if (!xllm_session__restore_check(pOptions, pError))
                    goto fail;
                xstrview tText;
                xvalue *pItem = xrtValueArrayGet(pLedger, n);
                uint64_t uAfterSequence = 0u;
                if (pSequences)
                {
                    int64 iSequence;
                    xvalue *pValue = xrtValueArrayGet(pSequences, n);
                    if (!pValue || !xrtValueGetInt(pValue, &iSequence) || iSequence < 0 ||
                        (uint64_t)iSequence >= pSession->uNextSequence)
                    {
                        goto fail;
                    }
                    uAfterSequence = (uint64_t)iSequence;
                }
                if (!xrtValueGetString(pItem, &tText) || !tText.Data || !tText.Size ||
                    memchr(tText.Data, 0, tText.Size) ||
                    !xllm_session__note_file(&pSession->psReadFiles, &pSession->puReadFileSequences,
                                             &pSession->iReadFileCount, &pSession->iReadFileCap,
                                             tText.Data, uAfterSequence, NULL))
                {
                    goto fail;
                }
            }
        }
        pLedger = xllm_session__json_get(pRoot, "modified_files");
        if (pLedger && !xrtValueIs(pLedger, XVALUE_ARRAY)) goto fail;
        pSequences = xllm_session__json_get(pRoot, "modified_file_sequences");
        if (pSequences && (!xrtValueIs(pSequences, XVALUE_ARRAY) || !pLedger ||
                           xrtValueCount(pSequences) != xrtValueCount(pLedger)))
        {
            goto fail;
        }
        if (pLedger && xrtValueIs(pLedger, XVALUE_ARRAY))
        {
            iCount = xrtValueCount(pLedger);
            for (n = 0u; n < iCount; ++n)
            {
                if (!xllm_session__restore_check(pOptions, pError))
                    goto fail;
                xstrview tText;
                xvalue *pItem = xrtValueArrayGet(pLedger, n);
                uint64_t uAfterSequence = 0u;
                if (pSequences)
                {
                    int64 iSequence;
                    xvalue *pValue = xrtValueArrayGet(pSequences, n);
                    if (!pValue || !xrtValueGetInt(pValue, &iSequence) || iSequence < 0 ||
                        (uint64_t)iSequence >= pSession->uNextSequence)
                    {
                        goto fail;
                    }
                    uAfterSequence = (uint64_t)iSequence;
                }
                if (!xrtValueGetString(pItem, &tText) || !tText.Data || !tText.Size ||
                    memchr(tText.Data, 0, tText.Size) ||
                    !xllm_session__note_file(
                        &pSession->psModifiedFiles, &pSession->puModifiedFileSequences,
                        &pSession->iModifiedFileCount, &pSession->iModifiedFileCap, tText.Data,
                        uAfterSequence, NULL))
                {
                    goto fail;
                }
            }
        }
    }
    if (!xllm_session__restore_check(pOptions, pError))
        goto fail;
    pSession->bRestoring = false;
    xrtValueRelease(pRoot);
    return pSession;
fail:
    {
        bool bOutOfMemory = xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_MEMORY;
        if (pSession)
        {
            pSession->bRestoring = false;
        }
        xrtValueRelease(pRoot);
        xllmSessionDestroy(pSession);
        if (pError &&
            (pError->eCode == XLLM_ERROR_CANCELLED || pError->eCode == XLLM_ERROR_TIMEOUT ||
             pError->eCode == XLLM_ERROR_OUT_OF_MEMORY))
            return NULL;
        xllm_session__error(pError, bOutOfMemory ? XLLM_ERROR_OUT_OF_MEMORY : XLLM_ERROR_PARSE,
                            bOutOfMemory ? "out of memory while loading session state"
                                         : "invalid session entry data");
        return NULL;
    }
}

xllm_session *xllmSessionLoad(const char *sPath, xllm_error *pError)
{
    char *pData;
    size_t iBytes = 0u;
    xllm_session *pSession;
    if (pError)
        xllmErrorInit(pError);
    if (!sPath || !sPath[0])
    {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT, "session path is required");
        return NULL;
    }
    pData = (char *)xllm_session__storage_read_all(sPath, &iBytes,
                                                   XLLM_SESSION_TEST_STORAGE_SNAPSHOT_READ);
    if (!pData)
    {
        xllm_session__storage_error(pError, XLLM_ERROR_STAGE_READ, "snapshot-read", sPath,
                                    "failed to read session state");
        return NULL;
    }
    pSession = xllm_session__load_snapshot(pData, iBytes, NULL, pError);
    xrtFree(pData);
    return pSession;
}
