/* The xllm unity's own bridge consumes <xrt.h> first; the session journal
 * additionally needs the JSONL module, so extend the module set before
 * that inclusion takes effect. */
#define XRT_MODULE_JSONL_READ
#include "../../xllm/xllm.c"
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

static xllm_response* test_make_response(const char* sContent, uint64_t uIn, uint64_t uOut)
{
    xllm_response* pResponse = (xllm_response*)calloc(1u, sizeof(*pResponse));
    if ( !pResponse ) { return NULL; }
    pResponse->sContent = xllm_session__strdup(sContent);
    pResponse->tUsage.uInputTokens = uIn;
    pResponse->tUsage.uOutputTokens = uOut;
    pResponse->tUsage.uTotalTokens = uIn + uOut;
    pResponse->eFinish = XLLM_FINISH_STOP;
    if ( !pResponse->sContent ) { xllmResponseDestroy(pResponse); return NULL; }
    return pResponse;
}

static const char* test_pi_summary(const char* sGoal)
{
    static char sBuffer[1024];
    (void)snprintf(sBuffer, sizeof(sBuffer),
        "## Goal\n%s\n"
        "## Constraints & Preferences\nPreserve the pinned contract and exact tool pairing.\n"
        "## Progress\nPrior turns and tool outcomes were captured.\n"
        "## Key Decisions\nUse the durable session checkpoint as the history bridge.\n"
        "## Next Steps\nContinue from the retained messages.\n"
        "## Critical Context\nThe retained suffix stays verbatim; read-files: none; modified-files: none.\n",
        sGoal);
    return sBuffer;
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
    tConfig.uKeepRecentTokens = 16u; /* tiny tail so turn 1 is compactable */
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
    SESSION_CHECK(pCompaction && strstr(xllmCompactionPrompt(pCompaction), "[User]: Implement the requested project.") != NULL,
        "Pi serialization tags the candidate");
    SESSION_CHECK(pCompaction && xllmCompactionEvaluateSummary(pCompaction,
        "## Goal\ntoo little continuity.", &tQuality, &tError) &&
        !tQuality.bAccepted && tQuality.uMissingSections != 0u,
        "quality report rejects an incomplete compaction summary");
    SESSION_CHECK(pCompaction && xllmSessionCommitCompaction(pSession, pCompaction,
        test_pi_summary("Implement the requested project and preserve tool continuity."), &tError),
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
    SESSION_CHECK(pSession && xllmSessionGetConfig(pSession, &tEffective) && tEffective.uKeepRecentTokens == 20000u,
        "default keep-recent is the Pi 20k budget");
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
    const char* sSummary =
        "## Goal\nContinue the code-agent implementation.\n"
        "## Constraints & Preferences\nPreserve the pinned contract, exact paths, and completed tool outcomes.\n"
        "## Progress\nRounds 1-4 inspected modules and preserved tool outcomes.\n"
        "## Key Decisions\nCompact only a safe completed prefix and retain recent turns verbatim.\n"
        "## Next Steps\nContinue from the retained messages (rounds 5-6).\n"
        "## Critical Context\nRecent rounds 5-6 remain verbatim; read-files: module_1.c; modified-files: none.";
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
    SESSION_CHECK(tBefore.ePressure == XLLM_SESSION_PRESSURE_PRUNE || tBefore.ePressure == XLLM_SESSION_PRESSURE_COMPACT, "large history activates offline pressure policy");
    SESSION_CHECK(tBefore.uRenderedActiveTokens < tBefore.uRawActiveTokens, "old large tool outputs are soft-pruned");

    xllmRequestInit(&tRequest);
    SESSION_CHECK(xllmSessionBuildRequest(pSession, &tRequest, &tError), "active request renders before compaction");
    SESSION_CHECK(tRequest.uMaxOutputTokens == 300u, "session output budget propagates to model request");
    SESSION_CHECK(request_has_text(&tRequest, "older tool output pruned"), "rendered request marks pruned tool output");
    SESSION_CHECK(tRequest.iMessageCount == 19u, "soft pruning preserves message and tool-call structure");
    xllmRequestUnit(&tRequest);

    pCompaction = xllmSessionPrepareCompaction(pSession, true, &tError);
    SESSION_CHECK(pCompaction != NULL, "forced compaction transaction prepares");
    SESSION_CHECK(pCompaction && strstr(xllmCompactionPrompt(pCompaction), "## Goal") != NULL, "compaction prompt encodes the Pi summary contract");
    SESSION_CHECK(pCompaction && strstr(xllmCompactionPrompt(pCompaction), "[Tool result call_1") != NULL, "candidate serialization tags tool results");
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
    SESSION_CHECK(pLoaded && xllmSessionGetStats(pLoaded, &tLoaded) && !tLoaded.bFillExactValid, "restored session reports unknown fill until the next call");
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
                request_has_text(&tRequest, "Rounds 1-4 inspected modules"),
                "fork request retains summary and isolates parent state");
            xllmRequestUnit(&tRequest);
        }
        xllmSessionDestroy(pFork);
        uint64_t uTurn = xllmSessionBeginTurn(pLoaded);
        SESSION_CHECK(xllmSessionAddText(pLoaded, uTurn, XLLM_ROLE_USER, "Continue after process restart.", 0u), "loaded session accepts a continuation turn");
        xllmRequestInit(&tRequest);
        SESSION_CHECK(xllmSessionBuildRequest(pLoaded, &tRequest, &tError), "loaded session renders continuation request");
        SESSION_CHECK(request_has_text(&tRequest, "Rounds 1-4 inspected modules") && request_has_text(&tRequest, "Continue after process restart"), "summary continuity survives restart");
        xllmRequestUnit(&tRequest);
        {
            /* Standalone fixture: the invariant under test is that an
             * unresolved tool-call turn never becomes a candidate. */
            xllm_session* pPending;
            xllm_tool_call tPendingCall;
            xllm_response tPendingResponse;
            xllm_compaction* pPendingCompaction;
            uint64_t uPendingTurn;
            tConfig.uKeepRecentTokens = 16u;
            pPending = xllmSessionCreate(&tConfig, &tError);
            tConfig.uKeepRecentTokens = 0u;
            SESSION_CHECK(pPending != NULL, "pending fixture session creates");
            if ( pPending ) {
                (void)xllmSessionAddText(pPending, 0u, XLLM_ROLE_SYSTEM, "pinned contract.", XLLM_SESSION_ENTRY_PINNED);
                uPendingTurn = xllmSessionBeginTurn(pPending);
                SESSION_CHECK(xllmSessionAddText(pPending, uPendingTurn, XLLM_ROLE_USER, "Start the objective.", 0u) &&
                    xllmSessionAddText(pPending, uPendingTurn, XLLM_ROLE_ASSISTANT, "objective captured", 0u),
                    "completed turn precedes the pending turn");
                memset(&tPendingCall, 0, sizeof(tPendingCall));
                tPendingCall.sId = "pending_call";
                tPendingCall.sName = "write_file";
                tPendingCall.sArgumentsJson = "{\"path\":\"pending.c\"}";
                memset(&tPendingResponse, 0, sizeof(tPendingResponse));
                tPendingResponse.sContent = "";
                tPendingResponse.pToolCalls = &tPendingCall;
                tPendingResponse.iToolCallCount = 1u;
                uPendingTurn = xllmSessionBeginTurn(pPending);
                SESSION_CHECK(xllmSessionAddAssistantResponse(pPending, uPendingTurn, &tPendingResponse), "pending assistant tool call added");
                SESSION_CHECK(xllmSessionGetStats(pPending, &tLoaded) && tLoaded.uPendingToolCalls == 1u, "pending tool call tracked");
                pPendingCompaction = xllmSessionPrepareCompaction(pPending, true, &tError);
                SESSION_CHECK(pPendingCompaction != NULL &&
                    strstr(xllmCompactionPrompt(pPendingCompaction), "pending_call") == NULL &&
                    strstr(xllmCompactionPrompt(pPendingCompaction), "[User]: Start the objective.") != NULL,
                    "compaction excludes unresolved tool-call turn");
                xllmCompactionDestroy(pPendingCompaction);
                xllmSessionDestroy(pPending);
            }
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
    const char* sSummary =
        "## Goal\nPreserve a crash-safe coding session.\n"
        "## Constraints & Preferences\nJournal acknowledged mutations before exposing them to callers.\n"
        "## Progress\nThe initial journaled turns were recovered.\n"
        "## Key Decisions\nRecover from the snapshot plus a valid journal prefix.\n"
        "## Next Steps\nContinue from the recovered checkpoint.\n"
        "## Critical Context\nThe latest branch remains verbatim; torn tails are ignored, corrupt records rejected.";
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
    tConfig.uKeepRecentTokens = 16u;
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
        (xbytesview){ (const uint8*)"\"} torn", 8u }) > 0 || true, "test simulates a torn final journal record");
    SESSION_CHECK(xrtFileAppend(sJournalPath,
        (xbytesview){ (const uint8*)"{\"partial\":", 11u }),
        "torn tail bytes appended");
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

