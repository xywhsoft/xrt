#include "../internal/xwork_internal.h"

/* Runtime-owned artifact metadata and bounded replay/audit events. */

#define XWORK_EVENT_METADATA_MAX 4096u
#define XWORK_ARTIFACT_PUBLISH_ATTEMPTS 128u
#define XWORK_ARTIFACT_TEMP_ATTEMPTS 16u

xwork_artifact_store *xworkArtifactStoreCreate(xroot Directory, xwork_error *pError)
{
    xwork_artifact_store *pStore;
    const char *sDirectory;
    xworkErrorInit(pError);
    if (!Directory || !(sDirectory = xrtRootPath(Directory)) || !sDirectory[0])
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
                         "an anchored artifact directory is required");
        return NULL;
    }
    pStore = (xwork_artifact_store *)calloc(1u, sizeof(*pStore));
    if (!pStore)
    {
        xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to allocate artifact store");
        return NULL;
    }
    pStore->iRefs = 1;
    pStore->pDirectory = xrtRootOpenIn(Directory, ".");
    if (!pStore->pDirectory)
    {
        xwork__set_error(pError, XWORK_ERROR_IO, "failed to clone anchored artifact directory");
        xworkArtifactStoreRelease(pStore);
        return NULL;
    }
    pStore->sDirectory = xrtPathAbs(sDirectory);
    if (!pStore->sDirectory)
    {
        xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY,
                         "failed to copy artifact display directory");
        xworkArtifactStoreRelease(pStore);
        return NULL;
    }
    return pStore;
}

xwork_artifact_store *xworkArtifactStoreRef(xwork_artifact_store *Store)
{
    if (!Store || xwork__atomic_inc(&Store->iRefs) <= 1)
        return NULL;
    return Store;
}

void xworkArtifactStoreRelease(xwork_artifact_store *Store)
{
    if (!Store || xwork__atomic_dec(&Store->iRefs) != 0)
        return;
    if (Store->pDirectory)
        (void)xrtRootClose(Store->pDirectory);
    if (Store->sDirectory)
        xrtFree(Store->sDirectory);
    free(Store);
}

static char *xwork__observability_copy_string(const char *sValue, size_t iLimit, bool *pbTruncated,
                                              size_t *piBytes)
{
    size_t iLength;
    size_t iKeep;
    char *sCopy;
    if (!sValue)
        return NULL;
    iLength = strlen(sValue);
    iKeep = iLength > iLimit ? iLimit : iLength;
    while (iKeep && ((unsigned char)sValue[iKeep] & 0xc0u) == 0x80u)
        --iKeep;
    sCopy = xwork__strndup(sValue, iKeep);
    if (!sCopy)
        return NULL;
    if (iKeep != iLength && pbTruncated)
        *pbTruncated = true;
    if (piBytes)
        *piBytes += iKeep + 1u;
    return sCopy;
}

static char *xwork__observability_copy_text(const char *pText, size_t iLength, size_t iLimit,
                                            bool *pbTruncated, size_t *piBytes)
{
    static const char sMarker[] = "\n...[event text truncated]...\n";
    const size_t iMarker = sizeof(sMarker) - 1u;
    char *sCopy;
    size_t iHead;
    size_t iTail;
    if (!pText || !iLength)
        return NULL;
    if (iLength <= iLimit)
    {
        sCopy = xwork__strndup(pText, iLength);
        if (sCopy && piBytes)
            *piBytes += iLength + 1u;
        return sCopy;
    }
    if (pbTruncated)
        *pbTruncated = true;
    sCopy = (char *)malloc(iLimit + 1u);
    if (!sCopy)
        return NULL;
    if (iLimit <= iMarker)
    {
        while (iLimit && ((unsigned char)pText[iLimit] & 0xc0u) == 0x80u)
            --iLimit;
        memcpy(sCopy, pText, iLimit);
    }
    else
    {
        iHead = (iLimit - iMarker) * 2u / 3u;
        iTail = iLimit - iMarker - iHead;
        while (iHead && ((unsigned char)pText[iHead] & 0xc0u) == 0x80u)
            --iHead;
        while (iTail && ((unsigned char)pText[iLength - iTail] & 0xc0u) == 0x80u)
            --iTail;
        memcpy(sCopy, pText, iHead);
        memcpy(sCopy + iHead, sMarker, iMarker);
        memcpy(sCopy + iHead + iMarker, pText + iLength - iTail, iTail);
        iLimit = iHead + iMarker + iTail;
    }
    sCopy[iLimit] = '\0';
    if (piBytes)
        *piBytes += iLimit + 1u;
    return sCopy;
}

static void xwork__event_entry_unit(xwork_event_entry *pEntry)
{
    if (!pEntry)
        return;
    free((void *)pEntry->tEvent.sText);
    free((void *)pEntry->tEvent.sToolName);
    free((void *)pEntry->tEvent.sToolCallId);
    free((void *)pEntry->tEvent.sArtifactPath);
    free((void *)pEntry->tEvent.sModel);
    free((void *)pEntry->tEvent.sProviderRequestId);
    free((void *)pEntry->tEvent.sProviderCode);
    free((void *)pEntry->tEvent.sProviderMessage);
    free((void *)pEntry->tEvent.sFinishReason);
    free((void *)pEntry->tEvent.sRequestFingerprint);
    free(pEntry);
}

static xwork_event_entry *xwork__event_entry_ref(xwork_event_entry *pEntry)
{
    if (!pEntry || xwork__atomic_inc(&pEntry->iRefs) <= 1)
        return NULL;
    return pEntry;
}

