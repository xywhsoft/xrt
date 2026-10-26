#ifndef XLLM_SESSION_TEST_PERSISTENCE_NUMERIC_CASES_H
#define XLLM_SESSION_TEST_PERSISTENCE_NUMERIC_CASES_H

static const char sNumericSnapshot[] = "out/xllm-session-tests/numeric.json";
static const char sNumericJournal[] = "out/xllm-session-tests/numeric.jsonl";

static xllm_session* test_numeric_load(const char* sFields, xllm_error* pError)
{
    char sJson[8192];
    int iSize = snprintf(sJson, sizeof(sJson),
        "{\"format\":\"xllm-session\",\"version\":3,%s}", sFields);
    xllmErrorInit(pError);
    if ( iSize < 0 || (size_t)iSize >= sizeof(sJson) ||
         !xrtFileWriteAll(sNumericSnapshot, (xbytesview){ (const uint8*)sJson, (size_t)iSize }) ) {
        SESSION_CHECK(false, "numeric snapshot fixture is written");
        return NULL;
    }
    return xllmSessionLoad(sNumericSnapshot, pError);
}

static bool test_numeric_journal(const char* sSequence, const char* sOperation, const char* sFields)
{
    char sJson[8192];
    int iSize = snprintf(sJson, sizeof(sJson),
        "{\"format\":\"xllm-session-journal\",\"version\":3,\"journal_sequence\":%s,"
        "\"operation\":\"%s\",%s}\n", sSequence, sOperation, sFields);
    bool bOk = iSize > 0 && (size_t)iSize < sizeof(sJson) &&
        xrtFileWriteAll(sNumericJournal, (xbytesview){ (const uint8*)sJson, (size_t)iSize });
    SESSION_CHECK(bOk, "numeric journal fixture is written");
    return bOk;
}

