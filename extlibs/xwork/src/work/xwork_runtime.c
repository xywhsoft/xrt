#include "../internal/xwork_internal.h"

/* xwork v3 lifetime spine.
 *
 * The runtime owns process-wide task state. Definitions are immutable policy
 * generations. Agents retain both and bind one session. Runs retain an agent
 * and provide a single-request cancellation, deadline, event, and result
 * boundary. The legacy xworkAgentCreate API is implemented with a private
 * runtime and definition so existing hosts keep their old cleanup behavior.
 */

bool xwork__record_valid(uint32_t uSize, uint32_t uVersion, size_t iNeed)
{
    return uVersion == XWORK_ABI_VERSION && (size_t)uSize >= iNeed;
}

void xworkRuntimeConfigInit(xwork_runtime_config *pConfig)
{
    if (!pConfig)
        return;
    memset(pConfig, 0, sizeof(*pConfig));
    pConfig->uSize = (uint32_t)sizeof(*pConfig);
    pConfig->uAbiVersion = XWORK_ABI_VERSION;
    pConfig->uMaxTasks = 64u;
    pConfig->uMaxCatalogTools = 4096u;
    pConfig->iMaxRetainedTaskBytes = 8u * 1024u * 1024u;
    pConfig->uMaxSchedules = 256u;
    pConfig->uMaxArtifacts = 4096u;
    pConfig->uMaxEvents = 4096u;
    pConfig->iMaxRetainedEventBytes = 8u * 1024u * 1024u;
    pConfig->iMaxEventTextBytes = 64u * 1024u;
}

xwork_runtime *xworkRuntimeCreate(const xwork_runtime_config *pConfig, xwork_error *pError)
{
    xwork_runtime_config tDefault;
    xwork_runtime *pRuntime;
    size_t iEffectiveEventBytes;
    size_t iEffectiveEventTextBytes;
    xworkErrorInit(pError);
    if (!pConfig)
    {
        xworkRuntimeConfigInit(&tDefault);
        pConfig = &tDefault;
    }
    if (!xwork__record_valid(pConfig->uSize, pConfig->uAbiVersion, sizeof(*pConfig)))
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
                         "invalid runtime configuration ABI or resource limit");
        return NULL;
    }
    iEffectiveEventBytes =
        pConfig->iMaxRetainedEventBytes ? pConfig->iMaxRetainedEventBytes : 8u * 1024u * 1024u;
    iEffectiveEventTextBytes =
        pConfig->iMaxEventTextBytes ? pConfig->iMaxEventTextBytes : 64u * 1024u;
    if (pConfig->uMaxTasks > 65536u || pConfig->uMaxCatalogTools > 65536u ||
        pConfig->uMaxSchedules > 65536u || pConfig->uMaxArtifacts > 65536u ||
        pConfig->uMaxEvents > 65536u || pConfig->iMaxRetainedEventBytes > 1024u * 1024u * 1024u ||
        pConfig->iMaxEventTextBytes > 16u * 1024u * 1024u ||
        iEffectiveEventBytes < sizeof(xwork_event_entry) ||
        iEffectiveEventTextBytes > iEffectiveEventBytes)
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
                         "invalid runtime configuration ABI or resource limit");
        return NULL;
    }
    pRuntime = (xwork_runtime *)calloc(1u, sizeof(*pRuntime));
    if (!pRuntime)
    {
        xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to allocate runtime");
        return NULL;
    }
    pRuntime->iRefs = 1;
    pRuntime->pLock = xrtMutexCreate();
    pRuntime->pSchedulerLock = xrtMutexCreate();
    pRuntime->pObservabilityLock = xrtMutexCreate();
    pRuntime->uNextCatalogGeneration = 1u;
    pRuntime->pToolCatalog = xwork__tool_catalog_empty(1u);
    pRuntime->uMaxTasks = pConfig->uMaxTasks ? pConfig->uMaxTasks : 64u;
    pRuntime->uMaxCatalogTools = pConfig->uMaxCatalogTools ? pConfig->uMaxCatalogTools : 4096u;
    pRuntime->iMaxRetainedTaskBytes =
        pConfig->iMaxRetainedTaskBytes ? pConfig->iMaxRetainedTaskBytes : 8u * 1024u * 1024u;
    pRuntime->uMaxSchedules = pConfig->uMaxSchedules ? pConfig->uMaxSchedules : 256u;
    pRuntime->uNextScheduleGeneration = 1u;
    pRuntime->uMaxArtifacts = pConfig->uMaxArtifacts ? pConfig->uMaxArtifacts : 4096u;
    pRuntime->uMaxEvents = pConfig->uMaxEvents ? pConfig->uMaxEvents : 4096u;
    pRuntime->iMaxRetainedEventBytes = iEffectiveEventBytes;
    pRuntime->iMaxEventTextBytes = iEffectiveEventTextBytes;
    pRuntime->uNextArtifactId = 1u;
    pRuntime->uNextEventId = 1u;
    pRuntime->ppArtifacts =
        (xwork_artifact_entry **)calloc(pRuntime->uMaxArtifacts, sizeof(*pRuntime->ppArtifacts));
    pRuntime->ppEvents =
        (xwork_event_entry **)calloc(pRuntime->uMaxEvents, sizeof(*pRuntime->ppEvents));
    if (!pRuntime->pLock || !pRuntime->pSchedulerLock || !pRuntime->pObservabilityLock ||
        !pRuntime->pToolCatalog || !pRuntime->ppArtifacts || !pRuntime->ppEvents)
    {
        free(pRuntime->ppArtifacts);
        free(pRuntime->ppEvents);
        if (pRuntime->pToolCatalog)
            xworkToolCatalogRelease(pRuntime->pToolCatalog);
        if (pRuntime->pObservabilityLock)
            (void)xrtMutexDestroy(pRuntime->pObservabilityLock);
        if (pRuntime->pSchedulerLock)
            (void)xrtMutexDestroy(pRuntime->pSchedulerLock);
        if (pRuntime->pLock)
            (void)xrtMutexDestroy(pRuntime->pLock);
        free(pRuntime);
        xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY,
                         "failed to create runtime synchronization");
        return NULL;
    }
    return pRuntime;
}

