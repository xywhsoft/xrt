#include "../internal/xllm_session_internal.h"

static bool xllm_session__journal_prefix(xllm_session_buf *pRecord, const xllm_session *pSession,
                                         const char *sOperation)
{
    if (pSession->uJournalSequence == UINT64_MAX)
    {
        return false;
    }
    return xllm_session__buf_cstr(pRecord, "{\"format\":\"xllm-session-journal\",\"version\":") &&
           xllm_session__buf_u64(pRecord, XLLM_SESSION_PERSISTENCE_SCHEMA_VERSION) &&
           xllm_session__buf_cstr(pRecord, ",\"sequence\":") &&
           xllm_session__buf_u64(pRecord, pSession->uJournalSequence + 1u) &&
           xllm_session__buf_cstr(pRecord, ",\"type\":") &&
           xllm_session__json_string(pRecord, sOperation);
}

static bool xllm_session__journal_rollback(const xllm_session *pSession, bool bExisted,
                                           uint64_t iOriginalSize)
{
    bool bNowExists;
    uint64_t iNowSize;
    if (!xllm_session__storage_path_info(pSession->sJournalPath, &bNowExists, &iNowSize))
    {
        return false;
    }
    (void)iNowSize;
    if (!bNowExists)
    {
        return !bExisted;
    }
    if (bExisted)
    {
        return xllm_session__storage_set_size(pSession->sJournalPath, iOriginalSize);
    }
    return xllm_session__storage_delete(pSession->sJournalPath);
}

static bool xllm_session__journal_write(xllm_session *pSession, xllm_session_buf *pRecord)
{
    bool bJournalExisted;
    uint64_t iOriginalSize;
    xfile File;
    bool bWritten;
    bool bFlushed = true;
    bool bClosed;
    if (!pSession->sJournalPath)
    {
        return true;
    }
    pSession->bHasLastPersistenceError = false;
    xllmErrorInit(&pSession->tLastPersistenceError);
    if (pSession->uJournalSequence == UINT64_MAX)
    {
        xllm_session__error(&pSession->tLastPersistenceError, XLLM_ERROR_LIMIT,
                            "session journal sequence is exhausted");
        pSession->bHasLastPersistenceError = true;
        return false;
    }
    if (!xllm_session__checksum_append(pRecord) || !xllm_session__buf_char(pRecord, '\n') ||
        pRecord->iLen > INT_MAX)
    {
        xllm_session__error(&pSession->tLastPersistenceError,
                            pRecord->iLen > INT_MAX ? XLLM_ERROR_LIMIT : XLLM_ERROR_OUT_OF_MEMORY,
                            "failed to serialize the session journal record");
        pSession->bHasLastPersistenceError = true;
        return false;
    }
    if (!xllm_session__storage_path_info(pSession->sJournalPath, &bJournalExisted, &iOriginalSize))
    {
        xllm_session__storage_error_remember(pSession, XLLM_ERROR_STAGE_READ, "journal-stat",
                                             pSession->sJournalPath,
                                             "failed to inspect the session journal");
        return false;
    }
    File = xllm_session__storage_open(pSession->sJournalPath,
                                      XFILE_WRITE | XFILE_CREATE | XFILE_APPEND);
    if (!File)
    {
        xllm_session__storage_error_remember(pSession, XLLM_ERROR_STAGE_WRITE, "journal-open",
                                             pSession->sJournalPath,
                                             "failed to open the session journal");
        return false;
    }
    bWritten = xllm_session__storage_write_full(File, pRecord->pData, pRecord->iLen);
    if (!bWritten)
    {
        xllm_session__storage_error_remember(pSession, XLLM_ERROR_STAGE_WRITE, "journal-append",
                                             pSession->sJournalPath,
                                             "failed to append the session journal record");
    }
    if (bWritten && pSession->tConfig.eJournalDurability == XLLM_SESSION_DURABILITY_FLUSH)
    {
        bFlushed = xllm_session__storage_flush(File);
        if (!bFlushed)
        {
            xllm_session__storage_error_remember(pSession, XLLM_ERROR_STAGE_WRITE, "journal-flush",
                                                 pSession->sJournalPath,
                                                 "failed to flush the session journal record");
        }
    }
    bClosed = xllm_session__storage_close(File);
    if (bWritten && bFlushed && !bClosed)
    {
        xllm_session__storage_error_remember(pSession, XLLM_ERROR_STAGE_WRITE, "journal-close",
                                             pSession->sJournalPath,
                                             "failed to close the session journal record");
    }
    if (!bWritten)
    {
        if (!xllm_session__journal_rollback(pSession, bJournalExisted, iOriginalSize) || !bClosed)
        {
            xllm_session__state_set(pSession, XLLM_SESSION_STATE_RECOVERY_REQUIRED);
        }
        return false;
    }
    if (!bFlushed || !bClosed)
    {
        /* A complete record may already be durable. Keep its sequence reserved
         * and stop this process from appending until Recover establishes the
         * authoritative prefix. */
        ++pSession->uJournalSequence;
        pSession->bStatsDirty = true;
        xllm_session__state_set(pSession, XLLM_SESSION_STATE_RECOVERY_REQUIRED);
        return false;
    }
    ++pSession->uJournalSequence;
    pSession->bStatsDirty = true;
    pSession->bHasLastPersistenceError = false;
    xllmErrorInit(&pSession->tLastPersistenceError);
    xllm_session__event(pSession, XLLM_SESSION_EVENT_JOURNAL_RECORD, 0u, pSession->uJournalSequence,
                        NULL);
    return true;
}

