#include "../xwork.c"
#include "xllm.c"
#include "xllm-session.c"

static int g_iFailures = 0;
static const char* g_sSelfPath = NULL;

#define CHECK(expr, name) do { \
    bool xwork_ok__ = !!(expr); \
    printf("  %-62s %s\n", (name), xwork_ok__ ? "PASS" : "FAIL"); \
    if ( !xwork_ok__ ) ++g_iFailures; \
} while (0)

typedef struct mock_model {
    uint32_t uCompactionCalls;
    uint32_t uAgentCalls;
    bool bSawTools;
    bool bSawParallel;
    bool bSawToolResults;
    bool bSawCompactionSummary;
    bool bSawVerificationGate;
    bool bSawContext;
    uint32_t uPermissionCalls;
    bool bSawPathPermission;
    bool bSawCommandPermission;
    bool bSawHighRiskPermission;
    uint32_t uBeforeToolHooks;
    uint32_t uAfterToolHooks;
    xwork_agent* pAgent;
    bool bRegistryMutationBlocked;
} mock_model;

typedef struct test_events {
    uint32_t uTextDeltas;
    uint32_t uToolStarts;
    uint32_t uToolDone;
    uint32_t uToolSuccess;
    uint32_t uToolFailure;
    uint32_t uCompactions;
    uint32_t uRejectedCompactions;
    uint32_t uErrors;
    uint32_t uModelStarts;
    uint32_t uModelDone;
    bool bRequestMetadata;
    bool bResponseMetadata;
    bool bCompactionQualityMetadata;
    char sLastArtifact[512];
} test_events;

typedef struct subagent_mock {
    uint32_t uModelCalls;
    uint32_t uAgentStarts;
    uint32_t uAgentDone;
    uint32_t uToolStarts;
    uint32_t uToolDone;
    bool bReadOnlyToolSet;
    bool bSawReadableFile;
    bool bSawInternalDenial;
    bool bScopedEvents;
} subagent_mock;

static char* test_strdup(const char* sText)
{
    size_t iLen = strlen(sText);
    char* sCopy = (char*)malloc(iLen + 1u);
    if ( sCopy ) memcpy(sCopy, sText, iLen + 1u);
    return sCopy;
}

static xwork_result registry_probe_execute(
    void* pUserData,
    const xwork_tool_context* pContext,
    const char* sArgumentsJson,
    xwork_tool_output* pOutput,
    xwork_error* pError
)
{
    (void)pUserData;
    (void)pContext;
    (void)sArgumentsJson;
    (void)pError;
    return xworkToolOutputSet(pOutput, true, "dynamic registry probe")
        ? XWORK_RESULT_OK : XWORK_RESULT_ERROR;
}

static xllm_response* mock_response(const char* sContent, size_t iToolCount)
{
    xllm_response* pResponse = (xllm_response*)calloc(1u, sizeof(*pResponse));
    if ( !pResponse ) return NULL;
    pResponse->sContent = test_strdup(sContent ? sContent : "");
    pResponse->sModel = test_strdup("mock-model");
    pResponse->sRequestId = test_strdup("mock-request-id");
    pResponse->sFinishReason = test_strdup(iToolCount ? "tool_calls" : "stop");
    if ( iToolCount ) pResponse->pToolCalls = (xllm_tool_call*)calloc(iToolCount, sizeof(*pResponse->pToolCalls));
    if ( !pResponse->sContent || !pResponse->sModel || !pResponse->sRequestId ||
         !pResponse->sFinishReason || (iToolCount && !pResponse->pToolCalls) ) {
        xllmResponseDestroy(pResponse);
        return NULL;
    }
    pResponse->iToolCallCount = iToolCount;
    pResponse->iToolCallCap = iToolCount;
    pResponse->tUsage.uInputTokens = 100u;
    pResponse->tUsage.uOutputTokens = 20u;
    pResponse->tUsage.uTotalTokens = 120u;
    pResponse->uHttpStatus = 200u;
    pResponse->tDiagnostics.uAttemptCount = 1u;
    pResponse->tDiagnostics.uMaxAttempts = 3u;
    pResponse->tDiagnostics.uTotalDurationMs = 7u;
    pResponse->tDiagnostics.uResponseBodyBytes = 42u;
    return pResponse;
}

static bool mock_set_call(xllm_response* pResponse, size_t i, const char* sId, const char* sName, const char* sArgs)
{
    pResponse->pToolCalls[i].sId = test_strdup(sId);
    pResponse->pToolCalls[i].sName = test_strdup(sName);
    pResponse->pToolCalls[i].sArgumentsJson = test_strdup(sArgs);
    return pResponse->pToolCalls[i].sId && pResponse->pToolCalls[i].sName && pResponse->pToolCalls[i].sArgumentsJson;
}

static bool request_has_role(const xllm_request* pRequest, xllm_role eRole, size_t iAtLeast)
{
    size_t i;
    size_t iCount = 0u;
    for ( i = 0u; i < pRequest->iMessageCount; ++i ) {
        if ( pRequest->pMessages[i].eRole == eRole ) ++iCount;
    }
    return iCount >= iAtLeast;
}

static bool request_has_text(const xllm_request* pRequest, const char* sNeedle)
{
    size_t i;
    for ( i = 0u; i < pRequest->iMessageCount; ++i ) {
        const char* sText = pRequest->pMessages[i].sContent;
        if ( sText && strstr(sText, sNeedle) ) return true;
    }
    return false;
}

static xllm_result subagent_complete(
    void* pUserData,
    const xllm_request* pRequest,
    const xllm_stream_callbacks* pCallbacks,
    xllm_response** ppResponse,
    xllm_error* pError
)
{
    static const char sLongFinal[] =
        "Evidence report: the readable workspace file was inspected and the internal control directory was denied as required. "
        "The child received only the read tool, performed no writes or process execution, and cannot delegate recursively. "
        "Recommended next action: let the parent agent use this bounded evidence while retaining authority for every mutation. "
        "Additional padding verifies that the host byte budget truncates this final response deterministically without creating an artifact.";
    subagent_mock* pMock = (subagent_mock*)pUserData;
    xllm_response* pResponse;
    size_t i;
    bool bNames = pRequest && pRequest->iToolCount == 1u;
    (void)pCallbacks;
    (void)pError;
    *ppResponse = NULL;
    if ( !pRequest || !pMock ) return XLLM_RESULT_ERROR;
    for ( i = 0u; bNames && i < pRequest->iToolCount; ++i ) {
        const char* sName = pRequest->pTools[i].sName;
        if ( strcmp(sName, "read") != 0 ) bNames = false;
    }
    pMock->bReadOnlyToolSet = pMock->bReadOnlyToolSet || bNames;
    ++pMock->uModelCalls;
    if ( pMock->uModelCalls == 1u ) {
        pResponse = mock_response("", 2u);
        if ( !pResponse ||
             !mock_set_call(pResponse, 0u, "sub_read", "read",
                "{\"path\":\"evidence.txt\",\"max_lines\":100}") ||
             !mock_set_call(pResponse, 1u, "sub_internal", "read",
                "{\"path\":\".xcode/secrets.local.json\",\"max_lines\":20}") ) {
            xllmResponseDestroy(pResponse);
            return XLLM_RESULT_ERROR;
        }
    } else {
        pMock->bSawReadableFile = request_has_text(pRequest, "readonly-evidence-marker") &&
            request_has_text(pRequest, "artifact writes disabled");
        pMock->bSawInternalDenial = request_has_text(pRequest, "denied by approval policy");
        pResponse = mock_response(sLongFinal, 0u);
    }
    *ppResponse = pResponse;
    return pResponse ? XLLM_RESULT_OK : XLLM_RESULT_ERROR;
}

static bool subagent_event(void* pUserData, const xwork_event* pEvent)
{
    subagent_mock* pMock = (subagent_mock*)pUserData;
    if ( !pMock || !pEvent ) return false;
    if ( pEvent->uAgentDepth != 1u || pEvent->uDelegationId != 1u ||
         pEvent->uParentAgentTurn != 42u ) pMock->bScopedEvents = false;
    if ( pEvent->eKind == XWORK_EVENT_AGENT_START ) ++pMock->uAgentStarts;
    if ( pEvent->eKind == XWORK_EVENT_AGENT_DONE ) ++pMock->uAgentDone;
    if ( pEvent->eKind == XWORK_EVENT_TOOL_START ) ++pMock->uToolStarts;
    if ( pEvent->eKind == XWORK_EVENT_TOOL_DONE ) ++pMock->uToolDone;
    return true;
}

static xllm_result mock_complete(
    void* pUserData,
    const xllm_request* pRequest,
    const xllm_stream_callbacks* pCallbacks,
    xllm_response** ppResponse,
    xllm_error* pError
)
{
    mock_model* pMock = (mock_model*)pUserData;
    xllm_response* pResponse = NULL;
    char* sLargeArgs = NULL;
    size_t i;
    (void)pError;
    *ppResponse = NULL;
    if ( pRequest->pCancel || pRequest->uDeadline != XRT_DEADLINE_NEVER )
        pMock->bSawContext = true;
    if ( pRequest->iToolCount == 0u ) {
        ++pMock->uCompactionCalls;
        if ( pMock->uCompactionCalls == 1u ) {
            pResponse = mock_response("Objective: incomplete first compaction candidate.", 0u);
        } else {
            pResponse = mock_response(
                "Objective: test the xwork tool loop after compaction and finish the requested file workflow.\n"
                "Constraints: remain inside the workspace, preserve tool-call pairing, and verify every write.\n"
                "Architecture and decisions: use a durable session checkpoint while retaining the newest complete turn verbatim.\n"
                "Completed work: old synthetic rounds were inspected and their successful outcomes were retained.\n"
                "Current repository state: sandbox paths are active and the requested note workflow remains ready to execute.\n"
                "Verification evidence: the compacted prefix contains only completed turns with no unresolved tool calls.\n"
                "Open issues and risks: the requested edits and command verification are not complete yet.\n"
                "Exact next actions: execute the requested file tools, inspect results, run verification, and then report completion.",
                0u
            );
        }
    } else {
        if ( !pMock->bRegistryMutationBlocked && pMock->pAgent ) {
            xwork_error tRegistryError;
            xworkErrorInit(&tRegistryError);
            pMock->bRegistryMutationBlocked =
                !xworkAgentUnregisterTool(pMock->pAgent, "read", &tRegistryError) &&
                tRegistryError.eCode == XWORK_ERROR_CONTEXT;
        }
        ++pMock->uAgentCalls;
        pMock->bSawTools = pRequest->iToolCount == 9u;
        pMock->bSawParallel = pRequest->bParallelToolCalls;
        if ( pMock->uAgentCalls > 1u && request_has_role(pRequest, XLLM_ROLE_TOOL, 1u) ) pMock->bSawToolResults = true;
        if ( request_has_text(pRequest, "Objective: test the xwork tool loop after compaction") ) pMock->bSawCompactionSummary = true;
        if ( request_has_text(pRequest, "Completion verification gate") ) pMock->bSawVerificationGate = true;
        if ( pMock->uAgentCalls == 1u ) {
            xwork_buf tArgs = {0};
            pResponse = mock_response("", 6u);
            if ( !pResponse ) return XLLM_RESULT_ERROR;
            if ( !xwork__buf_append_cstr(&tArgs, "{\"path\":\"sandbox/note.txt\",\"content\":\"hello-") ) goto oom;
            for ( i = 0u; i < 1600u; ++i ) if ( !xwork__buf_append_char(&tArgs, (char)('a' + (i % 26u))) ) goto oom;
            if ( !xwork__buf_append_cstr(&tArgs, "note tail\",\"mode\":\"create\"}") ) goto oom;
            sLargeArgs = xwork__buf_detach(&tArgs);
            if ( !sLargeArgs ||
                 !mock_set_call(pResponse, 0u, "call_write", "write", sLargeArgs) ||
                 !mock_set_call(pResponse, 1u, "call_read", "read", "{\"path\":\"sandbox/note.txt\",\"max_lines\":20}") ||
                 !mock_set_call(pResponse, 2u, "call_escape", "read", "{\"path\":\"../outside.txt\"}") ||
                 !mock_set_call(pResponse, 3u, "call_list", "read", "{\"path\":\"sandbox/note.txt\",\"max_lines\":5}") ||
                 !mock_set_call(pResponse, 4u, "call_search", "read", "{\"path\":\"sandbox/note.txt\",\"start_line\":2,\"max_lines\":3}") ||
                 !mock_set_call(pResponse, 5u, "call_replace", "edit", "{\"path\":\"sandbox/note.txt\",\"edits\":[{\"old_text\":\"hello-\",\"new_text\":\"HELLO-\"}]}") ) goto oom;
        } else if ( pMock->uAgentCalls == 2u ) {
            pResponse = mock_response("", 1u);
            if ( !pResponse || !mock_set_call(pResponse, 0u, "call_patch", "edit",
                    "{\"path\":\"sandbox/note.txt\",\"edits\":[{\"old_text\":\"HELLO-\",\"new_text\":\"PATCHED-\"},{\"old_text\":\"note tail\",\"new_text\":\"EDITED tail\"}]}") ) goto oom;
        } else if ( pMock->uAgentCalls == 3u ) {
            pResponse = mock_response("The requested edits are complete.", 0u);
        } else if ( pMock->uAgentCalls == 4u ) {
            #if defined(_WIN32) || defined(_WIN64)
                static const char sVerifyArgs[] = "{\"argv\":[\"cmd\",\"/c\",\"type\",\"sandbox\\\\note.txt\"],\"timeout_ms\":10000}";
            #else
                static const char sVerifyArgs[] = "{\"argv\":[\"cat\",\"sandbox/note.txt\"],\"timeout_ms\":10000}";
            #endif
            pResponse = mock_response("", 1u);
            if ( !pResponse || !mock_set_call(pResponse, 0u, "call_exec", "exec",
                    sVerifyArgs) ) goto oom;
        } else {
            pResponse = mock_response("Implemented, inspected, edited, and verified the workspace file successfully.", 0u);
        }
    }
    free(sLargeArgs);
    if ( !pResponse ) return XLLM_RESULT_ERROR;
    if ( pCallbacks && pCallbacks->OnEvent && pResponse->sContent && pResponse->sContent[0] ) {
        xllm_event tEvent;
        memset(&tEvent, 0, sizeof(tEvent));
        tEvent.eKind = XLLM_EVENT_TEXT_DELTA;
        tEvent.as.tText.sData = pResponse->sContent;
        tEvent.as.tText.iLen = strlen(pResponse->sContent);
        if ( !pCallbacks->OnEvent(pCallbacks->pUserData, &tEvent) ) {
            xllmResponseDestroy(pResponse);
            return XLLM_RESULT_CANCELLED;
        }
    }
    *ppResponse = pResponse;
    return XLLM_RESULT_OK;
oom:
    free(sLargeArgs);
    xllmResponseDestroy(pResponse);
    return XLLM_RESULT_ERROR;
}

