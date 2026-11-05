#include "../internal/xwork_internal.h"

/* Stable runtime task metadata, output and lifecycle API. */

static bool xwork__task_terminal(xwork_task_state eState)
{
    return eState >= XWORK_TASK_SUCCEEDED && eState <= XWORK_TASK_LOST;
}

static void xwork__task_info_copy_locked(const xwork_process_entry *pEntry, xwork_task_info *pInfo)
{
    xworkTaskInfoInit(pInfo);
    pInfo->uTaskId = pEntry->uId;
    pInfo->uOwnerAgentId = pEntry->uOwnerAgentId;
    pInfo->uOwnerRunId = pEntry->uOwnerRunId;
    pInfo->uParentTaskId = pEntry->uParentTaskId;
    pInfo->eKind = pEntry->eKind;
    pInfo->eState = pEntry->eState;
    pInfo->uRevision = pEntry->uRevision;
    pInfo->iCreatedAtUs = pEntry->iCreatedAtUs;
    pInfo->iStartedAtUs = pEntry->iStartedAtUs;
    pInfo->iEndedAtUs = pEntry->iEndedAtUs;
    pInfo->iScheduledAtUs = pEntry->iScheduledAtUs;
    pInfo->bExitStatusValid = pEntry->bExitStatusValid;
    pInfo->iExitCode = pEntry->iExitCode;
    pInfo->iExitSignal = pEntry->iExitSignal;
    pInfo->iStopReason = pEntry->iStopReason;
    pInfo->bNoticeTaken = pEntry->bNoticeTaken;
    pInfo->sOwnerSession = pEntry->sOwnerSession;
    pInfo->sLabel = pEntry->sLabel ? pEntry->sLabel : pEntry->sCommand;
    pInfo->sNotify = pEntry->sNotify;
    pInfo->sScheduleId = pEntry->sScheduleId;
    pInfo->uScheduleGeneration = pEntry->uScheduleGeneration;
}

void xworkTaskInfoInit(xwork_task_info *pInfo)
{
    if (!pInfo)
        return;
    memset(pInfo, 0, sizeof(*pInfo));
    pInfo->uSize = (uint32_t)sizeof(*pInfo);
    pInfo->uAbiVersion = XWORK_ABI_VERSION;
}

void xworkTaskOutputInit(xwork_task_output *pOutput)
{
    if (pOutput)
        memset(pOutput, 0, sizeof(*pOutput));
}

void xworkTaskOutputUnit(xwork_task_output *pOutput)
{
    if (!pOutput)
        return;
    free(pOutput->pStdout);
    free(pOutput->pStderr);
    free(pOutput->sResult);
    memset(pOutput, 0, sizeof(*pOutput));
}

void xworkScheduledTaskConfigInit(xwork_scheduled_task_config *pConfig)
{
    if (!pConfig)
        return;
    memset(pConfig, 0, sizeof(*pConfig));
    pConfig->uSize = (uint32_t)sizeof(*pConfig);
    pConfig->uAbiVersion = XWORK_ABI_VERSION;
}

xwork_process_entry *xwork__runtime_task_find(xwork_runtime *pRuntime, uint64_t uId)
{
    size_t i;
    if (!pRuntime || !uId || !xrtMutexLock(pRuntime->pLock))
        return NULL;
    for (i = 0u; i < pRuntime->iTaskCount; ++i)
    {
        xwork_process_entry *pEntry = pRuntime->ppTasks[i];
        if (pEntry->uId == uId && xwork__atomic_load(&pEntry->iReady) != 0)
        {
            xwork__task_entry_ref(pEntry);
            (void)xrtMutexUnlock(pRuntime->pLock);
            return pEntry;
        }
    }
    (void)xrtMutexUnlock(pRuntime->pLock);
    return NULL;
}

static int xwork__task_info_compare(const void *pA, const void *pB)
{
    const xwork_task_snapshot_entry *pLeft = (const xwork_task_snapshot_entry *)pA;
    const xwork_task_snapshot_entry *pRight = (const xwork_task_snapshot_entry *)pB;
    return pLeft->tInfo.uTaskId < pRight->tInfo.uTaskId
               ? -1
               : (pLeft->tInfo.uTaskId > pRight->tInfo.uTaskId ? 1 : 0);
}

static bool xwork__snapshot_copy_string(const char *sSource, char **psTarget)
{
    if (!sSource)
    {
        *psTarget = NULL;
        return true;
    }
    *psTarget = xwork__strdup(sSource);
    return *psTarget != NULL;
}