xwork_runtime *xworkRuntimeRef(xwork_runtime *pRuntime)
{
    if (!pRuntime || xwork__atomic_inc(&pRuntime->iRefs) <= 1)
        return NULL;
    return pRuntime;
}

void xworkRuntimeRelease(xwork_runtime *pRuntime)
{
    if (!pRuntime)
        return;
    if (xwork__atomic_dec(&pRuntime->iRefs) != 0)
        return;
    xwork__runtime_tasks_unit(pRuntime);
    xworkToolCatalogRelease(pRuntime->pToolCatalog);
    xwork__runtime_mcp_unit(pRuntime);
    xwork__runtime_scheduler_unit(pRuntime);
    xwork__runtime_observability_unit(pRuntime);
    (void)xrtMutexDestroy(pRuntime->pObservabilityLock);
    (void)xrtMutexDestroy(pRuntime->pSchedulerLock);
    (void)xrtMutexDestroy(pRuntime->pLock);
    free(pRuntime);
}

size_t xworkRuntimeTaskCount(const xwork_runtime *pRuntime)
{
    size_t iCount = 0u;
    size_t i;
    if (!pRuntime || !xrtMutexLock(pRuntime->pLock))
        return 0u;
    for (i = 0u; i < pRuntime->iTaskCount; ++i)
    {
        if (xwork__atomic_load(&pRuntime->ppTasks[i]->iReady) != 0)
            ++iCount;
    }
    (void)xrtMutexUnlock(pRuntime->pLock);
    return iCount;
}

uint64_t xwork__runtime_next_catalog_generation(xwork_runtime *pRuntime)
{
    uint64_t uGeneration;
    if (!pRuntime)
        return 0u;
    uGeneration = xwork__atomic_add_u64(&pRuntime->uNextCatalogGeneration, 1u);
    if (uGeneration == 0u)
    {
        uGeneration = xwork__atomic_add_u64(&pRuntime->uNextCatalogGeneration, 1u);
    }
    return uGeneration;
}

xwork_tool_catalog *xworkRuntimeToolCatalogSnapshot(xwork_runtime *pRuntime)
{
    xwork_tool_catalog *pCatalog = NULL;
    if (!pRuntime || !xrtMutexLock(pRuntime->pLock))
        return NULL;
    pCatalog = xworkToolCatalogRef(pRuntime->pToolCatalog);
    (void)xrtMutexUnlock(pRuntime->pLock);
    return pCatalog;
}

bool xworkRuntimeRegisterTool(xwork_runtime *pRuntime, const xwork_tool_definition *pDefinition,
                              xwork_error *pError)
{
    xwork_tool_catalog *pNext;
    xwork_tool_catalog *pOld;
    xworkErrorInit(pError);
    if (!pRuntime || !xrtMutexLock(pRuntime->pLock))
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "runtime is null or unavailable");
        return false;
    }
    pNext = xwork__tool_catalog_add(pRuntime->pToolCatalog,
                                    xwork__runtime_next_catalog_generation(pRuntime), pDefinition,
                                    pError);
    if (!pNext)
    {
        (void)xrtMutexUnlock(pRuntime->pLock);
        return false;
    }
    if (pNext->iToolCount > pRuntime->uMaxCatalogTools)
    {
        xworkToolCatalogRelease(pNext);
        (void)xrtMutexUnlock(pRuntime->pLock);
        xwork__set_error(pError, XWORK_ERROR_LIMIT, "runtime tool catalog limit was exceeded");
        return false;
    }
    pOld = pRuntime->pToolCatalog;
    pRuntime->pToolCatalog = pNext;
    (void)xrtMutexUnlock(pRuntime->pLock);
    xworkToolCatalogRelease(pOld);
    return true;
}

bool xworkRuntimeUnregisterTool(xwork_runtime *pRuntime, const char *sName, xwork_error *pError)
{
    xwork_tool_catalog *pNext;
    xwork_tool_catalog *pOld;
    xworkErrorInit(pError);
    if (!pRuntime || !xrtMutexLock(pRuntime->pLock))
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "runtime is null or unavailable");
        return false;
    }
    pNext = xwork__tool_catalog_remove_name(
        pRuntime->pToolCatalog, xwork__runtime_next_catalog_generation(pRuntime), sName, pError);
    if (!pNext)
    {
        (void)xrtMutexUnlock(pRuntime->pLock);
        return false;
    }
    pOld = pRuntime->pToolCatalog;
    pRuntime->pToolCatalog = pNext;
    (void)xrtMutexUnlock(pRuntime->pLock);
    xworkToolCatalogRelease(pOld);
    return true;
}

bool xworkRuntimeReplaceToolsBySource(xwork_runtime *pRuntime, const char *sSource,
                                      const xwork_tool_definition *pDefinitions, size_t iCount,
                                      size_t *piRemoved, xwork_error *pError)
{
    xwork_tool_catalog *pNext;
    xwork_tool_catalog *pOld;
    xworkErrorInit(pError);
    if (piRemoved)
        *piRemoved = 0u;
    if (!pRuntime || !xrtMutexLock(pRuntime->pLock))
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "runtime is null or unavailable");
        return false;
    }
    if (iCount > pRuntime->uMaxCatalogTools)
    {
        (void)xrtMutexUnlock(pRuntime->pLock);
        xwork__set_error(pError, XWORK_ERROR_LIMIT,
                         "runtime tool replacement exceeds the catalog limit");
        return false;
    }
    pNext = xwork__tool_catalog_replace_source(pRuntime->pToolCatalog,
                                               xwork__runtime_next_catalog_generation(pRuntime),
                                               sSource, pDefinitions, iCount, piRemoved, pError);
    if (!pNext)
    {
        (void)xrtMutexUnlock(pRuntime->pLock);
        return false;
    }
    if (pNext->iToolCount > pRuntime->uMaxCatalogTools)
    {
        xworkToolCatalogRelease(pNext);
        (void)xrtMutexUnlock(pRuntime->pLock);
        if (piRemoved)
            *piRemoved = 0u;
        xwork__set_error(pError, XWORK_ERROR_LIMIT, "runtime tool catalog limit was exceeded");
        return false;
    }
    pOld = pRuntime->pToolCatalog;
    pRuntime->pToolCatalog = pNext;
    (void)xrtMutexUnlock(pRuntime->pLock);
    xworkToolCatalogRelease(pOld);
    return true;
}

