#include "../internal/xwork_internal.h"

/* Runtime-owned recurrence and misfire scheduler. */

#define XWORK_SCHEDULE_DEFAULT_GRACE_SECONDS 60u
#define XWORK_SCHEDULE_MAX_CATCH_UP 64u
#define XWORK_SCHEDULE_MAX_CONCURRENT 16u
#define XWORK_SCHEDULE_ID_MAX 128u
#define XWORK_SCHEDULE_INPUT_MAX (1024u * 1024u)

static bool xwork__schedule_terminal(xwork_task_state eState)
{
    return eState >= XWORK_TASK_SUCCEEDED && eState <= XWORK_TASK_LOST;
}

static bool xwork__schedule_id_valid(const char *sId)
{
    size_t i;
    if (!sId || !sId[0] || strlen(sId) > XWORK_SCHEDULE_ID_MAX)
        return false;
    for (i = 0u; sId[i]; ++i)
    {
        unsigned char ch = (unsigned char)sId[i];
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
              ch == '_' || ch == '-' || ch == '.'))
            return false;
    }
    return true;
}

void xworkScheduleConfigInit(xwork_schedule_config *pConfig)
{
    if (!pConfig)
        return;
    memset(pConfig, 0, sizeof(*pConfig));
    pConfig->uSize = (uint32_t)sizeof(*pConfig);
    pConfig->uAbiVersion = XWORK_ABI_VERSION;
    pConfig->eFrequency = XWORK_SCHEDULE_ONCE;
    pConfig->uInterval = 1u;
    pConfig->eTimezone = XWORK_SCHEDULE_TIMEZONE_UTC;
    pConfig->eFoldPolicy = XWORK_SCHEDULE_FOLD_EARLIER;
    pConfig->eMisfirePolicy = XWORK_SCHEDULE_MISFIRE_RUN_ONCE;
    pConfig->uMisfireGraceSeconds = XWORK_SCHEDULE_DEFAULT_GRACE_SECONDS;
    pConfig->uMaxCatchUp = 1u;
    pConfig->eOverlapPolicy = XWORK_SCHEDULE_OVERLAP_QUEUE_ONE;
    pConfig->uMaxConcurrentRuns = 1u;
    pConfig->bEnabled = true;
}

void xworkScheduleInfoInit(xwork_schedule_info *pInfo)
{
    if (!pInfo)
        return;
    memset(pInfo, 0, sizeof(*pInfo));
    pInfo->uSize = (uint32_t)sizeof(*pInfo);
    pInfo->uAbiVersion = XWORK_ABI_VERSION;
    xworkScheduleConfigInit(&pInfo->tConfig);
}

void xworkScheduleClaimInit(xwork_schedule_claim *pClaim)
{
    if (!pClaim)
        return;
    memset(pClaim, 0, sizeof(*pClaim));
    pClaim->uSize = (uint32_t)sizeof(*pClaim);
    pClaim->uAbiVersion = XWORK_ABI_VERSION;
}

static bool xwork__schedule_config_valid(const xwork_schedule_config *pConfig)
{
    uint32_t uInterval;
    uint32_t uCatchUp;
    uint32_t uConcurrent;
    if (!pConfig || !xwork__record_valid(pConfig->uSize, pConfig->uAbiVersion, sizeof(*pConfig)) ||
        !xwork__schedule_id_valid(pConfig->sScheduleId) || !pConfig->sLabel ||
        !pConfig->sLabel[0] || strlen(pConfig->sLabel) > 2048u ||
        (pConfig->sNotify && strlen(pConfig->sNotify) > 500u) ||
        (pConfig->sOwnerSession && strlen(pConfig->sOwnerSession) > 4096u) ||
        (pConfig->sAgentProfile &&
         (!pConfig->sAgentProfile[0] || strlen(pConfig->sAgentProfile) > 256u)) ||
        (pConfig->sInput && strlen(pConfig->sInput) > XWORK_SCHEDULE_INPUT_MAX) ||
        pConfig->iStartAtUs <= 0 || pConfig->eFrequency < XWORK_SCHEDULE_ONCE ||
        pConfig->eFrequency > XWORK_SCHEDULE_WEEKLY ||
        pConfig->eTimezone < XWORK_SCHEDULE_TIMEZONE_UTC ||
        pConfig->eTimezone > XWORK_SCHEDULE_TIMEZONE_SYSTEM_LOCAL ||
        pConfig->eFoldPolicy < XWORK_SCHEDULE_FOLD_EARLIER ||
        pConfig->eFoldPolicy > XWORK_SCHEDULE_FOLD_LATER ||
        pConfig->eMisfirePolicy < XWORK_SCHEDULE_MISFIRE_SKIP ||
        pConfig->eMisfirePolicy > XWORK_SCHEDULE_MISFIRE_CATCH_UP ||
        pConfig->eOverlapPolicy < XWORK_SCHEDULE_OVERLAP_SKIP ||
        pConfig->eOverlapPolicy > XWORK_SCHEDULE_OVERLAP_QUEUE_ONE ||
        pConfig->uWeekdayMask > 0x7fu || pConfig->uMisfireGraceSeconds > 366u * 24u * 60u * 60u)
        return false;
    if (pConfig->eTimezone == XWORK_SCHEDULE_TIMEZONE_FIXED_OFFSET)
    {
        if (pConfig->iUtcOffsetSeconds <= -86400 || pConfig->iUtcOffsetSeconds >= 86400)
            return false;
    }
    else if (pConfig->iUtcOffsetSeconds != 0)
    {
        return false;
    }
    uInterval = pConfig->uInterval ? pConfig->uInterval : 1u;
    uCatchUp = pConfig->uMaxCatchUp ? pConfig->uMaxCatchUp : 1u;
    uConcurrent = pConfig->uMaxConcurrentRuns ? pConfig->uMaxConcurrentRuns : 1u;
    return uInterval <= 1000000u && uCatchUp <= XWORK_SCHEDULE_MAX_CATCH_UP &&
           uConcurrent <= XWORK_SCHEDULE_MAX_CONCURRENT;
}

static bool xwork__schedule_split(const xwork_schedule_config *pConfig, int64_t iTimeUs,
                                  xdatetime *pDateTime)
{
    if (pConfig->eTimezone == XWORK_SCHEDULE_TIMEZONE_SYSTEM_LOCAL)
    {
        return xrtTimeLocal((xtime)(iTimeUs / 1000 + XRT_TIME_UNIX_EPOCH), pDateTime);
    }
    return xrtTimeSplitAt(
        (xtime)(iTimeUs / 1000 + XRT_TIME_UNIX_EPOCH),
        pConfig->eTimezone == XWORK_SCHEDULE_TIMEZONE_FIXED_OFFSET ? pConfig->iUtcOffsetSeconds : 0,
        pDateTime);
}

