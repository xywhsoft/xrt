#include "xllm_session_internal.h"

/* v2 render-time soft prune: old oversized tool outputs collapse to a head/
 * marker/tail form so message and tool-call structure survives pressure. */
char* xllm_session__pruned_content(const xllm_session* pSession, const xllm_session_entry* pEntry)
{
    static const char sMarker[] = "\n\n[... older tool output pruned from active context; full output remains in the persisted session ledger ...]\n\n";
    const char* sContent = pEntry->tMessage.sContent ? pEntry->tMessage.sContent : "";
    size_t iLen = strlen(sContent);
    size_t iKeep = pSession->tConfig.uToolPruneBytes;
    size_t iHead;
    size_t iTail;
    char* sOut;
    if ( iLen <= iKeep ) { return xllm_session__strdup(sContent); }
    iHead = (iKeep * 3u) / 4u;
    iTail = iKeep - iHead;
    while ( iHead && ((unsigned char)sContent[iHead] & 0xC0u) == 0x80u ) { --iHead; }
    while ( iTail < iLen && ((unsigned char)sContent[iLen - iTail] & 0xC0u) == 0x80u ) { ++iTail; }
    sOut = (char*)malloc(iHead + sizeof(sMarker) - 1u + iTail + 1u);
    if ( !sOut ) { return NULL; }
    memcpy(sOut, sContent, iHead);
    memcpy(sOut + iHead, sMarker, sizeof(sMarker) - 1u);
    memcpy(sOut + iHead + sizeof(sMarker) - 1u, sContent + iLen - iTail, iTail);
    sOut[iHead + sizeof(sMarker) - 1u + iTail] = '\0';
    return sOut;
}

/* Rendered view of one entry with the v2 prune applied, ready for hooks. */
static bool xllm_session__render_entry(const xllm_session* pSession, const xllm_session_entry* pEntry,
    xllm_message* pWork, bool bPrune)
{
    if ( !xllm_session__message_clone(pWork, &pEntry->tMessage) ) { return false; }
    if ( bPrune && xllm_session__should_prune_tool(pSession, pEntry) ) {
        char* sPruned = xllm_session__pruned_content(pSession, pEntry);
        bool bOk = sPruned && xllmMessageSetContent(pWork, sPruned);
        free(sPruned);
        if ( !bOk ) {
            xllmMessageUnit(pWork);
            return false;
        }
    }
    return true;
}

/* Pair-safety of SKIP decisions: a rendered tool result whose call was
 * skipped, or a rendered assistant call whose only result was skipped,
 * would produce a provider-invalid request. */
static bool xllm_session__skip_pair_safe(const xllm_session* pSession, const bool* pbKept)
{
    size_t i;
    for ( i = 0u; i < pSession->iEntryCount; ++i ) {
        const xllm_session_entry* pEntry = &pSession->pEntries[i];
        if ( !pbKept[i] ) continue;
        if ( pEntry->tMessage.eRole == XLLM_ROLE_TOOL && pEntry->tMessage.sToolCallId ) {
            const char* sId = pEntry->tMessage.sToolCallId;
            size_t k;
            bool bCallExists = false;
            bool bCallKept = false;
            for ( k = 0u; k < pSession->iEntryCount; ++k ) {
                const xllm_session_entry* pCall = &pSession->pEntries[k];
                size_t m;
                if ( pCall->tMessage.eRole != XLLM_ROLE_ASSISTANT ) continue;
                for ( m = 0u; m < pCall->tMessage.iToolCallCount; ++m ) {
                    if ( strcmp(pCall->tMessage.pToolCalls[m].sId, sId) == 0 ) {
                        bCallExists = true;
                        if ( pbKept[k] ) { bCallKept = true; }
                    }
                }
            }
            if ( bCallExists && !bCallKept ) { return false; }
        } else if ( pEntry->tMessage.eRole == XLLM_ROLE_ASSISTANT ) {
            size_t j;
            for ( j = 0u; j < pEntry->tMessage.iToolCallCount; ++j ) {
                const char* sId = pEntry->tMessage.pToolCalls[j].sId;
                size_t k;
                bool bResultExists = false;
                bool bResultKept = false;
                for ( k = 0u; k < pSession->iEntryCount; ++k ) {
                    const xllm_session_entry* pTool = &pSession->pEntries[k];
                    if ( pTool->tMessage.eRole != XLLM_ROLE_TOOL || !pTool->tMessage.sToolCallId ||
                         strcmp(pTool->tMessage.sToolCallId, sId) != 0 ) continue;
                    bResultExists = true;
                    if ( pbKept[k] ) { bResultKept = true; }
                }
                if ( bResultExists && !bResultKept ) { return false; }
            }
        }
    }
    return true;
}

