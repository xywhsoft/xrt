#include "xllm_session_internal.h"

/* ------------------------------------------------------------------ */
/* Summary styles: one table drives instruction text and the evaluator  */
/* ------------------------------------------------------------------ */

static const xllm_session_section xllm_session__sections_coding[] = {
    { XLLM_COMPACTION_SECTION_GOAL, "Goal" },
    { XLLM_COMPACTION_SECTION_PREFERENCES, "Constraints & Preferences" },
    { XLLM_COMPACTION_SECTION_PROGRESS, "Progress" },
    { XLLM_COMPACTION_SECTION_KEY_DECISIONS, "Key Decisions" },
    { XLLM_COMPACTION_SECTION_NEXT_STEPS, "Next Steps" },
    { XLLM_COMPACTION_SECTION_CRITICAL_CONTEXT, "Critical Context" }
};

static const xllm_session_section xllm_session__sections_general[] = {
    { XLLM_COMPACTION_SECTION_GOAL, "Goal" },
    { XLLM_COMPACTION_SECTION_PREFERENCES, "Constraints" },
    { XLLM_COMPACTION_SECTION_PROGRESS, "Progress" },
    { XLLM_COMPACTION_SECTION_KEY_DECISIONS, "Key Decisions" },
    { XLLM_COMPACTION_SECTION_NEXT_STEPS, "Next Steps" },
    { XLLM_COMPACTION_SECTION_CRITICAL_CONTEXT, "Critical Context" }
};

static const xllm_session_section xllm_session__sections_durable[] = {
    { XLLM_COMPACTION_SECTION_OBJECTIVE, "Objective" },
    { XLLM_COMPACTION_SECTION_CONSTRAINTS, "Constraints" },
    { XLLM_COMPACTION_SECTION_ARCHITECTURE, "Architecture and decisions" },
    { XLLM_COMPACTION_SECTION_COMPLETED, "Completed work" },
    { XLLM_COMPACTION_SECTION_REPOSITORY_STATE, "Current repository state" },
    { XLLM_COMPACTION_SECTION_VERIFICATION, "Verification evidence" },
    { XLLM_COMPACTION_SECTION_OPEN_ISSUES, "Open issues and risks" },
    { XLLM_COMPACTION_SECTION_NEXT_ACTIONS, "Exact next actions" }
};

static const xllm_session_style xllm_session__style_coding = {
    "coding",
    "You are compacting the durable state of a long-running coding session.\n"
    "Produce a dense, factual continuation summary. Preserve the goal, constraints and preferences, progress (done / in progress / blocked), key decisions, exact next steps, and critical context including files read or modified and tool-call outcomes, not conversational filler. Do not claim unfinished work is complete.\n\n"
    "Use these Markdown headings:\n## Goal\n## Constraints & Preferences\n## Progress\n## Key Decisions\n## Next Steps\n## Critical Context\n\n",
    xllm_session__sections_coding, 6u
};

static const xllm_session_style xllm_session__style_general = {
    "general",
    "You are compacting the durable state of a long-running conversation.\n"
    "Produce a dense, factual continuation summary covering the goal, user preferences and constraints, established facts, topics in progress, key decisions, and concrete follow-ups. Do not invent facts and do not claim unfinished work is complete.\n\n"
    "Use these Markdown headings:\n## Goal\n## Constraints\n## Progress\n## Key Decisions\n## Next Steps\n## Critical Context\n\n",
    xllm_session__sections_general, 6u
};

static const xllm_session_style xllm_session__style_durable = {
    "durable",
    "You are compacting the durable state of a long-running code-agent session.\n"
    "Produce a dense, factual continuation summary. Preserve the objective, constraints, architecture decisions, files changed, commands and test evidence, unresolved failures, active hypotheses, exact next steps, and every identifier or path needed to continue. Preserve tool-call outcomes, not conversational filler. Do not claim unfinished work is complete.\n\n"
    "Use these headings: Objective; Constraints; Architecture and decisions; Completed work; Current repository state; Verification evidence; Open issues and risks; Exact next actions.\n\n",
    xllm_session__sections_durable, 8u
};

const xllm_session_style* xllm_session__style(const xllm_session* pSession)
{
    const char* sStyle;
    if ( !pSession ) { return &xllm_session__style_coding; }
    sStyle = pSession->tConfig.sSummaryStyle;
    if ( sStyle == NULL || sStyle[0] == '\0' || strcmp(sStyle, "coding") == 0 ) {
        return &xllm_session__style_coding;
    }
    if ( strcmp(sStyle, "general") == 0 ) { return &xllm_session__style_general; }
    if ( strcmp(sStyle, "durable") == 0 ) { return &xllm_session__style_durable; }
    return &xllm_session__style_coding;
}

/* ------------------------------------------------------------------ */
/* Turn safety and pair completeness                                    */
/* ------------------------------------------------------------------ */

static bool xllm_session__tool_resolved_range(const xllm_session* pSession, size_t iAssistantEntry,
    const char* sCallId)
{
    size_t i;
    uint64_t uTurn = pSession->pEntries[iAssistantEntry].uTurn;
    for ( i = iAssistantEntry + 1u; i < pSession->iEntryCount; ++i ) {
        const xllm_session_entry* pEntry = &pSession->pEntries[i];
        if ( pEntry->uTurn != uTurn ) {
            if ( pEntry->uTurn > uTurn ) { break; }
            continue;
        }
        if ( pEntry->tMessage.eRole == XLLM_ROLE_TOOL && pEntry->tMessage.sToolCallId &&
             strcmp(pEntry->tMessage.sToolCallId, sCallId) == 0 ) return true;
    }
    return false;
}

bool xllm_session__turn_is_safe(const xllm_session* pSession, uint64_t uTurn)
{
    size_t i;
    for ( i = 0u; i < pSession->iEntryCount; ++i ) {
        const xllm_session_entry* pEntry = &pSession->pEntries[i];
        size_t j;
        if ( pEntry->uTurn != uTurn || pEntry->tMessage.eRole != XLLM_ROLE_ASSISTANT ) continue;
        for ( j = 0u; j < pEntry->tMessage.iToolCallCount; ++j ) {
            if ( !xllm_session__tool_resolved_range(pSession, i, pEntry->tMessage.pToolCalls[j].sId) ) return false;
        }
    }
    return true;
}

