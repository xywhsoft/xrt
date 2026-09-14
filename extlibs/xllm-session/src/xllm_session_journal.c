#include "xllm_session_internal.h"

static uint64_t xllm_session__path_size(const char* sPath)
{
    xfileinfo tInfo;
    return sPath && xrtPathStat(sPath, true, &tInfo) ? tInfo.Size : 0u;
}

static bool xllm_session__journal_prefix(xllm_session_buf* pRecord,
    const xllm_session* pSession, const char* sOperation)
{
    if ( pSession->uJournalSequence == UINT64_MAX ) { return false; }
    return xllm_session__buf_cstr(pRecord,
            "{\"format\":\"xllm-session-journal\",\"version\":2,\"journal_sequence\":") &&
        xllm_session__buf_u64(pRecord, pSession->uJournalSequence + 1u) &&
        xllm_session__buf_cstr(pRecord, ",\"operation\":") &&
        xllm_session__json_string(pRecord, sOperation);
}

static bool xllm_session__journal_write(xllm_session* pSession, xllm_session_buf* pRecord)
{
    bool bJournalExisted;
    uint64_t iOriginalSize;
    if ( !pSession->sJournalPath ) { return true; }
    if ( pSession->uJournalSequence == UINT64_MAX ||
         !xllm_session__buf_cstr(pRecord, "}\n") || pRecord->iLen > INT_MAX ) {
        return false;
    }
    bJournalExisted = xrtFileExists(pSession->sJournalPath);
    iOriginalSize = bJournalExisted ? xllm_session__path_size(pSession->sJournalPath) : 0u;
    if ( !xrtFileAppend(pSession->sJournalPath,
            (xbytesview){ (const uint8*)pRecord->pData, pRecord->iLen }) ) {
        if ( xrtFileExists(pSession->sJournalPath) ) {
            if ( iOriginalSize ) (void)xrtFileSetSize(pSession->sJournalPath, iOriginalSize);
            else (void)xrtFileDelete(pSession->sJournalPath);
        }
        return false;
    }
    ++pSession->uJournalSequence;
    xllm_session__event(pSession, XLLM_SESSION_EVENT_JOURNAL_RECORD, 0u,
        pSession->uJournalSequence, NULL);
    return true;
}

bool xllm_session__journal_append_turn(xllm_session* pSession, uint64_t uTurn)
{
    xllm_session_buf tRecord = {0};
    bool bOk;
    if ( !pSession || !pSession->sJournalPath ) { return pSession != NULL; }
    bOk = xllm_session__journal_prefix(&tRecord, pSession, "begin_turn") &&
        xllm_session__buf_cstr(&tRecord, ",\"turn\":") &&
        xllm_session__buf_u64(&tRecord, uTurn) &&
        xllm_session__journal_write(pSession, &tRecord);
    xllm_session__buf_unit(&tRecord);
    return bOk;
}

bool xllm_session__journal_append_entry(xllm_session* pSession, const xllm_session_entry* pEntry)
{
    xllm_session_buf tRecord = {0};
    bool bOk;
    if ( !pSession || !pEntry || !pSession->sJournalPath ) { return pSession != NULL && pEntry != NULL; }
    bOk = xllm_session__journal_prefix(&tRecord, pSession, "add_message") &&
        xllm_session__buf_cstr(&tRecord, ",\"entry\":") &&
        xllm_session__write_entry(&tRecord, pEntry) &&
        xllm_session__journal_write(pSession, &tRecord);
    xllm_session__buf_unit(&tRecord);
    return bOk;
}

bool xllm_session__journal_append_compaction(xllm_session* pSession, uint64_t uThroughSequence,
    uint32_t uGeneration, uint64_t uPromptTokens, uint64_t uOutputTokens, const char* sSummary)
{
    xllm_session_buf tRecord = {0};
    bool bOk;
    if ( !pSession || !sSummary || !pSession->sJournalPath ) { return pSession != NULL && sSummary != NULL; }
    bOk = xllm_session__journal_prefix(&tRecord, pSession, "compact") &&
        xllm_session__buf_cstr(&tRecord, ",\"through_sequence\":") &&
        xllm_session__buf_u64(&tRecord, uThroughSequence) &&
        xllm_session__buf_cstr(&tRecord, ",\"generation\":") &&
        xllm_session__buf_u64(&tRecord, uGeneration) &&
        xllm_session__buf_cstr(&tRecord, ",\"usage\":{\"prompt_tokens\":") &&
        xllm_session__buf_u64(&tRecord, uPromptTokens) &&
        xllm_session__buf_cstr(&tRecord, ",\"output_tokens\":") &&
        xllm_session__buf_u64(&tRecord, uOutputTokens) &&
        xllm_session__buf_cstr(&tRecord, "}") &&
        xllm_session__buf_cstr(&tRecord, ",\"compaction_count\":") &&
        xllm_session__buf_u64(&tRecord, pSession->uCompactionCount + 1u) &&
        xllm_session__buf_cstr(&tRecord, ",\"summary\":") &&
        xllm_session__json_string(&tRecord, sSummary) &&
        xllm_session__journal_write(pSession, &tRecord);
    xllm_session__buf_unit(&tRecord);
    return bOk;
}

