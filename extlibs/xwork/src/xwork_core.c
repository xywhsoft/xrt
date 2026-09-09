static int xwork__path_char_equal(char a, char b)
{
    if ( a == '/' || a == '\\' ) a = '/';
    if ( b == '/' || b == '\\' ) b = '/';
#if defined(_WIN32)
    return tolower((unsigned char)a) == tolower((unsigned char)b);
#else
    return a == b;
#endif
}

char* xwork__strdup(const char* sText)
{
    size_t iLen;
    char* sCopy;
    if ( !sText ) return NULL;
    iLen = strlen(sText);
    sCopy = (char*)malloc(iLen + 1u);
    if ( sCopy ) memcpy(sCopy, sText, iLen + 1u);
    return sCopy;
}

bool xwork__replace(char** ppDst, const char* sText)
{
    char* sCopy = sText ? xwork__strdup(sText) : NULL;
    if ( sText && !sCopy ) return false;
    free(*ppDst);
    *ppDst = sCopy;
    return true;
}

void xworkErrorInit(xwork_error* pError)
{
    if ( !pError ) return;
    memset(pError, 0, sizeof(*pError));
    xllmErrorInit(&pError->tModelError);
}

const char* xworkErrorCodeName(xwork_error_code eCode)
{
    switch ( eCode ) {
        case XWORK_ERROR_NONE: return "none";
        case XWORK_ERROR_INVALID_ARGUMENT: return "invalid_argument";
        case XWORK_ERROR_OUT_OF_MEMORY: return "out_of_memory";
        case XWORK_ERROR_MODEL: return "model";
        case XWORK_ERROR_TOOL: return "tool";
        case XWORK_ERROR_POLICY: return "policy";
        case XWORK_ERROR_IO: return "io";
        case XWORK_ERROR_CONTEXT: return "context";
        case XWORK_ERROR_LOOP_GUARD: return "loop_guard";
        case XWORK_ERROR_CANCELLED: return "cancelled";
        case XWORK_ERROR_TIMEOUT: return "timeout";
        default: return "unknown";
    }
}

void xwork__set_error(xwork_error* pError, xwork_error_code eCode, const char* sMessage)
{
    if ( !pError ) return;
    xworkErrorInit(pError);
    pError->eCode = eCode;
    if ( sMessage ) {
        snprintf(pError->sMessage, sizeof(pError->sMessage), "%s", sMessage);
    }
}

void xwork__copy_model_error(xwork_error* pError, const xllm_error* pModelError)
{
    if ( !pError ) return;
    xwork__set_error(
        pError,
        XWORK_ERROR_MODEL,
        (pModelError && pModelError->sMessage[0]) ? pModelError->sMessage : "model call failed"
    );
    if ( pModelError ) pError->tModelError = *pModelError;
}

void xworkToolOutputInit(xwork_tool_output* pOutput)
{
    if ( pOutput ) memset(pOutput, 0, sizeof(*pOutput));
}

void xworkToolOutputUnit(xwork_tool_output* pOutput)
{
    if ( !pOutput ) return;
    free(pOutput->sContent);
    memset(pOutput, 0, sizeof(*pOutput));
}

bool xworkToolOutputSet(xwork_tool_output* pOutput, bool bSuccess, const char* sContent)
{
    char* sCopy;
    if ( !pOutput ) return false;
    sCopy = xwork__strdup(sContent ? sContent : "");
    if ( !sCopy ) return false;
    free(pOutput->sContent);
    pOutput->sContent = sCopy;
    pOutput->bSuccess = bSuccess;
    return true;
}

bool xwork__buf_reserve(xwork_buf* pBuf, size_t iNeed)
{
    size_t iCap;
    char* pNew;
    if ( iNeed <= pBuf->iCap ) return true;
    iCap = pBuf->iCap ? pBuf->iCap : 256u;
    while ( iCap < iNeed ) {
        if ( iCap > ((size_t)-1) / 2u ) { iCap = iNeed; break; }
        iCap *= 2u;
    }
    pNew = (char*)realloc(pBuf->pData, iCap);
    if ( !pNew ) return false;
    pBuf->pData = pNew;
    pBuf->iCap = iCap;
    return true;
}

