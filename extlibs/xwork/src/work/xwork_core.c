#include "../internal/xwork_internal.h"

static int xwork__path_char_equal(char a, char b)
{
    if (a == '/' || a == '\\')
        a = '/';
    if (b == '/' || b == '\\')
        b = '/';
#if defined(_WIN32)
    return tolower((unsigned char)a) == tolower((unsigned char)b);
#else
    return a == b;
#endif
}

char *xwork__strdup(const char *sText)
{
    size_t iLen;
    char *sCopy;
    if (!sText)
        return NULL;
    iLen = strlen(sText);
    sCopy = (char *)malloc(iLen + 1u);
    if (sCopy)
        memcpy(sCopy, sText, iLen + 1u);
    return sCopy;
}

char *xwork__strndup(const char *sText, size_t iLen)
{
    char *sCopy;
    if (!sText || !iLen)
        return NULL;
    sCopy = (char *)malloc(iLen + 1u);
    if (sCopy)
    {
        memcpy(sCopy, sText, iLen);
        sCopy[iLen] = '\0';
    }
    return sCopy;
}

bool xwork__replace(char **ppDst, const char *sText)
{
    char *sCopy = sText ? xwork__strdup(sText) : NULL;
    if (sText && !sCopy)
        return false;
    free(*ppDst);
    *ppDst = sCopy;
    return true;
}

void xworkErrorInit(xwork_error *pError)
{
    if (!pError)
        return;
    memset(pError, 0, sizeof(*pError));
    xllmErrorInit(&pError->tModelError);
}

const char *xworkErrorCodeName(xwork_error_code eCode)
{
    switch (eCode)
    {
    case XWORK_ERROR_NONE:
        return "none";
    case XWORK_ERROR_INVALID_ARGUMENT:
        return "invalid_argument";
    case XWORK_ERROR_OUT_OF_MEMORY:
        return "out_of_memory";
    case XWORK_ERROR_MODEL:
        return "model";
    case XWORK_ERROR_TOOL:
        return "tool";
    case XWORK_ERROR_POLICY:
        return "policy";
    case XWORK_ERROR_IO:
        return "io";
    case XWORK_ERROR_CONTEXT:
        return "context";
    case XWORK_ERROR_LOOP_GUARD:
        return "loop_guard";
    case XWORK_ERROR_CANCELLED:
        return "cancelled";
    case XWORK_ERROR_TIMEOUT:
        return "timeout";
    case XWORK_ERROR_LIMIT:
        return "limit";
    case XWORK_ERROR_RECOVERY_REQUIRED:
        return "recovery_required";
    default:
        return "unknown";
    }
}

void xwork__set_error(xwork_error *pError, xwork_error_code eCode, const char *sMessage)
{
    if (!pError)
        return;
    xworkErrorInit(pError);
    pError->eCode = eCode;
    if (sMessage)
    {
        snprintf(pError->sMessage, sizeof(pError->sMessage), "%s", sMessage);
    }
}

void xwork__copy_model_error(xwork_error *pError, const xllm_error *pModelError)
{
    if (!pError)
        return;
    xwork__set_error(pError, XWORK_ERROR_MODEL,
                     (pModelError && pModelError->sMessage[0]) ? pModelError->sMessage
                                                               : "model call failed");
    if (pModelError)
        pError->tModelError = *pModelError;
}

void xworkToolOutputInit(xwork_tool_output *pOutput)
{
    if (pOutput)
        memset(pOutput, 0, sizeof(*pOutput));
}

void xworkToolOutputUnit(xwork_tool_output *pOutput)
{
    if (!pOutput)
        return;
    free(pOutput->sContent);
    free(pOutput->pImageBytes);
    memset(pOutput, 0, sizeof(*pOutput));
}

bool xworkToolOutputSetImage(xwork_tool_output *pOutput, const unsigned char *pData, size_t iSize,
                             const char *sMime)
{
    unsigned char *pCopy;
    if (!pOutput || !pData || !iSize || !sMime || !sMime[0])
        return false;
    pCopy = (unsigned char *)malloc(iSize);
    if (!pCopy)
        return false;
    memcpy(pCopy, pData, iSize);
    free(pOutput->pImageBytes);
    pOutput->pImageBytes = pCopy;
    pOutput->iImageSize = iSize;
    (void)snprintf(pOutput->sImageMime, sizeof(pOutput->sImageMime), "%s", sMime);
    return true;
}

bool xworkToolOutputSet(xwork_tool_output *pOutput, bool bSuccess, const char *sContent)
{
    char *sCopy;
    if (!pOutput)
        return false;
    sCopy = xwork__strdup(sContent ? sContent : "");
    if (!sCopy)
        return false;
    free(pOutput->sContent);
    pOutput->sContent = sCopy;
    pOutput->bSuccess = bSuccess;
    return true;
}

bool xwork__buf_reserve(xwork_buf *pBuf, size_t iNeed)
{
    size_t iCap;
    char *pNew;
    if (iNeed <= pBuf->iCap)
        return true;
    iCap = pBuf->iCap ? pBuf->iCap : 256u;
    while (iCap < iNeed)
    {
        if (iCap > ((size_t)-1) / 2u)
        {
            iCap = iNeed;
            break;
        }
        iCap *= 2u;
    }
    pNew = (char *)realloc(pBuf->pData, iCap);
    if (!pNew)
        return false;
    pBuf->pData = pNew;
    pBuf->iCap = iCap;
    return true;
}

bool xwork__buf_append(xwork_buf *pBuf, const void *pData, size_t iLen)
{
    if (!pBuf || (!pData && iLen))
        return false;
    if (iLen > (size_t)-1 - pBuf->iLen - 1u)
        return false;
    if (!xwork__buf_reserve(pBuf, pBuf->iLen + iLen + 1u))
        return false;
    if (iLen)
        memcpy(pBuf->pData + pBuf->iLen, pData, iLen);
    pBuf->iLen += iLen;
    pBuf->pData[pBuf->iLen] = '\0';
    return true;
}

bool xwork__buf_append_cstr(xwork_buf *pBuf, const char *sText)
{
    return xwork__buf_append(pBuf, sText ? sText : "", sText ? strlen(sText) : 0u);
}

bool xwork__buf_append_char(xwork_buf *pBuf, char ch) { return xwork__buf_append(pBuf, &ch, 1u); }

bool xwork__buf_appendf(xwork_buf *pBuf, const char *sFormat, ...)
{
    va_list tArgs;
    va_list tCopy;
    int iNeeded;
    if (!pBuf || !sFormat)
        return false;
    va_start(tArgs, sFormat);
    va_copy(tCopy, tArgs);
    iNeeded = vsnprintf(NULL, 0u, sFormat, tCopy);
    va_end(tCopy);
    if (iNeeded < 0 || !xwork__buf_reserve(pBuf, pBuf->iLen + (size_t)iNeeded + 1u))
    {
        va_end(tArgs);
        return false;
    }
    (void)vsnprintf(pBuf->pData + pBuf->iLen, (size_t)iNeeded + 1u, sFormat, tArgs);
    va_end(tArgs);
    pBuf->iLen += (size_t)iNeeded;
    return true;
}

char *xwork__buf_detach(xwork_buf *pBuf)
{
    char *pData;
    if (!pBuf)
        return NULL;
    if (!pBuf->pData)
    {
        pBuf->pData = xwork__strdup("");
        pBuf->iCap = pBuf->pData ? 1u : 0u;
    }
    pData = pBuf->pData;
    memset(pBuf, 0, sizeof(*pBuf));
    return pData;
}

void xwork__buf_unit(xwork_buf *pBuf)
{
    if (!pBuf)
        return;
    free(pBuf->pData);
    memset(pBuf, 0, sizeof(*pBuf));
}