xwork_task_snapshot *xworkRuntimeTaskSnapshot(xwork_runtime *pRuntime, uint64_t uOwnerAgentId,
                                              xwork_error *pError)
{
    xwork_task_snapshot *pSnapshot = NULL;
    size_t i;
    size_t n = 0u;
    xworkErrorInit(pError);
    if (!pRuntime || !xrtMutexLock(pRuntime->pLock))
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "runtime is null or unavailable");
        return NULL;
    }
    for (i = 0u; i < pRuntime->iTaskCount; ++i)
    {
        xwork_process_entry *pEntry = pRuntime->ppTasks[i];
        if (xwork__atomic_load(&pEntry->iReady) != 0 &&
            (!uOwnerAgentId || pEntry->uOwnerAgentId == uOwnerAgentId))
            ++n;
    }
    pSnapshot = (xwork_task_snapshot *)calloc(1u, sizeof(*pSnapshot));
    if (pSnapshot)
        pSnapshot->iRefs = 1;
    if (pSnapshot && n)
    {
        pSnapshot->pTasks = (xwork_task_snapshot_entry *)calloc(n, sizeof(*pSnapshot->pTasks));
    }
    if (!pSnapshot || (n && !pSnapshot->pTasks))
        goto oom_locked;
    for (i = 0u; i < pRuntime->iTaskCount; ++i)
    {
        xwork_process_entry *pEntry = pRuntime->ppTasks[i];
        xwork_task_info *pInfo;
        const char *sOwner;
        const char *sLabel;
        const char *sNotify;
        const char *sScheduleId;
        char *sOwnerCopy = NULL;
        char *sLabelCopy = NULL;
        char *sNotifyCopy = NULL;
        char *sScheduleIdCopy = NULL;
        if (xwork__atomic_load(&pEntry->iReady) == 0 ||
            (uOwnerAgentId && pEntry->uOwnerAgentId != uOwnerAgentId))
            continue;
        if (!xrtMutexLock(pEntry->pStateLock))
            goto oom_locked;
        xwork__task_refresh_locked(pEntry);
        pInfo = &pSnapshot->pTasks[pSnapshot->iTaskCount].tInfo;
        xwork__task_info_copy_locked(pEntry, pInfo);
        pSnapshot->pTasks[pSnapshot->iTaskCount].bStopRequested = pEntry->bStopRequested;
        sOwner = pInfo->sOwnerSession;
        sLabel = pInfo->sLabel;
        sNotify = pInfo->sNotify;
        sScheduleId = pInfo->sScheduleId;
        if (!xwork__snapshot_copy_string(sOwner, &sOwnerCopy) ||
            !xwork__snapshot_copy_string(sLabel, &sLabelCopy) ||
            !xwork__snapshot_copy_string(sNotify, &sNotifyCopy) ||
            !xwork__snapshot_copy_string(sScheduleId, &sScheduleIdCopy))
        {
            free(sOwnerCopy);
            free(sLabelCopy);
            free(sNotifyCopy);
            free(sScheduleIdCopy);
            (void)xrtMutexUnlock(pEntry->pStateLock);
            goto oom_locked;
        }
        pInfo->sOwnerSession = sOwnerCopy;
        pInfo->sLabel = sLabelCopy;
        pInfo->sNotify = sNotifyCopy;
        pInfo->sScheduleId = sScheduleIdCopy;
        ++pSnapshot->iTaskCount;
        (void)xrtMutexUnlock(pEntry->pStateLock);
    }
    (void)xrtMutexUnlock(pRuntime->pLock);
    if (pSnapshot->iTaskCount > 1u)
    {
        qsort(pSnapshot->pTasks, pSnapshot->iTaskCount, sizeof(*pSnapshot->pTasks),
              xwork__task_info_compare);
    }
    return pSnapshot;
oom_locked:
    (void)xrtMutexUnlock(pRuntime->pLock);
    xworkTaskSnapshotRelease(pSnapshot);
    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to create task snapshot");
    return NULL;
}

xwork_task_snapshot *xworkTaskSnapshotRef(xwork_task_snapshot *pSnapshot)
{
    if (!pSnapshot || xwork__atomic_inc(&pSnapshot->iRefs) <= 1)
        return NULL;
    return pSnapshot;
}

void xworkTaskSnapshotRelease(xwork_task_snapshot *pSnapshot)
{
    size_t i;
    if (!pSnapshot || xwork__atomic_dec(&pSnapshot->iRefs) != 0)
        return;
    for (i = 0u; i < pSnapshot->iTaskCount; ++i)
    {
        free((void *)pSnapshot->pTasks[i].tInfo.sOwnerSession);
        free((void *)pSnapshot->pTasks[i].tInfo.sLabel);
        free((void *)pSnapshot->pTasks[i].tInfo.sNotify);
        free((void *)pSnapshot->pTasks[i].tInfo.sScheduleId);
    }
    free(pSnapshot->pTasks);
    free(pSnapshot);
}

size_t xworkTaskSnapshotCount(const xwork_task_snapshot *pSnapshot)
{
    return pSnapshot ? pSnapshot->iTaskCount : 0u;
}

bool xworkTaskSnapshotTaskAt(const xwork_task_snapshot *pSnapshot, size_t iIndex,
                             xwork_task_info *pInfo)
{
    if (!pSnapshot || !pInfo || iIndex >= pSnapshot->iTaskCount)
        return false;
    *pInfo = pSnapshot->pTasks[iIndex].tInfo;
    return true;
}