static void xwork__event_entry_release(xwork_event_entry *pEntry)
{
    if (pEntry && xwork__atomic_dec(&pEntry->iRefs) == 0)
        xwork__event_entry_unit(pEntry);
}

static xwork_event_entry *xwork__event_entry_create(const xwork_event *pEvent, size_t iTextLimit)
{
    xwork_event_entry *pEntry;
    bool bMetadataTruncated = false;
    size_t iBytes = sizeof(*pEntry);
    if (!pEvent)
        return NULL;
    pEntry = (xwork_event_entry *)calloc(1u, sizeof(*pEntry));
    if (!pEntry)
        return NULL;
    pEntry->iRefs = 1;
    pEntry->tEvent = *pEvent;
    pEntry->tEvent.sText = NULL;
    pEntry->tEvent.sToolName = NULL;
    pEntry->tEvent.sToolCallId = NULL;
    pEntry->tEvent.sArtifactPath = NULL;
    pEntry->tEvent.sModel = NULL;
    pEntry->tEvent.sProviderRequestId = NULL;
    pEntry->tEvent.sProviderCode = NULL;
    pEntry->tEvent.sProviderMessage = NULL;
    pEntry->tEvent.sFinishReason = NULL;
    pEntry->tEvent.sRequestFingerprint = NULL;
    pEntry->tEvent.iOriginalTextLength =
        pEvent->iOriginalTextLength ? pEvent->iOriginalTextLength : pEvent->iTextLength;
    pEntry->tEvent.sText = xwork__observability_copy_text(
        pEvent->sText, pEvent->iTextLength, iTextLimit, &pEntry->tEvent.bTextTruncated, &iBytes);
    if (pEvent->sText && pEvent->iTextLength && !pEntry->tEvent.sText)
        goto oom;
    pEntry->tEvent.iTextLength =
        pEntry->tEvent.sText ? (pEvent->iTextLength > iTextLimit ? iTextLimit : pEvent->iTextLength)
                             : 0u;
#define XWORK_COPY_EVENT_STRING(Field)                                                             \
    do                                                                                             \
    {                                                                                              \
        pEntry->tEvent.Field = xwork__observability_copy_string(                                   \
            pEvent->Field, XWORK_EVENT_METADATA_MAX, &bMetadataTruncated, &iBytes);                \
        if (pEvent->Field && !pEntry->tEvent.Field)                                                \
            goto oom;                                                                              \
    } while (0)
    XWORK_COPY_EVENT_STRING(sToolName);
    XWORK_COPY_EVENT_STRING(sToolCallId);
    XWORK_COPY_EVENT_STRING(sArtifactPath);
    XWORK_COPY_EVENT_STRING(sModel);
    XWORK_COPY_EVENT_STRING(sProviderRequestId);
    XWORK_COPY_EVENT_STRING(sProviderCode);
    XWORK_COPY_EVENT_STRING(sProviderMessage);
    XWORK_COPY_EVENT_STRING(sFinishReason);
    XWORK_COPY_EVENT_STRING(sRequestFingerprint);
#undef XWORK_COPY_EVENT_STRING
    pEntry->tEvent.bMetadataTruncated = pEvent->bMetadataTruncated || bMetadataTruncated;
    pEntry->iRetainedBytes = iBytes;
    return pEntry;
oom:
    xwork__event_entry_unit(pEntry);
    return NULL;
}

static void xwork__runtime_drop_oldest_event_locked(xwork_runtime *pRuntime)
{
    xwork_event_entry *pEntry;
    if (!pRuntime || !pRuntime->iEventCount)
        return;
    pEntry = pRuntime->ppEvents[pRuntime->iEventStart];
    pRuntime->ppEvents[pRuntime->iEventStart] = NULL;
    pRuntime->iEventStart = (pRuntime->iEventStart + 1u) % pRuntime->uMaxEvents;
    --pRuntime->iEventCount;
    if (pEntry && pEntry->iRetainedBytes <= pRuntime->iRetainedEventBytes)
        pRuntime->iRetainedEventBytes -= pEntry->iRetainedBytes;
    else
        pRuntime->iRetainedEventBytes = 0u;
    if (pRuntime->uDroppedEvents != UINT64_MAX)
        ++pRuntime->uDroppedEvents;
    xwork__event_entry_release(pEntry);
}

void xwork__runtime_record_event(xwork_runtime *pRuntime, xwork_event *pEvent)
{
    xwork_event_entry *pEntry;
    size_t iSlot;
    if (!pEvent)
        return;
    pEvent->uSchemaVersion = XWORK_EVENT_SCHEMA_VERSION;
    if (!pEvent->iOccurredAtUs)
        pEvent->iOccurredAtUs = xllmUnixUs();
    if (!pRuntime)
        return;
    pEntry = xwork__event_entry_create(pEvent, pRuntime->iMaxEventTextBytes);
    if (!xrtMutexLock(pRuntime->pObservabilityLock))
    {
        xwork__event_entry_release(pEntry);
        return;
    }
    if (!pRuntime->uNextEventId)
    {
        if (pRuntime->uDroppedEvents != UINT64_MAX)
            ++pRuntime->uDroppedEvents;
        (void)xrtMutexUnlock(pRuntime->pObservabilityLock);
        xwork__event_entry_release(pEntry);
        return;
    }
    pEvent->uEventId = pRuntime->uNextEventId++;
    if (pEntry)
        pEntry->tEvent.uEventId = pEvent->uEventId;
    if (!pEntry || pEntry->iRetainedBytes > pRuntime->iMaxRetainedEventBytes)
    {
        if (pRuntime->uDroppedEvents != UINT64_MAX)
            ++pRuntime->uDroppedEvents;
        (void)xrtMutexUnlock(pRuntime->pObservabilityLock);
        xwork__event_entry_release(pEntry);
        return;
    }
    while (pRuntime->iEventCount >= pRuntime->uMaxEvents ||
           pEntry->iRetainedBytes >
               pRuntime->iMaxRetainedEventBytes - pRuntime->iRetainedEventBytes)
    {
        xwork__runtime_drop_oldest_event_locked(pRuntime);
    }
    iSlot = (pRuntime->iEventStart + pRuntime->iEventCount) % pRuntime->uMaxEvents;
    pRuntime->ppEvents[iSlot] = pEntry;
    ++pRuntime->iEventCount;
    pRuntime->iRetainedEventBytes += pEntry->iRetainedBytes;
    (void)xrtMutexUnlock(pRuntime->pObservabilityLock);
}