static void test_persistence_numeric_types(void)
{
    static const char* aNarrow[] = {
        "max_output_tokens", "output_reserve_tokens", "safety_reserve_tokens",
        "recent_turns_to_keep", "tool_prune_bytes", "summary_max_tokens", "summary_min_tokens",
        "compaction_required_sections", "keep_recent_tokens", "summary_max_bytes",
        "user_message_cap_bytes", "tool_result_cap_bytes", "tool_result_total_cap_bytes"
    };
    static const char* aInvalid[] = {
        "\"config\":[],\"entries\":[]",
        "\"config\":{\"tool_result_cap_bytes\":\"7\"},\"entries\":[]",
        "\"config\":{\"tool_result_cap_bytes\":-1},\"entries\":[]",
        "\"config\":{\"tool_result_cap_bytes\":null},\"entries\":[]",
        "\"config\":{\"tool_result_cap_bytes\":true},\"entries\":[]",
        "\"config\":{\"tool_result_cap_bytes\":1.5},\"entries\":[]",
        "\"config\":{\"prune_trigger\":\"0.5\"},\"entries\":[]",
        "\"config\":{\"max_output_tokens\":4294967295},\"entries\":[]",
        "\"config\":{\"summary_style\":123},\"entries\":[]",
        "\"summary\":123,\"entries\":[]",
        "\"read_files\":{},\"entries\":[]",
        "\"modified_files\":[\"a\\u0000b\"],\"entries\":[]",
        "\"current_turn\":\"1\",\"entries\":[]",
        "\"current_turn\":-1,\"entries\":[]",
        "\"current_turn\":null,\"entries\":[]",
        "\"current_turn\":1.5,\"entries\":[]",
        "\"next_sequence\":0,\"entries\":[]",
        "\"summary_generation\":4294967296,\"entries\":[]",
        "\"fill_seen\":2,\"entries\":[]",
        "\"journal_sequence\":\"1\",\"entries\":[]",
        "\"compaction_count\":true,\"entries\":[]",
        "\"current_turn\":1,\"next_sequence\":1,\"entries\":[{\"sequence\":1,\"turn\":1,\"role\":1}]",
        "\"current_turn\":1,\"next_sequence\":2,\"compacted_through\":2,\"entries\":[{\"sequence\":1,\"turn\":1,\"role\":1}]",
        "\"current_turn\":1,\"next_sequence\":2,\"tail_floor\":2,\"entries\":[{\"sequence\":1,\"turn\":1,\"role\":1}]",
        "\"current_turn\":1,\"entries\":[{\"sequence\":1,\"turn\":1,\"role\":1},{\"sequence\":1,\"turn\":1,\"role\":1}]",
        "\"current_turn\":1,\"entries\":[{\"sequence\":2,\"turn\":1,\"role\":1},{\"sequence\":1,\"turn\":1,\"role\":1}]",
        "\"current_turn\":1,\"entries\":[{\"sequence\":0,\"turn\":1,\"role\":1}]",
        "\"current_turn\":1,\"entries\":[{\"sequence\":18446744073709551615,\"turn\":1,\"role\":1}]",
        "\"current_turn\":1,\"entries\":[{\"sequence\":\"1\",\"turn\":1,\"role\":1}]",
        "\"current_turn\":1,\"entries\":[{\"sequence\":1,\"turn\":\"1\",\"role\":1}]",
        "\"current_turn\":1,\"entries\":[{\"sequence\":1,\"turn\":1,\"role\":1,\"flags\":4294967297}]"
    };
    static const uint64_t aWindows[] = { INT64_MAX, (uint64_t)INT64_MAX + 1u, UINT64_MAX };
    xllm_error tError;
    xllm_session* pSession;
    xllm_session* pLoaded;
    xllm_session_config tConfig, tRestored;
    char sFields[1024];
    size_t i;
    for ( i = 0u; i < sizeof(aWindows) / sizeof(aWindows[0]); ++i ) {
        xllmSessionConfigInit(&tConfig);
        tConfig.uContextWindowTokens = aWindows[i];
        tConfig.uJournalMaxBytes = aWindows[i];
        tConfig.uMaxOutputTokens = tConfig.uOutputReserveTokens = tConfig.uSafetyReserveTokens = 1u;
        pSession = xllmSessionCreate(&tConfig, &tError);
        SESSION_CHECK(pSession && xllmSessionSave(pSession, sNumericSnapshot, &tError),
            "full-width unsigned config snapshot saves");
        pLoaded = xllmSessionLoad(sNumericSnapshot, &tError);
        SESSION_CHECK(pLoaded && xllmSessionGetConfig(pLoaded, &tRestored) &&
            tRestored.uContextWindowTokens == aWindows[i] && tRestored.uJournalMaxBytes == aWindows[i],
            "unsigned config values survive above INT64_MAX exactly");
        xllmSessionDestroy(pLoaded);
        xllmSessionDestroy(pSession);
    }
    for ( i = 0u; i < sizeof(aNarrow) / sizeof(aNarrow[0]); ++i ) {
        snprintf(sFields, sizeof(sFields), "\"config\":{\"%s\":4294967297},\"entries\":[]", aNarrow[i]);
        pSession = test_numeric_load(sFields, &tError);
        if ( pSession || tError.eCode != XLLM_ERROR_PARSE ) printf("       invalid narrow field: %s\n", aNarrow[i]);
        SESSION_CHECK(!pSession && tError.eCode == XLLM_ERROR_PARSE,
            "out-of-range config cannot wrap to a valid uint32 value");
        xllmSessionDestroy(pSession);
    }
    for ( i = 0u; i < sizeof(aInvalid) / sizeof(aInvalid[0]); ++i ) {
        pSession = test_numeric_load(aInvalid[i], &tError);
        if ( pSession || tError.eCode != XLLM_ERROR_PARSE ) printf("       invalid numeric snapshot case: %u\n", (unsigned)i);
        SESSION_CHECK(!pSession && tError.eCode == XLLM_ERROR_PARSE,
            "invalid numeric snapshot fields and sequence relations fail explicitly");
        xllmSessionDestroy(pSession);
    }
    pSession = test_numeric_load("\"current_turn\":1,\"next_sequence\":9,\"entries\":[{\"sequence\":3,\"turn\":1,\"role\":1}]", &tError);
    SESSION_CHECK(pSession && xllmSessionAddText(pSession, 1u, XLLM_ROLE_USER, "after a valid gap", 0u),
        "strict recovery keeps valid gaps in increasing sequence numbers");
    xllmSessionDestroy(pSession);
    {
        xllm_session_summary tSummary;
        pSession = test_numeric_load("\"summary_prompt_at_birth\":18446744073709551615,"
            "\"summary_output_at_birth\":9223372036854775808,\"entries\":[]", &tError);
        SESSION_CHECK(pSession && xllmSessionGetSummary(pSession, &tSummary) &&
            tSummary.uPromptTokensAtBirth == UINT64_MAX &&
            tSummary.uOutputTokensAtBirth == (uint64_t)INT64_MAX + 1u,
            "summary usage metadata retains unsigned 64-bit values");
        xllmSessionDestroy(pSession);
    }
}