bool xwork__buf_append(xwork_buf* pBuf, const void* pData, size_t iLen)
{
    if ( !pBuf || (!pData && iLen) ) return false;
    if ( iLen > (size_t)-1 - pBuf->iLen - 1u ) return false;
    if ( !xwork__buf_reserve(pBuf, pBuf->iLen + iLen + 1u) ) return false;
    if ( iLen ) memcpy(pBuf->pData + pBuf->iLen, pData, iLen);
    pBuf->iLen += iLen;
    pBuf->pData[pBuf->iLen] = '\0';
    return true;
}

bool xwork__buf_append_cstr(xwork_buf* pBuf, const char* sText)
{
    return xwork__buf_append(pBuf, sText ? sText : "", sText ? strlen(sText) : 0u);
}

bool xwork__buf_append_char(xwork_buf* pBuf, char ch)
{
    return xwork__buf_append(pBuf, &ch, 1u);
}

bool xwork__buf_appendf(xwork_buf* pBuf, const char* sFormat, ...)
{
    va_list tArgs;
    va_list tCopy;
    int iNeeded;
    if ( !pBuf || !sFormat ) return false;
    va_start(tArgs, sFormat);
    va_copy(tCopy, tArgs);
    iNeeded = vsnprintf(NULL, 0u, sFormat, tCopy);
    va_end(tCopy);
    if ( iNeeded < 0 || !xwork__buf_reserve(pBuf, pBuf->iLen + (size_t)iNeeded + 1u) ) {
        va_end(tArgs);
        return false;
    }
    (void)vsnprintf(pBuf->pData + pBuf->iLen, (size_t)iNeeded + 1u, sFormat, tArgs);
    va_end(tArgs);
    pBuf->iLen += (size_t)iNeeded;
    return true;
}

char* xwork__buf_detach(xwork_buf* pBuf)
{
    char* pData;
    if ( !pBuf ) return NULL;
    if ( !pBuf->pData ) {
        pBuf->pData = xwork__strdup("");
        pBuf->iCap = pBuf->pData ? 1u : 0u;
    }
    pData = pBuf->pData;
    memset(pBuf, 0, sizeof(*pBuf));
    return pData;
}

void xwork__buf_unit(xwork_buf* pBuf)
{
    if ( !pBuf ) return;
    free(pBuf->pData);
    memset(pBuf, 0, sizeof(*pBuf));
}

bool xwork__json_string(xwork_buf* pBuf, const char* sText)
{
    const unsigned char* p = (const unsigned char*)(sText ? sText : "");
    if ( !xwork__buf_append_char(pBuf, '"') ) return false;
    while ( *p ) {
        char sEscape[7];
        switch ( *p ) {
            case '"': if ( !xwork__buf_append_cstr(pBuf, "\\\"") ) return false; break;
            case '\\': if ( !xwork__buf_append_cstr(pBuf, "\\\\") ) return false; break;
            case '\b': if ( !xwork__buf_append_cstr(pBuf, "\\b") ) return false; break;
            case '\f': if ( !xwork__buf_append_cstr(pBuf, "\\f") ) return false; break;
            case '\n': if ( !xwork__buf_append_cstr(pBuf, "\\n") ) return false; break;
            case '\r': if ( !xwork__buf_append_cstr(pBuf, "\\r") ) return false; break;
            case '\t': if ( !xwork__buf_append_cstr(pBuf, "\\t") ) return false; break;
            default:
                if ( *p < 0x20u ) {
                    snprintf(sEscape, sizeof(sEscape), "\\u%04x", (unsigned int)*p);
                    if ( !xwork__buf_append_cstr(pBuf, sEscape) ) return false;
                } else if ( !xwork__buf_append_char(pBuf, (char)*p) ) return false;
                break;
        }
        ++p;
    }
    return xwork__buf_append_char(pBuf, '"');
}