static char *xwork__artifact_safe_name(const char *sName)
{
    size_t i;
    size_t iLength = sName ? strlen(sName) : 0u;
    char *sSafe;
    if (iLength > 128u)
        iLength = 128u;
    sSafe = (char *)malloc(iLength + 1u);
    if (!sSafe)
        return NULL;
    for (i = 0u; i < iLength; ++i)
    {
        unsigned char ch = (unsigned char)sName[i];
        sSafe[i] = (isalnum(ch) || ch == '-' || ch == '_') ? (char)ch : '_';
    }
    sSafe[iLength] = '\0';
    return sSafe;
}

static void xwork__artifact_entry_unit(xwork_artifact_entry *pEntry)
{
    if (!pEntry)
        return;
    free(pEntry->sSource);
    free(pEntry->sPath);
    free(pEntry->sMediaType);
    free(pEntry->sAbsolutePath);
    if (pEntry->sStoreRelativePath)
        xrtFree(pEntry->sStoreRelativePath);
    xworkArtifactStoreRelease(pEntry->pStore);
    free(pEntry);
}

static void xwork__artifact_unreserve(xwork_runtime *pRuntime)
{
    if (!pRuntime || !xrtMutexLock(pRuntime->pObservabilityLock))
        return;
    if (pRuntime->iArtifactReservations)
        --pRuntime->iArtifactReservations;
    (void)xrtMutexUnlock(pRuntime->pObservabilityLock);
}

/* Anchor every parent below the workspace before creating a temporary file.
 * Existing links are rejected; races are still confined by xrt's root resolver.
 * The final directory handle owns all create/rename/remove operations, including
 * cleanup, so a changed diagnostic path cannot redirect a failed write. */
xroot xwork__artifact_open_directory(const xwork_agent *pAgent, const char *sRunAbsolute,
                                     const char *sRunName, xwork_error *pError)
{
    xroot pWorkspace = NULL;
    xroot pDirectory = NULL;
    char *sRelative = NULL;
    char *p;
    pWorkspace = pAgent->pArtifactStore ? xrtRootOpenIn(pAgent->pArtifactStore->pDirectory, ".")
                                        : xrtRootOpen(pAgent->sWorkspaceRoot);
    if (!pWorkspace)
        goto io;
    sRelative = pAgent->pArtifactStore ? xrtStrDup(sRunName)
                                       : xrtPathRel(pAgent->sWorkspaceRoot, sRunAbsolute);
    if (!sRelative)
        goto io;
    for (p = sRelative;; ++p)
    {
        char ch = *p;
        xfileinfo tInfo;
        if (ch && ch != '/' && ch != '\\')
            continue;
        *p = '\0';
        if (sRelative[0])
        {
            if (!xrtRootStat(pWorkspace, sRelative, false, &tInfo))
            {
                if (xrtErrorKind(xrtGetError()) != XERR_NOT_FOUND)
                    goto io;
                if (!xrtRootDirCreate(pWorkspace, sRelative, 0700u))
                {
                    /* A second writer may have created the same directory. */
                    if (xrtErrorKind(xrtGetError()) != XERR_EXISTS)
                        goto io;
                }
                if (!xrtRootStat(pWorkspace, sRelative, false, &tInfo))
                    goto io;
            }
            if (tInfo.Type != XFILE_TYPE_DIRECTORY)
                goto io;
        }
        *p = ch;
        if (!ch)
            break;
    }
    pDirectory = xrtRootOpenIn(pWorkspace, sRelative);
    if (!pDirectory)
        goto io;
    xrtFree(sRelative);
    (void)xrtRootClose(pWorkspace);
    return pDirectory;
io:
    xwork__set_error(pError, XWORK_ERROR_IO, "failed to anchor tool output artifact directory");
    if (sRelative)
        xrtFree(sRelative);
    if (pWorkspace)
        (void)xrtRootClose(pWorkspace);
    return NULL;
}

/* Only an exclusively created name is ours to remove. No target file is opened
 * for writing: a complete, flushed temporary file is published without replace. */