static bool test_split_plan_override(xllm_session*, uint64_t, xllm_compaction_plan*, void*);


/* ------------------------------------------------------------------ */
/* v3: split-turn compaction (GAP-SPLIT-TURN)                          */
/* ------------------------------------------------------------------ */

static void test_split_turn(void)
{
    xllm_session_config tConfig;
    xllm_session* pSession;
    xllm_compaction* pCompaction = NULL;
    xllm_session_stats tStats;
    xllm_request tRequest;
    xllm_error tError;
    char* sFatEarly = make_large_output(4000u, 1u);
    char* sFatLate = make_large_output(4000u, 2u);
    uint64_t uTurn;
    uint64_t uPrefixThrough = 0u;

    xllmSessionConfigInit(&tConfig);
    tConfig.uContextWindowTokens = 4000u;
    tConfig.uMaxOutputTokens = 512u;
    tConfig.uOutputReserveTokens = 200u;
    tConfig.uSafetyReserveTokens = 200u;
    tConfig.uKeepRecentTokens = 800u;
    tConfig.uToolPruneBytes = 1024u * 1024u; /* keep the fixture verbatim */
    pSession = xllmSessionCreate(&tConfig, &tError);
    SESSION_CHECK(pSession != NULL, "split-turn session creates");
    if ( !pSession || !sFatEarly || !sFatLate ) { free(sFatEarly); free(sFatLate); return; }

    (void)xllmSessionAddText(pSession, 0u, XLLM_ROLE_SYSTEM, "pinned contract.", XLLM_SESSION_ENTRY_PINNED);
    uTurn = xllmSessionBeginTurn(pSession);
    SESSION_CHECK(xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, "compile the module now", 0u) &&
        xllmSessionAddText(pSession, uTurn, XLLM_ROLE_ASSISTANT, sFatEarly, 0u),
        "split fixture: original request and early fat work");
    {
        xllm_tool_call tCall;
        xllm_response tResponse;
        memset(&tCall, 0, sizeof(tCall));
        tCall.sId = "split_c1";
        tCall.sName = "run_build";
        tCall.sArgumentsJson = "{\"target\":\"module\"}";
        memset(&tResponse, 0, sizeof(tResponse));
        tResponse.sContent = "";
        tResponse.pToolCalls = &tCall;
        tResponse.iToolCallCount = 1u;
        SESSION_CHECK(xllmSessionAddAssistantResponse(pSession, uTurn, &tResponse) &&
            xllmSessionAddToolResult(pSession, uTurn, "split_c1", "build ok"),
            "split fixture: complete tool pair inside the fat turn");
    }
    SESSION_CHECK(xllmSessionAddText(pSession, uTurn, XLLM_ROLE_ASSISTANT, sFatLate, 0u),
        "split fixture: late fat work stays in the suffix");
    uTurn = xllmSessionBeginTurn(pSession);
    SESSION_CHECK(xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, "summarize the build", 0u) &&
        xllmSessionAddText(pSession, uTurn, XLLM_ROLE_ASSISTANT, "the build succeeded", 0u),
        "split fixture: small recent turn");

    pCompaction = xllmSessionPrepareCompaction(pSession, true, &tError);
    SESSION_CHECK(pCompaction != NULL, "split-turn compaction prepares");
    if ( pCompaction ) {
        const char* sPrompt = xllmCompactionPrompt(pCompaction);
        uPrefixThrough = xllmCompactionThroughSequence(pCompaction);
        SESSION_CHECK(sPrompt != NULL &&
            strstr(sPrompt, "PREFIX of a turn that was too large to keep") != NULL &&
            strstr(sPrompt, "## Original Request") != NULL &&
            strstr(sPrompt, "compile the module now") != NULL &&
            strstr(sPrompt, "## Early Progress") != NULL &&
            strstr(sPrompt, "## Context for Suffix") != NULL,
            "split prompt carries the Pi turn-context template and the request");
        SESSION_CHECK(strstr(sPrompt, sFatLate) == NULL,
            "suffix fat content stays out of the candidates");
        SESSION_CHECK(uPrefixThrough > 0u, "committed boundary covers the prefix");
        SESSION_CHECK(xllmSessionCommitCompaction(pSession, pCompaction,
            "## Goal\ncompile the module\n"
            "## Constraints & Preferences\npinned contract\n"
            "## Progress\nearly work summarized; build ok\n"
            "## Key Decisions\nsplit the oversized turn\n"
            "## Next Steps\ncontinue from the late work\n"
            "## Critical Context\ntool pair preserved; read-files: none", &tError),
            "split-turn compaction commits");
        xllmCompactionDestroy(pCompaction);
        pCompaction = NULL;
    }
    SESSION_CHECK(xllmSessionGetStats(pSession, &tStats) &&
        tStats.uCompactionCount == 1u && tStats.uCompactedThroughSequence == uPrefixThrough,
        "split compaction advances through the prefix (not a truncate)");
    xllmRequestInit(&tRequest);
    SESSION_CHECK(xllmSessionBuildRequest(pSession, &tRequest, &tError) &&
        request_has_text(&tRequest, "Compacted session state") &&
        request_has_text(&tRequest, sFatLate) &&
        strstr(tRequest.pMessages[2].sContent ? "" : "", "x") == NULL &&
        tRequest.pMessages[2].eRole == XLLM_ROLE_ASSISTANT,
        "rendered suffix keeps the pair and starts at an assistant entry");
    SESSION_CHECK(xllmSessionPendingToolCallCount(pSession) == 0u,
        "split cut never breaks the tool pair");
    for ( size_t i = 0u; i < tRequest.iMessageCount; ++i ) {
        SESSION_CHECK(!(tRequest.pMessages[i].sContent &&
            strstr(tRequest.pMessages[i].sContent, "truncated by overflow")),
            "no L2 truncation marker: the summary path won");
        if ( tRequest.pMessages[i].sContent &&
             strstr(tRequest.pMessages[i].sContent, "truncated by overflow") ) { break; }
    }
    xllmRequestUnit(&tRequest);

    /* custom ops can replace the planning behavior (split plans included) */
    {
        xllm_compaction_ops tOps;
        memset(&tOps, 0, sizeof(tOps));
        tOps.pPlan = test_split_plan_override;
        SESSION_CHECK(xllmSessionSetCompactionOps(pSession, &tOps), "custom split ops installs");
        pCompaction = xllmSessionPrepareCompaction(pSession, true, &tError);
        SESSION_CHECK(pCompaction != NULL &&
            strstr(xllmCompactionPrompt(pCompaction), "Turn Context (split turn):") != NULL,
            "driver honors a custom split plan");
        xllmCompactionDestroy(pCompaction);
        (void)xllmSessionSetCompactionOps(pSession, NULL);
    }
    free(sFatEarly);
    free(sFatLate);
    xllmSessionDestroy(pSession);
}