static bool xwork__schedule_compose(const xwork_schedule_config *pConfig,
                                    const xdatetime *pDateTime, int64_t *piTimeUs)
{
    xdatetime tValue = *pDateTime;
    xtime iTime;
    if (pConfig->eTimezone == XWORK_SCHEDULE_TIMEZONE_SYSTEM_LOCAL)
    {
        tValue.Offset = 0;
        if (!xrtTimeFromLocal(&tValue,
                              pConfig->eFoldPolicy == XWORK_SCHEDULE_FOLD_LATER
                                  ? XTIME_FOLD_LATER
                                  : XTIME_FOLD_EARLIER,
                              &iTime))
            return false;
    }
    else
    {
        tValue.Offset = pConfig->eTimezone == XWORK_SCHEDULE_TIMEZONE_FIXED_OFFSET
                            ? pConfig->iUtcOffsetSeconds
                            : 0;
        if (!xrtTimeMake(&tValue, &iTime))
            return false;
    }
    iTime -= XRT_TIME_UNIX_EPOCH;
    if (iTime > INT64_MAX / 1000 || iTime < INT64_MIN / 1000)
        return false;
    *piTimeUs = (int64_t)iTime * 1000;
    return true;
}

static bool xwork__schedule_date(const xdatetime *pDateTime, xtime *pDate)
{
    return xrtDate(pDateTime->Year, pDateTime->Month, pDateTime->Day, pDate);
}

static bool xwork__schedule_calendar_next(const xwork_schedule_config *pConfig,
                                          int64_t iAfterExclusiveUs, bool bWeekly,
                                          int64_t *piNextUs, bool *pbHas)
{
    xdatetime tAnchor;
    xdatetime tAfter;
    xtime iAnchorDay;
    xtime iAfterDay;
    int64 iDayDiff = 0;
    uint32_t uInterval = pConfig->uInterval ? pConfig->uInterval : 1u;
    uint8_t uMask;
    uint32_t uAttempt;
    if (!xwork__schedule_split(pConfig, pConfig->iStartAtUs, &tAnchor) ||
        !xwork__schedule_split(pConfig, iAfterExclusiveUs >= 0 ? iAfterExclusiveUs : 0, &tAfter) ||
        !xwork__schedule_date(&tAnchor, &iAnchorDay) ||
        !xwork__schedule_date(&tAfter, &iAfterDay) ||
        !xrtDateDiff(iAnchorDay, iAfterDay, XTIME_UNIT_DAY, &iDayDiff))
    {
        return false;
    }
    if (!bWeekly)
    {
        int64 iStep = iDayDiff > 0 ? iDayDiff / (int64)uInterval : 0;
        for (uAttempt = 0u; uAttempt < 8u; ++uAttempt, ++iStep)
        {
            xtime iCandidateDay;
            xdatetime tCandidate;
            int64 iCandidate;
            if (iStep > INT64_MAX / (int64)uInterval ||
                !xrtTimeAdd(iAnchorDay, iStep * (int64)uInterval, XTIME_UNIT_DAY, &iCandidateDay) ||
                !xrtTimeSplit(iCandidateDay, &tCandidate))
            {
                *pbHas = false;
                return true;
            }
            tCandidate.Hour = tAnchor.Hour;
            tCandidate.Minute = tAnchor.Minute;
            tCandidate.Second = tAnchor.Second;
            tCandidate.Millisecond = tAnchor.Millisecond;
            if (!xwork__schedule_compose(pConfig, &tCandidate, &iCandidate))
            {
                continue; /* A system-local DST gap skips this occurrence. */
            }
            iCandidate += pConfig->iStartAtUs % 1000;
            if (iCandidate >= pConfig->iStartAtUs && iCandidate > iAfterExclusiveUs)
            {
                *piNextUs = iCandidate;
                *pbHas = true;
                return true;
            }
        }
        return false;
    }

    uMask = pConfig->uWeekdayMask ? pConfig->uWeekdayMask : (uint8_t)(1u << tAnchor.Weekday);
    {
        xtime iAnchorWeek;
        int64 iWeekDiff;
        int64 iAlignedWeek;
        if (!xrtTimeAdd(iAnchorDay, -(int64)tAnchor.Weekday, XTIME_UNIT_DAY, &iAnchorWeek) ||
            !xrtDateDiff(iAnchorWeek, iAfterDay, XTIME_UNIT_WEEK, &iWeekDiff))
            return false;
        iAlignedWeek = iWeekDiff > 0 ? (iWeekDiff / (int64)uInterval) * (int64)uInterval : 0;
        for (uAttempt = 0u; uAttempt < 8u; ++uAttempt)
        {
            int iWeekday;
            for (iWeekday = XTIME_SUNDAY; iWeekday <= XTIME_SATURDAY; ++iWeekday)
            {
                xtime iCandidateDay;
                xdatetime tCandidate;
                int64 iCandidate;
                int64 iDays;
                if (!(uMask & (uint8_t)(1u << iWeekday)))
                    continue;
                if (iAlignedWeek > (INT64_MAX - iWeekday) / 7)
                {
                    *pbHas = false;
                    return true;
                }
                iDays = iAlignedWeek * 7 + iWeekday;
                if (!xrtTimeAdd(iAnchorWeek, iDays, XTIME_UNIT_DAY, &iCandidateDay) ||
                    !xrtTimeSplit(iCandidateDay, &tCandidate))
                {
                    *pbHas = false;
                    return true;
                }
                tCandidate.Hour = tAnchor.Hour;
                tCandidate.Minute = tAnchor.Minute;
                tCandidate.Second = tAnchor.Second;
                tCandidate.Millisecond = tAnchor.Millisecond;
                if (!xwork__schedule_compose(pConfig, &tCandidate, &iCandidate))
                    continue;
                iCandidate += pConfig->iStartAtUs % 1000;
                if (iCandidate >= pConfig->iStartAtUs && iCandidate > iAfterExclusiveUs)
                {
                    *piNextUs = iCandidate;
                    *pbHas = true;
                    return true;
                }
            }
            if (iAlignedWeek > INT64_MAX - (int64)uInterval)
            {
                *pbHas = false;
                return true;
            }
            iAlignedWeek += (int64)uInterval;
        }
    }
    return false;
}

