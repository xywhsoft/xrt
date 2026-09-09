#include "../xllm.c"
#include "../xllm-session.c"

static int g_iSessionFailures = 0;

static uint64_t test_path_size(const char* sPath)
{
    xfileinfo tInfo;
    return xrtPathStat(sPath, true, &tInfo) ? tInfo.Size : 0u;
}

#define SESSION_CHECK(expr, name) do { \
    bool xllm_session_ok__ = !!(expr); \
    printf("  %-56s %s\n", (name), xllm_session_ok__ ? "PASS" : "FAIL"); \
    if ( !xllm_session_ok__ ) { ++g_iSessionFailures; } \
} while (0)

static char* make_large_output(size_t iSize, unsigned iSeed)
{
    char* sText = (char*)malloc(iSize + 1u);
    size_t i;
    if ( !sText ) { return NULL; }
    for ( i = 0u; i < iSize; ++i ) {
        sText[i] = (char)('a' + (int)((i + iSeed) % 26u));
    }
    sText[iSize] = '\0';
    return sText;
}

static bool add_tool_round(xllm_session* pSession, uint64_t uTurn, unsigned iRound, const char* sOutput)
{
    char sUser[128];
    char sCallId[64];
    char sArguments[128];
    xllm_tool_call tCall;
    xllm_response tResponse;
    (void)snprintf(sUser, sizeof(sUser), "Round %u: inspect module and continue the implementation.", iRound);
    (void)snprintf(sCallId, sizeof(sCallId), "call_%u", iRound);
    (void)snprintf(sArguments, sizeof(sArguments), "{\"path\":\"module_%u.c\"}", iRound);
    memset(&tCall, 0, sizeof(tCall));
    tCall.sId = sCallId;
    tCall.sName = "read_file";
    tCall.sArgumentsJson = sArguments;
    memset(&tResponse, 0, sizeof(tResponse));
    tResponse.sContent = "";
    tResponse.sReasoningContent = "Inspect the requested source before editing.";
    tResponse.sFinishReason = "tool_calls";
    tResponse.pToolCalls = &tCall;
    tResponse.iToolCallCount = 1u;
    return xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, sUser, 0u) &&
        xllmSessionAddAssistantResponse(pSession, uTurn, &tResponse) &&
        xllmSessionAddToolResult(pSession, uTurn, sCallId, sOutput);
}

static bool request_has_text(const xllm_request* pRequest, const char* sNeedle)
{
    size_t i;
    for ( i = 0u; i < pRequest->iMessageCount; ++i ) {
        const char* sContent = pRequest->pMessages[i].sContent;
        if ( sContent && strstr(sContent, sNeedle) ) { return true; }
    }
    return false;
}