void xworkToolConcurrencyConfigInit(xwork_tool_concurrency_config *pConfig)
{
    if (!pConfig)
        return;
    memset(pConfig, 0, sizeof(*pConfig));
    pConfig->uSize = (uint32_t)sizeof(*pConfig);
    pConfig->uAbiVersion = XWORK_ABI_VERSION;
}

bool xworkRuntimeConfigureToolConcurrency(xwork_runtime *pRuntime,
                                          const xwork_tool_concurrency_config *pConfig,
                                          xwork_error *pError)
{
    xwork_tool_catalog *pNext;
    xwork_tool_catalog *pOld;
    xworkErrorInit(pError);
    if (!pRuntime || !xrtMutexLock(pRuntime->pLock))
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "runtime is null or unavailable");
        return false;
    }
    pNext = xwork__tool_catalog_configure_concurrency(
        pRuntime->pToolCatalog, xwork__runtime_next_catalog_generation(pRuntime), pConfig, pError);
    if (!pNext)
    {
        (void)xrtMutexUnlock(pRuntime->pLock);
        return false;
    }
    pOld = pRuntime->pToolCatalog;
    pRuntime->pToolCatalog = pNext;
    (void)xrtMutexUnlock(pRuntime->pLock);
    xworkToolCatalogRelease(pOld);
    return true;
}

bool xworkRuntimeConfigureToolRecovery(xwork_runtime *Runtime, const char *ToolName,
                                       bool RetrySafeRead, xwork_error *Error)
{
    const xwork_tool_effects ReadEffects = XWORK_TOOL_EFFECT_READ | XWORK_TOOL_EFFECT_NETWORK |
                                           XWORK_TOOL_EFFECT_EXTERNAL_SERVICE |
                                           XWORK_TOOL_EFFECT_SECRETS;
    xwork_tool_catalog *Next = NULL;
    xwork_tool_catalog *Old = NULL;
    size_t Index, i;
    bool Ok = false;
    xworkErrorInit(Error);
    if (!Runtime || !ToolName || !ToolName[0] || !xrtMutexLock(Runtime->pLock))
    {
        xwork__set_error(Error, XWORK_ERROR_INVALID_ARGUMENT,
                         "invalid tool recovery configuration");
        return false;
    }
    for (Index = 0u; Index < Runtime->pToolCatalog->iToolCount; ++Index)
        if (strcmp(Runtime->pToolCatalog->pTools[Index].sName, ToolName) == 0)
            break;
    if (Index == Runtime->pToolCatalog->iToolCount ||
        (RetrySafeRead && (Runtime->pToolCatalog->pTools[Index].uEffects & ~ReadEffects) != 0u))
    {
        xwork__set_error(Error, XWORK_ERROR_POLICY,
                         "recovery requires a known read-operation descriptor");
        goto done;
    }
    Next = xwork__tool_catalog_allocate(xwork__runtime_next_catalog_generation(Runtime),
                                        Runtime->pToolCatalog->iToolCount);
    if (!Next)
    {
        xwork__set_error(Error, XWORK_ERROR_OUT_OF_MEMORY, "cannot configure tool recovery");
        goto done;
    }
    for (i = 0u; i < Next->iToolCount; ++i)
        if (!xwork__tool_entry_copy(&Next->pTools[i], &Runtime->pToolCatalog->pTools[i], Error))
            goto done;
    Next->pTools[Index].bRetrySafeRead = RetrySafeRead;
    Old = Runtime->pToolCatalog;
    Runtime->pToolCatalog = Next;
    Next = NULL;
    Ok = true;
done:
    (void)xrtMutexUnlock(Runtime->pLock);
    xworkToolCatalogRelease(Old);
    xworkToolCatalogRelease(Next);
    return Ok;
}

void xworkAgentDefinitionConfigInit(xwork_agent_definition_config *pConfig)
{
    if (!pConfig)
        return;
    memset(pConfig, 0, sizeof(*pConfig));
    pConfig->uSize = (uint32_t)sizeof(*pConfig);
    pConfig->uAbiVersion = XWORK_ABI_VERSION;
    pConfig->sId = "default";
    pConfig->sSystemPrompt =
        "You are a careful coding agent. Inspect the workspace, use tools to make changes, run "
        "relevant tests, and continue until the user's task is complete.";
    pConfig->eApprovalMode = XWORK_APPROVAL_AUTO;
    pConfig->eEolPolicy = XWORK_EOL_AUTO;
    pConfig->uCommandTimeoutMs = 120000u;
    pConfig->uRepeatedToolBatchLimit = 3u;
    pConfig->uConsecutiveFailureLimit = 5u;
    pConfig->uMaxManagedTasks = 8u;
    pConfig->uMaxParallelTools = 4u;
    pConfig->uMaxSubagentDepth = 1u;
    pConfig->uMaxConcurrentSubagents = 4u;
    pConfig->uCompletionVerificationRetries = 2u;
    pConfig->uCompactionQualityRetries = 1u;
    pConfig->iMaxInlineToolBytes = 64u * 1024u;
    pConfig->iMaxCapturedCommandBytes = 8u * 1024u * 1024u;
    pConfig->bRegisterBuiltinTools = true;
    pConfig->bAutoSaveSession = true;
    pConfig->bAllowArtifactWrites = true;
    pConfig->bRequireVerificationAfterWrite = true;
}