bool xworkScheduleNextOccurrence(const xwork_schedule_config *pConfig, int64_t iAfterExclusiveUs,
                                 int64_t *piNextOccurrenceUs, bool *pbHasOccurrence,
                                 xwork_error *pError)
{
    uint32_t uInterval;
    int64_t iPeriod;
    int64_t iDelta;
    int64_t iSteps;
    xworkErrorInit(pError);
    if (piNextOccurrenceUs)
        *piNextOccurrenceUs = 0;
    if (pbHasOccurrence)
        *pbHasOccurrence = false;
    if (!piNextOccurrenceUs || !pbHasOccurrence || !xwork__schedule_config_valid(pConfig) ||
        iAfterExclusiveUs < -1)
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
                         "invalid schedule recurrence request");
        return false;
    }
    if (pConfig->eFrequency == XWORK_SCHEDULE_ONCE)
    {
        if (pConfig->iStartAtUs > iAfterExclusiveUs)
        {
            *piNextOccurrenceUs = pConfig->iStartAtUs;
            *pbHasOccurrence = true;
        }
        return true;
    }
    if (pConfig->eFrequency == XWORK_SCHEDULE_DAILY || pConfig->eFrequency == XWORK_SCHEDULE_WEEKLY)
    {
        if (!xwork__schedule_calendar_next(pConfig, iAfterExclusiveUs,
                                           pConfig->eFrequency == XWORK_SCHEDULE_WEEKLY,
                                           piNextOccurrenceUs, pbHasOccurrence))
        {
            xwork__set_error(pError, XWORK_ERROR_TOOL, "schedule calendar conversion failed");
            return false;
        }
        return true;
    }
    uInterval = pConfig->uInterval ? pConfig->uInterval : 1u;
    iPeriod = pConfig->eFrequency == XWORK_SCHEDULE_MINUTELY ? INT64_C(60) * INT64_C(1000000)
                                                             : INT64_C(3600) * INT64_C(1000000);
    if (uInterval > (uint64_t)INT64_MAX / (uint64_t)iPeriod)
    {
        xwork__set_error(pError, XWORK_ERROR_LIMIT, "schedule interval overflows epoch time");
        return false;
    }
    iPeriod *= (int64_t)uInterval;
    if (pConfig->iStartAtUs > iAfterExclusiveUs)
    {
        *piNextOccurrenceUs = pConfig->iStartAtUs;
        *pbHasOccurrence = true;
        return true;
    }
    iDelta = iAfterExclusiveUs - pConfig->iStartAtUs;
    iSteps = iDelta / iPeriod + 1;
    if (iSteps > (INT64_MAX - pConfig->iStartAtUs) / iPeriod)
        return true;
    *piNextOccurrenceUs = pConfig->iStartAtUs + iSteps * iPeriod;
    *pbHasOccurrence = true;
    return true;
}

static void xwork__schedule_entry_unit(xwork_schedule_entry *pEntry)
{
    if (!pEntry)
        return;
    free(pEntry->sScheduleId);
    free(pEntry->sLabel);
    free(pEntry->sNotify);
    free(pEntry->sOwnerSession);
    free(pEntry->sAgentProfile);
    free(pEntry->sInput);
    free(pEntry);
}

static xwork_schedule_entry *xwork__schedule_find_locked(xwork_runtime *pRuntime,
                                                         const char *sScheduleId)
{
    size_t i;
    for (i = 0u; pRuntime && i < pRuntime->iScheduleCount; ++i)
    {
        xwork_schedule_entry *pEntry = pRuntime->ppSchedules[i];
        if (pEntry && pEntry->bRegistered && strcmp(pEntry->sScheduleId, sScheduleId) == 0)
            return pEntry;
    }
    return NULL;
}

static xwork_schedule_entry *xwork__schedule_copy(const xwork_schedule_config *pConfig,
                                                  xwork_error *pError)
{
    xwork_schedule_entry *pEntry = (xwork_schedule_entry *)calloc(1u, sizeof(*pEntry));
    bool bHas = false;
    if (!pEntry)
        goto oom;
    pEntry->sScheduleId = xwork__strdup(pConfig->sScheduleId);
    pEntry->sLabel = xwork__strdup(pConfig->sLabel);
    pEntry->sNotify = pConfig->sNotify ? xwork__strdup(pConfig->sNotify) : NULL;
    pEntry->sOwnerSession = pConfig->sOwnerSession ? xwork__strdup(pConfig->sOwnerSession) : NULL;
    pEntry->sAgentProfile = pConfig->sAgentProfile ? xwork__strdup(pConfig->sAgentProfile) : NULL;
    pEntry->sInput = pConfig->sInput ? xwork__strdup(pConfig->sInput) : NULL;
    if (!pEntry->sScheduleId || !pEntry->sLabel || (pConfig->sNotify && !pEntry->sNotify) ||
        (pConfig->sOwnerSession && !pEntry->sOwnerSession) ||
        (pConfig->sAgentProfile && !pEntry->sAgentProfile) || (pConfig->sInput && !pEntry->sInput))
        goto oom;
    pEntry->tConfig = *pConfig;
    pEntry->tConfig.uSize = (uint32_t)sizeof(pEntry->tConfig);
    pEntry->tConfig.uAbiVersion = XWORK_ABI_VERSION;
    pEntry->tConfig.sScheduleId = pEntry->sScheduleId;
    pEntry->tConfig.sLabel = pEntry->sLabel;
    pEntry->tConfig.sNotify = pEntry->sNotify;
    pEntry->tConfig.sOwnerSession = pEntry->sOwnerSession;
    pEntry->tConfig.sAgentProfile = pEntry->sAgentProfile;
    pEntry->tConfig.sInput = pEntry->sInput;
    pEntry->tConfig.uInterval = pConfig->uInterval ? pConfig->uInterval : 1u;
    pEntry->tConfig.uMisfireGraceSeconds = pConfig->uMisfireGraceSeconds
                                               ? pConfig->uMisfireGraceSeconds
                                               : XWORK_SCHEDULE_DEFAULT_GRACE_SECONDS;
    pEntry->tConfig.uMaxCatchUp = pConfig->uMaxCatchUp ? pConfig->uMaxCatchUp : 1u;
    pEntry->tConfig.uMaxConcurrentRuns =
        pConfig->uMaxConcurrentRuns ? pConfig->uMaxConcurrentRuns : 1u;
    if (!xworkScheduleNextOccurrence(&pEntry->tConfig, pEntry->tConfig.iStartAtUs - 1,
                                     &pEntry->iNextOccurrenceAtUs, &bHas, pError))
        goto fail;
    if (!bHas)
        pEntry->iNextOccurrenceAtUs = 0;
    pEntry->bRegistered = true;
    return pEntry;
oom:
    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to copy schedule definition");
fail:
    xwork__schedule_entry_unit(pEntry);
    return NULL;
}