static bool on_event(void* pUserData, const xwork_event* pEvent)
{
    test_events* pEvents = (test_events*)pUserData;
    switch ( pEvent->eKind ) {
        case XWORK_EVENT_MODEL_START:
            ++pEvents->uModelStarts;
            if ( pEvent->sModel && strcmp(pEvent->sModel, "mock-model") == 0 &&
                 pEvent->sRequestFingerprint && strlen(pEvent->sRequestFingerprint) == 16u &&
                 pEvent->iMessageCount > 0u && pEvent->iToolDefinitionCount == 9u &&
                 pEvent->uMaxOutputTokens > 0u ) pEvents->bRequestMetadata = true;
            break;
        case XWORK_EVENT_MODEL_TEXT_DELTA: ++pEvents->uTextDeltas; break;
        case XWORK_EVENT_MODEL_DONE:
            ++pEvents->uModelDone;
            if ( pEvent->sModel && strcmp(pEvent->sModel, "mock-model") == 0 &&
                 pEvent->sProviderRequestId && strcmp(pEvent->sProviderRequestId, "mock-request-id") == 0 &&
                 pEvent->sFinishReason && pEvent->uHttpStatus == 200u &&
                 pEvent->tDiagnostics.uAttemptCount == 1u && pEvent->tDiagnostics.uTotalDurationMs == 7u ) {
                pEvents->bResponseMetadata = true;
            }
            break;
        case XWORK_EVENT_TOOL_START: ++pEvents->uToolStarts; break;
        case XWORK_EVENT_TOOL_DONE:
            ++pEvents->uToolDone;
            if ( pEvent->bSuccess ) ++pEvents->uToolSuccess;
            else ++pEvents->uToolFailure;
            if ( pEvent->sArtifactPath ) snprintf(pEvents->sLastArtifact, sizeof(pEvents->sLastArtifact), "%s", pEvent->sArtifactPath);
            break;
        case XWORK_EVENT_COMPACTION_REJECTED:
            ++pEvents->uRejectedCompactions;
            if ( pEvent->uCompactionAttempt == 1u &&
                 pEvent->tCompactionQuality.uMissingSections != 0u &&
                 !pEvent->tCompactionQuality.bAccepted ) pEvents->bCompactionQualityMetadata = true;
            break;
        case XWORK_EVENT_COMPACTION_DONE:
            ++pEvents->uCompactions;
            break;
        case XWORK_EVENT_ERROR: ++pEvents->uErrors; break;
        default: break;
    }
    return true;
}

static xwork_permission_decision on_permission(void* pUserData, const xwork_permission_request* pRequest)
{
    mock_model* pMock = (mock_model*)pUserData;
    ++pMock->uPermissionCalls;
    if ( pRequest->eResourceKind == XWORK_RESOURCE_PATH && pRequest->sResource && strstr(pRequest->sResource, "sandbox") ) {
        pMock->bSawPathPermission = true;
    }
    if ( pRequest->eResourceKind == XWORK_RESOURCE_COMMAND && pRequest->sResource && strstr(pRequest->sResource, "sandbox") ) {
        pMock->bSawCommandPermission = true;
    }
    if ( pRequest->eRisk == XWORK_RISK_HIGH ) pMock->bSawHighRiskPermission = true;
    return XWORK_PERMISSION_DEFAULT;
}

static xwork_hook_action on_hook(void* pUserData, const xwork_hook_event* pEvent)
{
    mock_model* pMock = (mock_model*)pUserData;
    if ( pEvent->ePhase == XWORK_HOOK_BEFORE_TOOL ) ++pMock->uBeforeToolHooks;
    else if ( pEvent->ePhase == XWORK_HOOK_AFTER_TOOL ) {
        ++pMock->uAfterToolHooks;
        if ( !pEvent->sOutput ) return XWORK_HOOK_CANCEL;
    }
    return XWORK_HOOK_CONTINUE;
}

static char* make_text(size_t iSize, char ch)
{
    char* sText = (char*)malloc(iSize + 1u);
    if ( !sText ) return NULL;
    memset(sText, ch, iSize);
    sText[iSize] = '\0';
    return sText;
}

static void test_process_text_normalization(void)
{
    static const unsigned char sOverlong[] = {0xC0u, 0xAFu};
    static const unsigned char sSurrogate[] = {0xEDu, 0xA0u, 0x80u};
    static const unsigned char sTooHigh[] = {0xF4u, 0x90u, 0x80u, 0x80u};
    static const unsigned char sTruncated[] = {0xE2u, 0x82u};
    static const unsigned char sLocalBytes[] = {
        0xCFu, 0xB5u, 0xCDu, 0xB3u, 0xD5u, 0xD2u, 0xB2u, 0xBBu,
        0xB5u, 0xBDu, 0xD6u, 0xB8u, 0xB6u, 0xA8u, 0xB5u, 0xC4u
    };
    static const unsigned char sBinary[] = {'a', 0u, 0xFFu, 'b'};
    xwork_buf tText = {0};

    CHECK(xrtUtf8Valid((xstrview){ "valid UTF-8: \xE4\xB8\xAD\xE6\x96\x87", 19u }, NULL), "strict UTF-8 accepts valid Unicode text");
    CHECK(!xrtUtf8Valid((xstrview){ (const char*)sOverlong, sizeof(sOverlong) }, NULL), "strict UTF-8 rejects overlong encodings");
    CHECK(!xrtUtf8Valid((xstrview){ (const char*)sSurrogate, sizeof(sSurrogate) }, NULL), "strict UTF-8 rejects surrogate code points");
    CHECK(!xrtUtf8Valid((xstrview){ (const char*)sTooHigh, sizeof(sTooHigh) }, NULL), "strict UTF-8 rejects code points above U+10FFFF");
    CHECK(!xrtUtf8Valid((xstrview){ (const char*)sTruncated, sizeof(sTruncated) }, NULL), "strict UTF-8 rejects truncated sequences");

    CHECK(xwork__buf_append_process_text(&tText, sLocalBytes, sizeof(sLocalBytes)), "local process bytes normalize without loss of control flow");
    CHECK(tText.pData && xrtUtf8Valid((xstrview){ tText.pData, tText.iLen }, NULL), "normalized process output is valid UTF-8");
    xwork__buf_unit(&tText);

    CHECK(xwork__buf_append_process_text(&tText, sBinary, sizeof(sBinary)), "binary-like process output is safely represented");
    CHECK(tText.pData && strstr(tText.pData, "\\x00") && strstr(tText.pData, "\\xFF"), "binary bytes are escaped instead of entering JSON");
    CHECK(tText.pData && xrtUtf8Valid((xstrview){ tText.pData, tText.iLen }, NULL), "escaped binary representation remains valid UTF-8");
    xwork__buf_unit(&tText);
}