xwork_agent_definition *xworkAgentDefinitionCreate(const xwork_agent_definition_config *pConfig,
                                                   xwork_error *pError)
{
    xwork_agent_definition *pDefinition;
    xworkErrorInit(pError);
    if (!pConfig || !xwork__record_valid(pConfig->uSize, pConfig->uAbiVersion, sizeof(*pConfig)) ||
        !pConfig->sId || !pConfig->sId[0] || !pConfig->sSystemPrompt ||
        !pConfig->sSystemPrompt[0] || pConfig->uMaxManagedTasks > 65536u ||
        pConfig->uMaxParallelTools > 64u || pConfig->uMaxSubagentDepth > 16u ||
        pConfig->uMaxConcurrentSubagents > 64u)
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
                         "invalid agent definition configuration");
        return NULL;
    }
    pDefinition = (xwork_agent_definition *)calloc(1u, sizeof(*pDefinition));
    if (!pDefinition)
        goto oom;
    pDefinition->iRefs = 1;
    pDefinition->sId = xwork__strdup(pConfig->sId);
    pDefinition->sSystemPrompt = xwork__strdup(pConfig->sSystemPrompt);
    pDefinition->sModel = pConfig->sModel ? xwork__strdup(pConfig->sModel) : NULL;
    pDefinition->sReasoningEffort =
        pConfig->sReasoningEffort ? xwork__strdup(pConfig->sReasoningEffort) : NULL;
    if (!pDefinition->sId || !pDefinition->sSystemPrompt ||
        (pConfig->sModel && !pDefinition->sModel) ||
        (pConfig->sReasoningEffort && !pDefinition->sReasoningEffort))
        goto oom;
    pDefinition->eApprovalMode = pConfig->eApprovalMode;
    pDefinition->eEolPolicy = pConfig->eEolPolicy;
    pDefinition->uCommandTimeoutMs =
        pConfig->uCommandTimeoutMs ? pConfig->uCommandTimeoutMs : 120000u;
    pDefinition->uMaxAgentTurns = pConfig->uMaxAgentTurns;
    pDefinition->uRepeatedToolBatchLimit =
        pConfig->uRepeatedToolBatchLimit ? pConfig->uRepeatedToolBatchLimit : 3u;
    pDefinition->uConsecutiveFailureLimit =
        pConfig->uConsecutiveFailureLimit ? pConfig->uConsecutiveFailureLimit : 5u;
    pDefinition->uMaxManagedTasks = pConfig->uMaxManagedTasks ? pConfig->uMaxManagedTasks : 8u;
    pDefinition->uMaxParallelTools = pConfig->uMaxParallelTools ? pConfig->uMaxParallelTools : 4u;
    pDefinition->uMaxSubagentDepth = pConfig->uMaxSubagentDepth ? pConfig->uMaxSubagentDepth : 1u;
    pDefinition->uMaxConcurrentSubagents =
        pConfig->uMaxConcurrentSubagents ? pConfig->uMaxConcurrentSubagents : 4u;
    pDefinition->uCompletionVerificationRetries =
        pConfig->uCompletionVerificationRetries ? pConfig->uCompletionVerificationRetries : 2u;
    pDefinition->uCompactionQualityRetries = pConfig->uCompactionQualityRetries;
    pDefinition->iMaxInlineToolBytes =
        pConfig->iMaxInlineToolBytes ? pConfig->iMaxInlineToolBytes : 64u * 1024u;
    pDefinition->iMaxCapturedCommandBytes =
        pConfig->iMaxCapturedCommandBytes ? pConfig->iMaxCapturedCommandBytes : 8u * 1024u * 1024u;
    pDefinition->bRegisterBuiltinTools = pConfig->bRegisterBuiltinTools;
    pDefinition->bAutoSaveSession = pConfig->bAutoSaveSession;
    pDefinition->bAllowArtifactWrites = pConfig->bAllowArtifactWrites;
    pDefinition->bRequireVerificationAfterWrite = pConfig->bRequireVerificationAfterWrite;
    pDefinition->bInjectSystemPrompt = pConfig->bInjectSystemPrompt;
    return pDefinition;
oom:
    if (pDefinition)
    {
        free(pDefinition->sId);
        free(pDefinition->sSystemPrompt);
        free(pDefinition->sModel);
        free(pDefinition->sReasoningEffort);
        free(pDefinition);
    }
    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to copy agent definition");
    return NULL;
}

xwork_agent_definition *xworkAgentDefinitionRef(xwork_agent_definition *pDefinition)
{
    if (!pDefinition || xwork__atomic_inc(&pDefinition->iRefs) <= 1)
        return NULL;
    return pDefinition;
}

void xworkAgentDefinitionRelease(xwork_agent_definition *pDefinition)
{
    if (!pDefinition || xwork__atomic_dec(&pDefinition->iRefs) != 0)
        return;
    free(pDefinition->sId);
    free(pDefinition->sSystemPrompt);
    free(pDefinition->sModel);
    free(pDefinition->sReasoningEffort);
    free(pDefinition);
}

void xworkAgentOptionsInit(xwork_agent_options *pOptions)
{
    if (!pOptions)
        return;
    memset(pOptions, 0, sizeof(*pOptions));
    pOptions->uSize = (uint32_t)sizeof(*pOptions);
    pOptions->uAbiVersion = XWORK_ABI_VERSION;
    pOptions->uDeadline = XLLM_DEADLINE_NEVER;
}