/* A cut after uThrough is pair-complete when no tool result <= uThrough answers
 * a tool call > uThrough and vice versa. */
bool xllm_session__plan_pair_safe(const xllm_session* pSession, uint64_t uThrough)
{
    size_t i;
    for ( i = 0u; i < pSession->iEntryCount; ++i ) {
        const xllm_session_entry* pEntry = &pSession->pEntries[i];
        size_t j;
        if ( pEntry->tMessage.eRole != XLLM_ROLE_ASSISTANT ) continue;
        for ( j = 0u; j < pEntry->tMessage.iToolCallCount; ++j ) {
            const char* sId = pEntry->tMessage.pToolCalls[j].sId;
            bool bCallSide = pEntry->uSequence <= uThrough;
            size_t k;
            for ( k = 0u; k < pSession->iEntryCount; ++k ) {
                const xllm_session_entry* pTool = &pSession->pEntries[k];
                if ( pTool->tMessage.eRole != XLLM_ROLE_TOOL || !pTool->tMessage.sToolCallId ||
                     strcmp(pTool->tMessage.sToolCallId, sId) != 0 ) continue;
                if ( (pTool->uSequence <= uThrough) != bCallSide ) { return false; }
            }
        }
    }
    return true;
}

/* Shrink uThrough (candidates shrink, tail grows) until the cut no longer
 * splits an assistant tool call from its result. Returns 0 when nothing
 * pair-safe remains above uFloor. */
static uint64_t xllm_session__pair_fix(const xllm_session* pSession, uint64_t uThrough, uint64_t uFloor)
{
    while ( uThrough > uFloor ) {
        uint64_t uMinOffending = UINT64_MAX;
        size_t i;
        for ( i = 0u; i < pSession->iEntryCount; ++i ) {
            const xllm_session_entry* pEntry = &pSession->pEntries[i];
            size_t j;
            if ( pEntry->tMessage.eRole != XLLM_ROLE_ASSISTANT || pEntry->uSequence > uThrough ) continue;
            for ( j = 0u; j < pEntry->tMessage.iToolCallCount; ++j ) {
                const char* sId = pEntry->tMessage.pToolCalls[j].sId;
                size_t k;
                for ( k = 0u; k < pSession->iEntryCount; ++k ) {
                    const xllm_session_entry* pTool = &pSession->pEntries[k];
                    if ( pTool->tMessage.eRole != XLLM_ROLE_TOOL || !pTool->tMessage.sToolCallId ||
                         strcmp(pTool->tMessage.sToolCallId, sId) != 0 || pTool->uSequence <= uThrough ) continue;
                    /* this call sits in the candidates while its result is in the tail */
                    if ( pEntry->uSequence < uMinOffending ) { uMinOffending = pEntry->uSequence; }
                }
            }
        }
        if ( uMinOffending == UINT64_MAX ) { return uThrough; }
        uThrough = uMinOffending - 1u; /* push the call out to the tail */
    }
    return 0u;
}

/* ------------------------------------------------------------------ */
/* Default stage 2: Pi cut point (keep-recent walk at message boundaries) */
/*                                                                      */
/* Split turns follow Pi: when the retained-window-start turn alone      */
/* exceeds the keep-recent budget, its prefix is summarized separately   */
/* (assistant-boundary cut, pair-complete) and the suffix stays in the   */
/* tail. Lexical estimates are used ONLY for this structural walk        */
/* (design §5), never for the trigger decision. The L2 ladder remains   */
/* the structural fallback when summarization is unavailable.            */
/* ------------------------------------------------------------------ */

uint64_t xllm_session__tail_cut(const xllm_session* pSession, uint64_t uKeepTokens, uint64_t uFloor)
{
    uint64_t uTotal = 0u;
    uint64_t uThrough = 0u;
    size_t i;
    size_t k;
    if ( uKeepTokens == 0u ) { uKeepTokens = 1u; }
    /* k = oldest entry index belonging to the retained tail */
    k = pSession->iEntryCount;
    for ( i = pSession->iEntryCount; i > 0u; --i ) {
        const xllm_session_entry* pEntry = &pSession->pEntries[i - 1u];
        if ( (pEntry->uFlags & XLLM_SESSION_ENTRY_PINNED) != 0u || pEntry->uSequence <= uFloor ) { continue; }
        if ( uTotal >= uKeepTokens ) { break; }
        uTotal += pEntry->uEstimatedTokens;
        k = i - 1u;
    }
    /* Snap the boundary forward to the first entry of its turn so the tail
     * keeps whole turns (and their tool pairs) rather than a mid-turn suffix. */
    if ( k < pSession->iEntryCount ) {
        uint64_t uBoundaryTurn = pSession->pEntries[k].uTurn;
        while ( k > 0u ) {
            const xllm_session_entry* pPrev = &pSession->pEntries[k - 1u];
            if ( pPrev->uTurn != uBoundaryTurn || (pPrev->uFlags & XLLM_SESSION_ENTRY_PINNED) != 0u ||
                 pPrev->uSequence <= uFloor ) { break; }
            --k;
        }
    }
    /* candidate through = newest compactable entry older than the tail */
    for ( i = 0u; i < k; ++i ) {
        const xllm_session_entry* pEntry = &pSession->pEntries[i];
        if ( (pEntry->uFlags & XLLM_SESSION_ENTRY_PINNED) != 0u || pEntry->uSequence <= uFloor ) { continue; }
        if ( pEntry->uSequence > uThrough ) { uThrough = pEntry->uSequence; }
    }
    if ( uThrough <= uFloor ) { return 0u; }
    return xllm_session__pair_fix(pSession, uThrough, uFloor);
}

/* Split-turn prefix cut: when the oldest active turn alone exceeds the
 * keep-recent budget, return the sequence its prefix may cover up to
 * (assistant boundary, pair-complete); 0 when no split applies. */