xfile xwork__artifact_temp(xroot pDirectory, char sName[64])
{
    uint8_t arrRandom[16];
    xfileoptions tOptions;
    unsigned int iAttempt;
    size_t i;
    xfile pFile;
    xrtFileOptionsInit(&tOptions);
    tOptions.Flags = XFILE_WRITE | XFILE_CREATE | XFILE_EXCLUSIVE | XFILE_NOFOLLOW;
    tOptions.Mode = 0600u;
    tOptions.Share = XFILE_SHARE_READ;
    for (iAttempt = 0u; iAttempt < XWORK_ARTIFACT_TEMP_ATTEMPTS; ++iAttempt)
    {
        if (!xrtSecureRandom(arrRandom, sizeof(arrRandom)))
            return NULL;
        memcpy(sName, ".xwork-", 7u);
        for (i = 0u; i < sizeof(arrRandom); ++i)
        {
            static const char sHex[] = "0123456789abcdef";
            sName[7u + i * 2u] = sHex[arrRandom[i] >> 4];
            sName[8u + i * 2u] = sHex[arrRandom[i] & 15u];
        }
        memcpy(sName + 39u, ".tmp", 5u);
        pFile = xrtRootFileOpen(pDirectory, sName, &tOptions);
        if (pFile)
            return pFile;
        if (xrtErrorKind(xrtGetError()) != XERR_EXISTS)
            return NULL;
    }
    return NULL;
}