static void xwork__definition_to_legacy(const xwork_agent_definition *pDefinition,
                                        const xwork_agent_options *pOptions,
                                        xwork_agent_config *pConfig)
{
    xworkAgentConfigInit(pConfig);
    pConfig->pClient = pOptions->pClient;
    pConfig->pSession = pOptions->pSession;
    pConfig->sWorkspaceRoot = pOptions->sWorkspaceRoot;
    pConfig->sSystemPrompt = pDefinition->sSystemPrompt;
    pConfig->sSessionPath = pOptions->sSessionPath;
    pConfig->sArtifactDirectory = pOptions->sArtifactDirectory;
    pConfig->pArtifactStore = pOptions->pArtifactStore;
    pConfig->sModel = pDefinition->sModel;
    pConfig->sReasoningEffort = pDefinition->sReasoningEffort;
    pConfig->pCancel = pOptions->pCancel;
    pConfig->uDeadline = pOptions->uDeadline;
    pConfig->eApprovalMode = pDefinition->eApprovalMode;
    pConfig->OnApproval = pOptions->OnApproval;
    pConfig->pApprovalUserData = pOptions->pApprovalUserData;
    pConfig->OnPermission = pOptions->OnPermission;
    pConfig->pPermissionUserData = pOptions->pPermissionUserData;
    pConfig->OnHook = pOptions->OnHook;
    pConfig->pHookUserData = pOptions->pHookUserData;
    pConfig->eEolPolicy = pDefinition->eEolPolicy;
    pConfig->OnEvent = pOptions->OnEvent;
    pConfig->pEventUserData = pOptions->pEventUserData;
    pConfig->OnModelComplete = pOptions->OnModelComplete;
    pConfig->pModelUserData = pOptions->pModelUserData;
    pConfig->OnExtension = pOptions->OnExtension;
    pConfig->pExtensionUserData = pOptions->pExtensionUserData;
    pConfig->pOwnerUserData = pOptions->pOwnerUserData;
    pConfig->OnOwnerRetain = pOptions->OnOwnerRetain;
    pConfig->OnOwnerRelease = pOptions->OnOwnerRelease;
    pConfig->uCommandTimeoutMs = pDefinition->uCommandTimeoutMs;
    pConfig->uMaxAgentTurns = pDefinition->uMaxAgentTurns;
    pConfig->uRepeatedToolBatchLimit = pDefinition->uRepeatedToolBatchLimit;
    pConfig->uConsecutiveFailureLimit = pDefinition->uConsecutiveFailureLimit;
    pConfig->uMaxManagedProcesses = pDefinition->uMaxManagedTasks;
    pConfig->uMaxParallelTools = pDefinition->uMaxParallelTools;
    pConfig->uMaxSubagentDepth = pDefinition->uMaxSubagentDepth;
    pConfig->uMaxConcurrentSubagents = pDefinition->uMaxConcurrentSubagents;
    pConfig->uCompletionVerificationRetries = pDefinition->uCompletionVerificationRetries;
    pConfig->uCompactionQualityRetries = pDefinition->uCompactionQualityRetries;
    pConfig->iMaxInlineToolBytes = pDefinition->iMaxInlineToolBytes;
    pConfig->iMaxCapturedCommandBytes = pDefinition->iMaxCapturedCommandBytes;
    pConfig->bRegisterBuiltinTools = pDefinition->bRegisterBuiltinTools;
    pConfig->bAutoSaveSession = pDefinition->bAutoSaveSession;
    pConfig->bAllowArtifactWrites = pDefinition->bAllowArtifactWrites;
    pConfig->bRequireVerificationAfterWrite = pDefinition->bRequireVerificationAfterWrite;
    pConfig->bInjectSystemPrompt = pDefinition->bInjectSystemPrompt;
}

xwork_agent *xworkAgentCreateWithRuntime(xwork_runtime *pRuntime,
                                         xwork_agent_definition *pDefinition,
                                         const xwork_agent_options *pOptions, xwork_error *pError)
{
    xwork_agent_config tConfig;
    xworkErrorInit(pError);
    if (!pRuntime || !pDefinition || !pOptions ||
        !xwork__record_valid(pOptions->uSize, pOptions->uAbiVersion, sizeof(*pOptions)))
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
                         "runtime, definition, and valid agent options are required");
        return NULL;
    }
    xwork__definition_to_legacy(pDefinition, pOptions, &tConfig);
    return xwork__agent_create_bound(&tConfig, pRuntime, pDefinition, pError);
}