static uint64_t xllm_session__split_prefix(const xllm_session* pSession, uint64_t uFloor)
{
    size_t iFirst = pSession->iEntryCount;
    uint64_t uTurn = 0u;
    uint64_t uTurnTokens = 0u;
    uint64_t uKeep = pSession->tConfig.uKeepRecentTokens;
    uint64_t uSuffixTokens = 0u;
    size_t iSuffixStart;
    size_t i;
    for ( i = 0u; i < pSession->iEntryCount; ++i ) {
        const xllm_session_entry* pEntry = &pSession->pEntries[i];
        if ( (pEntry->uFlags & XLLM_SESSION_ENTRY_PINNED) != 0u || pEntry->uSequence <= uFloor ) { continue; }
        iFirst = i;
        uTurn = pEntry->uTurn;
        break;
    }
    if ( iFirst >= pSession->iEntryCount ) { return 0u; }
    for ( i = iFirst; i < pSession->iEntryCount; ++i ) {
        const xllm_session_entry* pEntry = &pSession->pEntries[i];
        if ( pEntry->uTurn != uTurn || (pEntry->uFlags & XLLM_SESSION_ENTRY_PINNED) != 0u ) { break; }
        uTurnTokens += pEntry->uEstimatedTokens;
    }
    if ( uTurnTokens <= uKeep || uKeep == 0u ) { return 0u; }
    /* walk the suffix backward from the turn end until it reaches budget */
    for ( i = pSession->iEntryCount; i > iFirst; --i ) {
        const xllm_session_entry* pEntry = &pSession->pEntries[i - 1u];
        if ( pEntry->uTurn != uTurn || (pEntry->uFlags & XLLM_SESSION_ENTRY_PINNED) != 0u ) { continue; }
        uSuffixTokens += pEntry->uEstimatedTokens;
        if ( uSuffixTokens >= uKeep ) { break; }
    }
    iSuffixStart = i - 1u; /* [i-1] crossed the budget and is retained */
    if ( iSuffixStart <= iFirst + 1u ) { return 0u; } /* prefix would be empty */
    /* Pi rule: the prefix ends on an assistant entry, never between a tool
     * call and its result (pair-completeness checked per candidate). */
    for ( i = iSuffixStart - 1u; i > iFirst; --i ) {
        const xllm_session_entry* pEntry = &pSession->pEntries[i];
        if ( pEntry->tMessage.eRole != XLLM_ROLE_ASSISTANT ) { continue; }
        if ( !xllm_session__plan_pair_safe(pSession, pEntry->uSequence) ) { continue; }
        return pEntry->uSequence;
    }
    return 0u;
}

static bool xllm_session__default_plan(xllm_session* pSession, uint64_t uPrevThrough,
    xllm_compaction_plan* pPlan, void* pUserData)
{
    uint64_t uFloor = pSession->uCompactedThrough > pSession->uTailFloor
        ? pSession->uCompactedThrough : pSession->uTailFloor;
    uint64_t uThrough = xllm_session__tail_cut(pSession, pSession->tConfig.uKeepRecentTokens, uFloor);
    (void)pUserData;
    pPlan->uPrefixThroughSequence = 0u;
    if ( uThrough > uPrevThrough ) {
        pPlan->uThroughSequence = uThrough;
        return true;
    }
    /* no complete-turn candidates: a split-turn prefix may still apply */
    pPlan->uThroughSequence = uPrevThrough;
    pPlan->uPrefixThroughSequence = xllm_session__split_prefix(pSession, uFloor);
    return pPlan->uPrefixThroughSequence > uPrevThrough;
}

/* ------------------------------------------------------------------ */
/* Default stage 3: Pi candidate serialization                          */
/* ------------------------------------------------------------------ */

static bool xllm_session__append_truncated(xllm_session_buf* pBuf, const char* sText, uint32_t uCapBytes)
{
    size_t iLen = strlen(sText);
    if ( uCapBytes && iLen > uCapBytes ) {
        size_t iHead = uCapBytes / 2u;
        size_t iTail = uCapBytes - iHead;
        char sMarker[64];
        while ( iHead && ((unsigned char)sText[iHead] & 0xC0u) == 0x80u ) { --iHead; }
        if ( iTail > iLen - iHead ) { iTail = iLen - iHead; }
        while ( iHead + iTail < iLen && ((unsigned char)sText[iLen - iTail] & 0xC0u) == 0x80u ) { ++iTail; }
        (void)snprintf(sMarker, sizeof(sMarker), "[... truncated %llu bytes]",
            (unsigned long long)(iLen - iHead - iTail));
        return xllm_session__buf_append(pBuf, sText, iHead) &&
            xllm_session__buf_cstr(pBuf, "\n") && xllm_session__buf_cstr(pBuf, sMarker) &&
            xllm_session__buf_cstr(pBuf, "\n") &&
            xllm_session__buf_append(pBuf, sText + iLen - iTail, iTail);
    }
    return xllm_session__buf_cstr(pBuf, sText);
}

