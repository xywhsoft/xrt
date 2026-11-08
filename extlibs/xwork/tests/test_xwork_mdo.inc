#include <inttypes.h>

static int test_task_id(const char* text, void* id)
{
    xvalue* value = xrtJsonParse(xrtStrView(text)); int64 number = 0;
    bool ok = value && xrtValueGetInt(xrtValueObjectGet(value, XRT_STR_LITERAL("task_id")), &number) && number > 0;
    xrtValueRelease(value); if (ok) { uint64_t value = (uint64_t)number; memcpy(id, &value, sizeof(value)); } return ok ? 1 : 0;
}
static xwork_result test_entry_execute(const xwork_tool_entry* tool, void* data,
    const xwork_tool_context* context, const char* args, xwork_tool_output* output, xwork_error* error)
{
    if (tool->OnExecute) return tool->OnExecute(data, context, args, output, error);
    xwork_tool_result_writer writer;
    xllm_tool_call call = {0};
    call.sId = "fixture-call"; call.sName = tool->sName; call.sArgumentsJson = (char*)args;
    xwork__result_writer_init(&writer, output, tool->iMaxResultBytes ? tool->iMaxResultBytes : 1048576u);
    writer.pAgent = context->pAgent; writer.pContext = context; writer.pTool = tool; writer.pCall = &call;
    xwork_result result = tool->OnExecuteV2(data, context, args, &writer, error);
    if (result == XWORK_RESULT_OK && !xwork__result_writer_finish(&writer, error)) result = XWORK_RESULT_ERROR;
    xwork__result_writer_unit(&writer);
    return result;
}
static bool test_wait_task(xwork_agent* agent, uint64_t id, xwork_error* error)
{
    return xworkRuntimeWaitTasks(agent->pRuntime, &id, 1u, XWORK_TASK_WAIT_ALL,
        xllmDeadlineAfterUs(5000000u), NULL, NULL, error) == XWORK_RESULT_OK;
}
static bool test_task_stdout(xwork_agent* agent, uint64_t id, const char* expected, xwork_error* error)
{
    xwork_task_output output; xworkTaskOutputInit(&output);
    bool ok = xworkRuntimeReadTaskOutput(agent->pRuntime,id,0u,0u,0u,4096u,&output,error) &&
        output.bComplete && output.iStdoutSize >= strlen(expected) &&
        memcmp(output.pStdout,expected,strlen(expected)) == 0;
    xworkTaskOutputUnit(&output); return ok;
}
static int g_iFailures = 0;
static const char *g_sSelfPath = NULL;
static xerrkind g_eArtifactPublishFailure = XERR_NONE;
static unsigned int g_iArtifactPublishCalls;
static bool g_bArtifactPublishRace;
static bool g_bRequestTokenRace;
static bool g_bProcessStopFailure;

static bool test_process_kill_tree(xprocess *pProcess)
{
    if (g_bProcessStopFailure)
        return false;
    return xrtProcessKillTree(pProcess);
}

/* Deterministic publication boundary injection; no timing or load dependency. */
static bool test_artifact_publish(xroot pRoot, cstr sSource, cstr sTarget)
{
    ++g_iArtifactPublishCalls;
    if (g_eArtifactPublishFailure != XERR_NONE)
    {
        xrtSetErrorKind(g_eArtifactPublishFailure);
        return false;
    }
    if (g_bArtifactPublishRace)
    {
        static const char sWinner[] = "other-publisher";
        xfileoptions tOptions;
        xfile pFile;
        g_bArtifactPublishRace = false;
        xrtFileOptionsInit(&tOptions);
        tOptions.Flags = XFILE_WRITE | XFILE_CREATE | XFILE_EXCLUSIVE;
        pFile = xrtRootFileOpen(pRoot, sTarget, &tOptions);
        if (!pFile)
            return false;
        if (!xrtWriteFull(pFile, sWinner, sizeof(sWinner) - 1u, NULL))
        {
            (void)xrtClose(pFile);
            return false;
        }
        if (!xrtClose(pFile))
            return false;
    }
    if (g_bRequestTokenRace)
    {
        char Text[1025];
        size_t Read = 0u;
        xfileoptions Options;
        xfile File;
        xvalue *Object;
        char *Json;
        size_t Size;
        g_bRequestTokenRace = false;
        xrtFileOptionsInit(&Options);
        Options.Flags = XFILE_READ;
        File = xrtRootFileOpen(pRoot, sSource, &Options);
        if (!File || !xrtRead(File, Text, 1024u, &Read))
            return false;
        (void)xrtClose(File);
        Text[Read] = 0;
        Object = xrtJsonParse(xrtStrViewN(Text, Read));
        xrtValueObjectSetNew(Object, xrtStrView("id"),
                             xrtValueString(xrtStrView("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb")));
        Json = xrtJsonStringify(Object, false, &Size);
        xrtValueRelease(Object);
        Options.Flags = XFILE_WRITE | XFILE_CREATE | XFILE_EXCLUSIVE;
        File = xrtRootFileOpen(pRoot, sTarget, &Options);
        bool Ok = File && Json && xrtWriteFull(File, Json, Size, NULL) && xrtFlush(File);
        if (File)
            (void)xrtClose(File);
        xrtFree(Json);
        if (!Ok)
            return false;
    }
    return xrtRootLinkHard(pRoot, sSource, sTarget);
}

static xllm_response *mock_response(const char *sContent, size_t iToolCount);

#define CHECK(expr, name)                                                                          \
    do                                                                                             \
    {                                                                                              \
        bool xwork_ok__ = !!(expr);                                                                \
        printf("  %-62s %s\n", (name), xwork_ok__ ? "PASS" : "FAIL");                              \
        if (!xwork_ok__)                                                                           \
            ++g_iFailures;                                                                         \
    } while (0)

typedef struct mock_model
{
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
    xwork_agent *pAgent;
    bool bRegistryMutationBlocked;
    bool bCatalogAdoptionBlocked;
} mock_model;

typedef struct test_events
{
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
    uint32_t uRecoveryRequired;
    uint32_t uRecoveryResolved;
    uint32_t uRecoveryAutomatic;
    uint32_t uRecoveryUncertain;
    bool bRequestMetadata;
    bool bResponseMetadata;
    bool bCompactionQualityMetadata;
    bool bRuntimeIdentity;
    uint64_t uLastUserMessageSequence;
    uint64_t uLastArtifactId;
    char sLastArtifact[512];
} test_events;

typedef struct subagent_mock
{
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

static char *test_strdup(const char *sText)
{
    size_t iLen = strlen(sText);
    char *sCopy = (char *)malloc(iLen + 1u);
    if (sCopy)
        memcpy(sCopy, sText, iLen + 1u);
    return sCopy;
}

static xllm_result v3_final_complete(void *pUserData, const xllm_request *pRequest,
                                     const xllm_stream_callbacks *pCallbacks,
                                     xllm_response **ppResponse, xllm_error *pError)
{
    uint32_t *pCalls = (uint32_t *)pUserData;
    (void)pRequest;
    (void)pCallbacks;
    (void)pError;
    if (pCalls)
        ++*pCalls;
    *ppResponse = mock_response("v3 run completed", 0u);
    return *ppResponse ? XLLM_RESULT_OK : XLLM_RESULT_ERROR;
}

typedef struct agent_owner_fixture
{
    uint32_t uRefs;
    uint32_t uRetainAttempts;
    uint32_t uReleases;
    uint32_t uModelCalls;
    bool bRetainable;
} agent_owner_fixture;

static bool agent_owner_retain(void *pUserData)
{
    agent_owner_fixture *pOwner = (agent_owner_fixture *)pUserData;
    if (!pOwner)
        return false;
    ++pOwner->uRetainAttempts;
    if (!pOwner->bRetainable)
        return false;
    ++pOwner->uRefs;
    return true;
}

static void agent_owner_release(void *pUserData)
{
    agent_owner_fixture *pOwner = (agent_owner_fixture *)pUserData;
    if (!pOwner || pOwner->uRefs == 0u)
    {
        ++g_iFailures;
        return;
    }
    --pOwner->uRefs;
    ++pOwner->uReleases;
}

static xllm_result agent_owner_complete(void *pUserData, const xllm_request *pRequest,
                                        const xllm_stream_callbacks *pCallbacks,
                                        xllm_response **ppResponse, xllm_error *pError)
{
    agent_owner_fixture *pOwner = (agent_owner_fixture *)pUserData;
    (void)pRequest;
    (void)pCallbacks;
    (void)pError;
    if (pOwner)
        ++pOwner->uModelCalls;
    *ppResponse = mock_response("owner-pinned", 0u);
    return *ppResponse ? XLLM_RESULT_OK : XLLM_RESULT_ERROR;
}

typedef struct user_image_fixture
{
    bool bSawImage;
    uint32_t uCalls;
} user_image_fixture;

static xllm_result user_image_complete(void *pUserData, const xllm_request *pRequest,
                                       const xllm_stream_callbacks *pCallbacks,
                                       xllm_response **ppResponse, xllm_error *pError)
{
    user_image_fixture *pFixture = (user_image_fixture *)pUserData;
    size_t i;
    (void)pCallbacks;
    (void)pError;
    ++pFixture->uCalls;
    for (i = 0u; i < pRequest->iMessageCount; ++i)
    {
        const xllm_message *pMessage = &pRequest->pMessages[i];
        if (pMessage->eRole == XLLM_ROLE_USER && pMessage->iPartCount == 2u &&
            pMessage->pParts[0].eKind == XLLM_PART_TEXT && pMessage->pParts[0].sText &&
            strcmp(pMessage->pParts[0].sText, "Describe this image") == 0 &&
            pMessage->pParts[1].eKind == XLLM_PART_IMAGE && pMessage->pParts[1].sMediaType &&
            strcmp(pMessage->pParts[1].sMediaType, "image/png") == 0 &&
            pMessage->pParts[1].iDataSize == 4u && pMessage->pParts[1].pData[0] == 0x89u)
            pFixture->bSawImage = true;
    }
    *ppResponse = mock_response("image received", 0u);
    return *ppResponse ? XLLM_RESULT_OK : XLLM_RESULT_ERROR;
}

typedef struct v3_cancel_model
{
    volatile long iEntered;
} v3_cancel_model;

static xllm_result v3_cancel_complete(void *pUserData, const xllm_request *pRequest,
                                      const xllm_stream_callbacks *pCallbacks,
                                      xllm_response **ppResponse, xllm_error *pError)
{
    v3_cancel_model *pModel = (v3_cancel_model *)pUserData;
    uint64_t uDeadline = xllmDeadlineAfterUs(UINT64_C(2000000));
    (void)pCallbacks;
    if (ppResponse)
        *ppResponse = NULL;
    if (pModel)
        xwork__atomic_store(&pModel->iEntered, 1);
    while (!pRequest->pCancel || !xrtCancelRequested(pRequest->pCancel))
    {
        if (xllmDeadlineExpired(uDeadline))
        {
            if (pError)
            {
                xllmErrorInit(pError);
                pError->eCode = XLLM_ERROR_TIMEOUT;
                (void)snprintf(pError->sMessage, sizeof(pError->sMessage), "%s",
                               "bounded cancellation fixture expired");
            }
            return XLLM_RESULT_TIMEOUT;
        }
        mdo_test_sleep_ms(1u);
    }
    if (pError)
    {
        xllmErrorInit(pError);
        pError->eCode = XLLM_ERROR_CANCELLED;
        (void)snprintf(pError->sMessage, sizeof(pError->sMessage), "%s",
                       "bounded cancellation fixture observed cancellation");
    }
    return XLLM_RESULT_CANCELLED;
}

static xwork_result registry_probe_execute(void *pUserData, const xwork_tool_context *pContext,
                                           const char *sArgumentsJson, xwork_tool_output *pOutput,
                                           xwork_error *pError)
{
    (void)pUserData;
    (void)pContext;
    (void)sArgumentsJson;
    (void)pError;
    return xworkToolOutputSet(pOutput, true, "dynamic registry probe") ? XWORK_RESULT_OK
                                                                       : XWORK_RESULT_ERROR;
}

typedef struct tool_contract_fixture
{
    xwork_permission_decision eDecision;
    uint32_t uPermissionCalls;
    uint32_t uExecuteCalls;
    bool bSawEffects;
    bool bSawNetwork;
    bool bSawSecret;
    bool bSawIdentity;
    bool bSawToolContext;
    bool bDescribeFail;
    uint64_t uLastRequestId;
} tool_contract_fixture;

static xwork_result tool_contract_describe(void *pUserData, const xwork_tool_context *pContext,
                                           const char *sArgumentsJson,
                                           xwork_permission_resource_writer *pWriter,
                                           xwork_error *pError)
{
    tool_contract_fixture *pFixture = (tool_contract_fixture *)pUserData;
    char sEndpoint[] = "https://ling.example/v1/responses";
    (void)pContext;
    (void)sArgumentsJson;
    if (pFixture->bDescribeFail)
    {
        xwork__set_error(pError, XWORK_ERROR_POLICY, "fixture refused to describe its resources");
        return XWORK_RESULT_ERROR;
    }
    if (!xworkPermissionResourceWriterAdd(pWriter, XWORK_RESOURCE_NETWORK,
                                          XWORK_RESOURCE_ACCESS_CONNECT, sEndpoint))
    {
        return XWORK_RESULT_ERROR;
    }
    memset(sEndpoint, 'x', sizeof(sEndpoint) - 1u);
    if (!xworkPermissionResourceWriterAdd(pWriter, XWORK_RESOURCE_SECRET, XWORK_RESOURCE_ACCESS_USE,
                                          "LING_API_KEY"))
    {
        return XWORK_RESULT_ERROR;
    }
    return XWORK_RESULT_OK;
}

static xwork_result tool_contract_execute_v2(void *pUserData, const xwork_tool_context *pContext,
                                             const char *sArgumentsJson,
                                             xwork_tool_result_writer *pWriter, xwork_error *pError)
{
    tool_contract_fixture *pFixture = (tool_contract_fixture *)pUserData;
    char sPrefix[] = "alpha";
    (void)pContext;
    (void)sArgumentsJson;
    (void)pError;
    ++pFixture->uExecuteCalls;
    pFixture->bSawToolContext =
        pContext && pContext->uAgentId != 0u && pContext->uCatalogGeneration != 0u &&
        pContext->uRunId == 0u && pContext->uDeadline == XLLM_DEADLINE_NEVER &&
        pContext->sToolName && strcmp(pContext->sToolName, "contract.multi") == 0 &&
        pContext->sToolCallId && strcmp(pContext->sToolCallId, "contract-1") == 0 &&
        pContext->uEffects == (XWORK_TOOL_EFFECT_NETWORK | XWORK_TOOL_EFFECT_EXTERNAL_SERVICE |
                               XWORK_TOOL_EFFECT_SECRETS);
    if (!xworkToolResultWriterSetSuccess(pWriter, true) ||
        !xworkToolResultWriterWrite(pWriter, sPrefix, strlen(sPrefix)))
    {
        return XWORK_RESULT_ERROR;
    }
    memset(sPrefix, 'z', sizeof(sPrefix) - 1u);
    return xworkToolResultWriterWriteText(pWriter, "-beta") ? XWORK_RESULT_OK : XWORK_RESULT_ERROR;
}

static xwork_permission_decision tool_contract_permission(void *pUserData,
                                                          const xwork_permission_request *pRequest)
{
    tool_contract_fixture *pFixture = (tool_contract_fixture *)pUserData;
    size_t i;
    ++pFixture->uPermissionCalls;
    pFixture->bSawIdentity = pRequest->uRequestId > pFixture->uLastRequestId &&
                             pRequest->uAgentId != 0u && pRequest->uRunId == 0u &&
                             pRequest->uCatalogGeneration != 0u && pRequest->sToolCallId &&
                             strcmp(pRequest->sToolCallId, "contract-1") == 0 &&
                             pRequest->uDeadline == XLLM_DEADLINE_NEVER;
    pFixture->uLastRequestId = pRequest->uRequestId;
    pFixture->bSawEffects =
        pRequest->uEffects == (XWORK_TOOL_EFFECT_NETWORK | XWORK_TOOL_EFFECT_EXTERNAL_SERVICE |
                               XWORK_TOOL_EFFECT_SECRETS) &&
        pRequest->eRisk == XWORK_RISK_HIGH;
    for (i = 0u; i < pRequest->iResourceCount; ++i)
    {
        const xwork_permission_resource *pResource = &pRequest->pResources[i];
        if (pResource->eKind == XWORK_RESOURCE_NETWORK &&
            pResource->uAccess == XWORK_RESOURCE_ACCESS_CONNECT &&
            strcmp(pResource->sResource, "https://ling.example/v1/responses") == 0)
        {
            pFixture->bSawNetwork = true;
        }
        if (pResource->eKind == XWORK_RESOURCE_SECRET &&
            pResource->uAccess == XWORK_RESOURCE_ACCESS_USE &&
            strcmp(pResource->sResource, "LING_API_KEY") == 0)
        {
            pFixture->bSawSecret = true;
        }
    }
    return pFixture->eDecision;
}

static void test_v3_tool_contract(void)
{
    static const char sWorkspace[] = "tests/tmp_xwork_contract";
    xllm_session_config tSessionConfig;
    xllm_session *pSession = NULL;
    xwork_agent_config tAgentConfig;
    xwork_agent *pAgent = NULL;
    xwork_tool_definition tDefinition;
    xwork_tool_info tInfo;
    xwork_error tError;
    xllm_executor tExecutor;
    xllm_executor_ctx tCtx;
    xllm_executor_result tOut;
    xllm_tool_call tCall;
    tool_contract_fixture tFixture;
    uint32_t uBefore;

    (void)xrtDirRemoveAll(sWorkspace);
    CHECK(xrtDirCreateAll((str)sWorkspace), "tool contract workspace created");
    memset(&tFixture, 0, sizeof(tFixture));
    tFixture.eDecision = XWORK_PERMISSION_ALLOW;
    xllmSessionConfigInit(&tSessionConfig);
    pSession = xllmSessionCreate(&tSessionConfig, NULL);
    xworkAgentConfigInit(&tAgentConfig);
    tAgentConfig.pSession = pSession;
    tAgentConfig.sWorkspaceRoot = sWorkspace;
    tAgentConfig.bRegisterBuiltinTools = false;
    tAgentConfig.OnPermission = tool_contract_permission;
    tAgentConfig.pPermissionUserData = &tFixture;
    pAgent = xworkAgentCreate(&tAgentConfig, &tError);
    CHECK(pSession && pAgent, "writer-only tool agent creates");

    memset(&tDefinition, 0, sizeof(tDefinition));
    tDefinition.sName = "contract.multi";
    tDefinition.sDescription = "Exercise the vNext tool ABI.";
    tDefinition.sParametersJson = "{\"type\":\"object\",\"additionalProperties\":false}";
    tDefinition.bStrict = true;
    tDefinition.uEffects =
        XWORK_TOOL_EFFECT_NETWORK | XWORK_TOOL_EFFECT_EXTERNAL_SERVICE | XWORK_TOOL_EFFECT_SECRETS;
    tDefinition.pUserData = &tFixture;
    tDefinition.sSource = "test.contract";
    tDefinition.OnDescribePermissions = tool_contract_describe;
    tDefinition.OnExecuteV2 = tool_contract_execute_v2;
    tDefinition.iMaxResultBytes = 64u;
    CHECK(pAgent && xworkAgentRegisterTool(pAgent, &tDefinition, &tError),
          "writer-only multi-effect tool registers");
    memset(&tInfo, 0, sizeof(tInfo));
    CHECK(pAgent && xworkAgentToolAt(pAgent, 0u, &tInfo) &&
              tInfo.uEffects == tDefinition.uEffects && tInfo.bDescribesPermissions &&
              tInfo.bUsesResultWriter && tInfo.iMaxResultBytes == 64u,
          "catalog exposes the effect bitset and result-writer contract");

    memset(&tExecutor, 0, sizeof(tExecutor));
    CHECK(pAgent && xworkExecutorBind(&tExecutor, pAgent, &tError), "tool contract executor binds");
    memset(&tCtx, 0, sizeof(tCtx));
    tCtx.uRound = 1u;
    tCtx.uDeadline = XLLM_DEADLINE_NEVER;
    memset(&tCall, 0, sizeof(tCall));
    tCall.sId = (char *)"contract-1";
    tCall.sName = (char *)"contract.multi";
    tCall.sArgumentsJson = (char *)"{}";
    memset(&tOut, 0, sizeof(tOut));
    CHECK(tExecutor.pExecute && tExecutor.pExecute(tExecutor.pUserData, &tCall, &tCtx, &tOut) &&
              tOut.bSuccess && tOut.sContent && strstr(tOut.sContent, "alpha-beta") &&
              tFixture.uExecuteCalls == 1u,
          "result writer copies transient chunks into the host-owned result");
    CHECK(tFixture.uPermissionCalls == 1u && tFixture.bSawEffects && tFixture.bSawNetwork &&
              tFixture.bSawSecret && tFixture.bSawIdentity && tFixture.bSawToolContext,
          "permission and tool contexts carry identity, scope, effects, and resources");

    tFixture.eDecision = XWORK_PERMISSION_DENY;
    uBefore = tFixture.uExecuteCalls;
    memset(&tOut, 0, sizeof(tOut));
    CHECK(tExecutor.pExecute(tExecutor.pUserData, &tCall, &tCtx, &tOut) && !tOut.bSuccess &&
              tOut.sContent && strstr(tOut.sContent, "denied") && tFixture.uExecuteCalls == uBefore,
          "multi-effect permission denial prevents execution");

    tFixture.eDecision = (xwork_permission_decision)99;
    memset(&tOut, 0, sizeof(tOut));
    CHECK(tExecutor.pExecute(tExecutor.pUserData, &tCall, &tCtx, &tOut) && !tOut.bSuccess &&
              tFixture.uExecuteCalls == uBefore,
          "unknown permission decisions fail closed");

    tFixture.eDecision = XWORK_PERMISSION_ALLOW;
    tFixture.bDescribeFail = true;
    memset(&tOut, 0, sizeof(tOut));
    CHECK(tExecutor.pExecute(tExecutor.pUserData, &tCall, &tCtx, &tOut) && !tOut.bSuccess &&
              tFixture.uExecuteCalls == uBefore,
          "permission resource discovery failure fails closed");
    tFixture.bDescribeFail = false;

    tDefinition.sName = "contract.limit";
    tDefinition.iMaxResultBytes = 4u;
    CHECK(xworkAgentRegisterTool(pAgent, &tDefinition, &tError),
          "per-tool result limit publishes in a separate descriptor");
    tCall.sName = (char *)"contract.limit";
    memset(&tOut, 0, sizeof(tOut));
    CHECK(tExecutor.pExecute(tExecutor.pUserData, &tCall, &tCtx, &tOut) && !tOut.bSuccess &&
              tFixture.uExecuteCalls == uBefore + 1u,
          "result writer rejects output beyond the descriptor limit");

    tDefinition.sName = "contract.invalid";
    tDefinition.OnExecute = registry_probe_execute;
    CHECK(!xworkAgentRegisterTool(pAgent, &tDefinition, &tError) &&
              tError.eCode == XWORK_ERROR_INVALID_ARGUMENT,
          "tool descriptor rejects ambiguous legacy and writer executors");
    tDefinition.sName = "contract.no-resources";
    tDefinition.OnExecute = NULL;
    tDefinition.OnDescribePermissions = NULL;
    CHECK(!xworkAgentRegisterTool(pAgent, &tDefinition, &tError) &&
              tError.eCode == XWORK_ERROR_INVALID_ARGUMENT,
          "effectful tool descriptor requires permission resource discovery");

    xworkExecutorUnbind(&tExecutor);
    xworkAgentDestroy(pAgent);
    xllmSessionDestroy(pSession);
    (void)xrtDirRemoveAll(sWorkspace);
}

static void test_native_explore_tools(void)
{
    static const char sWorkspace[] = "tests/tmp_xwork_explore";
    static const unsigned char pBinary[] = {'n', 'e', 'e', 'd', 'l', 'e', 0u, 'b', 'i', 'n'};
    char sLongLine[700];
    xllm_session_config tSessionConfig;
    xllm_session *pSession = NULL;
    xwork_agent_config tAgentConfig;
    xwork_agent *pAgent = NULL;
    const xwork_tool_entry *pLs;
    const xwork_tool_entry *pGlob;
    const xwork_tool_entry *pGrep;
    xwork_tool_context tContext;
    xwork_tool_output tOutput;
    xwork_error tError;
    xcancel *pCancel = NULL;
    xwork_result eResult;

    memset(sLongLine, 'a', sizeof(sLongLine));
    memcpy(sLongLine + 520u, "needle", 6u);
    sLongLine[sizeof(sLongLine) - 2u] = '\n';
    sLongLine[sizeof(sLongLine) - 1u] = '\0';
    (void)xrtDirRemoveAll(sWorkspace);
    CHECK(xrtDirCreateAll("tests/tmp_xwork_explore/Dir") &&
              xrtDirCreateAll("tests/tmp_xwork_explore/.github") &&
              xrtFileWriteAtomic("tests/tmp_xwork_explore/a.txt",
                                 (xbytesview){(const uint8 *)sLongLine, sizeof(sLongLine) - 1u}) &&
              xrtFileWriteAtomic("tests/tmp_xwork_explore/Dir/B.TXT",
                                 (xbytesview){(const uint8 *)"Needle second\r\n", 15u}) &&
              xrtFileWriteAtomic("tests/tmp_xwork_explore/Dir/\xE7\x8C\xAB.c",
                                 (xbytesview){(const uint8 *)"unicode name\n", 13u}) &&
              xrtFileWriteAtomic("tests/tmp_xwork_explore/.github/workflow.yml",
                                 (xbytesview){(const uint8 *)"needle hidden\n", 14u}) &&
              xrtFileWriteAtomic("tests/tmp_xwork_explore/binary.bin",
                                 (xbytesview){pBinary, sizeof(pBinary)}) &&
              xrtFileWriteAtomic("tests/tmp_xwork_explore/z.txt",
                                 (xbytesview){(const uint8 *)"needle last\n", 12u}),
          "native explore workspace creates deterministic text and binary fixtures");

    xllmSessionConfigInit(&tSessionConfig);
    pSession = xllmSessionCreate(&tSessionConfig, NULL);
    xworkAgentConfigInit(&tAgentConfig);
    tAgentConfig.pSession = pSession;
    tAgentConfig.sWorkspaceRoot = sWorkspace;
    pAgent = xworkAgentCreate(&tAgentConfig, &tError);
    pLs = pAgent ? xwork__find_tool(pAgent, "ls") : NULL;
    pGlob = pAgent ? xwork__find_tool(pAgent, "glob") : NULL;
    pGrep = pAgent ? xwork__find_tool(pAgent, "grep") : NULL;
    CHECK(pSession && pAgent && pLs && pGlob && pGrep &&
              pLs->uEffects == XWORK_TOOL_EFFECT_READ_ONLY &&
              pGlob->uEffects == XWORK_TOOL_EFFECT_READ_ONLY &&
              pGrep->uEffects == XWORK_TOOL_EFFECT_READ_ONLY,
          "ls, glob, and grep register as compact read-only builtins");

    memset(&tContext, 0, sizeof(tContext));
    tContext.pAgent = pAgent;
    tContext.sWorkspaceRoot = sWorkspace;
    xworkToolOutputInit(&tOutput);
    eResult = pLs ? test_entry_execute(pLs, pLs->pUserData, &tContext, "{}", &tOutput, &tError)
                  : XWORK_RESULT_ERROR;
    CHECK(eResult == XWORK_RESULT_OK && tOutput.bSuccess && tOutput.sContent &&
              strcmp(tOutput.sContent, "Dir/\na.txt\nbinary.bin\nz.txt\n") == 0,
          "ls hides dot entries and sorts names by raw UTF-8 bytes");
    xworkToolOutputUnit(&tOutput);

    xworkToolOutputInit(&tOutput);
    eResult = pGlob ? test_entry_execute(pGlob, pGlob->pUserData, &tContext, "{\"pattern\":\"**/*.txt\"}",
                                       &tOutput, &tError)
                    : XWORK_RESULT_ERROR;
    CHECK(eResult == XWORK_RESULT_OK && tOutput.bSuccess && tOutput.sContent &&
              strcmp(tOutput.sContent, "a.txt\nz.txt\n") == 0,
          "glob is case-sensitive by default on every platform");
    xworkToolOutputUnit(&tOutput);

    xworkToolOutputInit(&tOutput);
    eResult = pGlob ? test_entry_execute(pGlob, pGlob->pUserData, &tContext,
                                       "{\"pattern\":\"**/*.txt\",\"ignore_case\":true}", &tOutput,
                                       &tError)
                    : XWORK_RESULT_ERROR;
    CHECK(eResult == XWORK_RESULT_OK && tOutput.bSuccess && tOutput.sContent &&
              strcmp(tOutput.sContent, "Dir/B.TXT\na.txt\nz.txt\n") == 0 &&
              strchr(tOutput.sContent, '\\') == NULL,
          "glob ignore_case is explicit and returns sorted forward-slash paths");
    xworkToolOutputUnit(&tOutput);

    xworkToolOutputInit(&tOutput);
    eResult = pGlob ? test_entry_execute(pGlob, pGlob->pUserData, &tContext, "{\"pattern\":\"Dir/?.c\"}",
                                       &tOutput, &tError)
                    : XWORK_RESULT_ERROR;
    CHECK(eResult == XWORK_RESULT_OK && tOutput.bSuccess && tOutput.sContent &&
              strcmp(tOutput.sContent, "Dir/\xE7\x8C\xAB.c\n") == 0,
          "glob question mark consumes one UTF-8 scalar");
    xworkToolOutputUnit(&tOutput);

    xworkToolOutputInit(&tOutput);
    eResult = pGlob ? test_entry_execute(pGlob, pGlob->pUserData, &tContext, "{\"pattern\":\"**/*.yml\"}",
                                       &tOutput, &tError)
                    : XWORK_RESULT_ERROR;
    CHECK(eResult == XWORK_RESULT_OK && tOutput.bSuccess && tOutput.sContent &&
              strcmp(tOutput.sContent, "(no matches)\n") == 0,
          "glob excludes hidden paths by default");
    xworkToolOutputUnit(&tOutput);

    xworkToolOutputInit(&tOutput);
    eResult =
        pGlob ? test_entry_execute(pGlob, pGlob->pUserData, &tContext,
                                 "{\"pattern\":\"**/*.yml\",\"hidden\":true}", &tOutput, &tError)
              : XWORK_RESULT_ERROR;
    CHECK(eResult == XWORK_RESULT_OK && tOutput.bSuccess && tOutput.sContent &&
              strcmp(tOutput.sContent, ".github/workflow.yml\n") == 0,
          "glob includes hidden paths only when requested");
    xworkToolOutputUnit(&tOutput);

    xworkToolOutputInit(&tOutput);
    eResult = pGrep
                  ? test_entry_execute(pGrep, pGrep->pUserData, &tContext,
                                     "{\"pattern\":\"needle\",\"path\":\"a.txt\",\"regex\":false}",
                                     &tOutput, &tError)
                  : XWORK_RESULT_ERROR;
    CHECK(eResult == XWORK_RESULT_OK && tOutput.bSuccess && tOutput.sContent &&
              strstr(tOutput.sContent, "a.txt:1:") && strstr(tOutput.sContent, "needle") &&
              strlen(tOutput.sContent) < 500u,
          "grep matches the complete long line before emitting a centered preview");
    xworkToolOutputUnit(&tOutput);

    xworkToolOutputInit(&tOutput);
    eResult =
        pGrep ? test_entry_execute(pGrep, pGrep->pUserData, &tContext,
                                 "{\"pattern\":\"needle\",\"regex\":false,\"ignore_case\":true}",
                                 &tOutput, &tError)
              : XWORK_RESULT_ERROR;
    CHECK(eResult == XWORK_RESULT_OK && tOutput.bSuccess && tOutput.sContent &&
              strstr(tOutput.sContent, "Dir/B.TXT:1:") && strstr(tOutput.sContent, "a.txt:1:") &&
              strstr(tOutput.sContent, "z.txt:1:") &&
              strstr(tOutput.sContent, "binary_or_invalid_utf8=1") &&
              !strstr(tOutput.sContent, "workflow.yml"),
          "grep applies explicit ASCII case folding and reports skipped binary input");
    xworkToolOutputUnit(&tOutput);

    xworkToolOutputInit(&tOutput);
    eResult = pGrep ? test_entry_execute(pGrep, pGrep->pUserData, &tContext,
                                       "{\"pattern\":\"needle\",\"regex\":false,\"max_results\":1}",
                                       &tOutput, &tError)
                    : XWORK_RESULT_ERROR;
    CHECK(eResult == XWORK_RESULT_OK && tOutput.bSuccess && tOutput.sContent &&
              strstr(tOutput.sContent, "[truncated: shown=1") &&
              strstr(tOutput.sContent, "binary_or_invalid_utf8=1"),
          "grep enforces its result ceiling and retains scan diagnostics");
    xworkToolOutputUnit(&tOutput);

    xworkToolOutputInit(&tOutput);
    eResult = pGrep ? test_entry_execute(pGrep, pGrep->pUserData, &tContext,
                                       "{\"pattern\":\"[\",\"regex\":true}", &tOutput, &tError)
                    : XWORK_RESULT_ERROR;
    CHECK(eResult == XWORK_RESULT_OK && !tOutput.bSuccess && tOutput.sContent &&
              strstr(tOutput.sContent, "invalid"),
          "grep rejects an invalid regular expression as a tool-level error");
    xworkToolOutputUnit(&tOutput);

    xworkToolOutputInit(&tOutput);
    eResult =
        pLs ? test_entry_execute(pLs, pLs->pUserData, &tContext, "{\"path\":\"../\"}", &tOutput, &tError)
            : XWORK_RESULT_ERROR;
    CHECK(eResult == XWORK_RESULT_OK && !tOutput.bSuccess,
          "native discovery rejects lexical workspace escape");
    xworkToolOutputUnit(&tOutput);

    pCancel = xrtCancelCreate();
    if (pCancel)
        (void)xrtCancelRequest(pCancel);
    tContext.pCancel = pCancel;
    xworkToolOutputInit(&tOutput);
    xworkErrorInit(&tError);
    eResult = pGlob ? test_entry_execute(pGlob, pGlob->pUserData, &tContext, "{\"pattern\":\"**/*\"}",
                                       &tOutput, &tError)
                    : XWORK_RESULT_ERROR;
    CHECK(pCancel && eResult == XWORK_RESULT_CANCELLED && tError.eCode == XWORK_ERROR_CANCELLED,
          "native discovery observes cancellation at a bounded traversal boundary");
    xworkToolOutputUnit(&tOutput);
    xrtCancelDestroy(pCancel);
    pCancel = NULL;

    tContext.pCancel = NULL;
    tContext.uDeadline = xllmDeadlineAfterUs(0u);
    xworkToolOutputInit(&tOutput);
    xworkErrorInit(&tError);
    eResult = pGrep
                  ? test_entry_execute(pGrep, pGrep->pUserData, &tContext,
                                     "{\"pattern\":\"needle\",\"regex\":false}", &tOutput, &tError)
                  : XWORK_RESULT_ERROR;
    CHECK(eResult == XWORK_RESULT_TIMEOUT && tError.eCode == XWORK_ERROR_TIMEOUT,
          "native discovery observes an expired monotonic deadline");
    xworkToolOutputUnit(&tOutput);

    xworkAgentDestroy(pAgent);
    xllmSessionDestroy(pSession);
    (void)xrtDirRemoveAll(sWorkspace);
}

typedef struct catalog_reader_fixture
{
    xwork_runtime *pRuntime;
    volatile long iReady;
    volatile long iPublished;
    volatile long iValid;
} catalog_reader_fixture;

typedef struct tool_owner_fixture
{
    size_t iRefs;
    size_t iRetainCalls;
    size_t iReleaseCalls;
    bool bRetainable;
} tool_owner_fixture;

static bool tool_owner_retain(void *pUserData)
{
    tool_owner_fixture *pOwner = (tool_owner_fixture *)pUserData;
    if (!pOwner)
        return false;
    ++pOwner->iRetainCalls;
    if (!pOwner->bRetainable)
        return false;
    ++pOwner->iRefs;
    return true;
}

static void tool_owner_release(void *pUserData)
{
    tool_owner_fixture *pOwner = (tool_owner_fixture *)pUserData;
    if (!pOwner || pOwner->iRefs == 0u)
        return;
    --pOwner->iRefs;
    ++pOwner->iReleaseCalls;
}

static int32 catalog_reader_thread(void *pData)
{
    catalog_reader_fixture *pFixture = (catalog_reader_fixture *)pData;
    xwork_tool_catalog *pCatalog = xworkRuntimeToolCatalogSnapshot(pFixture->pRuntime);
    xwork_tool_info tInfo;
    uint64_t uDeadline = xllmDeadlineAfterUs(UINT64_C(2000000));
    xwork__atomic_store(&pFixture->iReady, 1);
    while (xwork__atomic_load(&pFixture->iPublished) == 0 && !xllmDeadlineExpired(uDeadline))
    {
        mdo_test_sleep_ms(1u);
    }
    if (pCatalog && xworkToolCatalogCount(pCatalog) == 2u &&
        xworkToolCatalogToolAt(pCatalog, 0u, &tInfo) && tInfo.sName &&
        strcmp(tInfo.sName, "catalog.alpha") == 0)
    {
        xwork__atomic_store(&pFixture->iValid, 1);
    }
    xworkToolCatalogRelease(pCatalog);
    return 0;
}

static void test_v3_tool_catalog_generations(void)
{
    static const char sWorkspace[] = "tests/tmp_xwork_v3_catalog";
    xwork_runtime_config tRuntimeConfig;
    xwork_agent_definition_config tDefinitionConfig;
    xwork_agent_options tOptions;
    xllm_session_config tSessionConfig;
    xwork_runtime *pRuntime = NULL;
    xwork_agent_definition *pDefinition = NULL;
    xllm_session *pSession = NULL;
    xwork_agent *pAgent = NULL;
    xwork_tool_catalog *pOldRuntime = NULL;
    xwork_tool_catalog *pNewRuntime = NULL;
    xwork_tool_catalog *pOldAgent = NULL;
    xwork_tool_catalog *pCurrentAgent = NULL;
    xwork_tool_catalog *pFailedRuntime = NULL;
    xwork_tool_catalog *pLimitRuntime = NULL;
    xwork_tool_catalog *pAfterLimitRuntime = NULL;
    xwork_tool_definition pInitial[2];
    xwork_tool_definition tReplacement;
    xwork_tool_definition pDuplicate[2];
    xwork_tool_info tInfo;
    xwork_error tError;
    uint64_t uFailedGeneration;
    uint64_t uReaderDeadline;
    size_t iRemoved = 0u;
    catalog_reader_fixture tReader;
    xthread *pReaderThread = NULL;

    memset(pInitial, 0, sizeof(pInitial));
    pInitial[0].sName = "catalog.alpha";
    pInitial[0].sDescription = "first immutable catalog probe";
    pInitial[0].sParametersJson = "{\"type\":\"object\"}";
    pInitial[0].uEffects = XWORK_TOOL_EFFECT_READ_ONLY;
    pInitial[0].OnExecute = registry_probe_execute;
    pInitial[0].sSource = "test.catalog";
    pInitial[1] = pInitial[0];
    pInitial[1].sName = "catalog.beta";
    pInitial[1].sDescription = "second immutable catalog probe";
    memset(&tReplacement, 0, sizeof(tReplacement));
    tReplacement = pInitial[0];
    tReplacement.sName = "catalog.gamma";
    tReplacement.sDescription = "replacement immutable catalog probe";
    tReplacement.bParallelSafe = true;
    tReplacement.sSerialGroup = "catalog.parallel";

    xworkRuntimeConfigInit(&tRuntimeConfig);
    tRuntimeConfig.uMaxCatalogTools = 2u;
    pRuntime = xworkRuntimeCreate(&tRuntimeConfig, &tError);
    CHECK(pRuntime &&
              xworkRuntimeReplaceToolsBySource(pRuntime, "test.catalog", pInitial, 2u, &iRemoved,
                                               &tError) &&
              iRemoved == 0u,
          "runtime publishes an initial catalog generation atomically");
    pOldRuntime = xworkRuntimeToolCatalogSnapshot(pRuntime);
    CHECK(pOldRuntime && xworkToolCatalogCount(pOldRuntime) == 2u,
          "runtime catalog snapshot owns a complete immutable generation");
    memset(&tReader, 0, sizeof(tReader));
    tReader.pRuntime = pRuntime;
    pReaderThread = xrtThreadCreate(catalog_reader_thread, &tReader, 0u);
    uReaderDeadline = xllmDeadlineAfterUs(UINT64_C(2000000));
    while (pReaderThread && xwork__atomic_load(&tReader.iReady) == 0 &&
           !xllmDeadlineExpired(uReaderDeadline))
    {
        mdo_test_sleep_ms(1u);
    }
    iRemoved = 0u;
    CHECK(pRuntime &&
              xworkRuntimeReplaceToolsBySource(pRuntime, "test.catalog", &tReplacement, 1u,
                                               &iRemoved, &tError) &&
              iRemoved == 2u,
          "source replacement removes and publishes in one generation");
    xwork__atomic_store(&tReader.iPublished, 1);
    if (pReaderThread)
    {
        (void)xrtThreadWait(pReaderThread);
        xrtThreadDestroy(pReaderThread);
    }
    CHECK(pReaderThread && xwork__atomic_load(&tReader.iReady) != 0 &&
              xwork__atomic_load(&tReader.iValid) != 0,
          "fixed reader/publisher interleaving preserves the reader generation");
    pNewRuntime = xworkRuntimeToolCatalogSnapshot(pRuntime);
    CHECK(pOldRuntime && pNewRuntime &&
              xworkToolCatalogGeneration(pNewRuntime) > xworkToolCatalogGeneration(pOldRuntime) &&
              xworkToolCatalogCount(pOldRuntime) == 2u && xworkToolCatalogCount(pNewRuntime) == 1u,
          "new publication does not mutate a retained old generation");
    memset(&tInfo, 0, sizeof(tInfo));
    CHECK(pNewRuntime && xworkToolCatalogToolAt(pNewRuntime, 0u, &tInfo) && tInfo.bParallelSafe &&
              tInfo.sSerialGroup && strcmp(tInfo.sSerialGroup, "catalog.parallel") == 0,
          "source replacement publishes scheduling metadata atomically");
    pDuplicate[0] = tReplacement;
    pDuplicate[1] = tReplacement;
    uFailedGeneration = xworkToolCatalogGeneration(pNewRuntime);
    CHECK(pRuntime &&
              !xworkRuntimeReplaceToolsBySource(pRuntime, "test.catalog", pDuplicate, 2u, NULL,
                                                &tError) &&
              (pFailedRuntime = xworkRuntimeToolCatalogSnapshot(pRuntime)) != NULL &&
              xworkToolCatalogGeneration(pFailedRuntime) == uFailedGeneration,
          "invalid replacement leaves the current generation unchanged");
    xworkToolCatalogRelease(pFailedRuntime);

    (void)xrtDirRemoveAll(sWorkspace);
    CHECK(xrtDirCreateAll((str)sWorkspace), "catalog adoption workspace created");
    xworkAgentDefinitionConfigInit(&tDefinitionConfig);
    tDefinitionConfig.sId = "test.catalog.agent";
    tDefinitionConfig.bRegisterBuiltinTools = false;
    tDefinitionConfig.bAutoSaveSession = false;
    tDefinitionConfig.bRequireVerificationAfterWrite = false;
    pDefinition = xworkAgentDefinitionCreate(&tDefinitionConfig, &tError);
    xllmSessionConfigInit(&tSessionConfig);
    pSession = xllmSessionCreate(&tSessionConfig, NULL);
    xworkAgentOptionsInit(&tOptions);
    tOptions.pSession = pSession;
    tOptions.sWorkspaceRoot = sWorkspace;
    pAgent = xworkAgentCreateWithRuntime(pRuntime, pDefinition, &tOptions, &tError);
    pOldAgent = xworkAgentToolCatalogSnapshot(pAgent);
    CHECK(pAgent && pOldAgent && xworkAgentToolCount(pAgent) == 1u &&
              xworkToolCatalogToolAt(pOldAgent, 0u, &tInfo) &&
              strcmp(tInfo.sName, "catalog.gamma") == 0 &&
              tInfo.uCatalogGeneration == xworkToolCatalogGeneration(pOldAgent),
          "new agent pins the runtime catalog generation it was created with");

    CHECK(pRuntime &&
              xworkRuntimeReplaceToolsBySource(pRuntime, "test.catalog", pInitial, 2u, NULL,
                                               &tError) &&
              xworkAgentToolCount(pAgent) == 1u,
          "runtime catalog updates do not rewrite a live agent implicitly");
    pLimitRuntime = xworkRuntimeToolCatalogSnapshot(pRuntime);
    CHECK(pLimitRuntime && !xworkRuntimeRegisterTool(pRuntime, &tReplacement, &tError) &&
              tError.eCode == XWORK_ERROR_LIMIT &&
              (pAfterLimitRuntime = xworkRuntimeToolCatalogSnapshot(pRuntime)) != NULL &&
              xworkToolCatalogGeneration(pLimitRuntime) ==
                  xworkToolCatalogGeneration(pAfterLimitRuntime),
          "catalog limit rejection leaves the published generation unchanged");
    xworkToolCatalogRelease(pLimitRuntime);
    xworkToolCatalogRelease(pAfterLimitRuntime);
    CHECK(pAgent && xworkAgentAdoptRuntimeToolCatalog(pAgent, &tError) &&
              xworkAgentToolCount(pAgent) == 2u,
          "idle agent explicitly adopts the latest runtime generation");
    pCurrentAgent = xworkAgentToolCatalogSnapshot(pAgent);

    xworkAgentDestroy(pAgent);
    xllmSessionDestroy(pSession);
    xworkAgentDefinitionRelease(pDefinition);
    xworkRuntimeRelease(pRuntime);
    CHECK(pOldRuntime && pNewRuntime && pOldAgent && pCurrentAgent &&
              xworkToolCatalogCount(pOldRuntime) == 2u &&
              xworkToolCatalogCount(pNewRuntime) == 1u && xworkToolCatalogCount(pOldAgent) == 1u &&
              xworkToolCatalogCount(pCurrentAgent) == 2u,
          "retained generations remain readable after runtime and agent destruction");
    xworkToolCatalogRelease(pOldRuntime);
    xworkToolCatalogRelease(pNewRuntime);
    xworkToolCatalogRelease(pOldAgent);
    xworkToolCatalogRelease(pCurrentAgent);
    (void)xrtDirRemoveAll(sWorkspace);
}

static void test_v3_tool_owner_lifetime(void)
{
    xwork_runtime_config tRuntimeConfig;
    xwork_runtime *pRuntime;
    xwork_tool_catalog *pOldCatalog = NULL;
    xwork_tool_catalog *pNewCatalog = NULL;
    xwork_tool_catalog *pAfterFailure = NULL;
    xwork_tool_catalog *pReboundCatalog = NULL;
    xwork_tool_definition tOld;
    xwork_tool_definition tNew;
    xwork_tool_definition tFailing;
    xwork_tool_definition tMalformed;
    xwork_error tError;
    tool_owner_fixture tOldOwner = {1u, 0u, 0u, true};
    tool_owner_fixture tNewOwner = {1u, 0u, 0u, true};
    tool_owner_fixture tFailingOwner = {1u, 0u, 0u, false};
    tool_owner_fixture tReboundOwner = {1u, 0u, 0u, true};
    uint64_t uGeneration;

    memset(&tOld, 0, sizeof(tOld));
    tOld.sName = "owner.old";
    tOld.sDescription = "old dynamically owned callback";
    tOld.sParametersJson = "{\"type\":\"object\"}";
    tOld.uEffects = XWORK_TOOL_EFFECT_READ_ONLY;
    tOld.OnExecute = registry_probe_execute;
    tOld.pUserData = &tOldOwner;
    tOld.sSource = "test.owner";
    tOld.OnOwnerRetain = tool_owner_retain;
    tOld.OnOwnerRelease = tool_owner_release;
    tNew = tOld;
    tNew.sName = "owner.new";
    tNew.sDescription = "new dynamically owned callback";
    tNew.pUserData = &tNewOwner;
    tFailing = tOld;
    tFailing.sName = "owner.failing";
    tFailing.sDescription = "owner that refuses a new catalog reference";
    tFailing.pUserData = &tFailingOwner;
    tFailing.sSource = "test.failure";
    tMalformed = tFailing;
    tMalformed.OnOwnerRelease = NULL;

    xworkRuntimeConfigInit(&tRuntimeConfig);
    pRuntime = xworkRuntimeCreate(&tRuntimeConfig, &tError);
    CHECK(pRuntime &&
              xworkRuntimeReplaceToolsBySource(pRuntime, "test.owner", &tOld, 1u, NULL, &tError) &&
              tOldOwner.iRefs == 2u && tOldOwner.iRetainCalls == 1u,
          "published dynamic tool catalog retains its callback owner");
    pOldCatalog = xworkRuntimeToolCatalogSnapshot(pRuntime);
    uGeneration = xworkToolCatalogGeneration(pOldCatalog);

    CHECK(pRuntime &&
              !xworkRuntimeReplaceToolsBySource(pRuntime, "test.failure", &tFailing, 1u, NULL,
                                                &tError) &&
              tError.eCode == XWORK_ERROR_CONTEXT &&
              (pAfterFailure = xworkRuntimeToolCatalogSnapshot(pRuntime)) != NULL &&
              xworkToolCatalogGeneration(pAfterFailure) == uGeneration && tOldOwner.iRefs == 2u &&
              tOldOwner.iRetainCalls == 2u && tOldOwner.iReleaseCalls == 1u &&
              tFailingOwner.iRefs == 1u && tFailingOwner.iRetainCalls == 1u &&
              tFailingOwner.iReleaseCalls == 0u,
          "failed owner retain rolls back the candidate and preserves publication");
    xworkToolCatalogRelease(pAfterFailure);
    pAfterFailure = NULL;
    pReboundCatalog = xwork__tool_catalog_filter_subagent(pOldCatalog, uGeneration + 100u, NULL, 0u,
                                                          XWORK_TOOL_EFFECT_ALL, &tOldOwner,
                                                          &tReboundOwner, &tError);
    CHECK(pReboundCatalog && tOldOwner.iRefs == 2u && tOldOwner.iRetainCalls == 3u &&
              tOldOwner.iReleaseCalls == 2u && tReboundOwner.iRefs == 2u &&
              tReboundOwner.iRetainCalls == 1u,
          "derived catalog transfers a managed user-data owner reference");
    xworkToolCatalogRelease(pReboundCatalog);
    pReboundCatalog = NULL;
    CHECK(tReboundOwner.iRefs == 1u && tReboundOwner.iReleaseCalls == 1u,
          "rebound owner releases with its derived catalog");
    CHECK(pRuntime &&
              !xworkRuntimeReplaceToolsBySource(pRuntime, "test.failure", &tMalformed, 1u, NULL,
                                                &tError) &&
              tError.eCode == XWORK_ERROR_INVALID_ARGUMENT && tFailingOwner.iRetainCalls == 1u,
          "tool owner retain and release callbacks must be paired");

    CHECK(pRuntime &&
              xworkRuntimeReplaceToolsBySource(pRuntime, "test.owner", &tNew, 1u, NULL, &tError) &&
              tOldOwner.iRefs == 2u && tNewOwner.iRefs == 2u,
          "source replacement keeps a retained old snapshot owner alive");
    pNewCatalog = xworkRuntimeToolCatalogSnapshot(pRuntime);
    xworkRuntimeRelease(pRuntime);
    pRuntime = NULL;
    CHECK(tOldOwner.iRefs == 2u && tNewOwner.iRefs == 2u,
          "runtime release cannot unload owners pinned by catalog snapshots");
    xworkToolCatalogRelease(pOldCatalog);
    pOldCatalog = NULL;
    CHECK(tOldOwner.iRefs == 1u && tOldOwner.iReleaseCalls == 3u,
          "old callback owner releases after its last catalog snapshot");
    xworkToolCatalogRelease(pNewCatalog);
    pNewCatalog = NULL;
    CHECK(tNewOwner.iRefs == 1u && tNewOwner.iReleaseCalls == 1u,
          "current callback owner releases after its last catalog snapshot");
}

static xllm_response *mock_response(const char *sContent, size_t iToolCount)
{
    xllm_response *pResponse = (xllm_response *)calloc(1u, sizeof(*pResponse));
    if (!pResponse)
        return NULL;
    pResponse->sContent = test_strdup(sContent ? sContent : "");
    pResponse->sModel = test_strdup("mock-model");
    pResponse->sRequestId = test_strdup("mock-request-id");
    pResponse->sFinishReason = test_strdup(iToolCount ? "tool_calls" : "stop");
    if (iToolCount)
        pResponse->pToolCalls =
            (xllm_tool_call *)calloc(iToolCount, sizeof(*pResponse->pToolCalls));
    if (!pResponse->sContent || !pResponse->sModel || !pResponse->sRequestId ||
        !pResponse->sFinishReason || (iToolCount && !pResponse->pToolCalls))
    {
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

static bool mock_set_call(xllm_response *pResponse, size_t i, const char *sId, const char *sName,
                          const char *sArgs)
{
    pResponse->pToolCalls[i].sId = test_strdup(sId);
    pResponse->pToolCalls[i].sName = test_strdup(sName);
    pResponse->pToolCalls[i].sArgumentsJson = test_strdup(sArgs);
    return pResponse->pToolCalls[i].sId && pResponse->pToolCalls[i].sName &&
           pResponse->pToolCalls[i].sArgumentsJson;
}

static bool request_has_role(const xllm_request *pRequest, xllm_role eRole, size_t iAtLeast)
{
    size_t i;
    size_t iCount = 0u;
    for (i = 0u; i < pRequest->iMessageCount; ++i)
    {
        if (pRequest->pMessages[i].eRole == eRole)
            ++iCount;
    }
    return iCount >= iAtLeast;
}

static bool request_has_text(const xllm_request *pRequest, const char *sNeedle)
{
    size_t i;
    for (i = 0u; i < pRequest->iMessageCount; ++i)
    {
        const char *sText = pRequest->pMessages[i].sContent;
        if (sText && strstr(sText, sNeedle))
            return true;
    }
    return false;
}

typedef struct lazy_mcp_model
{
    uint32_t uCalls;
    bool bSawCompactDiscovery;
    bool bSawLoadedSchema;
    bool bSawRemoteResult;
} lazy_mcp_model;

static bool request_has_tool(const xllm_request *pRequest, const char *sName)
{
    size_t i;
    for (i = 0u; pRequest && i < pRequest->iToolCount; ++i)
    {
        if (pRequest->pTools[i].sName && strcmp(pRequest->pTools[i].sName, xwork__canonical_tool_name(sName)) == 0)
            return true;
    }
    return false;
}

static xllm_result lazy_mcp_complete(void *pUserData, const xllm_request *pRequest,
                                     const xllm_stream_callbacks *pCallbacks,
                                     xllm_response **ppResponse, xllm_error *pError)
{
    lazy_mcp_model *pModel = (lazy_mcp_model *)pUserData;
    xllm_response *pResponse = NULL;
    (void)pCallbacks;
    (void)pError;
    *ppResponse = NULL;
    if (!pModel || !pRequest)
        return XLLM_RESULT_ERROR;
    ++pModel->uCalls;
    if (pModel->uCalls == 1u)
    {
        pModel->bSawCompactDiscovery = request_has_tool(pRequest, "tool_search") &&
                                       request_has_tool(pRequest, "tool_load") &&
                                       !request_has_tool(pRequest, "mcp__lazy__echo");
        pResponse = mock_response("", 1u);
        if (!pResponse || !mock_set_call(pResponse, 0u, "lazy-load-1", "tool_load",
                                         "{\"server\":\"lazy\",\"tool\":\"echo\"}"))
        {
            xllmResponseDestroy(pResponse);
            return XLLM_RESULT_ERROR;
        }
    }
    else if (pModel->uCalls == 2u)
    {
        size_t i;
        pModel->bSawLoadedSchema = false;
        for (i = 0u; i < pRequest->iToolCount; ++i)
        {
            if (pRequest->pTools[i].sName &&
                strcmp(pRequest->pTools[i].sName, "mcp__lazy__echo") == 0 &&
                pRequest->pTools[i].sParametersJson &&
                strstr(pRequest->pTools[i].sParametersJson, "required"))
            {
                pModel->bSawLoadedSchema = true;
                break;
            }
        }
        pResponse = mock_response("", 1u);
        if (!pResponse || !mock_set_call(pResponse, 0u, "lazy-call-2", "mcp__lazy__echo",
                                         "{\"text\":\"hello lazy mcp\"}"))
        {
            xllmResponseDestroy(pResponse);
            return XLLM_RESULT_ERROR;
        }
    }
    else
    {
        pModel->bSawRemoteResult = request_has_text(pRequest, "echo: hello mcp");
        pResponse = mock_response("lazy MCP completed", 0u);
    }
    *ppResponse = pResponse;
    return pResponse ? XLLM_RESULT_OK : XLLM_RESULT_ERROR;
}

static xllm_result subagent_complete(void *pUserData, const xllm_request *pRequest,
                                     const xllm_stream_callbacks *pCallbacks,
                                     xllm_response **ppResponse, xllm_error *pError)
{
    static const char sLongFinal[] =
        "Evidence report: the readable workspace file was inspected and the internal control "
        "directory was denied as required. "
        "The child received only the read tool, performed no writes or process execution, and "
        "cannot delegate recursively. "
        "Recommended next action: let the parent agent use this bounded evidence while retaining "
        "authority for every mutation. "
        "Additional padding verifies that the host byte budget truncates this final response "
        "deterministically without creating an artifact.";
    subagent_mock *pMock = (subagent_mock *)pUserData;
    xllm_response *pResponse;
    size_t i;
    bool bNames = pRequest && pRequest->iToolCount == 4u;
    (void)pCallbacks;
    (void)pError;
    *ppResponse = NULL;
    if (!pRequest || !pMock)
        return XLLM_RESULT_ERROR;
    for (i = 0u; bNames && i < pRequest->iToolCount; ++i)
    {
        const char *sName = pRequest->pTools[i].sName;
        if (strcmp(sName, "read") != 0 && strcmp(sName, "ls") != 0 && strcmp(sName, "glob") != 0 &&
            strcmp(sName, "grep") != 0)
        {
            bNames = false;
        }
    }
    pMock->bReadOnlyToolSet = pMock->bReadOnlyToolSet || bNames;
    ++pMock->uModelCalls;
    if (pMock->uModelCalls == 1u)
    {
        pResponse = mock_response("", 2u);
        if (!pResponse ||
            !mock_set_call(pResponse, 0u, "sub_read", "read",
                           "{\"path\":\"evidence.txt\",\"max_lines\":100}") ||
            !mock_set_call(pResponse, 1u, "sub_internal", "read",
                           "{\"path\":\".xcode/secrets.local.json\",\"max_lines\":20}"))
        {
            xllmResponseDestroy(pResponse);
            return XLLM_RESULT_ERROR;
        }
    }
    else
    {
        pMock->bSawReadableFile = request_has_text(pRequest, "readonly-evidence-marker") &&
                                  request_has_text(pRequest, "artifact writes disabled");
        pMock->bSawInternalDenial = request_has_text(pRequest, "denied by approval policy");
        pResponse = mock_response(sLongFinal, 0u);
    }
    *ppResponse = pResponse;
    return pResponse ? XLLM_RESULT_OK : XLLM_RESULT_ERROR;
}

static bool subagent_event(void *pUserData, const xwork_event *pEvent)
{
    subagent_mock *pMock = (subagent_mock *)pUserData;
    if (!pMock || !pEvent)
        return false;
    if (pEvent->uAgentDepth != 1u || pEvent->uDelegationId != 1u || pEvent->uParentAgentTurn != 42u)
        pMock->bScopedEvents = false;
    if (pEvent->eKind == XWORK_EVENT_AGENT_START)
        ++pMock->uAgentStarts;
    if (pEvent->eKind == XWORK_EVENT_AGENT_DONE)
        ++pMock->uAgentDone;
    if (pEvent->eKind == XWORK_EVENT_TOOL_START)
        ++pMock->uToolStarts;
    if (pEvent->eKind == XWORK_EVENT_TOOL_DONE)
        ++pMock->uToolDone;
    return true;
}

static xllm_result mock_complete(void *pUserData, const xllm_request *pRequest,
                                 const xllm_stream_callbacks *pCallbacks,
                                 xllm_response **ppResponse, xllm_error *pError)
{
    mock_model *pMock = (mock_model *)pUserData;
    xllm_response *pResponse = NULL;
    char *sLargeArgs = NULL;
    size_t i;
    (void)pError;
    *ppResponse = NULL;
    if (pRequest->pCancel || pRequest->uDeadline != XLLM_DEADLINE_NEVER)
        pMock->bSawContext = true;
    if (pRequest->iToolCount == 0u)
    {
        ++pMock->uCompactionCalls;
        if (pMock->uCompactionCalls == 1u)
        {
            pResponse = mock_response("Objective: incomplete first compaction candidate.", 0u);
        }
        else
        {
            pResponse =
                mock_response("Objective: test the xwork tool loop after compaction and finish the "
                              "requested file workflow.\n"
                              "Constraints: remain inside the workspace, preserve tool-call "
                              "pairing, and verify every write.\n"
                              "Architecture and decisions: use a durable session checkpoint while "
                              "retaining the newest complete turn verbatim.\n"
                              "Completed work: old synthetic rounds were inspected and their "
                              "successful outcomes were retained.\n"
                              "Current repository state: sandbox paths are active and the "
                              "requested note workflow remains ready to execute.\n"
                              "Verification evidence: the compacted prefix contains only completed "
                              "turns with no unresolved tool calls.\n"
                              "Open issues and risks: the requested edits and command verification "
                              "are not complete yet.\n"
                              "Exact next actions: execute the requested file tools, inspect "
                              "results, run verification, and then report completion.",
                              0u);
        }
    }
    else
    {
        if (!pMock->bRegistryMutationBlocked && pMock->pAgent)
        {
            xwork_error tRegistryError;
            xworkErrorInit(&tRegistryError);
            pMock->bRegistryMutationBlocked =
                !xworkAgentUnregisterTool(pMock->pAgent, "read", &tRegistryError) &&
                tRegistryError.eCode == XWORK_ERROR_CONTEXT;
        }
        if (!pMock->bCatalogAdoptionBlocked && pMock->pAgent)
        {
            xwork_error tCatalogError;
            xworkErrorInit(&tCatalogError);
            pMock->bCatalogAdoptionBlocked =
                !xworkAgentAdoptRuntimeToolCatalog(pMock->pAgent, &tCatalogError) &&
                tCatalogError.eCode == XWORK_ERROR_CONTEXT;
        }
        ++pMock->uAgentCalls;
        pMock->bSawTools = pRequest->iToolCount == 12u;
        pMock->bSawParallel = pRequest->bParallelToolCalls;
        if (pMock->uAgentCalls > 1u && request_has_role(pRequest, XLLM_ROLE_TOOL, 1u))
            pMock->bSawToolResults = true;
        if (request_has_text(pRequest, "Objective: test the xwork tool loop after compaction"))
            pMock->bSawCompactionSummary = true;
        if (request_has_text(pRequest, "Completion verification gate"))
            pMock->bSawVerificationGate = true;
        if (pMock->uAgentCalls == 1u)
        {
            xwork_buf tArgs = {0};
            pResponse = mock_response("", 6u);
            if (!pResponse)
                return XLLM_RESULT_ERROR;
            if (!xwork__buf_append_cstr(&tArgs,
                                        "{\"path\":\"sandbox/note.txt\",\"content\":\"hello-"))
                goto oom;
            for (i = 0u; i < 1600u; ++i)
                if (!xwork__buf_append_char(&tArgs, (char)('a' + (i % 26u))))
                    goto oom;
            if (!xwork__buf_append_cstr(&tArgs, "note tail\",\"mode\":\"create\"}"))
                goto oom;
            sLargeArgs = xwork__buf_detach(&tArgs);
            if (!sLargeArgs || !mock_set_call(pResponse, 0u, "call_write", "write", sLargeArgs) ||
                !mock_set_call(pResponse, 1u, "call_read", "read",
                               "{\"path\":\"sandbox/note.txt\",\"max_lines\":20}") ||
                !mock_set_call(pResponse, 2u, "call_escape", "read",
                               "{\"path\":\"../outside.txt\"}") ||
                !mock_set_call(pResponse, 3u, "call_list", "read",
                               "{\"path\":\"sandbox/note.txt\",\"max_lines\":5}") ||
                !mock_set_call(
                    pResponse, 4u, "call_search", "read",
                    "{\"path\":\"sandbox/note.txt\",\"start_line\":2,\"max_lines\":3}") ||
                !mock_set_call(
                    pResponse, 5u, "call_replace", "edit",
                    "{\"path\":\"sandbox/"
                    "note.txt\",\"edits\":[{\"old_text\":\"hello-\",\"new_text\":\"HELLO-\"}]}"))
                goto oom;
        }
        else if (pMock->uAgentCalls == 2u)
        {
            pResponse = mock_response("", 1u);
            if (!pResponse ||
                !mock_set_call(
                    pResponse, 0u, "call_patch", "edit",
                    "{\"path\":\"sandbox/"
                    "note.txt\",\"edits\":[{\"old_text\":\"HELLO-\",\"new_text\":\"PATCHED-\"},{"
                    "\"old_text\":\"note tail\",\"new_text\":\"EDITED tail\"}]}"))
                goto oom;
        }
        else if (pMock->uAgentCalls == 3u)
        {
            pResponse = mock_response("The requested edits are complete.", 0u);
        }
        else if (pMock->uAgentCalls == 4u)
        {
#if defined(_WIN32) || defined(_WIN64)
            static const char sVerifyArgs[] =
                "{\"argv\":[\"cmd\",\"/c\",\"type\",\"sandbox\\\\note.txt\"],\"timeout_ms\":10000}";
#else
            static const char sVerifyArgs[] =
                "{\"argv\":[\"cat\",\"sandbox/note.txt\"],\"timeout_ms\":10000}";
#endif
            pResponse = mock_response("", 1u);
            if (!pResponse || !mock_set_call(pResponse, 0u, "call_exec", "exec", sVerifyArgs))
                goto oom;
        }
        else
        {
            pResponse = mock_response(
                "Implemented, inspected, edited, and verified the workspace file successfully.",
                0u);
        }
    }
    free(sLargeArgs);
    if (!pResponse)
        return XLLM_RESULT_ERROR;
    if (pCallbacks && pCallbacks->OnEvent && pResponse->sContent && pResponse->sContent[0])
    {
        xllm_event tEvent;
        memset(&tEvent, 0, sizeof(tEvent));
        tEvent.eKind = XLLM_EVENT_TEXT_DELTA;
        tEvent.as.tText.sData = pResponse->sContent;
        tEvent.as.tText.iLen = strlen(pResponse->sContent);
        if (!pCallbacks->OnEvent(pCallbacks->pUserData, &tEvent))
        {
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

static bool on_event(void *pUserData, const xwork_event *pEvent)
{
    test_events *pEvents = (test_events *)pUserData;
    if (pEvent->uSchemaVersion == XWORK_EVENT_SCHEMA_VERSION && pEvent->uEventId != 0u &&
        pEvent->iOccurredAtUs > 0 && pEvent->uAgentId != 0u)
    {
        pEvents->bRuntimeIdentity = true;
    }
    switch (pEvent->eKind)
    {
    case XWORK_EVENT_AGENT_START:
        pEvents->uLastUserMessageSequence = pEvent->uUserMessageSequence;
        break;
    case XWORK_EVENT_MODEL_START:
        ++pEvents->uModelStarts;
        if (pEvent->sModel && strcmp(pEvent->sModel, "mock-model") == 0 &&
            pEvent->sRequestFingerprint && strlen(pEvent->sRequestFingerprint) == 16u &&
            pEvent->iMessageCount > 0u && pEvent->iToolDefinitionCount == 12u &&
            pEvent->uMaxOutputTokens > 0u)
            pEvents->bRequestMetadata = true;
        break;
    case XWORK_EVENT_MODEL_TEXT_DELTA:
        ++pEvents->uTextDeltas;
        break;
    case XWORK_EVENT_MODEL_DONE:
        ++pEvents->uModelDone;
        if (pEvent->sModel && strcmp(pEvent->sModel, "mock-model") == 0 &&
            pEvent->sProviderRequestId &&
            strcmp(pEvent->sProviderRequestId, "mock-request-id") == 0 && pEvent->sFinishReason &&
            pEvent->uHttpStatus == 200u && pEvent->tDiagnostics.uAttemptCount == 1u &&
            pEvent->tDiagnostics.uTotalDurationMs == 7u)
        {
            pEvents->bResponseMetadata = true;
        }
        break;
    case XWORK_EVENT_TOOL_START:
        ++pEvents->uToolStarts;
        break;
    case XWORK_EVENT_TOOL_DONE:
        ++pEvents->uToolDone;
        if (pEvent->bSuccess)
            ++pEvents->uToolSuccess;
        else
            ++pEvents->uToolFailure;
        if (pEvent->sArtifactPath)
        {
            pEvents->uLastArtifactId = pEvent->uArtifactId;
            snprintf(pEvents->sLastArtifact, sizeof(pEvents->sLastArtifact), "%s",
                     pEvent->sArtifactPath);
        }
        break;
    case XWORK_EVENT_COMPACTION_REJECTED:
        ++pEvents->uRejectedCompactions;
        if (pEvent->uCompactionAttempt == 1u && pEvent->tCompactionQuality.uMissingSections != 0u &&
            !pEvent->tCompactionQuality.bAccepted)
            pEvents->bCompactionQualityMetadata = true;
        break;
    case XWORK_EVENT_COMPACTION_DONE:
        ++pEvents->uCompactions;
        break;
    case XWORK_EVENT_ERROR:
        ++pEvents->uErrors;
        break;
    case XWORK_EVENT_RECOVERY_REQUIRED:
        ++pEvents->uRecoveryRequired;
        break;
    case XWORK_EVENT_RECOVERY_RESOLVED:
        ++pEvents->uRecoveryResolved;
        if (pEvent->bRecoveryAutomatic)
            ++pEvents->uRecoveryAutomatic;
        if (pEvent->bRecoveryUncertain)
            ++pEvents->uRecoveryUncertain;
        break;
    default:
        break;
    }
    return true;
}

static xwork_permission_decision on_permission(void *pUserData,
                                               const xwork_permission_request *pRequest)
{
    mock_model *pMock = (mock_model *)pUserData;
    size_t i;
    ++pMock->uPermissionCalls;
    for (i = 0u; i < pRequest->iResourceCount; ++i)
    {
        const xwork_permission_resource *pResource = &pRequest->pResources[i];
        if (pResource->eKind == XWORK_RESOURCE_PATH && pResource->sResource &&
            strstr(pResource->sResource, "sandbox"))
        {
            pMock->bSawPathPermission = true;
        }
        if (pResource->eKind == XWORK_RESOURCE_COMMAND && pResource->sResource &&
            strstr(pResource->sResource, "sandbox"))
        {
            pMock->bSawCommandPermission = true;
        }
    }
    if (pRequest->eRisk == XWORK_RISK_HIGH)
        pMock->bSawHighRiskPermission = true;
    return XWORK_PERMISSION_DEFAULT;
}

static xwork_hook_action on_hook(void *pUserData, const xwork_hook_event *pEvent)
{
    mock_model *pMock = (mock_model *)pUserData;
    if (pEvent->ePhase == XWORK_HOOK_BEFORE_TOOL)
        ++pMock->uBeforeToolHooks;
    else if (pEvent->ePhase == XWORK_HOOK_AFTER_TOOL)
    {
        ++pMock->uAfterToolHooks;
        if (!pEvent->sOutput)
            return XWORK_HOOK_CANCEL;
    }
    return XWORK_HOOK_CONTINUE;
}

static char *make_text(size_t iSize, char ch)
{
    char *sText = (char *)malloc(iSize + 1u);
    if (!sText)
        return NULL;
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
    static const unsigned char sLocalBytes[] = {0xCFu, 0xB5u, 0xCDu, 0xB3u, 0xD5u, 0xD2u,
                                                0xB2u, 0xBBu, 0xB5u, 0xBDu, 0xD6u, 0xB8u,
                                                0xB6u, 0xA8u, 0xB5u, 0xC4u};
    static const unsigned char sBinary[] = {'a', 0u, 0xFFu, 'b'};
    xwork_buf tText = {0};

    CHECK(xrtUtf8Valid((xstrview){"valid UTF-8: \xE4\xB8\xAD\xE6\x96\x87", 19u}, NULL),
          "strict UTF-8 accepts valid Unicode text");
    CHECK(!xrtUtf8Valid((xstrview){(const char *)sOverlong, sizeof(sOverlong)}, NULL),
          "strict UTF-8 rejects overlong encodings");
    CHECK(!xrtUtf8Valid((xstrview){(const char *)sSurrogate, sizeof(sSurrogate)}, NULL),
          "strict UTF-8 rejects surrogate code points");
    CHECK(!xrtUtf8Valid((xstrview){(const char *)sTooHigh, sizeof(sTooHigh)}, NULL),
          "strict UTF-8 rejects code points above U+10FFFF");
    CHECK(!xrtUtf8Valid((xstrview){(const char *)sTruncated, sizeof(sTruncated)}, NULL),
          "strict UTF-8 rejects truncated sequences");

    CHECK(xwork__buf_append_process_text(&tText, sLocalBytes, sizeof(sLocalBytes)),
          "local process bytes normalize without loss of control flow");
    CHECK(tText.pData && xrtUtf8Valid((xstrview){tText.pData, tText.iLen}, NULL),
          "normalized process output is valid UTF-8");
    xwork__buf_unit(&tText);

    CHECK(xwork__buf_append_process_text(&tText, sBinary, sizeof(sBinary)),
          "binary-like process output is safely represented");
    CHECK(tText.pData && strstr(tText.pData, "\\x00") && strstr(tText.pData, "\\xFF"),
          "binary bytes are escaped instead of entering JSON");
    CHECK(tText.pData && xrtUtf8Valid((xstrview){tText.pData, tText.iLen}, NULL),
          "escaped binary representation remains valid UTF-8");
    xwork__buf_unit(&tText);
}

static void test_agent_loop(void)
{
    static const char sWorkspace[] = "tests/tmp_xwork";
    static const char sSessionPath[] = "tests/tmp_xwork/.xcode/session.json";
    xllm_session_config tSessionConfig;
    xllm_session *pSession;
    xllm_error tLlmError;
    xwork_agent_config tAgentConfig;
    xwork_agent *pAgent;
    xwork_mcp_client *pMcpClient = NULL;
    xcancel *pCancel = NULL;
    xwork_error tError;
    xwork_run_result tResult;
    mock_model tMock;
    test_events tEvents;
    char *sOld = make_text(1300u, 'x');
    char *sFile = NULL;
    char *sArtifactAbsolute = NULL;
    size_t iFileSize = 0u;
    uint32_t uCompactionsBeforeExplicit = 0u;
    const xwork_tool_entry *pPatchTool;
    const xwork_tool_entry *pStartTool;
    const xwork_tool_entry *pWriteProcessTool;
    const xwork_tool_entry *pPollTool;
    const xwork_tool_entry *pExecTool;
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
    if (!pSession || !sOld)
        goto cleanup;
    CHECK(xllmSessionAddText(pSession, 0u, XLLM_ROLE_SYSTEM, "Pinned test agent contract.",
                             XLLM_SESSION_ENTRY_PINNED),
          "pinned system message added");
    for (i = 0u; i < 7u; ++i)
    {
        uint64_t uTurn = xllmSessionBeginTurn(pSession);
        CHECK(xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, sOld, 0u),
              "old user context added");
        CHECK(xllmSessionAddText(pSession, uTurn, XLLM_ROLE_ASSISTANT, sOld, 0u),
              "old assistant context added");
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
    CHECK(pAgent && xworkAgentToolCount(pAgent) == 12u,
          "twelve practical builtin tools registered");
    if (!pAgent)
        goto cleanup;
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
        tDynamic.uEffects = XWORK_TOOL_EFFECT_READ_ONLY;
        tDynamic.OnExecute = registry_probe_execute;
        tDynamic.sSource = "test.dynamic";
        CHECK(xworkAgentToolAt(pAgent, 0u, &tInfo) && tInfo.sSource &&
                  strcmp(tInfo.sSource, "builtin") == 0,
              "tool registry enumeration exposes stable source metadata");
        CHECK(xworkAgentRegisterTool(pAgent, &tDynamic, &tError) &&
                  xworkAgentToolCount(pAgent) == 13u &&
                  xworkAgentToolRegistryGeneration(pAgent) == uGeneration + 1u,
              "dynamic tool registration advances the registry generation");
        xworkErrorInit(&tError);
        CHECK(xworkAgentUnregisterToolsBySource(pAgent, "test.dynamic", &iRemoved, &tError) &&
                  iRemoved == 1u && xworkAgentToolCount(pAgent) == 12u &&
                  xworkAgentToolRegistryGeneration(pAgent) == uGeneration + 2u,
              "bulk source removal atomically retires dynamic tools");
    }

    {
        const char *arrMcpArgs[] = {"--mcp-test-server"};
        xwork_mcp_stdio_config tMcpConfig;
        xwork_mcp_info tMcpInfo;
        const xwork_tool_entry *pMcpTool;
        const xwork_tool_entry *pOldMcpTool = NULL;
        xwork_tool_catalog *pOldMcpCatalog = NULL;
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
                  xworkAgentToolCount(pAgent) == 13u,
              "MCP tools/list dynamically registers namespaced proxy tools");
        pOldMcpCatalog = xworkAgentToolCatalogSnapshot(pAgent);
        if (pOldMcpCatalog)
        {
            size_t iTool;
            for (iTool = 0u; iTool < pOldMcpCatalog->iToolCount; ++iTool)
            {
                if (strcmp(pOldMcpCatalog->pTools[iTool].sName, "mcp__phase3__echo") == 0)
                {
                    pOldMcpTool = &pOldMcpCatalog->pTools[iTool];
                    break;
                }
            }
        }
        CHECK(pOldMcpCatalog && pMcpClient &&
                  xworkMcpClientRefreshTools(pMcpClient, pAgent, &tError) &&
                  xworkAgentToolRegistryGeneration(pAgent) >
                      xworkToolCatalogGeneration(pOldMcpCatalog) &&
                  xworkAgentToolCount(pAgent) == 13u,
              "MCP refresh publishes one complete replacement generation");
        pMcpTool = xwork__find_tool(pAgent, "mcp__phase3__echo");
        memset(&tMcpToolContext, 0, sizeof(tMcpToolContext));
        tMcpToolContext.pAgent = pAgent;
        tMcpToolContext.sWorkspaceRoot = sWorkspace;
        xworkToolOutputInit(&tMcpOutput);
        CHECK(pMcpTool && strcmp(pMcpTool->sSource, "mcp:phase3") == 0 &&
                  pMcpTool->uEffects == XWORK_TOOL_EFFECT_EXTERNAL_SERVICE,
              "MCP proxies retain source ownership and distrust read-only hints by default");
        CHECK(pOldMcpTool &&
                  test_entry_execute(pOldMcpTool, pOldMcpTool->pUserData, &tMcpToolContext,
                                         "{\"text\":\"hello old generation\"}", &tMcpOutput,
                                         &tError) == XWORK_RESULT_OK &&
                  tMcpOutput.bSuccess && tMcpOutput.sContent &&
                  strstr(tMcpOutput.sContent, "echo: hello mcp"),
              "retained MCP generation keeps its proxy data after refresh");
        xworkToolOutputUnit(&tMcpOutput);
        xworkToolOutputInit(&tMcpOutput);
        CHECK(pMcpTool &&
                  test_entry_execute(pMcpTool, pMcpTool->pUserData, &tMcpToolContext,
                                      "{\"text\":\"hello mcp\"}", &tMcpOutput,
                                      &tError) == XWORK_RESULT_OK &&
                  tMcpOutput.bSuccess && tMcpOutput.sContent &&
                  strstr(tMcpOutput.sContent, "echo: hello mcp") &&
                  strstr(tMcpOutput.sContent, "structured_content"),
              "MCP tools/call returns text and structured content through the proxy");
        xworkToolOutputUnit(&tMcpOutput);
        memset(&tMcpInfo, 0, sizeof(tMcpInfo));
        CHECK(xworkMcpClientGetInfo(pMcpClient, &tMcpInfo) && tMcpInfo.bConnected &&
                  tMcpInfo.iToolCount == 1u && tMcpInfo.uRequestsCompleted == 5u &&
                  strcmp(tMcpInfo.sProtocolVersion, "2025-11-25") == 0,
              "MCP diagnostics expose negotiated version, tool count, and request count");
        {
            uint64_t uStartedMs = xllmClockUs() / UINT64_C(1000);
            xwork_result eMcpDeadlineResult;
            uint64_t uElapsedMs;
            xworkToolOutputInit(&tMcpOutput);
            eMcpDeadlineResult =
                xworkMcpClientCallTool(pMcpClient, "echo", "{\"delay\":true}", NULL,
                                       xllmDeadlineAfterUs(UINT64_C(100000)), &tMcpOutput, &tError);
            uElapsedMs = xllmClockUs() / UINT64_C(1000) - uStartedMs;
            if (eMcpDeadlineResult != XWORK_RESULT_TIMEOUT || tError.eCode != XWORK_ERROR_TIMEOUT ||
                uElapsedMs >= 2000u)
            {
                fprintf(stderr, "MCP deadline: result=%d error=%d elapsed=%llu message=%s\n",
                        (int)eMcpDeadlineResult, (int)tError.eCode, (unsigned long long)uElapsedMs,
                        tError.sMessage);
            }
            CHECK(eMcpDeadlineResult == XWORK_RESULT_TIMEOUT &&
                      tError.eCode == XWORK_ERROR_TIMEOUT && uElapsedMs < 2000u,
                  "MCP tool calls honor operation deadlines and send cancellation promptly");
            xworkToolOutputUnit(&tMcpOutput);
        }
        CHECK(xworkAgentUnregisterToolsBySource(pAgent, "mcp:phase3", &iRemoved, &tError) &&
                  iRemoved == 1u && xworkAgentToolCount(pAgent) == 12u,
              "MCP source can be detached without disturbing builtin tools");
        xworkToolCatalogRelease(pOldMcpCatalog);
        xworkMcpClientDestroy(pMcpClient);
        pMcpClient = NULL;
    }

    eAgentRun =
        xworkAgentRun(pAgent, "Create and verify the requested note file.", &tResult, &tError);
    if (eAgentRun != XWORK_RESULT_OK)
    {
        fprintf(stderr, "agent loop error: result=%d code=%s message=%s\n", (int)eAgentRun,
                xworkErrorCodeName(tError.eCode), tError.sMessage);
    }
    CHECK(eAgentRun == XWORK_RESULT_OK, "multi-turn tool loop completes");
    CHECK(tMock.bRegistryMutationBlocked,
          "tool registry mutation is rejected while an agent run is active");
    CHECK(tMock.bCatalogAdoptionBlocked,
          "runtime catalog adoption is rejected while an agent run is active");
    CHECK(tResult.sFinalText && strstr(tResult.sFinalText, "verified"),
          "final assistant response returned");
    CHECK(tResult.uAgentTurns == 5u && tResult.uToolCalls == 8u,
          "verification gate adds one model turn while eight tool calls run");
    CHECK(tResult.uCompactions >= 1u && tMock.uCompactionCalls == tResult.uCompactions + 1u &&
              tResult.uRejectedCompactionSummaries == 1u,
          "rejected compaction is corrected before the durable checkpoint advances");
    CHECK(tEvents.uCompactions == tResult.uCompactions && tEvents.uRejectedCompactions == 1u &&
              tEvents.bCompactionQualityMetadata && tEvents.uErrors == 0u,
          "compaction quality and completion events are balanced");
    CHECK(tEvents.uModelStarts == tResult.uAgentTurns &&
              tEvents.uModelDone == tResult.uAgentTurns && tEvents.bRequestMetadata &&
              tEvents.bResponseMetadata && tEvents.bRuntimeIdentity,
          "model lifecycle events expose reproducible request and provider diagnostics");
    CHECK(tEvents.uLastUserMessageSequence > 0u,
          "agent start identifies its durable user-message ledger boundary");
    CHECK(tMock.bSawTools && tMock.bSawParallel && tMock.bSawToolResults && tMock.bSawContext,
          "tools, parallel flag, continuity, and operation context reach model");
    CHECK(tMock.bSawCompactionSummary,
          "post-compaction model turns receive the summary checkpoint");
    CHECK(tMock.bSawVerificationGate,
          "premature completion receives a durable verification-gate prompt");
    CHECK(tMock.uPermissionCalls == 8u && tMock.bSawPathPermission && tMock.bSawCommandPermission &&
              tMock.bSawHighRiskPermission,
          "structured permission callback sees every tool plus path, command, and risk metadata");
    CHECK(tMock.uBeforeToolHooks == 8u && tMock.uAfterToolHooks == 8u,
          "before/after tool hooks bracket every executed tool");
    CHECK(tEvents.uToolStarts == 8u && tEvents.uToolDone == 8u,
          "tool lifecycle events are balanced");
    CHECK(tEvents.uToolSuccess == 7u && tEvents.uToolFailure == 1u,
          "seven builtin operations succeed and escape is a tool-level failure");
    CHECK(tEvents.sLastArtifact[0] != '\0' && tEvents.uLastArtifactId != 0u,
          "oversized tool output spills to an identified artifact");
    CHECK(xrtFileExists((str)sSessionPath), "session autosaves atomically during the run");

    memset(&tPatchContext, 0, sizeof(tPatchContext));
    xworkToolOutputInit(&tPatchOutput);
    pPatchTool = xwork__find_tool(pAgent, "edit");
    tPatchContext.pAgent = pAgent;
    /* 0-match edit returns candidate context lines (self-correction). */
    CHECK(pPatchTool &&
              test_entry_execute(pPatchTool, 
                  pPatchTool->pUserData, &tPatchContext,
                  "{\"path\":\"sandbox/"
                  "note.txt\",\"edits\":[{\"old_text\":\"NOT-PRESENT-TEXT\",\"new_text\":\"X\"}]}",
                  &tPatchOutput, &tError) == XWORK_RESULT_OK &&
              !tPatchOutput.bSuccess && tPatchOutput.sContent &&
              strstr(tPatchOutput.sContent, "context") &&
              strstr(tPatchOutput.sContent, "0 matches"),
          "0-match edit returns numbered candidate lines for self-correction");
    xworkToolOutputUnit(&tPatchOutput);
    xworkToolOutputInit(&tPatchOutput);
    /* Ambiguous edit (no replace_all) lists the matching lines. */
    CHECK(pPatchTool &&
              test_entry_execute(pPatchTool, 
                  pPatchTool->pUserData, &tPatchContext,
                  "{\"path\":\"sandbox/"
                  "note.txt\",\"edits\":[{\"old_text\":\"abc\",\"new_text\":\"X\"}]}",
                  &tPatchOutput, &tError) == XWORK_RESULT_OK &&
              !tPatchOutput.bSuccess && tPatchOutput.sContent &&
              strstr(tPatchOutput.sContent, "times"),
          "ambiguous edit reports match count and candidates");
    xworkToolOutputUnit(&tPatchOutput);

    sFile = (char *)xrtFileReadAll("tests/tmp_xwork/sandbox/note.txt", &iFileSize);
    CHECK(sFile && iFileSize > 1600u && strncmp(sFile, "PATCHED-", 8u) == 0,
          "workspace file was created then edited by both edit tools");
    CHECK(!xrtFileExists((str) "tests/tmp_xwork/sandbox/note.txt/child.txt"),
          "failed edit left no partial target behind");

    pStartTool = xwork__find_tool(pAgent, "spawn");
    pWriteProcessTool = xwork__find_tool(pAgent, "stdin");
    pPollTool = xwork__find_tool(pAgent, "poll");
    xworkToolOutputInit(&tProcessOutput);
#if defined(_WIN32)
    CHECK(pStartTool &&
              test_entry_execute(pStartTool, pStartTool->pUserData, &tPatchContext,
                                    "{\"argv\":[\"findstr\",\"persistent\"]}", &tProcessOutput,
                                    &tError) == XWORK_RESULT_OK &&
              tProcessOutput.bSuccess && tProcessOutput.sContent &&
              test_task_id(tProcessOutput.sContent, &uManagedId) == 1,
          "spawn starts and returns a stable task id");
#else
    CHECK(pStartTool &&
              test_entry_execute(pStartTool, pStartTool->pUserData, &tPatchContext,
                                    "{\"argv\":[\"grep\",\"persistent\"]}", &tProcessOutput,
                                    &tError) == XWORK_RESULT_OK &&
              tProcessOutput.bSuccess && tProcessOutput.sContent &&
              test_task_id(tProcessOutput.sContent, &uManagedId) == 1,
          "spawn starts and returns a stable task id");
#endif
    xworkToolOutputUnit(&tProcessOutput);
    xworkToolOutputInit(&tProcessOutput);
    if (uManagedId)
    {
        char sProcessArgs[512];
        snprintf(sProcessArgs, sizeof(sProcessArgs),
                 "{\"task_id\":%llu,\"input\":\"persistent "
                 "hello\",\"append_newline\":true,\"close_stdin\":true}",
                 uManagedId);
        CHECK(pWriteProcessTool &&
                  test_entry_execute(pWriteProcessTool, pWriteProcessTool->pUserData, &tPatchContext,
                                               sProcessArgs, &tProcessOutput,
                                               &tError) == XWORK_RESULT_OK &&
                  tProcessOutput.bSuccess,
              "stdin writes and an explicit stdin close work");
        xworkToolOutputUnit(&tProcessOutput);
        xworkToolOutputInit(&tProcessOutput);
        CHECK(test_wait_task(pAgent, uManagedId, &tError), "stdin process terminates after EOF");
        CHECK(test_wait_task(pAgent, uManagedId, &tError), "stdin process terminates after EOF");
        snprintf(sProcessArgs, sizeof(sProcessArgs), "{\"task_id\":%llu}", uManagedId);
        CHECK(pPollTool &&
                  test_entry_execute(pPollTool, pPollTool->pUserData, &tPatchContext, sProcessArgs,
                                       &tProcessOutput, &tError) == XWORK_RESULT_OK &&
                  tProcessOutput.bSuccess && tProcessOutput.sContent &&
                  test_task_stdout(pAgent, uManagedId, "persistent hello", &tError) &&
                  strstr(tProcessOutput.sContent, "\"state\":\"succeeded\""),
              "poll returns incremental output and final exit state");
        CHECK(xworkRuntimeReleaseTask(pAgent->pRuntime, uManagedId, &tError) && xworkRuntimeTaskCount(pAgent->pRuntime) == 0u,
              "released task leaves no live registry entry");
    }
    xworkToolOutputUnit(&tProcessOutput);

    pExecTool = xwork__find_tool(pAgent, "exec");
    xworkToolOutputInit(&tProcessOutput);
#if defined(_WIN32)
    CHECK(pExecTool &&
              test_entry_execute(pExecTool, 
                  pExecTool->pUserData, &tPatchContext,
                  "{\"argv\":[\"cmd\",\"/c\",\"exit\",\"7\"],\"expected_exit_codes\":[7]}",
                  &tProcessOutput, &tError) == XWORK_RESULT_OK &&
              tProcessOutput.bSuccess && tProcessOutput.sContent &&
              strstr(tProcessOutput.sContent, "\"exit_code\":7") &&
              strstr(tProcessOutput.sContent, "\"exit_expected\":true"),
          "exec accepts an explicitly expected nonzero exit code");
#else
    CHECK(pExecTool &&
              test_entry_execute(pExecTool, 
                  pExecTool->pUserData, &tPatchContext,
                  "{\"argv\":[\"sh\",\"-c\",\"exit 7\"],\"expected_exit_codes\":[7]}",
                  &tProcessOutput, &tError) == XWORK_RESULT_OK &&
              tProcessOutput.bSuccess && tProcessOutput.sContent &&
              strstr(tProcessOutput.sContent, "\"exit_code\":7") &&
              strstr(tProcessOutput.sContent, "\"exit_expected\":true"),
          "exec accepts an explicitly expected nonzero exit code");
#endif
    xworkToolOutputUnit(&tProcessOutput);
    xworkToolOutputInit(&tProcessOutput);
#if defined(_WIN32)
    CHECK(pExecTool &&
              test_entry_execute(pExecTool, pExecTool->pUserData, &tPatchContext,
                                   "{\"argv\":[\"cmd\",\"/c\",\"exit\",\"7\"]}", &tProcessOutput,
                                   &tError) == XWORK_RESULT_OK &&
              !tProcessOutput.bSuccess && tProcessOutput.sContent &&
              strstr(tProcessOutput.sContent, "\"exit_expected\":false"),
          "exec still rejects a nonzero exit code by default");
#else
    CHECK(pExecTool &&
              test_entry_execute(pExecTool, pExecTool->pUserData, &tPatchContext,
                                   "{\"argv\":[\"sh\",\"-c\",\"exit 7\"]}", &tProcessOutput,
                                   &tError) == XWORK_RESULT_OK &&
              !tProcessOutput.bSuccess && tProcessOutput.sContent &&
              strstr(tProcessOutput.sContent, "\"exit_expected\":false"),
          "exec still rejects a nonzero exit code by default");
#endif
    xworkToolOutputUnit(&tProcessOutput);
    xworkToolOutputInit(&tProcessOutput);
    CHECK(pExecTool &&
              test_entry_execute(pExecTool, pExecTool->pUserData, &tPatchContext,
                                   "{\"argv\":[\"echo\",\"invalid\"],\"expected_exit_codes\":[]}",
                                   &tProcessOutput, &tError) == XWORK_RESULT_OK &&
              !tProcessOutput.bSuccess && tProcessOutput.sContent &&
              strstr(tProcessOutput.sContent, "expected_exit_codes"),
          "exec command rejects an empty expected exit-code contract");
    xworkToolOutputUnit(&tProcessOutput);

    if (tEvents.sLastArtifact[0])
    {
        sArtifactAbsolute = xrtPathJoin(sWorkspace, tEvents.sLastArtifact);
        CHECK(sArtifactAbsolute && xrtFileExists((str)sArtifactAbsolute),
              "full oversized output artifact exists");
    }
    CHECK(!xrtFileExists((str) "tests/outside.txt"),
          "workspace escape tool call could not access outside path");
    CHECK(tError.eCode == XWORK_ERROR_NONE,
          "successful run does not leak a stale recoverable tool error");
    uCompactionsBeforeExplicit = tMock.uCompactionCalls;
    CHECK(xworkAgentCompact(pAgent, &tError) == XWORK_RESULT_OK,
          "explicit safe-prefix compaction completes after the run");
    CHECK(tMock.uCompactionCalls == uCompactionsBeforeExplicit + 1u &&
              tEvents.uCompactions + tEvents.uRejectedCompactions == tMock.uCompactionCalls,
          "explicit compaction adds one model summary and lifecycle event");

    {
        xllm_tool_call arrInterruptedCalls[2];
        xllm_response tInterruptedResponse;
        xllm_session_stats tInterruptedStats;
        xwork_recovery_snapshot *pRecoverySnapshot;
        xwork_recovery_snapshot *pRecoveryRef;
        xwork_recovery_call_info tRecoveryCall;
        xwork_recovery_decision tDecision;
        xwork_resume_options tResumeOptions;
        const char *sVerifyArgs;
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
        arrInterruptedCalls[1].sArgumentsJson = (char *)sVerifyArgs;
        tInterruptedResponse.pToolCalls = arrInterruptedCalls;
        tInterruptedResponse.iToolCallCount = 2u;
        uInterruptedTurn = xllmSessionBeginTurn(pSession);
        CHECK(uInterruptedTurn &&
                  xllmSessionAddText(pSession, uInterruptedTurn, XLLM_ROLE_USER,
                                     "Finish this interrupted verification batch.", 0u) &&
                  xllmSessionAddAssistantResponse(pSession, uInterruptedTurn,
                                                  &tInterruptedResponse) &&
                  xllmSessionAddToolResult(pSession, uInterruptedTurn, "call_recovered_list",
                                           "status: success"),
              "interrupted batch fixture records one completed and one pending tool");
        CHECK(xllmSessionGetStats(pSession, &tInterruptedStats) &&
                  tInterruptedStats.uPendingToolCalls == 1u,
              "interrupted batch exposes one pending tool before resume");
        xworkRunResultUnit(&tResult);
        CHECK(xworkAgentRun(pAgent, "This prompt must not be appended.", &tResult, &tError) ==
                      XWORK_RESULT_ERROR &&
                  tError.eCode == XWORK_ERROR_CONTEXT &&
                  xllmSessionCurrentTurn(pSession) == uInterruptedTurn,
              "normal run refuses to duplicate a prompt over interrupted work");
        pRecoverySnapshot = xworkAgentRecoverySnapshot(pAgent, &tError);
        xworkRecoveryCallInfoInit(&tRecoveryCall);
        CHECK(pRecoverySnapshot && xworkRecoverySnapshotCount(pRecoverySnapshot) == 1u &&
                  xworkRecoverySnapshotAt(pRecoverySnapshot, 0u, &tRecoveryCall) &&
                  strcmp(tRecoveryCall.sToolCallId, "call_recovered_verify") == 0 &&
                  tRecoveryCall.bToolAvailable && !tRecoveryCall.bAutomaticRetrySafe &&
                  (tRecoveryCall.uEffects & XWORK_TOOL_EFFECT_PROCESS) != 0u,
              "recovery snapshot identifies the pending effectful command");
        pRecoveryRef = xworkRecoverySnapshotRef(pRecoverySnapshot);
        CHECK(pRecoveryRef == pRecoverySnapshot,
              "recovery snapshots expose explicit retained ownership");
        xworkRecoverySnapshotRelease(pRecoveryRef);
        xworkRunResultUnit(&tResult);
        CHECK(xworkAgentResume(pAgent, &tResult, &tError) == XWORK_RESULT_ERROR &&
                  tError.eCode == XWORK_ERROR_RECOVERY_REQUIRED &&
                  strcmp(xworkErrorCodeName(tError.eCode), "recovery_required") == 0 &&
                  xllmSessionPendingToolCallCount(pSession) == 1u,
              "safe-default resume blocks the complete pending batch before execution");
        xworkRecoveryDecisionInit(&tDecision);
        tDecision.sToolCallId = "call_recovered_verify";
        tDecision.eAction = XWORK_RECOVERY_RETRY;
        xworkResumeOptionsInit(&tResumeOptions);
        tResumeOptions.pDecisions = &tDecision;
        tResumeOptions.iDecisionCount = 1u;
        xworkRunResultUnit(&tResult);
        CHECK(xworkAgentResumeWithOptions(pAgent, &tResumeOptions, &tResult, &tError) ==
                  XWORK_RESULT_OK,
              "explicit retry executes the pending command and continues the model loop");
        CHECK(tResult.uToolCalls == 1u && tResult.uModelCalls == 1u && tResult.sFinalText &&
                  strstr(tResult.sFinalText, "verified"),
              "resumed run reports only newly recovered work and returns final text");
        CHECK(xllmSessionGetStats(pSession, &tInterruptedStats) &&
                  tInterruptedStats.uPendingToolCalls == 0u,
              "resumed run durably resolves the pending tool call");
        CHECK(tEvents.uRecoveryRequired == 1u && tEvents.uRecoveryResolved == 1u &&
                  tEvents.uRecoveryAutomatic == 0u && tEvents.uRecoveryUncertain == 0u,
              "effectful recovery publishes required and explicit-resolution audit events");
        xworkAgentDestroy(pAgent);
        pAgent = NULL;
        CHECK(xworkRecoverySnapshotAt(pRecoverySnapshot, 0u, &tRecoveryCall) &&
                  strcmp(tRecoveryCall.sToolName, "exec") == 0,
              "recovery snapshot strings outlive the inspected agent");
        xworkRecoverySnapshotRelease(pRecoverySnapshot);
    }

    xworkRunResultUnit(&tResult);
    xworkAgentDestroy(pAgent);
cleanup:
    xworkMcpClientDestroy(pMcpClient);
    xrtCancelDestroy(pCancel);
    if (sFile && iFileSize)
        xrtFree(sFile);
    if (sArtifactAbsolute)
        xrtFree(sArtifactAbsolute);
    free(sOld);
    xllmSessionDestroy(pSession);
    (void)xrtDirRemoveAll(sWorkspace);
}

static void test_lazy_mcp_runtime(void)
{
    static const char sWorkspace[] = "tests/tmp_xwork_lazy_mcp";
    const char *arrArgs[] = {"--mcp-lazy-server"};
    const char *arrModernArgs[] = {"--mcp-modern-server"};
    const char *arrMalformedArgs[] = {"--mcp-malformed-server"};
    const char *arrCursorArgs[] = {"--mcp-repeated-cursor-server"};
    const char *arrExitArgs[] = {"--mcp-exit-server"};
    const char *arrSlowStartArgs[] = {"--mcp-slow-start-server"};
    const char *arrAllowed[] = {"echo"};
    const xwork_mcp_environment arrEnvironment[] = {{"XWORK_MCP_LAZY_FIXTURE", "enabled"}};
    xwork_runtime_config tRuntimeConfig;
    xwork_mcp_server_config tServerConfig;
    xwork_mcp_server_config arrBatch[2];
    xwork_mcp_server_info tServerInfo;
    xwork_agent_definition_config tDefinitionConfig;
    xwork_agent_options tOptions;
    xllm_session_config tSessionConfig;
    xwork_runtime *pRuntime = NULL;
    xwork_agent_definition *pDefinition = NULL;
    xllm_session *pSession = NULL;
    xwork_agent *pAgent = NULL;
    xwork_error tError;
    lazy_mcp_model tModel;
    xwork_run_result tResult;
    const xwork_tool_entry *pSearch;
    const xwork_tool_entry *pEcho;
    xwork_tool_context tContext;
    xwork_tool_output tOutput;
    size_t iReplaced = 0u;

    memset(&tModel, 0, sizeof(tModel));
    memset(&tResult, 0, sizeof(tResult));
    (void)xrtDirRemoveAll(sWorkspace);
    CHECK(xrtDirCreateAll((str)sWorkspace), "lazy MCP workspace created");
    xworkRuntimeConfigInit(&tRuntimeConfig);
    pRuntime = xworkRuntimeCreate(&tRuntimeConfig, &tError);
    xworkMcpServerConfigInit(&tServerConfig);
    tServerConfig.sServerId = "lazy";
    tServerConfig.sSummary = "A bounded fixture with an echo tool.";
    tServerConfig.sProgram = g_sSelfPath;
    tServerConfig.psArguments = arrArgs;
    tServerConfig.iArgumentCount = 1u;
    tServerConfig.psAllowedTools = arrAllowed;
    tServerConfig.iAllowedToolCount = 1u;
    tServerConfig.pEnvironment = arrEnvironment;
    tServerConfig.iEnvironmentCount = 1u;
    tServerConfig.sProtocolVersion = "2025-11-25";
    tServerConfig.uRequestTimeoutMs = 5000u;
    CHECK(pRuntime && xworkRuntimeRegisterMcpServer(pRuntime, &tServerConfig, &tError) &&
              xworkRuntimeMcpServerCount(pRuntime) == 1u,
          "runtime registers an MCP definition without starting it");
    xworkMcpServerInfoInit(&tServerInfo);
    CHECK(xworkRuntimeMcpServerGetInfo(pRuntime, "lazy", &tServerInfo) &&
              tServerInfo.eState == XWORK_MCP_SERVER_DISCONNECTED && !tServerInfo.bConnected &&
              !tServerInfo.bToolsDiscovered && tServerInfo.uRequestsCompleted == 0u &&
              strcmp(tServerInfo.sProtocolVersion, "2025-11-25") == 0,
          "registered MCP server stays cold with a pinned legacy revision");
    CHECK(!xworkRuntimeRegisterMcpServer(pRuntime, &tServerConfig, &tError) &&
              xworkRuntimeMcpServerCount(pRuntime) == 1u,
          "duplicate MCP registration leaves the runtime unchanged");

    xworkAgentDefinitionConfigInit(&tDefinitionConfig);
    tDefinitionConfig.sId = "test.lazy-mcp";
    tDefinitionConfig.bRegisterBuiltinTools = true;
    tDefinitionConfig.bAutoSaveSession = false;
    tDefinitionConfig.bRequireVerificationAfterWrite = false;
    pDefinition = xworkAgentDefinitionCreate(&tDefinitionConfig, &tError);
    xllmSessionConfigInit(&tSessionConfig);
    pSession = xllmSessionCreate(&tSessionConfig, NULL);
    xworkAgentOptionsInit(&tOptions);
    tOptions.pSession = pSession;
    tOptions.sWorkspaceRoot = sWorkspace;
    tOptions.OnModelComplete = lazy_mcp_complete;
    tOptions.pModelUserData = &tModel;
    pAgent = xworkAgentCreateWithRuntime(pRuntime, pDefinition, &tOptions, &tError);
    CHECK(pAgent && xworkAgentToolCount(pAgent) == 15u && xwork__find_tool(pAgent, "tool_search") &&
              xwork__find_tool(pAgent, "tool_load") && !xwork__find_tool(pAgent, "mcp__lazy__echo"),
          "agent starts with two compact discovery tools and no remote schema");

    pSearch = pAgent ? xwork__find_tool(pAgent, "tool_search") : NULL;
    memset(&tContext, 0, sizeof(tContext));
    tContext.pAgent = pAgent;
    tContext.sWorkspaceRoot = sWorkspace;
    tContext.uDeadline = XLLM_DEADLINE_NEVER;
    xworkToolOutputInit(&tOutput);
    CHECK(pSearch &&
              test_entry_execute(pSearch, pSearch->pUserData, &tContext, "{}", &tOutput, &tError) ==
                  XWORK_RESULT_OK &&
              tOutput.bSuccess && tOutput.sContent && strstr(tOutput.sContent, "server=lazy") &&
              xworkRuntimeMcpServerGetInfo(pRuntime, "lazy", &tServerInfo) &&
              tServerInfo.eState == XWORK_MCP_SERVER_DISCONNECTED,
          "summary-only discovery does not connect the MCP server");
    xworkToolOutputUnit(&tOutput);

    {
        xwork_result eLazyRun =
            pAgent ? xworkAgentRun(pAgent, "Load and call the selected MCP echo tool.", &tResult,
                                   &tError)
                   : XWORK_RESULT_ERROR;
        if (eLazyRun != XWORK_RESULT_OK)
        {
            fprintf(stderr, "lazy MCP run: result=%d error=%d message=%s calls=%u\n", (int)eLazyRun,
                    (int)tError.eCode, tError.sMessage, (unsigned)tModel.uCalls);
        }
        CHECK(eLazyRun == XWORK_RESULT_OK && tResult.sFinalText &&
                  strcmp(tResult.sFinalText, "lazy MCP completed") == 0,
              "tool_load can publish one remote schema inside an active run");
    }
    CHECK(tModel.uCalls == 3u && tModel.bSawCompactDiscovery && tModel.bSawLoadedSchema &&
              tModel.bSawRemoteResult && xworkAgentToolCount(pAgent) == 16u,
          "the next model turn sees only the selected MCP schema and result");
    xworkMcpServerInfoInit(&tServerInfo);
    CHECK(xworkRuntimeMcpServerGetInfo(pRuntime, "lazy", &tServerInfo) &&
              tServerInfo.eState == XWORK_MCP_SERVER_READY && tServerInfo.bToolsDiscovered &&
              tServerInfo.iDiscoveredToolCount == 1u && tServerInfo.uSchemaGeneration == 1u &&
              tServerInfo.uRequestsCompleted == 3u,
          "lazy MCP diagnostics expose filtered schema generation and calls");

    pSearch = xwork__find_tool(pAgent, "tool_search");
    xworkToolOutputInit(&tOutput);
    CHECK(pSearch &&
              test_entry_execute(pSearch, pSearch->pUserData, &tContext,
                                 "{\"server\":\"lazy\",\"query\":\"tool\"}", &tOutput,
                                 &tError) == XWORK_RESULT_OK &&
              tOutput.bSuccess && tOutput.sContent && !strstr(tOutput.sContent, "hidden"),
          "MCP allowlist filters discovery before schema exposure");
    xworkToolOutputUnit(&tOutput);
    CHECK(xworkAgentLoadMcpTool(pAgent, "lazy", "echo", &tError) &&
              xworkAgentToolCount(pAgent) == 16u,
          "explicit MCP load is idempotent between runs");

    pEcho = xwork__find_tool(pAgent, "mcp__lazy__echo");
    CHECK(xworkRuntimeSetMcpServerEnabled(pRuntime, "lazy", false, &tError) &&
              xworkRuntimeMcpServerGetInfo(pRuntime, "lazy", &tServerInfo) &&
              tServerInfo.eState == XWORK_MCP_SERVER_DISABLED,
          "disabling a runtime MCP server stops its process");
    xworkToolOutputInit(&tOutput);
    CHECK(pEcho &&
              test_entry_execute(pEcho, pEcho->pUserData, &tContext, "{\"text\":\"disabled\"}", &tOutput,
                               &tError) == XWORK_RESULT_ERROR &&
              tError.eCode == XWORK_ERROR_POLICY,
          "loaded proxies fail closed while their server is disabled");
    xworkToolOutputUnit(&tOutput);
    CHECK(xworkRuntimeSetMcpServerEnabled(pRuntime, "lazy", true, &tError),
          "runtime MCP server can be re-enabled explicitly");
    xworkToolOutputInit(&tOutput);
    CHECK(pEcho &&
              test_entry_execute(pEcho, pEcho->pUserData, &tContext, "{\"text\":\"reconnect\"}", &tOutput,
                               &tError) == XWORK_RESULT_OK &&
              tOutput.bSuccess && tOutput.sContent && strstr(tOutput.sContent, "echo: hello mcp"),
          "loaded proxy reconnects the server on its next real call");
    xworkToolOutputUnit(&tOutput);
    CHECK(xworkRuntimeUnregisterMcpServer(pRuntime, "lazy", &tError) &&
              xworkRuntimeMcpServerCount(pRuntime) == 0u &&
              !xworkRuntimeMcpServerGetInfo(pRuntime, "lazy", &tServerInfo),
          "runtime unregister hides the server while retaining proxy storage");
    xworkToolOutputInit(&tOutput);
    CHECK(pEcho &&
              test_entry_execute(pEcho, pEcho->pUserData, &tContext, "{\"text\":\"retired\"}", &tOutput,
                               &tError) == XWORK_RESULT_ERROR &&
              tError.eCode == XWORK_ERROR_POLICY,
          "retained proxy fails closed after server unregister");
    xworkToolOutputUnit(&tOutput);

    xworkMcpServerConfigInit(&tServerConfig);
    tServerConfig.sServerId = "modern";
    tServerConfig.sSummary = "A 2026-07-28 per-request metadata fixture.";
    tServerConfig.sProgram = g_sSelfPath;
    tServerConfig.psArguments = arrModernArgs;
    tServerConfig.iArgumentCount = 1u;
    tServerConfig.uRequestTimeoutMs = 5000u;
    CHECK(xworkRuntimeRegisterMcpServer(pRuntime, &tServerConfig, &tError) &&
              xworkRuntimeMcpServerGetInfo(pRuntime, "modern", &tServerInfo) &&
              strcmp(tServerInfo.sProtocolVersion, "2026-07-28") == 0 &&
              tServerInfo.eState == XWORK_MCP_SERVER_DISCONNECTED,
          "runtime MCP definitions default to the current stateless revision");
    CHECK(xworkRuntimeRefreshMcpServer(pRuntime, "modern", NULL, XLLM_DEADLINE_NEVER, &tError) &&
              xworkRuntimeMcpServerGetInfo(pRuntime, "modern", &tServerInfo) &&
              tServerInfo.eState == XWORK_MCP_SERVER_READY &&
              tServerInfo.iDiscoveredToolCount == 1u && tServerInfo.uSchemaGeneration == 1u &&
              tServerInfo.uRequestsCompleted == 2u,
          "modern stdio conformance uses discover metadata and cacheable tools/list");
    CHECK(xworkRuntimeRefreshMcpServer(pRuntime, "modern", NULL, XLLM_DEADLINE_NEVER, &tError) &&
              xworkRuntimeMcpServerGetInfo(pRuntime, "modern", &tServerInfo) &&
              tServerInfo.uSchemaGeneration == 2u && tServerInfo.uRequestsCompleted == 3u,
          "an unpublished MCP cache generation is replaced without retention");
    CHECK(xworkAgentLoadMcpTool(pAgent, "modern", "echo", &tError),
          "host can load one schema from the modern MCP generation");
    pEcho = xwork__find_tool(pAgent, "mcp__modern__echo");
    xworkToolOutputInit(&tOutput);
    CHECK(pEcho &&
              test_entry_execute(pEcho, pEcho->pUserData, &tContext, "{\"text\":\"modern\"}", &tOutput,
                               &tError) == XWORK_RESULT_OK &&
              tOutput.bSuccess && tOutput.sContent && strstr(tOutput.sContent, "echo: hello mcp"),
          "modern MCP tools/call carries metadata and consumes complete result");
    xworkToolOutputUnit(&tOutput);
    CHECK(xworkRuntimeUnregisterMcpServer(pRuntime, "modern", &tError),
          "modern MCP server unregisters cleanly");

    xworkMcpServerConfigInit(&tServerConfig);
    tServerConfig.sServerId = "malformed";
    tServerConfig.sSummary = "A malformed response fixture.";
    tServerConfig.sProgram = g_sSelfPath;
    tServerConfig.psArguments = arrMalformedArgs;
    tServerConfig.iArgumentCount = 1u;
    tServerConfig.sProtocolVersion = "2025-11-25";
    tServerConfig.uRequestTimeoutMs = 5000u;
    CHECK(xworkRuntimeRegisterMcpServer(pRuntime, &tServerConfig, &tError) &&
              !xworkRuntimeRefreshMcpServer(pRuntime, "malformed", NULL, XLLM_DEADLINE_NEVER,
                                            &tError) &&
              tError.eCode == XWORK_ERROR_TOOL &&
              xworkRuntimeMcpServerGetInfo(pRuntime, "malformed", &tServerInfo) &&
              tServerInfo.eState == XWORK_MCP_SERVER_FAILED && !tServerInfo.bConnected &&
              xworkRuntimeUnregisterMcpServer(pRuntime, "malformed", &tError),
          "malformed MCP response fails closed as a bounded protocol error");

    tServerConfig.sServerId = "cursor";
    tServerConfig.sSummary = "A repeated pagination cursor fixture.";
    tServerConfig.psArguments = arrCursorArgs;
    CHECK(
        xworkRuntimeRegisterMcpServer(pRuntime, &tServerConfig, &tError) &&
            !xworkRuntimeRefreshMcpServer(pRuntime, "cursor", NULL, XLLM_DEADLINE_NEVER, &tError) &&
            tError.eCode == XWORK_ERROR_TOOL &&
            xworkRuntimeMcpServerGetInfo(pRuntime, "cursor", &tServerInfo) &&
            tServerInfo.eState == XWORK_MCP_SERVER_FAILED && !tServerInfo.bConnected &&
            xworkRuntimeUnregisterMcpServer(pRuntime, "cursor", &tError),
        "repeated MCP pagination cursor is rejected without looping");

    tServerConfig.sServerId = "exit";
    tServerConfig.sSummary = "An unexpected process exit fixture.";
    tServerConfig.psArguments = arrExitArgs;
    CHECK(xworkRuntimeRegisterMcpServer(pRuntime, &tServerConfig, &tError) &&
              !xworkRuntimeRefreshMcpServer(pRuntime, "exit", NULL, XLLM_DEADLINE_NEVER, &tError) &&
              tError.eCode == XWORK_ERROR_IO &&
              xworkRuntimeMcpServerGetInfo(pRuntime, "exit", &tServerInfo) &&
              tServerInfo.eState == XWORK_MCP_SERVER_FAILED && !tServerInfo.bConnected &&
              xworkRuntimeUnregisterMcpServer(pRuntime, "exit", &tError),
          "MCP process exit during discovery fails as an I/O error");

    tServerConfig.sServerId = "slow-start";
    tServerConfig.sSummary = "A bounded slow startup fixture.";
    tServerConfig.psArguments = arrSlowStartArgs;
    tServerConfig.uStartupTimeoutMs = 20u;
    tServerConfig.uRequestTimeoutMs = 1000u;
    CHECK(xworkRuntimeRegisterMcpServer(pRuntime, &tServerConfig, &tError) &&
              !xworkRuntimeRefreshMcpServer(pRuntime, "slow-start", NULL, XLLM_DEADLINE_NEVER,
                                            &tError) &&
              tError.eCode == XWORK_ERROR_TIMEOUT &&
              xworkRuntimeUnregisterMcpServer(pRuntime, "slow-start", &tError),
          "MCP startup timeout is independent from the request timeout");

    xworkMcpServerConfigInit(&arrBatch[0]);
    arrBatch[0].sServerId = "batch-one";
    arrBatch[0].sSummary = "First source-managed MCP server.";
    arrBatch[0].sProgram = g_sSelfPath;
    arrBatch[0].psArguments = arrArgs;
    arrBatch[0].iArgumentCount = 1u;
    arrBatch[0].pEnvironment = arrEnvironment;
    arrBatch[0].iEnvironmentCount = 1u;
    arrBatch[0].sProtocolVersion = "2025-11-25";
    arrBatch[0].uStartupTimeoutMs = 5000u;
    arrBatch[0].uRequestTimeoutMs = 5000u;
    arrBatch[1] = arrBatch[0];
    arrBatch[1].sServerId = "batch-two";
    arrBatch[1].sSummary = "Second source-managed MCP server.";
    CHECK(xworkRuntimeReplaceMcpServersBySource(pRuntime, "test.batch", arrBatch, 2u, &iReplaced,
                                                &tError) &&
              iReplaced == 0u && xworkRuntimeMcpServerCount(pRuntime) == 2u,
          "MCP source batch publishes cold definitions atomically");
    CHECK(xworkRuntimeRefreshMcpServer(pRuntime, "batch-one", NULL, XLLM_DEADLINE_NEVER, &tError) &&
              xworkRuntimeMcpServerGetInfo(pRuntime, "batch-one", &tServerInfo) &&
              tServerInfo.bConnected && tServerInfo.uRequestsCompleted == 2u,
          "source-managed MCP server connects with a distinct startup timeout");
    CHECK(xworkRuntimeReplaceMcpServersBySource(pRuntime, "test.batch", arrBatch, 2u, &iReplaced,
                                                &tError) &&
              iReplaced == 0u &&
              xworkRuntimeMcpServerGetInfo(pRuntime, "batch-one", &tServerInfo) &&
              tServerInfo.bConnected && tServerInfo.uRequestsCompleted == 2u,
          "unchanged MCP source definitions retain live connection state");
    CHECK(xworkRuntimeDisconnectMcpServer(pRuntime, "batch-one", &tError) &&
              xworkRuntimeMcpServerGetInfo(pRuntime, "batch-one", &tServerInfo) &&
              tServerInfo.bEnabled && !tServerInfo.bConnected &&
              tServerInfo.eState == XWORK_MCP_SERVER_DISCONNECTED,
          "manual MCP disconnect preserves the enabled definition");
    CHECK(xworkRuntimeRefreshMcpServer(pRuntime, "batch-one", NULL, XLLM_DEADLINE_NEVER, &tError) &&
              xworkRuntimeMcpServerGetInfo(pRuntime, "batch-one", &tServerInfo) &&
              tServerInfo.bConnected,
          "a disconnected enabled MCP server reconnects explicitly");
    arrBatch[0].sSummary = "Changed source-managed MCP server.";
    arrBatch[1].sServerId = "batch-three";
    arrBatch[1].sSummary = "Replacement source-managed MCP server.";
    CHECK(xworkRuntimeReplaceMcpServersBySource(pRuntime, "test.batch", arrBatch, 2u, &iReplaced,
                                                &tError) &&
              iReplaced == 2u && xworkRuntimeMcpServerCount(pRuntime) == 2u &&
              xworkRuntimeMcpServerGetInfo(pRuntime, "batch-one", &tServerInfo) &&
              strcmp(tServerInfo.sSummary, "Changed source-managed MCP server.") == 0 &&
              !xworkRuntimeMcpServerGetInfo(pRuntime, "batch-two", &tServerInfo) &&
              xworkRuntimeMcpServerGetInfo(pRuntime, "batch-three", &tServerInfo),
          "MCP source replacement swaps changed, removed, and added ids together");
    xworkMcpServerConfigInit(&tServerConfig);
    tServerConfig.sServerId = "foreign";
    tServerConfig.sSummary = "Independently registered MCP server.";
    tServerConfig.sProgram = g_sSelfPath;
    tServerConfig.psArguments = arrArgs;
    tServerConfig.iArgumentCount = 1u;
    tServerConfig.sProtocolVersion = "2025-11-25";
    tServerConfig.uRequestTimeoutMs = 5000u;
    CHECK(xworkRuntimeRegisterMcpServer(pRuntime, &tServerConfig, &tError),
          "independent MCP source fixture registers");
    arrBatch[1].sServerId = "foreign";
    CHECK(!xworkRuntimeReplaceMcpServersBySource(pRuntime, "test.batch", arrBatch, 2u, &iReplaced,
                                                 &tError) &&
              xworkRuntimeMcpServerCount(pRuntime) == 3u &&
              xworkRuntimeMcpServerGetInfo(pRuntime, "batch-three", &tServerInfo),
          "cross-source MCP id conflict preserves the published batch");
    arrBatch[1].sServerId = "batch-three";
    {
        xwork_mcp_client *pBusy = xwork__runtime_mcp_find(pRuntime, "batch-one");
        bool bBusyLocked = pBusy && xwork__mcp_try_lock(pBusy);
        bool bRejected;
        arrBatch[0].sSummary = "Busy replacement must not publish.";
        bRejected = bBusyLocked && !xworkRuntimeReplaceMcpServersBySource(
                                       pRuntime, "test.batch", arrBatch, 2u, &iReplaced, &tError);
        if (bBusyLocked)
            xwork__mcp_unlock(pBusy);
        CHECK(bRejected && xworkRuntimeMcpServerGetInfo(pRuntime, "batch-one", &tServerInfo) &&
                  strcmp(tServerInfo.sSummary, "Changed source-managed MCP server.") == 0,
              "busy MCP source rejects replacement before any publication");
    }
    CHECK(xworkRuntimeReplaceMcpServersBySource(pRuntime, "test.batch", NULL, 0u, &iReplaced,
                                                &tError) &&
              iReplaced == 2u && xworkRuntimeMcpServerCount(pRuntime) == 1u &&
              xworkRuntimeUnregisterMcpServer(pRuntime, "foreign", &tError),
          "empty MCP source batch retires only its owned definitions");

    xworkRunResultUnit(&tResult);
    xworkAgentDestroy(pAgent);
    xllmSessionDestroy(pSession);
    xworkAgentDefinitionRelease(pDefinition);
    xworkRuntimeRelease(pRuntime);
    (void)xrtDirRemoveAll(sWorkspace);
}

static void test_readonly_subagent(void)
{
    static const char sWorkspace[] = "tests/tmp_xwork_subagent";
    char sEvidence[2048];
    xllm_session_config tSessionConfig;
    xllm_session *pSession = NULL;
    xllm_error tLlmError;
    xwork_agent_config tAgentConfig;
    xwork_agent *pParent = NULL;
    xwork_readonly_subagent_config tSubagentConfig;
    xwork_run_result tResult;
    xwork_error tError;
    subagent_mock tMock;
    xwork_result eResult = XWORK_RESULT_ERROR;
    size_t i;
    memset(&tMock, 0, sizeof(tMock));
    tMock.bScopedEvents = true;
    memset(&tResult, 0, sizeof(tResult));
    for (i = 0u; i + 1u < sizeof(sEvidence); ++i)
    {
        static const char sMarker[] = "readonly-evidence-marker ";
        sEvidence[i] = sMarker[i % (sizeof(sMarker) - 1u)];
    }
    sEvidence[sizeof(sEvidence) - 1u] = '\0';
    (void)xrtDirRemoveAll(sWorkspace);
    CHECK(xrtDirCreateAll("tests/tmp_xwork_subagent/.xcode") &&
              xrtFileWriteAtomic("tests/tmp_xwork_subagent/evidence.txt",
                                 (xbytesview){(const uint8 *)sEvidence, strlen(sEvidence)}) &&
              xrtFileWriteAtomic(
                  "tests/tmp_xwork_subagent/.xcode/secrets.local.json",
                  (xbytesview){(const uint8 *)"private-fixture", strlen("private-fixture")}),
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
    if (pParent)
    {
        eResult = xworkAgentRunReadOnlySubagent(
            pParent, &tSubagentConfig, "Inspect evidence and report isolation.", &tResult, &tError);
    }
    CHECK(eResult == XWORK_RESULT_OK && tMock.uModelCalls == 2u && tResult.uAgentTurns == 2u &&
              tResult.uToolCalls == 2u,
          "isolated read-only subagent completes a bounded tool loop");
    CHECK(tMock.bReadOnlyToolSet && tMock.bSawReadableFile && tMock.bSawInternalDenial,
          "subagent exposes only inspection tools and denies internal control paths");
    CHECK(tMock.bScopedEvents && tMock.uAgentStarts == 1u && tMock.uAgentDone == 1u &&
              tMock.uToolStarts == 2u && tMock.uToolDone == 2u && tResult.uAgentDepth == 1u &&
              tResult.uDelegationId == 1u,
          "subagent lifecycle is tagged with depth, delegation, and parent turn");
    CHECK(tResult.sFinalText && strlen(tResult.sFinalText) <= 256u &&
              strstr(tResult.sFinalText, "truncated by host budget") &&
              !xrtDirExists((str) "tests/tmp_xwork_subagent/.xcode/artifacts"),
          "subagent final output is capped without artifact side effects");
    xworkRunResultUnit(&tResult);
    memset(&tResult, 0, sizeof(tResult));
    tMock.uModelCalls = 0u;
    tSubagentConfig.uMaxAgentTurns = 1u;
    tSubagentConfig.OnEvent = NULL;
    tSubagentConfig.pEventUserData = NULL;
    eResult = pParent ? xworkAgentRunReadOnlySubagent(pParent, &tSubagentConfig,
                                                      "Exercise the hard child turn budget.",
                                                      &tResult, &tError)
                      : XWORK_RESULT_ERROR;
    CHECK(eResult == XWORK_RESULT_LIMIT && tError.eCode == XWORK_ERROR_LOOP_GUARD &&
              tResult.uAgentTurns == 1u && tResult.uAgentDepth == 1u && tResult.uDelegationId == 2u,
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
    xllm_session *pSession = NULL;
    xllm_error tLlmError;
    xwork_agent_config tAgentConfig;
    xwork_agent *pAgent = NULL;
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
    tAgentConfig.uDeadline = xllmDeadlineAfterUs(0u);
    tAgentConfig.sWorkspaceRoot = sWorkspace;
    tAgentConfig.OnModelComplete = mock_complete;
    tAgentConfig.pModelUserData = &tMock;
    pAgent = xworkAgentCreate(&tAgentConfig, &tError);
    CHECK(pSession && pAgent, "deadline-scoped agent creates");
    if (pSession)
        uTurnBefore = xllmSessionCurrentTurn(pSession);
    if (pAgent)
    {
        CHECK(xworkAgentRun(pAgent, "This prompt must not be appended.", &tResult, &tError) ==
                      XWORK_RESULT_TIMEOUT &&
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
    xllm_session *pSession = NULL;
    xwork_agent_config tAgentConfig;
    xwork_agent *pAgent = NULL;
    xwork_error tError;
    const xwork_tool_entry *pExecTool;
    const xwork_tool_entry *pSpawnTool;
    const xwork_tool_entry *pWaitTool;
    const xwork_tool_entry *pStopTool;
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
    CHECK(pExecTool &&
              test_entry_execute(pExecTool, pExecTool->pUserData, &tCtx,
                                   "{\"argv\":[\"cmd\",\"/c\",\"echo\",\"task-ok\"]}", &tOut,
                                   &tError) == XWORK_RESULT_OK &&
              tOut.bSuccess && tOut.sContent && strstr(tOut.sContent, "task-ok"),
          "exec runs argv directly and captures stdout");
#else
    CHECK(pExecTool &&
              test_entry_execute(pExecTool, pExecTool->pUserData, &tCtx, "{\"argv\":[\"echo\",\"task-ok\"]}",
                                   &tOut, &tError) == XWORK_RESULT_OK &&
              tOut.bSuccess && tOut.sContent && strstr(tOut.sContent, "task-ok"),
          "exec runs argv directly and captures stdout");
#endif
    xworkToolOutputUnit(&tOut);

    /* Two background tasks: fast with notify, slow with a remind clock. */
    xworkToolOutputInit(&tOut);
#if defined(_WIN32)
    CHECK(pSpawnTool &&
              test_entry_execute(pSpawnTool, pSpawnTool->pUserData, &tCtx,
                                    "{\"argv\":[\"ping\",\"-n\",\"1\",\"127.0.0.1\"],\"notify\":"
                                    "\"fast probe finished; collect its tail\"}",
                                    &tOut, &tError) == XWORK_RESULT_OK &&
              tOut.bSuccess && tOut.sContent &&
              test_task_id(tOut.sContent, &uFast) == 1,
          "spawn starts the fast task with a notify message");
#else
    CHECK(pSpawnTool &&
              test_entry_execute(pSpawnTool, 
                  pSpawnTool->pUserData, &tCtx,
                  "{\"argv\":[\"true\"],\"notify\":\"fast probe finished; collect its tail\"}",
                  &tOut, &tError) == XWORK_RESULT_OK &&
              tOut.bSuccess && tOut.sContent &&
              test_task_id(tOut.sContent, &uFast) == 1,
          "spawn starts the fast task with a notify message");
#endif
    xworkToolOutputUnit(&tOut);
    xworkToolOutputInit(&tOut);
#if defined(_WIN32)
    CHECK(pSpawnTool &&
              test_entry_execute(pSpawnTool, 
                  pSpawnTool->pUserData, &tCtx,
                  "{\"argv\":[\"ping\",\"-n\",\"30\",\"127.0.0.1\"],\"remind_after_ms\":100}",
                  &tOut, &tError) == XWORK_RESULT_OK &&
              tOut.bSuccess && tOut.sContent &&
              test_task_id(tOut.sContent, &uSlow) == 1,
          "spawn starts the slow task with a model-set reminder");
#else
    CHECK(pSpawnTool &&
              test_entry_execute(pSpawnTool, pSpawnTool->pUserData, &tCtx,
                                    "{\"argv\":[\"sleep\",\"30\"],\"remind_after_ms\":100}", &tOut,
                                    &tError) == XWORK_RESULT_OK &&
              tOut.bSuccess && tOut.sContent &&
              test_task_id(tOut.sContent, &uSlow) == 1,
          "spawn starts the slow task with a model-set reminder");
#endif
    xworkToolOutputUnit(&tOut);

    /* wait(any) returns with the fast task exited and the slow one running. */
    if (uFast && uSlow && pWaitTool)
    {
        xworkToolOutputInit(&tOut);
        (void)snprintf(sWaitArgs, sizeof(sWaitArgs),
                       "{\"task_ids\":[%" PRIu64 ",%" PRIu64
                       "],\"mode\":\"any\",\"timeout_ms\":15000}",
                       uFast, uSlow);
        CHECK(test_entry_execute(pWaitTool, pWaitTool->pUserData, &tCtx, sWaitArgs, &tOut, &tError) ==
                      XWORK_RESULT_OK &&
                  tOut.bSuccess && tOut.sContent && strstr(tOut.sContent, "\"state\":\"succeeded\"") &&
                  strstr(tOut.sContent, "\"state\":\"running\""),
              "wait(any) reports the finished task beside the running one");
        xworkToolOutputUnit(&tOut);
    }

    /* The completion notice carries the model's notify message exactly once. */
    if (pAgent)
    {
        size_t iTaken = xworkAgentTakeTaskNotices(pAgent, tNotices, 4u);
        CHECK(iTaken == 1u && tNotices[0].uTaskId == uFast &&
                  tNotices[0].eKind == XWORK_TASK_PROCESS && tNotices[0].bExitedCleanly &&
                  tNotices[0].sNotify &&
                  strcmp(tNotices[0].sNotify, "fast probe finished; collect its tail") == 0,
              "task notice delivers the model's notify message");
        CHECK(xworkAgentTakeTaskNotices(pAgent, tNotices, 4u) == 0u,
              "consumed notices are not redelivered");
    }

    /* The model-clock watchdog fires on the stalled reminder. */
    if (pAgent)
    {
        mdo_test_sleep_ms(200u);
        CHECK(xworkTaskWatchdog(pAgent, &tDigest) && tDigest.bShouldWake &&
                  tDigest.iStalledTasks == 1u && tDigest.iRunningTasks >= 1u,
              "watchdog wakes on the model-set remind deadline");
    }

    /* Stop and release both tasks. */
    if (uFast && pStopTool)
    {
        xworkToolOutputInit(&tOut);
        (void)snprintf(sWaitArgs, sizeof(sWaitArgs), "{\"task_id\":%" PRIu64 ",\"release\":true}",
                       uFast);
        CHECK(test_entry_execute(pStopTool, pStopTool->pUserData, &tCtx, sWaitArgs, &tOut, &tError) ==
                  XWORK_RESULT_OK,
              "fast task released after its notice");
        xworkToolOutputUnit(&tOut);
    }
    if (uSlow && pStopTool)
    {
        xworkToolOutputInit(&tOut);
        (void)snprintf(sWaitArgs, sizeof(sWaitArgs),
                       "{\"task_id\":%" PRIu64 ",\"mode\":\"kill_tree\",\"release\":true}", uSlow);
        CHECK(test_entry_execute(pStopTool, pStopTool->pUserData, &tCtx, sWaitArgs, &tOut, &tError) ==
                      XWORK_RESULT_OK &&
                  tOut.bSuccess,
              "stop releases the stalled task");
        xworkToolOutputUnit(&tOut);
    }
    CHECK(pAgent && xworkRuntimeTaskCount(pAgent->pRuntime) == 0u,
          "task table drains after release");

    /* wait(all) on two quick tasks. */
    if (pSpawnTool && pWaitTool)
    {
        uint64_t uA = 0u;
        uint64_t uB = 0u;
        xworkToolOutputInit(&tOut);
#if defined(_WIN32)
        CHECK(test_entry_execute(pSpawnTool, pSpawnTool->pUserData, &tCtx,
                                    "{\"argv\":[\"ping\",\"-n\",\"1\",\"127.0.0.1\"]}", &tOut,
                                    &tError) == XWORK_RESULT_OK &&
                  tOut.bSuccess && tOut.sContent &&
                  test_task_id(tOut.sContent, &uA) == 1,
              "wait-all fixture task A starts");
#else
        CHECK(test_entry_execute(pSpawnTool, pSpawnTool->pUserData, &tCtx, "{\"argv\":[\"true\"]}", &tOut,
                                    &tError) == XWORK_RESULT_OK &&
                  tOut.bSuccess && tOut.sContent &&
                  test_task_id(tOut.sContent, &uA) == 1,
              "wait-all fixture task A starts");
#endif
        xworkToolOutputUnit(&tOut);
        xworkToolOutputInit(&tOut);
#if defined(_WIN32)
        CHECK(test_entry_execute(pSpawnTool, pSpawnTool->pUserData, &tCtx,
                                    "{\"argv\":[\"ping\",\"-n\",\"2\",\"127.0.0.1\"]}", &tOut,
                                    &tError) == XWORK_RESULT_OK &&
                  tOut.bSuccess && tOut.sContent &&
                  test_task_id(tOut.sContent, &uB) == 1,
              "wait-all fixture task B starts");
#else
        CHECK(test_entry_execute(pSpawnTool, pSpawnTool->pUserData, &tCtx, "{\"argv\":[\"sleep\",\"1\"]}",
                                    &tOut, &tError) == XWORK_RESULT_OK &&
                  tOut.bSuccess && tOut.sContent &&
                  test_task_id(tOut.sContent, &uB) == 1,
              "wait-all fixture task B starts");
#endif
        xworkToolOutputUnit(&tOut);
        if (uA && uB)
        {
            xworkToolOutputInit(&tOut);
            (void)snprintf(sWaitArgs, sizeof(sWaitArgs),
                           "{\"task_ids\":[%" PRIu64 ",%" PRIu64
                           "],\"mode\":\"all\",\"timeout_ms\":15000}",
                           uA, uB);
            CHECK(test_entry_execute(pWaitTool, pWaitTool->pUserData, &tCtx, sWaitArgs, &tOut, &tError) ==
                          XWORK_RESULT_OK &&
                      tOut.bSuccess && tOut.sContent && strstr(tOut.sContent, "\"state\":\"succeeded\""),
                  "wait(all) returns with both tasks exited");
            xworkToolOutputUnit(&tOut);
            (void)snprintf(sWaitArgs, sizeof(sWaitArgs),
                           "{\"task_id\":%" PRIu64 ",\"release\":true}", uA);
            if (pStopTool)
                (void)test_entry_execute(pStopTool, pStopTool->pUserData, &tCtx, sWaitArgs, &tOut, &tError);
            (void)snprintf(sWaitArgs, sizeof(sWaitArgs),
                           "{\"task_id\":%" PRIu64 ",\"release\":true}", uB);
            if (pStopTool)
                (void)test_entry_execute(pStopTool, pStopTool->pUserData, &tCtx, sWaitArgs, &tOut, &tError);
            xworkToolOutputUnit(&tOut);
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
    xllm_session *pSession = NULL;
    xwork_agent_config tAgentConfig;
    xwork_agent *pAgent = NULL;
    xwork_error tError;
    const xwork_tool_entry *pEditTool;
    const xwork_tool_entry *pWriteTool;
    const xwork_tool_entry *pReadTool;
    xwork_tool_context tCtx;
    xwork_tool_output tOut;
    size_t iSize = 0u;
    char *sFile = NULL;

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
    CHECK(pWriteTool &&
              test_entry_execute(pWriteTool, pWriteTool->pUserData, &tCtx,
                                    "{\"path\":\"a/b/c.txt\",\"content\":\"alpha\\r\\nbeta\\r\\n "
                                    "gamma delta\\r\\n\",\"mode\":\"create\"}",
                                    &tOut, &tError) == XWORK_RESULT_OK &&
              tOut.bSuccess && tOut.sContent && strstr(tOut.sContent, "created parent directories"),
          "write auto-creates parents and reports them");
    xworkToolOutputUnit(&tOut);
    sFile = (char *)xrtFileReadAll("tests/tmp_xwork_edit/a/b/c.txt", &iSize);
    CHECK(sFile && memchr(sFile, '\r', iSize) == NULL, "AUTO stores new files in pure LF");
    xrtFree(sFile);
    sFile = NULL;

    /* Batch edit: two disjoint edits in one atomic call. */
    xworkToolOutputInit(&tOut);
    CHECK(pEditTool &&
              test_entry_execute(pEditTool, pEditTool->pUserData, &tCtx,
                                   "{\"path\":\"a/b/"
                                   "c.txt\",\"edits\":[{\"old_text\":\"alpha\",\"new_text\":"
                                   "\"ALPHA\"},{\"old_text\":\"gamma\",\"new_text\":\"GAMMA\"}]}",
                                   &tOut, &tError) == XWORK_RESULT_OK &&
              tOut.bSuccess && tOut.sContent && strstr(tOut.sContent, "applied 2 edits"),
          "edit applies a disjoint batch atomically");
    xworkToolOutputUnit(&tOut);

    /* replace_all replaces every occurrence in one edit. */
    xworkToolOutputInit(&tOut);
    CHECK(
        pEditTool &&
            test_entry_execute(pEditTool, 
                pEditTool->pUserData, &tCtx,
                "{\"path\":\"a/b/"
                "c.txt\",\"edits\":[{\"old_text\":\"a\",\"new_text\":\"-\",\"replace_all\":true}]}",
                &tOut, &tError) == XWORK_RESULT_OK &&
            tOut.bSuccess,
        "replace_all edits every occurrence");
    xworkToolOutputUnit(&tOut);

    /* FORCE_CRLF: the same file round-trips with CRLF storage while the
     * model keeps editing in LF. */
    if (pAgent)
        pAgent->eEolPolicy = XWORK_EOL_FORCE_CRLF;
    xworkToolOutputInit(&tOut);
    CHECK(pEditTool &&
              test_entry_execute(pEditTool, 
                  pEditTool->pUserData, &tCtx,
                  "{\"path\":\"a/b/"
                  "c.txt\",\"edits\":[{\"old_text\":\"ALPHA\",\"new_text\":\"FINAL\"}]}",
                  &tOut, &tError) == XWORK_RESULT_OK &&
              tOut.bSuccess,
          "FORCE_CRLF edit matches in LF space");
    xworkToolOutputUnit(&tOut);
    sFile = (char *)xrtFileReadAll("tests/tmp_xwork_edit/a/b/c.txt", &iSize);
    CHECK(sFile && strstr(sFile, "\r\n") != NULL, "FORCE_CRLF stores CRLF");
    xrtFree(sFile);
    sFile = NULL;
    /* Read still shows LF to the model. */
    xworkToolOutputInit(&tOut);
    CHECK(pReadTool &&
              test_entry_execute(pReadTool, pReadTool->pUserData, &tCtx, "{\"path\":\"a/b/c.txt\"}", &tOut,
                                   &tError) == XWORK_RESULT_OK &&
              tOut.bSuccess && tOut.sContent && strstr(tOut.sContent, "FINAL") &&
              strstr(tOut.sContent, "\r\n") == NULL,
          "read normalizes display to LF under any policy");
    xworkToolOutputUnit(&tOut);
    if (pAgent)
        pAgent->eEolPolicy = XWORK_EOL_AUTO;

    xworkAgentDestroy(pAgent);
    xllmSessionDestroy(pSession);
    (void)xrtDirRemoveAll(sWorkspace);
}

static void test_command_context_deadline(void)
{
    static const char sWorkspace[] = "tests/tmp_xwork_command_deadline";
    xllm_session_config tSessionConfig;
    xllm_session *pSession = NULL;
    xllm_error tLlmError;
    xwork_agent_config tAgentConfig;
    xwork_agent *pAgent = NULL;
    xwork_error tError;
    mock_model tMock;
    const xwork_tool_entry *pExecTool;
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
    tAgentConfig.uDeadline = xllmDeadlineAfterUs(UINT64_C(300000));
    tAgentConfig.sWorkspaceRoot = sWorkspace;
    tAgentConfig.OnModelComplete = mock_complete;
    tAgentConfig.pModelUserData = &tMock;
    pAgent = xworkAgentCreate(&tAgentConfig, &tError);
    pExecTool = pAgent ? xwork__find_tool(pAgent, "exec") : NULL;
    tToolContext.pAgent = pAgent;
    tToolContext.sWorkspaceRoot = sWorkspace;
    xworkToolOutputInit(&tOutput);
    uStartedMs = xllmClockUs() / UINT64_C(1000);
#if defined(_WIN32)
    if (pExecTool)
        eResult = test_entry_execute(pExecTool, 
            pExecTool->pUserData, &tToolContext,
            "{\"argv\":[\"ping\",\"-n\",\"6\",\"127.0.0.1\"],\"timeout_ms\":5000}", &tOutput,
            &tError);
#else
    if (pExecTool)
        eResult = test_entry_execute(pExecTool, pExecTool->pUserData, &tToolContext,
                                       "{\"argv\":[\"sleep\",\"5\"],\"timeout_ms\":5000}", &tOutput,
                                       &tError);
#endif
    uElapsedMs = xllmClockUs() / UINT64_C(1000) - uStartedMs;
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
    xllm_session *pSession = NULL;
    xwork_agent_config tAgentConfig;
    xwork_agent *pAgent = NULL;
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
    CHECK(pSession && pAgent && xworkAgentToolCount(pAgent) == 12u,
          "executor host agent carries the builtin registry");
    memset(&tExecutor, 0, sizeof(tExecutor));
    CHECK(pAgent && xworkExecutorBind(&tExecutor, pAgent, &tError), "executor binding attaches");

    xllmRequestInit(&tRequest);
    CHECK(tExecutor.pListTools && tExecutor.pListTools(tExecutor.pUserData, &tRequest) &&
              tRequest.iToolCount == 12u,
          "executor lists the builtin registry into a request");
    xllmRequestUnit(&tRequest);

    memset(&tCall, 0, sizeof(tCall));
    tCall.sId = (char *)"exec-1";
    tCall.sName = (char *)"write";
    tCall.sArgumentsJson =
        (char *)"{\"path\":\"note.txt\",\"content\":\"executor wrote this\",\"mode\":\"create\"}";
    memset(&tCtx, 0, sizeof(tCtx));
    tCtx.uRound = 1u;
    tCtx.uDeadline = XLLM_DEADLINE_NEVER;
    memset(&tOut, 0, sizeof(tOut));
    CHECK(tExecutor.pExecute && tExecutor.pExecute(tExecutor.pUserData, &tCall, &tCtx, &tOut) &&
              tOut.bSuccess && tOut.sContent && strstr(tOut.sContent, "status: success"),
          "executor runs write_file through the full policy path");

    tCall.sName = (char *)"definitely_not_a_tool";
    memset(&tOut, 0, sizeof(tOut));
    CHECK(tExecutor.pExecute(tExecutor.pUserData, &tCall, &tCtx, &tOut) && !tOut.bSuccess &&
              strstr(tOut.sContent, "unknown tool name"),
          "unknown tools become tool-level failures, not run failures");

    CHECK(xrtFileExists((str) "tests/tmp_xwork_exec/note.txt"),
          "executor side effect landed in the workspace");
    xworkExecutorUnbind(&tExecutor);
    xworkAgentDestroy(pAgent);
    xllmSessionDestroy(pSession);
    (void)xrtDirRemoveAll(sWorkspace);
}

typedef struct batch_schedule_fixture
{
    xmutex *pLock;
    uint32_t uActive;
    uint32_t uMaxActive;
    uint32_t uDescribeCalls;
    uint32_t uExecuteCalls;
    uint32_t uCompletionCount;
    char sCompletionOrder[2][32];
    uint32_t uModelCalls;
    bool bSessionOrder;
} batch_schedule_fixture;

static xwork_result batch_schedule_describe(void *pUserData, const xwork_tool_context *pContext,
                                            const char *sArgumentsJson,
                                            xwork_permission_resource_writer *pWriter,
                                            xwork_error *pError)
{
    batch_schedule_fixture *pFixture = (batch_schedule_fixture *)pUserData;
    const char *sPath = strstr(sArgumentsJson, "b.txt") ? "b.txt" : "a.txt";
    xwork_resource_access uAccess = (pContext->uEffects & XWORK_TOOL_EFFECT_WORKSPACE_WRITE)
                                        ? XWORK_RESOURCE_ACCESS_WRITE
                                        : XWORK_RESOURCE_ACCESS_READ;
    (void)pError;
    ++pFixture->uDescribeCalls;
    if (strstr(sArgumentsJson, "describe_fail"))
    {
        return XWORK_RESULT_ERROR;
    }
    return xworkPermissionResourceWriterAdd(pWriter, XWORK_RESOURCE_PATH, uAccess, sPath)
               ? XWORK_RESULT_OK
               : XWORK_RESULT_ERROR;
}

static xwork_result batch_schedule_execute(void *pUserData, const xwork_tool_context *pContext,
                                           const char *sArgumentsJson, xwork_tool_output *pOutput,
                                           xwork_error *pError)
{
    batch_schedule_fixture *pFixture = (batch_schedule_fixture *)pUserData;
    uint32_t i;
    bool bParallel = strstr(sArgumentsJson, "parallel") != NULL;
    bool bReady = false;
    (void)pError;
    if (!xrtMutexLock(pFixture->pLock))
        return XWORK_RESULT_ERROR;
    ++pFixture->uActive;
    ++pFixture->uExecuteCalls;
    if (pFixture->uMaxActive < pFixture->uActive)
    {
        pFixture->uMaxActive = pFixture->uActive;
    }
    (void)xrtMutexUnlock(pFixture->pLock);
    if (bParallel)
    {
        for (i = 0u; i < 1000u; ++i)
        {
            if (!xrtMutexLock(pFixture->pLock))
                return XWORK_RESULT_ERROR;
            bReady = pFixture->uMaxActive >= 2u;
            (void)xrtMutexUnlock(pFixture->pLock);
            if (bReady)
                break;
            mdo_test_sleep_ms(1u);
        }
        if (pContext->sToolCallId && strcmp(pContext->sToolCallId, "parallel-0") == 0)
        {
            mdo_test_sleep_ms(20u);
        }
    }
    if (!xrtMutexLock(pFixture->pLock))
        return XWORK_RESULT_ERROR;
    if (pFixture->uCompletionCount < 2u)
    {
        (void)snprintf(pFixture->sCompletionOrder[pFixture->uCompletionCount],
                       sizeof(pFixture->sCompletionOrder[0]), "%s",
                       pContext->sToolCallId ? pContext->sToolCallId : "");
    }
    ++pFixture->uCompletionCount;
    --pFixture->uActive;
    (void)xrtMutexUnlock(pFixture->pLock);
    return xworkToolOutputSet(pOutput, true,
                              pContext->sToolCallId ? pContext->sToolCallId : "completed")
               ? XWORK_RESULT_OK
               : XWORK_RESULT_ERROR;
}

static xllm_result batch_schedule_model(void *pUserData, const xllm_request *pRequest,
                                        const xllm_stream_callbacks *pCallbacks,
                                        xllm_response **ppResponse, xllm_error *pError)
{
    batch_schedule_fixture *pFixture = (batch_schedule_fixture *)pUserData;
    xllm_response *pResponse;
    size_t i;
    size_t iToolResult = 0u;
    (void)pCallbacks;
    (void)pError;
    *ppResponse = NULL;
    ++pFixture->uModelCalls;
    if (pFixture->uModelCalls == 1u)
    {
        pResponse = mock_response("", 2u);
        if (!pResponse ||
            !mock_set_call(pResponse, 0u, "parallel-0", "fixture.read",
                           "{\"path\":\"a.txt\",\"mode\":\"parallel\"}") ||
            !mock_set_call(pResponse, 1u, "parallel-1", "fixture.read",
                           "{\"path\":\"a.txt\",\"mode\":\"parallel\"}"))
        {
            xllmResponseDestroy(pResponse);
            return XLLM_RESULT_ERROR;
        }
    }
    else
    {
        bool bOrdered = true;
        for (i = 0u; i < pRequest->iMessageCount; ++i)
        {
            const xllm_message *pMessage = &pRequest->pMessages[i];
            if (pMessage->eRole != XLLM_ROLE_TOOL)
                continue;
            if (iToolResult == 0u)
            {
                bOrdered = pMessage->sToolCallId &&
                           strcmp(pMessage->sToolCallId, "parallel-0") == 0 && pMessage->sContent &&
                           strstr(pMessage->sContent, "parallel-0");
            }
            else if (iToolResult == 1u)
            {
                bOrdered = bOrdered && pMessage->sToolCallId &&
                           strcmp(pMessage->sToolCallId, "parallel-1") == 0 && pMessage->sContent &&
                           strstr(pMessage->sContent, "parallel-1");
            }
            ++iToolResult;
        }
        pFixture->bSessionOrder = bOrdered && iToolResult == 2u;
        pResponse = mock_response("parallel batch complete", 0u);
    }
    *ppResponse = pResponse;
    return pResponse ? XLLM_RESULT_OK : XLLM_RESULT_ERROR;
}

static void test_parallel_tool_schedule(void)
{
    static const char sWorkspace[] = "tests/tmp_xwork_schedule";
    xllm_session_config tSessionConfig;
    xllm_session *pSession = NULL;
    xwork_agent_config tAgentConfig;
    xwork_agent *pAgent = NULL;
    xwork_tool_definition tTool;
    xwork_tool_concurrency_config tConcurrency;
    xwork_tool_batch_plan *pPlan = NULL;
    xwork_tool_batch_item_info tItem;
    xllm_tool_call arrCalls[5];
    xllm_executor_ctx tContext;
    xllm_executor tExecutor;
    xllm_run_summary tSummary;
    xllm_error tLlmError;
    xwork_error tError;
    batch_schedule_fixture tFixture;
    bool bPlanShape = true;
    static const uint32_t arrExpectedWaves[5] = {0u, 0u, 1u, 0u, 2u};
    size_t i;

    memset(&tFixture, 0, sizeof(tFixture));
    memset(&tExecutor, 0, sizeof(tExecutor));
    memset(&tSummary, 0, sizeof(tSummary));
    memset(&tContext, 0, sizeof(tContext));
    memset(arrCalls, 0, sizeof(arrCalls));
    (void)xrtDirRemoveAll(sWorkspace);
    CHECK(xrtDirCreateAll((str)sWorkspace), "parallel scheduler workspace created");
    tFixture.pLock = xrtMutexCreate();
    xllmSessionConfigInit(&tSessionConfig);
    pSession =
        xllmSessionCreateForTest(&tSessionConfig, batch_schedule_model, &tFixture, &tLlmError);
    xworkAgentConfigInit(&tAgentConfig);
    tAgentConfig.pSession = pSession;
    tAgentConfig.sWorkspaceRoot = sWorkspace;
    tAgentConfig.bRegisterBuiltinTools = false;
    tAgentConfig.uMaxParallelTools = 2u;
    pAgent = xworkAgentCreate(&tAgentConfig, &tError);
    CHECK(tFixture.pLock && pSession && pAgent, "parallel scheduler fixture creates");
    if (!tFixture.pLock || !pSession || !pAgent)
        goto cleanup;

    memset(&tTool, 0, sizeof(tTool));
    tTool.sName = "fixture.read";
    tTool.sDescription = "Read one fixture resource.";
    tTool.sParametersJson = "{\"type\":\"object\"}";
    tTool.bStrict = true;
    tTool.uEffects = XWORK_TOOL_EFFECT_READ_ONLY;
    tTool.OnExecute = batch_schedule_execute;
    tTool.pUserData = &tFixture;
    tTool.sSource = "fixture.schedule";
    tTool.OnDescribePermissions = batch_schedule_describe;
    CHECK(xworkAgentRegisterTool(pAgent, &tTool, &tError), "parallel read fixture registers");
    tTool.sName = "fixture.write";
    tTool.sDescription = "Write one fixture resource.";
    tTool.uEffects = XWORK_TOOL_EFFECT_WORKSPACE_WRITE;
    CHECK(xworkAgentRegisterTool(pAgent, &tTool, &tError), "parallel write fixture registers");
    xworkToolConcurrencyConfigInit(&tConcurrency);
    tConcurrency.sToolName = "fixture.read";
    tConcurrency.bParallelSafe = true;
    CHECK(xworkAgentConfigureToolConcurrency(pAgent, &tConcurrency, &tError),
          "read fixture opts into resource-aware concurrency");
    tConcurrency.sToolName = "fixture.write";
    CHECK(xworkAgentConfigureToolConcurrency(pAgent, &tConcurrency, &tError),
          "write fixture opts into resource-aware concurrency");

    arrCalls[0].sId = (char *)"r0";
    arrCalls[0].sName = (char *)"fixture.read";
    arrCalls[0].sArgumentsJson = (char *)"{\"path\":\"a.txt\"}";
    arrCalls[1].sId = (char *)"r1";
    arrCalls[1].sName = (char *)"fixture.read";
    arrCalls[1].sArgumentsJson = (char *)"{\"path\":\"a.txt\"}";
    arrCalls[2].sId = (char *)"w0";
    arrCalls[2].sName = (char *)"fixture.write";
    arrCalls[2].sArgumentsJson = (char *)"{\"path\":\"a.txt\"}";
    arrCalls[3].sId = (char *)"w1";
    arrCalls[3].sName = (char *)"fixture.write";
    arrCalls[3].sArgumentsJson = (char *)"{\"path\":\"b.txt\"}";
    arrCalls[4].sId = (char *)"r2";
    arrCalls[4].sName = (char *)"fixture.read";
    arrCalls[4].sArgumentsJson = (char *)"{\"path\":\"a.txt\"}";
    tContext.uTurn = 7u;
    pPlan = xworkAgentPlanToolBatch(pAgent, arrCalls, 5u, &tContext, &tError);
    for (i = 0u; pPlan && i < 5u; ++i)
    {
        xworkToolBatchItemInfoInit(&tItem);
        if (!xworkToolBatchPlanItemAt(pPlan, i, &tItem) || tItem.iSequence != i ||
            tItem.uWave != arrExpectedWaves[i] || !tItem.bParallelSafe ||
            tItem.iResourceCount != 1u)
        {
            bPlanShape = false;
        }
    }
    CHECK(pPlan && xworkToolBatchPlanCount(pPlan) == 5u &&
              xworkToolBatchPlanWaveCount(pPlan) == 3u && bPlanShape &&
              tFixture.uDescribeCalls == 5u,
          "resource conflicts produce stable sequences and deterministic waves");
    xworkToolBatchPlanRelease(pPlan);
    pPlan = NULL;

    xworkToolConcurrencyConfigInit(&tConcurrency);
    tConcurrency.sToolName = "fixture.read";
    tConcurrency.bParallelSafe = true;
    tConcurrency.sSerialGroup = "fixture.serial";
    CHECK(xworkAgentConfigureToolConcurrency(pAgent, &tConcurrency, &tError),
          "explicit serial module group configures");
    pPlan = xworkAgentPlanToolBatch(pAgent, arrCalls, 2u, &tContext, &tError);
    xworkToolBatchItemInfoInit(&tItem);
    CHECK(pPlan && xworkToolBatchPlanWaveCount(pPlan) == 2u &&
              xworkToolBatchPlanItemAt(pPlan, 1u, &tItem) && tItem.uWave == 1u,
          "same serial module is ordered even for read-only resources");
    xworkToolBatchPlanRelease(pPlan);
    pPlan = NULL;
    tConcurrency.sSerialGroup = NULL;
    CHECK(xworkAgentConfigureToolConcurrency(pAgent, &tConcurrency, &tError),
          "read fixture returns to resource-only scheduling");

    tFixture.uActive = 0u;
    tFixture.uMaxActive = 0u;
    tFixture.uExecuteCalls = 0u;
    tFixture.uCompletionCount = 0u;
    tFixture.uModelCalls = 0u;
    CHECK(xworkExecutorBind(&tExecutor, pAgent, &tError) &&
              tExecutor.uAbiVersion == XLLM_EXECUTOR_ABI_VERSION && tExecutor.pExecuteBatch &&
              tExecutor.pBatchResultsUnit,
          "executor publishes the optional owned batch contract");
    {
        xllm_tool_call tDenied;
        xllm_executor_result tDeniedResult;
        uint32_t uBefore = tFixture.uExecuteCalls;
        memset(&tDenied, 0, sizeof(tDenied));
        memset(&tDeniedResult, 0, sizeof(tDeniedResult));
        tDenied.sId = (char *)"denied-descriptor";
        tDenied.sName = (char *)"fixture.write";
        tDenied.sArgumentsJson = (char *)"{\"mode\":\"describe_fail\"}";
        CHECK(
            tExecutor.pExecuteBatch(tExecutor.pUserData, &tDenied, 1u, &tContext, &tDeniedResult) &&
                !tDeniedResult.bSuccess && tDeniedResult.sContent &&
                strstr(tDeniedResult.sContent, "descriptor") && tFixture.uExecuteCalls == uBefore,
            "permission descriptor failure fails closed before execution");
        tExecutor.pBatchResultsUnit(tExecutor.pUserData, &tDeniedResult, 1u);
    }
    CHECK(xworkAgentRunBegin(pAgent, &tError), "parallel session run pins the tool catalog");
    CHECK(xllmSessionRunWithTools(pSession, "run parallel reads", &tExecutor, NULL, NULL, &tSummary,
                                  &tLlmError) == XLLM_RESULT_OK,
          "parallel tool batch completes through xllm-session");
    xworkAgentRunEnd(pAgent);
    CHECK(tFixture.uMaxActive == 2u && tFixture.uCompletionCount == 2u &&
              strcmp(tFixture.sCompletionOrder[0], "parallel-1") == 0 &&
              strcmp(tFixture.sCompletionOrder[1], "parallel-0") == 0,
          "independent tools finish out of order under the bounded scheduler");
    CHECK(tFixture.bSessionOrder && tSummary.uToolCalls == 2u,
          "session commits batch results in original tool-call order");

cleanup:
    xworkToolBatchPlanRelease(pPlan);
    xllmRunSummaryUnit(&tSummary);
    xworkExecutorUnbind(&tExecutor);
    xworkAgentDestroy(pAgent);
    xllmSessionDestroy(pSession);
    if (tFixture.pLock)
        (void)xrtMutexDestroy(tFixture.pLock);
    (void)xrtDirRemoveAll(sWorkspace);
}

typedef struct
{
    unsigned iCalls;
    bool bSawTools;
    bool bFinalOnly;
} t1_script;

static xllm_response *t1_response_text(const char *sText)
{
    xllm_response *pResponse = (xllm_response *)calloc(1u, sizeof(*pResponse));
    if (!pResponse)
    {
        return NULL;
    }
    pResponse->sContent = test_strdup(sText);
    pResponse->sFinishReason = test_strdup("stop");
    if (!pResponse->sContent)
    {
        xllmResponseDestroy(pResponse);
        return NULL;
    }
    return pResponse;
}

static xllm_response *t1_response_write(void)
{
    xllm_response *pResponse = t1_response_text("");
    if (!pResponse)
    {
        return NULL;
    }
    pResponse->pToolCalls = (xllm_tool_call *)calloc(1u, sizeof(*pResponse->pToolCalls));
    if (!pResponse->pToolCalls)
    {
        xllmResponseDestroy(pResponse);
        return NULL;
    }
    pResponse->pToolCalls[0].sId = test_strdup("t1-call-1");
    pResponse->pToolCalls[0].sName = test_strdup("write");
    pResponse->pToolCalls[0].sArgumentsJson =
        test_strdup("{\"path\":\"t1.txt\",\"content\":\"t1 chain worked\",\"mode\":\"create\"}");
    pResponse->iToolCallCount = 1u;
    free(pResponse->sFinishReason);
    pResponse->sFinishReason = test_strdup("tool_calls");
    if (!pResponse->pToolCalls[0].sId || !pResponse->pToolCalls[0].sName ||
        !pResponse->pToolCalls[0].sArgumentsJson || !pResponse->sFinishReason)
    {
        xllmResponseDestroy(pResponse);
        return NULL;
    }
    return pResponse;
}

/* ------------------------------------------------------------------ */
/* Subagent delegation: archetype registry, foreground and background   */
/* `agent` tool runs, depth lock, model override, notices.              */
/* ------------------------------------------------------------------ */

typedef struct
{
    unsigned iCalls;
    char sLastModel[64];
    bool bSawDelegationPrompt;
} delegate_script;

static xllm_response *delegate_response_text(const char *sText) { return t1_response_text(sText); }

static xllm_result delegate_model_call(void *pUserData, const xllm_request *pRequest,
                                       const xllm_stream_callbacks *pCallbacks,
                                       xllm_response **ppResponse, xllm_error *pError)
{
    delegate_script *pScript = (delegate_script *)pUserData;
    (void)pCallbacks;
    if (pError)
    {
        xllmErrorInit(pError);
    }
    ++pScript->iCalls;
    if (pRequest->sModel)
    {
        (void)snprintf(pScript->sLastModel, sizeof(pScript->sLastModel), "%s", pRequest->sModel);
    }
    else
    {
        pScript->sLastModel[0] = '\0';
    }
    {
        size_t i;
        for (i = 0u; i < pRequest->iMessageCount; ++i)
        {
            const xllm_message *pMessage = &pRequest->pMessages[i];
            if (pMessage->eRole == XLLM_ROLE_USER && pMessage->sContent &&
                strstr(pMessage->sContent, "probe the workspace"))
            {
                pScript->bSawDelegationPrompt = true;
            }
            if (pMessage->eRole == XLLM_ROLE_SYSTEM && pMessage->sContent &&
                strstr(pMessage->sContent, "You are a fast read-only inspector."))
            {
                /* identity reached the child */
            }
        }
    }
    *ppResponse = delegate_response_text("probe complete: 3 files, no blockers");
    return *ppResponse ? XLLM_RESULT_OK : XLLM_RESULT_ERROR;
}

typedef struct configurable_subagent_model
{
    volatile long iEntered;
    volatile long iCancelled;
    volatile long iRelease;
    bool bHoldCancelled;
    uint32_t uCalls;
    bool bSawModel;
    bool bSawReasoning;
    bool bSawOutputBudget;
    bool bSawFiniteDeadline;
    bool bSawRead;
    bool bSawWrite;
    bool bSawAgent;
} configurable_subagent_model;

typedef struct configurable_subagent_events
{
    uint32_t uDepthMask;
    uint32_t uStarts;
    bool bParentRunPreserved;
    bool bDelegationIdsPresent;
    bool bSawContextBudget;
} configurable_subagent_events;

static xllm_result configurable_subagent_complete(void *pUserData, const xllm_request *pRequest,
                                                  const xllm_stream_callbacks *pCallbacks,
                                                  xllm_response **ppResponse, xllm_error *pError)
{
    static const char sLongReport[] =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    configurable_subagent_model *pModel = (configurable_subagent_model *)pUserData;
    size_t i;
    (void)pCallbacks;
    if (pError)
        xllmErrorInit(pError);
    ++pModel->uCalls;
    pModel->bSawModel = pRequest->sModel && strcmp(pRequest->sModel, "ling-tiny-test") == 0;
    pModel->bSawReasoning =
        pRequest->sReasoningEffort && strcmp(pRequest->sReasoningEffort, "high") == 0;
    pModel->bSawOutputBudget = pRequest->uMaxOutputTokens == 77u;
    pModel->bSawFiniteDeadline = pRequest->uDeadline != XLLM_DEADLINE_NEVER;
    for (i = 0u; i < pRequest->iToolCount; ++i)
    {
        if (strcmp(pRequest->pTools[i].sName, "read") == 0)
            pModel->bSawRead = true;
        if (strcmp(pRequest->pTools[i].sName, "write") == 0)
            pModel->bSawWrite = true;
        if (strcmp(pRequest->pTools[i].sName, "agent") == 0)
            pModel->bSawAgent = true;
    }
    *ppResponse = mock_response(sLongReport, 0u);
    return *ppResponse ? XLLM_RESULT_OK : XLLM_RESULT_ERROR;
}

static xllm_result nested_subagent_complete(void *pUserData, const xllm_request *pRequest,
                                            const xllm_stream_callbacks *pCallbacks,
                                            xllm_response **ppResponse, xllm_error *pError)
{
    configurable_subagent_model *pModel = (configurable_subagent_model *)pUserData;
    size_t i;
    bool bToolResult = request_has_role(pRequest, XLLM_ROLE_TOOL, 1u);
    bool bGrandchild = request_has_text(pRequest, "grandchild work");
    (void)pCallbacks;
    if (pError)
        xllmErrorInit(pError);
    ++pModel->uCalls;
    if (bGrandchild)
    {
        for (i = 0u; i < pRequest->iToolCount; ++i)
        {
            if (strcmp(pRequest->pTools[i].sName, "agent") == 0)
            {
                pModel->bSawAgent = true;
            }
        }
        *ppResponse = mock_response("grandchild private report", 0u);
    }
    else if (bToolResult)
    {
        *ppResponse = mock_response("child summary only", 0u);
    }
    else
    {
        *ppResponse = mock_response("", 1u);
        if (*ppResponse && !mock_set_call(*ppResponse, 0u, "nested-1", "agent",
                                          "{\"name\":\"nested\",\"prompt\":\"grandchild work\"}"))
        {
            xllmResponseDestroy(*ppResponse);
            *ppResponse = NULL;
        }
    }
    return *ppResponse ? XLLM_RESULT_OK : XLLM_RESULT_ERROR;
}

static bool configurable_subagent_event(void *pUserData, const xwork_event *pEvent)
{
    configurable_subagent_events *pEvents = (configurable_subagent_events *)pUserData;
    if (pEvent->eKind == XWORK_EVENT_AGENT_START)
    {
        ++pEvents->uStarts;
        if (pEvent->uAgentDepth < 32u)
        {
            pEvents->uDepthMask |= (uint32_t)1u << pEvent->uAgentDepth;
        }
        if (pEvent->uParentRunId != 77u)
            pEvents->bParentRunPreserved = false;
        if (pEvent->uDelegationId == 0u)
            pEvents->bDelegationIdsPresent = false;
    }
    if (pEvent->eKind == XWORK_EVENT_MODEL_START &&
        pEvent->tSessionStats.uContextWindowTokens == 32768u &&
        pEvent->tSessionStats.uMaxInputTokens == 20000u &&
        pEvent->tSessionStats.uMaxOutputTokens == 77u)
    {
        pEvents->bSawContextBudget = true;
    }
    return true;
}

static xllm_result blocking_subagent_complete(void *pUserData, const xllm_request *pRequest,
                                              const xllm_stream_callbacks *pCallbacks,
                                              xllm_response **ppResponse, xllm_error *pError)
{
    configurable_subagent_model *pModel = (configurable_subagent_model *)pUserData;
    uint64_t uGuard = xllmDeadlineAfterUs(UINT64_C(2000000));
    (void)pCallbacks;
    if (ppResponse)
        *ppResponse = NULL;
    xwork__atomic_store(&pModel->iEntered, 1);
    while (!pRequest->pCancel || !xrtCancelRequested(pRequest->pCancel))
    {
        if (xllmDeadlineExpired(uGuard))
        {
            if (pError)
            {
                xllmErrorInit(pError);
                pError->eCode = XLLM_ERROR_TIMEOUT;
                (void)snprintf(pError->sMessage, sizeof(pError->sMessage), "%s",
                               "bounded subagent cancellation fixture expired");
            }
            return XLLM_RESULT_TIMEOUT;
        }
        mdo_test_sleep_ms(1u);
    }
    xwork__atomic_store(&pModel->iCancelled, 1);
    while (pModel->bHoldCancelled && xwork__atomic_load(&pModel->iRelease) == 0 &&
           !xllmDeadlineExpired(uGuard))
        mdo_test_sleep_ms(1u);
    if (pError)
    {
        xllmErrorInit(pError);
        pError->eCode = XLLM_ERROR_CANCELLED;
        (void)snprintf(pError->sMessage, sizeof(pError->sMessage), "%s",
                       "subagent fixture observed cancellation");
    }
    return XLLM_RESULT_CANCELLED;
}

/* ------------------------------------------------------------------ */
/* Image passthrough: read detects an image, the executor forwards the  */
/* payload, the session records it as an IMAGE part on the tool result. */
/* ------------------------------------------------------------------ */

static void test_image_passthrough(void)
{
    static const char sWorkspace[] = "tests/tmp_xwork_image";
    static const unsigned char sPng[] = {0x89u, 'P',   'N',   'G',   0x0Du, 0x0Au, 0x1Au, 0x0Au,
                                         0x00u, 0x00u, 0x00u, 0x0Du, 'I',   'H',   'D',   'R',
                                         0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u,
                                         0x08u, 0x06u, 0x00u, 0x00u, 0x00u};
    xllm_session_config tSessionConfig;
    xllm_session *pSession = NULL;
    xwork_agent_config tAgentConfig;
    xwork_agent *pAgent = NULL;
    xwork_error tError;
    const xwork_tool_entry *pReadTool;
    xwork_tool_context tCtx;
    xwork_tool_output tOut;
    xllm_executor tExecutor;
    size_t i;

    (void)xrtDirRemoveAll(sWorkspace);
    CHECK(xrtDirCreateAll((str)sWorkspace), "image workspace created");
    CHECK(xrtFileWriteAtomic("tests/tmp_xwork_image/pic.dat", (xbytesview){sPng, sizeof(sPng)}),
          "png fixture written");

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
    CHECK(pReadTool &&
              test_entry_execute(pReadTool, pReadTool->pUserData, &tCtx, "{\"path\":\"pic.dat\"}", &tOut,
                                   &tError) == XWORK_RESULT_OK &&
              tOut.bSuccess && tOut.sContent && strstr(tOut.sContent, "image/png") &&
              tOut.pImageBytes && tOut.iImageSize == sizeof(sPng) &&
              memcmp(tOut.pImageBytes, sPng, sizeof(sPng)) == 0,
          "read detects the png magic and attaches the payload");
    xworkToolOutputUnit(&tOut);
    /* Extension lies: a text file named .png stays text. */
    CHECK(xrtFileWriteAtomic("tests/tmp_xwork_image/fake.png",
                             (xbytesview){(const uint8 *)"plain text", 10u}),
          "fake png written");
    xworkToolOutputInit(&tOut);
    CHECK(pReadTool &&
              test_entry_execute(pReadTool, pReadTool->pUserData, &tCtx, "{\"path\":\"fake.png\"}", &tOut,
                                   &tError) == XWORK_RESULT_OK &&
              tOut.bSuccess && !tOut.pImageBytes && strstr(tOut.sContent, "plain text"),
          "a lying extension does not trigger passthrough");
    xworkToolOutputUnit(&tOut);

    /* Full chain: executor forwards the image; the session tool message
     * carries it as an IMAGE part. */
    if (pAgent && xworkExecutorBind(&tExecutor, pAgent, &tError))
    {
        xllm_tool_call tCall;
        xllm_executor_ctx tECtx;
        xllm_executor_result tEResult;
        memset(&tCall, 0, sizeof(tCall));
        tCall.sId = (char *)"img-1";
        tCall.sName = (char *)"read";
        tCall.sArgumentsJson = (char *)"{\"path\":\"pic.dat\"}";
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
            tPair.sId = (char *)"img-1";
            tPair.sName = (char *)"read";
            tPair.sArgumentsJson = (char *)"{\"path\":\"pic.dat\"}";
            memset(&tPairResponse, 0, sizeof(tPairResponse));
            tPairResponse.sContent = (char *)"";
            tPairResponse.pToolCalls = &tPair;
            tPairResponse.iToolCallCount = 1u;
            CHECK(xllmSessionAddAssistantResponse(pSession, uTurn, &tPairResponse) &&
                      xllmSessionPendingToolCallCount(pSession) == 1u,
                  "image fixture call is pending before the result");
            CHECK(xllmSessionAddToolResultWithImage(pSession, uTurn, "img-1", tEResult.sContent,
                                                    tEResult.pImageBytes, tEResult.iImageSize,
                                                    tEResult.sImageMime),
                  "image tool result enters the ledger");
            CHECK(xllmSessionPendingToolCallCount(pSession) == 0u,
                  "the image result resolves its pending call");
        }
        xworkExecutorUnbind(&tExecutor);
    }
    else
    {
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
    xllm_session *pSession = NULL;
    xwork_agent_config tAgentConfig;
    xwork_agent *pAgent = NULL;
    xwork_error tError;
    xllm_error tLlmError;
    const xwork_tool_entry *pAgentTool;
    xwork_tool_context tCtx;
    xwork_tool_output tOut;
    delegate_script tScript;
    xwork_subagent_type tType;
    const char *sTools[2];
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
    CHECK(pAgentTool &&
              test_entry_execute(pAgentTool, 
                  pAgentTool->pUserData, &tCtx,
                  "{\"name\":\"probe\",\"prompt\":\"probe the workspace and report\"}", &tOut,
                  &tError) == XWORK_RESULT_OK &&
              tOut.bSuccess && tOut.sContent && strstr(tOut.sContent, "probe complete: 3 files") &&
              strstr(tOut.sContent, "final report"),
          "foreground delegation returns the child's final report");
    CHECK(tScript.iCalls >= 1u && tScript.bSawDelegationPrompt,
          "the child saw the delegation prompt in a fresh session");
    CHECK(strcmp(tScript.sLastModel, "ornith-35b") == 0,
          "the type's model override reaches every child request");
    xworkToolOutputUnit(&tOut);

    /* Unknown type: tool-level failure. */
    xworkToolOutputInit(&tOut);
    CHECK(pAgentTool &&
              test_entry_execute(pAgentTool, pAgentTool->pUserData, &tCtx,
                                    "{\"name\":\"nonexistent\",\"prompt\":\"x\"}", &tOut,
                                    &tError) == XWORK_RESULT_OK &&
              !tOut.bSuccess && tOut.sContent && strstr(tOut.sContent, "unknown subagent type"),
          "unknown roster names fail at tool level");
    xworkToolOutputUnit(&tOut);

    /* Background delegation: task table entry + notice delivery. */
    xworkToolOutputInit(&tOut);
    CHECK(
        pAgentTool &&
            test_entry_execute(pAgentTool, 
                pAgentTool->pUserData, &tCtx,
                "{\"name\":\"probe\",\"prompt\":\"probe the workspace again\",\"background\":true,"
                "\"notify\":\"probe done; fold the report in\"}",
                &tOut, &tError) == XWORK_RESULT_OK &&
            tOut.bSuccess && tOut.sContent && strstr(tOut.sContent, "task_id:"),
        "background delegation returns a task id");
    xworkToolOutputUnit(&tOut);
    {
        xwork_process_entry *pEntry = pAgent ? xwork__process_find(pAgent, 1u, NULL) : NULL;
        xwork_task_snapshot *pTasks = NULL;
        xwork_task_info tTaskInfo;
        size_t i;
        bool bDone = false;
        for (i = 0u; i < 2000u && pEntry && !bDone; ++i)
        {
            bDone = !xwork__task_running(pEntry);
            if (!bDone)
                mdo_test_sleep_ms(5u);
        }
        CHECK(pEntry && bDone, "background delegation finishes");
        pTasks =
            pAgent ? xworkRuntimeTaskSnapshot(pAgent->pRuntime, pAgent->uAgentId, &tError) : NULL;
        xworkTaskInfoInit(&tTaskInfo);
        CHECK(pTasks && xworkTaskSnapshotFind(pTasks, 1u, &tTaskInfo) &&
                  tTaskInfo.eKind == XWORK_TASK_AGENT && tTaskInfo.eState == XWORK_TASK_SUCCEEDED &&
                  tTaskInfo.iStartedAtUs != 0 && tTaskInfo.iEndedAtUs != 0,
              "background agent task publishes stable terminal metadata");
        xworkTaskSnapshotRelease(pTasks);
        CHECK(pAgent && xworkAgentTakeTaskNotices(pAgent, tNotices, 2u) == 1u &&
                  tNotices[0].eKind == XWORK_TASK_AGENT && tNotices[0].bExitedCleanly &&
                  tNotices[0].sNotify &&
                  strcmp(tNotices[0].sNotify, "probe done; fold the report in") == 0 &&
                  tNotices[0].sPreview && strstr(tNotices[0].sPreview, "probe complete"),
              "agent-task notice delivers the notify message and report preview");
        xwork__task_entry_release(pEntry);
    }

    /* Depth lock: from inside a delegation the agent tool refuses. We
     * simulate depth by running the tool through a child agent context —
     * simplest proof: a registered child copy would carry uAgentDepth=1;
     * here we verify the parent-level guard text via a manual depth set. */
    if (pAgent)
    {
        const xwork_tool_entry *pTool = xwork__find_tool(pAgent, "agent");
        pAgent->uAgentDepth = 1u;
        xworkToolOutputInit(&tOut);
        CHECK(pTool &&
                  test_entry_execute(pTool, pTool->pUserData, &tCtx,
                                   "{\"name\":\"probe\",\"prompt\":\"nested\"}", &tOut,
                                   &tError) == XWORK_RESULT_OK &&
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

static void test_subagent_definition_catalog(void)
{
    static const char sWorkspace[] = "tests/tmp_xwork_subagent_catalog";
    xllm_session_config tSessionConfig;
    xllm_session *pSession = NULL;
    xwork_agent_config tAgentConfig;
    xwork_agent *pAgent = NULL;
    xwork_subagent_definition_config arrDefinitions[2];
    xwork_subagent_definition_config tReplacement;
    xwork_subagent_definition_config tInvalid;
    xwork_subagent_definition_config tDefaults;
    xwork_subagent_catalog *pOld = NULL;
    xwork_subagent_catalog *pCurrent = NULL;
    xwork_subagent_catalog *pAfterFailure = NULL;
    xwork_subagent_definition_info tInfo;
    xwork_error tError;
    char sName[] = "reviewer";
    char sDescription[] = "Review repository evidence.";
    char sPrompt[] = "Inspect carefully and report evidence.";
    const char *arrTools[] = {"read", "grep"};
    const char *arrSkills[] = {"repo-map"};
    const char *arrMissing[] = {"tool.that.does.not.exist"};
    uint64_t uCurrentGeneration = 0u;

    (void)xrtDirRemoveAll(sWorkspace);
    CHECK(xrtDirCreateAll((str)sWorkspace), "subagent catalog workspace created");
    xllmSessionConfigInit(&tSessionConfig);
    pSession = xllmSessionCreate(&tSessionConfig, NULL);
    xworkAgentConfigInit(&tAgentConfig);
    tAgentConfig.pSession = pSession;
    tAgentConfig.sWorkspaceRoot = sWorkspace;
    tAgentConfig.uMaxSubagentDepth = 3u;
    tAgentConfig.uMaxConcurrentSubagents = 2u;
    pAgent = xworkAgentCreate(&tAgentConfig, &tError);
    CHECK(pSession && pAgent, "subagent catalog fixture creates");

    xworkSubagentDefinitionConfigInit(&tDefaults);
    CHECK(tDefaults.bReadOnly && tDefaults.uAllowedEffects == XWORK_TOOL_EFFECT_READ &&
              !tDefaults.bAllowBackground,
          "versioned subagent definitions default to a foreground read-only profile");

    xworkSubagentDefinitionConfigInit(&arrDefinitions[0]);
    arrDefinitions[0].sName = sName;
    arrDefinitions[0].sDescription = sDescription;
    arrDefinitions[0].sSystemPrompt = sPrompt;
    arrDefinitions[0].psTools = arrTools;
    arrDefinitions[0].iToolCount = 2u;
    arrDefinitions[0].psSkills = arrSkills;
    arrDefinitions[0].iSkillCount = 1u;
    arrDefinitions[0].uContextWindowTokens = 4096u;
    arrDefinitions[0].uMaxInputTokens = 3072u;
    arrDefinitions[0].uMaxOutputTokens = 256u;
    arrDefinitions[0].uMaxDepth = 2u;
    xworkSubagentDefinitionConfigInit(&arrDefinitions[1]);
    arrDefinitions[1].sName = "implementer";
    arrDefinitions[1].sDescription = "Apply a bounded implementation.";
    arrDefinitions[1].sSystemPrompt = "Implement only the assigned change.";
    arrDefinitions[1].bReadOnly = false;
    arrDefinitions[1].uAllowedEffects = XWORK_TOOL_EFFECT_READ | XWORK_TOOL_EFFECT_WORKSPACE_WRITE;
    CHECK(pAgent && xworkAgentReplaceSubagentDefinitions(pAgent, arrDefinitions, 2u, &tError),
          "complete subagent roster publishes atomically");
    pOld = pAgent ? xworkAgentSubagentCatalogSnapshot(pAgent) : NULL;
    xworkSubagentDefinitionInfoInit(&tInfo);
    CHECK(pOld && xworkSubagentCatalogCount(pOld) == 2u &&
              xworkSubagentCatalogDefinitionAt(pOld, 0u, &tInfo) && tInfo.sName &&
              strcmp(tInfo.sName, "reviewer") == 0 && tInfo.iToolCount == 2u &&
              strcmp(tInfo.psTools[1], "grep") == 0 && tInfo.iSkillCount == 1u &&
              strcmp(tInfo.psSkills[0], "repo-map") == 0 && tInfo.uContextWindowTokens == 4096u &&
              tInfo.uMaxInputTokens == 3072u,
          "catalog snapshot exposes copied tools, skills, and context budgets");

    sName[0] = 'X';
    sDescription[0] = 'X';
    sPrompt[0] = 'X';
    xworkSubagentDefinitionInfoInit(&tInfo);
    CHECK(pOld && xworkSubagentCatalogDefinitionAt(pOld, 0u, &tInfo) &&
              strcmp(tInfo.sName, "reviewer") == 0 &&
              strcmp(tInfo.sDescription, "Review repository evidence.") == 0 &&
              strcmp(tInfo.sSystemPrompt, "Inspect carefully and report evidence.") == 0,
          "published subagent generation owns every caller string");

    xworkSubagentDefinitionConfigInit(&tReplacement);
    tReplacement.sName = "planner";
    tReplacement.sDescription = "Produce a concise implementation plan.";
    tReplacement.sSystemPrompt = "Plan the assigned task.";
    CHECK(pAgent && xworkAgentReplaceSubagentDefinitions(pAgent, &tReplacement, 1u, &tError),
          "new complete roster replaces the previous generation");
    pCurrent = pAgent ? xworkAgentSubagentCatalogSnapshot(pAgent) : NULL;
    uCurrentGeneration = xworkSubagentCatalogGeneration(pCurrent);
    xworkSubagentDefinitionInfoInit(&tInfo);
    CHECK(pOld && pCurrent && xworkSubagentCatalogGeneration(pOld) != uCurrentGeneration &&
              xworkSubagentCatalogDefinitionAt(pOld, 0u, &tInfo) &&
              strcmp(tInfo.sName, "reviewer") == 0,
          "retained readers keep the old immutable roster generation");

    xworkSubagentDefinitionConfigInit(&tInvalid);
    tInvalid.sName = "invalid";
    tInvalid.sDescription = "References an unavailable tool.";
    tInvalid.sSystemPrompt = "Never runs.";
    tInvalid.psTools = arrMissing;
    tInvalid.iToolCount = 1u;
    CHECK(pAgent && !xworkAgentReplaceSubagentDefinitions(pAgent, &tInvalid, 1u, &tError) &&
              tError.eCode == XWORK_ERROR_INVALID_ARGUMENT,
          "invalid roster publication fails before changing the live generation");
    pAfterFailure = pAgent ? xworkAgentSubagentCatalogSnapshot(pAgent) : NULL;
    CHECK(pAfterFailure && xworkSubagentCatalogGeneration(pAfterFailure) == uCurrentGeneration &&
              xworkSubagentCatalogCount(pAfterFailure) == 1u,
          "failed roster replacement preserves the current generation");
    CHECK(pAgent && xworkAgentRunBegin(pAgent, &tError) &&
              !xworkAgentReplaceSubagentDefinitions(pAgent, &tReplacement, 1u, &tError) &&
              tError.eCode == XWORK_ERROR_CONTEXT,
          "active run pins both tool and subagent catalog generations");
    if (pAgent)
        xworkAgentRunEnd(pAgent);

    xworkSubagentCatalogRelease(pOld);
    xworkSubagentCatalogRelease(pCurrent);
    xworkSubagentCatalogRelease(pAfterFailure);
    xworkAgentDestroy(pAgent);
    xllmSessionDestroy(pSession);
    (void)xrtDirRemoveAll(sWorkspace);
}

static void test_configurable_subagent_policy(void)
{
    static const char sWorkspace[] = "tests/tmp_xwork_subagent_policy";
    xllm_session_config tSessionConfig;
    xllm_session *pSession = NULL;
    xwork_agent_config tAgentConfig;
    xwork_agent *pAgent = NULL;
    xwork_subagent_definition_config arrDefinitions[2];
    xwork_error tError;
    xllm_error tLlmError;
    configurable_subagent_model tModel;
    configurable_subagent_events tEvents;
    const xwork_tool_entry *pAgentTool;
    xwork_tool_context tCtx;
    xwork_tool_output tOut;
    const char *arrBudgetTools[] = {"read", "write", "agent"};
    const char *arrNestedTools[] = {"read", "agent"};
    const char *arrNoTools[1] = {NULL};
    const char *sFinal;
    uint64_t uTaskId = 0u;
    uint64_t uWaited = 0u;
    uint64_t uGuard;
    xwork_task_snapshot *pTasks = NULL;
    xwork_task_info tTaskInfo;

    (void)xrtDirRemoveAll(sWorkspace);
    CHECK(xrtDirCreateAll((str)sWorkspace), "configurable subagent workspace created");
    memset(&tModel, 0, sizeof(tModel));
    memset(&tEvents, 0, sizeof(tEvents));
    tEvents.bParentRunPreserved = true;
    tEvents.bDelegationIdsPresent = true;
    xllmSessionConfigInit(&tSessionConfig);
    tSessionConfig.uMaxOutputTokens = 512u;
    pSession = xllmSessionCreateForTest(&tSessionConfig, configurable_subagent_complete, &tModel,
                                        &tLlmError);
    xworkAgentConfigInit(&tAgentConfig);
    tAgentConfig.pSession = pSession;
    tAgentConfig.sWorkspaceRoot = sWorkspace;
    tAgentConfig.uMaxSubagentDepth = 2u;
    tAgentConfig.uMaxConcurrentSubagents = 3u;
    tAgentConfig.OnEvent = configurable_subagent_event;
    tAgentConfig.pEventUserData = &tEvents;
    pAgent = xworkAgentCreate(&tAgentConfig, &tError);
    CHECK(pSession && pAgent, "configurable subagent fixture creates");

    xworkSubagentDefinitionConfigInit(&arrDefinitions[0]);
    arrDefinitions[0].sName = "budgeted";
    arrDefinitions[0].sDescription = "Exercise model and permission ceilings.";
    arrDefinitions[0].sSystemPrompt = "Return a bounded report.";
    arrDefinitions[0].psTools = arrBudgetTools;
    arrDefinitions[0].iToolCount = 3u;
    arrDefinitions[0].sModel = "ling-tiny-test";
    arrDefinitions[0].sReasoningEffort = "high";
    arrDefinitions[0].uMaxTurns = 2u;
    arrDefinitions[0].uTimeoutMs = 5000u;
    arrDefinitions[0].uContextWindowTokens = 32768u;
    arrDefinitions[0].uMaxInputTokens = 20000u;
    arrDefinitions[0].uMaxOutputTokens = 77u;
    arrDefinitions[0].iMaxFinalBytes = 64u;
    arrDefinitions[0].bReadOnly = false;
    arrDefinitions[0].uAllowedEffects = XWORK_TOOL_EFFECT_READ;
    arrDefinitions[0].bAllowDelegation = true;
    arrDefinitions[0].bAllowBackground = false;
    CHECK(pAgent && xworkAgentReplaceSubagentDefinitions(pAgent, arrDefinitions, 1u, &tError),
          "budgeted subagent definition publishes");
    pAgentTool = pAgent ? xwork__find_tool(pAgent, "agent") : NULL;
    memset(&tCtx, 0, sizeof(tCtx));
    tCtx.pAgent = pAgent;
    tCtx.sWorkspaceRoot = sWorkspace;
    tCtx.uAgentTurn = 11u;
    tCtx.uRunId = 77u;
    tCtx.uDeadline = XLLM_DEADLINE_NEVER;
    xworkToolOutputInit(&tOut);
    CHECK(pAgentTool &&
              test_entry_execute(pAgentTool, 
                  pAgentTool->pUserData, &tCtx,
                  "{\"name\":\"budgeted\",\"prompt\":\"inspect within every budget\"}", &tOut,
                  &tError) == XWORK_RESULT_OK &&
              tOut.bSuccess,
          "foreground configurable subagent completes");
    sFinal = tOut.sContent ? strstr(tOut.sContent, "--- final report ---\n") : NULL;
    if (sFinal)
        sFinal += strlen("--- final report ---\n");
    CHECK(tModel.uCalls == 1u && tModel.bSawModel && tModel.bSawReasoning &&
              tModel.bSawOutputBudget && tModel.bSawFiniteDeadline && tEvents.bSawContextBudget,
          "model, reasoning, context, input, output, and time budgets reach the child");
    CHECK(tModel.bSawRead && !tModel.bSawWrite && !tModel.bSawAgent,
          "effect ceiling removes write and delegation tools from the child");
    CHECK(sFinal && strlen(sFinal) <= 64u && strstr(sFinal, "truncated") != NULL,
          "final report obeys its hard retained-byte budget");
    xworkToolOutputUnit(&tOut);
    xworkToolOutputInit(&tOut);
    CHECK(pAgentTool &&
              test_entry_execute(pAgentTool, 
                  pAgentTool->pUserData, &tCtx,
                  "{\"name\":\"budgeted\",\"prompt\":\"background forbidden\",\"background\":true}",
                  &tOut, &tError) == XWORK_RESULT_OK &&
              !tOut.bSuccess && tOut.sContent && strstr(tOut.sContent, "does not allow background"),
          "definition-level background ceiling rejects the request before execution");
    xworkToolOutputUnit(&tOut);

    memset(&tModel, 0, sizeof(tModel));
    memset(&tEvents, 0, sizeof(tEvents));
    tEvents.bParentRunPreserved = true;
    tEvents.bDelegationIdsPresent = true;
    CHECK(pSession && xllmSessionSetTestCall(pSession, nested_subagent_complete, &tModel),
          "nested delegation model driver installs");
    xworkSubagentDefinitionConfigInit(&arrDefinitions[0]);
    arrDefinitions[0].sName = "nested";
    arrDefinitions[0].sDescription = "Delegate one bounded child level.";
    arrDefinitions[0].sSystemPrompt = "Delegate once, then summarize.";
    arrDefinitions[0].psTools = arrNestedTools;
    arrDefinitions[0].iToolCount = 2u;
    arrDefinitions[0].bReadOnly = false;
    arrDefinitions[0].uAllowedEffects = XWORK_TOOL_EFFECT_READ | XWORK_TOOL_EFFECT_AGENT_DELEGATION;
    arrDefinitions[0].uMaxDepth = 2u;
    arrDefinitions[0].bAllowDelegation = true;
    CHECK(pAgent && xworkAgentReplaceSubagentDefinitions(pAgent, arrDefinitions, 1u, &tError),
          "nested subagent definition publishes");
    pAgentTool = pAgent ? xwork__find_tool(pAgent, "agent") : NULL;
    xworkToolOutputInit(&tOut);
    CHECK(pAgentTool &&
              test_entry_execute(pAgentTool, pAgentTool->pUserData, &tCtx,
                                    "{\"name\":\"nested\",\"prompt\":\"start nested work\"}", &tOut,
                                    &tError) == XWORK_RESULT_OK &&
              tOut.bSuccess && tOut.sContent && strstr(tOut.sContent, "child summary only") &&
              !strstr(tOut.sContent, "grandchild private report"),
          "nested child returns only its final summary to the parent");
    CHECK(tEvents.uStarts == 2u &&
              (tEvents.uDepthMask & ((1u << 1) | (1u << 2))) == ((1u << 1) | (1u << 2)) &&
              tEvents.bParentRunPreserved && tEvents.bDelegationIdsPresent,
          "child events preserve parent run, delegation id, and depth lineage");
    CHECK(!tModel.bSawAgent, "maximum depth removes the agent tool from the terminal child");
    xworkToolOutputUnit(&tOut);

    xworkAgentDestroy(pAgent);
    pAgent = NULL;
    xllmSessionDestroy(pSession);
    pSession = NULL;
    memset(&tModel, 0, sizeof(tModel));
    tModel.bHoldCancelled = true;
    pSession =
        xllmSessionCreateForTest(&tSessionConfig, blocking_subagent_complete, &tModel, &tLlmError);
    xworkAgentConfigInit(&tAgentConfig);
    tAgentConfig.pSession = pSession;
    tAgentConfig.sWorkspaceRoot = sWorkspace;
    tAgentConfig.uMaxSubagentDepth = 1u;
    tAgentConfig.uMaxConcurrentSubagents = 1u;
    pAgent = xworkAgentCreate(&tAgentConfig, &tError);
    CHECK(pSession && pAgent, "one-slot background delegation fixture creates");
    xworkSubagentDefinitionConfigInit(&arrDefinitions[0]);
    arrDefinitions[0].sName = "hold";
    arrDefinitions[0].sDescription = "Wait until cooperatively cancelled.";
    arrDefinitions[0].sSystemPrompt = "Wait for cancellation.";
    arrDefinitions[0].psTools = arrNoTools;
    arrDefinitions[0].iToolCount = 0u;
    arrDefinitions[0].bReadOnly = false;
    arrDefinitions[0].uAllowedEffects = 0u;
    arrDefinitions[0].bAllowBackground = true;
    xworkSubagentDefinitionConfigInit(&arrDefinitions[1]);
    arrDefinitions[1].sName = "foreground-only";
    arrDefinitions[1].sDescription = "Reject background execution.";
    arrDefinitions[1].sSystemPrompt = "Return immediately.";
    arrDefinitions[1].psTools = arrNoTools;
    arrDefinitions[1].iToolCount = 0u;
    CHECK(pAgent && xworkAgentReplaceSubagentDefinitions(pAgent, arrDefinitions, 2u, &tError),
          "background policy roster publishes");
    pAgentTool = pAgent ? xwork__find_tool(pAgent, "agent") : NULL;
    tCtx.pAgent = pAgent;
    xworkToolOutputInit(&tOut);
    CHECK(pAgentTool &&
              test_entry_execute(pAgentTool, pAgentTool->pUserData, &tCtx,
                                    "{\"name\":\"hold\",\"prompt\":\"hold\",\"background\":true}",
                                    &tOut, &tError) == XWORK_RESULT_OK &&
              tOut.bSuccess && tOut.sContent && strstr(tOut.sContent, "task_id:") &&
              (uTaskId = strtoull(strstr(tOut.sContent, "task_id:") + strlen("task_id:"), NULL,
                                  10)) != 0u,
          "background subagent enters the unified task runtime");
    xworkToolOutputUnit(&tOut);
    uGuard = xllmDeadlineAfterUs(UINT64_C(2000000));
    while (xwork__atomic_load(&tModel.iEntered) == 0 && !xllmDeadlineExpired(uGuard))
        mdo_test_sleep_ms(1u);
    xworkToolOutputInit(&tOut);
    CHECK(xwork__atomic_load(&tModel.iEntered) != 0 && pAgentTool &&
              test_entry_execute(pAgentTool, pAgentTool->pUserData, &tCtx,
                                    "{\"name\":\"hold\",\"prompt\":\"second\",\"background\":true}",
                                    &tOut, &tError) == XWORK_RESULT_OK &&
              !tOut.bSuccess && tOut.sContent && strstr(tOut.sContent, "concurrency ceiling"),
          "tree-wide concurrency ceiling rejects a second active delegation");
    xworkToolOutputUnit(&tOut);
    pTasks = pAgent ? xworkRuntimeTaskSnapshot(pAgent->pRuntime, 0u, &tError) : NULL;
    CHECK(pTasks && !xworkTaskSnapshotStopRequested(pTasks, uTaskId),
          "original child task snapshot has no stop request");
    CHECK(pAgent && uTaskId && xworkRuntimeCancelTask(pAgent->pRuntime, uTaskId, &tError),
          "background child stop request is accepted");
    CHECK(pTasks && !xworkTaskSnapshotStopRequested(pTasks, uTaskId),
          "older immutable task snapshot does not acquire a later stop request");
    xworkTaskSnapshotRelease(pTasks);
    pTasks = pAgent ? xworkRuntimeTaskSnapshot(pAgent->pRuntime, 0u, &tError) : NULL;
    CHECK(pTasks && xworkTaskSnapshotFind(pTasks, uTaskId, &tTaskInfo) &&
              tTaskInfo.eState == XWORK_TASK_RUNNING &&
              xworkTaskSnapshotStopRequested(pTasks, uTaskId),
          "new child snapshot captures stopping before the child finishes");
    CHECK(pAgent && xworkRuntimeCancelTask(pAgent->pRuntime, uTaskId, &tError),
          "repeated child cancellation is idempotent while it is stopping");
    {
        xwork_task_snapshot *pRepeat = xworkRuntimeTaskSnapshot(pAgent->pRuntime, 0u, &tError);
        xwork_task_info tRepeat;
        xworkTaskInfoInit(&tRepeat);
        CHECK(pRepeat && xworkTaskSnapshotFind(pRepeat, uTaskId, &tRepeat) &&
                  tRepeat.uRevision == tTaskInfo.uRevision,
              "repeated cancellation does not create another task state event");
        xworkTaskSnapshotRelease(pRepeat);
    }
    xworkTaskSnapshotRelease(pTasks);
    pTasks = NULL;
    xwork__atomic_store(&tModel.iRelease, 1);
    CHECK(pAgent && uTaskId &&
              xworkRuntimeWaitTasks(pAgent->pRuntime, &uTaskId, 1u, XWORK_TASK_WAIT_ALL,
                                    xllmDeadlineAfterUs(UINT64_C(3000000)), NULL, &uWaited,
                                    &tError) == XWORK_RESULT_OK,
          "background task cancellation reaches the child operation scope");
    pTasks = pAgent ? xworkRuntimeTaskSnapshot(pAgent->pRuntime, pAgent->uAgentId, &tError) : NULL;
    xworkTaskInfoInit(&tTaskInfo);
    CHECK(xwork__atomic_load(&tModel.iCancelled) != 0 && pTasks &&
              xworkTaskSnapshotFind(pTasks, uTaskId, &tTaskInfo) &&
              tTaskInfo.eState == XWORK_TASK_CANCELLED,
          "cancelled background child publishes a stable terminal task state");
    xworkTaskSnapshotRelease(pTasks);
    if (pAgent && uTaskId)
    {
        (void)xworkRuntimeReleaseTask(pAgent->pRuntime, uTaskId, &tError);
    }

    xworkAgentDestroy(pAgent);
    xllmSessionDestroy(pSession);
    (void)xrtDirRemoveAll(sWorkspace);
}

static xllm_result t1_script_call(void *pUserData, const xllm_request *pRequest,
                                  const xllm_stream_callbacks *pCallbacks,
                                  xllm_response **ppResponse, xllm_error *pError)
{
    t1_script *pScript = (t1_script *)pUserData;
    (void)pCallbacks;
    if (pError)
    {
        xllmErrorInit(pError);
    }
    ++pScript->iCalls;
    if (pRequest->iToolCount == 12u)
    {
        pScript->bSawTools = true;
    }
    *ppResponse = (pScript->iCalls == 1u && !pScript->bFinalOnly) ? t1_response_write()
                                                                  : t1_response_text("t1 finished");
    return *ppResponse ? XLLM_RESULT_OK : XLLM_RESULT_ERROR;
}

static bool durable_stop_guard(xllm_session *pSession, uint32_t uRound,
                               const xllm_response *pResponse, size_t iPendingToolCalls,
                               void *pUserData)
{
    (void)pSession;
    (void)uRound;
    (void)pResponse;
    (void)iPendingToolCalls;
    (void)pUserData;
    return false; /* simulate a crash right after the tool round */
}

static void test_session_run_with_tools_chain(void)
{
    static const char sWorkspace[] = "tests/tmp_xwork_t1";
    xllm_session_config tSessionConfig;
    xllm_session *pSession = NULL;
    xwork_agent_config tAgentConfig;
    xwork_agent *pAgent = NULL;
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
    CHECK(pSession && xllmSessionRunWithTools(pSession, "create t1.txt", &tExecutor, NULL, NULL,
                                              &tSummary, &tLlmError) == XLLM_RESULT_OK,
          "t1 bounded run completes without the xwork loop");
    CHECK(tScript.iCalls == 2u && tScript.bSawTools && tSummary.uRounds == 2u &&
              tSummary.uToolCalls == 1u && tSummary.sFinalText &&
              strcmp(tSummary.sFinalText, "t1 finished") == 0,
          "t1 summary: two rounds, one executed tool, final text");
    CHECK(xrtFileExists((str) "tests/tmp_xwork_t1/t1.txt"), "t1 write_file side effect landed");
    CHECK(xllmSessionPendingToolCallCount(pSession) == 0u, "t1 chain leaves no pending calls");
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
    xllm_session *pSession = NULL;
    xwork_agent_config tAgentConfig;
    xwork_agent *pAgent = NULL;
    xwork_error tError;
    xllm_error tLlmError;
    xllm_executor tExecutor;
    xllm_run_policy tPolicy;
    xllm_run_summary tSummary;
    t1_script tScript;
    xwork_tool_definition tDynamic;
    xwork_run_result tResult;
    xwork_recovery_snapshot *pRecoverySnapshot = NULL;
    xwork_recovery_call_info tRecoveryCall;
    xwork_recovery_decision tDecision;
    xwork_resume_options tResumeOptions;
    test_events tEvents;

    (void)xrtDirRemoveAll(sWorkspace);
    CHECK(xrtDirCreateAll((str)sWorkspace), "durable workspace created");
    memset(&tEvents, 0, sizeof(tEvents));

    /* Run window: registry mutation and re-entry are rejected while open. */
    memset(&tScript, 0, sizeof(tScript));
    xllmSessionConfigInit(&tSessionConfig);
    pSession = xllmSessionCreateForTest(&tSessionConfig, t1_script_call, &tScript, &tLlmError);
    xworkAgentConfigInit(&tAgentConfig);
    tAgentConfig.pSession = pSession;
    tAgentConfig.sWorkspaceRoot = sWorkspace;
    tAgentConfig.bRequireVerificationAfterWrite = false;
    tAgentConfig.OnEvent = on_event;
    tAgentConfig.pEventUserData = &tEvents;
    pAgent = xworkAgentCreate(&tAgentConfig, &tError);
    memset(&tExecutor, 0, sizeof(tExecutor));
    CHECK(pSession && pAgent && xworkExecutorBind(&tExecutor, pAgent, &tError),
          "durable fixture assembles");
    memset(&tDynamic, 0, sizeof(tDynamic));
    tDynamic.sName = "window_probe";
    tDynamic.sDescription = "mutation probe";
    tDynamic.sParametersJson = "{\"type\":\"object\"}";
    tDynamic.uEffects = XWORK_TOOL_EFFECT_READ_ONLY;
    tDynamic.OnExecute = registry_probe_execute;
    CHECK(pAgent && xworkAgentRunBegin(pAgent, &tError) &&
              !xworkAgentRegisterTool(pAgent, &tDynamic, &tError) &&
              tError.eCode == XWORK_ERROR_CONTEXT && !xworkAgentRunBegin(pAgent, &tError),
          "run window rejects registry mutation and re-entry");
    xworkAgentRunEnd(pAgent);
    CHECK(pAgent && xworkAgentRegisterTool(pAgent, &tDynamic, &tError),
          "registry mutation works after the window closes");
    memset(&tResult, 0, sizeof(tResult));
    CHECK(pSession && xllmSessionSetTestCall(pSession, NULL, NULL) && pAgent &&
              xworkAgentRun(pAgent, "no boundary", &tResult, &tError) == XWORK_RESULT_ERROR &&
              tError.eCode == XWORK_ERROR_INVALID_ARGUMENT,
          "built-in loop refuses an executor-only agent without a boundary");
    CHECK(pSession && xllmSessionSetTestCall(pSession, t1_script_call, &tScript),
          "session model driver reattaches after the boundary check");
    xworkAgentUnregisterTool(pAgent, "window_probe", &tError);

    /* Durable chain: journal on -> run stopped mid-batch (pending write) ->
     * Save -> destroy (crash) -> Recover -> inspect -> record uncertainty ->
     * continue without executing the ambiguous write. */
    CHECK(pSession && xllmSessionEnableJournal(pSession, sJournal, &tLlmError),
          "journal attaches to the fixture session");
    xllmRunPolicyInit(&tPolicy);
    tPolicy.pOnRound = durable_stop_guard; /* stop after the tool round */
    memset(&tSummary, 0, sizeof(tSummary));
    CHECK(pSession &&
              xllmSessionRunWithTools(pSession, "durable task", &tExecutor, NULL, &tPolicy,
                                      &tSummary, &tLlmError) == XLLM_RESULT_OK &&
              tSummary.bStoppedByPolicy,
          "durable run stops with a pending tool call");
    CHECK(pSession && xllmSessionPendingToolCallCount(pSession) == 1u,
          "crash fixture leaves exactly one pending call");
    CHECK(pSession && xllmSessionSave(pSession, sSnapshot, &tLlmError),
          "snapshot saves before the simulated crash");
    xllmRunSummaryUnit(&tSummary);
    xworkExecutorUnbind(&tExecutor);
    xworkAgentDestroy(pAgent);
    pAgent = NULL;
    xllmSessionDestroy(pSession);
    pSession = NULL; /* crash: everything above is gone */

    memset(&tScript, 0, sizeof(tScript));
    tScript.bFinalOnly = true; /* the resumed model round answers directly */
    pSession = xllmSessionRecover(sSnapshot, sJournal, NULL, &tLlmError);
    CHECK(pSession != NULL, "session recovers from snapshot plus journal");
    CHECK(pSession && xllmSessionSetTestCall(pSession, t1_script_call, &tScript),
          "recovered session binds a fresh model driver");
    xworkAgentConfigInit(&tAgentConfig);
    tAgentConfig.pSession = pSession;
    tAgentConfig.sWorkspaceRoot = sWorkspace;
    tAgentConfig.bRequireVerificationAfterWrite = false;
    tAgentConfig.OnEvent = on_event;
    tAgentConfig.pEventUserData = &tEvents;
    pAgent = xworkAgentCreate(&tAgentConfig, &tError);
    CHECK(pSession && pAgent, "post-crash fixture reassembles");
    pRecoverySnapshot = xworkAgentRecoverySnapshot(pAgent, &tError);
    xworkRecoveryCallInfoInit(&tRecoveryCall);
    CHECK(pRecoverySnapshot && xworkRecoverySnapshotCount(pRecoverySnapshot) == 1u &&
              xworkRecoverySnapshotAt(pRecoverySnapshot, 0u, &tRecoveryCall) &&
              strcmp(tRecoveryCall.sToolCallId, "t1-call-1") == 0 &&
              !tRecoveryCall.bAutomaticRetrySafe &&
              (tRecoveryCall.uEffects & XWORK_TOOL_EFFECT_WORKSPACE_WRITE) != 0u,
          "post-crash snapshot exposes the unresolved write boundary");
    memset(&tResult, 0, sizeof(tResult));
    CHECK(xworkAgentResume(pAgent, &tResult, &tError) == XWORK_RESULT_ERROR &&
              tError.eCode == XWORK_ERROR_RECOVERY_REQUIRED &&
              !xrtFileExists((str) "tests/tmp_xwork_durable/t1.txt") &&
              xllmSessionPendingToolCallCount(pSession) == 1u,
          "default recovery does not repeat an ambiguous workspace write");
    xworkRecoveryDecisionInit(&tDecision);
    tDecision.sToolCallId = "t1-call-1";
    tDecision.eAction = XWORK_RECOVERY_RECORD_UNCERTAIN;
    xworkResumeOptionsInit(&tResumeOptions);
    tResumeOptions.pDecisions = &tDecision;
    tResumeOptions.iDecisionCount = 1u;
    xworkRunResultUnit(&tResult);
    CHECK(xworkAgentResumeWithOptions(pAgent, &tResumeOptions, &tResult, &tError) ==
                  XWORK_RESULT_OK &&
              tResult.sFinalText && strcmp(tResult.sFinalText, "t1 finished") == 0 &&
              tResult.uToolCalls == 0u,
          "record-uncertain recovery continues without invoking the pending write");
    CHECK(!xrtFileExists((str) "tests/tmp_xwork_durable/t1.txt"),
          "record-uncertain leaves external state untouched");
    CHECK(pSession && xllmSessionPendingToolCallCount(pSession) == 0u,
          "uncertainty result durably resolves the pending call");
    xworkRecoverySnapshotRelease(pRecoverySnapshot);
    pRecoverySnapshot = NULL;
    xworkRunResultUnit(&tResult);

    {
        xllm_tool_call arrMixedCalls[2];
        xllm_response tMixedResponse;
        xwork_run_config tRecoveryRunConfig;
        xwork_run *pRecoveryRun = NULL;
        char sDecisionId[32] = "mixed_write";
        uint64_t uMixedTurn;
        uint32_t uStartsBefore;
        memset(arrMixedCalls, 0, sizeof(arrMixedCalls));
        memset(&tMixedResponse, 0, sizeof(tMixedResponse));
        CHECK(xrtFileWriteAtomic(
                  "tests/tmp_xwork_durable/input.txt",
                  (xbytesview){(const uint8 *)"recovery input", strlen("recovery input")}),
              "mixed recovery fixture creates readable input");
        arrMixedCalls[0].sId = "mixed_read";
        arrMixedCalls[0].sName = "read";
        arrMixedCalls[0].sArgumentsJson = "{\"path\":\"input.txt\"}";
        arrMixedCalls[1].sId = "mixed_write";
        arrMixedCalls[1].sName = "write";
        arrMixedCalls[1].sArgumentsJson =
            "{\"path\":\"must_not_exist.txt\",\"content\":\"unsafe\"}";
        tMixedResponse.pToolCalls = arrMixedCalls;
        tMixedResponse.iToolCallCount = 2u;
        uMixedTurn = xllmSessionBeginTurn(pSession);
        CHECK(uMixedTurn &&
                  xllmSessionAddText(pSession, uMixedTurn, XLLM_ROLE_USER, "recover a mixed batch",
                                     0u) &&
                  xllmSessionAddAssistantResponse(pSession, uMixedTurn, &tMixedResponse),
              "mixed recovery fixture records read and write calls");
        uStartsBefore = tEvents.uToolStarts;
        xworkRecoveryDecisionInit(&tDecision);
        tDecision.sToolCallId = "stale_call";
        tDecision.eAction = XWORK_RECOVERY_RETRY;
        xworkResumeOptionsInit(&tResumeOptions);
        tResumeOptions.pDecisions = &tDecision;
        tResumeOptions.iDecisionCount = 1u;
        memset(&tResult, 0, sizeof(tResult));
        CHECK(xworkAgentResumeWithOptions(pAgent, &tResumeOptions, &tResult, &tError) ==
                      XWORK_RESULT_ERROR &&
                  tError.eCode == XWORK_ERROR_INVALID_ARGUMENT &&
                  tEvents.uToolStarts == uStartsBefore &&
                  xllmSessionPendingToolCallCount(pSession) == 2u,
              "stale recovery decisions fail before any pending call executes");
        xworkRunResultUnit(&tResult);
        memset(&tResult, 0, sizeof(tResult));
        CHECK(xworkAgentResume(pAgent, &tResult, &tError) == XWORK_RESULT_ERROR &&
                  tError.eCode == XWORK_ERROR_RECOVERY_REQUIRED &&
                  tEvents.uToolStarts == uStartsBefore &&
                  xllmSessionPendingToolCallCount(pSession) == 2u,
              "mixed recovery preflights all calls before retrying the safe prefix");
        xworkRecoveryDecisionInit(&tDecision);
        tDecision.sToolCallId = sDecisionId;
        tDecision.eAction = XWORK_RECOVERY_RECORD_UNCERTAIN;
        xworkResumeOptionsInit(&tResumeOptions);
        tResumeOptions.pDecisions = &tDecision;
        tResumeOptions.iDecisionCount = 1u;
        xworkRunConfigInit(&tRecoveryRunConfig);
        tRecoveryRunConfig.bResume = true;
        tRecoveryRunConfig.pResumeOptions = &tResumeOptions;
        pRecoveryRun = xworkRunCreate(pAgent, &tRecoveryRunConfig, &tError);
        snprintf(sDecisionId, sizeof(sDecisionId), "%s", "caller_changed");
        xworkRunResultUnit(&tResult);
        CHECK(
            pRecoveryRun && xworkRunStart(pRecoveryRun, &tError) &&
                xworkRunWait(pRecoveryRun, XLLM_DEADLINE_NEVER, &tResult, &tError) ==
                    XWORK_RESULT_OK &&
                tResult.uToolCalls == 1u && tEvents.uToolStarts == uStartsBefore + 1u,
            "async recovery owns decisions, retries the read, and records the write as uncertain");
        CHECK(!xrtFileExists((str) "tests/tmp_xwork_durable/must_not_exist.txt") &&
                  xllmSessionPendingToolCallCount(pSession) == 0u &&
                  tEvents.uRecoveryAutomatic == 1u && tEvents.uRecoveryUncertain == 2u,
              "mixed recovery exposes automatic and uncertain audit decisions");
        xworkRunDestroy(pRecoveryRun);
        xworkRunResultUnit(&tResult);
    }
    xworkAgentDestroy(pAgent);
    xllmSessionDestroy(pSession);
    (void)xrtDirRemoveAll(sWorkspace);
}

static void test_agent_dependency_owner_lifetime(void)
{
    static const char sWorkspace[] = "tests/tmp_xwork_agent_owner";
    xwork_runtime_config tRuntimeConfig;
    xwork_agent_definition_config tDefinitionConfig;
    xwork_agent_options tOptions;
    xwork_run_config tRunConfig;
    xwork_run_result tResult;
    xllm_session_config tSessionConfig;
    xwork_runtime *pRuntime = NULL;
    xwork_agent_definition *pDefinition = NULL;
    xllm_session *pSession = NULL;
    xwork_agent *pAgent = NULL;
    xwork_run *pRun = NULL;
    xwork_error tError;
    agent_owner_fixture tOwner = {0u, 0u, 0u, 0u, true};

    (void)xrtDirRemoveAll(sWorkspace);
    CHECK(xrtDirCreateAll((str)sWorkspace), "agent owner workspace created");
    xworkRuntimeConfigInit(&tRuntimeConfig);
    pRuntime = xworkRuntimeCreate(&tRuntimeConfig, &tError);
    xworkAgentDefinitionConfigInit(&tDefinitionConfig);
    tDefinitionConfig.sId = "test.owner-pinned";
    tDefinitionConfig.bRegisterBuiltinTools = false;
    tDefinitionConfig.bAutoSaveSession = false;
    tDefinitionConfig.bRequireVerificationAfterWrite = false;
    pDefinition = xworkAgentDefinitionCreate(&tDefinitionConfig, &tError);
    xllmSessionConfigInit(&tSessionConfig);
    pSession = xllmSessionCreate(&tSessionConfig, NULL);
    xworkAgentOptionsInit(&tOptions);
    tOptions.pSession = pSession;
    tOptions.sWorkspaceRoot = sWorkspace;
    tOptions.OnModelComplete = agent_owner_complete;
    tOptions.pModelUserData = &tOwner;
    tOptions.pOwnerUserData = &tOwner;
    tOptions.OnOwnerRetain = agent_owner_retain;
    tOptions.OnOwnerRelease = agent_owner_release;
    pAgent = xworkAgentCreateWithRuntime(pRuntime, pDefinition, &tOptions, &tError);
    CHECK(pAgent && tOwner.uRefs == 1u && tOwner.uRetainAttempts == 1u && tOwner.uReleases == 0u,
          "agent creation retains its aggregate borrowed dependency owner");
    xworkRunConfigInit(&tRunConfig);
    tRunConfig.sPrompt = "complete after host agent destruction";
    pRun = pAgent ? xworkRunCreate(pAgent, &tRunConfig, &tError) : NULL;
    CHECK(pRun && xworkRunStart(pRun, &tError),
          "owner-pinned run starts before host agent release");
    xworkAgentDestroy(pAgent);
    pAgent = NULL;
    CHECK(pRun && tOwner.uRefs == 1u && tOwner.uReleases == 0u,
          "active run pins the agent dependency owner after host release");
    memset(&tResult, 0, sizeof(tResult));
    CHECK(pRun &&
              xworkRunWait(pRun, xllmDeadlineAfterUs(UINT64_C(5000000)), &tResult, &tError) ==
                  XWORK_RESULT_OK &&
              tOwner.uModelCalls == 1u && tOwner.uRefs == 1u,
          "retained owner remains callable until the run is destroyed");
    xworkRunResultUnit(&tResult);
    xworkRunDestroy(pRun);
    pRun = NULL;
    CHECK(tOwner.uRefs == 0u && tOwner.uReleases == 1u,
          "final agent destruction releases the dependency owner exactly once");

    xworkAgentOptionsInit(&tOptions);
    tOptions.pSession = pSession;
    tOptions.sWorkspaceRoot = sWorkspace;
    tOptions.OnModelComplete = agent_owner_complete;
    tOptions.pModelUserData = &tOwner;
    tOptions.pOwnerUserData = &tOwner;
    tOptions.OnOwnerRetain = agent_owner_retain;
    pAgent = xworkAgentCreateWithRuntime(pRuntime, pDefinition, &tOptions, &tError);
    CHECK(!pAgent && tError.eCode == XWORK_ERROR_INVALID_ARGUMENT && tOwner.uRetainAttempts == 1u &&
              tOwner.uReleases == 1u,
          "unpaired dependency owner callbacks fail before retaining");

    tOwner.bRetainable = false;
    tOptions.OnOwnerRelease = agent_owner_release;
    pAgent = xworkAgentCreateWithRuntime(pRuntime, pDefinition, &tOptions, &tError);
    CHECK(!pAgent && tError.eCode == XWORK_ERROR_CONTEXT && tOwner.uRetainAttempts == 2u &&
              tOwner.uRefs == 0u && tOwner.uReleases == 1u,
          "failed dependency owner retain leaves ownership unchanged");

    xllmSessionDestroy(pSession);
    xworkAgentDefinitionRelease(pDefinition);
    xworkRuntimeRelease(pRuntime);
    (void)xrtDirRemoveAll(sWorkspace);
}

static void test_v3_runtime_lifetimes(void)
{
    static const char sWorkspace[] = "tests/tmp_xwork_v3_runtime";
    xwork_runtime_config tRuntimeConfig;
    xwork_agent_definition_config tDefinitionConfig;
    xwork_agent_options tAgentOptions;
    xwork_run_config tRunConfig;
    xwork_run_info tRunInfo;
    xwork_runtime *pRuntime = NULL;
    xwork_agent_definition *pDefinition = NULL;
    xwork_agent *pAgentA = NULL;
    xwork_agent *pAgentB = NULL;
    xwork_agent *pCancelAgent = NULL;
    xwork_run *pRun = NULL;
    xllm_session_config tSessionConfig;
    xllm_session *pSessionA = NULL;
    xllm_session *pSessionB = NULL;
    xllm_session *pCancelSession = NULL;
    xwork_error tError;
    xwork_run_result tResult;
    xwork_tool_context tContext;
    xwork_tool_output tOutput;
    xwork_task_output tTaskOutput;
    xwork_task_snapshot *pTaskSnapshot = NULL;
    xwork_task_info tTaskInfo;
    xwork_task_notice tForeignNotice;
    const xwork_tool_entry *pSpawn;
    const xwork_tool_entry *pPoll;
    uint32_t uModelCalls = 0u;
    v3_cancel_model tCancelModel;
    uint64_t uTaskId = 0u;
    char sPollArgs[160];

    (void)xrtDirRemoveAll(sWorkspace);
    CHECK(xrtDirCreateAll((str)sWorkspace), "v3 runtime workspace created");
    xworkRuntimeConfigInit(&tRuntimeConfig);
    tRuntimeConfig.uMaxTasks = 8u;
    pRuntime = xworkRuntimeCreate(&tRuntimeConfig, &tError);
    xworkAgentDefinitionConfigInit(&tDefinitionConfig);
    tDefinitionConfig.sId = "test.v3";
    tDefinitionConfig.bAutoSaveSession = false;
    tDefinitionConfig.bRequireVerificationAfterWrite = false;
    pDefinition = xworkAgentDefinitionCreate(&tDefinitionConfig, &tError);
    xllmSessionConfigInit(&tSessionConfig);
    pSessionA = xllmSessionCreate(&tSessionConfig, NULL);
    pSessionB = xllmSessionCreate(&tSessionConfig, NULL);
    pCancelSession = xllmSessionCreate(&tSessionConfig, NULL);
    xworkAgentOptionsInit(&tAgentOptions);
    tAgentOptions.pSession = pSessionA;
    tAgentOptions.sWorkspaceRoot = sWorkspace;
    tAgentOptions.OnModelComplete = v3_final_complete;
    tAgentOptions.pModelUserData = &uModelCalls;
    pAgentA = xworkAgentCreateWithRuntime(pRuntime, pDefinition, &tAgentOptions, &tError);
    CHECK(pRuntime && pDefinition && pSessionA && pSessionB && pCancelSession && pAgentA,
          "v3 runtime, immutable definition, and session agent create");

    memset(&tResult, 0, sizeof(tResult));
    xworkRunConfigInit(&tRunConfig);
    tRunConfig.sPrompt = "complete this bounded v3 request";
    pRun = pAgentA ? xworkRunCreate(pAgentA, &tRunConfig, &tError) : NULL;
    CHECK(pRun && xworkRunStart(pRun, &tError), "v3 run starts asynchronously");
    CHECK(pRun &&
              xworkRunWait(pRun, xllmDeadlineAfterUs(UINT64_C(5000000)), &tResult, &tError) ==
                  XWORK_RESULT_OK &&
              tResult.sFinalText && strcmp(tResult.sFinalText, "v3 run completed") == 0 &&
              uModelCalls == 1u,
          "v3 run wait returns an owned result through the request boundary");
    xworkRunInfoInit(&tRunInfo);
    CHECK(pRun && xworkRunGetInfo(pRun, &tRunInfo) && tRunInfo.eState == XWORK_RUN_SUCCEEDED &&
              tRunInfo.uRunId != 0u && tRunInfo.uStartedUs >= tRunInfo.uCreatedUs &&
              tRunInfo.uEndedUs >= tRunInfo.uStartedUs,
          "v3 run exposes stable terminal state and timestamps");
    xworkRunResultUnit(&tResult);
    xworkRunDestroy(pRun);
    pRun = NULL;

    memset(&tCancelModel, 0, sizeof(tCancelModel));
    tAgentOptions.pSession = pCancelSession;
    tAgentOptions.OnModelComplete = v3_cancel_complete;
    tAgentOptions.pModelUserData = &tCancelModel;
    pCancelAgent = xworkAgentCreateWithRuntime(pRuntime, pDefinition, &tAgentOptions, &tError);
    xworkRunConfigInit(&tRunConfig);
    tRunConfig.sPrompt = "wait for explicit cancellation";
    pRun = pCancelAgent ? xworkRunCreate(pCancelAgent, &tRunConfig, &tError) : NULL;
    CHECK(pRun && xworkRunStart(pRun, &tError), "v3 cancellation fixture starts asynchronously");
    memset(&tResult, 0, sizeof(tResult));
    CHECK(pRun &&
              xworkRunWait(pRun, xllmDeadlineAfterUs(UINT64_C(20000)), &tResult, &tError) ==
                  XWORK_RESULT_TIMEOUT &&
              tError.eCode == XWORK_ERROR_TIMEOUT,
          "run wait deadline leaves the active run untouched");
    xworkRunResultUnit(&tResult);
    CHECK(pRun && xworkRunCancel(pRun) && xworkRunCancel(pRun),
          "run cancellation is safe and idempotent");
    memset(&tResult, 0, sizeof(tResult));
    CHECK(pRun &&
              xworkRunWait(pRun, xllmDeadlineAfterUs(UINT64_C(5000000)), &tResult, &tError) ==
                  XWORK_RESULT_CANCELLED &&
              tError.eCode == XWORK_ERROR_CANCELLED &&
              xwork__atomic_load(&tCancelModel.iEntered) == 1,
          "cancelled run publishes a stable terminal result");
    xworkRunResultUnit(&tResult);
    xworkRunDestroy(pRun);
    pRun = NULL;
    xworkAgentDestroy(pCancelAgent);
    pCancelAgent = NULL;

    tAgentOptions.pSession = pSessionA;
    tAgentOptions.OnModelComplete = v3_final_complete;
    tAgentOptions.pModelUserData = &uModelCalls;

    /* A runtime-owned process remains addressable after replacing the
     * session agent that launched it. */
    pSpawn = pAgentA ? xwork__find_tool(pAgentA, "spawn") : NULL;
    memset(&tContext, 0, sizeof(tContext));
    tContext.pAgent = pAgentA;
    tContext.sWorkspaceRoot = sWorkspace;
    tContext.uRunId = 313u;
    xworkToolOutputInit(&tOutput);
#if defined(_WIN32)
    CHECK(pSpawn &&
              test_entry_execute(pSpawn, 
                  pSpawn->pUserData, &tContext,
                  "{\"argv\":[\"cmd\",\"/c\",\"ping -n 2 127.0.0.1 >nul && echo runtime-ok\"]}",
                  &tOutput, &tError) == XWORK_RESULT_OK &&
              tOutput.bSuccess && tOutput.sContent &&
              test_task_id(tOutput.sContent, &uTaskId) == 1,
          "runtime-owned process starts from the first session agent");
#else
    CHECK(pSpawn &&
              test_entry_execute(pSpawn, pSpawn->pUserData, &tContext,
                                "{\"argv\":[\"sh\",\"-c\",\"sleep 0.1; echo runtime-ok\"]}",
                                &tOutput, &tError) == XWORK_RESULT_OK &&
              tOutput.bSuccess && tOutput.sContent &&
              test_task_id(tOutput.sContent, &uTaskId) == 1,
          "runtime-owned process starts from the first session agent");
#endif
    xworkToolOutputUnit(&tOutput);
    xworkAgentDestroy(pAgentA);
    pAgentA = NULL;
    tAgentOptions.pSession = pSessionB;
    pAgentB = xworkAgentCreateWithRuntime(pRuntime, pDefinition, &tAgentOptions, &tError);
    CHECK(pAgentB && xworkRuntimeTaskCount(pRuntime) == 1u,
          "background process survives first agent destruction");
    CHECK(test_wait_task(pAgentB, uTaskId, &tError), "shared runtime process reaches a terminal state");
    pPoll = pAgentB ? xwork__find_tool(pAgentB, "status") : NULL;
    memset(&tContext, 0, sizeof(tContext));
    tContext.pAgent = pAgentB;
    tContext.sWorkspaceRoot = sWorkspace;
    xworkToolOutputInit(&tOutput);
    (void)snprintf(sPollArgs, sizeof(sPollArgs),
                   "{\"task_id\":%" PRIu64 ",\"wait_ms\":5000,\"release\":false}", uTaskId);
    CHECK(uTaskId && pPoll &&
              test_entry_execute(pPoll, pPoll->pUserData, &tContext, sPollArgs, &tOutput, &tError) ==
                  XWORK_RESULT_OK &&
              tOutput.bSuccess && tOutput.sContent && test_task_stdout(pAgentB, uTaskId, "runtime-ok", &tError),
          "replacement agent polls the same runtime task to completion");
    xworkToolOutputUnit(&tOutput);
    xworkTaskOutputInit(&tTaskOutput);
    pTaskSnapshot = xworkRuntimeTaskSnapshot(pRuntime, 0u, &tError);
    xworkTaskInfoInit(&tTaskInfo);
    CHECK(pTaskSnapshot && xworkTaskSnapshotFind(pTaskSnapshot, uTaskId, &tTaskInfo) &&
              tTaskInfo.eKind == XWORK_TASK_PROCESS && tTaskInfo.eState == XWORK_TASK_SUCCEEDED &&
              tTaskInfo.uOwnerRunId == 313u && tTaskInfo.bExitStatusValid &&
              tTaskInfo.iExitCode == 0,
          "process task publishes terminal state, run ownership, and exit metadata");
    CHECK(xworkRuntimeReadTaskOutput(pRuntime, uTaskId, 0u, 0u, 0u, 4096u, &tTaskOutput, &tError) &&
              tTaskOutput.bComplete && tTaskOutput.pStdout && tTaskOutput.iStdoutSize >= 10u &&
              memcmp(tTaskOutput.pStdout, "runtime-ok", 10u) == 0,
          "public output cursor remains independent after model-tool polling");
    CHECK(xworkAgentTakeTaskNotices(pAgentB, &tForeignNotice, 1u) == 0u,
          "replacement agent cannot consume another agent's completion notice");
    xworkTaskOutputUnit(&tTaskOutput);
    xworkTaskSnapshotRelease(pTaskSnapshot);
    CHECK(xworkRuntimeReleaseTask(pRuntime, uTaskId, &tError) &&
              xworkRuntimeTaskCount(pRuntime) == 0u,
          "public release removes the completed process task");

    xworkAgentDestroy(pAgentB);
    xllmSessionDestroy(pSessionA);
    xllmSessionDestroy(pSessionB);
    xllmSessionDestroy(pCancelSession);
    xworkAgentDefinitionRelease(pDefinition);
    xworkRuntimeRelease(pRuntime);
    (void)xrtDirRemoveAll(sWorkspace);
}

static void test_runtime_observability(void)
{
    static const char sWorkspace[] = "tests/tmp_xwork_observability";
    static const unsigned char sPayload[] = "artifact-payload";
    static const unsigned char sTampered[] = "tampered-artifact-payload";
    static const char sHex[] = "0123456789abcdef";
    xwork_runtime_config tConfig;
    xwork_runtime *pEventRuntime = NULL;
    xwork_runtime *pArtifactRuntime = NULL;
    xwork_event tEvent;
    xwork_event_snapshot *pPage = NULL;
    xwork_event_snapshot *pNextPage = NULL;
    xwork_event_snapshot *pInvalidPage = NULL;
    xwork_agent_definition_config tDefinitionConfig;
    xwork_agent_definition *pDefinition = NULL;
    xwork_agent_options tAgentOptions;
    xwork_agent *pAgent = NULL;
    xllm_session_config tSessionConfig;
    xllm_session *pSession = NULL;
    xwork_artifact_info tInfo;
    xwork_artifact_chunk tChunk;
    xwork_error tError;
    char sLongText[96];
    char sLongMetadata[4200];
    char sExpectedSha[65];
    uint8_t arrDigest[32];
    uint64_t uArtifactId = 0u;
    uint64_t uRejectedId = 99u;
    char chRejectedSentinel = '\0';
    char *sRelative = NULL;
    char *sRejectedPath = &chRejectedSentinel;
    char *sAbsolute = NULL;
    uint32_t uModelCalls = 0u;
    size_t i;

    memset(sLongText, 'T', sizeof(sLongText) - 1u);
    sLongText[sizeof(sLongText) - 1u] = '\0';
    memset(sLongMetadata, 'M', sizeof(sLongMetadata) - 1u);
    sLongMetadata[sizeof(sLongMetadata) - 1u] = '\0';
    xworkRuntimeConfigInit(&tConfig);
    tConfig.iMaxRetainedEventBytes = 1u;
    tConfig.iMaxEventTextBytes = 1u;
    pEventRuntime = xworkRuntimeCreate(&tConfig, &tError);
    CHECK(!pEventRuntime && tError.eCode == XWORK_ERROR_INVALID_ARGUMENT,
          "runtime rejects an event budget that cannot retain one record");
    xworkRuntimeConfigInit(&tConfig);
    tConfig.uMaxEvents = 3u;
    tConfig.iMaxRetainedEventBytes = 64u * 1024u;
    tConfig.iMaxEventTextBytes = 48u;
    pEventRuntime = xworkRuntimeCreate(&tConfig, &tError);
    CHECK(pEventRuntime != NULL, "bounded runtime event ledger creates");
    for (i = 0u; pEventRuntime && i < 5u; ++i)
    {
        memset(&tEvent, 0, sizeof(tEvent));
        tEvent.eKind = XWORK_EVENT_ERROR;
        tEvent.sText = sLongText;
        tEvent.iTextLength = sizeof(sLongText) - 1u;
        tEvent.sToolName = sLongMetadata;
        tEvent.uRunId = 700u + (uint64_t)i;
        xwork__runtime_record_event(pEventRuntime, &tEvent);
    }
    pPage = pEventRuntime ? xworkRuntimeEventSnapshot(pEventRuntime, 0u, 2u, &tError) : NULL;
    memset(&tEvent, 0, sizeof(tEvent));
    CHECK(pPage && xworkEventSnapshotCount(pPage) == 2u && xworkEventSnapshotHistoryLost(pPage) &&
              xworkEventSnapshotNextCursor(pPage) == 4u &&
              xworkEventSnapshotLatestId(pPage) == 5u &&
              xworkEventSnapshotDroppedCount(pPage) == 2u &&
              xworkEventSnapshotAt(pPage, 0u, &tEvent) &&
              tEvent.uSchemaVersion == XWORK_EVENT_SCHEMA_VERSION && tEvent.uEventId == 3u &&
              tEvent.iOccurredAtUs > 0 && tEvent.uRunId == 702u && tEvent.bTextTruncated &&
              tEvent.bMetadataTruncated && tEvent.iOriginalTextLength == sizeof(sLongText) - 1u &&
              tEvent.iTextLength == 48u && tEvent.sText && tEvent.sToolName &&
              strlen(tEvent.sToolName) == 4096u,
          "event replay reports eviction, copied metadata, and bounded text");
    pNextPage = pEventRuntime ? xworkRuntimeEventSnapshot(pEventRuntime, 4u, 2u, &tError) : NULL;
    memset(&tEvent, 0, sizeof(tEvent));
    CHECK(pNextPage && xworkEventSnapshotCount(pNextPage) == 1u &&
              !xworkEventSnapshotHistoryLost(pNextPage) &&
              xworkEventSnapshotNextCursor(pNextPage) == 5u &&
              xworkEventSnapshotAt(pNextPage, 0u, &tEvent) && tEvent.uEventId == 5u &&
              tEvent.uRunId == 704u,
          "event replay resumes from an exact cursor without a false gap");
    pInvalidPage = pEventRuntime ? xworkRuntimeEventSnapshot(pEventRuntime, 6u, 1u, &tError) : NULL;
    CHECK(!pInvalidPage && tError.eCode == XWORK_ERROR_INVALID_ARGUMENT,
          "event replay rejects a cursor ahead of the runtime");
    xworkEventSnapshotRelease(pInvalidPage);
    pInvalidPage = xworkEventSnapshotRef(pPage);
    CHECK(pInvalidPage == pPage, "event snapshots expose explicit retained ownership");
    xworkEventSnapshotRelease(pPage);
    pPage = pInvalidPage;
    pInvalidPage = NULL;
    xworkRuntimeRelease(pEventRuntime);
    pEventRuntime = NULL;
    memset(&tEvent, 0, sizeof(tEvent));
    CHECK(pPage && xworkEventSnapshotAt(pPage, 0u, &tEvent) && tEvent.sText &&
              tEvent.sText[0] == 'T' && tEvent.sToolName && tEvent.sToolName[0] == 'M',
          "immutable event snapshots outlive their runtime");
    xworkEventSnapshotRelease(pNextPage);
    xworkEventSnapshotRelease(pPage);
    pNextPage = NULL;
    pPage = NULL;

    (void)xrtDirRemoveAll(sWorkspace);
    CHECK(xrtDirCreateAll((str)sWorkspace), "artifact registry workspace creates");
    xworkRuntimeConfigInit(&tConfig);
    tConfig.uMaxArtifacts = 1u;
    tConfig.uMaxEvents = 8u;
    pArtifactRuntime = xworkRuntimeCreate(&tConfig, &tError);
    xworkAgentDefinitionConfigInit(&tDefinitionConfig);
    tDefinitionConfig.sId = "test.observability";
    tDefinitionConfig.bRegisterBuiltinTools = false;
    tDefinitionConfig.bAutoSaveSession = false;
    pDefinition = xworkAgentDefinitionCreate(&tDefinitionConfig, &tError);
    xllmSessionConfigInit(&tSessionConfig);
    pSession = xllmSessionCreate(&tSessionConfig, NULL);
    xworkAgentOptionsInit(&tAgentOptions);
    tAgentOptions.pSession = pSession;
    tAgentOptions.sWorkspaceRoot = sWorkspace;
    tAgentOptions.OnModelComplete = v3_final_complete;
    tAgentOptions.pModelUserData = &uModelCalls;
    pAgent =
        pArtifactRuntime && pDefinition && pSession
            ? xworkAgentCreateWithRuntime(pArtifactRuntime, pDefinition, &tAgentOptions, &tError)
            : NULL;
    CHECK(pArtifactRuntime && pDefinition && pSession && pAgent,
          "artifact registry fixture creates");
    CHECK(pAgent &&
              xwork__runtime_write_artifact(pAgent, "observability", sPayload,
                                            sizeof(sPayload) - 1u, "application/octet-stream",
                                            &uArtifactId, &sRelative, &tError) &&
              uArtifactId == 1u && sRelative && sRelative[0],
          "runtime publishes an identified artifact atomically");
    CHECK(pAgent &&
              !xwork__runtime_write_artifact(pAgent, "rejected", sPayload, sizeof(sPayload) - 1u,
                                             "application/octet-stream", &uRejectedId,
                                             &sRejectedPath, &tError) &&
              tError.eCode == XWORK_ERROR_LIMIT && uRejectedId == 0u && sRejectedPath == NULL,
          "artifact lifetime cap fails before creating another record");
    xworkAgentDestroy(pAgent);
    pAgent = NULL;
    xllmSessionDestroy(pSession);
    pSession = NULL;
    xworkAgentDefinitionRelease(pDefinition);
    pDefinition = NULL;

    xworkArtifactInfoInit(&tInfo);
    CHECK(pArtifactRuntime && xworkRuntimeArtifactCount(pArtifactRuntime) == 1u &&
              xworkRuntimeArtifactAt(pArtifactRuntime, 0u, &tInfo) &&
              tInfo.uArtifactId == uArtifactId && tInfo.eKind == XWORK_ARTIFACT_TOOL_OUTPUT &&
              tInfo.uAgentId != 0u && tInfo.iCreatedAtUs > 0 && tInfo.sSource &&
              strcmp(tInfo.sSource, "observability") == 0 && tInfo.sPath &&
              strcmp(tInfo.sPath, sRelative) == 0 && tInfo.sMediaType &&
              strcmp(tInfo.sMediaType, "application/octet-stream") == 0 &&
              tInfo.uSizeBytes == sizeof(sPayload) - 1u,
          "artifact metadata remains queryable after agent destruction");
    xworkArtifactInfoInit(&tInfo);
    CHECK(pArtifactRuntime && xworkRuntimeArtifactGetInfo(pArtifactRuntime, uArtifactId, &tInfo) &&
              tInfo.sPath && strcmp(tInfo.sPath, sRelative) == 0 &&
              !xworkRuntimeArtifactGetInfo(pArtifactRuntime, uArtifactId + 1u, &tInfo),
          "artifact lookup is stable by id and rejects unknown ids");
    CHECK(xrtSha256(sPayload, sizeof(sPayload) - 1u, arrDigest),
          "artifact checksum fixture hashes");
    for (i = 0u; i < sizeof(arrDigest); ++i)
    {
        sExpectedSha[i * 2u] = sHex[arrDigest[i] >> 4];
        sExpectedSha[i * 2u + 1u] = sHex[arrDigest[i] & 0x0fu];
    }
    sExpectedSha[64] = '\0';
    CHECK(strcmp(tInfo.sSha256, sExpectedSha) == 0,
          "artifact metadata exposes the full SHA-256 digest");

    xworkArtifactChunkInit(&tChunk);
    CHECK(pArtifactRuntime &&
              xworkRuntimeReadArtifact(pArtifactRuntime, uArtifactId, 0u, 4u, &tChunk, &tError) &&
              tChunk.iSize == 4u && tChunk.uOffset == 0u &&
              tChunk.uTotalSize == sizeof(sPayload) - 1u && !tChunk.bEof &&
              memcmp(tChunk.pData, sPayload, 4u) == 0,
          "artifact reader returns a bounded first chunk");
    CHECK(pArtifactRuntime &&
              xworkRuntimeReadArtifact(pArtifactRuntime, uArtifactId, 4u, 3u, &tChunk, &tError) &&
              tChunk.iSize == 3u && !tChunk.bEof && memcmp(tChunk.pData, sPayload + 4u, 3u) == 0,
          "artifact reader safely reuses and replaces a chunk buffer");
    CHECK(pArtifactRuntime &&
              xworkRuntimeReadArtifact(pArtifactRuntime, uArtifactId, 7u, 0u, &tChunk, &tError) &&
              tChunk.iSize == sizeof(sPayload) - 1u - 7u && tChunk.bEof &&
              memcmp(tChunk.pData, sPayload + 7u, tChunk.iSize) == 0,
          "artifact reader reaches EOF with the default bounded page");
    CHECK(pArtifactRuntime &&
              !xworkRuntimeReadArtifact(pArtifactRuntime, uArtifactId, 0u,
                                        XWORK_ARTIFACT_READ_MAX_BYTES + 1u, &tChunk, &tError) &&
              tError.eCode == XWORK_ERROR_INVALID_ARGUMENT,
          "artifact reader rejects an oversized page request");
    CHECK(pArtifactRuntime &&
              xworkRuntimeReadArtifact(pArtifactRuntime, uArtifactId, sizeof(sPayload) - 1u, 1u,
                                       &tChunk, &tError) &&
              !tChunk.pData && tChunk.iSize == 0u && tChunk.bEof,
          "artifact reader accepts the exact EOF cursor");
    xworkArtifactChunkUnit(&tChunk);

    pPage = pArtifactRuntime ? xworkRuntimeEventSnapshot(pArtifactRuntime, 0u, 8u, &tError) : NULL;
    memset(&tEvent, 0, sizeof(tEvent));
    CHECK(
        pPage && xworkEventSnapshotCount(pPage) == 1u && xworkEventSnapshotAt(pPage, 0u, &tEvent) &&
            tEvent.eKind == XWORK_EVENT_ARTIFACT_CREATED && tEvent.uArtifactId == uArtifactId &&
            tEvent.bSuccess && tEvent.sArtifactPath && strcmp(tEvent.sArtifactPath, sRelative) == 0,
        "artifact creation joins the runtime audit stream");
    xworkEventSnapshotRelease(pPage);
    pPage = NULL;

    sAbsolute = tInfo.sPath ? xrtPathJoin(sWorkspace, tInfo.sPath) : NULL;
    CHECK(sAbsolute &&
              xrtFileWriteAtomic(sAbsolute, (xbytesview){sTampered, sizeof(sTampered) - 1u}),
          "artifact tamper fixture changes the stored file size");
    xworkArtifactChunkInit(&tChunk);
    CHECK(pArtifactRuntime &&
              !xworkRuntimeReadArtifact(pArtifactRuntime, uArtifactId, 0u, 4u, &tChunk, &tError) &&
              tError.eCode == XWORK_ERROR_IO && !tChunk.pData,
          "artifact reader detects a missing or size-changed file");
    xworkArtifactChunkUnit(&tChunk);
    if (sAbsolute)
        xrtFree(sAbsolute);
    free(sRelative);
    xworkRuntimeRelease(pArtifactRuntime);
    (void)xrtDirRemoveAll(sWorkspace);
}

static bool test_artifact_bytes(const char *sWorkspace, const char *sRelative,
                                const char *sExpected)
{
    char *sPath = sRelative ? xrtPathJoin(sWorkspace, sRelative) : NULL;
    size_t iSize = 0u;
    bytes pData = sPath ? xrtFileReadAllLimit(sPath, 1024u, &iSize) : NULL;
    bool bEqual = pData && iSize == strlen(sExpected) && memcmp(pData, sExpected, iSize) == 0;
    if (pData)
        xrtFree(pData);
    if (sPath)
        xrtFree(sPath);
    return bEqual;
}

static bool test_artifact_no_temporary(const char *sWorkspace)
{
    char *sPath = xrtPathJoin(sWorkspace, "artifacts/run-00000000000000000001");
    xdir pDirectory = sPath ? xrtDirOpen(sPath, 0u) : NULL;
    xdirentry tEntry;
    xdirnext eNext = XDIR_NEXT_ERROR;
    bool bClean = pDirectory != NULL;
    while (pDirectory && (eNext = xrtDirNext(pDirectory, &tEntry)) == XDIR_NEXT_ITEM)
    {
        if (tEntry.Name.Size >= 7u && memcmp(tEntry.Name.Data, ".xwork-", 7u) == 0)
            bClean = false;
    }
    if (eNext != XDIR_NEXT_END)
        bClean = false;
    if (pDirectory)
        (void)xrtDirClose(pDirectory);
    if (sPath)
        xrtFree(sPath);
    return bClean;
}

static void test_artifact_publication(void)
{
    static const char sWorkspace[] = "tests/tmp_xwork_artifact_publish";
    xwork_runtime_config tConfig;
    xwork_agent_definition_config tDefinitionConfig;
    xwork_agent_definition *pDefinition = NULL;
    xwork_agent_options tOptions;
    xllm_session_config tSessionConfig;
    xwork_runtime *arrRuntime[2] = {NULL, NULL};
    xllm_session *arrSession[2] = {NULL, NULL};
    xwork_agent *arrAgent[2] = {NULL, NULL};
    char *arrPath[3] = {NULL, NULL, NULL};
    char *sPath = NULL;
    uint64_t uId = 0u;
    uint32_t uModelCalls = 0u;
    xwork_error tError;
    xwork_artifact_info tInfo;
    xwork_event_snapshot *pEvents = NULL;
    xwork_event tEvent;
    xroot pRoot = NULL;
    xfileinfo tFileInfo;
    char sBlocked[160];
    uint64_t uNext;
    size_t i;
    (void)xrtDirRemoveAll(sWorkspace);
    CHECK(xrtDirCreateAll(sWorkspace), "artifact publication workspace creates");
    xworkRuntimeConfigInit(&tConfig);
    tConfig.uMaxArtifacts = 8u;
    xworkAgentDefinitionConfigInit(&tDefinitionConfig);
    tDefinitionConfig.sId = "test.artifact-publication";
    tDefinitionConfig.bRegisterBuiltinTools = false;
    tDefinitionConfig.bAutoSaveSession = false;
    pDefinition = xworkAgentDefinitionCreate(&tDefinitionConfig, &tError);
    xllmSessionConfigInit(&tSessionConfig);
    xworkAgentOptionsInit(&tOptions);
    tOptions.sWorkspaceRoot = sWorkspace;
    tOptions.sArtifactDirectory = "artifacts";
    tOptions.OnModelComplete = v3_final_complete;
    tOptions.pModelUserData = &uModelCalls;
    for (i = 0u; i < 2u; ++i)
    {
        arrRuntime[i] = xworkRuntimeCreate(&tConfig, &tError);
        arrSession[i] = xllmSessionCreate(&tSessionConfig, NULL);
        tOptions.pSession = arrSession[i];
        arrAgent[i] =
            arrRuntime[i] && arrSession[i] && pDefinition
                ? xworkAgentCreateWithRuntime(arrRuntime[i], pDefinition, &tOptions, &tError)
                : NULL;
        if (arrAgent[i])
            arrAgent[i]->uRunSequence = 1u;
    }
    CHECK(arrAgent[0] && arrAgent[1], "two independent artifact runtimes create");
    if (!arrAgent[0] || !arrAgent[1])
        goto unit;
    CHECK(xwork__runtime_write_artifact(arrAgent[0], "source", "first", 5u, NULL, &uId, &arrPath[0],
                                        &tError) &&
              uId == 1u,
          "first publisher records the original artifact");
    CHECK(xwork__runtime_write_artifact(arrAgent[1], "source", "second", 6u, NULL, &uId,
                                        &arrPath[1], &tError) &&
              uId == 2u && arrPath[0] && arrPath[1] && strcmp(arrPath[0], arrPath[1]) != 0,
          "independent runtime skips a preexisting artifact name");
    xworkAgentDestroy(arrAgent[0]);
    arrAgent[0] = NULL;
    xworkRuntimeRelease(arrRuntime[0]);
    arrRuntime[0] = xworkRuntimeCreate(&tConfig, &tError);
    tOptions.pSession = arrSession[0];
    arrAgent[0] = arrRuntime[0]
                      ? xworkAgentCreateWithRuntime(arrRuntime[0], pDefinition, &tOptions, &tError)
                      : NULL;
    if (arrAgent[0])
        arrAgent[0]->uRunSequence = 1u;
    CHECK(arrAgent[0] &&
              xwork__runtime_write_artifact(arrAgent[0], "source", "restart", 7u, NULL, &uId,
                                            &arrPath[2], &tError) &&
              uId == 3u && test_artifact_bytes(sWorkspace, arrPath[0], "first") &&
              test_artifact_bytes(sWorkspace, arrPath[1], "second") &&
              test_artifact_bytes(sWorkspace, arrPath[2], "restart"),
          "runtime restart preserves both historical artifact byte streams");
    if (!arrAgent[0])
        goto unit;
    g_bArtifactPublishRace = true;
    CHECK(xwork__runtime_write_artifact(arrAgent[1], "race", "new", 3u, NULL, &uId, &sPath,
                                        &tError) &&
              uId == 4u &&
              test_artifact_bytes(
                  sWorkspace, "artifacts/run-00000000000000000001/00000000000000000003-race.txt",
                  "other-publisher") &&
              test_artifact_bytes(sWorkspace, sPath, "new"),
          "destination appearing at the rename boundary is never replaced");
    free(sPath);
    sPath = NULL;
    xworkArtifactInfoInit(&tInfo);
    CHECK(xworkRuntimeArtifactGetInfo(arrRuntime[1], uId, &tInfo) && tInfo.uArtifactId == 4u &&
              tInfo.uSizeBytes == 3u,
          "collision retry publishes the actual id to artifact metadata");
    pEvents = xworkRuntimeEventSnapshot(arrRuntime[1], 0u, 8u, &tError);
    memset(&tEvent, 0, sizeof(tEvent));
    CHECK(pEvents && xworkEventSnapshotCount(pEvents) == 2u &&
              xworkEventSnapshotAt(pEvents, 1u, &tEvent) && tEvent.uArtifactId == 4u,
          "collision attempts emit no duplicate artifact creation events");
    xworkEventSnapshotRelease(pEvents);
    pEvents = NULL;
    g_iArtifactPublishCalls = 0u;
    g_eArtifactPublishFailure = XERR_EXISTS;
    CHECK(!xwork__runtime_write_artifact(arrAgent[1], "collision-limit", "x", 1u, NULL, &uId,
                                         &sPath, &tError) &&
              tError.eCode == XWORK_ERROR_LIMIT &&
              g_iArtifactPublishCalls == XWORK_ARTIFACT_PUBLISH_ATTEMPTS && !uId && !sPath &&
              arrRuntime[1]->iArtifactReservations == 0u &&
              xworkRuntimeArtifactCount(arrRuntime[1]) == 2u &&
              test_artifact_no_temporary(sWorkspace),
          "collision limit releases only its temporary file and quota");
    g_iArtifactPublishCalls = 0u;
    g_eArtifactPublishFailure = XERR_IO;
    CHECK(!xwork__runtime_write_artifact(arrAgent[1], "io-failure", "x", 1u, NULL, &uId, &sPath,
                                         &tError) &&
              tError.eCode == XWORK_ERROR_IO && g_iArtifactPublishCalls == 1u && !uId && !sPath &&
              arrRuntime[1]->iArtifactReservations == 0u &&
              xworkRuntimeArtifactCount(arrRuntime[1]) == 2u &&
              test_artifact_no_temporary(sWorkspace),
          "noncollision rename errors stop immediately and release ownership");
    arrRuntime[1]->uNextArtifactId = UINT64_MAX;
    g_iArtifactPublishCalls = 0u;
    g_eArtifactPublishFailure = XERR_EXISTS;
    CHECK(!xwork__runtime_write_artifact(arrAgent[1], "exhausted", "x", 1u, NULL, &uId, &sPath,
                                         &tError) &&
              tError.eCode == XWORK_ERROR_LIMIT && g_iArtifactPublishCalls == 1u && !uId &&
              !sPath && arrRuntime[1]->uNextArtifactId == 0u &&
              arrRuntime[1]->iArtifactReservations == 0u && test_artifact_no_temporary(sWorkspace),
          "artifact id exhaustion cannot wrap to an existing id");
    g_eArtifactPublishFailure = XERR_NONE;
    CHECK(xwork__runtime_write_artifact(arrAgent[0], "empty", NULL, 0u, NULL, &uId, &sPath,
                                        &tError) &&
              test_artifact_bytes(sWorkspace, sPath, ""),
          "an empty artifact still publishes as a complete regular file");
    free(sPath);
    sPath = NULL;
    pRoot = xrtRootOpen(sWorkspace);
    uNext = arrRuntime[0]->uNextArtifactId;
    snprintf(sBlocked, sizeof(sBlocked), "artifacts/run-00000000000000000001/%020llu-directory.txt",
             (unsigned long long)uNext);
    CHECK(pRoot && xrtRootDirCreate(pRoot, sBlocked, 0700u) &&
              xwork__runtime_write_artifact(arrAgent[0], "directory", "dir", 3u, NULL, &uId, &sPath,
                                            &tError) &&
              uId == uNext + 1u && xrtRootStat(pRoot, sBlocked, false, &tFileInfo) &&
              tFileInfo.Type == XFILE_TYPE_DIRECTORY &&
              test_artifact_bytes(sWorkspace, sPath, "dir"),
          "an existing destination directory is retained and skipped");
    free(sPath);
    sPath = NULL;
    if (pRoot && xrtRootLinkCreate(pRoot, "artifacts", "linked", true))
    {
        char *sOriginalDirectory = arrAgent[0]->sArtifactDirectory;
        arrAgent[0]->sArtifactDirectory = test_strdup("linked");
        CHECK(!xwork__runtime_write_artifact(arrAgent[0], "parent-link", "x", 1u, NULL, &uId,
                                             &sPath, &tError) &&
                  tError.eCode == XWORK_ERROR_IO && !uId && !sPath &&
                  arrRuntime[0]->iArtifactReservations == 0u &&
                  test_artifact_no_temporary(sWorkspace),
              "existing link parents are rejected before temporary creation");
        free(arrAgent[0]->sArtifactDirectory);
        arrAgent[0]->sArtifactDirectory = sOriginalDirectory;
    }
    else
    {
        printf("  artifact parent-link fixture SKIP (platform link privilege unavailable)\n");
    }
    if (pRoot)
        (void)xrtRootClose(pRoot);
    pRoot = NULL;
    CHECK(test_artifact_bytes(sWorkspace, arrPath[0], "first") &&
              test_artifact_bytes(sWorkspace, arrPath[1], "second") &&
              test_artifact_bytes(sWorkspace, arrPath[2], "restart") &&
              test_artifact_no_temporary(sWorkspace) && uModelCalls == 0u,
          "all failure paths retain old bytes without model execution");
unit:
    g_eArtifactPublishFailure = XERR_NONE;
    g_bArtifactPublishRace = false;
    xworkEventSnapshotRelease(pEvents);
    if (pRoot)
        (void)xrtRootClose(pRoot);
    free(sPath);
    for (i = 0u; i < 3u; ++i)
        free(arrPath[i]);
    for (i = 0u; i < 2u; ++i)
    {
        xworkAgentDestroy(arrAgent[i]);
        xllmSessionDestroy(arrSession[i]);
        xworkRuntimeRelease(arrRuntime[i]);
    }
    xworkAgentDefinitionRelease(pDefinition);
    (void)xrtDirRemoveAll(sWorkspace);
}

static void test_artifact_store(void)
{
    static const char sBase[] = "tests/tmp_xwork_artifact_store";
    static const char sWorkspace[] = "tests/tmp_xwork_artifact_store/workspace";
    static const char sStorage[] = "tests/tmp_xwork_artifact_store/storage";
    static const char sMoved[] = "tests/tmp_xwork_artifact_store/moved";
    xroot pRoot = NULL;
    xroot pBase = NULL;
    xwork_artifact_store *pStore = NULL;
    xwork_runtime_config tConfig;
    xwork_runtime *pRuntime = NULL;
    xwork_agent_definition_config tDefinitionConfig;
    xwork_agent_definition *pDefinition = NULL;
    xwork_agent_options tOptions;
    xllm_session_config tSessionConfig;
    xllm_session *pSession = NULL;
    xwork_agent *pAgent = NULL;
    xwork_artifact_chunk tChunk;
    xwork_error tError;
    char *sPath = NULL;
    char *sDenied = NULL;
    char *sSpoof = NULL;
    uint64_t uId = 0u;
    uint32_t uModelCalls = 0u;
    (void)xrtDirRemoveAll(sBase);
    xworkArtifactChunkInit(&tChunk);
    CHECK(!xworkArtifactStoreCreate(NULL, &tError) && tError.eCode == XWORK_ERROR_INVALID_ARGUMENT,
          "artifact store rejects a missing anchor");
    CHECK(xrtDirCreateAll(sWorkspace) && xrtDirCreateAll(sStorage),
          "artifact store creates separate workspace and storage fixtures");
    pRoot = xrtRootOpen(sStorage);
    pStore = xworkArtifactStoreCreate(pRoot, &tError);
    if (pRoot)
        (void)xrtRootClose(pRoot);
    pRoot = NULL;
    CHECK(pStore && xworkArtifactStoreRef(pStore) == pStore && pStore->iRefs == 2,
          "artifact store clones its anchor and exposes retained ownership");
    if (pStore)
        xworkArtifactStoreRelease(pStore);
    xworkRuntimeConfigInit(&tConfig);
    pRuntime = xworkRuntimeCreate(&tConfig, &tError);
    xworkAgentDefinitionConfigInit(&tDefinitionConfig);
    tDefinitionConfig.sId = "test.artifact-store";
    tDefinitionConfig.bRegisterBuiltinTools = false;
    tDefinitionConfig.bAutoSaveSession = false;
    pDefinition = xworkAgentDefinitionCreate(&tDefinitionConfig, &tError);
    xllmSessionConfigInit(&tSessionConfig);
    pSession = xllmSessionCreate(&tSessionConfig, NULL);
    xworkAgentOptionsInit(&tOptions);
    tOptions.pSession = pSession;
    tOptions.sWorkspaceRoot = sWorkspace;
    tOptions.sArtifactDirectory = "../ignored-by-explicit-store";
    tOptions.pArtifactStore = pStore;
    tOptions.OnModelComplete = v3_final_complete;
    tOptions.pModelUserData = &uModelCalls;
    pAgent = pRuntime && pDefinition && pSession && pStore
                 ? xworkAgentCreateWithRuntime(pRuntime, pDefinition, &tOptions, &tError)
                 : NULL;
    xworkArtifactStoreRelease(pStore);
    pStore = NULL;
    CHECK(pAgent &&
              xwork__runtime_write_artifact(pAgent, "store", "original", 8u, NULL, &uId, &sPath,
                                            &tError) &&
              uId == 1u && sPath && xrtPathIsAbs(sPath),
          "explicit host storage publishes outside the model workspace");
    if (!pAgent || !sPath)
        goto unit;
    sDenied = xwork__resolve_path(pAgent, sPath, &tError);
    CHECK(!sDenied && tError.eCode == XWORK_ERROR_POLICY,
          "artifact storage grants no ordinary workspace path permission");
    pBase = xrtRootOpen(sBase);
    CHECK(pBase && rename(sStorage, sMoved) == 0 && xrtDirCreateAll(sStorage),
          "artifact storage moves while its independent anchor remains alive");
    sSpoof = xrtPathJoin(sStorage, "run-00000000000000000000/00000000000000000001-store.txt");
    CHECK(sSpoof && xwork__ensure_parent(sSpoof) &&
              xrtFileWriteAll(sSpoof, (xbytesview){(const uint8 *)"replaced", 8u}),
          "old diagnostic location gets a same-size replacement file");
    CHECK(xworkRuntimeReadArtifact(pRuntime, uId, 0u, 16u, &tChunk, &tError) &&
              tChunk.iSize == 8u && tChunk.bEof && memcmp(tChunk.pData, "original", 8u) == 0,
          "artifact reads use the retained anchor rather than a replaced path");
    xworkArtifactChunkUnit(&tChunk);
    free(sPath);
    sPath = NULL;
    CHECK(xwork__runtime_write_artifact(pAgent, "store", "new", 3u, NULL, &uId, &sPath, &tError) &&
              uId == 2u &&
              test_artifact_bytes(sMoved, "run-00000000000000000000/00000000000000000002-store.txt",
                                  "new") &&
              test_artifact_bytes(
                  sStorage, "run-00000000000000000000/00000000000000000001-store.txt", "replaced"),
          "later artifact writes stay in the moved store without touching its replacement");
    xworkAgentDestroy(pAgent);
    pAgent = NULL;
    CHECK(xworkRuntimeReadArtifact(pRuntime, uId, 0u, 16u, &tChunk, &tError) &&
              tChunk.iSize == 3u && memcmp(tChunk.pData, "new", 3u) == 0 && !uModelCalls,
          "registry ownership keeps storage readable after agent destruction");
unit:
    xworkArtifactChunkUnit(&tChunk);
    if (pRoot)
        (void)xrtRootClose(pRoot);
    if (pBase)
        (void)xrtRootClose(pBase);
    xworkArtifactStoreRelease(pStore);
    free(sPath);
    free(sDenied);
    if (sSpoof)
        xrtFree(sSpoof);
    xworkAgentDestroy(pAgent);
    xllmSessionDestroy(pSession);
    xworkAgentDefinitionRelease(pDefinition);
    xworkRuntimeRelease(pRuntime);
    (void)xrtDirRemoveAll(sBase);
}

static void test_unified_task_runtime(void)
{
    xwork_runtime_config tRuntimeConfig;
    xwork_runtime *pRuntime;
    xwork_scheduled_task_config tSchedule;
    xwork_task_snapshot *pSnapshot = NULL;
    xwork_event_snapshot *pEventSnapshot = NULL;
    xwork_task_info tInfo;
    xwork_event tRuntimeEvent;
    xwork_task_info tRestore;
    xwork_task_output tOutput;
    xwork_task_event arrEvents[8];
    xwork_error tError;
    xcancel *pCancel = NULL;
    uint64_t uScheduled = 0u;
    uint64_t uRestoredProcess = 0u;
    uint64_t uRestoredPending = 0u;
    uint64_t uWaited = 0u;
    uint64_t arrWait[2];
    size_t iBefore;
    size_t iEventCount = 0u;
    uint64_t uNextRevision = 0u;
    bool bHistoryLost = false;

    xworkRuntimeConfigInit(&tRuntimeConfig);
    tRuntimeConfig.uMaxTasks = 8u;
    tRuntimeConfig.iMaxRetainedTaskBytes = 8u;
    pRuntime = xworkRuntimeCreate(&tRuntimeConfig, &tError);
    CHECK(pRuntime != NULL, "unified task runtime creates with bounded retention");

    xworkScheduledTaskConfigInit(&tSchedule);
    tSchedule.uOwnerAgentId = 41u;
    tSchedule.uOwnerRunId = 42u;
    tSchedule.uParentTaskId = 40u;
    tSchedule.sOwnerSession = "sessions/task-test.json";
    tSchedule.sLabel = "scheduled fixture";
    tSchedule.sNotify = "fixture completed";
    tSchedule.iScheduledAtUs = xllmUnixUs() + INT64_C(1000000);
    CHECK(pRuntime && xworkRuntimeCreateScheduledTask(pRuntime, &tSchedule, &uScheduled, &tError) &&
              uScheduled != 0u,
          "scheduled task enters the unified id space as pending");

    pSnapshot = pRuntime ? xworkRuntimeTaskSnapshot(pRuntime, 41u, &tError) : NULL;
    xworkTaskInfoInit(&tInfo);
    CHECK(pSnapshot && xworkTaskSnapshotCount(pSnapshot) == 1u &&
              xworkTaskSnapshotFind(pSnapshot, uScheduled, &tInfo) &&
              tInfo.eKind == XWORK_TASK_SCHEDULED && tInfo.eState == XWORK_TASK_PENDING &&
              tInfo.uOwnerRunId == 42u && tInfo.uParentTaskId == 40u && tInfo.sOwnerSession &&
              strcmp(tInfo.sOwnerSession, "sessions/task-test.json") == 0 && tInfo.sLabel &&
              strcmp(tInfo.sLabel, "scheduled fixture") == 0,
          "task snapshot deep-copies ownership, lineage, state, and labels");
    xworkTaskSnapshotRelease(pSnapshot);
    pSnapshot = NULL;

    CHECK(xworkRuntimeStartScheduledTask(pRuntime, uScheduled, &tError),
          "scheduled task performs the pending-to-running transition");
    CHECK(!xworkRuntimeStartScheduledTask(pRuntime, uScheduled, &tError) &&
              tError.eCode == XWORK_ERROR_CONTEXT,
          "invalid scheduled transition is rejected");
    CHECK(xworkRuntimeFinishScheduledTask(pRuntime, uScheduled, XWORK_RESULT_OK, "0123456789",
                                          &tError),
          "running scheduled task publishes one terminal result");
    CHECK(xworkRuntimeReadTaskEvents(pRuntime, uScheduled, 0u, arrEvents, 8u, &iEventCount,
                                     &uNextRevision, &bHistoryLost, &tError) &&
              iEventCount == 3u && !bHistoryLost && uNextRevision == 3u &&
              arrEvents[0].eKind == XWORK_TASK_EVENT_CREATED &&
              arrEvents[1].eState == XWORK_TASK_RUNNING &&
              arrEvents[2].eState == XWORK_TASK_SUCCEEDED,
          "incremental task events preserve ordered state revisions");
    pEventSnapshot = xworkRuntimeEventSnapshot(pRuntime, 0u, 8u, &tError);
    memset(&tRuntimeEvent, 0, sizeof(tRuntimeEvent));
    CHECK(pEventSnapshot && xworkEventSnapshotCount(pEventSnapshot) == 3u &&
              xworkEventSnapshotAt(pEventSnapshot, 0u, &tRuntimeEvent) &&
              tRuntimeEvent.eKind == XWORK_EVENT_TASK_UPDATED &&
              tRuntimeEvent.uTaskId == uScheduled && tRuntimeEvent.uAgentId == 41u &&
              tRuntimeEvent.uRunId == 42u && tRuntimeEvent.eTaskEvent == XWORK_TASK_EVENT_CREATED &&
              tRuntimeEvent.eTaskKind == XWORK_TASK_SCHEDULED &&
              tRuntimeEvent.eTaskState == XWORK_TASK_PENDING && tRuntimeEvent.uTaskRevision == 1u &&
              xworkEventSnapshotAt(pEventSnapshot, 2u, &tRuntimeEvent) &&
              tRuntimeEvent.eTaskState == XWORK_TASK_SUCCEEDED && tRuntimeEvent.uTaskRevision == 3u,
          "task state changes join the runtime audit stream with lineage");
    xworkEventSnapshotRelease(pEventSnapshot);
    pEventSnapshot = NULL;

    xworkTaskOutputInit(&tOutput);
    CHECK(xworkRuntimeReadTaskOutput(pRuntime, uScheduled, 0u, 0u, 0u, 64u, &tOutput, &tError) &&
              tOutput.bComplete && tOutput.bResultDropped && tOutput.uResultStart == 2u &&
              tOutput.uResultNext == 10u && tOutput.iResultSize == 8u && tOutput.sResult &&
              strcmp(tOutput.sResult, "23456789") == 0,
          "task output uses independent cursors and reports retained-tail loss");
    xworkTaskOutputUnit(&tOutput);
    CHECK(xworkRuntimeWaitTasks(pRuntime, &uScheduled, 1u, XWORK_TASK_WAIT_ANY,
                                xllmDeadlineAfterUs(UINT64_C(1000000)), NULL, &uWaited,
                                &tError) == XWORK_RESULT_OK &&
              uWaited == uScheduled,
          "wait-any observes a terminal task through the public state machine");

    xworkTaskInfoInit(&tRestore);
    tRestore.uTaskId = 900u;
    tRestore.uOwnerAgentId = 51u;
    tRestore.uOwnerRunId = 52u;
    tRestore.eKind = XWORK_TASK_PROCESS;
    tRestore.eState = XWORK_TASK_RUNNING;
    tRestore.uRevision = 7u;
    tRestore.iCreatedAtUs = xllmUnixUs() - INT64_C(1000000);
    tRestore.iStartedAtUs = tRestore.iCreatedAtUs;
    tRestore.sLabel = "lost process";
    CHECK(xworkRuntimeRestoreTask(pRuntime, &tRestore, &uRestoredProcess, &tError) &&
              uRestoredProcess == 900u,
          "restore preserves an unclaimed stable task id");
    iBefore = xworkRuntimeTaskCount(pRuntime);
    CHECK(!xworkRuntimeRestoreTask(pRuntime, &tRestore, NULL, &tError) &&
              tError.eCode == XWORK_ERROR_CONTEXT && xworkRuntimeTaskCount(pRuntime) == iBefore,
          "duplicate restore fails without publishing a partial task");

    xworkTaskInfoInit(&tRestore);
    tRestore.uTaskId = 901u;
    tRestore.eKind = XWORK_TASK_SCHEDULED;
    tRestore.eState = XWORK_TASK_PENDING;
    tRestore.iCreatedAtUs = xllmUnixUs();
    tRestore.iScheduledAtUs = xllmUnixUs() + INT64_C(1000000);
    tRestore.sLabel = "pending restore";
    CHECK(xworkRuntimeRestoreTask(pRuntime, &tRestore, &uRestoredPending, &tError) &&
              uRestoredPending == 901u,
          "pending scheduled metadata remains recoverable after restart");

    pSnapshot = xworkRuntimeTaskSnapshot(pRuntime, 0u, &tError);
    xworkTaskInfoInit(&tInfo);
    CHECK(pSnapshot && xworkTaskSnapshotFind(pSnapshot, uRestoredProcess, &tInfo) &&
              tInfo.eState == XWORK_TASK_LOST && tInfo.uRevision == 9u && tInfo.iEndedAtUs != 0,
          "restored live process becomes LOST because its OS handle is gone");
    xworkTaskSnapshotRelease(pSnapshot);
    pSnapshot = NULL;

    pCancel = xrtCancelCreate();
    CHECK(pCancel && xrtCancelRequest(pCancel) &&
              xworkRuntimeWaitTasks(pRuntime, &uRestoredPending, 1u, XWORK_TASK_WAIT_ALL,
                                    xllmDeadlineAfterUs(UINT64_C(1000000)), pCancel, NULL,
                                    &tError) == XWORK_RESULT_CANCELLED &&
              tError.eCode == XWORK_ERROR_CANCELLED,
          "task wait observes cooperative cancellation while work is pending");
    xrtCancelDestroy(pCancel);
    pCancel = NULL;
    CHECK(xworkRuntimeCancelTask(pRuntime, uRestoredPending, &tError),
          "generic task cancellation terminates pending scheduled work");
    arrWait[0] = uRestoredProcess;
    arrWait[1] = uRestoredPending;
    CHECK(xworkRuntimeWaitTasks(pRuntime, arrWait, 2u, XWORK_TASK_WAIT_ALL,
                                xllmDeadlineAfterUs(UINT64_C(1000000)), NULL, NULL,
                                &tError) == XWORK_RESULT_OK,
          "wait-all observes LOST and CANCELLED as terminal states");

    pSnapshot = xworkRuntimeTaskSnapshot(pRuntime, 0u, &tError);
    xworkTaskInfoInit(&tInfo);
    CHECK(pSnapshot && xworkTaskSnapshotStopRequested(pSnapshot, uRestoredPending) &&
              !xworkTaskSnapshotStopRequested(pSnapshot, uRestoredProcess) &&
              !xworkTaskSnapshotStopRequested(pSnapshot, uScheduled),
          "sorted snapshot keeps each scheduled cancellation flag with its task id");
    CHECK(pSnapshot && xworkTaskSnapshotFind(pSnapshot, uScheduled, &tInfo) &&
              xworkRuntimeReleaseTask(pRuntime, uScheduled, &tError) && tInfo.sLabel &&
              strcmp(tInfo.sLabel, "scheduled fixture") == 0,
          "retained snapshot strings survive task release");
    xworkTaskSnapshotRelease(pSnapshot);
    CHECK(xworkRuntimeReleaseTask(pRuntime, uRestoredProcess, &tError) &&
              xworkRuntimeReleaseTask(pRuntime, uRestoredPending, &tError) &&
              xworkRuntimeTaskCount(pRuntime) == 0u,
          "terminal tasks release cleanly from the runtime table");
    xworkRuntimeRelease(pRuntime);
}

static int64_t test_schedule_time(int64_t iYear, int iMonth, int iDay, int iHour, int iMinute,
                                  int iSecond, int iOffset)
{
    xdatetime tDateTime;
    xtime iTime = 0;
    memset(&tDateTime, 0, sizeof(tDateTime));
    tDateTime.Year = iYear;
    tDateTime.Month = iMonth;
    tDateTime.Day = iDay;
    tDateTime.Hour = iHour;
    tDateTime.Minute = iMinute;
    tDateTime.Second = iSecond;
    tDateTime.Offset = iOffset;
    return mdo_test_time_make(&tDateTime, &iTime) ? (int64_t)iTime : 0;
}

static bool finish_schedule_claim(xwork_runtime *pRuntime, const xwork_schedule_claim *pClaim,
                                  const char *sResult)
{
    xwork_error tError;
    return pClaim && pClaim->uTaskId &&
           xworkRuntimeFinishScheduledTask(pRuntime, pClaim->uTaskId, XWORK_RESULT_OK, sResult,
                                           &tError) &&
           xworkRuntimeReleaseTask(pRuntime, pClaim->uTaskId, &tError);
}

static void test_runtime_scheduler(void)
{
    const int64_t iMinute = INT64_C(60) * INT64_C(1000000);
    const int64_t iStart = test_schedule_time(2026, 3, 2, 9, 30, 0, 8 * 60 * 60);
    const int64_t iWednesday = test_schedule_time(2026, 3, 4, 9, 30, 0, 8 * 60 * 60);
    const int64_t iNextMonday = test_schedule_time(2026, 3, 9, 9, 30, 0, 8 * 60 * 60);
    xwork_runtime_config tRuntimeConfig;
    xwork_schedule_config tConfig;
    xwork_schedule_info tInfo;
    xwork_schedule_info tRestoreInfo;
    xwork_schedule_claim tClaim;
    xwork_task_snapshot *pTasks = NULL;
    xwork_task_info tTaskInfo;
    xwork_runtime *pRuntime = NULL;
    xwork_runtime *pRestored = NULL;
    xwork_error tError;
    int64_t iNext = 0;
    int64_t iNextWake = 0;
    bool bHas = false;
    bool bClaimed = false;

    xworkScheduleConfigInit(&tConfig);
    tConfig.sScheduleId = "calendar";
    tConfig.sLabel = "calendar recurrence";
    tConfig.eFrequency = XWORK_SCHEDULE_DAILY;
    tConfig.iStartAtUs = iStart;
    tConfig.eTimezone = XWORK_SCHEDULE_TIMEZONE_FIXED_OFFSET;
    tConfig.iUtcOffsetSeconds = 8 * 60 * 60;
    CHECK(iStart > 0 && xworkScheduleNextOccurrence(&tConfig, iStart, &iNext, &bHas, &tError) &&
              bHas && iNext == iStart + INT64_C(24) * INT64_C(60) * iMinute,
          "daily recurrence preserves fixed-offset wall-clock time");
    tConfig.eFrequency = XWORK_SCHEDULE_WEEKLY;
    tConfig.uWeekdayMask = (uint8_t)((1u << XTIME_MONDAY) | (1u << XTIME_WEDNESDAY));
    CHECK(xworkScheduleNextOccurrence(&tConfig, iStart, &iNext, &bHas, &tError) && bHas &&
              iNext == iWednesday &&
              xworkScheduleNextOccurrence(&tConfig, iWednesday, &iNext, &bHas, &tError) && bHas &&
              iNext == iNextMonday,
          "weekly recurrence honors weekday mask and anchor week");
    {
        xdatetime tLocal;
        xdatetime tNextLocal;
        xtime iLocalStart = 0;
        memset(&tLocal, 0, sizeof(tLocal));
        tLocal.Year = 2026;
        tLocal.Month = 1;
        tLocal.Day = 15;
        tLocal.Hour = 12;
        tLocal.Minute = 34;
        CHECK(mdo_test_time_from_local(&tLocal, XTIME_FOLD_EARLIER, &iLocalStart),
              "system-local recurrence fixture converts to an epoch");
        tConfig.eFrequency = XWORK_SCHEDULE_DAILY;
        tConfig.uWeekdayMask = 0u;
        tConfig.eTimezone = XWORK_SCHEDULE_TIMEZONE_SYSTEM_LOCAL;
        tConfig.iUtcOffsetSeconds = 0;
        tConfig.iStartAtUs = (int64_t)iLocalStart;
        CHECK(xworkScheduleNextOccurrence(&tConfig, (int64_t)iLocalStart, &iNext, &bHas, &tError) &&
                  bHas && mdo_test_time_local((xtime)iNext, &tNextLocal) && tNextLocal.Hour == 12 &&
                  tNextLocal.Minute == 34,
              "system-local daily recurrence keeps wall-clock fields");
    }

    xworkRuntimeConfigInit(&tRuntimeConfig);
    tRuntimeConfig.uMaxTasks = 16u;
    tRuntimeConfig.uMaxSchedules = 16u;
    pRuntime = xworkRuntimeCreate(&tRuntimeConfig, &tError);
    pRestored = xworkRuntimeCreate(&tRuntimeConfig, &tError);
    CHECK(pRuntime && pRestored, "runtime scheduler fixtures create with explicit bounds");

    memset(&tClaim, 0, sizeof(tClaim));
    tClaim.uSize = (uint32_t)sizeof(tClaim);
    tClaim.uAbiVersion = XWORK_ABI_VERSION + 1u;
    CHECK(
        !xworkRuntimeClaimDueSchedule(pRuntime, iStart, &tClaim, &bClaimed, &iNextWake, &tError) &&
            tError.eCode == XWORK_ERROR_INVALID_ARGUMENT &&
            tClaim.uAbiVersion == XWORK_ABI_VERSION + 1u,
        "scheduler claim rejects an invalid output ABI without rewriting it");

    {
        xwork_scheduled_task_config tInvalidTask;
        uint64_t uInvalidTaskId = 0u;
        xworkScheduledTaskConfigInit(&tInvalidTask);
        tInvalidTask.sLabel = "invalid schedule lineage";
        tInvalidTask.iScheduledAtUs = iStart;
        tInvalidTask.sScheduleId = "orphan";
        CHECK(!xworkRuntimeCreateScheduledTask(pRuntime, &tInvalidTask, &uInvalidTaskId, &tError) &&
                  uInvalidTaskId == 0u,
              "scheduled task creation rejects incomplete definition lineage");
    }

    xworkScheduleConfigInit(&tConfig);
    tConfig.sScheduleId = "once";
    tConfig.sLabel = "one-shot fixture";
    tConfig.sAgentProfile = "review";
    tConfig.sInput = "inspect the selected workspace";
    tConfig.iStartAtUs = iStart;
    CHECK(xworkRuntimeRegisterSchedule(pRuntime, &tConfig, &tError) &&
              xworkRuntimeScheduleCount(pRuntime) == 1u &&
              !xworkRuntimeRegisterSchedule(pRuntime, &tConfig, &tError) &&
              xworkRuntimeScheduleCount(pRuntime) == 1u,
          "cold schedule registration is copied and duplicate-safe");
    xworkScheduleClaimInit(&tClaim);
    CHECK(xworkRuntimeClaimDueSchedule(pRuntime, iStart, &tClaim, &bClaimed, &iNextWake, &tError) &&
              bClaimed && tClaim.uTaskId != 0u && tClaim.iOccurrenceAtUs == iStart &&
              strcmp(tClaim.sScheduleId, "once") == 0 &&
              strcmp(tClaim.sAgentProfile, "review") == 0 &&
              strcmp(tClaim.sInput, "inspect the selected workspace") == 0,
          "due one-shot claim carries stable execution metadata");
    pTasks = xworkRuntimeTaskSnapshot(pRuntime, 0u, &tError);
    xworkTaskInfoInit(&tTaskInfo);
    CHECK(pTasks && xworkTaskSnapshotFind(pTasks, tClaim.uTaskId, &tTaskInfo) &&
              tTaskInfo.eKind == XWORK_TASK_SCHEDULED && tTaskInfo.eState == XWORK_TASK_RUNNING &&
              tTaskInfo.sScheduleId && strcmp(tTaskInfo.sScheduleId, "once") == 0 &&
              tTaskInfo.uScheduleGeneration == tClaim.uScheduleGeneration,
          "scheduler claim enters the unified task table with definition lineage");
    xworkTaskSnapshotRelease(pTasks);
    pTasks = NULL;
    CHECK(finish_schedule_claim(pRuntime, &tClaim, "one-shot complete") &&
              xworkRuntimeUnregisterSchedule(pRuntime, "once", &tError) &&
              xworkRuntimeScheduleCount(pRuntime) == 0u,
          "claimed schedule completes and unregisters through stable task APIs");

    xworkScheduleConfigInit(&tConfig);
    tConfig.sScheduleId = "manual";
    tConfig.sLabel = "explicit trigger fixture";
    tConfig.sAgentProfile = "review";
    tConfig.sInput = "inspect now";
    tConfig.iStartAtUs = iStart + iMinute;
    tConfig.bEnabled = false;
    CHECK(xworkRuntimeRegisterSchedule(pRuntime, &tConfig, &tError),
          "paused schedule registers for explicit triggering");
    xworkScheduleClaimInit(&tClaim);
    CHECK(xworkRuntimeTriggerSchedule(pRuntime, "manual", iStart, &tClaim, &tError) &&
              tClaim.uTaskId != 0u && tClaim.iOccurrenceAtUs == iStart &&
              strcmp(tClaim.sAgentProfile, "review") == 0 &&
              strcmp(tClaim.sInput, "inspect now") == 0,
          "explicit trigger starts a paused schedule with copied definition");
    xworkScheduleInfoInit(&tInfo);
    CHECK(xworkRuntimeScheduleGetInfo(pRuntime, "manual", &tInfo) &&
              tInfo.iNextOccurrenceAtUs == iStart + iMinute && tInfo.iLastClaimedAtUs == iStart &&
              tInfo.uClaimCount == 1u && tInfo.iActiveRuns == 1u,
          "explicit trigger records work without advancing recurrence");
    pTasks = xworkRuntimeTaskSnapshot(pRuntime, 0u, &tError);
    xworkTaskInfoInit(&tTaskInfo);
    CHECK(pTasks && xworkTaskSnapshotFind(pTasks, tClaim.uTaskId, &tTaskInfo) &&
              tTaskInfo.eKind == XWORK_TASK_SCHEDULED && tTaskInfo.eState == XWORK_TASK_RUNNING &&
              tTaskInfo.uScheduleGeneration == tClaim.uScheduleGeneration,
          "explicit trigger retains unified task state and lineage");
    xworkTaskSnapshotRelease(pTasks);
    pTasks = NULL;
    {
        xwork_schedule_claim tConcurrent;
        xworkScheduleClaimInit(&tConcurrent);
        CHECK(!xworkRuntimeTriggerSchedule(pRuntime, "manual", iStart, &tConcurrent, &tError) &&
                  tError.eCode == XWORK_ERROR_CONTEXT && tConcurrent.uTaskId == 0u,
              "explicit trigger enforces concurrent-run limit atomically");
    }
    CHECK(finish_schedule_claim(pRuntime, &tClaim, "manual complete"),
          "explicitly triggered task completes through scheduled task APIs");
    xworkScheduleClaimInit(&tClaim);
    CHECK(xworkRuntimeTriggerSchedule(pRuntime, "manual", iStart + 1, &tClaim, &tError) &&
              finish_schedule_claim(pRuntime, &tClaim, "manual again") &&
              xworkRuntimeUnregisterSchedule(pRuntime, "manual", &tError),
          "explicit trigger can run again after completed work is reaped");

    {
        xwork_schedule_info tOldInfo;
        xwork_schedule_info tNewInfo;
        const char *sBorrowedOldLabel;
        xworkScheduleConfigInit(&tConfig);
        tConfig.sScheduleId = "replace";
        tConfig.sLabel = "original definition";
        tConfig.sInput = "original input";
        tConfig.iStartAtUs = iStart;
        xworkScheduleInfoInit(&tOldInfo);
        CHECK(xworkRuntimeRegisterSchedule(pRuntime, &tConfig, &tError) &&
                  xworkRuntimeScheduleGetInfo(pRuntime, "replace", &tOldInfo),
              "replace fixture publishes its original definition");
        sBorrowedOldLabel = tOldInfo.tConfig.sLabel;
        tConfig.sLabel = "replacement definition";
        tConfig.sInput = "replacement input";
        tConfig.iStartAtUs = iStart + iMinute;
        xworkScheduleInfoInit(&tNewInfo);
        CHECK(xworkRuntimeReplaceSchedule(pRuntime, &tConfig, &tError) &&
                  xworkRuntimeScheduleCount(pRuntime) == 1u &&
                  xworkRuntimeScheduleGetInfo(pRuntime, "replace", &tNewInfo) &&
                  tNewInfo.uGeneration > tOldInfo.uGeneration &&
                  tNewInfo.iNextOccurrenceAtUs == iStart + iMinute &&
                  strcmp(tNewInfo.tConfig.sLabel, "replacement definition") == 0 &&
                  strcmp(tNewInfo.tConfig.sInput, "replacement input") == 0 &&
                  strcmp(sBorrowedOldLabel, "original definition") == 0,
              "definition replacement is atomic and keeps old borrows stable");
        CHECK(!xworkRuntimeRevertScheduleReplacement(pRuntime, "replace", tNewInfo.uGeneration + 1u,
                                                     tOldInfo.uGeneration, &tError) &&
                  tError.eCode == XWORK_ERROR_CONTEXT &&
                  xworkRuntimeRevertScheduleReplacement(pRuntime, "replace", tNewInfo.uGeneration,
                                                        tOldInfo.uGeneration, &tError) &&
                  xworkRuntimeScheduleGetInfo(pRuntime, "replace", &tNewInfo) &&
                  tNewInfo.uGeneration == tOldInfo.uGeneration &&
                  tNewInfo.iNextOccurrenceAtUs == iStart &&
                  strcmp(tNewInfo.tConfig.sLabel, "original definition") == 0,
              "exact generations can roll back an unclaimed replacement");
        tConfig.sLabel = "replacement definition";
        tConfig.sInput = "replacement input";
        tConfig.iStartAtUs = iStart + iMinute;
        CHECK(xworkRuntimeReplaceSchedule(pRuntime, &tConfig, &tError) &&
                  xworkRuntimeScheduleGetInfo(pRuntime, "replace", &tNewInfo),
              "replacement can be published again after rollback");
        tConfig.sLabel = "";
        CHECK(!xworkRuntimeReplaceSchedule(pRuntime, &tConfig, &tError) &&
                  tError.eCode == XWORK_ERROR_INVALID_ARGUMENT &&
                  xworkRuntimeScheduleGetInfo(pRuntime, "replace", &tNewInfo) &&
                  strcmp(tNewInfo.tConfig.sLabel, "replacement definition") == 0,
              "invalid replacement leaves the current definition unchanged");
        tConfig.sLabel = "blocked replacement";
        tConfig.iStartAtUs = iStart + 2 * iMinute;
        xworkScheduleClaimInit(&tClaim);
        CHECK(xworkRuntimeClaimDueSchedule(pRuntime, iStart + iMinute, &tClaim, &bClaimed, NULL,
                                           &tError) &&
                  bClaimed && !xworkRuntimeReplaceSchedule(pRuntime, &tConfig, &tError) &&
                  tError.eCode == XWORK_ERROR_CONTEXT &&
                  xworkRuntimeScheduleGetInfo(pRuntime, "replace", &tNewInfo) &&
                  strcmp(tNewInfo.tConfig.sLabel, "replacement definition") == 0,
              "active scheduled work blocks definition replacement");
        CHECK(finish_schedule_claim(pRuntime, &tClaim, "replacement done") &&
                  xworkRuntimeUnregisterSchedule(pRuntime, "replace", &tError),
              "replacement fixture completes and unregisters");
    }

    xworkScheduleConfigInit(&tConfig);
    tConfig.sScheduleId = "skip";
    tConfig.sLabel = "skip missed work";
    tConfig.eFrequency = XWORK_SCHEDULE_MINUTELY;
    tConfig.iStartAtUs = iStart;
    tConfig.eMisfirePolicy = XWORK_SCHEDULE_MISFIRE_SKIP;
    tConfig.uMisfireGraceSeconds = 1u;
    CHECK(xworkRuntimeRegisterSchedule(pRuntime, &tConfig, &tError),
          "skip-misfire fixture registers");
    xworkScheduleClaimInit(&tClaim);
    CHECK(xworkRuntimeClaimDueSchedule(pRuntime, iStart + 5 * iMinute, &tClaim, &bClaimed,
                                       &iNextWake, &tError) &&
              !bClaimed && iNextWake == iStart + 6 * iMinute,
          "skip policy discards missed occurrences and reports the next wake");
    xworkScheduleInfoInit(&tInfo);
    CHECK(xworkRuntimeScheduleGetInfo(pRuntime, "skip", &tInfo) &&
              tInfo.iNextOccurrenceAtUs == iStart + 6 * iMinute && tInfo.uClaimCount == 0u &&
              tInfo.uMisfireCount == 1u,
          "skip policy persists its advanced cursor and diagnostics");
    CHECK(xworkRuntimeUnregisterSchedule(pRuntime, "skip", &tError), "skip fixture unregisters");

    xworkScheduleConfigInit(&tConfig);
    tConfig.sScheduleId = "run-once";
    tConfig.sLabel = "coalesced recovery";
    tConfig.eFrequency = XWORK_SCHEDULE_MINUTELY;
    tConfig.iStartAtUs = iStart;
    tConfig.eMisfirePolicy = XWORK_SCHEDULE_MISFIRE_RUN_ONCE;
    tConfig.uMisfireGraceSeconds = 1u;
    CHECK(xworkRuntimeRegisterSchedule(pRuntime, &tConfig, &tError),
          "run-once misfire fixture registers");
    xworkScheduleClaimInit(&tClaim);
    CHECK(xworkRuntimeClaimDueSchedule(pRuntime, iStart + 5 * iMinute + iMinute / 2, &tClaim,
                                       &bClaimed, &iNextWake, &tError) &&
              bClaimed && tClaim.iOccurrenceAtUs == iStart + 5 * iMinute &&
              iNextWake == iStart + 6 * iMinute,
          "run-once policy coalesces a missed range to its latest occurrence");
    CHECK(finish_schedule_claim(pRuntime, &tClaim, "coalesced"),
          "coalesced scheduled task completes");
    xworkScheduleInfoInit(&tInfo);
    CHECK(xworkRuntimeScheduleGetInfo(pRuntime, "run-once", &tInfo) &&
              tInfo.iNextOccurrenceAtUs == iStart + 6 * iMinute &&
              tInfo.iLastClaimedAtUs == iStart + 5 * iMinute && tInfo.uClaimCount == 1u &&
              tInfo.uMisfireCount == 1u && tInfo.iActiveRuns == 0u,
          "run-once state is serializable after completion");
    tRestoreInfo = tInfo;
    tRestoreInfo.uGeneration = UINT64_MAX;
    CHECK(!xworkRuntimeRestoreSchedule(pRestored, &tRestoreInfo, &tError) &&
              xworkRuntimeScheduleCount(pRestored) == 0u,
          "invalid restored generation fails before definition publication");
    tRestoreInfo = tInfo;
    CHECK(xworkRuntimeRestoreSchedule(pRestored, &tRestoreInfo, &tError) &&
              !xworkRuntimeRestoreSchedule(pRestored, &tRestoreInfo, &tError) &&
              xworkRuntimeScheduleCount(pRestored) == 1u,
          "restart restore preserves one definition and rejects duplicates");
    xworkScheduleClaimInit(&tClaim);
    CHECK(xworkRuntimeClaimDueSchedule(pRestored, iStart + 6 * iMinute, &tClaim, &bClaimed,
                                       &iNextWake, &tError) &&
              bClaimed && tClaim.iOccurrenceAtUs == iStart + 6 * iMinute &&
              tClaim.uScheduleGeneration == tRestoreInfo.uGeneration,
          "restored scheduler resumes from the persisted next occurrence");
    CHECK(finish_schedule_claim(pRestored, &tClaim, "restored") &&
              xworkRuntimeUnregisterSchedule(pRestored, "run-once", &tError),
          "restored scheduled task completes normally");
    CHECK(xworkRuntimeUnregisterSchedule(pRuntime, "run-once", &tError),
          "source run-once fixture unregisters");

    xworkScheduleConfigInit(&tConfig);
    tConfig.sScheduleId = "catch-up";
    tConfig.sLabel = "bounded catch-up";
    tConfig.eFrequency = XWORK_SCHEDULE_MINUTELY;
    tConfig.iStartAtUs = iStart;
    tConfig.eMisfirePolicy = XWORK_SCHEDULE_MISFIRE_CATCH_UP;
    tConfig.uMisfireGraceSeconds = 1u;
    tConfig.uMaxCatchUp = 2u;
    CHECK(xworkRuntimeRegisterSchedule(pRuntime, &tConfig, &tError),
          "bounded catch-up fixture registers");
    xworkScheduleClaimInit(&tClaim);
    CHECK(xworkRuntimeClaimDueSchedule(pRuntime, iStart + 5 * iMinute + iMinute / 2, &tClaim,
                                       &bClaimed, NULL, &tError) &&
              bClaimed && tClaim.iOccurrenceAtUs == iStart &&
              finish_schedule_claim(pRuntime, &tClaim, "catch-up-1"),
          "catch-up starts with the oldest missed occurrence");
    xworkScheduleClaimInit(&tClaim);
    CHECK(xworkRuntimeClaimDueSchedule(pRuntime, iStart + 5 * iMinute + iMinute / 2, &tClaim,
                                       &bClaimed, &iNextWake, &tError) &&
              bClaimed && tClaim.iOccurrenceAtUs == iStart + iMinute &&
              finish_schedule_claim(pRuntime, &tClaim, "catch-up-2"),
          "catch-up emits no more than the configured recovery budget");
    xworkScheduleClaimInit(&tClaim);
    CHECK(xworkRuntimeClaimDueSchedule(pRuntime, iStart + 5 * iMinute + iMinute / 2, &tClaim,
                                       &bClaimed, &iNextWake, &tError) &&
              !bClaimed && iNextWake == iStart + 6 * iMinute,
          "catch-up skips the remaining backlog after its hard cap");
    xworkScheduleInfoInit(&tInfo);
    CHECK(xworkRuntimeScheduleGetInfo(pRuntime, "catch-up", &tInfo) && tInfo.uClaimCount == 2u &&
              tInfo.uMisfireCount == 1u,
          "catch-up diagnostics count one bounded misfire episode");
    CHECK(xworkRuntimeUnregisterSchedule(pRuntime, "catch-up", &tError),
          "catch-up fixture unregisters");

    xworkScheduleConfigInit(&tConfig);
    tConfig.sScheduleId = "overlap-skip";
    tConfig.sLabel = "skip overlapping occurrence";
    tConfig.eFrequency = XWORK_SCHEDULE_MINUTELY;
    tConfig.iStartAtUs = iStart;
    tConfig.eOverlapPolicy = XWORK_SCHEDULE_OVERLAP_SKIP;
    CHECK(xworkRuntimeRegisterSchedule(pRuntime, &tConfig, &tError),
          "skip-overlap fixture registers");
    xworkScheduleClaimInit(&tClaim);
    CHECK(xworkRuntimeClaimDueSchedule(pRuntime, iStart, &tClaim, &bClaimed, NULL, &tError) &&
              bClaimed,
          "skip-overlap fixture claims its first occurrence");
    {
        xwork_schedule_claim tSkippedClaim;
        xworkScheduleClaimInit(&tSkippedClaim);
        CHECK(xworkRuntimeClaimDueSchedule(pRuntime, iStart + iMinute, &tSkippedClaim, &bClaimed,
                                           &iNextWake, &tError) &&
                  !bClaimed && iNextWake == iStart + 2 * iMinute,
              "skip-overlap advances past a due occurrence while work is active");
    }
    CHECK(finish_schedule_claim(pRuntime, &tClaim, "overlap-first"),
          "skip-overlap first task completes");
    xworkScheduleInfoInit(&tInfo);
    CHECK(xworkRuntimeScheduleGetInfo(pRuntime, "overlap-skip", &tInfo) &&
              tInfo.iNextOccurrenceAtUs == iStart + 2 * iMinute && tInfo.uClaimCount == 1u &&
              tInfo.uMisfireCount == 1u && tInfo.iActiveRuns == 0u,
          "skip-overlap state records the skipped occurrence deterministically");
    CHECK(xworkRuntimeUnregisterSchedule(pRuntime, "overlap-skip", &tError),
          "skip-overlap fixture unregisters");

    xworkScheduleConfigInit(&tConfig);
    tConfig.sScheduleId = "z-tie";
    tConfig.sLabel = "later tie";
    tConfig.iStartAtUs = iStart;
    CHECK(xworkRuntimeRegisterSchedule(pRuntime, &tConfig, &tError), "later tie fixture registers");
    tConfig.sScheduleId = "a-tie";
    tConfig.sLabel = "earlier tie";
    CHECK(xworkRuntimeRegisterSchedule(pRuntime, &tConfig, &tError),
          "earlier tie fixture registers");
    xworkScheduleClaimInit(&tClaim);
    CHECK(xworkRuntimeClaimDueSchedule(pRuntime, iStart, &tClaim, &bClaimed, NULL, &tError) &&
              bClaimed && strcmp(tClaim.sScheduleId, "a-tie") == 0 &&
              finish_schedule_claim(pRuntime, &tClaim, "tie winner"),
          "equal due times use schedule id as a stable tie breaker");
    CHECK(xworkRuntimeUnregisterSchedule(pRuntime, "a-tie", &tError) &&
              xworkRuntimeUnregisterSchedule(pRuntime, "z-tie", &tError),
          "tie fixtures unregister cleanly");

    xworkScheduleConfigInit(&tConfig);
    tConfig.sScheduleId = "queue";
    tConfig.sLabel = "queue one overlap";
    tConfig.eFrequency = XWORK_SCHEDULE_MINUTELY;
    tConfig.iStartAtUs = iStart;
    tConfig.eOverlapPolicy = XWORK_SCHEDULE_OVERLAP_QUEUE_ONE;
    CHECK(xworkRuntimeRegisterSchedule(pRuntime, &tConfig, &tError),
          "queue-one overlap fixture registers");
    xworkScheduleClaimInit(&tClaim);
    CHECK(xworkRuntimeClaimDueSchedule(pRuntime, iStart, &tClaim, &bClaimed, NULL, &tError) &&
              bClaimed,
          "queue-one fixture claims its first occurrence");
    {
        xwork_schedule_claim tBlockedClaim;
        xworkScheduleClaimInit(&tBlockedClaim);
        CHECK(xworkRuntimeClaimDueSchedule(pRuntime, iStart + iMinute, &tBlockedClaim, &bClaimed,
                                           &iNextWake, &tError) &&
                  !bClaimed && iNextWake == 0,
              "queue-one holds one due occurrence while the prior run is active");
    }
    CHECK(finish_schedule_claim(pRuntime, &tClaim, "queue-first"),
          "queue-one first task completes");
    xworkScheduleClaimInit(&tClaim);
    CHECK(xworkRuntimeClaimDueSchedule(pRuntime, iStart + iMinute, &tClaim, &bClaimed, NULL,
                                       &tError) &&
              bClaimed && tClaim.iOccurrenceAtUs == iStart + iMinute &&
              finish_schedule_claim(pRuntime, &tClaim, "queue-second"),
          "queue-one releases the retained occurrence after completion");
    CHECK(xworkRuntimeSetScheduleEnabled(pRuntime, "queue", false, &tError),
          "schedule can be disabled without cancelling completed work");
    xworkScheduleInfoInit(&tInfo);
    CHECK(xworkRuntimeScheduleAt(pRuntime, 0u, &tInfo) &&
              strcmp(tInfo.tConfig.sScheduleId, "queue") == 0 && !tInfo.tConfig.bEnabled,
          "schedule enumeration exposes copied configuration and enabled state");
    CHECK(xworkRuntimeUnregisterSchedule(pRuntime, "queue", &tError) &&
              xworkRuntimeScheduleCount(pRuntime) == 0u,
          "scheduler registry drains after explicit unregister");

    {
        xwork_runtime_config tSmallConfig;
        xwork_runtime *pSmallRuntime;
        xworkRuntimeConfigInit(&tSmallConfig);
        tSmallConfig.uMaxSchedules = 1u;
        pSmallRuntime = xworkRuntimeCreate(&tSmallConfig, &tError);
        xworkScheduleConfigInit(&tConfig);
        tConfig.sScheduleId = "capacity-one";
        tConfig.sLabel = "stable definition slot";
        tConfig.iStartAtUs = iStart;
        CHECK(pSmallRuntime && xworkRuntimeRegisterSchedule(pSmallRuntime, &tConfig, &tError) &&
                  xworkRuntimeUnregisterSchedule(pSmallRuntime, "capacity-one", &tError),
              "bounded scheduler retains an unregistered definition safely");
        tConfig.sScheduleId = "capacity-two";
        CHECK(!xworkRuntimeRegisterSchedule(pSmallRuntime, &tConfig, &tError) &&
                  tError.eCode == XWORK_ERROR_LIMIT,
              "stable borrowed definitions make the schedule cap a lifetime bound");
        xworkRuntimeRelease(pSmallRuntime);
    }

    xworkRuntimeRelease(pRestored);
    xworkRuntimeRelease(pRuntime);
}

#include "mcp_http_errors.inc.c"

static int run_mcp_http_probe(const char *sModernEndpoint, const char *sLegacyEndpoint)
{
    xwork_runtime_config tRuntimeConfig;
    xwork_runtime *pRuntime;
    xwork_mcp_server_config tModern;
    xwork_mcp_server_config tLegacy;
    xwork_mcp_server_config tInvalid;
    xwork_mcp_http_header tModernHeader;
    xwork_mcp_http_header tLegacyHeader;
    xwork_mcp_http_header tReservedHeader;
    xwork_mcp_server_info tInfo;
    xwork_mcp_client *pModern;
    xwork_mcp_client *pReused;
    xwork_mcp_client *pRotated;
    xwork_mcp_client *pLegacy;
    xwork_tool_output tOutput;
    xwork_error tError;
    size_t iReplaced = 0u;

    setvbuf(stdout, NULL, _IONBF, 0);
    g_iFailures = 0;
    printf("xwork MCP Streamable HTTP probe\n");
    xworkRuntimeConfigInit(&tRuntimeConfig);
    xworkErrorInit(&tError);
    pRuntime = xworkRuntimeCreate(&tRuntimeConfig, &tError);
    CHECK(pRuntime != NULL, "HTTP probe creates an isolated runtime");
    if (!pRuntime)
        return 1;

    xworkMcpServerConfigInit(&tInvalid);
    tInvalid.sServerId = "remote-plain-http";
    tInvalid.sSummary = "Invalid non-loopback cleartext HTTP fixture.";
    tInvalid.eTransport = XWORK_MCP_TRANSPORT_STREAMABLE_HTTP;
    tInvalid.sEndpoint = "http://example.com/mcp";
    CHECK(!xworkRuntimeRegisterMcpServer(pRuntime, &tInvalid, &tError) &&
              tError.eCode == XWORK_ERROR_INVALID_ARGUMENT,
          "non-loopback cleartext HTTP endpoint is rejected");

    tReservedHeader.sName = "Content-Type";
    tReservedHeader.sValue = "text/plain";
    xworkMcpServerConfigInit(&tInvalid);
    tInvalid.sServerId = "reserved-header";
    tInvalid.sSummary = "Invalid reserved HTTP header fixture.";
    tInvalid.eTransport = XWORK_MCP_TRANSPORT_STREAMABLE_HTTP;
    tInvalid.sEndpoint = sModernEndpoint;
    tInvalid.pHttpHeaders = &tReservedHeader;
    tInvalid.iHttpHeaderCount = 1u;
    CHECK(!xworkRuntimeRegisterMcpServer(pRuntime, &tInvalid, &tError) &&
              tError.eCode == XWORK_ERROR_INVALID_ARGUMENT,
          "transport-owned HTTP headers cannot be overridden");

    xworkMcpServerConfigInit(&tInvalid);
    tInvalid.sServerId = "mixed-transport";
    tInvalid.sSummary = "Invalid mixed transport fixture.";
    tInvalid.eTransport = XWORK_MCP_TRANSPORT_STREAMABLE_HTTP;
    tInvalid.sEndpoint = sModernEndpoint;
    tInvalid.sProgram = "forbidden";
    CHECK(!xworkRuntimeRegisterMcpServer(pRuntime, &tInvalid, &tError) &&
              tError.eCode == XWORK_ERROR_INVALID_ARGUMENT,
          "HTTP definitions reject stdio-only process fields");

    tModernHeader.sName = "Authorization";
    tModernHeader.sValue = "Bearer modern-secret";
    xworkMcpServerConfigInit(&tModern);
    tModern.sServerId = "modern-http";
    tModern.sSummary = "Bounded modern Streamable HTTP fixture.";
    tModern.eTransport = XWORK_MCP_TRANSPORT_STREAMABLE_HTTP;
    tModern.sEndpoint = sModernEndpoint;
    tModern.pHttpHeaders = &tModernHeader;
    tModern.iHttpHeaderCount = 1u;
    tModern.uStartupTimeoutMs = 3000u;
    tModern.uRequestTimeoutMs = 3000u;
    CHECK(xworkRuntimeReplaceMcpServersBySource(pRuntime, "test.http", &tModern, 1u, &iReplaced,
                                                &tError) &&
              iReplaced == 0u && pRuntime->pMcpNetEngine == NULL &&
              pRuntime->pMcpHttpClient == NULL,
          "HTTP source registration remains cold and copies its definition");
    pModern = xwork__runtime_mcp_find(pRuntime, "modern-http");
    CHECK(pModern != NULL && xworkRuntimeRefreshMcpServer(pRuntime, "modern-http", NULL,
                                                          XLLM_DEADLINE_NEVER, &tError),
          "modern HTTP discovery completes on first explicit refresh");
    xworkMcpServerInfoInit(&tInfo);
    CHECK(xworkRuntimeMcpServerGetInfo(pRuntime, "modern-http", &tInfo) &&
              tInfo.eTransport == XWORK_MCP_TRANSPORT_STREAMABLE_HTTP && tInfo.sEndpoint &&
              strcmp(tInfo.sEndpoint, sModernEndpoint) == 0 &&
              tInfo.eState == XWORK_MCP_SERVER_READY && tInfo.iDiscoveredToolCount == 1u &&
              tInfo.uRequestsCompleted == 2u,
          "modern HTTP diagnostics expose one valid routed tool");

    xworkToolOutputInit(&tOutput);
    CHECK(pModern &&
              xworkMcpClientCallTool(
                  pModern, "echo",
                  "{\"text\":\"hello world\",\"count\":7,\"nested\":{\"flag\":true}}", NULL,
                  XLLM_DEADLINE_NEVER, &tOutput, &tError) == XWORK_RESULT_OK &&
              tOutput.bSuccess && tOutput.sContent && strstr(tOutput.sContent, "modern http ok"),
          "modern HTTP call sends routing and schema-derived parameter headers");
    xworkToolOutputUnit(&tOutput);

    CHECK(xworkRuntimeReplaceMcpServersBySource(pRuntime, "test.http", &tModern, 1u, &iReplaced,
                                                &tError) &&
              iReplaced == 0u &&
              (pReused = xwork__runtime_mcp_find(pRuntime, "modern-http")) == pModern &&
              pReused->bConnected,
          "unchanged HTTP definitions retain connection and schema state");
    tModernHeader.sValue = "Bearer rotated-secret";
    CHECK(xworkRuntimeReplaceMcpServersBySource(pRuntime, "test.http", &tModern, 1u, &iReplaced,
                                                &tError) &&
              iReplaced == 1u &&
              (pRotated = xwork__runtime_mcp_find(pRuntime, "modern-http")) != NULL &&
              pRotated != pModern && !pRotated->bConnected && !pModern->bRegistered,
          "credential changes retire the live HTTP definition atomically");

    tLegacyHeader.sName = "Authorization";
    tLegacyHeader.sValue = "Bearer legacy-secret";
    xworkMcpServerConfigInit(&tLegacy);
    tLegacy.sServerId = "legacy-http";
    tLegacy.sSummary = "Bounded legacy Streamable HTTP fixture.";
    tLegacy.eTransport = XWORK_MCP_TRANSPORT_STREAMABLE_HTTP;
    tLegacy.sEndpoint = sLegacyEndpoint;
    tLegacy.sProtocolVersion = "2025-11-25";
    tLegacy.pHttpHeaders = &tLegacyHeader;
    tLegacy.iHttpHeaderCount = 1u;
    tLegacy.uStartupTimeoutMs = 3000u;
    tLegacy.uRequestTimeoutMs = 3000u;
    CHECK(xworkRuntimeRegisterMcpServer(pRuntime, &tLegacy, &tError) &&
              xworkRuntimeRefreshMcpServer(pRuntime, "legacy-http", NULL, XLLM_DEADLINE_NEVER,
                                           &tError),
          "legacy HTTP session initializes and resumes an SSE response");
    pLegacy = xwork__runtime_mcp_find(pRuntime, "legacy-http");
    xworkToolOutputInit(&tOutput);
    CHECK(pLegacy && pLegacy->sHttpSessionId &&
              xworkMcpClientCallTool(pLegacy, "echo", "{\"text\":\"legacy\"}", NULL,
                                     XLLM_DEADLINE_NEVER, &tOutput, &tError) == XWORK_RESULT_OK &&
              tOutput.bSuccess && tOutput.sContent && strstr(tOutput.sContent, "legacy http ok"),
          "legacy HTTP tool call carries the negotiated session id");
    xworkToolOutputUnit(&tOutput);
    CHECK(xworkRuntimeDisconnectMcpServer(pRuntime, "legacy-http", &tError) &&
              xworkRuntimeMcpServerGetInfo(pRuntime, "legacy-http", &tInfo) &&
              tInfo.eState == XWORK_MCP_SERVER_DISCONNECTED && !tInfo.bConnected &&
              pLegacy->sHttpSessionId == NULL,
          "legacy HTTP disconnect terminates and clears its server session");

    xworkRuntimeRelease(pRuntime);
    printf("xwork MCP Streamable HTTP: %s (%d failures)\n", g_iFailures ? "FAIL" : "PASS",
           g_iFailures);
    return g_iFailures ? 1 : 0;
}

enum
{
    TEST_MCP_NORMAL = 0,
    TEST_MCP_LAZY,
    TEST_MCP_MODERN,
    TEST_MCP_MALFORMED,
    TEST_MCP_REPEATED_CURSOR,
    TEST_MCP_EXIT,
    TEST_MCP_SLOW_START
};

static int run_mcp_test_server(int iMode)
{
    char sLine[65536];
    bool bLazy = iMode == TEST_MCP_LAZY;
    bool bModern = iMode == TEST_MCP_MODERN;
    bool bInitialized = bModern;
    if (bLazy)
    {
        const char *sFixture = getenv("XWORK_MCP_LAZY_FIXTURE");
        if (!sFixture || strcmp(sFixture, "enabled") != 0)
            return 4;
    }
    while (fgets(sLine, sizeof(sLine), stdin))
    {
        const char *sIdText = strstr(sLine, "\"id\":");
        unsigned long long uId = sIdText ? strtoull(sIdText + 5u, NULL, 10) : 0u;
        if (bModern && !strstr(sLine, "\"io.modelcontextprotocol/protocolVersion\":\"2026-07-28\""))
        {
            printf("{\"jsonrpc\":\"2.0\",\"id\":%llu,\"error\":{\"code\":-32602,\"message\":"
                   "\"missing modern metadata\"}}\n",
                   uId);
            fflush(stdout);
        }
        else if (strstr(sLine, "\"method\":\"server/discover\""))
        {
            printf("{\"jsonrpc\":\"2.0\",\"id\":%llu,\"result\":{\"resultType\":\"complete\","
                   "\"supportedVersions\":[\"2026-07-28\"],\"capabilities\":{\"tools\":{}},"
                   "\"ttlMs\":60000,\"cacheScope\":\"private\"}}\n",
                   uId);
            fflush(stdout);
        }
        else if (strstr(sLine, "\"method\":\"initialize\""))
        {
            if (iMode == TEST_MCP_SLOW_START)
                mdo_test_sleep_ms(100u);
            printf("{\"jsonrpc\":\"2.0\",\"id\":%llu,\"result\":{\"protocolVersion\":\"2025-11-"
                   "25\",\"capabilities\":{\"tools\":{\"listChanged\":false}},\"serverInfo\":{"
                   "\"name\":\"xwork-test-mcp\",\"version\":\"1.0\"}}}\n",
                   uId);
            fflush(stdout);
        }
        else if (strstr(sLine, "\"method\":\"notifications/initialized\""))
        {
            bInitialized = true;
        }
        else if (strstr(sLine, "\"method\":\"tools/list\"") && !bInitialized)
        {
            printf("{\"jsonrpc\":\"2.0\",\"id\":%llu,\"error\":{\"code\":-32002,\"message\":\"not "
                   "initialized\"}}\n",
                   uId);
            fflush(stdout);
        }
        else if (strstr(sLine, "\"method\":\"tools/list\""))
        {
            if (iMode == TEST_MCP_MALFORMED)
            {
                printf("{malformed\n");
            }
            else if (iMode == TEST_MCP_REPEATED_CURSOR)
            {
                printf("{\"jsonrpc\":\"2.0\",\"id\":%llu,\"result\":{\"tools\":[],\"nextCursor\":"
                       "\"repeat\"}}\n",
                       uId);
            }
            else if (iMode == TEST_MCP_EXIT)
            {
                return 0;
            }
            else if (bModern)
            {
                printf("{\"jsonrpc\":\"2.0\",\"id\":%llu,\"result\":{\"resultType\":\"complete\","
                       "\"tools\":[{\"name\":\"echo\",\"description\":\"Modern echo test "
                       "text.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"text\":{"
                       "\"type\":\"string\"}},\"required\":[\"text\"],\"additionalProperties\":"
                       "false}}],\"ttlMs\":60000,\"cacheScope\":\"private\"}}\n",
                       uId);
            }
            else if (bLazy)
            {
                printf(
                    "{\"jsonrpc\":\"2.0\",\"id\":%llu,\"result\":{\"tools\":[{\"name\":\"echo\","
                    "\"description\":\"Echo test "
                    "text.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"text\":{"
                    "\"type\":\"string\"}},\"required\":[\"text\"],\"additionalProperties\":false},"
                    "\"annotations\":{\"readOnlyHint\":true}},{\"name\":\"hidden\",\"description\":"
                    "\"Filtered test tool.\",\"inputSchema\":{\"type\":\"object\"}}]}}\n",
                    uId);
            }
            else
            {
                printf("{\"jsonrpc\":\"2.0\",\"id\":%llu,\"result\":{\"tools\":[{\"name\":\"echo\","
                       "\"description\":\"Echo test "
                       "text.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"text\":{"
                       "\"type\":\"string\"}},\"required\":[\"text\"],\"additionalProperties\":"
                       "false},\"annotations\":{\"readOnlyHint\":true}}]}}\n",
                       uId);
            }
            fflush(stdout);
        }
        else if (strstr(sLine, "\"method\":\"tools/call\""))
        {
            if (strstr(sLine, "\"delay\":true"))
                mdo_test_sleep_ms(3000u);
            printf("{\"jsonrpc\":\"2.0\",\"id\":%llu,\"result\":{%s\"content\":[{\"type\":\"text\","
                   "\"text\":\"echo: hello "
                   "mcp\"}],\"structuredContent\":{\"echoed\":true},\"isError\":false}}\n",
                   uId, bModern ? "\"resultType\":\"complete\"," : "");
            fflush(stdout);
        }
    }
    return ferror(stdin) ? 1 : 0;
}

static void test_user_image_run(void)
{
    static const unsigned char sImage[] = {0x89u, 'P', 'N', 'G'};
    static const char sWorkspace[] = "tests/tmp_xwork_user_image";
    xwork_runtime_config tRuntimeConfig;
    xwork_agent_definition_config tDefinitionConfig;
    xwork_agent_options tOptions;
    xwork_run_config tRunConfig;
    xwork_run_result tResult;
    xllm_session_config tSessionConfig;
    xwork_runtime *pRuntime = NULL;
    xwork_agent_definition *pDefinition = NULL;
    xllm_session *pSession = NULL;
    xwork_agent *pAgent = NULL;
    xwork_run *pRun = NULL;
    xllm_message tMessage;
    xllm_part tText;
    xllm_part tImage;
    xwork_error tError;
    user_image_fixture tFixture = {false, 0u};

    (void)xrtDirRemoveAll(sWorkspace);
    CHECK(xrtDirCreateAll((str)sWorkspace), "image run workspace created");
    xworkRuntimeConfigInit(&tRuntimeConfig);
    pRuntime = xworkRuntimeCreate(&tRuntimeConfig, &tError);
    xworkAgentDefinitionConfigInit(&tDefinitionConfig);
    tDefinitionConfig.sId = "test.user-image";
    tDefinitionConfig.bRegisterBuiltinTools = false;
    tDefinitionConfig.bAutoSaveSession = false;
    tDefinitionConfig.bRequireVerificationAfterWrite = false;
    pDefinition = xworkAgentDefinitionCreate(&tDefinitionConfig, &tError);
    xllmSessionConfigInit(&tSessionConfig);
    pSession = xllmSessionCreate(&tSessionConfig, NULL);
    xworkAgentOptionsInit(&tOptions);
    tOptions.pSession = pSession;
    tOptions.sWorkspaceRoot = sWorkspace;
    tOptions.OnModelComplete = user_image_complete;
    tOptions.pModelUserData = &tFixture;
    pAgent = pRuntime && pDefinition && pSession
                 ? xworkAgentCreateWithRuntime(pRuntime, pDefinition, &tOptions, &tError)
                 : NULL;
    CHECK(pAgent != NULL, "image run Agent creates");

    xllmMessageInit(&tMessage, XLLM_ROLE_USER);
    xllmPartInit(&tText, XLLM_PART_TEXT);
    xllmPartInit(&tImage, XLLM_PART_IMAGE);
    CHECK(xllmPartSetText(&tText, "Describe this image") &&
              xllmPartSetImageData(&tImage, sImage, sizeof(sImage), "image/png") &&
              xllmMessageAddPart(&tMessage, &tText) && xllmMessageAddPart(&tMessage, &tImage),
          "user image message constructs");
    xllmPartUnit(&tText);
    xllmPartUnit(&tImage);
    xworkRunConfigInit(&tRunConfig);
    tRunConfig.sPrompt = "mismatched text";
    CHECK(!xworkRunCreateWithUserMessage(pAgent, &tRunConfig, &tMessage, &tError) &&
              tError.eCode == XWORK_ERROR_INVALID_ARGUMENT,
          "image run rejects text that differs from its event projection");
    tRunConfig.sPrompt = "Describe this image";
    pRun = pAgent ? xworkRunCreateWithUserMessage(pAgent, &tRunConfig, &tMessage, &tError) : NULL;
    xllmMessageUnit(&tMessage);
    CHECK(pRun != NULL, "image run takes an owned user message copy");
    memset(&tResult, 0, sizeof(tResult));
    CHECK(pRun && xworkRunStart(pRun, &tError) &&
              xworkRunWait(pRun, xllmDeadlineAfterUs(UINT64_C(5000000)), &tResult, &tError) ==
                  XWORK_RESULT_OK &&
              tFixture.uCalls == 1u && tFixture.bSawImage,
          "async image run keeps bytes after caller releases its message");
    xworkRunResultUnit(&tResult);
    xworkRunDestroy(pRun);
    xworkAgentDestroy(pAgent);
    xllmSessionDestroy(pSession);
    xworkAgentDefinitionRelease(pDefinition);
    xworkRuntimeRelease(pRuntime);
    (void)xrtDirRemoveAll(sWorkspace);
}

static void test_resume_workspace_effects(void)
{
    xllm_session_config config;
    xwork_agent_config agent_config;
    xllm_error llm_error;
    xwork_error error;
    xllm_session *session;
    xwork_agent *agent;
    t1_script script;
    xwork_run_result result;
    xllm_message message;
    uint64_t turn;
    bool changed = true;
    memset(&script, 0, sizeof(script));
    script.bFinalOnly = true;
    xllmSessionConfigInit(&config);
    session = xllmSessionCreateForTest(&config, t1_script_call, &script, &llm_error);
    xworkAgentConfigInit(&agent_config);
    agent_config.pSession = session;
    agent_config.sWorkspaceRoot = ".";
    agent = xworkAgentCreate(&agent_config, &error);
    CHECK(session && agent, "resume effects fixture created");
    if (!session || !agent)
        goto done;
    turn = xllmSessionBeginTurn(session);
    CHECK(xllmSessionAddText(session, turn, XLLM_ROLE_USER, "Hello", 0u),
          "interrupted chat retained");
    CHECK(xwork__resume_may_have_written(agent, &changed, &error) && !changed,
          "plain chat recovery does not imply a write");
    memset(&result, 0, sizeof(result));
    CHECK(xworkAgentResume(agent, &result, &error) == XWORK_RESULT_OK && result.uModelCalls == 1u,
          "plain chat resumes to completion without verification tools");
    xworkRunResultUnit(&result);
    turn = xllmSessionBeginTurn(session);
    CHECK(xllmSessionAddText(session, turn, XLLM_ROLE_USER, "Inspect a file", 0u),
          "new request retained");
    xllmMessageInit(&message, XLLM_ROLE_ASSISTANT);
    CHECK(xllmMessageAddToolCall(&message, "read-call", "read", "{\"path\":\"x\"}") &&
              xllmSessionAddMessage(session, turn, &message, 0u),
          "read call retained");
    xllmMessageUnit(&message);
    CHECK(xwork__resume_may_have_written(agent, &changed, &error) && !changed,
          "pending read recovery does not imply a write");
    CHECK(xllmSessionAddToolResult(session, turn, "read-call", "read complete"),
          "read result retained");
    CHECK(xwork__resume_may_have_written(agent, &changed, &error) && !changed,
          "completed read recovery does not imply a write");
    xllmMessageInit(&message, XLLM_ROLE_ASSISTANT);
    CHECK(xllmMessageAddToolCall(&message, "write-call", "write",
                                 "{\"path\":\"x\",\"content\":\"y\"}") &&
              xllmSessionAddMessage(session, turn, &message, 0u),
          "write call retained");
    xllmMessageUnit(&message);
    CHECK(xwork__resume_may_have_written(agent, &changed, &error) && changed,
          "pending writes retain verification");
    CHECK(xllmSessionAddToolResult(session, turn, "write-call", "write complete") &&
              xllmSessionAddText(session, turn, XLLM_ROLE_USER,
                                 "Completion verification gate: check",
                                 XLLM_SESSION_ENTRY_SYNTHETIC),
          "synthetic verification boundary retained");
    CHECK(xwork__resume_may_have_written(agent, &changed, &error) && changed,
          "synthetic verification prompt does not hide prior writes");
    turn = xllmSessionBeginTurn(session);
    CHECK(xllmSessionAddText(session, turn, XLLM_ROLE_USER, "New unrelated request", 0u),
          "next request retained");
    CHECK(xwork__resume_may_have_written(agent, &changed, &error) && !changed,
          "writes in older requests do not contaminate a new request");
done:
    xworkAgentDestroy(agent);
    xllmSessionDestroy(session);
}

/* Follow the actual model instruction, so this catches a conflicting system
 * format even when the user-side compaction prompt names the correct style. */
static xllm_result style_summary_complete(void *Data, const xllm_request *Request,
                                          const xllm_stream_callbacks *Callbacks,
                                          xllm_response **Response, xllm_error *Error)
{
    const char *Coding =
        "Goal: preserve the original coding task and the exact pending next action.\n"
        "Constraints & Preferences: keep all completed work and stay within the original workspace "
        "boundaries.\n"
        "Progress: prior synthetic turns completed successfully and no tool calls are currently "
        "unresolved.\n"
        "Key Decisions: retain the original files, commands and verification evidence without "
        "inventing outcomes.\n"
        "Next Steps: continue the latest user request after committing this valid summary "
        "checkpoint.\n"
        "Critical Context: this is a synthetic local test with no production credentials or file "
        "mutations.\n";
    const char *General =
        "Goal: preserve the original conversation and the exact pending next action.\n"
        "Constraints: keep all established preferences and facts without inventing any completed "
        "work.\n"
        "Progress: prior synthetic turns completed successfully and no tool calls are currently "
        "unresolved.\n"
        "Key Decisions: retain the original facts, constraints and verification evidence for the "
        "next turn.\n"
        "Next Steps: continue the latest user request after committing this valid summary "
        "checkpoint.\n"
        "Critical Context: this is a synthetic local test with no production credentials or file "
        "mutations.\n";
    const char *Durable =
        "Objective: preserve the original task and the exact pending next action.\n"
        "Constraints: keep completed work and obey the original workspace boundaries.\n"
        "Architecture and decisions: retain the same session and safe completed prefix.\n"
        "Completed work: prior synthetic turns completed with no unresolved tool calls.\n"
        "Current repository state: no production files or credentials were changed.\n"
        "Verification evidence: the old turns contain only completed synthetic user and assistant "
        "messages.\n"
        "Open issues and risks: the latest request remains pending and is not claimed complete.\n"
        "Exact next actions: commit this valid summary checkpoint and continue the user's "
        "request.\n";
    const char *Text = request_has_text(Request, "## Constraints & Preferences") ? Coding
                       : request_has_text(Request, "## Goal")                    ? General
                                                                                 : Durable;
    (void)Data;
    (void)Callbacks;
    (void)Error;
    if (strstr(Request->pMessages[0].sContent,
               "Return exactly these populated headings: Objective"))
        Text = Durable;
    *Response = mock_response(Text, 0u);
    return *Response ? XLLM_RESULT_OK : XLLM_RESULT_ERROR;
}

static void test_task_stop_failure(void)
{
    xwork_runtime *pRuntime;
    xwork_process_entry *pEntry;
    xprocessconfig tProcess;
    const char *arrArgv[] = {g_sSelfPath, "--task-cancel-child", NULL};
    xwork_error tError;
    xwork_task_snapshot *pBefore;
    xwork_task_snapshot *pAfter;
    xwork_task_info tInfo;
    xllm_session_config tSessionConfig;
    xllm_session *pSession;
    xllm_error tLlmError;
    xwork_agent_config tAgentConfig;
    xwork_agent *pAgent;
    xwork_tool_context tContext;
    xwork_tool_output tOutput;
    char sStopArgs[128];
    xllmSessionConfigInit(&tSessionConfig);
    pSession = xllmSessionCreate(&tSessionConfig, &tLlmError);
    xworkAgentConfigInit(&tAgentConfig);
    tAgentConfig.pSession = pSession;
    tAgentConfig.sWorkspaceRoot = ".";
    pAgent = xworkAgentCreate(&tAgentConfig, &tError);
    pRuntime = pAgent ? xworkRuntimeRef(pAgent->pRuntime) : NULL;
    pEntry = pRuntime ? xwork__runtime_task_allocate(pRuntime, 0u, &tError) : NULL;
    xrtProcessConfigInit(&tProcess);
    tProcess.Program = arrArgv[0];
    tProcess.Arg0 = arrArgv[0];
    tProcess.Args = arrArgv + 1;
    tProcess.ArgCount = 1u;
    tProcess.NewGroup = true;
    tProcess.HideWindow = true;
    tProcess.Stdin.Mode = XPROCESS_IO_PIPE;
    if (pEntry)
    {
        pEntry->eKind = XWORK_TASK_PROCESS;
        pEntry->uOwnerAgentId = pAgent ? pAgent->uAgentId : 0u;
        pEntry->pProcess = xrtProcessSpawn(&tProcess);
        xwork__task_set_state_locked(pEntry, XWORK_TASK_RUNNING);
        xwork__atomic_store(&pEntry->iReady, 1);
    }
    CHECK(pEntry && pEntry->pProcess, "bounded process stop fixture creates");
    pBefore = pRuntime ? xworkRuntimeTaskSnapshot(pRuntime, 0u, &tError) : NULL;
    g_bProcessStopFailure = true;
    CHECK(pEntry && !xworkRuntimeCancelTask(pRuntime, pEntry->uId, &tError) &&
              tError.eCode == XWORK_ERROR_IO,
          "failed process stop keeps its precise failure");
    memset(&tContext, 0, sizeof(tContext));
    tContext.pAgent = pAgent;
    (void)snprintf(sStopArgs, sizeof(sStopArgs),
                   "{\"task_id\":%llu,\"mode\":\"kill_tree\",\"wait_ms\":0,\"release\":false}",
                   (unsigned long long)(pEntry ? pEntry->uId : 0u));
    xworkToolOutputInit(&tOutput);
    CHECK(pAgent && pEntry &&
              xwork__tool_stop(pAgent, &tContext, sStopArgs, &tOutput, &tError) ==
                  XWORK_RESULT_OK &&
              !tOutput.bSuccess,
          "model stop tool reports the same unaccepted process stop");
    xworkToolOutputUnit(&tOutput);
    g_bProcessStopFailure = false;
    pAfter = pRuntime ? xworkRuntimeTaskSnapshot(pRuntime, 0u, &tError) : NULL;
    CHECK(pEntry && pAfter && !xworkTaskSnapshotStopRequested(pAfter, pEntry->uId) &&
              xworkTaskSnapshotFind(pAfter, pEntry->uId, &tInfo) &&
              tInfo.eState == XWORK_TASK_RUNNING,
          "failed process stop does not falsely publish stopping or terminal state");
    xworkTaskSnapshotRelease(pAfter);
    CHECK(!xworkTaskSnapshotStopRequested(NULL, 1u) &&
              !xworkTaskSnapshotStopRequested(pBefore, 0u) &&
              !xworkTaskSnapshotStopRequested(pBefore, UINT64_MAX),
          "empty and unknown task stop queries are false");
    CHECK(pEntry && xworkRuntimeCancelTask(pRuntime, pEntry->uId, &tError),
          "explicit retry can stop the unchanged process");
    pAfter = pRuntime ? xworkRuntimeTaskSnapshot(pRuntime, 0u, &tError) : NULL;
    CHECK(pEntry && pAfter && xworkTaskSnapshotStopRequested(pAfter, pEntry->uId) &&
              !xworkTaskSnapshotStopRequested(pBefore, pEntry->uId),
          "successful process stop is captured only in the new snapshot");
    xworkTaskSnapshotRelease(pAfter);
    xworkTaskSnapshotRelease(pBefore);
    xwork__task_entry_release(pEntry);
    xworkAgentDestroy(pAgent);
    xllmSessionDestroy(pSession);
    xworkRuntimeRelease(pRuntime);
}
static void test_compaction_styles(void)
{
    const char *pStyles[] = {NULL, "general", "durable"};
    char sHistory[2001];
    size_t i;
    memset(sHistory, 'x', sizeof(sHistory) - 1u);
    sHistory[sizeof(sHistory) - 1u] = '\0';
    for (i = 0u; i < sizeof(pStyles) / sizeof(pStyles[0]); ++i)
    {
        xllm_session_config tConfig;
        xllm_error tModelError;
        xwork_error tError;
        xwork_agent_config tAgentConfig;
        xllm_session *pSession;
        xwork_agent *pAgent;
        xllm_session_summary tSummary;
        xwork_result eResult;
        unsigned int uTurn;
        xllmSessionConfigInit(&tConfig);
        tConfig.sSummaryStyle = pStyles[i];
        tConfig.uContextWindowTokens = 8000u;
        tConfig.uMaxOutputTokens = 600u;
        tConfig.uSafetyReserveTokens = 100u;
        tConfig.uSummaryMaxTokens = 400u;
        tConfig.uKeepRecentTokens = 1000u;
        pSession = xllmSessionCreate(&tConfig, &tModelError);
        CHECK(pSession != NULL, "summary style session created");
        if (!pSession)
            continue;
        for (uTurn = 0u; uTurn < 3u; ++uTurn)
        {
            uint64_t uId = xllmSessionBeginTurn(pSession);
            CHECK(uId && xllmSessionAddText(pSession, uId, XLLM_ROLE_USER, sHistory, 0u) &&
                      xllmSessionAddText(pSession, uId, XLLM_ROLE_ASSISTANT, sHistory, 0u),
                  "summary style completed turn seeded");
        }
        xworkAgentConfigInit(&tAgentConfig);
        tAgentConfig.pSession = pSession;
        tAgentConfig.sModel = "mock-model";
        tAgentConfig.sWorkspaceRoot = ".";
        tAgentConfig.OnModelComplete = style_summary_complete;
        pAgent = xworkAgentCreate(&tAgentConfig, &tError);
        eResult = pAgent ? xworkAgentCompact(pAgent, &tError) : XWORK_RESULT_ERROR;
        if (eResult != XWORK_RESULT_OK)
        {
            fprintf(stderr, "summary style %s: %s\n", pStyles[i] ? pStyles[i] : "coding",
                    tError.sMessage);
        }
        CHECK(eResult == XWORK_RESULT_OK,
              pStyles[i] ? pStyles[i] : "default coding summary honors its own style");
        CHECK(xllmSessionGetSummary(pSession, &tSummary) && tSummary.sText && tSummary.sText[0],
              "style summary checkpoint committed");
        xworkAgentDestroy(pAgent);
        xllmSessionDestroy(pSession);
    }
}

static unsigned g_ResultFileCalls;
static bool g_ResultFileSinkFail;
static char g_ResultFileLocator[300];
static char *g_ResultFileText;

static bool result_file_event(void *Data, const xwork_event *Event)
{
    (void)Data;
    if (g_ResultFileSinkFail && Event->eKind == XWORK_EVENT_TOOL_DONE)
    {
        xrtSetErrorKind(XERR_IO);
        return false;
    }
    return true;
}

static xwork_result result_file_describe(void *Data, const xwork_tool_context *Context,
                                         const char *Arguments,
                                         xwork_permission_resource_writer *Writer,
                                         xwork_error *Error)
{
    (void)Data;
    (void)Context;
    (void)Arguments;
    (void)Error;
    return xworkPermissionResourceWriterAdd(Writer, XWORK_RESOURCE_NETWORK,
                                            XWORK_RESOURCE_ACCESS_CONNECT, "https://example.com")
               ? XWORK_RESULT_OK
               : XWORK_RESULT_ERROR;
}

static xwork_permission_decision result_file_permission(void *Data,
                                                        const xwork_permission_request *Request)
{
    (void)Data;
    (void)Request;
    return XWORK_PERMISSION_ALLOW;
}

static xwork_result result_file_execute(void *Data, const xwork_tool_context *Context,
                                        const char *Arguments, xwork_tool_result_writer *Writer,
                                        xwork_error *Error)
{
    xwork_artifact_info Info;
    char *Locator;
    (void)Data;
    (void)Arguments;
    ++g_ResultFileCalls;
    xworkArtifactInfoInit(&Info);
    if (!xworkToolResultWriterSaveArtifact(Writer, Context, g_ResultFileText,
                                           strlen(g_ResultFileText), "text/markdown", &Info, Error))
        return XWORK_RESULT_ERROR;
    Locator = xwork__result_locator(Context->pAgent, Info.sPath);
    if (!Locator)
        return XWORK_RESULT_ERROR;
    snprintf(g_ResultFileLocator, sizeof(g_ResultFileLocator), "%s", Locator);
    free(Locator);
    return xworkToolResultWriterWriteText(Writer, "saved result; use read on the result file") &&
                   xworkToolResultWriterSetSuccess(Writer, true)
               ? XWORK_RESULT_OK
               : XWORK_RESULT_ERROR;
}

static void test_result_file_pages(void)
{
    const char *Base = "tests/tmp_xwork_result_pages";
    xwork_runtime_config RuntimeConfig;
    xwork_runtime *Runtime = NULL;
    xwork_agent_definition_config DefinitionConfig;
    xwork_agent_definition *Definition = NULL;
    xwork_agent_options Options;
    xllm_session_config SessionConfig;
    xllm_session *Session = NULL;
    xwork_agent *Agent = NULL;
    xwork_artifact_store *Store = NULL;
    xroot Root = NULL;
    xwork_tool_definition Tool = {0};
    xllm_tool_call Call = {0};
    xwork_error Error;
    char *Content = NULL;
    bool Success = false;
    uint32_t ModelCalls = 0u;
    size_t i;
    xwork_tool_output Page;
    char Args[512];
    (void)xrtDirRemoveAll(Base);
    CHECK(xrtDirCreateAll("tests/tmp_xwork_result_pages/workspace") &&
              xrtDirCreateAll("tests/tmp_xwork_result_pages/storage"),
          "paged result fixture directories create");
    g_ResultFileText = (char *)malloc(18001u);
    if (!g_ResultFileText)
        return;
    for (i = 0u; i < 18000u; i += 12u)
        memcpy(g_ResultFileText + i, "\xe6\x90\x9c\xe7\xb4\xa2\xd1\x8f\xf0\x9f\x8c\x8d", 12u);
    g_ResultFileText[18000] = 0;
    g_ResultFileCalls = 0u;
    Root = xrtRootOpen("tests/tmp_xwork_result_pages/storage");
    Store = xworkArtifactStoreCreate(Root, &Error);
    if (Root)
        (void)xrtRootClose(Root);
    xworkRuntimeConfigInit(&RuntimeConfig);
    Runtime = xworkRuntimeCreate(&RuntimeConfig, &Error);
    Tool.sName = "fixture.search";
    Tool.sDescription = "Read search fixture";
    Tool.sParametersJson = "{\"type\":\"object\"}";
    Tool.uEffects = XWORK_TOOL_EFFECT_READ | XWORK_TOOL_EFFECT_NETWORK;
    Tool.OnDescribePermissions = result_file_describe;
    Tool.OnExecuteV2 = result_file_execute;
    CHECK(Runtime && xworkRuntimeRegisterTool(Runtime, &Tool, &Error) &&
              xworkRuntimeConfigureToolRecovery(Runtime, Tool.sName, true, &Error),
          "host declares read-operation recovery independently of network effects");
    xworkAgentDefinitionConfigInit(&DefinitionConfig);
    Definition = xworkAgentDefinitionCreate(&DefinitionConfig, &Error);
    xllmSessionConfigInit(&SessionConfig);
    Session = xllmSessionCreate(&SessionConfig, NULL);
    xworkAgentOptionsInit(&Options);
    Options.pSession = Session;
    Options.sWorkspaceRoot = "tests/tmp_xwork_result_pages/workspace";
    Options.pArtifactStore = Store;
    Options.OnModelComplete = v3_final_complete;
    Options.OnPermission = result_file_permission;
    Options.OnEvent = result_file_event;
    Options.pModelUserData = &ModelCalls;
    Agent = Runtime && Definition && Store && Session
                ? xworkAgentCreateWithRuntime(Runtime, Definition, &Options, &Error)
                : NULL;
    Call.sId = "persistent-result-call";
    Call.sName = "fixture.search";
    Call.sArgumentsJson = "{}";
    CHECK(Agent &&
              xwork__execute_tool(Agent, &Call, 1u, NULL, XLLM_DEADLINE_NEVER, &Content, &Success,
                                  NULL, NULL, NULL, NULL, &Error) == XWORK_RESULT_OK &&
              Success && g_ResultFileCalls == 1u && strlen(Content) < 256u,
          "full multilingual search is stored with a small model result");
    free(Content);
    Content = NULL;
    CHECK(Agent &&
              xwork__execute_tool(Agent, &Call, 1u, NULL, XLLM_DEADLINE_NEVER, &Content, &Success,
                                  NULL, NULL, NULL, NULL, &Error) == XWORK_RESULT_OK &&
              g_ResultFileCalls == 1u,
          "same completed call reuses its receipt without another request");
    free(Content);
    Content = NULL;
    xworkAgentDestroy(Agent);
    xworkRuntimeRelease(Runtime);
    Runtime = xworkRuntimeCreate(&RuntimeConfig, &Error);
    CHECK(Runtime && xworkRuntimeRegisterTool(Runtime, &Tool, &Error) &&
              xworkRuntimeConfigureToolRecovery(Runtime, Tool.sName, true, &Error),
          "receipt fixture recreates its runtime catalog");
    xworkArtifactStoreRelease(Store);
    Root = xrtRootOpen("tests/tmp_xwork_result_pages/storage");
    Store = xworkArtifactStoreCreate(Root, &Error);
    if (Root)
        (void)xrtRootClose(Root);
    Options.pArtifactStore = Store;
    Agent = xworkAgentCreateWithRuntime(Runtime, Definition, &Options, &Error);
    CHECK(Agent &&
              xwork__execute_tool(Agent, &Call, 1u, NULL, XLLM_DEADLINE_NEVER, &Content, &Success,
                                  NULL, NULL, NULL, NULL, &Error) == XWORK_RESULT_OK &&
              g_ResultFileCalls == 1u,
          "rebuilt runtime and reopened store reuse the durable completion receipt");
    free(Content);
    Content = NULL;
    xworkToolOutputInit(&Page);
    snprintf(Args, sizeof(Args), "{\"path\":\"%s\",\"max_bytes\":512}", g_ResultFileLocator);
    CHECK(Agent && xwork__tool_read(Agent, NULL, Args, &Page, &Error) == XWORK_RESULT_OK &&
              Page.bSuccess && strlen(Page.sContent) <= 512u && strstr(Page.sContent, "offset=") &&
              xrtUtf8Valid(xrtStrView(Page.sContent), NULL),
          "one long multilingual line is paged within the byte budget");
    xworkToolOutputUnit(&Page);
    {
        xllm_tool_call ReadCall = {0};
        xllm_executor_ctx Execution = {0};
        xwork_tool_batch_plan *Plan;
        ReadCall.sId = "planned-result-read";
        ReadCall.sName = "read";
        ReadCall.sArgumentsJson = Args;
        Execution.uRound = 1u;
        Execution.uDeadline = XLLM_DEADLINE_NEVER;
        Plan = xworkAgentPlanToolBatch(Agent, &ReadCall, 1u, &Execution, &Error);
        CHECK(Plan && !Plan->pEntries[0].bDescriptorFailed,
              "batch planner accepts a current-store result read without a workspace mount");
        xworkToolBatchPlanRelease(Plan);
    }
    {
        uint64_t Cursor = 0u;
        size_t Bytes = 0u;
        bool Exact = true;
        while (Cursor < 18000u && Exact)
        {
            char *Start;
            char *End;
            char *Next;
            snprintf(Args, sizeof(Args), "{\"path\":\"%s\",\"offset\":%llu,\"max_bytes\":512}",
                     g_ResultFileLocator, (unsigned long long)Cursor);
            Exact = xwork__tool_read(Agent, NULL, Args, &Page, &Error) == XWORK_RESULT_OK &&
                    Page.bSuccess;
            if (!Exact)
                break;
            Start = strchr(Page.sContent, '\n');
            End = Start ? strstr(Start + 1u, "\n[") : NULL;
            Next = End ? strstr(End, "offset=") : NULL;
            if (!Start || !End || !xrtUtf8Valid(xrtStrView(Page.sContent), NULL))
                Exact = false;
            else
            {
                size_t Part = (size_t)(End - Start - 1u);
                if (Bytes + Part > 18000u || memcmp(Start + 1u, g_ResultFileText + Bytes, Part))
                    Exact = false;
                Bytes += Part;
                uint64_t Following = Next ? strtoull(Next + 7u, NULL, 10) : 18000u;
                if (Following <= Cursor)
                    Exact = false;
                Cursor = Following;
            }
            xworkToolOutputUnit(&Page);
        }
        CHECK(Exact && Bytes == 18000u,
              "all UTF-8 pages reconstruct the complete long line without overlap or omission");
    }
    snprintf(Args, sizeof(Args), "{\"path\":\"%s\",\"offset\":1,\"max_bytes\":512}",
             g_ResultFileLocator);
    CHECK(xwork__tool_read(Agent, NULL, Args, &Page, &Error) == XWORK_RESULT_OK && Page.bSuccess && Page.sContent && xrtUtf8Valid(xrtStrView(Page.sContent), NULL),
          "byte cursors inside a scalar align to valid UTF-8 output");
    xworkToolOutputUnit(&Page);
    {
        xwork_tool_definition WriteTool = Tool;
        WriteTool.sName = "fixture.write";
        WriteTool.uEffects = XWORK_TOOL_EFFECT_WORKSPACE_WRITE;
        CHECK(xworkRuntimeRegisterTool(Runtime, &WriteTool, &Error) &&
                  !xworkRuntimeConfigureToolRecovery(Runtime, WriteTool.sName, true, &Error),
              "mutation tools cannot opt into automatic read recovery");
    }
    Call.sId = "receipt-before-journal";
    g_ResultFileSinkFail = true;
    CHECK(xwork__execute_tool(Agent, &Call, 1u, NULL, XLLM_DEADLINE_NEVER, &Content, &Success, NULL,
                              NULL, NULL, NULL, &Error) == XWORK_RESULT_ERROR &&
              Error.eCode == XWORK_ERROR_IO && g_ResultFileCalls == 2u,
          "journal failure after completion is IO failure, not a user stop");
    free(Content);
    Content = NULL;
    g_ResultFileSinkFail = false;
    xworkAgentDestroy(Agent);
    Agent = xworkAgentCreateWithRuntime(Runtime, Definition, &Options, &Error);
    CHECK(Agent &&
              xwork__execute_tool(Agent, &Call, 1u, NULL, XLLM_DEADLINE_NEVER, &Content, &Success,
                                  NULL, NULL, NULL, NULL, &Error) == XWORK_RESULT_OK &&
              g_ResultFileCalls == 2u,
          "result committed before failed journal is reused without another request");
    free(Content);
    Content = NULL;
    CHECK(xwork__tool_read(Agent, NULL, "{\"path\":\"@results/../outside\"}", &Page, &Error) ==
                  XWORK_RESULT_OK &&
              !Page.bSuccess,
          "result paths do not escape the session store");
    xworkToolOutputUnit(&Page);
    if (Agent)
    {
        xfile File = xwork__result_file_open(Agent, g_ResultFileLocator);
        char *Absolute = xrtPathJoin(Agent->pArtifactStore->sDirectory,
                                     xwork__result_file_relative(g_ResultFileLocator));
        if (File)
            (void)xrtClose(File);
        CHECK(Absolute && xrtFileWriteAtomic(Absolute, (xbytesview){(const uint8 *)"changed", 7u}),
              "receipt corruption fixture changes the stored file");
        xrtFree(Absolute);
        CHECK(xwork__execute_tool(Agent, &Call, 1u, NULL, XLLM_DEADLINE_NEVER, &Content, &Success,
                                  NULL, NULL, NULL, NULL, &Error) != XWORK_RESULT_OK &&
                  Error.eCode == XWORK_ERROR_IO && g_ResultFileCalls == 2u,
              "damaged results fail explicitly instead of silently billing another request");
    }
    free(Content);
    xworkAgentDestroy(Agent);
    xllmSessionDestroy(Session);
    xworkAgentDefinitionRelease(Definition);
    xworkRuntimeRelease(Runtime);
    xworkArtifactStoreRelease(Store);
    free(g_ResultFileText);
    g_ResultFileText = NULL;
    (void)xrtDirRemoveAll(Base);
}

static void test_utf8_tool_output(void)
{
    /* Offset every boundary through Chinese, Cyrillic and four-byte scalars.
     * This is a deterministic reproduction of large search result previews. */
    static const char Pattern[] = "\xe6\x90\x9c\xe7\xb4\xa2\xd1\x8f\xf0\x9f\x8c\x8d";
    char Text[18001];
    size_t i, Limit;
    xwork_agent Agent = {0};
    for (i = 0u; i < 18000u; ++i)
        Text[i] = Pattern[i % (sizeof(Pattern) - 1u)];
    Text[18000] = '\0';
    CHECK(xrtUtf8Valid(xrtStrView(Text), NULL), "multilingual spill fixture is valid UTF-8");
    for (Limit = 1u; Limit <= 160u; ++Limit)
    {
        char *Inline = NULL;
        char *Artifact = NULL;
        uint64_t Id = 0u;
        xwork_error Error;
        bool Truncated = false;
        char *EventText;
        Agent.iMaxInlineToolBytes = Limit == 160u ? 12u * 1024u : Limit;
        CHECK(
            xwork__spill_tool_output(&Agent, "web_search", Text, &Inline, &Artifact, &Id, &Error) &&
                Inline && xrtUtf8Valid(xrtStrView(Inline), NULL),
            "spill preserves complete UTF-8 scalars");
        EventText = xwork__observability_copy_text(Text, strlen(Text), Limit, &Truncated, NULL);
        CHECK(EventText && strlen(EventText) <= Limit && Truncated &&
                  xrtUtf8Valid(xrtStrView(EventText), NULL),
              "event preview respects its byte cap and UTF-8");
        free(EventText);
        free(Inline);
        free(Artifact);
    }
}

static xwork_tool_request_token g_RequestToken;
static const char *g_RequestScope = "fixture-service:member-1";
static bool g_RequestPrepared;

static xwork_result request_token_execute(void *Data, const xwork_tool_context *Context,
                                          const char *Arguments, xwork_tool_result_writer *Writer,
                                          xwork_error *Error)
{
    (void)Data;
    (void)Arguments;
    xworkToolRequestTokenInit(&g_RequestToken);
    g_RequestPrepared = xworkToolResultWriterPrepareRequest(Writer, Context, g_RequestScope,
                                                            &g_RequestToken, Error);
    if (!g_RequestPrepared)
        return XWORK_RESULT_ERROR;
    /* Interrupt before a result exists. Completion receipts cannot supply this
     * ID; a subsequent callback must read the persisted request token itself. */
    xwork__set_error(Error, XWORK_ERROR_CANCELLED, "fixture interruption before completion");
    return XWORK_RESULT_CANCELLED;
}

static void request_token_call(xwork_agent *Agent, xllm_tool_call *Call, uint64_t Turn,
                               xwork_error *Error)
{
    char *Content = NULL;
    bool Success = false;
    g_RequestPrepared = false;
    (void)xwork__execute_tool(Agent, Call, Turn, NULL, XLLM_DEADLINE_NEVER, &Content, &Success, NULL,
                              NULL, NULL, NULL, Error);
    free(Content);
}

static void test_request_tokens(void)
{
    const char *Base = "tests/tmp_xwork_request_tokens";
    xwork_runtime_config RuntimeConfig;
    xwork_runtime *Runtime;
    xwork_agent_definition_config DefinitionConfig;
    xwork_agent_definition *Definition;
    xwork_agent_options Options;
    xllm_session_config SessionConfig;
    xllm_session *Session;
    xwork_artifact_store *Store;
    xroot Root;
    xwork_agent *Agent;
    xwork_tool_definition Tool = {0};
    xllm_tool_call Call = {0};
    xwork_error Error;
    char Original[33], Key[65], Path[256];
    xwork_tool_request_token Token;
    (void)xrtDirRemoveAll(Base);
    CHECK(xrtDirCreateAll("tests/tmp_xwork_request_tokens/workspace") &&
              xrtDirCreateAll("tests/tmp_xwork_request_tokens/storage"),
          "request identity fixture creates isolated roots");
    Root = xrtRootOpen("tests/tmp_xwork_request_tokens/storage");
    Store = xworkArtifactStoreCreate(Root, &Error);
    if (Root)
        (void)xrtRootClose(Root);
    xworkRuntimeConfigInit(&RuntimeConfig);
    Runtime = xworkRuntimeCreate(&RuntimeConfig, &Error);
    Tool.sName = "fixture.request";
    Tool.sDescription = "Externally idempotent read fixture";
    Tool.sParametersJson = "{\"type\":\"object\"}";
    Tool.uEffects = XWORK_TOOL_EFFECT_READ | XWORK_TOOL_EFFECT_NETWORK;
    Tool.OnDescribePermissions = result_file_describe;
    Tool.OnExecuteV2 = request_token_execute;
    CHECK(Runtime && xworkRuntimeRegisterTool(Runtime, &Tool, &Error) &&
              xworkRuntimeConfigureToolRecovery(Runtime, Tool.sName, true, &Error),
          "request fixture declares recoverable read contract");
    xworkAgentDefinitionConfigInit(&DefinitionConfig);
    Definition = xworkAgentDefinitionCreate(&DefinitionConfig, &Error);
    xllmSessionConfigInit(&SessionConfig);
    Session = xllmSessionCreate(&SessionConfig, NULL);
    xworkAgentOptionsInit(&Options);
    Options.pSession = Session;
    Options.pArtifactStore = Store;
    Options.sWorkspaceRoot = "tests/tmp_xwork_request_tokens/workspace";
    Options.OnPermission = result_file_permission;
    Agent = xworkAgentCreateWithRuntime(Runtime, Definition, &Options, &Error);
    Call.sId = "unfinished-request";
    Call.sName = "fixture.request";
    Call.sArgumentsJson = "{}";
    request_token_call(Agent, &Call, 1u, &Error);
    CHECK(g_RequestPrepared && g_RequestToken.bPersistent && !g_RequestToken.bRecovered &&
              strlen(g_RequestToken.sId) == 32u && g_RequestToken.iCreatedAt > 0,
          "first callback commits request identity before interruption");
    strcpy(Original, g_RequestToken.sId);
    request_token_call(Agent, &Call, 1u, &Error);
    CHECK(g_RequestPrepared && g_RequestToken.bRecovered && !strcmp(Original, g_RequestToken.sId),
          "same unfinished callback reuses one committed request identity");
    xworkAgentDestroy(Agent);
    xworkRuntimeRelease(Runtime);
    xworkArtifactStoreRelease(Store);
    Root = xrtRootOpen("tests/tmp_xwork_request_tokens/storage");
    Store = xworkArtifactStoreCreate(Root, &Error);
    if (Root)
        (void)xrtRootClose(Root);
    Runtime = xworkRuntimeCreate(&RuntimeConfig, &Error);
    CHECK(xworkRuntimeRegisterTool(Runtime, &Tool, &Error) &&
              xworkRuntimeConfigureToolRecovery(Runtime, Tool.sName, true, &Error),
          "rebuilt runtime restores callback descriptor");
    Options.pArtifactStore = Store;
    Agent = xworkAgentCreateWithRuntime(Runtime, Definition, &Options, &Error);
    request_token_call(Agent, &Call, 1u, &Error);
    CHECK(g_RequestPrepared && g_RequestToken.bRecovered && !strcmp(Original, g_RequestToken.sId),
          "reopened store and new agent recover unfinished request without completion receipt");
    g_RequestScope = "fixture-service:member-2";
    request_token_call(Agent, &Call, 1u, &Error);
    CHECK(!g_RequestPrepared && Error.eCode == XWORK_ERROR_POLICY && !g_RequestToken.sId[0],
          "different account or service cannot replace an unfinished request identity");
    g_RequestScope = "fixture-service:member-1";
    Call.sArgumentsJson = "{\"query\":\"changed\"}";
    request_token_call(Agent, &Call, 1u, &Error);
    CHECK(g_RequestPrepared && !g_RequestToken.bRecovered && strcmp(Original, g_RequestToken.sId),
          "new arguments create a distinct operation identity");
    Call.sArgumentsJson = "{}";
    request_token_call(Agent, &Call, 2u, &Error);
    CHECK(g_RequestPrepared && !g_RequestToken.bRecovered && strcmp(Original, g_RequestToken.sId),
          "same model call ID in another turn creates a distinct operation");
    const xwork_tool_entry *Entry = xwork__find_tool(Agent, Call.sName);
    CHECK(Entry && xwork__receipt_key(Entry, &Call, 1u, Key),
          "request fixture resolves its persisted record");
    snprintf(Path, sizeof(Path), "%s/storage/requests/%s.json", Base, Key);
    CHECK(xrtFileWriteAtomic(Path, (xbytesview){(const uint8 *)"damaged", 7u}),
          "fixture damages only the known request record");
    request_token_call(Agent, &Call, 1u, &Error);
    CHECK(!g_RequestPrepared && Error.eCode == XWORK_ERROR_IO && !g_RequestToken.sId[0],
          "damaged identity does not silently generate a new paid request");
    Call.sId = "publish-failure";
    g_eArtifactPublishFailure = XERR_IO;
    request_token_call(Agent, &Call, 1u, &Error);
    CHECK(!g_RequestPrepared && Error.eCode == XWORK_ERROR_IO && !g_RequestToken.sId[0],
          "publication failure returns no usable request identity");
    g_eArtifactPublishFailure = XERR_NONE;
    request_token_call(Agent, &Call, 1u, &Error);
    CHECK(g_RequestPrepared && !g_RequestToken.bRecovered,
          "failed publication did not retain an orphan committed token");
    Call.sId = "publish-race";
    g_bRequestTokenRace = true;
    request_token_call(Agent, &Call, 1u, &Error);
    CHECK(g_RequestPrepared && g_RequestToken.bRecovered &&
              !strcmp(g_RequestToken.sId, "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"),
          "non-replacing publication adopts the complete winning identity");
    g_RequestScope = "invalid\nscope";
    Call.sId = "invalid-scope";
    request_token_call(Agent, &Call, 1u, &Error);
    CHECK(!g_RequestPrepared && Error.eCode == XWORK_ERROR_INVALID_ARGUMENT,
          "scope controls are rejected before creating files");
    g_RequestScope = "fixture-service:member-1";
    xworkToolRequestTokenInit(&Token);
    CHECK(!xworkToolResultWriterPrepareRequest(NULL, NULL, g_RequestScope, &Token, &Error) &&
              Error.eCode == XWORK_ERROR_INVALID_ARGUMENT,
          "request storage is accessible only through a live callback");
    xworkAgentDestroy(Agent);
    Options.pArtifactStore = NULL;
    Agent = xworkAgentCreateWithRuntime(Runtime, Definition, &Options, &Error);
    Call.sId = "ephemeral";
    request_token_call(Agent, &Call, 1u, &Error);
    strcpy(Original, g_RequestToken.sId);
    request_token_call(Agent, &Call, 1u, &Error);
    CHECK(g_RequestPrepared && !g_RequestToken.bPersistent && !g_RequestToken.bRecovered &&
              strcmp(Original, g_RequestToken.sId),
          "callbacks without a host store receive fresh nonpersistent identities");
    xworkAgentDestroy(Agent);
    xworkArtifactStoreRelease(Store);
    xllmSessionDestroy(Session);
    xworkAgentDefinitionRelease(Definition);
    xworkRuntimeRelease(Runtime);
    (void)xrtDirRemoveAll(Base);
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc == 2 && strcmp(argv[1], "--request-tokens") == 0)
    {
        test_request_tokens();
        printf("request tokens: %s (%d failures)\n", g_iFailures ? "FAIL" : "PASS", g_iFailures);
        return g_iFailures ? 1 : 0;
    }
    if (argc == 2 && strcmp(argv[1], "--utf8-tool-output") == 0)
    {
        test_utf8_tool_output();
        printf("UTF-8 tool output: %s (%d failures)\n", g_iFailures ? "FAIL" : "PASS", g_iFailures);
        return g_iFailures ? 1 : 0;
    }
    if (argc == 2 && strcmp(argv[1], "--result-files") == 0)
    {
        test_result_file_pages();
        printf("result file pages: %s (%d failures)\n", g_iFailures ? "FAIL" : "PASS", g_iFailures);
        return g_iFailures ? 1 : 0;
    }
    if (argc == 2 && strcmp(argv[1], "--task-cancel-child") == 0)
    {
        (void)getchar();
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "--task-cancel-state") == 0)
    {
        g_sSelfPath = argv[0];
        test_task_stop_failure();
        test_configurable_subagent_policy();
        test_unified_task_runtime();
        printf("task cancel state: %s (%d failures)\n", g_iFailures ? "FAIL" : "PASS", g_iFailures);
        return g_iFailures ? 1 : 0;
    }
    if (argc == 2 && strcmp(argv[1], "--resume-effects") == 0)
    {
        test_resume_workspace_effects();
        printf("resume effects: %s (%d failures)\n", g_iFailures ? "FAIL" : "PASS", g_iFailures);
        return g_iFailures ? 1 : 0;
    }
    if (argc == 2 && strcmp(argv[1], "--artifact-publication") == 0)
    {
        test_artifact_publication();
        test_artifact_store();
        printf("xwork artifact publication: %s (%d failures)\n", g_iFailures ? "FAIL" : "PASS",
               g_iFailures);
        return g_iFailures ? 1 : 0;
    }
    if (argc == 2 && strcmp(argv[1], "--user-image") == 0)
    {
        test_user_image_run();
        printf("xwork user image: %s (%d failures)\n", g_iFailures ? "FAIL" : "PASS", g_iFailures);
        return g_iFailures ? 1 : 0;
    }
    if (argc == 2 && strcmp(argv[1], "--agent-loop") == 0)
    {
        g_sSelfPath = argv[0];
        test_agent_loop();
        test_compaction_styles();
        printf("xwork agent loop: %s (%d failures)\n", g_iFailures ? "FAIL" : "PASS", g_iFailures);
        return g_iFailures ? 1 : 0;
    }
    if (argc == 4 && strcmp(argv[1], "--mcp-http-probe") == 0)
    {
        return run_mcp_http_probe(argv[2], argv[3]);
    }
    if (argc == 3 && strcmp(argv[1], "--mcp-http-errors") == 0)
    {
        return run_mcp_http_errors(argv[2]);
    }
    if (argc == 2 && strcmp(argv[1], "--mcp-test-server") == 0)
    {
        return run_mcp_test_server(TEST_MCP_NORMAL);
    }
    if (argc == 2 && strcmp(argv[1], "--mcp-lazy-server") == 0)
    {
        return run_mcp_test_server(TEST_MCP_LAZY);
    }
    if (argc == 2 && strcmp(argv[1], "--mcp-modern-server") == 0)
    {
        return run_mcp_test_server(TEST_MCP_MODERN);
    }
    if (argc == 2 && strcmp(argv[1], "--mcp-malformed-server") == 0)
    {
        return run_mcp_test_server(TEST_MCP_MALFORMED);
    }
    if (argc == 2 && strcmp(argv[1], "--mcp-repeated-cursor-server") == 0)
    {
        return run_mcp_test_server(TEST_MCP_REPEATED_CURSOR);
    }
    if (argc == 2 && strcmp(argv[1], "--mcp-exit-server") == 0)
    {
        return run_mcp_test_server(TEST_MCP_EXIT);
    }
    if (argc == 2 && strcmp(argv[1], "--mcp-slow-start-server") == 0)
    {
        return run_mcp_test_server(TEST_MCP_SLOW_START);
    }
    setvbuf(stdout, NULL, _IONBF, 0);
    g_sSelfPath = argc > 0 ? argv[0] : NULL;
    printf("xwork v3 tests\n");
    test_task_stop_failure();
    test_process_text_normalization();
    test_v3_tool_contract();
    test_native_explore_tools();
    test_v3_tool_catalog_generations();
    test_v3_tool_owner_lifetime();
    test_agent_dependency_owner_lifetime();
    test_v3_runtime_lifetimes();
    test_runtime_observability();
    test_utf8_tool_output();
    test_result_file_pages();
    test_artifact_publication();
    test_artifact_store();
    test_unified_task_runtime();
    test_runtime_scheduler();
    test_agent_context_deadline();
    test_command_context_deadline();
    test_edit_eol_write();
    test_task_system();
    test_image_passthrough();
    test_subagent_delegation();
    test_subagent_definition_catalog();
    test_configurable_subagent_policy();
    test_parallel_tool_schedule();
    test_executor_bind();
    test_readonly_subagent();
    test_lazy_mcp_runtime();
    test_agent_loop();
    test_compaction_styles();
    test_session_run_with_tools_chain();
    test_resume_workspace_effects();
    test_run_window_and_durable_recovery();
    printf("xwork v3: %s (%d failures)\n", g_iFailures ? "FAIL" : "PASS", g_iFailures);
    return g_iFailures ? 1 : 0;
}