static bool test_split_plan_override(xllm_session* pSession, uint64_t uPrevThrough,
    xllm_compaction_plan* pPlan, void* pUserData)
{
    size_t i;
    (void)pUserData;
    pPlan->uThroughSequence = uPrevThrough;
    pPlan->uPrefixThroughSequence = 0u;
    /* force a fresh split over whatever active prefix exists */
    for ( i = 0u; i < pSession->iEntryCount; ++i ) {
        const xllm_session_entry* pEntry = &pSession->pEntries[i];
        if ( (pEntry->uFlags & XLLM_SESSION_ENTRY_PINNED) != 0u ) { continue; }
        if ( pEntry->uSequence <= uPrevThrough ) { continue; }
        if ( pEntry->tMessage.eRole == XLLM_ROLE_ASSISTANT &&
             pEntry->uSequence > pPlan->uPrefixThroughSequence ) {
            pPlan->uPrefixThroughSequence = pEntry->uSequence;
        }
    }
    return pPlan->uPrefixThroughSequence > uPrevThrough;
}

/* ------------------------------------------------------------------ */
/* v3: exact-feedback governance                                        */
/* ------------------------------------------------------------------ */

static void test_exact_governance(void)
{
    xllm_session_config tConfig;
    xllm_session* pSession;
    xllm_session* pFork = NULL;
    xllm_session_stats tStats;
    xllm_usage tUsage;
    xllm_error tError;
    xllmSessionConfigInit(&tConfig);
    tConfig.uContextWindowTokens = 8000u;
    tConfig.uMaxOutputTokens = 1000u;
    tConfig.uOutputReserveTokens = 1000u;
    tConfig.uSafetyReserveTokens = 500u;
    pSession = xllmSessionCreate(&tConfig, &tError);
    SESSION_CHECK(pSession != NULL, "governance session creates");
    if ( !pSession ) { return; }
    SESSION_CHECK(xllmSessionGetStats(pSession, &tStats) && !tStats.bFillExactValid &&
        tStats.uFillExact == UINT64_MAX && tStats.ePressure == XLLM_SESSION_PRESSURE_NONE,
        "fresh session reports unknown fill and no pressure");
    memset(&tUsage, 0, sizeof(tUsage));
    tUsage.uInputTokens = 100u;
    tUsage.uOutputTokens = 50u;
    tUsage.uCachedInputTokens = 60u;
    SESSION_CHECK(xllmSessionRecordUsage(pSession, &tUsage), "usage feedback accepted");
    SESSION_CHECK(xllmSessionGetStats(pSession, &tStats) && tStats.bFillExactValid &&
        tStats.uFillExact == 150u && tStats.uCachedInputTokens == 60u &&
        tStats.uIncrementMax >= 256u && tStats.ePressure == XLLM_SESSION_PRESSURE_NONE,
        "exact fill recorded with a bounded increment envelope");
    /* budget = 8000 - 1000 - 500 = 6500; soft = 0.95 * 6500 = 6175 */
    tUsage.uInputTokens = 6000u;
    tUsage.uOutputTokens = 10u;
    SESSION_CHECK(xllmSessionRecordUsage(pSession, &tUsage) &&
        xllmSessionGetStats(pSession, &tStats) &&
        tStats.ePressure == XLLM_SESSION_PRESSURE_COMPACT,
        "soft threshold crosses into compact pressure");
    tUsage.uInputTokens = 6400u;
    tUsage.uOutputTokens = 100u;
    SESSION_CHECK(xllmSessionRecordUsage(pSession, &tUsage) &&
        xllmSessionGetStats(pSession, &tStats) &&
        tStats.ePressure == XLLM_SESSION_PRESSURE_OVERFLOW,
        "fill plus increment beyond the input budget is overflow");
    tUsage.uInputTokens = 100u;
    tUsage.uOutputTokens = 50u;
    SESSION_CHECK(xllmSessionRecordUsage(pSession, &tUsage), "usage back to sparse");
    pFork = xllmSessionFork(pSession, &tError);
    SESSION_CHECK(pFork && xllmSessionGetStats(pFork, &tStats) && !tStats.bFillExactValid,
        "fork invalidates the exact fill");
    xllmSessionDestroy(pFork);
    xllmSessionDestroy(pSession);
}