char* xllm_session__serialize_candidates(const xllm_session* pSession, uint64_t uFrom, uint64_t uTo)
{
    xllm_session_buf tBuf = {0};
    size_t i;
    bool bOk = true;
    for ( i = 0u; bOk && i < pSession->iEntryCount; ++i ) {
        const xllm_session_entry* pEntry = &pSession->pEntries[i];
        if ( pEntry->uSequence <= uFrom || pEntry->uSequence > uTo ) continue;
        if ( (pEntry->uFlags & XLLM_SESSION_ENTRY_PINNED) != 0u ) continue;
        switch ( pEntry->tMessage.eRole ) {
            case XLLM_ROLE_USER:
                bOk = xllm_session__buf_cstr(&tBuf, "[User]: ") &&
                    xllm_session__append_truncated(&tBuf,
                        pEntry->tMessage.sContent ? pEntry->tMessage.sContent : "",
                        pSession->tConfig.uUserMessageCapBytes) &&
                    xllm_session__buf_cstr(&tBuf, "\n");
                break;
            case XLLM_ROLE_SYSTEM:
                bOk = xllm_session__buf_cstr(&tBuf, "[System]: ") &&
                    xllm_session__buf_cstr(&tBuf,
                        pEntry->tMessage.sContent ? pEntry->tMessage.sContent : "") &&
                    xllm_session__buf_cstr(&tBuf, "\n");
                break;
            case XLLM_ROLE_ASSISTANT:
                if ( pEntry->tMessage.sReasoningContent && pEntry->tMessage.sReasoningContent[0] ) {
                    bOk = xllm_session__buf_cstr(&tBuf, "[Assistant thinking]: ") &&
                        xllm_session__buf_cstr(&tBuf, pEntry->tMessage.sReasoningContent) &&
                        xllm_session__buf_cstr(&tBuf, "\n");
                }
                if ( bOk && pEntry->tMessage.sContent && pEntry->tMessage.sContent[0] ) {
                    bOk = xllm_session__buf_cstr(&tBuf, "[Assistant]: ") &&
                        xllm_session__buf_cstr(&tBuf, pEntry->tMessage.sContent) &&
                        xllm_session__buf_cstr(&tBuf, "\n");
                }
                {
                    size_t j;
                    for ( j = 0u; bOk && j < pEntry->tMessage.iToolCallCount; ++j ) {
                        const xllm_tool_call* pCall = &pEntry->tMessage.pToolCalls[j];
                        bOk = xllm_session__buf_cstr(&tBuf, "[Assistant tool calls]: ") &&
                            xllm_session__buf_cstr(&tBuf, pCall->sName ? pCall->sName : "") &&
                            xllm_session__buf_cstr(&tBuf, "(") &&
                            xllm_session__buf_cstr(&tBuf, pCall->sArgumentsJson ? pCall->sArgumentsJson : "") &&
                            xllm_session__buf_cstr(&tBuf, ")\n");
                    }
                }
                break;
            case XLLM_ROLE_TOOL:
            default:
                bOk = xllm_session__buf_cstr(&tBuf, "[Tool result") &&
                    (pEntry->tMessage.sToolCallId
                        ? xllm_session__buf_cstr(&tBuf, " ") &&
                          xllm_session__buf_cstr(&tBuf, pEntry->tMessage.sToolCallId)
                        : true) &&
                    xllm_session__buf_cstr(&tBuf, "]: ") &&
                    xllm_session__append_truncated(&tBuf,
                        pEntry->tMessage.sContent ? pEntry->tMessage.sContent : "",
                        pSession->tConfig.uToolResultCapBytes) &&
                    xllm_session__buf_cstr(&tBuf, "\n");
                break;
        }
    }
    if ( !bOk ) {
        xllm_session__buf_unit(&tBuf);
        return NULL;
    }
    return xllm_session__buf_detach(&tBuf);
}

static bool xllm_session__default_serialize(xllm_session* pSession, uint64_t uFrom, uint64_t uTo,
    char** psText, void* pUserData)
{
    (void)pUserData;
    *psText = xllm_session__serialize_candidates(pSession, uFrom, uTo);
    return *psText != NULL;
}

/* ------------------------------------------------------------------ */
/* Default stage 4: prompt construction (instruction + rolling summary) */
/* ------------------------------------------------------------------ */

static bool xllm_session__default_build_prompt(xllm_session* pSession, const char* sPrevSummary,
    const char* sCandidates, char** psPrompt, void* pUserData)
{
    xllm_session_buf tBuf = {0};
    const xllm_session_style* pStyle = xllm_session__style(pSession);
    (void)pUserData;
    if ( !xllm_session__buf_cstr(&tBuf, pStyle->sInstruction) ) goto fail;
    if ( sPrevSummary && sPrevSummary[0] ) {
        if ( !xllm_session__buf_cstr(&tBuf, "<previous_summary>\n") ||
             !xllm_session__buf_cstr(&tBuf, sPrevSummary) ||
             !xllm_session__buf_cstr(&tBuf, "\n</previous_summary>\n\n") ) goto fail;
    }
    if ( !xllm_session__buf_cstr(&tBuf, "<conversation>\n") ||
         !xllm_session__buf_cstr(&tBuf, sCandidates ? sCandidates : "") ||
         !xllm_session__buf_cstr(&tBuf, "</conversation>\n") ) goto fail;
    *psPrompt = xllm_session__buf_detach(&tBuf);
    return *psPrompt != NULL;
fail:
    xllm_session__buf_unit(&tBuf);
    return false;
}

/* ------------------------------------------------------------------ */
/* Default stage 6: byte-based structural quality gate                  */
/* ------------------------------------------------------------------ */

static bool xllm_session__heading_at_line(const char* sText, const char* sHeading)
{
    const char* p = sText;
    size_t iHeading = strlen(sHeading);
    while ( p && *p ) {
        const char* q = p;
        size_t i;
        while ( *q == ' ' || *q == '\t' || *q == '#' || *q == '*' ) ++q;
        for ( i = 0u; i < iHeading; ++i ) {
            if ( !q[i] || tolower((unsigned char)q[i]) != tolower((unsigned char)sHeading[i]) ) break;
        }
        if ( i == iHeading ) {
            q += iHeading;
            while ( *q == ' ' || *q == '\t' ) ++q;
            if ( *q == ':' || *q == ';' || *q == '\r' || *q == '\n' || *q == '\0' ) return true;
        }
        p = strchr(p, '\n');
        if ( p ) ++p;
    }
    return false;
}

static uint32_t xllm_session__required_sections(const xllm_session* pSession)
{
    uint32_t uRequired = pSession->tConfig.uCompactionRequiredSections;
    const xllm_session_style* pStyle = xllm_session__style(pSession);
    uint32_t uStyleMask = 0u;
    size_t i;
    for ( i = 0u; i < pStyle->iSectionCount; ++i ) { uStyleMask |= pStyle->pSections[i].uFlag; }
    if ( uRequired == 0u ) { return uStyleMask; }
    if ( (uRequired & XLLM_COMPACTION_SECTION_PI_ALL) != 0u ) {
        uRequired |= uStyleMask; /* any Pi bit present selects the full style set */
    }
    return uRequired & (uStyleMask | XLLM_COMPACTION_SECTION_ALL);
}