static void test_agent_loop(void)
{
    static const char sWorkspace[] = "tests/tmp_xwork";
    static const char sSessionPath[] = "tests/tmp_xwork/.xcode/session.json";
    xllm_session_config tSessionConfig;
    xllm_session* pSession;
    xllm_error tLlmError;
    xwork_agent_config tAgentConfig;
    xwork_agent* pAgent;
    xwork_mcp_client* pMcpClient = NULL;
    xcancel* pCancel = NULL;
    xwork_error tError;
    xwork_run_result tResult;
    mock_model tMock;
    test_events tEvents;
    char* sOld = make_text(1300u, 'x');
    char* sFile = NULL;
    char* sArtifactAbsolute = NULL;
    size_t iFileSize = 0u;
    uint32_t uCompactionsBeforeExplicit = 0u;
    const xwork_tool_entry* pPatchTool;
    const xwork_tool_entry* pStartTool;
    const xwork_tool_entry* pWriteProcessTool;
    const xwork_tool_entry* pPollTool;
    const xwork_tool_entry* pExecTool;
    xwork_tool_context tPatchContext;
    xwork_tool_output tPatchOutput;
    xwork_tool_output tProcessOutput;
    unsigned long long uManagedId = 0u;
    xwork_result eAgentRun;
    uint64_t uInterruptedTurn = 0u;
    unsigned i;

    memset(&tMock, 0, sizeof(tMock));
    memset(&tEvents, 0, sizeof(tEvents));
    memset(&tResult, 0, sizeof(tResult));
    (void)xrtDirRemoveAll(sWorkspace);
    CHECK(xrtDirCreateAll((str)sWorkspace), "test workspace created");

    xllmSessionConfigInit(&tSessionConfig);
    /* The mock compaction script speaks the v2 durable 8-section format;
     * session v3 defaults to the Pi coding style, so select durable here. */
    tSessionConfig.sSummaryStyle = "durable";
    tSessionConfig.uContextWindowTokens = 8000u;
    tSessionConfig.uMaxOutputTokens = 600u;
    tSessionConfig.uSafetyReserveTokens = 100u;
    tSessionConfig.uRecentTurnsToKeep = 1u;
    tSessionConfig.uToolPruneBytes = 512u;
    tSessionConfig.uSummaryMaxTokens = 400u;
    tSessionConfig.fPruneTrigger = 0.20;
    tSessionConfig.fCompactTrigger = 0.50;
    pSession = xllmSessionCreate(&tSessionConfig, &tLlmError);
    CHECK(pSession != NULL, "small session created for deterministic compaction");
    if ( !pSession || !sOld ) goto cleanup;
    CHECK(xllmSessionAddText(pSession, 0u, XLLM_ROLE_SYSTEM, "Pinned test agent contract.", XLLM_SESSION_ENTRY_PINNED), "pinned system message added");
    for ( i = 0u; i < 7u; ++i ) {
        uint64_t uTurn = xllmSessionBeginTurn(pSession);
        CHECK(xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, sOld, 0u), "old user context added");
        CHECK(xllmSessionAddText(pSession, uTurn, XLLM_ROLE_ASSISTANT, sOld, 0u), "old assistant context added");
    }

    xworkAgentConfigInit(&tAgentConfig);
    CHECK(tAgentConfig.uMaxAgentTurns == 0u, "agent turns are unlimited by default");
    tAgentConfig.pSession = pSession;
    tAgentConfig.sWorkspaceRoot = sWorkspace;
    tAgentConfig.sSessionPath = sSessionPath;
    tAgentConfig.sModel = "mock-model";
    tAgentConfig.sReasoningEffort = "high";
    pCancel = xrtCancelCreate();
    tAgentConfig.pCancel = pCancel;
    tAgentConfig.OnModelComplete = mock_complete;
    tAgentConfig.pModelUserData = &tMock;
    tAgentConfig.OnEvent = on_event;
    tAgentConfig.pEventUserData = &tEvents;
    tAgentConfig.OnPermission = on_permission;
    tAgentConfig.pPermissionUserData = &tMock;
    tAgentConfig.OnHook = on_hook;
    tAgentConfig.pHookUserData = &tMock;
    tAgentConfig.iMaxInlineToolBytes = 300u;
    pAgent = xworkAgentCreate(&tAgentConfig, &tError);
    CHECK(pAgent != NULL, "agent creates with injected model boundary");
    CHECK(pAgent && xworkAgentToolCount(pAgent) == 9u, "nine practical builtin tools registered");
    if ( !pAgent ) goto cleanup;
    tMock.pAgent = pAgent;

    {
        xwork_tool_info tInfo;
        xwork_tool_definition tDynamic;
        uint64_t uGeneration = xworkAgentToolRegistryGeneration(pAgent);
        size_t iRemoved = 0u;
        memset(&tInfo, 0, sizeof(tInfo));
        memset(&tDynamic, 0, sizeof(tDynamic));
        tDynamic.sName = "registry_probe";
        tDynamic.sDescription = "Exercise dynamic registry lifecycle.";
        tDynamic.sParametersJson = "{\"type\":\"object\",\"additionalProperties\":false}";
        tDynamic.bStrict = true;
        tDynamic.eEffect = XWORK_TOOL_EFFECT_READ_ONLY;
        tDynamic.OnExecute = registry_probe_execute;
        tDynamic.sSource = "test.dynamic";
        CHECK(xworkAgentToolAt(pAgent, 0u, &tInfo) && tInfo.sSource &&
            strcmp(tInfo.sSource, "builtin") == 0,
            "tool registry enumeration exposes stable source metadata");
        CHECK(xworkAgentRegisterTool(pAgent, &tDynamic, &tError) &&
            xworkAgentToolCount(pAgent) == 10u &&
            xworkAgentToolRegistryGeneration(pAgent) == uGeneration + 1u,
            "dynamic tool registration advances the registry generation");
        xworkErrorInit(&tError);
        CHECK(xworkAgentUnregisterToolsBySource(pAgent, "test.dynamic", &iRemoved, &tError) &&
            iRemoved == 1u && xworkAgentToolCount(pAgent) == 9u &&
            xworkAgentToolRegistryGeneration(pAgent) == uGeneration + 2u,
            "bulk source removal atomically retires dynamic tools");
    }

    {
        const char* arrMcpArgs[] = {"--mcp-test-server"};
        xwork_mcp_stdio_config tMcpConfig;
        xwork_mcp_info tMcpInfo;
        const xwork_tool_entry* pMcpTool;
        xwork_tool_context tMcpToolContext;
        xwork_tool_output tMcpOutput;
        size_t iRemoved = 0u;
        xworkMcpStdioConfigInit(&tMcpConfig);
        tMcpConfig.sServerName = "phase3";
        tMcpConfig.sProgram = g_sSelfPath;
        tMcpConfig.psArguments = arrMcpArgs;
        tMcpConfig.iArgumentCount = 1u;
        tMcpConfig.uRequestTimeoutMs = 5000u;
        pMcpClient = xworkMcpClientCreate(&tMcpConfig, &tError);
        CHECK(pMcpClient && xworkMcpClientConnect(pMcpClient, &tError),
            "MCP stdio client completes initialize and initialized handshake");
        CHECK(pMcpClient && xworkMcpClientRefreshTools(pMcpClient, pAgent, &tError) &&
            xworkAgentToolCount(pAgent) == 10u,
            "MCP tools/list dynamically registers namespaced proxy tools");
        pMcpTool = xwork__find_tool(pAgent, "mcp__phase3__echo");
        memset(&tMcpToolContext, 0, sizeof(tMcpToolContext));
        tMcpToolContext.pAgent = pAgent;
        tMcpToolContext.sWorkspaceRoot = sWorkspace;
        xworkToolOutputInit(&tMcpOutput);
        CHECK(pMcpTool && strcmp(pMcpTool->sSource, "mcp:phase3") == 0 &&
            pMcpTool->eEffect == XWORK_TOOL_EFFECT_PROCESS,
            "MCP proxies retain source ownership and distrust read-only hints by default");
        CHECK(pMcpTool && pMcpTool->OnExecute(
                pMcpTool->pUserData, &tMcpToolContext,
                "{\"text\":\"hello mcp\"}", &tMcpOutput, &tError) == XWORK_RESULT_OK &&
            tMcpOutput.bSuccess && tMcpOutput.sContent &&
            strstr(tMcpOutput.sContent, "echo: hello mcp") &&
            strstr(tMcpOutput.sContent, "structured_content"),
            "MCP tools/call returns text and structured content through the proxy");
        xworkToolOutputUnit(&tMcpOutput);
        memset(&tMcpInfo, 0, sizeof(tMcpInfo));
        CHECK(xworkMcpClientGetInfo(pMcpClient, &tMcpInfo) && tMcpInfo.bConnected &&
            tMcpInfo.iToolCount == 1u && tMcpInfo.uRequestsCompleted == 3u &&
            strcmp(tMcpInfo.sProtocolVersion, "2025-06-18") == 0,
            "MCP diagnostics expose negotiated version, tool count, and request count");
        {
            uint64_t uStartedMs = xrtClock() / UINT64_C(1000);
            xwork_result eMcpDeadlineResult;
            uint64_t uElapsedMs;
            xworkToolOutputInit(&tMcpOutput);
            eMcpDeadlineResult = xworkMcpClientCallTool(
                pMcpClient, "echo", "{\"delay\":true}", NULL,
                xrtDeadlineAfter(UINT64_C(100000)), &tMcpOutput, &tError);
            uElapsedMs = xrtClock() / UINT64_C(1000) - uStartedMs;
            if ( eMcpDeadlineResult != XWORK_RESULT_TIMEOUT ||
                 tError.eCode != XWORK_ERROR_TIMEOUT || uElapsedMs >= 2000u ) {
                fprintf(stderr, "MCP deadline: result=%d error=%d elapsed=%llu message=%s\n",
                    (int)eMcpDeadlineResult, (int)tError.eCode,
                    (unsigned long long)uElapsedMs, tError.sMessage);
            }
            CHECK(eMcpDeadlineResult == XWORK_RESULT_TIMEOUT &&
                tError.eCode == XWORK_ERROR_TIMEOUT &&
                uElapsedMs < 2000u,
                "MCP tool calls honor operation deadlines and send cancellation promptly");
            xworkToolOutputUnit(&tMcpOutput);
        }
        CHECK(xworkAgentUnregisterToolsBySource(
                pAgent, "mcp:phase3", &iRemoved, &tError) && iRemoved == 1u &&
            xworkAgentToolCount(pAgent) == 9u,
            "MCP source can be detached without disturbing builtin tools");
        xworkMcpClientDestroy(pMcpClient);
        pMcpClient = NULL;
    }

    eAgentRun = xworkAgentRun(pAgent, "Create and verify the requested note file.", &tResult, &tError);
    if ( eAgentRun != XWORK_RESULT_OK ) {
        fprintf(stderr, "agent loop error: result=%d code=%s message=%s\n",
            (int)eAgentRun, xworkErrorCodeName(tError.eCode), tError.sMessage);
    }
    CHECK(eAgentRun == XWORK_RESULT_OK, "multi-turn tool loop completes");
    CHECK(tMock.bRegistryMutationBlocked,
        "tool registry mutation is rejected while an agent run is active");
    CHECK(tResult.sFinalText && strstr(tResult.sFinalText, "verified"), "final assistant response returned");
    CHECK(tResult.uAgentTurns == 5u && tResult.uToolCalls == 8u, "verification gate adds one model turn while eight tool calls run");
    CHECK(tResult.uCompactions >= 1u && tMock.uCompactionCalls == tResult.uCompactions + 1u &&
        tResult.uRejectedCompactionSummaries == 1u,
        "rejected compaction is corrected before the durable checkpoint advances");
    CHECK(tEvents.uCompactions == tResult.uCompactions && tEvents.uRejectedCompactions == 1u &&
        tEvents.bCompactionQualityMetadata && tEvents.uErrors == 0u,
        "compaction quality and completion events are balanced");
    CHECK(tEvents.uModelStarts == tResult.uAgentTurns && tEvents.uModelDone == tResult.uAgentTurns &&
          tEvents.bRequestMetadata && tEvents.bResponseMetadata,
        "model lifecycle events expose reproducible request and provider diagnostics");
    CHECK(tMock.bSawTools && tMock.bSawParallel && tMock.bSawToolResults && tMock.bSawContext,
        "tools, parallel flag, continuity, and operation context reach model");
    CHECK(tMock.bSawCompactionSummary, "post-compaction model turns receive the summary checkpoint");
    CHECK(tMock.bSawVerificationGate, "premature completion receives a durable verification-gate prompt");
    CHECK(tMock.uPermissionCalls == 8u && tMock.bSawPathPermission && tMock.bSawCommandPermission && tMock.bSawHighRiskPermission,
        "structured permission callback sees every tool plus path, command, and risk metadata");
    CHECK(tMock.uBeforeToolHooks == 8u && tMock.uAfterToolHooks == 8u, "before/after tool hooks bracket every executed tool");
    CHECK(tEvents.uToolStarts == 8u && tEvents.uToolDone == 8u, "tool lifecycle events are balanced");
    CHECK(tEvents.uToolSuccess == 7u && tEvents.uToolFailure == 1u, "seven builtin operations succeed and escape is a tool-level failure");
    CHECK(tEvents.sLastArtifact[0] != '\0', "oversized tool output spills to an artifact");
    CHECK(xrtFileExists((str)sSessionPath), "session autosaves atomically during the run");

    memset(&tPatchContext, 0, sizeof(tPatchContext));
    xworkToolOutputInit(&tPatchOutput);
    pPatchTool = xwork__find_tool(pAgent, "edit");
    tPatchContext.pAgent = pAgent;
    /* 0-match edit returns candidate context lines (self-correction). */
    CHECK(pPatchTool && pPatchTool->OnExecute(pPatchTool->pUserData, &tPatchContext,
        "{\"path\":\"sandbox/note.txt\",\"edits\":[{\"old_text\":\"NOT-PRESENT-TEXT\",\"new_text\":\"X\"}]}",
        &tPatchOutput, &tError) == XWORK_RESULT_OK && !tPatchOutput.bSuccess &&
        tPatchOutput.sContent && strstr(tPatchOutput.sContent, "candidates:") &&
        strstr(tPatchOutput.sContent, "0 matches"),
        "0-match edit returns numbered candidate lines for self-correction");
    xworkToolOutputUnit(&tPatchOutput);
    xworkToolOutputInit(&tPatchOutput);
    /* Ambiguous edit (no replace_all) lists the matching lines. */
    CHECK(pPatchTool && pPatchTool->OnExecute(pPatchTool->pUserData, &tPatchContext,
        "{\"path\":\"sandbox/note.txt\",\"edits\":[{\"old_text\":\"abc\",\"new_text\":\"X\"}]}",
        &tPatchOutput, &tError) == XWORK_RESULT_OK && !tPatchOutput.bSuccess &&
        tPatchOutput.sContent && strstr(tPatchOutput.sContent, "times"),
        "ambiguous edit reports match count and candidates");
    xworkToolOutputUnit(&tPatchOutput);

    sFile = (char*)xrtFileReadAll("tests/tmp_xwork/sandbox/note.txt", &iFileSize);
    CHECK(sFile && iFileSize > 1600u && strncmp(sFile, "PATCHED-", 8u) == 0, "workspace file was created then edited by both edit tools");
    CHECK(!xrtFileExists((str)"tests/tmp_xwork/sandbox/note.txt/child.txt"), "failed edit left no partial target behind");

    pStartTool = xwork__find_tool(pAgent, "spawn");
    pWriteProcessTool = xwork__find_tool(pAgent, "stdin");
    pPollTool = xwork__find_tool(pAgent, "poll");
    xworkToolOutputInit(&tProcessOutput);
#if defined(_WIN32)
    CHECK(pStartTool && pStartTool->OnExecute(pStartTool->pUserData, &tPatchContext,
        "{\"argv\":[\"findstr\",\"persistent\"]}", &tProcessOutput, &tError) == XWORK_RESULT_OK &&
        tProcessOutput.bSuccess && tProcessOutput.sContent && sscanf(tProcessOutput.sContent, "task_id: %llu", &uManagedId) == 1,
        "spawn starts and returns a stable task id");
#else
    CHECK(pStartTool && pStartTool->OnExecute(pStartTool->pUserData, &tPatchContext,
        "{\"argv\":[\"grep\",\"persistent\"]}", &tProcessOutput, &tError) == XWORK_RESULT_OK &&
        tProcessOutput.bSuccess && tProcessOutput.sContent && sscanf(tProcessOutput.sContent, "task_id: %llu", &uManagedId) == 1,
        "spawn starts and returns a stable task id");
#endif
    xworkToolOutputUnit(&tProcessOutput);
    xworkToolOutputInit(&tProcessOutput);
    if ( uManagedId ) {
        char sProcessArgs[512];
        snprintf(sProcessArgs, sizeof(sProcessArgs),
            "{\"task_id\":%llu,\"input\":\"persistent hello\",\"append_newline\":true,\"close_stdin\":true}", uManagedId);
        CHECK(pWriteProcessTool && pWriteProcessTool->OnExecute(pWriteProcessTool->pUserData, &tPatchContext,
            sProcessArgs, &tProcessOutput, &tError) == XWORK_RESULT_OK && tProcessOutput.bSuccess,
            "stdin writes and an explicit stdin close work");
        xworkToolOutputUnit(&tProcessOutput);
        xworkToolOutputInit(&tProcessOutput);
        snprintf(sProcessArgs, sizeof(sProcessArgs),
            "{\"task_id\":%llu,\"wait_ms\":5000,\"release\":true}", uManagedId);
        CHECK(pPollTool && pPollTool->OnExecute(pPollTool->pUserData, &tPatchContext,
            sProcessArgs, &tProcessOutput, &tError) == XWORK_RESULT_OK && tProcessOutput.bSuccess &&
            tProcessOutput.sContent && strstr(tProcessOutput.sContent, "persistent hello") && strstr(tProcessOutput.sContent, "state: exited"),
            "poll returns incremental output and final exit state");
        CHECK(pAgent->iProcessCount == 0u, "released task leaves no live registry entry");
    }
    xworkToolOutputUnit(&tProcessOutput);

    pExecTool = xwork__find_tool(pAgent, "exec");
    xworkToolOutputInit(&tProcessOutput);