static bool xwork__schedule_publish(xwork_runtime *pRuntime, xwork_schedule_entry *pEntry,
                                    xwork_error *pError)
{
    xwork_schedule_entry **ppNext;
    size_t iNewCap;
    if (!xrtMutexLock(pRuntime->pSchedulerLock))
    {
        xwork__schedule_entry_unit(pEntry);
        xwork__set_error(pError, XWORK_ERROR_CONTEXT, "runtime scheduler is unavailable");
        return false;
    }
    if (xwork__schedule_find_locked(pRuntime, pEntry->sScheduleId))
    {
        (void)xrtMutexUnlock(pRuntime->pSchedulerLock);
        xwork__schedule_entry_unit(pEntry);
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
                         "runtime schedule id already exists");
        return false;
    }
    if (pRuntime->iScheduleCount >= pRuntime->uMaxSchedules)
    {
        (void)xrtMutexUnlock(pRuntime->pSchedulerLock);
        xwork__schedule_entry_unit(pEntry);
        xwork__set_error(pError, XWORK_ERROR_LIMIT, "runtime schedule limit was exceeded");
        return false;
    }
    if (pRuntime->iScheduleCount == pRuntime->iScheduleCap)
    {
        iNewCap = pRuntime->iScheduleCap ? pRuntime->iScheduleCap * 2u : 8u;
        if (iNewCap > pRuntime->uMaxSchedules)
            iNewCap = pRuntime->uMaxSchedules;
        ppNext = (xwork_schedule_entry **)realloc(pRuntime->ppSchedules, iNewCap * sizeof(*ppNext));
        if (!ppNext)
        {
            (void)xrtMutexUnlock(pRuntime->pSchedulerLock);
            xwork__schedule_entry_unit(pEntry);
            xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY,
                             "failed to grow runtime scheduler registry");
            return false;
        }
        pRuntime->ppSchedules = ppNext;
        pRuntime->iScheduleCap = iNewCap;
    }
    if (!pEntry->uGeneration)
    {
        if (!pRuntime->uNextScheduleGeneration)
        {
            (void)xrtMutexUnlock(pRuntime->pSchedulerLock);
            xwork__schedule_entry_unit(pEntry);
            xwork__set_error(pError, XWORK_ERROR_LIMIT,
                             "runtime schedule generation space was exhausted");
            return false;
        }
        pEntry->uGeneration = pRuntime->uNextScheduleGeneration++;
    }
    else if (pRuntime->uNextScheduleGeneration <= pEntry->uGeneration)
    {
        pRuntime->uNextScheduleGeneration = pEntry->uGeneration + 1u;
    }
    pRuntime->ppSchedules[pRuntime->iScheduleCount++] = pEntry;
    (void)xrtMutexUnlock(pRuntime->pSchedulerLock);
    return true;
}

bool xworkRuntimeRegisterSchedule(xwork_runtime *pRuntime, const xwork_schedule_config *pConfig,
                                  xwork_error *pError)
{
    xwork_schedule_entry *pEntry;
    xworkErrorInit(pError);
    if (!pRuntime || !xwork__schedule_config_valid(pConfig))
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
                         "invalid runtime schedule configuration");
        return false;
    }
    pEntry = xwork__schedule_copy(pConfig, pError);
    if (!pEntry)
        return false;
    return xwork__schedule_publish(pRuntime, pEntry, pError);
}

static void xwork__schedule_cleanup_active(xwork_runtime *pRuntime, xwork_schedule_entry *pSchedule)
{
    size_t i = 0u;
    while (i < pSchedule->iActiveTaskCount)
    {
        xwork_process_entry *pTask =
            xwork__runtime_task_find(pRuntime, pSchedule->arrActiveTaskIds[i]);
        bool bDone = pTask == NULL;
        if (pTask)
        {
            if (xrtMutexLock(pTask->pStateLock))
            {
                bDone = xwork__schedule_terminal(pTask->eState);
                (void)xrtMutexUnlock(pTask->pStateLock);
            }
            xwork__task_entry_release(pTask);
        }
        if (bDone)
        {
            pSchedule->arrActiveTaskIds[i] =
                pSchedule->arrActiveTaskIds[pSchedule->iActiveTaskCount - 1u];
            --pSchedule->iActiveTaskCount;
        }
        else
        {
            ++i;
        }
    }
}

bool xworkRuntimeReplaceSchedule(xwork_runtime *pRuntime, const xwork_schedule_config *pConfig,
                                 xwork_error *pError)
{
    xwork_schedule_entry *pPrevious;
    xwork_schedule_entry *pCandidate;
    xwork_schedule_entry **ppNext;
    size_t iNewCap;
    xworkErrorInit(pError);
    if (!pRuntime || !xwork__schedule_config_valid(pConfig))
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
                         "invalid runtime schedule replacement");
        return false;
    }
    pCandidate = xwork__schedule_copy(pConfig, pError);
    if (!pCandidate)
        return false;
    if (!xrtMutexLock(pRuntime->pSchedulerLock))
    {
        xwork__schedule_entry_unit(pCandidate);
        xwork__set_error(pError, XWORK_ERROR_CONTEXT, "runtime scheduler is unavailable");
        return false;
    }
    pPrevious = xwork__schedule_find_locked(pRuntime, pCandidate->sScheduleId);
    if (!pPrevious)
    {
        (void)xrtMutexUnlock(pRuntime->pSchedulerLock);
        xwork__schedule_entry_unit(pCandidate);
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "runtime schedule was not found");
        return false;
    }
    xwork__schedule_cleanup_active(pRuntime, pPrevious);
    if (pPrevious->iActiveTaskCount != 0u)
    {
        (void)xrtMutexUnlock(pRuntime->pSchedulerLock);
        xwork__schedule_entry_unit(pCandidate);
        xwork__set_error(pError, XWORK_ERROR_CONTEXT, "runtime schedule has active work");
        return false;
    }
    if (pRuntime->iScheduleCount >= pRuntime->uMaxSchedules)
    {
        (void)xrtMutexUnlock(pRuntime->pSchedulerLock);
        xwork__schedule_entry_unit(pCandidate);
        xwork__set_error(pError, XWORK_ERROR_LIMIT, "runtime schedule lifetime limit was exceeded");
        return false;
    }
    if (pRuntime->iScheduleCount == pRuntime->iScheduleCap)
    {
        iNewCap = pRuntime->iScheduleCap ? pRuntime->iScheduleCap * 2u : 8u;
        if (iNewCap > pRuntime->uMaxSchedules)
            iNewCap = pRuntime->uMaxSchedules;
        ppNext = (xwork_schedule_entry **)realloc(pRuntime->ppSchedules, iNewCap * sizeof(*ppNext));
        if (!ppNext)
        {
            (void)xrtMutexUnlock(pRuntime->pSchedulerLock);
            xwork__schedule_entry_unit(pCandidate);
            xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY,
                             "failed to grow runtime scheduler registry");
            return false;
        }
        pRuntime->ppSchedules = ppNext;
        pRuntime->iScheduleCap = iNewCap;
    }
    if (!pRuntime->uNextScheduleGeneration)
    {
        (void)xrtMutexUnlock(pRuntime->pSchedulerLock);
        xwork__schedule_entry_unit(pCandidate);
        xwork__set_error(pError, XWORK_ERROR_LIMIT,
                         "runtime schedule generation space was exhausted");
        return false;
    }
    pCandidate->uGeneration = pRuntime->uNextScheduleGeneration++;
    pRuntime->ppSchedules[pRuntime->iScheduleCount++] = pCandidate;
    pPrevious->bRegistered = false;
    (void)xrtMutexUnlock(pRuntime->pSchedulerLock);
    return true;
}