bool xllm_session__journal_append_turn(xllm_session *pSession, uint64_t uTurn)
{
    xllm_session_buf tRecord = {0};
    bool bOk;
    if (!pSession || !pSession->sJournalPath)
    {
        return pSession != NULL;
    }
    bOk = xllm_session__journal_prefix(&tRecord, pSession, "begin_turn") &&
          xllm_session__buf_cstr(&tRecord, ",\"turn\":") &&
          xllm_session__buf_u64(&tRecord, uTurn) && xllm_session__journal_write(pSession, &tRecord);
    xllm_session__buf_unit(&tRecord);
    return bOk;
}

bool xllm_session__journal_append_entry(xllm_session *pSession, const xllm_session_entry *pEntry)
{
    xllm_session_buf tRecord = {0};
    bool bOk;
    if (!pSession || !pEntry || !pSession->sJournalPath)
    {
        return pSession != NULL && pEntry != NULL;
    }
    bOk = xllm_session__journal_prefix(&tRecord, pSession, "add_message") &&
          xllm_session__buf_cstr(&tRecord, ",\"entry\":") &&
          xllm_session__write_entry(&tRecord, pEntry) &&
          xllm_session__journal_write(pSession, &tRecord);
    xllm_session__buf_unit(&tRecord);
    return bOk;
}

bool xllm_session__journal_append_usage(xllm_session *pSession, uint64_t uFill, uint64_t uEstimate,
                                        uint64_t uSequence, uint64_t uCached)
{
    xllm_session_buf tRecord = {0};
    bool bOk;
    if (!pSession || !pSession->sJournalPath || pSession->bRestoring)
        return pSession != NULL;
    bOk = xllm_session__journal_prefix(&tRecord, pSession, "usage") &&
          xllm_session__buf_cstr(&tRecord, ",\"context_usage\":{\"fill_tokens\":") &&
          xllm_session__buf_u64(&tRecord, uFill) &&
          xllm_session__buf_cstr(&tRecord, ",\"prefix_estimate\":") &&
          xllm_session__buf_u64(&tRecord, uEstimate) &&
          xllm_session__buf_cstr(&tRecord, ",\"through_sequence\":") &&
          xllm_session__buf_u64(&tRecord, uSequence) &&
          xllm_session__buf_cstr(&tRecord, ",\"cached_input_tokens\":") &&
          xllm_session__buf_u64(&tRecord, uCached) &&
          xllm_session__buf_cstr(&tRecord, ",\"profile_fingerprint\":") &&
          xllm_session__buf_u64(&tRecord, pSession->uProfileFingerprint) &&
          xllm_session__buf_char(&tRecord, '}') && xllm_session__journal_write(pSession, &tRecord);
    xllm_session__buf_unit(&tRecord);
    return bOk;
}

bool xllm_session__journal_append_compaction(xllm_session *pSession, uint64_t uThroughSequence,
                                             uint32_t uGeneration, uint64_t uPromptTokens,
                                             uint64_t uOutputTokens, const char *sSummary)
{
    xllm_session_buf tRecord = {0};
    bool bOk;
    if (!pSession || !sSummary || !pSession->sJournalPath)
    {
        return pSession != NULL && sSummary != NULL;
    }
    bOk = xllm_session__journal_prefix(&tRecord, pSession, "compact") &&
          xllm_session__buf_cstr(&tRecord, ",\"through_sequence\":") &&
          xllm_session__buf_u64(&tRecord, uThroughSequence) &&
          xllm_session__buf_cstr(&tRecord, ",\"generation\":") &&
          xllm_session__buf_u64(&tRecord, uGeneration) &&
          xllm_session__buf_cstr(&tRecord, ",\"usage\":{\"prompt_tokens\":") &&
          xllm_session__buf_u64(&tRecord, uPromptTokens) &&
          xllm_session__buf_cstr(&tRecord, ",\"output_tokens\":") &&
          xllm_session__buf_u64(&tRecord, uOutputTokens) && xllm_session__buf_cstr(&tRecord, "}") &&
          xllm_session__buf_cstr(&tRecord, ",\"compaction_count\":") &&
          xllm_session__buf_u64(&tRecord, pSession->uCompactionCount + 1u) &&
          xllm_session__buf_cstr(&tRecord, ",\"summary\":") &&
          xllm_session__json_string(&tRecord, sSummary) &&
          xllm_session__journal_write(pSession, &tRecord);
    xllm_session__buf_unit(&tRecord);
    return bOk;
}

bool xllm_session__journal_append_truncate(xllm_session *pSession, uint64_t uFrom, uint64_t uTo)
{
    xllm_session_buf tRecord = {0};
    bool bOk;
    if (!pSession || !pSession->sJournalPath)
    {
        return pSession != NULL;
    }
    bOk = xllm_session__journal_prefix(&tRecord, pSession, "truncate") &&
          xllm_session__buf_cstr(&tRecord, ",\"from_sequence\":") &&
          xllm_session__buf_u64(&tRecord, uFrom) &&
          xllm_session__buf_cstr(&tRecord, ",\"to_sequence\":") &&
          xllm_session__buf_u64(&tRecord, uTo) &&
          xllm_session__buf_cstr(&tRecord, ",\"reason\":\"overflow_l2\"") &&
          xllm_session__journal_write(pSession, &tRecord);
    xllm_session__buf_unit(&tRecord);
    return bOk;
}