static void test_persistence_numeric_endpoints(void)
{
    xllm_error tError;
    xllm_session* pSession;
    xllm_session* pRecovered;
    xllm_session_stats tStats;
    uint64_t uSize;
    pSession = test_numeric_load("\"current_turn\":0,\"next_sequence\":1,\"journal_sequence\":18446744073709551614,\"entries\":[]", &tError);
    xllmSessionDestroy(pSession);
    test_numeric_journal("18446744073709551615", "begin_turn", "\"turn\":1");
    pRecovered = xllmSessionRecover(sNumericSnapshot, sNumericJournal, NULL, &tError);
    uSize = test_path_size(sNumericJournal);
    SESSION_CHECK(pRecovered && xllmSessionGetStats(pRecovered, &tStats) &&
        tStats.uJournalSequence == UINT64_MAX && xllmSessionCurrentTurn(pRecovered) == 1u &&
        xllmSessionBeginTurn(pRecovered) == 0u && test_path_size(sNumericJournal) == uSize,
        "last journal sequence replays once and cannot wrap on append");
    xllmSessionDestroy(pRecovered);

    pSession = test_numeric_load("\"current_turn\":18446744073709551614,\"next_sequence\":1,\"entries\":[]", &tError);
    xllmSessionDestroy(pSession);
    test_numeric_journal("1", "begin_turn", "\"turn\":18446744073709551615");
    pRecovered = xllmSessionRecover(sNumericSnapshot, sNumericJournal, NULL, &tError);
    SESSION_CHECK(pRecovered && xllmSessionCurrentTurn(pRecovered) == UINT64_MAX &&
        xllmSessionBeginTurn(pRecovered) == 0u &&
        xllmSessionAddText(pRecovered, UINT64_MAX, XLLM_ROLE_USER, "last turn", 0u) &&
        xllmSessionSave(pRecovered, sNumericSnapshot, &tError),
        "last turn stays usable for messages without creating turn zero");
    xllmSessionDestroy(pRecovered);
    pSession = xllmSessionLoad(sNumericSnapshot, &tError);
    SESSION_CHECK(pSession && xllmSessionCurrentTurn(pSession) == UINT64_MAX,
        "a message in the last turn survives snapshot recovery");
    xllmSessionDestroy(pSession);
    test_numeric_journal("3", "begin_turn", "\"turn\":0");
    pRecovered = xllmSessionRecover(sNumericSnapshot, sNumericJournal, NULL, &tError);
    SESSION_CHECK(!pRecovered && tError.eCode == XLLM_ERROR_PARSE,
        "turn-zero replay cannot masquerade as an exhausted BeginTurn result");
    xllmSessionDestroy(pRecovered);

    pSession = test_numeric_load("\"current_turn\":1,\"next_sequence\":18446744073709551614,\"entries\":[]", &tError);
    xllmSessionDestroy(pSession);
    test_numeric_journal("1", "add_message", "\"entry\":{\"sequence\":18446744073709551614,\"turn\":1,\"role\":1,\"content\":\"last message\"}");
    pRecovered = xllmSessionRecover(sNumericSnapshot, sNumericJournal, NULL, &tError);
    SESSION_CHECK(pRecovered && !xllmSessionAddText(pRecovered, 1u, XLLM_ROLE_USER, "cannot append", 0u) &&
        xllmSessionSave(pRecovered, sNumericSnapshot, &tError),
        "last message sequence reaches a stable append barrier");
    xllmSessionDestroy(pRecovered);
    pSession = xllmSessionLoad(sNumericSnapshot, &tError);
    SESSION_CHECK(pSession && !xllmSessionAddText(pSession, 1u, XLLM_ROLE_USER, "still cannot append", 0u),
        "message sequence exhaustion survives save and load");
    xllmSessionDestroy(pSession);
    (void)xrtFileDelete(sNumericSnapshot);
    (void)xrtFileDelete(sNumericJournal);
}