static void test_compaction_user_bridge(void)
{
    xllm_session_config tConfig;
    xllm_session* pSession;
    xllm_compaction* pCompaction;
    xllm_response tResponse;
    xllm_tool_call tCall;
    xllm_request tRequest;
    xllm_error tError;
    xllm_compaction_quality tQuality;
    uint64_t uTurn;

    xllmSessionConfigInit(&tConfig);
    tConfig.uContextWindowTokens = 8000u;
    tConfig.uMaxOutputTokens = 1000u;
    tConfig.uOutputReserveTokens = 1000u;
    tConfig.uSafetyReserveTokens = 500u;
    tConfig.uRecentTurnsToKeep = 1u;
    pSession = xllmSessionCreate(&tConfig, &tError);
    SESSION_CHECK(pSession != NULL, "compaction bridge session creates");
    if ( !pSession ) return;

    uTurn = xllmSessionBeginTurn(pSession);
    SESSION_CHECK(xllmSessionAddText(pSession, uTurn, XLLM_ROLE_SYSTEM, "Pinned coding-agent contract.", XLLM_SESSION_ENTRY_PINNED), "bridge test pins one system message");
    uTurn = xllmSessionBeginTurn(pSession);
    SESSION_CHECK(xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, "Implement the requested project.", 0u), "bridge test adds the original user objective");

    memset(&tCall, 0, sizeof(tCall));
    tCall.sId = "retained_call";
    tCall.sName = "write_file";
    tCall.sArgumentsJson = "{\"path\":\"retained.txt\",\"content\":\"ok\"}";
    memset(&tResponse, 0, sizeof(tResponse));
    tResponse.sContent = "";
    tResponse.pToolCalls = &tCall;
    tResponse.iToolCallCount = 1u;
    uTurn = xllmSessionBeginTurn(pSession);
    SESSION_CHECK(xllmSessionAddAssistantResponse(pSession, uTurn, &tResponse) &&
        xllmSessionAddToolResult(pSession, uTurn, tCall.sId, "status: success"),
        "bridge test retains an assistant/tool-only agent turn");

    pCompaction = xllmSessionPrepareCompaction(pSession, true, &tError);
    SESSION_CHECK(pCompaction != NULL, "bridge compaction prepares across the original user turn");
    SESSION_CHECK(pCompaction && xllmCompactionEvaluateSummary(pCompaction,
        "Objective: too little continuity.", &tQuality, &tError) &&
        !tQuality.bAccepted && tQuality.uMissingSections != 0u,
        "quality report rejects an incomplete compaction summary");
    SESSION_CHECK(pCompaction && xllmSessionCommitCompaction(pSession, pCompaction,
        "Objective: implement the requested project and preserve tool continuity.\n"
        "Constraints: retain the pinned coding-agent contract and exact tool pairing.\n"
        "Architecture and decisions: use the durable session checkpoint as the history bridge.\n"
        "Completed work: the original user objective and prior tool outcome were captured.\n"
        "Current repository state: the retained write call remains in the verbatim suffix.\n"
        "Verification evidence: the safe prefix excludes the retained assistant/tool turn.\n"
        "Open issues and risks: subsequent work must not lose the retained tool correlation.\n"
        "Exact next actions: continue from the retained tool result and verify the project.", &tError),
        "bridge compaction commits a continuation summary");
    xllmCompactionDestroy(pCompaction);

    xllmRequestInit(&tRequest);
    SESSION_CHECK(xllmSessionBuildRequest(pSession, &tRequest, &tError), "bridge request renders after compaction");
    SESSION_CHECK(tRequest.iMessageCount == 4u &&
        tRequest.pMessages[0].eRole == XLLM_ROLE_SYSTEM &&
        tRequest.pMessages[1].eRole == XLLM_ROLE_USER &&
        tRequest.pMessages[2].eRole == XLLM_ROLE_ASSISTANT &&
        tRequest.pMessages[3].eRole == XLLM_ROLE_TOOL,
        "compaction summary bridges system to retained assistant/tool suffix as a user turn");
    SESSION_CHECK(tRequest.iMessageCount == 4u &&
        tRequest.pMessages[2].iToolCallCount == 1u &&
        tRequest.pMessages[3].sToolCallId &&
        strcmp(tRequest.pMessages[3].sToolCallId, "retained_call") == 0,
        "compaction bridge preserves retained tool-call pairing");
    xllmRequestUnit(&tRequest);
    xllmSessionDestroy(pSession);
}