bool xllm_session__journal_append_truncate(xllm_session* pSession, uint64_t uFrom, uint64_t uTo)
{
    xllm_session_buf tRecord = {0};
    bool bOk;
    if ( !pSession || !pSession->sJournalPath ) { return pSession != NULL; }
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

static bool xllm_session__replay_message(xllm_session* pSession, xvalue* pRoot)
{
    xvalue* pEntry = xllm_session__json_get(pRoot, "entry");
    xllm_message tMessage;
    uint64_t uTurn;
    uint64_t uSequence;
    uint32_t uFlags;
    if ( !pEntry || !xrtValueIs(pEntry, XVALUE_OBJECT) ) return false;
    uTurn = xllm_session__json_u64(pEntry, "turn", UINT64_MAX);
    uSequence = xllm_session__json_u64(pEntry, "sequence", 0u);
    uFlags = (uint32_t)xllm_session__json_u64(pEntry, "flags", 0u);
    if ( uTurn > pSession->uCurrentTurn || uSequence == 0u ||
         uSequence != pSession->uNextSequence ||
         !xllm_session__load_message(&tMessage, pEntry) ) {
        return false;
    }
    if ( !xllmSessionAddMessage(pSession, uTurn, &tMessage, uFlags) ) {
        xllmMessageUnit(&tMessage);
        return false;
    }
    xllmMessageUnit(&tMessage);
    return true;
}

static bool xllm_session__replay_compaction(xllm_session* pSession, xvalue* pRoot)
{
    uint64_t uThrough = xllm_session__json_u64(pRoot, "through_sequence", 0u);
    uint64_t uCount = xllm_session__json_u64(pRoot, "compaction_count", 0u);
    uint64_t uGeneration = xllm_session__json_u64(pRoot, "generation", 0u);
    uint64_t uPrompt = 0u;
    uint64_t uOutput = 0u;
    xvalue* pUsage = xllm_session__json_get(pRoot, "usage");
    const char* sSummary = xllm_session__json_text(pRoot, "summary");
    char* sCopy;
    if ( pUsage ) {
        uPrompt = xllm_session__json_u64(pUsage, "prompt_tokens", 0u);
        uOutput = xllm_session__json_u64(pUsage, "output_tokens", 0u);
    }
    /* Quality gate without estimation (design D4): structure and byte cap. */
    if ( !sSummary || !sSummary[0] || uThrough <= pSession->uCompactedThrough ||
         uThrough >= pSession->uNextSequence || uCount != pSession->uCompactionCount + 1u ||
         !xllm_session__summary_text_ok(pSession, sSummary) ) {
        return false;
    }
    sCopy = xllm_session__strdup(sSummary);
    if ( !sCopy ) { return false; }
    free(pSession->sSummary);
    pSession->sSummary = sCopy;
    pSession->uCompactedThrough = uThrough;
    pSession->uCompactionCount = uCount;
    if ( uGeneration > pSession->uSummaryGeneration ) {
        pSession->uSummaryGeneration = (uint32_t)uGeneration;
    } else {
        pSession->uSummaryGeneration = pSession->uCompactionCount;
    }
    pSession->uSummaryPromptAtBirth = uPrompt;
    pSession->uSummaryOutputAtBirth = uOutput;
    return true;
}

static bool xllm_session__replay_truncate(xllm_session* pSession, xvalue* pRoot)
{
    uint64_t uFrom = xllm_session__json_u64(pRoot, "from_sequence", 0u);
    uint64_t uTo = xllm_session__json_u64(pRoot, "to_sequence", 0u);
    if ( uFrom == 0u || uTo <= uFrom || uTo >= pSession->uNextSequence ||
         uTo <= pSession->uTailFloor ) {
        return false;
    }
    pSession->uTailFloor = uTo;
    ++pSession->uSummaryGeneration;
    return true;
}

static bool xllm_session__replay_record(xllm_session* pSession, xvalue* pRoot, xllm_error* pError)
{
    uint64_t uSequence = 0u;
    const char* sFormat = NULL;
    const char* sOperation = NULL;
    bool bOk = false;
    if ( !pRoot || !xrtValueIs(pRoot, XVALUE_OBJECT) ) goto invalid;
    sFormat = xllm_session__json_text(pRoot, "format");
    sOperation = xllm_session__json_text(pRoot, "operation");
    uSequence = xllm_session__json_u64(pRoot, "journal_sequence", 0u);
    if ( !sFormat || strcmp(sFormat, "xllm-session-journal") != 0 ||
         (xllm_session__json_u64(pRoot, "version", 0u) != 1u &&
           xllm_session__json_u64(pRoot, "version", 0u) != 2u) ||
         !sOperation || uSequence == 0u ) {
        goto invalid;
    }
    if ( uSequence <= pSession->uJournalSequence ) {
        return true; /* covered by the snapshot: deduplicated */
    }
    if ( pSession->uJournalSequence == UINT64_MAX || uSequence != pSession->uJournalSequence + 1u ) {
        goto invalid;
    }
    if ( strcmp(sOperation, "begin_turn") == 0 ) {
        uint64_t uTurn = xllm_session__json_u64(pRoot, "turn", 0u);
        bOk = uTurn == pSession->uCurrentTurn + 1u && xllmSessionBeginTurn(pSession) == uTurn;
    } else if ( strcmp(sOperation, "add_message") == 0 ) {
        bOk = xllm_session__replay_message(pSession, pRoot);
    } else if ( strcmp(sOperation, "compact") == 0 ) {
        bOk = xllm_session__replay_compaction(pSession, pRoot);
    } else if ( strcmp(sOperation, "truncate") == 0 ) {
        bOk = xllm_session__replay_truncate(pSession, pRoot);
    }
    if ( !bOk ) { goto invalid; }
    pSession->uJournalSequence = uSequence;
    return true;
invalid:
    {
        char sMessage[256];
        (void)snprintf(sMessage, sizeof(sMessage),
            "invalid session journal record: sequence=%llu expected=%llu operation=%s turn=%llu next_message=%llu",
            (unsigned long long)uSequence,
            (unsigned long long)(pSession->uJournalSequence == UINT64_MAX ? UINT64_MAX : pSession->uJournalSequence + 1u),
            sOperation ? sOperation : "unknown",
            (unsigned long long)pSession->uCurrentTurn,
            (unsigned long long)pSession->uNextSequence);
        xllm_session__error(pError, XLLM_ERROR_PARSE, sMessage);
    }
    return false;
}

static bool xllm_session__replay_journal(xllm_session* pSession, const char* sJournalPath,
    xllm_error* pError)
{
    char* pData;
    size_t iLen = 0u;
    size_t iComplete = 0u; /* bytes up to and including the last '\n' */
    size_t i;
    if ( !xrtFileExists(sJournalPath) || xllm_session__path_size(sJournalPath) == 0u ) return true;
    if ( xllm_session__path_size(sJournalPath) > pSession->tConfig.uJournalMaxBytes ) {
        xllm_session__error(pError, XLLM_ERROR_LIMIT, "session journal exceeds the replay budget");
        return false;
    }
    pData = (char*)xrtFileReadAll(sJournalPath, &iLen);
    if ( !pData ) {
        xllm_session__error(pError, XLLM_ERROR_NETWORK, "failed to read session journal");
        return false;
    }
    /* Write-ahead semantics: only fully newline-terminated records replay;
     * the torn tail is discarded below (xrtJsonlRead would otherwise accept
     * an unterminated final line as a record). */
    for ( i = 0u; i < iLen; ++i ) {
        if ( pData[i] == '\n' ) { iComplete = i + 1u; }
    }
    if ( iComplete > 0u ) {
        /* JSONL is the only replay path (xrt >= 2639487c): line framing,
         * budgets, and record-indexed error locations come from the core
         * module. Blank lines are corruption here (machine-written journal). */
        xjsonlreadconfig tRead;
        xvalue* pRecords;
        xrtJsonlReadConfigInit(&tRead);
        tRead.Flags = XJSONL_READ_REJECT_EMPTY_LINES;
        tRead.MaxInputBytes = pSession->tConfig.uJournalMaxBytes;
        pRecords = xrtJsonlRead((xstrview){ pData, iComplete }, &tRead);
        if ( !pRecords ) {
            xrtClearError();
            xllm_session__error(pError, XLLM_ERROR_PARSE,
                "invalid session journal record (see the jsonl error location)");
            xrtFree(pData);
            return false;
        }
        for ( i = 0u; i < xrtValueCount(pRecords); ++i ) {
            if ( !xllm_session__replay_record(pSession, xrtValueArrayGet(pRecords, i), pError) ) {
                xrtValueRelease(pRecords);
                xrtFree(pData);
                return false;
            }
        }
        xrtValueRelease(pRecords);
    }
    if ( iComplete < iLen && !xrtFileSetSize((str)sJournalPath, iComplete) ) {
        xrtFree(pData);
        xllm_session__error(pError, XLLM_ERROR_NETWORK,
            "failed to discard an incomplete session journal tail");
        return false;
    }
    xrtFree(pData);
    return true;
}

static bool xllm_session__set_journal_path(xllm_session* pSession, const char* sJournalPath,
    xllm_error* pError)
{
    char* sCopy;
    if ( !pSession || !sJournalPath || !sJournalPath[0] ) {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT, "session and journal path are required");
        return false;
    }
    sCopy = xllm_session__strdup(sJournalPath);
    if ( !sCopy ) {
        xllm_session__error(pError, XLLM_ERROR_OUT_OF_MEMORY, "failed to store session journal path");
        return false;
    }
    free(pSession->sJournalPath);
    pSession->sJournalPath = sCopy;
    return true;
}