xwork_agent *xworkAgentCreate(const xwork_agent_config *pConfig, xwork_error *pError)
{
    xwork_runtime_config tRuntimeConfig;
    xwork_agent_definition_config tDefinitionConfig;
    xwork_runtime *pRuntime = NULL;
    xwork_agent_definition *pDefinition = NULL;
    xwork_agent *pAgent = NULL;
    if (!pConfig)
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "agent configuration is required");
        return NULL;
    }
    xworkRuntimeConfigInit(&tRuntimeConfig);
    if (pConfig->uMaxManagedProcesses > tRuntimeConfig.uMaxTasks)
    {
        tRuntimeConfig.uMaxTasks = pConfig->uMaxManagedProcesses;
    }
    pRuntime = xworkRuntimeCreate(&tRuntimeConfig, pError);
    if (!pRuntime)
        goto cleanup;
    xworkAgentDefinitionConfigInit(&tDefinitionConfig);
    tDefinitionConfig.sSystemPrompt =
        pConfig->sSystemPrompt ? pConfig->sSystemPrompt : tDefinitionConfig.sSystemPrompt;
    tDefinitionConfig.sModel = pConfig->sModel;
    tDefinitionConfig.sReasoningEffort = pConfig->sReasoningEffort;
    tDefinitionConfig.eApprovalMode = pConfig->eApprovalMode;
    tDefinitionConfig.eEolPolicy = pConfig->eEolPolicy;
    tDefinitionConfig.uCommandTimeoutMs = pConfig->uCommandTimeoutMs;
    tDefinitionConfig.uMaxAgentTurns = pConfig->uMaxAgentTurns;
    tDefinitionConfig.uRepeatedToolBatchLimit = pConfig->uRepeatedToolBatchLimit;
    tDefinitionConfig.uConsecutiveFailureLimit = pConfig->uConsecutiveFailureLimit;
    tDefinitionConfig.uMaxManagedTasks = pConfig->uMaxManagedProcesses;
    tDefinitionConfig.uMaxParallelTools = pConfig->uMaxParallelTools;
    tDefinitionConfig.uMaxSubagentDepth = pConfig->uMaxSubagentDepth;
    tDefinitionConfig.uMaxConcurrentSubagents = pConfig->uMaxConcurrentSubagents;
    tDefinitionConfig.uCompletionVerificationRetries = pConfig->uCompletionVerificationRetries;
    tDefinitionConfig.uCompactionQualityRetries = pConfig->uCompactionQualityRetries;
    tDefinitionConfig.iMaxInlineToolBytes = pConfig->iMaxInlineToolBytes;
    tDefinitionConfig.iMaxCapturedCommandBytes = pConfig->iMaxCapturedCommandBytes;
    tDefinitionConfig.bRegisterBuiltinTools = pConfig->bRegisterBuiltinTools;
    tDefinitionConfig.bAutoSaveSession = pConfig->bAutoSaveSession;
    tDefinitionConfig.bAllowArtifactWrites = pConfig->bAllowArtifactWrites;
    tDefinitionConfig.bRequireVerificationAfterWrite = pConfig->bRequireVerificationAfterWrite;
    tDefinitionConfig.bInjectSystemPrompt = pConfig->bInjectSystemPrompt;
    pDefinition = xworkAgentDefinitionCreate(&tDefinitionConfig, pError);
    if (!pDefinition)
        goto cleanup;
    pAgent = xwork__agent_create_bound(pConfig, pRuntime, pDefinition, pError);
cleanup:
    xworkAgentDefinitionRelease(pDefinition);
    xworkRuntimeRelease(pRuntime);
    return pAgent;
}

void xworkRunConfigInit(xwork_run_config *pConfig)
{
    if (!pConfig)
        return;
    memset(pConfig, 0, sizeof(*pConfig));
    pConfig->uSize = (uint32_t)sizeof(*pConfig);
    pConfig->uAbiVersion = XWORK_ABI_VERSION;
    pConfig->uDeadline = XLLM_DEADLINE_NEVER;
}

void xworkRunInfoInit(xwork_run_info *pInfo)
{
    if (!pInfo)
        return;
    memset(pInfo, 0, sizeof(*pInfo));
    pInfo->uSize = (uint32_t)sizeof(*pInfo);
    pInfo->uAbiVersion = XWORK_ABI_VERSION;
}

static void xwork__run_external_cancel(ptr pData)
{
    xwork_run *pRun = (xwork_run *)pData;
    if (pRun && pRun->pCancel)
        (void)xrtCancelRequest(pRun->pCancel);
}

static void xwork__run_free(xwork_run *pRun)
{
    if (!pRun)
        return;
    xrtCancelUnwatch(pRun->pExternalWatch);
    xrtCancelDestroy(pRun->pCancel);
    if (pRun->pThread)
        xrtThreadDestroy(pRun->pThread);
    xworkRunResultUnit(&pRun->tResult);
    xwork__resume_options_unit(&pRun->tResumeOptions, pRun->pRecoveryDecisions);
    if (pRun->bHasUserMessage)
        xllmMessageUnit(&pRun->tUserMessage);
    free(pRun->sPrompt);
    xwork__agent_release(pRun->pAgent);
    if (pRun->pDone)
        (void)xrtCondDestroy(pRun->pDone);
    if (pRun->pLock)
        (void)xrtMutexDestroy(pRun->pLock);
    free(pRun);
}

static void xwork__run_ref(xwork_run *pRun)
{
    if (pRun)
        (void)xwork__atomic_inc(&pRun->iRefs);
}

static void xwork__run_release(xwork_run *pRun)
{
    if (pRun && xwork__atomic_dec(&pRun->iRefs) == 0)
        xwork__run_free(pRun);
}

static bool xwork__run_user_message_valid(const xllm_message *pMessage, const char *sPrompt)
{
    size_t i;
    size_t iTotal = 0u;
    size_t iText = 0u;
    if (!pMessage)
        return true;
    if (pMessage->eRole != XLLM_ROLE_USER || pMessage->sContent || pMessage->sReasoningContent ||
        pMessage->sToolCallId || pMessage->sNative || pMessage->iToolCallCount ||
        !pMessage->pParts || pMessage->iPartCount < 2u || pMessage->iPartCount > 5u ||
        pMessage->pParts[0].eKind != XLLM_PART_TEXT || !pMessage->pParts[0].sText)
        return false;
    while (iText <= 65536u && pMessage->pParts[0].sText[iText] != '\0')
        ++iText;
    if (iText > 65536u || !xrtUtf8Valid((xstrview){pMessage->pParts[0].sText, iText}, NULL) ||
        strcmp(pMessage->pParts[0].sText, sPrompt ? sPrompt : "") != 0)
        return false;
    for (i = 1u; i < pMessage->iPartCount; ++i)
    {
        const xllm_part *pPart = &pMessage->pParts[i];
        if (pPart->eKind != XLLM_PART_IMAGE || !pPart->pData || !pPart->iDataSize ||
            pPart->iDataSize > 8u * 1024u * 1024u || !pPart->sMediaType || pPart->sSourceUrl ||
            (strcmp(pPart->sMediaType, "image/png") != 0 &&
             strcmp(pPart->sMediaType, "image/jpeg") != 0 &&
             strcmp(pPart->sMediaType, "image/webp") != 0))
            return false;
        iTotal += pPart->iDataSize;
        if (iTotal > 16u * 1024u * 1024u)
            return false;
    }
    return true;
}

