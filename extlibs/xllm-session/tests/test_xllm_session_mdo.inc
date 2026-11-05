static int g_iSessionFailures = 0;

static uint64_t test_path_size(const char *sPath)
{
    xfileinfo tInfo;
    return xrtPathStat(sPath, true, &tInfo) ? tInfo.Size : 0u;
}

#define SESSION_CHECK(expr, name)                                                                  \
    do                                                                                             \
    {                                                                                              \
        bool xllm_session_ok__ = !!(expr);                                                         \
        printf("  %-56s %s\n", (name), xllm_session_ok__ ? "PASS" : "FAIL");                       \
        if (!xllm_session_ok__)                                                                    \
        {                                                                                          \
            ++g_iSessionFailures;                                                                  \
        }                                                                                          \
    } while (0)

static char *make_large_output(size_t iSize, unsigned iSeed)
{
    char *sText = (char *)malloc(iSize + 1u);
    size_t i;
    if (!sText)
    {
        return NULL;
    }
    for (i = 0u; i < iSize; ++i)
    {
        sText[i] = (char)('a' + (int)((i + iSeed) % 26u));
    }
    sText[iSize] = '\0';
    return sText;
}

static bool add_tool_round(xllm_session *pSession, uint64_t uTurn, unsigned iRound,
                           const char *sOutput)
{
    char sUser[128];
    char sCallId[64];
    char sArguments[128];
    xllm_tool_call tCall;
    xllm_response tResponse;
    (void)snprintf(sUser, sizeof(sUser),
                   "Round %u: inspect module and continue the implementation.", iRound);
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

static bool request_has_text(const xllm_request *pRequest, const char *sNeedle)
{
    size_t i;
    for (i = 0u; i < pRequest->iMessageCount; ++i)
    {
        const char *sContent = pRequest->pMessages[i].sContent;
        if (sContent && strstr(sContent, sNeedle))
        {
            return true;
        }
    }
    return false;
}

static xllm_response *test_make_response(const char *sContent, uint64_t uIn, uint64_t uOut)
{
    xllm_response *pResponse = (xllm_response *)calloc(1u, sizeof(*pResponse));
    if (!pResponse)
    {
        return NULL;
    }
    pResponse->sContent = xllm_session__strdup(sContent);
    pResponse->tUsage.uInputTokens = uIn;
    pResponse->tUsage.uOutputTokens = uOut;
    pResponse->tUsage.uTotalTokens = uIn + uOut;
    pResponse->eFinish = XLLM_FINISH_STOP;
    if (!pResponse->sContent)
    {
        xllmResponseDestroy(pResponse);
        return NULL;
    }
    return pResponse;
}

static const char *test_pi_summary(const char *sGoal)
{
    static char sBuffer[1024];
    (void)snprintf(
        sBuffer, sizeof(sBuffer),
        "## Goal\n%s\n"
        "## Constraints & Preferences\nPreserve the pinned contract and exact tool pairing.\n"
        "## Progress\nPrior turns and tool outcomes were captured.\n"
        "## Key Decisions\nUse the durable session checkpoint as the history bridge.\n"
        "## Next Steps\nContinue from the retained messages.\n"
        "## Critical Context\nThe retained suffix stays verbatim; read-files: none; "
        "modified-files: none.\n",
        sGoal);
    return sBuffer;
}

static bool test_copy_file(const char *sSource, const char *sTarget)
{
    size_t iSize = 0u;
    uint8 *pData = xrtFileReadAll(sSource, &iSize);
    bool bOk = pData && xrtFileWriteAll(sTarget, (xbytesview){pData, iSize});
    xrtFree(pData);
    return bOk;
}

static bool test_file_equals(const char *sPath, const void *pExpected, size_t iExpected)
{
    size_t iSize = 0u;
    uint8 *pData = xrtFileReadAll(sPath, &iSize);
    bool bEqual =
        pData && iSize == iExpected && (iSize == 0u || memcmp(pData, pExpected, iSize) == 0);
    xrtFree(pData);
    return bEqual;
}

static bool test_is_injected_storage_error(const xllm_error *pError, xllm_error_stage eStage,
                                           const char *sOperation, const char *sPath)
{
    return pError && pError->eCode == XLLM_ERROR_NETWORK &&
           pError->tCause.eDomain == XLLM_ERROR_DOMAIN_RESOURCE &&
           pError->tCause.eStage == eStage && pError->tCause.iRuntimeKind == XERR_IO &&
           pError->tCause.iRuntimeCode >= 10800 && pError->tCause.iSystemCode == 28 &&
           strcmp(pError->tCause.sOperation, sOperation) == 0 &&
           strstr(pError->sMessage, sPath) != NULL && !xllmErrorRetryable(pError);
}

static bool test_plan_through_call(xllm_session *pSession, uint64_t uPrevThrough,
                                   xllm_compaction_plan *pPlan, void *pUserData)
{
    const char *sWanted = (const char *)pUserData;
    size_t i;
    pPlan->uThroughSequence = uPrevThrough;
    pPlan->uPrefixThroughSequence = 0u;
    for (i = 0u; i < pSession->iEntryCount; ++i)
    {
        const xllm_session_entry *pEntry = &pSession->pEntries[i];
        size_t j;
        if (pEntry->tMessage.eRole != XLLM_ROLE_ASSISTANT)
        {
            continue;
        }
        for (j = 0u; j < pEntry->tMessage.iToolCallCount; ++j)
        {
            const char *sId = pEntry->tMessage.pToolCalls[j].sId;
            if (sId && strcmp(sId, sWanted) == 0)
            {
                pPlan->uThroughSequence = pEntry->uSequence;
                return pPlan->uThroughSequence > uPrevThrough;
            }
        }
    }
    return false;
}

static xllm_render_action test_cross_turn_pair_skip(xllm_session *pSession, uint64_t uSequence,
                                                    uint64_t uTurn, uint32_t uFlags,
                                                    xllm_message *pWork, void *pUserData)
{
    (void)pSession;
    (void)uSequence;
    (void)uFlags;
    (void)pUserData;
    if (uTurn == 1u && pWork->eRole == XLLM_ROLE_TOOL)
    {
        return XLLM_RENDER_SKIP;
    }
    if (uTurn == 2u && pWork->eRole == XLLM_ROLE_ASSISTANT && pWork->iToolCallCount != 0u)
    {
        return XLLM_RENDER_SKIP;
    }
    return XLLM_RENDER_KEEP;
}

static void test_tool_pair_invariants(void)
{
    static const char sSnapshot[] = "out/mdo-integration/fixtures/pair_snapshot.json";
    xllm_session_config tConfig;
    xllm_session *pSession;
    xllm_session *pLoaded = NULL;
    xllm_session *pCompact = NULL;
    xllm_session *pRender = NULL;
    xllm_error tError;
    xllm_message tMessage;
    xllm_tool_call aCalls[2];
    xllm_response tResponse;
    xllm_compaction *pCompaction = NULL;
    xllm_compaction_ops tOps;
    xllm_session_hooks tHooks;
    xllm_request tRequest;
    uint64_t uTurn;
    size_t iBefore;

    xllmSessionConfigInit(&tConfig);
    tConfig.uContextWindowTokens = 8000u;
    tConfig.uMaxOutputTokens = 1000u;
    tConfig.uOutputReserveTokens = 1000u;
    tConfig.uSafetyReserveTokens = 500u;
    tConfig.uKeepRecentTokens = 1u;
    pSession = xllmSessionCreate(&tConfig, &tError);
    SESSION_CHECK(pSession != NULL, "pair-invariant session creates");
    if (!pSession)
    {
        return;
    }
    uTurn = xllmSessionBeginTurn(pSession);
    (void)xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, "run two tools", 0u);
    iBefore = pSession->iEntryCount;

    xllmMessageInit(&tMessage, XLLM_ROLE_ASSISTANT);
    (void)xllmMessageAddToolCall(&tMessage, "", "read", "{}");
    SESSION_CHECK(!xllmSessionAddMessage(pSession, uTurn, &tMessage, 0u) &&
                      pSession->iEntryCount == iBefore &&
                      xllmSessionGetState(pSession) == XLLM_SESSION_STATE_USER_APPENDED,
                  "empty tool-call id is rejected atomically");
    xllmMessageUnit(&tMessage);
    memset(aCalls, 0, sizeof(aCalls));
    aCalls[0].sId = "bad-name";
    aCalls[0].sName = "";
    memset(&tMessage, 0, sizeof(tMessage));
    tMessage.eRole = XLLM_ROLE_ASSISTANT;
    tMessage.pToolCalls = aCalls;
    tMessage.iToolCallCount = 1u;
    SESSION_CHECK(!xllmSessionAddMessage(pSession, uTurn, &tMessage, 0u) &&
                      pSession->iEntryCount == iBefore,
                  "empty tool name is rejected atomically");

    xllmMessageInit(&tMessage, XLLM_ROLE_ASSISTANT);
    (void)xllmMessageAddToolCall(&tMessage, "same", "read", "{}");
    (void)xllmMessageAddToolCall(&tMessage, "same", "write", "{}");
    SESSION_CHECK(!xllmSessionAddMessage(pSession, uTurn, &tMessage, 0u) &&
                      pSession->iEntryCount == iBefore,
                  "duplicate ids in one assistant response are rejected");
    xllmMessageUnit(&tMessage);

    memset(aCalls, 0, sizeof(aCalls));
    aCalls[0].sId = "pair-a";
    aCalls[0].sName = "read";
    aCalls[0].sArgumentsJson = "{}";
    aCalls[1].sId = "pair-b";
    aCalls[1].sName = "write";
    aCalls[1].sArgumentsJson = "{}";
    memset(&tResponse, 0, sizeof(tResponse));
    tResponse.pToolCalls = aCalls;
    tResponse.iToolCallCount = 2u;
    SESSION_CHECK(xllmSessionAddAssistantResponse(pSession, uTurn, &tResponse) &&
                      xllmSessionPendingToolCallCount(pSession) == 2u,
                  "distinct calls enter the pending set in ledger order");

    xllmMessageInit(&tMessage, XLLM_ROLE_TOOL);
    (void)xllmMessageSetToolCallId(&tMessage, "unknown");
    (void)xllmMessageSetContent(&tMessage, "bad");
    SESSION_CHECK(!xllmSessionAddMessage(pSession, uTurn, &tMessage, 0u),
                  "generic append rejects an unknown tool result");
    xllmMessageUnit(&tMessage);
    xllmMessageInit(&tMessage, XLLM_ROLE_TOOL);
    (void)xllmMessageSetToolCallId(&tMessage, "pair-a");
    (void)xllmMessageSetContent(&tMessage, "one");
    SESSION_CHECK(xllmSessionAddMessage(pSession, uTurn, &tMessage, 0u) &&
                      xllmSessionGetState(pSession) == XLLM_SESSION_STATE_TOOLS_PARTIAL,
                  "generic append resolves exactly one pending call");
    SESSION_CHECK(!xllmSessionAddMessage(pSession, uTurn, &tMessage, 0u),
                  "generic append rejects a duplicate result");
    xllmMessageUnit(&tMessage);
    xllmMessageInit(&tMessage, XLLM_ROLE_TOOL);
    (void)xllmMessageSetToolCallId(&tMessage, "pair-b");
    (void)xllmMessageSetContent(&tMessage, "two");
    SESSION_CHECK(!xllmSessionAddMessage(pSession, 0u, &tMessage, 0u) &&
                      xllmSessionAddMessage(pSession, uTurn, &tMessage, 0u),
                  "tool result must match the call turn");
    xllmMessageUnit(&tMessage);

    xllmMessageInit(&tMessage, XLLM_ROLE_ASSISTANT);
    (void)xllmMessageAddToolCall(&tMessage, "pair-a", "read", "{}");
    SESSION_CHECK(!xllmSessionAddMessage(pSession, uTurn, &tMessage, 0u),
                  "resolved id cannot be reused inside the same turn");
    xllmMessageUnit(&tMessage);
    (void)xllmSessionAddText(pSession, uTurn, XLLM_ROLE_ASSISTANT, "done", 0u);

    uTurn = xllmSessionBeginTurn(pSession);
    (void)xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, "reuse provider id", 0u);
    memset(aCalls, 0, sizeof(aCalls));
    aCalls[0].sId = "pair-a";
    aCalls[0].sName = "read";
    aCalls[0].sArgumentsJson = "{}";
    memset(&tResponse, 0, sizeof(tResponse));
    tResponse.pToolCalls = aCalls;
    tResponse.iToolCallCount = 1u;
    SESSION_CHECK(xllmSessionAddAssistantResponse(pSession, uTurn, &tResponse) &&
                      xllmSessionAddToolResult(pSession, uTurn, "pair-a", "reused") &&
                      xllmSessionAddText(pSession, uTurn, XLLM_ROLE_ASSISTANT, "done again", 0u),
                  "provider ids may be reused in a later turn");

    uTurn = xllmSessionBeginTurn(pSession);
    (void)xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, "persist pending", 0u);
    aCalls[0].sId = "persist-call";
    SESSION_CHECK(xllmSessionAddAssistantResponse(pSession, uTurn, &tResponse) &&
                      xllmSessionSave(pSession, sSnapshot, &tError),
                  "snapshot records an unresolved pair without cutting it");
    pLoaded = xllmSessionLoad(sSnapshot, &tError);
    SESSION_CHECK(pLoaded && xllmSessionPendingToolCallCount(pLoaded) == 1u &&
                      xllmSessionAddToolResult(pLoaded, uTurn, "persist-call", "restored"),
                  "snapshot restore preserves the pending pair exactly");
    (void)xrtFileDelete((str)sSnapshot);

    pCompact = xllmSessionCreate(&tConfig, &tError);
    if (pCompact)
    {
        uint64_t uCallSequence;
        uTurn = xllmSessionBeginTurn(pCompact);
        (void)xllmSessionAddText(pCompact, uTurn, XLLM_ROLE_USER, "old turn", 0u);
        (void)xllmSessionAddText(pCompact, uTurn, XLLM_ROLE_ASSISTANT, "old answer", 0u);
        uTurn = xllmSessionBeginTurn(pCompact);
        (void)xllmSessionAddText(pCompact, uTurn, XLLM_ROLE_USER, "pending turn", 0u);
        aCalls[0].sId = "cut-call";
        (void)xllmSessionAddAssistantResponse(pCompact, uTurn, &tResponse);
        uCallSequence = pCompact->pEntries[pCompact->iEntryCount - 1u].uSequence;
        pCompaction = xllmSessionPrepareCompaction(pCompact, true, &tError);
        SESSION_CHECK(pCompaction && xllmCompactionThroughSequence(pCompaction) < uCallSequence,
                      "default compaction leaves an unresolved pair in the tail");
        xllmCompactionDestroy(pCompaction);
        pCompaction = NULL;
        memset(&tOps, 0, sizeof(tOps));
        tOps.pPlan = test_plan_through_call;
        tOps.pUserData = (void *)"cut-call";
        (void)xllmSessionSetCompactionOps(pCompact, &tOps);
        SESSION_CHECK(xllmSessionPrepareCompaction(pCompact, true, &tError) == NULL &&
                          tError.eCode == XLLM_ERROR_PROTOCOL,
                      "custom plan cannot capture an unresolved call");
        (void)xllmSessionAddToolResult(pCompact, uTurn, "cut-call", "complete");
        (void)xllmSessionAddText(pCompact, uTurn, XLLM_ROLE_ASSISTANT, "finished", 0u);
        SESSION_CHECK(xllmSessionPrepareCompaction(pCompact, true, &tError) == NULL &&
                          tError.eCode == XLLM_ERROR_PROTOCOL,
                      "custom plan cannot split a completed call/result pair");
    }

    pRender = xllmSessionCreate(&tConfig, &tError);
    if (pRender)
    {
        unsigned n;
        for (n = 1u; n <= 2u; ++n)
        {
            uTurn = xllmSessionBeginTurn(pRender);
            (void)xllmSessionAddText(pRender, uTurn, XLLM_ROLE_USER, "same id", 0u);
            aCalls[0].sId = "provider-reused";
            (void)xllmSessionAddAssistantResponse(pRender, uTurn, &tResponse);
            (void)xllmSessionAddToolResult(pRender, uTurn, "provider-reused", "ok");
            (void)xllmSessionAddText(pRender, uTurn, XLLM_ROLE_ASSISTANT, "done", 0u);
        }
        memset(&tHooks, 0, sizeof(tHooks));
        tHooks.pRenderMessage = test_cross_turn_pair_skip;
        (void)xllmSessionSetHooks(pRender, &tHooks);
        xllmRequestInit(&tRequest);
        SESSION_CHECK(!xllmSessionBuildRequest(pRender, &tRequest, &tError) &&
                          tError.eCode == XLLM_ERROR_PROTOCOL,
                      "render pairing is scoped by turn and cannot cross-cancel reused ids");
        xllmRequestUnit(&tRequest);
    }

    xllmCompactionDestroy(pCompaction);
    xllmSessionDestroy(pRender);
    xllmSessionDestroy(pCompact);
    xllmSessionDestroy(pLoaded);
    xllmSessionDestroy(pSession);
}

static void test_compaction_txn_config(xllm_session_config *pConfig)
{
    xllmSessionConfigInit(pConfig);
    pConfig->uContextWindowTokens = 8000u;
    pConfig->uMaxOutputTokens = 1000u;
    pConfig->uOutputReserveTokens = 1000u;
    pConfig->uSafetyReserveTokens = 500u;
    pConfig->uKeepRecentTokens = 1u;
}

static xllm_session *test_compaction_txn_fixture(const char *sSnapshot, const char *sJournal,
                                                 xllm_error *pError)
{
    xllm_session_config tConfig;
    xllm_session *pSession;
    uint64_t uTurn;
    (void)xrtFileDelete((str)sSnapshot);
    (void)xrtFileDelete((str)sJournal);
    test_compaction_txn_config(&tConfig);
    pSession = xllmSessionCreate(&tConfig, pError);
    if (!pSession || !xllmSessionEnableJournal(pSession, sJournal, pError))
    {
        xllmSessionDestroy(pSession);
        return NULL;
    }
    uTurn = xllmSessionBeginTurn(pSession);
    if (!uTurn || !xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, "durable old request", 0u) ||
        !xllmSessionAddText(pSession, uTurn, XLLM_ROLE_ASSISTANT, "durable old response", 0u))
    {
        xllmSessionDestroy(pSession);
        return NULL;
    }
    uTurn = xllmSessionBeginTurn(pSession);
    if (!uTurn ||
        !xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, "retained recent request", 0u) ||
        !xllmSessionAddText(pSession, uTurn, XLLM_ROLE_ASSISTANT, "retained recent response", 0u))
    {
        xllmSessionDestroy(pSession);
        return NULL;
    }
    return pSession;
}

static xllm_session *test_compaction_txn_recover(const char *sSnapshot, const char *sJournal,
                                                 xllm_error *pError)
{
    xllm_session_config tConfig;
    test_compaction_txn_config(&tConfig);
    return xllmSessionRecover(sSnapshot, sJournal, &tConfig, pError);
}

static void test_compaction_transaction_recovery(void)
{
    static const char *const psSnapshots[] = {
        "out/mdo-integration/fixtures/txn_prepare_snapshot.json",
        "out/mdo-integration/fixtures/txn_execute_snapshot.json",
        "out/mdo-integration/fixtures/txn_abort_snapshot.json",
        "out/mdo-integration/fixtures/txn_quality_snapshot.json",
        "out/mdo-integration/fixtures/txn_redo_snapshot.json",
        "out/mdo-integration/fixtures/txn_commit_snapshot.json",
        "out/mdo-integration/fixtures/txn_invalid_pair_snapshot.json"};
    static const char *const psJournals[] = {
        "out/mdo-integration/fixtures/txn_prepare.journal",
        "out/mdo-integration/fixtures/txn_execute.journal",
        "out/mdo-integration/fixtures/txn_abort.journal",
        "out/mdo-integration/fixtures/txn_quality.journal",
        "out/mdo-integration/fixtures/txn_redo.journal",
        "out/mdo-integration/fixtures/txn_commit.journal",
        "out/mdo-integration/fixtures/txn_invalid_pair.journal"};
    xllm_session *pSession = NULL;
    xllm_session *pRecovered = NULL;
    xllm_compaction *pCompaction = NULL;
    xllm_session_stats tStats;
    xllm_session_summary tSummary;
    xllm_compaction_quality tQuality;
    xllm_usage tUsage;
    xllm_error tError;
    xllm_tool_call tCall;
    xllm_response tResponse;
    uint64_t uThrough;
    size_t i;

    /* PREPARED is volatile: losing the process rolls back to the journaled
     * ledger, and destroying the session first safely detaches the handle. */
    pSession = test_compaction_txn_fixture(psSnapshots[0], psJournals[0], &tError);
    pCompaction = pSession ? xllmSessionPrepareCompaction(pSession, true, &tError) : NULL;
    SESSION_CHECK(pCompaction &&
                      xllmSessionGetState(pSession) == XLLM_SESSION_STATE_COMPACTION_PREPARED,
                  "prepared transaction is explicit and volatile");
    xllmSessionDestroy(pSession);
    pSession = NULL;
    memset(&tUsage, 0, sizeof(tUsage));
    SESSION_CHECK(pCompaction && !xllmCompactionSetUsage(pCompaction, &tUsage) &&
                      !xllmCompactionEvaluateSummary(pCompaction, test_pi_summary("detached"),
                                                     &tQuality, &tError),
                  "session destruction detaches the prepared transaction safely");
    xllmCompactionDestroy(pCompaction);
    pCompaction = NULL;
    pRecovered = test_compaction_txn_recover(psSnapshots[0], psJournals[0], &tError);
    SESSION_CHECK(pRecovered && xllmSessionGetStats(pRecovered, &tStats) &&
                      tStats.uCompactionCount == 0u && tStats.uEntryCount == 4u &&
                      xllmSessionGetState(pRecovered) == XLLM_SESSION_STATE_TURN_COMPLETE,
                  "crash after prepare restores the exact pre-transaction ledger");
    xllmSessionDestroy(pRecovered);
    pRecovered = NULL;

    /* IN_FLIGHT is equally volatile; no summary output is durable until the
     * compact commit record is complete. */
    pSession = test_compaction_txn_fixture(psSnapshots[1], psJournals[1], &tError);
    pCompaction = pSession ? xllmSessionPrepareCompaction(pSession, true, &tError) : NULL;
    SESSION_CHECK(pCompaction && xllmCompactionBegin(pCompaction, &tError) &&
                      xllmSessionGetState(pSession) == XLLM_SESSION_STATE_COMPACTION_IN_FLIGHT,
                  "begin marks the external summary call in flight");
    xllmSessionDestroy(pSession);
    pSession = NULL;
    xllmCompactionDestroy(pCompaction);
    pCompaction = NULL;
    pRecovered = test_compaction_txn_recover(psSnapshots[1], psJournals[1], &tError);
    SESSION_CHECK(pRecovered && xllmSessionGetStats(pRecovered, &tStats) &&
                      tStats.uCompactionCount == 0u && tStats.uCompactedThroughSequence == 0u,
                  "crash during summary execution rolls back without a phantom commit");
    xllmSessionDestroy(pRecovered);
    pRecovered = NULL;

    /* Explicit abort and implicit Destroy share the same state restoration,
     * while a detached handle cannot be re-used. */
    pSession = test_compaction_txn_fixture(psSnapshots[2], psJournals[2], &tError);
    pCompaction = pSession ? xllmSessionPrepareCompaction(pSession, true, &tError) : NULL;
    SESSION_CHECK(pCompaction && xllmCompactionBegin(pCompaction, &tError) &&
                      xllmCompactionAbort(pCompaction, &tError) &&
                      xllmSessionGetState(pSession) == XLLM_SESSION_STATE_TURN_COMPLETE,
                  "explicit abort restores the exact stable state");
    SESSION_CHECK(pCompaction && !xllmCompactionAbort(pCompaction, &tError) &&
                      !xllmCompactionSetUsage(pCompaction, &tUsage),
                  "aborted transaction is permanently detached");
    xllmCompactionDestroy(pCompaction);
    pCompaction = NULL;
    xllmSessionDestroy(pSession);
    pSession = NULL;
    pRecovered = test_compaction_txn_recover(psSnapshots[2], psJournals[2], &tError);
    SESSION_CHECK(pRecovered && xllmSessionGetStats(pRecovered, &tStats) &&
                      tStats.uCompactionCount == 0u,
                  "abort leaves no replayable compaction fact");
    xllmSessionDestroy(pRecovered);
    pRecovered = NULL;

    /* A quality failure keeps the transaction abortable and writes no commit
     * record; process loss still restores the old checkpoint. */
    pSession = test_compaction_txn_fixture(psSnapshots[3], psJournals[3], &tError);
    pCompaction = pSession ? xllmSessionPrepareCompaction(pSession, true, &tError) : NULL;
    (void)(pCompaction && xllmCompactionBegin(pCompaction, &tError));
    SESSION_CHECK(
        pCompaction &&
            !xllmSessionCommitCompaction(pSession, pCompaction, "incomplete summary", &tError) &&
            xllmSessionGetState(pSession) == XLLM_SESSION_STATE_COMPACTION_IN_FLIGHT,
        "quality rejection does not publish or destroy the transaction");
    xllmSessionDestroy(pSession);
    pSession = NULL;
    xllmCompactionDestroy(pCompaction);
    pCompaction = NULL;
    pRecovered = test_compaction_txn_recover(psSnapshots[3], psJournals[3], &tError);
    SESSION_CHECK(pRecovered && xllmSessionGetStats(pRecovered, &tStats) &&
                      tStats.uCompactionCount == 0u,
                  "crash after rejected summary retains the old checkpoint");
    xllmSessionDestroy(pRecovered);
    pRecovered = NULL;

    /* Simulate the only commit crash window: the complete redo record reached
     * the journal, then the process disappeared before in-memory publication. */
    pSession = test_compaction_txn_fixture(psSnapshots[4], psJournals[4], &tError);
    pCompaction = pSession ? xllmSessionPrepareCompaction(pSession, true, &tError) : NULL;
    (void)(pCompaction && xllmCompactionBegin(pCompaction, &tError));
    uThrough = xllmCompactionThroughSequence(pCompaction);
    memset(&tUsage, 0, sizeof(tUsage));
    tUsage.uInputTokens = 321u;
    tUsage.uOutputTokens = 45u;
    SESSION_CHECK(pCompaction && xllmCompactionSetUsage(pCompaction, &tUsage) &&
                      xllm_session__journal_append_compaction(
                          pSession, uThrough, pSession->uSummaryGeneration + 1u,
                          tUsage.uInputTokens, tUsage.uOutputTokens,
                          test_pi_summary("redo after commit record")) &&
                      xllmSessionGetStats(pSession, &tStats) && tStats.uCompactionCount == 0u,
                  "complete commit record precedes in-memory publication");
    xllmSessionDestroy(pSession);
    pSession = NULL;
    xllmCompactionDestroy(pCompaction);
    pCompaction = NULL;
    pRecovered = test_compaction_txn_recover(psSnapshots[4], psJournals[4], &tError);
    SESSION_CHECK(
        pRecovered && xllmSessionGetStats(pRecovered, &tStats) &&
            xllmSessionGetSummary(pRecovered, &tSummary) && tStats.uCompactionCount == 1u &&
            tStats.uCompactedThroughSequence == uThrough && tStats.uSummaryTokensExact == 45u &&
            tSummary.sText && strstr(tSummary.sText, "redo after commit record") != NULL,
        "recovery redoes a complete commit record exactly once");
    xllmSessionDestroy(pRecovered);
    pRecovered = NULL;

    /* Normal commit detaches the transaction before the session can die;
     * getters remain usable but mutating/evaluating operations do not. */
    pSession = test_compaction_txn_fixture(psSnapshots[5], psJournals[5], &tError);
    pCompaction = pSession ? xllmSessionPrepareCompaction(pSession, true, &tError) : NULL;
    (void)(pCompaction && xllmCompactionBegin(pCompaction, &tError));
    uThrough = xllmCompactionThroughSequence(pCompaction);
    SESSION_CHECK(pCompaction &&
                      xllmSessionCommitCompaction(pSession, pCompaction,
                                                  test_pi_summary("normal durable commit"),
                                                  &tError) &&
                      xllmCompactionPrompt(pCompaction) != NULL &&
                      !xllmCompactionSetUsage(pCompaction, &tUsage) &&
                      !xllmCompactionEvaluateSummary(pCompaction, test_pi_summary("late"),
                                                     &tQuality, &tError),
                  "committed transaction is detached and read-only");
    xllmSessionDestroy(pSession);
    pSession = NULL;
    pRecovered = test_compaction_txn_recover(psSnapshots[5], psJournals[5], &tError);
    SESSION_CHECK(pRecovered && xllmSessionGetStats(pRecovered, &tStats) &&
                      tStats.uCompactionCount == 1u && tStats.uCompactedThroughSequence == uThrough,
                  "normal committed transaction survives immediate process loss");
    xllmSessionDestroy(pRecovered);
    pRecovered = NULL;
    xllmCompactionDestroy(pCompaction);
    pCompaction = NULL;

    /* A syntactically complete redo record is still untrusted recovery input:
     * replay revalidates the tool-pair boundary before publishing it. */
    pSession = test_compaction_txn_fixture(psSnapshots[6], psJournals[6], &tError);
    if (pSession)
    {
        uint64_t uTurn = xllmSessionBeginTurn(pSession);
        (void)xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, "pair replay guard", 0u);
        memset(&tCall, 0, sizeof(tCall));
        tCall.sId = "redo-pair";
        tCall.sName = "probe";
        tCall.sArgumentsJson = "{}";
        memset(&tResponse, 0, sizeof(tResponse));
        tResponse.pToolCalls = &tCall;
        tResponse.iToolCallCount = 1u;
        (void)xllmSessionAddAssistantResponse(pSession, uTurn, &tResponse);
        uThrough = pSession->pEntries[pSession->iEntryCount - 1u].uSequence;
        (void)xllmSessionAddToolResult(pSession, uTurn, "redo-pair", "complete");
        (void)xllmSessionAddText(pSession, uTurn, XLLM_ROLE_ASSISTANT, "pair done", 0u);
        SESSION_CHECK(xllm_session__journal_append_compaction(
                          pSession, uThrough, pSession->uSummaryGeneration + 1u, 10u, 5u,
                          test_pi_summary("invalid split redo")),
                      "test writes a complete compact record with a split pair");
    }
    xllmSessionDestroy(pSession);
    pSession = NULL;
    pRecovered = test_compaction_txn_recover(psSnapshots[6], psJournals[6], &tError);
    SESSION_CHECK(pRecovered == NULL && tError.eCode == XLLM_ERROR_PARSE,
                  "recovery rejects a complete commit record that splits a tool pair");
    xllmSessionDestroy(pRecovered);
    pRecovered = NULL;

    for (i = 0u; i < sizeof(psSnapshots) / sizeof(psSnapshots[0]); ++i)
    {
        (void)xrtFileDelete((str)psSnapshots[i]);
        (void)xrtFileDelete((str)psJournals[i]);
    }
}