xvalue* xwork__json_parse_object(const char* sJson)
{
    xvalue* pValue;
    if ( !sJson ) return NULL;
    pValue = xrtJsonParse((xstrview){ sJson, strlen(sJson) });
    if ( !pValue || xrtValueType(pValue) != XVALUE_OBJECT ) {
        if ( pValue ) xrtValueRelease(pValue);
        return NULL;
    }
    return pValue;
}

xvalue* xwork__json_get(xvalue* pObject, const char* sKey)
{
    xvalue* pValue = (pObject && xrtValueType(pObject) == XVALUE_OBJECT)
        ? xrtValueObjectGet(pObject, (xstrview){ sKey, strlen(sKey) }) : NULL;
    return (pValue && xrtValueType(pValue) != XVALUE_NULL) ? pValue : NULL;
}

const char* xwork__json_text(xvalue* pObject, const char* sKey)
{
    xvalue* pValue = xwork__json_get(pObject, sKey);
    xstrview tText = {0};
    return (pValue && xrtValueGetString(pValue, &tText)) ? tText.Data : NULL;
}

bool xwork__json_bool(xvalue* pObject, const char* sKey, bool bDefault, bool* pValid)
{
    xvalue* pValue = xwork__json_get(pObject, sKey);
    bool bValue;
    if ( pValid ) *pValid = true;
    if ( !pValue ) return bDefault;
    if ( !xrtValueGetBool(pValue, &bValue) ) { if ( pValid ) *pValid = false; return bDefault; }
    return bValue;
}

uint64_t xwork__json_u64(xvalue* pObject, const char* sKey, uint64_t uDefault, bool* pValid)
{
    xvalue* pValue = xwork__json_get(pObject, sKey);
    int64_t iValue;
    if ( pValid ) *pValid = true;
    if ( !pValue ) return uDefault;
    if ( !xrtValueGetInt(pValue, &iValue) ) { if ( pValid ) *pValid = false; return uDefault; }
    if ( iValue < 0 ) { if ( pValid ) *pValid = false; return uDefault; }
    return (uint64_t)iValue;
}

static bool xwork__path_is_inside(const char* sRoot, const char* sPath)
{
    size_t i;
    size_t iRootLen = strlen(sRoot);
    for ( i = 0u; i < iRootLen; ++i ) {
        if ( !sPath[i] || !xwork__path_char_equal(sRoot[i], sPath[i]) ) return false;
    }
    return sPath[iRootLen] == '\0' || sPath[iRootLen] == '/' || sPath[iRootLen] == '\\';
}

char* xwork__resolve_path(const xwork_agent* pAgent, const char* sPath, xwork_error* pError)
{
    char* sJoined = NULL;
    char* sAbs;
    char* sCopy;
    if ( !pAgent || !sPath || !sPath[0] ) {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "tool path is empty");
        return NULL;
    }
    if ( xrtPathIsAbs(sPath) ) {
        sAbs = xrtPathAbs(sPath);
    } else {
        sJoined = xrtPathJoin(pAgent->sWorkspaceRoot, sPath);
        if ( !sJoined ) {
            xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to join workspace path");
            return NULL;
        }
        sAbs = xrtPathAbs(sJoined);
        xrtFree(sJoined);
    }
    if ( !sAbs ) {
        xwork__set_error(pError, XWORK_ERROR_IO, "failed to resolve workspace path");
        return NULL;
    }
    if ( !xwork__path_is_inside(pAgent->sWorkspaceRoot, sAbs) ) {
        xrtFree(sAbs);
        xwork__set_error(pError, XWORK_ERROR_POLICY, "path escapes the configured workspace");
        return NULL;
    }
    sCopy = xwork__strdup(sAbs);
    xrtFree(sAbs);
    if ( !sCopy ) xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to copy resolved path");
    return sCopy;
}