static void test_default_profile(void)
{
    xllm_session_config tConfig;
    xllm_session_config tEffective;
    xllm_session* pSession;
    xllm_session_stats tStats;
    xllm_request tRequest;
    xllm_error tError;
    char* sLarge;
    uint64_t uTurn;
    xllmSessionConfigInit(&tConfig);
    pSession = xllmSessionCreate(&tConfig, &tError);
    SESSION_CHECK(pSession != NULL, "default GLM-5.1 session profile creates");
    SESSION_CHECK(tConfig.uContextWindowTokens == XLLM_SESSION_DEFAULT_CONTEXT_WINDOW_TOKENS &&
        tConfig.uMaxOutputTokens == XLLM_SESSION_DEFAULT_MAX_OUTPUT_TOKENS,
        "current context and maximum-output profile");
    SESSION_CHECK(xllmSessionComputeSafetyReserve(XLLM_SESSION_DEFAULT_CONTEXT_WINDOW_TOKENS) == 8000u,
        "bounded 3 percent safety reserve profile");
    SESSION_CHECK(pSession && xllmSessionGetConfig(pSession, &tEffective) && tEffective.uOutputReserveTokens == 32768u, "dynamic output reserve is distinct from output ceiling");
    SESSION_CHECK(pSession && xllmSessionGetStats(pSession, &tStats) && tStats.uInputBudgetTokens == 164032u && tStats.uNextMaxOutputTokens == 131072u, "effective input budget preserves full output on sparse context");
    sLarge = make_large_output(300000u, 7u);
    uTurn = pSession ? xllmSessionBeginTurn(pSession) : 0u;
    SESSION_CHECK(sLarge && uTurn && xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, sLarge, 0u), "large active input added for dynamic output test");
    xllmRequestInit(&tRequest);
    SESSION_CHECK(xllmSessionBuildRequest(pSession, &tRequest, &tError) && tRequest.uMaxOutputTokens < 131072u && tRequest.uMaxOutputTokens > 32768u, "request output ceiling shrinks only as active input grows");
    xllmRequestUnit(&tRequest);
    free(sLarge);
    xllmSessionDestroy(pSession);
}