static void test_session_state_machine(void)
{
    xllm_session_config tConfig;
    xllm_session *pSession;
    xllm_compaction *pCompaction;
    xllm_error tError;
    xllm_tool_call aCalls[2];
    xllm_response tResponse;
    uint64_t uTurn;

    SESSION_CHECK(XLLM_SESSION_VERSION_MAJOR == 3 &&
                      strcmp(XLLM_SESSION_VERSION_STRING, "3.0.0-dev") == 0,
                  "session release version is explicit");
    xllmSessionConfigInit(&tConfig);
    tConfig.uContextWindowTokens = 8000u;
    tConfig.uMaxOutputTokens = 1000u;
    tConfig.uOutputReserveTokens = 1000u;
    tConfig.uSafetyReserveTokens = 500u;
    tConfig.uKeepRecentTokens = 16u;
    pSession = xllmSessionCreate(&tConfig, &tError);
    SESSION_CHECK(pSession && xllmSessionGetState(pSession) == XLLM_SESSION_STATE_IDLE &&
                      strcmp(xllmSessionStateName(xllmSessionGetState(pSession)), "idle") == 0,
                  "new session starts idle");
    if (!pSession)
    {
        return;
    }
    SESSION_CHECK(!xllmSessionBeginModelCall(pSession, &tError) &&
                      tError.eCode == XLLM_ERROR_INVALID_ARGUMENT,
                  "model call before user input is rejected");

    uTurn = xllmSessionBeginTurn(pSession);
    SESSION_CHECK(uTurn == 1u && xllmSessionGetState(pSession) == XLLM_SESSION_STATE_USER_APPENDED,
                  "begin turn opens the user-appended phase");
    SESSION_CHECK(xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, "inspect two files", 0u) &&
                      xllmSessionBeginModelCall(pSession, &tError) &&
                      xllmSessionGetState(pSession) == XLLM_SESSION_STATE_MODEL_IN_FLIGHT,
                  "explicit model boundary enters model-in-flight");
    SESSION_CHECK(xllmSessionBeginTurn(pSession) == 0u &&
                      !xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, "illegal mutation", 0u),
                  "model-in-flight rejects ledger mutation");
    SESSION_CHECK(xllmSessionAbortModelCall(pSession, &tError) &&
                      xllmSessionGetState(pSession) == XLLM_SESSION_STATE_USER_APPENDED,
                  "aborted model call restores the ready state");

    memset(aCalls, 0, sizeof(aCalls));
    aCalls[0].sId = "state-call-1";
    aCalls[0].sName = "read";
    aCalls[0].sArgumentsJson = "{}";
    aCalls[1].sId = "state-call-2";
    aCalls[1].sName = "read";
    aCalls[1].sArgumentsJson = "{}";
    memset(&tResponse, 0, sizeof(tResponse));
    tResponse.pToolCalls = aCalls;
    tResponse.iToolCallCount = 2u;
    SESSION_CHECK(xllmSessionBeginModelCall(pSession, &tError) &&
                      xllmSessionAddAssistantResponse(pSession, uTurn, &tResponse) &&
                      xllmSessionGetState(pSession) == XLLM_SESSION_STATE_TOOLS_PENDING,
                  "tool response enters tools-pending");
    SESSION_CHECK(xllmSessionBeginTurn(pSession) == 0u &&
                      !xllmSessionBeginModelCall(pSession, &tError),
                  "pending tools block new turns and model calls");
    SESSION_CHECK(xllmSessionAddToolResult(pSession, uTurn, "state-call-1", "one") &&
                      xllmSessionGetState(pSession) == XLLM_SESSION_STATE_TOOLS_PARTIAL,
                  "first result enters tools-partial");
    SESSION_CHECK(!xllmSessionAddToolResult(pSession, uTurn, "state-call-1", "duplicate") &&
                      !xllmSessionAddToolResult(pSession, uTurn, "unknown", "invalid"),
                  "duplicate and unknown tool results are rejected");
    SESSION_CHECK(xllmSessionAddToolResult(pSession, uTurn, "state-call-2", "two") &&
                      xllmSessionGetState(pSession) == XLLM_SESSION_STATE_USER_APPENDED,
                  "resolved tool batch returns to model-ready");

    memset(&tResponse, 0, sizeof(tResponse));
    tResponse.sContent = "done";
    SESSION_CHECK(xllmSessionBeginModelCall(pSession, &tError) &&
                      xllmSessionAddAssistantResponse(pSession, uTurn, &tResponse) &&
                      xllmSessionGetState(pSession) == XLLM_SESSION_STATE_TURN_COMPLETE,
                  "final assistant response completes the turn");
    uTurn = xllmSessionBeginTurn(pSession);
    SESSION_CHECK(uTurn == 2u &&
                      xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, "second turn", 0u) &&
                      xllmSessionAddText(pSession, uTurn, XLLM_ROLE_ASSISTANT, "complete", 0u) &&
                      xllmSessionGetState(pSession) == XLLM_SESSION_STATE_TURN_COMPLETE,
                  "manual ledger driving uses the same transition table");

    pCompaction = xllmSessionPrepareCompaction(pSession, true, &tError);
    SESSION_CHECK(pCompaction &&
                      xllmSessionGetState(pSession) == XLLM_SESSION_STATE_COMPACTION_PREPARED,
                  "prepare owns the compaction transaction");
    SESSION_CHECK(pCompaction && !xllmSessionPrepareCompaction(pSession, true, &tError) &&
                      !xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, "blocked", 0u),
                  "active compaction rejects competing mutations");
    SESSION_CHECK(pCompaction && xllmCompactionBegin(pCompaction, &tError) &&
                      xllmSessionGetState(pSession) == XLLM_SESSION_STATE_COMPACTION_IN_FLIGHT &&
                      !xllmCompactionBegin(pCompaction, &tError),
                  "compaction execution has one legal begin edge");
    xllmCompactionDestroy(pCompaction);
    SESSION_CHECK(xllmSessionGetState(pSession) == XLLM_SESSION_STATE_TURN_COMPLETE,
                  "destroying an uncommitted compaction aborts to the prior state");
    xllmSessionDestroy(pSession);
}

static void test_apply_model_profile(void)
{
    xllm_model_profile tLarge, tSmall, tBad;
    xllm_session_config tBefore, tAfter;
    xllm_error tError;
    xllm_session *pSession;
    xllm_session *pLoaded;
    uint64_t uTurn, uSequence;
    size_t iCount;
    xllmModelProfileInit(&tLarge);
    tLarge.sId = "large";
    tLarge.sModel = "large";
    tLarge.eProvider = XLLM_PROVIDER_OPENAI_COMPAT;
    tLarge.uCapabilities = XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT | XLLM_CAP_STREAM;
    tLarge.eWindowMode = XLLM_WINDOW_SHARED_CONTEXT;
    tLarge.uContextWindowTokens = 1000000u;
    tLarge.uMaxInputTokens = 1000000u;
    tLarge.uMaxOutputTokens = 65536u;
    tLarge.uRecommendedOutputReserveTokens = 16384u;
    tLarge.uRecommendedSummaryTokens = 4000u;
    tSmall = tLarge;
    tSmall.sId = "small";
    tSmall.sModel = "small";
    tSmall.uContextWindowTokens = 240128u;
    tSmall.uMaxInputTokens = 240128u;
    tSmall.uMaxOutputTokens = 16384u;
    tSmall.uRecommendedOutputReserveTokens = 4096u;
    pSession = xllmSessionCreateForProfile(NULL, &tLarge, &tError);
    SESSION_CHECK(pSession != NULL, "create large model profile for switching");
    if (!pSession)
        return;
    uTurn = xllmSessionBeginTurn(pSession);
    (void)xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, "prior prompt", 0u);
    (void)xllmSessionAddText(pSession, uTurn, XLLM_ROLE_ASSISTANT, "prior answer", 0u);
    (void)xllmSessionNoteFileRead(pSession, "retained.txt");
    uSequence = xllmSessionLastSequence(pSession);
    iCount = xllmSessionEntryCount(pSession);
    (void)xllmSessionGetConfig(pSession, &tBefore);
    tBad = tSmall;
    tBad.uMaxInputTokens = tSmall.uContextWindowTokens + 1u;
    SESSION_CHECK(!xllmSessionApplyProfile(pSession, NULL, &tBad, &tError) &&
                      xllmSessionGetConfig(pSession, &tAfter) &&
                      tAfter.uContextWindowTokens == tBefore.uContextWindowTokens &&
                      xllmSessionLastSequence(pSession) == uSequence,
                  "invalid model switch is atomic");
    xllm_session__test_alloc_fail(1u);
    SESSION_CHECK(!xllmSessionApplyProfile(pSession, NULL, &tSmall, &tError) &&
                      tError.eCode == XLLM_ERROR_OUT_OF_MEMORY &&
                      pSession->tConfig.uContextWindowTokens == tBefore.uContextWindowTokens,
                  "failed allocation leaves the previous model budget intact");
    xllm_session__test_alloc_reset();
    uTurn = xllmSessionBeginTurn(pSession);
    (void)xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, "next prompt", 0u);
    uSequence = xllmSessionLastSequence(pSession);
    iCount = xllmSessionEntryCount(pSession);
    SESSION_CHECK(xllmSessionBeginModelCall(pSession, &tError) &&
                      !xllmSessionApplyProfile(pSession, NULL, &tSmall, &tError) &&
                      xllmSessionAbortModelCall(pSession, &tError),
                  "model switch is rejected while a model call is in flight");
    SESSION_CHECK(xllmSessionApplyProfile(pSession, NULL, &tSmall, &tError) &&
                      xllmSessionGetConfig(pSession, &tAfter) &&
                      tAfter.uContextWindowTokens == 240128u && tAfter.uMaxOutputTokens == 16384u &&
                      xllmSessionLastSequence(pSession) == uSequence &&
                      xllmSessionEntryCount(pSession) == iCount && pSession->iReadFileCount == 1u,
                  "smaller model changes limits and preserves history and file ledger");
    SESSION_CHECK(
        xllmSessionSave(pSession, "out/mdo-integration/fixtures/profile-switch.json", &tError),
        "model budget can be persisted with the preserved ledger");
    pLoaded = xllmSessionLoad("out/mdo-integration/fixtures/profile-switch.json", &tError);
    SESSION_CHECK(pLoaded && pLoaded->tConfig.uContextWindowTokens == 240128u &&
                      xllmSessionLastSequence(pLoaded) == uSequence &&
                      xllmSessionApplyProfile(pLoaded, NULL, &tLarge, &tError) &&
                      pLoaded->tConfig.uContextWindowTokens == 1000000u &&
                      pLoaded->tConfig.uMaxOutputTokens == 65536u,
                  "reload and switch back to a larger model preserve the ledger");
    xllmSessionDestroy(pLoaded);
    xllmSessionDestroy(pSession);
    (void)xrtFileDelete("out/mdo-integration/fixtures/profile-switch.json");
}

static void test_budget_and_profile(void)
{
    const xllm_capability_flags uCaps = XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT | XLLM_CAP_STREAM;
    xllm_model_profile tProfile;
    xllm_session_config tConfig;
    xllm_session *pSession;
    xllm_session_budget tBudget;
    xllm_session_stats tStats;
    xllm_usage tUsage;
    xllm_error tError;
    bool bMatrix = true;
    size_t i;

    xllmModelProfileInit(&tProfile);
    tProfile.sId = "test-shared";
    tProfile.sModel = "test-model";
    tProfile.eProvider = XLLM_PROVIDER_OPENAI_COMPAT;
    tProfile.uCapabilities = uCaps;
    tProfile.eWindowMode = XLLM_WINDOW_SHARED_CONTEXT;
    tProfile.uContextWindowTokens = 100000u;
    tProfile.uMaxInputTokens = 90000u;
    tProfile.uMaxOutputTokens = 20000u;
    tProfile.uRecommendedOutputReserveTokens = 10000u;
    tProfile.uRecommendedSummaryTokens = 4000u;
    SESSION_CHECK(xllmSessionConfigInitFromProfile(&tConfig, &tProfile, &tError) &&
                      tConfig.eWindowMode == XLLM_WINDOW_SHARED_CONTEXT &&
                      tConfig.uMaxInputTokens == 90000u && tConfig.uMaxOutputTokens == 20000u,
                  "model profile initializes explicit session limits");
    pSession = xllmSessionCreateForProfile(NULL, &tProfile, &tError);
    xllmSessionBudgetInit(&tBudget);
    SESSION_CHECK(pSession && xllmSessionGetBudget(pSession, &tBudget) &&
                      tBudget.uInputBudgetTokens == 82000u &&
                      tBudget.uOutputReserveTokens == 10000u && tBudget.uSummaryMaxTokens == 4000u,
                  "shared context subtracts output and safety reserves");
    xllmSessionDestroy(pSession);

    tProfile.eWindowMode = XLLM_WINDOW_SPLIT_INPUT_OUTPUT;
    pSession = xllmSessionCreateForProfile(NULL, &tProfile, &tError);
    xllmSessionBudgetInit(&tBudget);
    SESSION_CHECK(pSession && xllmSessionGetBudget(pSession, &tBudget) &&
                      tBudget.uInputBudgetTokens == 82000u,
                  "split profile keeps output reserve outside the input budget");
    /* The budget descriptor intentionally has no dynamic next-output field;
     * stats reports it after rendered input is known. */
    SESSION_CHECK(pSession && xllmSessionGetStats(pSession, &tStats) &&
                      tStats.uNextMaxOutputTokens == 20000u &&
                      tStats.eWindowMode == XLLM_WINDOW_SPLIT_INPUT_OUTPUT,
                  "split profile preserves the independent output ceiling");
    xllmSessionDestroy(pSession);

    SESSION_CHECK(xllmSessionConfigInitFromProfile(&tConfig, &tProfile, &tError),
                  "profile override fixture initializes");
    tConfig.uContextWindowTokens = tProfile.uContextWindowTokens + 1u;
    SESSION_CHECK(xllmSessionCreateForProfile(&tConfig, &tProfile, &tError) == NULL &&
                      tError.eCode == XLLM_ERROR_INVALID_ARGUMENT,
                  "profile ceilings cannot be widened by session overrides");
    tConfig.uContextWindowTokens = tProfile.uContextWindowTokens;
    tConfig.eWindowMode = XLLM_WINDOW_SHARED_CONTEXT;
    SESSION_CHECK(xllmSessionCreateForProfile(&tConfig, &tProfile, &tError) == NULL,
                  "profile window semantics cannot be changed by overrides");
    xllmSessionConfigInit(&tConfig);
    tConfig.eWindowMode = XLLM_WINDOW_UNSPECIFIED;
    tConfig.uContextWindowTokens = 50000u;
    tConfig.uMaxInputTokens = 0u;
    tConfig.uMaxOutputTokens = 0u;
    tConfig.uOutputReserveTokens = 0u;
    pSession = xllmSessionCreateForProfile(&tConfig, &tProfile, &tError);
    xllmSessionBudgetInit(&tBudget);
    SESSION_CHECK(pSession && xllmSessionGetBudget(pSession, &tBudget) &&
                      tBudget.eWindowMode == XLLM_WINDOW_SPLIT_INPUT_OUTPUT &&
                      tBudget.uContextWindowTokens == 50000u && tBudget.uMaxInputTokens == 50000u &&
                      tBudget.uMaxOutputTokens == 20000u,
                  "zero profile overrides inherit and clamp to a narrower context");
    xllmSessionDestroy(pSession);

    for (i = 0u; i < 6u; ++i)
    {
        const uint64_t uContext = (i % 3u == 0u) ? 1000u : ((i % 3u == 1u) ? 4096u : 10000u);
        const xllm_window_mode eMode =
            i < 3u ? XLLM_WINDOW_SHARED_CONTEXT : XLLM_WINDOW_SPLIT_INPUT_OUTPUT;
        uint64_t uExpected;
        xllmSessionConfigInit(&tConfig);
        tConfig.eWindowMode = eMode;
        tConfig.uContextWindowTokens = uContext;
        tConfig.uMaxInputTokens = uContext - 1u;
        tConfig.uMaxOutputTokens = 100u;
        tConfig.uOutputReserveTokens = 100u;
        tConfig.uSafetyReserveTokens = 50u;
        tConfig.uKeepRecentTokens = 1u;
        tConfig.uSummaryMaxTokens = 64u;
        tConfig.uSummaryMinTokens = 1u;
        tConfig.uSummaryMaxBytes = 64u;
        pSession = xllmSessionCreate(&tConfig, &tError);
        xllmSessionBudgetInit(&tBudget);
        uExpected = eMode == XLLM_WINDOW_SHARED_CONTEXT ? uContext - 150u : uContext - 51u;
        if (!pSession || !xllmSessionGetBudget(pSession, &tBudget) ||
            tBudget.uInputBudgetTokens != uExpected ||
            tBudget.uInputBudgetTokens > tBudget.uMaxInputTokens)
        {
            bMatrix = false;
        }
        xllmSessionDestroy(pSession);
    }
    SESSION_CHECK(bMatrix, "bounded budget matrix matches both window formulas");

    xllmSessionConfigInit(&tConfig);
    tConfig.uContextWindowTokens = 100u;
    tConfig.uMaxInputTokens = 100u;
    tConfig.uMaxOutputTokens = 50u;
    tConfig.uOutputReserveTokens = 50u;
    tConfig.uSafetyReserveTokens = 50u;
    tConfig.uKeepRecentTokens = 1u;
    tConfig.uSummaryMaxTokens = 10u;
    tConfig.uSummaryMinTokens = 1u;
    tConfig.uSummaryMaxBytes = 8u;
    SESSION_CHECK(xllmSessionCreate(&tConfig, &tError) == NULL,
                  "shared reserves that exhaust input are rejected");
    tConfig.eWindowMode = XLLM_WINDOW_SPLIT_INPUT_OUTPUT;
    pSession = xllmSessionCreate(&tConfig, &tError);
    xllmSessionBudgetInit(&tBudget);
    SESSION_CHECK(pSession && xllmSessionGetBudget(pSession, &tBudget) &&
                      tBudget.uInputBudgetTokens == 50u,
                  "split output reserve does not exhaust independent input");
    xllmSessionDestroy(pSession);

    xllmSessionConfigInit(&tConfig);
    tConfig.uContextWindowTokens = UINT64_MAX;
    tConfig.uMaxInputTokens = UINT64_MAX;
    tConfig.uMaxOutputTokens = UINT32_MAX;
    tConfig.uOutputReserveTokens = UINT32_MAX;
    tConfig.uSafetyReserveTokens = 32000u;
    pSession = xllmSessionCreate(&tConfig, &tError);
    memset(&tUsage, 0, sizeof(tUsage));
    tUsage.uInputTokens = UINT64_MAX;
    tUsage.uOutputTokens = 1u;
    SESSION_CHECK(xllmSessionComputeSafetyReserve(UINT64_MAX) == 32000u && pSession &&
                      xllmSessionRecordUsage(pSession, &tUsage) &&
                      xllmSessionGetStats(pSession, &tStats) && tStats.uFillExact == UINT64_MAX &&
                      tStats.ePressure == XLLM_SESSION_PRESSURE_OVERFLOW &&
                      tStats.uPruneThresholdTokens <= tStats.uInputBudgetTokens &&
                      tStats.uCompactThresholdTokens <= tStats.uInputBudgetTokens,
                  "maximum token values saturate without integer wraparound");
    xllmSessionDestroy(pSession);
}

static void test_compaction_user_bridge(void)
{
    xllm_session_config tConfig;
    xllm_session *pSession;
    xllm_compaction *pCompaction;
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
    if (!pSession)
        return;

    uTurn = xllmSessionBeginTurn(pSession);
    SESSION_CHECK(xllmSessionAddText(pSession, uTurn, XLLM_ROLE_SYSTEM,
                                     "Pinned coding-agent contract.", XLLM_SESSION_ENTRY_PINNED),
                  "bridge test pins one system message");
    uTurn = xllmSessionBeginTurn(pSession);
    SESSION_CHECK(
        xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, "Implement the requested project.", 0u),
        "bridge test adds the original user objective");

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
    SESSION_CHECK(pCompaction && strstr(xllmCompactionPrompt(pCompaction),
                                        "[User]: Implement the requested project.") != NULL,
                  "Pi serialization tags the candidate");
    SESSION_CHECK(pCompaction &&
                      xllmCompactionEvaluateSummary(pCompaction, "## Goal\ntoo little continuity.",
                                                    &tQuality, &tError) &&
                      !tQuality.bAccepted && tQuality.uMissingSections != 0u,
                  "quality report rejects an incomplete compaction summary");
    SESSION_CHECK(
        pCompaction &&
            xllmSessionCommitCompaction(
                pSession, pCompaction,
                test_pi_summary("Implement the requested project and preserve tool continuity."),
                &tError),
        "bridge compaction commits a continuation summary");
    xllmCompactionDestroy(pCompaction);

    xllmRequestInit(&tRequest);
    SESSION_CHECK(xllmSessionBuildRequest(pSession, &tRequest, &tError),
                  "bridge request renders after compaction");
    SESSION_CHECK(
        tRequest.iMessageCount == 4u && tRequest.pMessages[0].eRole == XLLM_ROLE_SYSTEM &&
            tRequest.pMessages[1].eRole == XLLM_ROLE_USER &&
            tRequest.pMessages[2].eRole == XLLM_ROLE_ASSISTANT &&
            tRequest.pMessages[3].eRole == XLLM_ROLE_TOOL,
        "compaction summary bridges system to retained assistant/tool suffix as a user turn");
    SESSION_CHECK(tRequest.iMessageCount == 4u && tRequest.pMessages[2].iToolCallCount == 1u &&
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
    xllm_session *pSession;
    xllm_session_stats tStats;
    xllm_request tRequest;
    xllm_error tError;
    char *sLarge;
    uint64_t uTurn;
    xllmSessionConfigInit(&tConfig);
    pSession = xllmSessionCreate(&tConfig, &tError);
    SESSION_CHECK(pSession != NULL, "default GLM-5.1 session profile creates");
    SESSION_CHECK(tConfig.uContextWindowTokens == XLLM_SESSION_DEFAULT_CONTEXT_WINDOW_TOKENS &&
                      tConfig.uMaxOutputTokens == XLLM_SESSION_DEFAULT_MAX_OUTPUT_TOKENS,
                  "current context and maximum-output profile");
    SESSION_CHECK(xllmSessionComputeSafetyReserve(XLLM_SESSION_DEFAULT_CONTEXT_WINDOW_TOKENS) ==
                      8000u,
                  "bounded 3 percent safety reserve profile");
    SESSION_CHECK(pSession && xllmSessionGetConfig(pSession, &tEffective) &&
                      tEffective.uOutputReserveTokens == 32768u,
                  "dynamic output reserve is distinct from output ceiling");
    SESSION_CHECK(pSession && xllmSessionGetStats(pSession, &tStats) &&
                      tStats.uInputBudgetTokens == 164032u &&
                      tStats.uNextMaxOutputTokens == 131072u,
                  "effective input budget preserves full output on sparse context");
    SESSION_CHECK(pSession && xllmSessionGetConfig(pSession, &tEffective) &&
                      tEffective.uKeepRecentTokens == 20000u,
                  "default keep-recent is the Pi 20k budget");
    sLarge = make_large_output(300000u, 7u);
    uTurn = pSession ? xllmSessionBeginTurn(pSession) : 0u;
    SESSION_CHECK(sLarge && uTurn &&
                      xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, sLarge, 0u),
                  "large active input added for dynamic output test");
    xllmRequestInit(&tRequest);
    SESSION_CHECK(xllmSessionBuildRequest(pSession, &tRequest, &tError) &&
                      tRequest.uMaxOutputTokens < 131072u && tRequest.uMaxOutputTokens > 32768u,
                  "request output ceiling shrinks only as active input grows");
    xllmRequestUnit(&tRequest);
    free(sLarge);
    xllmSessionDestroy(pSession);
}

static void test_add_reference(void)
{
    xllm_session_config tConfig;
    xllm_session *pSession;
    xllm_request tRequest;
    const xllm_message *pFrame = NULL;
    size_t i;
    uint64_t uTurn;
    xllmSessionConfigInit(&tConfig);
    pSession = xllmSessionCreate(&tConfig, NULL);
    uTurn = pSession ? xllmSessionBeginTurn(pSession) : 0u;
    SESSION_CHECK(pSession && uTurn &&
                      xllmSessionAddReference(pSession, uTurn, "workspace://notes.md",
                                              "note body with a hidden instruction"),
                  "reference entry appends with provenance framing");
    SESSION_CHECK(!xllmSessionAddReference(pSession, uTurn, NULL, ""),
                  "empty reference content is rejected");
    xllmRequestInit(&tRequest);
    if (pSession && xllmSessionBuildRequest(pSession, &tRequest, NULL))
    {
        for (i = 0u; i < tRequest.iMessageCount; ++i)
        {
            const xllm_message *pMessage = &tRequest.pMessages[i];
            if (pMessage->eRole == XLLM_ROLE_USER && pMessage->sContent &&
                strstr(pMessage->sContent, "[retrieved-context]"))
            {
                pFrame = pMessage;
                break;
            }
        }
    }
    SESSION_CHECK(pFrame && strstr(pFrame->sContent, "untrusted reference material") &&
                      strstr(pFrame->sContent, "Source: workspace://notes.md") &&
                      strstr(pFrame->sContent, "note body with a hidden instruction") &&
                      strstr(pFrame->sContent, "[/retrieved-context]"),
                  "rendered request carries the full untrusted-reference frame");
    xllmRequestUnit(&tRequest);
    xllmSessionDestroy(pSession);
}