bool xwork__json_string(xwork_buf *pBuf, const char *sText)
{
    const unsigned char *p = (const unsigned char *)(sText ? sText : "");
    if (!xwork__buf_append_char(pBuf, '"'))
        return false;
    while (*p)
    {
        char sEscape[7];
        switch (*p)
        {
        case '"':
            if (!xwork__buf_append_cstr(pBuf, "\\\""))
                return false;
            break;
        case '\\':
            if (!xwork__buf_append_cstr(pBuf, "\\\\"))
                return false;
            break;
        case '\b':
            if (!xwork__buf_append_cstr(pBuf, "\\b"))
                return false;
            break;
        case '\f':
            if (!xwork__buf_append_cstr(pBuf, "\\f"))
                return false;
            break;
        case '\n':
            if (!xwork__buf_append_cstr(pBuf, "\\n"))
                return false;
            break;
        case '\r':
            if (!xwork__buf_append_cstr(pBuf, "\\r"))
                return false;
            break;
        case '\t':
            if (!xwork__buf_append_cstr(pBuf, "\\t"))
                return false;
            break;
        default:
            if (*p < 0x20u)
            {
                snprintf(sEscape, sizeof(sEscape), "\\u%04x", (unsigned int)*p);
                if (!xwork__buf_append_cstr(pBuf, sEscape))
                    return false;
            }
            else if (!xwork__buf_append_char(pBuf, (char)*p))
                return false;
            break;
        }
        ++p;
    }
    return xwork__buf_append_char(pBuf, '"');
}

xvalue *xwork__json_parse_object(const char *sJson)
{
    xvalue *pValue;
    if (!sJson)
        return NULL;
    pValue = xrtJsonParse((xstrview){sJson, strlen(sJson)});
    if (!pValue || xrtValueType(pValue) != XVALUE_OBJECT)
    {
        if (pValue)
            xrtValueRelease(pValue);
        return NULL;
    }
    return pValue;
}

xvalue *xwork__json_get(xvalue *pObject, const char *sKey)
{
    xvalue *pValue = (pObject && xrtValueType(pObject) == XVALUE_OBJECT)
                         ? xrtValueObjectGet(pObject, (xstrview){sKey, strlen(sKey)})
                         : NULL;
    return (pValue && xrtValueType(pValue) != XVALUE_NULL) ? pValue : NULL;
}

const char *xwork__json_text(xvalue *pObject, const char *sKey)
{
    xvalue *pValue = xwork__json_get(pObject, sKey);
    xstrview tText = {0};
    return (pValue && xrtValueGetString(pValue, &tText)) ? tText.Data : NULL;
}

bool xwork__json_bool(xvalue *pObject, const char *sKey, bool bDefault, bool *pValid)
{
    xvalue *pValue = xwork__json_get(pObject, sKey);
    bool bValue;
    if (pValid)
        *pValid = true;
    if (!pValue)
        return bDefault;
    if (!xrtValueGetBool(pValue, &bValue))
    {
        if (pValid)
            *pValid = false;
        return bDefault;
    }
    return bValue;
}

uint64_t xwork__json_u64(xvalue *pObject, const char *sKey, uint64_t uDefault, bool *pValid)
{
    xvalue *pValue = xwork__json_get(pObject, sKey);
    int64_t iValue;
    if (pValid)
        *pValid = true;
    if (!pValue)
        return uDefault;
    if (!xrtValueGetInt(pValue, &iValue))
    {
        if (pValid)
            *pValid = false;
        return uDefault;
    }
    if (iValue < 0)
    {
        if (pValid)
            *pValid = false;
        return uDefault;
    }
    return (uint64_t)iValue;
}

bool xwork__path_is_inside(const char *sRoot, const char *sPath)
{
    size_t i;
    size_t iRootLen = strlen(sRoot);
    for (i = 0u; i < iRootLen; ++i)
    {
        if (!sPath[i] || !xwork__path_char_equal(sRoot[i], sPath[i]))
            return false;
    }
    return sPath[iRootLen] == '\0' || sPath[iRootLen] == '/' || sPath[iRootLen] == '\\';
}

const char *xwork__file_root_for_path(const xwork_agent *pAgent, const char *sPath)
{
    size_t i;
    if (!pAgent || !sPath)
        return NULL;
    if (sPath[0] != '@')
        return pAgent->sWorkspaceRoot;
    if (!strncmp(sPath, "@results", 8u) && (sPath[8] == '/' || !sPath[8]))
        return pAgent->pArtifactStore ? pAgent->pArtifactStore->sDirectory
                                      : pAgent->sProcessOutputDirectory;
    for (i = 0u; i < pAgent->iPathMountCount; ++i)
    {
        size_t n = strlen(pAgent->PathMounts[i].sAlias);
        if (strncmp(sPath + 1u, pAgent->PathMounts[i].sAlias, n) == 0 &&
            (sPath[n + 1u] == '/' || sPath[n + 1u] == '\0'))
            return pAgent->PathMounts[i].sDirectory;
    }
    return NULL;
}

struct xwork_file_scope
{
    volatile long iRefs;
    volatile long iAllowExternal;
};
xwork_file_scope *xworkFileScopeCreate(bool bAllowExternal)
{
    xwork_file_scope *pScope = (xwork_file_scope *)calloc(1u, sizeof(*pScope));
    if (pScope)
    {
        pScope->iRefs = 1;
        pScope->iAllowExternal = bAllowExternal ? 1 : 0;
    }
    return pScope;
}
xwork_file_scope *xworkFileScopeRef(xwork_file_scope *pScope)
{
    if (pScope)
        (void)xwork__atomic_inc(&pScope->iRefs);
    return pScope;
}
void xworkFileScopeRelease(xwork_file_scope *pScope)
{
    if (pScope && xwork__atomic_dec(&pScope->iRefs) == 0)
        free(pScope);
}
void xworkFileScopeSetExternal(xwork_file_scope *pScope, bool bAllow)
{
    if (pScope)
        xwork__atomic_store(&pScope->iAllowExternal, bAllow ? 1 : 0);
}
bool xworkFileScopeExternal(const xwork_file_scope *pScope)
{
    return pScope && xwork__atomic_load((volatile long *)&pScope->iAllowExternal) != 0;
}
bool xwork__external_paths(const xwork_agent *pAgent)
{
    return pAgent && (pAgent->pFileScope ? xworkFileScopeExternal(pAgent->pFileScope)
                                         : pAgent->bAllowExternalPaths);
}
bool xworkAgentSetFileScope(xwork_agent *pAgent, xwork_file_scope *pScope, xwork_error *pError)
{
    bool bOk = false;
    xworkErrorInit(pError);
    if (!pAgent || !pScope)
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "agent or file scope is missing");
        return false;
    }
    if (!xrtMutexLock(pAgent->pStateLock))
        return false;
    if (pAgent->bRunning || pAgent->bClosing || pAgent->uRunSequence != 0u ||
        pAgent->bFileMountsSealed)
    {
        xwork__set_error(pError, XWORK_ERROR_CONTEXT,
                         "attach file scope before executor binding or first run");
    }
    else
    {
        xwork_file_scope *pOld = pAgent->pFileScope;
        pAgent->pFileScope = xworkFileScopeRef(pScope);
        xworkFileScopeRelease(pOld);
        bOk = true;
    }
    (void)xrtMutexUnlock(pAgent->pStateLock);
    return bOk;
}

bool xworkAgentAllowExternalPaths(xwork_agent *pAgent, bool bAllow, xwork_error *pError)
{
    bool bOk = false;
    xworkErrorInit(pError);
    if (!pAgent)
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "agent is missing");
        return false;
    }
    if (!xrtMutexLock(pAgent->pStateLock))
        return false;
    if (pAgent->bRunning || pAgent->bClosing || pAgent->uRunSequence != 0u ||
        pAgent->bFileMountsSealed)
    {
        xwork__set_error(pError, XWORK_ERROR_CONTEXT,
                         "file scope must be configured before executor binding or the first run");
    }
    else
    {
        pAgent->bAllowExternalPaths = bAllow;
        bOk = true;
    }
    (void)xrtMutexUnlock(pAgent->pStateLock);
    return bOk;
}