static void test_numeric_journal_fields(void)
{
    static const char* aFlags[] = { "4294967297", "-1", "null", "true", "\"1\"", "1.5" };
    static const char* aGeneration[] = { "4294967296", "0", "5", "-1", "null", "\"6\"", "6.5" };
    static const char sBase[] = "\"current_turn\":2,\"next_sequence\":3,\"summary_generation\":5,"
        "\"entries\":[{\"sequence\":1,\"turn\":1,\"role\":1,\"content\":\"old\"},"
        "{\"sequence\":2,\"turn\":2,\"role\":1,\"content\":\"recent\"}]";
    xllm_error tError;
    xllm_session* pSession;
    xllm_session_stats tStats;
    xllm_session_buf tSummary = {0};
    char sFields[2048];
    size_t i;
    SESSION_CHECK(xllm_session__json_string(&tSummary, test_pi_summary("numeric journal validation")),
        "valid summary fixture is escaped independently of numeric fields");
    for ( i = 0u; i < sizeof(aFlags) / sizeof(aFlags[0]); ++i ) {
        pSession = test_numeric_load(sBase, &tError);
        xllmSessionDestroy(pSession);
        snprintf(sFields, sizeof(sFields), "\"entry\":{\"sequence\":3,\"turn\":2,\"role\":1,\"flags\":%s}", aFlags[i]);
        test_numeric_journal("1", "add_message", sFields);
        pSession = xllmSessionRecover(sNumericSnapshot, sNumericJournal, NULL, &tError);
        SESSION_CHECK(!pSession && tError.eCode == XLLM_ERROR_PARSE,
            "journal flags cannot truncate or silently become zero");
        xllmSessionDestroy(pSession);
    }
    for ( i = 0u; i < sizeof(aGeneration) / sizeof(aGeneration[0]); ++i ) {
        pSession = test_numeric_load(sBase, &tError);
        xllmSessionDestroy(pSession);
        snprintf(sFields, sizeof(sFields), "\"through_sequence\":1,\"compaction_count\":1,"
            "\"generation\":%s,\"summary\":%s", aGeneration[i], tSummary.pData);
        test_numeric_journal("1", "compact", sFields);
        pSession = xllmSessionRecover(sNumericSnapshot, sNumericJournal, NULL, &tError);
        SESSION_CHECK(!pSession && tError.eCode == XLLM_ERROR_PARSE,
            "journal generation cannot narrow, regress or default on bad types");
        xllmSessionDestroy(pSession);
    }
    pSession = test_numeric_load(sBase, &tError);
    xllmSessionDestroy(pSession);
    snprintf(sFields, sizeof(sFields), "\"through_sequence\":1,\"compaction_count\":1,\"summary\":%s", tSummary.pData);
    test_numeric_journal("1", "compact", sFields);
    pSession = xllmSessionRecover(sNumericSnapshot, sNumericJournal, NULL, &tError);
    SESSION_CHECK(pSession && xllmSessionGetStats(pSession, &tStats) && tStats.uSummaryGeneration == 6u,
        "legacy records without generation advance the current epoch");
    xllmSessionDestroy(pSession);

    pSession = test_numeric_load("\"current_turn\":2,\"next_sequence\":3,\"summary_generation\":4294967295,\"tail_floor\":1,"
        "\"entries\":[{\"sequence\":1,\"turn\":1,\"role\":1},{\"sequence\":2,\"turn\":2,\"role\":1}]", &tError);
    xllmSessionDestroy(pSession);
    test_numeric_journal("1", "truncate", "\"from_sequence\":1,\"to_sequence\":2");
    pSession = xllmSessionRecover(sNumericSnapshot, sNumericJournal, NULL, &tError);
    SESSION_CHECK(!pSession && tError.eCode == XLLM_ERROR_PARSE,
        "replayed truncation cannot wrap an exhausted generation");
    xllmSessionDestroy(pSession);

    pSession = test_numeric_load("\"journal_sequence\":1,\"entries\":[]", &tError);
    xllmSessionDestroy(pSession);
    test_numeric_journal("1", "unknown_operation", "\"turn\":1");
    pSession = xllmSessionRecover(sNumericSnapshot, sNumericJournal, NULL, &tError);
    SESSION_CHECK(!pSession && tError.eCode == XLLM_ERROR_PARSE,
        "deduplication cannot hide an unknown journal operation");
    xllmSessionDestroy(pSession);
    xllm_session__buf_unit(&tSummary);
}