bool xworkTaskSnapshotFind(const xwork_task_snapshot *pSnapshot, uint64_t uTaskId,
                           xwork_task_info *pInfo)
{
    size_t i;
    if (!pSnapshot || !pInfo || !uTaskId)
        return false;
    for (i = 0u; i < pSnapshot->iTaskCount; ++i)
    {
        if (pSnapshot->pTasks[i].tInfo.uTaskId == uTaskId)
        {
            *pInfo = pSnapshot->pTasks[i].tInfo;
            return true;
        }
    }
    return false;
}

bool xworkTaskSnapshotStopRequested(const xwork_task_snapshot *pSnapshot, uint64_t uTaskId)
{
    size_t i;
    if (!pSnapshot || !uTaskId)
        return false;
    for (i = 0u; i < pSnapshot->iTaskCount; ++i)
    {
        if (pSnapshot->pTasks[i].tInfo.uTaskId == uTaskId)
            return pSnapshot->pTasks[i].bStopRequested;
    }
    return false;
}

static bool xwork__copy_result_since(const xwork_process_entry *pEntry, uint64_t uOffset,
                                     size_t iMaxBytes, xwork_task_output *pOutput)
{
    size_t iLen;
    size_t iStart;
    size_t iCopy;
    uint64_t uBase = pEntry->uResultBaseOffset;
    if (!pEntry->sResult)
    {
        pOutput->uResultStart = uOffset;
        pOutput->uResultNext = uOffset;
        return true;
    }
    iLen = strlen(pEntry->sResult);
    pOutput->bResultDropped = uOffset < uBase || pEntry->bResultDropped;
    if (uOffset < uBase)
        uOffset = uBase;
    iStart = uOffset > uBase ? (size_t)(uOffset - uBase) : 0u;
    if (iStart > iLen)
        iStart = iLen;
    iCopy = iLen - iStart;
    if (iCopy > iMaxBytes)
        iCopy = iMaxBytes;
    pOutput->uResultStart = uBase + iStart;
    pOutput->uResultNext = pOutput->uResultStart + iCopy;
    if (!iCopy)
        return true;
    pOutput->sResult = xwork__strndup(pEntry->sResult + iStart, iCopy);
    if (!pOutput->sResult)
        return false;
    pOutput->iResultSize = iCopy;
    return true;
}

bool xworkRuntimeReadTaskOutput(xwork_runtime *pRuntime, uint64_t uTaskId, uint64_t uStdoutOffset,
                                uint64_t uStderrOffset, uint64_t uResultOffset, size_t iMaxBytes,
                                xwork_task_output *pOutput, xwork_error *pError)
{
    xwork_process_entry *pEntry;
    size_t iRemain;
    xworkErrorInit(pError);
    if (!pRuntime || !uTaskId || !pOutput)
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
                         "runtime, task id, and output are required");
        return false;
    }
    xworkTaskOutputUnit(pOutput);
    if (iMaxBytes == 0u)
        iMaxBytes = 64u * 1024u;
    if (iMaxBytes > pRuntime->iMaxRetainedTaskBytes)
    {
        iMaxBytes = pRuntime->iMaxRetainedTaskBytes;
    }
    pEntry = xwork__runtime_task_find(pRuntime, uTaskId);
    if (!pEntry)
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "unknown or released task id");
        return false;
    }
    iRemain = iMaxBytes;
    if (pEntry->eKind == XWORK_TASK_PROCESS && pEntry->Output)
    {
        if (!xwork__process_output_read(pEntry->Output, false, uStdoutOffset, iRemain,
                                        &pOutput->pStdout, &pOutput->iStdoutSize,
                                        &pOutput->uStdoutNext, &pOutput->bStdoutDropped))
            goto oom;
        pOutput->uStdoutStart = pOutput->uStdoutNext - pOutput->iStdoutSize;
        iRemain -= pOutput->iStdoutSize;
        if (!xwork__process_output_read(pEntry->Output, true, uStderrOffset, iRemain,
                                        &pOutput->pStderr, &pOutput->iStderrSize,
                                        &pOutput->uStderrNext, &pOutput->bStderrDropped))
            goto oom;
        pOutput->uStderrStart = pOutput->uStderrNext - pOutput->iStderrSize;
        iRemain -= pOutput->iStderrSize;
    }
    else
    {
        pOutput->uStdoutStart = pOutput->uStdoutNext = uStdoutOffset;
        pOutput->uStderrStart = pOutput->uStderrNext = uStderrOffset;
    }
    if (!xrtMutexLock(pEntry->pStateLock))
        goto oom;
    xwork__task_refresh_locked(pEntry);
    if (!xwork__copy_result_since(pEntry, uResultOffset, iRemain, pOutput))
    {
        (void)xrtMutexUnlock(pEntry->pStateLock);
        goto oom;
    }
    pOutput->bComplete = xwork__task_terminal(pEntry->eState);
    (void)xrtMutexUnlock(pEntry->pStateLock);
    xwork__task_entry_release(pEntry);
    return true;