char* xwork__relative_path(const xwork_agent* pAgent, const char* sPath)
{
    char* sRelative;
    char* sCopy;
    if ( !pAgent || !sPath ) return NULL;
    sRelative = xrtPathRel(pAgent->sWorkspaceRoot, sPath);
    if ( !sRelative ) return xwork__strdup(sPath);
    sCopy = xwork__strdup(sRelative);
    xrtFree(sRelative);
    return sCopy;
}

bool xwork__ensure_parent(const char* sPath)
{
    char* sDir;
    bool bOk;
    if ( !sPath ) return false;
    sDir = xrtPathParent(sPath);
    if ( !sDir || !sDir[0] ) { if ( sDir ) xrtFree(sDir); return true; }
    bOk = xrtDirExists((str)sDir) || xrtDirCreateAll((str)sDir);
    xrtFree(sDir);
    return bOk;
}

bool xwork__emit(xwork_agent* pAgent, const xwork_event* pEvent)
{
    xwork_event tEvent;
    if ( !pAgent || !pEvent ) return false;
    if ( xwork__is_cancelled(pAgent) ) return false;
    tEvent = *pEvent;
    tEvent.uAgentDepth = pAgent->uAgentDepth;
    tEvent.uDelegationId = pAgent->uDelegationId;
    tEvent.uParentAgentTurn = pAgent->uParentAgentTurn;
    if ( pAgent->OnEvent && !pAgent->OnEvent(pAgent->pEventUserData, &tEvent) ) {
        xwork__atomic_store(&pAgent->iCancelled, 1);
        return false;
    }
    return true;
}

bool xwork__save(xwork_agent* pAgent, xwork_error* pError)
{
    xllm_error tError;
    if ( !pAgent || !pAgent->bAutoSaveSession || !pAgent->sSessionPath || !pAgent->sSessionPath[0] ) return true;
    if ( !xwork__ensure_parent(pAgent->sSessionPath) ) {
        xwork__set_error(pError, XWORK_ERROR_IO, "failed to create session directory");
        return false;
    }
    xllmErrorInit(&tError);
    if ( !xllmSessionCheckpoint(pAgent->pSession, pAgent->sSessionPath, &tError) ) {
        xwork__set_error(pError, XWORK_ERROR_IO, tError.sMessage[0] ? tError.sMessage : "failed to save session");
        return false;
    }
    return true;
}

void xworkAgentConfigInit(xwork_agent_config* pConfig)
{
    if ( !pConfig ) return;
    memset(pConfig, 0, sizeof(*pConfig));
    pConfig->uDeadline = XRT_DEADLINE_NEVER;
    pConfig->eApprovalMode = XWORK_APPROVAL_AUTO;
    pConfig->uCommandTimeoutMs = 120000u;
    pConfig->uMaxAgentTurns = 0u;
    pConfig->uRepeatedToolBatchLimit = 3u;
    pConfig->uConsecutiveFailureLimit = 5u;
    pConfig->uMaxManagedProcesses = 8u;
    pConfig->uCompletionVerificationRetries = 2u;
    pConfig->uCompactionQualityRetries = 1u;
    pConfig->iMaxInlineToolBytes = 64u * 1024u;
    pConfig->iMaxCapturedCommandBytes = 8u * 1024u * 1024u;
    pConfig->uMemoryMaxHitsPerLayer = 8u;
    pConfig->iMemoryMaxContextBytesPerLayer = 16u * 1024u;
    pConfig->eMemoryMaximumSensitivity = XLLM_MEMORY_SENSITIVITY_INTERNAL;
    pConfig->bRegisterBuiltinTools = true;
    pConfig->bAutoSaveSession = true;
    pConfig->bAllowArtifactWrites = true;
    pConfig->bRequireVerificationAfterWrite = true;
    pConfig->bRetrieveMemory = true;
}