bool xworkAgentMountDirectory(xwork_agent *pAgent, const char *sAlias, const char *sDirectory,
                              xwork_error *pError)
{
    char *sReal = NULL;
    char *sAliasCopy = NULL;
    char *sDirectoryCopy = NULL;
    size_t i, n;
    bool bOk = false;
    xworkErrorInit(pError);
    if (!pAgent || !sAlias || !sDirectory || (n = strlen(sAlias)) == 0u || n > 48u)
        goto invalid;
    for (i = 0u; i < n; ++i)
        if (!((sAlias[i] >= 'a' && sAlias[i] <= 'z') || (sAlias[i] >= '0' && sAlias[i] <= '9') ||
              sAlias[i] == '-'))
            goto invalid;
    sReal = xrtPathReal(sDirectory);
    if (!sReal || !xrtDirExists(sReal))
        goto invalid;
    sAliasCopy = xwork__strdup(sAlias);
    sDirectoryCopy = xwork__strdup(sReal);
    if (!sAliasCopy || !sDirectoryCopy)
    {
        xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "cannot copy file mount");
        goto done;
    }
    if (!xrtMutexLock(pAgent->pStateLock))
        goto done;
    if (pAgent->bRunning || pAgent->bClosing || pAgent->uRunSequence != 0u ||
        pAgent->bFileMountsSealed)
    {
        xwork__set_error(pError, XWORK_ERROR_CONTEXT,
                         "file mounts must be configured before executor binding or the first run");
    }
    else
    {
        for (i = 0u; i < pAgent->iPathMountCount; ++i)
            if (strcmp(pAgent->PathMounts[i].sAlias, sAlias) == 0)
                break;
        if (i != pAgent->iPathMountCount)
        {
            xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "duplicate file mount alias");
        }
        else if (i == 8u)
        {
            xwork__set_error(pError, XWORK_ERROR_LIMIT, "file mount limit exceeded");
        }
        else
        {
            pAgent->PathMounts[i].sAlias = sAliasCopy;
            sAliasCopy = NULL;
            pAgent->PathMounts[i].sDirectory = sDirectoryCopy;
            sDirectoryCopy = NULL;
            ++pAgent->iPathMountCount;
            bOk = true;
        }
    }
    (void)xrtMutexUnlock(pAgent->pStateLock);
    goto done;
invalid:
    xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "invalid file mount alias or directory");
done:
    xrtFree(sReal);
    free(sAliasCopy);
    free(sDirectoryCopy);
    return bOk;
}

char *xwork__resolve_path(const xwork_agent *pAgent, const char *sPath, xwork_error *pError)
{
    char *sJoined = NULL;
    char *sAbs;
    char *sCopy;
    const char *sRoot;
    bool bMounted;
    if (!pAgent || !sPath || !sPath[0])
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "tool path is empty");
        return NULL;
    }
    sRoot = xwork__file_root_for_path(pAgent, sPath);
    bMounted = sPath[0] == '@' && sRoot != NULL;
    if (bMounted)
    {
        const char *sSlash = strchr(sPath, '/');
        sPath = sSlash ? sSlash + 1u : ".";
        if (xrtPathIsAbs(sPath))
        {
            xwork__set_error(pError, XWORK_ERROR_POLICY, "absolute path in file mount");
            return NULL;
        }
    }
    else if (!sRoot)
    {
        xwork__set_error(pError, XWORK_ERROR_POLICY, "unknown file mount alias");
        return NULL;
    }
    if (xrtPathIsAbs(sPath))
    {
        sAbs = xrtPathAbs(sPath);
    }
    else
    {
        sJoined = xrtPathJoin(sRoot, sPath);
        if (!sJoined)
        {
            xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to join workspace path");
            return NULL;
        }
        sAbs = xrtPathAbs(sJoined);
        xrtFree(sJoined);
    }
    if (!sAbs)
    {
        xwork__set_error(pError, XWORK_ERROR_IO, "failed to resolve workspace path");
        return NULL;
    }
    if ((bMounted || !xwork__external_paths(pAgent)) && !xwork__path_is_inside(sRoot, sAbs))
    {
        xrtFree(sAbs);
        xwork__set_error(pError, XWORK_ERROR_POLICY, "path escapes the configured workspace");
        return NULL;
    }
    if (bMounted)
    {
        /* Do not traverse existing links in a host mount, including the final
         * file. Newly created descendants still use the ordinary atomic writer.
         * As with workspace discovery, hostile concurrent OS changes require an
         * OS sandbox; the host-selected mount is not one. */
        size_t i = strlen(sRoot);
        for (;; ++i)
        {
            if (sAbs[i] == '/' || sAbs[i] == '\\' || sAbs[i] == '\0')
            {
                char c = sAbs[i];
                xfileinfo Info;
                sAbs[i] = '\0';
                if (xrtPathStat(sAbs, false, &Info) && Info.Type == XFILE_TYPE_LINK)
                {
                    xrtFree(sAbs);
                    xwork__set_error(pError, XWORK_ERROR_POLICY,
                                     "file mount does not traverse symbolic links");
                    return NULL;
                }
                sAbs[i] = c;
                if (c == '\0')
                    break;
            }
        }
    }
    sCopy = xwork__strdup(sAbs);
    xrtFree(sAbs);
    if (!sCopy)
        xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to copy resolved path");
    return sCopy;
}

char *xwork__relative_path(const xwork_agent *pAgent, const char *sPath)
{
    char *sRelative;
    char *sCopy;
    if (!pAgent || !sPath)
        return NULL;
    {
        size_t i;
        for (i = 0u; i < pAgent->iPathMountCount; ++i)
        {
            const char *sRoot = pAgent->PathMounts[i].sDirectory;
            if (xwork__path_is_inside(sRoot, sPath))
            {
                const char *sTail = sPath + strlen(sRoot);
                size_t n;
                if (*sTail == '/' || *sTail == '\\')
                    ++sTail;
                n = strlen(pAgent->PathMounts[i].sAlias) + strlen(sTail) + 3u;
                sCopy = (char *)malloc(n);
                if (sCopy)
                {
                    size_t j;
                    snprintf(sCopy, n, *sTail ? "@%s/%s" : "@%s%s", pAgent->PathMounts[i].sAlias,
                             sTail);
                    for (j = 0u; sCopy[j]; ++j)
                        if (sCopy[j] == '\\')
                            sCopy[j] = '/';
                }
                return sCopy;
            }
        }
    }
    sRelative = xrtPathRel(pAgent->sWorkspaceRoot, sPath);
    if (!sRelative)
        return xwork__strdup(sPath);
    sCopy = xwork__strdup(sRelative);
    xrtFree(sRelative);
    return sCopy;
}

bool xwork__ensure_parent(const char *sPath)
{
    char *sDir;
    bool bOk;
    if (!sPath)
        return false;
    sDir = xrtPathParent(sPath);
    if (!sDir || !sDir[0])
    {
        if (sDir)
            xrtFree(sDir);
        return true;
    }
    bOk = xrtDirExists((str)sDir) || xrtDirCreateAll((str)sDir);
    xrtFree(sDir);
    return bOk;
}

bool xwork__parent_exists(const char *sPath)
{
    char *sDir;
    bool bExists;
    if (!sPath)
        return false;
    sDir = xrtPathParent(sPath);
    if (!sDir || !sDir[0])
    {
        if (sDir)
            xrtFree(sDir);
        return true;
    }
    bExists = xrtDirExists((str)sDir);
    xrtFree(sDir);
    return bExists;
}