bool xwork__runtime_write_artifact(xwork_agent *pAgent, const char *sSource, const void *pData,
                                   size_t iSize, const char *sMediaType, uint64_t *puArtifactId,
                                   char **ppRelativePath, xwork_error *pError)
{
    xwork_runtime *pRuntime;
    xwork_artifact_entry *pEntry = NULL;
    xwork_event tEvent;
    char *sSafe = NULL;
    char sRunName[96];
    char sFileName[256];
    char *sRunRelative = NULL;
    char *sRunAbsolute = NULL;
    char *sArtifactRelative = NULL;
    char *sArtifactAbsolute = NULL;
    char *sCallerPath = NULL;
    uint8_t arrDigest[32];
    uint64_t uArtifactId = 0u;
    uint64_t uRunId;
    xroot pDirectory = NULL;
    xfile pTemporary = NULL;
    char sTemporary[64];
    unsigned int iAttempt;
    size_t i;
    bool bReserved = false;
    bool bTemporaryOwned = false;
    if (puArtifactId)
        *puArtifactId = 0u;
    if (ppRelativePath)
        *ppRelativePath = NULL;
    if (!pAgent || !pAgent->pRuntime || !sSource || !sSource[0] || (iSize && !pData) ||
        !ppRelativePath || !puArtifactId)
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "invalid artifact write request");
        return false;
    }
    pRuntime = pAgent->pRuntime;
    if (!xrtMutexLock(pRuntime->pObservabilityLock))
    {
        xwork__set_error(pError, XWORK_ERROR_CONTEXT, "runtime artifact registry is unavailable");
        return false;
    }
    if (pRuntime->iArtifactCount + pRuntime->iArtifactReservations >= pRuntime->uMaxArtifacts ||
        !pRuntime->uNextArtifactId)
    {
        (void)xrtMutexUnlock(pRuntime->pObservabilityLock);
        xwork__set_error(pError, XWORK_ERROR_LIMIT, "runtime artifact registry limit was exceeded");
        return false;
    }
    uArtifactId = pRuntime->uNextArtifactId++;
    ++pRuntime->iArtifactReservations;
    bReserved = true;
    (void)xrtMutexUnlock(pRuntime->pObservabilityLock);

    sSafe = xwork__artifact_safe_name(sSource);
    if (!sSafe)
        goto oom;
    uRunId = pAgent->pActiveRun ? pAgent->pActiveRun->uRunId : pAgent->uRunSequence;
    snprintf(sRunName, sizeof(sRunName), "run-%020llu", (unsigned long long)uRunId);
    sRunRelative = xrtPathJoin(pAgent->pArtifactStore ? pAgent->pArtifactStore->sDirectory
                                                      : pAgent->sArtifactDirectory,
                               sRunName);
    if (!sRunRelative)
        goto oom;
    sRunAbsolute = pAgent->pArtifactStore ? xwork__strdup(sRunRelative)
                                          : xwork__resolve_path(pAgent, sRunRelative, pError);
    if (!sRunAbsolute)
    {
        if (pAgent->pArtifactStore)
            goto oom;
        goto fail;
    }
    pDirectory = xwork__artifact_open_directory(pAgent, sRunAbsolute, sRunName, pError);
    if (!pDirectory)
        goto fail;
    if (!xrtSha256(pData, iSize, arrDigest))
    {
        xwork__set_error(pError, XWORK_ERROR_CONTEXT, "failed to hash tool output artifact");
        goto fail;
    }
    pEntry = (xwork_artifact_entry *)calloc(1u, sizeof(*pEntry));
    if (!pEntry)
        goto oom;
    pEntry->sSource = xwork__strdup(sSource);
    pEntry->pStore = xworkArtifactStoreRef(pAgent->pArtifactStore);
    pEntry->sMediaType =
        xwork__strdup(sMediaType && sMediaType[0] ? sMediaType : "text/plain; charset=utf-8");
    if (!pEntry->sSource || !pEntry->sMediaType)
        goto oom;
    xworkArtifactInfoInit(&pEntry->tInfo);
    pEntry->tInfo.eKind = XWORK_ARTIFACT_TOOL_OUTPUT;
    pEntry->tInfo.iCreatedAtUs = xllmUnixUs();
    pEntry->tInfo.uAgentId = pAgent->uAgentId;
    pEntry->tInfo.uRunId = pAgent->pActiveRun ? pAgent->pActiveRun->uRunId : pAgent->uParentRunId;
    pEntry->tInfo.uTaskId = pAgent->uParentTaskId;
    pEntry->tInfo.sSource = pEntry->sSource;
    pEntry->tInfo.sMediaType = pEntry->sMediaType;
    pEntry->tInfo.uSizeBytes = (uint64_t)iSize;
    for (i = 0u; i < sizeof(arrDigest); ++i)
    {
        static const char sHex[] = "0123456789abcdef";
        pEntry->tInfo.sSha256[i * 2u] = sHex[arrDigest[i] >> 4];
        pEntry->tInfo.sSha256[i * 2u + 1u] = sHex[arrDigest[i] & 0x0fu];
    }
    pEntry->tInfo.sSha256[64] = '\0';
    pTemporary = xwork__artifact_temp(pDirectory, sTemporary);
    if (!pTemporary)
        goto io;
    bTemporaryOwned = true;
    if (!xrtWriteFull(pTemporary, pData, iSize, NULL) || !xrtFlush(pTemporary))
        goto io;
    if (!xrtClose(pTemporary))
    {
        pTemporary = NULL;
        goto io;
    }
    pTemporary = NULL;
    for (iAttempt = 0u; iAttempt < XWORK_ARTIFACT_PUBLISH_ATTEMPTS; ++iAttempt)
    {
        xerrkind eFailure;
        snprintf(sFileName, sizeof(sFileName), "%020llu-%s.%s", (unsigned long long)uArtifactId,
                 sSafe,
                 sMediaType && strncmp(sMediaType, "text/markdown", 13u) == 0 ? "md" : "txt");
        sArtifactRelative = xrtPathJoin(sRunRelative, sFileName);
        if (!sArtifactRelative)
            goto oom;
        sArtifactAbsolute = pAgent->pArtifactStore
                                ? xwork__strdup(sArtifactRelative)
                                : xwork__resolve_path(pAgent, sArtifactRelative, pError);
        if (!sArtifactAbsolute)
        {
            if (pAgent->pArtifactStore)
                goto oom;
            goto fail;
        }
        pEntry->sPath = xwork__strdup(sArtifactRelative);
        pEntry->sAbsolutePath = xwork__strdup(sArtifactAbsolute);
        sCallerPath = xwork__strdup(sArtifactRelative);
        if (pEntry->pStore)
        {
            pEntry->sStoreRelativePath = xrtPathJoin(sRunName, sFileName);
            if (!pEntry->sStoreRelativePath)
                goto oom;
        }
        if (!pEntry->sPath || !pEntry->sAbsolutePath || !sCallerPath)
            goto oom;
        pEntry->tInfo.uArtifactId = uArtifactId;
        pEntry->tInfo.sPath = pEntry->sPath;
        if (!xrtMutexLock(pRuntime->pObservabilityLock))
        {
            xwork__set_error(pError, XWORK_ERROR_CONTEXT,
                             "runtime artifact registry is unavailable during publication");
            goto fail;
        }
        /* Prepare all memory before this atomic hard-link publication. Hold the registry lock
         * across the one filesystem commit, so no fallible registry acquisition or allocation
         * remains after publication. Never delete a published path. */
        if (xrtRootLinkHard(pDirectory, sTemporary, sFileName))
        {
            --pRuntime->iArtifactReservations;
            bReserved = false;
            (void)xrtRootRemove(pDirectory, sTemporary);
            xrtClearError();
            bTemporaryOwned = false;
            pRuntime->ppArtifacts[pRuntime->iArtifactCount++] = pEntry;
            (void)xrtMutexUnlock(pRuntime->pObservabilityLock);
            break;
        }
        eFailure = xrtErrorKind(xrtGetError());
        if (eFailure != XERR_EXISTS)
        {
            (void)xrtMutexUnlock(pRuntime->pObservabilityLock);
            goto io;
        }
        if (iAttempt + 1u == XWORK_ARTIFACT_PUBLISH_ATTEMPTS || !pRuntime->uNextArtifactId)
        {
            (void)xrtMutexUnlock(pRuntime->pObservabilityLock);
            xwork__set_error(pError, XWORK_ERROR_LIMIT,
                             "artifact publication collision or identifier limit was exceeded");
            goto fail;
        }
        uArtifactId = pRuntime->uNextArtifactId++;
        (void)xrtMutexUnlock(pRuntime->pObservabilityLock);
        free(pEntry->sPath);
        pEntry->sPath = NULL;
        free(pEntry->sAbsolutePath);
        pEntry->sAbsolutePath = NULL;
        xrtFree(pEntry->sStoreRelativePath);
        pEntry->sStoreRelativePath = NULL;
        free(sCallerPath);
        sCallerPath = NULL;
        xrtFree(sArtifactRelative);
        sArtifactRelative = NULL;
        free(sArtifactAbsolute);
        sArtifactAbsolute = NULL;
    }
    *puArtifactId = uArtifactId;
    *ppRelativePath = sCallerPath;
    sCallerPath = NULL;
    memset(&tEvent, 0, sizeof(tEvent));
    tEvent.eKind = XWORK_EVENT_ARTIFACT_CREATED;
    tEvent.uAgentId = pEntry->tInfo.uAgentId;
    tEvent.uRunId = pEntry->tInfo.uRunId;
    tEvent.uTaskId = pEntry->tInfo.uTaskId;
    tEvent.uArtifactId = uArtifactId;
    tEvent.sToolName = pEntry->sSource;
    tEvent.sArtifactPath = pEntry->sPath;
    tEvent.bSuccess = true;
    xwork__runtime_record_event(pRuntime, &tEvent);
    pEntry = NULL;
    free(sSafe);
    (void)xrtRootClose(pDirectory);
    free(sRunAbsolute);
    xrtFree(sRunRelative);
    xrtFree(sArtifactRelative);
    free(sArtifactAbsolute);
    return true;