bool xllm_session__journal_append_ledger(xllm_session *pSession, const char *sKind,
                                         const char *sPath)
{
    xllm_session_buf tRecord = {0};
    bool bOk;
    if (!pSession || !sKind || !sPath || !pSession->sJournalPath)
    {
        return pSession != NULL && sKind != NULL && sPath != NULL;
    }
    bOk = xllm_session__journal_prefix(&tRecord, pSession, "ledger") &&
          xllm_session__buf_cstr(&tRecord, ",\"kind\":") &&
          xllm_session__json_string(&tRecord, sKind) &&
          xllm_session__buf_cstr(&tRecord, ",\"path\":") &&
          xllm_session__json_string(&tRecord, sPath) &&
          xllm_session__buf_cstr(&tRecord, ",\"after_sequence\":") &&
          xllm_session__buf_u64(&tRecord, xllmSessionLastSequence(pSession)) &&
          xllm_session__journal_write(pSession, &tRecord);
    xllm_session__buf_unit(&tRecord);
    return bOk;
}

bool xllm_session__journal_append_rewrite(xllm_session *pSession, const char *sOperation,
                                          uint64_t uThroughSequence, uint64_t uPreviousNextSequence)
{
    xllm_session_buf tRecord = {0};
    bool bOk;
    if (!pSession || !sOperation ||
        (strcmp(sOperation, "rewind") != 0 && strcmp(sOperation, "clear") != 0))
    {
        return false;
    }
    if (!pSession->sJournalPath)
    {
        return true;
    }
    bOk = xllm_session__journal_prefix(&tRecord, pSession, sOperation) &&
          xllm_session__buf_cstr(&tRecord, ",\"through_sequence\":") &&
          xllm_session__buf_u64(&tRecord, uThroughSequence) &&
          xllm_session__buf_cstr(&tRecord, ",\"previous_next_sequence\":") &&
          xllm_session__buf_u64(&tRecord, uPreviousNextSequence) &&
          xllm_session__journal_write(pSession, &tRecord);
    xllm_session__buf_unit(&tRecord);
    return bOk;
}

static bool xllm_session__replay_message(xllm_session *pSession, xvalue *pRoot, uint64_t uVersion,
                                         const xllm_session_restore_options *pOptions,
                                         xllm_error *pError)
{
    xvalue *pEntry = xllm_session__json_get(pRoot, "entry");
    xllm_message tMessage;
    uint64_t uTurn;
    uint64_t uSequence;
    uint32_t uFlags;
    if (!pEntry || !xrtValueIs(pEntry, XVALUE_OBJECT) ||
        !xllm_session__json_uint_valid(pEntry, "turn", UINT64_MAX, true) ||
        !xllm_session__json_uint_valid(pEntry, "sequence", UINT64_MAX - 1u, true) ||
        !xllm_session__json_uint_valid(pEntry, "flags", UINT32_MAX, false))
        return false;
    uTurn = xllm_session__json_u64(pEntry, "turn", UINT64_MAX);
    uSequence = xllm_session__json_u64(pEntry, "sequence", 0u);
    uFlags = (uint32_t)xllm_session__json_u64(pEntry, "flags", 0u);
    if (uTurn > pSession->uCurrentTurn || uSequence == 0u || uSequence != pSession->uNextSequence ||
        !xllm_session__load_message(&tMessage, pEntry, uVersion, pOptions, pError))
    {
        return false;
    }
    if (!xllmSessionAddMessage(pSession, uTurn, &tMessage, uFlags))
    {
        xllmMessageUnit(&tMessage);
        return false;
    }
    xllmMessageUnit(&tMessage);
    return true;
}

static bool xllm_session__replay_compaction(xllm_session *pSession, xvalue *pRoot)
{
    uint64_t uThrough = xllm_session__json_u64(pRoot, "through_sequence", 0u);
    uint64_t uCount = xllm_session__json_u64(pRoot, "compaction_count", 0u);
    uint64_t uGeneration = xllm_session__json_u64(pRoot, "generation", 0u);
    uint64_t uPrompt = 0u;
    uint64_t uOutput = 0u;
    xvalue *pUsage = xllm_session__json_get(pRoot, "usage");
    const char *sSummary = xllm_session__json_text(pRoot, "summary");
    char *sCopy;
    uint64_t uExpectedGeneration;
    if (pUsage)
    {
        uPrompt = xllm_session__json_u64(pUsage, "prompt_tokens", 0u);
        uOutput = xllm_session__json_u64(pUsage, "output_tokens", 0u);
    }
    /* Quality gate without estimation (design D4): structure and byte cap. */
    if (pSession->uSummaryGeneration == UINT32_MAX || pSession->uCompactionCount == UINT64_MAX)
    {
        return false;
    }
    uExpectedGeneration = (uint64_t)pSession->uSummaryGeneration + 1u;
    if (!sSummary || !sSummary[0] || uThrough <= pSession->uCompactedThrough ||
        uThrough >= pSession->uNextSequence || uCount != pSession->uCompactionCount + 1u ||
        (uGeneration != 0u && uGeneration != uExpectedGeneration) ||
        !xllm_session__plan_pair_safe(pSession, uThrough) ||
        !xllm_session__summary_text_ok(pSession, sSummary))
    {
        return false;
    }
    sCopy = xllm_session__strdup(sSummary);
    if (!sCopy)
    {
        return false;
    }
    free(pSession->sSummary);
    pSession->sSummary = sCopy;
    pSession->uCompactedThrough = uThrough;
    pSession->bStatsDirty = true;
    ++pSession->uRenderGeneration;
    pSession->uCompactionCount = uCount;
    pSession->uSummaryGeneration = (uint32_t)uExpectedGeneration;
    pSession->uSummaryPromptAtBirth = uPrompt;
    pSession->uSummaryOutputAtBirth = uOutput;
    pSession->uLastUserSequence = pSession->uNextSequence;
    return true;
}