xwork_run *xworkRunCreateWithUserMessage(xwork_agent *pAgent, const xwork_run_config *pConfig,
                                         const xllm_message *pUserMessage, xwork_error *pError)
{
    xwork_run *pRun;
    xworkErrorInit(pError);
    if (!pAgent || !pConfig ||
        !xwork__record_valid(pConfig->uSize, pConfig->uAbiVersion, sizeof(*pConfig)) ||
        (!pConfig->bResume && !pUserMessage && (!pConfig->sPrompt || !pConfig->sPrompt[0])) ||
        (!pConfig->bResume && pConfig->pResumeOptions) || (pConfig->bResume && pUserMessage) ||
        !xwork__run_user_message_valid(pUserMessage, pConfig->sPrompt))
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
                         "agent and a valid prompt or resume run configuration are required");
        return NULL;
    }
    pRun = (xwork_run *)calloc(1u, sizeof(*pRun));
    if (!pRun)
        goto oom;
    pRun->iRefs = 1;
    pRun->pAgent = xwork__agent_ref(pAgent);
    pRun->pLock = xrtMutexCreate();
    pRun->pDone = xrtCondCreate();
    pRun->pCancel = xrtCancelChild(pAgent->pCancel);
    pRun->sPrompt = pConfig->sPrompt ? xwork__strdup(pConfig->sPrompt)
                                     : (pUserMessage ? xwork__strdup("") : NULL);
    if (pUserMessage)
    {
        size_t i;
        xllmMessageInit(&pRun->tUserMessage, XLLM_ROLE_USER);
        pRun->bHasUserMessage = true;
        for (i = 0u; i < pUserMessage->iPartCount; ++i)
        {
            if (!xllmMessageAddPart(&pRun->tUserMessage, &pUserMessage->pParts[i]))
                goto oom;
        }
    }
    pRun->OnEvent = pConfig->OnEvent;
    pRun->pEventUserData = pConfig->pEventUserData;
    pRun->uDeadline = pConfig->uDeadline;
    pRun->uCreatedUs = xllmClockUs();
    pRun->eState = XWORK_RUN_CREATED;
    pRun->eResult = XWORK_RESULT_ERROR;
    pRun->bResume = pConfig->bResume;
    xworkErrorInit(&pRun->tError);
    if (pConfig->pResumeOptions)
    {
        if (!xwork__resume_options_clone(&pRun->tResumeOptions, &pRun->pRecoveryDecisions,
                                         pConfig->pResumeOptions, pError))
        {
            xwork__run_free(pRun);
            return NULL;
        }
        pRun->bHasResumeOptions = true;
    }
    if (pAgent->pRuntime && xrtMutexLock(pAgent->pRuntime->pLock))
    {
        pRun->uRunId = ++pAgent->pRuntime->uNextRunId;
        if (pRun->uRunId == 0u)
            pRun->uRunId = ++pAgent->pRuntime->uNextRunId;
        (void)xrtMutexUnlock(pAgent->pRuntime->pLock);
    }
    if (pConfig->pCancel)
    {
        pRun->pExternalWatch =
            xrtCancelWatch(pConfig->pCancel, xwork__run_external_cancel, (ptr)pRun);
    }
    if (!pRun->pAgent || !pRun->pLock || !pRun->pDone || !pRun->pCancel ||
        (!pConfig->bResume && !pRun->sPrompt) || !pRun->uRunId ||
        (pConfig->pCancel && !pRun->pExternalWatch))
        goto oom;
    return pRun;
oom:
    xwork__run_free(pRun);
    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to create run");
    return NULL;
}

xwork_run *xworkRunCreate(xwork_agent *pAgent, const xwork_run_config *pConfig, xwork_error *pError)
{
    return xworkRunCreateWithUserMessage(pAgent, pConfig, NULL, pError);
}

static xwork_run_state xwork__run_state_from_result(xwork_result eResult)
{
    switch (eResult)
    {
    case XWORK_RESULT_OK:
        return XWORK_RUN_SUCCEEDED;
    case XWORK_RESULT_CANCELLED:
        return XWORK_RUN_CANCELLED;
    case XWORK_RESULT_TIMEOUT:
        return XWORK_RUN_TIMED_OUT;
    default:
        return XWORK_RUN_FAILED;
    }
}

static int32_t xwork__run_thread_proc(ptr pData)
{
    xwork_run *pRun = (xwork_run *)pData;
    xwork_run_result tResult;
    xwork_error tError;
    xwork_result eResult;
    memset(&tResult, 0, sizeof(tResult));
    xworkErrorInit(&tError);
    eResult = xwork__agent_run_claimed(
        pRun->pAgent, pRun->bResume ? NULL : pRun->sPrompt,
        pRun->bHasUserMessage ? &pRun->tUserMessage : NULL, pRun->bResume,
        pRun->bHasResumeOptions ? &pRun->tResumeOptions : NULL, &tResult, &tError);
    xwork__agent_run_release(pRun->pAgent, pRun);
    if (xrtMutexLock(pRun->pLock))
    {
        pRun->eResult = eResult;
        pRun->eState = xwork__run_state_from_result(eResult);
        pRun->uEndedUs = xllmClockUs();
        pRun->tResult = tResult;
        memset(&tResult, 0, sizeof(tResult));
        pRun->tError = tError;
        (void)xrtCondBroadcast(pRun->pDone);
        (void)xrtMutexUnlock(pRun->pLock);
    }
    xworkRunResultUnit(&tResult);
    xwork__run_release(pRun); /* worker reference */
    return 0;
}