io:
    xwork__set_error(pError, XWORK_ERROR_IO,
                     "failed to publish tool output artifact without replacement");
    goto fail;
oom:
    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY,
                     "failed to retain tool output artifact metadata");
fail:
    if (bReserved)
        xwork__artifact_unreserve(pRuntime);
    if (pTemporary)
        (void)xrtClose(pTemporary);
    if (bTemporaryOwned)
        (void)xrtRootRemove(pDirectory, sTemporary);
    if (pDirectory)
        (void)xrtRootClose(pDirectory);
    xwork__artifact_entry_unit(pEntry);
    free(sCallerPath);
    free(sSafe);
    if (sRunRelative)
        xrtFree(sRunRelative);
    free(sRunAbsolute);
    if (sArtifactRelative)
        xrtFree(sArtifactRelative);
    free(sArtifactAbsolute);
    return false;
}

bool xworkToolResultWriterSaveArtifact(xwork_tool_result_writer *Writer,
                                       const xwork_tool_context *Context, const char *Text,
                                       size_t Size, const char *MediaType,
                                       xwork_artifact_info *Info, xwork_error *Error)
{
    uint64_t Id = 0u;
    char *Path = NULL;
    xworkErrorInit(Error);
    if (!Writer || Writer->uMagic != XWORK_RESULT_WRITER_MAGIC || Writer->bFailed || !Context ||
        Writer->pAgent != Context->pAgent || !Writer->pAgent || Writer->sArtifactPath || !Info ||
        Info->uSize < sizeof(*Info) || Info->uAbiVersion != XWORK_ABI_VERSION || !Text ||
        memchr(Text, 0, Size) || !xrtUtf8Valid(xrtStrViewN(Text, Size), NULL))
    {
        xwork__set_error(Error, XWORK_ERROR_INVALID_ARGUMENT,
                         "invalid callback artifact publication");
        return false;
    }
    if (!Writer->pAgent->bAllowArtifactWrites)
    {
        xwork__set_error(Error, XWORK_ERROR_POLICY,
                         "result file storage is disabled for this session");
        return false;
    }
    if (Size > Writer->pAgent->iMaxCapturedCommandBytes)
    {
        xwork__set_error(Error, XWORK_ERROR_LIMIT, "result file exceeds its capture budget");
        return false;
    }
    if (!xwork__runtime_write_artifact(Writer->pAgent,
                                       Context->sToolName ? Context->sToolName : "tool", Text, Size,
                                       MediaType, &Id, &Path, Error))
        return false;
    /* Publication has already committed. Retain its locator even if a later
     * caller fails to construct an inline index. Never delete the result. */
    Writer->sArtifactPath = Path;
    Writer->uArtifactId = Id;
    return xworkRuntimeArtifactGetInfo(Writer->pAgent->pRuntime, Id, Info);
}

void xworkArtifactInfoInit(xwork_artifact_info *pInfo)
{
    if (!pInfo)
        return;
    memset(pInfo, 0, sizeof(*pInfo));
    pInfo->uSize = (uint32_t)sizeof(*pInfo);
    pInfo->uAbiVersion = XWORK_ABI_VERSION;
}

void xworkArtifactChunkInit(xwork_artifact_chunk *pChunk)
{
    if (!pChunk)
        return;
    memset(pChunk, 0, sizeof(*pChunk));
    pChunk->uSize = (uint32_t)sizeof(*pChunk);
    pChunk->uAbiVersion = XWORK_ABI_VERSION;
}

void xworkArtifactChunkUnit(xwork_artifact_chunk *pChunk)
{
    if (!pChunk)
        return;
    free(pChunk->pData);
    xworkArtifactChunkInit(pChunk);
}

static void xwork__artifact_info_fill(const xwork_artifact_entry *pEntry,
                                      xwork_artifact_info *pInfo)
{
    uint32_t uSize = pInfo->uSize;
    uint32_t uVersion = pInfo->uAbiVersion;
    *pInfo = pEntry->tInfo;
    pInfo->uSize = uSize;
    pInfo->uAbiVersion = uVersion;
}

size_t xworkRuntimeArtifactCount(xwork_runtime *pRuntime)
{
    size_t iCount;
    if (!pRuntime || !xrtMutexLock(pRuntime->pObservabilityLock))
        return 0u;
    iCount = pRuntime->iArtifactCount;
    (void)xrtMutexUnlock(pRuntime->pObservabilityLock);
    return iCount;
}

bool xworkRuntimeArtifactGetInfo(xwork_runtime *pRuntime, uint64_t uArtifactId,
                                 xwork_artifact_info *pInfo)
{
    size_t i;
    xwork_artifact_entry *pFound = NULL;
    if (!pRuntime || !uArtifactId || !pInfo ||
        !xwork__record_valid(pInfo->uSize, pInfo->uAbiVersion, sizeof(*pInfo)) ||
        !xrtMutexLock(pRuntime->pObservabilityLock))
        return false;
    for (i = 0u; i < pRuntime->iArtifactCount; ++i)
    {
        if (pRuntime->ppArtifacts[i]->tInfo.uArtifactId == uArtifactId)
        {
            pFound = pRuntime->ppArtifacts[i];
            break;
        }
    }
    if (pFound)
        xwork__artifact_info_fill(pFound, pInfo);
    (void)xrtMutexUnlock(pRuntime->pObservabilityLock);
    return pFound != NULL;
}