/* ------------------------------------------------------------------ */
/* v3: ops table and render/event hooks                                 */
/* ------------------------------------------------------------------ */

typedef struct {
    xllm_session_event_type aEvents[64];
    size_t iEvents;
    bool bReenter;
} test_event_sink;

static void test_on_event(xllm_session* pSession, const xllm_session_event* pEvent, void* pUserData)
{
    test_event_sink* pSink = (test_event_sink*)pUserData;
    (void)pSession;
    if ( pSink->iEvents < 64u ) { pSink->aEvents[pSink->iEvents++] = pEvent->eType; }
}

static bool test_has_event(const test_event_sink* pSink, xllm_session_event_type eType)
{
    size_t i;
    for ( i = 0u; i < pSink->iEvents; ++i ) {
        if ( pSink->aEvents[i] == eType ) { return true; }
    }
    return false;
}

static xllm_render_action test_skip_users(xllm_session* pSession, uint64_t uSequence,
    uint64_t uTurn, uint32_t uFlags, xllm_message* pWork, void* pUserData)
{
    (void)pSession; (void)uSequence; (void)uTurn; (void)pWork; (void)pUserData;
    return (uFlags & XLLM_SESSION_ENTRY_PINNED) == 0u && pWork->eRole == XLLM_ROLE_USER
        ? XLLM_RENDER_SKIP : XLLM_RENDER_KEEP;
}