static void xwork__tool_entry_unit(xwork_tool_entry* pTool)
{
    if ( !pTool ) return;
    free(pTool->sName);
    free(pTool->sDescription);
    free(pTool->sParametersJson);
    free(pTool->sSource);
    memset(pTool, 0, sizeof(*pTool));
}

const xwork_tool_entry* xwork__find_tool(const xwork_agent* pAgent, const char* sName)
{
    size_t i;
    if ( !pAgent || !sName ) return NULL;
    for ( i = 0u; i < pAgent->iToolCount; ++i ) {
        if ( strcmp(pAgent->pTools[i].sName, sName) == 0 ) return &pAgent->pTools[i];
    }
    return NULL;
}

bool xworkAgentRegisterTool(xwork_agent* pAgent, const xwork_tool_definition* pDefinition, xwork_error* pError)
{
    xwork_tool_entry* pTool;
    xwork_tool_entry* pNew;
    size_t iCap;
    if ( !pAgent || !pDefinition || !pDefinition->sName || !pDefinition->sName[0] ||
         !pDefinition->sDescription || !pDefinition->sParametersJson || !pDefinition->OnExecute ) {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "invalid tool definition");
        return false;
    }
    if ( pAgent->bRunning ) {
        xwork__set_error(pError, XWORK_ERROR_CONTEXT, "tool registry cannot change while the agent is running");
        return false;
    }
    if ( xwork__find_tool(pAgent, pDefinition->sName) ) {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "tool name is already registered");
        return false;
    }
    if ( pAgent->iToolCount == pAgent->iToolCap ) {
        iCap = pAgent->iToolCap ? pAgent->iToolCap * 2u : 8u;
        pNew = (xwork_tool_entry*)realloc(pAgent->pTools, iCap * sizeof(*pNew));
        if ( !pNew ) {
            xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to grow tool registry");
            return false;
        }
        pAgent->pTools = pNew;
        pAgent->iToolCap = iCap;
    }
    pTool = &pAgent->pTools[pAgent->iToolCount];
    memset(pTool, 0, sizeof(*pTool));
    pTool->sName = xwork__strdup(pDefinition->sName);
    pTool->sDescription = xwork__strdup(pDefinition->sDescription);
    pTool->sParametersJson = xwork__strdup(pDefinition->sParametersJson);
    pTool->sSource = xwork__strdup(
        pDefinition->sSource && pDefinition->sSource[0] ? pDefinition->sSource : "application");
    if ( !pTool->sName || !pTool->sDescription || !pTool->sParametersJson || !pTool->sSource ) {
        xwork__tool_entry_unit(pTool);
        xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to copy tool definition");
        return false;
    }
    pTool->bStrict = pDefinition->bStrict;
    pTool->eEffect = pDefinition->eEffect;
    pTool->OnExecute = pDefinition->OnExecute;
    pTool->pUserData = pDefinition->pUserData;
    ++pAgent->iToolCount;
    ++pAgent->uToolRegistryGeneration;
    return true;
}

bool xworkAgentUnregisterTool(xwork_agent* pAgent, const char* sName, xwork_error* pError)
{
    size_t i;
    if ( !pAgent || !sName || !sName[0] ) {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "agent and tool name are required");
        return false;
    }
    if ( pAgent->bRunning ) {
        xwork__set_error(pError, XWORK_ERROR_CONTEXT, "tool registry cannot change while the agent is running");
        return false;
    }
    for ( i = 0u; i < pAgent->iToolCount; ++i ) {
        if ( strcmp(pAgent->pTools[i].sName, sName) != 0 ) continue;
        xwork__tool_entry_unit(&pAgent->pTools[i]);
        if ( i + 1u < pAgent->iToolCount ) {
            memmove(&pAgent->pTools[i], &pAgent->pTools[i + 1u],
                (pAgent->iToolCount - i - 1u) * sizeof(*pAgent->pTools));
        }
        --pAgent->iToolCount;
        memset(&pAgent->pTools[pAgent->iToolCount], 0, sizeof(*pAgent->pTools));
        ++pAgent->uToolRegistryGeneration;
        return true;
    }
    xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "tool name is not registered");
    return false;
}