#if defined(_WIN32)
    CHECK(pExecTool && pExecTool->OnExecute(pExecTool->pUserData, &tPatchContext,
        "{\"argv\":[\"cmd\",\"/c\",\"exit\",\"7\"],\"expected_exit_codes\":[7]}",
        &tProcessOutput, &tError) == XWORK_RESULT_OK && tProcessOutput.bSuccess &&
        tProcessOutput.sContent && strstr(tProcessOutput.sContent, "exit_code: 7") &&
        strstr(tProcessOutput.sContent, "exit_expected: true"),
        "exec accepts an explicitly expected nonzero exit code");
#else
    CHECK(pExecTool && pExecTool->OnExecute(pExecTool->pUserData, &tPatchContext,
        "{\"argv\":[\"sh\",\"-c\",\"exit 7\"],\"expected_exit_codes\":[7]}",
        &tProcessOutput, &tError) == XWORK_RESULT_OK && tProcessOutput.bSuccess &&
        tProcessOutput.sContent && strstr(tProcessOutput.sContent, "exit_code: 7") &&
        strstr(tProcessOutput.sContent, "exit_expected: true"),
        "exec accepts an explicitly expected nonzero exit code");
#endif
    xworkToolOutputUnit(&tProcessOutput);
    xworkToolOutputInit(&tProcessOutput);
#if defined(_WIN32)
    CHECK(pExecTool && pExecTool->OnExecute(pExecTool->pUserData, &tPatchContext,
        "{\"argv\":[\"cmd\",\"/c\",\"exit\",\"7\"]}", &tProcessOutput, &tError) == XWORK_RESULT_OK &&
        !tProcessOutput.bSuccess && tProcessOutput.sContent && strstr(tProcessOutput.sContent, "exit_expected: false"),
        "exec still rejects a nonzero exit code by default");
#else
    CHECK(pExecTool && pExecTool->OnExecute(pExecTool->pUserData, &tPatchContext,
        "{\"argv\":[\"sh\",\"-c\",\"exit 7\"]}", &tProcessOutput, &tError) == XWORK_RESULT_OK &&
        !tProcessOutput.bSuccess && tProcessOutput.sContent && strstr(tProcessOutput.sContent, "exit_expected: false"),
        "exec still rejects a nonzero exit code by default");
#endif
    xworkToolOutputUnit(&tProcessOutput);
    xworkToolOutputInit(&tProcessOutput);
    CHECK(pExecTool && pExecTool->OnExecute(pExecTool->pUserData, &tPatchContext,
        "{\"argv\":[\"echo\",\"invalid\"],\"expected_exit_codes\":[]}",
        &tProcessOutput, &tError) == XWORK_RESULT_OK && !tProcessOutput.bSuccess &&
        tProcessOutput.sContent && strstr(tProcessOutput.sContent, "between 1 and 32"),
        "exec command rejects an empty expected exit-code contract");
    xworkToolOutputUnit(&tProcessOutput);

    if ( tEvents.sLastArtifact[0] ) {
        sArtifactAbsolute = xrtPathJoin(sWorkspace, tEvents.sLastArtifact);
        CHECK(sArtifactAbsolute && xrtFileExists((str)sArtifactAbsolute), "full oversized output artifact exists");
    }
    CHECK(!xrtFileExists((str)"tests/outside.txt"), "workspace escape tool call could not access outside path");
    CHECK(tError.eCode == XWORK_ERROR_NONE, "successful run does not leak a stale recoverable tool error");
    uCompactionsBeforeExplicit = tMock.uCompactionCalls;
    CHECK(xworkAgentCompact(pAgent, &tError) == XWORK_RESULT_OK, "explicit safe-prefix compaction completes after the run");
    CHECK(tMock.uCompactionCalls == uCompactionsBeforeExplicit + 1u &&
        tEvents.uCompactions + tEvents.uRejectedCompactions == tMock.uCompactionCalls,
        "explicit compaction adds one model summary and lifecycle event");

    {
        xllm_tool_call arrInterruptedCalls[2];
        xllm_response tInterruptedResponse;
        xllm_session_stats tInterruptedStats;
        const char* sVerifyArgs;
        memset(arrInterruptedCalls, 0, sizeof(arrInterruptedCalls));
        memset(&tInterruptedResponse, 0, sizeof(tInterruptedResponse));
        arrInterruptedCalls[0].sId = "call_recovered_list";
        arrInterruptedCalls[0].sName = "read";
        arrInterruptedCalls[0].sArgumentsJson = "{\"path\":\"sandbox\"}";
        arrInterruptedCalls[1].sId = "call_recovered_verify";
        arrInterruptedCalls[1].sName = "exec";
#if defined(_WIN32)
        sVerifyArgs = "{\"argv\":[\"cmd\",\"/c\",\"type\",\"sandbox\\\\note.txt\"]}";
#else
        sVerifyArgs = "{\"argv\":[\"cat\",\"sandbox/note.txt\"]}";
#endif
        arrInterruptedCalls[1].sArgumentsJson = (char*)sVerifyArgs;
        tInterruptedResponse.pToolCalls = arrInterruptedCalls;
        tInterruptedResponse.iToolCallCount = 2u;
        uInterruptedTurn = xllmSessionBeginTurn(pSession);
        CHECK(uInterruptedTurn &&
            xllmSessionAddText(pSession, uInterruptedTurn, XLLM_ROLE_USER, "Finish this interrupted verification batch.", 0u) &&
            xllmSessionAddAssistantResponse(pSession, uInterruptedTurn, &tInterruptedResponse) &&
            xllmSessionAddToolResult(pSession, uInterruptedTurn, "call_recovered_list", "status: success"),
            "interrupted batch fixture records one completed and one pending tool");
        CHECK(xllmSessionGetStats(pSession, &tInterruptedStats) && tInterruptedStats.uPendingToolCalls == 1u,
            "interrupted batch exposes one pending tool before resume");
        xworkRunResultUnit(&tResult);
        CHECK(xworkAgentRun(pAgent, "This prompt must not be appended.", &tResult, &tError) == XWORK_RESULT_ERROR &&
            tError.eCode == XWORK_ERROR_CONTEXT && xllmSessionCurrentTurn(pSession) == uInterruptedTurn,
            "normal run refuses to duplicate a prompt over interrupted work");
        CHECK(xworkAgentResume(pAgent, &tResult, &tError) == XWORK_RESULT_OK,
            "resume executes the pending tool and continues the model loop");
        CHECK(tResult.uToolCalls == 1u && tResult.uModelCalls == 1u &&
            tResult.sFinalText && strstr(tResult.sFinalText, "verified"),
            "resumed run reports only newly recovered work and returns final text");
        CHECK(xllmSessionGetStats(pSession, &tInterruptedStats) && tInterruptedStats.uPendingToolCalls == 0u,
            "resumed run durably resolves the pending tool call");
    }

    xworkRunResultUnit(&tResult);
    xworkAgentDestroy(pAgent);
cleanup:
    xworkMcpClientDestroy(pMcpClient);
    xrtCancelDestroy(pCancel);
    if ( sFile && iFileSize ) xrtFree(sFile);
    if ( sArtifactAbsolute ) xrtFree(sArtifactAbsolute);
    free(sOld);
    xllmSessionDestroy(pSession);
    (void)xrtDirRemoveAll(sWorkspace);
}

static void test_readonly_subagent(void)
{
    static const char sWorkspace[] = "tests/tmp_xwork_subagent";
    char sEvidence[2048];
    xllm_session_config tSessionConfig;
    xllm_session* pSession = NULL;
    xllm_error tLlmError;
    xwork_agent_config tAgentConfig;
    xwork_agent* pParent = NULL;
    xwork_readonly_subagent_config tSubagentConfig;
    xwork_run_result tResult;
    xwork_error tError;
    subagent_mock tMock;
    xwork_result eResult = XWORK_RESULT_ERROR;
    size_t i;
    memset(&tMock, 0, sizeof(tMock));
    tMock.bScopedEvents = true;
    memset(&tResult, 0, sizeof(tResult));
    for ( i = 0u; i + 1u < sizeof(sEvidence); ++i ) {
        static const char sMarker[] = "readonly-evidence-marker ";
        sEvidence[i] = sMarker[i % (sizeof(sMarker) - 1u)];
    }
    sEvidence[sizeof(sEvidence) - 1u] = '\0';
    (void)xrtDirRemoveAll(sWorkspace);
    CHECK(xrtDirCreateAll("tests/tmp_xwork_subagent/.xcode") &&
          xrtFileWriteAtomic("tests/tmp_xwork_subagent/evidence.txt",
            (xbytesview){ (const uint8*)sEvidence, strlen(sEvidence) }) &&
          xrtFileWriteAtomic("tests/tmp_xwork_subagent/.xcode/secrets.local.json",
            (xbytesview){ (const uint8*)"private-fixture", strlen("private-fixture") }),
        "read-only subagent workspace and protected internal fixture created");
    xllmSessionConfigInit(&tSessionConfig);
    pSession = xllmSessionCreate(&tSessionConfig, &tLlmError);
    xworkAgentConfigInit(&tAgentConfig);
    tAgentConfig.pSession = pSession;
    tAgentConfig.sWorkspaceRoot = sWorkspace;
    tAgentConfig.OnModelComplete = subagent_complete;
    tAgentConfig.pModelUserData = &tMock;
    tAgentConfig.bRegisterBuiltinTools = false;
    tAgentConfig.iMaxInlineToolBytes = 128u;
    pParent = xworkAgentCreate(&tAgentConfig, &tError);
    CHECK(pSession && pParent && xworkAgentToolCount(pParent) == 0u,
        "parent fixture creates without exposing tools to the child implicitly");
    xworkReadOnlySubagentConfigInit(&tSubagentConfig);
    tSubagentConfig.uParentAgentTurn = 42u;
    tSubagentConfig.uTimeoutMs = 5000u;
    tSubagentConfig.uMaxAgentTurns = 4u;
    tSubagentConfig.uMaxOutputTokens = 1024u;
    tSubagentConfig.iMaxFinalBytes = 256u;
    tSubagentConfig.OnEvent = subagent_event;
    tSubagentConfig.pEventUserData = &tMock;
    if ( pParent ) {
        eResult = xworkAgentRunReadOnlySubagent(
            pParent, &tSubagentConfig, "Inspect evidence and report isolation.",
            &tResult, &tError);
    }
    CHECK(eResult == XWORK_RESULT_OK && tMock.uModelCalls == 2u &&
          tResult.uAgentTurns == 2u && tResult.uToolCalls == 2u,
        "isolated read-only subagent completes a bounded tool loop");
    CHECK(tMock.bReadOnlyToolSet && tMock.bSawReadableFile && tMock.bSawInternalDenial,
        "subagent exposes only inspection tools and denies internal control paths");
    CHECK(tMock.bScopedEvents && tMock.uAgentStarts == 1u && tMock.uAgentDone == 1u &&
          tMock.uToolStarts == 2u && tMock.uToolDone == 2u &&
          tResult.uAgentDepth == 1u && tResult.uDelegationId == 1u,
        "subagent lifecycle is tagged with depth, delegation, and parent turn");
    CHECK(tResult.sFinalText && strlen(tResult.sFinalText) <= 256u &&
          strstr(tResult.sFinalText, "truncated by host budget") &&
          !xrtDirExists((str)"tests/tmp_xwork_subagent/.xcode/artifacts"),
        "subagent final output is capped without artifact side effects");
    xworkRunResultUnit(&tResult);
    memset(&tResult, 0, sizeof(tResult));
    tMock.uModelCalls = 0u;
    tSubagentConfig.uMaxAgentTurns = 1u;
    tSubagentConfig.OnEvent = NULL;
    tSubagentConfig.pEventUserData = NULL;
    eResult = pParent ? xworkAgentRunReadOnlySubagent(
        pParent, &tSubagentConfig, "Exercise the hard child turn budget.",
        &tResult, &tError) : XWORK_RESULT_ERROR;
    CHECK(eResult == XWORK_RESULT_LIMIT && tError.eCode == XWORK_ERROR_LOOP_GUARD &&
          tResult.uAgentTurns == 1u && tResult.uAgentDepth == 1u &&
          tResult.uDelegationId == 2u,
        "subagent hard turn budget stops an unfinished child deterministically");
    xworkRunResultUnit(&tResult);
    xworkAgentDestroy(pParent);
    xllmSessionDestroy(pSession);
    (void)xrtDirRemoveAll(sWorkspace);
}