static void xllm_session__quality_of(xllm_session* pSession, const char* sSummary,
    xllm_compaction_quality* pQuality)
{
    const xllm_session_style* pStyle = xllm_session__style(pSession);
    uint32_t uRequired = xllm_session__required_sections(pSession);
    uint32_t uPresent = 0u;
    size_t i;
    memset(pQuality, 0, sizeof(*pQuality));
    pQuality->uRequiredSections = uRequired;
    pQuality->uMaximumSummaryBytes = pSession->tConfig.uSummaryMaxBytes;
    pQuality->uMaximumSummaryTokens = pSession->tConfig.uSummaryMaxTokens;
    pQuality->uMinimumSummaryTokens = pSession->tConfig.uSummaryMinTokens;
    if ( sSummary ) {
        pQuality->uSummaryBytes = strlen(sSummary);
        pQuality->uSummaryTokens = xllmEstimateTextTokens(sSummary);
        for ( i = 0u; i < pStyle->iSectionCount; ++i ) {
            if ( xllm_session__heading_at_line(sSummary, pStyle->pSections[i].sHeading) ) {
                uPresent |= pStyle->pSections[i].uFlag;
            }
        }
    }
    pQuality->uPresentSections = uPresent;
    pQuality->uMissingSections = uRequired & ~uPresent;
    pQuality->bAccepted = sSummary != NULL && sSummary[0] != '\0' &&
        pQuality->uSummaryBytes <= pQuality->uMaximumSummaryBytes &&
        pQuality->uMissingSections == 0u;
}

bool xllm_session__summary_text_ok(const xllm_session* pSession, const char* sSummary)
{
    xllm_compaction_quality tQuality;
    xllm_session__quality_of((xllm_session*)pSession, sSummary, &tQuality);
    return tQuality.bAccepted;
}

static bool xllm_session__default_evaluate(xllm_session* pSession, const char* sSummary,
    xllm_compaction_quality* pQuality, void* pUserData)
{
    (void)pUserData;
    xllm_session__quality_of(pSession, sSummary, pQuality);
    return true;
}

/* ------------------------------------------------------------------ */
/* The default ops table: each entry is the Pi implementation stage.    */
/* ------------------------------------------------------------------ */

static xllm_compact_decision xllm_session__default_should_compact(xllm_session* pSession,
    const xllm_session_stats* pStats, void* pUserData)
{
    (void)pSession; (void)pUserData;
    return pStats && pStats->ePressure >= XLLM_SESSION_PRESSURE_COMPACT
        ? XLLM_COMPACT_YES : XLLM_COMPACT_NO;
}

const xllm_compaction_ops* xllmSessionDefaultCompactionOps(void)
{
    static const xllm_compaction_ops tDefault = {
        xllm_session__default_should_compact,
        xllm_session__default_plan,
        xllm_session__default_serialize,
        xllm_session__default_build_prompt,
        NULL, /* pSummarize: the easy layer falls back to the bound client */
        xllm_session__default_evaluate,
        NULL, /* pOnCommitted: observation only */
        NULL, /* pUserData */
        {0, 0, 0, 0}
    };
    return &tDefault;
}

bool xllmSessionSetCompactionOps(xllm_session* pSession, const xllm_compaction_ops* pOps)
{
    if ( !pSession ) { return false; }
    pSession->pOps = pOps;
    return true;
}

/* Resolve an ops stage: the session override or the default table. */
#define XLLM_SESSION_OPS(pSession, member) \
    ((pSession)->pOps && (pSession)->pOps->member ? (pSession)->pOps->member \
        : xllmSessionDefaultCompactionOps()->member)
#define XLLM_SESSION_OPS_DATA(pSession) \
    ((pSession)->pOps ? (pSession)->pOps->pUserData : NULL)

/* Host callbacks arm the re-entrancy guard; internal pipeline hops
 * (MaybeCompact -> auto_compact -> Prepare/Commit) must not. */
static bool xllm_session__ops_plan(xllm_session* pSession, uint64_t uPrev,
    xllm_compaction_plan* pPlan, xllm_error* pError)
{
    bool bOk;
    if ( !xllm_session__hook_enter(pSession, pError, "ops.plan") ) { return false; }
    bOk = XLLM_SESSION_OPS(pSession, pPlan)(pSession, uPrev, pPlan, XLLM_SESSION_OPS_DATA(pSession));
    xllm_session__hook_leave(pSession);
    return bOk;
}

static bool xllm_session__ops_serialize(xllm_session* pSession, uint64_t uFrom, uint64_t uTo,
    char** psText, xllm_error* pError)
{
    bool bOk;
    if ( !xllm_session__hook_enter(pSession, pError, "ops.serialize") ) { return false; }
    bOk = XLLM_SESSION_OPS(pSession, pSerialize)(pSession, uFrom, uTo, psText, XLLM_SESSION_OPS_DATA(pSession));
    xllm_session__hook_leave(pSession);
    return bOk;
}

static bool xllm_session__ops_build_prompt(xllm_session* pSession, const char* sPrev,
    const char* sCandidates, char** psPrompt, xllm_error* pError)
{
    bool bOk;
    if ( !xllm_session__hook_enter(pSession, pError, "ops.build_prompt") ) { return false; }
    bOk = XLLM_SESSION_OPS(pSession, pBuildPrompt)(pSession, sPrev, sCandidates, psPrompt, XLLM_SESSION_OPS_DATA(pSession));
    xllm_session__hook_leave(pSession);
    return bOk;
}

static xllm_compact_decision xllm_session__ops_should(xllm_session* pSession,
    const xllm_session_stats* pStats)
{
    xllm_compact_decision eDecision;
    if ( !xllm_session__hook_enter(pSession, NULL, "ops.should_compact") ) { return XLLM_COMPACT_NO; }
    eDecision = XLLM_SESSION_OPS(pSession, pShouldCompact)(pSession, pStats, XLLM_SESSION_OPS_DATA(pSession));
    xllm_session__hook_leave(pSession);
    return eDecision;
}