bool xllmSessionEnableJournal(xllm_session* pSession, const char* sJournalPath, xllm_error* pError)
{
    if ( pError ) { xllmErrorInit(pError); }
    if ( sJournalPath && sJournalPath[0] && xrtFileExists(sJournalPath) &&
         xllm_session__path_size(sJournalPath) != 0u ) {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT,
            "journal is not empty; recover it instead of attaching it directly");
        return false;
    }
    return xllm_session__set_journal_path(pSession, sJournalPath, pError);
}

void xllmSessionDisableJournal(xllm_session* pSession)
{
    if ( !pSession ) { return; }
    free(pSession->sJournalPath);
    pSession->sJournalPath = NULL;
}

const char* xllmSessionJournalPath(const xllm_session* pSession)
{
    return pSession ? pSession->sJournalPath : NULL;
}

bool xllmSessionCheckpoint(xllm_session* pSession, const char* sSnapshotPath, xllm_error* pError)
{
    if ( pError ) { xllmErrorInit(pError); }
    if ( !pSession || !sSnapshotPath || !sSnapshotPath[0] ) {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT, "session and snapshot path are required");
        return false;
    }
    if ( !xllmSessionSave(pSession, sSnapshotPath, pError) ) { return false; }
    if ( pSession->sJournalPath && xrtFileExists((str)pSession->sJournalPath) &&
         !xrtFileDelete((str)pSession->sJournalPath) ) {
        xllm_session__error(pError, XLLM_ERROR_NETWORK,
            "session checkpoint is durable but covered journal records could not be removed");
        return false;
    }
    xllm_session__event(pSession, XLLM_SESSION_EVENT_CHECKPOINT_SAVED, 0u, 0u, NULL);
    return true;
}

xllm_session* xllmSessionRecover(const char* sSnapshotPath, const char* sJournalPath,
    const xllm_session_config* pConfigIfNew, xllm_error* pError)
{
    xllm_session* pSession;
    if ( pError ) { xllmErrorInit(pError); }
    if ( !sSnapshotPath || !sSnapshotPath[0] || !sJournalPath || !sJournalPath[0] ) {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT,
            "snapshot and journal paths are required for recovery");
        return NULL;
    }
    if ( xrtFileExists((str)sSnapshotPath) ) {
        pSession = xllmSessionLoad(sSnapshotPath, pError);
    } else {
        pSession = xllmSessionCreate(pConfigIfNew, pError);
    }
    if ( !pSession ) { return NULL; }
    if ( !xllm_session__replay_journal(pSession, sJournalPath, pError) ||
         !xllm_session__set_journal_path(pSession, sJournalPath, pError) ) {
        xllmSessionDestroy(pSession);
        return NULL;
    }
    /* Restored governance is unknown until the next real call (§4.2). */
    pSession->bFillExactValid = false;
    xllm_session__event(pSession, XLLM_SESSION_EVENT_SESSION_RECOVERED, 0u, 0u, NULL);
    return pSession;
}