static void test_agent_context_deadline(void)
{
    static const char sWorkspace[] = "tests/tmp_xwork_deadline";
    xllm_session_config tSessionConfig;
    xllm_session* pSession = NULL;
    xllm_error tLlmError;
    xwork_agent_config tAgentConfig;
    xwork_agent* pAgent = NULL;
    xwork_error tError;
    xwork_run_result tResult;
    mock_model tMock;
    uint64_t uTurnBefore = 0u;
    memset(&tMock, 0, sizeof(tMock));
    memset(&tResult, 0, sizeof(tResult));
    (void)xrtDirRemoveAll(sWorkspace);
    CHECK(xrtDirCreateAll((str)sWorkspace), "deadline test workspace created");
    xllmSessionConfigInit(&tSessionConfig);
    pSession = xllmSessionCreate(&tSessionConfig, &tLlmError);
    xworkAgentConfigInit(&tAgentConfig);
    tAgentConfig.pSession = pSession;
    tAgentConfig.uDeadline = xrtDeadlineAfter(0u);
    tAgentConfig.sWorkspaceRoot = sWorkspace;
    tAgentConfig.OnModelComplete = mock_complete;
    tAgentConfig.pModelUserData = &tMock;
    pAgent = xworkAgentCreate(&tAgentConfig, &tError);
    CHECK(pSession && pAgent, "deadline-scoped agent creates");
    if ( pSession ) uTurnBefore = xllmSessionCurrentTurn(pSession);
    if ( pAgent ) {
        CHECK(xworkAgentRun(pAgent, "This prompt must not be appended.", &tResult, &tError) == XWORK_RESULT_TIMEOUT &&
            tError.eCode == XWORK_ERROR_TIMEOUT && tMock.uAgentCalls == 0u &&
            tMock.uCompactionCalls == 0u && xllmSessionCurrentTurn(pSession) == uTurnBefore,
            "expired operation deadline stops before session mutation or model call");
    }
    xworkRunResultUnit(&tResult);
    xworkAgentDestroy(pAgent);
    xllmSessionDestroy(pSession);
    (void)xrtDirRemoveAll(sWorkspace);
}

/* ------------------------------------------------------------------ */
/* Unified task system: argv exec, wait(any|all), notify delivery,     */
/* and the model-clock watchdog.                                        */
/* ------------------------------------------------------------------ */

static void test_task_system(void)
{
    static const char sWorkspace[] = "tests/tmp_xwork_tasks";
    xllm_session_config tSessionConfig;
    xllm_session* pSession = NULL;
    xwork_agent_config tAgentConfig;
    xwork_agent* pAgent = NULL;
    xwork_error tError;
    const xwork_tool_entry* pExecTool;
    const xwork_tool_entry* pSpawnTool;
    const xwork_tool_entry* pWaitTool;
    const xwork_tool_entry* pStopTool;
    xwork_tool_context tCtx;
    xwork_tool_output tOut;
    xwork_task_notice tNotices[4];
    xwork_watchdog_digest tDigest;
    uint64_t uFast = 0u;
    uint64_t uSlow = 0u;
    char sWaitArgs[256];

    (void)xrtDirRemoveAll(sWorkspace);
    CHECK(xrtDirCreateAll((str)sWorkspace), "task system workspace created");
    xllmSessionConfigInit(&tSessionConfig);
    pSession = xllmSessionCreate(&tSessionConfig, NULL);
    xworkAgentConfigInit(&tAgentConfig);
    tAgentConfig.pSession = pSession;
    tAgentConfig.sWorkspaceRoot = sWorkspace;
    pAgent = xworkAgentCreate(&tAgentConfig, &tError);
    CHECK(pSession && pAgent, "task fixture agent creates");
    pExecTool = pAgent ? xwork__find_tool(pAgent, "exec") : NULL;
    pSpawnTool = pAgent ? xwork__find_tool(pAgent, "spawn") : NULL;
    pWaitTool = pAgent ? xwork__find_tool(pAgent, "wait") : NULL;
    pStopTool = pAgent ? xwork__find_tool(pAgent, "stop") : NULL;
    memset(&tCtx, 0, sizeof(tCtx));
    tCtx.pAgent = pAgent;
    tCtx.sWorkspaceRoot = sWorkspace;

    /* argv exec round trip. */
    xworkToolOutputInit(&tOut);
#if defined(_WIN32)
    CHECK(pExecTool && pExecTool->OnExecute(pExecTool->pUserData, &tCtx,
        "{\"argv\":[\"cmd\",\"/c\",\"echo\",\"task-ok\"]}", &tOut, &tError) == XWORK_RESULT_OK &&
        tOut.bSuccess && tOut.sContent && strstr(tOut.sContent, "task-ok"),
        "exec runs argv directly and captures stdout");
#else
    CHECK(pExecTool && pExecTool->OnExecute(pExecTool->pUserData, &tCtx,
        "{\"argv\":[\"echo\",\"task-ok\"]}", &tOut, &tError) == XWORK_RESULT_OK &&
        tOut.bSuccess && tOut.sContent && strstr(tOut.sContent, "task-ok"),
        "exec runs argv directly and captures stdout");
#endif
    xworkToolOutputUnit(&tOut);

    /* Two background tasks: fast with notify, slow with a remind clock. */
    xworkToolOutputInit(&tOut);
#if defined(_WIN32)
    CHECK(pSpawnTool && pSpawnTool->OnExecute(pSpawnTool->pUserData, &tCtx,
        "{\"argv\":[\"ping\",\"-n\",\"1\",\"127.0.0.1\"],\"notify\":\"fast probe finished; collect its tail\"}",
        &tOut, &tError) == XWORK_RESULT_OK && tOut.bSuccess &&
        tOut.sContent && sscanf(tOut.sContent, "task_id: %llu", &uFast) == 1,
        "spawn starts the fast task with a notify message");
#else
    CHECK(pSpawnTool && pSpawnTool->OnExecute(pSpawnTool->pUserData, &tCtx,
        "{\"argv\":[\"true\"],\"notify\":\"fast probe finished; collect its tail\"}",
        &tOut, &tError) == XWORK_RESULT_OK && tOut.bSuccess &&
        tOut.sContent && sscanf(tOut.sContent, "task_id: %llu", &uFast) == 1,
        "spawn starts the fast task with a notify message");
#endif
    xworkToolOutputUnit(&tOut);
    xworkToolOutputInit(&tOut);
#if defined(_WIN32)
    CHECK(pSpawnTool && pSpawnTool->OnExecute(pSpawnTool->pUserData, &tCtx,
        "{\"argv\":[\"ping\",\"-n\",\"30\",\"127.0.0.1\"],\"remind_after_ms\":100}",
        &tOut, &tError) == XWORK_RESULT_OK && tOut.bSuccess &&
        tOut.sContent && sscanf(tOut.sContent, "task_id: %llu", &uSlow) == 1,
        "spawn starts the slow task with a model-set reminder");
#else
    CHECK(pSpawnTool && pSpawnTool->OnExecute(pSpawnTool->pUserData, &tCtx,
        "{\"argv\":[\"sleep\",\"30\"],\"remind_after_ms\":100}",
        &tOut, &tError) == XWORK_RESULT_OK && tOut.bSuccess &&
        tOut.sContent && sscanf(tOut.sContent, "task_id: %llu", &uSlow) == 1,
        "spawn starts the slow task with a model-set reminder");
#endif
    xworkToolOutputUnit(&tOut);

    /* wait(any) returns with the fast task exited and the slow one running. */
    if ( uFast && uSlow && pWaitTool ) {
        xworkToolOutputInit(&tOut);
        (void)snprintf(sWaitArgs, sizeof(sWaitArgs),
            "{\"task_ids\":[%llu,%llu],\"mode\":\"any\",\"timeout_ms\":15000}", uFast, uSlow);
        CHECK(pWaitTool->OnExecute(pWaitTool->pUserData, &tCtx, sWaitArgs, &tOut, &tError)
            == XWORK_RESULT_OK && tOut.bSuccess && tOut.sContent &&
            strstr(tOut.sContent, "state: exited") && strstr(tOut.sContent, "state: running"),
            "wait(any) reports the finished task beside the running one");
        xworkToolOutputUnit(&tOut);
    }

    /* The completion notice carries the model's notify message exactly once. */
    if ( pAgent ) {
        size_t iTaken = xworkAgentTakeTaskNotices(pAgent, tNotices, 4u);
        CHECK(iTaken == 1u && tNotices[0].uTaskId == uFast &&
            tNotices[0].eKind == XWORK_TASK_PROCESS && tNotices[0].bExitedCleanly &&
            tNotices[0].sNotify && strcmp(tNotices[0].sNotify,
                "fast probe finished; collect its tail") == 0,
            "task notice delivers the model's notify message");
        CHECK(xworkAgentTakeTaskNotices(pAgent, tNotices, 4u) == 0u,
            "consumed notices are not redelivered");
    }

    /* The model-clock watchdog fires on the stalled reminder. */
    if ( pAgent ) {
        xrtSleep(200u);
        CHECK(xworkTaskWatchdog(pAgent, &tDigest) &&
            tDigest.bShouldWake && tDigest.iStalledTasks == 1u &&
            tDigest.iRunningTasks >= 1u,
            "watchdog wakes on the model-set remind deadline");
    }

    /* Stop and release both tasks. */
    if ( uFast && pStopTool ) {
        xworkToolOutputInit(&tOut);
        (void)snprintf(sWaitArgs, sizeof(sWaitArgs),
            "{\"task_id\":%llu,\"release\":true}", uFast);
        CHECK(pStopTool->OnExecute(pStopTool->pUserData, &tCtx, sWaitArgs, &tOut, &tError)
            == XWORK_RESULT_OK, "fast task released after its notice");
        xworkToolOutputUnit(&tOut);
    }
    if ( uSlow && pStopTool ) {
        xworkToolOutputInit(&tOut);
        (void)snprintf(sWaitArgs, sizeof(sWaitArgs),
            "{\"task_id\":%llu,\"mode\":\"kill_tree\",\"release\":true}", uSlow);
        CHECK(pStopTool->OnExecute(pStopTool->pUserData, &tCtx, sWaitArgs, &tOut, &tError)
            == XWORK_RESULT_OK && tOut.bSuccess,
            "stop releases the stalled task");
        xworkToolOutputUnit(&tOut);
    }
    CHECK(pAgent && pAgent->iProcessCount == 0u, "task table drains after release");

    /* wait(all) on two quick tasks. */
    if ( pSpawnTool && pWaitTool ) {
        uint64_t uA = 0u;
        uint64_t uB = 0u;
        xworkToolOutputInit(&tOut);
#if defined(_WIN32)
        CHECK(pSpawnTool->OnExecute(pSpawnTool->pUserData, &tCtx,
            "{\"argv\":[\"ping\",\"-n\",\"1\",\"127.0.0.1\"]}", &tOut, &tError) == XWORK_RESULT_OK &&
            tOut.bSuccess && tOut.sContent && sscanf(tOut.sContent, "task_id: %llu", &uA) == 1,
            "wait-all fixture task A starts");
#else
        CHECK(pSpawnTool->OnExecute(pSpawnTool->pUserData, &tCtx,
            "{\"argv\":[\"true\"]}", &tOut, &tError) == XWORK_RESULT_OK &&
            tOut.bSuccess && tOut.sContent && sscanf(tOut.sContent, "task_id: %llu", &uA) == 1,
            "wait-all fixture task A starts");
#endif
        xworkToolOutputUnit(&tOut);
        xworkToolOutputInit(&tOut);
#if defined(_WIN32)
        CHECK(pSpawnTool->OnExecute(pSpawnTool->pUserData, &tCtx,
            "{\"argv\":[\"ping\",\"-n\",\"2\",\"127.0.0.1\"]}", &tOut, &tError) == XWORK_RESULT_OK &&
            tOut.bSuccess && tOut.sContent && sscanf(tOut.sContent, "task_id: %llu", &uB) == 1,
            "wait-all fixture task B starts");
#else
        CHECK(pSpawnTool->OnExecute(pSpawnTool->pUserData, &tCtx,
            "{\"argv\":[\"sleep\",\"1\"]}", &tOut, &tError) == XWORK_RESULT_OK &&
            tOut.bSuccess && tOut.sContent && sscanf(tOut.sContent, "task_id: %llu", &uB) == 1,
            "wait-all fixture task B starts");
#endif
        xworkToolOutputUnit(&tOut);
        if ( uA && uB ) {
            xworkToolOutputInit(&tOut);
            (void)snprintf(sWaitArgs, sizeof(sWaitArgs),
                "{\"task_ids\":[%llu,%llu],\"mode\":\"all\",\"timeout_ms\":15000}", uA, uB);
            CHECK(pWaitTool->OnExecute(pWaitTool->pUserData, &tCtx, sWaitArgs, &tOut, &tError)
                == XWORK_RESULT_OK && tOut.bSuccess && tOut.sContent &&
                strstr(tOut.sContent, "state: exited"),
                "wait(all) returns with both tasks exited");
            xworkToolOutputUnit(&tOut);
            (void)snprintf(sWaitArgs, sizeof(sWaitArgs),
                "{\"task_id\":%llu,\"release\":true}", uA);
            if ( pStopTool ) (void)pStopTool->OnExecute(pStopTool->pUserData, &tCtx, sWaitArgs, &tOut, &tError);
            (void)snprintf(sWaitArgs, sizeof(sWaitArgs),
                "{\"task_id\":%llu,\"release\":true}", uB);
            if ( pStopTool ) (void)pStopTool->OnExecute(pStopTool->pUserData, &tCtx, sWaitArgs, &tOut, &tError);
        }
    }

    xworkAgentDestroy(pAgent);
    xllmSessionDestroy(pSession);
    (void)xrtDirRemoveAll(sWorkspace);
}

/* ------------------------------------------------------------------ */
/* Edit/EOL/write batch: batch atomic edits, candidate lines, model-LF */
/* discipline, auto parent creation.                                   */
/* ------------------------------------------------------------------ */