static bool xllm_session__replay_truncate(xllm_session *pSession, xvalue *pRoot)
{
    uint64_t uFrom = xllm_session__json_u64(pRoot, "from_sequence", 0u);
    uint64_t uTo = xllm_session__json_u64(pRoot, "to_sequence", 0u);
    if (pSession->uSummaryGeneration == UINT32_MAX || uFrom != pSession->uTailFloor || uTo <= uFrom || uTo >= pSession->uNextSequence ||
        uTo <= pSession->uCompactedThrough)
    {
        return false;
    }
    pSession->uTailFloor = uTo;
    pSession->bStatsDirty = true;
    ++pSession->uRenderGeneration;
    ++pSession->uSummaryGeneration;
    return true;
}

static bool xllm_session__replay_rewrite(xllm_session *pSession, xvalue *pRoot, bool bClear)
{
    uint64_t uThrough = xllm_session__json_u64(pRoot, "through_sequence", UINT64_MAX);
    uint64_t uPreviousNext = xllm_session__json_u64(pRoot, "previous_next_sequence", 0u);
    if (uPreviousNext != pSession->uNextSequence ||
        (bClear ? uThrough != 0u : !xllm_session__rewrite_boundary_valid(pSession, uThrough)))
    {
        return false;
    }
    xllm_session__truncate_apply(pSession, uThrough, bClear);
    return true;
}

static bool xllm_session__journal_fields_valid(xvalue* pRoot, const char* sOperation)
{
    if ( strcmp(sOperation, "begin_turn") == 0 ) {
        return xllm_session__json_uint_valid(pRoot, "turn", UINT64_MAX, true) &&
            xllm_session__json_u64(pRoot, "turn", 0u) != 0u;
    }
    if ( strcmp(sOperation, "add_message") == 0 ) {
        xvalue* pEntry = xllm_session__json_get(pRoot, "entry");
        return xllm_session__json_uint_valid(pEntry, "turn", UINT64_MAX, true) &&
            xllm_session__json_uint_valid(pEntry, "sequence", UINT64_MAX - 1u, true) &&
            xllm_session__json_u64(pEntry, "sequence", 0u) != 0u &&
            xllm_session__json_uint_valid(pEntry, "role", XLLM_ROLE_TOOL, true) &&
            xllm_session__json_uint_valid(pEntry, "flags", UINT32_MAX, false);
    }
    if ( strcmp(sOperation, "compact") == 0 ) {
        xvalue* pUsage = xrtValueObjectGet(pRoot, xrtStrView("usage"));
        const char* sSummary = xllm_session__json_text(pRoot, "summary");
        return xllm_session__json_uint_valid(pRoot, "through_sequence", UINT64_MAX - 1u, true) &&
            xllm_session__json_u64(pRoot, "through_sequence", 0u) != 0u &&
            xllm_session__json_uint_valid(pRoot, "compaction_count", UINT64_MAX, true) &&
            xllm_session__json_u64(pRoot, "compaction_count", 0u) != 0u &&
            xllm_session__json_uint_valid(pRoot, "generation", UINT32_MAX, false) &&
            (!xrtValueObjectGet(pRoot, xrtStrView("generation")) ||
                xllm_session__json_u64(pRoot, "generation", 0u) != 0u) &&
            sSummary && sSummary[0] && (!pUsage ||
                (xllm_session__json_uint_valid(pUsage, "prompt_tokens", UINT64_MAX, false) &&
                 xllm_session__json_uint_valid(pUsage, "output_tokens", UINT64_MAX, false)));
    }
    if ( strcmp(sOperation, "truncate") == 0 ) {
        return xllm_session__json_uint_valid(pRoot, "from_sequence", UINT64_MAX - 1u, true) &&
            xllm_session__json_uint_valid(pRoot, "to_sequence", UINT64_MAX - 1u, true) &&
            xllm_session__json_u64(pRoot, "to_sequence", 0u) >
                xllm_session__json_u64(pRoot, "from_sequence", 0u);
    }
    if ( strcmp(sOperation, "ledger") == 0 ) {
        const char* sKind = xllm_session__json_text(pRoot, "kind");
        const char* sPath = xllm_session__json_text(pRoot, "path");
        return sKind && (strcmp(sKind, "read") == 0 || strcmp(sKind, "modified") == 0) && sPath && sPath[0];
    }
    return strcmp(sOperation, "usage") == 0 || strcmp(sOperation, "rewind") == 0 || strcmp(sOperation, "clear") == 0;
}

