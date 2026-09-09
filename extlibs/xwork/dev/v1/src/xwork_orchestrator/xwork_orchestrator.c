#include "../xwork_core/xwork_internal.h"
#include "../../lib/xrt.h"
#include "../../lib/xllm-session.h"
#include "../../lib/xllm-memory.h"

struct xwork_run_async {
    xwork_run *pRun;
    xwork_orchestrator_options tOptions;
    xthread pThread;
    xmutex_struct tLock;
    xllm_cancel_token *pOwnedCancelToken;
    xwork_status iStatus;
    bool bCompleted;
    bool bCancelRequested;
    bool bLockInitialized;
};

static void xwork__init_text_part(xllm_content_part *pPart, const char *sText)
{
    memset(pPart, 0, sizeof(*pPart));
    pPart->eKind = XLLM_PART_TEXT;
    pPart->as.tSource.eKind = XLLM_SOURCE_INLINE_TEXT;
    pPart->as.tSource.as.sText = sText ? sText : "";
}

static void xwork__init_user_message(
    xllm_message *pMessage,
    xllm_content_part *pPart,
    const char *sText
)
{
    memset(pMessage, 0, sizeof(*pMessage));
    xwork__init_text_part(pPart, sText);
    pMessage->eRole = XLLM_ROLE_USER;
    pMessage->pParts = pPart;
    pMessage->iPartCount = 1u;
}

static void xwork__init_system_message(
    xllm_message *pMessage,
    xllm_content_part *pPart,
    const char *sText
)
{
    memset(pMessage, 0, sizeof(*pMessage));
    xwork__init_text_part(pPart, sText);
    pMessage->eRole = XLLM_ROLE_SYSTEM;
    pMessage->pParts = pPart;
    pMessage->iPartCount = 1u;
}

static void xwork__init_memory_context_block(
    xllm_context_block *pBlock,
    xllm_message *pMessage,
    xllm_content_part *pPart,
    const char *sText,
    int iPriority,
    bool bPinned
)
{
    memset(pBlock, 0, sizeof(*pBlock));
    xwork__init_system_message(pMessage, pPart, sText);
    pBlock->eKind = XLLM_CONTEXT_MEMORY;
    pBlock->iPriority = iPriority;
    pBlock->bPinned = bPinned;
    pBlock->pMessages = pMessage;
    pBlock->iMessageCount = 1u;
}

static void xwork__init_tool_result_message(
    xllm_message *pMessage,
    xllm_content_part *pPart,
    const xwork_run *pRun
)
{
    memset(pMessage, 0, sizeof(*pMessage));
    xwork__init_text_part(pPart, pRun->sLastToolResultText);
    pMessage->eRole = XLLM_ROLE_TOOL;
    pMessage->sToolCallId = pRun->sLastToolCallId;
    pMessage->sToolName = pRun->sLastToolId;
    pMessage->pParts = pPart;
    pMessage->iPartCount = 1u;
}