static void test_edit_eol_write(void)
{
    static const char sWorkspace[] = "tests/tmp_xwork_edit";
    xllm_session_config tSessionConfig;
    xllm_session* pSession = NULL;
    xwork_agent_config tAgentConfig;
    xwork_agent* pAgent = NULL;
    xwork_error tError;
    const xwork_tool_entry* pEditTool;
    const xwork_tool_entry* pWriteTool;
    const xwork_tool_entry* pReadTool;
    xwork_tool_context tCtx;
    xwork_tool_output tOut;
    size_t iSize = 0u;
    char* sFile = NULL;

    (void)xrtDirRemoveAll(sWorkspace);
    CHECK(xrtDirCreateAll((str)sWorkspace), "edit workspace created");
    xllmSessionConfigInit(&tSessionConfig);
    pSession = xllmSessionCreate(&tSessionConfig, NULL);
    xworkAgentConfigInit(&tAgentConfig);
    tAgentConfig.pSession = pSession;
    tAgentConfig.sWorkspaceRoot = sWorkspace;
    pAgent = xworkAgentCreate(&tAgentConfig, &tError);
    CHECK(pSession && pAgent, "edit fixture agent creates");
    pEditTool = pAgent ? xwork__find_tool(pAgent, "edit") : NULL;
    pWriteTool = pAgent ? xwork__find_tool(pAgent, "write") : NULL;
    pReadTool = pAgent ? xwork__find_tool(pAgent, "read") : NULL;
    memset(&tCtx, 0, sizeof(tCtx));
    tCtx.pAgent = pAgent;
    tCtx.sWorkspaceRoot = sWorkspace;

    /* Write with CRLF content into a fresh file; AUTO stores LF (new file). */
    xworkToolOutputInit(&tOut);
    CHECK(pWriteTool && pWriteTool->OnExecute(pWriteTool->pUserData, &tCtx,
        "{\"path\":\"a/b/c.txt\",\"content\":\"alpha\\r\\nbeta\\r\\n gamma delta\\r\\n\",\"mode\":\"create\"}",
        &tOut, &tError) == XWORK_RESULT_OK && tOut.bSuccess &&
        tOut.sContent && strstr(tOut.sContent, "created parent directories"),
        "write auto-creates parents and reports them");
    xworkToolOutputUnit(&tOut);
    sFile = (char*)xrtFileReadAll("tests/tmp_xwork_edit/a/b/c.txt", &iSize);
    CHECK(sFile && memchr(sFile, '\r', iSize) == NULL, "AUTO stores new files in pure LF");
    xrtFree(sFile); sFile = NULL;

    /* Batch edit: two disjoint edits in one atomic call. */
    xworkToolOutputInit(&tOut);
    CHECK(pEditTool && pEditTool->OnExecute(pEditTool->pUserData, &tCtx,
        "{\"path\":\"a/b/c.txt\",\"edits\":[{\"old_text\":\"alpha\",\"new_text\":\"ALPHA\"},{\"old_text\":\"gamma\",\"new_text\":\"GAMMA\"}]}",
        &tOut, &tError) == XWORK_RESULT_OK && tOut.bSuccess &&
        tOut.sContent && strstr(tOut.sContent, "applied 2 edits"),
        "edit applies a disjoint batch atomically");
    xworkToolOutputUnit(&tOut);

    /* replace_all replaces every occurrence in one edit. */
    xworkToolOutputInit(&tOut);
    CHECK(pEditTool && pEditTool->OnExecute(pEditTool->pUserData, &tCtx,
        "{\"path\":\"a/b/c.txt\",\"edits\":[{\"old_text\":\"a\",\"new_text\":\"-\",\"replace_all\":true}]}",
        &tOut, &tError) == XWORK_RESULT_OK && tOut.bSuccess,
        "replace_all edits every occurrence");
    xworkToolOutputUnit(&tOut);

    /* FORCE_CRLF: the same file round-trips with CRLF storage while the
     * model keeps editing in LF. */
    if ( pAgent ) pAgent->eEolPolicy = XWORK_EOL_FORCE_CRLF;
    xworkToolOutputInit(&tOut);
    CHECK(pEditTool && pEditTool->OnExecute(pEditTool->pUserData, &tCtx,
        "{\"path\":\"a/b/c.txt\",\"edits\":[{\"old_text\":\"ALPHA\",\"new_text\":\"FINAL\"}]}",
        &tOut, &tError) == XWORK_RESULT_OK && tOut.bSuccess,
        "FORCE_CRLF edit matches in LF space");
    xworkToolOutputUnit(&tOut);
    sFile = (char*)xrtFileReadAll("tests/tmp_xwork_edit/a/b/c.txt", &iSize);
    CHECK(sFile && strstr(sFile, "\r\n") != NULL, "FORCE_CRLF stores CRLF");
    xrtFree(sFile); sFile = NULL;
    /* Read still shows LF to the model. */
    xworkToolOutputInit(&tOut);
    CHECK(pReadTool && pReadTool->OnExecute(pReadTool->pUserData, &tCtx,
        "{\"path\":\"a/b/c.txt\"}", &tOut, &tError) == XWORK_RESULT_OK &&
        tOut.bSuccess && tOut.sContent && strstr(tOut.sContent, "FINAL") &&
        strstr(tOut.sContent, "\r\n") == NULL,
        "read normalizes display to LF under any policy");
    xworkToolOutputUnit(&tOut);
    if ( pAgent ) pAgent->eEolPolicy = XWORK_EOL_AUTO;

    xworkAgentDestroy(pAgent);
    xllmSessionDestroy(pSession);
    (void)xrtDirRemoveAll(sWorkspace);
}

static void test_command_context_deadline(void)
{
    static const char sWorkspace[] = "tests/tmp_xwork_command_deadline";
    xllm_session_config tSessionConfig;
    xllm_session* pSession = NULL;
    xllm_error tLlmError;
    xwork_agent_config tAgentConfig;
    xwork_agent* pAgent = NULL;
    xwork_error tError;
    mock_model tMock;
    const xwork_tool_entry* pExecTool;
    xwork_tool_context tToolContext;
    xwork_tool_output tOutput;
    xwork_result eResult = XWORK_RESULT_ERROR;
    uint64_t uStartedMs;
    uint64_t uElapsedMs;
    memset(&tMock, 0, sizeof(tMock));
    memset(&tToolContext, 0, sizeof(tToolContext));
    (void)xrtDirRemoveAll(sWorkspace);
    CHECK(xrtDirCreateAll((str)sWorkspace), "command deadline test workspace created");
    xllmSessionConfigInit(&tSessionConfig);
    pSession = xllmSessionCreate(&tSessionConfig, &tLlmError);
    xworkAgentConfigInit(&tAgentConfig);
    tAgentConfig.pSession = pSession;
    tAgentConfig.uDeadline = xrtDeadlineAfter(UINT64_C(300000));
    tAgentConfig.sWorkspaceRoot = sWorkspace;
    tAgentConfig.OnModelComplete = mock_complete;
    tAgentConfig.pModelUserData = &tMock;
    pAgent = xworkAgentCreate(&tAgentConfig, &tError);
    pExecTool = pAgent ? xwork__find_tool(pAgent, "exec") : NULL;
    tToolContext.pAgent = pAgent;
    tToolContext.sWorkspaceRoot = sWorkspace;
    xworkToolOutputInit(&tOutput);
    uStartedMs = xrtClock() / UINT64_C(1000);
#if defined(_WIN32)
    if ( pExecTool ) eResult = pExecTool->OnExecute(pExecTool->pUserData, &tToolContext,
        "{\"argv\":[\"ping\",\"-n\",\"6\",\"127.0.0.1\"],\"timeout_ms\":5000}", &tOutput, &tError);
#else
    if ( pExecTool ) eResult = pExecTool->OnExecute(pExecTool->pUserData, &tToolContext,
        "{\"argv\":[\"sleep\",\"5\"],\"timeout_ms\":5000}", &tOutput, &tError);
#endif
    uElapsedMs = xrtClock() / UINT64_C(1000) - uStartedMs;
    CHECK(pAgent && pExecTool && eResult == XWORK_RESULT_TIMEOUT &&
        tError.eCode == XWORK_ERROR_TIMEOUT && uElapsedMs < 3000u,
        "operation deadline interrupts a long command without waiting for tool timeout");
    xworkToolOutputUnit(&tOutput);
    xworkAgentDestroy(pAgent);
    xllmSessionDestroy(pSession);
    (void)xrtDirRemoveAll(sWorkspace);
}

/* ------------------------------------------------------------------ */
/* Executor adapter: the registry behind the xllm contract, and the    */
/* flagship T1 chain (session test driver -> RunWithTools -> executor)  */
/* with the built-in xwork loop completely out of the picture.          */
/* ------------------------------------------------------------------ */

static void test_executor_bind(void)
{
    static const char sWorkspace[] = "tests/tmp_xwork_exec";
    xllm_session_config tSessionConfig;
    xllm_session* pSession = NULL;
    xwork_agent_config tAgentConfig;
    xwork_agent* pAgent = NULL;
    xwork_error tError;
    xllm_executor tExecutor;
    xllm_request tRequest;
    xllm_tool_call tCall;
    xllm_executor_ctx tCtx;
    xllm_executor_result tOut;

    (void)xrtDirRemoveAll(sWorkspace);
    CHECK(xrtDirCreateAll((str)sWorkspace), "executor test workspace created");
    xllmSessionConfigInit(&tSessionConfig);
    pSession = xllmSessionCreate(&tSessionConfig, NULL);
    xworkAgentConfigInit(&tAgentConfig);
    tAgentConfig.pSession = pSession;
    tAgentConfig.sWorkspaceRoot = sWorkspace;
    /* No client and no model callback: an executor-only agent must create. */
    pAgent = xworkAgentCreate(&tAgentConfig, &tError);
    CHECK(pSession && pAgent && xworkAgentToolCount(pAgent) == 9u,
        "executor host agent carries the builtin registry");
    memset(&tExecutor, 0, sizeof(tExecutor));
    CHECK(pAgent && xworkExecutorBind(&tExecutor, pAgent, &tError),
        "executor binding attaches");

    xllmRequestInit(&tRequest);
    CHECK(tExecutor.pListTools && tExecutor.pListTools(tExecutor.pUserData, &tRequest) &&
        tRequest.iToolCount == 9u,
        "executor lists the builtin registry into a request");
    xllmRequestUnit(&tRequest);

    memset(&tCall, 0, sizeof(tCall));
    tCall.sId = (char*)"exec-1";
    tCall.sName = (char*)"write";
    tCall.sArgumentsJson = (char*)
        "{\"path\":\"note.txt\",\"content\":\"executor wrote this\",\"mode\":\"create\"}";
    memset(&tCtx, 0, sizeof(tCtx));
    tCtx.uRound = 1u;
    tCtx.uDeadline = XRT_DEADLINE_NEVER;
    memset(&tOut, 0, sizeof(tOut));
    CHECK(tExecutor.pExecute && tExecutor.pExecute(tExecutor.pUserData, &tCall, &tCtx, &tOut) &&
        tOut.bSuccess && tOut.sContent && strstr(tOut.sContent, "status: success"),
        "executor runs write_file through the full policy path");

    tCall.sName = (char*)"definitely_not_a_tool";
    memset(&tOut, 0, sizeof(tOut));
    CHECK(tExecutor.pExecute(tExecutor.pUserData, &tCall, &tCtx, &tOut) &&
        !tOut.bSuccess && strstr(tOut.sContent, "unknown tool name"),
        "unknown tools become tool-level failures, not run failures");

    CHECK(xrtFileExists((str)"tests/tmp_xwork_exec/note.txt"),
        "executor side effect landed in the workspace");
    xworkExecutorUnbind(&tExecutor);
    xworkAgentDestroy(pAgent);
    xllmSessionDestroy(pSession);
    (void)xrtDirRemoveAll(sWorkspace);
}

typedef struct {
    unsigned iCalls;
    bool bSawTools;
    bool bFinalOnly;
} t1_script;

static xllm_response* t1_response_text(const char* sText)
{
    xllm_response* pResponse = (xllm_response*)calloc(1u, sizeof(*pResponse));
    if ( !pResponse ) { return NULL; }
    pResponse->sContent = test_strdup(sText);
    pResponse->sFinishReason = test_strdup("stop");
    if ( !pResponse->sContent ) { xllmResponseDestroy(pResponse); return NULL; }
    return pResponse;
}

static xllm_response* t1_response_write(void)
{
    xllm_response* pResponse = t1_response_text("");
    if ( !pResponse ) { return NULL; }
    pResponse->pToolCalls = (xllm_tool_call*)calloc(1u, sizeof(*pResponse->pToolCalls));
    if ( !pResponse->pToolCalls ) { xllmResponseDestroy(pResponse); return NULL; }
    pResponse->pToolCalls[0].sId = test_strdup("t1-call-1");
    pResponse->pToolCalls[0].sName = test_strdup("write");
    pResponse->pToolCalls[0].sArgumentsJson = test_strdup(
        "{\"path\":\"t1.txt\",\"content\":\"t1 chain worked\",\"mode\":\"create\"}");
    pResponse->iToolCallCount = 1u;
    free(pResponse->sFinishReason);
    pResponse->sFinishReason = test_strdup("tool_calls");
    if ( !pResponse->pToolCalls[0].sId || !pResponse->pToolCalls[0].sName ||
         !pResponse->pToolCalls[0].sArgumentsJson || !pResponse->sFinishReason ) {
        xllmResponseDestroy(pResponse);
        return NULL;
    }
    return pResponse;
}