static void test_budget_compaction_persistence(void)
{
    static const char sStatePath[] = "out/mdo-integration/fixtures/session_state_test.json";
    const char *sSummary =
        "## Goal\nContinue the code-agent implementation.\n"
        "## Constraints & Preferences\nPreserve the pinned contract, exact paths, and completed "
        "tool outcomes.\n"
        "## Progress\nRounds 1-4 inspected modules and preserved tool outcomes.\n"
        "## Key Decisions\nCompact only a safe completed prefix and retain recent turns verbatim.\n"
        "## Next Steps\nContinue from the retained messages (rounds 5-6).\n"
        "## Critical Context\nRecent rounds 5-6 remain verbatim; read-files: module_1.c; "
        "modified-files: none.";
    xllm_session_config tConfig;
    xllm_session *pSession;
    xllm_session *pLoaded = NULL;
    xllm_session_stats tBefore;
    xllm_session_stats tAfter;
    xllm_session_stats tLoaded;
    xllm_request tRequest;
    xllm_compaction *pCompaction = NULL;
    xllm_error tError;
    char *psOutputs[6] = {0};
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
    if (!pSession)
    {
        return;
    }
    SESSION_CHECK(xllmSessionAddText(pSession, 0u, XLLM_ROLE_SYSTEM,
                                     "Pinned agent contract and repository constraints.",
                                     XLLM_SESSION_ENTRY_PINNED),
                  "pinned system contract added");
    for (i = 0u; i < 6u; ++i)
    {
        uint64_t uTurn = xllmSessionBeginTurn(pSession);
        psOutputs[i] = make_large_output(1200u, i);
        SESSION_CHECK(psOutputs[i] != NULL && add_tool_round(pSession, uTurn, i + 1u, psOutputs[i]),
                      "complete tool round added");
    }
    SESSION_CHECK(xllmSessionGetStats(pSession, &tBefore), "session pressure stats available");
    SESSION_CHECK(tBefore.uPendingToolCalls == 0u, "completed tool rounds have no pending calls");
    SESSION_CHECK(tBefore.ePressure == XLLM_SESSION_PRESSURE_PRUNE ||
                      tBefore.ePressure == XLLM_SESSION_PRESSURE_COMPACT,
                  "large history activates offline pressure policy");
    SESSION_CHECK(tBefore.uRenderedActiveTokens < tBefore.uRawActiveTokens,
                  "old large tool outputs are soft-pruned");

    xllmRequestInit(&tRequest);
    SESSION_CHECK(xllmSessionBuildRequest(pSession, &tRequest, &tError),
                  "active request renders before compaction");
    SESSION_CHECK(tRequest.uMaxOutputTokens == 300u,
                  "session output budget propagates to model request");
    SESSION_CHECK(request_has_text(&tRequest, "older tool output pruned"),
                  "rendered request marks pruned tool output");
    SESSION_CHECK(tRequest.iMessageCount == 19u,
                  "soft pruning preserves message and tool-call structure");
    xllmRequestUnit(&tRequest);

    pCompaction = xllmSessionPrepareCompaction(pSession, true, &tError);
    SESSION_CHECK(pCompaction != NULL, "forced compaction transaction prepares");
    SESSION_CHECK(pCompaction && strstr(xllmCompactionPrompt(pCompaction), "## Goal") != NULL,
                  "compaction prompt encodes the Pi summary contract");
    SESSION_CHECK(pCompaction &&
                      strstr(xllmCompactionPrompt(pCompaction), "[Tool result call_1") != NULL,
                  "candidate serialization tags tool results");
    SESSION_CHECK(pCompaction && xllmCompactionEstimatedTokens(pCompaction) > 0u,
                  "compaction prompt token estimate available");
    uThrough = xllmCompactionThroughSequence(pCompaction);
    SESSION_CHECK(uThrough > 0u, "compaction selects a completed prefix");
    SESSION_CHECK(pCompaction &&
                      xllmSessionCommitCompaction(pSession, pCompaction, sSummary, &tError),
                  "compaction summary commits atomically");
    xllmCompactionDestroy(pCompaction);
    pCompaction = NULL;
    SESSION_CHECK(xllmSessionGetStats(pSession, &tAfter), "post-compaction stats available");
    SESSION_CHECK(tAfter.uCompactionCount == 1u && tAfter.uCompactedThroughSequence == uThrough,
                  "compaction checkpoint advances exactly once");
    SESSION_CHECK(tAfter.uRawActiveTokens < tBefore.uRawActiveTokens,
                  "compaction reduces active token ledger");

    xllmRequestInit(&tRequest);
    SESSION_CHECK(xllmSessionBuildRequest(pSession, &tRequest, &tError),
                  "request renders after compaction");
    SESSION_CHECK(request_has_text(&tRequest, "Compacted session state"),
                  "request contains authoritative summary checkpoint");
    SESSION_CHECK(request_has_text(&tRequest, "Round 5") && request_has_text(&tRequest, "Round 6"),
                  "recent turns remain verbatim after compaction");
    SESSION_CHECK(!request_has_text(&tRequest, "Round 1"),
                  "covered old turns leave the active request");
    xllmRequestUnit(&tRequest);

    SESSION_CHECK(xllmSessionSave(pSession, sStatePath, &tError),
                  "session snapshot writes atomically");
    pLoaded = xllmSessionLoad(sStatePath, &tError);
    SESSION_CHECK(pLoaded != NULL, "session snapshot loads");
    SESSION_CHECK(pLoaded && xllmSessionGetStats(pLoaded, &tLoaded) &&
                      tLoaded.uCompactionCount == 1u &&
                      tLoaded.uCompactedThroughSequence == uThrough,
                  "loaded session preserves compaction checkpoint");
    SESSION_CHECK(pLoaded && xllmSessionGetStats(pLoaded, &tLoaded) && !tLoaded.bFillExactValid,
                  "restored session reports unknown fill until the next call");
    if (pLoaded)
    {
        xllm_session *pFork = xllmSessionFork(pLoaded, &tError);
        SESSION_CHECK(pFork != NULL, "loaded compacted session forks");
        SESSION_CHECK(pFork && xllmSessionGetStats(pFork, &tLoaded) &&
                          tLoaded.uCompactionCount == 1u &&
                          tLoaded.uCompactedThroughSequence == uThrough,
                      "fork preserves compaction checkpoint");
        if (pFork)
        {
            uint64_t uParentTurn = xllmSessionBeginTurn(pLoaded);
            uint64_t uForkTurn = xllmSessionBeginTurn(pFork);
            SESSION_CHECK(xllmSessionAddText(pLoaded, uParentTurn, XLLM_ROLE_USER,
                                             "Continue on parent branch.", 0u) &&
                              xllmSessionAddText(pFork, uForkTurn, XLLM_ROLE_USER,
                                                 "Explore an isolated branch.", 0u),
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
        SESSION_CHECK(xllmSessionAddText(pLoaded, uTurn, XLLM_ROLE_USER,
                                         "Continue after process restart.", 0u),
                      "loaded session accepts a continuation turn");
        xllmRequestInit(&tRequest);
        SESSION_CHECK(xllmSessionBuildRequest(pLoaded, &tRequest, &tError),
                      "loaded session renders continuation request");
        SESSION_CHECK(request_has_text(&tRequest, "Rounds 1-4 inspected modules") &&
                          request_has_text(&tRequest, "Continue after process restart"),
                      "summary continuity survives restart");
        xllmRequestUnit(&tRequest);
        {
            /* Standalone fixture: the invariant under test is that an
             * unresolved tool-call turn never becomes a candidate. */
            xllm_session *pPending;
            xllm_tool_call tPendingCall;
            xllm_response tPendingResponse;
            xllm_compaction *pPendingCompaction;
            uint64_t uPendingTurn;
            tConfig.uKeepRecentTokens = 16u;
            pPending = xllmSessionCreate(&tConfig, &tError);
            tConfig.uKeepRecentTokens = 0u;
            SESSION_CHECK(pPending != NULL, "pending fixture session creates");
            if (pPending)
            {
                (void)xllmSessionAddText(pPending, 0u, XLLM_ROLE_SYSTEM, "pinned contract.",
                                         XLLM_SESSION_ENTRY_PINNED);
                uPendingTurn = xllmSessionBeginTurn(pPending);
                SESSION_CHECK(xllmSessionAddText(pPending, uPendingTurn, XLLM_ROLE_USER,
                                                 "Start the objective.", 0u) &&
                                  xllmSessionAddText(pPending, uPendingTurn, XLLM_ROLE_ASSISTANT,
                                                     "objective captured", 0u),
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
                SESSION_CHECK(
                    xllmSessionAddAssistantResponse(pPending, uPendingTurn, &tPendingResponse),
                    "pending assistant tool call added");
                SESSION_CHECK(xllmSessionGetStats(pPending, &tLoaded) &&
                                  tLoaded.uPendingToolCalls == 1u,
                              "pending tool call tracked");
                pPendingCompaction = xllmSessionPrepareCompaction(pPending, true, &tError);
                SESSION_CHECK(
                    pPendingCompaction != NULL &&
                        strstr(xllmCompactionPrompt(pPendingCompaction), "pending_call") == NULL &&
                        strstr(xllmCompactionPrompt(pPendingCompaction),
                               "[User]: Start the objective.") != NULL,
                    "compaction excludes unresolved tool-call turn");
                xllmCompactionDestroy(pPendingCompaction);
                xllmSessionDestroy(pPending);
            }
        }
    }
    for (i = 0u; i < 6u; ++i)
    {
        free(psOutputs[i]);
    }
    xllmSessionDestroy(pLoaded);
    xllmSessionDestroy(pSession);
    (void)xrtFileDelete((str)sStatePath);
}

static void test_journal_checkpoint_recovery(void)
{
    static const char sSnapshotPath[] = "out/mdo-integration/fixtures/session_recovery_test.json";
    static const char sJournalPath[] = "out/mdo-integration/fixtures/session_recovery_test.ndjson";
    const char *sSummary =
        "## Goal\nPreserve a crash-safe coding session.\n"
        "## Constraints & Preferences\nJournal acknowledged mutations before exposing them to "
        "callers.\n"
        "## Progress\nThe initial journaled turns were recovered.\n"
        "## Key Decisions\nRecover from the snapshot plus a valid journal prefix.\n"
        "## Next Steps\nContinue from the recovered checkpoint.\n"
        "## Critical Context\nThe latest branch remains verbatim; torn tails are ignored, corrupt "
        "records rejected.";
    xllm_session_config tConfig;
    xllm_session *pSession = NULL;
    xllm_session *pRecovered = NULL;
    xllm_session *pStale = NULL;
    xllm_session *pCorrupt = NULL;
    xllm_compaction *pCompaction = NULL;
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
    if (!pSession || !xllmSessionJournalPath(pSession))
        goto done;

    uTurn = xllmSessionBeginTurn(pSession);
    SESSION_CHECK(
        uTurn &&
            xllmSessionAddText(pSession, uTurn, XLLM_ROLE_SYSTEM, "Crash-safe agent contract.",
                               XLLM_SESSION_ENTRY_PINNED) &&
            xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, "Start durable work.", 0u),
        "first turn is journaled before acknowledgement");
    uTurn = xllmSessionBeginTurn(pSession);
    SESSION_CHECK(
        uTurn &&
            xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, "Implement the first slice.", 0u) &&
            xllmSessionAddText(pSession, uTurn, XLLM_ROLE_ASSISTANT, "First slice complete.", 0u),
        "second turn is journaled");
    SESSION_CHECK(xllmSessionGetStats(pSession, &tStats) && tStats.bJournalEnabled &&
                      tStats.uJournalSequence == 6u,
                  "journal sequence covers turns and messages");

    pRecovered = xllmSessionRecover(sSnapshotPath, sJournalPath, &tConfig, &tError);
    SESSION_CHECK(pRecovered != NULL, "journal-only process recovery succeeds");
    xllmRequestInit(&tRequest);
    SESSION_CHECK(pRecovered && xllmSessionBuildRequest(pRecovered, &tRequest, &tError) &&
                      request_has_text(&tRequest, "Start durable work") &&
                      request_has_text(&tRequest, "First slice complete"),
                  "journal-only recovery preserves conversation state");
    xllmRequestUnit(&tRequest);
    xllmSessionDestroy(pSession);
    pSession = NULL;
    if (!pRecovered)
        goto done;

    SESSION_CHECK(xllmSessionCheckpoint(pRecovered, sSnapshotPath, &tError),
                  "checkpoint atomically snapshots and retires covered journal");
    SESSION_CHECK(!xrtFileExists((str)sJournalPath), "covered journal is removed after checkpoint");
    uTurn = xllmSessionBeginTurn(pRecovered);
    SESSION_CHECK(uTurn &&
                      xllmSessionAddText(pRecovered, uTurn, XLLM_ROLE_USER,
                                         "Continue after checkpoint.", 0u) &&
                      xllmSessionAddText(pRecovered, uTurn, XLLM_ROLE_ASSISTANT,
                                         "Checkpoint continuation complete.", 0u),
                  "post-checkpoint delta is journaled");
    uTurn = xllmSessionBeginTurn(pRecovered);
    SESSION_CHECK(uTurn &&
                      xllmSessionAddText(pRecovered, uTurn, XLLM_ROLE_USER,
                                         "Keep this latest branch verbatim.", 0u) &&
                      xllmSessionAddText(pRecovered, uTurn, XLLM_ROLE_ASSISTANT,
                                         "Latest branch retained.", 0u),
                  "latest recovery turn is journaled");
    pCompaction = xllmSessionPrepareCompaction(pRecovered, true, &tError);
    SESSION_CHECK(pCompaction &&
                      xllmSessionCommitCompaction(pRecovered, pCompaction, sSummary, &tError),
                  "compaction checkpoint is journaled");
    xllmCompactionDestroy(pCompaction);
    pCompaction = NULL;

    iCompleteSize = (size_t)test_path_size(sJournalPath);
    SESSION_CHECK(xrtFileAppend(sJournalPath, (xbytesview){(const uint8 *)"\"} torn", 8u}) > 0 ||
                      true,
                  "test simulates a torn final journal record");
    SESSION_CHECK(xrtFileAppend(sJournalPath, (xbytesview){(const uint8 *)"{\"partial\":", 11u}),
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
    SESSION_CHECK(pStale && xllmSessionGetStats(pStale, &tStats) && tStats.uCompactionCount == 1u &&
                      tStats.uCurrentTurn == 4u,
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
                          xllmSessionAddText(pRecovered, uTurn, XLLM_ROLE_USER,
                                             "Resume after a tool-batch crash.", 0u) &&
                          xllmSessionAddAssistantResponse(pRecovered, uTurn, &tResponse),
                      "interrupted parallel tool batch is journaled");
        SESSION_CHECK(xllmSessionCheckpoint(pRecovered, sSnapshotPath, &tError),
                      "assistant tool batch is checkpointed before simulated crash");
        SESSION_CHECK(xllmSessionAddToolResult(pRecovered, uTurn, "recovered_done", "listed"),
                      "first tool result is journaled before simulated crash");
        pStale = xllmSessionRecover(sSnapshotPath, sJournalPath, NULL, &tError);
        SESSION_CHECK(pStale && xllmSessionPendingToolCallCount(pStale) == 1u &&
                          xllmSessionPendingToolCallAt(pStale, 0u, &tPendingCall) &&
                          tPendingCall.uTurn == uTurn &&
                          strcmp(tPendingCall.sId, "recovered_pending") == 0 &&
                          strcmp(tPendingCall.sName, "exec_command") == 0 &&
                          strcmp(tPendingCall.sArgumentsJson, "{\"command\":\"verify\"}") == 0,
                      "recovery exposes only the unresolved tool call with its original turn");
        SESSION_CHECK(xllmSessionGetTail(pStale, &tTail) && tTail.bHasMessage &&
                          tTail.uTurn == uTurn && tTail.eRole == XLLM_ROLE_TOOL,
                      "lightweight recovered tail identifies the completed tool result without "
                      "rendering context");
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
                                              "This valid record precedes corruption.", 0u),
                  "valid delta precedes corrupt record");
    SESSION_CHECK(xrtFileAppend(sJournalPath, (xbytesview){(const uint8 *)"not-json\n", 9u}),
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

/* ------------------------------------------------------------------ */
/* SES-105: persistence v3 checksum/durability and v1/v2 migration.    */
/* ------------------------------------------------------------------ */

static void test_persistence_v3_migration(void)
{
    static const char sMissingSnapshot[] = "out/mdo-integration/fixtures/migration_missing.json";
    static const char sJournalCopy[] = "out/mdo-integration/fixtures/migration_legacy.jsonl";
    static const char sSnapshotV3[] = "out/mdo-integration/fixtures/migration_v3.json";
    static const char sJournalV3[] = "out/mdo-integration/fixtures/migration_v3.jsonl";
    xllm_session_config tConfig;
    xllm_session_config tLoadedConfig;
    xllm_session *pSession = NULL;
    xllm_session *pLoaded = NULL;
    xllm_request tRequest;
    xllm_error tError;
    char *pData = NULL;
    size_t iSize = 0u;
    char *pLineEnd;
    char *pNeedle;
    uint64_t uTurn;

    (void)xrtFileDelete((str)sMissingSnapshot);
    (void)xrtFileDelete((str)sJournalCopy);
    (void)xrtFileDelete((str)sSnapshotV3);
    (void)xrtFileDelete((str)sJournalV3);

    pLoaded = xllmSessionLoad("extlibs/xllm-session/tests/fixtures/snapshot-v1.json", &tError);
    xllmRequestInit(&tRequest);
    SESSION_CHECK(pLoaded && xllmSessionBuildRequest(pLoaded, &tRequest, &tError) &&
                      request_has_text(&tRequest, "legacy snapshot v1") &&
                      xllmSessionGetConfig(pLoaded, &tLoadedConfig) &&
                      tLoadedConfig.eJournalDurability == XLLM_SESSION_DURABILITY_FLUSH,
                  "v1 snapshot fixture migrates to v3 defaults");
    xllmRequestUnit(&tRequest);
    xllmSessionDestroy(pLoaded);
    pLoaded = NULL;

    pLoaded = xllmSessionLoad("extlibs/xllm-session/tests/fixtures/snapshot-v2.json", &tError);
    xllmRequestInit(&tRequest);
    SESSION_CHECK(pLoaded && xllmSessionBuildRequest(pLoaded, &tRequest, &tError) &&
                      request_has_text(&tRequest, "legacy snapshot v2"),
                  "v2 snapshot fixture migrates without data loss");
    xllmRequestUnit(&tRequest);
    xllmSessionDestroy(pLoaded);
    pLoaded = NULL;

    SESSION_CHECK(test_copy_file("extlibs/xllm-session/tests/fixtures/journal-v1.jsonl", sJournalCopy),
                  "v1 journal fixture copied for replay");
    pLoaded = xllmSessionRecover(sMissingSnapshot, sJournalCopy, NULL, &tError);
    xllmRequestInit(&tRequest);
    SESSION_CHECK(pLoaded && xllmSessionBuildRequest(pLoaded, &tRequest, &tError) &&
                      request_has_text(&tRequest, "legacy journal v1"),
                  "v1 journal fixture replays through migration path");
    xllmRequestUnit(&tRequest);
    xllmSessionDestroy(pLoaded);
    pLoaded = NULL;
    (void)xrtFileDelete((str)sJournalCopy);

    SESSION_CHECK(test_copy_file("extlibs/xllm-session/tests/fixtures/journal-v2.jsonl", sJournalCopy),
                  "v2 journal fixture copied for replay");
    pLoaded = xllmSessionRecover(sMissingSnapshot, sJournalCopy, NULL, &tError);
    xllmRequestInit(&tRequest);
    SESSION_CHECK(pLoaded && xllmSessionBuildRequest(pLoaded, &tRequest, &tError) &&
                      request_has_text(&tRequest, "legacy journal v2"),
                  "v2 journal fixture replays through migration path");
    xllmRequestUnit(&tRequest);
    uTurn = pLoaded ? xllmSessionBeginTurn(pLoaded) : 0u;
    SESSION_CHECK(
        uTurn && xllmSessionAddText(pLoaded, uTurn, XLLM_ROLE_USER, "v3 continuation after v2", 0u),
        "recovered v2 journal accepts a v3 continuation");
    xllmSessionDestroy(pLoaded);
    pLoaded = NULL;
    pLoaded = xllmSessionRecover(sMissingSnapshot, sJournalCopy, NULL, &tError);
    xllmRequestInit(&tRequest);
    SESSION_CHECK(pLoaded && xllmSessionBuildRequest(pLoaded, &tRequest, &tError) &&
                      request_has_text(&tRequest, "legacy journal v2") &&
                      request_has_text(&tRequest, "v3 continuation after v2"),
                  "mixed v2 and v3 journal prefix replays exactly once");
    xllmRequestUnit(&tRequest);
    xllmSessionDestroy(pLoaded);
    pLoaded = NULL;
    (void)xrtFileDelete((str)sJournalCopy);

    xllmSessionConfigInit(&tConfig);
    tConfig.eJournalDurability = XLLM_SESSION_DURABILITY_APPEND;
    pSession = xllmSessionCreate(&tConfig, &tError);
    SESSION_CHECK(pSession && xllmSessionEnableJournal(pSession, sJournalV3, &tError),
                  "v3 append-policy journal attaches");
    uTurn = pSession ? xllmSessionBeginTurn(pSession) : 0u;
    SESSION_CHECK(uTurn && xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER,
                                              "checksummed persistence v3", 0u),
                  "v3 records append before publication");
    SESSION_CHECK(xllmSessionFlushJournal(pSession, &tError),
                  "append policy exposes an explicit durable flush barrier");
    pData = (char *)xrtFileReadAll(sJournalV3, &iSize);
    pLineEnd = pData ? strchr(pData, '\n') : NULL;
    SESSION_CHECK(pData && pLineEnd && strstr(pData, "\"version\":4") &&
                      strstr(pData, "\"sequence\":1") && strstr(pData, "\"type\":\"begin_turn\"") &&
                      xllm_session__checksum_verify(pData, (size_t)(pLineEnd - pData)),
                  "v3 journal record carries version sequence type and valid checksum");
    xrtFree(pData);
    pData = NULL;

    SESSION_CHECK(xllmSessionSave(pSession, sSnapshotV3, &tError),
                  "v3 snapshot writes through flushed atomic replacement");
    pData = (char *)xrtFileReadAll(sSnapshotV3, &iSize);
    SESSION_CHECK(pData && strstr(pData, "\"version\":4") &&
                      strstr(pData, "\"checkpoint_sequence\":2") &&
                      strstr(pData, "\"journal_durability\":0") &&
                      xllm_session__checksum_verify(pData, iSize),
                  "v3 snapshot carries checkpoint policy and valid checksum");
    pLoaded = xllmSessionLoad(sSnapshotV3, &tError);
    SESSION_CHECK(pLoaded && xllmSessionGetConfig(pLoaded, &tLoadedConfig) &&
                      tLoadedConfig.eJournalDurability == XLLM_SESSION_DURABILITY_APPEND,
                  "v3 snapshot restores the durability policy");
    xllmSessionDestroy(pLoaded);
    pLoaded = NULL;

    pNeedle = pData ? strstr(pData, "checksummed persistence v3") : NULL;
    if (pNeedle)
    {
        pNeedle[0] = 'C';
    }
    SESSION_CHECK(pNeedle &&
                      xrtFileWriteAll(sSnapshotV3, (xbytesview){(const uint8 *)pData, iSize}),
                  "test corrupts one complete v3 snapshot byte");
    xrtFree(pData);
    pData = NULL;
    pLoaded = xllmSessionLoad(sSnapshotV3, &tError);
    SESSION_CHECK(pLoaded == NULL && tError.eCode == XLLM_ERROR_PARSE,
                  "v3 snapshot checksum damage is rejected");
    xllmSessionDestroy(pLoaded);
    pLoaded = NULL;

    xllmSessionDestroy(pSession);
    pSession = NULL;
    pData = (char *)xrtFileReadAll(sJournalV3, &iSize);
    pNeedle = pData ? strstr(pData, "begin_turn") : NULL;
    if (pNeedle)
    {
        pNeedle[0] = 'B';
    }
    SESSION_CHECK(pNeedle && xrtFileWriteAll(sJournalV3, (xbytesview){(const uint8 *)pData, iSize}),
                  "test corrupts one complete v3 journal record");
    xrtFree(pData);
    pData = NULL;
    pLoaded = xllmSessionRecover(sMissingSnapshot, sJournalV3, &tConfig, &tError);
    SESSION_CHECK(pLoaded == NULL && tError.eCode == XLLM_ERROR_PARSE,
                  "v3 complete journal checksum damage is rejected");

    xllmSessionDestroy(pLoaded);
    xllmSessionDestroy(pSession);
    xrtFree(pData);
    (void)xrtFileDelete((str)sJournalV3);
    (void)xrtFileDelete((str)sSnapshotV3);
    (void)xrtFileDelete((str)sJournalCopy);
    (void)xrtFileDelete((str)sMissingSnapshot);
}

static bool test_split_plan_override(xllm_session *, uint64_t, xllm_compaction_plan *, void *);

/* ------------------------------------------------------------------ */
/* v3: split-turn compaction (GAP-SPLIT-TURN)                          */
/* ------------------------------------------------------------------ */

static void test_split_turn(void)
{
    xllm_session_config tConfig;
    xllm_session *pSession;
    xllm_compaction *pCompaction = NULL;
    xllm_session_stats tStats;
    xllm_request tRequest;
    xllm_error tError;
    char *sFatEarly = make_large_output(4000u, 1u);
    char *sFatLate = make_large_output(4000u, 2u);
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
    if (!pSession || !sFatEarly || !sFatLate)
    {
        free(sFatEarly);
        free(sFatLate);
        return;
    }

    (void)xllmSessionAddText(pSession, 0u, XLLM_ROLE_SYSTEM, "pinned contract.",
                             XLLM_SESSION_ENTRY_PINNED);
    uTurn = xllmSessionBeginTurn(pSession);
    SESSION_CHECK(
        xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, "compile the module now", 0u) &&
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
    SESSION_CHECK(
        xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, "summarize the build", 0u) &&
            xllmSessionAddText(pSession, uTurn, XLLM_ROLE_ASSISTANT, "the build succeeded", 0u),
        "split fixture: small recent turn");

    pCompaction = xllmSessionPrepareCompaction(pSession, true, &tError);
    SESSION_CHECK(pCompaction != NULL, "split-turn compaction prepares");
    if (pCompaction)
    {
        const char *sPrompt = xllmCompactionPrompt(pCompaction);
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
        SESSION_CHECK(xllmSessionCommitCompaction(
                          pSession, pCompaction,
                          "## Goal\ncompile the module\n"
                          "## Constraints & Preferences\npinned contract\n"
                          "## Progress\nearly work summarized; build ok\n"
                          "## Key Decisions\nsplit the oversized turn\n"
                          "## Next Steps\ncontinue from the late work\n"
                          "## Critical Context\ntool pair preserved; read-files: none",
                          &tError),
                      "split-turn compaction commits");
        xllmCompactionDestroy(pCompaction);
        pCompaction = NULL;
    }
    SESSION_CHECK(xllmSessionGetStats(pSession, &tStats) && tStats.uCompactionCount == 1u &&
                      tStats.uCompactedThroughSequence == uPrefixThrough,
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
    for (size_t i = 0u; i < tRequest.iMessageCount; ++i)
    {
        SESSION_CHECK(!(tRequest.pMessages[i].sContent &&
                        strstr(tRequest.pMessages[i].sContent, "truncated by overflow")),
                      "no L2 truncation marker: the summary path won");
        if (tRequest.pMessages[i].sContent &&
            strstr(tRequest.pMessages[i].sContent, "truncated by overflow"))
        {
            break;
        }
    }
    xllmRequestUnit(&tRequest);

    /* custom ops can replace the planning behavior (split plans included) */
    {
        xllm_compaction_ops tOps;
        memset(&tOps, 0, sizeof(tOps));
        tOps.pPlan = test_split_plan_override;
        SESSION_CHECK(xllmSessionSetCompactionOps(pSession, &tOps), "custom split ops installs");
        pCompaction = xllmSessionPrepareCompaction(pSession, true, &tError);
        SESSION_CHECK(pCompaction != NULL && strstr(xllmCompactionPrompt(pCompaction),
                                                    "Turn Context (split turn):") != NULL,
                      "driver honors a custom split plan");
        xllmCompactionDestroy(pCompaction);
        (void)xllmSessionSetCompactionOps(pSession, NULL);
    }
    free(sFatEarly);
    free(sFatLate);
    xllmSessionDestroy(pSession);
}

static bool test_split_plan_override(xllm_session *pSession, uint64_t uPrevThrough,
                                     xllm_compaction_plan *pPlan, void *pUserData)
{
    size_t i;
    (void)pUserData;
    pPlan->uThroughSequence = uPrevThrough;
    pPlan->uPrefixThroughSequence = 0u;
    /* force a fresh split over whatever active prefix exists */
    for (i = 0u; i < pSession->iEntryCount; ++i)
    {
        const xllm_session_entry *pEntry = &pSession->pEntries[i];
        if ((pEntry->uFlags & XLLM_SESSION_ENTRY_PINNED) != 0u)
        {
            continue;
        }
        if (pEntry->uSequence <= uPrevThrough)
        {
            continue;
        }
        if (pEntry->tMessage.eRole == XLLM_ROLE_ASSISTANT &&
            pEntry->uSequence > pPlan->uPrefixThroughSequence)
        {
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
    xllm_session *pSession;
    xllm_session *pFork = NULL;
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
    if (!pSession)
    {
        return;
    }
    SESSION_CHECK(xllmSessionGetStats(pSession, &tStats) && !tStats.bFillExactValid &&
                      tStats.uFillExact == UINT64_MAX &&
                      tStats.ePressure == XLLM_SESSION_PRESSURE_NONE,
                  "fresh session reports unknown fill and no pressure");
    memset(&tUsage, 0, sizeof(tUsage));
    tUsage.uInputTokens = 100u;
    tUsage.uOutputTokens = 50u;
    tUsage.uCachedInputTokens = 60u;
    SESSION_CHECK(xllmSessionRecordUsage(pSession, &tUsage), "usage feedback accepted");
    SESSION_CHECK(xllmSessionGetStats(pSession, &tStats) && tStats.bFillExactValid &&
                      tStats.uFillExact == 150u && tStats.uCachedInputTokens == 60u &&
                      tStats.uIncrementMax == 0u &&
                      tStats.ePressure == XLLM_SESSION_PRESSURE_NONE,
                  "exact fill recorded with a bounded increment envelope");
    /* budget = 8000 - 1000 - 500 = 6500; soft = 0.95 * 6500 = 6175 */
    tUsage.uInputTokens = 6200u;
    tUsage.uOutputTokens = 10u;
    SESSION_CHECK(xllmSessionRecordUsage(pSession, &tUsage) &&
                      xllmSessionGetStats(pSession, &tStats) &&
                      tStats.ePressure == XLLM_SESSION_PRESSURE_COMPACT,
                  "soft threshold crosses into compact pressure");
    tUsage.uInputTokens = 6500u;
    tUsage.uOutputTokens = 100u;
    SESSION_CHECK(xllmSessionRecordUsage(pSession, &tUsage) &&
                      xllmSessionGetStats(pSession, &tStats) &&
                      tStats.ePressure == XLLM_SESSION_PRESSURE_OVERFLOW,
                  "fill plus increment beyond the input budget is overflow");
    tUsage.uInputTokens = 100u;
    tUsage.uOutputTokens = 50u;
    SESSION_CHECK(xllmSessionRecordUsage(pSession, &tUsage), "usage back to sparse");
    pFork = xllmSessionFork(pSession, &tError);
    SESSION_CHECK(pFork && xllmSessionGetStats(pFork, &tStats) && tStats.bFillExactValid,
                  "unchanged fork retains the usage prefix anchor");
    xllmSessionDestroy(pFork);
    xllmSessionDestroy(pSession);
}

/* ------------------------------------------------------------------ */
/* v3: ops table and render/event hooks                                 */
/* ------------------------------------------------------------------ */

typedef struct
{
    xllm_session_event_type aEvents[64];
    size_t iEvents;
    bool bReenter;
} test_event_sink;

static void test_on_event(xllm_session *pSession, const xllm_session_event *pEvent, void *pUserData)
{
    test_event_sink *pSink = (test_event_sink *)pUserData;
    (void)pSession;
    if (pSink->iEvents < 64u)
    {
        pSink->aEvents[pSink->iEvents++] = pEvent->eType;
    }
}

static bool test_has_event(const test_event_sink *pSink, xllm_session_event_type eType)
{
    size_t i;
    for (i = 0u; i < pSink->iEvents; ++i)
    {
        if (pSink->aEvents[i] == eType)
        {
            return true;
        }
    }
    return false;
}

static xllm_render_action test_skip_users(xllm_session *pSession, uint64_t uSequence,
                                          uint64_t uTurn, uint32_t uFlags, xllm_message *pWork,
                                          void *pUserData)
{
    (void)pSession;
    (void)uSequence;
    (void)uTurn;
    (void)pWork;
    (void)pUserData;
    return (uFlags & XLLM_SESSION_ENTRY_PINNED) == 0u && pWork->eRole == XLLM_ROLE_USER
               ? XLLM_RENDER_SKIP
               : XLLM_RENDER_KEEP;
}

static xllm_render_action test_skip_assistant(xllm_session *pSession, uint64_t uSequence,
                                              uint64_t uTurn, uint32_t uFlags, xllm_message *pWork,
                                              void *pUserData)
{
    (void)pSession;
    (void)uSequence;
    (void)uTurn;
    (void)uFlags;
    (void)pUserData;
    return pWork->eRole == XLLM_ROLE_ASSISTANT && pWork->iToolCallCount > 0u ? XLLM_RENDER_SKIP
                                                                             : XLLM_RENDER_KEEP;
}

static bool test_summary_as_system(xllm_session *pSession, const xllm_session_summary *pSummary,
                                   xllm_message *pWork, void *pUserData)
{
    (void)pSession;
    (void)pSummary;
    (void)pUserData;
    if (pWork->eRole != XLLM_ROLE_USER)
    {
        return true;
    }
    pWork->eRole = XLLM_ROLE_SYSTEM;
    return xllmMessageSetContent(pWork, "[summary]");
}

static bool test_append_marker(xllm_session *pSession, xllm_request *pRequest, void *pUserData)
{
    (void)pSession;
    (void)pUserData;
    return xllmRequestAddTextMessage(pRequest, XLLM_ROLE_USER, "ephemeral tail context");
}

static bool test_serialize_wrapped(xllm_session *pSession, uint64_t uFrom, uint64_t uTo,
                                   char **psText, void *pUserData)
{
    char *sBase;
    xllm_session_buf tBuf = {0};
    bool bOk;
    (void)pUserData;
    sBase = xllm_session__serialize_candidates(pSession, uFrom, uTo);
    if (!sBase)
    {
        return false;
    }
    bOk = xllm_session__buf_cstr(&tBuf, "[[[\n") && xllm_session__buf_cstr(&tBuf, sBase) &&
          xllm_session__buf_cstr(&tBuf, "]]]\n");
    free(sBase);
    *psText = bOk ? xllm_session__buf_detach(&tBuf) : NULL;
    xllm_session__buf_unit(&tBuf);
    return *psText != NULL;
}

static void test_ops_and_hooks(void)
{
    xllm_session_config tConfig;
    xllm_session *pSession;
    xllm_compaction *pA;
    xllm_compaction *pB;
    xllm_request tRequest;
    xllm_error tError;
    xllm_compaction_ops tOps;
    xllm_session_hooks tHooks;
    test_event_sink tSink = {0};
    xllm_tool_call tCall;
    xllm_response tResponse;
    uint64_t uTurn;
    char *sDefaultPrompt = NULL;

    xllmSessionConfigInit(&tConfig);
    tConfig.uContextWindowTokens = 8000u;
    tConfig.uMaxOutputTokens = 1000u;
    tConfig.uOutputReserveTokens = 1000u;
    tConfig.uSafetyReserveTokens = 500u;
    tConfig.uKeepRecentTokens = 16u;
    pSession = xllmSessionCreate(&tConfig, &tError);
    SESSION_CHECK(pSession != NULL, "hooks session creates");
    if (!pSession)
    {
        return;
    }
    uTurn = xllmSessionBeginTurn(pSession);
    (void)xllmSessionAddText(pSession, uTurn, XLLM_ROLE_SYSTEM, "pinned",
                             XLLM_SESSION_ENTRY_PINNED);
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
    if (pA)
    {
        sDefaultPrompt = xllm_session__strdup(xllmCompactionPrompt(pA));
    }
    xllmCompactionDestroy(pA);
    pA = NULL;
    SESSION_CHECK(xllmSessionSetCompactionOps(pSession, xllmSessionDefaultCompactionOps()),
                  "default ops table installs");
    pB = xllmSessionPrepareCompaction(pSession, true, &tError);
    SESSION_CHECK(sDefaultPrompt && pB && strcmp(sDefaultPrompt, xllmCompactionPrompt(pB)) == 0,
                  "NULL ops and the default table render byte-identical prompts");
    xllmCompactionDestroy(pB);
    free(sDefaultPrompt);

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
    SESSION_CHECK(
        pA && xllmSessionCommitCompaction(pSession, pA, test_pi_summary("hook coverage"), &tError),
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
    xllm_session *pSession;
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
    if (pSession)
    {
        for (i = 0u; i < 6u; ++i)
        {
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
        SESSION_CHECK(xllmSessionGetStats(pSession, &tStats) && tStats.uSummaryGeneration == 1u &&
                          tStats.ePressure != XLLM_SESSION_PRESSURE_OVERFLOW,
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
    if (pSession)
    {
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

typedef struct
{
    const char *sContent; /* reply for normal calls */
    uint64_t uIn, uOut;
    unsigned iCalls;
} test_script;
static char g_sMetaRoutingKey[64];
static size_t g_iMetaHeaderCount;

static xllm_result test_script_call(void *pUserData, const xllm_request *pRequest,
                                    const xllm_stream_callbacks *pCallbacks,
                                    xllm_response **ppResponse, xllm_error *pError)
{
    test_script *pScript = (test_script *)pUserData;
    size_t i;
    bool bSummaryCall = false;
    (void)pCallbacks;
    for (i = 0u; i < pRequest->iMessageCount; ++i)
    {
        if (pRequest->pMessages[i].sContent &&
            strstr(pRequest->pMessages[i].sContent, "<conversation>"))
        {
            bSummaryCall = true;
        }
    }
    ++pScript->iCalls;
    if (bSummaryCall && pRequest->iExtraHeaderCount)
    {
        g_iMetaHeaderCount = pRequest->iExtraHeaderCount;
        if (pRequest->pExtraHeaders[0].sValue)
        {
            (void)snprintf(g_sMetaRoutingKey, sizeof(g_sMetaRoutingKey), "%s",
                           pRequest->pExtraHeaders[0].sValue);
        }
    }
    if (pError)
    {
        xllmErrorInit(pError);
    }
    *ppResponse = test_make_response(bSummaryCall ? test_pi_summary("scripted auto compaction")
                                                  : pScript->sContent,
                                     pScript->uIn, pScript->uOut);
    return *ppResponse ? XLLM_RESULT_OK : XLLM_RESULT_ERROR;
}

static void test_easy_send(void)
{
    xllm_session_config tConfig;
    xllm_session *pSession;
    xllm_session_stats tStats;
    xllm_response *pResponse = NULL;
    xllm_error tError;
    test_script tScript = {"model reply", 5000u, 500u, 0u};
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
    if (!pSession)
    {
        return;
    }

    {
        xllm_request tDirect;
        xllm_session *pForwarded = xllmSessionCreate(&tConfig, &tError);
        xllmRequestInit(&tDirect);
        SESSION_CHECK(
            pForwarded && !xllmSessionHasDriver(pForwarded) && xllmSessionHasDriver(pSession) &&
                xllmSessionForwardDriver(pForwarded, pSession) &&
                xllmSessionHasDriver(pForwarded) &&
                xllmRequestAddTextMessage(&tDirect, XLLM_ROLE_USER, "direct host request") &&
                xllmSessionDispatchRequest(pForwarded, &tDirect, NULL, &pResponse, &tError) ==
                    XLLM_RESULT_OK &&
                pResponse && strcmp(pResponse->sContent, "model reply") == 0 &&
                xllmSessionGetStats(pForwarded, &tStats) && tStats.uEntryCount == 0u,
            "forwarded host request dispatch reuses the driver without ledger mutation");
        xllmResponseDestroy(pResponse);
        pResponse = NULL;
        xllmRequestUnit(&tDirect);
        xllmSessionDestroy(pForwarded);
    }

    /* First turn: normal reply, usage recorded, no compaction due. */
    SESSION_CHECK(xllmSessionSend(pSession, "hello", NULL, &pResponse, &tError) == XLLM_RESULT_OK &&
                      pResponse && strcmp(pResponse->sContent, "model reply") == 0,
                  "Send returns the scripted reply");
    xllmResponseDestroy(pResponse);
    pResponse = NULL;
    SESSION_CHECK(xllmSessionGetStats(pSession, &tStats) && tStats.bFillExactValid &&
                      tStats.uFillExact == 5500u && tStats.uCurrentTurn == 1u &&
                      tStats.uEntryCount == 2u,
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

/* ------------------------------------------------------------------ */
/* v3: bounded tool round-trips (RunWithTools)                          */
/* ------------------------------------------------------------------ */

typedef struct
{
    unsigned iCalls;
    unsigned iToolRounds; /* rounds that answer with a tool call */
    bool bSawToolsInRequest;
} run_script;

typedef struct
{
    unsigned iExecutes;
} run_executor_state;

typedef struct
{
    unsigned iCalls;
    bool bSawOrderedResults;
} batch_run_script;

typedef struct
{
    unsigned iSingleExecutes;
    unsigned iBatchExecutes;
    unsigned iBatchUnits;
} batch_executor_state;

static char g_sRunExecutorContent[] = "status: success\nprobe output";

static xllm_response *test_make_tool_response(const char *sId, const char *sName, const char *sArgs)
{
    xllm_response *pResponse = (xllm_response *)calloc(1u, sizeof(*pResponse));
    if (!pResponse)
    {
        return NULL;
    }
    pResponse->sContent = xllm_session__strdup("");
    pResponse->eFinish = XLLM_FINISH_TOOL_CALLS;
    pResponse->pToolCalls = (xllm_tool_call *)calloc(1u, sizeof(*pResponse->pToolCalls));
    if (pResponse->pToolCalls)
    {
        pResponse->pToolCalls[0].sId = xllm_session__strdup(sId);
        pResponse->pToolCalls[0].sName = xllm_session__strdup(sName);
        pResponse->pToolCalls[0].sArgumentsJson = xllm_session__strdup(sArgs);
        pResponse->iToolCallCount = 1u;
    }
    if (!pResponse->sContent || !pResponse->pToolCalls || !pResponse->pToolCalls[0].sId ||
        !pResponse->pToolCalls[0].sName || !pResponse->pToolCalls[0].sArgumentsJson)
    {
        xllmResponseDestroy(pResponse);
        return NULL;
    }
    return pResponse;
}

static xllm_response *test_make_two_tool_response(void)
{
    xllm_response *pResponse = (xllm_response *)calloc(1u, sizeof(*pResponse));
    size_t i;
    static const char *const sIds[2] = {"batch-first", "batch-second"};
    if (!pResponse)
        return NULL;
    pResponse->sContent = xllm_session__strdup("");
    pResponse->eFinish = XLLM_FINISH_TOOL_CALLS;
    pResponse->pToolCalls = (xllm_tool_call *)calloc(2u, sizeof(*pResponse->pToolCalls));
    if (pResponse->pToolCalls)
    {
        for (i = 0u; i < 2u; ++i)
        {
            pResponse->pToolCalls[i].sId = xllm_session__strdup(sIds[i]);
            pResponse->pToolCalls[i].sName = xllm_session__strdup("probe");
            pResponse->pToolCalls[i].sArgumentsJson = xllm_session__strdup("{}");
        }
        pResponse->iToolCallCount = 2u;
    }
    if (!pResponse->sContent || !pResponse->pToolCalls || !pResponse->pToolCalls[0].sId ||
        !pResponse->pToolCalls[0].sName || !pResponse->pToolCalls[0].sArgumentsJson ||
        !pResponse->pToolCalls[1].sId || !pResponse->pToolCalls[1].sName ||
        !pResponse->pToolCalls[1].sArgumentsJson)
    {
        xllmResponseDestroy(pResponse);
        return NULL;
    }
    return pResponse;
}

static xllm_result test_run_script_call(void *pUserData, const xllm_request *pRequest,
                                        const xllm_stream_callbacks *pCallbacks,
                                        xllm_response **ppResponse, xllm_error *pError)
{
    run_script *pScript = (run_script *)pUserData;
    (void)pCallbacks;
    if (pError)
    {
        xllmErrorInit(pError);
    }
    ++pScript->iCalls;
    if (pRequest->iToolCount == 1u)
    {
        pScript->bSawToolsInRequest = true;
    }
    if (pScript->iCalls <= pScript->iToolRounds)
    {
        char sCallId[32];
        (void)snprintf(sCallId, sizeof(sCallId), "call-%u", pScript->iCalls);
        *ppResponse = test_make_tool_response(sCallId, "probe", "{}");
    }
    else
    {
        *ppResponse = test_make_response("all done", 10u, 5u);
    }
    return *ppResponse ? XLLM_RESULT_OK : XLLM_RESULT_ERROR;
}

static bool test_run_list(void *pUserData, xllm_request *pRequest)
{
    (void)pUserData;
    return xllmRequestAddTool(pRequest, "probe", "test probe tool",
                              "{\"type\":\"object\",\"properties\":{}}", false);
}

static bool test_run_execute(void *pUserData, const xllm_tool_call *pCall,
                             const xllm_executor_ctx *pCtx, xllm_executor_result *pResult)
{
    run_executor_state *pState = (run_executor_state *)pUserData;
    (void)pCall;
    (void)pCtx;
    memset(pResult, 0, sizeof(*pResult));
    ++pState->iExecutes;
    pResult->sContent = g_sRunExecutorContent;
    pResult->bSuccess = true;
    return true;
}

static xllm_result test_batch_run_script_call(void *pUserData, const xllm_request *pRequest,
                                              const xllm_stream_callbacks *pCallbacks,
                                              xllm_response **ppResponse, xllm_error *pError)
{
    batch_run_script *pScript = (batch_run_script *)pUserData;
    size_t i;
    unsigned iSeen = 0u;
    (void)pCallbacks;
    if (pError)
        xllmErrorInit(pError);
    ++pScript->iCalls;
    if (pScript->iCalls == 1u)
    {
        *ppResponse = test_make_two_tool_response();
    }
    else
    {
        for (i = 0u; i < pRequest->iMessageCount; ++i)
        {
            const xllm_message *pMessage = &pRequest->pMessages[i];
            if (pMessage->eRole != XLLM_ROLE_TOOL)
                continue;
            if (iSeen == 0u && pMessage->sToolCallId && pMessage->sContent &&
                strcmp(pMessage->sToolCallId, "batch-first") == 0 &&
                strcmp(pMessage->sContent, "first-result") == 0)
            {
                ++iSeen;
            }
            else if (iSeen == 1u && pMessage->sToolCallId && pMessage->sContent &&
                     strcmp(pMessage->sToolCallId, "batch-second") == 0 &&
                     strcmp(pMessage->sContent, "second-result") == 0)
            {
                ++iSeen;
            }
        }
        pScript->bSawOrderedResults = iSeen == 2u;
        *ppResponse = test_make_response("batch done", 12u, 4u);
    }
    return *ppResponse ? XLLM_RESULT_OK : XLLM_RESULT_ERROR;
}

static bool test_batch_single_execute(void *pUserData, const xllm_tool_call *pCall,
                                      const xllm_executor_ctx *pCtx, xllm_executor_result *pResult)
{
    batch_executor_state *pState = (batch_executor_state *)pUserData;
    (void)pCall;
    (void)pCtx;
    (void)pResult;
    ++pState->iSingleExecutes;
    return false;
}

static bool test_batch_execute(void *pUserData, const xllm_tool_call *pCalls, size_t iCallCount,
                               const xllm_executor_ctx *pCtx, xllm_executor_result *pResults)
{
    batch_executor_state *pState = (batch_executor_state *)pUserData;
    (void)pCalls;
    (void)pCtx;
    ++pState->iBatchExecutes;
    if (iCallCount != 2u)
        return false;
    memset(pResults, 0, iCallCount * sizeof(*pResults));
    pResults[0].sContent = "first-result";
    pResults[0].bSuccess = true;
    pResults[1].sContent = "second-result";
    pResults[1].bSuccess = true;
    return true;
}

static void test_batch_results_unit(void *pUserData, xllm_executor_result *pResults,
                                    size_t iResultCount)
{
    batch_executor_state *pState = (batch_executor_state *)pUserData;
    (void)pResults;
    (void)iResultCount;
    ++pState->iBatchUnits;
}

static bool test_run_stop_guard(xllm_session *pSession, uint32_t uRound,
                                const xllm_response *pResponse, size_t iPendingToolCalls,
                                void *pUserData)
{
    (void)pSession;
    (void)uRound;
    (void)pResponse;
    (void)iPendingToolCalls;
    (void)pUserData;
    return false; /* stop immediately, leaving the batch pending */
}

static void test_run_with_tools(void)
{
    xllm_session_config tConfig;
    xllm_session *pSession;
    xllm_run_policy tPolicy;
    xllm_run_summary tSummary;
    xllm_session_tail tTail;
    xllm_error tError;
    run_script tScript;
    run_executor_state tExecutorState;
    xllm_executor tExecutor;

    /* Happy path: one tool round, then a final answer. */
    memset(&tScript, 0, sizeof(tScript));
    tScript.iToolRounds = 1u;
    memset(&tExecutorState, 0, sizeof(tExecutorState));
    memset(&tExecutor, 0, sizeof(tExecutor));
    tExecutor.pListTools = test_run_list;
    tExecutor.pExecute = test_run_execute;
    tExecutor.pUserData = &tExecutorState;
    xllmSessionConfigInit(&tConfig);
    pSession = xllmSessionCreateForTest(&tConfig, test_run_script_call, &tScript, &tError);
    SESSION_CHECK(pSession != NULL, "run test session creates");
    memset(&tSummary, 0, sizeof(tSummary));
    SESSION_CHECK(pSession && xllmSessionRunWithTools(pSession, "run the probe", &tExecutor, NULL,
                                                      NULL, &tSummary, &tError) == XLLM_RESULT_OK,
                  "bounded tool run completes");
    SESSION_CHECK(tScript.iCalls == 2u && tExecutorState.iExecutes == 1u &&
                      tSummary.uRounds == 2u && tSummary.uToolCalls == 1u,
                  "run summary counts rounds and executor calls");
    SESSION_CHECK(tSummary.sFinalText && strcmp(tSummary.sFinalText, "all done") == 0 &&
                      !tSummary.bStoppedByPolicy,
                  "final assistant text captured");
    SESSION_CHECK(tScript.bSawToolsInRequest, "executor tools reached the model request");
    SESSION_CHECK(pSession && xllmSessionPendingToolCallCount(pSession) == 0u,
                  "no unresolved calls after a completed run");
    SESSION_CHECK(pSession && xllmSessionGetTail(pSession, &tTail) && tTail.bHasMessage &&
                      tTail.eRole == XLLM_ROLE_ASSISTANT,
                  "run tail is the final assistant message");
    xllmRunSummaryUnit(&tSummary);
    xllmSessionDestroy(pSession);

    /* Optional batch executors execute once and commit in call order. */
    {
        batch_run_script tBatchScript;
        batch_executor_state tBatchState;
        memset(&tBatchScript, 0, sizeof(tBatchScript));
        memset(&tBatchState, 0, sizeof(tBatchState));
        xllmExecutorInit(&tExecutor);
        tExecutor.pListTools = test_run_list;
        tExecutor.pExecute = test_batch_single_execute;
        tExecutor.pUserData = &tBatchState;
        tExecutor.pExecuteBatch = test_batch_execute;
        tExecutor.pBatchResultsUnit = test_batch_results_unit;
        xllmSessionConfigInit(&tConfig);
        pSession =
            xllmSessionCreateForTest(&tConfig, test_batch_run_script_call, &tBatchScript, &tError);
        memset(&tSummary, 0, sizeof(tSummary));
        SESSION_CHECK(pSession &&
                          xllmSessionRunWithTools(pSession, "run the batch", &tExecutor, NULL, NULL,
                                                  &tSummary, &tError) == XLLM_RESULT_OK,
                      "optional batch executor completes one two-call response");
        SESSION_CHECK(tBatchState.iBatchExecutes == 1u && tBatchState.iSingleExecutes == 0u &&
                          tBatchState.iBatchUnits == 1u && tSummary.uToolCalls == 2u,
                      "batch ABI executes and releases exactly once");
        SESSION_CHECK(tBatchScript.bSawOrderedResults,
                      "batch results enter the session in model call order");
        xllmRunSummaryUnit(&tSummary);
        xllmSessionDestroy(pSession);
    }

    /* Continue legacy coverage with a zero-initialized serial executor. */
    memset(&tExecutor, 0, sizeof(tExecutor));
    tExecutor.pListTools = test_run_list;
    tExecutor.pExecute = test_run_execute;
    tExecutor.pUserData = &tExecutorState;

    /* Guard stop leaves the batch pending; resume drains it without a new prompt. */
    memset(&tScript, 0, sizeof(tScript));
    tScript.iToolRounds = 1u;
    memset(&tExecutorState, 0, sizeof(tExecutorState));
    xllmSessionConfigInit(&tConfig);
    pSession = xllmSessionCreateForTest(&tConfig, test_run_script_call, &tScript, &tError);
    xllmRunPolicyInit(&tPolicy);
    tPolicy.pOnRound = test_run_stop_guard;
    memset(&tSummary, 0, sizeof(tSummary));
    SESSION_CHECK(pSession &&
                      xllmSessionRunWithTools(pSession, "probe then get stopped", &tExecutor, NULL,
                                              &tPolicy, &tSummary, &tError) == XLLM_RESULT_OK &&
                      tSummary.bStoppedByPolicy && tSummary.uToolCalls == 0u,
                  "guard seam stops the run before the tool batch executes");
    SESSION_CHECK(pSession && xllmSessionPendingToolCallCount(pSession) == 1u,
                  "stopped run leaves its tool call pending");
    xllmRunSummaryUnit(&tSummary);
    memset(&tSummary, 0, sizeof(tSummary));
    SESSION_CHECK(pSession &&
                      xllmSessionRunWithTools(pSession, NULL, &tExecutor, NULL, NULL, &tSummary,
                                              &tError) == XLLM_RESULT_OK &&
                      tSummary.uToolCalls == 1u && tSummary.uRounds == 1u && tSummary.sFinalText &&
                      strcmp(tSummary.sFinalText, "all done") == 0,
                  "resume drains the pending call and finishes without a new prompt");
    SESSION_CHECK(pSession && xllmSessionPendingToolCallCount(pSession) == 0u,
                  "resume resolved every pending call");
    xllmRunSummaryUnit(&tSummary);
    xllmSessionDestroy(pSession);

    /* Round budget trips with LIMIT. */
    memset(&tScript, 0, sizeof(tScript));
    tScript.iToolRounds = 100u; /* every round wants a tool */
    memset(&tExecutorState, 0, sizeof(tExecutorState));
    xllmSessionConfigInit(&tConfig);
    pSession = xllmSessionCreateForTest(&tConfig, test_run_script_call, &tScript, &tError);
    xllmRunPolicyInit(&tPolicy);
    tPolicy.uMaxRounds = 2u;
    memset(&tSummary, 0, sizeof(tSummary));
    SESSION_CHECK(pSession &&
                      xllmSessionRunWithTools(pSession, "never settle", &tExecutor, NULL, &tPolicy,
                                              &tSummary, &tError) == XLLM_RESULT_ERROR &&
                      tError.eCode == XLLM_ERROR_LIMIT && tSummary.uRounds == 2u,
                  "round budget trips with a limit error");
    xllmRunSummaryUnit(&tSummary);
    xllmSessionDestroy(pSession);

    /* Round sentinel: UINT32_MAX disables the round bound (guard-driven hosts). */
    memset(&tScript, 0, sizeof(tScript));
    tScript.iToolRounds = 3u;
    memset(&tExecutorState, 0, sizeof(tExecutorState));
    xllmSessionConfigInit(&tConfig);
    pSession = xllmSessionCreateForTest(&tConfig, test_run_script_call, &tScript, &tError);
    xllmRunPolicyInit(&tPolicy);
    tPolicy.uMaxRounds = UINT32_MAX;
    memset(&tSummary, 0, sizeof(tSummary));
    SESSION_CHECK(pSession &&
                      xllmSessionRunWithTools(pSession, "keep going", &tExecutor, NULL, &tPolicy,
                                              &tSummary, &tError) == XLLM_RESULT_OK &&
                      tSummary.uRounds == 4u && tSummary.uToolCalls == 3u,
                  "UINT32_MAX round bound lets a long tool run finish");
    xllmRunSummaryUnit(&tSummary);
    xllmSessionDestroy(pSession);

    /* Contract enforcement: incomplete executor is rejected. */
    xllmSessionConfigInit(&tConfig);
    pSession = xllmSessionCreateForTest(&tConfig, test_run_script_call, &tScript, &tError);
    memset(&tExecutor, 0, sizeof(tExecutor));
    SESSION_CHECK(pSession &&
                      xllmSessionRunWithTools(pSession, "x", &tExecutor, NULL, NULL, NULL,
                                              &tError) == XLLM_RESULT_ERROR &&
                      tError.eCode == XLLM_ERROR_INVALID_ARGUMENT,
                  "incomplete executor is rejected up front");
    xllmSessionDestroy(pSession);
}

/* ------------------------------------------------------------------ */
/* Engineering layer: rebinding, identity, policy cancellation.         */
/* ------------------------------------------------------------------ */

static size_t test_count_system_text(const xllm_request *pRequest, const char *sNeedle)
{
    size_t i;
    size_t iHits = 0u;
    for (i = 0u; i < pRequest->iMessageCount; ++i)
    {
        const xllm_message *pMessage = &pRequest->pMessages[i];
        if (pMessage->eRole == XLLM_ROLE_SYSTEM && pMessage->sContent &&
            strstr(pMessage->sContent, sNeedle))
        {
            ++iHits;
        }
    }
    return iHits;
}

static void test_bind_identity_and_cancel(void)
{
    xllm_session_config tConfig;
    xllm_session *pSession;
    xllm_request tRequest;
    xllm_error tError;
    xllm_run_policy tPolicy;
    xllm_run_summary tSummary;
    run_script tScript;
    run_executor_state tExecutorState;
    xllm_executor tExecutor;
    xcancel *pCancel = NULL;

    /* Identity: set, idempotent re-set, upgrade renders newest only. */
    xllmSessionConfigInit(&tConfig);
    pSession = xllmSessionCreate(&tConfig, &tError);
    SESSION_CHECK(pSession && xllmSessionSetSystemPrompt(pSession, "identity v1", &tError),
                  "system prompt pins on a fresh session");
    xllmRequestInit(&tRequest);
    SESSION_CHECK(pSession && xllmSessionBuildRequest(pSession, &tRequest, &tError) &&
                      test_count_system_text(&tRequest, "identity v1") == 1u,
                  "pinned identity renders exactly once");
    xllmRequestUnit(&tRequest);
    {
        xllm_session_stats tBefore, tAfter;
        SESSION_CHECK(pSession && xllmSessionGetStats(pSession, &tBefore) &&
                          xllmSessionSetSystemPrompt(pSession, "identity v1", &tError) &&
                          xllmSessionGetStats(pSession, &tAfter) &&
                          tBefore.uEntryCount == tAfter.uEntryCount,
                      "re-setting the same identity is a no-op");
    }
    SESSION_CHECK(pSession && xllmSessionSetSystemPrompt(pSession, "identity v2", &tError),
                  "identity upgrade appends");
    xllmRequestInit(&tRequest);
    SESSION_CHECK(pSession && xllmSessionBuildRequest(pSession, &tRequest, &tError) &&
                      test_count_system_text(&tRequest, "identity v2") == 1u &&
                      test_count_system_text(&tRequest, "identity v1") == 0u,
                  "render shows only the newest pinned identity");
    xllmRequestUnit(&tRequest);
    SESSION_CHECK(!xllmSessionSetSystemPrompt(pSession, "", &tError),
                  "empty system text is rejected");
    xllmSessionDestroy(pSession);

    /* Unbound session refuses the run; SetTestCall revives it. */
    memset(&tScript, 0, sizeof(tScript));
    tScript.iToolRounds = 0u; /* first call answers with final text */
    memset(&tExecutorState, 0, sizeof(tExecutorState));
    memset(&tExecutor, 0, sizeof(tExecutor));
    tExecutor.pListTools = test_run_list;
    tExecutor.pExecute = test_run_execute;
    tExecutor.pUserData = &tExecutorState;
    xllmSessionConfigInit(&tConfig);
    pSession = xllmSessionCreate(&tConfig, &tError);
    SESSION_CHECK(pSession &&
                      xllmSessionRunWithTools(pSession, "x", &tExecutor, NULL, NULL, NULL,
                                              &tError) == XLLM_RESULT_ERROR &&
                      tError.eCode == XLLM_ERROR_INVALID_ARGUMENT,
                  "unbound session refuses RunWithTools");
    SESSION_CHECK(pSession && xllmSessionSetTestCall(pSession, test_run_script_call, &tScript) &&
                      xllmSessionRunWithTools(pSession, "revive", &tExecutor, NULL, NULL, &tSummary,
                                              &tError) == XLLM_RESULT_OK &&
                      tSummary.sFinalText && strcmp(tSummary.sFinalText, "all done") == 0,
                  "SetTestCall revives a plain session for the run loop");
    xllmRunSummaryUnit(&tSummary);
    xllmSessionDestroy(pSession);

    /* BindClient validation. */
    SESSION_CHECK(!xllmSessionBindClient(NULL, NULL) &&
                      (pSession = xllmSessionCreate(&tConfig, &tError)) != NULL &&
                      !xllmSessionBindClient(pSession, NULL),
                  "BindClient rejects null slots");
    xllmSessionDestroy(pSession);

    /* Policy cancellation stops the run before any model round. */
    memset(&tScript, 0, sizeof(tScript));
    memset(&tExecutorState, 0, sizeof(tExecutorState));
    xllmSessionConfigInit(&tConfig);
    pSession = xllmSessionCreateForTest(&tConfig, test_run_script_call, &tScript, &tError);
    pCancel = xrtCancelCreate();
    xllmRunPolicyInit(&tPolicy);
    tPolicy.pCancel = pCancel;
    SESSION_CHECK(pCancel && xrtCancelRequest(pCancel) && pSession &&
                      xllmSessionRunWithTools(pSession, "never started", &tExecutor, NULL, &tPolicy,
                                              NULL, &tError) == XLLM_RESULT_CANCELLED &&
                      tScript.iCalls == 0u,
                  "a pre-cancelled policy token stops the run before any model call");
    xrtCancelDestroy(pCancel);
    xllmSessionDestroy(pSession);
}

/* ------------------------------------------------------------------ */
/* JSONL persistence round-trip: adversarial content through the       */
/* hand-rolled journal writer and the xrt JSONL reader must agree.     */
/* The writer lives here (session) and the parser in xrt, so this      */
/* property test is the guardrail pinning both encoders together.      */
/* ------------------------------------------------------------------ */

static uint32_t s_uRoundTripSeed = 0x9E3779B9u;
static uint32_t round_trip_rand(void)
{
    s_uRoundTripSeed ^= s_uRoundTripSeed << 13;
    s_uRoundTripSeed ^= s_uRoundTripSeed >> 17;
    s_uRoundTripSeed ^= s_uRoundTripSeed << 5;
    return s_uRoundTripSeed;
}

/* Random adversarial text: valid UTF-8 only (the message API rejects
 * anything else); control bytes skip NUL because the C-string contract
 * has no embedded-NUL channel. */
static const char *round_trip_adversarial(void)
{
    static char sText[384];
    static const char *sPieces[] = {
        "\"",
        "\\",
        "/",
        "\b",
        "\f",
        "\n",
        "\r",
        "\t",
        "\x01\x02\x1f",
        "\x7f",
        "中文与标点，。！",
        "\xF0\x9F\x98\x80",
        "mixed \" \\ \n end",
    };
    unsigned iPieces = 4u + (round_trip_rand() % 16u);
    unsigned i;
    size_t iLen = 0u;
    for (i = 0u; i < iPieces; ++i)
    {
        const char *sPiece = sPieces[round_trip_rand() % (sizeof(sPieces) / sizeof(sPieces[0]))];
        size_t iPiece = strlen(sPiece);
        if (iLen + iPiece + 1u >= sizeof(sText))
        {
            break;
        }
        memcpy(sText + iLen, sPiece, iPiece);
        iLen += iPiece;
    }
    sText[iLen] = '\0';
    return sText;
}

static bool round_trip_requests_equal(const xllm_request *pA, const xllm_request *pB)
{
    size_t i, j;
    if (pA->iMessageCount != pB->iMessageCount)
    {
        return false;
    }
    for (i = 0u; i < pA->iMessageCount; ++i)
    {
        const xllm_message *pMA = &pA->pMessages[i];
        const xllm_message *pMB = &pB->pMessages[i];
        if (pMA->eRole != pMB->eRole)
        {
            return false;
        }
        if ((pMA->sContent == NULL) != (pMB->sContent == NULL))
        {
            return false;
        }
        if (pMA->sContent && strcmp(pMA->sContent, pMB->sContent) != 0)
        {
            return false;
        }
        if ((pMA->sReasoningContent == NULL) != (pMB->sReasoningContent == NULL))
        {
            return false;
        }
        if (pMA->sReasoningContent && strcmp(pMA->sReasoningContent, pMB->sReasoningContent) != 0)
        {
            return false;
        }
        if (pMA->iToolCallCount != pMB->iToolCallCount)
        {
            return false;
        }
        for (j = 0u; j < pMA->iToolCallCount; ++j)
        {
            const xllm_tool_call *pCA = &pMA->pToolCalls[j];
            const xllm_tool_call *pCB = &pMB->pToolCalls[j];
            if (strcmp(pCA->sId, pCB->sId) != 0 || strcmp(pCA->sName, pCB->sName) != 0 ||
                strcmp(pCA->sArgumentsJson, pCB->sArgumentsJson) != 0)
            {
                return false;
            }
        }
    }
    return true;
}

static void test_journal_roundtrip_property(void)
{
    static const char sJournal[] = "out/mdo-integration/fixtures/rt_journal.jsonl";
    static const char sSnapshot[] = "out/mdo-integration/fixtures/rt_snapshot.json";
    static const char *sCorpus[] = {
        "plain text",
        "",
        "quote\"inside",
        "back\\slash",
        "b\bf\fn\nr\rt\tt",
        "del\x7f byte",
        "中文内容与标点，。！",
        "emoji \xF0\x9F\x98\x80 four-byte scalar",
        "mixed \" \\ \n \x01\x02\x1f 中文 \xF0\x9F\x98\x80 end",
    };
    xllm_session_config tConfig;
    xllm_session *pOriginal = NULL;
    xllm_session *pFromJournal = NULL;
    xllm_session *pFromSnapshot = NULL;
    xllm_request tWant, tGot;
    xllm_error tError;
    xllm_tool_call tCall;
    xllm_response tResponse;
    uint64_t uTurn;
    size_t i;
    size_t iAdded = 0u;

    (void)xrtFileDelete(sJournal);
    (void)xrtFileDelete(sSnapshot);

    xllmSessionConfigInit(&tConfig);
    pOriginal = xllmSessionCreate(&tConfig, &tError);
    SESSION_CHECK(pOriginal != NULL, "round-trip fixture session creates");
    SESSION_CHECK(
        pOriginal && !xllmSessionAddText(pOriginal, 1u, XLLM_ROLE_USER, "\xFF\xFE", 0u),
        "invalid UTF-8 is rejected at the message boundary (writer passthrough stays safe)");
    if (!pOriginal)
    {
        return;
    }
    SESSION_CHECK(xllmSessionEnableJournal(pOriginal, sJournal, &tError),
                  "journal attaches before any adversarial entry");

    uTurn = xllmSessionBeginTurn(pOriginal);
    for (i = 0u; i < sizeof(sCorpus) / sizeof(sCorpus[0]); ++i)
    {
        if (xllmSessionAddText(pOriginal, uTurn, XLLM_ROLE_USER, sCorpus[i], 0u))
        {
            ++iAdded;
        }
    }
    for (i = 0u; i < 32u; ++i)
    {
        if (xllmSessionAddText(pOriginal, uTurn, XLLM_ROLE_USER, round_trip_adversarial(), 0u))
        {
            ++iAdded;
        }
    }
    /* Adversarial tool call arguments and a tool result body. */
    memset(&tCall, 0, sizeof(tCall));
    tCall.sId = (char *)"rt-\"1\"";
    tCall.sName = (char *)"probe\\tool";
    tCall.sArgumentsJson = (char *)"{\"path\":\"a\\\"b\\\\c\\n\x01\"}";
    memset(&tResponse, 0, sizeof(tResponse));
    tResponse.sContent = (char *)"assistant \"quoted\" \\body\\ \n";
    tResponse.pToolCalls = &tCall;
    tResponse.iToolCallCount = 1u;
    SESSION_CHECK(
        xllmSessionAddAssistantResponse(pOriginal, uTurn, &tResponse) &&
            xllmSessionAddToolResult(pOriginal, uTurn, "rt-\"1\"",
                                     "status: success\ntool: \"quoted\" name\nbody \\ \n \x1f end"),
        "adversarial tool call and result recorded");
    iAdded += 2u;
    SESSION_CHECK(iAdded >= sizeof(sCorpus) / sizeof(sCorpus[0]) + 32u,
                  "every adversarial entry was accepted");

    xllmRequestInit(&tWant);
    SESSION_CHECK(xllmSessionBuildRequest(pOriginal, &tWant, &tError),
                  "original renders for comparison");

    /* Scenario A: journal-only recovery — the pure JSONL replay path
     * (the snapshot path names a file that does not exist, so the session
     * is created fresh and every entry comes back from the journal). */
    pFromJournal =
        xllmSessionRecover("out/mdo-integration/fixtures/rt_absent.json", sJournal, NULL, &tError);
    SESSION_CHECK(pFromJournal != NULL, "journal-only recovery replays adversarial records");
    xllmRequestInit(&tGot);
    if (pFromJournal && xllmSessionBuildRequest(pFromJournal, &tGot, &tError))
    {
        SESSION_CHECK(round_trip_requests_equal(&tWant, &tGot),
                      "journal-only round trip is byte-equal for adversarial content");
        xllmRequestUnit(&tGot);
    }

    /* Scenario B: snapshot + journal recovery. */
    SESSION_CHECK(xllmSessionSave(pOriginal, sSnapshot, &tError),
                  "snapshot saves the adversarial state");
    pFromSnapshot = xllmSessionRecover(sSnapshot, sJournal, NULL, &tError);
    SESSION_CHECK(pFromSnapshot != NULL, "snapshot+journal recovery succeeds");
    xllmRequestInit(&tGot);
    if (pFromSnapshot && xllmSessionBuildRequest(pFromSnapshot, &tGot, &tError))
    {
        SESSION_CHECK(round_trip_requests_equal(&tWant, &tGot),
                      "snapshot round trip is byte-equal for adversarial content");
        xllmRequestUnit(&tGot);
    }

    xllmRequestUnit(&tWant);
    xllmSessionDestroy(pFromJournal);
    xllmSessionDestroy(pFromSnapshot);
    xllmSessionDestroy(pOriginal);
    (void)xrtFileDelete(sJournal);
    (void)xrtFileDelete(sSnapshot);
}

/* ------------------------------------------------------------------ */
/* Asset ledger: note/dedup, compaction prompt + summary render,        */
/* persistence (snapshot + journal replay), fork.                       */
/* ------------------------------------------------------------------ */

static void test_file_ledger(void)
{
    static const char sJournal[] = "out/mdo-integration/fixtures/ledger_journal.jsonl";
    static const char sSnapshot[] = "out/mdo-integration/fixtures/ledger_snapshot.json";
    xllm_session_config tConfig;
    xllm_session *pSession = NULL;
    xllm_session *pLoaded = NULL;
    xllm_session *pForked = NULL;
    xllm_session *pReplayed = NULL;
    xllm_compaction *pCompaction = NULL;
    xllm_file_ledger tLedger;
    xllm_request tRequest;
    xllm_error tError;
    test_script tScript;
    uint64_t uTurn;

    (void)xrtFileDelete(sJournal);
    (void)xrtFileDelete(sSnapshot);
    memset(&tScript, 0, sizeof(tScript));
    xllmSessionConfigInit(&tConfig);
    tConfig.uKeepRecentTokens = 8u; /* the single fixture turn must be compactable */
    pSession = xllmSessionCreateForTest(&tConfig, test_script_call, &tScript, &tError);
    SESSION_CHECK(pSession != NULL, "ledger fixture session creates");
    SESSION_CHECK(pSession && xllmSessionEnableJournal(pSession, sJournal, &tError),
                  "ledger journal attaches");

    SESSION_CHECK(pSession && xllmSessionNoteFileRead(pSession, "src/main.c") &&
                      xllmSessionNoteFileRead(pSession, "src/main.c") &&
                      xllmSessionNoteFileRead(pSession, "docs/README.md") &&
                      xllmSessionNoteFileModified(pSession, "src/main.c"),
                  "ledger notes succeed");
    SESSION_CHECK(pSession && xllmSessionGetFileLedger(pSession, &tLedger) &&
                      tLedger.iReadFileCount == 2u && tLedger.iModifiedFileCount == 1u,
                  "ledger dedups by exact path");
    SESSION_CHECK(pSession && !xllmSessionNoteFileRead(pSession, ""),
                  "empty ledger path is rejected");

    /* Two complete turns: the large old one falls outside the keep-recent
     * window and becomes the compactable candidate; the small one stays. */
    {
        char *sLarge = make_large_output(2000u, 3u);
        uTurn = pSession ? xllmSessionBeginTurn(pSession) : 0u;
        SESSION_CHECK(
            pSession && uTurn && sLarge &&
                xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, sLarge, 0u) &&
                xllmSessionAddText(pSession, uTurn, XLLM_ROLE_ASSISTANT, "done inspecting", 0u),
            "ledger fixture turn recorded");
        free(sLarge);
        uTurn = pSession ? xllmSessionBeginTurn(pSession) : 0u;
        SESSION_CHECK(
            pSession && uTurn &&
                xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, "recent follow-up", 0u) &&
                xllmSessionAddText(pSession, uTurn, XLLM_ROLE_ASSISTANT, "ok", 0u),
            "ledger fixture recent turn recorded");
    }

    /* The compaction prompt carries the harness ledger. */
    pCompaction = pSession ? xllmSessionPrepareCompaction(pSession, true, &tError) : NULL;
    SESSION_CHECK(pCompaction != NULL &&
                      strstr(xllmCompactionPrompt(pCompaction), "<read-files>") != NULL &&
                      strstr(xllmCompactionPrompt(pCompaction), "src/main.c") != NULL &&
                      strstr(xllmCompactionPrompt(pCompaction), "<modified-files>") != NULL,
                  "compaction prompt carries the asset ledger");

    /* Commit; the summary bridge renders the ledger back to the model. */
    SESSION_CHECK(pSession && pCompaction &&
                      xllmSessionCommitCompaction(pSession, pCompaction,
                                                  test_pi_summary("ledger survives compaction"),
                                                  &tError),
                  "compaction commits with the ledger present");
    xllmCompactionDestroy(pCompaction);
    pCompaction = NULL;
    xllmRequestInit(&tRequest);
    if (pSession && xllmSessionBuildRequest(pSession, &tRequest, &tError))
    {
        size_t i;
        bool bSawLedger = false;
        for (i = 0u; i < tRequest.iMessageCount; ++i)
        {
            const xllm_message *pMessage = &tRequest.pMessages[i];
            if (pMessage->eRole == XLLM_ROLE_USER && pMessage->sContent &&
                strstr(pMessage->sContent, "<read-files>") &&
                strstr(pMessage->sContent, "src/main.c") &&
                strstr(pMessage->sContent, "<modified-files>"))
            {
                bSawLedger = true;
            }
        }
        SESSION_CHECK(bSawLedger, "summary bridge renders the asset ledger");
        xllmRequestUnit(&tRequest);
    }

    /* Snapshot round trip keeps the ledger. */
    SESSION_CHECK(pSession && xllmSessionSave(pSession, sSnapshot, &tError),
                  "ledger snapshot saves");
    pLoaded = xllmSessionLoad(sSnapshot, &tError);
    SESSION_CHECK(pLoaded && xllmSessionGetFileLedger(pLoaded, &tLedger) &&
                      tLedger.iReadFileCount == 2u && tLedger.iModifiedFileCount == 1u &&
                      strcmp(tLedger.psModifiedFiles[0], "src/main.c") == 0,
                  "snapshot round trip preserves the ledger");

    /* Fork copies the ledger verbatim. */
    pForked = pSession ? xllmSessionFork(pSession, &tError) : NULL;
    SESSION_CHECK(pForked && xllmSessionGetFileLedger(pForked, &tLedger) &&
                      tLedger.iReadFileCount == 2u && tLedger.iModifiedFileCount == 1u,
                  "fork carries the parent ledger");

    /* Journal-only replay restores the ledger (ledger records replay). */
    pReplayed = xllmSessionRecover("out/mdo-integration/fixtures/ledger_absent.json", sJournal,
                                   NULL, &tError);
    SESSION_CHECK(pReplayed && xllmSessionGetFileLedger(pReplayed, &tLedger) &&
                      tLedger.iReadFileCount == 2u && tLedger.iModifiedFileCount == 1u,
                  "journal-only replay restores the ledger");

    xllmSessionDestroy(pReplayed);
    xllmSessionDestroy(pForked);
    xllmSessionDestroy(pLoaded);
    xllmSessionDestroy(pSession);
    (void)xrtFileDelete(sJournal);
    (void)xrtFileDelete(sSnapshot);
}

/* ------------------------------------------------------------------ */
/* 改造 A/B acceptance: borrowed views must produce identical output   */
/* and eliminate the per-turn full-history allocation churn.           */
/* ------------------------------------------------------------------ */

static void test_borrowed_view_render(void)
{
    xllm_session_config tConfig;
    xllm_session *pSession = NULL;
    xllm_session *pFork = NULL;
    xllm_request tOwned;
    xllm_request tView;
    xllm_error tError;
    char *sBig = NULL;
    uint64_t uTurn;
    size_t i;
    uint64_t uOwnedMs;
    uint64_t uViewMs;
    const size_t iTurns = 400u;

    sBig = make_large_output(2000u, 5u);
    xllmSessionConfigInit(&tConfig);
    tConfig.uContextWindowTokens = 1000000ull; /* no compaction interference */
    pSession = xllmSessionCreate(&tConfig, &tError);
    SESSION_CHECK(pSession != NULL && sBig != NULL, "borrow fixture session creates");
    if (!pSession || !sBig)
    {
        free(sBig);
        return;
    }
    for (i = 0u; i < iTurns; ++i)
    {
        uTurn = xllmSessionBeginTurn(pSession);
        if (!xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, sBig, 0u) ||
            !xllmSessionAddText(pSession, uTurn, XLLM_ROLE_ASSISTANT, "ack", 0u))
        {
            SESSION_CHECK(false, "borrow fixture fill");
            break;
        }
    }

    /* 1. Differential: owned vs view produce identical message sequences. */
    xllmRequestInit(&tOwned);
    xllmRequestInit(&tView);
    SESSION_CHECK(xllmSessionBuildRequest(pSession, &tOwned, &tError) &&
                      xllmSessionBuildRequestView(pSession, &tView, &tError) &&
                      round_trip_requests_equal(&tOwned, &tView) &&
                      tOwned.iMessageCount == 2u * iTurns,
                  "borrowed view renders a byte-identical message sequence");

    /* 2. The view really borrows: its message strings point into the ledger. */
    SESSION_CHECK(tView.pbMessageBorrowed != NULL && tView.iMessageCount > 1u &&
                      tView.pbMessageBorrowed[0],
                  "view entries are flagged borrowed");
    SESSION_CHECK(tOwned.pbMessageBorrowed == NULL, "owned path carries no borrow bitmap");

    /* 3. Unit on the view must not corrupt the ledger: rebuild and compare. */
    xllmRequestUnit(&tView);
    xllmRequestUnit(&tOwned);
    xllmRequestInit(&tView);
    SESSION_CHECK(xllmSessionBuildRequestView(pSession, &tView, &tError) &&
                      tView.iMessageCount == 2u * iTurns,
                  "ledger survives view unit (no double free)");

    /* 4. Performance: the view path must be dramatically cheaper on a
     * large ledger (allocation churn dominates; assert >= 5x on wall time,
     * tolerant of fast machines via the >= 3x floor). */
    uOwnedMs = 0u;
    for (i = 0u; i < 60u; ++i)
    {
        uint64_t uStart = xllmClockUs();
        xllmRequestInit(&tOwned);
        if (!xllmSessionBuildRequest(pSession, &tOwned, &tError))
            break;
        xllmRequestUnit(&tOwned);
        uOwnedMs += (xllmClockUs() - uStart) / 1000u;
    }
    uViewMs = 0u;
    for (i = 0u; i < 60u; ++i)
    {
        uint64_t uStart = xllmClockUs();
        xllmRequestInit(&tView);
        if (!xllmSessionBuildRequestView(pSession, &tView, &tError))
            break;
        xllmRequestUnit(&tView);
        uViewMs += (xllmClockUs() - uStart) / 1000u;
    }
    SESSION_CHECK(uOwnedMs == 0u || (uViewMs < uOwnedMs && uViewMs * 3u <= uOwnedMs),
                  "borrowed view renders a large ledger at least 3x faster when measurable");
    {
        char sPerf[128];
        (void)snprintf(sPerf, sizeof(sPerf),
                       "borrowed view render: owned %llu ms vs view %llu ms over 60 passes",
                       (unsigned long long)uOwnedMs, (unsigned long long)uViewMs);
        printf("       %s\n", sPerf);
        SESSION_CHECK(true, sPerf);
    }

    /* 5. Fork inherits the same content; view on the fork borrows fork
     * storage independently. */
    pFork = xllmSessionFork(pSession, &tError);
    xllmRequestInit(&tOwned);
    xllmRequestInit(&tView);
    SESSION_CHECK(pFork && xllmSessionBuildRequest(pFork, &tOwned, &tError) &&
                      xllmSessionBuildRequestView(pFork, &tView, &tError) &&
                      round_trip_requests_equal(&tOwned, &tView),
                  "fork view render matches fork owned render");
    xllmRequestUnit(&tOwned);
    xllmRequestUnit(&tView);

    free(sBig);
    xllmSessionDestroy(pFork);
    xllmSessionDestroy(pSession);
}

/* Wire-prefix cache (尾账 #3): the client's incremental serialization must
 * produce byte-identical bodies across growth (cache hit) and after stamp
 * invalidation (miss). Runs against a never-connected client through the
 * internal serialize path (the test TU includes the xllm unity). */
static void test_wire_prefix_cache(void)
{
    xllm_session_config tSessionConfig;
    xllm_session *pSession = NULL;
    xllm_client_config tClientConfig;
    xllm_client *pClient = NULL;
    xllm_error tError;
    xllm_request tReq;
    xllm_request tRef;
    char *sRef = NULL;
    char *sCached = NULL;
    uint64_t uTurn;

    xllmClientConfigInit(&tClientConfig);
    tClientConfig.sBaseUrl = "http://127.0.0.1:1/v1/chat/completions";
    tClientConfig.sModel = "mock-model";
    tClientConfig.sApiKey = "test-key";
    pClient = xllmClientCreate(&tClientConfig, &tError);
    SESSION_CHECK(pClient != NULL, "prefix-cache client creates");
    xllmSessionConfigInit(&tSessionConfig);
    tSessionConfig.uContextWindowTokens = 1000000ull;
    pSession = xllmSessionCreateBound(&tSessionConfig, pClient, &tError);
    SESSION_CHECK(pSession != NULL, "prefix-cache session creates");
    if (!pSession || !pClient)
    {
        goto cleanup;
    }

    /* Round 1: miss -> full build through the cached op; must equal the
     * classic full serialization byte for byte. */
    uTurn = xllmSessionBeginTurn(pSession);
    (void)xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, "first question", 0u);
    (void)xllmSessionAddText(pSession, uTurn, XLLM_ROLE_ASSISTANT, "first answer", 0u);
    xllmRequestInit(&tReq);
    SESSION_CHECK(xllmSessionBuildRequestView(pSession, &tReq, &tError) &&
                      tReq.pStablePrefixOwner == (void *)pSession && tReq.iStableMessages == 2u,
                  "view render stamps the request");
    sRef = xllmClientBuildRequestJson(pClient, &tReq, &tError);
    sCached = xllm__client_serialize_body(pClient, &tReq, &tError);
    SESSION_CHECK(sRef && sCached && strcmp(sRef, sCached) == 0,
                  "round-1 miss body is byte-identical to the full build");
    SESSION_CHECK(pClient->iPrefixCacheMessages == 2u && pClient->iPrefixCacheLen > 0u,
                  "round-1 populates the prefix cache");
    xllm__free(sRef);
    sRef = NULL;
    xllm__free(sCached);
    sCached = NULL;
    xllmRequestUnit(&tReq);

    /* Round 2: growth -> hit path (prefix 2 + delta 2); still identical. */
    uTurn = xllmSessionBeginTurn(pSession);
    (void)xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, "second question", 0u);
    (void)xllmSessionAddText(pSession, uTurn, XLLM_ROLE_ASSISTANT, "second answer", 0u);
    xllmRequestInit(&tReq);
    SESSION_CHECK(xllmSessionBuildRequestView(pSession, &tReq, &tError) &&
                      tReq.iStableMessages == 4u,
                  "round-2 stable count grows");
    /* Reference: pristine copy without the stamp drives the classic path. */
    xllmRequestInit(&tRef);
    SESSION_CHECK(xllmSessionBuildRequest(pSession, &tRef, &tError), "round-2 reference render");
    sRef = xllmClientBuildRequestJson(pClient, &tRef, &tError);
    sCached = xllm__client_serialize_body(pClient, &tReq, &tError);
    SESSION_CHECK(sRef && sCached && strcmp(sRef, sCached) == 0 &&
                      strstr(sCached, "second answer") != NULL,
                  "round-2 hit body is byte-identical to the full build");
    SESSION_CHECK(pClient->iPrefixCacheMessages == 4u, "cache count tracks the stable messages");
    xllm__free(sRef);
    sRef = NULL;
    xllm__free(sCached);
    sCached = NULL;
    xllmRequestUnit(&tReq);
    xllmRequestUnit(&tRef);

    /* Identity upgrade: SetSystemProgress bumps the generation -> stamp
     * changes -> forced miss; body must still be exact. */
    SESSION_CHECK(xllmSessionSetSystemPrompt(pSession, "v2 identity", &tError),
                  "identity upgrade applies");
    xllmRequestInit(&tReq);
    xllmRequestInit(&tRef);
    SESSION_CHECK(xllmSessionBuildRequestView(pSession, &tReq, &tError) &&
                      xllmSessionBuildRequest(pSession, &tRef, &tError),
                  "post-identity renders");
    sRef = xllmClientBuildRequestJson(pClient, &tRef, &tError);
    sCached = xllm__client_serialize_body(pClient, &tReq, &tError);
    SESSION_CHECK(sRef && sCached && strcmp(sRef, sCached) == 0 &&
                      strstr(sCached, "v2 identity") != NULL,
                  "stamp-invalidation miss rebuilds exactly");
    xllm__free(sRef);
    xllm__free(sCached);
    xllmRequestUnit(&tReq);
    xllmRequestUnit(&tRef);

cleanup:
    xllmSessionDestroy(pSession);
    xllmClientDestroy(pClient);
}

/* ------------------------------------------------------------------ */
/* v3: immutable snapshots are the only cross-thread read surface      */
/* ------------------------------------------------------------------ */

typedef struct test_snapshot_worker
{
    xllm_session_snapshot *pSnapshot;
    xatomic32 *pStart;
    xatomic32 *pReady;
    xatomic32 *pContinue;
    xatomic32 *pFailures;
    uint64_t uExpectedTurn;
} test_snapshot_worker;

static bool test_snapshot_read_once(const test_snapshot_worker *pWorker)
{
    xllm_session_config tConfig;
    xllm_session_budget tBudget;
    xllm_session_stats tStats;
    xllm_session_summary tSummary;
    xllm_session_tail tTail;
    xllm_pending_tool_call tPending;
    xllm_file_ledger tLedger;
    xllm_request tRequest;
    xllm_error tError;
    bool bOk;
    xllmSessionBudgetInit(&tBudget);
    xllmRequestInit(&tRequest);
    bOk = xllmSessionSnapshotGetConfig(pWorker->pSnapshot, &tConfig) && tConfig.sSummaryStyle &&
          strcmp(tConfig.sSummaryStyle, "coding") == 0 && tConfig.sSnapshotPath &&
          strcmp(tConfig.sSnapshotPath, "snapshot-state.json") == 0 &&
          xllmSessionSnapshotGetBudget(pWorker->pSnapshot, &tBudget) &&
          tBudget.uAbiVersion == XLLM_SESSION_BUDGET_ABI_VERSION &&
          xllmSessionSnapshotGetState(pWorker->pSnapshot) == XLLM_SESSION_STATE_TOOLS_PENDING &&
          xllmSessionSnapshotCurrentTurn(pWorker->pSnapshot) == pWorker->uExpectedTurn &&
          xllmSessionSnapshotGetStats(pWorker->pSnapshot, &tStats) &&
          tStats.uCurrentTurn == pWorker->uExpectedTurn && tStats.uPendingToolCalls == 1u &&
          xllmSessionSnapshotGetSummary(pWorker->pSnapshot, &tSummary) && tSummary.sText &&
          strstr(tSummary.sText, "snapshot continuity") != NULL &&
          xllmSessionSnapshotGetTail(pWorker->pSnapshot, &tTail) && tTail.bHasMessage &&
          tTail.uTurn == pWorker->uExpectedTurn && tTail.eRole == XLLM_ROLE_ASSISTANT &&
          xllmSessionSnapshotPendingToolCallCount(pWorker->pSnapshot) == 1u &&
          xllmSessionSnapshotPendingToolCallAt(pWorker->pSnapshot, 0u, &tPending) &&
          tPending.uTurn == pWorker->uExpectedTurn && strcmp(tPending.sId, "snapshot-call") == 0 &&
          strcmp(tPending.sName, "read_file") == 0 &&
          strcmp(tPending.sArgumentsJson, "{\"path\":\"frozen.c\"}") == 0 &&
          xllmSessionSnapshotGetFileLedger(pWorker->pSnapshot, &tLedger) &&
          tLedger.iReadFileCount == 1u && tLedger.iModifiedFileCount == 1u &&
          strcmp(tLedger.psReadFiles[0], "src/frozen.c") == 0 &&
          strcmp(tLedger.psModifiedFiles[0], "src/frozen.c") == 0 &&
          xllmSessionSnapshotBuildRequest(pWorker->pSnapshot, &tRequest, &tError) &&
          request_has_text(&tRequest, "snapshot hook marker") &&
          !request_has_text(&tRequest, "post-snapshot response") && tRequest.sModel &&
          strcmp(tRequest.sModel, "snapshot-model") == 0 && tRequest.iToolCount == 1u &&
          tRequest.pExtraHeaders == NULL && tRequest.iExtraHeaderCount == 0u &&
          tRequest.pCancel == NULL && tRequest.uDeadline == UINT64_MAX && tRequest.pHooks == NULL &&
          tRequest.pStablePrefixOwner == NULL;
    xllmRequestUnit(&tRequest);
    return bOk;
}

static int32 test_snapshot_reader_thread(ptr pData)
{
    test_snapshot_worker *pWorker = (test_snapshot_worker *)pData;
    unsigned i;
    while (xrtAtomic32Load(pWorker->pStart, XMEMORY_ACQUIRE) == 0u)
    {
        xrtThreadYield();
    }
    if (!test_snapshot_read_once(pWorker))
    {
        (void)xrtAtomic32FetchAdd(pWorker->pFailures, 1u, XMEMORY_RELAXED);
    }
    (void)xrtAtomic32FetchAdd(pWorker->pReady, 1u, XMEMORY_RELEASE);
    while (xrtAtomic32Load(pWorker->pContinue, XMEMORY_ACQUIRE) == 0u)
    {
        xrtThreadYield();
    }
    for (i = 0u; i < 8u; ++i)
    {
        if (!test_snapshot_read_once(pWorker))
        {
            (void)xrtAtomic32FetchAdd(pWorker->pFailures, 1u, XMEMORY_RELAXED);
        }
    }
    xllmSessionSnapshotRelease(pWorker->pSnapshot);
    return 0;
}

static bool test_snapshot_render_complete(xllm_session *pSession, xllm_request *pRequest,
                                          void *pUserData)
{
    static const xllm_header tHeader = {"x-snapshot-runtime", "borrowed"};
    (void)pUserData;
    pRequest->pExtraHeaders = &tHeader;
    pRequest->iExtraHeaderCount = 1u;
    pRequest->pCancel = (xcancel *)pSession;
    pRequest->uDeadline = 123u;
    pRequest->pHooks = (const xllm_hooks *)pSession;
    return xllmRequestSetModel(pRequest, "snapshot-model") &&
           xllmRequestSetReasoningEffort(pRequest, "medium") &&
           xllmRequestSetExtraBody(pRequest, "{\"snapshot\":true}") &&
           xllmRequestAddTool(pRequest, "read_file", "Read a file", "{\"type\":\"object\"}",
                              true) &&
           xllmRequestSetToolChoice(pRequest, XLLM_TOOL_CHOICE_NAMED, "read_file") &&
           xllmRequestAddTextMessage(pRequest, XLLM_ROLE_USER, "snapshot hook marker");
}

static void test_immutable_snapshot(void)
{
    enum
    {
        TEST_SNAPSHOT_THREADS = 4
    };
    xllm_session_config tConfig;
    xllm_session *pSession = NULL;
    xllm_session_snapshot *pSnapshot = NULL;
    xllm_compaction *pCompaction = NULL;
    xllm_session_hooks tHooks;
    xllm_tool_call tCall;
    xllm_response tResponse;
    xllm_error tError;
    xllm_request tRequestA;
    xllm_request tRequestB;
    xthread *pThreads[TEST_SNAPSHOT_THREADS] = {0};
    test_snapshot_worker tWorkers[TEST_SNAPSHOT_THREADS];
    xatomic32 tStart;
    xatomic32 tReady;
    xatomic32 tContinue;
    xatomic32 tFailures;
    char sStyle[] = "coding";
    char sSnapshotPath[] = "snapshot-state.json";
    uint64_t uTurn = 0u;
    size_t i;
    size_t iCreated = 0u;

    xllmSessionConfigInit(&tConfig);
    tConfig.uContextWindowTokens = 32000u;
    tConfig.uMaxOutputTokens = 4000u;
    tConfig.uOutputReserveTokens = 4000u;
    tConfig.uSafetyReserveTokens = 1000u;
    tConfig.uKeepRecentTokens = 16u;
    tConfig.sSummaryStyle = sStyle;
    tConfig.sSnapshotPath = sSnapshotPath;
    pSession = xllmSessionCreate(&tConfig, &tError);
    SESSION_CHECK(pSession != NULL, "snapshot source session creates");
    if (!pSession)
    {
        return;
    }
    SESSION_CHECK(xllmSessionSetSystemPrompt(pSession, "immutable snapshot test", &tError),
                  "snapshot source identity installs");
    for (i = 0u; i < 3u; ++i)
    {
        char sUser[64];
        char sAssistant[64];
        (void)snprintf(sUser, sizeof(sUser), "snapshot history user %u", (unsigned)i);
        (void)snprintf(sAssistant, sizeof(sAssistant), "snapshot history assistant %u",
                       (unsigned)i);
        uTurn = xllmSessionBeginTurn(pSession);
        memset(&tResponse, 0, sizeof(tResponse));
        tResponse.sContent = sAssistant;
        SESSION_CHECK(uTurn != 0u &&
                          xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, sUser, 0u) &&
                          xllmSessionAddAssistantResponse(pSession, uTurn, &tResponse),
                      "snapshot source history turn completes");
    }
    pCompaction = xllmSessionPrepareCompaction(pSession, true, &tError);
    SESSION_CHECK(pCompaction &&
                      xllmSessionCommitCompaction(pSession, pCompaction,
                                                  test_pi_summary("snapshot continuity"), &tError),
                  "snapshot source summary commits");
    xllmCompactionDestroy(pCompaction);
    pCompaction = NULL;

    uTurn = xllmSessionBeginTurn(pSession);
    memset(&tCall, 0, sizeof(tCall));
    tCall.sId = "snapshot-call";
    tCall.sName = "read_file";
    tCall.sArgumentsJson = "{\"path\":\"frozen.c\"}";
    memset(&tResponse, 0, sizeof(tResponse));
    tResponse.sContent = "";
    tResponse.pToolCalls = &tCall;
    tResponse.iToolCallCount = 1u;
    SESSION_CHECK(uTurn != 0u &&
                      xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, "inspect frozen.c", 0u) &&
                      xllmSessionAddAssistantResponse(pSession, uTurn, &tResponse) &&
                      xllmSessionNoteFileRead(pSession, "src/frozen.c") &&
                      xllmSessionNoteFileModified(pSession, "src/frozen.c"),
                  "snapshot source pending turn and ledger prepare");
    memset(&tHooks, 0, sizeof(tHooks));
    tHooks.pRenderComplete = test_snapshot_render_complete;
    SESSION_CHECK(xllmSessionSetHooks(pSession, &tHooks), "snapshot render hook installs");
    pSnapshot = xllmSessionSnapshotCreate(pSession, &tError);
    SESSION_CHECK(pSnapshot != NULL, "stable writer publishes immutable snapshot");
    if (!pSnapshot)
    {
        xllmSessionDestroy(pSession);
        return;
    }
    sStyle[0] = 'x';
    sSnapshotPath[0] = 'x';

    xrtAtomic32Init(&tStart, 0u);
    xrtAtomic32Init(&tReady, 0u);
    xrtAtomic32Init(&tContinue, 0u);
    xrtAtomic32Init(&tFailures, 0u);
    memset(tWorkers, 0, sizeof(tWorkers));
    for (i = 0u; i < TEST_SNAPSHOT_THREADS; ++i)
    {
        tWorkers[i].pSnapshot = xllmSessionSnapshotRetain(pSnapshot);
        tWorkers[i].pStart = &tStart;
        tWorkers[i].pReady = &tReady;
        tWorkers[i].pContinue = &tContinue;
        tWorkers[i].pFailures = &tFailures;
        tWorkers[i].uExpectedTurn = uTurn;
        if (tWorkers[i].pSnapshot)
        {
            pThreads[i] = xrtThreadCreate(test_snapshot_reader_thread, &tWorkers[i], 0u);
        }
        if (pThreads[i])
        {
            ++iCreated;
        }
        else if (tWorkers[i].pSnapshot)
        {
            xllmSessionSnapshotRelease(tWorkers[i].pSnapshot);
            tWorkers[i].pSnapshot = NULL;
        }
    }
    SESSION_CHECK(iCreated == TEST_SNAPSHOT_THREADS, "fixed snapshot reader set starts");
    xrtAtomic32Store(&tStart, 1u, XMEMORY_RELEASE);
    while (xrtAtomic32Load(&tReady, XMEMORY_ACQUIRE) < iCreated)
    {
        xrtThreadYield();
    }

    SESSION_CHECK(xllmSessionAddToolResult(pSession, uTurn, "snapshot-call", "frozen body") &&
                      xllmSessionBeginModelCall(pSession, &tError),
                  "writer advances independently after snapshot publication");
    SESSION_CHECK(xllmSessionSnapshotCreate(pSession, &tError) == NULL &&
                      tError.eCode == XLLM_ERROR_INVALID_ARGUMENT,
                  "model-in-flight state rejects snapshot capture");
    SESSION_CHECK(xllmSessionAbortModelCall(pSession, &tError),
                  "writer returns from transient model state");
    memset(&tResponse, 0, sizeof(tResponse));
    tResponse.sContent = "post-snapshot response";
    SESSION_CHECK(xllmSessionAddAssistantResponse(pSession, uTurn, &tResponse) &&
                      xllmSessionNoteFileRead(pSession, "src/after.c"),
                  "writer mutates source without changing published snapshot");
    xllmSessionDestroy(pSession);
    pSession = NULL;
    xrtAtomic32Store(&tContinue, 1u, XMEMORY_RELEASE);
    for (i = 0u; i < TEST_SNAPSHOT_THREADS; ++i)
    {
        if (pThreads[i])
        {
            (void)xrtThreadWait(pThreads[i]);
            xrtThreadDestroy(pThreads[i]);
        }
    }
    SESSION_CHECK(xrtAtomic32Load(&tFailures, XMEMORY_ACQUIRE) == 0u,
                  "concurrent readers keep the frozen view after source destruction");

    xllmRequestInit(&tRequestA);
    xllmRequestInit(&tRequestB);
    SESSION_CHECK(xllmSessionSnapshotBuildRequest(pSnapshot, &tRequestA, &tError) &&
                      tRequestA.iMessageCount != 0u &&
                      xllmMessageSetContent(&tRequestA.pMessages[0], "caller mutation") &&
                      xllmSessionSnapshotBuildRequest(pSnapshot, &tRequestB, &tError) &&
                      !request_has_text(&tRequestB, "caller mutation") &&
                      request_has_text(&tRequestB, "snapshot hook marker"),
                  "snapshot request clones are owned and isolated");
    xllmRequestUnit(&tRequestA);
    xllmRequestUnit(&tRequestB);
    xllmSessionSnapshotRelease(pSnapshot);

    /* An over-budget ledger remains inspectable; only its captured model
     * request is unavailable. This is required by UI diagnostics. */
    {
        xllm_session *pOverflow;
        xllm_session_snapshot *pOverflowSnapshot;
        char *sLarge = make_large_output(40000u, 7u);
        xllmSessionConfigInit(&tConfig);
        tConfig.uContextWindowTokens = 8000u;
        tConfig.uMaxOutputTokens = 1000u;
        tConfig.uOutputReserveTokens = 1000u;
        tConfig.uSafetyReserveTokens = 500u;
        pOverflow = xllmSessionCreate(&tConfig, &tError);
        uTurn = pOverflow ? xllmSessionBeginTurn(pOverflow) : 0u;
        SESSION_CHECK(pOverflow && sLarge && uTurn != 0u &&
                          xllmSessionAddText(pOverflow, uTurn, XLLM_ROLE_USER, sLarge, 0u),
                      "overflow snapshot source prepares");
        pOverflowSnapshot = pOverflow ? xllmSessionSnapshotCreate(pOverflow, &tError) : NULL;
        xllmRequestInit(&tRequestA);
        SESSION_CHECK(
            pOverflowSnapshot &&
                !xllmSessionSnapshotBuildRequest(pOverflowSnapshot, &tRequestA, &tError) &&
                tError.eCode == XLLM_ERROR_PROTOCOL,
            "overflow snapshot remains readable and reports request unavailability");
        xllmRequestUnit(&tRequestA);
        xllmSessionSnapshotRelease(pOverflowSnapshot);
        xllmSessionDestroy(pOverflow);
        free(sLarge);
    }
}

/* ------------------------------------------------------------------ */
/* v3: fork-at, suffix rewind and clear are audited ledger operations  */
/* ------------------------------------------------------------------ */

static void test_branch_rewrite_contract(void)
{
    static const char sJournalPath[] = "out/mdo-integration/fixtures/session_branch_test.jsonl";
    static const char sMissingSnapshot[] =
        "out/mdo-integration/fixtures/session_branch_missing.json";
    xllm_session_config tConfig;
    xllm_session_config tView;
    xllm_session *pSession = NULL;
    xllm_session *pFullFork = NULL;
    xllm_session *pBeforeTool = NULL;
    xllm_session *pAfterTool = NULL;
    xllm_session *pEmptyFork = NULL;
    xllm_session *pRecovered = NULL;
    xllm_session *pUnexpectedFork = NULL;
    xllm_session_snapshot *pUnexpectedSnapshot = NULL;
    xllm_session_hooks tHooks;
    test_event_sink tEvents = {0};
    xllm_tool_call tCall;
    xllm_response tResponse;
    xllm_error tError;
    xllm_session_stats tStats;
    xllm_file_ledger tLedger;
    xllm_request tRequest;
    char sStyle[] = "coding";
    char sDefaultSnapshot[] = "branch-default.json";
    uint64_t uTurn1;
    uint64_t uTurn2;
    uint64_t uBeforeTool;
    uint64_t uCallSequence;
    uint64_t uResultSequence;
    uint64_t uLast;

    (void)xrtFileDelete((str)sJournalPath);
    (void)xrtFileDelete((str)sMissingSnapshot);
    xllmSessionConfigInit(&tConfig);
    tConfig.sSummaryStyle = sStyle;
    tConfig.sSnapshotPath = sDefaultSnapshot;
    pSession = xllmSessionCreate(&tConfig, &tError);
    SESSION_CHECK(pSession != NULL, "branch source session creates");
    if (!pSession)
    {
        return;
    }
    sStyle[0] = 'x';
    sDefaultSnapshot[0] = 'x';
    SESSION_CHECK(xllmSessionGetConfig(pSession, &tView) && tView.sSummaryStyle &&
                      strcmp(tView.sSummaryStyle, "coding") == 0 && tView.sSnapshotPath &&
                      strcmp(tView.sSnapshotPath, "branch-default.json") == 0,
                  "session owns copied config strings");
    memset(&tHooks, 0, sizeof(tHooks));
    tHooks.pOnEvent = test_on_event;
    tHooks.pUserData = &tEvents;
    SESSION_CHECK(xllmSessionSetHooks(pSession, &tHooks) &&
                      xllmSessionEnableJournal(pSession, sJournalPath, &tError),
                  "branch source attaches an audited journal");

    uTurn1 = xllmSessionBeginTurn(pSession);
    memset(&tResponse, 0, sizeof(tResponse));
    tResponse.sContent = "base answer";
    SESSION_CHECK(uTurn1 == 1u &&
                      xllmSessionAddText(pSession, uTurn1, XLLM_ROLE_USER, "base prompt", 0u) &&
                      xllmSessionAddAssistantResponse(pSession, uTurn1, &tResponse) &&
                      xllmSessionNoteFileRead(pSession, "src/base.c"),
                  "branch base turn and ledger note commit");

    uTurn2 = xllmSessionBeginTurn(pSession);
    SESSION_CHECK(uTurn2 == 2u &&
                      xllmSessionAddText(pSession, uTurn2, XLLM_ROLE_USER, "edit prompt", 0u),
                  "branch edit point appends");
    uBeforeTool = xllmSessionLastSequence(pSession);
    memset(&tCall, 0, sizeof(tCall));
    tCall.sId = "branch-call";
    tCall.sName = "read_file";
    tCall.sArgumentsJson = "{\"path\":\"src/old.c\"}";
    memset(&tResponse, 0, sizeof(tResponse));
    tResponse.sContent = "";
    tResponse.pToolCalls = &tCall;
    tResponse.iToolCallCount = 1u;
    SESSION_CHECK(xllmSessionAddAssistantResponse(pSession, uTurn2, &tResponse),
                  "branch tool call appends");
    uCallSequence = xllmSessionLastSequence(pSession);
    SESSION_CHECK(xllmSessionAddToolResult(pSession, uTurn2, "branch-call", "old file body"),
                  "branch tool result appends");
    uResultSequence = xllmSessionLastSequence(pSession);
    memset(&tResponse, 0, sizeof(tResponse));
    tResponse.sContent = "old final answer";
    SESSION_CHECK(xllmSessionAddAssistantResponse(pSession, uTurn2, &tResponse) &&
                      xllmSessionNoteFileModified(pSession, "src/old.c"),
                  "branch suffix and ledger note commit");
    uLast = xllmSessionLastSequence(pSession);

    SESSION_CHECK(xllmSessionForkAt(pSession, uCallSequence, &tError) == NULL &&
                      tError.eCode == XLLM_ERROR_INVALID_ARGUMENT,
                  "fork rejects a boundary that splits a completed tool pair");
    pBeforeTool = xllmSessionForkAt(pSession, uBeforeTool, &tError);
    SESSION_CHECK(pBeforeTool &&
                      xllmSessionGetState(pBeforeTool) == XLLM_SESSION_STATE_USER_APPENDED &&
                      xllmSessionLastSequence(pBeforeTool) == uBeforeTool &&
                      xllmSessionGetFileLedger(pBeforeTool, &tLedger) &&
                      tLedger.iReadFileCount == 1u && tLedger.iModifiedFileCount == 0u,
                  "fork-at keeps the exact prefix and sequence-scoped ledger");
    pAfterTool = xllmSessionForkAt(pSession, uResultSequence, &tError);
    memset(&tResponse, 0, sizeof(tResponse));
    tResponse.sContent = "fork completion";
    SESSION_CHECK(pAfterTool &&
                      xllmSessionGetState(pAfterTool) == XLLM_SESSION_STATE_USER_APPENDED &&
                      xllmSessionAddAssistantResponse(pAfterTool, uTurn2, &tResponse) &&
                      xllmSessionGetState(pAfterTool) == XLLM_SESSION_STATE_TURN_COMPLETE,
                  "pair-complete fork resumes from a resolved tool boundary");
    pEmptyFork = xllmSessionForkAt(pSession, 0u, &tError);
    SESSION_CHECK(pEmptyFork && xllmSessionGetState(pEmptyFork) == XLLM_SESSION_STATE_IDLE &&
                      xllmSessionLastSequence(pEmptyFork) == 0u &&
                      xllmSessionGetFileLedger(pEmptyFork, &tLedger) &&
                      tLedger.iReadFileCount == 0u && tLedger.iModifiedFileCount == 0u,
                  "zero-boundary fork creates an empty independent ledger");
    pFullFork = xllmSessionFork(pSession, &tError);
    SESSION_CHECK(pFullFork && xllmSessionGetStats(pFullFork, &tStats) && !tStats.bJournalEnabled &&
                      tStats.uJournalSequence == 0u && xllmSessionLastSequence(pFullFork) == uLast,
                  "full fork starts a fresh journal epoch");

    pSession->bInHook = true;
    pUnexpectedFork = xllmSessionFork(pSession, &tError);
    SESSION_CHECK(!pUnexpectedFork && tError.eCode == XLLM_ERROR_HOOK &&
                      !xllmSessionTruncateAfter(pSession, uBeforeTool, &tError) &&
                      tError.eCode == XLLM_ERROR_HOOK && !xllmSessionClear(pSession, &tError) &&
                      tError.eCode == XLLM_ERROR_HOOK,
                  "history mutations reject hook re-entry with the hook error code");
    pUnexpectedSnapshot = xllmSessionSnapshotCreate(pSession, &tError);
    SESSION_CHECK(!pUnexpectedSnapshot && tError.eCode == XLLM_ERROR_HOOK,
                  "snapshot creation rejects hook re-entry with the hook error code");
    pSession->bInHook = false;
    xllmSessionDestroy(pUnexpectedFork);
    xllmSessionSnapshotRelease(pUnexpectedSnapshot);

    SESSION_CHECK(!xllmSessionTruncateAfter(pSession, uCallSequence, &tError) &&
                      tError.eCode == XLLM_ERROR_INVALID_ARGUMENT &&
                      xllmSessionLastSequence(pSession) == uLast,
                  "truncate rejects a split pair without mutating the ledger");
    SESSION_CHECK(xllmSessionTruncateAfter(pSession, uBeforeTool, &tError) &&
                      xllmSessionLastSequence(pSession) == uBeforeTool &&
                      xllmSessionGetState(pSession) == XLLM_SESSION_STATE_USER_APPENDED &&
                      xllmSessionGetFileLedger(pSession, &tLedger) &&
                      tLedger.iReadFileCount == 1u && tLedger.iModifiedFileCount == 0u &&
                      test_has_event(&tEvents, XLLM_SESSION_EVENT_SESSION_TRUNCATED),
                  "truncate publishes the prefix and drops suffix ledger provenance");
    SESSION_CHECK(xllmSessionBeginModelCall(pSession, &tError) &&
                      !xllmSessionTruncateAfter(pSession, uBeforeTool - 1u, &tError) &&
                      tError.eCode == XLLM_ERROR_INVALID_ARGUMENT &&
                      xllmSessionAbortModelCall(pSession, &tError),
                  "transient model state rejects truncate");
    memset(&tResponse, 0, sizeof(tResponse));
    tResponse.sContent = "new branch answer";
    SESSION_CHECK(xllmSessionAddAssistantResponse(pSession, uTurn2, &tResponse) &&
                      xllmSessionLastSequence(pSession) == uBeforeTool + 1u,
                  "truncated source reuses the next message sequence");

    xllmSessionConfigInit(&tConfig);
    pRecovered = xllmSessionRecover(sMissingSnapshot, sJournalPath, &tConfig, &tError);
    xllmRequestInit(&tRequest);
    SESSION_CHECK(pRecovered && xllmSessionBuildRequest(pRecovered, &tRequest, &tError) &&
                      request_has_text(&tRequest, "new branch answer") &&
                      !request_has_text(&tRequest, "old file body") &&
                      !request_has_text(&tRequest, "old final answer") &&
                      xllmSessionGetFileLedger(pRecovered, &tLedger) &&
                      tLedger.iReadFileCount == 1u && tLedger.iModifiedFileCount == 0u,
                  "journal replay audits rewind and replacement suffix exactly");
    xllmRequestUnit(&tRequest);
    xllmSessionDestroy(pRecovered);
    pRecovered = NULL;

    SESSION_CHECK(xllmSessionClear(pSession, &tError) &&
                      xllmSessionGetState(pSession) == XLLM_SESSION_STATE_IDLE &&
                      xllmSessionCurrentTurn(pSession) == 0u &&
                      xllmSessionLastSequence(pSession) == 0u &&
                      xllmSessionGetStats(pSession, &tStats) && tStats.uEntryCount == 0u &&
                      xllmSessionGetFileLedger(pSession, &tLedger) &&
                      tLedger.iReadFileCount == 0u && tLedger.iModifiedFileCount == 0u &&
                      test_has_event(&tEvents, XLLM_SESSION_EVENT_SESSION_CLEARED),
                  "clear resets conversation state while retaining the runtime");
    uTurn1 = xllmSessionBeginTurn(pSession);
    SESSION_CHECK(uTurn1 == 1u && xllmSessionClear(pSession, &tError) &&
                      xllmSessionCurrentTurn(pSession) == 0u &&
                      xllmSessionGetState(pSession) == XLLM_SESSION_STATE_IDLE,
                  "clear resets an empty open turn instead of treating it as a no-op");
    uTurn1 = xllmSessionBeginTurn(pSession);
    memset(&tResponse, 0, sizeof(tResponse));
    tResponse.sContent = "fresh answer";
    SESSION_CHECK(uTurn1 == 1u &&
                      xllmSessionAddText(pSession, uTurn1, XLLM_ROLE_USER, "fresh prompt", 0u) &&
                      xllmSessionAddAssistantResponse(pSession, uTurn1, &tResponse) &&
                      xllmSessionNoteFileRead(pSession, "src/fresh.c"),
                  "cleared session starts a new sequence and turn epoch");
    pRecovered = xllmSessionRecover(sMissingSnapshot, sJournalPath, &tConfig, &tError);
    xllmRequestInit(&tRequest);
    SESSION_CHECK(pRecovered && xllmSessionBuildRequest(pRecovered, &tRequest, &tError) &&
                      request_has_text(&tRequest, "fresh prompt") &&
                      request_has_text(&tRequest, "fresh answer") &&
                      !request_has_text(&tRequest, "base prompt") &&
                      !request_has_text(&tRequest, "new branch answer") &&
                      xllmSessionGetFileLedger(pRecovered, &tLedger) &&
                      tLedger.iReadFileCount == 1u &&
                      strcmp(tLedger.psReadFiles[0], "src/fresh.c") == 0,
                  "journal replay audits clear and the new epoch exactly");
    xllmRequestUnit(&tRequest);
    xllmSessionDestroy(pRecovered);
    pRecovered = NULL;

    xllmSessionDestroy(pSession);
    pSession = NULL;
    SESSION_CHECK(pFullFork && xllmSessionGetConfig(pFullFork, &tView) && tView.sSummaryStyle &&
                      strcmp(tView.sSummaryStyle, "coding") == 0 && tView.sSnapshotPath &&
                      strcmp(tView.sSnapshotPath, "branch-default.json") == 0 &&
                      xllmSessionBeginTurn(pFullFork) == 3u,
                  "fork owns config and ledger after parent destruction");

    xllmSessionDestroy(pFullFork);
    xllmSessionDestroy(pBeforeTool);
    xllmSessionDestroy(pAfterTool);
    xllmSessionDestroy(pEmptyFork);
    (void)xrtFileDelete((str)sJournalPath);
    (void)xrtFileDelete((str)sMissingSnapshot);
}

/* ------------------------------------------------------------------ */
/* v3: bounded allocation and storage-failure atomicity                */
/* ------------------------------------------------------------------ */

static void test_persistence_fault_atomicity(void)
{
    static const char sSnapshot[] = "out/mdo-integration/fixtures/session_fault_snapshot.json";
    static const char sJournal[] = "out/mdo-integration/fixtures/session_fault_journal.jsonl";
    static const char sAmbiguousSnapshot[] =
        "out/mdo-integration/fixtures/session_fault_checkpoint.json";
    static const char sAmbiguousJournal[] =
        "out/mdo-integration/fixtures/session_fault_ambiguous.jsonl";
    static const char sMissingSnapshot[] =
        "out/mdo-integration/fixtures/session_fault_missing.json";
    xllm_session_config tConfig;
    xllm_session *pSession = NULL;
    xllm_session *pJournal = NULL;
    xllm_session *pAmbiguous = NULL;
    xllm_session *pRecovered = NULL;
    xllm_session *pLoaded = NULL;
    xllm_session *pChild = NULL;
    xllm_session_snapshot *pSnapshot = NULL;
    xllm_session_stats tBefore;
    xllm_session_stats tAfter;
    xllm_file_ledger tLedger;
    xllm_request tRequest;
    xllm_error tError;
    xllm_error tLast;
    uint8 *pBaseline = NULL;
    size_t iBaseline = 0u;
    uint8 *pJournalBaseline = NULL;
    size_t iJournalBaseline = 0u;
    uint64_t uTurn;
    uint64_t uParentLast;
    size_t i;
    size_t iFailures;
    bool bOk;

    xllm_session__test_alloc_reset();
    xllm_session__test_storage_reset();
    (void)xrtFileDelete((str)sSnapshot);
    (void)xrtFileDelete((str)sJournal);
    (void)xrtFileDelete((str)sAmbiguousSnapshot);
    (void)xrtFileDelete((str)sAmbiguousJournal);
    (void)xrtFileDelete((str)sMissingSnapshot);

    xllmSessionConfigInit(&tConfig);
    pSession = xllmSessionCreate(&tConfig, &tError);
    uTurn = pSession ? xllmSessionBeginTurn(pSession) : 0u;
    SESSION_CHECK(
        pSession && uTurn == 1u &&
            xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, "durable baseline", 0u) &&
            xllmSessionAddText(pSession, uTurn, XLLM_ROLE_ASSISTANT, "baseline answer", 0u) &&
            xllmSessionSave(pSession, sSnapshot, &tError),
        "fault fixture writes an initial durable snapshot");
    pBaseline = xrtFileReadAll(sSnapshot, &iBaseline);
    uTurn = pSession ? xllmSessionBeginTurn(pSession) : 0u;
    SESSION_CHECK(pBaseline && uTurn == 2u &&
                      xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, "new unsaved state", 0u),
                  "fault fixture advances only the in-memory ledger");

    xllm_session__test_storage_fail(XLLM_SESSION_TEST_STORAGE_ATOMIC_WRITE,
                                    XLLM_SESSION_TEST_STORAGE_MODE_FAIL, 1u);
    bOk = xllmSessionSave(pSession, sSnapshot, &tError);
    SESSION_CHECK(!bOk &&
                      test_is_injected_storage_error(&tError, XLLM_ERROR_STAGE_WRITE,
                                                     "snapshot-write", sSnapshot) &&
                      test_file_equals(sSnapshot, pBaseline, iBaseline),
                  "atomic snapshot failure preserves the previously published file");
    xllm_session__test_storage_reset();

    xllm_session__test_storage_fail(XLLM_SESSION_TEST_STORAGE_SNAPSHOT_READ,
                                    XLLM_SESSION_TEST_STORAGE_MODE_FAIL, 1u);
    pLoaded = xllmSessionLoad(sSnapshot, &tError);
    SESSION_CHECK(!pLoaded && test_is_injected_storage_error(&tError, XLLM_ERROR_STAGE_READ,
                                                             "snapshot-read", sSnapshot),
                  "snapshot read failure preserves path and XRT cause");
    xllmSessionDestroy(pLoaded);
    pLoaded = NULL;
    xllm_session__test_storage_reset();

    pJournal = xllmSessionCreate(&tConfig, &tError);
    xllm_session__test_storage_fail(XLLM_SESSION_TEST_STORAGE_STAT,
                                    XLLM_SESSION_TEST_STORAGE_MODE_FAIL, 1u);
    bOk = pJournal && xllmSessionEnableJournal(pJournal, sJournal, &tError);
    SESSION_CHECK(!bOk && test_is_injected_storage_error(&tError, XLLM_ERROR_STAGE_READ,
                                                         "journal-stat", sJournal),
                  "journal attach reports metadata failure without binding the path");
    xllm_session__test_storage_reset();
    SESSION_CHECK(pJournal && xllmSessionEnableJournal(pJournal, sJournal, &tError),
                  "journal fault fixture attaches after metadata recovery");
    xllm_session__test_storage_fail(XLLM_SESSION_TEST_STORAGE_OPEN,
                                    XLLM_SESSION_TEST_STORAGE_MODE_FAIL, 1u);
    uTurn = xllmSessionBeginTurn(pJournal);
    SESSION_CHECK(uTurn == 0u && xllmSessionCurrentTurn(pJournal) == 0u &&
                      xllmSessionGetLastPersistenceError(pJournal, &tLast) &&
                      test_is_injected_storage_error(&tLast, XLLM_ERROR_STAGE_WRITE, "journal-open",
                                                     sJournal),
                  "journal open failure leaves the turn ledger unchanged and diagnosable");
    xllm_session__test_storage_reset();
    uTurn = xllmSessionBeginTurn(pJournal);
    pJournalBaseline = xrtFileReadAll(sJournal, &iJournalBaseline);
    xllm_session__test_storage_fail(XLLM_SESSION_TEST_STORAGE_WRITE,
                                    XLLM_SESSION_TEST_STORAGE_MODE_SHORT_WRITE, 1u);
    bOk = xllmSessionAddText(pJournal, uTurn, XLLM_ROLE_USER, "must roll back after a short write",
                             0u);
    SESSION_CHECK(!bOk && pJournalBaseline && xllmSessionLastSequence(pJournal) == 0u &&
                      xllmSessionGetState(pJournal) == XLLM_SESSION_STATE_USER_APPENDED &&
                      test_file_equals(sJournal, pJournalBaseline, iJournalBaseline) &&
                      xllmSessionGetLastPersistenceError(pJournal, &tLast) &&
                      test_is_injected_storage_error(&tLast, XLLM_ERROR_STAGE_WRITE,
                                                     "journal-append", sJournal),
                  "short journal write rolls back bytes and in-memory publication");
    xllm_session__test_storage_reset();
    SESSION_CHECK(xllmSessionAddText(pJournal, uTurn, XLLM_ROLE_USER,
                                     "journal recovered after short write", 0u),
                  "journal remains writable after an exact short-write rollback");

    xllmSessionConfigInit(&tConfig);
    tConfig.eJournalDurability = XLLM_SESSION_DURABILITY_FLUSH;
    pAmbiguous = xllmSessionCreate(&tConfig, &tError);
    SESSION_CHECK(pAmbiguous && xllmSessionEnableJournal(pAmbiguous, sAmbiguousJournal, &tError) &&
                      xllmSessionBeginTurn(pAmbiguous) == 1u,
                  "ambiguous durability fixture prepares a flushed journal");
    xllm_session__test_storage_fail(XLLM_SESSION_TEST_STORAGE_FLUSH,
                                    XLLM_SESSION_TEST_STORAGE_MODE_FAIL, 1u);
    bOk = xllmSessionAddText(pAmbiguous, 1u, XLLM_ROLE_USER,
                             "durable despite uncertain flush acknowledgement", 0u);
    SESSION_CHECK(!bOk && xllmSessionLastSequence(pAmbiguous) == 0u &&
                      xllmSessionGetState(pAmbiguous) == XLLM_SESSION_STATE_RECOVERY_REQUIRED &&
                      xllmSessionGetLastPersistenceError(pAmbiguous, &tLast) &&
                      test_is_injected_storage_error(&tLast, XLLM_ERROR_STAGE_WRITE,
                                                     "journal-flush", sAmbiguousJournal),
                  "uncertain flush blocks the writer before in-memory publication");
    xllm_session__test_storage_reset();
    xllmSessionDestroy(pAmbiguous);
    pAmbiguous = NULL;
    pRecovered = xllmSessionRecover(sMissingSnapshot, sAmbiguousJournal, &tConfig, &tError);
    xllmRequestInit(&tRequest);
    SESSION_CHECK(
        pRecovered && xllmSessionBuildRequest(pRecovered, &tRequest, &tError) &&
            request_has_text(&tRequest, "durable despite uncertain flush acknowledgement") &&
            xllmSessionLastSequence(pRecovered) == 1u,
        "recovery treats the journal as authoritative after uncertain durability");
    xllmRequestUnit(&tRequest);

    xllm_session__test_storage_fail(XLLM_SESSION_TEST_STORAGE_DELETE,
                                    XLLM_SESSION_TEST_STORAGE_MODE_FAIL, 1u);
    bOk = xllmSessionCheckpoint(pRecovered, sAmbiguousSnapshot, &tError);
    SESSION_CHECK(!bOk &&
                      test_is_injected_storage_error(&tError, XLLM_ERROR_STAGE_WRITE,
                                                     "journal-delete", sAmbiguousJournal) &&
                      xrtFileExists((str)sAmbiguousSnapshot) &&
                      xrtFileExists((str)sAmbiguousJournal),
                  "checkpoint delete failure keeps both durable recovery sources");
    xllm_session__test_storage_reset();
    pLoaded = xllmSessionRecover(sAmbiguousSnapshot, sAmbiguousJournal, &tConfig, &tError);
    SESSION_CHECK(pLoaded && xllmSessionGetStats(pLoaded, &tAfter) && tAfter.uEntryCount == 1u &&
                      xllmSessionLastSequence(pLoaded) == 1u,
                  "checkpoint waterline suppresses duplicate replay after delete failure");
    xllmSessionDestroy(pLoaded);
    pLoaded = NULL;

    /* Direct ledger allocation is a three-step transaction: path copy, path
     * vector, and provenance vector.  Each failed point must leave count zero. */
    for (i = 1u; i <= 4u; ++i)
    {
        xllm_session *pLedgerSession = xllmSessionCreate(&tConfig, &tError);
        memset(&tLedger, 0, sizeof(tLedger));
        xllm_session__test_alloc_fail(i);
        bOk = pLedgerSession && xllmSessionNoteFileRead(pLedgerSession, "src/fault.c");
        SESSION_CHECK(pLedgerSession && xllmSessionGetFileLedger(pLedgerSession, &tLedger) &&
                          ((i <= 3u && !bOk && tLedger.iReadFileCount == 0u) ||
                           (i == 4u && bOk && tLedger.iReadFileCount == 1u)),
                      "ledger allocation failure commits all-or-nothing");
        xllm_session__test_alloc_reset();
        xllmSessionDestroy(pLedgerSession);
    }

    /* A failed entry-vector growth cannot consume a sequence or publish a
     * partially cloned message. */
    {
        xllm_session *pEntrySession = xllmSessionCreate(&tConfig, &tError);
        uTurn = xllmSessionBeginTurn(pEntrySession);
        xllm_session__test_alloc_fail(1u);
        bOk = xllmSessionAddText(pEntrySession, uTurn, XLLM_ROLE_USER,
                                 "allocation must fail atomically", 0u);
        SESSION_CHECK(!bOk && xllmSessionLastSequence(pEntrySession) == 0u &&
                          xllmSessionGetStats(pEntrySession, &tAfter) && tAfter.uEntryCount == 0u,
                      "message allocation failure leaves sequence and ledger unchanged");
        xllm_session__test_alloc_reset();
        xllmSessionDestroy(pEntrySession);
    }

    SESSION_CHECK(xllmSessionGetStats(pSession, &tBefore) &&
                      xllmSessionNoteFileRead(pSession, "src/parent.c") &&
                      xllmSessionGetStats(pSession, &tBefore),
                  "OOM branch fixture captures the parent baseline");
    uParentLast = xllmSessionLastSequence(pSession);
    iFailures = 0u;
    for (i = 1u; i <= 32u; ++i)
    {
        xllm_session__test_alloc_fail(i);
        pChild = xllmSessionFork(pSession, &tError);
        if (pChild)
        {
            xllmSessionDestroy(pChild);
            pChild = NULL;
            break;
        }
        ++iFailures;
        SESSION_CHECK(tError.eCode == XLLM_ERROR_OUT_OF_MEMORY &&
                          xllmSessionGetStats(pSession, &tAfter) &&
                          tAfter.uEntryCount == tBefore.uEntryCount &&
                          xllmSessionLastSequence(pSession) == uParentLast,
                      "fork allocation failure leaves the parent ledger unchanged");
    }
    xllm_session__test_alloc_reset();
    SESSION_CHECK(iFailures >= 5u && i <= 32u,
                  "fork succeeds after every session-owned allocation point is exercised");

    iFailures = 0u;
    for (i = 1u; i <= 64u; ++i)
    {
        xllm_session__test_alloc_fail(i);
        pSnapshot = xllmSessionSnapshotCreate(pSession, &tError);
        if (pSnapshot)
        {
            xllmSessionSnapshotRelease(pSnapshot);
            pSnapshot = NULL;
            break;
        }
        ++iFailures;
        SESSION_CHECK(tError.eCode == XLLM_ERROR_OUT_OF_MEMORY &&
                          xllmSessionGetStats(pSession, &tAfter) &&
                          tAfter.uEntryCount == tBefore.uEntryCount,
                      "immutable snapshot OOM leaves the source ledger unchanged");
    }
    xllm_session__test_alloc_reset();
    SESSION_CHECK(iFailures >= 3u && i <= 64u,
                  "immutable snapshot succeeds after bounded allocation fault coverage");

    iFailures = 0u;
    for (i = 1u; i <= 64u; ++i)
    {
        xllm_session__test_alloc_fail(i);
        bOk = xllmSessionSave(pSession, sSnapshot, &tError);
        if (bOk)
        {
            break;
        }
        ++iFailures;
        SESSION_CHECK(tError.eCode == XLLM_ERROR_OUT_OF_MEMORY &&
                          test_file_equals(sSnapshot, pBaseline, iBaseline),
                      "snapshot serialization OOM preserves the old snapshot bytes");
    }
    xllm_session__test_alloc_reset();
    SESSION_CHECK(iFailures > 0u && i <= 64u,
                  "snapshot save succeeds after bounded serialization fault coverage");

    iFailures = 0u;
    for (i = 1u; i <= 64u; ++i)
    {
        xllm_session__test_alloc_fail(i);
        pLoaded = xllmSessionLoad(sSnapshot, &tError);
        if (pLoaded)
        {
            xllmSessionDestroy(pLoaded);
            pLoaded = NULL;
            break;
        }
        ++iFailures;
        SESSION_CHECK(tError.eCode == XLLM_ERROR_OUT_OF_MEMORY,
                      "snapshot load reports injected allocation failure as OOM");
    }
    xllm_session__test_alloc_reset();
    SESSION_CHECK(iFailures > 0u && i <= 64u,
                  "snapshot load succeeds after bounded allocation fault coverage");

    xllmSessionDestroy(pRecovered);
    xllmSessionDestroy(pAmbiguous);
    xllmSessionDestroy(pJournal);
    xllmSessionDestroy(pSession);
    xllmSessionDestroy(pLoaded);
    xllmSessionDestroy(pChild);
    xllmSessionSnapshotRelease(pSnapshot);
    xrtFree(pBaseline);
    xrtFree(pJournalBaseline);
    xllm_session__test_alloc_reset();
    xllm_session__test_storage_reset();
    (void)xrtFileDelete((str)sSnapshot);
    (void)xrtFileDelete((str)sJournal);
    (void)xrtFileDelete((str)sAmbiguousSnapshot);
    (void)xrtFileDelete((str)sAmbiguousJournal);
    (void)xrtFileDelete((str)sMissingSnapshot);
}

static void test_memory_restore(void)
{
    const char *sSnapshot = "out/mdo-integration/fixtures/memory_restore_snapshot.json";
    const char *sJournal = "out/mdo-integration/fixtures/memory_restore_journal.jsonl";
    xllm_session *pSource = NULL, *pRestored = NULL;
    xllm_session_restore_options tOptions;
    xllm_session_config tConfig;
    xllm_request tWant, tGot;
    xllm_error tError;
    char *pSnapshot = NULL, *pJournal = NULL;
    size_t iSnapshot = 0u, iJournal = 0u, i, iFailures = 0u;
    xcancel *pCancel = NULL;
    (void)xrtFileDelete(sSnapshot);
    (void)xrtFileDelete(sJournal);
    xllmRequestInit(&tWant);
    xllmRequestInit(&tGot);
    SESSION_CHECK(~xllm_session__checksum_update(UINT32_MAX, "123456789", 9u) ==
                      UINT32_C(0xcbf43926),
                  "CRC nibble update matches the ISO-HDLC known answer");
    pSource = xllmSessionCreate(NULL, &tError);
    SESSION_CHECK(
        pSource && xllmSessionEnableJournal(pSource, sJournal, &tError) &&
            xllmSessionSetSystemPrompt(pSource, "Pinned identity", &tError) &&
            xllmSessionBeginTurn(pSource) == 1u && add_tool_round(pSource, 1u, 1u, "tool output") &&
            xllmSessionAddText(pSource, 1u, XLLM_ROLE_ASSISTANT, "First answer", 0u) &&
            xllmSessionNoteFileRead(pSource, "../source.c") &&
            xllmSessionSave(pSource, sSnapshot, &tError) && xllmSessionBeginTurn(pSource) == 2u &&
            xllmSessionAddText(pSource, 2u, XLLM_ROLE_USER, "Second question", 0u) &&
            xllmSessionAddText(pSource, 2u, XLLM_ROLE_ASSISTANT, "Second answer", 0u) &&
            xllmSessionNoteFileModified(pSource, "C:\\source\\file.c") &&
            xllmSessionBuildRequest(pSource, &tWant, &tError),
        "memory restore fixture writes covered records and a real tail");
    pSnapshot = (char *)xrtFileReadAll(sSnapshot, &iSnapshot);
    pJournal = (char *)xrtFileReadAll(sJournal, &iJournal);
    if (!pSnapshot || !pJournal)
    {
        SESSION_CHECK(false, "memory restore fixture loads");
        goto done;
    }
    pRestored = xllmSessionRestore(pSnapshot, iSnapshot, pJournal, iJournal, NULL, &tError);
    SESSION_CHECK(pRestored && !xllmSessionHasDriver(pRestored) &&
                      !xllmSessionJournalPath(pRestored) &&
                      xllmSessionGetConfig(pRestored, &tConfig) && !tConfig.sSnapshotPath &&
                      xllmSessionGetSystemPrompt(pRestored) &&
                      strcmp(xllmSessionGetSystemPrompt(pRestored), "Pinned identity") == 0 &&
                      xllmSessionBuildRequest(pRestored, &tGot, &tError) &&
                      round_trip_requests_equal(&tWant, &tGot),
                  "memory snapshot+tail restores the exact unbound model request");
    xllmRequestUnit(&tGot);
    xllmSessionDestroy(pRestored);
    pRestored = NULL;
    pRestored = xllmSessionRestore(NULL, 0u, pJournal, iJournal, NULL, &tError);
    SESSION_CHECK(pRestored && xllmSessionBuildRequest(pRestored, &tGot, &tError) &&
                      round_trip_requests_equal(&tWant, &tGot),
                  "memory journal-only replay preserves message/tool/ledger state");
    xllmRequestUnit(&tGot);
    xllmSessionDestroy(pRestored);
    pRestored = NULL;
    SESSION_CHECK(test_file_equals(sSnapshot, pSnapshot, iSnapshot) &&
                      test_file_equals(sJournal, pJournal, iJournal),
                  "memory restore never rewrites source snapshot/journal files");
    xllmSessionRestoreOptionsInit(&tOptions);
    tOptions.iMaxSnapshotBytes = 1u;
    pRestored = xllmSessionRestore(pSnapshot, iSnapshot, pJournal, iJournal, &tOptions, &tError);
    SESSION_CHECK(!pRestored && tError.eCode == XLLM_ERROR_LIMIT,
                  "memory restore rejects snapshot byte overflow");
    xllmSessionRestoreOptionsInit(&tOptions);
    tOptions.iMaxJournalBytes = 1u;
    pRestored = xllmSessionRestore(pSnapshot, iSnapshot, pJournal, iJournal, &tOptions, &tError);
    SESSION_CHECK(!pRestored && tError.eCode == XLLM_ERROR_LIMIT,
                  "memory restore rejects journal byte overflow");
    xllmSessionRestoreOptionsInit(&tOptions);
    tOptions.iMaxRecords = 1u;
    pRestored = xllmSessionRestore(pSnapshot, iSnapshot, pJournal, iJournal, &tOptions, &tError);
    SESSION_CHECK(!pRestored && tError.eCode == XLLM_ERROR_LIMIT,
                  "covered records consume the memory replay budget");
    xllmSessionRestoreOptionsInit(&tOptions);
    tOptions.iMaxValues = 1u;
    pRestored = xllmSessionRestore(pSnapshot, iSnapshot, NULL, 0u, &tOptions, &tError);
    SESSION_CHECK(!pRestored && tError.eCode == XLLM_ERROR_LIMIT,
                  "snapshot value limit retains a resource error");
    pRestored = xllmSessionRestore(NULL, 0u, pJournal, iJournal, &tOptions, &tError);
    SESSION_CHECK(!pRestored && tError.eCode == XLLM_ERROR_LIMIT,
                  "journal value limit retains a resource error");
    xllmSessionRestoreOptionsInit(&tOptions);
    tOptions.uMaxDepth = 1u;
    pRestored = xllmSessionRestore(pSnapshot, iSnapshot, NULL, 0u, &tOptions, &tError);
    SESSION_CHECK(!pRestored && tError.eCode == XLLM_ERROR_LIMIT,
                  "snapshot depth limit rejects before replay");
    xllmSessionRestoreOptionsInit(&tOptions);
    tOptions.uMaxDepth = 257u;
    pRestored = xllmSessionRestore(NULL, 0u, NULL, 0u, &tOptions, &tError);
    SESSION_CHECK(!pRestored && tError.eCode == XLLM_ERROR_INVALID_ARGUMENT,
                  "restore bounds recursive traversal depth");
    {
        static const char sNul[] =
            "{\"format\":\"xllm-session\",\"version\":1,\"entries\":[],\"unused\":\"a\\u0000b\"}";
        pRestored = xllmSessionRestore(sNul, sizeof(sNul) - 1u, NULL, 0u, NULL, &tError);
        SESSION_CHECK(!pRestored && tError.eCode == XLLM_ERROR_PARSE,
                      "memory restoration rejects embedded NUL values");
    }
    xllmSessionConfigInit(&tConfig);
    tConfig.sSnapshotPath = "must-not-be-used.json";
    xllmSessionRestoreOptionsInit(&tOptions);
    tOptions.pConfigIfNew = &tConfig;
    pRestored = xllmSessionRestore(NULL, 0u, NULL, 0u, &tOptions, &tError);
    SESSION_CHECK(pRestored && xllmSessionGetConfig(pRestored, &tConfig) && !tConfig.sSnapshotPath,
                  "empty byte restore never inherits a configured persistence path");
    xllmSessionDestroy(pRestored);
    pRestored = NULL;
    xllmSessionRestoreOptionsInit(&tOptions);
    tOptions.uDeadline = 1u;
    pRestored = xllmSessionRestore(pSnapshot, iSnapshot, pJournal, iJournal, &tOptions, &tError);
    SESSION_CHECK(!pRestored && tError.eCode == XLLM_ERROR_TIMEOUT,
                  "expired memory restoration stops before creating state");
    xllmSessionRestoreOptionsInit(&tOptions);
    tOptions.uAbiVersion += 1u;
    pRestored = xllmSessionRestore(pSnapshot, iSnapshot, pJournal, iJournal, &tOptions, &tError);
    SESSION_CHECK(!pRestored && tError.eCode == XLLM_ERROR_INVALID_ARGUMENT,
                  "memory restoration validates its ABI");
    pRestored = xllmSessionRestore(NULL, 1u, NULL, 0u, NULL, &tError);
    SESSION_CHECK(!pRestored && tError.eCode == XLLM_ERROR_INVALID_ARGUMENT,
                  "nonzero bytes require a buffer");
    pRestored = xllmSessionRestore(pSnapshot, iSnapshot, pJournal, iJournal - 1u, NULL, &tError);
    SESSION_CHECK(!pRestored && tError.eCode == XLLM_ERROR_PARSE &&
                      test_file_equals(sJournal, pJournal, iJournal),
                  "memory restore rejects an otherwise valid unterminated tail without repair");
    pCancel = xrtCancelCreate();
    xllmSessionRestoreOptionsInit(&tOptions);
    tOptions.pCancel = pCancel;
    xllm_session__test_restore_cancel = pCancel;
    xllm_session__test_restore_cancel_after = 1u;
    pRestored = xllmSessionRestore(pSnapshot, iSnapshot, pJournal, iJournal, &tOptions, &tError);
    SESSION_CHECK(pCancel && !pRestored && tError.eCode == XLLM_ERROR_CANCELLED,
                  "cancellation after one replay record releases the entire partial context");
    xllm_session__test_restore_cancel = NULL;
    xllm_session__test_restore_cancel_after = 0u;
    xrtCancelDestroy(pCancel);
    pCancel = NULL;
    for (i = XLLM_SESSION_TEST_STORAGE_STAT; i <= XLLM_SESSION_TEST_STORAGE_JOURNAL_READ; ++i)
    {
        xllm_session__test_storage_fail((xllm_session_test_storage_operation)i,
                                        XLLM_SESSION_TEST_STORAGE_MODE_FAIL, 1u);
        pRestored = xllmSessionRestore(pSnapshot, iSnapshot, pJournal, iJournal, NULL, &tError);
        SESSION_CHECK(pRestored && xllm_session__test_storage_fault_state.iSeen == 0u,
                      "memory restoration bypasses every instrumented storage operation");
        xllmSessionDestroy(pRestored);
        pRestored = NULL;
    }
    xllm_session__test_storage_reset();
    for (i = 1u; i <= 128u; ++i)
    {
        xllm_session__test_alloc_fail(i);
        pRestored = xllmSessionRestore(pSnapshot, iSnapshot, pJournal, iJournal, NULL, &tError);
        if (pRestored)
        {
            xllmSessionDestroy(pRestored);
            pRestored = NULL;
            break;
        }
        ++iFailures;
        SESSION_CHECK(tError.eCode == XLLM_ERROR_OUT_OF_MEMORY,
                      "memory restore reports each injected allocation failure");
    }
    xllm_session__test_alloc_reset();
    SESSION_CHECK(iFailures && i <= 128u,
                  "memory restore succeeds after bounded allocation fault coverage");
    for (i = 1u; i <= 2u; ++i)
    {
        char sFixture[80];
        size_t iFixture = 0u;
        char *pFixture;
        (void)snprintf(sFixture, sizeof(sFixture), "extlibs/xllm-session/tests/fixtures/snapshot-v%u.json", (unsigned)i);
        pFixture = (char *)xrtFileReadAll(sFixture, &iFixture);
        pRestored =
            pFixture ? xllmSessionRestore(pFixture, iFixture, NULL, 0u, NULL, &tError) : NULL;
        SESSION_CHECK(pRestored && xllmSessionBuildRequest(pRestored, &tGot, &tError) &&
                          tGot.iMessageCount,
                      "legacy snapshot bytes restore through the same migration core");
        xrtFree(pFixture);
        xllmRequestUnit(&tGot);
        xllmSessionDestroy(pRestored);
        pRestored = NULL;
        (void)snprintf(sFixture, sizeof(sFixture), "extlibs/xllm-session/tests/fixtures/journal-v%u.jsonl", (unsigned)i);
        pFixture = (char *)xrtFileReadAll(sFixture, &iFixture);
        pRestored =
            pFixture ? xllmSessionRestore(NULL, 0u, pFixture, iFixture, NULL, &tError) : NULL;
        SESSION_CHECK(pRestored && xllmSessionBuildRequest(pRestored, &tGot, &tError) &&
                          tGot.iMessageCount,
                      "legacy journal bytes restore through the same migration core");
        xrtFree(pFixture);
        xllmRequestUnit(&tGot);
        xllmSessionDestroy(pRestored);
        pRestored = NULL;
    }
    SESSION_CHECK(xllmSessionTruncateAfter(pSource, 0u, &tError) &&
                      xllmSessionClear(pSource, &tError) && xllmSessionBeginTurn(pSource) == 1u &&
                      xllmSessionAddText(pSource, 1u, XLLM_ROLE_USER, "After rewrite", 0u),
                  "memory restoration fixture appends actual rewind and clear records");
    xllmRequestUnit(&tWant);
    SESSION_CHECK(xllmSessionBuildRequest(pSource, &tWant, &tError),
                  "rewrite reference request renders");
    xrtFree(pJournal);
    pJournal = (char *)xrtFileReadAll(sJournal, &iJournal);
    pRestored = pJournal
                    ? xllmSessionRestore(pSnapshot, iSnapshot, pJournal, iJournal, NULL, &tError)
                    : NULL;
    SESSION_CHECK(pRestored && xllmSessionBuildRequest(pRestored, &tGot, &tError) &&
                      round_trip_requests_equal(&tWant, &tGot),
                  "memory restoration replays rewind/clear without reviving removed entries");
    xllmRequestUnit(&tGot);
    xllmSessionDestroy(pRestored);
    pRestored = NULL;
    pRestored = xllmSessionRestore(pSnapshot, iSnapshot, pJournal, iJournal, NULL, &tError);
    memset(pSnapshot, 'x', iSnapshot);
    memset(pJournal, 'x', iJournal);
    SESSION_CHECK(pRestored && xllmSessionBuildRequest(pRestored, &tGot, &tError) &&
                      round_trip_requests_equal(&tWant, &tGot),
                  "restored messages survive destruction of both input buffers");
done:
    xllm_session__test_alloc_reset();
    xllm_session__test_storage_reset();
    xllm_session__test_restore_cancel = NULL;
    xllm_session__test_restore_cancel_after = 0u;
    xrtCancelDestroy(pCancel);
    xrtFree(pSnapshot);
    xrtFree(pJournal);
    xllmSessionDestroy(pRestored);
    xllmSessionDestroy(pSource);
    xllmRequestUnit(&tWant);
    xllmRequestUnit(&tGot);
    (void)xrtFileDelete(sSnapshot);
    (void)xrtFileDelete(sJournal);
}

static bool test_part_string_equal(const char *pA, const char *pB)
{
    return (pA == NULL) == (pB == NULL) && (!pA || strcmp(pA, pB) == 0);
}

static bool test_parts_equal(const xllm_message *pA, const xllm_message *pB)
{
    size_t i;
    if (pA->iPartCount != pB->iPartCount || !test_part_string_equal(pA->sNative, pB->sNative) ||
        !test_part_string_equal(pA->sContent, pB->sContent))
        return false;
    for (i = 0u; i < pA->iPartCount; ++i)
    {
        const xllm_part *pLeft = &pA->pParts[i], *pRight = &pB->pParts[i];
        if (pLeft->eKind != pRight->eKind || pLeft->iDataSize != pRight->iDataSize ||
            !test_part_string_equal(pLeft->sText, pRight->sText) ||
            !test_part_string_equal(pLeft->sNativeType, pRight->sNativeType) ||
            !test_part_string_equal(pLeft->sMediaType, pRight->sMediaType) ||
            !test_part_string_equal(pLeft->sSourceUrl, pRight->sSourceUrl) ||
            !test_part_string_equal(pLeft->sDetail, pRight->sDetail) ||
            (pLeft->iDataSize && memcmp(pLeft->pData, pRight->pData, pLeft->iDataSize) != 0))
            return false;
    }
    return true;
}

/* Corrupt one field in a real writer's snapshot, then recompute its CRC so
 * the semantic boundary (rather than the outer checksum) is exercised. */
static char *test_parts_corrupt(const char *pInput, size_t iBytes, const char *sFrom,
                                const char *sTo)
{
    const char *pAt = strstr(pInput, sFrom);
    size_t iPrefix = iBytes - (sizeof(",\"checksum\":\"") - 1u + 10u);
    xllm_session_buf tBuffer = {0};
    if (!pAt || (size_t)(pAt - pInput) >= iPrefix ||
        !xllm_session__buf_append(&tBuffer, pInput, (size_t)(pAt - pInput)) ||
        !xllm_session__buf_cstr(&tBuffer, sTo) ||
        !xllm_session__buf_append(&tBuffer, pAt + strlen(sFrom),
                                  iPrefix - (size_t)(pAt - pInput) - strlen(sFrom)) ||
        !xllm_session__checksum_append(&tBuffer))
    {
        xllm_session__buf_unit(&tBuffer);
        return NULL;
    }
    return xllm_session__buf_detach(&tBuffer);
}

static void test_multimodal_persistence(void)
{
    static const char sSnapshot[] = "out/mdo-integration/fixtures/parts_snapshot.json",
                      sJournal[] = "out/mdo-integration/fixtures/parts_journal.jsonl";
    static uint8_t tBytes[] = {0u, 255u, 65u, 0u};
    static const char *const tFrom[] = {"\"version\":4", "\"kind\":0",  "\"data_bytes\":4",
                                        "AP9BAA==",      "\"parts\":[", "\"version\":4"};
    static const char *const tTo[] = {"\"version\":3", "\"kind\":9",  "\"data_bytes\":5",
                                      "AP9BAB==",      "\"extra\":[", "\"version\":5"};
    xllm_session *pSource = NULL, *pRestored = NULL;
    xllm_message tMessage;
    xllm_part tPart = {0};
    xllm_request tRequest;
    xllm_error tError;
    xllm_session_restore_options tOptions;
    char *pSnapshot = NULL, *pJournal = NULL;
    size_t iSnapshot = 0u, iJournal = 0u, i, iAllocations, iFaults = 0u;
    xcancel *pCancel = NULL;
    (void)xrtFileDelete(sSnapshot);
    (void)xrtFileDelete(sJournal);
    xllmMessageInit(&tMessage, XLLM_ROLE_USER);
    xllmRequestInit(&tRequest);
    tPart.eKind = XLLM_PART_TEXT;
    tPart.sText = "Question \xe4\xb8\xad\xe6\x96\x87";
    SESSION_CHECK(xllmMessageAddPart(&tMessage, &tPart), "parts fixture keeps UTF-8 prompt text");
    tPart.eKind = XLLM_PART_REASONING;
    tPart.sText = "opaque reasoning";
    SESSION_CHECK(xllmMessageAddPart(&tMessage, &tPart), "parts fixture keeps reasoning blocks");
    memset(&tPart, 0, sizeof(tPart));
    tPart.eKind = XLLM_PART_IMAGE;
    tPart.pData = tBytes;
    tPart.iDataSize = sizeof(tBytes);
    tPart.sMediaType = "image/png";
    tPart.sDetail = "high";
    SESSION_CHECK(xllmMessageAddPart(&tMessage, &tPart),
                  "parts fixture owns binary NUL and high bytes");
    tPart.pData = NULL;
    tPart.iDataSize = 0u;
    tPart.sSourceUrl = "https://example.invalid/image.png";
    SESSION_CHECK(xllmMessageAddPart(&tMessage, &tPart),
                  "parts fixture retains a URL without fetching it");
    tPart.sSourceUrl = NULL;
    tPart.sDetail = NULL;
    tPart.pData = tBytes;
    tPart.iDataSize = sizeof(tBytes);
    tPart.eKind = XLLM_PART_AUDIO;
    tPart.sMediaType = "audio/wav";
    SESSION_CHECK(xllmMessageAddPart(&tMessage, &tPart), "parts fixture owns audio bytes");
    tPart.eKind = XLLM_PART_FILE;
    tPart.sMediaType = "application/octet-stream";
    SESSION_CHECK(xllmMessageAddPart(&tMessage, &tPart), "parts fixture owns file bytes");
    memset(&tPart, 0, sizeof(tPart));
    tPart.eKind = XLLM_PART_NATIVE;
    tPart.sNativeType = "signed-reasoning";
    tPart.sText = "{\"signature\":\"opaque\"}";
    SESSION_CHECK(xllmMessageAddPart(&tMessage, &tPart) &&
                      xllmMessageSetNative(&tMessage, "{\"provider\":\"opaque-envelope\"}"),
                  "parts fixture retains native data");
    pSource = xllmSessionCreate(NULL, &tError);
    SESSION_CHECK(pSource && xllmSessionEnableJournal(pSource, sJournal, &tError) &&
                      xllmSessionBeginTurn(pSource) == 1u &&
                      xllmSessionAddMessage(pSource, 1u, &tMessage, 0u) &&
                      xllmSessionSave(pSource, sSnapshot, &tError) &&
                      xllmSessionAddText(pSource, 1u, XLLM_ROLE_ASSISTANT, "Answer", 0u),
                  "real writer saves parts and appends a tail");
    pSnapshot = (char *)xrtFileReadAll(sSnapshot, &iSnapshot);
    pJournal = (char *)xrtFileReadAll(sJournal, &iJournal);
    if (!pSnapshot || !pJournal)
    {
        SESSION_CHECK(false, "parts fixture loads");
        goto done;
    }
    pRestored = xllmSessionLoad(sSnapshot, &tError);
    SESSION_CHECK(pRestored && xllmSessionBuildRequest(pRestored, &tRequest, &tError) &&
                      tRequest.iMessageCount == 1u &&
                      test_parts_equal(&tMessage, &tRequest.pMessages[0]),
                  "file load preserves every part and native field");
    xllmRequestUnit(&tRequest);
    xllmSessionDestroy(pRestored);
    pRestored = xllmSessionRecover(sSnapshot, sJournal, NULL, &tError);
    SESSION_CHECK(pRestored && xllmSessionBuildRequest(pRestored, &tRequest, &tError) &&
                      tRequest.iMessageCount == 2u &&
                      test_parts_equal(&tMessage, &tRequest.pMessages[0]),
                  "file recovery preserves checkpoint parts and tail");
    xllmRequestUnit(&tRequest);
    xllmSessionDestroy(pRestored);
    pRestored = xllmSessionRestore(NULL, 0u, pJournal, iJournal, NULL, &tError);
    SESSION_CHECK(pRestored && xllmSessionBuildRequest(pRestored, &tRequest, &tError) &&
                      tRequest.iMessageCount == 2u &&
                      test_parts_equal(&tMessage, &tRequest.pMessages[0]),
                  "journal-only byte replay preserves every part");
    xllmRequestUnit(&tRequest);
    xllmSessionDestroy(pRestored);
    pRestored = NULL;
    for (i = 0u; i < sizeof(tFrom) / sizeof(tFrom[0]); ++i)
    {
        char *pBad = test_parts_corrupt(pSnapshot, iSnapshot, tFrom[i], tTo[i]);
        pRestored = pBad ? xllmSessionRestore(pBad, strlen(pBad), NULL, 0u, NULL, &tError) : NULL;
        SESSION_CHECK(pBad && !pRestored && tError.eCode == XLLM_ERROR_PARSE,
                      "parts corruption with valid CRC is rejected");
        free(pBad);
        xllmSessionDestroy(pRestored);
        pRestored = NULL;
    }
    pCancel = xrtCancelCreate();
    xllmSessionRestoreOptionsInit(&tOptions);
    tOptions.pCancel = pCancel;
    xllm_session__test_parts_cancel = pCancel;
    pRestored = xllmSessionRestore(pSnapshot, iSnapshot, NULL, 0u, &tOptions, &tError);
    SESSION_CHECK(pCancel && !pRestored && tError.eCode == XLLM_ERROR_CANCELLED,
                  "cancelling between parts destroys partial snapshot replay");
    xllmSessionDestroy(pRestored);
    pRestored = NULL;
    xrtCancelDestroy(pCancel);
    pCancel = xrtCancelCreate();
    tOptions.pCancel = pCancel;
    xllm_session__test_parts_cancel = pCancel;
    pRestored = xllmSessionRestore(NULL, 0u, pJournal, iJournal, &tOptions, &tError);
    SESSION_CHECK(pCancel && !pRestored && tError.eCode == XLLM_ERROR_CANCELLED,
                  "cancelling parts preserves journal cancellation error");
    xllmSessionDestroy(pRestored);
    pRestored = NULL;
    xrtCancelDestroy(pCancel);
    pCancel = NULL;
    xllm_session__test_parts_cancel = NULL;
    xllm_session__test_alloc_reset();
    pRestored = xllmSessionRestore(pSnapshot, iSnapshot, NULL, 0u, NULL, &tError);
    iAllocations = xllm_session__test_alloc_count();
    xllmSessionDestroy(pRestored);
    pRestored = NULL;
    for (i = 1u; i <= iAllocations; ++i)
    {
        xllm_session__test_alloc_fail(i);
        pRestored = xllmSessionRestore(pSnapshot, iSnapshot, NULL, 0u, NULL, &tError);
        if (!pRestored)
            ++iFaults;
        xllmSessionDestroy(pRestored);
        pRestored = NULL;
    }
    xllm_session__test_alloc_reset();
    SESSION_CHECK(iAllocations && iFaults == iAllocations,
                  "every session allocation failure aborts parts replay");
    pRestored = xllmSessionRestore(pSnapshot, iSnapshot, pJournal, iJournal, NULL, &tError);
    xllmSessionDestroy(pSource);
    pSource = NULL;
    xrtFree(pSnapshot);
    pSnapshot = NULL;
    xrtFree(pJournal);
    pJournal = NULL;
    SESSION_CHECK(pRestored && xllmSessionBuildRequest(pRestored, &tRequest, &tError) &&
                      tRequest.iMessageCount == 2u &&
                      test_parts_equal(&tMessage, &tRequest.pMessages[0]),
                  "restored parts survive all source ownership release");
done:
    xllm_session__test_parts_cancel = NULL;
    xllm_session__test_alloc_reset();
    xrtCancelDestroy(pCancel);
    xllmRequestUnit(&tRequest);
    xllmMessageUnit(&tMessage);
    xllmSessionDestroy(pSource);
    xllmSessionDestroy(pRestored);
    xrtFree(pSnapshot);
    xrtFree(pJournal);
    (void)xrtFileDelete(sSnapshot);
    (void)xrtFileDelete(sJournal);
}

static void test_signed_assistant_persistence(void)
{
    static const char sSnapshot[] = "out/mdo-integration/fixtures/signed_snapshot.json",
                      sJournal[] = "out/mdo-integration/fixtures/signed_journal.jsonl";
    char sReasoning[] = "original reasoning",
         sSignature[] = "opaque/one+=", sText[] = "Answer \"quoted\"\n";
    xllm_block tBlocks[3] = {0};
    xllm_tool_call tTool = {"signed-call", "read", "{\"path\":\"a.c\"}"};
    xllm_response tResponse = {0};
    xllm_message tExpected;
    xllm_session *pSource = NULL, *pRestored = NULL;
    xllm_client_config tClientConfig;
    xllm_client *pClient = NULL;
    xllm_request tRequest;
    xllm_error tError;
    char *pSnapshot = NULL, *pJournal = NULL;
    size_t iSnapshot = 0u, iJournal = 0u, i;
    bool bExpected;
    (void)xrtFileDelete(sSnapshot);
    (void)xrtFileDelete(sJournal);
    tBlocks[0].eKind = XLLM_BLOCK_REASONING;
    tBlocks[0].sText = sReasoning;
    tBlocks[0].sNative = sSignature;
    tBlocks[1].eKind = XLLM_BLOCK_TEXT;
    tBlocks[1].sText = sText;
    tBlocks[2].eKind = XLLM_BLOCK_REASONING;
    tBlocks[2].sText = "";
    tBlocks[2].sNative = "opaque/empty+=";
    tResponse.sContent = sText;
    tResponse.sReasoningContent = sReasoning;
    tResponse.pBlocks = tBlocks;
    tResponse.iBlockCount = 3u;
    tResponse.pToolCalls = &tTool;
    tResponse.iToolCallCount = 1u;
    tResponse.tUsage.uInputTokens = 19u;
    tResponse.tUsage.uOutputTokens = 7u;
    tResponse.tUsage.uTotalTokens = 26u;
    bExpected = xllmMessageFromResponse(&tResponse, &tExpected);
    xllmRequestInit(&tRequest);
    pSource = xllmSessionCreate(NULL, &tError);
    SESSION_CHECK(bExpected && pSource && xllmSessionEnableJournal(pSource, sJournal, &tError) &&
                      xllmSessionBeginTurn(pSource) == 1u &&
                      xllmSessionAddText(pSource, 1u, XLLM_ROLE_USER, "Question", 0u) &&
                      xllmSessionAddAssistantResponse(pSource, 1u, &tResponse) &&
                      xllmSessionAddToolResult(pSource, 1u, "signed-call", "Done"),
                  "session adds a real signed assistant response and tool pair");
    /* Source views are mutable and outlive neither the API call nor restart. */
    sReasoning[0] = sSignature[0] = sText[0] = 'X';
    SESSION_CHECK(pSource && xllmSessionSave(pSource, sSnapshot, &tError) &&
                      xllmSessionBeginTurn(pSource) == 2u &&
                      xllmSessionAddText(pSource, 2u, XLLM_ROLE_USER, "Next", 0u) &&
                      xllmSessionAddText(pSource, 2u, XLLM_ROLE_ASSISTANT, "Tail", 0u),
                  "signed assistant checkpoint accepts a normal journal tail");
    pSnapshot = (char *)xrtFileReadAll(sSnapshot, &iSnapshot);
    pJournal = (char *)xrtFileReadAll(sJournal, &iJournal);
    xllmSessionDestroy(pSource);
    pSource = NULL;
    xllmClientConfigInit(&tClientConfig);
    tClientConfig.eProvider = XLLM_PROVIDER_ANTHROPIC;
    tClientConfig.sBaseUrl = "http://127.0.0.1:1/v1";
    tClientConfig.sModel = "signed-test";
    pClient = xllmClientCreate(&tClientConfig, &tError);
    if (!pSnapshot || !pJournal || !pClient)
    {
        SESSION_CHECK(false, "signed assistant fixture is complete");
        goto done;
    }
    for (i = 0u; i < 4u; ++i)
    {
        char *sJson;
        if (i == 0u)
            pRestored = xllmSessionLoad(sSnapshot, &tError);
        else if (i == 1u)
            pRestored = xllmSessionRecover(sSnapshot, sJournal, NULL, &tError);
        else
            pRestored = xllmSessionRestore(i == 2u ? NULL : pSnapshot, i == 2u ? 0u : iSnapshot,
                                           pJournal, iJournal, NULL, &tError);
        SESSION_CHECK(pRestored && xllmSessionBuildRequest(pRestored, &tRequest, &tError) &&
                          tRequest.iMessageCount == (i == 0u ? 3u : 5u) &&
                          test_parts_equal(&tExpected, &tRequest.pMessages[1]),
                      "file/byte restore keeps signed response text, signatures and tools");
        sJson = pRestored ? xllmClientBuildRequestJson(pClient, &tRequest, &tError) : NULL;
        SESSION_CHECK(
            sJson && xrtJsonValid(xllm__sv(sJson)) &&
                strstr(sJson, "{\"type\":\"thinking\",\"thinking\":\"original "
                              "reasoning\",\"signature\":\"opaque/one+=\"}") &&
                strstr(
                    sJson,
                    "{\"type\":\"thinking\",\"thinking\":\"\",\"signature\":\"opaque/empty+=\"}") &&
                strstr(sJson, "{\"type\":\"text\",\"text\":\"Answer \\\"quoted\\\"\\n\"}") &&
                strstr(sJson, "\"tool_use\",\"id\":\"signed-call\""),
            "restored signed assistant builds an unchanged thinking/text request");
        xllmFree(sJson);
        xllmRequestUnit(&tRequest);
        xllmSessionDestroy(pRestored);
        pRestored = NULL;
    }
done:
    xllmMessageUnit(&tExpected);
    xllmRequestUnit(&tRequest);
    xllmClientDestroy(pClient);
    xllmSessionDestroy(pSource);
    xllmSessionDestroy(pRestored);
    xrtFree(pSnapshot);
    xrtFree(pJournal);
    (void)xrtFileDelete(sSnapshot);
    (void)xrtFileDelete(sJournal);
}

static void test_raw_entry_view(void)
{
    xllm_error tError;
    xllm_session *pSession = xllmSessionCreate(NULL, &tError);
    xllm_session_entry_view tEntry = {0};
    size_t iSmall[2] = {sizeof(size_t), 0x12345678u};
    SESSION_CHECK(xllmSessionEntryCount(NULL) == 0u && pSession &&
                      xllmSessionEntryCount(pSession) == 0u,
                  "empty raw ledger count");
    SESSION_CHECK(!xllmSessionEntryAt(pSession, 0u, (xllm_session_entry_view *)iSmall) &&
                      iSmall[0] == sizeof(size_t) && iSmall[1] == 0x12345678u,
                  "entry size mismatch preserves short output");
    if (!pSession)
    {
        return;
    }
    SESSION_CHECK(xllmSessionSetSystemPrompt(pSession, "Pinned identity", &tError) &&
                      xllmSessionBeginTurn(pSession) == 1u &&
                      xllmSessionAddText(pSession, 1u, XLLM_ROLE_USER, "Question", 0u) &&
                      xllmSessionAddText(pSession, 1u, XLLM_ROLE_ASSISTANT, "Answer", 0u),
                  "raw view fixture contains stable owned entries");
    tEntry.iSize = sizeof(tEntry);
    xllm_session__test_alloc_fail(1u);
    SESSION_CHECK(xllmSessionEntryAt(pSession, 0u, &tEntry) && tEntry.uSequence == 1u &&
                      tEntry.uFlags == XLLM_SESSION_ENTRY_PINNED &&
                      strcmp(tEntry.pMessage->sContent, "Pinned identity") == 0 &&
                      xllm_session__test_alloc_count() == 0u,
                  "raw view is allocation-free and retains pinned identity");
    xllm_session__test_alloc_reset();
    SESSION_CHECK(xllmSessionEntryAt(pSession, 1u, &tEntry) && tEntry.uTurn == 1u &&
                      tEntry.uSequence == 2u && tEntry.pMessage->eRole == XLLM_ROLE_USER &&
                      strcmp(tEntry.pMessage->sContent, "Question") == 0,
                  "raw view exposes original sequence and role");
    /* Render watermarks cannot erase retained evidence from inspection. */
    pSession->uCompactedThrough = 2u;
    SESSION_CHECK(xllmSessionEntryAt(pSession, 1u, &tEntry) &&
                      strcmp(tEntry.pMessage->sContent, "Question") == 0,
                  "raw view includes compacted entries");
    pSession->uCompactedThrough = 0u;
    SESSION_CHECK(!xllmSessionEntryAt(pSession, SIZE_MAX, &tEntry) &&
                      tEntry.iSize == sizeof(tEntry) && !tEntry.pMessage && !tEntry.uSequence &&
                      !tEntry.uTurn,
                  "invalid index clears borrowed output");
    SESSION_CHECK(xllmSessionBeginTurn(pSession) == 2u &&
                      xllmSessionAddText(pSession, 2u, XLLM_ROLE_USER, "Next question", 0u) &&
                      xllmSessionBeginModelCall(pSession, &tError) &&
                      !xllmSessionEntryAt(pSession, 0u, &tEntry) && !tEntry.pMessage &&
                      xllmSessionAbortModelCall(pSession, &tError),
                  "transient writer states cannot expose a raw view");
    pSession->bInHook = true;
    SESSION_CHECK(!xllmSessionEntryAt(pSession, 0u, &tEntry),
                  "hook re-entry cannot expose a raw view");
    pSession->bInHook = false;
    SESSION_CHECK(xllmSessionTruncateAfter(pSession, 2u, &tError) &&
                      xllmSessionEntryCount(pSession) == 2u &&
                      !xllmSessionEntryAt(pSession, 2u, &tEntry) &&
                      xllmSessionClear(pSession, &tError) && xllmSessionEntryCount(pSession) == 0u,
                  "rewind and clear remove raw entries instead of reviving them");
    xllmSessionDestroy(pSession);
    SESSION_CHECK(!xllmSessionEntryAt(NULL, 0u, &tEntry) && !tEntry.pMessage &&
                      !xllmSessionEntryAt(NULL, 0u, NULL),
                  "null raw inspection inputs fail safely");
}

static void test_context_refresh_tail(void)
{
    xllm_session_config tConfig;
    xllm_session *pSession;
    xllm_error tError;
    xllm_session_tail tTail;
    xllm_response tResponse;
    xllm_tool_call tCall;
    uint64_t uTurn;
    xllmSessionConfigInit(&tConfig);
    pSession = xllmSessionCreate(&tConfig, &tError);
    SESSION_CHECK(pSession != NULL, "context-tail session creates");
    if (!pSession)
        return;
    SESSION_CHECK(xllmSessionGetTail(pSession, &tTail) && !tTail.bHasMessage,
                  "empty session has no conversation tail");
    SESSION_CHECK(xllmSessionSetSystemPrompt(pSession, "initial context", &tError) &&
                      xllmSessionGetTail(pSession, &tTail) && !tTail.bHasMessage,
                  "system-only session has no conversation tail");
    uTurn = xllmSessionBeginTurn(pSession);
    SESSION_CHECK(
        uTurn != 0u &&
            xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, "resume this request", 0u) &&
            xllmSessionSetSystemPrompt(pSession, "updated language", &tError) &&
            xllmSessionGetTail(pSession, &tTail) && tTail.bHasMessage &&
            tTail.eRole == XLLM_ROLE_USER && tTail.uTurn == uTurn,
        "context refresh preserves interrupted user tail");
    SESSION_CHECK(xllmSessionBeginModelCall(pSession, &tError) &&
                      !xllmSessionSetSystemPrompt(pSession, "illegal in-flight update", &tError) &&
                      xllmSessionAbortModelCall(pSession, &tError),
                  "in-flight model still rejects context mutation");
    memset(&tCall, 0, sizeof(tCall));
    tCall.sId = "context-tail-call";
    tCall.sName = "read";
    tCall.sArgumentsJson = "{}";
    memset(&tResponse, 0, sizeof(tResponse));
    tResponse.pToolCalls = &tCall;
    tResponse.iToolCallCount = 1u;
    SESSION_CHECK(xllmSessionBeginModelCall(pSession, &tError) &&
                      xllmSessionAddAssistantResponse(pSession, uTurn, &tResponse) &&
                      xllmSessionSetSystemPrompt(pSession, "pending-call context", &tError) &&
                      xllmSessionPendingToolCallCount(pSession) == 1u &&
                      xllmSessionGetState(pSession) == XLLM_SESSION_STATE_TOOLS_PENDING,
                  "context refresh preserves pending tool identity");
    SESSION_CHECK(xllmSessionAddToolResult(pSession, uTurn, tCall.sId, "result") &&
                      xllmSessionSetSystemPrompt(pSession, "after-tool context", &tError) &&
                      xllmSessionGetTail(pSession, &tTail) && tTail.bHasMessage &&
                      tTail.eRole == XLLM_ROLE_TOOL && tTail.uTurn == uTurn,
                  "context refresh preserves tool tail for model continuation");
    SESSION_CHECK(xllmSessionAddText(pSession, uTurn, XLLM_ROLE_ASSISTANT, "complete", 0u) &&
                      xllmSessionSetSystemPrompt(pSession, "idle context", &tError) &&
                      xllmSessionGetTail(pSession, &tTail) && tTail.bHasMessage &&
                      tTail.eRole == XLLM_ROLE_ASSISTANT && tTail.uTurn == uTurn,
                  "context refresh leaves completed response complete");
    xllmSessionDestroy(pSession);
}

static bool tool_image_result_equal(const xllm_request *pRequest)
{
    static const unsigned char tBytes[] = {0u, 255u, 65u, 0u};
    const xllm_message *pMessage;
    if (pRequest->iMessageCount != 3u)
        return false;
    pMessage = &pRequest->pMessages[2];
    return pMessage->eRole == XLLM_ROLE_TOOL && pMessage->sToolCallId &&
           strcmp(pMessage->sToolCallId, "image-call") == 0 && pMessage->sContent == NULL &&
           pMessage->iPartCount == 2u && pMessage->pParts[0].eKind == XLLM_PART_TEXT &&
           strcmp(pMessage->pParts[0].sText, "read: source/image.png") == 0 &&
           pMessage->pParts[1].eKind == XLLM_PART_IMAGE &&
           pMessage->pParts[1].iDataSize == sizeof(tBytes) &&
           memcmp(pMessage->pParts[1].pData, tBytes, sizeof(tBytes)) == 0;
}

static void test_tool_image_text(void)
{
    static const unsigned char tBytes[] = {0u, 255u, 65u, 0u};
    static const char sPath[] = "out/mdo-integration/fixtures/tool_image_text.json";
    xllm_error tError;
    xllm_session *pSession, *pLoaded = NULL;
    xllm_request tRequest;
    xllm_response tResponse = {0};
    xllm_tool_call tCall = {0};
    uint64_t uTurn;
    xllmRequestInit(&tRequest);
    pSession = xllmSessionCreate(NULL, &tError);
    SESSION_CHECK(pSession != NULL, "tool image session creates");
    if (!pSession)
        return;
    uTurn = xllmSessionBeginTurn(pSession);
    tCall.sId = "image-call";
    tCall.sName = "read";
    tCall.sArgumentsJson = "{\"path\":\"source/image.png\"}";
    tResponse.pToolCalls = &tCall;
    tResponse.iToolCallCount = 1u;
    SESSION_CHECK(xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, "inspect image", 0u) &&
                      xllmSessionBeginModelCall(pSession, &tError) &&
                      xllmSessionAddAssistantResponse(pSession, uTurn, &tResponse) &&
                      xllmSessionAddToolResultWithImage(pSession, uTurn, tCall.sId,
                                                        "read: source/image.png", tBytes,
                                                        sizeof(tBytes), "image/png") &&
                      xllmSessionBuildRequest(pSession, &tRequest, &tError) &&
                      tool_image_result_equal(&tRequest),
                  "tool image retains text and binary");
    xllmRequestUnit(&tRequest);
    SESSION_CHECK(xllmSessionSave(pSession, sPath, &tError) &&
                      (pLoaded = xllmSessionLoad(sPath, &tError)) != NULL &&
                      xllmSessionBuildRequest(pLoaded, &tRequest, &tError) &&
                      tool_image_result_equal(&tRequest),
                  "tool image text survives restart");
    SESSION_CHECK(xllmSessionPendingToolCallCount(pSession) == 0u &&
                      !xllmSessionAddToolResultWithImage(pSession, uTurn, tCall.sId, "duplicate",
                                                         tBytes, sizeof(tBytes), "image/png"),
                  "tool image keeps result pairing and rejects replay");
    xllmRequestUnit(&tRequest);
    xllmSessionDestroy(pLoaded);
    xllmSessionDestroy(pSession);
    (void)xrtFileDelete(sPath);
}