oom:
    xwork__task_entry_release(pEntry);
    xworkTaskOutputUnit(pOutput);
    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to read task output");
    return false;
}

bool xworkRuntimeReadTaskEvents(xwork_runtime *pRuntime, uint64_t uTaskId, uint64_t uAfterRevision,
                                xwork_task_event *pEvents, size_t iCapacity, size_t *piCount,
                                uint64_t *puNextRevision, bool *pbHistoryLost, xwork_error *pError)
{
    xwork_process_entry *pEntry;
    size_t i;
    size_t n = 0u;
    uint64_t uOldest = 0u;
    xworkErrorInit(pError);
    if (piCount)
        *piCount = 0u;
    if (puNextRevision)
        *puNextRevision = uAfterRevision;
    if (pbHistoryLost)
        *pbHistoryLost = false;
    if (!pRuntime || !uTaskId || (iCapacity && !pEvents))
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "invalid task event read request");
        return false;
    }
    pEntry = xwork__runtime_task_find(pRuntime, uTaskId);
    if (!pEntry)
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "unknown or released task id");
        return false;
    }
    if (!xrtMutexLock(pEntry->pStateLock))
    {
        xwork__task_entry_release(pEntry);
        xwork__set_error(pError, XWORK_ERROR_CONTEXT, "task state is unavailable");
        return false;
    }
    xwork__task_refresh_locked(pEntry);
    if (pEntry->iEventCount)
    {
        uOldest = pEntry->arrEvents[pEntry->iEventStart].uRevision;
        if (pbHistoryLost && uAfterRevision < uOldest && uOldest - uAfterRevision > 1u)
        {
            *pbHistoryLost = true;
        }
    }
    for (i = 0u; i < pEntry->iEventCount && n < iCapacity; ++i)
    {
        size_t iSlot =
            (pEntry->iEventStart + i) % (sizeof(pEntry->arrEvents) / sizeof(pEntry->arrEvents[0]));
        const xwork_task_event *pEvent = &pEntry->arrEvents[iSlot];
        if (pEvent->uRevision <= uAfterRevision)
            continue;
        pEvents[n++] = *pEvent;
    }
    if (puNextRevision)
    {
        *puNextRevision = n ? pEvents[n - 1u].uRevision : uAfterRevision;
    }
    if (piCount)
        *piCount = n;
    (void)xrtMutexUnlock(pEntry->pStateLock);
    xwork__task_entry_release(pEntry);
    return true;
}

xwork_result xworkRuntimeWaitTasks(xwork_runtime *pRuntime, const uint64_t *puTaskIds,
                                   size_t iTaskCount, xwork_task_wait_mode eMode,
                                   uint64_t uDeadline, xcancel *pCancel,
                                   uint64_t *puCompletedTaskId, xwork_error *pError)
{
    size_t i;
    xworkErrorInit(pError);
    if (puCompletedTaskId)
        *puCompletedTaskId = 0u;
    if (!pRuntime || !puTaskIds || iTaskCount == 0u || iTaskCount > 65536u ||
        (eMode != XWORK_TASK_WAIT_ANY && eMode != XWORK_TASK_WAIT_ALL))
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "invalid task wait request");
        return XWORK_RESULT_ERROR;
    }
    for (i = 0u; i < iTaskCount; ++i)
    {
        xwork_process_entry *pEntry = xwork__runtime_task_find(pRuntime, puTaskIds[i]);
        if (!pEntry)
        {
            xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
                             "task wait contains an unknown or released id");
            return XWORK_RESULT_ERROR;
        }
        xwork__task_entry_release(pEntry);
    }
    for (;;)
    {
        size_t iDone = 0u;
        uint64_t uFirst = 0u;
        for (i = 0u; i < iTaskCount; ++i)
        {
            xwork_process_entry *pEntry = xwork__runtime_task_find(pRuntime, puTaskIds[i]);
            if (!pEntry)
            {
                xwork__set_error(pError, XWORK_ERROR_CONTEXT, "task was released while waiting");
                return XWORK_RESULT_ERROR;
            }
            if (!xwork__task_running(pEntry))
            {
                ++iDone;
                if (!uFirst)
                    uFirst = puTaskIds[i];
            }
            xwork__task_entry_release(pEntry);
        }
        if ((eMode == XWORK_TASK_WAIT_ANY && iDone != 0u) ||
            (eMode == XWORK_TASK_WAIT_ALL && iDone == iTaskCount))
        {
            if (puCompletedTaskId)
                *puCompletedTaskId = uFirst;
            return XWORK_RESULT_OK;
        }
        if (pCancel && xrtCancelRequested(pCancel))
        {
            xwork__set_error(pError, XWORK_ERROR_CANCELLED, "task wait was cancelled");
            return XWORK_RESULT_CANCELLED;
        }
        if (uDeadline != XLLM_DEADLINE_NEVER && xllmDeadlineExpired(uDeadline))
        {
            xwork__set_error(pError, XWORK_ERROR_TIMEOUT, "task wait deadline expired");
            return XWORK_RESULT_TIMEOUT;
        }
        xrtSleep(5u);
    }
}