static void test_budget_compaction_persistence(void)
{
    static const char sStatePath[] = "build/session_state_test.json";
    static const char sSummary[] =
        "Objective: continue the code-agent implementation.\n"
        "Constraints: preserve the pinned contract, exact paths, and completed tool outcomes.\n"
        "Architecture and decisions: compact only a safe completed prefix and retain recent turns verbatim.\n"
        "Completed work: rounds 1-4 inspected modules and preserved tool outcomes.\n"
        "Current repository state: recent rounds 5-6 remain verbatim and are ready to continue.\n"
        "Verification evidence: every compacted round has a paired successful tool result.\n"
        "Open issues and risks: no unresolved failures; avoid dropping retained tool-call structure.\n"
        "Exact next actions: continue from the retained messages.";
    xllm_session_config tConfig;
    xllm_session* pSession;
    xllm_session* pLoaded = NULL;
    xllm_session_stats tBefore;
    xllm_session_stats tAfter;
    xllm_session_stats tLoaded;
    xllm_request tRequest;
    xllm_compaction* pCompaction = NULL;
    xllm_error tError;
    char* psOutputs[6] = {0};
    unsigned i;
    uint64_t uThrough = 0u;

    xllmSessionConfigInit(&tConfig);
    tConfig.uContextWindowTokens = 2600u;
    tConfig.uMaxOutputTokens = 300u;
    tConfig.uSafetyReserveTokens = 100u;
    tConfig.uRecentTurnsToKeep = 2u;
    tConfig.uToolPruneBytes = 256u;
    tConfig.uSummaryMaxTokens = 256u;
    tConfig.fPruneTrigger = 0.25;
    tConfig.fCompactTrigger = 0.90;
    pSession = xllmSessionCreate(&tConfig, &tError);
    SESSION_CHECK(pSession != NULL, "small deterministic session creates");
    if ( !pSession ) { return; }
    SESSION_CHECK(xllmSessionAddText(pSession, 0u, XLLM_ROLE_SYSTEM, "Pinned agent contract and repository constraints.", XLLM_SESSION_ENTRY_PINNED), "pinned system contract added");
    for ( i = 0u; i < 6u; ++i ) {
        uint64_t uTurn = xllmSessionBeginTurn(pSession);
        psOutputs[i] = make_large_output(1200u, i);
        SESSION_CHECK(psOutputs[i] != NULL && add_tool_round(pSession, uTurn, i + 1u, psOutputs[i]), "complete tool round added");
    }
    SESSION_CHECK(xllmSessionGetStats(pSession, &tBefore), "session pressure stats available");
    SESSION_CHECK(tBefore.uPendingToolCalls == 0u, "completed tool rounds have no pending calls");
    SESSION_CHECK(tBefore.ePressure == XLLM_SESSION_PRESSURE_PRUNE || tBefore.ePressure == XLLM_SESSION_PRESSURE_COMPACT, "large history activates pressure policy");
    SESSION_CHECK(tBefore.uRenderedActiveTokens < tBefore.uRawActiveTokens, "old large tool outputs are soft-pruned");

    xllmRequestInit(&tRequest);
    SESSION_CHECK(xllmSessionBuildRequest(pSession, &tRequest, &tError), "active request renders before compaction");
    SESSION_CHECK(tRequest.uMaxOutputTokens == 300u, "session output budget propagates to model request");
    SESSION_CHECK(request_has_text(&tRequest, "older tool output pruned"), "rendered request marks pruned tool output");
    SESSION_CHECK(tRequest.iMessageCount == 19u, "soft pruning preserves message and tool-call structure");
    xllmRequestUnit(&tRequest);

    pCompaction = xllmSessionPrepareCompaction(pSession, true, &tError);
    SESSION_CHECK(pCompaction != NULL, "forced compaction transaction prepares");
    SESSION_CHECK(pCompaction && strstr(xllmCompactionPrompt(pCompaction), "Objective; Constraints") != NULL, "compaction prompt encodes durable summary contract");
    SESSION_CHECK(pCompaction && xllmCompactionEstimatedTokens(pCompaction) > 0u, "compaction prompt token estimate available");
    uThrough = xllmCompactionThroughSequence(pCompaction);
    SESSION_CHECK(uThrough > 0u, "compaction selects a completed prefix");
    SESSION_CHECK(pCompaction && xllmSessionCommitCompaction(pSession, pCompaction, sSummary, &tError), "compaction summary commits atomically");
    xllmCompactionDestroy(pCompaction);
    pCompaction = NULL;
    SESSION_CHECK(xllmSessionGetStats(pSession, &tAfter), "post-compaction stats available");
    SESSION_CHECK(tAfter.uCompactionCount == 1u && tAfter.uCompactedThroughSequence == uThrough, "compaction checkpoint advances exactly once");
    SESSION_CHECK(tAfter.uRawActiveTokens < tBefore.uRawActiveTokens, "compaction reduces active token ledger");

    xllmRequestInit(&tRequest);
    SESSION_CHECK(xllmSessionBuildRequest(pSession, &tRequest, &tError), "request renders after compaction");
    SESSION_CHECK(request_has_text(&tRequest, "Compacted session state"), "request contains authoritative summary checkpoint");
    SESSION_CHECK(request_has_text(&tRequest, "Round 5") && request_has_text(&tRequest, "Round 6"), "recent turns remain verbatim after compaction");
    SESSION_CHECK(!request_has_text(&tRequest, "Round 1"), "covered old turns leave the active request");
    xllmRequestUnit(&tRequest);

    SESSION_CHECK(xllmSessionSave(pSession, sStatePath, &tError), "session snapshot writes atomically");
    pLoaded = xllmSessionLoad(sStatePath, &tError);
    SESSION_CHECK(pLoaded != NULL, "session snapshot loads");
    SESSION_CHECK(pLoaded && xllmSessionGetStats(pLoaded, &tLoaded) && tLoaded.uCompactionCount == 1u && tLoaded.uCompactedThroughSequence == uThrough, "loaded session preserves compaction checkpoint");
    if ( pLoaded ) {
        xllm_session* pFork = xllmSessionFork(pLoaded, &tError);
        SESSION_CHECK(pFork != NULL, "loaded compacted session forks");
        SESSION_CHECK(pFork && xllmSessionGetStats(pFork, &tLoaded) &&
            tLoaded.uCompactionCount == 1u && tLoaded.uCompactedThroughSequence == uThrough,
            "fork preserves compaction checkpoint");
        if ( pFork ) {
            uint64_t uParentTurn = xllmSessionBeginTurn(pLoaded);
            uint64_t uForkTurn = xllmSessionBeginTurn(pFork);
            SESSION_CHECK(xllmSessionAddText(pLoaded, uParentTurn, XLLM_ROLE_USER, "Continue on parent branch.", 0u) &&
                xllmSessionAddText(pFork, uForkTurn, XLLM_ROLE_USER, "Explore an isolated branch.", 0u),
                "parent and fork accept divergent turns");
            xllmRequestInit(&tRequest);
            SESSION_CHECK(xllmSessionBuildRequest(pLoaded, &tRequest, &tError) &&
                request_has_text(&tRequest, "Continue on parent branch") &&
                !request_has_text(&tRequest, "Explore an isolated branch"),
                "parent request excludes fork-only state");
            xllmRequestUnit(&tRequest);
            xllmRequestInit(&tRequest);
            SESSION_CHECK(xllmSessionBuildRequest(pFork, &tRequest, &tError) &&
                request_has_text(&tRequest, "Explore an isolated branch") &&
                !request_has_text(&tRequest, "Continue on parent branch") &&
                request_has_text(&tRequest, "Completed work: rounds 1-4"),
                "fork request retains summary and isolates parent state");
            xllmRequestUnit(&tRequest);
        }
        xllmSessionDestroy(pFork);
        uint64_t uTurn = xllmSessionBeginTurn(pLoaded);
        SESSION_CHECK(xllmSessionAddText(pLoaded, uTurn, XLLM_ROLE_USER, "Continue after process restart.", 0u), "loaded session accepts a continuation turn");
        xllmRequestInit(&tRequest);
        SESSION_CHECK(xllmSessionBuildRequest(pLoaded, &tRequest, &tError), "loaded session renders continuation request");
        SESSION_CHECK(request_has_text(&tRequest, "Completed work: rounds 1-4") && request_has_text(&tRequest, "Continue after process restart"), "summary continuity survives restart");
        xllmRequestUnit(&tRequest);
        {
            xllm_tool_call tPendingCall;
            xllm_response tPendingResponse;
            xllm_compaction* pPendingCompaction;
            uint64_t uPendingTurn = xllmSessionBeginTurn(pLoaded);
            memset(&tPendingCall, 0, sizeof(tPendingCall));
            tPendingCall.sId = "pending_call";
            tPendingCall.sName = "write_file";
            tPendingCall.sArgumentsJson = "{\"path\":\"pending.c\"}";
            memset(&tPendingResponse, 0, sizeof(tPendingResponse));
            tPendingResponse.sContent = "";
            tPendingResponse.pToolCalls = &tPendingCall;
            tPendingResponse.iToolCallCount = 1u;
            SESSION_CHECK(xllmSessionAddAssistantResponse(pLoaded, uPendingTurn, &tPendingResponse), "pending assistant tool call added");
            SESSION_CHECK(xllmSessionGetStats(pLoaded, &tLoaded) && tLoaded.uPendingToolCalls == 1u, "pending tool call tracked");
            pPendingCompaction = xllmSessionPrepareCompaction(pLoaded, true, &tError);
            SESSION_CHECK(pPendingCompaction != NULL && strstr(xllmCompactionPrompt(pPendingCompaction), "pending_call") == NULL, "compaction excludes unresolved tool-call turn");
            xllmCompactionDestroy(pPendingCompaction);
        }
    }
    for ( i = 0u; i < 6u; ++i ) { free(psOutputs[i]); }
    xllmSessionDestroy(pLoaded);
    xllmSessionDestroy(pSession);
    (void)xrtFileDelete((str)sStatePath);
}