static xllm_render_action test_skip_assistant(xllm_session* pSession, uint64_t uSequence,
    uint64_t uTurn, uint32_t uFlags, xllm_message* pWork, void* pUserData)
{
    (void)pSession; (void)uSequence; (void)uTurn; (void)uFlags; (void)pUserData;
    return pWork->eRole == XLLM_ROLE_ASSISTANT && pWork->iToolCallCount > 0u
        ? XLLM_RENDER_SKIP : XLLM_RENDER_KEEP;
}

static bool test_summary_as_system(xllm_session* pSession, const xllm_session_summary* pSummary,
    xllm_message* pWork, void* pUserData)
{
    (void)pSession; (void)pSummary; (void)pUserData;
    if ( pWork->eRole != XLLM_ROLE_USER ) { return true; }
    pWork->eRole = XLLM_ROLE_SYSTEM;
    return xllmMessageSetContent(pWork, "[summary]");
}

static bool test_append_marker(xllm_session* pSession, xllm_request* pRequest, void* pUserData)
{
    (void)pSession; (void)pUserData;
    return xllmRequestAddTextMessage(pRequest, XLLM_ROLE_USER, "ephemeral tail context");
}

static bool test_serialize_wrapped(xllm_session* pSession, uint64_t uFrom, uint64_t uTo,
    char** psText, void* pUserData)
{
    char* sBase;
    xllm_session_buf tBuf = {0};
    bool bOk;
    (void)pUserData;
    sBase = xllm_session__serialize_candidates(pSession, uFrom, uTo);
    if ( !sBase ) { return false; }
    bOk = xllm_session__buf_cstr(&tBuf, "[[[\n") &&
        xllm_session__buf_cstr(&tBuf, sBase) &&
        xllm_session__buf_cstr(&tBuf, "]]]\n");
    free(sBase);
    *psText = bOk ? xllm_session__buf_detach(&tBuf) : NULL;
    xllm_session__buf_unit(&tBuf);
    return *psText != NULL;
}