bool xworkRunStart(xwork_run *pRun, xwork_error *pError)
{
    xwork_error tClaimError;
    xworkErrorInit(pError);
    if (!pRun || !xrtMutexLock(pRun->pLock))
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "run is null or unavailable");
        return false;
    }
    if (pRun->eState != XWORK_RUN_CREATED)
    {
        (void)xrtMutexUnlock(pRun->pLock);
        xwork__set_error(pError, XWORK_ERROR_CONTEXT, "run has already been started");
        return false;
    }
    pRun->eState = XWORK_RUN_RUNNING;
    pRun->uStartedUs = xllmClockUs();
    (void)xrtMutexUnlock(pRun->pLock);
    xworkErrorInit(&tClaimError);
    if (!xwork__agent_run_claim(pRun->pAgent, pRun, &tClaimError))
    {
        if (xrtMutexLock(pRun->pLock))
        {
            pRun->eState = XWORK_RUN_FAILED;
            pRun->eResult = XWORK_RESULT_ERROR;
            pRun->uEndedUs = xllmClockUs();
            pRun->tError = tClaimError;
            (void)xrtCondBroadcast(pRun->pDone);
            (void)xrtMutexUnlock(pRun->pLock);
        }
        if (pError)
            *pError = tClaimError;
        return false;
    }
    xwork__run_ref(pRun);
    pRun->pThread = xrtThreadCreate(xwork__run_thread_proc, (ptr)pRun, 0u);
    if (!pRun->pThread)
    {
        xwork__run_release(pRun);
        xwork__agent_run_release(pRun->pAgent, pRun);
        if (xrtMutexLock(pRun->pLock))
        {
            pRun->eState = XWORK_RUN_FAILED;
            pRun->eResult = XWORK_RESULT_ERROR;
            pRun->uEndedUs = xllmClockUs();
            xwork__set_error(&pRun->tError, XWORK_ERROR_OUT_OF_MEMORY,
                             "failed to start run thread");
            (void)xrtCondBroadcast(pRun->pDone);
            (void)xrtMutexUnlock(pRun->pLock);
        }
        xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to start run thread");
        return false;
    }
    return true;
}

static bool xwork__run_result_copy(xwork_run_result *pOut, const xwork_run_result *pSource)
{
    char *sFinal = NULL;
    if (!pOut || !pSource)
        return false;
    if (pSource->sFinalText)
    {
        sFinal = xwork__strdup(pSource->sFinalText);
        if (!sFinal)
            return false;
    }
    *pOut = *pSource;
    pOut->sFinalText = sFinal;
    return true;
}

xwork_result xworkRunWait(xwork_run *pRun, uint64_t uDeadline, xwork_run_result *pResult,
                          xwork_error *pError)
{
    xwork_result eResult;
    xwaitresult eWait;
    if (pResult)
        memset(pResult, 0, sizeof(*pResult));
    xworkErrorInit(pError);
    if (!pRun || !pResult || !xrtMutexLock(pRun->pLock))
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "run and result are required");
        return XWORK_RESULT_ERROR;
    }
    if (pRun->eState == XWORK_RUN_CREATED)
    {
        (void)xrtMutexUnlock(pRun->pLock);
        xwork__set_error(pError, XWORK_ERROR_CONTEXT, "run has not been started");
        return XWORK_RESULT_ERROR;
    }
    while (pRun->eState == XWORK_RUN_RUNNING)
    {
        eWait = xrtCondWaitFor(pRun->pDone, pRun->pLock, xllmDeadlineWaitMs(uDeadline));
        if (eWait == XWAIT_TIMEOUT)
        {
            (void)xrtMutexUnlock(pRun->pLock);
            xwork__set_error(pError, XWORK_ERROR_TIMEOUT,
                             "run wait deadline was exceeded; the run remains active");
            return XWORK_RESULT_TIMEOUT;
        }
        if (eWait != XWAIT_OK)
        {
            (void)xrtMutexUnlock(pRun->pLock);
            xwork__set_error(pError, XWORK_ERROR_CONTEXT, "run wait failed");
            return XWORK_RESULT_ERROR;
        }
    }
    eResult = pRun->eResult;
    if (!xwork__run_result_copy(pResult, &pRun->tResult))
    {
        (void)xrtMutexUnlock(pRun->pLock);
        xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to copy run result");
        return XWORK_RESULT_ERROR;
    }
    if (pError)
        *pError = pRun->tError;
    (void)xrtMutexUnlock(pRun->pLock);
    return eResult;
}

bool xworkRunCancel(xwork_run *pRun)
{
    if (!pRun || !pRun->pCancel)
        return false;
    (void)xrtCancelRequest(pRun->pCancel);
    return true;
}

bool xworkRunGetInfo(const xwork_run *pRun, xwork_run_info *pInfo)
{
    xwork_run *pMutable = (xwork_run *)pRun;
    uint32_t uSize;
    uint32_t uVersion;
    if (!pRun || !pInfo || !xwork__record_valid(pInfo->uSize, pInfo->uAbiVersion, sizeof(*pInfo)) ||
        !xrtMutexLock(pMutable->pLock))
        return false;
    uSize = pInfo->uSize;
    uVersion = pInfo->uAbiVersion;
    memset(pInfo, 0, sizeof(*pInfo));
    pInfo->uSize = uSize;
    pInfo->uAbiVersion = uVersion;
    pInfo->uRunId = pRun->uRunId;
    pInfo->eState = pRun->eState;
    pInfo->eResult = pRun->eResult;
    pInfo->uCreatedUs = pRun->uCreatedUs;
    pInfo->uStartedUs = pRun->uStartedUs;
    pInfo->uEndedUs = pRun->uEndedUs;
    pInfo->bResume = pRun->bResume;
    (void)xrtMutexUnlock(pMutable->pLock);
    return true;
}

void xworkRunDestroy(xwork_run *pRun)
{
    xthread *pThread = NULL;
    if (!pRun)
        return;
    (void)xworkRunCancel(pRun);
    if (pRun->pLock && xrtMutexLock(pRun->pLock))
    {
        pThread = pRun->pThread ? xrtThreadRef(pRun->pThread) : NULL;
        (void)xrtMutexUnlock(pRun->pLock);
    }
    if (pThread)
    {
        (void)xrtThreadWait(pThread);
        xrtThreadDestroy(pThread);
    }
    xwork__run_release(pRun);
}