bool xworkRuntimeRevertScheduleReplacement(xwork_runtime *pRuntime, const char *sScheduleId,
                                           uint64_t uCurrentGeneration,
                                           uint64_t uPreviousGeneration, xwork_error *pError)
{
    xwork_schedule_entry *pCurrent;
    xwork_schedule_entry *pPrevious = NULL;
    size_t i;
    xworkErrorInit(pError);
    if (!pRuntime || !xwork__schedule_id_valid(sScheduleId) || !uCurrentGeneration ||
        !uPreviousGeneration || uCurrentGeneration == uPreviousGeneration ||
        !xrtMutexLock(pRuntime->pSchedulerLock))
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
                         "invalid runtime schedule replacement rollback");
        return false;
    }
    pCurrent = xwork__schedule_find_locked(pRuntime, sScheduleId);
    for (i = 0u; i < pRuntime->iScheduleCount; ++i)
    {
        xwork_schedule_entry *pEntry = pRuntime->ppSchedules[i];
        if (pEntry && !pEntry->bRegistered && pEntry->uGeneration == uPreviousGeneration &&
            strcmp(pEntry->sScheduleId, sScheduleId) == 0)
        {
            pPrevious = pEntry;
            break;
        }
    }
    if (!pCurrent || pCurrent->uGeneration != uCurrentGeneration || !pPrevious)
    {
        (void)xrtMutexUnlock(pRuntime->pSchedulerLock);
        xwork__set_error(pError, XWORK_ERROR_CONTEXT,
                         "runtime schedule replacement generations changed");
        return false;
    }
    xwork__schedule_cleanup_active(pRuntime, pCurrent);
    if (pCurrent->iActiveTaskCount != 0u || pCurrent->uClaimCount != 0u ||
        pCurrent->uMisfireCount != 0u || pCurrent->iLastClaimedAtUs != 0)
    {
        (void)xrtMutexUnlock(pRuntime->pSchedulerLock);
        xwork__set_error(pError, XWORK_ERROR_CONTEXT,
                         "runtime schedule replacement already advanced");
        return false;
    }
    pCurrent->bRegistered = false;
    pPrevious->bRegistered = true;
    (void)xrtMutexUnlock(pRuntime->pSchedulerLock);
    return true;
}

static void xwork__schedule_info_fill(const xwork_schedule_entry *pEntry,
                                      xwork_schedule_info *pInfo)
{
    uint32_t uSize = pInfo->uSize;
    uint32_t uVersion = pInfo->uAbiVersion;
    memset(pInfo, 0, sizeof(*pInfo));
    pInfo->uSize = uSize;
    pInfo->uAbiVersion = uVersion;
    pInfo->tConfig = pEntry->tConfig;
    pInfo->uGeneration = pEntry->uGeneration;
    pInfo->iNextOccurrenceAtUs = pEntry->iNextOccurrenceAtUs;
    pInfo->iLastClaimedAtUs = pEntry->iLastClaimedAtUs;
    pInfo->uClaimCount = pEntry->uClaimCount;
    pInfo->uMisfireCount = pEntry->uMisfireCount;
    pInfo->iActiveRuns = pEntry->iActiveTaskCount;
}

bool xworkRuntimeScheduleGetInfo(xwork_runtime *pRuntime, const char *sScheduleId,
                                 xwork_schedule_info *pInfo)
{
    xwork_schedule_entry *pEntry;
    if (!pRuntime || !sScheduleId || !pInfo ||
        !xwork__record_valid(pInfo->uSize, pInfo->uAbiVersion, sizeof(*pInfo)) ||
        !xrtMutexLock(pRuntime->pSchedulerLock))
        return false;
    pEntry = xwork__schedule_find_locked(pRuntime, sScheduleId);
    if (pEntry)
    {
        xwork__schedule_cleanup_active(pRuntime, pEntry);
        xwork__schedule_info_fill(pEntry, pInfo);
    }
    (void)xrtMutexUnlock(pRuntime->pSchedulerLock);
    return pEntry != NULL;
}

bool xworkRuntimeScheduleAt(xwork_runtime *pRuntime, size_t iIndex, xwork_schedule_info *pInfo)
{
    size_t i;
    size_t n = 0u;
    xwork_schedule_entry *pFound = NULL;
    if (!pRuntime || !pInfo ||
        !xwork__record_valid(pInfo->uSize, pInfo->uAbiVersion, sizeof(*pInfo)) ||
        !xrtMutexLock(pRuntime->pSchedulerLock))
        return false;
    for (i = 0u; i < pRuntime->iScheduleCount; ++i)
    {
        xwork_schedule_entry *pEntry = pRuntime->ppSchedules[i];
        if (!pEntry || !pEntry->bRegistered)
            continue;
        if (n++ == iIndex)
        {
            pFound = pEntry;
            break;
        }
    }
    if (pFound)
    {
        xwork__schedule_cleanup_active(pRuntime, pFound);
        xwork__schedule_info_fill(pFound, pInfo);
    }
    (void)xrtMutexUnlock(pRuntime->pSchedulerLock);
    return pFound != NULL;
}