static void test_ops_and_hooks(void)
{
    xllm_session_config tConfig;
    xllm_session* pSession;
    xllm_compaction* pA;
    xllm_compaction* pB;
    xllm_request tRequest;
    xllm_error tError;
    xllm_compaction_ops tOps;
    xllm_session_hooks tHooks;
    test_event_sink tSink = {0};
    xllm_tool_call tCall;
    xllm_response tResponse;
    uint64_t uTurn;

    xllmSessionConfigInit(&tConfig);
    tConfig.uContextWindowTokens = 8000u;
    tConfig.uMaxOutputTokens = 1000u;
    tConfig.uOutputReserveTokens = 1000u;
    tConfig.uSafetyReserveTokens = 500u;
    tConfig.uKeepRecentTokens = 16u;
    pSession = xllmSessionCreate(&tConfig, &tError);
    SESSION_CHECK(pSession != NULL, "hooks session creates");
    if ( !pSession ) { return; }
    uTurn = xllmSessionBeginTurn(pSession);
    (void)xllmSessionAddText(pSession, uTurn, XLLM_ROLE_SYSTEM, "pinned", XLLM_SESSION_ENTRY_PINNED);
    uTurn = xllmSessionBeginTurn(pSession);
    (void)xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, "old user drop me", 0u);
    uTurn = xllmSessionBeginTurn(pSession);
    memset(&tCall, 0, sizeof(tCall));
    tCall.sId = "pair_call";
    tCall.sName = "tool_x";
    tCall.sArgumentsJson = "{}";
    memset(&tResponse, 0, sizeof(tResponse));
    tResponse.sContent = "";
    tResponse.pToolCalls = &tCall;
    tResponse.iToolCallCount = 1u;
    (void)xllmSessionAddAssistantResponse(pSession, uTurn, &tResponse);
    (void)xllmSessionAddToolResult(pSession, uTurn, "pair_call", "pair result");

    /* default-equivalence: NULL ops and the default table produce identical prompts */
    pA = xllmSessionPrepareCompaction(pSession, true, &tError);
    SESSION_CHECK(xllmSessionSetCompactionOps(pSession, xllmSessionDefaultCompactionOps()),
        "default ops table installs");
    pB = xllmSessionPrepareCompaction(pSession, true, &tError);
    SESSION_CHECK(pA && pB && strcmp(xllmCompactionPrompt(pA), xllmCompactionPrompt(pB)) == 0,
        "NULL ops and the default table render byte-identical prompts");
    xllmCompactionDestroy(pA);
    xllmCompactionDestroy(pB);

    /* stage-level override: only serialization changes */
    memset(&tOps, 0, sizeof(tOps));
    tOps.pSerialize = test_serialize_wrapped;
    SESSION_CHECK(xllmSessionSetCompactionOps(pSession, &tOps), "custom ops installs");
    pA = xllmSessionPrepareCompaction(pSession, true, &tError);
    SESSION_CHECK(pA && strstr(xllmCompactionPrompt(pA), "[[[\n[User]:") != NULL,
        "custom serialize stage replaces only that stage");
    xllmCompactionDestroy(pA);
    SESSION_CHECK(xllmSessionSetCompactionOps(pSession, NULL), "ops reset to default");

    /* render hooks: SKIP a plain user entry, then verify pairing protection */
    memset(&tHooks, 0, sizeof(tHooks));
    tHooks.pRenderMessage = test_skip_users;
    SESSION_CHECK(xllmSessionSetHooks(pSession, &tHooks), "render hooks install");
    xllmRequestInit(&tRequest);
    SESSION_CHECK(xllmSessionBuildRequest(pSession, &tRequest, &tError) &&
        !request_has_text(&tRequest, "old user drop me") &&
        request_has_text(&tRequest, "pair result"),
        "SKIP removes a user entry from the render");
    xllmRequestUnit(&tRequest);
    tHooks.pRenderMessage = test_skip_assistant;
    xllmRequestInit(&tRequest);
    SESSION_CHECK(!xllmSessionBuildRequest(pSession, &tRequest, &tError) &&
        tError.eCode == XLLM_ERROR_PROTOCOL,
        "SKIP breaking a tool pair fails the render");
    xllmRequestUnit(&tRequest);
    xrtClearError();

    /* summary hook: switch the bridge to a system message; completion hook appends */
    pA = xllmSessionPrepareCompaction(pSession, true, &tError);
    SESSION_CHECK(pA && xllmSessionCommitCompaction(pSession, pA, test_pi_summary("hook coverage"), &tError),
        "compaction for the summary hook commits");
    xllmCompactionDestroy(pA);
    pA = NULL;
    tHooks.pRenderMessage = NULL;
    tHooks.pRenderSummary = test_summary_as_system;
    tHooks.pRenderComplete = test_append_marker;
    xllmRequestInit(&tRequest);
    SESSION_CHECK(xllmSessionBuildRequest(pSession, &tRequest, &tError) &&
        tRequest.iMessageCount >= 2u &&
        tRequest.pMessages[1].eRole == XLLM_ROLE_SYSTEM &&
        strcmp(tRequest.pMessages[1].sContent, "[summary]") == 0 &&
        request_has_text(&tRequest, "ephemeral tail context"),
        "summary and completion hooks reshape the request");
    xllmRequestUnit(&tRequest);

    /* event stream: mutations emit the documented lifecycle */
    tHooks.pRenderSummary = NULL;
    tHooks.pRenderComplete = NULL;
    tHooks.pOnEvent = test_on_event;
    tHooks.pUserData = &tSink;
    uTurn = xllmSessionBeginTurn(pSession);
    (void)xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, "event probe", 0u);
    SESSION_CHECK(test_has_event(&tSink, XLLM_SESSION_EVENT_TURN_BEGIN) &&
        test_has_event(&tSink, XLLM_SESSION_EVENT_ENTRY_ADDED) &&
        test_has_event(&tSink, XLLM_SESSION_EVENT_JOURNAL_RECORD) == false &&
        tSink.iEvents >= 2u,
        "turn and entry events fire without a journal");
    SESSION_CHECK(xllmSessionSetHooks(pSession, NULL), "hooks removed");
    xllmSessionDestroy(pSession);
}