bool xwork__emit(xwork_agent *pAgent, const xwork_event *pEvent)
{
    xwork_event tEvent;
    xwork_event_fn OnEvent;
    void *pEventUserData;
    bool bCancelledTerminal;
    if (!pAgent || !pEvent)
        return false;
    /* Cancellation stops new work, but its final status must reach the
     * runtime and durable callbacks after the cancel token is set. */
    bCancelledTerminal = pEvent->eKind == XWORK_EVENT_AGENT_DONE && !pEvent->bSuccess;
    if (!bCancelledTerminal && xwork__is_cancelled(pAgent))
        return false;
    tEvent = *pEvent;
    if (pAgent->pSession &&
        (tEvent.eKind == XWORK_EVENT_AGENT_START || tEvent.eKind == XWORK_EVENT_AGENT_DONE ||
         tEvent.eKind == XWORK_EVENT_MODEL_START || tEvent.eKind == XWORK_EVENT_MODEL_DONE ||
         tEvent.eKind == XWORK_EVENT_TOOL_DONE || tEvent.eKind == XWORK_EVENT_COMPACTION_DONE))
        (void)xllmSessionGetStats(pAgent->pSession, &tEvent.tSessionStats);
    tEvent.uAgentDepth = pAgent->uAgentDepth;
    tEvent.uDelegationId = pAgent->uDelegationId;
    tEvent.uParentAgentTurn = pAgent->uParentAgentTurn;
    tEvent.uParentRunId = pAgent->uParentRunId;
    if (!tEvent.uAgentId)
        tEvent.uAgentId = pAgent->uAgentId;
    if (!tEvent.uRunId)
        tEvent.uRunId = pAgent->pActiveRun ? pAgent->pActiveRun->uRunId : pAgent->uParentRunId;
    if (!tEvent.uTaskId)
        tEvent.uTaskId = pAgent->uParentTaskId;
    xwork__runtime_record_event(pAgent->pRuntime, &tEvent);
    OnEvent = pAgent->pActiveRun && pAgent->pActiveRun->OnEvent ? pAgent->pActiveRun->OnEvent
                                                                : pAgent->OnEvent;
    pEventUserData = pAgent->pActiveRun && pAgent->pActiveRun->OnEvent
                         ? pAgent->pActiveRun->pEventUserData
                         : pAgent->pEventUserData;
    if (OnEvent && (!pAgent->pCallbackLock || !xrtMutexLock(pAgent->pCallbackLock)))
        return false;
    if (OnEvent && !bCancelledTerminal && xwork__is_cancelled(pAgent))
    {
        (void)xrtMutexUnlock(pAgent->pCallbackLock);
        return false;
    }
    if (OnEvent)
        xrtClearError();
    if (OnEvent && !OnEvent(pEventUserData, &tEvent))
    {
        xerrkind Cause = xrtErrorKind(xrtGetError());
        if (Cause != XERR_NONE && Cause != XERR_CANCELLED)
            pAgent->bEventSinkFailed = true;
        xwork__atomic_store(&pAgent->iCancelled, 1);
        (void)xrtMutexUnlock(pAgent->pCallbackLock);
        return false;
    }
    if (OnEvent)
        (void)xrtMutexUnlock(pAgent->pCallbackLock);
    return true;
}

xcancel *xwork__active_cancel(const xwork_agent *pAgent)
{
    if (!pAgent)
        return NULL;
    return pAgent->pActiveRun && pAgent->pActiveRun->pCancel ? pAgent->pActiveRun->pCancel
                                                             : pAgent->pCancel;
}

uint64_t xwork__active_deadline(const xwork_agent *pAgent)
{
    uint64_t uDeadline;
    if (!pAgent)
        return XLLM_DEADLINE_NEVER;
    uDeadline = pAgent->pActiveRun ? pAgent->pActiveRun->uDeadline : XLLM_DEADLINE_NEVER;
    if (uDeadline == XLLM_DEADLINE_NEVER ||
        (pAgent->uDeadline != XLLM_DEADLINE_NEVER && pAgent->uDeadline < uDeadline))
    {
        uDeadline = pAgent->uDeadline;
    }
    return uDeadline;
}

/* Keep the failed operation's stack/result alive while the host waits.
 * No completed model/tool execution is restarted by this helper. */
bool xwork__recover(xwork_agent *Agent,const char *Stage,xwork_error *Error,
                     uint32_t Attempt,bool Safe)
{
    xwork_agent_extension_context Context={0};xwork_error PolicyError;
    if(!Agent||!Agent->OnExtension||!Safe)return false;
    if(xwork__operation_status(Agent)==XWORK_OPERATION_TIMED_OUT){xwork__set_error(Error,XWORK_ERROR_TIMEOUT,"recovery deadline exceeded");return false;}
    if(xwork__is_cancelled(Agent)){xwork__set_error(Error,XWORK_ERROR_CANCELLED,"recovery was cancelled");return false;}
    Context.ePhase=XWORK_AGENT_EXTENSION_RECOVERY;Context.sRecoveryStage=Stage;
    Context.pRecoveryError=Error;Context.uRecoveryAttempt=Attempt;Context.bRecoverySafe=Safe;
    xworkErrorInit(&PolicyError);
    if(xwork__agent_extension(Agent,&Context,&PolicyError)!=XWORK_RESULT_OK||!Context.bRecoveryHandled)return false;
    xwork_event Event={0};Event.eKind=XWORK_EVENT_RECOVERY_WAIT;Event.sText=Stage;
    xllm_session_tail Tail={0};if(xllmSessionGetTail(Agent->pSession,&Tail))Event.uAgentTurn=Tail.uTurn;
    Event.sProviderCode=Context.bRecoveryWaiting?"waiting_resource":"recovering_resource";
    if(!strcmp(Stage,"storage"))Event.sProviderCode="waiting_storage";
    if(!strncmp(Stage,"compaction",10))Event.sProviderCode="waiting_compaction";
    if(!strcmp(Stage,"context"))Event.sProviderCode="waiting_context";
    if(!strcmp(Stage,"callback"))Event.sProviderCode="waiting_callback";
    if(!strcmp(Stage,"verification"))Event.sProviderCode="waiting_verification";
    if(!strcmp(Stage,"loop_guard"))Event.sProviderCode="waiting_tools";
    Event.tDiagnostics.uAttemptCount=Attempt;Event.tDiagnostics.uRetryAfterMs=Context.uRecoveryDelayMs;
    /* A storage outage must not recursively ask the failed sink to persist
     * its own recovery notice. The original operation remains authoritative. */
    if(strcmp(Stage,"storage")&&!xwork__emit(Agent,&Event))return false;
    uint64_t Until=xllmClockUs()+(uint64_t)Context.uRecoveryDelayMs*1000u,NextProbe=xllmClockUs();
    do{
        if(xwork__operation_status(Agent)==XWORK_OPERATION_TIMED_OUT){xwork__set_error(Error,XWORK_ERROR_TIMEOUT,"recovery deadline exceeded");return false;}
        if(xwork__is_cancelled(Agent)){xwork__set_error(Error,XWORK_ERROR_CANCELLED,"recovery was cancelled");return false;}
        if(Context.bRecoveryNeedsChange&&xllmClockUs()>=NextProbe){
            Context.bRecoveryProbe=true;Context.bRecoveryReady=false;
            if(xwork__agent_extension(Agent,&Context,&PolicyError)!=XWORK_RESULT_OK)return false;
            if(Context.bRecoveryReady)break;
            NextProbe=xllmClockUs()+1000000u;
        }
        if(!Context.bRecoveryNeedsChange&&xllmClockUs()>=Until)break;
        xrtSleep(20);
    }while(true);
    if(strcmp(Stage,"storage")){Event.tDiagnostics.uRetryAfterMs=0;(void)xwork__emit(Agent,&Event);}
    return true;
}