/* ------------------------------------------------------------------ */
/* Subagent delegation: archetype registry, foreground and background   */
/* `agent` tool runs, depth lock, model override, notices.              */
/* ------------------------------------------------------------------ */

typedef struct {
    unsigned iCalls;
    char sLastModel[64];
    bool bSawDelegationPrompt;
} delegate_script;

static xllm_response* delegate_response_text(const char* sText)
{
    return t1_response_text(sText);
}

static xllm_result delegate_model_call(void* pUserData, const xllm_request* pRequest,
    const xllm_stream_callbacks* pCallbacks, xllm_response** ppResponse, xllm_error* pError)
{
    delegate_script* pScript = (delegate_script*)pUserData;
    (void)pCallbacks;
    if ( pError ) { xllmErrorInit(pError); }
    ++pScript->iCalls;
    if ( pRequest->sModel ) {
        (void)snprintf(pScript->sLastModel, sizeof(pScript->sLastModel), "%s", pRequest->sModel);
    } else {
        pScript->sLastModel[0] = '\0';
    }
    {
        size_t i;
        for ( i = 0u; i < pRequest->iMessageCount; ++i ) {
            const xllm_message* pMessage = &pRequest->pMessages[i];
            if ( pMessage->eRole == XLLM_ROLE_USER && pMessage->sContent &&
                 strstr(pMessage->sContent, "probe the workspace") ) {
                pScript->bSawDelegationPrompt = true;
            }
            if ( pMessage->eRole == XLLM_ROLE_SYSTEM && pMessage->sContent &&
                 strstr(pMessage->sContent, "You are a fast read-only inspector.") ) {
                /* identity reached the child */
            }
        }
    }
    *ppResponse = delegate_response_text("probe complete: 3 files, no blockers");
    return *ppResponse ? XLLM_RESULT_OK : XLLM_RESULT_ERROR;
}

/* ------------------------------------------------------------------ */
/* Image passthrough: read detects an image, the executor forwards the  */
/* payload, the session records it as an IMAGE part on the tool result. */
/* ------------------------------------------------------------------ */

static void test_image_passthrough(void)
{
    static const char sWorkspace[] = "tests/tmp_xwork_image";
    static const unsigned char sPng[] = {
        0x89u, 'P', 'N', 'G', 0x0Du, 0x0Au, 0x1Au, 0x0Au,
        0x00u, 0x00u, 0x00u, 0x0Du, 'I', 'H', 'D', 'R',
        0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u,
        0x08u, 0x06u, 0x00u, 0x00u, 0x00u
    };
    xllm_session_config tSessionConfig;
    xllm_session* pSession = NULL;
    xwork_agent_config tAgentConfig;
    xwork_agent* pAgent = NULL;
    xwork_error tError;
    const xwork_tool_entry* pReadTool;
    xwork_tool_context tCtx;
    xwork_tool_output tOut;
    xllm_executor tExecutor;
    size_t i;

    (void)xrtDirRemoveAll(sWorkspace);
    CHECK(xrtDirCreateAll((str)sWorkspace), "image workspace created");
    CHECK(xrtFileWriteAtomic("tests/tmp_xwork_image/pic.dat",
            (xbytesview){ sPng, sizeof(sPng) }), "png fixture written");

    xllmSessionConfigInit(&tSessionConfig);
    pSession = xllmSessionCreate(&tSessionConfig, NULL);
    xworkAgentConfigInit(&tAgentConfig);
    tAgentConfig.pSession = pSession;
    tAgentConfig.sWorkspaceRoot = sWorkspace;
    pAgent = xworkAgentCreate(&tAgentConfig, &tError);
    pReadTool = pAgent ? xwork__find_tool(pAgent, "read") : NULL;
    memset(&tCtx, 0, sizeof(tCtx));
    tCtx.pAgent = pAgent;
    tCtx.sWorkspaceRoot = sWorkspace;

    /* read detects the image by magic and returns the payload. */
    xworkToolOutputInit(&tOut);
    CHECK(pReadTool && pReadTool->OnExecute(pReadTool->pUserData, &tCtx,
        "{\"path\":\"pic.dat\"}", &tOut, &tError) == XWORK_RESULT_OK &&
        tOut.bSuccess && tOut.sContent && strstr(tOut.sContent, "image/png") &&
        tOut.pImageBytes && tOut.iImageSize == sizeof(sPng) &&
        memcmp(tOut.pImageBytes, sPng, sizeof(sPng)) == 0,
        "read detects the png magic and attaches the payload");
    xworkToolOutputUnit(&tOut);
    /* Extension lies: a text file named .png stays text. */
    CHECK(xrtFileWriteAtomic("tests/tmp_xwork_image/fake.png",
            (xbytesview){ (const uint8*)"plain text", 10u }), "fake png written");
    xworkToolOutputInit(&tOut);
    CHECK(pReadTool && pReadTool->OnExecute(pReadTool->pUserData, &tCtx,
        "{\"path\":\"fake.png\"}", &tOut, &tError) == XWORK_RESULT_OK &&
        tOut.bSuccess && !tOut.pImageBytes && strstr(tOut.sContent, "plain text"),
        "a lying extension does not trigger passthrough");
    xworkToolOutputUnit(&tOut);

    /* Full chain: executor forwards the image; the session tool message
     * carries it as an IMAGE part. */
    if ( pAgent && xworkExecutorBind(&tExecutor, pAgent, &tError) ) {
        xllm_tool_call tCall;
        xllm_executor_ctx tECtx;
        xllm_executor_result tEResult;
        memset(&tCall, 0, sizeof(tCall));
        tCall.sId = (char*)"img-1";
        tCall.sName = (char*)"read";
        tCall.sArgumentsJson = (char*)"{\"path\":\"pic.dat\"}";
        memset(&tECtx, 0, sizeof(tECtx));
        memset(&tEResult, 0, sizeof(tEResult));
        CHECK(tExecutor.pExecute(tExecutor.pUserData, &tCall, &tECtx, &tEResult) &&
            tEResult.pImageBytes && tEResult.iImageSize == sizeof(sPng) &&
            tEResult.sImageMime && strcmp(tEResult.sImageMime, "image/png") == 0,
            "executor forwards the image payload with mime");
        {
            uint64_t uTurn = xllmSessionBeginTurn(pSession);
            xllm_tool_call tPair;
            xllm_response tPairResponse;
            memset(&tPair, 0, sizeof(tPair));
            tPair.sId = (char*)"img-1";
            tPair.sName = (char*)"read";
            tPair.sArgumentsJson = (char*)"{\"path\":\"pic.dat\"}";
            memset(&tPairResponse, 0, sizeof(tPairResponse));
            tPairResponse.sContent = (char*)"";
            tPairResponse.pToolCalls = &tPair;
            tPairResponse.iToolCallCount = 1u;
            CHECK(xllmSessionAddAssistantResponse(pSession, uTurn, &tPairResponse) &&
                xllmSessionPendingToolCallCount(pSession) == 1u,
                "image fixture call is pending before the result");
            CHECK(xllmSessionAddToolResultWithImage(pSession, uTurn, "img-1",
                tEResult.sContent, tEResult.pImageBytes, tEResult.iImageSize,
                tEResult.sImageMime), "image tool result enters the ledger");
            CHECK(xllmSessionPendingToolCallCount(pSession) == 0u,
                "the image result resolves its pending call");
        }
        xworkExecutorUnbind(&tExecutor);
    }
    else {
        CHECK(false, "executor binding for image chain");
    }

    (void)i;
    xworkAgentDestroy(pAgent);
    xllmSessionDestroy(pSession);
    (void)xrtDirRemoveAll(sWorkspace);
}

static void test_subagent_delegation(void)
{
    static const char sWorkspace[] = "tests/tmp_xwork_delegation";
    xllm_session_config tSessionConfig;
    xllm_session* pSession = NULL;
    xwork_agent_config tAgentConfig;
    xwork_agent* pAgent = NULL;
    xwork_error tError;
    xllm_error tLlmError;
    const xwork_tool_entry* pAgentTool;
    xwork_tool_context tCtx;
    xwork_tool_output tOut;
    delegate_script tScript;
    xwork_subagent_type tType;
    const char* sTools[2];
    xwork_task_notice tNotices[2];

    (void)xrtDirRemoveAll(sWorkspace);
    CHECK(xrtDirCreateAll((str)sWorkspace), "delegation workspace created");
    memset(&tScript, 0, sizeof(tScript));
    xllmSessionConfigInit(&tSessionConfig);
    pSession = xllmSessionCreateForTest(&tSessionConfig, delegate_model_call, &tScript, &tLlmError);
    xworkAgentConfigInit(&tAgentConfig);
    tAgentConfig.pSession = pSession;
    tAgentConfig.sWorkspaceRoot = sWorkspace;
    pAgent = xworkAgentCreate(&tAgentConfig, &tError);
    CHECK(pSession && pAgent, "delegation fixture agent creates");
    CHECK(pAgent && xworkAgentSubagentTypeCount(pAgent) == 0u &&
        xwork__find_tool(pAgent, "agent") == NULL,
        "agent tool is absent before any type is registered");

    memset(&tType, 0, sizeof(tType));
    tType.sName = "probe";
    tType.sDescription = "fast read-only inspection; returns a short report";
    tType.sSystemPrompt = "You are a fast read-only inspector. Report findings only.";
    sTools[0] = "read";
    tType.psTools = sTools;
    tType.iToolCount = 1u;
    tType.sModel = "ornith-35b";
    tType.uMaxTurns = 4u;
    tType.uTimeoutMs = 10000u;
    tType.iMaxFinalBytes = 4096u;
    tType.bReadOnly = true;
    CHECK(pAgent && xworkAgentRegisterSubagentType(pAgent, &tType, &tError),
        "subagent type registers");
    pAgentTool = pAgent ? xwork__find_tool(pAgent, "agent") : NULL;
    CHECK(pAgentTool && pAgentTool->sDescription &&
        strstr(pAgentTool->sDescription, "probe") != NULL &&
        strstr(pAgentTool->sDescription, "fast read-only inspection") != NULL,
        "agent tool appears with the roster woven into its description");
    CHECK(!xworkAgentRegisterSubagentType(pAgent, &tType, &tError),
        "duplicate type names are rejected");

    memset(&tCtx, 0, sizeof(tCtx));
    tCtx.pAgent = pAgent;
    tCtx.sWorkspaceRoot = sWorkspace;
    tCtx.uAgentTurn = 7u;

    /* Foreground delegation: blocking, returns the final report. */
    xworkToolOutputInit(&tOut);
    CHECK(pAgentTool && pAgentTool->OnExecute(pAgentTool->pUserData, &tCtx,
        "{\"name\":\"probe\",\"prompt\":\"probe the workspace and report\"}",
        &tOut, &tError) == XWORK_RESULT_OK && tOut.bSuccess &&
        tOut.sContent && strstr(tOut.sContent, "probe complete: 3 files") &&
        strstr(tOut.sContent, "final report"),
        "foreground delegation returns the child's final report");
    CHECK(tScript.iCalls >= 1u && tScript.bSawDelegationPrompt,
        "the child saw the delegation prompt in a fresh session");
    CHECK(strcmp(tScript.sLastModel, "ornith-35b") == 0,
        "the type's model override reaches every child request");
    xworkToolOutputUnit(&tOut);

    /* Unknown type: tool-level failure. */
    xworkToolOutputInit(&tOut);
    CHECK(pAgentTool && pAgentTool->OnExecute(pAgentTool->pUserData, &tCtx,
        "{\"name\":\"nonexistent\",\"prompt\":\"x\"}", &tOut, &tError) == XWORK_RESULT_OK &&
        !tOut.bSuccess && tOut.sContent && strstr(tOut.sContent, "unknown subagent type"),
        "unknown roster names fail at tool level");
    xworkToolOutputUnit(&tOut);

    /* Background delegation: task table entry + notice delivery. */
    xworkToolOutputInit(&tOut);
    CHECK(pAgentTool && pAgentTool->OnExecute(pAgentTool->pUserData, &tCtx,
        "{\"name\":\"probe\",\"prompt\":\"probe the workspace again\",\"background\":true,"
        "\"notify\":\"probe done; fold the report in\"}",
        &tOut, &tError) == XWORK_RESULT_OK && tOut.bSuccess &&
        tOut.sContent && strstr(tOut.sContent, "task_id:"),
        "background delegation returns a task id");
    xworkToolOutputUnit(&tOut);
    {
        xwork_process_entry* pEntry = pAgent ? xwork__process_find(pAgent, 1u, NULL) : NULL;
        size_t i;
        bool bDone = false;
        for ( i = 0u; i < 2000u && pEntry && !bDone; ++i ) {
            bDone = !xwork__task_running(pEntry);
            if ( !bDone ) xrtSleep(5u);
        }
        CHECK(pEntry && bDone, "background delegation finishes");
        CHECK(pAgent && xworkAgentTakeTaskNotices(pAgent, tNotices, 2u) == 1u &&
            tNotices[0].eKind == XWORK_TASK_AGENT && tNotices[0].bExitedCleanly &&
            tNotices[0].sNotify && strcmp(tNotices[0].sNotify, "probe done; fold the report in") == 0 &&
            tNotices[0].sPreview && strstr(tNotices[0].sPreview, "probe complete"),
            "agent-task notice delivers the notify message and report preview");
    }

    /* Depth lock: from inside a delegation the agent tool refuses. We
     * simulate depth by running the tool through a child agent context —
     * simplest proof: a registered child copy would carry uAgentDepth=1;
     * here we verify the parent-level guard text via a manual depth set. */
    if ( pAgent ) {
        const xwork_tool_entry* pTool = xwork__find_tool(pAgent, "agent");
        pAgent->uAgentDepth = 1u;
        xworkToolOutputInit(&tOut);
        CHECK(pTool && pTool->OnExecute(pTool->pUserData, &tCtx,
            "{\"name\":\"probe\",\"prompt\":\"nested\"}", &tOut, &tError) == XWORK_RESULT_OK &&
            !tOut.bSuccess && strstr(tOut.sContent, "depth lock"),
            "depth lock refuses nested delegation");
        xworkToolOutputUnit(&tOut);
        pAgent->uAgentDepth = 0u;
    }

    /* Unregister: tool disappears when the roster empties. */
    CHECK(pAgent && xworkAgentUnregisterSubagentType(pAgent, "probe", &tError) &&
        xwork__find_tool(pAgent, "agent") == NULL &&
        xworkAgentSubagentTypeCount(pAgent) == 0u,
        "unregister removes the agent tool with the roster");

    xworkAgentDestroy(pAgent);
    xllmSessionDestroy(pSession);
    (void)xrtDirRemoveAll(sWorkspace);
}