static bool xllm_session__ops_evaluate(xllm_session* pSession, const char* sSummary,
    xllm_compaction_quality* pQuality, xllm_error* pError)
{
    bool bOk;
    if ( !xllm_session__hook_enter(pSession, pError, "ops.evaluate") ) { return false; }
    bOk = XLLM_SESSION_OPS(pSession, pEvaluate)(pSession, sSummary, pQuality, XLLM_SESSION_OPS_DATA(pSession));
    xllm_session__hook_leave(pSession);
    return bOk;
}

static bool xllm_session__ops_summarize(xllm_session* pSession, const char* sPrompt,
    char** psSummary, xllm_usage* pUsage, xllm_error* pError)
{
    bool bOk;
    if ( !xllm_session__hook_enter(pSession, pError, "ops.summarize") ) { return false; }
    bOk = pSession->pOps->pSummarize(pSession, sPrompt, psSummary, pUsage, pSession->pOps->pUserData);
    xllm_session__hook_leave(pSession);
    return bOk;
}

/* ------------------------------------------------------------------ */
/* Two-phase transaction (v2 API shape, ops-driven inside)              */
/* ------------------------------------------------------------------ */

xllm_compaction* xllmSessionPrepareCompaction(xllm_session* pSession, bool bForce, xllm_error* pError)
{
    xllm_session_stats tStats;
    xllm_compaction* pCompaction = NULL;
    xllm_compaction_plan tPlan = {0};
    char* sCandidates = NULL;
    char* sPrompt = NULL;
    if ( pError ) { xllmErrorInit(pError); }
    if ( !pSession || !xllmSessionGetStats(pSession, &tStats) ) {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT, "session is required");
        return NULL;
    }
    xllm_session__event(pSession, XLLM_SESSION_EVENT_COMPACT_PREPARE, 0u, 0u, NULL);
    if ( !bForce && tStats.ePressure < XLLM_SESSION_PRESSURE_COMPACT ) {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT, "compaction threshold has not been reached");
        return NULL;
    }
    if ( !xllm_session__ops_plan(pSession, pSession->uCompactedThrough, &tPlan, pError) ) {
        xllm_session__error(pError, XLLM_ERROR_PROTOCOL, "no completed prefix is safe to compact yet");
        return NULL;
    }
    if ( tPlan.uPrefixThroughSequence != 0u ) {
        if ( tPlan.uPrefixThroughSequence <= pSession->uCompactedThrough ||
             tPlan.uPrefixThroughSequence <= tPlan.uThroughSequence ) {
            xllm_session__error(pError, XLLM_ERROR_PROTOCOL, "invalid split-turn prefix range");
            return NULL;
        }
    } else if ( tPlan.uThroughSequence <= pSession->uCompactedThrough ) {
        xllm_session__error(pError, XLLM_ERROR_PROTOCOL, "no completed prefix is safe to compact yet");
        return NULL;
    }
    xllm_session__event(pSession, XLLM_SESSION_EVENT_COMPACT_PLAN, pSession->uCompactedThrough,
        tPlan.uPrefixThroughSequence != 0u ? tPlan.uPrefixThroughSequence : tPlan.uThroughSequence, NULL);
    if ( tPlan.uThroughSequence > pSession->uCompactedThrough &&
         !xllm_session__ops_serialize(pSession, pSession->uCompactedThrough,
             tPlan.uThroughSequence, &sCandidates, pError) ) {
        xllm_session__error(pError, XLLM_ERROR_OUT_OF_MEMORY, "failed to serialize compaction candidates");
        return NULL;
    }
    if ( tPlan.uPrefixThroughSequence != 0u ) {
        /* Split turn: serialize the prefix range and frame it with the Pi
         * turn-context template; pBuildPrompt keeps its single-text contract. */
        char* sPrefix = NULL;
        const char* sOriginal = "";
        xllm_session_buf tFramed = {0};
        size_t i;
        for ( i = 0u; i < pSession->iEntryCount; ++i ) {
            const xllm_session_entry* pEntry = &pSession->pEntries[i];
            if ( pEntry->uSequence > tPlan.uThroughSequence &&
                 pEntry->uSequence <= tPlan.uPrefixThroughSequence &&
                 pEntry->tMessage.eRole == XLLM_ROLE_USER && pEntry->tMessage.sContent ) {
                sOriginal = pEntry->tMessage.sContent;
                break;
            }
        }
        if ( !xllm_session__ops_serialize(pSession, tPlan.uThroughSequence,
                 tPlan.uPrefixThroughSequence, &sPrefix, pError) ) {
            free(sCandidates);
            xllm_session__error(pError, XLLM_ERROR_OUT_OF_MEMORY, "failed to serialize the split-turn prefix");
            return NULL;
        }
        if ( !xllm_session__buf_cstr(&tFramed, sCandidates ? sCandidates : "") ) goto split_oom;
        if ( !xllm_session__buf_cstr(&tFramed,
                "\n**Turn Context (split turn):**\n"
                "This is the PREFIX of a turn that was too large to keep. "
                "The SUFFIX (recent work) is retained.\n\n"
                "## Original Request\n") ||
             !xllm_session__buf_cstr(&tFramed, sOriginal) ||
             !xllm_session__buf_cstr(&tFramed, "\n\n## Early Progress\n") ||
             !xllm_session__buf_cstr(&tFramed, sPrefix) ||
             !xllm_session__buf_cstr(&tFramed,
                 "\n\n## Context for Suffix\n"
                 "The suffix of this turn is retained verbatim in the recent "
                 "window; continue from it.\n") ) goto split_oom;
        free(sPrefix);
        free(sCandidates);
        sCandidates = xllm_session__buf_detach(&tFramed);
        if ( !sCandidates ) goto split_oom;
        goto split_done;
split_oom:
        free(sPrefix);
        free(sCandidates);
        xllm_session__buf_unit(&tFramed);
        xllm_session__error(pError, XLLM_ERROR_OUT_OF_MEMORY, "failed to frame the split-turn candidates");
        return NULL;
split_done:;
    }
    if ( !xllm_session__ops_build_prompt(pSession, pSession->sSummary, sCandidates,
            &sPrompt, pError) ) {
        free(sCandidates);
        xllm_session__error(pError, XLLM_ERROR_OUT_OF_MEMORY, "failed to build the compaction prompt");
        return NULL;
    }
    free(sCandidates);
    pCompaction = (xllm_compaction*)calloc(1u, sizeof(*pCompaction));
    if ( !pCompaction ) {
        free(sPrompt);
        xllm_session__error(pError, XLLM_ERROR_OUT_OF_MEMORY, "failed to build compaction transaction");
        return NULL;
    }
    pCompaction->pSession = pSession;
    pCompaction->uBaseCompactedThrough = pSession->uCompactedThrough;
    /* the committed boundary covers the split-turn prefix when present */
    pCompaction->uThroughSequence = tPlan.uPrefixThroughSequence != 0u
        ? tPlan.uPrefixThroughSequence : tPlan.uThroughSequence;
    pCompaction->sPrompt = sPrompt;
    pCompaction->uEstimatedTokens = xllmEstimateTextTokens(sPrompt);
    xllm_session__event(pSession, XLLM_SESSION_EVENT_COMPACT_PROMPT, 0u,
        pCompaction->uEstimatedTokens, NULL);
    return pCompaction;
}