static void test_journal_checkpoint_recovery(void)
{
    static const char sSnapshotPath[] = "build/session_recovery_test.json";
    static const char sJournalPath[] = "build/session_recovery_test.ndjson";
    static const char sSummary[] =
        "Objective: preserve a crash-safe coding session.\n"
        "Constraints: journal acknowledged mutations before exposing them to callers.\n"
        "Architecture and decisions: recover from the snapshot plus a valid journal prefix.\n"
        "Completed work: the initial journaled turns were recovered.\n"
        "Current repository state: the latest branch remains verbatim.\n"
        "Verification evidence: journal sequence and recovered turns were checked.\n"
        "Open issues and risks: ignore a torn tail but reject complete corrupt records.\n"
        "Exact next actions: continue from the recovered checkpoint.";
    xllm_session_config tConfig;
    xllm_session* pSession = NULL;
    xllm_session* pRecovered = NULL;
    xllm_session* pStale = NULL;
    xllm_session* pCorrupt = NULL;
    xllm_compaction* pCompaction = NULL;
    xllm_session_stats tStats;
    xllm_request tRequest;
    xllm_error tError;
    uint64_t uTurn;
    size_t iCompleteSize;
    xllm_pending_tool_call tPendingCall;
    xllm_session_tail tTail;

    (void)xrtFileDelete((str)sSnapshotPath);
    (void)xrtFileDelete((str)sJournalPath);
    xllmSessionConfigInit(&tConfig);
    tConfig.uContextWindowTokens = 8000u;
    tConfig.uMaxOutputTokens = 1000u;
    tConfig.uOutputReserveTokens = 1000u;
    tConfig.uSafetyReserveTokens = 500u;
    tConfig.uRecentTurnsToKeep = 1u;
    tConfig.uSummaryMaxTokens = 1000u;
    pSession = xllmSessionCreate(&tConfig, &tError);
    SESSION_CHECK(pSession != NULL, "journal recovery session creates");
    SESSION_CHECK(pSession && xllmSessionEnableJournal(pSession, sJournalPath, &tError),
        "empty incremental journal attaches");
    if ( !pSession || !xllmSessionJournalPath(pSession) ) goto done;

    uTurn = xllmSessionBeginTurn(pSession);
    SESSION_CHECK(uTurn &&
        xllmSessionAddText(pSession, uTurn, XLLM_ROLE_SYSTEM, "Crash-safe agent contract.", XLLM_SESSION_ENTRY_PINNED) &&
        xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, "Start durable work.", 0u),
        "first turn is journaled before acknowledgement");
    uTurn = xllmSessionBeginTurn(pSession);
    SESSION_CHECK(uTurn &&
        xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, "Implement the first slice.", 0u) &&
        xllmSessionAddText(pSession, uTurn, XLLM_ROLE_ASSISTANT, "First slice complete.", 0u),
        "second turn is journaled");
    SESSION_CHECK(xllmSessionGetStats(pSession, &tStats) && tStats.bJournalEnabled &&
        tStats.uJournalSequence == 6u, "journal sequence covers turns and messages");

    pRecovered = xllmSessionRecover(sSnapshotPath, sJournalPath, &tConfig, &tError);
    SESSION_CHECK(pRecovered != NULL, "journal-only process recovery succeeds");
    xllmRequestInit(&tRequest);
    SESSION_CHECK(pRecovered && xllmSessionBuildRequest(pRecovered, &tRequest, &tError) &&
        request_has_text(&tRequest, "Start durable work") && request_has_text(&tRequest, "First slice complete"),
        "journal-only recovery preserves conversation state");
    xllmRequestUnit(&tRequest);
    xllmSessionDestroy(pSession);
    pSession = NULL;
    if ( !pRecovered ) goto done;

    SESSION_CHECK(xllmSessionCheckpoint(pRecovered, sSnapshotPath, &tError),
        "checkpoint atomically snapshots and retires covered journal");
    SESSION_CHECK(!xrtFileExists((str)sJournalPath), "covered journal is removed after checkpoint");
    uTurn = xllmSessionBeginTurn(pRecovered);
    SESSION_CHECK(uTurn &&
        xllmSessionAddText(pRecovered, uTurn, XLLM_ROLE_USER, "Continue after checkpoint.", 0u) &&
        xllmSessionAddText(pRecovered, uTurn, XLLM_ROLE_ASSISTANT, "Checkpoint continuation complete.", 0u),
        "post-checkpoint delta is journaled");
    uTurn = xllmSessionBeginTurn(pRecovered);
    SESSION_CHECK(uTurn &&
        xllmSessionAddText(pRecovered, uTurn, XLLM_ROLE_USER, "Keep this latest branch verbatim.", 0u) &&
        xllmSessionAddText(pRecovered, uTurn, XLLM_ROLE_ASSISTANT, "Latest branch retained.", 0u),
        "latest recovery turn is journaled");
    pCompaction = xllmSessionPrepareCompaction(pRecovered, true, &tError);
    SESSION_CHECK(pCompaction && xllmSessionCommitCompaction(pRecovered, pCompaction, sSummary, &tError),
        "compaction checkpoint is journaled");
    xllmCompactionDestroy(pCompaction);
    pCompaction = NULL;

    iCompleteSize = (size_t)test_path_size(sJournalPath);
    SESSION_CHECK(xrtFileAppend(sJournalPath,
        (xbytesview){ (const uint8*)"{\"partial\":", 11u }),
        "test simulates a torn final journal record");
    pStale = xllmSessionRecover(sSnapshotPath, sJournalPath, NULL, &tError);
    SESSION_CHECK(pStale != NULL, "snapshot plus journal recovery ignores torn tail");
    SESSION_CHECK(test_path_size(sJournalPath) == (uint64_t)iCompleteSize,
        "recovery truncates torn tail before resuming writes");
    xllmRequestInit(&tRequest);
    SESSION_CHECK(pStale && xllmSessionBuildRequest(pStale, &tRequest, &tError) &&
        request_has_text(&tRequest, "initial journaled turns were recovered") &&
        request_has_text(&tRequest, "Keep this latest branch verbatim"),
        "replayed compaction and recent suffix preserve continuity");
    xllmRequestUnit(&tRequest);
    xllmSessionDestroy(pStale);
    pStale = NULL;

    SESSION_CHECK(xllmSessionSave(pRecovered, sSnapshotPath, &tError),
        "test snapshots before stale journal retirement");
    pStale = xllmSessionRecover(sSnapshotPath, sJournalPath, NULL, &tError);
    SESSION_CHECK(pStale && xllmSessionGetStats(pStale, &tStats) &&
        tStats.uCompactionCount == 1u && tStats.uCurrentTurn == 4u,
        "snapshot sequence prevents duplicate journal replay");
    xllmSessionDestroy(pStale);
    pStale = NULL;
    SESSION_CHECK(xllmSessionCheckpoint(pRecovered, sSnapshotPath, &tError),
        "recovered session establishes a clean checkpoint");

    {
        xllm_tool_call arrCalls[2];
        xllm_response tResponse;
        memset(arrCalls, 0, sizeof(arrCalls));
        arrCalls[0].sId = "recovered_done";
        arrCalls[0].sName = "list_files";
        arrCalls[0].sArgumentsJson = "{\"path\":\".\"}";
        arrCalls[1].sId = "recovered_pending";
        arrCalls[1].sName = "exec_command";
        arrCalls[1].sArgumentsJson = "{\"command\":\"verify\"}";
        memset(&tResponse, 0, sizeof(tResponse));
        tResponse.pToolCalls = arrCalls;
        tResponse.iToolCallCount = 2u;
        uTurn = xllmSessionBeginTurn(pRecovered);
        SESSION_CHECK(uTurn &&
            xllmSessionAddText(pRecovered, uTurn, XLLM_ROLE_USER, "Resume after a tool-batch crash.", 0u) &&
            xllmSessionAddAssistantResponse(pRecovered, uTurn, &tResponse),
            "interrupted parallel tool batch is journaled");
        SESSION_CHECK(xllmSessionCheckpoint(pRecovered, sSnapshotPath, &tError),
            "assistant tool batch is checkpointed before simulated crash");
        SESSION_CHECK(xllmSessionAddToolResult(pRecovered, uTurn, "recovered_done", "listed"),
            "first tool result is journaled before simulated crash");
        pStale = xllmSessionRecover(sSnapshotPath, sJournalPath, NULL, &tError);
        SESSION_CHECK(pStale && xllmSessionPendingToolCallCount(pStale) == 1u &&
            xllmSessionPendingToolCallAt(pStale, 0u, &tPendingCall) &&
            tPendingCall.uTurn == uTurn && strcmp(tPendingCall.sId, "recovered_pending") == 0 &&
            strcmp(tPendingCall.sName, "exec_command") == 0 &&
            strcmp(tPendingCall.sArgumentsJson, "{\"command\":\"verify\"}") == 0,
            "recovery exposes only the unresolved tool call with its original turn");
        SESSION_CHECK(xllmSessionGetTail(pStale, &tTail) && tTail.bHasMessage &&
            tTail.uTurn == uTurn && tTail.eRole == XLLM_ROLE_TOOL,
            "lightweight recovered tail identifies the completed tool result without rendering context");
        SESSION_CHECK(!xllmSessionPendingToolCallAt(pStale, 1u, &tPendingCall),
            "pending tool view rejects an out-of-range index");
        xllmSessionDestroy(pStale);
        pStale = NULL;
        SESSION_CHECK(xllmSessionAddToolResult(pRecovered, uTurn, "recovered_pending", "verified"),
            "remaining tool call resolves before the recovery fixture closes");
        SESSION_CHECK(xllmSessionCheckpoint(pRecovered, sSnapshotPath, &tError),
            "tool recovery fixture establishes a clean checkpoint");
    }

    uTurn = xllmSessionBeginTurn(pRecovered);
    SESSION_CHECK(uTurn && xllmSessionAddText(pRecovered, uTurn, XLLM_ROLE_USER,
        "This valid record precedes corruption.", 0u), "valid delta precedes corrupt record");
    SESSION_CHECK(xrtFileAppend(sJournalPath,
        (xbytesview){ (const uint8*)"not-json\n", 9u }),
        "test appends one complete corrupt record");
    pCorrupt = xllmSessionRecover(sSnapshotPath, sJournalPath, NULL, &tError);
    SESSION_CHECK(pCorrupt == NULL && tError.eCode == XLLM_ERROR_PARSE,
        "complete journal corruption fails recovery explicitly");

done:
    xllmCompactionDestroy(pCompaction);
    xllmSessionDestroy(pCorrupt);
    xllmSessionDestroy(pStale);
    xllmSessionDestroy(pRecovered);
    xllmSessionDestroy(pSession);
    (void)xrtFileDelete((str)sJournalPath);
    (void)xrtFileDelete((str)sSnapshotPath);
}

int main(void)
{
    printf("xllm-session v2 tests\n");
    test_default_profile();
    test_compaction_user_bridge();
    test_budget_compaction_persistence();
    test_journal_checkpoint_recovery();
    printf("xllm-session v2: %s (%d failures)\n", g_iSessionFailures ? "FAIL" : "PASS", g_iSessionFailures);
    return g_iSessionFailures ? 1 : 0;
}