bool xwork__save(xwork_agent *pAgent, xwork_error *pError)
{
    xllm_error tError;
    uint32_t Attempt=0;
    if (!pAgent || !pAgent->bAutoSaveSession || !pAgent->sSessionPath || !pAgent->sSessionPath[0])
        return true;
retry_checkpoint:
    if (!xwork__ensure_parent(pAgent->sSessionPath))
    {
        xwork__set_error(pError, XWORK_ERROR_IO, "failed to create session directory");
        if(xwork__recover(pAgent,"storage",pError,++Attempt,true))goto retry_checkpoint;
        return false;
    }
    xllmErrorInit(&tError);
    if (!xllmSessionCheckpoint(pAgent->pSession, pAgent->sSessionPath, &tError))
    {
        xwork__set_error(pError, XWORK_ERROR_IO,
                         tError.sMessage[0] ? tError.sMessage : "failed to save session");
        if(xwork__recover(pAgent,"storage",pError,++Attempt,true))goto retry_checkpoint;
        return false;
    }
    return true;
}

bool xworkAgentWaitRecovery(xwork_agent *Agent,const char *Stage,xwork_error *Error,uint32_t Attempt)
{
    return Stage&&Stage[0]&&xwork__recover(Agent,Stage,Error,Attempt,true);
}

bool xworkPathIsProtected(const char *sPath)
{
    /* Generalized from the readonly-subagent internal-directory check; the
     * component scan is case-insensitive and accepts both separators. */
    const char *p = sPath;
    if (!p)
        return false;
    while (*p)
    {
        const char *sStart;
        size_t iLen;
        while (*p == '/' || *p == '\\')
            ++p;
        sStart = p;
        while (*p && *p != '/' && *p != '\\')
            ++p;
        iLen = (size_t)(p - sStart);
        if (iLen == 4u && sStart[0] == '.' && tolower((unsigned char)sStart[1]) == 'g' &&
            tolower((unsigned char)sStart[2]) == 'i' && tolower((unsigned char)sStart[3]) == 't')
            return true;
        if (iLen == 6u && sStart[0] == '.' && tolower((unsigned char)sStart[1]) == 'x' &&
            tolower((unsigned char)sStart[2]) == 'c' && tolower((unsigned char)sStart[3]) == 'o' &&
            tolower((unsigned char)sStart[4]) == 'd' && tolower((unsigned char)sStart[5]) == 'e')
            return true;
    }
    return false;
}

bool xwork__agent_run_claim(xwork_agent *pAgent, xwork_run *pRun, xwork_error *pError)
{
    if (pError)
    {
        xworkErrorInit(pError);
    }
    if (!pAgent)
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "agent is null");
        return false;
    }
    if (!xrtMutexLock(pAgent->pStateLock))
    {
        xwork__set_error(pError, XWORK_ERROR_CONTEXT, "failed to lock agent state");
        return false;
    }
    if (pAgent->bClosing)
    {
        (void)xrtMutexUnlock(pAgent->pStateLock);
        xwork__set_error(pError, XWORK_ERROR_CONTEXT, "agent is closing");
        return false;
    }
    if (pAgent->bRunning)
    {
        (void)xrtMutexUnlock(pAgent->pStateLock);
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "agent is already running");
        return false;
    }
    /* Same window the built-in loop opens on its own: registry mutation and
     * concurrent runs stay rejected while an external driver (for example
     * xllmSessionRunWithTools) owns the loop. */
    pAgent->bRunning = true;
    pAgent->pActiveRun = pRun;
    (void)xrtMutexUnlock(pAgent->pStateLock);
    return true;
}

void xwork__agent_run_release(xwork_agent *pAgent, xwork_run *pRun)
{
    if (!pAgent)
    {
        return;
    }
    if (!xrtMutexLock(pAgent->pStateLock))
        return;
    if (pAgent->pActiveRun == pRun)
    {
        pAgent->pActiveRun = NULL;
        pAgent->bRunning = false;
    }
    (void)xrtMutexUnlock(pAgent->pStateLock);
}

bool xworkAgentRunBegin(xwork_agent *pAgent, xwork_error *pError)
{
    return xwork__agent_run_claim(pAgent, NULL, pError);
}

void xworkAgentRunEnd(xwork_agent *pAgent) { xwork__agent_run_release(pAgent, NULL); }

void xworkAgentConfigInit(xwork_agent_config *pConfig)
{
    if (!pConfig)
        return;
    memset(pConfig, 0, sizeof(*pConfig));
    pConfig->uDeadline = XLLM_DEADLINE_NEVER;
    pConfig->eApprovalMode = XWORK_APPROVAL_AUTO;
    pConfig->uCommandTimeoutMs = 120000u;
    pConfig->uMaxAgentTurns = 0u;
    pConfig->uRepeatedToolBatchLimit = 3u;
    pConfig->uConsecutiveFailureLimit = 5u;
    pConfig->uMaxManagedProcesses = 8u;
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

const char *xwork__canonical_tool_name(const char *Name)
{
    static const char *const Aliases[][2] = {
        {"poll", "status"},       {"task_status", "status"}, {"ask_user", "ask"},
        {"web_search", "search"}, {"web_open", "open"},      {"web_find", "find"},
        {"tool_search", "tools"}, {"tool_load", "load"},     {"mcp_read", "fetch"}};
    if (!Name)
        return Name;
    for (size_t i = 0; i < sizeof(Aliases) / sizeof(*Aliases); i++)
        if (!strcmp(Name, Aliases[i][0]))
            return Aliases[i][1];
    return Name;
}

const xwork_tool_entry *xwork__find_tool(const xwork_agent *pAgent, const char *sName)
{
    size_t i;
    if (!pAgent || !sName)
        return NULL;
    sName = xwork__canonical_tool_name(sName);
    for (i = 0u; i < pAgent->iToolCount; ++i)
    {
        if (strcmp(pAgent->pTools[i].sName, sName) == 0)
            return &pAgent->pTools[i];
    }
    return NULL;
}

bool xworkAgentRegisterTool(xwork_agent *pAgent, const xwork_tool_definition *pDefinition,
                            xwork_error *pError)
{
    xwork_tool_catalog *pNext;
    xwork_tool_catalog *pOld;
    xworkErrorInit(pError);
    if (!pAgent || !pAgent->pStateLock || !xrtMutexLock(pAgent->pStateLock))
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "agent is null or unavailable");
        return false;
    }
    if (pAgent->bRunning || pAgent->bClosing)
    {
        (void)xrtMutexUnlock(pAgent->pStateLock);
        xwork__set_error(pError, XWORK_ERROR_CONTEXT,
                         "tool catalog cannot change while the agent is running or closing");
        return false;
    }
    pNext = xwork__tool_catalog_add(pAgent->pToolCatalog,
                                    xwork__runtime_next_catalog_generation(pAgent->pRuntime),
                                    pDefinition, pError);
    if (!pNext)
    {
        (void)xrtMutexUnlock(pAgent->pStateLock);
        return false;
    }
    if (pNext->iToolCount > pAgent->pRuntime->uMaxCatalogTools)
    {
        xworkToolCatalogRelease(pNext);
        (void)xrtMutexUnlock(pAgent->pStateLock);
        xwork__set_error(pError, XWORK_ERROR_LIMIT, "agent tool catalog limit was exceeded");
        return false;
    }
    pOld = pAgent->pToolCatalog;
    pAgent->pToolCatalog = pNext;
    pAgent->pTools = pNext->pTools;
    pAgent->iToolCount = pNext->iToolCount;
    pAgent->uToolRegistryGeneration = pNext->uGeneration;
    (void)xrtMutexUnlock(pAgent->pStateLock);
    xworkToolCatalogRelease(pOld);
    return true;
}