const char* xllmCompactionPrompt(const xllm_compaction* pCompaction)
{
    return pCompaction ? pCompaction->sPrompt : NULL;
}

uint64_t xllmCompactionThroughSequence(const xllm_compaction* pCompaction)
{
    return pCompaction ? pCompaction->uThroughSequence : 0u;
}

uint64_t xllmCompactionEstimatedTokens(const xllm_compaction* pCompaction)
{
    return pCompaction ? pCompaction->uEstimatedTokens : 0u;
}

bool xllmCompactionSetUsage(xllm_compaction* pCompaction, const xllm_usage* pUsage)
{
    if ( !pCompaction || !pUsage ) { return false; }
    pCompaction->uUsagePromptTokens = pUsage->uInputTokens;
    pCompaction->uUsageOutputTokens = pUsage->uOutputTokens;
    return true;
}

bool xllmCompactionEvaluateSummary(const xllm_compaction* pCompaction, const char* sSummary,
    xllm_compaction_quality* pQuality, xllm_error* pError)
{
    if ( pError ) { xllmErrorInit(pError); }
    if ( !pCompaction || !pCompaction->pSession || !pQuality ) {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT,
            "compaction, summary, and quality report are required");
        return false;
    }
    return xllm_session__ops_evaluate(pCompaction->pSession, sSummary, pQuality, pError);
}

bool xllmSessionCommitCompaction(xllm_session* pSession, xllm_compaction* pCompaction,
    const char* sSummary, xllm_error* pError)
{
    xllm_compaction_quality tQuality;
    char* sCopy;
    if ( pError ) { xllmErrorInit(pError); }
    if ( !pSession || !pCompaction || pCompaction->pSession != pSession || pCompaction->bCommitted ||
         !sSummary || !sSummary[0] || pSession->uCompactedThrough != pCompaction->uBaseCompactedThrough ) {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT, "invalid or stale compaction transaction");
        return false;
    }
    if ( !xllm_session__plan_pair_safe(pSession, pCompaction->uThroughSequence) ) {
        xllm_session__error(pError, XLLM_ERROR_HOOK, "compaction plan breaks tool-call pairing");
        return false;
    }
    if ( !xllmCompactionEvaluateSummary(pCompaction, sSummary, &tQuality, pError) ) {
        return false;
    }
    if ( !tQuality.bAccepted ) {
        xllm_session__event(pSession, XLLM_SESSION_EVENT_COMPACT_ABORT, 0u, 0u, "quality_gate");
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT,
            "compaction summary failed the configured quality policy");
        return false;
    }
    sCopy = xllm_session__strdup(sSummary);
    if ( !sCopy ) {
        xllm_session__error(pError, XLLM_ERROR_OUT_OF_MEMORY, "failed to store compaction summary");
        return false;
    }
    if ( !xllm_session__journal_append_compaction(pSession, pCompaction->uThroughSequence,
            pSession->uSummaryGeneration + 1u, pCompaction->uUsagePromptTokens,
            pCompaction->uUsageOutputTokens, sSummary) ) {
        free(sCopy);
        xllm_session__error(pError, XLLM_ERROR_NETWORK, "failed to append compaction to the session journal");
        return false;
    }
    free(pSession->sSummary);
    pSession->sSummary = sCopy;
    pSession->uCompactedThrough = pCompaction->uThroughSequence;
    ++pSession->uCompactionCount;
    ++pSession->uSummaryGeneration;
    pSession->uSummaryPromptAtBirth = pCompaction->uUsagePromptTokens;
    pSession->uSummaryOutputAtBirth = pCompaction->uUsageOutputTokens;
    /* Streak anchor: compactions from here need a new user entry to reset. */
    pSession->uLastUserSequence = pSession->uNextSequence;
    pCompaction->bCommitted = true;
    xllm_session__invalidate_fill(pSession);
    xllm_session__event(pSession, XLLM_SESSION_EVENT_COMPACT_COMMIT,
        pCompaction->uBaseCompactedThrough, pCompaction->uThroughSequence, sSummary);
    if ( pSession->pOps && pSession->pOps->pOnCommitted ) {
        xllm_session_summary tSummaryView;
        xllmSessionGetSummary(pSession, &tSummaryView);
        if ( xllm_session__hook_enter(pSession, NULL, "ops.on_committed") ) {
            pSession->pOps->pOnCommitted(pSession, &tSummaryView, pSession->pOps->pUserData);
            xllm_session__hook_leave(pSession);
        }
    }
    return true;
}

void xllmCompactionDestroy(xllm_compaction* pCompaction)
{
    if ( !pCompaction ) { return; }
    free(pCompaction->sPrompt);
    free(pCompaction);
}

/* ------------------------------------------------------------------ */
/* Auto compaction (threshold/overflow path through the current ops)    */
/* ------------------------------------------------------------------ */