/* ------------------------------------------------------------------ */
/* v3: overflow ladder and the compaction loop guard                    */
/* ------------------------------------------------------------------ */

static void test_ladder_and_guard(void)
{
    xllm_session_config tConfig;
    xllm_session* pSession;
    xllm_session_stats tStats;
    xllm_request tRequest;
    xllm_error tError;
    uint64_t uTurn;
    unsigned i;

    /* L2 truncation: several medium turns, a window that cannot compact
     * (single-shot summaries unavailable without a client) must truncate. */
    xllmSessionConfigInit(&tConfig);
    tConfig.uContextWindowTokens = 1000u;
    tConfig.uMaxOutputTokens = 100u;
    tConfig.uOutputReserveTokens = 100u;
    tConfig.uSafetyReserveTokens = 50u;
    tConfig.fPruneTrigger = 0.10;
    tConfig.fCompactTrigger = 0.20;
    pSession = xllmSessionCreate(&tConfig, &tError);
    SESSION_CHECK(pSession != NULL, "ladder session creates");
    if ( pSession ) {
        for ( i = 0u; i < 6u; ++i ) {
            char sText[600];
            memset(sText, 'x', sizeof(sText) - 1u);
            sText[sizeof(sText) - 1u] = '\0';
            uTurn = xllmSessionBeginTurn(pSession);
            (void)xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, sText, 0u);
            (void)xllmSessionAddText(pSession, uTurn, XLLM_ROLE_ASSISTANT, "done", 0u);
        }
        SESSION_CHECK(xllmSessionGetStats(pSession, &tStats) &&
            tStats.ePressure == XLLM_SESSION_PRESSURE_OVERFLOW,
            "offline estimate ladder reports overflow");
        SESSION_CHECK(xllmSessionOverflowLadder(pSession, &tError),
            "overflow ladder truncates the tail");
        SESSION_CHECK(xllmSessionGetStats(pSession, &tStats) &&
            tStats.uSummaryGeneration == 1u && tStats.ePressure != XLLM_SESSION_PRESSURE_OVERFLOW,
            "L2 truncation advances the generation and relieves overflow");
        xllmRequestInit(&tRequest);
        SESSION_CHECK(xllmSessionBuildRequest(pSession, &tRequest, &tError) &&
            request_has_text(&tRequest, "truncated by overflow recovery") &&
            tRequest.iMessageCount <= 4u,
            "rendered tail carries the truncation marker and the newest turns");
        xllmRequestUnit(&tRequest);
        xllmSessionDestroy(pSession);
    }
    xrtClearError();

    /* Loop guard: compactions without an intervening user entry stop after
     * two consecutive rounds. */
    xllmSessionConfigInit(&tConfig);
    tConfig.uContextWindowTokens = 8000u;
    tConfig.uMaxOutputTokens = 1000u;
    tConfig.uOutputReserveTokens = 1000u;
    tConfig.uSafetyReserveTokens = 500u;
    pSession = xllmSessionCreate(&tConfig, &tError);
    SESSION_CHECK(pSession != NULL, "guard session creates");
    if ( pSession ) {
        xllm_usage tUsage;
        memset(&tUsage, 0, sizeof(tUsage));
        tUsage.uInputTokens = 6100u;
        tUsage.uOutputTokens = 100u;
        uTurn = xllmSessionBeginTurn(pSession);
        (void)xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, "guard turn", 0u);
        SESSION_CHECK(xllmSessionRecordUsage(pSession, &tUsage) &&
            xllmSessionGetStats(pSession, &tStats) &&
            tStats.ePressure >= XLLM_SESSION_PRESSURE_COMPACT,
            "guard fixture raises pressure through exact feedback");
        pSession->uAutoCompactStreak = 2u; /* simulate two prior auto rounds */
        SESSION_CHECK(!xllmSessionMaybeCompact(pSession, NULL, &tError) &&
            tError.eCode == XLLM_ERROR_LIMIT,
            "loop guard refuses the third consecutive auto compaction");
        xrtClearError();
        xllmSessionDestroy(pSession);
    }
    (void)i;
}

/* ------------------------------------------------------------------ */
/* v3: easy layer (bound/test sessions, auto compaction)                */
/* ------------------------------------------------------------------ */

typedef struct {
    const char* sContent;      /* reply for normal calls */
    uint64_t uIn, uOut;
    unsigned iCalls;
} test_script;
static char g_sMetaRoutingKey[64];
static size_t g_iMetaHeaderCount;