typedef struct test_numeric_counter_hook {
    xllm_compaction* pPending;
    bool bAttempted;
    bool bRejected;
} test_numeric_counter_hook;

static void test_numeric_counter_event(xllm_session* pSession, const xllm_session_event* pEvent, void* pData)
{
    test_numeric_counter_hook* pProbe = (test_numeric_counter_hook*)pData;
    if ( pEvent->eType == XLLM_SESSION_EVENT_JOURNAL_RECORD && !pProbe->bAttempted ) {
        xllm_error tError;
        xllm_compaction* pNested;
        bool bRejected;
        bool bCompact = true;
        pProbe->bAttempted = true;
        pNested = xllmSessionPrepareCompaction(pSession, true, &tError);
        bRejected = !pNested && tError.eCode == XLLM_ERROR_HOOK;
        xllmCompactionDestroy(pNested);
        bRejected = !xllmSessionCommitCompaction(pSession, pProbe->pPending,
            test_pi_summary("reentrant commit is forbidden"), &tError) &&
            tError.eCode == XLLM_ERROR_HOOK && bRejected;
        bRejected = !xllmSessionOverflowLadder(pSession, &tError) &&
            tError.eCode == XLLM_ERROR_HOOK && bRejected;
        bRejected = !xllmSessionMaybeCompact(pSession, &bCompact, &tError) &&
            tError.eCode == XLLM_ERROR_HOOK && !bCompact && bRejected;
        pProbe->bRejected = bRejected;
    }
}