size_t xworkRuntimeScheduleCount(xwork_runtime *pRuntime)
{
    size_t i;
    size_t n = 0u;
    if (!pRuntime || !xrtMutexLock(pRuntime->pSchedulerLock))
        return 0u;
    for (i = 0u; i < pRuntime->iScheduleCount; ++i)
    {
        if (pRuntime->ppSchedules[i] && pRuntime->ppSchedules[i]->bRegistered)
            ++n;
    }
    (void)xrtMutexUnlock(pRuntime->pSchedulerLock);
    return n;
}

bool xworkRuntimeSetScheduleEnabled(xwork_runtime *pRuntime, const char *sScheduleId, bool bEnabled,
                                    xwork_error *pError)
{
    xwork_schedule_entry *pEntry;
    xworkErrorInit(pError);
    if (!pRuntime || !sScheduleId || !xrtMutexLock(pRuntime->pSchedulerLock))
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
                         "runtime and schedule id are required");
        return false;
    }
    pEntry = xwork__schedule_find_locked(pRuntime, sScheduleId);
    if (!pEntry)
    {
        (void)xrtMutexUnlock(pRuntime->pSchedulerLock);
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "runtime schedule was not found");
        return false;
    }
    pEntry->tConfig.bEnabled = bEnabled;
    if (!bEnabled)
    {
        pEntry->bCatchingUp = false;
        pEntry->uCatchUpRemaining = 0u;
    }
    (void)xrtMutexUnlock(pRuntime->pSchedulerLock);
    return true;
}

bool xworkRuntimeUnregisterSchedule(xwork_runtime *pRuntime, const char *sScheduleId,
                                    xwork_error *pError)
{
    xwork_schedule_entry *pEntry;
    xworkErrorInit(pError);
    if (!pRuntime || !sScheduleId || !xrtMutexLock(pRuntime->pSchedulerLock))
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
                         "runtime and schedule id are required");
        return false;
    }
    pEntry = xwork__schedule_find_locked(pRuntime, sScheduleId);
    if (!pEntry)
    {
        (void)xrtMutexUnlock(pRuntime->pSchedulerLock);
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "runtime schedule was not found");
        return false;
    }
    pEntry->bRegistered = false;
    pEntry->tConfig.bEnabled = false;
    pEntry->bCatchingUp = false;
    pEntry->uCatchUpRemaining = 0u;
    (void)xrtMutexUnlock(pRuntime->pSchedulerLock);
    return true;
}

bool xworkRuntimeRestoreSchedule(xwork_runtime *pRuntime, const xwork_schedule_info *pInfo,
                                 xwork_error *pError)
{
    xwork_schedule_entry *pEntry;
    int64_t iExpected = 0;
    bool bHas = false;
    xworkErrorInit(pError);
    if (!pRuntime || !pInfo ||
        !xwork__record_valid(pInfo->uSize, pInfo->uAbiVersion, sizeof(*pInfo)) ||
        !xwork__schedule_config_valid(&pInfo->tConfig) || pInfo->uGeneration == 0u ||
        pInfo->uGeneration == UINT64_MAX || pInfo->iNextOccurrenceAtUs < 0 ||
        pInfo->iLastClaimedAtUs < 0 ||
        (pInfo->iNextOccurrenceAtUs > 0 &&
         (!xworkScheduleNextOccurrence(&pInfo->tConfig, pInfo->iNextOccurrenceAtUs - 1, &iExpected,
                                       &bHas, pError) ||
          !bHas || iExpected != pInfo->iNextOccurrenceAtUs)))
    {
        if (!pError || pError->eCode == XWORK_ERROR_NONE)
        {
            xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
                             "invalid restored schedule metadata");
        }
        return false;
    }
    pEntry = xwork__schedule_copy(&pInfo->tConfig, pError);
    if (!pEntry)
        return false;
    pEntry->uGeneration = pInfo->uGeneration;
    pEntry->iNextOccurrenceAtUs = pInfo->iNextOccurrenceAtUs;
    pEntry->iLastClaimedAtUs = pInfo->iLastClaimedAtUs;
    pEntry->uClaimCount = pInfo->uClaimCount;
    pEntry->uMisfireCount = pInfo->uMisfireCount;
    return xwork__schedule_publish(pRuntime, pEntry, pError);
}

static bool xwork__schedule_next_after(const xwork_schedule_entry *pEntry, int64_t iAfterUs,
                                       int64_t *piNextUs, xwork_error *pError)
{
    bool bHas = false;
    if (!xworkScheduleNextOccurrence(&pEntry->tConfig, iAfterUs, piNextUs, &bHas, pError))
        return false;
    if (!bHas)
        *piNextUs = 0;
    return true;
}

static bool xwork__schedule_latest_due(const xwork_schedule_entry *pEntry, int64_t iNowUs,
                                       int64_t *piLatestUs, xwork_error *pError)
{
    int64_t iWindow;
    int64_t iAfter;
    int64_t iCurrent = 0;
    int64_t iLatest = 0;
    bool bHas = false;
    uint32_t i;
    uint32_t uInterval = pEntry->tConfig.uInterval;
    if (pEntry->tConfig.eFrequency == XWORK_SCHEDULE_ONCE)
    {
        *piLatestUs = pEntry->tConfig.iStartAtUs;
        return true;
    }
    if (pEntry->tConfig.eFrequency == XWORK_SCHEDULE_MINUTELY)
    {
        iWindow = (int64_t)uInterval * INT64_C(60) * INT64_C(1000000);
    }
    else if (pEntry->tConfig.eFrequency == XWORK_SCHEDULE_HOURLY)
    {
        iWindow = (int64_t)uInterval * INT64_C(3600) * INT64_C(1000000);
    }
    else if (pEntry->tConfig.eFrequency == XWORK_SCHEDULE_DAILY)
    {
        iWindow = ((int64_t)uInterval + 2) * XRT_TIME_DAY;
    }
    else
    {
        iWindow = ((int64_t)uInterval * 7 + 9) * XRT_TIME_DAY;
    }
    iAfter = iNowUs > iWindow ? iNowUs - iWindow : -1;
    for (i = 0u; i < 32u; ++i)
    {
        if (!xworkScheduleNextOccurrence(&pEntry->tConfig, iAfter, &iCurrent, &bHas, pError))
            return false;
        if (!bHas || iCurrent > iNowUs)
            break;
        iLatest = iCurrent;
        iAfter = iCurrent;
    }
    if (!iLatest)
        iLatest = pEntry->iNextOccurrenceAtUs;
    *piLatestUs = iLatest;
    return true;
}