static bool xllm_session__summary_message(const xllm_session* pSession, xllm_message* pMessage)
{
    xllm_session_buf tSummary = {0};
    bool bOk;
    /* The summary is a synthetic continuation turn, not a second system
     * message. Keeping it as user content also gives providers a valid
     * user bridge when the retained suffix begins with assistant/tool
     * messages from an in-progress agent loop. */
    xllmMessageInit(pMessage, XLLM_ROLE_USER);
    bOk = xllm_session__buf_cstr(&tSummary, "Compacted session state. Treat this as authoritative continuity for history through sequence ") &&
        xllm_session__buf_u64(&tSummary, pSession->uCompactedThrough) &&
        xllm_session__buf_cstr(&tSummary, ":\n\n") &&
        xllm_session__buf_cstr(&tSummary, pSession->sSummary) &&
        xllmMessageSetContent(pMessage, tSummary.pData);
    xllm_session__buf_unit(&tSummary);
    return bOk;
}

bool xllmSessionBuildRequest(const xllm_session* pSession, xllm_request* pRequest, xllm_error* pError)
{
    xllm_session_stats tStats;
    bool* pbKept = NULL;
    size_t i;
    bool bPrune;
    bool bOk = false;
    const xllm_session_hooks* pHooks;
    if ( pError ) { xllmErrorInit(pError); }
    if ( !pSession || !pRequest || !xllmSessionGetStats(pSession, &tStats) ) {
        xllm_session__error(pError, XLLM_ERROR_INVALID_ARGUMENT, "session and initialized request are required");
        return false;
    }
    pHooks = pSession->pHooks;
    if ( tStats.ePressure == XLLM_SESSION_PRESSURE_OVERFLOW ) {
        xllm_session__error(pError, XLLM_ERROR_PROTOCOL, "active context exceeds the model input budget and must be compacted");
        return false;
    }
    if ( pRequest->uMaxOutputTokens == 0u || pRequest->uMaxOutputTokens > tStats.uNextMaxOutputTokens ) {
        pRequest->uMaxOutputTokens = tStats.uNextMaxOutputTokens;
    }
    if ( pRequest->uMaxOutputTokens == 0u ) {
        xllm_session__error(pError, XLLM_ERROR_PROTOCOL, "active context leaves no room for model output");
        return false;
    }
    if ( pHooks && pHooks->pRenderMessage ) {
        pbKept = (bool*)calloc(pSession->iEntryCount ? pSession->iEntryCount : 1u, sizeof(bool));
        if ( !pbKept ) {
            xllm_session__error(pError, XLLM_ERROR_OUT_OF_MEMORY, "failed to render session request");
            return false;
        }
    }
    bPrune = tStats.uRawActiveTokens >= tStats.uPruneThresholdTokens;
#define XLLM_SESSION_RENDER_FAIL(code, msg) do { \
    xllm_session__error(pError, (code), (msg)); \
    goto done; \
} while (0)
    /* PINNED entries first: the never-compacted cache anchor. */
    for ( i = 0u; i < pSession->iEntryCount; ++i ) {
        const xllm_session_entry* pEntry = &pSession->pEntries[i];
        if ( (pEntry->uFlags & XLLM_SESSION_ENTRY_PINNED) == 0u ) { continue; }
        if ( pHooks && pHooks->pRenderMessage ) {
            xllm_message tWork;
            xllm_render_action eAction;
            if ( !xllm_session__render_entry(pSession, pEntry, &tWork, false) ) {
                XLLM_SESSION_RENDER_FAIL(XLLM_ERROR_OUT_OF_MEMORY, "failed to render session request");
            }
            if ( !xllm_session__hook_enter((xllm_session*)pSession, NULL, "render.message") ) {
                xllmMessageUnit(&tWork);
                XLLM_SESSION_RENDER_FAIL(XLLM_ERROR_HOOK, "render hook re-entered a mutating API");
            }
            eAction = pHooks->pRenderMessage((xllm_session*)pSession,
                pEntry->uSequence, pEntry->uTurn, pEntry->uFlags, &tWork, pHooks->pUserData);
            xllm_session__hook_leave((xllm_session*)pSession);
            if ( eAction == XLLM_RENDER_SKIP ) {
                pbKept[i] = false;
                xllmMessageUnit(&tWork);
                continue;
            }
            pbKept[i] = true;
            if ( !xllmRequestAddMessage(pRequest, &tWork) ) {
                xllmMessageUnit(&tWork);
                XLLM_SESSION_RENDER_FAIL(XLLM_ERROR_OUT_OF_MEMORY, "failed to render session request");
            }
            xllmMessageUnit(&tWork);
        } else if ( !xllmRequestAddMessage(pRequest, &pEntry->tMessage) ) {
            XLLM_SESSION_RENDER_FAIL(XLLM_ERROR_OUT_OF_MEMORY, "failed to render session request");
        }
    }
    /* Rolling summary as the user bridge (design §6.6). */
    if ( pSession->sSummary && pSession->sSummary[0] ) {
        xllm_message tWork;
        if ( !xllm_session__summary_message(pSession, &tWork) ) {
            XLLM_SESSION_RENDER_FAIL(XLLM_ERROR_OUT_OF_MEMORY, "failed to render the compaction summary");
        }
        if ( pHooks && pHooks->pRenderSummary ) {
            xllm_session_summary tView;
            bool bInject = false;
            xllmSessionGetSummary(pSession, &tView);
            if ( xllm_session__hook_enter((xllm_session*)pSession, NULL, "render.summary") ) {
                bInject = pHooks->pRenderSummary((xllm_session*)pSession, &tView, &tWork, pHooks->pUserData);
                xllm_session__hook_leave((xllm_session*)pSession);
            }
            if ( bInject && !xllmRequestAddMessage(pRequest, &tWork) ) {
                xllmMessageUnit(&tWork);
                XLLM_SESSION_RENDER_FAIL(XLLM_ERROR_OUT_OF_MEMORY, "failed to render the compaction summary");
            }
            xllmMessageUnit(&tWork);
        } else if ( !xllmRequestAddMessage(pRequest, &tWork) ) {
            xllmMessageUnit(&tWork);
            XLLM_SESSION_RENDER_FAIL(XLLM_ERROR_OUT_OF_MEMORY, "failed to render the compaction summary");
        } else {
            xllmMessageUnit(&tWork);
        }
    }
    /* L2 truncation marker, so the model knows older tail turns were dropped. */
    if ( pSession->uTailFloor > pSession->uCompactedThrough ) {
        if ( !xllmRequestAddTextMessage(pRequest, XLLM_ROLE_SYSTEM,
                "[Earlier turns of the retained tail were truncated by overflow recovery.]") ) {
            XLLM_SESSION_RENDER_FAIL(XLLM_ERROR_OUT_OF_MEMORY, "failed to render the truncation marker");
        }
    }
    /* Tail window: verbatim entries after the compaction checkpoint/floor. */
    for ( i = 0u; i < pSession->iEntryCount; ++i ) {
        const xllm_session_entry* pEntry = &pSession->pEntries[i];
        if ( (pEntry->uFlags & XLLM_SESSION_ENTRY_PINNED) != 0u || pEntry->uSequence <= pSession->uCompactedThrough ) continue;
        if ( pEntry->uSequence <= pSession->uTailFloor ) continue;
        if ( pHooks && pHooks->pRenderMessage ) {
            xllm_message tWork;
            xllm_render_action eAction;
            if ( !xllm_session__render_entry(pSession, pEntry, &tWork, bPrune) ) {
                XLLM_SESSION_RENDER_FAIL(XLLM_ERROR_OUT_OF_MEMORY, "failed to render session request");
            }
            if ( !xllm_session__hook_enter((xllm_session*)pSession, NULL, "render.message") ) {
                xllmMessageUnit(&tWork);
                XLLM_SESSION_RENDER_FAIL(XLLM_ERROR_HOOK, "render hook re-entered a mutating API");
            }
            eAction = pHooks->pRenderMessage((xllm_session*)pSession,
                pEntry->uSequence, pEntry->uTurn, pEntry->uFlags, &tWork, pHooks->pUserData);
            xllm_session__hook_leave((xllm_session*)pSession);
            if ( eAction == XLLM_RENDER_SKIP ) {
                pbKept[i] = false;
                xllmMessageUnit(&tWork);
                continue;
            }
            pbKept[i] = true;
            if ( !xllmRequestAddMessage(pRequest, &tWork) ) {
                xllmMessageUnit(&tWork);
                XLLM_SESSION_RENDER_FAIL(XLLM_ERROR_OUT_OF_MEMORY, "failed to render session request");
            }
            xllmMessageUnit(&tWork);
        } else if ( bPrune && xllm_session__should_prune_tool(pSession, pEntry) ) {
            xllm_message tWork;
            bool bAdd;
            if ( !xllm_session__render_entry(pSession, pEntry, &tWork, true) ) {
                XLLM_SESSION_RENDER_FAIL(XLLM_ERROR_OUT_OF_MEMORY, "failed to render session request");
            }
            bAdd = xllmRequestAddMessage(pRequest, &tWork);
            xllmMessageUnit(&tWork);
            if ( !bAdd ) {
                XLLM_SESSION_RENDER_FAIL(XLLM_ERROR_OUT_OF_MEMORY, "failed to render session request");
            }
        } else if ( !xllmRequestAddMessage(pRequest, &pEntry->tMessage) ) {
            XLLM_SESSION_RENDER_FAIL(XLLM_ERROR_OUT_OF_MEMORY, "failed to render session request");
        }
    }
    if ( pbKept && !xllm_session__skip_pair_safe(pSession, pbKept) ) {
        XLLM_SESSION_RENDER_FAIL(XLLM_ERROR_PROTOCOL,
            "render hook skipped an entry and broke tool-call pairing");
    }
    if ( pHooks && pHooks->pRenderComplete ) {
        bool bHookOk;
        if ( !xllm_session__hook_enter((xllm_session*)pSession, NULL, "render.complete") ) {
            XLLM_SESSION_RENDER_FAIL(XLLM_ERROR_HOOK, "render hook re-entered a mutating API");
        }
        bHookOk = pHooks->pRenderComplete((xllm_session*)pSession, pRequest, pHooks->pUserData);
        xllm_session__hook_leave((xllm_session*)pSession);
        if ( !bHookOk ) {
            XLLM_SESSION_RENDER_FAIL(XLLM_ERROR_HOOK, "render completion hook failed");
        }
    }
#undef XLLM_SESSION_RENDER_FAIL
    bOk = true;
done:
    free(pbKept);
    return bOk;
}