bool xworkAgentUnregisterToolsBySource(
    xwork_agent* pAgent,
    const char* sSource,
    size_t* piRemoved,
    xwork_error* pError
)
{
    size_t i = 0u;
    size_t iRemoved = 0u;
    if ( piRemoved ) *piRemoved = 0u;
    if ( !pAgent || !sSource || !sSource[0] ) {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "agent and tool source are required");
        return false;
    }
    if ( pAgent->bRunning ) {
        xwork__set_error(pError, XWORK_ERROR_CONTEXT, "tool registry cannot change while the agent is running");
        return false;
    }
    while ( i < pAgent->iToolCount ) {
        if ( strcmp(pAgent->pTools[i].sSource, sSource) != 0 ) { ++i; continue; }
        xwork__tool_entry_unit(&pAgent->pTools[i]);
        if ( i + 1u < pAgent->iToolCount ) {
            memmove(&pAgent->pTools[i], &pAgent->pTools[i + 1u],
                (pAgent->iToolCount - i - 1u) * sizeof(*pAgent->pTools));
        }
        --pAgent->iToolCount;
        memset(&pAgent->pTools[pAgent->iToolCount], 0, sizeof(*pAgent->pTools));
        ++iRemoved;
    }
    if ( iRemoved ) ++pAgent->uToolRegistryGeneration;
    if ( piRemoved ) *piRemoved = iRemoved;
    return true;
}