static bool xllm_session__replay_record(xllm_session *pSession, xvalue *pRoot,
                                        const xllm_session_restore_options *pOptions,
                                        xllm_error *pError)
{
    uint64_t uSequence = 0u;
    uint64_t uVersion = 0u;
    const char *sFormat = NULL;
    const char *sOperation = NULL;
    bool bOk = false;
    if (!pRoot || !xrtValueIs(pRoot, XVALUE_OBJECT))
        goto invalid;
    sFormat = xllm_session__json_text(pRoot, "format");
    uVersion = xllm_session__json_u64(pRoot, "version", 0u);
    sOperation = xllm_session__json_text(pRoot, uVersion >= 3u ? "type" : "operation");
    uSequence = xllm_session__json_u64(pRoot, uVersion >= 3u ? "sequence" : "journal_sequence", 0u);
    if (!sFormat || strcmp(sFormat, "xllm-session-journal") != 0 ||
        (uVersion < 1u || uVersion > 4u) || !sOperation || uSequence == 0u)
    {
        goto invalid;
    }
    if (!xllm_session__json_uint_valid(pRoot, "version", 4u, true) ||
        !xllm_session__json_uint_valid(pRoot, uVersion >= 3u ? "sequence" : "journal_sequence", UINT64_MAX, true) ||
        !xllm_session__journal_fields_valid(pRoot, sOperation)) goto invalid;
    if (uSequence <= pSession->uJournalSequence)
    {
        return true; /* covered by the snapshot: deduplicated */
    }
    if (pSession->uJournalSequence == UINT64_MAX || uSequence != pSession->uJournalSequence + 1u)
    {
        goto invalid;
    }
    if (strcmp(sOperation, "begin_turn") == 0)
    {
        uint64_t uTurn = xllm_session__json_u64(pRoot, "turn", 0u);
        bOk = pSession->uCurrentTurn != UINT64_MAX && uTurn == pSession->uCurrentTurn + 1u && xllmSessionBeginTurn(pSession) == uTurn;
    }
    else if (strcmp(sOperation, "add_message") == 0)
    {
        bOk = xllm_session__replay_message(pSession, pRoot, uVersion, pOptions, pError);
    }
    else if (strcmp(sOperation, "compact") == 0)
    {
        bOk = xllm_session__replay_compaction(pSession, pRoot);
    }
    else if (strcmp(sOperation, "usage") == 0)
    {
        xvalue *pUsage = xllm_session__json_get(pRoot, "context_usage");
        bOk = pUsage && xllm_session__restore_usage(pSession, pUsage);
    }
    else if (strcmp(sOperation, "ledger") == 0)
    {
        const char *sKind = xllm_session__json_text(pRoot, "kind");
        const char *sPath = xllm_session__json_text(pRoot, "path");
        uint64_t uAfterSequence = xllm_session__json_u64(pRoot, "after_sequence", 0u);
        if (uAfterSequence >= pSession->uNextSequence)
        {
            goto invalid;
        }
        /* Replay applies through the internal note (no re-journaling). */
        bOk = sPath && sPath[0] &&
              ((sKind && strcmp(sKind, "read") == 0)
                   ? xllm_session__note_file(&pSession->psReadFiles, &pSession->puReadFileSequences,
                                             &pSession->iReadFileCount, &pSession->iReadFileCap,
                                             sPath, uAfterSequence, NULL)
                   : (sKind && strcmp(sKind, "modified") == 0 &&
                      xllm_session__note_file(
                          &pSession->psModifiedFiles, &pSession->puModifiedFileSequences,
                          &pSession->iModifiedFileCount, &pSession->iModifiedFileCap, sPath,
                          uAfterSequence, NULL)));
    }
    else if (strcmp(sOperation, "truncate") == 0)
    {
        bOk = xllm_session__replay_truncate(pSession, pRoot);
    }
    else if (strcmp(sOperation, "rewind") == 0)
    {
        bOk = xllm_session__replay_rewrite(pSession, pRoot, false);
    }
    else if (strcmp(sOperation, "clear") == 0)
    {
        bOk = xllm_session__replay_rewrite(pSession, pRoot, true);
    }
    if (!bOk)
    {
        goto invalid;
    }
    pSession->uJournalSequence = uSequence;
    pSession->bStatsDirty = true;
    return true;
invalid:
    {
        char sMessage[256];
        if (pError &&
            (pError->eCode == XLLM_ERROR_CANCELLED || pError->eCode == XLLM_ERROR_TIMEOUT ||
             pError->eCode == XLLM_ERROR_OUT_OF_MEMORY))
            return false;
        (void)snprintf(sMessage, sizeof(sMessage),
                       "invalid session journal record: sequence=%llu expected=%llu operation=%s "
                       "turn=%llu next_message=%llu",
                       (unsigned long long)uSequence,
                       (unsigned long long)(pSession->uJournalSequence == UINT64_MAX
                                                ? UINT64_MAX
                                                : pSession->uJournalSequence + 1u),
                       sOperation ? sOperation : "unknown",
                       (unsigned long long)pSession->uCurrentTurn,
                       (unsigned long long)pSession->uNextSequence);
        xllm_session__error(pError, XLLM_ERROR_PARSE, sMessage);
    }
    return false;
}

