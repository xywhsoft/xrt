#include "xllm_session_internal.h"

/* ------------------------------------------------------------------ */
/* Exact-feedback governance (design §4)                                */
/* ------------------------------------------------------------------ */

/* Worst-case next-turn growth envelope, in tokens. Byte caps fold to a
 * token upper bound (bytes/4); this is a pessimistic envelope, not an
 * estimate feeding a decision. */
static uint64_t xllm_session__compute_increment(const xllm_session* pSession)
{
    uint64_t uIncrement = 256u; /* structural overhead: roles, JSON wrapping */
    if ( pSession->tConfig.uUserMessageCapBytes ) {
        uIncrement += (uint64_t)pSession->tConfig.uUserMessageCapBytes / 4u;
    }
    if ( pSession->tConfig.uToolResultTotalCapBytes ) {
        uIncrement += (uint64_t)pSession->tConfig.uToolResultTotalCapBytes / 4u;
    }
    return uIncrement;
}

xllm_session_pressure xllm_session__pressure_exact(const xllm_session* pSession)
{
    uint64_t uBudget;
    uint64_t uSoft;
    uint64_t uPrune;
    uint64_t uLoad;
    if ( !pSession || !pSession->bFillExactValid ) { return XLLM_SESSION_PRESSURE_NONE; }
    uBudget = xllm_session__input_budget(pSession);
    uSoft = (uint64_t)((double)uBudget * pSession->tConfig.fCompactTrigger);
    uPrune = (uint64_t)((double)uBudget * pSession->tConfig.fPruneTrigger);
    /* Next-call input approximates this call's prompt+output plus the bounded
     * increment, so the fill used here is prompt+output, not prompt alone. */
    uLoad = pSession->uFillExact + pSession->uIncrementMax;
    if ( uLoad > uBudget ) { return XLLM_SESSION_PRESSURE_OVERFLOW; }
    if ( uLoad >= uSoft ) { return XLLM_SESSION_PRESSURE_COMPACT; }
    if ( uLoad >= uPrune ) { return XLLM_SESSION_PRESSURE_PRUNE; }
    return XLLM_SESSION_PRESSURE_NONE;
}

void xllm_session__record_usage(xllm_session* pSession, const xllm_usage* pUsage)
{
    if ( !pSession || !pUsage ) { return; }
    if ( pUsage->uInputTokens == 0u && pUsage->uOutputTokens == 0u && pUsage->uTotalTokens == 0u ) {
        return; /* no feedback channel (offline fixtures); keep current state */
    }
    pSession->bFillSeen = true;
    pSession->bFillExactValid = true;
    pSession->uFillExact = pUsage->uInputTokens + pUsage->uOutputTokens;
    pSession->uCachedInputTokens = pUsage->uCachedInputTokens;
    pSession->uIncrementMax = xllm_session__compute_increment(pSession);
    xllm_session__event(pSession, XLLM_SESSION_EVENT_FILL_UPDATED, 0u, pSession->uFillExact, NULL);
    xllm_session__pressure_event(pSession);
}

void xllm_session__invalidate_fill(xllm_session* pSession)
{
    if ( !pSession || pSession->bFillExactValid == false ) { return; }
    pSession->bFillExactValid = false;
    xllm_session__pressure_event(pSession);
}

bool xllmSessionRecordUsage(xllm_session* pSession, const xllm_usage* pUsage)
{
    if ( !pSession || !pUsage ) { return false; }
    xllm_session__record_usage(pSession, pUsage);
    return true;
}

/* ------------------------------------------------------------------ */
/* Event stream (design §9.4)                                           */
/* ------------------------------------------------------------------ */

void xllm_session__event(xllm_session* pSession, xllm_session_event_type eType,
    uint64_t uSeqFrom, uint64_t uSeqTo, const char* sText)
{
    xllm_session_event tEvent;
    xllm_session_stats tStats;
    if ( !pSession || !pSession->pHooks || !pSession->pHooks->pOnEvent ) { return; }
    memset(&tEvent, 0, sizeof(tEvent));
    tEvent.eType = eType;
    tEvent.uTurn = pSession->uCurrentTurn;
    tEvent.uSeqFrom = uSeqFrom;
    tEvent.uSeqTo = uSeqTo;
    tEvent.sText = sText;
    tEvent.pStats = xllmSessionGetStats(pSession, &tStats) ? &tStats : NULL;
    if ( xllm_session__hook_enter(pSession, NULL, "event") ) {
        pSession->pHooks->pOnEvent(pSession, &tEvent, pSession->pHooks->pUserData);
        xllm_session__hook_leave(pSession);
    }
}

void xllm_session__pressure_event(xllm_session* pSession)
{
    xllm_session_stats tStats;
    xllm_session_pressure ePressure;
    if ( !pSession ) { return; }
    if ( !xllmSessionGetStats(pSession, &tStats) ) { return; }
    ePressure = tStats.ePressure;
    if ( ePressure == pSession->eLastPressure ) { return; }
    pSession->eLastPressure = ePressure;
    xllm_session__event(pSession, XLLM_SESSION_EVENT_PRESSURE_CHANGED,
        (uint64_t)ePressure, 0u, NULL);
}

/* ------------------------------------------------------------------ */
/* Hook re-entrancy guard (design §9.1)                                 */
/* ------------------------------------------------------------------ */

bool xllm_session__hook_enter(xllm_session* pSession, xllm_error* pError, const char* sStage)
{
    char sMessage[96];
    if ( pError ) { xllmErrorInit(pError); }
    if ( !pSession ) { return false; }
    if ( pSession->bInHook ) {
        (void)snprintf(sMessage, sizeof(sMessage),
            "session hook re-entered a mutating API at stage '%s'", sStage ? sStage : "?");
        xllm_session__error(pError, XLLM_ERROR_HOOK, sMessage);
        return false;
    }
    pSession->bInHook = true;
    return true;
}

void xllm_session__hook_leave(xllm_session* pSession)
{
    if ( pSession ) { pSession->bInHook = false; }
}