xwork_agent* xworkAgentCreate(const xwork_agent_config* pConfig, xwork_error* pError)
{
    xwork_agent* pAgent;
    char* sRoot;
    xllm_session_stats tStats;
    uint64_t uTurn;
    if ( !pConfig || !pConfig->pSession || !pConfig->sWorkspaceRoot || !pConfig->sWorkspaceRoot[0] ||
         (!pConfig->pClient && !pConfig->OnModelComplete) ) {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "agent requires a session, workspace, and model boundary");
        return NULL;
    }
    sRoot = xrtPathAbs(pConfig->sWorkspaceRoot);
    if ( !sRoot || !xrtDirExists((str)sRoot) ) {
        if ( sRoot ) xrtFree(sRoot);
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "workspace root does not exist");
        return NULL;
    }
    pAgent = (xwork_agent*)calloc(1u, sizeof(*pAgent));
    if ( !pAgent ) {
        xrtFree(sRoot);
        xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to allocate agent");
        return NULL;
    }
    pAgent->pClient = pConfig->pClient;
    pAgent->pSession = pConfig->pSession;
    pAgent->pMemory = pConfig->pMemory;
    pAgent->sWorkspaceRoot = xwork__strdup(sRoot);
    pAgent->sSystemPrompt = xwork__strdup(pConfig->sSystemPrompt ? pConfig->sSystemPrompt : "You are a careful coding agent. Inspect the workspace, use tools to make changes, run relevant tests, and continue until the user's task is complete.");
    pAgent->sSessionPath = pConfig->sSessionPath ? xwork__strdup(pConfig->sSessionPath) : NULL;
    pAgent->sArtifactDirectory = xwork__strdup(pConfig->sArtifactDirectory ? pConfig->sArtifactDirectory : ".xcode/artifacts");
    pAgent->sModel = pConfig->sModel ? xwork__strdup(pConfig->sModel) : NULL;
    pAgent->sReasoningEffort = pConfig->sReasoningEffort ? xwork__strdup(pConfig->sReasoningEffort) : NULL;
    pAgent->pCancel = xrtCancelChild(pConfig->pCancel);
    pAgent->uDeadline = pConfig->uDeadline;
    xrtFree(sRoot);
    if ( !pAgent->sWorkspaceRoot || !pAgent->sSystemPrompt || !pAgent->sArtifactDirectory ||
         (pConfig->sSessionPath && !pAgent->sSessionPath) ||
         (pConfig->sModel && !pAgent->sModel) ||
         (pConfig->sReasoningEffort && !pAgent->sReasoningEffort) ||
         !pAgent->pCancel ) {
        xworkAgentDestroy(pAgent);
        xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to copy agent configuration");
        return NULL;
    }
    pAgent->eApprovalMode = pConfig->eApprovalMode;
    pAgent->OnApproval = pConfig->OnApproval;
    pAgent->pApprovalUserData = pConfig->pApprovalUserData;
    pAgent->OnPermission = pConfig->OnPermission;
    pAgent->pPermissionUserData = pConfig->pPermissionUserData;
    pAgent->OnHook = pConfig->OnHook;
    pAgent->pHookUserData = pConfig->pHookUserData;
    pAgent->OnEvent = pConfig->OnEvent;
    pAgent->pEventUserData = pConfig->pEventUserData;
    pAgent->OnModelComplete = pConfig->OnModelComplete;
    pAgent->pModelUserData = pConfig->pModelUserData;
    pAgent->uCommandTimeoutMs = pConfig->uCommandTimeoutMs ? pConfig->uCommandTimeoutMs : 120000u;
    pAgent->uMaxAgentTurns = pConfig->uMaxAgentTurns;
    pAgent->uRepeatedToolBatchLimit = pConfig->uRepeatedToolBatchLimit ? pConfig->uRepeatedToolBatchLimit : 3u;
    pAgent->uConsecutiveFailureLimit = pConfig->uConsecutiveFailureLimit ? pConfig->uConsecutiveFailureLimit : 5u;
    pAgent->uMaxManagedProcesses = pConfig->uMaxManagedProcesses ? pConfig->uMaxManagedProcesses : 8u;
    pAgent->uCompletionVerificationRetries = pConfig->uCompletionVerificationRetries ? pConfig->uCompletionVerificationRetries : 2u;
    pAgent->uCompactionQualityRetries = pConfig->uCompactionQualityRetries;
    pAgent->iMaxInlineToolBytes = pConfig->iMaxInlineToolBytes ? pConfig->iMaxInlineToolBytes : 64u * 1024u;
    pAgent->iMaxCapturedCommandBytes = pConfig->iMaxCapturedCommandBytes ? pConfig->iMaxCapturedCommandBytes : 8u * 1024u * 1024u;
    pAgent->uMemoryMaxHitsPerLayer = pConfig->uMemoryMaxHitsPerLayer ? pConfig->uMemoryMaxHitsPerLayer : 8u;
    pAgent->iMemoryMaxContextBytesPerLayer = pConfig->iMemoryMaxContextBytesPerLayer
        ? pConfig->iMemoryMaxContextBytesPerLayer : 16u * 1024u;
    pAgent->eMemoryMaximumSensitivity = pConfig->eMemoryMaximumSensitivity;
    pAgent->bAutoSaveSession = pConfig->bAutoSaveSession;
    pAgent->bAllowArtifactWrites = pConfig->bAllowArtifactWrites;
    pAgent->bRequireVerificationAfterWrite = pConfig->bRequireVerificationAfterWrite;
    pAgent->bRetrieveMemory = pConfig->bRetrieveMemory;
    if ( pAgent->eMemoryMaximumSensitivity < XLLM_MEMORY_SENSITIVITY_PUBLIC ||
         pAgent->eMemoryMaximumSensitivity > XLLM_MEMORY_SENSITIVITY_SECRET ||
         pAgent->iMemoryMaxContextBytesPerLayer < 512u ) {
        xworkAgentDestroy(pAgent);
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "invalid memory retrieval policy");
        return NULL;
    }

    if ( !xllmSessionGetStats(pAgent->pSession, &tStats) ) {
        xworkAgentDestroy(pAgent);
        xwork__set_error(pError, XWORK_ERROR_CONTEXT, "failed to inspect session");
        return NULL;
    }
    if ( tStats.uEntryCount == 0u ) {
        uTurn = xllmSessionBeginTurn(pAgent->pSession);
        if ( !uTurn || !xllmSessionAddText(pAgent->pSession, uTurn, XLLM_ROLE_SYSTEM, pAgent->sSystemPrompt, XLLM_SESSION_ENTRY_PINNED) ) {
            xworkAgentDestroy(pAgent);
            xwork__set_error(pError, XWORK_ERROR_CONTEXT, "failed to initialize system context");
            return NULL;
        }
    }
    if ( pConfig->bRegisterBuiltinTools && !xworkAgentRegisterBuiltinTools(pAgent, pError) ) {
        xworkAgentDestroy(pAgent);
        return NULL;
    }
    return pAgent;
}