static bool xllm_session__new_user_since(const xllm_session* pSession, uint64_t uSinceSequence)
{
    size_t i;
    for ( i = 0u; i < pSession->iEntryCount; ++i ) {
        const xllm_session_entry* pEntry = &pSession->pEntries[i];
        if ( pEntry->uSequence > uSinceSequence && pEntry->tMessage.eRole == XLLM_ROLE_USER &&
             (pEntry->uFlags & XLLM_SESSION_ENTRY_PINNED) == 0u ) {
            return true;
        }
    }
    return false;
}

bool xllm_session__auto_compact(xllm_session* pSession, xllm_error* pError)
{
    xllm_compaction* pCompaction = NULL;
    xllm_usage tUsage;
    char* sSummary = NULL;
    bool bOk = false;
    if ( pSession->uAutoCompactStreak >= 2u ) {
        xllm_session__error(pError, XLLM_ERROR_LIMIT,
            "auto-compaction loop guard engaged; widen the window or lower keep-recent");
        return false;
    }
    if ( !xllm_session__new_user_since(pSession, pSession->uLastUserSequence) &&
         pSession->uCompactionCount != 0u ) {
        ++pSession->uAutoCompactStreak;
    } else {
        pSession->uAutoCompactStreak = 0u;
    }
    pCompaction = xllmSessionPrepareCompaction(pSession, true, pError);
    if ( !pCompaction ) { return false; }
    memset(&tUsage, 0, sizeof(tUsage));
    if ( pSession->pOps && pSession->pOps->pSummarize ) {
        if ( !xllm_session__ops_summarize(pSession, pCompaction->sPrompt, &sSummary, &tUsage, pError) ||
             !sSummary ) {
            xllm_session__event(pSession, XLLM_SESSION_EVENT_COMPACT_ABORT, 0u, 0u, "summarize");
            xllm_session__error(pError, XLLM_ERROR_HOOK, "compaction summarize hook failed");
            goto done;
        }
    } else if ( pSession->pClient || pSession->pTestCall ) {
        if ( !xllm_session__client_summarize(pSession, pCompaction->sPrompt, &sSummary, &tUsage, pError) ) {
            xllm_session__event(pSession, XLLM_SESSION_EVENT_COMPACT_ABORT, 0u, 0u, "summarize");
            goto done;
        }
    } else {
        xllm_session__event(pSession, XLLM_SESSION_EVENT_COMPACT_ABORT, 0u, 0u, "no_meta_call");
        xllm_session__error(pError, XLLM_ERROR_HOOK,
            "auto compaction needs a bound client or a custom pSummarize; drive it manually");
        goto done;
    }
    xllm_session__event(pSession, XLLM_SESSION_EVENT_COMPACT_SUMMARY, 0u, 0u, sSummary);
    (void)xllmCompactionSetUsage(pCompaction, &tUsage);
    bOk = xllmSessionCommitCompaction(pSession, pCompaction, sSummary, pError);
done:
    free(sSummary);
    xllmCompactionDestroy(pCompaction);
    return bOk;
}

bool xllmSessionMaybeCompact(xllm_session* pSession, bool* pbCompact, xllm_error* pError)
{
    xllm_session_stats tStats;
    if ( pError ) { xllmErrorInit(pError); }
    if ( pbCompact ) { *pbCompact = false; }
    if ( !pSession || !xllmSessionGetStats(pSession, &tStats) ) {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT, "session is required");
        return false;
    }
    if ( tStats.ePressure < XLLM_SESSION_PRESSURE_COMPACT ) {
        return true; /* not due */
    }
    if ( xllm_session__ops_should(pSession, &tStats) != XLLM_COMPACT_YES ) {
        return true; /* due but vetoed by the strategy */
    }
    if ( !xllm_session__auto_compact(pSession, pError) ) { return false; }
    if ( pbCompact ) { *pbCompact = true; }
    return true;
}

/* ------------------------------------------------------------------ */
/* Overflow ladder (design §7)                                          */
/* ------------------------------------------------------------------ */

bool xllmSessionOverflowLadder(xllm_session* pSession, xllm_error* pError)
{
    xllm_session_stats tStats;
    if ( pError ) { xllmErrorInit(pError); }
    if ( !pSession || !xllmSessionGetStats(pSession, &tStats) ) {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT, "session is required");
        return false;
    }
    /* L1: full compaction via the current ops (correctness path, no veto). */
    if ( tStats.ePressure >= XLLM_SESSION_PRESSURE_COMPACT ) {
        if ( xllm_session__auto_compact(pSession, pError) ) {
            return true;
        }
        /* fall through to L2; a summary failure must not block truncation */
        if ( pError ) { pError->eCode = XLLM_ERROR_NONE; pError->sMessage[0] = '\0'; }
    }
    /* L2: structural tail truncation at pair-safe message boundaries down to
     * keep-recent/2; the floor never crosses the compaction checkpoint. */
    {
        uint64_t uKeepHalf = pSession->tConfig.uKeepRecentTokens / 2u;
        uint64_t uFloor = pSession->uCompactedThrough > pSession->uTailFloor
            ? pSession->uCompactedThrough : pSession->uTailFloor;
        uint64_t uOldFloor = pSession->uTailFloor;
        uint64_t uCut;
        if ( uKeepHalf == 0u ) { uKeepHalf = 1u; }
        uCut = xllm_session__tail_cut(pSession, uKeepHalf, uFloor);
        if ( uCut == 0u || uCut <= pSession->uTailFloor ||
             !xllm_session__plan_pair_safe(pSession, uCut) ) {
            xllm_session__error(pError, XLLM_ERROR_LIMIT,
                "overflow ladder exhausted; the tail cannot be reduced further");
            return false;
        }
        if ( !xllm_session__journal_append_truncate(pSession, uOldFloor, uCut) ) {
            xllm_session__error(pError, XLLM_ERROR_NETWORK,
                "failed to journal the overflow truncation");
            return false;
        }
        pSession->uTailFloor = uCut;
        ++pSession->uSummaryGeneration;
        xllm_session__invalidate_fill(pSession);
        xllm_session__event(pSession, XLLM_SESSION_EVENT_LADDER_TRUNCATE, uOldFloor, uCut, "overflow_l2");
        return true;
    }
}