bool xworkRuntimeCancelTask(xwork_runtime *pRuntime, uint64_t uTaskId, xwork_error *pError)
{
    xwork_process_entry *pEntry;
    bool bRequestAgent = false;
    bool bAccepted = true;
    xworkErrorInit(pError);
    pEntry = xwork__runtime_task_find(pRuntime, uTaskId);
    if (!pEntry)
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "unknown or released task id");
        return false;
    }
    if (!xrtMutexLock(pEntry->pStateLock))
        goto unavailable;
    xwork__task_refresh_locked(pEntry);
    if (!xwork__task_terminal(pEntry->eState))
    {
        if (pEntry->eKind == XWORK_TASK_PROCESS)
        {
            bAccepted = xwork__task_stop_process_locked(pEntry, "kill_tree");
            if (!bAccepted)
            {
                xwork__task_refresh_locked(pEntry);
                bAccepted = xwork__task_terminal(pEntry->eState);
            }
        }
        else
        {
            xwork__task_mark_stop_locked(pEntry);
            bRequestAgent = pEntry->eKind == XWORK_TASK_AGENT;
            if (pEntry->eKind == XWORK_TASK_SCHEDULED)
            {
                pEntry->bDone = true;
                xwork__task_set_state_locked(pEntry, XWORK_TASK_CANCELLED);
            }
        }
    }
    (void)xrtMutexUnlock(pEntry->pStateLock);
    if (bRequestAgent && pEntry->pChildCancel)
    {
        (void)xrtCancelRequest(pEntry->pChildCancel);
    }
    if (!bAccepted)
    {
        xwork__task_entry_release(pEntry);
        xwork__set_error(pError, XWORK_ERROR_IO, "process task cancellation request failed");
        return false;
    }
    xwork__task_entry_release(pEntry);
    return true;
unavailable:
    xwork__task_entry_release(pEntry);
    xwork__set_error(pError, XWORK_ERROR_CONTEXT, "task state is unavailable");
    return false;
}

bool xworkRuntimeReleaseTask(xwork_runtime *pRuntime, uint64_t uTaskId, xwork_error *pError)
{
    xwork_process_entry *pEntry = NULL;
    size_t i;
    xworkErrorInit(pError);
    if (!pRuntime || !uTaskId || !xrtMutexLock(pRuntime->pLock))
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "runtime and task id are required");
        return false;
    }
    for (i = 0u; i < pRuntime->iTaskCount; ++i)
    {
        xwork_process_entry *pCandidate = pRuntime->ppTasks[i];
        if (pCandidate->uId != uTaskId || xwork__atomic_load(&pCandidate->iReady) == 0)
            continue;
        if (!xrtMutexLock(pCandidate->pStateLock))
            break;
        xwork__task_refresh_locked(pCandidate);
        if (!xwork__task_terminal(pCandidate->eState))
        {
            (void)xrtMutexUnlock(pCandidate->pStateLock);
            (void)xrtMutexUnlock(pRuntime->pLock);
            xwork__set_error(pError, XWORK_ERROR_CONTEXT, "cannot release an active task");
            return false;
        }
        (void)xrtMutexUnlock(pCandidate->pStateLock);
        pEntry = pCandidate;
        pRuntime->ppTasks[i] = pRuntime->ppTasks[pRuntime->iTaskCount - 1u];
        pRuntime->ppTasks[pRuntime->iTaskCount - 1u] = NULL;
        --pRuntime->iTaskCount;
        break;
    }
    (void)xrtMutexUnlock(pRuntime->pLock);
    if (!pEntry)
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "unknown or released task id");
        return false;
    }
    xwork__task_entry_release(pEntry);
    return true;
}