bool xworkRuntimeArtifactAt(xwork_runtime *pRuntime, size_t iIndex, xwork_artifact_info *pInfo)
{
    bool bFound = false;
    if (!pRuntime || !pInfo ||
        !xwork__record_valid(pInfo->uSize, pInfo->uAbiVersion, sizeof(*pInfo)) ||
        !xrtMutexLock(pRuntime->pObservabilityLock))
        return false;
    if (iIndex < pRuntime->iArtifactCount)
    {
        xwork__artifact_info_fill(pRuntime->ppArtifacts[iIndex], pInfo);
        bFound = true;
    }
    (void)xrtMutexUnlock(pRuntime->pObservabilityLock);
    return bFound;
}

bool xworkRuntimeReadArtifact(xwork_runtime *pRuntime, uint64_t uArtifactId, uint64_t uOffset,
                              size_t iMaxBytes, xwork_artifact_chunk *pChunk, xwork_error *pError)
{
    uint32_t uSize = pChunk ? pChunk->uSize : 0u;
    uint32_t uVersion = pChunk ? pChunk->uAbiVersion : 0u;
    char *sAbsolute = NULL;
    xwork_artifact_store *pStore = NULL;
    uint64_t uExpected = 0u;
    xfileoptions tOptions;
    xfile hFile = NULL;
    xfileinfo tStat;
    size_t iRequest;
    size_t iRead = 0u;
    size_t i;
    bool bFound = false;
    xworkErrorInit(pError);
    if (!pRuntime || !uArtifactId || !pChunk ||
        !xwork__record_valid(uSize, uVersion, sizeof(*pChunk)) ||
        iMaxBytes > XWORK_ARTIFACT_READ_MAX_BYTES)
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "invalid artifact read request");
        return false;
    }
    xworkArtifactChunkUnit(pChunk);
    pChunk->uSize = uSize;
    pChunk->uAbiVersion = uVersion;
    if (!iMaxBytes)
        iMaxBytes = XWORK_ARTIFACT_READ_DEFAULT_BYTES;
    if (!xrtMutexLock(pRuntime->pObservabilityLock))
    {
        xwork__set_error(pError, XWORK_ERROR_CONTEXT, "runtime artifact registry is unavailable");
        return false;
    }
    for (i = 0u; i < pRuntime->iArtifactCount; ++i)
    {
        xwork_artifact_entry *pEntry = pRuntime->ppArtifacts[i];
        if (pEntry->tInfo.uArtifactId == uArtifactId)
        {
            bFound = true;
            pStore = xworkArtifactStoreRef(pEntry->pStore);
            sAbsolute = xwork__strdup(pStore ? pEntry->sStoreRelativePath : pEntry->sAbsolutePath);
            uExpected = pEntry->tInfo.uSizeBytes;
            break;
        }
    }
    (void)xrtMutexUnlock(pRuntime->pObservabilityLock);
    if (!sAbsolute)
    {
        xworkArtifactStoreRelease(pStore);
        xwork__set_error(pError, bFound ? XWORK_ERROR_OUT_OF_MEMORY : XWORK_ERROR_INVALID_ARGUMENT,
                         bFound ? "failed to copy artifact path" : "artifact id was not found");
        return false;
    }
    if (uOffset > uExpected)
    {
        free(sAbsolute);
        xworkArtifactStoreRelease(pStore);
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
                         "artifact read offset exceeds its size");
        return false;
    }
    xrtFileOptionsInit(&tOptions);
    tOptions.Flags = XFILE_READ | XFILE_NOFOLLOW;
    hFile = pStore ? xrtRootFileOpen(pStore->pDirectory, sAbsolute, &tOptions)
                   : xrtFileOpen(sAbsolute, &tOptions);
    xworkArtifactStoreRelease(pStore);
    if (!hFile || !xrtFileStat(hFile, &tStat) || tStat.Type != XFILE_TYPE_FILE ||
        tStat.Size != uExpected)
    {
        if (hFile)
            (void)xrtClose(hFile);
        free(sAbsolute);
        xwork__set_error(pError, XWORK_ERROR_IO, "artifact file is missing or changed");
        return false;
    }
    iRequest =
        (size_t)((uExpected - uOffset) > (uint64_t)iMaxBytes ? iMaxBytes
                                                             : (size_t)(uExpected - uOffset));
    if (iRequest)
    {
        pChunk->pData = (unsigned char *)malloc(iRequest);
        if (!pChunk->pData)
        {
            (void)xrtClose(hFile);
            free(sAbsolute);
            xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY,
                             "failed to allocate artifact read buffer");
            return false;
        }
        if (!xrtReadAt(hFile, uOffset, pChunk->pData, iRequest, &iRead) || iRead != iRequest)
        {
            xworkArtifactChunkUnit(pChunk);
            (void)xrtClose(hFile);
            free(sAbsolute);
            xwork__set_error(pError, XWORK_ERROR_IO, "artifact read was incomplete");
            return false;
        }
    }
    pChunk->iSize = iRead;
    pChunk->uOffset = uOffset;
    pChunk->uTotalSize = uExpected;
    pChunk->bEof = uOffset + (uint64_t)iRead == uExpected;
    (void)xrtClose(hFile);
    free(sAbsolute);
    return true;
}