bool xworkAgentConfigureToolConcurrency(xwork_agent *pAgent,
                                        const xwork_tool_concurrency_config *pConfig,
                                        xwork_error *pError)
{
    xwork_tool_catalog *pNext;
    xwork_tool_catalog *pOld;
    xworkErrorInit(pError);
    if (!pAgent || !pAgent->pStateLock || !xrtMutexLock(pAgent->pStateLock))
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "agent is null or unavailable");
        return false;
    }
    if (pAgent->bRunning || pAgent->bClosing)
    {
        (void)xrtMutexUnlock(pAgent->pStateLock);
        xwork__set_error(pError, XWORK_ERROR_CONTEXT,
                         "tool concurrency cannot change while the agent is running or closing");
        return false;
    }
    pNext = xwork__tool_catalog_configure_concurrency(
        pAgent->pToolCatalog, xwork__runtime_next_catalog_generation(pAgent->pRuntime), pConfig,
        pError);
    if (!pNext)
    {
        (void)xrtMutexUnlock(pAgent->pStateLock);
        return false;
    }
    pOld = pAgent->pToolCatalog;
    pAgent->pToolCatalog = pNext;
    pAgent->pTools = pNext->pTools;
    pAgent->iToolCount = pNext->iToolCount;
    pAgent->uToolRegistryGeneration = pNext->uGeneration;
    (void)xrtMutexUnlock(pAgent->pStateLock);
    xworkToolCatalogRelease(pOld);
    return true;
}

bool xworkAgentUnregisterTool(xwork_agent *pAgent, const char *sName, xwork_error *pError)
{
    xwork_tool_catalog *pNext;
    xwork_tool_catalog *pOld;
    xworkErrorInit(pError);
    if (!pAgent || !pAgent->pStateLock || !xrtMutexLock(pAgent->pStateLock))
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "agent is null or unavailable");
        return false;
    }
    if (pAgent->bRunning || pAgent->bClosing)
    {
        (void)xrtMutexUnlock(pAgent->pStateLock);
        xwork__set_error(pError, XWORK_ERROR_CONTEXT,
                         "tool catalog cannot change while the agent is running or closing");
        return false;
    }
    pNext = xwork__tool_catalog_remove_name(
        pAgent->pToolCatalog, xwork__runtime_next_catalog_generation(pAgent->pRuntime), sName,
        pError);
    if (!pNext)
    {
        (void)xrtMutexUnlock(pAgent->pStateLock);
        return false;
    }
    pOld = pAgent->pToolCatalog;
    pAgent->pToolCatalog = pNext;
    pAgent->pTools = pNext->pTools;
    pAgent->iToolCount = pNext->iToolCount;
    pAgent->uToolRegistryGeneration = pNext->uGeneration;
    (void)xrtMutexUnlock(pAgent->pStateLock);
    xworkToolCatalogRelease(pOld);
    return true;
}

bool xworkAgentUnregisterToolsBySource(xwork_agent *pAgent, const char *sSource, size_t *piRemoved,
                                       xwork_error *pError)
{
    return xworkAgentReplaceToolsBySource(pAgent, sSource, NULL, 0u, piRemoved, pError);
}

bool xworkAgentReplaceToolsBySource(xwork_agent *pAgent, const char *sSource,
                                    const xwork_tool_definition *pDefinitions, size_t iCount,
                                    size_t *piRemoved, xwork_error *pError)
{
    xwork_tool_catalog *pNext;
    xwork_tool_catalog *pOld;
    xworkErrorInit(pError);
    if (piRemoved)
        *piRemoved = 0u;
    if (!pAgent || !pAgent->pStateLock || !xrtMutexLock(pAgent->pStateLock))
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "agent is null or unavailable");
        return false;
    }
    if (pAgent->bRunning || pAgent->bClosing)
    {
        (void)xrtMutexUnlock(pAgent->pStateLock);
        xwork__set_error(pError, XWORK_ERROR_CONTEXT,
                         "tool catalog cannot change while the agent is running or closing");
        return false;
    }
    if (iCount > pAgent->pRuntime->uMaxCatalogTools)
    {
        (void)xrtMutexUnlock(pAgent->pStateLock);
        xwork__set_error(pError, XWORK_ERROR_LIMIT,
                         "agent tool replacement exceeds the catalog limit");
        return false;
    }
    pNext = xwork__tool_catalog_replace_source(
        pAgent->pToolCatalog, xwork__runtime_next_catalog_generation(pAgent->pRuntime), sSource,
        pDefinitions, iCount, piRemoved, pError);
    if (!pNext)
    {
        (void)xrtMutexUnlock(pAgent->pStateLock);
        return false;
    }
    if (pNext->iToolCount > pAgent->pRuntime->uMaxCatalogTools)
    {
        xworkToolCatalogRelease(pNext);
        (void)xrtMutexUnlock(pAgent->pStateLock);
        if (piRemoved)
            *piRemoved = 0u;
        xwork__set_error(pError, XWORK_ERROR_LIMIT, "agent tool catalog limit was exceeded");
        return false;
    }
    pOld = pAgent->pToolCatalog;
    pAgent->pToolCatalog = pNext;
    pAgent->pTools = pNext->pTools;
    pAgent->iToolCount = pNext->iToolCount;
    pAgent->uToolRegistryGeneration = pNext->uGeneration;
    (void)xrtMutexUnlock(pAgent->pStateLock);
    xworkToolCatalogRelease(pOld);
    return true;
}