static xwork_process_entry *xwork__runtime_task_allocate(xwork_runtime *pRuntime,
                                                         uint64_t uRequestedId, xwork_error *pError)
{
    xwork_process_entry *pEntry;
    xwork_process_entry **ppNew;
    size_t i;
    size_t iCap;
    if (!pRuntime || !xrtMutexLock(pRuntime->pLock))
        return NULL;
    if (pRuntime->iTaskCount >= pRuntime->uMaxTasks)
    {
        (void)xrtMutexUnlock(pRuntime->pLock);
        xwork__set_error(pError, XWORK_ERROR_LIMIT, "runtime task limit reached");
        return NULL;
    }
    if (uRequestedId)
    {
        for (i = 0u; i < pRuntime->iTaskCount; ++i)
        {
            if (pRuntime->ppTasks[i]->uId == uRequestedId)
            {
                (void)xrtMutexUnlock(pRuntime->pLock);
                xwork__set_error(pError, XWORK_ERROR_CONTEXT, "task id is already in use");
                return NULL;
            }
        }
    }
    if (pRuntime->iTaskCount == pRuntime->iTaskCap)
    {
        iCap = pRuntime->iTaskCap ? pRuntime->iTaskCap * 2u : 8u;
        if (iCap > pRuntime->uMaxTasks)
            iCap = pRuntime->uMaxTasks;
        ppNew = (xwork_process_entry **)realloc(pRuntime->ppTasks, iCap * sizeof(*ppNew));
        if (!ppNew)
            goto oom_locked;
        pRuntime->ppTasks = ppNew;
        pRuntime->iTaskCap = iCap;
    }
    pEntry = (xwork_process_entry *)calloc(1u, sizeof(*pEntry));
    if (!pEntry)
        goto oom_locked;
    pEntry->pStateLock = xrtMutexCreate();
    if (!pEntry->pStateLock)
    {
        free(pEntry);
        goto oom_locked;
    }
    pEntry->iRefs = 2;
    pEntry->pRuntime = pRuntime;
    pEntry->uId = uRequestedId ? uRequestedId : ++pRuntime->uNextTaskId;
    if (pEntry->uId == 0u)
        pEntry->uId = ++pRuntime->uNextTaskId;
    if (pEntry->uId > pRuntime->uNextTaskId)
        pRuntime->uNextTaskId = pEntry->uId;
    pRuntime->ppTasks[pRuntime->iTaskCount++] = pEntry;
    (void)xrtMutexUnlock(pRuntime->pLock);
    return pEntry;
oom_locked:
    (void)xrtMutexUnlock(pRuntime->pLock);
    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to allocate task entry");
    return NULL;
}

static bool xwork__task_copy_metadata(xwork_process_entry *pEntry, const char *sOwnerSession,
                                      const char *sLabel, const char *sNotify)
{
    pEntry->sOwnerSession = sOwnerSession ? xwork__strdup(sOwnerSession) : NULL;
    pEntry->sLabel = sLabel ? xwork__strdup(sLabel) : NULL;
    pEntry->sCommand = sLabel ? xwork__strdup(sLabel) : NULL;
    pEntry->sNotify = sNotify ? xwork__strdup(sNotify) : NULL;
    return (!sOwnerSession || pEntry->sOwnerSession) &&
           (!sLabel || (pEntry->sLabel && pEntry->sCommand)) && (!sNotify || pEntry->sNotify);
}

bool xworkRuntimeCreateScheduledTask(xwork_runtime *pRuntime,
                                     const xwork_scheduled_task_config *pConfig, uint64_t *puTaskId,
                                     xwork_error *pError)
{
    xwork_process_entry *pEntry;
    xworkErrorInit(pError);
    if (puTaskId)
        *puTaskId = 0u;
    if (!pRuntime || !pConfig ||
        !xwork__record_valid(pConfig->uSize, pConfig->uAbiVersion, sizeof(*pConfig)) ||
        pConfig->iScheduledAtUs < 0 || !pConfig->sLabel || !pConfig->sLabel[0] ||
        strlen(pConfig->sLabel) > 2048u ||
        (pConfig->sOwnerSession && strlen(pConfig->sOwnerSession) > 4096u) ||
        (pConfig->sNotify && strlen(pConfig->sNotify) > 500u) ||
        (pConfig->sScheduleId &&
         (!pConfig->sScheduleId[0] || strlen(pConfig->sScheduleId) > 128u)) ||
        (!!pConfig->sScheduleId != (pConfig->uScheduleGeneration != 0u)))
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
                         "invalid scheduled task configuration");
        return false;
    }
    pEntry = xwork__runtime_task_allocate(pRuntime, 0u, pError);
    if (!pEntry)
        return false;
    pEntry->uOwnerAgentId = pConfig->uOwnerAgentId;
    pEntry->uOwnerRunId = pConfig->uOwnerRunId;
    pEntry->uParentTaskId = pConfig->uParentTaskId;
    pEntry->eKind = XWORK_TASK_SCHEDULED;
    pEntry->eState = XWORK_TASK_PENDING;
    pEntry->uRevision = 0u;
    pEntry->iCreatedAtUs = xllmUnixUs();
    pEntry->iScheduledAtUs = pConfig->iScheduledAtUs;
    pEntry->uScheduleGeneration = pConfig->uScheduleGeneration;
    if (!xwork__task_copy_metadata(pEntry, pConfig->sOwnerSession, pConfig->sLabel,
                                   pConfig->sNotify) ||
        (pConfig->sScheduleId && !(pEntry->sScheduleId = xwork__strdup(pConfig->sScheduleId))))
    {
        if (xrtMutexLock(pRuntime->pLock))
        {
            size_t i;
            for (i = 0u; i < pRuntime->iTaskCount; ++i)
            {
                if (pRuntime->ppTasks[i] == pEntry)
                {
                    pRuntime->ppTasks[i] = pRuntime->ppTasks[pRuntime->iTaskCount - 1u];
                    --pRuntime->iTaskCount;
                    break;
                }
            }
            (void)xrtMutexUnlock(pRuntime->pLock);
        }
        xwork__task_entry_release(pEntry);
        xwork__task_entry_release(pEntry);
        xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY,
                         "failed to copy scheduled task metadata");
        return false;
    }
    xwork__task_record_event_locked(pEntry, XWORK_TASK_EVENT_CREATED);
    xwork__atomic_store(&pEntry->iReady, 1);
    if (puTaskId)
        *puTaskId = pEntry->uId;
    xwork__task_entry_release(pEntry);
    return true;
}