static void test_numeric_compaction_endpoints(void)
{
    char* sLarge = make_large_output(2000u, 11u);
    char sFields[4096];
    xllm_error tError;
    xllm_session* pSession;
    xllm_session* pRecovered;
    xllm_compaction* pPending;
    xllm_compaction* pAnother;
    xllm_session_stats tStats;
    xllm_session_hooks tHooks = {0};
    test_numeric_counter_hook tProbe = {0};
    uint64_t uBytes;
    if ( !sLarge ) { SESSION_CHECK(false, "counter endpoint fixture allocates"); return; }
    snprintf(sFields, sizeof(sFields), "\"config\":{\"keep_recent_tokens\":1},\"current_turn\":2,\"next_sequence\":3,"
        "\"summary_generation\":4294967294,\"entries\":[{\"sequence\":1,\"turn\":1,\"role\":1,\"content\":\"%s\"},"
        "{\"sequence\":2,\"turn\":2,\"role\":1,\"content\":\"recent\"}]", sLarge);
    pSession = test_numeric_load(sFields, &tError);
    pPending = pSession ? xllmSessionPrepareCompaction(pSession, true, &tError) : NULL;
    SESSION_CHECK(pPending != NULL, "valid prefix prepares just before the last generation");
    tProbe.pPending = pPending;
    tHooks.pOnEvent = test_numeric_counter_event;
    tHooks.pUserData = &tProbe;
    SESSION_CHECK(pSession && xllmSessionSetHooks(pSession, &tHooks), "numeric counter observer installs");
    (void)xrtFileDelete(sNumericJournal);
    SESSION_CHECK(pSession && xllmSessionEnableJournal(pSession, sNumericJournal, &tError) &&
        xllmSessionOverflowLadder(pSession, &tError) && xllmSessionGetStats(pSession, &tStats) &&
        tStats.uSummaryGeneration == UINT32_MAX,
        "first truncation from floor zero reaches the last generation");
    SESSION_CHECK(tProbe.bAttempted && tProbe.bRejected,
        "observation hooks cannot re-enter generation-changing operations");
    uBytes = test_path_size(sNumericJournal);
    SESSION_CHECK(pSession && pPending && !xllmSessionCommitCompaction(pSession, pPending,
            test_pi_summary("no generation remains"), &tError) && tError.eCode == XLLM_ERROR_LIMIT &&
        test_path_size(sNumericJournal) == uBytes,
        "pending compaction cannot wrap a generation exhausted by truncation");
    pAnother = pSession ? xllmSessionPrepareCompaction(pSession, true, &tError) : NULL;
    SESSION_CHECK(!pAnother && tError.eCode == XLLM_ERROR_LIMIT,
        "new compaction stops at an exhausted generation");
    xllmCompactionDestroy(pAnother);
    SESSION_CHECK(pSession && !xllmSessionOverflowLadder(pSession, &tError) &&
        tError.eCode == XLLM_ERROR_LIMIT && test_path_size(sNumericJournal) == uBytes,
        "live truncation cannot wrap an exhausted generation or write a record");
    xllmCompactionDestroy(pPending);
    xllmSessionDestroy(pSession);
    pRecovered = xllmSessionRecover(sNumericSnapshot, sNumericJournal, NULL, &tError);
    SESSION_CHECK(pRecovered && xllmSessionGetStats(pRecovered, &tStats) &&
        tStats.uSummaryGeneration == UINT32_MAX,
        "the real first-truncation record replays from floor zero");
    xllmSessionDestroy(pRecovered);

    snprintf(sFields, sizeof(sFields), "\"config\":{\"keep_recent_tokens\":1},\"current_turn\":2,\"next_sequence\":3,"
        "\"compaction_count\":18446744073709551615,\"entries\":[{\"sequence\":1,\"turn\":1,\"role\":1,\"content\":\"%s\"},"
        "{\"sequence\":2,\"turn\":2,\"role\":1,\"content\":\"recent\"}]", sLarge);
    pSession = test_numeric_load(sFields, &tError);
    pPending = pSession ? xllmSessionPrepareCompaction(pSession, true, &tError) : NULL;
    SESSION_CHECK(pSession && !pPending && tError.eCode == XLLM_ERROR_LIMIT,
        "exhausted compaction count cannot restart at zero");
    xllmCompactionDestroy(pPending);
    xllmSessionDestroy(pSession);
    free(sLarge);
    (void)xrtFileDelete(sNumericSnapshot);
    (void)xrtFileDelete(sNumericJournal);
}

static void test_persistence_numeric_cases(void)
{
    test_persistence_numeric_types();
    test_persistence_numeric_endpoints();
    test_numeric_journal_fields();
    test_numeric_compaction_endpoints();
}

#endif