xwork_agent *xwork__agent_create_bound(const xwork_agent_config *pConfig, xwork_runtime *pRuntime,
                                       xwork_agent_definition *pDefinition, xwork_error *pError)
{
    xwork_agent *pAgent;
    char *sRoot;
    xllm_session_stats tStats;
    uint64_t uTurn;
    if (!pConfig || !pRuntime || !pDefinition || !pConfig->pSession || !pConfig->sWorkspaceRoot ||
        !pConfig->sWorkspaceRoot[0] ||
        ((pConfig->OnOwnerRetain != NULL) != (pConfig->OnOwnerRelease != NULL)) ||
        pConfig->uMaxParallelTools > 64u || pConfig->uMaxSubagentDepth > 16u ||
        pConfig->uMaxConcurrentSubagents > 64u)
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
                         "agent requires a runtime, definition, session, and workspace root");
        return NULL;
    }
    sRoot = xrtPathAbs(pConfig->sWorkspaceRoot);
    if (!sRoot || !xrtDirExists((str)sRoot))
    {
        if (sRoot)
            xrtFree(sRoot);
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "workspace root does not exist");
        return NULL;
    }
    pAgent = (xwork_agent *)calloc(1u, sizeof(*pAgent));
    if (!pAgent)
    {
        xrtFree(sRoot);
        xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to allocate agent");
        return NULL;
    }
    pAgent->iRefs = 1;
    pAgent->pStateLock = xrtMutexCreate();
    pAgent->pCallbackLock = xrtMutexCreate();
    pAgent->pRuntime = xworkRuntimeRef(pRuntime);
    pAgent->pDefinition = xworkAgentDefinitionRef(pDefinition);
    pAgent->pToolCatalog = xworkRuntimeToolCatalogSnapshot(pRuntime);
    pAgent->pSubagentCatalog = xwork__subagent_catalog_empty(pRuntime);
    pAgent->pDelegationScope = xwork__delegation_scope_create(
        pConfig->uMaxSubagentDepth ? pConfig->uMaxSubagentDepth : 1u,
        pConfig->uMaxConcurrentSubagents ? pConfig->uMaxConcurrentSubagents : 4u);
    pAgent->bAllowSubagentDelegation = true;
    if (pAgent->pToolCatalog)
    {
        pAgent->pTools = pAgent->pToolCatalog->pTools;
        pAgent->iToolCount = pAgent->pToolCatalog->iToolCount;
        pAgent->uToolRegistryGeneration = pAgent->pToolCatalog->uGeneration;
    }
    if (pAgent->pRuntime && xrtMutexLock(pAgent->pRuntime->pLock))
    {
        pAgent->uAgentId = ++pAgent->pRuntime->uNextAgentId;
        if (pAgent->uAgentId == 0u)
            pAgent->uAgentId = ++pAgent->pRuntime->uNextAgentId;
        (void)xrtMutexUnlock(pAgent->pRuntime->pLock);
    }
    if (!pAgent->pStateLock || !pAgent->pCallbackLock || !pAgent->pRuntime ||
        !pAgent->pDefinition || !pAgent->pToolCatalog || !pAgent->pSubagentCatalog ||
        !pAgent->pDelegationScope || !pAgent->uAgentId)
    {
        xrtFree(sRoot);
        xworkAgentDestroy(pAgent);
        xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to initialize agent ownership");
        return NULL;
    }
    pAgent->pClient = pConfig->pClient;
    pAgent->pSession = pConfig->pSession;
    pAgent->sWorkspaceRoot = xwork__strdup(sRoot);
    pAgent->sSystemPrompt = xwork__strdup(
        pConfig->sSystemPrompt
            ? pConfig->sSystemPrompt
            : "You are a careful coding agent. Inspect the workspace, use tools to make changes, "
              "run relevant tests, and continue until the user's task is complete.");
    pAgent->sSessionPath = pConfig->sSessionPath ? xwork__strdup(pConfig->sSessionPath) : NULL;
    pAgent->sArtifactDirectory = xwork__strdup(
        pConfig->sArtifactDirectory ? pConfig->sArtifactDirectory : ".xcode/artifacts");
    if (pAgent->sArtifactDirectory)
    {
        char *Absolute = xrtPathIsAbs(pAgent->sArtifactDirectory)
                             ? xrtPathAbs(pAgent->sArtifactDirectory)
                             : xrtPathJoin(pAgent->sWorkspaceRoot, pAgent->sArtifactDirectory);
        pAgent->sProcessOutputDirectory = Absolute ? xwork__strdup(Absolute) : NULL;
        if (Absolute)
            xrtFree(Absolute);
    }
    pAgent->pArtifactStore = xworkArtifactStoreRef(pConfig->pArtifactStore);
    pAgent->sModel = pConfig->sModel ? xwork__strdup(pConfig->sModel) : NULL;
    pAgent->sReasoningEffort =
        pConfig->sReasoningEffort ? xwork__strdup(pConfig->sReasoningEffort) : NULL;
    pAgent->pCancel = xrtCancelChild(pConfig->pCancel);
    pAgent->uDeadline = pConfig->uDeadline;
    xrtFree(sRoot);
    if (!pAgent->sWorkspaceRoot || !pAgent->sSystemPrompt || !pAgent->sArtifactDirectory ||
        !pAgent->sProcessOutputDirectory || (pConfig->sSessionPath && !pAgent->sSessionPath) ||
        (pConfig->sModel && !pAgent->sModel) ||
        (pConfig->sReasoningEffort && !pAgent->sReasoningEffort) || !pAgent->pCancel)
    {
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
    pAgent->eEolPolicy = pConfig->eEolPolicy;
    pAgent->pHookUserData = pConfig->pHookUserData;
    pAgent->OnEvent = pConfig->OnEvent;
    pAgent->pEventUserData = pConfig->pEventUserData;
    pAgent->OnModelComplete = pConfig->OnModelComplete;
    pAgent->pModelUserData = pConfig->pModelUserData;
    pAgent->OnExtension = pConfig->OnExtension;
    pAgent->pExtensionUserData = pConfig->pExtensionUserData;
    pAgent->pOwnerUserData = pConfig->pOwnerUserData;
    pAgent->OnOwnerRetain = pConfig->OnOwnerRetain;
    pAgent->OnOwnerRelease = pConfig->OnOwnerRelease;
    if (pAgent->OnOwnerRetain != NULL)
    {
        if (!pAgent->OnOwnerRetain(pAgent->pOwnerUserData))
        {
            xworkAgentDestroy(pAgent);
            xwork__set_error(pError, XWORK_ERROR_CONTEXT,
                             "agent dependency owner is no longer retainable");
            return NULL;
        }
        pAgent->bOwnerRetained = true;
    }
    pAgent->uCommandTimeoutMs = pConfig->uCommandTimeoutMs ? pConfig->uCommandTimeoutMs : 120000u;
    pAgent->uMaxAgentTurns = pConfig->uMaxAgentTurns;
    pAgent->uRepeatedToolBatchLimit =
        pConfig->uRepeatedToolBatchLimit ? pConfig->uRepeatedToolBatchLimit : 3u;
    pAgent->uConsecutiveFailureLimit =
        pConfig->uConsecutiveFailureLimit ? pConfig->uConsecutiveFailureLimit : 5u;
    pAgent->uMaxManagedProcesses =
        pConfig->uMaxManagedProcesses ? pConfig->uMaxManagedProcesses : 8u;
    pAgent->uMaxParallelTools = pConfig->uMaxParallelTools ? pConfig->uMaxParallelTools : 4u;
    pAgent->uMaxSubagentDepth = pConfig->uMaxSubagentDepth ? pConfig->uMaxSubagentDepth : 1u;
    pAgent->uMaxConcurrentSubagents =
        pConfig->uMaxConcurrentSubagents ? pConfig->uMaxConcurrentSubagents : 4u;
    pAgent->uCompletionVerificationRetries =
        pConfig->uCompletionVerificationRetries ? pConfig->uCompletionVerificationRetries : 2u;
    pAgent->uCompactionQualityRetries = pConfig->uCompactionQualityRetries;
    pAgent->iMaxInlineToolBytes =
        pConfig->iMaxInlineToolBytes ? pConfig->iMaxInlineToolBytes : 64u * 1024u;
    pAgent->iMaxCapturedCommandBytes =
        pConfig->iMaxCapturedCommandBytes ? pConfig->iMaxCapturedCommandBytes : 8u * 1024u * 1024u;
    pAgent->bAutoSaveSession = pConfig->bAutoSaveSession;
    pAgent->bAllowArtifactWrites = pConfig->bAllowArtifactWrites;
    pAgent->bRequireVerificationAfterWrite = pConfig->bRequireVerificationAfterWrite;

    if (!xllmSessionGetStats(pAgent->pSession, &tStats))
    {
        xworkAgentDestroy(pAgent);
        xwork__set_error(pError, XWORK_ERROR_CONTEXT, "failed to inspect session");
        return NULL;
    }
    if (pConfig->bInjectSystemPrompt && tStats.uEntryCount == 0u)
    {
        uTurn = xllmSessionBeginTurn(pAgent->pSession);
        if (!uTurn || !xllmSessionAddText(pAgent->pSession, uTurn, XLLM_ROLE_SYSTEM,
                                          pAgent->sSystemPrompt, XLLM_SESSION_ENTRY_PINNED))
        {
            xworkAgentDestroy(pAgent);
            xwork__set_error(pError, XWORK_ERROR_CONTEXT, "failed to initialize system context");
            return NULL;
        }
    }
    if (pConfig->bRegisterBuiltinTools && !xworkAgentRegisterBuiltinTools(pAgent, pError))
    {
        xworkAgentDestroy(pAgent);
        return NULL;
    }
    return pAgent;
}

static void xwork__agent_free(xwork_agent *pAgent)
{
    if (pAgent->OnExtension && pAgent->bOwnerRetained)
    {
        xwork_agent_extension_context Context = {0};
        Context.ePhase = XWORK_AGENT_EXTENSION_RELEASE;
        xwork_error Error;
        xworkErrorInit(&Error);
        (void)xwork__agent_extension(pAgent, &Context, &Error);
    }
    size_t i;
    if (!pAgent)
        return;
    for (i = 0u; i < pAgent->iPathMountCount; ++i)
    {
        free(pAgent->PathMounts[i].sAlias);
        free(pAgent->PathMounts[i].sDirectory);
    }
    xworkFileScopeRelease(pAgent->pFileScope);
    xworkToolCatalogRelease(pAgent->pToolCatalog);
    xworkSubagentCatalogRelease(pAgent->pSubagentCatalog);
    xwork__delegation_scope_release(pAgent->pDelegationScope);
    free(pAgent->sWorkspaceRoot);
    for (size_t iScope = 0u; iScope < pAgent->iMcpServerScopeCount; ++iScope)
        free(pAgent->psMcpServerScope[iScope]);
    free(pAgent->psMcpServerScope);
    free(pAgent->sSystemPrompt);
    free(pAgent->sSessionPath);
    free(pAgent->sArtifactDirectory);
    free(pAgent->sProcessOutputDirectory);
    xworkArtifactStoreRelease(pAgent->pArtifactStore);
    free(pAgent->sModel);
    free(pAgent->sReasoningEffort);
    xrtCancelDestroy(pAgent->pCancel);
    if (pAgent->pStateLock)
        (void)xrtMutexDestroy(pAgent->pStateLock);
    if (pAgent->pCallbackLock)
        (void)xrtMutexDestroy(pAgent->pCallbackLock);
    xworkAgentDefinitionRelease(pAgent->pDefinition);
    xworkRuntimeRelease(pAgent->pRuntime);
    if (pAgent->bOwnerRetained && pAgent->OnOwnerRelease)
        pAgent->OnOwnerRelease(pAgent->pOwnerUserData);
    free(pAgent);
}