static xwork_status xwork__build_request_tools(
    const xwork_runtime *pRuntime,
    xllm_tool_def **ppTools,
    size_t *piToolCount
)
{
    xwork_tool_record *pCursor;
    xllm_tool_def *pTools;
    size_t iCount = 0u;
    size_t i = 0u;

    if ( !pRuntime || !ppTools || !piToolCount ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    *ppTools = NULL;
    *piToolCount = 0u;

    for ( pCursor = pRuntime->pTools; pCursor; pCursor = pCursor->pNext ) {
        ++iCount;
    }
    if ( iCount == 0u ) {
        return XWORK_OK;
    }

    pTools = (xllm_tool_def *)calloc(iCount, sizeof(*pTools));
    if ( !pTools ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    for ( pCursor = pRuntime->pTools; pCursor; pCursor = pCursor->pNext, ++i ) {
        pTools[i].sToolId = pCursor->tDef.sToolId;
        pTools[i].sWireName = pCursor->tDef.sToolId;
        pTools[i].sDescription = pCursor->tDef.sDescription;
        pTools[i].eKind = XLLM_TOOL_CLIENT;
    }

    *ppTools = pTools;
    *piToolCount = iCount;
    return XWORK_OK;
}

static const char *xwork__tool_call_id(const xllm_output_tool_call *pToolCall)
{
    if ( !pToolCall ) {
        return NULL;
    }
    if ( pToolCall->sToolId && pToolCall->sToolId[0] ) {
        return pToolCall->sToolId;
    }
    return pToolCall->sToolName;
}

static const char *xwork__run_session_profile_id(const xwork_run *pRun)
{
    return pRun ? pRun->sLlmProfileId : NULL;
}

void xwork__run_discard_session(xwork_run *pRun)
{
    if ( !pRun ) {
        return;
    }
    if ( pRun->pSession ) {
        xllm_session_destroy(pRun->pSession);
        pRun->pSession = NULL;
    }
}

void xwork__run_reset_session_state(xwork_run *pRun)
{
    if ( !pRun ) {
        return;
    }

    xwork__run_discard_session(pRun);
    xwork__free_cstr(&pRun->sSessionStateData);
}

typedef struct {
    size_t iHistoryCount;
    uint32 uCommittedTurns;
    uint32 uSummaryTurns;
    bool bHasSessionSummary;
} xwork__session_compaction_metrics;

static xllm_compact_strategy xwork__to_xllm_compact_strategy(
    xwork_session_compact_strategy eStrategy
)
{
    switch ( eStrategy ) {
        case XWORK_SESSION_COMPACT_TRUNCATE:
            return XLLM_COMPACT_TRUNCATE;
        case XWORK_SESSION_COMPACT_SUMMARIZE:
            return XLLM_COMPACT_SUMMARIZE;
        case XWORK_SESSION_COMPACT_CUSTOM:
            return XLLM_COMPACT_CUSTOM;
        default:
            return XLLM_COMPACT_SUMMARIZE;
    }
}

static uint32 xwork__size_to_u32_saturating(size_t iValue)
{
    return iValue > 0xFFFFFFFFu ? 0xFFFFFFFFu : (uint32)iValue;
}

static xwork_status xwork__session_get_compaction_metrics(
    xllm_session *pSession,
    xwork__session_compaction_metrics *pMetrics
)
{
    xllm_session_state *pState = NULL;
    xvalue tStateValue = NULL;
    xvalue tHistoryValue = NULL;
    const char *sSessionSummary;

    if ( !pSession || !pMetrics ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    memset(pMetrics, 0, sizeof(*pMetrics));
    if ( xllm_session_export_state(pSession, &pState) != XRT_NET_OK || !pState ) {
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }

    if ( xllm_session_state_to_xvalue(pState, &tStateValue) != XRT_NET_OK || !tStateValue ) {
        xllm_session_state_free(pState);
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }

    tHistoryValue = xvoTableGetValue(tStateValue, (str)"history", 0u);
    if ( tHistoryValue && xvoType(tHistoryValue) == XVO_DT_ARRAY ) {
        pMetrics->iHistoryCount = (size_t)xvoArrayItemCount(tHistoryValue);
    }
    pMetrics->uCommittedTurns =
        (uint32)xvoTableGetInt(tStateValue, (str)"committed_turns", 0u);
    pMetrics->uSummaryTurns =
        (uint32)xvoTableGetInt(tStateValue, (str)"summary_turns", 0u);
    sSessionSummary = (const char *)xvoTableGetText(tStateValue, (str)"session_summary", 0u);
    pMetrics->bHasSessionSummary = sSessionSummary && sSessionSummary[0];

    xvoUnref(tStateValue);
    xllm_session_state_free(pState);
    return XWORK_OK;
}

static bool xwork__session_compacted_between(
    const xwork__session_compaction_metrics *pBefore,
    const xwork__session_compaction_metrics *pAfter
)
{
    uint32 uExpectedCommittedTurns;

    if ( !pBefore || !pAfter ) {
        return false;
    }

    if ( pAfter->uSummaryTurns > pBefore->uSummaryTurns ) {
        return true;
    }
    if ( !pBefore->bHasSessionSummary && pAfter->bHasSessionSummary ) {
        return true;
    }

    uExpectedCommittedTurns = pBefore->uCommittedTurns < 0xFFFFFFFFu
        ? pBefore->uCommittedTurns + 1u
        : pBefore->uCommittedTurns;
    if ( pAfter->uCommittedTurns < uExpectedCommittedTurns ) {
        return true;
    }
    if ( pAfter->iHistoryCount < pBefore->iHistoryCount ) {
        return true;
    }

    return false;
}

static bool xwork__session_policy_should_compact_now(
    const xwork_session_policy *pPolicy,
    const xwork__session_compaction_metrics *pMetrics
)
{
    if ( !pPolicy || !pMetrics || !pPolicy->bEnableAutoCompact ) {
        return false;
    }
    if ( pPolicy->iCompactTriggerTurns != 0u &&
         pMetrics->uCommittedTurns < pPolicy->iCompactTriggerTurns ) {
        return false;
    }
    if ( pPolicy->iKeepRecentTurns != 0u &&
         pMetrics->uCommittedTurns <= pPolicy->iKeepRecentTurns ) {
        return false;
    }
    return pMetrics->uCommittedTurns > 0u && pMetrics->iHistoryCount > 0u;
}

static xwork_status xwork__serialize_session_state(
    xllm_session *pSession,
    char **psStateData
)
{
    xllm_session_state *pState = NULL;
    xvalue tStateValue = NULL;
    str sStateData = NULL;
    char *sStateCopy = NULL;
    xwork_status iStatus = XWORK_ERROR_EXTERNAL_FAILURE;

    if ( !pSession || !psStateData ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    *psStateData = NULL;

    if ( xllm_session_export_state(pSession, &pState) != XRT_NET_OK || !pState ) {
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }
    if ( xllm_session_state_to_xvalue(pState, &tStateValue) != XRT_NET_OK || !tStateValue ) {
        goto cleanup;
    }

    sStateData = xrtStringifyXSON(tStateValue, 0, 0u, NULL);
    if ( !sStateData ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto cleanup;
    }

    sStateCopy = xwork__dup_cstr((const char *)sStateData);
    if ( !sStateCopy ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto cleanup;
    }

    *psStateData = sStateCopy;
    sStateCopy = NULL;
    iStatus = XWORK_OK;

cleanup:
    free(sStateCopy);
    if ( sStateData ) {
        xrtFree(sStateData);
    }
    if ( tStateValue ) {
        xvoUnref(tStateValue);
    }
    if ( pState ) {
        xllm_session_state_free(pState);
    }
    return iStatus;
}

xwork_status xwork__run_refresh_session_state(xwork_run *pRun)
{
    char *sStateData = NULL;
    xwork_status iStatus;

    if ( !pRun ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !pRun->pSession ) {
        return XWORK_OK;
    }

    iStatus = xwork__serialize_session_state(pRun->pSession, &sStateData);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    iStatus = xwork__replace_cstr(&pRun->sSessionStateData, sStateData);
    free(sStateData);
    return iStatus;
}

static xwork_status xwork__import_session_state(
    xwork_runtime *pRuntime,
    const char *sStateData,
    xllm_session **ppSession
)
{
    xvalue tStateValue = NULL;
    xllm_session_state *pState = NULL;
    xllm_session *pSession = NULL;
    xwork_status iStatus = XWORK_ERROR_EXTERNAL_FAILURE;

    if ( !pRuntime || !pRuntime->pLlmRuntime || !sStateData || !sStateData[0] || !ppSession ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    *ppSession = NULL;
    tStateValue = xrtParseXSON((str)sStateData, strlen(sStateData));
    if ( !tStateValue || xvoType(tStateValue) != XVO_DT_TABLE ) {
        goto cleanup;
    }
    if ( xllm_session_state_from_xvalue(tStateValue, &pState) != XRT_NET_OK || !pState ) {
        goto cleanup;
    }
    if ( xllm_session_import_state(pRuntime->pLlmRuntime, pState, &pSession) != XRT_NET_OK ||
         !pSession ) {
        goto cleanup;
    }

    *ppSession = pSession;
    pSession = NULL;
    iStatus = XWORK_OK;

cleanup:
    if ( pSession ) {
        xllm_session_destroy(pSession);
    }
    if ( pState ) {
        xllm_session_state_free(pState);
    }
    if ( tStateValue ) {
        xvoUnref(tStateValue);
    }
    return iStatus;
}

xwork_status xwork__run_ensure_session(xwork_run *pRun)
{
    const char *sProfileId;
    xllm_session_options tOptions;
    xllm_session *pSession = NULL;

    if ( !pRun || !pRun->pRuntime || !pRun->pRuntime->pLlmRuntime ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( pRun->pSession ) {
        return XWORK_OK;
    }

    if ( pRun->sSessionStateData && pRun->sSessionStateData[0] ) {
        return xwork__import_session_state(
            pRun->pRuntime,
            pRun->sSessionStateData,
            &pRun->pSession
        );
    }

    sProfileId = xwork__run_session_profile_id(pRun);
    if ( !sProfileId || !sProfileId[0] ) {
        return XWORK_ERROR_UNSUPPORTED;
    }

    xllm_session_options_init(&tOptions);
    tOptions.sProfileId = sProfileId;
    tOptions.bEnableAutoCompact = false;
    tOptions.fCompactTriggerRatio = pRun->tSessionPolicy.fCompactTriggerRatio;
    tOptions.uCompactTriggerTurns =
        xwork__size_to_u32_saturating(pRun->tSessionPolicy.iCompactTriggerTurns);
    tOptions.uReserveOutputTokens =
        xwork__size_to_u32_saturating(pRun->tSessionPolicy.iReserveOutputTokens);
    tOptions.uKeepRecentTurns =
        xwork__size_to_u32_saturating(pRun->tSessionPolicy.iKeepRecentTurns);
    tOptions.bKeepActiveToolChain = pRun->tSessionPolicy.bKeepActiveToolChain;
    tOptions.eCompactStrategy =
        xwork__to_xllm_compact_strategy(pRun->tSessionPolicy.eCompactStrategy);
    if ( xllm_session_create(pRun->pRuntime->pLlmRuntime, &tOptions, &pSession) != XRT_NET_OK ||
         !pSession ) {
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }

    pRun->pSession = pSession;
    return XWORK_OK;
}

static bool xwork__run_has_memory_enabled_workspace(const xwork_run *pRun)
{
    size_t i;

    if ( !pRun || !pRun->pRuntime ) {
        return false;
    }

    for ( i = 0u; i < pRun->iWorkspaceCount; ++i ) {
        const char *sWorkspaceId = pRun->psWorkspaceIds ? pRun->psWorkspaceIds[i] : NULL;
        const xwork_workspace *pWorkspace;

        if ( !sWorkspaceId ) {
            continue;
        }
        pWorkspace = xwork_runtime_find_workspace(pRun->pRuntime, sWorkspaceId);
        if ( pWorkspace && xwork_workspace_is_memory_enabled(pWorkspace) ) {
            return true;
        }
    }

    return false;
}

static bool xwork__workspace_has_searchable_memory(const xwork_workspace *pWorkspace)
{
    return pWorkspace &&
           xwork_workspace_is_memory_enabled(pWorkspace) &&
           xwork_workspace_get_memory(pWorkspace) != NULL;
}

static const char *xwork__context_block_first_text(const xllm_context_block *pBlock)
{
    const xllm_message *pMessage;

    if ( !pBlock || !pBlock->pMessages || pBlock->iMessageCount == 0u ) {
        return NULL;
    }

    pMessage = &pBlock->pMessages[0];
    if ( pMessage->eRole != XLLM_ROLE_SYSTEM ) {
        return NULL;
    }
    if ( !pMessage->pParts || pMessage->iPartCount == 0u ) {
        return NULL;
    }
    if ( pMessage->pParts[0].eKind != XLLM_PART_TEXT ||
         pMessage->pParts[0].as.tSource.eKind != XLLM_SOURCE_INLINE_TEXT ) {
        return NULL;
    }
    return pMessage->pParts[0].as.tSource.as.sText;
}

static xwork_status xwork__append_memory_context_text(
    xwork_memory_context *pContext,
    const char *sText
)
{
    char *sCombined;

    if ( !pContext ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !sText || !sText[0] ) {
        return XWORK_OK;
    }
    if ( !pContext->sText || !pContext->sText[0] ) {
        return xwork__replace_cstr((char **)&pContext->sText, sText);
    }

    sCombined = xwork__dup_printf("%s\n\n%s", pContext->sText, sText);
    if ( !sCombined ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    xwork__free_cstr((char **)&pContext->sText);
    pContext->sText = sCombined;
    return XWORK_OK;
}

static xwork_status xwork__apply_workspace_memory_context(
    const xwork_run *pRun,
    const xwork_orchestrator_options *pOptions,
    xllm_request *pRequest,
    xwork_memory_context *pContext,
    xllm_error *pError
)
{
    size_t i;
    size_t iWorkspaceCount = 0u;

    if ( !pRun || !pRun->pRuntime || !pRequest || !pContext ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_memory_context_reset(pContext);

    for ( i = 0u; i < pRun->iWorkspaceCount; ++i ) {
        const char *sWorkspaceId = pRun->psWorkspaceIds ? pRun->psWorkspaceIds[i] : NULL;
        const xwork_workspace *pWorkspace;
        xllm_memory *pMemory;
        xllm_memory_search_options tSearchOptions;
        xllm_memory_search_result tSearchResult;
        xllm_memory_context_options tContextOptions;
        char sLabel[128];
        size_t iExistingBlockCount;
        const char *sBlockText;
        xwork_status iStatus;

        if ( !sWorkspaceId ) {
            continue;
        }

        pWorkspace = xwork_runtime_find_workspace(pRun->pRuntime, sWorkspaceId);
        if ( !xwork__workspace_has_searchable_memory(pWorkspace) ) {
            continue;
        }
        if ( pOptions &&
             pOptions->iMemoryContextMaxBlocks > 0u &&
             iWorkspaceCount >= pOptions->iMemoryContextMaxBlocks ) {
            break;
        }

        pMemory = xwork_workspace_get_memory(pWorkspace);
        xllm_memory_search_options_init(&tSearchOptions);
        memset(&tSearchResult, 0, sizeof(tSearchResult));
        xllm_memory_context_options_init(&tContextOptions);

        tSearchOptions.sQuery = pRun->sInstruction;
        if ( pOptions && pOptions->iMemorySearchMaxHits > 0u ) {
            tSearchOptions.uMaxHits = (uint32)pOptions->iMemorySearchMaxHits;
        }
        tContextOptions.sLabel = sLabel;
        if ( pOptions ) {
            tContextOptions.iPriority = (int32)pOptions->iMemoryContextPriority;
            tContextOptions.bPinned = pOptions->bMemoryContextPinned;
            if ( pOptions->iMemoryContextMaxCharsPerHit > 0u ) {
                tContextOptions.uMaxCharsPerHit =
                    (uint32)pOptions->iMemoryContextMaxCharsPerHit;
            }
            if ( pOptions->iMemoryContextMaxTotalChars > 0u ) {
                tContextOptions.uMaxTotalChars =
                    (uint32)pOptions->iMemoryContextMaxTotalChars;
            }
        }
        (void)snprintf(sLabel, sizeof(sLabel), "Workspace memory: %s", sWorkspaceId);

        iExistingBlockCount = pRequest->iContextBlockCount;
        if ( xllm_memory_search(pMemory, &tSearchOptions, &tSearchResult, pError) != XRT_NET_OK ) {
            xllm_memory_search_result_reset(&tSearchResult);
            return XWORK_ERROR_EXTERNAL_FAILURE;
        }
        if ( tSearchResult.iHitCount == 0u ) {
            xllm_memory_search_result_reset(&tSearchResult);
            continue;
        }

        if ( xllm_memory_apply_search_to_request(
                pRequest,
                &tSearchResult,
                &tContextOptions,
                pError
             ) != XRT_NET_OK ) {
            xllm_memory_search_result_reset(&tSearchResult);
            return XWORK_ERROR_EXTERNAL_FAILURE;
        }
        xllm_memory_search_result_reset(&tSearchResult);

        if ( pRequest->iContextBlockCount <= iExistingBlockCount ) {
            return XWORK_ERROR_EXTERNAL_FAILURE;
        }

        sBlockText = xwork__context_block_first_text(
            &pRequest->pContextBlocks[pRequest->iContextBlockCount - 1u]
        );
        iStatus = xwork__append_memory_context_text(pContext, sBlockText);
        if ( iStatus != XWORK_OK ) {
            return iStatus;
        }

        ++iWorkspaceCount;
    }

    pContext->iWorkspaceCount = iWorkspaceCount;
    return XWORK_OK;
}

static xwork_status xwork__ingest_tool_result_to_memory(
    xwork_run *pRun,
    const xwork_tool_call *pToolCall,
    const xwork_tool_result *pToolResult
)
{
    size_t i;

    if ( !pRun || !pRun->pRuntime || !pToolCall || !pToolResult ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    for ( i = 0u; i < pRun->iWorkspaceCount; ++i ) {
        const char *sWorkspaceId = pRun->psWorkspaceIds ? pRun->psWorkspaceIds[i] : NULL;
        const xwork_workspace *pWorkspace;
        xllm_memory *pMemory;
        xllm_memory_ingest_options tIngestOptions;
        xllm_error tError;
        char *sRecordId = NULL;
        char *sSourceUri = NULL;
        char *sTitle = NULL;
        char *sText = NULL;
        char *sSummary = NULL;
        const char *sToolId;
        xwork_status iStatus;

        if ( !sWorkspaceId ) {
            continue;
        }

        pWorkspace = xwork_runtime_find_workspace(pRun->pRuntime, sWorkspaceId);
        if ( !xwork__workspace_has_searchable_memory(pWorkspace) ) {
            continue;
        }

        sToolId = (pToolCall->sToolId && pToolCall->sToolId[0]) ? pToolCall->sToolId : "tool";
        if ( pToolResult->sVisibleSummary && pToolResult->sVisibleSummary[0] &&
             pToolResult->sOutputText && pToolResult->sOutputText[0] ) {
            sText = xwork__dup_printf(
                "Tool: %s\nSummary: %s\nOutput:\n%s",
                sToolId,
                pToolResult->sVisibleSummary,
                pToolResult->sOutputText
            );
        } else if ( pToolResult->sVisibleSummary && pToolResult->sVisibleSummary[0] ) {
            sText = xwork__dup_printf("Tool: %s\nSummary: %s", sToolId, pToolResult->sVisibleSummary);
        } else if ( pToolResult->sOutputText && pToolResult->sOutputText[0] ) {
            sText = xwork__dup_printf("Tool: %s\nOutput:\n%s", sToolId, pToolResult->sOutputText);
        }
        if ( !sText || !sText[0] ) {
            free(sText);
            continue;
        }

        sRecordId = xwork__dup_printf(
            "run:%s:workspace:%s:tool:%s",
            pRun->sRunId ? pRun->sRunId : "run",
            sWorkspaceId,
            pToolCall->sCallId ? pToolCall->sCallId : sToolId
        );
        sSourceUri = xwork__dup_printf(
            "xwork://run/%s/workspace/%s/tool/%s",
            pRun->sRunId ? pRun->sRunId : "run",
            sWorkspaceId,
            pToolCall->sCallId ? pToolCall->sCallId : sToolId
        );
        sTitle = xwork__dup_printf("Tool result: %s", sToolId);
        if ( !sRecordId || !sSourceUri || !sTitle ) {
            free(sTitle);
            free(sSourceUri);
            free(sRecordId);
            free(sText);
            return XWORK_ERROR_NO_MEMORY;
        }

        xllm_memory_ingest_options_init(&tIngestOptions);
        xllm_error_init(&tError);
        pMemory = xwork_workspace_get_memory(pWorkspace);
        tIngestOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
        tIngestOptions.sRecordId = sRecordId;
        tIngestOptions.sTitle = sTitle;
        tIngestOptions.sSourceUri = sSourceUri;
        tIngestOptions.sText = sText;
        tIngestOptions.bReplaceExisting = true;

        if ( xllm_memory_ingest_text(pMemory, &tIngestOptions, &tError) != XRT_NET_OK ) {
            xllm_error_free(&tError);
            free(sTitle);
            free(sSourceUri);
            free(sRecordId);
            free(sText);
            return XWORK_ERROR_EXTERNAL_FAILURE;
        }
        xllm_error_free(&tError);

        sSummary = xwork__dup_printf(
            "Tool result ingested into workspace memory: %s",
            sWorkspaceId
        );
        iStatus = xwork__run_record_event(
            pRun,
            XWORK_EVENT_MEMORY_RECORD_INGESTED,
            pRun->sLastToolId,
            pRun->sLastApprovalRequestId,
            pRun->sLastCheckpointId,
            sSummary ? sSummary : "Tool result ingested into workspace memory."
        );
        free(sSummary);
        free(sTitle);
        free(sSourceUri);
        free(sRecordId);
        free(sText);
        if ( iStatus != XWORK_OK ) {
            return iStatus;
        }
    }

    return XWORK_OK;
}

static const char *xwork__artifact_kind_name(xwork_artifact_kind eKind)
{
    switch ( eKind ) {
        case XWORK_ARTIFACT_PATCH:
            return "patch";
        case XWORK_ARTIFACT_REPORT:
            return "report";
        case XWORK_ARTIFACT_COMMAND:
            return "command";
        case XWORK_ARTIFACT_OUTPUT:
        default:
            return "output";
    }
}

static const char *xwork__artifact_output_class_name(xwork_artifact_output_class eClass)
{
    switch ( eClass ) {
        case XWORK_ARTIFACT_OUTPUT_TEXT:
            return "text";
        case XWORK_ARTIFACT_OUTPUT_JSON:
            return "json";
        case XWORK_ARTIFACT_OUTPUT_FILE_CONTENT:
            return "file_content";
        case XWORK_ARTIFACT_OUTPUT_FILE_CHANGE:
            return "file_change";
        case XWORK_ARTIFACT_OUTPUT_TERMINAL_STATE:
            return "terminal_state";
        case XWORK_ARTIFACT_OUTPUT_TERMINAL_INVENTORY:
            return "terminal_inventory";
        case XWORK_ARTIFACT_OUTPUT_UNSPECIFIED:
        default:
            return "unspecified";
    }
}

static const char *xwork__artifact_report_class_name(xwork_artifact_report_class eClass)
{
    switch ( eClass ) {
        case XWORK_ARTIFACT_REPORT_DOCUMENT:
            return "document";
        case XWORK_ARTIFACT_REPORT_SUMMARY:
            return "summary";
        case XWORK_ARTIFACT_REPORT_PLAN:
            return "plan";
        case XWORK_ARTIFACT_REPORT_REVIEW:
            return "review";
        case XWORK_ARTIFACT_REPORT_DIAGNOSTICS:
            return "diagnostics";
        case XWORK_ARTIFACT_REPORT_PROGRESS:
            return "progress";
        case XWORK_ARTIFACT_REPORT_FINAL:
            return "final";
        case XWORK_ARTIFACT_REPORT_UNSPECIFIED:
        default:
            return "unspecified";
    }
}

static bool xwork__mask_allows_value(unsigned int uMask, int iValue)
{
    if ( uMask == 0u ) {
        return true;
    }
    if ( iValue < 0 || iValue >= 32 ) {
        return false;
    }
    return (uMask & (1u << (unsigned int)iValue)) != 0u;
}

static bool xwork__ascii_contains_ci(const char *sText, const char *sPattern);

static bool xwork__artifact_text_looks_sensitive(const char *sText)
{
    static const char *const psSensitivePatterns[] = {
        "password",
        "passwd",
        "secret",
        "api_key",
        "apikey",
        "access_token",
        "refresh_token",
        "private key",
        "authorization:",
        "bearer ",
        "x-api-key",
        "credential",
        ".env"
    };
    size_t i;

    if ( !sText || !sText[0] ) {
        return false;
    }

    for ( i = 0u; i < sizeof(psSensitivePatterns) / sizeof(psSensitivePatterns[0]); ++i ) {
        if ( xwork__ascii_contains_ci(sText, psSensitivePatterns[i]) ) {
            return true;
        }
    }
    return false;
}

static bool xwork__artifact_looks_sensitive(const xwork_artifact *pArtifact)
{
    if ( !pArtifact ) {
        return false;
    }

    return xwork__artifact_text_looks_sensitive(pArtifact->sName) ||
        xwork__artifact_text_looks_sensitive(pArtifact->sMimeType) ||
        xwork__artifact_text_looks_sensitive(pArtifact->sStorageRef) ||
        xwork__artifact_text_looks_sensitive(pArtifact->sSummary) ||
        xwork__artifact_text_looks_sensitive(pArtifact->sContentText) ||
        xwork__artifact_text_looks_sensitive(pArtifact->sPatchApplyResultJson) ||
        xwork__artifact_text_looks_sensitive(pArtifact->sPatchFileSummaryJson) ||
        xwork__artifact_text_looks_sensitive(pArtifact->sCommandText);
}

static bool xwork__artifact_memory_ingest_policy_allows(
    const xwork_artifact *pArtifact,
    const xwork_orchestrator_options *pOptions
)
{
    if ( !pArtifact ) {
        return false;
    }
    if ( !pOptions ) {
        return !xwork__artifact_looks_sensitive(pArtifact);
    }

    if ( !xwork__mask_allows_value(
            pOptions->uArtifactMemoryIngestKindMask,
            (int)pArtifact->eKind
         ) ) {
        return false;
    }
    if ( pOptions->uArtifactMemoryIngestOutputClassMask != 0u &&
         !xwork__mask_allows_value(
             pOptions->uArtifactMemoryIngestOutputClassMask,
             (int)pArtifact->eOutputClass
         ) ) {
        return false;
    }
    if ( pOptions->uArtifactMemoryIngestReportClassMask != 0u &&
         !xwork__mask_allows_value(
             pOptions->uArtifactMemoryIngestReportClassMask,
             (int)pArtifact->eReportClass
         ) ) {
        return false;
    }
    if ( !pOptions->bIngestSensitiveArtifactsToMemory &&
         xwork__artifact_looks_sensitive(pArtifact) ) {
        return false;
    }

    return true;
}

static char *xwork__json_escape_string(const char *sText)
{
    size_t i;
    size_t iLength;
    size_t iOutLength = 0u;
    char *sEscaped;
    char *pOut;

    if ( !sText ) {
        sText = "";
    }
    iLength = strlen(sText);
    for ( i = 0u; i < iLength; ++i ) {
        unsigned char ch = (unsigned char)sText[i];
        switch ( ch ) {
            case '"':
            case '\\':
            case '\b':
            case '\f':
            case '\n':
            case '\r':
            case '\t':
                iOutLength += 2u;
                break;
            default:
                if ( ch < 0x20u ) {
                    iOutLength += 6u;
                } else {
                    iOutLength += 1u;
                }
                break;
        }
    }

    sEscaped = (char *)malloc(iOutLength + 1u);
    if ( !sEscaped ) {
        return NULL;
    }
    pOut = sEscaped;
    for ( i = 0u; i < iLength; ++i ) {
        unsigned char ch = (unsigned char)sText[i];
        switch ( ch ) {
            case '"': *pOut++ = '\\'; *pOut++ = '"'; break;
            case '\\': *pOut++ = '\\'; *pOut++ = '\\'; break;
            case '\b': *pOut++ = '\\'; *pOut++ = 'b'; break;
            case '\f': *pOut++ = '\\'; *pOut++ = 'f'; break;
            case '\n': *pOut++ = '\\'; *pOut++ = 'n'; break;
            case '\r': *pOut++ = '\\'; *pOut++ = 'r'; break;
            case '\t': *pOut++ = '\\'; *pOut++ = 't'; break;
            default:
                if ( ch < 0x20u ) {
                    sprintf(pOut, "\\u%04x", (unsigned int)ch);
                    pOut += 6u;
                } else {
                    *pOut++ = (char)ch;
                }
                break;
        }
    }
    *pOut = '\0';
    return sEscaped;
}

static bool xwork__command_looks_like_build_or_test(const char *sCommand)
{
    if ( !sCommand || !sCommand[0] ) {
        return false;
    }
    return strstr(sCommand, "test") != NULL ||
        strstr(sCommand, "build") != NULL ||
        strstr(sCommand, "make") != NULL ||
        strstr(sCommand, "gcc") != NULL ||
        strstr(sCommand, "clang") != NULL ||
        strstr(sCommand, "cmake") != NULL ||
        strstr(sCommand, "ctest") != NULL ||
        strstr(sCommand, "pytest") != NULL ||
        strstr(sCommand, "npm run") != NULL;
}

static xwork_status xwork__emit_process_diagnostics_artifact(
    xwork_run *pRun,
    const char *sCommand,
    const char *sCwd,
    const char *sStdout,
    const char *sStderr,
    bool bHasExitCode,
    int iExitCode,
    bool bStdoutTruncated,
    bool bStderrTruncated
)
{
    xwork_report_artifact_options tOptions;
    char *sEscapedCommand = NULL;
    char *sEscapedCwd = NULL;
    char *sEscapedMessage = NULL;
    char *sReportText = NULL;
    const char *sMessage;
    const char *sSeverity;
    const char *sStatus;
    bool bBuildOrTest;
    bool bHasStderr;
    bool bShouldEmit;
    size_t iDiagnosticCount;
    xwork_status iStatus;

    if ( !pRun || !sCommand ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    bBuildOrTest = xwork__command_looks_like_build_or_test(sCommand);
    bHasStderr = sStderr && sStderr[0];
    bShouldEmit = bBuildOrTest || bHasStderr || (bHasExitCode && iExitCode != 0);
    if ( !bShouldEmit ) {
        return XWORK_OK;
    }

    sMessage = bHasStderr ? sStderr : (sStdout ? sStdout : "");
    sSeverity = (bHasExitCode && iExitCode != 0) ? "error" :
        (bHasStderr ? "warning" : "note");
    sStatus = (bHasExitCode && iExitCode != 0) ? "failed" : "passed";
    iDiagnosticCount = (bHasStderr || (bHasExitCode && iExitCode != 0)) ? 1u : 0u;

    sEscapedCommand = xwork__json_escape_string(sCommand);
    sEscapedCwd = xwork__json_escape_string(sCwd ? sCwd : "");
    sEscapedMessage = xwork__json_escape_string(sMessage);
    if ( !sEscapedCommand || !sEscapedCwd || !sEscapedMessage ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto cleanup;
    }

    sReportText = xwork__dup_printf(
        "{\"schema\":\"" XWORK_DIAGNOSTICS_SCHEMA_V1 "\","
        "\"source\":\"process.exec\",\"command\":\"%s\",\"cwd\":\"%s\","
        "\"status\":\"%s\",\"exit_code\":%d,\"has_exit_code\":%s,"
        "\"stdout_truncated\":%s,\"stderr_truncated\":%s,"
        "\"diagnostic_count\":%lu,\"diagnostics\":[%s"
        "{\"severity\":\"%s\",\"source\":\"process.exec\","
        "\"location\":\"\",\"message\":\"%s\"}%s]}",
        sEscapedCommand,
        sEscapedCwd,
        sStatus,
        iExitCode,
        bHasExitCode ? "true" : "false",
        bStdoutTruncated ? "true" : "false",
        bStderrTruncated ? "true" : "false",
        (unsigned long)iDiagnosticCount,
        iDiagnosticCount ? "" : "",
        sSeverity,
        sEscapedMessage,
        iDiagnosticCount ? "" : ""
    );
    if ( !sReportText ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto cleanup;
    }
    if ( iDiagnosticCount == 0u ) {
        free(sReportText);
        sReportText = xwork__dup_printf(
            "{\"schema\":\"" XWORK_DIAGNOSTICS_SCHEMA_V1 "\","
            "\"source\":\"process.exec\",\"command\":\"%s\",\"cwd\":\"%s\","
            "\"status\":\"%s\",\"exit_code\":%d,\"has_exit_code\":%s,"
            "\"stdout_truncated\":%s,\"stderr_truncated\":%s,"
            "\"diagnostic_count\":0,\"diagnostics\":[]}",
            sEscapedCommand,
            sEscapedCwd,
            sStatus,
            iExitCode,
            bHasExitCode ? "true" : "false",
            bStdoutTruncated ? "true" : "false",
            bStderrTruncated ? "true" : "false"
        );
        if ( !sReportText ) {
            iStatus = XWORK_ERROR_NO_MEMORY;
            goto cleanup;
        }
    }

    xwork_report_artifact_options_init(&tOptions);
    tOptions.sName = "process.diagnostics.json";
    tOptions.sMimeType = "application/json";
    tOptions.sStorageRef = sCwd;
    tOptions.sSummary = (bHasExitCode && iExitCode != 0)
        ? "process.exec diagnostics failed"
        : "process.exec diagnostics";
    tOptions.eReportClass = XWORK_ARTIFACT_REPORT_DIAGNOSTICS;
    tOptions.sReportSubjectRef = sCommand;
    tOptions.sReportText = sReportText;
    iStatus = xwork_run_emit_report_artifact(pRun, &tOptions, NULL);

cleanup:
    free(sEscapedCommand);
    free(sEscapedCwd);
    free(sEscapedMessage);
    free(sReportText);
    return iStatus;
}

static xwork_status xwork__ingest_artifact_to_memory(
    xwork_run *pRun,
    const xwork_artifact *pArtifact,
    const xwork_orchestrator_options *pOptions
)
{
    size_t i;

    if ( !pRun || !pRun->pRuntime || !pArtifact ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !xwork__artifact_memory_ingest_policy_allows(pArtifact, pOptions) ) {
        return XWORK_OK;
    }

    for ( i = 0u; i < pRun->iWorkspaceCount; ++i ) {
        const char *sWorkspaceId = pRun->psWorkspaceIds ? pRun->psWorkspaceIds[i] : NULL;
        const xwork_workspace *pWorkspace;
        xllm_memory *pMemory;
        xllm_memory_ingest_options tIngestOptions;
        xllm_error tError;
        char *sRecordId = NULL;
        char *sSourceUri = NULL;
        char *sTitle = NULL;
        char *sText = NULL;
        char *sSummary = NULL;
        xwork_status iStatus;
        const char *sArtifactId;
        const char *sArtifactName;

        if ( !sWorkspaceId ) {
            continue;
        }

        pWorkspace = xwork_runtime_find_workspace(pRun->pRuntime, sWorkspaceId);
        if ( !xwork__workspace_has_searchable_memory(pWorkspace) ) {
            continue;
        }

        sArtifactId = (pArtifact->sArtifactId && pArtifact->sArtifactId[0])
            ? pArtifact->sArtifactId
            : "artifact";
        sArtifactName = (pArtifact->sName && pArtifact->sName[0])
            ? pArtifact->sName
            : sArtifactId;

        if ( pArtifact->bHasContentStats && pArtifact->bHasCommandIoStats ) {
            sText = xwork__dup_printf(
                "Artifact kind: %s\nName: %s\nMime type: %s\nStorage ref: %s\n"
                "Output class: %s\nOutput role: %s\nReport class: %s\n"
                "Report subject ref: %s\nSummary: %s\n"
                "Content bytes: %lu\nContent lines: %lu\n"
                "Stdout bytes: %lu\nStderr bytes: %lu\n"
                "Stdout truncated: %s\nStderr truncated: %s",
                xwork__artifact_kind_name(pArtifact->eKind),
                sArtifactName,
                pArtifact->sMimeType ? pArtifact->sMimeType : "",
                pArtifact->sStorageRef ? pArtifact->sStorageRef : "",
                xwork__artifact_output_class_name(pArtifact->eOutputClass),
                pArtifact->sOutputRole ? pArtifact->sOutputRole : "",
                xwork__artifact_report_class_name(pArtifact->eReportClass),
                pArtifact->sReportSubjectRef ? pArtifact->sReportSubjectRef : "",
                pArtifact->sSummary ? pArtifact->sSummary : "",
                (unsigned long)pArtifact->iContentByteCount,
                (unsigned long)pArtifact->iContentLineCount,
                (unsigned long)pArtifact->iStdoutByteCount,
                (unsigned long)pArtifact->iStderrByteCount,
                pArtifact->bStdoutTruncated ? "true" : "false",
                pArtifact->bStderrTruncated ? "true" : "false"
            );
        } else if ( pArtifact->bHasContentStats && pArtifact->bHasPatchStats ) {
            sText = xwork__dup_printf(
                "Artifact kind: %s\nName: %s\nMime type: %s\nStorage ref: %s\n"
                "Output class: %s\nOutput role: %s\nReport class: %s\n"
                "Report subject ref: %s\nSummary: %s\n"
                "Content bytes: %lu\nContent lines: %lu\n"
                "Patch files: %lu\nPatch hunks: %lu\n"
                "Patch added lines: %lu\nPatch deleted lines: %lu\n"
                "Patch apply result: %s\nPatch file summary: %s",
                xwork__artifact_kind_name(pArtifact->eKind),
                sArtifactName,
                pArtifact->sMimeType ? pArtifact->sMimeType : "",
                pArtifact->sStorageRef ? pArtifact->sStorageRef : "",
                xwork__artifact_output_class_name(pArtifact->eOutputClass),
                pArtifact->sOutputRole ? pArtifact->sOutputRole : "",
                xwork__artifact_report_class_name(pArtifact->eReportClass),
                pArtifact->sReportSubjectRef ? pArtifact->sReportSubjectRef : "",
                pArtifact->sSummary ? pArtifact->sSummary : "",
                (unsigned long)pArtifact->iContentByteCount,
                (unsigned long)pArtifact->iContentLineCount,
                (unsigned long)pArtifact->iPatchFileCount,
                (unsigned long)pArtifact->iPatchHunkCount,
                (unsigned long)pArtifact->iPatchAddedLineCount,
                (unsigned long)pArtifact->iPatchDeletedLineCount,
                pArtifact->sPatchApplyResultJson ? pArtifact->sPatchApplyResultJson : "",
                pArtifact->sPatchFileSummaryJson ? pArtifact->sPatchFileSummaryJson : ""
            );
        } else if ( pArtifact->bHasContentStats ) {
            sText = xwork__dup_printf(
                "Artifact kind: %s\nName: %s\nMime type: %s\nStorage ref: %s\n"
                "Output class: %s\nOutput role: %s\nReport class: %s\n"
                "Report subject ref: %s\nSummary: %s\n"
                "Content bytes: %lu\nContent lines: %lu",
                xwork__artifact_kind_name(pArtifact->eKind),
                sArtifactName,
                pArtifact->sMimeType ? pArtifact->sMimeType : "",
                pArtifact->sStorageRef ? pArtifact->sStorageRef : "",
                xwork__artifact_output_class_name(pArtifact->eOutputClass),
                pArtifact->sOutputRole ? pArtifact->sOutputRole : "",
                xwork__artifact_report_class_name(pArtifact->eReportClass),
                pArtifact->sReportSubjectRef ? pArtifact->sReportSubjectRef : "",
                pArtifact->sSummary ? pArtifact->sSummary : "",
                (unsigned long)pArtifact->iContentByteCount,
                (unsigned long)pArtifact->iContentLineCount
            );
        } else if ( pArtifact->bHasPatchStats ) {
            sText = xwork__dup_printf(
                "Artifact kind: %s\nName: %s\nMime type: %s\nStorage ref: %s\n"
                "Output class: %s\nOutput role: %s\nReport class: %s\n"
                "Report subject ref: %s\nSummary: %s\n"
                "Patch files: %lu\nPatch hunks: %lu\n"
                "Patch added lines: %lu\nPatch deleted lines: %lu\n"
                "Patch apply result: %s\nPatch file summary: %s",
                xwork__artifact_kind_name(pArtifact->eKind),
                sArtifactName,
                pArtifact->sMimeType ? pArtifact->sMimeType : "",
                pArtifact->sStorageRef ? pArtifact->sStorageRef : "",
                xwork__artifact_output_class_name(pArtifact->eOutputClass),
                pArtifact->sOutputRole ? pArtifact->sOutputRole : "",
                xwork__artifact_report_class_name(pArtifact->eReportClass),
                pArtifact->sReportSubjectRef ? pArtifact->sReportSubjectRef : "",
                pArtifact->sSummary ? pArtifact->sSummary : "",
                (unsigned long)pArtifact->iPatchFileCount,
                (unsigned long)pArtifact->iPatchHunkCount,
                (unsigned long)pArtifact->iPatchAddedLineCount,
                (unsigned long)pArtifact->iPatchDeletedLineCount,
                pArtifact->sPatchApplyResultJson ? pArtifact->sPatchApplyResultJson : "",
                pArtifact->sPatchFileSummaryJson ? pArtifact->sPatchFileSummaryJson : ""
            );
        } else {
            sText = xwork__dup_printf(
                "Artifact kind: %s\nName: %s\nMime type: %s\nStorage ref: %s\n"
                "Output class: %s\nOutput role: %s\nReport class: %s\n"
                "Report subject ref: %s\nSummary: %s",
                xwork__artifact_kind_name(pArtifact->eKind),
                sArtifactName,
                pArtifact->sMimeType ? pArtifact->sMimeType : "",
                pArtifact->sStorageRef ? pArtifact->sStorageRef : "",
                xwork__artifact_output_class_name(pArtifact->eOutputClass),
                pArtifact->sOutputRole ? pArtifact->sOutputRole : "",
                xwork__artifact_report_class_name(pArtifact->eReportClass),
                pArtifact->sReportSubjectRef ? pArtifact->sReportSubjectRef : "",
                pArtifact->sSummary ? pArtifact->sSummary : ""
            );
        }
        if ( !sText ) {
            return XWORK_ERROR_NO_MEMORY;
        }

        sRecordId = xwork__dup_printf(
            "run:%s:workspace:%s:artifact:%s",
            pRun->sRunId ? pRun->sRunId : "run",
            sWorkspaceId,
            sArtifactId
        );
        sSourceUri = xwork__dup_printf(
            "xwork://run/%s/workspace/%s/artifact/%s",
            pRun->sRunId ? pRun->sRunId : "run",
            sWorkspaceId,
            sArtifactId
        );
        sTitle = xwork__dup_printf("Artifact: %s", sArtifactName);
        if ( !sRecordId || !sSourceUri || !sTitle ) {
            free(sTitle);
            free(sSourceUri);
            free(sRecordId);
            free(sText);
            return XWORK_ERROR_NO_MEMORY;
        }

        xllm_memory_ingest_options_init(&tIngestOptions);
        xllm_error_init(&tError);
        pMemory = xwork_workspace_get_memory(pWorkspace);
        tIngestOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
        tIngestOptions.sRecordId = sRecordId;
        tIngestOptions.sTitle = sTitle;
        tIngestOptions.sSourceUri = sSourceUri;
        tIngestOptions.sText = sText;
        tIngestOptions.bReplaceExisting = true;

        if ( xllm_memory_ingest_text(pMemory, &tIngestOptions, &tError) != XRT_NET_OK ) {
            xllm_error_free(&tError);
            free(sTitle);
            free(sSourceUri);
            free(sRecordId);
            free(sText);
            return XWORK_ERROR_EXTERNAL_FAILURE;
        }
        xllm_error_free(&tError);

        sSummary = xwork__dup_printf(
            "Artifact ingested into workspace memory: %s",
            sWorkspaceId
        );
        iStatus = xwork__run_record_event(
            pRun,
            XWORK_EVENT_MEMORY_RECORD_INGESTED,
            pRun->sLastToolId,
            pRun->sLastApprovalRequestId,
            pRun->sLastCheckpointId,
            sSummary ? sSummary : "Artifact ingested into workspace memory."
        );
        free(sSummary);
        free(sTitle);
        free(sSourceUri);
        free(sRecordId);
        free(sText);
        if ( iStatus != XWORK_OK ) {
            return iStatus;
        }
    }

    return XWORK_OK;
}

static xwork_status xwork__ingest_artifacts_to_memory_range(
    xwork_run *pRun,
    size_t iStartIndex,
    const xwork_orchestrator_options *pOptions
)
{
    size_t i;

    if ( !pRun ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    for ( i = iStartIndex; i < pRun->iArtifactCount; ++i ) {
        xwork_status iStatus = xwork__ingest_artifact_to_memory(
            pRun,
            &pRun->pArtifactLog[i].tArtifact,
            pOptions
        );
        if ( iStatus != XWORK_OK ) {
            return iStatus;
        }
    }

    return XWORK_OK;
}

static char xwork__ascii_lower(char c)
{
    if ( c >= 'A' && c <= 'Z' ) {
        return (char)(c - 'A' + 'a');
    }
    return c;
}

static bool xwork__ascii_contains_ci(const char *sText, const char *sPattern)
{
    size_t i;
    size_t j;
    size_t iTextLength;
    size_t iPatternLength;

    if ( !sText || !sPattern || !sPattern[0] ) {
        return false;
    }

    iTextLength = strlen(sText);
    iPatternLength = strlen(sPattern);
    if ( iPatternLength > iTextLength ) {
        return false;
    }

    for ( i = 0u; i <= iTextLength - iPatternLength; ++i ) {
        bool bMatches = true;

        for ( j = 0u; j < iPatternLength; ++j ) {
            if ( xwork__ascii_lower(sText[i + j]) !=
                 xwork__ascii_lower(sPattern[j]) ) {
                bMatches = false;
                break;
            }
        }
        if ( bMatches ) {
            return true;
        }
    }
    return false;
}

static bool xwork__tool_arguments_look_destructive_command(
    const xwork_tool_def *pTool,
    const char *sArgumentsJson
)
{
    static const char *const psDestructivePatterns[] = {
        "rm -",
        "rm /",
        "del ",
        "erase ",
        "rd ",
        "rmdir ",
        "remove-item",
        "git clean",
        "git reset --hard",
        "git checkout --"
    };
    size_t i;

    if ( !pTool || !sArgumentsJson ) {
        return false;
    }
    if ( strcmp(pTool->sToolId, XWORK_TOOL_PROCESS_EXEC) != 0 &&
         strcmp(pTool->sToolId, XWORK_TOOL_PROCESS_START_TERMINAL) != 0 &&
         strcmp(pTool->sToolId, XWORK_TOOL_PROCESS_TERMINAL_WRITE) != 0 ) {
        return false;
    }

    for ( i = 0u; i < sizeof(psDestructivePatterns) / sizeof(psDestructivePatterns[0]); ++i ) {
        if ( xwork__ascii_contains_ci(sArgumentsJson, psDestructivePatterns[i]) ) {
            return true;
        }
    }
    return false;
}

static bool xwork__tool_arguments_json_bool_is_true(
    const char *sArgumentsJson,
    const char *sName
)
{
    size_t i;
    size_t j;
    size_t iNameLength;

    if ( !sArgumentsJson || !sName || !sName[0] ) {
        return false;
    }
    iNameLength = strlen(sName);
    for ( i = 0u; sArgumentsJson[i] != '\0'; ++i ) {
        if ( sArgumentsJson[i] != '"' ) {
            continue;
        }
        for ( j = 0u; j < iNameLength; ++j ) {
            if ( xwork__ascii_lower(sArgumentsJson[i + 1u + j]) !=
                 xwork__ascii_lower(sName[j]) ) {
                break;
            }
        }
        if ( j != iNameLength || sArgumentsJson[i + 1u + j] != '"' ) {
            continue;
        }
        i += 1u + j + 1u;
        while ( sArgumentsJson[i] == ' ' ||
                sArgumentsJson[i] == '\t' ||
                sArgumentsJson[i] == '\r' ||
                sArgumentsJson[i] == '\n' ) {
            ++i;
        }
        if ( sArgumentsJson[i] != ':' ) {
            continue;
        }
        ++i;
        while ( sArgumentsJson[i] == ' ' ||
                sArgumentsJson[i] == '\t' ||
                sArgumentsJson[i] == '\r' ||
                sArgumentsJson[i] == '\n' ) {
            ++i;
        }
        return xwork__ascii_lower(sArgumentsJson[i]) == 't' &&
            xwork__ascii_lower(sArgumentsJson[i + 1u]) == 'r' &&
            xwork__ascii_lower(sArgumentsJson[i + 2u]) == 'u' &&
            xwork__ascii_lower(sArgumentsJson[i + 3u]) == 'e';
    }
    return false;
}

static bool xwork__tool_arguments_look_destructive_filesystem(
    const xwork_tool_def *pTool,
    const char *sArgumentsJson
)
{
    bool bDryRun;

    if ( !pTool || !sArgumentsJson ) {
        return false;
    }
    bDryRun = xwork__tool_arguments_json_bool_is_true(sArgumentsJson, "dry_run");
    if ( strcmp(pTool->sToolId, XWORK_TOOL_FILESYSTEM_DELETE) == 0 ) {
        return !bDryRun;
    }
    if ( strcmp(pTool->sToolId, XWORK_TOOL_FILESYSTEM_MOVE) == 0 ) {
        return !bDryRun &&
            xwork__tool_arguments_json_bool_is_true(sArgumentsJson, "overwrite");
    }
    return false;
}

static xwork_status xwork__evaluate_tool_approval(
    const xwork_run *pRun,
    const xwork_tool_def *pTool,
    const xllm_output_tool_call *pToolCall,
    const xwork_orchestrator_options *pOptions,
    xwork_approval_decision *pDecision
)
{
    xwork_approval_eval_input tInput;

    if ( !pRun || !pRun->pRuntime || !pTool || !pOptions || !pDecision ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_approval_eval_input_init(&tInput);
    tInput.eAutonomy = pRun->eAutonomy;
    tInput.eApprovalMode = pTool->eApprovalMode;
    tInput.eSideEffect = pTool->eSideEffect;
    tInput.bAutoApproveRequested = pOptions->bAutoApprove;
    if ( xwork__tool_arguments_look_destructive_command(
             pTool,
             pToolCall ? pToolCall->sArgumentsJson : NULL
         ) ) {
        tInput.bHasRiskLevelOverride = true;
        tInput.eRiskLevelOverride = XWORK_RISK_CRITICAL;
        tInput.sRiskScopeOverride = "destructive_command";
        tInput.sRiskReasonOverride = "Tool arguments are classified as destructive.";
    } else if ( xwork__tool_arguments_look_destructive_filesystem(
                    pTool,
                    pToolCall ? pToolCall->sArgumentsJson : NULL
                ) ) {
        tInput.bHasRiskLevelOverride = true;
        tInput.eRiskLevelOverride = XWORK_RISK_CRITICAL;
        tInput.sRiskScopeOverride = "destructive_filesystem";
        tInput.sRiskReasonOverride = "Filesystem arguments are classified as destructive.";
    }
    return xwork_policy_evaluate_approval(&pRun->pRuntime->tPolicy, &tInput, pDecision);
}

typedef struct {
    xwork_run *pRun;
    const xwork_orchestrator_options *pOptions;
    xllm_cancel_token *pCancelToken;
    bool bCancelledByCallback;
} xwork__model_event_bridge_ctx;

static bool xwork__orchestrator_should_interrupt(
    const xwork_run *pRun,
    const xwork_orchestrator_options *pOptions,
    const char *sPhase
)
{
    if ( !pOptions || !pOptions->pfnShouldInterrupt ) {
        return false;
    }
    return pOptions->pfnShouldInterrupt(pRun, sPhase, pOptions->pInterruptUserData);
}

bool xwork_tool_exec_context_should_cancel(
    const xwork_run *pRun,
    const xwork_tool_exec_context *pContext,
    const char *sPhase
)
{
    if ( !pContext ) {
        return false;
    }
    if ( pContext->pCancelToken &&
         xllm_cancel_token_is_cancelled(pContext->pCancelToken) ) {
        return true;
    }
    if ( pContext->pfnShouldInterrupt &&
         pContext->pfnShouldInterrupt(
             pRun,
             sPhase && sPhase[0]
                 ? sPhase
                 : (pContext->sPhase ? pContext->sPhase : "tool_execution"),
             pContext->pInterruptUserData
         ) ) {
        return true;
    }
    return false;
}

static xllm_stream_mode xwork__to_xllm_stream_mode(xwork_model_stream_mode eMode)
{
    switch ( eMode ) {
        case XWORK_MODEL_STREAM_OFF:
            return XLLM_STREAM_OFF;
        case XWORK_MODEL_STREAM_PREFER:
            return XLLM_STREAM_PREFER;
        case XWORK_MODEL_STREAM_REQUIRE:
            return XLLM_STREAM_REQUIRE;
        case XWORK_MODEL_STREAM_AUTO:
        default:
            return XLLM_STREAM_AUTO;
    }
}

static void xwork__model_event_from_xllm(
    const xllm_event *pSource,
    xwork_model_event *pTarget
)
{
    if ( !pTarget ) {
        return;
    }
    memset(pTarget, 0, sizeof(*pTarget));
    if ( !pSource ) {
        return;
    }

    pTarget->eType = (int)pSource->eType;
    pTarget->bSynthetic = pSource->bSynthetic;
    pTarget->iOutputIndex = (size_t)pSource->uOutputIndex;

    switch ( pSource->eType ) {
        case XLLM_EVENT_START:
            pTarget->sResponseId = pSource->as.tStart.sResponseId;
            pTarget->sModel = pSource->as.tStart.sModel;
            break;
        case XLLM_EVENT_TEXT_DELTA:
            pTarget->sText = pSource->as.tTextDelta.sText;
            break;
        case XLLM_EVENT_THINKING_DELTA:
            pTarget->sText = pSource->as.tThinkingDelta.sText;
            pTarget->sFormat = pSource->as.tThinkingDelta.sFormat;
            break;
        case XLLM_EVENT_TOOL_CALL_DELTA:
            pTarget->sToolCallId = pSource->as.tToolCallDelta.sCallId;
            pTarget->sToolId = pSource->as.tToolCallDelta.sToolId;
            pTarget->sToolName = pSource->as.tToolCallDelta.sToolName;
            pTarget->sArgumentsDelta = pSource->as.tToolCallDelta.sArgumentsDelta;
            break;
        case XLLM_EVENT_TOOL_CALL_READY:
            pTarget->sToolCallId = pSource->as.tToolCallReady.tToolCall.sCallId;
            pTarget->sToolId = pSource->as.tToolCallReady.tToolCall.sToolId;
            pTarget->sToolName = pSource->as.tToolCallReady.tToolCall.sToolName;
            pTarget->sArgumentsDelta = pSource->as.tToolCallReady.tToolCall.sArgumentsJson;
            break;
        case XLLM_EVENT_ARTIFACT_BEGIN:
            pTarget->sArtifactId = pSource->as.tArtifactBegin.tInfo.sArtifactId;
            break;
        case XLLM_EVENT_ARTIFACT_CHUNK:
            pTarget->sArtifactId = pSource->as.tArtifactChunk.sArtifactId;
            pTarget->pArtifactData = pSource->as.tArtifactChunk.pData;
            pTarget->iArtifactSize = pSource->as.tArtifactChunk.iSize;
            break;
        case XLLM_EVENT_ARTIFACT_READY:
            pTarget->sArtifactId = pSource->as.tArtifactReady.tInfo.sArtifactId;
            break;
        case XLLM_EVENT_REFUSAL:
            pTarget->sText = pSource->as.tRefusal.tRefusal.sText;
            break;
        case XLLM_EVENT_ERROR:
            pTarget->sText = pSource->as.tError.tError.sMessage;
            break;
        default:
            break;
    }
}

static bool xwork__model_event_bridge(const xllm_event *pEvent, void *pUserData)
{
    xwork__model_event_bridge_ctx *pCtx = (xwork__model_event_bridge_ctx *)pUserData;
    xwork_model_event tEvent;
    bool bContinue = true;

    if ( !pCtx || !pCtx->pOptions ) {
        return true;
    }

    if ( xwork__orchestrator_should_interrupt(pCtx->pRun, pCtx->pOptions, "model_event") ) {
        pCtx->bCancelledByCallback = true;
        if ( pCtx->pCancelToken ) {
            xllm_cancel_token_cancel(pCtx->pCancelToken, "xwork interrupted during model event");
        }
        return false;
    }

    if ( pCtx->pOptions->pfnModelEvent ) {
        xwork__model_event_from_xllm(pEvent, &tEvent);
        bContinue = pCtx->pOptions->pfnModelEvent(
            pCtx->pRun,
            &tEvent,
            pCtx->pOptions->pModelEventUserData
        );
        if ( !bContinue ) {
            pCtx->bCancelledByCallback = true;
            if ( pCtx->pCancelToken ) {
                xllm_cancel_token_cancel(pCtx->pCancelToken, "xwork model event callback cancelled");
            }
        }
    }

    return bContinue;
}

static xwork_status xwork__record_run_state_event(
    xwork_run *pRun,
    xwork_run_state eState,
    xwork_event_kind eKind,
    const char *sSummary
)
{
    if ( !pRun ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    pRun->eState = eState;
    return xwork__run_record_event(pRun, eKind, NULL, NULL, NULL, sSummary);
}

static xwork_status xwork__store_tool_result(
    xwork_run *pRun,
    const xwork_tool_result *pResult
);

static xwork_status xwork__fail_run_with_summary(xwork_run *pRun, const char *sSummary)
{
    xwork_status iStatus;
    const char *sEffectiveSummary = sSummary ? sSummary : "Run failed.";

    iStatus = xwork__replace_cstr(&pRun->sLastOutputText, sEffectiveSummary);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    return xwork__record_run_state_event(
        pRun,
        XWORK_RUN_FAILED,
        XWORK_EVENT_RUN_FAILED,
        sEffectiveSummary
    );
}

static const char *xwork__xllm_error_code_cstr(xllm_error_code eCode)
{
    switch ( eCode ) {
        case XLLM_ERROR_NONE:
            return "none";
        case XLLM_ERROR_AUTH:
            return "auth";
        case XLLM_ERROR_QUOTA:
            return "quota";
        case XLLM_ERROR_RATE_LIMIT:
            return "rate_limit";
        case XLLM_ERROR_TIMEOUT:
            return "timeout";
        case XLLM_ERROR_NETWORK:
            return "network";
        case XLLM_ERROR_CANCELLED:
            return "cancelled";
        case XLLM_ERROR_INVALID_REQUEST:
            return "invalid_request";
        case XLLM_ERROR_UNSUPPORTED_CAPABILITY:
            return "unsupported_capability";
        case XLLM_ERROR_UNSUPPORTED_INPUT_TYPE:
            return "unsupported_input_type";
        case XLLM_ERROR_UNSUPPORTED_MIME_TYPE:
            return "unsupported_mime_type";
        case XLLM_ERROR_INPUT_TOO_LARGE:
            return "input_too_large";
        case XLLM_ERROR_TOO_MANY_INPUT_PARTS:
            return "too_many_input_parts";
        case XLLM_ERROR_MISSING_MULTIMODAL_MODEL:
            return "missing_multimodal_model";
        case XLLM_ERROR_MODEL_NOT_FOUND:
            return "model_not_found";
        case XLLM_ERROR_UPSTREAM_4XX:
            return "upstream_4xx";
        case XLLM_ERROR_UPSTREAM_5XX:
            return "upstream_5xx";
        case XLLM_ERROR_PARSE:
            return "parse";
        case XLLM_ERROR_INTERNAL:
            return "internal";
        case XLLM_ERROR_SESSION_CONTEXT_OVERFLOW:
            return "session_context_overflow";
        case XLLM_ERROR_SESSION_COMPACT_FAILED:
            return "session_compact_failed";
        case XLLM_ERROR_SESSION_SUMMARY_FAILED:
            return "session_summary_failed";
        case XLLM_ERROR_SESSION_REQUIRES_MODEL_LIMITS:
            return "session_requires_model_limits";
        default:
            return "unknown";
    }
}

static char *xwork__format_model_turn_error_summary(const xllm_error *pError)
{
    const char *sErrorCode = pError
        ? xwork__xllm_error_code_cstr(pError->eCode)
        : "unknown";
    const char *sMessage = (pError && pError->sMessage)
        ? pError->sMessage
        : "unknown error";

    return xwork__dup_printf(
        "xllm session chat failed: xllm_error=%s message=%s",
        sErrorCode,
        sMessage
    );
}

static void xwork__fail_run_after_tool_error(
    xwork_run *pRun,
    const xwork_tool_result *pToolResult,
    const char *sFallbackSummary
)
{
    const char *sSummary = sFallbackSummary ? sFallbackSummary : "Tool execution failed.";

    if ( !pRun ) {
        return;
    }

    if ( pToolResult &&
         ((pToolResult->sOutputText && pToolResult->sOutputText[0]) ||
          (pToolResult->sVisibleSummary && pToolResult->sVisibleSummary[0])) ) {
        (void)xwork__store_tool_result(pRun, pToolResult);
    }
    if ( pToolResult &&
         pToolResult->sVisibleSummary &&
         pToolResult->sVisibleSummary[0] ) {
        sSummary = pToolResult->sVisibleSummary;
    }

    (void)xwork__fail_run_with_summary(pRun, sSummary);
}

static xwork_status xwork__save_checkpoint(
    xwork_run *pRun,
    xwork_checkpoint_kind eKind,
    const char *sPendingStep,
    const char *sToolOutputsRef,
    const char *sSummary
)
{
    char *sArtifactRefs;
    char *sSessionStateRef = NULL;
    char *sCheckpointId = NULL;
    xwork_status iStatus;

    iStatus = xwork__run_refresh_session_state(pRun);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    if ( pRun->sSessionStateData && pRun->sSessionStateData[0] ) {
        sCheckpointId = xwork__dup_scoped_id(
            pRun->sRunId,
            "checkpoint",
            pRun->iNextCheckpointSequence + 1u
        );
        if ( !sCheckpointId ) {
            return XWORK_ERROR_NO_MEMORY;
        }

        sSessionStateRef = xwork__dup_printf(
            "xwork://run/%s/checkpoint/%s/session",
            pRun->sRunId,
            sCheckpointId
        );
        free(sCheckpointId);
        if ( !sSessionStateRef ) {
            return XWORK_ERROR_NO_MEMORY;
        }
    }

    sArtifactRefs = xwork__run_build_artifact_refs(pRun);
    iStatus = xwork__run_record_checkpoint(
        pRun,
        eKind,
        sPendingStep,
        sSessionStateRef,
        sToolOutputsRef,
        NULL,
        sArtifactRefs
    );
    free(sSessionStateRef);
    free(sArtifactRefs);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    return xwork__run_record_event(
        pRun,
        XWORK_EVENT_CHECKPOINT_SAVED,
        pRun->sLastToolId,
        pRun->sLastApprovalRequestId,
        pRun->sLastCheckpointId,
        sSummary
    );
}

static xwork_status xwork__record_session_compacted(
    xwork_run *pRun,
    const xwork__session_compaction_metrics *pBefore,
    const xwork__session_compaction_metrics *pAfter
)
{
    char *sSummary;
    xwork_status iStatus;

    if ( !pRun || !pBefore || !pAfter ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    sSummary = xwork__dup_printf(
        "Session compacted: committed_turns=%u->%u history_messages=%u->%u summary_turns=%u->%u.",
        (unsigned)pBefore->uCommittedTurns,
        (unsigned)pAfter->uCommittedTurns,
        (unsigned)pBefore->iHistoryCount,
        (unsigned)pAfter->iHistoryCount,
        (unsigned)pBefore->uSummaryTurns,
        (unsigned)pAfter->uSummaryTurns
    );
    if ( !sSummary ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    iStatus = xwork__save_checkpoint(
        pRun,
        XWORK_CHECKPOINT_SESSION_COMPACTED,
        "session_compacted",
        NULL,
        sSummary
    );
    if ( iStatus == XWORK_OK ) {
        iStatus = xwork__run_record_event(
            pRun,
            XWORK_EVENT_SESSION_COMPACTED,
            pRun->sLastToolId,
            pRun->sLastApprovalRequestId,
            pRun->sLastCheckpointId,
            sSummary
        );
    }

    free(sSummary);
    return iStatus;
}

static xwork_status xwork__cancel_run_with_checkpoint(
    xwork_run *pRun,
    const char *sPendingStep,
    const char *sSummary
)
{
    const char *sEffectiveSummary = sSummary ? sSummary : "Run cancelled.";
    xwork_status iStatus;

    if ( !pRun ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    iStatus = xwork__replace_cstr(&pRun->sLastOutputText, sEffectiveSummary);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    pRun->eState = XWORK_RUN_CANCELLED;
    iStatus = xwork__save_checkpoint(
        pRun,
        XWORK_CHECKPOINT_COMPLETION,
        sPendingStep ? sPendingStep : "cancelled",
        pRun->sLastToolResultText,
        "Checkpoint saved at cancellation."
    );
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    iStatus = xwork__run_record_event(
        pRun,
        XWORK_EVENT_RUN_CANCELLED,
        pRun->sLastToolId,
        pRun->sLastApprovalRequestId,
        pRun->sLastCheckpointId,
        sEffectiveSummary
    );
    return iStatus == XWORK_OK ? XWORK_ERROR_CANCELLED : iStatus;
}

static xwork_status xwork__check_interrupt_or_cancelled(
    xwork_run *pRun,
    const xwork_orchestrator_options *pOptions,
    const char *sPhase,
    const char *sPendingStep
)
{
    if ( pOptions &&
         pOptions->pCancelToken &&
         xllm_cancel_token_is_cancelled(pOptions->pCancelToken) ) {
        return xwork__cancel_run_with_checkpoint(
            pRun,
            sPendingStep,
            "Run cancelled by model cancel token."
        );
    }

    if ( xwork__orchestrator_should_interrupt(pRun, pOptions, sPhase) ) {
        return xwork__cancel_run_with_checkpoint(
            pRun,
            sPendingStep,
            "Run interrupted by orchestrator callback."
        );
    }

    return XWORK_OK;
}

static void xwork__retry_backoff_delay(const xwork_orchestrator_options *pOptions)
{
    size_t iDelayMs;

    if ( !pOptions || pOptions->iRetryBackoffMs == 0u ) {
        return;
    }

    iDelayMs = pOptions->iRetryBackoffMs;
    if ( iDelayMs > (size_t)0xffffffffu ) {
        iDelayMs = (size_t)0xffffffffu;
    }
    xrtSleep((uint32)iDelayMs);
}

static bool xwork__tool_status_is_retryable(
    xwork_status iStatus,
    const xwork_tool_result *pToolResult
)
{
    if ( iStatus == XWORK_OK ||
         iStatus == XWORK_ERROR_CANCELLED ||
         iStatus == XWORK_ERROR_INVALID_ARGUMENT ||
         iStatus == XWORK_ERROR_INVALID_STATE ||
         iStatus == XWORK_ERROR_NOT_FOUND ||
         iStatus == XWORK_ERROR_ALREADY_EXISTS ||
         iStatus == XWORK_ERROR_UNSUPPORTED ||
         iStatus == XWORK_ERROR_NO_MEMORY ) {
        return false;
    }

    return pToolResult && pToolResult->bRetryable;
}

static const char *xwork__planner_system_text(const xwork_orchestrator_options *pOptions)
{
    if ( !pOptions || pOptions->ePlannerMode == XWORK_PLANNER_OFF ) {
        return NULL;
    }
    if ( pOptions->sPlannerContextText && pOptions->sPlannerContextText[0] ) {
        return pOptions->sPlannerContextText;
    }
    if ( pOptions->sPlannerPlanJson && pOptions->sPlannerPlanJson[0] ) {
        return pOptions->sPlannerPlanJson;
    }
    return NULL;
}

static xllm_tool_choice_mode xwork__to_xllm_tool_choice(xwork_tool_choice_mode eMode)
{
    switch ( eMode ) {
        case XWORK_TOOL_CHOICE_NONE:
            return XLLM_TOOL_CHOICE_NONE;
        case XWORK_TOOL_CHOICE_REQUIRED:
            return XLLM_TOOL_CHOICE_REQUIRED;
        case XWORK_TOOL_CHOICE_NAMED:
            return XLLM_TOOL_CHOICE_NAMED;
        case XWORK_TOOL_CHOICE_AUTO:
        default:
            return XLLM_TOOL_CHOICE_AUTO;
    }
}

static xwork_status xwork__store_tool_call(
    xwork_run *pRun,
    const xllm_output_tool_call *pToolCall
)
{
    const char *sToolId = xwork__tool_call_id(pToolCall);
    xwork_status iStatus;

    iStatus = xwork__replace_cstr(&pRun->sLastToolCallId, pToolCall ? pToolCall->sCallId : NULL);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__replace_cstr(&pRun->sLastToolId, sToolId);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__replace_cstr(
        &pRun->sLastToolArgumentsJson,
        pToolCall ? pToolCall->sArgumentsJson : NULL
    );
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    pRun->bHasLastToolCall = sToolId != NULL && sToolId[0] != '\0';
    pRun->bHasLastToolResult = false;
    iStatus = xwork__replace_cstr(&pRun->sLastToolResultText, NULL);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    return xwork__replace_cstr(&pRun->sLastToolVisibleSummary, NULL);
}

static xwork_status xwork__store_tool_result(
    xwork_run *pRun,
    const xwork_tool_result *pResult
)
{
    xwork_status iStatus;

    iStatus = xwork__replace_cstr(&pRun->sLastToolResultText, pResult ? pResult->sOutputText : NULL);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__replace_cstr(
        &pRun->sLastToolVisibleSummary,
        pResult ? pResult->sVisibleSummary : NULL
    );
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    pRun->bHasLastToolResult = (pResult != NULL);
    return XWORK_OK;
}

static xwork_status xwork__record_retry_checkpoint(
    xwork_run *pRun,
    const char *sToolId,
    const char *sPendingStep,
    const char *sRetrySummary,
    const char *sCheckpointSummary
)
{
    xwork_status iStatus;

    iStatus = xwork__run_record_event(
        pRun,
        XWORK_EVENT_RETRY_SCHEDULED,
        sToolId,
        pRun ? pRun->sLastApprovalRequestId : NULL,
        pRun ? pRun->sLastCheckpointId : NULL,
        sRetrySummary
    );
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    return xwork__save_checkpoint(
        pRun,
        XWORK_CHECKPOINT_MANUAL,
        sPendingStep,
        pRun ? pRun->sLastToolResultText : NULL,
        sCheckpointSummary
    );
}

static xwork_status xwork__store_final_output(xwork_run *pRun, const char *sOutputText)
{
    return xwork__replace_cstr(&pRun->sLastOutputText, sOutputText);
}

static bool xwork__orchestrator_has_pending_tool_call(const xwork_run *pRun)
{
    return pRun &&
           pRun->bHasLastToolCall &&
           !pRun->bHasLastToolResult &&
           pRun->sLastToolId &&
           pRun->sLastToolId[0];
}

static bool xwork__parse_json_table(const char *sJson, xvalue *ptTable)
{
    xvalue tValue;

    if ( !ptTable ) {
        return false;
    }

    *ptTable = NULL;
    if ( !sJson || !sJson[0] ) {
        return true;
    }

    tValue = xrtParseJSON((str)sJson, strlen(sJson));
    if ( !tValue || xvoType(tValue) != XVO_DT_TABLE ) {
        if ( tValue ) {
            xvoUnref(tValue);
        }
        return false;
    }

    *ptTable = tValue;
    return true;
}

static const char *xwork__json_table_get_text(xvalue tTable, const char *sKey)
{
    xvalue tValue;

    if ( !tTable || !sKey ) {
        return NULL;
    }

    tValue = xvoTableGetValue(tTable, sKey, (uint32)strlen(sKey));
    if ( !tValue || xvoType(tValue) != XVO_DT_TEXT ) {
        return NULL;
    }

    return (const char *)xvoGetText(tValue);
}

static bool xwork__json_table_get_int(xvalue tTable, const char *sKey, int *piValue)
{
    xvalue tValue;

    if ( piValue ) {
        *piValue = 0;
    }
    if ( !tTable || !sKey || !piValue ) {
        return false;
    }

    tValue = xvoTableGetValue(tTable, sKey, (uint32)strlen(sKey));
    if ( !tValue || xvoType(tValue) != XVO_DT_INT ) {
        return false;
    }

    *piValue = (int)xvoGetInt(tValue);
    return true;
}

static bool xwork__json_table_get_bool(xvalue tTable, const char *sKey, bool *pbValue)
{
    xvalue tValue;

    if ( pbValue ) {
        *pbValue = false;
    }
    if ( !tTable || !sKey || !pbValue ) {
        return false;
    }

    tValue = xvoTableGetValue(tTable, sKey, (uint32)strlen(sKey));
    if ( !tValue || xvoType(tValue) != XVO_DT_BOOL ) {
        return false;
    }

    *pbValue = xvoGetBool(tValue);
    return true;
}

static void xwork__patch_text_stats(
    const char *sPatchText,
    size_t *piHunkCount,
    size_t *piAddedLineCount,
    size_t *piDeletedLineCount
)
{
    const char *sCursor;

    if ( piHunkCount ) *piHunkCount = 0u;
    if ( piAddedLineCount ) *piAddedLineCount = 0u;
    if ( piDeletedLineCount ) *piDeletedLineCount = 0u;
    if ( !sPatchText ) {
        return;
    }

    sCursor = sPatchText;
    while ( *sCursor ) {
        const char *sLine = sCursor;

        if ( sLine[0] == '@' && sLine[1] == '@' && piHunkCount ) {
            ++*piHunkCount;
        } else if ( sLine[0] == '+' && strncmp(sLine, "+++", 3u) != 0 &&
                    piAddedLineCount ) {
            ++*piAddedLineCount;
        } else if ( sLine[0] == '-' && strncmp(sLine, "---", 3u) != 0 &&
                    piDeletedLineCount ) {
            ++*piDeletedLineCount;
        }

        while ( *sCursor && *sCursor != '\n' ) {
            ++sCursor;
        }
        if ( *sCursor == '\n' ) {
            ++sCursor;
        }
    }
}

static xwork_status xwork__build_patch_apply_artifact_metadata(
    xvalue tResult,
    const char *sToolId,
    const char *sPath,
    const char *sPatchText,
    char **ppsApplyResultJson,
    char **ppsFileSummaryJson
)
{
    char *sEscapedToolId = NULL;
    char *sEscapedPath = NULL;
    char *sEscapedErrorKind = NULL;
    char *sEscapedError = NULL;
    char *sApplyResultJson = NULL;
    char *sFileSummaryJson = NULL;
    const char *sErrorKind;
    const char *sError;
    bool bOk = false;
    bool bDryRun = false;
    bool bChanged = false;
    int iBytesBefore = 0;
    int iBytesAfter = 0;
    size_t iHunkCount = 0u;
    size_t iAddedLineCount = 0u;
    size_t iDeletedLineCount = 0u;

    if ( !ppsApplyResultJson || !ppsFileSummaryJson ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    *ppsApplyResultJson = NULL;
    *ppsFileSummaryJson = NULL;

    (void)xwork__json_table_get_bool(tResult, "ok", &bOk);
    (void)xwork__json_table_get_bool(tResult, "dry_run", &bDryRun);
    (void)xwork__json_table_get_bool(tResult, "changed", &bChanged);
    (void)xwork__json_table_get_int(tResult, "bytes_before", &iBytesBefore);
    (void)xwork__json_table_get_int(tResult, "bytes_after", &iBytesAfter);
    sErrorKind = xwork__json_table_get_text(tResult, "error_kind");
    sError = xwork__json_table_get_text(tResult, "error");
    xwork__patch_text_stats(
        sPatchText,
        &iHunkCount,
        &iAddedLineCount,
        &iDeletedLineCount
    );

    sEscapedToolId = xwork__json_escape_string(sToolId ? sToolId : "");
    sEscapedPath = xwork__json_escape_string(sPath ? sPath : "");
    sEscapedErrorKind = xwork__json_escape_string(sErrorKind ? sErrorKind : "");
    sEscapedError = xwork__json_escape_string(sError ? sError : "");
    if ( !sEscapedToolId || !sEscapedPath || !sEscapedErrorKind || !sEscapedError ) {
        free(sEscapedError);
        free(sEscapedErrorKind);
        free(sEscapedPath);
        free(sEscapedToolId);
        return XWORK_ERROR_NO_MEMORY;
    }

    sApplyResultJson = xwork__dup_printf(
        "{\"schema\":\"" XWORK_PATCH_APPLY_RESULT_SCHEMA_V1 "\","
        "\"tool\":\"%s\",\"path\":\"%s\",\"ok\":%s,\"dry_run\":%s,"
        "\"changed\":%s,\"bytes_before\":%d,\"bytes_after\":%d,"
        "\"error_kind\":\"%s\",\"error\":\"%s\"}",
        sEscapedToolId,
        sEscapedPath,
        bOk ? "true" : "false",
        bDryRun ? "true" : "false",
        bChanged ? "true" : "false",
        iBytesBefore,
        iBytesAfter,
        sEscapedErrorKind,
        sEscapedError
    );
    sFileSummaryJson = xwork__dup_printf(
        "{\"schema\":\"" XWORK_PATCH_FILE_SUMMARY_SCHEMA_V1 "\","
        "\"files\":[{\"path\":\"%s\",\"change_kind\":\"modify\","
        "\"hunks\":%lu,\"added_lines\":%lu,\"deleted_lines\":%lu}]}",
        sEscapedPath,
        (unsigned long)iHunkCount,
        (unsigned long)iAddedLineCount,
        (unsigned long)iDeletedLineCount
    );

    free(sEscapedError);
    free(sEscapedErrorKind);
    free(sEscapedPath);
    free(sEscapedToolId);
    if ( !sApplyResultJson || !sFileSummaryJson ) {
        free(sFileSummaryJson);
        free(sApplyResultJson);
        return XWORK_ERROR_NO_MEMORY;
    }

    *ppsApplyResultJson = sApplyResultJson;
    *ppsFileSummaryJson = sFileSummaryJson;
    return XWORK_OK;
}

static xvalue xwork__json_table_get_array(xvalue tTable, const char *sKey)
{
    xvalue tValue;

    if ( !tTable || !sKey ) {
        return NULL;
    }

    tValue = xvoTableGetValue(tTable, sKey, (uint32)strlen(sKey));
    if ( !tValue || xvoType(tValue) != XVO_DT_ARRAY ) {
        return NULL;
    }

    return tValue;
}

static xwork_status xwork__append_text(char **psTarget, size_t *piLength, const char *sText)
{
    char *sNext;
    size_t iAppendLength;

    if ( !psTarget || !piLength ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !sText || !sText[0] ) {
        return XWORK_OK;
    }

    iAppendLength = strlen(sText);
    sNext = (char *)realloc(*psTarget, *piLength + iAppendLength + 1u);
    if ( !sNext ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    memcpy(sNext + *piLength, sText, iAppendLength);
    *piLength += iAppendLength;
    sNext[*piLength] = '\0';
    *psTarget = sNext;
    return XWORK_OK;
}

static xwork_status xwork__build_terminal_event_output(
    xvalue tResult,
    char **psOutputText
)
{
    xvalue tEvents;
    char *sOutputText = NULL;
    size_t iOutputLength = 0u;
    uint32 i;
    xwork_status iStatus = XWORK_OK;

    if ( !psOutputText ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    *psOutputText = NULL;
    if ( !tResult ) {
        return XWORK_OK;
    }

    tEvents = xwork__json_table_get_array(tResult, "events");
    if ( !tEvents ) {
        return XWORK_OK;
    }

    for ( i = 0u; i < xvoArrayItemCount(tEvents); ++i ) {
        xvalue tEvent = xvoArrayGetValue(tEvents, i);
        const char *sKind;
        const char *sText;

        if ( !tEvent || xvoType(tEvent) != XVO_DT_TABLE ) {
            continue;
        }

        sKind = xwork__json_table_get_text(tEvent, "kind");
        if ( !sKind || strcmp(sKind, "output") != 0 ) {
            continue;
        }

        sText = xwork__json_table_get_text(tEvent, "text");
        iStatus = xwork__append_text(&sOutputText, &iOutputLength, sText);
        if ( iStatus != XWORK_OK ) {
            goto cleanup;
        }
    }

    *psOutputText = sOutputText;
    sOutputText = NULL;

cleanup:
    free(sOutputText);
    return iStatus;
}

static const char *xwork__path_leaf_name(const char *sPath)
{
    const char *sLeaf;

    if ( !sPath || !sPath[0] ) {
        return NULL;
    }

    sLeaf = strrchr(sPath, '/');
    if ( !sLeaf ) {
        sLeaf = strrchr(sPath, '\\');
    }
    return (sLeaf && sLeaf[1]) ? (sLeaf + 1) : sPath;
}

static xwork_status xwork__emit_builtin_tool_artifact(
    xwork_run *pRun,
    const xwork_tool_def *pToolDef,
    const xwork_tool_call *pToolCall,
    const xwork_tool_result *pToolResult
)
{
    xvalue tArguments = NULL;
    xvalue tResult = NULL;
    char *sCombinedOutput = NULL;
    char *sTerminalOutput = NULL;
    xwork_status iStatus = XWORK_OK;

    if ( !pRun || !pToolDef || !pToolCall || !pToolResult ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    if ( !xwork__parse_json_table(pToolCall->sArgumentsJson, &tArguments) ||
         !xwork__parse_json_table(pToolResult->sOutputText, &tResult) ) {
        return XWORK_OK;
    }

    if ( strcmp(pToolDef->sToolId, XWORK_TOOL_FILESYSTEM_READ_TEXT) == 0 ) {
        xwork_output_artifact_options tOptions;
        const char *sText = xwork__json_table_get_text(tResult, "text");
        const char *sPath = xwork__json_table_get_text(tResult, "resolved_path");

        if ( !sPath ) {
            sPath = xwork__json_table_get_text(tArguments, "path");
        }
        if ( sText ) {
            xwork_output_artifact_options_init(&tOptions);
            tOptions.sName = xwork__path_leaf_name(sPath);
            tOptions.sStorageRef = sPath;
            tOptions.sSummary = pToolResult->sVisibleSummary;
            tOptions.eOutputClass = XWORK_ARTIFACT_OUTPUT_FILE_CONTENT;
            tOptions.sOutputRole = XWORK_TOOL_FILESYSTEM_READ_TEXT;
            tOptions.sOutputText = sText;
            iStatus = xwork_run_emit_output_artifact(pRun, &tOptions, NULL);
        }
    } else if ( strcmp(pToolDef->sToolId, XWORK_TOOL_FILESYSTEM_WRITE_TEXT) == 0 ) {
        xwork_output_artifact_options tOptions;
        const char *sText = xwork__json_table_get_text(tArguments, "text");
        const char *sPath = xwork__json_table_get_text(tResult, "resolved_path");

        if ( !sPath ) {
            sPath = xwork__json_table_get_text(tArguments, "path");
        }
        if ( sText ) {
            xwork_output_artifact_options_init(&tOptions);
            tOptions.sName = xwork__path_leaf_name(sPath);
            tOptions.sStorageRef = sPath;
            tOptions.sSummary = pToolResult->sVisibleSummary;
            tOptions.eOutputClass = XWORK_ARTIFACT_OUTPUT_FILE_CHANGE;
            tOptions.sOutputRole = XWORK_TOOL_FILESYSTEM_WRITE_TEXT;
            tOptions.sOutputText = sText;
            iStatus = xwork_run_emit_output_artifact(pRun, &tOptions, NULL);
        }
    } else if ( strcmp(pToolDef->sToolId, XWORK_TOOL_FILESYSTEM_LIST) == 0 ||
                strcmp(pToolDef->sToolId, XWORK_TOOL_FILESYSTEM_STAT) == 0 ||
                strcmp(pToolDef->sToolId, XWORK_TOOL_FILESYSTEM_GLOB) == 0 ||
                strcmp(pToolDef->sToolId, XWORK_TOOL_FILESYSTEM_MKDIR) == 0 ||
                strcmp(pToolDef->sToolId, XWORK_TOOL_FILESYSTEM_MOVE) == 0 ||
                strcmp(pToolDef->sToolId, XWORK_TOOL_FILESYSTEM_DELETE) == 0 ) {
        xwork_output_artifact_options tOptions;
        const char *sPath = xwork__json_table_get_text(tResult, "resolved_path");
        const char *sName = "filesystem.query.json";

        if ( strcmp(pToolDef->sToolId, XWORK_TOOL_FILESYSTEM_LIST) == 0 ) {
            sName = "filesystem.list.json";
        } else if ( strcmp(pToolDef->sToolId, XWORK_TOOL_FILESYSTEM_STAT) == 0 ) {
            sName = "filesystem.stat.json";
        } else if ( strcmp(pToolDef->sToolId, XWORK_TOOL_FILESYSTEM_GLOB) == 0 ) {
            sName = "filesystem.glob.json";
        } else if ( strcmp(pToolDef->sToolId, XWORK_TOOL_FILESYSTEM_MKDIR) == 0 ) {
            sName = "filesystem.mkdir.json";
        } else if ( strcmp(pToolDef->sToolId, XWORK_TOOL_FILESYSTEM_MOVE) == 0 ) {
            sName = "filesystem.move.json";
        } else if ( strcmp(pToolDef->sToolId, XWORK_TOOL_FILESYSTEM_DELETE) == 0 ) {
            sName = "filesystem.delete.json";
        }
        if ( !sPath ) {
            sPath = xwork__json_table_get_text(tArguments, "path");
        }
        if ( pToolResult->sOutputText && pToolResult->sOutputText[0] ) {
            xwork_output_artifact_options_init(&tOptions);
            tOptions.sName = sName;
            tOptions.sMimeType = "application/json";
            tOptions.sStorageRef = sPath;
            tOptions.sSummary = pToolResult->sVisibleSummary;
            tOptions.eOutputClass = XWORK_ARTIFACT_OUTPUT_JSON;
            tOptions.sOutputRole = pToolDef->sToolId;
            tOptions.sOutputText = pToolResult->sOutputText;
            iStatus = xwork_run_emit_output_artifact(pRun, &tOptions, NULL);
        }
    } else if ( strcmp(pToolDef->sToolId, XWORK_TOOL_FILESYSTEM_APPLY_PATCH) == 0 ) {
        xwork_patch_artifact_options tOptions;
        const char *sPatchText = xwork__json_table_get_text(tResult, "patch_text");
        const char *sPath = xwork__json_table_get_text(tResult, "resolved_path");
        char *sApplyResultJson = NULL;
        char *sFileSummaryJson = NULL;

        if ( !sPath ) {
            sPath = xwork__json_table_get_text(tArguments, "path");
        }
        if ( sPatchText && sPatchText[0] ) {
            iStatus = xwork__build_patch_apply_artifact_metadata(
                tResult,
                pToolDef->sToolId,
                sPath,
                sPatchText,
                &sApplyResultJson,
                &sFileSummaryJson
            );
            if ( iStatus != XWORK_OK ) {
                free(sFileSummaryJson);
                free(sApplyResultJson);
                goto cleanup;
            }
            xwork_patch_artifact_options_init(&tOptions);
            tOptions.sName = "filesystem.apply_patch.diff";
            tOptions.sTargetRef = sPath;
            tOptions.sSummary = pToolResult->sVisibleSummary;
            tOptions.sPatchText = sPatchText;
            tOptions.sApplyResultJson = sApplyResultJson;
            tOptions.sFileSummaryJson = sFileSummaryJson;
            iStatus = xwork_run_emit_patch_artifact(pRun, &tOptions, NULL);
            free(sFileSummaryJson);
            free(sApplyResultJson);
        }
    } else if ( strcmp(pToolDef->sToolId, XWORK_TOOL_EDITOR_OPEN_BUFFER) == 0 ||
                strcmp(pToolDef->sToolId, XWORK_TOOL_EDITOR_APPLY_EDIT) == 0 ) {
        xwork_output_artifact_options tOptions;
        const char *sPath = xwork__json_table_get_text(tResult, "resolved_path");
        const char *sText = xwork__json_table_get_text(tResult, "text");
        const char *sName = (strcmp(pToolDef->sToolId, XWORK_TOOL_EDITOR_OPEN_BUFFER) == 0)
            ? "editor.open_buffer.json"
            : "editor.apply_edit.json";

        if ( !sPath ) {
            sPath = xwork__json_table_get_text(tArguments, "path");
        }
        if ( pToolResult->sOutputText && pToolResult->sOutputText[0] ) {
            xwork_output_artifact_options_init(&tOptions);
            tOptions.sName = sName;
            tOptions.sMimeType = "application/json";
            tOptions.sStorageRef = sPath;
            tOptions.sSummary = pToolResult->sVisibleSummary;
            tOptions.eOutputClass = (strcmp(pToolDef->sToolId, XWORK_TOOL_EDITOR_OPEN_BUFFER) == 0)
                ? XWORK_ARTIFACT_OUTPUT_FILE_CONTENT
                : XWORK_ARTIFACT_OUTPUT_FILE_CHANGE;
            tOptions.sOutputRole = pToolDef->sToolId;
            tOptions.sOutputText = pToolResult->sOutputText;
            iStatus = xwork_run_emit_output_artifact(pRun, &tOptions, NULL);
        }
        (void)sText;
    } else if ( strcmp(pToolDef->sToolId, XWORK_TOOL_PROCESS_EXEC) == 0 ) {
        xwork_command_artifact_options tOptions;
        const char *sCommand = xwork__json_table_get_text(tArguments, "command");
        const char *sStdout = xwork__json_table_get_text(tResult, "stdout");
        const char *sStderr = xwork__json_table_get_text(tResult, "stderr");
        const char *sOutput = sStdout ? sStdout : "";
        const char *sCwd = xwork__json_table_get_text(tResult, "cwd");
        int iExitCode = 0;
        bool bHasExitCode = xwork__json_table_get_int(tResult, "exit_code", &iExitCode);
        bool bStdoutTruncated = false;
        bool bStderrTruncated = false;

        (void)xwork__json_table_get_bool(tResult, "stdout_truncated", &bStdoutTruncated);
        (void)xwork__json_table_get_bool(tResult, "stderr_truncated", &bStderrTruncated);

        if ( sStderr && sStderr[0] ) {
            if ( sStdout && sStdout[0] ) {
                sCombinedOutput = xwork__dup_printf(
                    "STDOUT:\n%s\n\nSTDERR:\n%s",
                    sStdout,
                    sStderr
                );
            } else {
                sCombinedOutput = xwork__dup_printf("STDERR:\n%s", sStderr);
            }
            if ( !sCombinedOutput ) {
                iStatus = XWORK_ERROR_NO_MEMORY;
                goto cleanup;
            }
            sOutput = sCombinedOutput;
        }
        if ( sCommand ) {
            xwork_command_artifact_options_init(&tOptions);
            tOptions.sName = "process.exec.txt";
            tOptions.sStorageRef = sCwd;
            tOptions.sSummary = pToolResult->sVisibleSummary;
            tOptions.sCommandText = sCommand;
            tOptions.sOutputText = sOutput;
            tOptions.bHasCommandIoStats = true;
            tOptions.iStdoutByteCount = sStdout ? strlen(sStdout) : 0u;
            tOptions.iStderrByteCount = sStderr ? strlen(sStderr) : 0u;
            tOptions.bStdoutTruncated = bStdoutTruncated;
            tOptions.bStderrTruncated = bStderrTruncated;
            tOptions.bHasExitCode = bHasExitCode;
            tOptions.iExitCode = iExitCode;
            iStatus = xwork_run_emit_command_artifact(pRun, &tOptions, NULL);
            if ( iStatus != XWORK_OK ) {
                goto cleanup;
            }
            iStatus = xwork__emit_process_diagnostics_artifact(
                pRun,
                sCommand,
                sCwd,
                sStdout,
                sStderr,
                bHasExitCode,
                iExitCode,
                bStdoutTruncated,
                bStderrTruncated
            );
        }
    } else if ( strcmp(pToolDef->sToolId, XWORK_TOOL_PROCESS_START_TERMINAL) == 0 ) {
        xwork_command_artifact_options tOptions;
        const char *sCommand = xwork__json_table_get_text(tResult, "command");
        const char *sSessionId = xwork__json_table_get_text(tResult, "session_id");

        if ( !sCommand ) {
            sCommand = xwork__json_table_get_text(tArguments, "command");
        }
        if ( sCommand ) {
            xwork_command_artifact_options_init(&tOptions);
            tOptions.sName = "process.start_terminal.txt";
            tOptions.sStorageRef = sSessionId;
            tOptions.sSummary = pToolResult->sVisibleSummary;
            tOptions.sCommandText = sCommand;
            tOptions.sOutputText = "";
            iStatus = xwork_run_emit_command_artifact(pRun, &tOptions, NULL);
        }
    } else if ( strcmp(pToolDef->sToolId, XWORK_TOOL_PROCESS_TERMINAL_WRITE) == 0 ) {
        xwork_command_artifact_options tOptions;
        xwork_output_artifact_options tStateOptions;
        const char *sInputText = xwork__json_table_get_text(tArguments, "input_text");
        const char *sSessionId = xwork__json_table_get_text(tResult, "session_id");

        if ( !sSessionId ) {
            sSessionId = xwork__json_table_get_text(tArguments, "session_id");
        }
        if ( sInputText ) {
            xwork_command_artifact_options_init(&tOptions);
            tOptions.sName = "process.terminal_write.txt";
            tOptions.sStorageRef = sSessionId;
            tOptions.sSummary = pToolResult->sVisibleSummary;
            tOptions.sCommandText = sInputText;
            tOptions.sOutputText = "";
            iStatus = xwork_run_emit_command_artifact(pRun, &tOptions, NULL);
        }
        if ( iStatus == XWORK_OK && pToolResult->sOutputText && pToolResult->sOutputText[0] ) {
            xwork_output_artifact_options_init(&tStateOptions);
            tStateOptions.sName = "process.terminal_write.json";
            tStateOptions.sMimeType = "application/json";
            tStateOptions.sStorageRef = sSessionId;
            tStateOptions.sSummary = pToolResult->sVisibleSummary;
            tStateOptions.eOutputClass = XWORK_ARTIFACT_OUTPUT_TERMINAL_STATE;
            tStateOptions.sOutputRole = XWORK_TOOL_PROCESS_TERMINAL_WRITE;
            tStateOptions.sOutputText = pToolResult->sOutputText;
            iStatus = xwork_run_emit_output_artifact(pRun, &tStateOptions, NULL);
        }
    } else if ( strcmp(pToolDef->sToolId, XWORK_TOOL_PROCESS_TERMINAL_READ) == 0 ) {
        xwork_output_artifact_options tOptions;
        const char *sSessionId = xwork__json_table_get_text(tResult, "session_id");

        if ( !sSessionId ) {
            sSessionId = xwork__json_table_get_text(tArguments, "session_id");
        }
        if ( pToolResult->sOutputText && pToolResult->sOutputText[0] ) {
            xwork_output_artifact_options_init(&tOptions);
            tOptions.sName = "process.terminal_read.json";
            tOptions.sMimeType = "application/json";
            tOptions.sStorageRef = sSessionId;
            tOptions.sSummary = pToolResult->sVisibleSummary;
            tOptions.eOutputClass = XWORK_ARTIFACT_OUTPUT_TERMINAL_STATE;
            tOptions.sOutputRole = XWORK_TOOL_PROCESS_TERMINAL_READ;
            tOptions.sOutputText = pToolResult->sOutputText;
            iStatus = xwork_run_emit_output_artifact(pRun, &tOptions, NULL);
        }
    } else if ( strcmp(pToolDef->sToolId, XWORK_TOOL_PROCESS_TERMINAL_RESIZE) == 0 ) {
        xwork_output_artifact_options tOptions;
        const char *sSessionId = xwork__json_table_get_text(tResult, "session_id");

        if ( !sSessionId ) {
            sSessionId = xwork__json_table_get_text(tArguments, "session_id");
        }
        if ( pToolResult->sOutputText && pToolResult->sOutputText[0] ) {
            xwork_output_artifact_options_init(&tOptions);
            tOptions.sName = "process.terminal_resize.json";
            tOptions.sMimeType = "application/json";
            tOptions.sStorageRef = sSessionId;
            tOptions.sSummary = pToolResult->sVisibleSummary;
            tOptions.eOutputClass = XWORK_ARTIFACT_OUTPUT_TERMINAL_STATE;
            tOptions.sOutputRole = XWORK_TOOL_PROCESS_TERMINAL_RESIZE;
            tOptions.sOutputText = pToolResult->sOutputText;
            iStatus = xwork_run_emit_output_artifact(pRun, &tOptions, NULL);
        }
    } else if ( strcmp(pToolDef->sToolId, XWORK_TOOL_PROCESS_TERMINAL_STOP) == 0 ) {
        xwork_output_artifact_options tOptions;
        const char *sSessionId = xwork__json_table_get_text(tResult, "session_id");
        const char *sOutputText = xwork__json_table_get_text(tResult, "output_text");

        if ( pToolResult->sOutputText && pToolResult->sOutputText[0] ) {
            xwork_output_artifact_options_init(&tOptions);
            tOptions.sName = "process.terminal_stop.json";
            tOptions.sMimeType = "application/json";
            tOptions.sStorageRef = sSessionId;
            tOptions.sSummary = pToolResult->sVisibleSummary;
            tOptions.eOutputClass = XWORK_ARTIFACT_OUTPUT_TERMINAL_STATE;
            tOptions.sOutputRole = XWORK_TOOL_PROCESS_TERMINAL_STOP;
            tOptions.sOutputText = pToolResult->sOutputText;
            iStatus = xwork_run_emit_output_artifact(pRun, &tOptions, NULL);
            if ( iStatus != XWORK_OK ) {
                goto cleanup;
            }
        }
        if ( !sOutputText ) {
            iStatus = xwork__build_terminal_event_output(tResult, &sTerminalOutput);
            if ( iStatus != XWORK_OK ) {
                goto cleanup;
            }
            sOutputText = sTerminalOutput;
        }
        if ( !sSessionId ) {
            sSessionId = xwork__json_table_get_text(tArguments, "session_id");
        }
        if ( sOutputText && sOutputText[0] ) {
            xwork_output_artifact_options_init(&tOptions);
            tOptions.sName = "process.terminal_session.txt";
            tOptions.sStorageRef = sSessionId;
            tOptions.sSummary = pToolResult->sVisibleSummary;
            tOptions.eOutputClass = XWORK_ARTIFACT_OUTPUT_TERMINAL_STATE;
            tOptions.sOutputRole = XWORK_TOOL_PROCESS_TERMINAL_STOP;
            tOptions.sOutputText = sOutputText;
            iStatus = xwork_run_emit_output_artifact(pRun, &tOptions, NULL);
        }
    } else if ( strcmp(pToolDef->sToolId, XWORK_TOOL_PROCESS_LIST_TERMINALS) == 0 ) {
        xwork_output_artifact_options tOptions;

        if ( pToolResult->sOutputText && pToolResult->sOutputText[0] ) {
            xwork_output_artifact_options_init(&tOptions);
            tOptions.sName = "process.list_terminals.json";
            tOptions.sMimeType = "application/json";
            tOptions.sStorageRef = "terminal-sessions://active";
            tOptions.sSummary = pToolResult->sVisibleSummary;
            tOptions.eOutputClass = XWORK_ARTIFACT_OUTPUT_TERMINAL_INVENTORY;
            tOptions.sOutputRole = XWORK_TOOL_PROCESS_LIST_TERMINALS;
            tOptions.sOutputText = pToolResult->sOutputText;
            iStatus = xwork_run_emit_output_artifact(pRun, &tOptions, NULL);
        }
    } else if ( strcmp(pToolDef->sToolId, XWORK_TOOL_VCS_STATUS) == 0 ||
                strcmp(pToolDef->sToolId, XWORK_TOOL_VCS_DIFF) == 0 ||
                strcmp(pToolDef->sToolId, XWORK_TOOL_VCS_LOG) == 0 ||
                strcmp(pToolDef->sToolId, XWORK_TOOL_VCS_BRANCH) == 0 ) {
        xwork_command_artifact_options tOptions;
        const char *sVcsText = xwork__json_table_get_text(tResult, "status");
        const char *sPath = xwork__json_table_get_text(tResult, "resolved_path");
        const char *sArtifactName = "git-status.txt";
        const char *sCommandText = "git status --short --branch";

        if ( !sPath ) {
            sPath = xwork__json_table_get_text(tArguments, "path");
        }
        if ( strcmp(pToolDef->sToolId, XWORK_TOOL_VCS_DIFF) == 0 ) {
            sVcsText = xwork__json_table_get_text(tResult, "diff");
            sArtifactName = "git-diff.txt";
            sCommandText = xwork__json_table_get_text(tResult, "command");
        } else if ( strcmp(pToolDef->sToolId, XWORK_TOOL_VCS_LOG) == 0 ) {
            sVcsText = xwork__json_table_get_text(tResult, "log");
            sArtifactName = "git-log.txt";
            sCommandText = "git log --oneline";
        } else if ( strcmp(pToolDef->sToolId, XWORK_TOOL_VCS_BRANCH) == 0 ) {
            sVcsText = xwork__json_table_get_text(tResult, "branch_status");
            sArtifactName = "git-branch.txt";
            sCommandText = "git status --short --branch";
        }
        if ( sVcsText ) {
            xwork_command_artifact_options_init(&tOptions);
            tOptions.sName = sArtifactName;
            tOptions.sStorageRef = sPath;
            tOptions.sSummary = pToolResult->sVisibleSummary;
            tOptions.sCommandText = sCommandText ? sCommandText : pToolDef->sToolId;
            tOptions.sOutputText = sVcsText;
            iStatus = xwork_run_emit_command_artifact(pRun, &tOptions, NULL);
        }
    }

cleanup:
    free(sCombinedOutput);
    free(sTerminalOutput);
    if ( tArguments ) {
        xvoUnref(tArguments);
    }
    if ( tResult ) {
        xvoUnref(tResult);
    }
    return iStatus;
}

static xwork_status xwork__execute_tool(
    xwork_run *pRun,
    const xwork_tool_def *pToolDef,
    const xwork_tool_call *pToolCall,
    xwork_tool_result *pToolResult,
    const xwork_orchestrator_options *pOptions
)
{
    if ( !pRun || !pToolDef || !pToolCall || !pToolResult || !pOptions ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    if ( pOptions->pfnToolExecEx ) {
        xwork_tool_exec_context tToolContext;

        memset(&tToolContext, 0, sizeof(tToolContext));
        tToolContext.pCancelToken = pOptions->pCancelToken;
        tToolContext.pfnShouldInterrupt = pOptions->pfnShouldInterrupt;
        tToolContext.pInterruptUserData = pOptions->pInterruptUserData;
        tToolContext.sPhase = "tool_execution";

        return pOptions->pfnToolExecEx(
            pRun,
            pToolCall,
            &tToolContext,
            pToolResult,
            pOptions->pUserData
        );
    }

    if ( pOptions->pfnToolExec ) {
        return pOptions->pfnToolExec(pRun, pToolCall, pToolResult, pOptions->pUserData);
    }

    if ( pToolDef->eKind == XWORK_TOOL_HOST_SERVICE &&
         pToolDef->eHostService != XWORK_HOST_NONE &&
         pToolDef->sOperationId &&
         pToolDef->sOperationId[0] ) {
        xwork_host_invoke_context tHostContext;

        memset(&tHostContext, 0, sizeof(tHostContext));
        tHostContext.pRun = pRun;
        tHostContext.pCancelToken = pOptions->pCancelToken;
        tHostContext.pfnShouldInterrupt = pOptions->pfnShouldInterrupt;
        tHostContext.pInterruptUserData = pOptions->pInterruptUserData;
        tHostContext.sPhase = "tool_execution";

        return xwork_runtime_invoke_host_service_ex(
            pRun->pRuntime,
            pToolDef->eHostService,
            pToolDef->sOperationId,
            pToolCall->sArgumentsJson,
            &tHostContext,
            pToolResult
        );
    }

    return XWORK_ERROR_UNSUPPORTED;
}

static xwork_status xwork__execute_tool_with_retry(
    xwork_run *pRun,
    const xwork_tool_def *pToolDef,
    const xwork_tool_call *pToolCall,
    xwork_tool_result *pToolResult,
    const xwork_orchestrator_options *pOptions
)
{
    size_t iAttempt;
    xwork_status iStatus;

    if ( !pRun || !pToolDef || !pToolCall || !pToolResult || !pOptions ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    for ( iAttempt = 0u; ; ++iAttempt ) {
        xwork_tool_result_init(pToolResult);
        iStatus = xwork__execute_tool(pRun, pToolDef, pToolCall, pToolResult, pOptions);
        if ( iStatus == XWORK_OK ||
             iAttempt >= pOptions->iMaxRetries ||
             !xwork__tool_status_is_retryable(iStatus, pToolResult) ) {
            return iStatus;
        }

        iStatus = xwork__store_tool_result(pRun, pToolResult);
        if ( iStatus != XWORK_OK ) {
            return iStatus;
        }
        iStatus = xwork__record_retry_checkpoint(
            pRun,
            pToolDef->sToolId,
            "retry_tool",
            pToolResult->sVisibleSummary ?
                pToolResult->sVisibleSummary :
                "Retrying tool execution after retryable failure.",
            "Checkpoint saved before retrying tool execution."
        );
        if ( iStatus != XWORK_OK ) {
            return iStatus;
        }

        xwork__retry_backoff_delay(pOptions);
        iStatus = xwork__check_interrupt_or_cancelled(
            pRun,
            pOptions,
            "before_tool_retry",
            "retry_tool"
        );
        if ( iStatus != XWORK_OK ) {
            return iStatus;
        }
    }
}

static xwork_status xwork__resume_pending_tool(
    xwork_run *pRun,
    const xwork_orchestrator_options *pOptions
)
{
    const xwork_tool_def *pToolDef;
    xwork_tool_call tToolCall;
    xwork_tool_result tToolResult;
    xwork_status iStatus;
    bool bPendingToolApprovalLinked = false;
    size_t iArtifactCountBefore = 0u;

    if ( !pRun || !pOptions ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !xwork__orchestrator_has_pending_tool_call(pRun) ) {
        return XWORK_OK;
    }

    bPendingToolApprovalLinked =
        pRun->bHasLastApprovalRequest &&
        pRun->sLastApprovalRequestId &&
        pRun->sLastApprovalToolId &&
        strcmp(pRun->sLastApprovalToolId, pRun->sLastToolId) == 0;

    if ( bPendingToolApprovalLinked ) {
        if ( pRun->eLastApprovalState == XWORK_APPROVAL_PENDING ) {
            return XWORK_ERROR_INVALID_STATE;
        }
        if ( pRun->eLastApprovalState != XWORK_APPROVAL_APPROVED ) {
            return XWORK_ERROR_INVALID_STATE;
        }
    }

    pToolDef = xwork_runtime_find_tool(pRun->pRuntime, pRun->sLastToolId);
    if ( !pToolDef ) {
        return xwork__fail_run_with_summary(
            pRun,
            "Resumed run references an unknown tool."
        );
    }

    memset(&tToolCall, 0, sizeof(tToolCall));
    xwork_tool_result_init(&tToolResult);
    tToolCall.sCallId = pRun->sLastToolCallId;
    tToolCall.sToolId = pRun->sLastToolId;
    tToolCall.sArgumentsJson = pRun->sLastToolArgumentsJson;

    pRun->eState = XWORK_RUN_WAITING_TOOL;
    iStatus = xwork__run_record_event(
        pRun,
        XWORK_EVENT_TOOL_EXEC_STARTED,
        pRun->sLastToolId,
        pRun->sLastApprovalRequestId,
        pRun->sLastCheckpointId,
        "Resuming pending tool execution."
    );
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    pRun->eState = XWORK_RUN_RUNNING;
    iArtifactCountBefore = pRun->iArtifactCount;
    iStatus = xwork__execute_tool_with_retry(
        pRun,
        pToolDef,
        &tToolCall,
        &tToolResult,
        pOptions
    );
    if ( iStatus != XWORK_OK ) {
        if ( iStatus == XWORK_ERROR_CANCELLED ) {
            return xwork__cancel_run_with_checkpoint(
                pRun,
                "tool_cancelled",
                "tool_execution"
            );
        }
        xwork__fail_run_after_tool_error(
            pRun,
            &tToolResult,
            "Tool execution failed after resume."
        );
        return iStatus;
    }

    iStatus = xwork__store_tool_result(pRun, &tToolResult);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    iStatus = xwork__emit_builtin_tool_artifact(
        pRun,
        pToolDef,
        &tToolCall,
        &tToolResult
    );
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    if ( pOptions->bIngestToolResultsToMemory ) {
        iStatus = xwork__ingest_tool_result_to_memory(pRun, &tToolCall, &tToolResult);
        if ( iStatus != XWORK_OK ) {
            return iStatus;
        }
    }

    if ( pOptions->bIngestArtifactsToMemory && pRun->iArtifactCount > iArtifactCountBefore ) {
        iStatus = xwork__ingest_artifacts_to_memory_range(
            pRun,
            iArtifactCountBefore,
            pOptions
        );
        if ( iStatus != XWORK_OK ) {
            return iStatus;
        }
    }

    iStatus = xwork__run_record_event(
        pRun,
        XWORK_EVENT_TOOL_EXEC_COMPLETED,
        pRun->sLastToolId,
        pRun->sLastApprovalRequestId,
        pRun->sLastCheckpointId,
        tToolResult.sVisibleSummary ? tToolResult.sVisibleSummary : "Tool execution completed."
    );
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    return xwork__save_checkpoint(
        pRun,
        XWORK_CHECKPOINT_AFTER_TOOL,
        "model_turn",
        pRun->sLastToolResultText,
        "Checkpoint saved after resumed tool execution."
    );
}

static xwork_status xwork__run_execute_body(
    xwork_run *pRun,
    const xwork_orchestrator_options *pOptions
)
{
    xwork_orchestrator_options tDefaultOptions;
    const xwork_orchestrator_options *pExecOptions = pOptions;
    size_t iTurn;

    if ( !pRun || !pRun->pRuntime ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    if ( !pExecOptions ) {
        xwork_orchestrator_options_init(&tDefaultOptions);
        pExecOptions = &tDefaultOptions;
    }

    if ( pExecOptions->iMaxTurns == 0u ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( pExecOptions->eToolChoiceMode == XWORK_TOOL_CHOICE_NAMED &&
         (!pExecOptions->sToolChoiceToolId || !pExecOptions->sToolChoiceToolId[0]) ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !pRun->pRuntime->pLlmRuntime ||
         !xwork__run_session_profile_id(pRun) ||
         !xwork__run_session_profile_id(pRun)[0] ) {
        return XWORK_ERROR_UNSUPPORTED;
    }
    if ( pRun->eState == XWORK_RUN_CREATED ||
         pRun->eState == XWORK_RUN_READY ||
         pRun->eState == XWORK_RUN_PAUSED ) {
        xwork_status iStartStatus = xwork_run_start(pRun);
        if ( iStartStatus != XWORK_OK ) {
            return iStartStatus;
        }
    } else if ( pRun->eState != XWORK_RUN_RUNNING ) {
        return XWORK_ERROR_INVALID_STATE;
    }

    {
        xwork_status iCancelStatus = xwork__check_interrupt_or_cancelled(
            pRun,
            pExecOptions,
            "run_start",
            "run_start"
        );
        if ( iCancelStatus != XWORK_OK ) {
            return iCancelStatus;
        }
    }

    if ( xwork__orchestrator_has_pending_tool_call(pRun) ) {
        xwork_status iCancelStatus = xwork__check_interrupt_or_cancelled(
            pRun,
            pExecOptions,
            "before_resume_tool",
            "resume_tool"
        );
        if ( iCancelStatus != XWORK_OK ) {
            return iCancelStatus;
        }
        {
            xwork_status iResumeStatus = xwork__resume_pending_tool(pRun, pExecOptions);
            if ( iResumeStatus != XWORK_OK ) {
                return iResumeStatus;
            }
        }
        iCancelStatus = xwork__check_interrupt_or_cancelled(
            pRun,
            pExecOptions,
            "after_resume_tool",
            "resume_tool"
        );
        if ( iCancelStatus != XWORK_OK ) {
            return iCancelStatus;
        }
    }

    for ( iTurn = 0u; iTurn < pExecOptions->iMaxTurns; ++iTurn ) {
        xllm_request tMemoryRequest;
        xllm_turn tTurn;
        xllm_error tError;
        xllm_call_options tCallOptions;
        xwork__model_event_bridge_ctx tModelEventCtx;
        xllm_response *pResponse = NULL;
        xllm_message atMessages[2];
        xllm_content_part atParts[2];
        xllm_context_block atContextBlocks[1];
        xllm_message atContextMessages[1];
        xllm_content_part atContextParts[1];
        xwork_memory_context tMemoryContext;
        xwork__session_compaction_metrics tCompactionBefore;
        xwork__session_compaction_metrics tCompactionAfter;
        xllm_compact_result tCompactResult;
        xllm_tool_def *pTools = NULL;
        size_t iToolCount = 0u;
        size_t iMessageCount = 0u;
        size_t iPartCount = 0u;
        size_t iContextBlockCount = 0u;
        xwork_status iStatus;
        int iChatStatus;
        bool bCanAttachMemoryContext;
        bool bUseDefaultWorkspaceMemory;
        bool bHasToolFollowup;
        bool bHaveCompactionBefore = false;
        bool bHaveCompactionAfter = false;
        const xllm_output_tool_call *pToolCall = NULL;
        const char *sVisibleText = NULL;
        const char *sPlannerSystemText = NULL;

        xllm_request_init(&tMemoryRequest);
        xllm_error_init(&tError);
        xllm_call_options_init(&tCallOptions);
        xwork_memory_context_init(&tMemoryContext);
        memset(&tCompactResult, 0, sizeof(tCompactResult));
        memset(&tTurn, 0, sizeof(tTurn));
        memset(&tModelEventCtx, 0, sizeof(tModelEventCtx));
        memset(atMessages, 0, sizeof(atMessages));
        memset(atParts, 0, sizeof(atParts));
        memset(atContextBlocks, 0, sizeof(atContextBlocks));
        memset(atContextMessages, 0, sizeof(atContextMessages));
        memset(atContextParts, 0, sizeof(atContextParts));

        iStatus = xwork__check_interrupt_or_cancelled(
            pRun,
            pExecOptions,
            "turn_start",
            "model_turn"
        );
        if ( iStatus != XWORK_OK ) {
            xllm_request_reset(&tMemoryRequest);
            xllm_error_free(&tError);
            xwork_memory_context_reset(&tMemoryContext);
            return iStatus;
        }

        iStatus = xwork__run_ensure_session(pRun);
        if ( iStatus != XWORK_OK ) {
            xllm_request_reset(&tMemoryRequest);
            xllm_error_free(&tError);
            xwork_memory_context_reset(&tMemoryContext);
            return iStatus;
        }

        iStatus = xwork__build_request_tools(pRun->pRuntime, &pTools, &iToolCount);
        if ( iStatus != XWORK_OK ) {
            xllm_request_reset(&tMemoryRequest);
            xllm_error_free(&tError);
            xwork_memory_context_reset(&tMemoryContext);
            return iStatus;
        }

        bCanAttachMemoryContext = pExecOptions->pfnResolveMemoryContext &&
                                  xwork__run_has_memory_enabled_workspace(pRun);
        bUseDefaultWorkspaceMemory = !pExecOptions->pfnResolveMemoryContext;
        bHasToolFollowup = pRun->bHasLastToolCall && pRun->bHasLastToolResult;
        iStatus = xwork__run_set_last_memory_context(pRun, NULL);
        if ( iStatus != XWORK_OK ) {
            free(pTools);
            xllm_request_reset(&tMemoryRequest);
            xllm_error_free(&tError);
            xwork_memory_context_reset(&tMemoryContext);
            return iStatus;
        }

        if ( bCanAttachMemoryContext ) {
            iStatus = pExecOptions->pfnResolveMemoryContext(
                pRun,
                &tMemoryContext,
                pExecOptions->pMemoryUserData
            );
            if ( iStatus != XWORK_OK ) {
                free(pTools);
                xllm_request_reset(&tMemoryRequest);
                xllm_error_free(&tError);
                xwork_memory_context_reset(&tMemoryContext);
                return xwork__fail_run_with_summary(
                    pRun,
                    "Failed to resolve memory context."
                );
            }
            if ( tMemoryContext.sText && tMemoryContext.sText[0] ) {
                iStatus = xwork__run_set_last_memory_context(pRun, &tMemoryContext);
                if ( iStatus != XWORK_OK ) {
                    free(pTools);
                    xllm_request_reset(&tMemoryRequest);
                    xllm_error_free(&tError);
                    xwork_memory_context_reset(&tMemoryContext);
                    return iStatus;
                }
                iStatus = xwork__run_record_event(
                    pRun,
                    XWORK_EVENT_MEMORY_CONTEXT_ATTACHED,
                    pRun->sLastToolId,
                    pRun->sLastApprovalRequestId,
                    pRun->sLastCheckpointId,
                    "Memory context attached."
                );
                if ( iStatus != XWORK_OK ) {
                    free(pTools);
                    xllm_request_reset(&tMemoryRequest);
                    xllm_error_free(&tError);
                    xwork_memory_context_reset(&tMemoryContext);
                    return iStatus;
                }
            }
        }

        if ( bUseDefaultWorkspaceMemory ) {
            iStatus = xwork__apply_workspace_memory_context(
                pRun,
                pExecOptions,
                &tMemoryRequest,
                &tMemoryContext,
                &tError
            );
            if ( iStatus != XWORK_OK ) {
                char *sErrorSummary = xwork__dup_printf(
                    "Workspace memory search failed: %s",
                    tError.sMessage ? tError.sMessage : "unknown error"
                );
                free(pTools);
                xllm_request_reset(&tMemoryRequest);
                xllm_error_free(&tError);
                xwork_memory_context_reset(&tMemoryContext);
                iStatus = xwork__fail_run_with_summary(
                    pRun,
                    sErrorSummary ? sErrorSummary : "Workspace memory search failed."
                );
                free(sErrorSummary);
                return iStatus;
            }
            if ( tMemoryContext.sText && tMemoryContext.sText[0] ) {
                iStatus = xwork__run_set_last_memory_context(pRun, &tMemoryContext);
                if ( iStatus != XWORK_OK ) {
                    free(pTools);
                    xllm_request_reset(&tMemoryRequest);
                    xllm_error_free(&tError);
                    xwork_memory_context_reset(&tMemoryContext);
                    return iStatus;
                }
                iStatus = xwork__run_record_event(
                    pRun,
                    XWORK_EVENT_MEMORY_CONTEXT_ATTACHED,
                    pRun->sLastToolId,
                    pRun->sLastApprovalRequestId,
                    pRun->sLastCheckpointId,
                    "Memory context attached."
                );
                if ( iStatus != XWORK_OK ) {
                    free(pTools);
                    xllm_request_reset(&tMemoryRequest);
                    xllm_error_free(&tError);
                    xwork_memory_context_reset(&tMemoryContext);
                    return iStatus;
                }
            }
        }

        if ( tMemoryContext.sText && tMemoryContext.sText[0] ) {
            xwork__init_memory_context_block(
                &atContextBlocks[0],
                &atContextMessages[0],
                &atContextParts[0],
                tMemoryContext.sText,
                pExecOptions->iMemoryContextPriority,
                pExecOptions->bMemoryContextPinned
            );
            iContextBlockCount = 1u;
        }

        sPlannerSystemText = xwork__planner_system_text(pExecOptions);
        if ( sPlannerSystemText && sPlannerSystemText[0] ) {
            xwork__init_system_message(
                &atMessages[iMessageCount],
                &atParts[iPartCount],
                sPlannerSystemText
            );
            ++iMessageCount;
            ++iPartCount;
        }

        if ( bHasToolFollowup ) {
            xwork__init_tool_result_message(
                &atMessages[iMessageCount],
                &atParts[iPartCount],
                pRun
            );
            ++iMessageCount;
            ++iPartCount;
        } else {
            xwork__init_user_message(
                &atMessages[iMessageCount],
                &atParts[iPartCount],
                pRun->sInstruction
            );
            ++iMessageCount;
            ++iPartCount;
        }

        tTurn.pMessages = atMessages;
        tTurn.iMessageCount = iMessageCount;
        tTurn.pContextBlocks = iContextBlockCount ? atContextBlocks : NULL;
        tTurn.iContextBlockCount = iContextBlockCount;
        tTurn.pTools = pTools;
        tTurn.iToolCount = iToolCount;
        tTurn.tToolPolicy.eMode = xwork__to_xllm_tool_choice(pExecOptions->eToolChoiceMode);
        tTurn.tToolPolicy.sToolName = pExecOptions->sToolChoiceToolId;
        tTurn.tToolPolicy.bAllowParallel = pExecOptions->bAllowParallelToolCalls;

        iStatus = xwork__check_interrupt_or_cancelled(
            pRun,
            pExecOptions,
            "before_model",
            "model_turn"
        );
        if ( iStatus != XWORK_OK ) {
            free(pTools);
            xllm_request_reset(&tMemoryRequest);
            xllm_error_free(&tError);
            xwork_memory_context_reset(&tMemoryContext);
            return iStatus;
        }

        iStatus = xwork__run_record_event(
            pRun,
            XWORK_EVENT_MODEL_TURN_STARTED,
            pRun->sLastToolId,
            pRun->sLastApprovalRequestId,
            pRun->sLastCheckpointId,
            "Starting model turn."
        );
        if ( iStatus != XWORK_OK ) {
            free(pTools);
            xllm_request_reset(&tMemoryRequest);
            xllm_error_free(&tError);
            xwork_memory_context_reset(&tMemoryContext);
            return iStatus;
        }

        tCallOptions.eStreamMode = xwork__to_xllm_stream_mode(pExecOptions->eModelStreamMode);
        tCallOptions.pCancelToken = pExecOptions->pCancelToken;
        tModelEventCtx.pRun = pRun;
        tModelEventCtx.pOptions = pExecOptions;
        tModelEventCtx.pCancelToken = pExecOptions->pCancelToken;
        if ( pExecOptions->pfnModelEvent || pExecOptions->pfnShouldInterrupt ) {
            tCallOptions.pfnOnEvent = xwork__model_event_bridge;
            tCallOptions.pUserData = &tModelEventCtx;
        }

        if ( pRun->tSessionPolicy.bEnableAutoCompact ) {
            if ( xwork__session_get_compaction_metrics(
                     pRun->pSession,
                     &tCompactionBefore
                 ) == XWORK_OK ) {
                bHaveCompactionBefore = true;
            }
        }

        {
            size_t iModelAttempt;

            for ( iModelAttempt = 0u; ; ++iModelAttempt ) {
                if ( pResponse ) {
                    xllm_response_free(pResponse);
                    pResponse = NULL;
                }
                xllm_error_free(&tError);
                xllm_error_init(&tError);
                tModelEventCtx.bCancelledByCallback = false;

                iChatStatus = xllm_session_chat_ex(
                    pRun->pSession,
                    &tTurn,
                    &tCallOptions,
                    &pResponse,
                    &tError
                );
                if ( iChatStatus == XRT_NET_CANCELLED ||
                     tError.eCode == XLLM_ERROR_CANCELLED ||
                     tModelEventCtx.bCancelledByCallback ||
                     (pExecOptions->pCancelToken &&
                      xllm_cancel_token_is_cancelled(pExecOptions->pCancelToken)) ||
                     (iChatStatus == XRT_NET_OK && pResponse) ) {
                    break;
                }
                if ( iModelAttempt >= pExecOptions->iMaxRetries ) {
                    break;
                }

                iStatus = xwork__record_retry_checkpoint(
                    pRun,
                    pRun->sLastToolId,
                    "retry_model",
                    "Retrying model turn after provider failure.",
                    "Checkpoint saved before retrying model turn."
                );
                if ( iStatus != XWORK_OK ) {
                    free(pTools);
                    xllm_request_reset(&tMemoryRequest);
                    xllm_error_free(&tError);
                    xwork_memory_context_reset(&tMemoryContext);
                    return iStatus;
                }

                xwork__retry_backoff_delay(pExecOptions);
                iStatus = xwork__check_interrupt_or_cancelled(
                    pRun,
                    pExecOptions,
                    "before_model_retry",
                    "retry_model"
                );
                if ( iStatus != XWORK_OK ) {
                    free(pTools);
                    xllm_request_reset(&tMemoryRequest);
                    xllm_error_free(&tError);
                    xwork_memory_context_reset(&tMemoryContext);
                    return iStatus;
                }
            }
        }
        if ( iChatStatus == XRT_NET_CANCELLED ||
             tError.eCode == XLLM_ERROR_CANCELLED ||
             tModelEventCtx.bCancelledByCallback ||
             (pExecOptions->pCancelToken &&
              xllm_cancel_token_is_cancelled(pExecOptions->pCancelToken)) ) {
            xllm_response_free(pResponse);
            free(pTools);
            xllm_request_reset(&tMemoryRequest);
            xllm_error_free(&tError);
            xwork_memory_context_reset(&tMemoryContext);
            return xwork__cancel_run_with_checkpoint(
                pRun,
                "model_turn",
                "Model turn cancelled."
            );
        }
        if ( iChatStatus != XRT_NET_OK || !pResponse ) {
            char *sErrorSummary = xwork__format_model_turn_error_summary(&tError);

            free(pTools);
            xllm_request_reset(&tMemoryRequest);
            xllm_error_free(&tError);
            xwork_memory_context_reset(&tMemoryContext);
            iStatus = xwork__fail_run_with_summary(
                pRun,
                sErrorSummary ? sErrorSummary : "xllm session chat failed."
            );
            free(sErrorSummary);
            return iStatus == XWORK_OK ? XWORK_ERROR_EXTERNAL_FAILURE : iStatus;
        }

        if ( bHaveCompactionBefore ) {
            if ( xwork__session_get_compaction_metrics(
                     pRun->pSession,
                     &tCompactionAfter
                 ) == XWORK_OK ) {
                bHaveCompactionAfter = true;
            }
            if ( bHaveCompactionAfter &&
                 !xwork__session_compacted_between(&tCompactionBefore, &tCompactionAfter) &&
                 xwork__session_policy_should_compact_now(
                     &pRun->tSessionPolicy,
                     &tCompactionAfter
                 ) ) {
                memset(&tCompactResult, 0, sizeof(tCompactResult));
                tCompactionBefore = tCompactionAfter;
                if ( xllm_session_compact(
                         pRun->pSession,
                         NULL,
                         &tCompactResult
                     ) != XRT_NET_OK ) {
                    xllm_response_free(pResponse);
                    free(pTools);
                    xllm_request_reset(&tMemoryRequest);
                    xllm_error_free(&tError);
                    xwork_memory_context_reset(&tMemoryContext);
                    return xwork__fail_run_with_summary(
                        pRun,
                        "xllm session compaction failed."
                    );
                }
                if ( tCompactResult.bCompacted &&
                     xwork__session_get_compaction_metrics(
                         pRun->pSession,
                         &tCompactionAfter
                     ) == XWORK_OK ) {
                    bHaveCompactionAfter = true;
                }
            }
        }
        if ( bHaveCompactionBefore &&
             bHaveCompactionAfter &&
             xwork__session_compacted_between(&tCompactionBefore, &tCompactionAfter) ) {
            iStatus = xwork__record_session_compacted(
                pRun,
                &tCompactionBefore,
                &tCompactionAfter
            );
            if ( iStatus != XWORK_OK ) {
                xllm_response_free(pResponse);
                free(pTools);
                xllm_request_reset(&tMemoryRequest);
                xllm_error_free(&tError);
                xwork_memory_context_reset(&tMemoryContext);
                return iStatus;
            }
        }

        free(pTools);
        xllm_request_reset(&tMemoryRequest);
        xllm_error_free(&tError);
        xwork_memory_context_reset(&tMemoryContext);

        iStatus = xwork__check_interrupt_or_cancelled(
            pRun,
            pExecOptions,
            "after_model",
            "model_turn"
        );
        if ( iStatus != XWORK_OK ) {
            xllm_response_free(pResponse);
            return iStatus;
        }

        sVisibleText = xllm_response_get_text(pResponse);
        if ( xllm_response_get_tool_call_count(pResponse) > 0u ) {
            pToolCall = xllm_response_get_tool_call(pResponse, 0u);
        }

        if ( pToolCall ) {
            const char *sToolId = xwork__tool_call_id(pToolCall);
            const xwork_tool_def *pToolDef = xwork_runtime_find_tool(pRun->pRuntime, sToolId);
            xwork_tool_call tToolCall;
            xwork_tool_result tToolResult;
            xwork_approval_decision tApprovalDecision;
            size_t iArtifactCountBefore = 0u;

            if ( !sToolId || !pToolDef ) {
                xllm_response_free(pResponse);
                return xwork__fail_run_with_summary(
                    pRun,
                    "Model requested an unknown tool."
                );
            }

            iStatus = xwork__store_tool_call(pRun, pToolCall);
            if ( iStatus != XWORK_OK ) {
                xllm_response_free(pResponse);
                return iStatus;
            }

            iStatus = xwork__run_record_event(
                pRun,
                XWORK_EVENT_MODEL_TURN_COMPLETED,
                sToolId,
                pRun->sLastApprovalRequestId,
                pRun->sLastCheckpointId,
                "Model requested a tool call."
            );
            if ( iStatus != XWORK_OK ) {
                xllm_response_free(pResponse);
                return iStatus;
            }

            iStatus = xwork__run_record_event(
                pRun,
                XWORK_EVENT_TOOL_CALL_REQUESTED,
                sToolId,
                pRun->sLastApprovalRequestId,
                pRun->sLastCheckpointId,
                "Tool call requested."
            );
            if ( iStatus != XWORK_OK ) {
                xllm_response_free(pResponse);
                return iStatus;
            }

            iStatus = xwork__check_interrupt_or_cancelled(
                pRun,
                pExecOptions,
                "before_approval",
                "approval"
            );
            if ( iStatus != XWORK_OK ) {
                xllm_response_free(pResponse);
                return iStatus;
            }

            xwork_approval_decision_init(&tApprovalDecision);
            iStatus = xwork__evaluate_tool_approval(
                pRun,
                pToolDef,
                pToolCall,
                pExecOptions,
                &tApprovalDecision
            );
            if ( iStatus != XWORK_OK ) {
                xllm_response_free(pResponse);
                return iStatus;
            }

            if ( tApprovalDecision.bRequiresApproval ) {
                pRun->eState = XWORK_RUN_WAITING_APPROVAL;
                iStatus = xwork__run_record_approval_request(
                    pRun,
                    NULL,
                    sToolId,
                    tApprovalDecision.sReason,
                    tApprovalDecision.eRiskLevel,
                    tApprovalDecision.sScope,
                    pToolDef->sDescription ? pToolDef->sDescription : sToolId,
                    XWORK_APPROVAL_PENDING
                );
                if ( iStatus != XWORK_OK ) {
                    xllm_response_free(pResponse);
                    return iStatus;
                }

                iStatus = xwork__run_record_event(
                    pRun,
                    XWORK_EVENT_APPROVAL_REQUESTED,
                    sToolId,
                    pRun->sLastApprovalRequestId,
                    pRun->sLastCheckpointId,
                    "Approval requested for tool execution."
                );
                if ( iStatus != XWORK_OK ) {
                    xllm_response_free(pResponse);
                    return iStatus;
                }

                if ( !tApprovalDecision.bAutoApproved ) {
                    xllm_response_free(pResponse);
                    return xwork__save_checkpoint(
                        pRun,
                        XWORK_CHECKPOINT_BEFORE_TOOL,
                        "approval",
                        NULL,
                        "Checkpoint saved while waiting for approval."
                    );
                }

                pRun->eState = XWORK_RUN_RUNNING;
                iStatus = xwork__run_record_approval_request(
                    pRun,
                    pRun->sLastApprovalRequestId,
                    sToolId,
                    tApprovalDecision.sReason,
                    tApprovalDecision.eRiskLevel,
                    tApprovalDecision.sScope,
                    pToolDef->sDescription ? pToolDef->sDescription : sToolId,
                    XWORK_APPROVAL_APPROVED
                );
                if ( iStatus != XWORK_OK ) {
                    xllm_response_free(pResponse);
                    return iStatus;
                }

                iStatus = xwork__run_record_event(
                    pRun,
                    XWORK_EVENT_APPROVAL_RESOLVED,
                    sToolId,
                    pRun->sLastApprovalRequestId,
                    pRun->sLastCheckpointId,
                    "Approval automatically granted."
                );
                if ( iStatus != XWORK_OK ) {
                    xllm_response_free(pResponse);
                    return iStatus;
                }
            }

            iStatus = xwork__check_interrupt_or_cancelled(
                pRun,
                pExecOptions,
                "before_tool",
                "tool_execution"
            );
            if ( iStatus != XWORK_OK ) {
                xllm_response_free(pResponse);
                return iStatus;
            }

            memset(&tToolCall, 0, sizeof(tToolCall));
            xwork_tool_result_init(&tToolResult);
            tToolCall.sCallId = pToolCall->sCallId;
            tToolCall.sToolId = sToolId;
            tToolCall.sArgumentsJson = pToolCall->sArgumentsJson;

            pRun->eState = XWORK_RUN_WAITING_TOOL;
            iStatus = xwork__run_record_event(
                pRun,
                XWORK_EVENT_TOOL_EXEC_STARTED,
                sToolId,
                pRun->sLastApprovalRequestId,
                pRun->sLastCheckpointId,
                "Starting tool execution."
            );
            if ( iStatus != XWORK_OK ) {
                xllm_response_free(pResponse);
                return iStatus;
            }

            pRun->eState = XWORK_RUN_RUNNING;
            iArtifactCountBefore = pRun->iArtifactCount;
            iStatus = xwork__execute_tool_with_retry(
                pRun,
                pToolDef,
                &tToolCall,
                &tToolResult,
                pExecOptions
            );
            if ( iStatus != XWORK_OK ) {
                xllm_response_free(pResponse);
                if ( iStatus == XWORK_ERROR_CANCELLED ) {
                    return xwork__cancel_run_with_checkpoint(
                        pRun,
                        "tool_cancelled",
                        "tool_execution"
                    );
                }
                xwork__fail_run_after_tool_error(
                    pRun,
                    &tToolResult,
                    "Tool execution failed."
                );
                return iStatus;
            }

            iStatus = xwork__check_interrupt_or_cancelled(
                pRun,
                pExecOptions,
                "after_tool",
                "tool_execution"
            );
            if ( iStatus != XWORK_OK ) {
                xllm_response_free(pResponse);
                return iStatus;
            }

            iStatus = xwork__store_tool_result(pRun, &tToolResult);
            if ( iStatus != XWORK_OK ) {
                xllm_response_free(pResponse);
                return iStatus;
            }

            iStatus = xwork__emit_builtin_tool_artifact(
                pRun,
                pToolDef,
                &tToolCall,
                &tToolResult
            );
            if ( iStatus != XWORK_OK ) {
                xllm_response_free(pResponse);
                return iStatus;
            }

            if ( pExecOptions->bIngestToolResultsToMemory ) {
                iStatus = xwork__ingest_tool_result_to_memory(pRun, &tToolCall, &tToolResult);
                if ( iStatus != XWORK_OK ) {
                    xllm_response_free(pResponse);
                    return iStatus;
                }
            }

            if ( pExecOptions->bIngestArtifactsToMemory &&
                 pRun->iArtifactCount > iArtifactCountBefore ) {
                iStatus = xwork__ingest_artifacts_to_memory_range(
                    pRun,
                    iArtifactCountBefore,
                    pExecOptions
                );
                if ( iStatus != XWORK_OK ) {
                    xllm_response_free(pResponse);
                    return iStatus;
                }
            }

            iStatus = xwork__run_record_event(
                pRun,
                XWORK_EVENT_TOOL_EXEC_COMPLETED,
                sToolId,
                pRun->sLastApprovalRequestId,
                pRun->sLastCheckpointId,
                tToolResult.sVisibleSummary ? tToolResult.sVisibleSummary : "Tool execution completed."
            );
            if ( iStatus != XWORK_OK ) {
                xllm_response_free(pResponse);
                return iStatus;
            }

            iStatus = xwork__save_checkpoint(
                pRun,
                XWORK_CHECKPOINT_AFTER_TOOL,
                "model_turn",
                pRun->sLastToolResultText,
                "Checkpoint saved after tool execution."
            );
            xllm_response_free(pResponse);
            if ( iStatus != XWORK_OK ) {
                return iStatus;
            }
            continue;
        }

        iStatus = xwork__run_record_event(
            pRun,
            XWORK_EVENT_MODEL_TURN_COMPLETED,
            pRun->sLastToolId,
            pRun->sLastApprovalRequestId,
            pRun->sLastCheckpointId,
            sVisibleText && sVisibleText[0] ? sVisibleText : "Model turn completed."
        );
        if ( iStatus != XWORK_OK ) {
            xllm_response_free(pResponse);
            return iStatus;
        }

        iStatus = xwork__store_final_output(pRun, sVisibleText);
        if ( iStatus != XWORK_OK ) {
            xllm_response_free(pResponse);
            return iStatus;
        }

        pRun->eState = XWORK_RUN_COMPLETED;
        iStatus = xwork__save_checkpoint(
            pRun,
            XWORK_CHECKPOINT_COMPLETION,
            "done",
            pRun->sLastToolResultText,
            "Checkpoint saved at completion."
        );
        xllm_response_free(pResponse);
        if ( iStatus != XWORK_OK ) {
            return iStatus;
        }

        return xwork__run_record_event(
            pRun,
            XWORK_EVENT_RUN_COMPLETED,
            pRun->sLastToolId,
            pRun->sLastApprovalRequestId,
            pRun->sLastCheckpointId,
            "Run completed."
        );
    }

    return xwork__record_run_state_event(
        pRun,
        XWORK_RUN_PAUSED,
        XWORK_EVENT_RUN_PAUSED,
        "Run paused after reaching the max turn budget."
    );
}

xwork_status xwork_run_execute(
    xwork_run *pRun,
    const xwork_orchestrator_options *pOptions
)
{
    xwork_status iStatus;

    iStatus = xwork__run_begin_execution(pRun);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    iStatus = xwork__run_execute_body(pRun, pOptions);
    xwork__run_end_execution(pRun);
    return iStatus;
}

static uint32 xwork__run_async_thread_proc(ptr pUserData)
{
    xwork_run_async *pAsync = (xwork_run_async *)pUserData;
    xwork_status iStatus;

    if ( !pAsync ) {
        return 1u;
    }

    iStatus = xwork_run_execute(pAsync->pRun, &pAsync->tOptions);
    xrtMutexLock(&pAsync->tLock);
    pAsync->iStatus = iStatus;
    pAsync->bCompleted = true;
    xrtMutexUnlock(&pAsync->tLock);

    return iStatus == XWORK_OK ? 0u : 1u;
}

static xwork_status xwork__run_async_read_status(
    const xwork_run_async *pAsync,
    xwork_status *pStatus,
    bool *pbCompleted
)
{
    xwork_run_async *pMutableAsync = (xwork_run_async *)pAsync;

    if ( !pAsync ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xrtMutexLock(&pMutableAsync->tLock);
    if ( pStatus ) {
        *pStatus = pMutableAsync->iStatus;
    }
    if ( pbCompleted ) {
        *pbCompleted = pMutableAsync->bCompleted;
    }
    xrtMutexUnlock(&pMutableAsync->tLock);
    return XWORK_OK;
}

static uint32 xwork__run_async_timeout_ms(size_t iTimeoutMs)
{
    if ( iTimeoutMs > (size_t)0xffffffffu ) {
        return 0xffffffffu;
    }
    return (uint32)iTimeoutMs;
}

static ptr xwork__run_async_thread_proc_ptr(uint32 (*pProc)(ptr))
{
    ptr pValue = NULL;

    if ( sizeof(pValue) < sizeof(pProc) ) {
        return NULL;
    }
    memcpy(&pValue, &pProc, sizeof(pProc));
    return pValue;
}

xwork_status xwork_run_execute_async(
    xwork_run *pRun,
    const xwork_orchestrator_options *pOptions,
    xwork_run_async **ppAsync
)
{
    xwork_orchestrator_options tDefaultOptions;
    const xwork_orchestrator_options *pExecOptions = pOptions;
    xwork_run_async *pAsync;
    ptr pThreadProc;

    if ( !pRun || !pRun->pRuntime || !ppAsync ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    *ppAsync = NULL;
    if ( !pExecOptions ) {
        xwork_orchestrator_options_init(&tDefaultOptions);
        pExecOptions = &tDefaultOptions;
    }
    if ( pExecOptions->iMaxTurns == 0u ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( pExecOptions->eToolChoiceMode == XWORK_TOOL_CHOICE_NAMED &&
         (!pExecOptions->sToolChoiceToolId || !pExecOptions->sToolChoiceToolId[0]) ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    pAsync = (xwork_run_async *)calloc(1u, sizeof(*pAsync));
    if ( !pAsync ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    pAsync->pRun = pRun;
    pAsync->tOptions = *pExecOptions;
    pAsync->iStatus = XWORK_ERROR_EXTERNAL_FAILURE;
    xrtMutexInit(&pAsync->tLock);
    pAsync->bLockInitialized = true;

    if ( !pAsync->tOptions.pCancelToken ) {
        if ( xllm_cancel_token_create(&pAsync->pOwnedCancelToken) != XRT_NET_OK ||
             !pAsync->pOwnedCancelToken ) {
            xrtMutexUnit(&pAsync->tLock);
            free(pAsync);
            return XWORK_ERROR_NO_MEMORY;
        }
        pAsync->tOptions.pCancelToken = pAsync->pOwnedCancelToken;
    }

    pThreadProc = xwork__run_async_thread_proc_ptr(xwork__run_async_thread_proc);
    if ( !pThreadProc ) {
        if ( pAsync->pOwnedCancelToken ) {
            xllm_cancel_token_destroy(pAsync->pOwnedCancelToken);
        }
        xrtMutexUnit(&pAsync->tLock);
        free(pAsync);
        return XWORK_ERROR_UNSUPPORTED;
    }
    pAsync->pThread = xrtThreadCreate(pThreadProc, pAsync, 0u);
    if ( !pAsync->pThread ) {
        if ( pAsync->pOwnedCancelToken ) {
            xllm_cancel_token_destroy(pAsync->pOwnedCancelToken);
        }
        xrtMutexUnit(&pAsync->tLock);
        free(pAsync);
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }

    *ppAsync = pAsync;
    return XWORK_OK;
}

xwork_status xwork_run_async_wait(xwork_run_async *pAsync)
{
    xwork_status iRunStatus;
    xwork_status iStatus;
    bool bCompleted;

    if ( !pAsync ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xrtThreadWait(pAsync->pThread);
    iStatus = xwork__run_async_read_status(pAsync, &iRunStatus, &bCompleted);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    if ( !bCompleted ) {
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }
    return iRunStatus;
}

xwork_status xwork_run_async_wait_timeout(
    xwork_run_async *pAsync,
    size_t iTimeoutMs,
    bool *pbCompleted
)
{
    int iWaitStatus;
    xwork_status iRunStatus;
    xwork_status iStatus;
    bool bCompleted;

    if ( !pAsync || !pbCompleted ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    *pbCompleted = false;
    iWaitStatus = xrtThreadWaitTimeout(
        pAsync->pThread,
        xwork__run_async_timeout_ms(iTimeoutMs)
    );
    if ( iWaitStatus == XRT_WAIT_TIMEOUT ) {
        return XWORK_OK;
    }
    if ( iWaitStatus != XRT_WAIT_OK ) {
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }

    iStatus = xwork__run_async_read_status(pAsync, &iRunStatus, &bCompleted);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    *pbCompleted = bCompleted;
    return bCompleted ? iRunStatus : XWORK_ERROR_EXTERNAL_FAILURE;
}

xwork_status xwork_run_async_get_status(
    const xwork_run_async *pAsync,
    xwork_status *pStatus,
    bool *pbCompleted
)
{
    if ( !pAsync || !pStatus || !pbCompleted ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    return xwork__run_async_read_status(pAsync, pStatus, pbCompleted);
}

xwork_status xwork_run_async_cancel(
    xwork_run_async *pAsync,
    const char *sReason
)
{
    bool bCompleted;

    if ( !pAsync ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xrtMutexLock(&pAsync->tLock);
    bCompleted = pAsync->bCompleted;
    pAsync->bCancelRequested = true;
    xrtMutexUnlock(&pAsync->tLock);

    if ( bCompleted ) {
        return XWORK_OK;
    }

    if ( pAsync->tOptions.pCancelToken ) {
        xllm_cancel_token_cancel(
            pAsync->tOptions.pCancelToken,
            sReason ? sReason : "xwork async run cancelled."
        );
    }
    if ( pAsync->pThread ) {
        xrtThreadStop(pAsync->pThread);
    }
    return XWORK_OK;
}

void xwork_run_async_destroy(xwork_run_async *pAsync)
{
    bool bCompleted = false;

    if ( !pAsync ) {
        return;
    }

    (void)xwork__run_async_read_status(pAsync, NULL, &bCompleted);
    if ( !bCompleted ) {
        (void)xwork_run_async_cancel(pAsync, "xwork async handle destroyed.");
        xrtThreadWait(pAsync->pThread);
    }

    if ( pAsync->pThread ) {
        xrtThreadDestroy(pAsync->pThread);
    }
    if ( pAsync->pOwnedCancelToken ) {
        xllm_cancel_token_destroy(pAsync->pOwnedCancelToken);
    }
    if ( pAsync->bLockInitialized ) {
        xrtMutexUnit(&pAsync->tLock);
    }
    free(pAsync);
}