static bool xllm_session__replay_journal(xllm_session *pSession, const char *sJournalPath,
                                         xllm_error *pError)
{
    char *pData;
    bool bExists;
    uint64_t iPathSize;
    size_t iLen = 0u;
    size_t iComplete = 0u; /* bytes up to and including the last '\n' */
    size_t i;
    if (!xllm_session__storage_path_info(sJournalPath, &bExists, &iPathSize))
    {
        xllm_session__storage_error(pError, XLLM_ERROR_STAGE_READ, "journal-stat", sJournalPath,
                                    "failed to inspect the session journal");
        return false;
    }
    if (!bExists || iPathSize == 0u)
    {
        return true;
    }
    if (iPathSize > pSession->tConfig.uJournalMaxBytes)
    {
        xllm_session__error(pError, XLLM_ERROR_LIMIT, "session journal exceeds the replay budget");
        return false;
    }
    pData = (char *)xllm_session__storage_read_all(sJournalPath, &iLen,
                                                   XLLM_SESSION_TEST_STORAGE_JOURNAL_READ);
    if (!pData)
    {
        xllm_session__storage_error(pError, XLLM_ERROR_STAGE_READ, "journal-read", sJournalPath,
                                    "failed to read session journal");
        return false;
    }
    /* Write-ahead semantics: only fully newline-terminated records replay;
     * the torn tail is discarded below (xrtJsonlRead would otherwise accept
     * an unterminated final line as a record). */
    for (i = 0u; i < iLen; ++i)
    {
        if (pData[i] == '\n')
        {
            iComplete = i + 1u;
        }
    }
    if (iComplete > 0u)
    {
        /* JSONL is the only replay path (xrt >= 2639487c): line framing,
         * budgets, and record-indexed error locations come from the core
         * module. Blank lines are corruption here (machine-written journal). */
        xjsonlreadconfig tRead;
        xvalue *pRecords;
        xrtJsonlReadConfigInit(&tRead);
        tRead.Flags = XJSONL_READ_REJECT_EMPTY_LINES;
        tRead.MaxInputBytes = pSession->tConfig.uJournalMaxBytes;
        pRecords = xrtJsonlRead((xstrview){pData, iComplete}, &tRead);
        if (!pRecords)
        {
            xrtClearError();
            xllm_session__error(pError, XLLM_ERROR_PARSE,
                                "invalid session journal record (see the jsonl error location)");
            xrtFree(pData);
            return false;
        }
        size_t iLineStart = 0u;
        for (i = 0u; i < xrtValueCount(pRecords); ++i)
        {
            size_t iLineEnd = iLineStart;
            size_t iLineLen;
            xvalue *pRecord = xrtValueArrayGet(pRecords, i);
            uint64_t uVersion;
            while (iLineEnd < iComplete && pData[iLineEnd] != '\n')
            {
                ++iLineEnd;
            }
            iLineLen = iLineEnd - iLineStart;
            if (iLineLen && pData[iLineStart + iLineLen - 1u] == '\r')
            {
                --iLineLen;
            }
            uVersion = xllm_session__json_u64(pRecord, "version", 0u);
            if ((uVersion >= 3u && !xllm_session__checksum_verify(pData + iLineStart, iLineLen)) ||
                !xllm_session__replay_record(pSession, pRecord, NULL, pError))
            {
                if (uVersion >= 3u && !xllm_session__checksum_verify(pData + iLineStart, iLineLen))
                {
                    xllm_session__error(pError, XLLM_ERROR_PARSE,
                                        "session journal checksum mismatch");
                }
                xrtValueRelease(pRecords);
                xrtFree(pData);
                return false;
            }
            iLineStart = iLineEnd + 1u;
        }
        xrtValueRelease(pRecords);
    }
    if (iComplete < iLen && !xllm_session__storage_set_size(sJournalPath, iComplete))
    {
        xrtFree(pData);
        xllm_session__storage_error(pError, XLLM_ERROR_STAGE_WRITE, "journal-tail-truncate",
                                    sJournalPath,
                                    "failed to discard an incomplete session journal tail");
        return false;
    }
    xrtFree(pData);
    return true;
}

static bool xllm_session__set_journal_path(xllm_session *pSession, const char *sJournalPath,
                                           xllm_error *pError)
{
    char *sCopy;
    if (!pSession || !sJournalPath || !sJournalPath[0])
    {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT,
                            "session and journal path are required");
        return false;
    }
    sCopy = xllm_session__strdup(sJournalPath);
    if (!sCopy)
    {
        xllm_session__error(pError, XLLM_ERROR_OUT_OF_MEMORY,
                            "failed to store session journal path");
        return false;
    }
    free(pSession->sJournalPath);
    pSession->sJournalPath = sCopy;
    return true;
}

bool xllmSessionEnableJournal(xllm_session *pSession, const char *sJournalPath, xllm_error *pError)
{
    bool bExists = false;
    uint64_t iSize = 0u;
    if (pError)
    {
        xllmErrorInit(pError);
    }
    if (!pSession)
    {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT, "session is required");
        return false;
    }
    if (pSession->bInHook)
    {
        xllm_session__error(pError, XLLM_ERROR_HOOK,
                            "session hook re-entered journal configuration");
        return false;
    }
    if (pSession->eState == XLLM_SESSION_STATE_MODEL_IN_FLIGHT ||
        xllm_session__state_is_compaction(pSession->eState) ||
        pSession->eState == XLLM_SESSION_STATE_RECOVERY_REQUIRED ||
        pSession->eState == XLLM_SESSION_STATE_CORRUPT)
    {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT,
                            "journal configuration requires a stable session state");
        return false;
    }
    pSession->bStatsDirty = true;
    if (sJournalPath && sJournalPath[0] &&
        !xllm_session__storage_path_info(sJournalPath, &bExists, &iSize))
    {
        xllm_session__storage_error(pError, XLLM_ERROR_STAGE_READ, "journal-stat", sJournalPath,
                                    "failed to inspect the session journal");
        return false;
    }
    if (bExists && iSize != 0u)
    {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT,
                            "journal is not empty; recover it instead of attaching it directly");
        return false;
    }
    return xllm_session__set_journal_path(pSession, sJournalPath, pError);
}

bool xllmSessionFlushJournal(xllm_session *pSession, xllm_error *pError)
{
    xfile File;
    bool bFlushed;
    bool bClosed;
    if (pError)
    {
        xllmErrorInit(pError);
    }
    if (!pSession || !pSession->sJournalPath)
    {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT,
                            "an enabled session journal is required");
        return false;
    }
    if (pSession->eState == XLLM_SESSION_STATE_RECOVERY_REQUIRED ||
        pSession->eState == XLLM_SESSION_STATE_CORRUPT)
    {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT,
                            "journal recovery is required before a durability barrier");
        return false;
    }
    pSession->bHasLastPersistenceError = false;
    xllmErrorInit(&pSession->tLastPersistenceError);
    File = xllm_session__storage_open(pSession->sJournalPath, XFILE_WRITE);
    if (!File)
    {
        xllm_session__storage_error_remember(pSession, XLLM_ERROR_STAGE_WRITE, "journal-open",
                                             pSession->sJournalPath,
                                             "failed to open the session journal for flush");
        xllm_session__storage_error_copy(pSession, pError, XLLM_ERROR_NETWORK,
                                         "failed to open the session journal for flush");
        return false;
    }
    bFlushed = xllm_session__storage_flush(File);
    if (!bFlushed)
    {
        xllm_session__storage_error_remember(
            pSession, XLLM_ERROR_STAGE_WRITE, "journal-flush", pSession->sJournalPath,
            "failed to flush the session journal to stable storage");
    }
    bClosed = xllm_session__storage_close(File);
    if (bFlushed && !bClosed)
    {
        xllm_session__storage_error_remember(pSession, XLLM_ERROR_STAGE_WRITE, "journal-close",
                                             pSession->sJournalPath,
                                             "failed to close the flushed session journal");
    }
    if (!bFlushed || !bClosed)
    {
        xllm_session__state_set(pSession, XLLM_SESSION_STATE_RECOVERY_REQUIRED);
        xllm_session__storage_error_copy(pSession, pError, XLLM_ERROR_NETWORK,
                                         "failed to flush the session journal to stable storage");
        return false;
    }
    pSession->bHasLastPersistenceError = false;
    xllmErrorInit(&pSession->tLastPersistenceError);
    return true;
}