xwork_agent *xwork__agent_ref(xwork_agent *pAgent)
{
    if (!pAgent || xwork__atomic_inc(&pAgent->iRefs) <= 1)
        return NULL;
    return pAgent;
}

void xwork__agent_release(xwork_agent *pAgent)
{
    if (!pAgent)
        return;
    if (xwork__atomic_dec(&pAgent->iRefs) == 0)
        xwork__agent_free(pAgent);
}

void xworkAgentDestroy(xwork_agent *pAgent)
{
    if (!pAgent)
        return;
    if (pAgent->pStateLock && xrtMutexLock(pAgent->pStateLock))
    {
        pAgent->bClosing = true;
        (void)xrtMutexUnlock(pAgent->pStateLock);
    }
    xwork__agent_release(pAgent);
}

size_t xworkAgentToolCount(const xwork_agent *pAgent) { return pAgent ? pAgent->iToolCount : 0u; }

bool xworkAgentToolAt(const xwork_agent *pAgent, size_t iIndex, xwork_tool_info *pInfo)
{
    const xwork_tool_entry *pTool;
    if (!pAgent || !pInfo || iIndex >= pAgent->iToolCount)
        return false;
    pTool = &pAgent->pTools[iIndex];
    memset(pInfo, 0, sizeof(*pInfo));
    pInfo->sName = pTool->sName;
    pInfo->sDescription = pTool->sDescription;
    pInfo->sParametersJson = pTool->sParametersJson;
    pInfo->sSource = pTool->sSource;
    pInfo->bStrict = pTool->bStrict;
    pInfo->uEffects = pTool->uEffects;
    pInfo->uCatalogGeneration = pAgent->uToolRegistryGeneration;
    pInfo->bDescribesPermissions = pTool->OnDescribePermissions != NULL;
    pInfo->bUsesResultWriter = pTool->OnExecuteV2 != NULL;
    pInfo->iMaxResultBytes = pTool->iMaxResultBytes;
    pInfo->bParallelSafe = pTool->bParallelSafe;
    pInfo->sSerialGroup = pTool->sSerialGroup;
    return true;
}

uint64_t xworkAgentToolRegistryGeneration(const xwork_agent *pAgent)
{
    return pAgent ? pAgent->uToolRegistryGeneration : 0u;
}

xwork_tool_catalog *xworkAgentToolCatalogSnapshot(xwork_agent *pAgent)
{
    xwork_tool_catalog *pCatalog = NULL;
    if (!pAgent || !pAgent->pStateLock || !xrtMutexLock(pAgent->pStateLock))
    {
        return NULL;
    }
    pCatalog = xworkToolCatalogRef(pAgent->pToolCatalog);
    (void)xrtMutexUnlock(pAgent->pStateLock);
    return pCatalog;
}

bool xworkAgentAdoptRuntimeToolCatalog(xwork_agent *pAgent, xwork_error *pError)
{
    xwork_tool_catalog *pNext;
    xwork_tool_catalog *pOld;
    xworkErrorInit(pError);
    if (!pAgent)
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "agent is null");
        return false;
    }
    pNext = xworkRuntimeToolCatalogSnapshot(pAgent->pRuntime);
    if (!pNext)
    {
        xwork__set_error(pError, XWORK_ERROR_CONTEXT,
                         "failed to snapshot the runtime tool catalog");
        return false;
    }
    if (!xrtMutexLock(pAgent->pStateLock))
    {
        xworkToolCatalogRelease(pNext);
        xwork__set_error(pError, XWORK_ERROR_CONTEXT, "failed to lock agent state");
        return false;
    }
    if (pAgent->bRunning || pAgent->bClosing)
    {
        (void)xrtMutexUnlock(pAgent->pStateLock);
        xworkToolCatalogRelease(pNext);
        xwork__set_error(pError, XWORK_ERROR_CONTEXT,
                         "tool catalog cannot change while the agent is running or closing");
        return false;
    }
    pOld = pAgent->pToolCatalog;
    pAgent->pToolCatalog = pNext;
    pAgent->pTools = pNext->pTools;
    pAgent->iToolCount = pNext->iToolCount;
    pAgent->uToolRegistryGeneration = pNext->uGeneration;
    (void)xrtMutexUnlock(pAgent->pStateLock);
    xworkToolCatalogRelease(pOld);
    return true;
}

bool xworkAgentCancel(xwork_agent *pAgent)
{
    if (!pAgent)
        return false;
    xwork__atomic_store(&pAgent->iCancelled, 1);
    if (pAgent->pCancel)
    {
        (void)xrtCancelRequest(pAgent->pCancel);
    }
    return true;
}

const char *xworkAgentWorkspaceRoot(const xwork_agent *pAgent)
{
    return pAgent ? pAgent->sWorkspaceRoot : NULL;
}

xllm_result xwork__model_complete(xwork_agent *pAgent, const xllm_request *pRequest,
                                  const xllm_stream_callbacks *pCallbacks,
                                  xllm_response **ppResponse, xllm_error *pError)
{
    if (pAgent->OnExtension)
    {
        xwork_agent_extension_context Context = {0};
        xwork_error Error;
        Context.ePhase = XWORK_AGENT_EXTENSION_MODEL_DISPATCH;
        Context.pRequest = (xllm_request *)pRequest;
        xworkErrorInit(&Error);
        if (xwork__agent_extension(pAgent, &Context, &Error) != XWORK_RESULT_OK)
        {
            if (pError)
            {
                xllmErrorInit(pError);
                pError->eCode = XLLM_ERROR_HOOK;
                snprintf(pError->sMessage, sizeof(pError->sMessage), "%.*s",
                         (int)sizeof(pError->sMessage) - 1, Error.sMessage);
            }
            return XLLM_RESULT_ERROR;
        }
    }
    if (pAgent->OnModelComplete)
    {
        return pAgent->OnModelComplete(pAgent->pModelUserData, pRequest, pCallbacks, ppResponse,
                                       pError);
    }
    if (pAgent->pClient)
    {
        return xllmClientComplete(pAgent->pClient, pRequest, pCallbacks, ppResponse, pError);
    }
    return xllmSessionDispatchRequest(pAgent->pSession, pRequest, pCallbacks, ppResponse, pError);
}

void xworkRunResultUnit(xwork_run_result *pResult)
{
    if (!pResult)
        return;
    free(pResult->sFinalText);
    memset(pResult, 0, sizeof(*pResult));
}

xwork_result xwork__agent_extension(xwork_agent *Agent, xwork_agent_extension_context *Context,
                                    xwork_error *Error)
{
    if (!Agent || !Agent->OnExtension)
        return XWORK_RESULT_OK;
    Context->pAgent = Agent;
    Context->pSession = Agent->pSession;
    Context->sDefinitionId = Agent->pDefinition ? Agent->pDefinition->sId : NULL;
    Context->pCancel = xwork__active_cancel(Agent);
    Context->uDeadline = xwork__active_deadline(Agent);
    return Agent->OnExtension(Agent->pExtensionUserData, Context, Error);
}