bool xworkRuntimeStartScheduledTask(xwork_runtime *pRuntime, uint64_t uTaskId, xwork_error *pError)
{
    xwork_process_entry *pEntry = xwork__runtime_task_find(pRuntime, uTaskId);
    xworkErrorInit(pError);
    if (!pEntry)
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "unknown or released task id");
        return false;
    }
    if (!xrtMutexLock(pEntry->pStateLock))
        goto unavailable;
    if (pEntry->eKind != XWORK_TASK_SCHEDULED || pEntry->eState != XWORK_TASK_PENDING)
    {
        (void)xrtMutexUnlock(pEntry->pStateLock);
        xwork__task_entry_release(pEntry);
        xwork__set_error(pError, XWORK_ERROR_CONTEXT, "scheduled task is not pending");
        return false;
    }
    xwork__task_set_state_locked(pEntry, XWORK_TASK_RUNNING);
    (void)xrtMutexUnlock(pEntry->pStateLock);
    xwork__task_entry_release(pEntry);
    return true;
unavailable:
    xwork__task_entry_release(pEntry);
    xwork__set_error(pError, XWORK_ERROR_CONTEXT, "task state is unavailable");
    return false;
}

static xwork_task_state xwork__task_state_from_result(xwork_result eResult)
{
    switch (eResult)
    {
    case XWORK_RESULT_OK:
        return XWORK_TASK_SUCCEEDED;
    case XWORK_RESULT_CANCELLED:
        return XWORK_TASK_CANCELLED;
    case XWORK_RESULT_TIMEOUT:
        return XWORK_TASK_TIMED_OUT;
    default:
        return XWORK_TASK_FAILED;
    }
}

bool xworkRuntimeFinishScheduledTask(xwork_runtime *pRuntime, uint64_t uTaskId,
                                     xwork_result eResult, const char *sResult, xwork_error *pError)
{
    xwork_process_entry *pEntry = xwork__runtime_task_find(pRuntime, uTaskId);
    char *sCopy = NULL;
    size_t iLen = sResult ? strlen(sResult) : 0u;
    size_t iKeep;
    xworkErrorInit(pError);
    if (!pEntry || (eResult != XWORK_RESULT_OK && eResult != XWORK_RESULT_ERROR &&
                    eResult != XWORK_RESULT_CANCELLED && eResult != XWORK_RESULT_LIMIT &&
                    eResult != XWORK_RESULT_TIMEOUT))
    {
        xwork__task_entry_release(pEntry);
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "invalid scheduled task completion");
        return false;
    }
    iKeep = iLen;
    if (iKeep > pRuntime->iMaxRetainedTaskBytes)
    {
        iKeep = pRuntime->iMaxRetainedTaskBytes;
    }
    if (iKeep)
    {
        sCopy = xwork__strndup(sResult + (iLen - iKeep), iKeep);
        if (!sCopy)
        {
            xwork__task_entry_release(pEntry);
            xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY,
                             "failed to retain scheduled task result");
            return false;
        }
    }
    if (!xrtMutexLock(pEntry->pStateLock))
    {
        free(sCopy);
        goto unavailable;
    }
    if (pEntry->eKind != XWORK_TASK_SCHEDULED || pEntry->eState != XWORK_TASK_RUNNING)
    {
        (void)xrtMutexUnlock(pEntry->pStateLock);
        free(sCopy);
        xwork__task_entry_release(pEntry);
        xwork__set_error(pError, XWORK_ERROR_CONTEXT, "scheduled task is not running");
        return false;
    }
    free(pEntry->sResult);
    pEntry->sResult = sCopy;
    pEntry->uResultBaseOffset = iLen - iKeep;
    pEntry->bResultDropped = iKeep != iLen;
    pEntry->bDone = true;
    pEntry->bSuccess = eResult == XWORK_RESULT_OK;
    xwork__task_set_state_locked(pEntry, xwork__task_state_from_result(eResult));
    pEntry->uExitedUs = xllmClockUs();
    (void)xrtMutexUnlock(pEntry->pStateLock);
    xwork__task_entry_release(pEntry);
    return true;
unavailable:
    xwork__task_entry_release(pEntry);
    xwork__set_error(pError, XWORK_ERROR_CONTEXT, "task state is unavailable");
    return false;
}

