#include "../internal/xllm_session_internal.h"

static uint64_t xllm_session__govern_sat_add(uint64_t a, uint64_t b)
{
    return a > UINT64_MAX - b ? UINT64_MAX : a + b;
}

static uint64_t xllm_session__govern_ratio(uint64_t uValue, double fRatio)
{
    long double fProduct;
    if (fRatio <= 0.0)
    {
        return 0u;
    }
    if (fRatio >= 1.0)
    {
        return uValue;
    }
    fProduct = (long double)uValue * (long double)fRatio;
    return fProduct >= (long double)UINT64_MAX ? UINT64_MAX : (uint64_t)fProduct;
}

/* ------------------------------------------------------------------ */
/* Exact-feedback governance (design §4)                                */
/* ------------------------------------------------------------------ */

/* Keep the server's prompt+output as the anchor. Only newly appended entries
 * and positive changes to its retained prefix are estimates. Tool byte caps
 * are storage limits, not the growth of an actual request. */
uint64_t xllm_session__usage_growth(const xllm_session *pSession)
{
    uint64_t uPrefix =
        pSession->sSummary
            ? xllm_session__govern_sat_add(24u, xllmEstimateTextTokens(pSession->sSummary))
            : 0u;
    uint64_t uGrowth = 0u;
    size_t i;
    for (i = 0u; i < pSession->iEntryCount; ++i)
    {
        const xllm_session_entry *pEntry = &pSession->pEntries[i];
        if (!xllm_session__entry_is_active(pSession, pEntry))
            continue;
        if (pEntry->uSequence > pSession->uFillSequence &&
            !(pEntry->uFlags & XLLM_SESSION_ENTRY_PINNED))
            uGrowth = xllm_session__govern_sat_add(uGrowth, pEntry->uEstimatedTokens);
        else
            uPrefix = xllm_session__govern_sat_add(uPrefix, pEntry->uEstimatedTokens);
    }
    if (uPrefix > pSession->uFillEstimate)
        uGrowth = xllm_session__govern_sat_add(uGrowth, uPrefix - pSession->uFillEstimate);
    return uGrowth;
}

uint64_t xllm_session__profile_fingerprint(const xllm_model_profile *pProfile)
{
    uint64_t uHash = 1469598103934665603ull;
    const char *psText[2] = {pProfile->sId, pProfile->sModel};
    size_t i;
    for (i = 0u; i < 2u; ++i)
    {
        const unsigned char *p = (const unsigned char *)(psText[i] ? psText[i] : "");
        while (*p)
        {
            uHash = (uHash ^ *p++) * 1099511628211ull;
        }
        uHash = (uHash ^ 0xffu) * 1099511628211ull;
    }
    uHash = (uHash ^ (uint64_t)pProfile->eProvider) * 1099511628211ull;
    /* Older Anthropic checkpoints excluded cached input. A tokenizer/profile
     * match must not bless that undercount as exact after the usage contract
     * changes. Invalidate only that dialect's legacy anchor on profile apply. */
    if (pProfile->eProvider == XLLM_PROVIDER_ANTHROPIC)
        uHash = (uHash ^ 0x63616368652d7631ull) * 1099511628211ull;
    return (uHash & INT64_MAX) | 1u; /* persistence's nonnegative integer range */
}

bool xllm_session__restore_usage(xllm_session *pSession, xvalue *pUsage)
{
    uint64_t uFill, uEstimate, uSequence, uCached, uProfile;
    if (!pUsage)
        return true; /* legacy snapshot */
    if (!xrtValueIs(pUsage, XVALUE_OBJECT))
        return false;
    uFill = xllm_session__json_u64(pUsage, "fill_tokens", UINT64_MAX);
    uEstimate = xllm_session__json_u64(pUsage, "prefix_estimate", UINT64_MAX);
    uSequence = xllm_session__json_u64(pUsage, "through_sequence", UINT64_MAX);
    uCached = xllm_session__json_u64(pUsage, "cached_input_tokens", UINT64_MAX);
    uProfile = xllm_session__json_u64(pUsage, "profile_fingerprint", UINT64_MAX);
    if (uFill == UINT64_MAX || !uFill || uEstimate == UINT64_MAX ||
        uSequence >= pSession->uNextSequence || uCached == UINT64_MAX || uProfile == UINT64_MAX)
        return false;
    pSession->bFillSeen = true;
    pSession->bFillExactValid = true;
    pSession->uFillExact = uFill;
    pSession->uFillEstimate = uEstimate;
    pSession->uFillSequence = uSequence;
    pSession->uCachedInputTokens = uCached;
    pSession->uProfileFingerprint = uProfile;
    pSession->bStatsDirty = true;
    return true;
}