static int64_t xwork__scheduler_next_wake_locked(xwork_runtime *pRuntime, int64_t iNowUs)
{
    size_t i;
    int64_t iNext = 0;
    for (i = 0u; i < pRuntime->iScheduleCount; ++i)
    {
        xwork_schedule_entry *pEntry = pRuntime->ppSchedules[i];
        if (!pEntry || !pEntry->bRegistered || !pEntry->tConfig.bEnabled ||
            !pEntry->iNextOccurrenceAtUs)
            continue;
        xwork__schedule_cleanup_active(pRuntime, pEntry);
        if (pEntry->iActiveTaskCount >= pEntry->tConfig.uMaxConcurrentRuns &&
            pEntry->tConfig.eOverlapPolicy == XWORK_SCHEDULE_OVERLAP_QUEUE_ONE &&
            pEntry->iNextOccurrenceAtUs <= iNowUs)
            continue;
        if (!iNext || pEntry->iNextOccurrenceAtUs < iNext)
            iNext = pEntry->iNextOccurrenceAtUs;
    }
    return iNext;
}

/* The scheduler lock serializes the active-run limit and task creation. Keep
 * manual and time-based starts on the same task and lineage path. */
static bool xwork__schedule_start_task_locked(xwork_runtime *pRuntime, xwork_schedule_entry *pEntry,
                                              int64_t iOccurrenceAtUs, xwork_schedule_claim *pClaim,
                                              xwork_error *pError)
{
    xwork_scheduled_task_config tTask;
    xwork_error tCleanupError;
    uint64_t uTaskId = 0u;
    xworkScheduledTaskConfigInit(&tTask);
    tTask.uOwnerAgentId = pEntry->tConfig.uOwnerAgentId;
    tTask.uOwnerRunId = pEntry->tConfig.uOwnerRunId;
    tTask.uParentTaskId = pEntry->tConfig.uParentTaskId;
    tTask.sOwnerSession = pEntry->tConfig.sOwnerSession;
    tTask.sLabel = pEntry->tConfig.sLabel;
    tTask.sNotify = pEntry->tConfig.sNotify;
    tTask.iScheduledAtUs = iOccurrenceAtUs;
    tTask.sScheduleId = pEntry->sScheduleId;
    tTask.uScheduleGeneration = pEntry->uGeneration;
    if (!xworkRuntimeCreateScheduledTask(pRuntime, &tTask, &uTaskId, pError))
        return false;
    if (!xworkRuntimeStartScheduledTask(pRuntime, uTaskId, pError))
    {
        xworkErrorInit(&tCleanupError);
        (void)xworkRuntimeCancelTask(pRuntime, uTaskId, &tCleanupError);
        (void)xworkRuntimeReleaseTask(pRuntime, uTaskId, &tCleanupError);
        return false;
    }
    pEntry->arrActiveTaskIds[pEntry->iActiveTaskCount++] = uTaskId;
    pClaim->uTaskId = uTaskId;
    pClaim->uScheduleGeneration = pEntry->uGeneration;
    pClaim->iOccurrenceAtUs = iOccurrenceAtUs;
    pClaim->sScheduleId = pEntry->sScheduleId;
    pClaim->sLabel = pEntry->sLabel;
    pClaim->sAgentProfile = pEntry->sAgentProfile;
    pClaim->sInput = pEntry->sInput;
    return true;
}

bool xworkRuntimeTriggerSchedule(xwork_runtime *pRuntime, const char *sScheduleId, int64_t iNowUs,
                                 xwork_schedule_claim *pClaim, xwork_error *pError)
{
    xwork_schedule_entry *pEntry;
    uint32_t uClaimSize = pClaim ? pClaim->uSize : 0u;
    uint32_t uClaimVersion = pClaim ? pClaim->uAbiVersion : 0u;
    xworkErrorInit(pError);
    if (!pRuntime || !xwork__schedule_id_valid(sScheduleId) || iNowUs <= 0 || !pClaim ||
        !xwork__record_valid(uClaimSize, uClaimVersion, sizeof(*pClaim)))
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "invalid explicit schedule trigger");
        return false;
    }
    if (!xrtMutexLock(pRuntime->pSchedulerLock))
    {
        xwork__set_error(pError, XWORK_ERROR_CONTEXT, "runtime scheduler is unavailable");
        return false;
    }
    memset(pClaim, 0, sizeof(*pClaim));
    pClaim->uSize = uClaimSize;
    pClaim->uAbiVersion = uClaimVersion;
    pEntry = xwork__schedule_find_locked(pRuntime, sScheduleId);
    if (!pEntry)
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "runtime schedule was not found");
        goto fail_locked;
    }
    xwork__schedule_cleanup_active(pRuntime, pEntry);
    if (pEntry->iActiveTaskCount >= pEntry->tConfig.uMaxConcurrentRuns)
    {
        xwork__set_error(pError, XWORK_ERROR_CONTEXT,
                         "runtime schedule has reached its concurrent-run limit");
        goto fail_locked;
    }
    if (!xwork__schedule_start_task_locked(pRuntime, pEntry, iNowUs, pClaim, pError))
        goto fail_locked;
    pEntry->iLastClaimedAtUs = iNowUs;
    if (pEntry->uClaimCount != UINT64_MAX)
        ++pEntry->uClaimCount;
    (void)xrtMutexUnlock(pRuntime->pSchedulerLock);
    return true;
fail_locked:
    (void)xrtMutexUnlock(pRuntime->pSchedulerLock);
    return false;
}