static xllm_result test_script_call(void* pUserData, const xllm_request* pRequest,
    const xllm_stream_callbacks* pCallbacks, xllm_response** ppResponse, xllm_error* pError)
{
    test_script* pScript = (test_script*)pUserData;
    size_t i;
    bool bSummaryCall = false;
    (void)pCallbacks;
    for ( i = 0u; i < pRequest->iMessageCount; ++i ) {
        if ( pRequest->pMessages[i].sContent &&
             strstr(pRequest->pMessages[i].sContent, "<conversation>") ) { bSummaryCall = true; }
    }
    ++pScript->iCalls;
    if ( bSummaryCall && pRequest->iExtraHeaderCount ) {
        g_iMetaHeaderCount = pRequest->iExtraHeaderCount;
        if ( pRequest->pExtraHeaders[0].sValue ) {
            (void)snprintf(g_sMetaRoutingKey, sizeof(g_sMetaRoutingKey), "%s",
                pRequest->pExtraHeaders[0].sValue);
        }
    }
    if ( pError ) { xllmErrorInit(pError); }
    *ppResponse = test_make_response(
        bSummaryCall ? test_pi_summary("scripted auto compaction") : pScript->sContent,
        pScript->uIn, pScript->uOut);
    return *ppResponse ? XLLM_RESULT_OK : XLLM_RESULT_ERROR;
}

static void test_easy_send(void)
{
    xllm_session_config tConfig;
    xllm_session* pSession;
    xllm_session_stats tStats;
    xllm_response* pResponse = NULL;
    xllm_error tError;
    test_script tScript = { "model reply", 5000u, 500u, 0u };
    bool bCompact = false;
    uint64_t uTurn;

    xllmSessionConfigInit(&tConfig);
    tConfig.uContextWindowTokens = 8000u;
    tConfig.uMaxOutputTokens = 1000u;
    tConfig.uOutputReserveTokens = 1000u;
    tConfig.uSafetyReserveTokens = 500u;
    tConfig.uKeepRecentTokens = 16u;
    pSession = xllmSessionCreateForTest(&tConfig, test_script_call, &tScript, &tError);
    SESSION_CHECK(pSession != NULL, "test-bound session creates");
    if ( !pSession ) { return; }

    /* First turn: normal reply, usage recorded, no compaction due. */
    SESSION_CHECK(xllmSessionSend(pSession, "hello", NULL, &pResponse, &tError) == XLLM_RESULT_OK &&
        pResponse && strcmp(pResponse->sContent, "model reply") == 0,
        "Send returns the scripted reply");
    xllmResponseDestroy(pResponse);
    pResponse = NULL;
    SESSION_CHECK(xllmSessionGetStats(pSession, &tStats) && tStats.bFillExactValid &&
        tStats.uFillExact == 5500u && tStats.uCurrentTurn == 1u && tStats.uEntryCount == 2u,
        "Send records the turn and the exact usage");
    SESSION_CHECK(xllmSessionMaybeCompact(pSession, &bCompact, &tError) && !bCompact,
        "below threshold: no compaction due (5500 + inc < 6175)");

    /* Second turn: usage crosses overflow; the next MaybeCompact runs the
     * full ops pipeline through the scripted meta call. */
    tScript.uIn = 6100u;
    tScript.uOut = 100u;
    uTurn = xllmSessionBeginTurn(pSession);
    (void)xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, "second turn", 0u);
    pResponse = test_make_response("ack", 6100u, 100u);
    (void)xllmSessionAddAssistantResponse(pSession, uTurn, pResponse);
    xllmResponseDestroy(pResponse);
    SESSION_CHECK(xllmSessionGetStats(pSession, &tStats) &&
        tStats.ePressure >= XLLM_SESSION_PRESSURE_COMPACT,
        "scripted usage raises the pressure");
    SESSION_CHECK(xllmSessionMaybeCompact(pSession, &bCompact, &tError) && bCompact,
        "auto compaction runs through the scripted meta call");
    SESSION_CHECK(xllmSessionGetStats(pSession, &tStats) && tStats.uCompactionCount == 1u &&
        tStats.uSummaryGeneration == 1u && tStats.uSummaryTokensExact == 100u,
        "compaction commits with exact summary accounting");
    SESSION_CHECK(g_iMetaHeaderCount == 1u && strlen(g_sMetaRoutingKey) == 36u &&
        g_sMetaRoutingKey[8] == '-' && g_sMetaRoutingKey[13] == '-' &&
        g_sMetaRoutingKey[18] == '-' && g_sMetaRoutingKey[23] == '-',
        "meta call carries a fresh UUID routing key header");
    {
        xllm_request tRequest;
        xllmRequestInit(&tRequest);
        SESSION_CHECK(xllmSessionBuildRequest(pSession, &tRequest, &tError) &&
            request_has_text(&tRequest, "scripted auto compaction"),
            "rendered request carries the rolled summary");
        xllmRequestUnit(&tRequest);
    }
    xllmSessionDestroy(pSession);
}

int main(void)
{
    printf("xllm-session v3 tests\n");
    test_default_profile();
    test_compaction_user_bridge();
    test_budget_compaction_persistence();
    test_journal_checkpoint_recovery();
    test_split_turn();
    test_exact_governance();
    test_ops_and_hooks();
    test_ladder_and_guard();
    test_easy_send();
    printf("xllm-session v3: %s (%d failures)\n", g_iSessionFailures ? "FAIL" : "PASS", g_iSessionFailures);
    return g_iSessionFailures ? 1 : 0;
}