int main(int argc, char **argv)
{
    bool bStateOnly = argc == 2 && strcmp(argv[1], "--state") == 0;
    bool bBudgetOnly = argc == 2 && strcmp(argv[1], "--budget") == 0;
    bool bPairsOnly = argc == 2 && strcmp(argv[1], "--pairs") == 0;
    bool bCompactionOnly = argc == 2 && strcmp(argv[1], "--compaction") == 0;
    bool bPersistenceOnly = argc == 2 && strcmp(argv[1], "--persistence") == 0;
    bool bSnapshotOnly = argc == 2 && strcmp(argv[1], "--snapshot") == 0;
    bool bBranchOnly = argc == 2 && strcmp(argv[1], "--branch") == 0;
    bool bFaultsOnly = argc == 2 && strcmp(argv[1], "--faults") == 0;
    bool bFull = argc == 2 && strcmp(argv[1], "--full") == 0;
    bool bBounded = argc == 1 || (argc == 2 && strcmp(argv[1], "--bounded") == 0);
    if (argc == 2 && strcmp(argv[1], "--tool-image") == 0)
    {
        test_tool_image_text();
        return g_iSessionFailures ? 1 : 0;
    }
    if (argc == 2 && strcmp(argv[1], "--context-tail") == 0)
    {
        test_context_refresh_tail();
        return g_iSessionFailures ? 1 : 0;
    }
    if (!bStateOnly && !bBudgetOnly && !bPairsOnly && !bCompactionOnly && !bPersistenceOnly &&
        !bSnapshotOnly && !bBranchOnly && !bFaultsOnly && !bFull && !bBounded)
    {
        fprintf(stderr,
                "usage: %s "
                "[--bounded|--context-tail|--state|--budget|--pairs|--compaction|--persistence|--"
                "snapshot|--branch|--faults|--full]\n",
                argv[0]);
        return 2;
    }
    printf("xllm-session v3 tests\n");
    if (!bBudgetOnly && !bPairsOnly && !bCompactionOnly && !bPersistenceOnly && !bSnapshotOnly &&
        !bBranchOnly && !bFaultsOnly)
    {
        test_session_state_machine();
    }
    if (bStateOnly)
    {
        printf("xllm-session state: %s (%d failures)\n", g_iSessionFailures ? "FAIL" : "PASS",
               g_iSessionFailures);
        return g_iSessionFailures ? 1 : 0;
    }
    if (!bPairsOnly && !bCompactionOnly && !bPersistenceOnly && !bSnapshotOnly && !bBranchOnly &&
        !bFaultsOnly)
    {
        test_budget_and_profile();
        test_apply_model_profile();
    }
    if (bBudgetOnly)
    {
        printf("xllm-session budget: %s (%d failures)\n", g_iSessionFailures ? "FAIL" : "PASS",
               g_iSessionFailures);
        return g_iSessionFailures ? 1 : 0;
    }
    if (!bCompactionOnly && !bPersistenceOnly && !bSnapshotOnly && !bBranchOnly && !bFaultsOnly)
    {
        test_tool_pair_invariants();
    }
    if (bPairsOnly)
    {
        printf("xllm-session pairs: %s (%d failures)\n", g_iSessionFailures ? "FAIL" : "PASS",
               g_iSessionFailures);
        return g_iSessionFailures ? 1 : 0;
    }
    if (!bPersistenceOnly && !bSnapshotOnly && !bBranchOnly && !bFaultsOnly)
    {
        test_compaction_transaction_recovery();
    }
    if (bCompactionOnly)
    {
        printf("xllm-session compaction: %s (%d failures)\n", g_iSessionFailures ? "FAIL" : "PASS",
               g_iSessionFailures);
        return g_iSessionFailures ? 1 : 0;
    }
    if (!bPersistenceOnly && !bSnapshotOnly && !bBranchOnly && !bFaultsOnly)
    {
        test_default_profile();
        test_compaction_user_bridge();
        test_add_reference();
        test_budget_compaction_persistence();
    }
    if (!bSnapshotOnly && !bBranchOnly && !bFaultsOnly)
    {
        test_persistence_v3_migration();
    }
    if (bPersistenceOnly)
    {
        printf("xllm-session persistence: %s (%d failures)\n", g_iSessionFailures ? "FAIL" : "PASS",
               g_iSessionFailures);
        return g_iSessionFailures ? 1 : 0;
    }
    if (!bBranchOnly && !bFaultsOnly)
    {
        test_immutable_snapshot();
    }
    if (bSnapshotOnly)
    {
        printf("xllm-session snapshot: %s (%d failures)\n", g_iSessionFailures ? "FAIL" : "PASS",
               g_iSessionFailures);
        return g_iSessionFailures ? 1 : 0;
    }
    test_branch_rewrite_contract();
    if (bBranchOnly)
    {
        printf("xllm-session branch: %s (%d failures)\n", g_iSessionFailures ? "FAIL" : "PASS",
               g_iSessionFailures);
        return g_iSessionFailures ? 1 : 0;
    }
    test_persistence_fault_atomicity();
    if (bFaultsOnly)
    {
        printf("xllm-session faults: %s (%d failures)\n", g_iSessionFailures ? "FAIL" : "PASS",
               g_iSessionFailures);
        return g_iSessionFailures ? 1 : 0;
    }
    test_journal_checkpoint_recovery();
    test_split_turn();
    test_exact_governance();
    test_ops_and_hooks();
    test_ladder_and_guard();
    test_easy_send();
    test_run_with_tools();
    test_bind_identity_and_cancel();
    test_journal_roundtrip_property();
    if (bFull)
    {
        test_borrowed_view_render();
    }
    test_wire_prefix_cache();
    test_file_ledger();
    test_memory_restore();
    test_raw_entry_view();
    test_context_refresh_tail();
    test_multimodal_persistence();
    test_tool_image_text();
    test_signed_assistant_persistence();
    printf("xllm-session v3: %s (%d failures)\n", g_iSessionFailures ? "FAIL" : "PASS",
           g_iSessionFailures);
    return g_iSessionFailures ? 1 : 0;
}
