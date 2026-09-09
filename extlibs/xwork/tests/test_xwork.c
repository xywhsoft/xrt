#include "../xwork.c"
#include "xllm.c"
#include "xllm-session.c"
#include "xllm-memory.c"

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
    bool bSawRetrievedMemory;
    bool bSawSecretMemory;
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
    uint32_t uMemoryRetrievals;
    uint32_t uMemoryHits;
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
        "The child received only read_file, list_files, and search_text, performed no writes or process execution, and cannot delegate recursively. "
        "Recommended next action: let the parent agent use this bounded evidence while retaining authority for every mutation. "
        "Additional padding verifies that the host byte budget truncates this final response deterministically without creating an artifact.";
    subagent_mock* pMock = (subagent_mock*)pUserData;
    xllm_response* pResponse;
    size_t i;
    bool bNames = pRequest && pRequest->iToolCount == 3u;
    (void)pCallbacks;
    (void)pError;
    *ppResponse = NULL;
    if ( !pRequest || !pMock ) return XLLM_RESULT_ERROR;
    for ( i = 0u; bNames && i < pRequest->iToolCount; ++i ) {
        const char* sName = pRequest->pTools[i].sName;
        if ( strcmp(sName, "read_file") != 0 && strcmp(sName, "list_files") != 0 &&
             strcmp(sName, "search_text") != 0 ) bNames = false;
    }
    pMock->bReadOnlyToolSet = pMock->bReadOnlyToolSet || bNames;
    ++pMock->uModelCalls;
    if ( pMock->uModelCalls == 1u ) {
        pResponse = mock_response("", 2u);
        if ( !pResponse ||
             !mock_set_call(pResponse, 0u, "sub_read", "read_file",
                "{\"path\":\"evidence.txt\",\"max_lines\":100}") ||
             !mock_set_call(pResponse, 1u, "sub_internal", "read_file",
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
                !xworkAgentUnregisterTool(pMock->pAgent, "read_file", &tRegistryError) &&
                tRegistryError.eCode == XWORK_ERROR_CONTEXT;
        }
        ++pMock->uAgentCalls;
        pMock->bSawTools = pRequest->iToolCount == 11u;
        pMock->bSawParallel = pRequest->bParallelToolCalls;
        if ( request_has_text(pRequest, "project-note-convention") &&
             request_has_text(pRequest, "note-verification-command") ) pMock->bSawRetrievedMemory = true;
        if ( request_has_text(pRequest, "never-inject-secret-memory") ) pMock->bSawSecretMemory = true;
        if ( pMock->uAgentCalls > 1u && request_has_role(pRequest, XLLM_ROLE_TOOL, 1u) ) pMock->bSawToolResults = true;
        if ( request_has_text(pRequest, "Objective: test the xwork tool loop after compaction") ) pMock->bSawCompactionSummary = true;
        if ( request_has_text(pRequest, "Completion verification gate") ) pMock->bSawVerificationGate = true;
        if ( pMock->uAgentCalls == 1u ) {
            xwork_buf tArgs = {0};
            pResponse = mock_response("", 6u);
            if ( !pResponse ) return XLLM_RESULT_ERROR;
            if ( !xwork__buf_append_cstr(&tArgs, "{\"path\":\"sandbox/note.txt\",\"content\":\"hello-") ) goto oom;
            for ( i = 0u; i < 1600u; ++i ) if ( !xwork__buf_append_char(&tArgs, (char)('a' + (i % 26u))) ) goto oom;
            if ( !xwork__buf_append_cstr(&tArgs, "\",\"mode\":\"create\",\"create_dirs\":true}") ) goto oom;
            sLargeArgs = xwork__buf_detach(&tArgs);
            if ( !sLargeArgs ||
                 !mock_set_call(pResponse, 0u, "call_write", "write_file", sLargeArgs) ||
                 !mock_set_call(pResponse, 1u, "call_read", "read_file", "{\"path\":\"sandbox/note.txt\",\"max_lines\":20}") ||
                 !mock_set_call(pResponse, 2u, "call_escape", "read_file", "{\"path\":\"../outside.txt\"}") ||
                 !mock_set_call(pResponse, 3u, "call_list", "list_files", "{\"path\":\"sandbox\",\"recursive\":true}") ||
                 !mock_set_call(pResponse, 4u, "call_search", "search_text", "{\"query\":\"hello-\",\"path\":\"sandbox\",\"pattern\":\"*.txt\"}") ||
                 !mock_set_call(pResponse, 5u, "call_replace", "replace_text", "{\"path\":\"sandbox/note.txt\",\"old_text\":\"hello-\",\"new_text\":\"HELLO-\"}") ) goto oom;
        } else if ( pMock->uAgentCalls == 2u ) {
            pResponse = mock_response("", 1u);
            if ( !pResponse || !mock_set_call(pResponse, 0u, "call_patch", "apply_patch",
                    "{\"changes\":[{\"path\":\"sandbox/note.txt\",\"operation\":\"replace\",\"old_text\":\"HELLO-\",\"new_text\":\"PATCHED-\"},{\"path\":\"sandbox/extra.txt\",\"operation\":\"create\",\"content\":\"transaction created this file\\n\"}]}") ) goto oom;
        } else if ( pMock->uAgentCalls == 3u ) {
            pResponse = mock_response("The requested edits are complete.", 0u);
        } else if ( pMock->uAgentCalls == 4u ) {
            #if defined(_WIN32) || defined(_WIN64)
                static const char sVerifyArgs[] = "{\"command\":\"type sandbox\\\\note.txt\",\"timeout_ms\":10000}";
            #else
                static const char sVerifyArgs[] = "{\"command\":\"cat sandbox/note.txt\",\"timeout_ms\":10000}";
            #endif
            pResponse = mock_response("", 1u);
            if ( !pResponse || !mock_set_call(pResponse, 0u, "call_exec", "exec_command",
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
                 pEvent->iMessageCount > 0u && pEvent->iToolDefinitionCount == 11u &&
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
        case XWORK_EVENT_MEMORY_RETRIEVED:
            ++pEvents->uMemoryRetrievals;
            pEvents->uMemoryHits += (uint32_t)pEvent->iMemoryHitCount;
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
    xllm_memory* pMemory = NULL;
    xllm_memory_config tMemoryConfig;
    xllm_memory_record_input tMemoryRecord;
    xllm_memory_receipt tMemoryReceipt;
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
    xllmMemoryConfigInit(&tMemoryConfig);
    tMemoryConfig.sPath = "tests/tmp_xwork/.xcode/memory.json";
    tMemoryConfig.sNamespace = "xwork-test";
    tMemoryConfig.bCreateIfMissing = true;
    pMemory = xllmMemoryOpen(&tMemoryConfig, &tLlmError);
    CHECK(pMemory != NULL, "layered memory store opens");
    xllmMemoryRecordInputInit(&tMemoryRecord);
    tMemoryRecord.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tMemoryRecord.eKind = XLLM_MEMORY_KIND_PREFERENCE;
    tMemoryRecord.eTrust = XLLM_MEMORY_TRUST_USER_APPROVED;
    tMemoryRecord.eSensitivity = XLLM_MEMORY_SENSITIVITY_INTERNAL;
    tMemoryRecord.sRecordId = "project-note-convention";
    tMemoryRecord.sTitle = "note file convention";
    tMemoryRecord.sText = "For requested note file work, use the sandbox directory and verify the final content.";
    tMemoryRecord.sSourceUri = "user://test/convention";
    tMemoryRecord.sActor = "test-user";
    tMemoryRecord.sReason = "approved project convention";
    CHECK(pMemory && xllmMemoryPut(pMemory, &tMemoryRecord, &tMemoryReceipt, &tLlmError),
        "approved memory-layer record is stored");
    xllmMemoryRecordInputInit(&tMemoryRecord);
    tMemoryRecord.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tMemoryRecord.eKind = XLLM_MEMORY_KIND_KNOWLEDGE;
    tMemoryRecord.eTrust = XLLM_MEMORY_TRUST_LOCAL;
    tMemoryRecord.eSensitivity = XLLM_MEMORY_SENSITIVITY_INTERNAL;
    tMemoryRecord.sRecordId = "note-verification-command";
    tMemoryRecord.sTitle = "note file verification";
    tMemoryRecord.sText = "Verify a requested note file by reading it after the final write command succeeds.";
    tMemoryRecord.sSourceUri = "repo://docs/testing";
    tMemoryRecord.sActor = "workspace-indexer";
    tMemoryRecord.sReason = "local project knowledge";
    CHECK(pMemory && xllmMemoryPut(pMemory, &tMemoryRecord, &tMemoryReceipt, &tLlmError),
        "knowledge-layer record is stored");
    xllmMemoryRecordInputInit(&tMemoryRecord);
    tMemoryRecord.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tMemoryRecord.eKind = XLLM_MEMORY_KIND_FACT;
    tMemoryRecord.eTrust = XLLM_MEMORY_TRUST_USER_APPROVED;
    tMemoryRecord.eSensitivity = XLLM_MEMORY_SENSITIVITY_SECRET;
    tMemoryRecord.sRecordId = "secret-note-record";
    tMemoryRecord.sTitle = "requested note secret";
    tMemoryRecord.sText = "never-inject-secret-memory";
    tMemoryRecord.sSourceUri = "secret://test";
    tMemoryRecord.sActor = "test-user";
    tMemoryRecord.sReason = "sensitivity filter fixture";
    CHECK(pMemory && xllmMemoryPut(pMemory, &tMemoryRecord, &tMemoryReceipt, &tLlmError),
        "secret memory fixture is stored with an explicit label");

    xllmSessionConfigInit(&tSessionConfig);
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
    tAgentConfig.pMemory = pMemory;
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
    CHECK(pAgent && xworkAgentToolCount(pAgent) == 11u, "eleven practical builtin tools registered");
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
            xworkAgentToolCount(pAgent) == 12u &&
            xworkAgentToolRegistryGeneration(pAgent) == uGeneration + 1u,
            "dynamic tool registration advances the registry generation");
        xworkErrorInit(&tError);
        CHECK(xworkAgentUnregisterToolsBySource(pAgent, "test.dynamic", &iRemoved, &tError) &&
            iRemoved == 1u && xworkAgentToolCount(pAgent) == 11u &&
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
            xworkAgentToolCount(pAgent) == 12u,
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
            xworkAgentToolCount(pAgent) == 11u,
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
    CHECK(tResult.uMemoryHits == 2u && tResult.uMemoryContextBytes > 0u &&
        tResult.uMemoryStoreRevision == 3u && tEvents.uMemoryRetrievals == 2u &&
        tEvents.uMemoryHits == 2u,
        "memory and knowledge layers are retrieved with auditable revision metrics");
    CHECK(tMock.bSawRetrievedMemory && !tMock.bSawSecretMemory,
        "retrieved memory reaches the model while secret records remain filtered");
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
    pPatchTool = xwork__find_tool(pAgent, "apply_patch");
    tPatchContext.pAgent = pAgent;
    CHECK(pPatchTool && pPatchTool->OnExecute(pPatchTool->pUserData, &tPatchContext,
        "{\"changes\":[{\"path\":\"sandbox/note.txt\",\"operation\":\"replace\",\"old_text\":\"PATCHED-\",\"new_text\":\"BROKEN-\"},{\"path\":\"sandbox/note.txt/child.txt\",\"operation\":\"create\",\"content\":\"must fail\"}]}",
        &tPatchOutput, &tError) == XWORK_RESULT_OK && !tPatchOutput.bSuccess,
        "failed multi-file transaction is reported as a tool-level failure");
    CHECK(tPatchOutput.sContent && strstr(tPatchOutput.sContent, "rollback completed"), "failed transaction reports successful rollback");
    xworkToolOutputUnit(&tPatchOutput);

    sFile = (char*)xrtFileReadAll("tests/tmp_xwork/sandbox/note.txt", &iFileSize);
    CHECK(sFile && iFileSize > 1600u && strncmp(sFile, "PATCHED-", 8u) == 0, "workspace file was created then edited by both edit tools");
    CHECK(xrtFileExists((str)"tests/tmp_xwork/sandbox/extra.txt"), "multi-file patch transaction created its second target");
    CHECK(!xrtFileExists((str)"tests/tmp_xwork/sandbox/note.txt/child.txt"), "failed transaction left no partial target behind");

    pStartTool = xwork__find_tool(pAgent, "start_process");
    pWriteProcessTool = xwork__find_tool(pAgent, "write_process");
    pPollTool = xwork__find_tool(pAgent, "poll_process");
    xworkToolOutputInit(&tProcessOutput);
#if defined(_WIN32)
    CHECK(pStartTool && pStartTool->OnExecute(pStartTool->pUserData, &tPatchContext,
        "{\"command\":\"findstr persistent\",\"wait_ms\":0}", &tProcessOutput, &tError) == XWORK_RESULT_OK &&
        tProcessOutput.bSuccess && tProcessOutput.sContent && sscanf(tProcessOutput.sContent, "process_id: %llu", &uManagedId) == 1,
        "managed process starts and returns a stable process id");
#else
    CHECK(pStartTool && pStartTool->OnExecute(pStartTool->pUserData, &tPatchContext,
        "{\"command\":\"grep persistent\",\"wait_ms\":0}", &tProcessOutput, &tError) == XWORK_RESULT_OK &&
        tProcessOutput.bSuccess && tProcessOutput.sContent && sscanf(tProcessOutput.sContent, "process_id: %llu", &uManagedId) == 1,
        "managed process starts and returns a stable process id");
#endif
    xworkToolOutputUnit(&tProcessOutput);
    xworkToolOutputInit(&tProcessOutput);
    if ( uManagedId ) {
        char sProcessArgs[512];
        snprintf(sProcessArgs, sizeof(sProcessArgs),
            "{\"process_id\":%llu,\"input\":\"persistent hello\",\"append_newline\":true,\"close_stdin\":true}", uManagedId);
        CHECK(pWriteProcessTool && pWriteProcessTool->OnExecute(pWriteProcessTool->pUserData, &tPatchContext,
            sProcessArgs, &tProcessOutput, &tError) == XWORK_RESULT_OK && tProcessOutput.bSuccess,
            "managed process accepts stdin and an explicit stdin close");
        xworkToolOutputUnit(&tProcessOutput);
        xworkToolOutputInit(&tProcessOutput);
        snprintf(sProcessArgs, sizeof(sProcessArgs),
            "{\"process_id\":%llu,\"wait_ms\":5000,\"release\":true}", uManagedId);
        CHECK(pPollTool && pPollTool->OnExecute(pPollTool->pUserData, &tPatchContext,
            sProcessArgs, &tProcessOutput, &tError) == XWORK_RESULT_OK && tProcessOutput.bSuccess &&
            tProcessOutput.sContent && strstr(tProcessOutput.sContent, "persistent hello") && strstr(tProcessOutput.sContent, "state: exited"),
            "managed process poll returns incremental output and final exit state");
        CHECK(pAgent->iProcessCount == 0u, "released managed process leaves no live registry entry");
    }
    xworkToolOutputUnit(&tProcessOutput);

    pExecTool = xwork__find_tool(pAgent, "exec_command");
    xworkToolOutputInit(&tProcessOutput);
#if defined(_WIN32)
    CHECK(pExecTool && pExecTool->OnExecute(pExecTool->pUserData, &tPatchContext,
        "{\"command\":\"cmd /c exit 7\",\"expected_exit_codes\":[7]}",
        &tProcessOutput, &tError) == XWORK_RESULT_OK && tProcessOutput.bSuccess &&
        tProcessOutput.sContent && strstr(tProcessOutput.sContent, "exit_code: 7") &&
        strstr(tProcessOutput.sContent, "exit_expected: true"),
        "exec command accepts an explicitly expected nonzero exit code");
#else
    CHECK(pExecTool && pExecTool->OnExecute(pExecTool->pUserData, &tPatchContext,
        "{\"command\":\"sh -c 'exit 7'\",\"expected_exit_codes\":[7]}",
        &tProcessOutput, &tError) == XWORK_RESULT_OK && tProcessOutput.bSuccess &&
        tProcessOutput.sContent && strstr(tProcessOutput.sContent, "exit_code: 7") &&
        strstr(tProcessOutput.sContent, "exit_expected: true"),
        "exec command accepts an explicitly expected nonzero exit code");
#endif
    xworkToolOutputUnit(&tProcessOutput);
    xworkToolOutputInit(&tProcessOutput);
#if defined(_WIN32)
    CHECK(pExecTool && pExecTool->OnExecute(pExecTool->pUserData, &tPatchContext,
        "{\"command\":\"cmd /c exit 7\"}", &tProcessOutput, &tError) == XWORK_RESULT_OK &&
        !tProcessOutput.bSuccess && tProcessOutput.sContent && strstr(tProcessOutput.sContent, "exit_expected: false"),
        "exec command still rejects a nonzero exit code by default");
#else
    CHECK(pExecTool && pExecTool->OnExecute(pExecTool->pUserData, &tPatchContext,
        "{\"command\":\"sh -c 'exit 7'\"}", &tProcessOutput, &tError) == XWORK_RESULT_OK &&
        !tProcessOutput.bSuccess && tProcessOutput.sContent && strstr(tProcessOutput.sContent, "exit_expected: false"),
        "exec command still rejects a nonzero exit code by default");
#endif
    xworkToolOutputUnit(&tProcessOutput);
    xworkToolOutputInit(&tProcessOutput);
    CHECK(pExecTool && pExecTool->OnExecute(pExecTool->pUserData, &tPatchContext,
        "{\"command\":\"echo invalid\",\"expected_exit_codes\":[]}",
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
        arrInterruptedCalls[0].sName = "list_files";
        arrInterruptedCalls[0].sArgumentsJson = "{\"path\":\"sandbox\"}";
        arrInterruptedCalls[1].sId = "call_recovered_verify";
        arrInterruptedCalls[1].sName = "exec_command";
#if defined(_WIN32)
        sVerifyArgs = "{\"command\":\"type sandbox\\\\note.txt\"}";
#else
        sVerifyArgs = "{\"command\":\"cat sandbox/note.txt\"}";
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
    xllmMemoryClose(pMemory);
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
    pExecTool = pAgent ? xwork__find_tool(pAgent, "exec_command") : NULL;
    tToolContext.pAgent = pAgent;
    tToolContext.sWorkspaceRoot = sWorkspace;
    xworkToolOutputInit(&tOutput);
    uStartedMs = xrtClock() / UINT64_C(1000);
#if defined(_WIN32)
    if ( pExecTool ) eResult = pExecTool->OnExecute(pExecTool->pUserData, &tToolContext,
        "{\"command\":\"ping -n 6 127.0.0.1 >nul\",\"timeout_ms\":5000}", &tOutput, &tError);
#else
    if ( pExecTool ) eResult = pExecTool->OnExecute(pExecTool->pUserData, &tToolContext,
        "{\"command\":\"sleep 5\",\"timeout_ms\":5000}", &tOutput, &tError);
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
    g_sSelfPath = argc > 0 ? argv[0] : NULL;
    printf("xwork v2 tests\n");
    test_process_text_normalization();
    test_agent_context_deadline();
    test_command_context_deadline();
    test_readonly_subagent();
    test_agent_loop();
    printf("xwork v2: %s (%d failures)\n", g_iFailures ? "FAIL" : "PASS", g_iFailures);
    return g_iFailures ? 1 : 0;
}