xllm_session_pressure xllm_session__pressure_exact(const xllm_session *pSession)
{
    uint64_t uBudget;
    uint64_t uSoft;
    uint64_t uPrune;
    uint64_t uLoad;
    if (!pSession || !pSession->bFillExactValid)
    {
        return XLLM_SESSION_PRESSURE_NONE;
    }
    uBudget = xllm_session__input_budget(pSession);
    uSoft = xllm_session__govern_ratio(uBudget, pSession->tConfig.fCompactTrigger);
    uPrune = xllm_session__govern_ratio(uBudget, pSession->tConfig.fPruneTrigger);
    /* Next-call input approximates this call's prompt+output plus the bounded
     * increment, so the fill used here is prompt+output, not prompt alone. */
    uLoad =
        xllm_session__govern_sat_add(pSession->uFillExact, xllm_session__usage_growth(pSession));
    if (uLoad > uBudget)
    {
        return XLLM_SESSION_PRESSURE_OVERFLOW;
    }
    if (uLoad >= uSoft)
    {
        return XLLM_SESSION_PRESSURE_COMPACT;
    }
    if (uLoad >= uPrune)
    {
        return XLLM_SESSION_PRESSURE_PRUNE;
    }
    return XLLM_SESSION_PRESSURE_NONE;
}

bool xllm_session__record_usage(xllm_session *pSession, const xllm_usage *pUsage)
{
    xllm_session_stats tStats;
    uint64_t uFill, uSequence;
    if (!pSession || !pUsage)
    {
        return false;
    }
    if (pUsage->uInputTokens == 0u && pUsage->uOutputTokens == 0u)
    {
        return true; /* no feedback channel (offline fixtures); keep current state */
    }
    if (!xllmSessionGetStats(pSession, &tStats))
        return false;
    uFill = xllm_session__govern_sat_add(pUsage->uInputTokens, pUsage->uOutputTokens);
    uSequence = xllmSessionLastSequence(pSession);
    if (!xllm_session__journal_append_usage(pSession, uFill, tStats.uRawActiveTokens, uSequence,
                                            pUsage->uCachedInputTokens))
        return false;
    pSession->bFillSeen = true;
    pSession->bFillExactValid = true;
    pSession->uFillExact = uFill;
    pSession->uFillSequence = uSequence;
    pSession->uFillEstimate = tStats.uRawActiveTokens;
    pSession->uCachedInputTokens = pUsage->uCachedInputTokens;
    pSession->uIncrementMax = 0u;
    pSession->bStatsDirty = true;
    xllm_session__event(pSession, XLLM_SESSION_EVENT_FILL_UPDATED, 0u, pSession->uFillExact, NULL);
    xllm_session__pressure_event(pSession);
    return true;
}

void xllm_session__invalidate_fill(xllm_session *pSession)
{
    if (!pSession || pSession->bFillExactValid == false)
    {
        return;
    }
    pSession->bFillExactValid = false;
    pSession->bStatsDirty = true;
    xllm_session__pressure_event(pSession);
}

bool xllmSessionRecordUsage(xllm_session *pSession, const xllm_usage *pUsage)
{
    if (!pSession || !pUsage || pSession->bInHook)
    {
        return false;
    }
    return xllm_session__record_usage(pSession, pUsage);
}

/* ------------------------------------------------------------------ */
/* Event stream (design §9.4)                                           */
/* ------------------------------------------------------------------ */

void xllm_session__event(xllm_session *pSession, xllm_session_event_type eType, uint64_t uSeqFrom,
                         uint64_t uSeqTo, const char *sText)
{
    xllm_session_event tEvent;
    xllm_session_stats tStats;
    if (!pSession || !pSession->pHooks || !pSession->pHooks->pOnEvent)
    {
        return;
    }
    memset(&tEvent, 0, sizeof(tEvent));
    tEvent.eType = eType;
    tEvent.uTurn = pSession->uCurrentTurn;
    tEvent.uSeqFrom = uSeqFrom;
    tEvent.uSeqTo = uSeqTo;
    tEvent.sText = sText;
    tEvent.pStats = xllmSessionGetStats(pSession, &tStats) ? &tStats : NULL;
    if (xllm_session__hook_enter(pSession, NULL, "event"))
    {
        pSession->pHooks->pOnEvent(pSession, &tEvent, pSession->pHooks->pUserData);
        xllm_session__hook_leave(pSession);
    }
}

void xllm_session__pressure_event(xllm_session *pSession)
{
    xllm_session_stats tStats;
    xllm_session_pressure ePressure;
    if (!pSession)
    {
        return;
    }
    if (!xllmSessionGetStats(pSession, &tStats))
    {
        return;
    }
    ePressure = tStats.ePressure;
    if (ePressure == pSession->eLastPressure)
    {
        return;
    }
    pSession->eLastPressure = ePressure;
    xllm_session__event(pSession, XLLM_SESSION_EVENT_PRESSURE_CHANGED, (uint64_t)ePressure, 0u,
                        NULL);
}

/* ------------------------------------------------------------------ */
/* Hook re-entrancy guard (design §9.1)                                 */
/* ------------------------------------------------------------------ */

bool xllm_session__hook_enter(xllm_session *pSession, xllm_error *pError, const char *sStage)
{
    char sMessage[96];
    if (pError)
    {
        xllmErrorInit(pError);
    }
    if (!pSession)
    {
        return false;
    }
    if (pSession->bInHook)
    {
        (void)snprintf(sMessage, sizeof(sMessage),
                       "session hook re-entered a mutating API at stage '%s'",
                       sStage ? sStage : "?");
        xllm_session__error(pError, XLLM_ERROR_HOOK, sMessage);
        return false;
    }
    pSession->bInHook = true;
    return true;
}

void xllm_session__hook_leave(xllm_session *pSession)
{
    if (pSession)
    {
        pSession->bInHook = false;
    }
}