void xworkAgentDestroy(xwork_agent* pAgent)
{
    size_t i;
    if ( !pAgent ) return;
    xwork__processes_unit(pAgent);
    for ( i = 0u; i < pAgent->iToolCount; ++i ) xwork__tool_entry_unit(&pAgent->pTools[i]);
    free(pAgent->pTools);
    free(pAgent->sWorkspaceRoot);
    free(pAgent->sSystemPrompt);
    free(pAgent->sSessionPath);
    free(pAgent->sArtifactDirectory);
    free(pAgent->sModel);
    free(pAgent->sReasoningEffort);
    xrtCancelDestroy(pAgent->pCancel);
    free(pAgent);
}

size_t xworkAgentToolCount(const xwork_agent* pAgent)
{
    return pAgent ? pAgent->iToolCount : 0u;
}

bool xworkAgentToolAt(const xwork_agent* pAgent, size_t iIndex, xwork_tool_info* pInfo)
{
    const xwork_tool_entry* pTool;
    if ( !pAgent || !pInfo || iIndex >= pAgent->iToolCount ) return false;
    pTool = &pAgent->pTools[iIndex];
    memset(pInfo, 0, sizeof(*pInfo));
    pInfo->sName = pTool->sName;
    pInfo->sDescription = pTool->sDescription;
    pInfo->sParametersJson = pTool->sParametersJson;
    pInfo->sSource = pTool->sSource;
    pInfo->bStrict = pTool->bStrict;
    pInfo->eEffect = pTool->eEffect;
    return true;
}

uint64_t xworkAgentToolRegistryGeneration(const xwork_agent* pAgent)
{
    return pAgent ? pAgent->uToolRegistryGeneration : 0u;
}

bool xworkAgentCancel(xwork_agent* pAgent)
{
    if ( !pAgent ) return false;
    xwork__atomic_store(&pAgent->iCancelled, 1);
    if ( pAgent->pCancel ) { (void)xrtCancelRequest(pAgent->pCancel); }
    return true;
}

const char* xworkAgentWorkspaceRoot(const xwork_agent* pAgent)
{
    return pAgent ? pAgent->sWorkspaceRoot : NULL;
}

xllm_result xwork__model_complete(
    xwork_agent* pAgent,
    const xllm_request* pRequest,
    const xllm_stream_callbacks* pCallbacks,
    xllm_response** ppResponse,
    xllm_error* pError
)
{
    if ( pAgent->OnModelComplete ) {
        return pAgent->OnModelComplete(pAgent->pModelUserData, pRequest, pCallbacks, ppResponse, pError);
    }
    return xllmClientComplete(pAgent->pClient, pRequest, pCallbacks, ppResponse, pError);
}

void xworkRunResultUnit(xwork_run_result* pResult)
{
    if ( !pResult ) return;
    free(pResult->sFinalText);
    memset(pResult, 0, sizeof(*pResult));
}