void xllmSessionDisableJournal(xllm_session *pSession)
{
    if (!pSession)
    {
        return;
    }
    pSession->bStatsDirty = true;
    free(pSession->sJournalPath);
    pSession->sJournalPath = NULL;
}

const char *xllmSessionJournalPath(const xllm_session *pSession)
{
    return pSession ? pSession->sJournalPath : NULL;
}

bool xllmSessionCheckpoint(xllm_session *pSession, const char *sSnapshotPath, xllm_error *pError)
{
    bool bJournalExists = false;
    uint64_t iJournalSize = 0u;
    if (pError)
    {
        xllmErrorInit(pError);
    }
    if (!pSession || !sSnapshotPath || !sSnapshotPath[0])
    {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT,
                            "session and snapshot path are required");
        return false;
    }
    if (!xllmSessionSave(pSession, sSnapshotPath, pError))
    {
        return false;
    }
    if (pSession->sJournalPath &&
        !xllm_session__storage_path_info(pSession->sJournalPath, &bJournalExists, &iJournalSize))
    {
        xllm_session__storage_error(
            pError, XLLM_ERROR_STAGE_READ, "journal-stat", pSession->sJournalPath,
            "session checkpoint is durable but the covered journal could not be inspected");
        return false;
    }
    (void)iJournalSize;
    if (bJournalExists && !xllm_session__storage_delete(pSession->sJournalPath))
    {
        xllm_session__storage_error(
            pError, XLLM_ERROR_STAGE_WRITE, "journal-delete", pSession->sJournalPath,
            "session checkpoint is durable but covered journal records could not be removed");
        return false;
    }
    xllm_session__event(pSession, XLLM_SESSION_EVENT_CHECKPOINT_SAVED, 0u, 0u, NULL);
    return true;
}

xllm_session *xllmSessionRecover(const char *sSnapshotPath, const char *sJournalPath,
                                 const xllm_session_config *pConfigIfNew, xllm_error *pError)
{
    xllm_session *pSession;
    bool bSnapshotExists;
    uint64_t iSnapshotSize;
    if (pError)
    {
        xllmErrorInit(pError);
    }
    if (!sSnapshotPath || !sSnapshotPath[0] || !sJournalPath || !sJournalPath[0])
    {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT,
                            "snapshot and journal paths are required for recovery");
        return NULL;
    }
    if (!xllm_session__storage_path_info(sSnapshotPath, &bSnapshotExists, &iSnapshotSize))
    {
        xllm_session__storage_error(pError, XLLM_ERROR_STAGE_READ, "snapshot-stat", sSnapshotPath,
                                    "failed to inspect the session snapshot");
        return NULL;
    }
    (void)iSnapshotSize;
    if (bSnapshotExists)
    {
        pSession = xllmSessionLoad(sSnapshotPath, pError);
    }
    else
    {
        pSession = xllmSessionCreate(pConfigIfNew, pError);
    }
    if (!pSession)
    {
        return NULL;
    }
    pSession->bRestoring = true;
    if (!xllm_session__replay_journal(pSession, sJournalPath, pError) ||
        !xllm_session__set_journal_path(pSession, sJournalPath, pError))
    {
        pSession->bRestoring = false;
        xllmSessionDestroy(pSession);
        return NULL;
    }
    pSession->bRestoring = false;
    /* Snapshot/journal usage anchors remain valid across process restarts. */
    xllm_session__event(pSession, XLLM_SESSION_EVENT_SESSION_RECOVERED, 0u, 0u, NULL);
    return pSession;
}

/* Byte restoration has stricter framing than crash recovery: every supplied
 * byte must belong to a complete record. State mutations are exactly the same
 * replay_record implementation used by file Recover. Nothing is re-journaled. */
#if defined(XLLM_SESSION_TEST_FAULTS)
static xcancel *xllm_session__test_restore_cancel;
static size_t xllm_session__test_restore_cancel_after;
#endif