bool xworkRuntimeClaimDueSchedule(xwork_runtime *pRuntime, int64_t iNowUs,
                                  xwork_schedule_claim *pClaim, bool *pbClaimed,
                                  int64_t *piNextWakeAtUs, xwork_error *pError)
{
    uint32_t uGuard = 0u;
    uint32_t uClaimSize = pClaim ? pClaim->uSize : 0u;
    uint32_t uClaimVersion = pClaim ? pClaim->uAbiVersion : 0u;
    bool bClaimValid = pClaim && xwork__record_valid(uClaimSize, uClaimVersion, sizeof(*pClaim));
    xworkErrorInit(pError);
    if (pbClaimed)
        *pbClaimed = false;
    if (piNextWakeAtUs)
        *piNextWakeAtUs = 0;
    if (!pRuntime || iNowUs <= 0 || !bClaimValid || !pbClaimed ||
        !xrtMutexLock(pRuntime->pSchedulerLock))
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "invalid scheduler claim request");
        return false;
    }
    memset(pClaim, 0, sizeof(*pClaim));
    pClaim->uSize = uClaimSize;
    pClaim->uAbiVersion = uClaimVersion;
    while (++uGuard <= pRuntime->uMaxSchedules + 1u)
    {
        xwork_schedule_entry *pDue = NULL;
        size_t i;
        int64_t iOccurrence;
        int64_t iNext;
        bool bNewCatchingUp;
        uint32_t uNewCatchUpRemaining;
        bool bMisfireEpisode = false;
        for (i = 0u; i < pRuntime->iScheduleCount; ++i)
        {
            xwork_schedule_entry *pEntry = pRuntime->ppSchedules[i];
            if (!pEntry || !pEntry->bRegistered || !pEntry->tConfig.bEnabled ||
                !pEntry->iNextOccurrenceAtUs || pEntry->iNextOccurrenceAtUs > iNowUs)
                continue;
            xwork__schedule_cleanup_active(pRuntime, pEntry);
            if (pEntry->iActiveTaskCount >= pEntry->tConfig.uMaxConcurrentRuns &&
                pEntry->tConfig.eOverlapPolicy == XWORK_SCHEDULE_OVERLAP_QUEUE_ONE)
                continue;
            if (!pDue || pEntry->iNextOccurrenceAtUs < pDue->iNextOccurrenceAtUs ||
                (pEntry->iNextOccurrenceAtUs == pDue->iNextOccurrenceAtUs &&
                 strcmp(pEntry->sScheduleId, pDue->sScheduleId) < 0))
            {
                pDue = pEntry;
            }
        }
        if (!pDue)
        {
            if (piNextWakeAtUs)
                *piNextWakeAtUs = xwork__scheduler_next_wake_locked(pRuntime, iNowUs);
            (void)xrtMutexUnlock(pRuntime->pSchedulerLock);
            return true;
        }
        if (pDue->iActiveTaskCount >= pDue->tConfig.uMaxConcurrentRuns)
        {
            if (!xwork__schedule_next_after(pDue, iNowUs, &pDue->iNextOccurrenceAtUs, pError))
                goto fail_locked;
            pDue->bCatchingUp = false;
            pDue->uCatchUpRemaining = 0u;
            if (pDue->uMisfireCount != UINT64_MAX)
                ++pDue->uMisfireCount;
            continue;
        }
        iOccurrence = pDue->iNextOccurrenceAtUs;
        iNext = 0;
        bNewCatchingUp = pDue->bCatchingUp;
        uNewCatchUpRemaining = pDue->uCatchUpRemaining;
        if ((uint64_t)(iNowUs - iOccurrence) >
            (uint64_t)pDue->tConfig.uMisfireGraceSeconds * UINT64_C(1000000))
        {
            if (pDue->tConfig.eMisfirePolicy == XWORK_SCHEDULE_MISFIRE_SKIP)
            {
                if (!xwork__schedule_next_after(pDue, iNowUs, &pDue->iNextOccurrenceAtUs, pError))
                    goto fail_locked;
                pDue->bCatchingUp = false;
                pDue->uCatchUpRemaining = 0u;
                if (pDue->uMisfireCount != UINT64_MAX)
                    ++pDue->uMisfireCount;
                continue;
            }
            if (pDue->tConfig.eMisfirePolicy == XWORK_SCHEDULE_MISFIRE_RUN_ONCE)
            {
                if (!xwork__schedule_latest_due(pDue, iNowUs, &iOccurrence, pError) ||
                    !xwork__schedule_next_after(pDue, iNowUs, &iNext, pError))
                    goto fail_locked;
                bNewCatchingUp = false;
                uNewCatchUpRemaining = 0u;
                bMisfireEpisode = true;
            }
            else
            {
                if (!bNewCatchingUp)
                {
                    bNewCatchingUp = true;
                    uNewCatchUpRemaining = pDue->tConfig.uMaxCatchUp;
                    bMisfireEpisode = true;
                }
                if (!uNewCatchUpRemaining)
                {
                    if (!xwork__schedule_next_after(pDue, iNowUs, &pDue->iNextOccurrenceAtUs,
                                                    pError))
                        goto fail_locked;
                    pDue->bCatchingUp = false;
                    continue;
                }
                --uNewCatchUpRemaining;
                if (!xwork__schedule_next_after(pDue, iOccurrence, &iNext, pError))
                    goto fail_locked;
                if (!iNext || iNext > iNowUs)
                {
                    bNewCatchingUp = false;
                    uNewCatchUpRemaining = 0u;
                }
                else if (!uNewCatchUpRemaining)
                {
                    if (!xwork__schedule_next_after(pDue, iNowUs, &iNext, pError))
                        goto fail_locked;
                    bNewCatchingUp = false;
                }
            }
        }
        else if (!xwork__schedule_next_after(pDue, iOccurrence, &iNext, pError))
        {
            goto fail_locked;
        }

        if (!xwork__schedule_start_task_locked(pRuntime, pDue, iOccurrence, pClaim, pError))
            goto fail_locked;
        pDue->iNextOccurrenceAtUs = iNext;
        pDue->iLastClaimedAtUs = iOccurrence;
        pDue->bCatchingUp = bNewCatchingUp;
        pDue->uCatchUpRemaining = uNewCatchUpRemaining;
        if (pDue->uClaimCount != UINT64_MAX)
            ++pDue->uClaimCount;
        if (bMisfireEpisode && pDue->uMisfireCount != UINT64_MAX)
            ++pDue->uMisfireCount;
        *pbClaimed = true;
        if (piNextWakeAtUs)
            *piNextWakeAtUs = xwork__scheduler_next_wake_locked(pRuntime, iNowUs);
        (void)xrtMutexUnlock(pRuntime->pSchedulerLock);
        return true;
    }
    xwork__set_error(pError, XWORK_ERROR_LIMIT,
                     "scheduler could not settle bounded skipped occurrences");
fail_locked:
    (void)xrtMutexUnlock(pRuntime->pSchedulerLock);
    return false;
}

void xwork__runtime_scheduler_unit(xwork_runtime *pRuntime)
{
    size_t i;
    if (!pRuntime)
        return;
    for (i = 0u; i < pRuntime->iScheduleCount; ++i)
        xwork__schedule_entry_unit(pRuntime->ppSchedules[i]);
    free(pRuntime->ppSchedules);
    pRuntime->ppSchedules = NULL;
    pRuntime->iScheduleCount = 0u;
    pRuntime->iScheduleCap = 0u;
}