xwork_event_snapshot *xworkRuntimeEventSnapshot(xwork_runtime *pRuntime, uint64_t uAfterEventId,
                                                size_t iLimit, xwork_error *pError)
{
    xwork_event_snapshot *pSnapshot;
    uint64_t uExpected;
    size_t i;
    xworkErrorInit(pError);
    if (!pRuntime || iLimit > XWORK_EVENT_SNAPSHOT_MAX_RECORDS)
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
                         "invalid runtime event snapshot request");
        return NULL;
    }
    if (!iLimit)
        iLimit = 256u;
    pSnapshot = (xwork_event_snapshot *)calloc(1u, sizeof(*pSnapshot));
    if (!pSnapshot)
        goto oom;
    pSnapshot->iRefs = 1;
    pSnapshot->ppEvents = (xwork_event_entry **)calloc(iLimit, sizeof(*pSnapshot->ppEvents));
    if (!pSnapshot->ppEvents)
        goto oom;
    pSnapshot->uNextCursor = uAfterEventId;
    if (!xrtMutexLock(pRuntime->pObservabilityLock))
    {
        xworkEventSnapshotRelease(pSnapshot);
        xwork__set_error(pError, XWORK_ERROR_CONTEXT, "runtime event ledger is unavailable");
        return NULL;
    }
    pSnapshot->uLatestId = pRuntime->uNextEventId ? pRuntime->uNextEventId - 1u : UINT64_MAX;
    pSnapshot->uDroppedCount = pRuntime->uDroppedEvents;
    if (uAfterEventId > pSnapshot->uLatestId)
    {
        (void)xrtMutexUnlock(pRuntime->pObservabilityLock);
        xworkEventSnapshotRelease(pSnapshot);
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
                         "event cursor is ahead of this runtime");
        return NULL;
    }
    uExpected = uAfterEventId + 1u;
    for (i = 0u; i < pRuntime->iEventCount && pSnapshot->iEventCount < iLimit; ++i)
    {
        size_t iSlot = (pRuntime->iEventStart + i) % pRuntime->uMaxEvents;
        xwork_event_entry *pEntry = pRuntime->ppEvents[iSlot];
        if (!pEntry || pEntry->tEvent.uEventId <= uAfterEventId)
            continue;
        if (pEntry->tEvent.uEventId != uExpected)
            pSnapshot->bHistoryLost = true;
        pSnapshot->ppEvents[pSnapshot->iEventCount++] = xwork__event_entry_ref(pEntry);
        pSnapshot->uNextCursor = pEntry->tEvent.uEventId;
        uExpected = pEntry->tEvent.uEventId + 1u;
    }
    if (!pSnapshot->iEventCount && uAfterEventId < pSnapshot->uLatestId)
        pSnapshot->bHistoryLost = true;
    (void)xrtMutexUnlock(pRuntime->pObservabilityLock);
    return pSnapshot;
oom:
    xworkEventSnapshotRelease(pSnapshot);
    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY,
                     "failed to allocate runtime event snapshot");
    return NULL;
}

xwork_event_snapshot *xworkEventSnapshotRef(xwork_event_snapshot *pSnapshot)
{
    if (!pSnapshot || xwork__atomic_inc(&pSnapshot->iRefs) <= 1)
        return NULL;
    return pSnapshot;
}

void xworkEventSnapshotRelease(xwork_event_snapshot *pSnapshot)
{
    size_t i;
    if (!pSnapshot || xwork__atomic_dec(&pSnapshot->iRefs) != 0)
        return;
    for (i = 0u; i < pSnapshot->iEventCount; ++i)
        xwork__event_entry_release(pSnapshot->ppEvents[i]);
    free(pSnapshot->ppEvents);
    free(pSnapshot);
}

size_t xworkEventSnapshotCount(const xwork_event_snapshot *pSnapshot)
{
    return pSnapshot ? pSnapshot->iEventCount : 0u;
}

bool xworkEventSnapshotAt(const xwork_event_snapshot *pSnapshot, size_t iIndex, xwork_event *pEvent)
{
    if (!pSnapshot || !pEvent || iIndex >= pSnapshot->iEventCount || !pSnapshot->ppEvents[iIndex])
        return false;
    *pEvent = pSnapshot->ppEvents[iIndex]->tEvent;
    return true;
}

uint64_t xworkEventSnapshotNextCursor(const xwork_event_snapshot *pSnapshot)
{
    return pSnapshot ? pSnapshot->uNextCursor : 0u;
}

uint64_t xworkEventSnapshotLatestId(const xwork_event_snapshot *pSnapshot)
{
    return pSnapshot ? pSnapshot->uLatestId : 0u;
}

uint64_t xworkEventSnapshotDroppedCount(const xwork_event_snapshot *pSnapshot)
{
    return pSnapshot ? pSnapshot->uDroppedCount : 0u;
}

bool xworkEventSnapshotHistoryLost(const xwork_event_snapshot *pSnapshot)
{
    return pSnapshot && pSnapshot->bHistoryLost;
}

void xwork__runtime_observability_unit(xwork_runtime *pRuntime)
{
    size_t i;
    if (!pRuntime)
        return;
    for (i = 0u; i < pRuntime->iArtifactCount; ++i)
        xwork__artifact_entry_unit(pRuntime->ppArtifacts[i]);
    for (i = 0u; i < pRuntime->iEventCount; ++i)
    {
        size_t iSlot = (pRuntime->iEventStart + i) % pRuntime->uMaxEvents;
        xwork__event_entry_release(pRuntime->ppEvents[iSlot]);
    }
    free(pRuntime->ppArtifacts);
    free(pRuntime->ppEvents);
    pRuntime->ppArtifacts = NULL;
    pRuntime->ppEvents = NULL;
    pRuntime->iArtifactCount = 0u;
    pRuntime->iEventCount = 0u;
}