bool xworkRuntimeRestoreTask(xwork_runtime *pRuntime, const xwork_task_info *pInfo,
                             uint64_t *puTaskId, xwork_error *pError)
{
    xwork_process_entry *pEntry;
    xwork_task_state eState;
    xworkErrorInit(pError);
    if (puTaskId)
        *puTaskId = 0u;
    if (!pRuntime || !pInfo ||
        !xwork__record_valid(pInfo->uSize, pInfo->uAbiVersion, sizeof(*pInfo)) ||
        pInfo->uTaskId == UINT64_MAX || pInfo->uRevision > UINT64_MAX - 2u ||
        (pInfo->uTaskId != 0u && pInfo->uParentTaskId == pInfo->uTaskId) ||
        pInfo->eKind < XWORK_TASK_PROCESS || pInfo->eKind > XWORK_TASK_SCHEDULED ||
        pInfo->eState < XWORK_TASK_PENDING || pInfo->eState > XWORK_TASK_LOST ||
        pInfo->iCreatedAtUs < 0 || pInfo->iStartedAtUs < 0 || pInfo->iEndedAtUs < 0 ||
        pInfo->iScheduledAtUs < 0 ||
        (pInfo->iStartedAtUs && pInfo->iCreatedAtUs && pInfo->iStartedAtUs < pInfo->iCreatedAtUs) ||
        (pInfo->iEndedAtUs && pInfo->iCreatedAtUs && pInfo->iEndedAtUs < pInfo->iCreatedAtUs) ||
        (pInfo->iEndedAtUs && pInfo->iStartedAtUs && pInfo->iEndedAtUs < pInfo->iStartedAtUs) ||
        (pInfo->sOwnerSession && strlen(pInfo->sOwnerSession) > 4096u) ||
        (pInfo->sLabel && strlen(pInfo->sLabel) > 2048u) ||
        (pInfo->sNotify && strlen(pInfo->sNotify) > 500u) ||
        (pInfo->sScheduleId && (!pInfo->sScheduleId[0] || strlen(pInfo->sScheduleId) > 128u)) ||
        (!!pInfo->sScheduleId != (pInfo->uScheduleGeneration != 0u)))
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "invalid restored task metadata");
        return false;
    }
    pEntry = xwork__runtime_task_allocate(pRuntime, pInfo->uTaskId, pError);
    if (!pEntry)
        return false;
    if (!xwork__task_copy_metadata(pEntry, pInfo->sOwnerSession, pInfo->sLabel, pInfo->sNotify))
        goto oom;
    if (pInfo->sScheduleId)
    {
        pEntry->sScheduleId = xwork__strdup(pInfo->sScheduleId);
        if (!pEntry->sScheduleId)
            goto oom;
    }
    pEntry->uOwnerAgentId = pInfo->uOwnerAgentId;
    pEntry->uOwnerRunId = pInfo->uOwnerRunId;
    pEntry->uParentTaskId = pInfo->uParentTaskId;
    pEntry->eKind = pInfo->eKind;
    eState = pInfo->eState;
    if (!xwork__task_terminal(eState) &&
        !(pInfo->eKind == XWORK_TASK_SCHEDULED && eState == XWORK_TASK_PENDING))
        eState = XWORK_TASK_LOST;
    pEntry->eState = pInfo->eState;
    pEntry->uRevision = pInfo->uRevision ? pInfo->uRevision : 1u;
    pEntry->iCreatedAtUs = pInfo->iCreatedAtUs ? pInfo->iCreatedAtUs : xllmUnixUs();
    pEntry->iStartedAtUs = pInfo->iStartedAtUs;
    pEntry->iEndedAtUs = pInfo->iEndedAtUs;
    if (xwork__task_terminal(eState) && !pEntry->iEndedAtUs)
    {
        pEntry->iEndedAtUs = xllmUnixUs();
    }
    pEntry->iScheduledAtUs = pInfo->iScheduledAtUs;
    pEntry->uScheduleGeneration = pInfo->uScheduleGeneration;
    pEntry->bExitStatusValid = pInfo->bExitStatusValid;
    pEntry->iExitCode = pInfo->iExitCode;
    pEntry->iExitSignal = pInfo->iExitSignal;
    pEntry->iStopReason = pInfo->iStopReason;
    pEntry->bNoticeTaken = pInfo->bNoticeTaken;
    pEntry->bDone = xwork__task_terminal(eState);
    pEntry->bSuccess = eState == XWORK_TASK_SUCCEEDED;
    xwork__task_record_event_locked(pEntry, XWORK_TASK_EVENT_RESTORED);
    if (eState != pEntry->eState)
    {
        xwork__task_set_state_locked(pEntry, eState);
    }
    xwork__atomic_store(&pEntry->iReady, 1);
    if (puTaskId)
        *puTaskId = pEntry->uId;
    xwork__task_entry_release(pEntry);
    return true;
oom:
    if (xrtMutexLock(pRuntime->pLock))
    {
        size_t i;
        for (i = 0u; i < pRuntime->iTaskCount; ++i)
        {
            if (pRuntime->ppTasks[i] == pEntry)
            {
                pRuntime->ppTasks[i] = pRuntime->ppTasks[pRuntime->iTaskCount - 1u];
                --pRuntime->iTaskCount;
                break;
            }
        }
        (void)xrtMutexUnlock(pRuntime->pLock);
    }
    xwork__task_entry_release(pEntry);
    xwork__task_entry_release(pEntry);
    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to copy restored task metadata");
    return false;
}