static bool xllm_session__restore_journal(xllm_session *pSession, const char *pData, size_t iBytes,
                                          const xllm_session_restore_options *pOptions,
                                          xllm_error *pError)
{
    size_t iOffset = 0u, iRecords = 0u;
    if (iBytes > pSession->tConfig.uJournalMaxBytes)
    {
        xllm_session__error(pError, XLLM_ERROR_LIMIT,
                            "journal exceeds restored session replay budget");
        return false;
    }
    while (iOffset < iBytes)
    {
        const char *pEnd;
        size_t iLineBytes, iFramed;
        uint64_t uVersion;
        xjsonlreadconfig tRead;
        xvalue *pRecords, *pRecord;
        bool bOk;
        if (!xllm_session__restore_check(pOptions, pError))
            return false;
        if (iRecords == pOptions->iMaxRecords)
        {
            xllm_session__error(pError, XLLM_ERROR_LIMIT, "journal record budget exceeded");
            return false;
        }
        pEnd = (const char *)memchr(pData + iOffset, '\n', iBytes - iOffset);
        if (!pEnd)
        {
            xllm_session__error(pError, XLLM_ERROR_PARSE, "incomplete session journal tail");
            return false;
        }
        iFramed = (size_t)(pEnd - (pData + iOffset));
        iLineBytes = iFramed;
        if (iLineBytes && pData[iOffset + iLineBytes - 1u] == '\r')
            --iLineBytes;
        xrtJsonlReadConfigInit(&tRead);
        tRead.Flags = XJSONL_READ_REJECT_EMPTY_LINES;
        tRead.MaxInputBytes = pOptions->iMaxJournalBytes;
        tRead.MaxRecords = 1u;
        tRead.MaxTotalValues = pOptions->iMaxValues;
        tRead.Record.MaxInputBytes = pOptions->iMaxJournalBytes;
        tRead.Record.MaxValues = pOptions->iMaxValues;
        tRead.Record.MaxDepth = pOptions->uMaxDepth;
        pRecords = xrtJsonlRead((xstrview){pData + iOffset, iFramed + 1u}, &tRead);
        if (!pRecords)
        {
            const xerror *pRuntime = xrtGetError();
            bool bMemory = pRuntime && xrtErrorKind(pRuntime) == XERR_MEMORY;
            bool bLimit = pRuntime && (xrtErrorCode(pRuntime) == XJSON_ERROR_LIMIT ||
                                       xrtErrorCode(pRuntime) == XJSONL_ERROR_LIMIT);
            xllm_session__error(pError,
                                bMemory  ? XLLM_ERROR_OUT_OF_MEMORY
                                : bLimit ? XLLM_ERROR_LIMIT
                                         : XLLM_ERROR_PARSE,
                                "cannot parse session journal record");
            return false;
        }
        pRecord = xrtValueArrayGet(pRecords, 0u);
        uVersion = xllm_session__json_u64(pRecord, "version", 0u);
        bOk = xllm_session__restore_check(pOptions, pError) &&
              xllm_session__restore_strings(pRecord, pOptions, pError) &&
              (uVersion < 3u ||
               xllm_session__checksum_restore(pData + iOffset, iLineBytes, pOptions, pError));
        if (bOk)
        {
            xrtClearError();
            bOk = xllm_session__replay_record(pSession, pRecord, pOptions, pError);
            if (!bOk && xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_MEMORY)
                xllm_session__error(pError, XLLM_ERROR_OUT_OF_MEMORY,
                                    "out of memory replaying session journal");
        }
        xrtValueRelease(pRecords);
        if (!bOk)
            return false;
        iOffset += iFramed + 1u;
        ++iRecords;
#if defined(XLLM_SESSION_TEST_FAULTS)
        if (xllm_session__test_restore_cancel &&
            iRecords == xllm_session__test_restore_cancel_after)
            (void)xrtCancelRequest(xllm_session__test_restore_cancel);
#endif
    }
    return xllm_session__restore_check(pOptions, pError);
}

xllm_session *xllmSessionRestore(const void *pSnapshot, size_t iSnapshotBytes, const void *pJournal,
                                 size_t iJournalBytes, const xllm_session_restore_options *pInput,
                                 xllm_error *pError)
{
    xllm_session_restore_options tOptions;
    xllm_session_config tConfig;
    xllm_session *pSession;
    if (pError)
        xllmErrorInit(pError);
    xrtClearError();
    xllmSessionRestoreOptionsInit(&tOptions);
    if (pInput)
    {
        if (pInput->uSize != sizeof(tOptions) ||
            pInput->uAbiVersion != XLLM_SESSION_RESTORE_ABI_VERSION)
            goto invalid;
        tOptions = *pInput;
    }
    if (tOptions.uReserved0 || !tOptions.iMaxSnapshotBytes || !tOptions.iMaxJournalBytes ||
        !tOptions.iMaxValues || !tOptions.iMaxRecords || !tOptions.uMaxDepth ||
        tOptions.uMaxDepth > XJSON_DEPTH_DEFAULT || (iSnapshotBytes && !pSnapshot) ||
        (iJournalBytes && !pJournal))
        goto invalid;
    if (!xllm_session__restore_check(&tOptions, pError))
        return NULL;
    if (iSnapshotBytes > tOptions.iMaxSnapshotBytes || iJournalBytes > tOptions.iMaxJournalBytes)
    {
        xllm_session__error(pError, XLLM_ERROR_LIMIT, "session restoration byte budget exceeded");
        return NULL;
    }
    if (iSnapshotBytes)
        pSession =
            xllm_session__load_snapshot((const char *)pSnapshot, iSnapshotBytes, &tOptions, pError);
    else
    {
        xllmSessionConfigInit(&tConfig);
        if (tOptions.pConfigIfNew)
            tConfig = *tOptions.pConfigIfNew;
        tConfig.sSnapshotPath = NULL;
        pSession = xllmSessionCreate(&tConfig, pError);
    }
    if (!pSession)
        return NULL;
    pSession->bRestoring = true;
    if (!xllm_session__restore_journal(pSession, (const char *)pJournal, iJournalBytes, &tOptions,
                                       pError))
    {
        pSession->bRestoring = false;
        xllmSessionDestroy(pSession);
        return NULL;
    }
    pSession->bRestoring = false;
    return pSession;
invalid:
    xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT,
                        "invalid session restoration options or buffers");
    return NULL;
}