static xllm_result t1_script_call(void* pUserData, const xllm_request* pRequest,
    const xllm_stream_callbacks* pCallbacks, xllm_response** ppResponse, xllm_error* pError)
{
    t1_script* pScript = (t1_script*)pUserData;
    (void)pCallbacks;
    if ( pError ) { xllmErrorInit(pError); }
    ++pScript->iCalls;
    if ( pRequest->iToolCount == 9u ) { pScript->bSawTools = true; }
    *ppResponse = ( pScript->iCalls == 1u && !pScript->bFinalOnly )
        ? t1_response_write() : t1_response_text("t1 finished");
    return *ppResponse ? XLLM_RESULT_OK : XLLM_RESULT_ERROR;
}

static bool durable_stop_guard(xllm_session* pSession, uint32_t uRound,
    const xllm_response* pResponse, size_t iPendingToolCalls, void* pUserData)
{
    (void)pSession; (void)uRound; (void)pResponse;
    (void)iPendingToolCalls; (void)pUserData;
    return false; /* simulate a crash right after the tool round */
}

static void test_session_run_with_tools_chain(void)
{
    static const char sWorkspace[] = "tests/tmp_xwork_t1";
    xllm_session_config tSessionConfig;
    xllm_session* pSession = NULL;
    xwork_agent_config tAgentConfig;
    xwork_agent* pAgent = NULL;
    xwork_error tError;
    xllm_error tLlmError;
    xllm_executor tExecutor;
    xllm_run_summary tSummary;
    t1_script tScript;

    (void)xrtDirRemoveAll(sWorkspace);
    CHECK(xrtDirCreateAll((str)sWorkspace), "t1 workspace created");
    memset(&tScript, 0, sizeof(tScript));
    xllmSessionConfigInit(&tSessionConfig);
    pSession = xllmSessionCreateForTest(&tSessionConfig, t1_script_call, &tScript, &tLlmError);
    xworkAgentConfigInit(&tAgentConfig);
    tAgentConfig.pSession = pSession;
    tAgentConfig.sWorkspaceRoot = sWorkspace;
    pAgent = xworkAgentCreate(&tAgentConfig, &tError);
    CHECK(pSession && pAgent && xworkExecutorBind(&tExecutor, pAgent, &tError),
        "t1 chain assembles: test session + agent + executor");
    memset(&tSummary, 0, sizeof(tSummary));
    CHECK(pSession && xllmSessionRunWithTools(pSession, "create t1.txt",
        &tExecutor, NULL, NULL, &tSummary, &tLlmError) == XLLM_RESULT_OK,
        "t1 bounded run completes without the xwork loop");
    CHECK(tScript.iCalls == 2u && tScript.bSawTools &&
        tSummary.uRounds == 2u && tSummary.uToolCalls == 1u &&
        tSummary.sFinalText && strcmp(tSummary.sFinalText, "t1 finished") == 0,
        "t1 summary: two rounds, one executed tool, final text");
    CHECK(xrtFileExists((str)"tests/tmp_xwork_t1/t1.txt"),
        "t1 write_file side effect landed");
    CHECK(xllmSessionPendingToolCallCount(pSession) == 0u,
        "t1 chain leaves no pending calls");
    xllmRunSummaryUnit(&tSummary);
    xworkExecutorUnbind(&tExecutor);
    xworkAgentDestroy(pAgent);
    xllmSessionDestroy(pSession);
    (void)xrtDirRemoveAll(sWorkspace);
}

static void test_run_window_and_durable_recovery(void)
{
    static const char sWorkspace[] = "tests/tmp_xwork_durable";
    static const char sSnapshot[] = "tests/tmp_xwork_durable/state.json";
    static const char sJournal[] = "tests/tmp_xwork_durable/journal.jsonl";
    xllm_session_config tSessionConfig;
    xllm_session* pSession = NULL;
    xwork_agent_config tAgentConfig;
    xwork_agent* pAgent = NULL;
    xwork_error tError;
    xllm_error tLlmError;
    xllm_executor tExecutor;
    xllm_run_policy tPolicy;
    xllm_run_summary tSummary;
    t1_script tScript;
    xwork_tool_definition tDynamic;
    xwork_run_result tResult;

    (void)xrtDirRemoveAll(sWorkspace);
    CHECK(xrtDirCreateAll((str)sWorkspace), "durable workspace created");

    /* Run window: registry mutation and re-entry are rejected while open. */
    memset(&tScript, 0, sizeof(tScript));
    xllmSessionConfigInit(&tSessionConfig);
    pSession = xllmSessionCreateForTest(&tSessionConfig, t1_script_call, &tScript, &tLlmError);
    xworkAgentConfigInit(&tAgentConfig);
    tAgentConfig.pSession = pSession;
    tAgentConfig.sWorkspaceRoot = sWorkspace;
    pAgent = xworkAgentCreate(&tAgentConfig, &tError);
    memset(&tExecutor, 0, sizeof(tExecutor));
    CHECK(pSession && pAgent && xworkExecutorBind(&tExecutor, pAgent, &tError),
        "durable fixture assembles");
    memset(&tDynamic, 0, sizeof(tDynamic));
    tDynamic.sName = "window_probe";
    tDynamic.sDescription = "mutation probe";
    tDynamic.sParametersJson = "{\"type\":\"object\"}";
    tDynamic.eEffect = XWORK_TOOL_EFFECT_READ_ONLY;
    tDynamic.OnExecute = registry_probe_execute;
    CHECK(pAgent && xworkAgentRunBegin(pAgent, &tError) &&
        !xworkAgentRegisterTool(pAgent, &tDynamic, &tError) &&
        tError.eCode == XWORK_ERROR_CONTEXT &&
        !xworkAgentRunBegin(pAgent, &tError),
        "run window rejects registry mutation and re-entry");
    xworkAgentRunEnd(pAgent);
    CHECK(pAgent && xworkAgentRegisterTool(pAgent, &tDynamic, &tError),
        "registry mutation works after the window closes");
    memset(&tResult, 0, sizeof(tResult));
    CHECK(pAgent && xworkAgentRun(pAgent, "no boundary", &tResult, &tError) == XWORK_RESULT_ERROR &&
        tError.eCode == XWORK_ERROR_INVALID_ARGUMENT,
        "built-in loop refuses an executor-only agent without a boundary");
    xworkAgentUnregisterTool(pAgent, "window_probe", &tError);

    /* Durable chain: journal on -> run stopped mid-batch (pending tool call)
     * -> Save (snapshot keeps the journal) -> destroy (crash) -> Recover ->
     * SetTestCall -> RunWithTools(NULL) drains and finishes, no new prompt. */
    CHECK(pSession && xllmSessionEnableJournal(pSession, sJournal, &tLlmError),
        "journal attaches to the fixture session");
    xllmRunPolicyInit(&tPolicy);
    tPolicy.pOnRound = durable_stop_guard; /* stop after the tool round */
    memset(&tSummary, 0, sizeof(tSummary));
    CHECK(pSession && xllmSessionRunWithTools(pSession, "durable task", &tExecutor,
        NULL, &tPolicy, &tSummary, &tLlmError) == XLLM_RESULT_OK &&
        tSummary.bStoppedByPolicy,
        "durable run stops with a pending tool call");
    CHECK(pSession && xllmSessionPendingToolCallCount(pSession) == 1u,
        "crash fixture leaves exactly one pending call");
    CHECK(pSession && xllmSessionSave(pSession, sSnapshot, &tLlmError),
        "snapshot saves before the simulated crash");
    xworkExecutorUnbind(&tExecutor);
    xworkAgentDestroy(pAgent);
    pAgent = NULL;
    xllmSessionDestroy(pSession);
    pSession = NULL;   /* crash: everything above is gone */

    memset(&tScript, 0, sizeof(tScript));
    tScript.bFinalOnly = true;   /* the resumed model round answers directly */
    pSession = xllmSessionRecover(sSnapshot, sJournal, NULL, &tLlmError);
    CHECK(pSession != NULL, "session recovers from snapshot plus journal");
    CHECK(pSession && xllmSessionSetTestCall(pSession, t1_script_call, &tScript),
        "recovered session binds a fresh model driver");
    xworkAgentConfigInit(&tAgentConfig);
    tAgentConfig.pSession = pSession;
    tAgentConfig.sWorkspaceRoot = sWorkspace;
    pAgent = xworkAgentCreate(&tAgentConfig, &tError);
    memset(&tExecutor, 0, sizeof(tExecutor));
    CHECK(pSession && pAgent && xworkExecutorBind(&tExecutor, pAgent, &tError),
        "post-crash fixture reassembles");
    memset(&tSummary, 0, sizeof(tSummary));
    CHECK(pSession && xllmSessionRunWithTools(pSession, NULL, &tExecutor,
        NULL, NULL, &tSummary, &tLlmError) == XLLM_RESULT_OK &&
        tSummary.sFinalText && strcmp(tSummary.sFinalText, "t1 finished") == 0,
        "resumed run drains the pending call and finishes");
    CHECK(xrtFileExists((str)"tests/tmp_xwork_durable/t1.txt"),
        "the pending write landed after recovery");
    CHECK(pSession && xllmSessionPendingToolCallCount(pSession) == 0u,
        "recovery leaves no pending calls");
    xllmRunSummaryUnit(&tSummary);
    xworkExecutorUnbind(&tExecutor);
    xworkAgentDestroy(pAgent);
    xllmSessionDestroy(pSession);
    (void)xrtDirRemoveAll(sWorkspace);
}

static int run_mcp_test_server(void)
{
    char sLine[65536];
    while ( fgets(sLine, sizeof(sLine), stdin) ) {
        const char* sIdText = strstr(sLine, "\"id\":");
        unsigned long long uId = sIdText ? strtoull(sIdText + 5u, NULL, 10) : 0u;
        if ( strstr(sLine, "\"method\":\"initialize\"") ) {
            printf("{\"jsonrpc\":\"2.0\",\"id\":%llu,\"result\":{\"protocolVersion\":\"2025-06-18\",\"capabilities\":{\"tools\":{\"listChanged\":false}},\"serverInfo\":{\"name\":\"xwork-test-mcp\",\"version\":\"1.0\"}}}\n", uId);
            fflush(stdout);
        } else if ( strstr(sLine, "\"method\":\"tools/list\"") ) {
            printf("{\"jsonrpc\":\"2.0\",\"id\":%llu,\"result\":{\"tools\":[{\"name\":\"echo\",\"description\":\"Echo test text.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\"}},\"required\":[\"text\"],\"additionalProperties\":false},\"annotations\":{\"readOnlyHint\":true}}]}}\n", uId);
            fflush(stdout);
        } else if ( strstr(sLine, "\"method\":\"tools/call\"") ) {
            if ( strstr(sLine, "\"delay\":true") ) xrtSleep(3000u);
            printf("{\"jsonrpc\":\"2.0\",\"id\":%llu,\"result\":{\"content\":[{\"type\":\"text\",\"text\":\"echo: hello mcp\"}],\"structuredContent\":{\"echoed\":true},\"isError\":false}}\n", uId);
            fflush(stdout);
        }
    }
    return ferror(stdin) ? 1 : 0;
}

int main(int argc, char** argv)
{
    if ( argc == 2 && strcmp(argv[1], "--mcp-test-server") == 0 ) {
        return run_mcp_test_server();
    }
    setvbuf(stdout, NULL, _IONBF, 0);
    g_sSelfPath = argc > 0 ? argv[0] : NULL;
    printf("xwork v2 tests\n");
    test_process_text_normalization();
    test_agent_context_deadline();
    test_command_context_deadline();
    test_edit_eol_write();
    test_task_system();
    test_image_passthrough();
    test_subagent_delegation();
    test_executor_bind();
    test_readonly_subagent();
    test_agent_loop();
    test_session_run_with_tools_chain();
    test_run_window_and_durable_recovery();
    printf("xwork v2: %s (%d failures)\n", g_iFailures ? "FAIL" : "PASS", g_iFailures);
    return g_iFailures ? 1 : 0;
}
