#include "xllm_session.h"

#include <stdarg.h>

typedef struct {
    xllm *pLlm;
    xllm_turn tTurn;
    xllm_call_options tOptions;
    bool bHasOptions;
} xllm__send_async_task;

typedef struct {
    xllm_session *pSession;
    xllm_turn tTurn;
    xllm_call_options tOptions;
    bool bHasOptions;
} xllm__session_async_chat_task;

#define XLLM__AUTO_TOOL_LOOP_MAX_ROUNDS 8u

static int xllm__session_options_clone(xllm_session_options *pOut, const xllm_session_options *pIn)
{
    if ( !pOut || !pIn ) {
        return XRT_NET_ERROR;
    }

    memset(pOut, 0, sizeof(*pOut));
    *pOut = *pIn;
    pOut->sProfileId = xllm__dup_cstr(pIn->sProfileId);
    pOut->sSystemPrompt = xllm__dup_cstr(pIn->sSystemPrompt);
    pOut->sSummarizerProfileId = xllm__dup_cstr(pIn->sSummarizerProfileId);
    pOut->tVendorExtra = pIn->tVendorExtra;
    xllm__xvalue_addref(pOut->tVendorExtra);
    return XRT_NET_OK;
}

static void xllm__session_options_reset(xllm_session_options *pOptions)
{
    if ( !pOptions ) {
        return;
    }

    xllm__free_cstr((char **)&pOptions->sProfileId);
    xllm__free_cstr((char **)&pOptions->sSystemPrompt);
    xllm__free_cstr((char **)&pOptions->sSummarizerProfileId);
    xllm__xvalue_release(&pOptions->tVendorExtra);
    memset(pOptions, 0, sizeof(*pOptions));
}

static void xllm__session_state_init_defaults(xllm_session_state *pState)
{
    xllm_session_options tDefaults;

    if ( !pState ) {
        return;
    }

    memset(pState, 0, sizeof(*pState));
    xllm_session_options_init(&tDefaults);
    pState->bEnableAutoCompact = tDefaults.bEnableAutoCompact;
    pState->fCompactTriggerRatio = tDefaults.fCompactTriggerRatio;
    pState->uCompactTriggerTurns = tDefaults.uCompactTriggerTurns;
    pState->uReserveOutputTokens = tDefaults.uReserveOutputTokens;
    pState->uKeepRecentTurns = tDefaults.uKeepRecentTurns;
    pState->bKeepActiveToolChain = tDefaults.bKeepActiveToolChain;
    pState->eCompactStrategy = tDefaults.eCompactStrategy;
}

typedef struct {
    char *sText;
    size_t iLength;
    size_t iCapacity;
    size_t iMaxLength;
    bool bTruncated;
} xllm__text_builder;

typedef struct {
    bool bRemoteSummaryAttempted;
    bool bRemoteSummarySucceeded;
    bool bLocalFallbackUsed;
    xllm_compact_strategy eStrategy;
    uint32 uKeepRecentTurns;
    uint32 uRemovedTurns;
    uint32 uSummaryTurnsBefore;
    uint32 uSummaryTurnsAfter;
    uint32 uInputTokensBefore;
    uint32 uInputTokensAfter;
    uint32 uHistoryTurnsAfter;
    const char *sSummaryProfileId;
} xllm__session_compact_trace_state;

static const char *xllm__kSessionSummarySystemPrompt =
    "You maintain a rolling session summary for a chat assistant.\n"
    "Merge the previous summary with the newly compacted conversation.\n"
    "Keep only durable facts, user preferences, unresolved tasks, tool state, and concise recent context.\n"
    "Do not invent missing information.\n"
    "Return plain text only using exactly these sections:\n"
    "Facts:\n"
    "Open items:\n"
    "Tool state:\n"
    "Recent context:\n";

static void xllm__text_builder_init(xllm__text_builder *pBuilder, size_t iMaxLength)
{
    if ( !pBuilder ) {
        return;
    }

    memset(pBuilder, 0, sizeof(*pBuilder));
    pBuilder->iMaxLength = iMaxLength;
}

static void xllm__text_builder_reset(xllm__text_builder *pBuilder)
{
    if ( !pBuilder ) {
        return;
    }

    xrtFree(pBuilder->sText);
    memset(pBuilder, 0, sizeof(*pBuilder));
}

static void xllm__trace_table_set_text(xvalue tPayload, const char *sKey, const char *sValue)
{
    if ( tPayload && sKey && sValue ) {
        xvoTableSetText(tPayload, (str)sKey, 0u, (str)sValue, 0u, FALSE);
    }
}

static void xllm__trace_table_set_bool(xvalue tPayload, const char *sKey, bool bValue)
{
    if ( tPayload && sKey ) {
        xvoTableSetBool(tPayload, (str)sKey, 0u, bValue);
    }
}

static void xllm__trace_table_set_u32(xvalue tPayload, const char *sKey, uint32 uValue)
{
    if ( tPayload && sKey ) {
        xvoTableSetInt(tPayload, (str)sKey, 0u, (int64)uValue);
    }
}

static void xllm__trace_table_set_i32(xvalue tPayload, const char *sKey, int32 iValue)
{
    if ( tPayload && sKey ) {
        xvoTableSetInt(tPayload, (str)sKey, 0u, (int64)iValue);
    }
}

static void xllm__session_logf(
    xllm_session *pSession,
    xllm_log_level eLevel,
    const char *sComponent,
    const char *sFormat,
    ...
)
{
    char sBuffer[512];
    va_list tArgs;

    if ( !pSession || !pSession->pRuntime || !pSession->pRuntime->tOptions.pfnLog || !sComponent || !sFormat ) {
        return;
    }

    va_start(tArgs, sFormat);
    (void)vsnprintf(sBuffer, sizeof(sBuffer), sFormat, tArgs);
    va_end(tArgs);
    sBuffer[sizeof(sBuffer) - 1u] = '\0';

    pSession->pRuntime->tOptions.pfnLog(
        pSession->pRuntime->tOptions.pLogCtx,
        eLevel,
        sComponent,
        sBuffer
    );
}

static void xllm__llm_logf(
    xllm *pLlm,
    xllm_log_level eLevel,
    const char *sComponent,
    const char *sFormat,
    ...
)
{
    char sBuffer[512];
    va_list tArgs;

    if ( !pLlm || !pLlm->pRuntime || !pLlm->pRuntime->tOptions.pfnLog || !sComponent || !sFormat ) {
        return;
    }

    va_start(tArgs, sFormat);
    (void)vsnprintf(sBuffer, sizeof(sBuffer), sFormat, tArgs);
    va_end(tArgs);
    sBuffer[sizeof(sBuffer) - 1u] = '\0';

    pLlm->pRuntime->tOptions.pfnLog(
        pLlm->pRuntime->tOptions.pLogCtx,
        eLevel,
        sComponent,
        sBuffer
    );
}

static void xllm__session_trace_emit(xllm_session *pSession, xllm_trace_kind eKind, xvalue tPayload)
{
    if ( pSession &&
         pSession->pRuntime &&
         pSession->pRuntime->tOptions.pfnTrace &&
         tPayload ) {
        pSession->pRuntime->tOptions.pfnTrace(
            pSession->pRuntime->tOptions.pTraceCtx,
            eKind,
            &tPayload
        );
    }
    xllm__xvalue_release(&tPayload);
}

static void xllm__session_trace_auto_compact_check(
    xllm_session *pSession,
    bool bTriggered,
    uint32 uCommittedTurns,
    uint32 uKeepRecentTurns,
    uint32 uInputTokens,
    uint32 uTargetInputTokens,
    uint32 uCompactTriggerTurns,
    double fCompactTriggerRatio
)
{
    xvalue tPayload;

    if ( !pSession || !pSession->pRuntime || !pSession->pRuntime->tOptions.pfnTrace ) {
        return;
    }

    tPayload = xvoCreateTable();
    if ( !tPayload ) {
        return;
    }

    xllm__trace_table_set_text(tPayload, "phase", "auto_check");
    xllm__trace_table_set_bool(tPayload, "triggered", bTriggered);
    xllm__trace_table_set_u32(tPayload, "committed_turns", uCommittedTurns);
    xllm__trace_table_set_u32(tPayload, "keep_recent_turns", uKeepRecentTurns);
    xllm__trace_table_set_u32(tPayload, "input_tokens", uInputTokens);
    xllm__trace_table_set_u32(tPayload, "target_input_tokens", uTargetInputTokens);
    xllm__trace_table_set_u32(tPayload, "trigger_turns", uCompactTriggerTurns);
    if ( tPayload ) {
        xvoTableSetFloat(tPayload, (str)"trigger_ratio", 0u, fCompactTriggerRatio);
    }
    xllm__session_trace_emit(pSession, XLLM_TRACE_COMPACT, tPayload);
}

static const char *xllm__compact_strategy_name(xllm_compact_strategy eStrategy)
{
    switch ( eStrategy ) {
        case XLLM_COMPACT_TRUNCATE:
            return "truncate";
        case XLLM_COMPACT_SUMMARIZE:
            return "summarize";
        case XLLM_COMPACT_CUSTOM:
            return "custom";
        default:
            return "unknown";
    }
}

static void xllm__session_trace_compact_result(
    xllm_session *pSession,
    const xllm__session_compact_trace_state *pState,
    const xllm_compact_result *pResult
)
{
    xvalue tPayload;

    if ( !pSession || !pState || !pResult ||
         !pSession->pRuntime || !pSession->pRuntime->tOptions.pfnTrace ) {
        return;
    }

    tPayload = xvoCreateTable();
    if ( !tPayload ) {
        return;
    }

    xllm__trace_table_set_text(tPayload, "phase", "compact_result");
    xllm__trace_table_set_text(tPayload, "strategy", xllm__compact_strategy_name(pState->eStrategy));
    xllm__trace_table_set_text(
        tPayload,
        "summary_source",
        pState->bRemoteSummarySucceeded ? "remote" : (pState->bLocalFallbackUsed ? "local_fallback" : "none")
    );
    if ( pState->sSummaryProfileId ) {
        xllm__trace_table_set_text(tPayload, "summary_profile_id", pState->sSummaryProfileId);
    }
    xllm__trace_table_set_bool(tPayload, "compacted", pResult->bCompacted);
    xllm__trace_table_set_bool(tPayload, "summarized", pResult->bSummarized);
    xllm__trace_table_set_bool(tPayload, "remote_summary_attempted", pState->bRemoteSummaryAttempted);
    xllm__trace_table_set_bool(tPayload, "remote_summary_succeeded", pState->bRemoteSummarySucceeded);
    xllm__trace_table_set_bool(tPayload, "local_fallback_used", pState->bLocalFallbackUsed);
    xllm__trace_table_set_u32(tPayload, "keep_recent_turns", pState->uKeepRecentTurns);
    xllm__trace_table_set_u32(tPayload, "removed_turns", pState->uRemovedTurns);
    xllm__trace_table_set_u32(tPayload, "summary_turns_before", pState->uSummaryTurnsBefore);
    xllm__trace_table_set_u32(tPayload, "summary_turns_after", pState->uSummaryTurnsAfter);
    xllm__trace_table_set_u32(tPayload, "history_turns_after", pState->uHistoryTurnsAfter);
    xllm__trace_table_set_u32(tPayload, "input_tokens_before", pState->uInputTokensBefore);
    xllm__trace_table_set_u32(tPayload, "input_tokens_after", pState->uInputTokensAfter);
    xllm__session_trace_emit(pSession, XLLM_TRACE_COMPACT, tPayload);
}

static const char *xllm__response_status_name(xllm_response_status eStatus)
{
    switch ( eStatus ) {
        case XLLM_STATUS_COMPLETED:
            return "completed";
        case XLLM_STATUS_INCOMPLETE:
            return "incomplete";
        case XLLM_STATUS_TOOL_CALL_REQUIRED:
            return "tool_call_required";
        case XLLM_STATUS_REFUSED:
            return "refused";
        case XLLM_STATUS_CONTENT_FILTERED:
            return "content_filtered";
        case XLLM_STATUS_CANCELLED:
            return "cancelled";
        case XLLM_STATUS_ERRORED:
            return "errored";
        default:
            return "unknown";
    }
}

static void xllm__tool_loop_trace_emit(xllm *pLlm, xvalue tPayload)
{
    if ( pLlm &&
         pLlm->pRuntime &&
         pLlm->pRuntime->tOptions.pfnTrace &&
         tPayload ) {
        pLlm->pRuntime->tOptions.pfnTrace(
            pLlm->pRuntime->tOptions.pTraceCtx,
            XLLM_TRACE_TOOL_LOOP,
            &tPayload
        );
    }
    xllm__xvalue_release(&tPayload);
}

static void xllm__tool_loop_trace_round_response(
    xllm *pLlm,
    uint32 uRound,
    const xllm_response *pResponse,
    uint32 uToolCallCount
)
{
    xvalue tPayload;

    if ( !pLlm || !pLlm->pRuntime || !pLlm->pRuntime->tOptions.pfnTrace || !pResponse ) {
        return;
    }

    tPayload = xvoCreateTable();
    if ( !tPayload ) {
        return;
    }

    xllm__trace_table_set_text(tPayload, "phase", "round_response");
    xllm__trace_table_set_u32(tPayload, "round", uRound);
    xllm__trace_table_set_text(tPayload, "response_status", xllm__response_status_name(pResponse->eStatus));
    xllm__trace_table_set_u32(tPayload, "tool_call_count", uToolCallCount);
    if ( pResponse->sId ) {
        xllm__trace_table_set_text(tPayload, "response_id", pResponse->sId);
    }
    if ( pResponse->sFinishReason ) {
        xllm__trace_table_set_text(tPayload, "finish_reason", pResponse->sFinishReason);
    }
    xllm__tool_loop_trace_emit(pLlm, tPayload);
}

static void xllm__tool_loop_trace_tool_result(
    xllm *pLlm,
    uint32 uRound,
    const xllm_output_tool_call *pToolCall,
    const xllm_tool_def *pToolDef,
    int32 iExecStatus,
    const xllm_tool_exec_result *pResult
)
{
    xvalue tPayload;

    if ( !pLlm || !pLlm->pRuntime || !pLlm->pRuntime->tOptions.pfnTrace ) {
        return;
    }

    tPayload = xvoCreateTable();
    if ( !tPayload ) {
        return;
    }

    xllm__trace_table_set_text(tPayload, "phase", "tool_result");
    xllm__trace_table_set_u32(tPayload, "round", uRound);
    xllm__trace_table_set_i32(tPayload, "exec_status", iExecStatus);
    if ( pToolCall ) {
        if ( pToolCall->sCallId ) {
            xllm__trace_table_set_text(tPayload, "call_id", pToolCall->sCallId);
        }
        if ( pToolCall->sToolId ) {
            xllm__trace_table_set_text(tPayload, "tool_id", pToolCall->sToolId);
        }
        if ( pToolCall->sToolName ) {
            xllm__trace_table_set_text(tPayload, "tool_name", pToolCall->sToolName);
        }
    }
    if ( pToolDef ) {
        if ( pToolDef->sToolId ) {
            xllm__trace_table_set_text(tPayload, "resolved_tool_id", pToolDef->sToolId);
        }
        if ( pToolDef->sWireName ) {
            xllm__trace_table_set_text(tPayload, "resolved_wire_name", pToolDef->sWireName);
        }
    }
    if ( pResult ) {
        xllm__trace_table_set_u32(tPayload, "result_part_count", (uint32)pResult->iPartCount);
    }
    xllm__tool_loop_trace_emit(pLlm, tPayload);
}

static void xllm__tool_loop_trace_stop(
    xllm *pLlm,
    uint32 uRoundsCompleted,
    const char *sReason,
    int32 iChatStatus,
    const xllm_response *pResponse,
    uint32 uToolCallCount
)
{
    xvalue tPayload;

    if ( !pLlm || !pLlm->pRuntime || !pLlm->pRuntime->tOptions.pfnTrace ) {
        return;
    }

    tPayload = xvoCreateTable();
    if ( !tPayload ) {
        return;
    }

    xllm__trace_table_set_text(tPayload, "phase", "stop");
    xllm__trace_table_set_u32(tPayload, "rounds_completed", uRoundsCompleted);
    xllm__trace_table_set_i32(tPayload, "chat_status", iChatStatus);
    xllm__trace_table_set_u32(tPayload, "tool_call_count", uToolCallCount);
    if ( sReason ) {
        xllm__trace_table_set_text(tPayload, "reason", sReason);
    }
    if ( pResponse ) {
        xllm__trace_table_set_text(tPayload, "response_status", xllm__response_status_name(pResponse->eStatus));
        if ( pResponse->sId ) {
            xllm__trace_table_set_text(tPayload, "response_id", pResponse->sId);
        }
    }
    xllm__tool_loop_trace_emit(pLlm, tPayload);
}

static int xllm__text_builder_reserve(xllm__text_builder *pBuilder, size_t iNeed)
{
    char *sNewText;
    size_t iNewCapacity;

    if ( !pBuilder ) {
        return XRT_NET_ERROR;
    }

    if ( iNeed <= pBuilder->iCapacity ) {
        return XRT_NET_OK;
    }

    iNewCapacity = pBuilder->iCapacity ? pBuilder->iCapacity : 128u;
    while ( iNewCapacity < iNeed ) {
        iNewCapacity *= 2u;
    }

    sNewText = (char *)xrtRealloc(pBuilder->sText, iNewCapacity);
    if ( !sNewText ) {
        return XRT_NET_ERROR;
    }

    pBuilder->sText = sNewText;
    pBuilder->iCapacity = iNewCapacity;
    return XRT_NET_OK;
}

static int xllm__text_builder_append_n(xllm__text_builder *pBuilder, const char *sText, size_t iTextLen)
{
    size_t iWritable;

    if ( !pBuilder || (!sText && iTextLen != 0u) ) {
        return XRT_NET_ERROR;
    }
    if ( iTextLen == 0u || pBuilder->bTruncated ) {
        return XRT_NET_OK;
    }

    iWritable = iTextLen;
    if ( pBuilder->iMaxLength != 0u && pBuilder->iLength + iWritable > pBuilder->iMaxLength ) {
        if ( pBuilder->iLength >= pBuilder->iMaxLength ) {
            pBuilder->bTruncated = true;
            return XRT_NET_OK;
        }
        iWritable = pBuilder->iMaxLength - pBuilder->iLength;
        pBuilder->bTruncated = true;
    }

    if ( xllm__text_builder_reserve(pBuilder, pBuilder->iLength + iWritable + 1u) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }

    memcpy(pBuilder->sText + pBuilder->iLength, sText, iWritable);
    pBuilder->iLength += iWritable;
    pBuilder->sText[pBuilder->iLength] = '\0';
    return XRT_NET_OK;
}

static int xllm__text_builder_append_cstr(xllm__text_builder *pBuilder, const char *sText)
{
    if ( !sText ) {
        return XRT_NET_OK;
    }
    return xllm__text_builder_append_n(pBuilder, sText, strlen(sText));
}

static int xllm__text_builder_append_char(xllm__text_builder *pBuilder, char ch)
{
    return xllm__text_builder_append_n(pBuilder, &ch, 1u);
}

static int xllm__text_builder_append_snippet(xllm__text_builder *pBuilder, const char *sText, size_t iMaxChars)
{
    size_t iRead = 0u;

    if ( !pBuilder || !sText ) {
        return XRT_NET_OK;
    }

    while ( sText[iRead] != '\0' && iRead < iMaxChars ) {
        char ch = sText[iRead++];
        if ( ch == '\r' || ch == '\n' || ch == '\t' ) {
            ch = ' ';
        }
        if ( xllm__text_builder_append_char(pBuilder, ch) != XRT_NET_OK ) {
            return XRT_NET_ERROR;
        }
    }

    if ( sText[iRead] != '\0' && !pBuilder->bTruncated ) {
        (void)xllm__text_builder_append_cstr(pBuilder, "...");
    }

    return XRT_NET_OK;
}

static char *xllm__dup_trimmed_cstr(const char *sText)
{
    const char *sBegin;
    const char *sEnd;
    size_t iLen;
    char *sCopy;

    if ( !sText ) {
        return NULL;
    }

    sBegin = sText;
    while ( *sBegin != '\0' && (*sBegin == ' ' || *sBegin == '\r' || *sBegin == '\n' || *sBegin == '\t') ) {
        ++sBegin;
    }

    sEnd = sText + strlen(sText);
    while ( sEnd > sBegin &&
            (sEnd[-1] == ' ' || sEnd[-1] == '\r' || sEnd[-1] == '\n' || sEnd[-1] == '\t') ) {
        --sEnd;
    }

    iLen = (size_t)(sEnd - sBegin);
    sCopy = (char *)xrtCalloc(iLen + 1u, sizeof(char));
    if ( !sCopy ) {
        return NULL;
    }

    if ( iLen != 0u ) {
        memcpy(sCopy, sBegin, iLen);
    }
    sCopy[iLen] = '\0';
    return sCopy;
}

static const xllm_call_options *xllm__resolve_call_options(
    const xllm_call_options *pDefaultOptions,
    const xllm_call_options *pCallOptions
)
{
    return pCallOptions ? pCallOptions : pDefaultOptions;
}

static int xllm__request_insert_owned_messages(
    xllm_request *pRequest,
    size_t iInsertAt,
    xllm_message *pOwnedMessages,
    size_t iOwnedCount
);

static const xllm_tool_def *xllm__request_find_tool_def(
    const xllm_request *pRequest,
    const xllm_output_tool_call *pToolCall
)
{
    size_t i;

    if ( !pRequest || !pToolCall ) {
        return NULL;
    }

    for ( i = 0; i < pRequest->iToolCount; ++i ) {
        const xllm_tool_def *pTool = &pRequest->pTools[i];
        const char *sWireName = pTool->sWireName ? pTool->sWireName : pTool->sToolId;

        if ( pToolCall->sToolId && pTool->sToolId && strcmp(pToolCall->sToolId, pTool->sToolId) == 0 ) {
            return pTool;
        }
        if ( pToolCall->sToolName && pTool->sToolId && strcmp(pToolCall->sToolName, pTool->sToolId) == 0 ) {
            return pTool;
        }
        if ( pToolCall->sToolName && sWireName && strcmp(pToolCall->sToolName, sWireName) == 0 ) {
            return pTool;
        }
    }

    return NULL;
}

static int xllm__tool_exec_result_ensure_default_part(xllm_tool_exec_result *pResult)
{
    if ( !pResult ) {
        return XRT_NET_ERROR;
    }

    if ( pResult->iPartCount != 0u || pResult->pParts ) {
        return XRT_NET_OK;
    }

    pResult->pParts = (xllm_content_part *)xrtCalloc(1u, sizeof(xllm_content_part));
    if ( !pResult->pParts ) {
        return XRT_NET_ERROR;
    }

    pResult->iPartCount = 1u;
    pResult->pParts[0].eKind = XLLM_PART_TEXT;
    pResult->pParts[0].as.tSource.eKind = XLLM_SOURCE_INLINE_TEXT;
    pResult->pParts[0].as.tSource.sMimeType = xllm__dup_cstr("text/plain");
    pResult->pParts[0].as.tSource.as.sText = xllm__dup_cstr("");
    if ( !pResult->pParts[0].as.tSource.sMimeType ||
         !pResult->pParts[0].as.tSource.as.sText ) {
        xllm__tool_exec_result_release(pResult);
        return XRT_NET_ERROR;
    }

    return XRT_NET_OK;
}

static int xllm__tool_exec_result_clone(xllm_tool_exec_result *pOut, const xllm_tool_exec_result *pIn)
{
    size_t i;

    if ( !pOut || !pIn ) {
        return XRT_NET_ERROR;
    }

    memset(pOut, 0, sizeof(*pOut));
    pOut->tVendorExtra = pIn->tVendorExtra;
    xllm__xvalue_addref(pOut->tVendorExtra);

    if ( pIn->iPartCount == 0u || !pIn->pParts ) {
        return XRT_NET_OK;
    }

    pOut->pParts = (xllm_content_part *)xrtCalloc(pIn->iPartCount, sizeof(xllm_content_part));
    if ( !pOut->pParts ) {
        xllm__tool_exec_result_release(pOut);
        return XRT_NET_ERROR;
    }

    pOut->iPartCount = pIn->iPartCount;
    for ( i = 0; i < pIn->iPartCount; ++i ) {
        if ( xllm__content_part_clone(&pOut->pParts[i], &pIn->pParts[i]) != XRT_NET_OK ) {
            xllm__tool_exec_result_release(pOut);
            return XRT_NET_ERROR;
        }
    }

    return XRT_NET_OK;
}

static int xllm__tool_call_clone_from_output(
    xllm_tool_call *pOut,
    const xllm_output_tool_call *pIn
);

static bool xllm__tool_loop_is_openai_reasoning_output(const xllm_output_item *pOutput)
{
    const char *sField;

    if ( !pOutput || pOutput->eKind != XLLM_OUTPUT_THINKING ||
         !pOutput->as.tThinking.tVendorExtra ||
         xvoType(pOutput->as.tThinking.tVendorExtra) != XVO_DT_TABLE ) {
        return false;
    }

    sField = (const char *)xvoTableGetText(
        pOutput->as.tThinking.tVendorExtra,
        (str)"openai_reasoning_field",
        0u
    );
    return (sField && strcmp(sField, "reasoning_content") == 0);
}

static int xllm__tool_loop_set_openai_reasoning_vendor_extra(
    xllm_message *pMessage,
    const xllm_response *pResponse
)
{
    xllm__text_builder tBuilder;
    xvalue tVendorExtra = NULL;
    size_t i;

    if ( !pMessage || !pResponse ) {
        return XRT_NET_ERROR;
    }

    xllm__text_builder_init(&tBuilder, 0u);
    for ( i = 0u; i < pResponse->iOutputCount; ++i ) {
        const xllm_output_item *pOutput = &pResponse->pOutputs[i];

        if ( !xllm__tool_loop_is_openai_reasoning_output(pOutput) ) {
            continue;
        }
        if ( pOutput->as.tThinking.sText && pOutput->as.tThinking.sText[0] ) {
            if ( tBuilder.iLength > 0u &&
                 xllm__text_builder_append_char(&tBuilder, '\n') != XRT_NET_OK ) {
                goto fail;
            }
            if ( xllm__text_builder_append_cstr(&tBuilder, pOutput->as.tThinking.sText) != XRT_NET_OK ) {
                goto fail;
            }
        }
    }

    if ( tBuilder.iLength == 0u || !tBuilder.sText ) {
        xllm__text_builder_reset(&tBuilder);
        return XRT_NET_OK;
    }

    tVendorExtra = xvoCreateTable();
    if ( !tVendorExtra ) {
        goto fail;
    }
    if ( !xvoTableSetText(tVendorExtra, (str)"openai_reasoning_field", 0u, (str)"reasoning_content", 0u, FALSE) ||
         !xvoTableSetText(tVendorExtra, (str)"reasoning_content", 0u, (str)tBuilder.sText, 0u, FALSE) ) {
        xvoUnref(tVendorExtra);
        goto fail;
    }

    pMessage->tVendorExtra = tVendorExtra;
    xllm__text_builder_reset(&tBuilder);
    return XRT_NET_OK;

fail:
    xllm__text_builder_reset(&tBuilder);
    return XRT_NET_ERROR;
}

static int xllm__tool_loop_build_assistant_message(
    xllm_message *pOut,
    const xllm_response *pResponse
)
{
    size_t i;
    size_t iToolCount = 0u;
    size_t iThinkingCount = 0u;
    size_t iWrite = 0u;
    size_t iThinkingWrite = 0u;

    if ( !pOut || !pResponse ) {
        return XRT_NET_ERROR;
    }

    memset(pOut, 0, sizeof(*pOut));
    pOut->eRole = XLLM_ROLE_ASSISTANT;

    for ( i = 0; i < pResponse->iOutputCount; ++i ) {
        if ( pResponse->pOutputs[i].eKind == XLLM_OUTPUT_TOOL_CALL ) {
            ++iToolCount;
        } else if ( pResponse->pOutputs[i].eKind == XLLM_OUTPUT_THINKING &&
                    pResponse->pOutputs[i].as.tThinking.tVendorExtra &&
                    xvoType(pResponse->pOutputs[i].as.tThinking.tVendorExtra) == XVO_DT_TABLE ) {
            const char *sBlockType = (const char *)xvoTableGetText(
                pResponse->pOutputs[i].as.tThinking.tVendorExtra,
                (str)"anthropic_block_type",
                0u
            );
            if ( sBlockType && strcmp(sBlockType, "thinking") == 0 ) {
                ++iThinkingCount;
            }
        }
    }

    if ( iToolCount == 0u ) {
        return XRT_NET_ERROR;
    }

    pOut->pToolCalls = (xllm_tool_call *)xrtCalloc(iToolCount, sizeof(xllm_tool_call));
    if ( !pOut->pToolCalls ) {
        return XRT_NET_ERROR;
    }
    if ( iThinkingCount > 0u ) {
        pOut->pParts = (xllm_content_part *)xrtCalloc(iThinkingCount, sizeof(xllm_content_part));
        if ( !pOut->pParts ) {
            xllm__message_free(pOut);
            return XRT_NET_ERROR;
        }
        pOut->iPartCount = iThinkingCount;
    }

    pOut->iToolCallCount = iToolCount;
    for ( i = 0; i < pResponse->iOutputCount; ++i ) {
        const xllm_output_item *pOutput = &pResponse->pOutputs[i];
        xllm_tool_call *pToolCall;

        if ( pOutput->eKind == XLLM_OUTPUT_THINKING &&
             pOutput->as.tThinking.tVendorExtra &&
             xvoType(pOutput->as.tThinking.tVendorExtra) == XVO_DT_TABLE ) {
            const char *sBlockType = (const char *)xvoTableGetText(
                pOutput->as.tThinking.tVendorExtra,
                (str)"anthropic_block_type",
                0u
            );
            if ( sBlockType && strcmp(sBlockType, "thinking") == 0 ) {
                xllm_content_part *pPart = &pOut->pParts[iThinkingWrite++];
                pPart->eKind = XLLM_PART_TEXT;
                pPart->as.tSource.eKind = XLLM_SOURCE_INLINE_TEXT;
                pPart->as.tSource.sMimeType = xllm__dup_cstr("text/plain");
                pPart->as.tSource.as.sText = xllm__dup_cstr(pOutput->as.tThinking.sText ? pOutput->as.tThinking.sText : "");
                pPart->tVendorExtra = pOutput->as.tThinking.tVendorExtra;
                xllm__xvalue_addref(pPart->tVendorExtra);
                if ( !pPart->as.tSource.sMimeType ||
                     ((pOutput->as.tThinking.sText != NULL) && !pPart->as.tSource.as.sText) ) {
                    xllm__message_free(pOut);
                    return XRT_NET_ERROR;
                }
            }
        }

        if ( pOutput->eKind != XLLM_OUTPUT_TOOL_CALL ) {
            continue;
        }

        pToolCall = &pOut->pToolCalls[iWrite++];
        if ( xllm__tool_call_clone_from_output(pToolCall, &pOutput->as.tToolCall) != XRT_NET_OK ) {
            xllm__message_free(pOut);
            return XRT_NET_ERROR;
        }
    }

    if ( xllm__tool_loop_set_openai_reasoning_vendor_extra(pOut, pResponse) != XRT_NET_OK ) {
        xllm__message_free(pOut);
        return XRT_NET_ERROR;
    }

    return XRT_NET_OK;
}

static int xllm__tool_call_clone_from_output(
    xllm_tool_call *pOut,
    const xllm_output_tool_call *pIn
)
{
    const char *sResolvedToolName;

    if ( !pOut || !pIn ) {
        return XRT_NET_ERROR;
    }

    memset(pOut, 0, sizeof(*pOut));
    sResolvedToolName = pIn->sToolName ? pIn->sToolName : pIn->sToolId;

    pOut->sCallId = xllm__dup_cstr(pIn->sCallId);
    pOut->sToolId = xllm__dup_cstr(pIn->sToolId);
    pOut->sToolName = xllm__dup_cstr(sResolvedToolName);
    pOut->sArgumentsJson = xllm__dup_cstr(pIn->sArgumentsJson);
    pOut->tContinuation = pIn->tContinuation;
    pOut->tVendorExtra = pIn->tVendorExtra;
    xllm__xvalue_addref(pOut->tContinuation);
    xllm__xvalue_addref(pOut->tVendorExtra);

    if ( (pIn->sCallId && !pOut->sCallId) ||
         (pIn->sToolId && !pOut->sToolId) ||
         (sResolvedToolName && !pOut->sToolName) ||
         (pIn->sArgumentsJson && !pOut->sArgumentsJson) ) {
        xllm__tool_call_free(pOut);
        return XRT_NET_ERROR;
    }

    return XRT_NET_OK;
}

static int xllm__tool_loop_build_tool_message(
    xllm_message *pOut,
    const xllm_output_tool_call *pToolCall,
    const xllm_tool_exec_result *pResult
)
{
    size_t i;

    if ( !pOut || !pToolCall || !pResult ) {
        return XRT_NET_ERROR;
    }

    memset(pOut, 0, sizeof(*pOut));
    pOut->eRole = XLLM_ROLE_TOOL;
    pOut->sToolCallId = xllm__dup_cstr(pToolCall->sCallId);
    pOut->sToolName = xllm__dup_cstr(pToolCall->sToolName ? pToolCall->sToolName : pToolCall->sToolId);
    pOut->tVendorExtra = pResult->tVendorExtra;
    xllm__xvalue_addref(pOut->tVendorExtra);
    if ( (pToolCall->sCallId && !pOut->sToolCallId) ||
         ((pToolCall->sToolName || pToolCall->sToolId) && !pOut->sToolName) ) {
        xllm__message_free(pOut);
        return XRT_NET_ERROR;
    }

    pOut->pParts = (xllm_content_part *)xrtCalloc(pResult->iPartCount, sizeof(xllm_content_part));
    if ( !pOut->pParts ) {
        xllm__message_free(pOut);
        return XRT_NET_ERROR;
    }

    pOut->iPartCount = pResult->iPartCount;
    for ( i = 0; i < pResult->iPartCount; ++i ) {
        if ( xllm__content_part_clone(&pOut->pParts[i], &pResult->pParts[i]) != XRT_NET_OK ) {
            xllm__message_free(pOut);
            return XRT_NET_ERROR;
        }
    }

    return XRT_NET_OK;
}

static int xllm__request_append_owned_messages(
    xllm_request *pRequest,
    xllm_message *pOwnedMessages,
    size_t iOwnedCount
)
{
    return xllm__request_insert_owned_messages(
        pRequest,
        pRequest ? pRequest->iMessageCount : 0u,
        pOwnedMessages,
        iOwnedCount
    );
}

static int xllm__tool_loop_append_round_messages(
    xllm_request *pRequest,
    const xllm_response *pResponse,
    const xllm_output_tool_call *const *ppToolCalls,
    const xllm_tool_exec_result *pResults,
    size_t iToolCount
)
{
    xllm_message *pOwnedMessages;
    size_t i;

    if ( !pRequest || !pResponse || !ppToolCalls || (!pResults && iToolCount != 0u) ) {
        return XRT_NET_ERROR;
    }

    pOwnedMessages = (xllm_message *)xrtCalloc(iToolCount + 1u, sizeof(xllm_message));
    if ( !pOwnedMessages ) {
        return XRT_NET_ERROR;
    }

    if ( xllm__tool_loop_build_assistant_message(&pOwnedMessages[0], pResponse) != XRT_NET_OK ) {
        xllm__message_array_free(pOwnedMessages, iToolCount + 1u);
        return XRT_NET_ERROR;
    }

    for ( i = 0; i < iToolCount; ++i ) {
        if ( xllm__tool_loop_build_tool_message(&pOwnedMessages[i + 1u], ppToolCalls[i], &pResults[i]) != XRT_NET_OK ) {
            xllm__message_array_free(pOwnedMessages, iToolCount + 1u);
            return XRT_NET_ERROR;
        }
    }

    return xllm__request_append_owned_messages(pRequest, pOwnedMessages, iToolCount + 1u);
}

static bool xllm__tool_loop_has_sync_executor(const xllm *pLlm)
{
    return pLlm && pLlm->bHasToolExecutor && pLlm->tToolExecutor.pfnExecute;
}

static bool xllm__tool_loop_has_async_executor(const xllm *pLlm)
{
    return pLlm && pLlm->bHasToolExecutorAsync && pLlm->tToolExecutorAsync.pfnExecute;
}

static bool xllm__tool_loop_can_run(const xllm *pLlm)
{
    return xllm__tool_loop_has_sync_executor(pLlm) || xllm__tool_loop_has_async_executor(pLlm);
}

static int xllm__tool_loop_execute_sync(
    xllm *pLlm,
    const xllm_tool_exec_request *pExecRequest,
    xllm_tool_exec_result *pExecResult,
    xllm_error *pError
)
{
    xllm_error tLocalError;
    xllm_error *pWorkError = pError ? pError : &tLocalError;
    int32 iStatus;

    if ( !pLlm || !pExecRequest || !pExecResult || !xllm__tool_loop_has_sync_executor(pLlm) ) {
        if ( pWorkError ) {
            xllm_error_reset(pWorkError);
            xllm__error_set(pWorkError, XLLM_ERROR_INTERNAL, "tool executor arguments are invalid");
        }
        return XRT_NET_ERROR;
    }

    xllm_error_init(&tLocalError);
    xllm_error_reset(pWorkError);
    memset(pExecResult, 0, sizeof(*pExecResult));
    iStatus = pLlm->tToolExecutor.pfnExecute(
        pLlm->tToolExecutor.pCtx,
        pExecRequest,
        pExecResult,
        pWorkError
    );
    if ( iStatus != XRT_NET_OK ) {
        if ( pWorkError->eCode == XLLM_ERROR_NONE ) {
            xllm__error_set(pWorkError, XLLM_ERROR_INTERNAL, "tool executor failed");
        }
        xllm__tool_exec_result_release(pExecResult);
        xllm_error_free(&tLocalError);
        return iStatus;
    }

    if ( xllm__tool_exec_result_ensure_default_part(pExecResult) != XRT_NET_OK ) {
        xllm__error_set(pWorkError, XLLM_ERROR_INTERNAL, "tool executor result normalization failed");
        xllm__tool_exec_result_release(pExecResult);
        xllm_error_free(&tLocalError);
        return XRT_NET_ERROR;
    }

    xllm_error_free(&tLocalError);
    return XRT_NET_OK;
}

static int xllm__tool_loop_execute_async_wait(
    xllm *pLlm,
    const xllm_tool_exec_request *pExecRequest,
    xllm_tool_exec_result *pExecResult,
    xllm_error *pError
)
{
    xllm_error tLocalError;
    xllm_error *pWorkError = pError ? pError : &tLocalError;
    xfuture *pFuture;
    xfuture_result tFutureResult;
    const xllm_tool_exec_result *pAsyncResult;

    if ( !pLlm || !pExecRequest || !pExecResult || !xllm__tool_loop_has_async_executor(pLlm) ) {
        if ( pWorkError ) {
            xllm_error_reset(pWorkError);
            xllm__error_set(pWorkError, XLLM_ERROR_INTERNAL, "async tool executor arguments are invalid");
        }
        return XRT_NET_ERROR;
    }

    xllm_error_init(&tLocalError);
    xllm_error_reset(pWorkError);
    memset(pExecResult, 0, sizeof(*pExecResult));
    memset(&tFutureResult, 0, sizeof(tFutureResult));

    pFuture = pLlm->tToolExecutorAsync.pfnExecute(
        pLlm->tToolExecutorAsync.pCtx,
        pExecRequest
    );
    if ( !pFuture ) {
        xllm__error_set(pWorkError, XLLM_ERROR_INTERNAL, "async tool executor returned null future");
        xllm_error_free(&tLocalError);
        return XRT_NET_ERROR;
    }

    if ( !xFutureWait(pFuture) || !xFutureGetResult(pFuture, &tFutureResult) ) {
        xFutureRelease(pFuture);
        xllm__error_set(pWorkError, XLLM_ERROR_INTERNAL, "async tool executor future wait failed");
        xllm_error_free(&tLocalError);
        return XRT_NET_ERROR;
    }

    if ( tFutureResult.iStatus != XRT_NET_OK ) {
        xFutureRelease(pFuture);
        if ( tFutureResult.iFlags & XFUTURE_RESULT_F_CANCELLED ) {
            xllm__error_set(pWorkError, XLLM_ERROR_CANCELLED, "async tool executor cancelled");
            xllm_error_free(&tLocalError);
            return XRT_NET_CANCELLED;
        }
        if ( tFutureResult.sError ) {
            xllm__error_set(pWorkError, XLLM_ERROR_INTERNAL, (const char *)tFutureResult.sError);
        } else {
            xllm__error_set(pWorkError, XLLM_ERROR_INTERNAL, "async tool executor failed");
        }
        xllm_error_free(&tLocalError);
        return tFutureResult.iStatus;
    }

    pAsyncResult = (const xllm_tool_exec_result *)tFutureResult.pValue;
    if ( !pAsyncResult ) {
        xFutureRelease(pFuture);
        xllm__error_set(pWorkError, XLLM_ERROR_INTERNAL, "async tool executor returned null result");
        xllm_error_free(&tLocalError);
        return XRT_NET_ERROR;
    }

    if ( xllm__tool_exec_result_clone(pExecResult, pAsyncResult) != XRT_NET_OK ) {
        xFutureRelease(pFuture);
        xllm__error_set(pWorkError, XLLM_ERROR_INTERNAL, "async tool executor result clone failed");
        xllm_error_free(&tLocalError);
        return XRT_NET_ERROR;
    }

    xFutureRelease(pFuture);

    if ( xllm__tool_exec_result_ensure_default_part(pExecResult) != XRT_NET_OK ) {
        xllm__error_set(pWorkError, XLLM_ERROR_INTERNAL, "async tool executor result normalization failed");
        xllm__tool_exec_result_release(pExecResult);
        xllm_error_free(&tLocalError);
        return XRT_NET_ERROR;
    }

    xllm_error_free(&tLocalError);
    return XRT_NET_OK;
}

static int xllm__send_with_tool_loop(
    xllm *pLlm,
    xllm_request *pRequest,
    const xllm_call_options *pOptions,
    xllm_response **ppResponse,
    xllm_error *pError
)
{
    xllm_response *pCurrent = NULL;
    xllm_error tLocalError;
    xllm_error *pWorkError = pError ? pError : &tLocalError;
    uint32 uRounds = 0u;
    int32 iStatus;

    xllm_error_init(&tLocalError);
    xllm_error_reset(pWorkError);

    if ( !pLlm || !pRequest || !ppResponse ) {
        xllm__error_set(pWorkError, XLLM_ERROR_INVALID_REQUEST, "send arguments are invalid");
        xllm_error_free(&tLocalError);
        return XRT_NET_ERROR;
    }

    *ppResponse = NULL;

    for (;;) {
        size_t i;
        size_t iToolCount = 0u;
        const xllm_output_tool_call **ppToolCalls = NULL;
        xllm_tool_exec_result *pResults = NULL;
        bool bAutoLoopReady = true;
        const char *sStopReason = NULL;
        int32 iExecStatus = XRT_NET_OK;

        iStatus = xllm_chat_ex(pLlm->pRuntime, pRequest, pOptions, &pCurrent, pWorkError);
        if ( iStatus != XRT_NET_OK ) {
            xllm__llm_logf(
                pLlm,
                XLLM_LOG_WARN,
                "xllm.tool_loop",
                "tool loop aborted: chat error status=%d completed_rounds=%u",
                iStatus,
                (unsigned)uRounds
            );
            xllm__tool_loop_trace_stop(pLlm, uRounds, "chat_error", iStatus, NULL, 0u);
            xllm_error_free(&tLocalError);
            return iStatus;
        }

        iToolCount = pCurrent ? (uint32)xllm_response_get_tool_call_count(pCurrent) : 0u;
        if ( pCurrent ) {
            xllm__llm_logf(
                pLlm,
                XLLM_LOG_DEBUG,
                "xllm.tool_loop",
                "round=%u response_status=%s tool_calls=%u",
                (unsigned)(uRounds + 1u),
                xllm__response_status_name(pCurrent->eStatus),
                (unsigned)iToolCount
            );
            xllm__tool_loop_trace_round_response(pLlm, uRounds + 1u, pCurrent, (uint32)iToolCount);
        }

        if ( !pCurrent ||
             pCurrent->eStatus != XLLM_STATUS_TOOL_CALL_REQUIRED ||
             !xllm__tool_loop_can_run(pLlm) ||
             uRounds >= XLLM__AUTO_TOOL_LOOP_MAX_ROUNDS ) {
            if ( !pCurrent ) {
                sStopReason = "null_response";
            } else if ( pCurrent->eStatus != XLLM_STATUS_TOOL_CALL_REQUIRED ) {
                sStopReason = "response_final";
            } else if ( !xllm__tool_loop_can_run(pLlm) ) {
                sStopReason = "executor_unavailable";
            } else {
                sStopReason = "max_rounds";
            }
            xllm__llm_logf(
                pLlm,
                pCurrent && pCurrent->eStatus != XLLM_STATUS_TOOL_CALL_REQUIRED ? XLLM_LOG_INFO : XLLM_LOG_DEBUG,
                "xllm.tool_loop",
                "tool loop stop: reason=%s completed_rounds=%u",
                sStopReason,
                (unsigned)uRounds
            );
            xllm__tool_loop_trace_stop(pLlm, uRounds, sStopReason, iStatus, pCurrent, (uint32)iToolCount);
            *ppResponse = pCurrent;
            xllm_error_reset(pWorkError);
            xllm_error_free(&tLocalError);
            return XRT_NET_OK;
        }

        if ( iToolCount == 0u ) {
            xllm__llm_logf(
                pLlm,
                XLLM_LOG_WARN,
                "xllm.tool_loop",
                "tool loop stop: empty tool call list at round=%u",
                (unsigned)(uRounds + 1u)
            );
            xllm__tool_loop_trace_stop(pLlm, uRounds, "empty_tool_calls", iStatus, pCurrent, 0u);
            *ppResponse = pCurrent;
            xllm_error_reset(pWorkError);
            xllm_error_free(&tLocalError);
            return XRT_NET_OK;
        }

        ppToolCalls = (const xllm_output_tool_call **)xrtCalloc(iToolCount, sizeof(*ppToolCalls));
        pResults = (xllm_tool_exec_result *)xrtCalloc(iToolCount, sizeof(*pResults));
        if ( !ppToolCalls || !pResults ) {
            xrtFree(ppToolCalls);
            xrtFree(pResults);
            xllm__llm_logf(
                pLlm,
                XLLM_LOG_ERROR,
                "xllm.tool_loop",
                "tool loop stop: allocation failed for %u tool call(s)",
                (unsigned)iToolCount
            );
            xllm__tool_loop_trace_stop(pLlm, uRounds, "allocation_failed", iStatus, pCurrent, (uint32)iToolCount);
            *ppResponse = pCurrent;
            xllm_error_reset(pWorkError);
            xllm_error_free(&tLocalError);
            return XRT_NET_OK;
        }

        for ( i = 0; i < iToolCount; ++i ) {
            const xllm_output_tool_call *pToolCall = xllm_response_get_tool_call(pCurrent, i);
            const xllm_tool_def *pToolDef;
            xllm_tool_exec_request tExecRequest;

            if ( !pToolCall ) {
                bAutoLoopReady = false;
                sStopReason = "tool_call_missing";
                break;
            }

            pToolDef = xllm__request_find_tool_def(pRequest, pToolCall);
            if ( !pToolDef || pToolDef->eKind != XLLM_TOOL_CLIENT ) {
                bAutoLoopReady = false;
                sStopReason = "tool_unavailable";
                xllm__llm_logf(
                    pLlm,
                    XLLM_LOG_WARN,
                    "xllm.tool_loop",
                    "tool loop cannot resolve client executor for call_id=%s tool=%s",
                    pToolCall->sCallId ? pToolCall->sCallId : "(null)",
                    pToolCall->sToolName ? pToolCall->sToolName : "(null)"
                );
                xllm__tool_loop_trace_tool_result(
                    pLlm,
                    uRounds + 1u,
                    pToolCall,
                    pToolDef,
                    XRT_NET_ERROR,
                    NULL
                );
                break;
            }

            memset(&tExecRequest, 0, sizeof(tExecRequest));
            tExecRequest.sToolId = pToolDef->sToolId ? pToolDef->sToolId : pToolCall->sToolId;
            tExecRequest.sWireName = pToolDef->sWireName ? pToolDef->sWireName : pToolCall->sToolName;
            tExecRequest.sCallId = pToolCall->sCallId;
            tExecRequest.sArgumentsJson = pToolCall->sArgumentsJson;
            tExecRequest.tContinuation = pToolCall->tContinuation;
            tExecRequest.tVendorExtra = pToolCall->tVendorExtra;

            if ( xllm__tool_loop_has_sync_executor(pLlm) ) {
                iExecStatus = xllm__tool_loop_execute_sync(pLlm, &tExecRequest, &pResults[i], pWorkError);
            } else {
                iExecStatus = xllm__tool_loop_execute_async_wait(pLlm, &tExecRequest, &pResults[i], pWorkError);
            }
            xllm__llm_logf(
                pLlm,
                iExecStatus == XRT_NET_OK ? XLLM_LOG_DEBUG : XLLM_LOG_WARN,
                "xllm.tool_loop",
                "tool execute: round=%u tool=%s call_id=%s status=%d",
                (unsigned)(uRounds + 1u),
                tExecRequest.sToolId ? tExecRequest.sToolId : "(null)",
                tExecRequest.sCallId ? tExecRequest.sCallId : "(null)",
                iExecStatus
            );
            xllm__tool_loop_trace_tool_result(
                pLlm,
                uRounds + 1u,
                pToolCall,
                pToolDef,
                iExecStatus,
                iExecStatus == XRT_NET_OK ? &pResults[i] : NULL
            );
            if ( iExecStatus != XRT_NET_OK ) {
                bAutoLoopReady = false;
                sStopReason = "tool_exec_failed";
                break;
            }

            ppToolCalls[i] = pToolCall;
        }

        if ( !bAutoLoopReady ) {
            for ( i = 0; i < iToolCount; ++i ) {
                xllm__tool_exec_result_release(&pResults[i]);
            }
            xrtFree(ppToolCalls);
            xrtFree(pResults);
            xllm__llm_logf(
                pLlm,
                XLLM_LOG_WARN,
                "xllm.tool_loop",
                "tool loop stop: reason=%s completed_rounds=%u",
                sStopReason ? sStopReason : "tool_loop_aborted",
                (unsigned)uRounds
            );
            xllm__tool_loop_trace_stop(
                pLlm,
                uRounds,
                sStopReason ? sStopReason : "tool_loop_aborted",
                iStatus,
                pCurrent,
                (uint32)iToolCount
            );
            *ppResponse = pCurrent;
            xllm_error_reset(pWorkError);
            xllm_error_free(&tLocalError);
            return XRT_NET_OK;
        }
        if ( xllm__tool_loop_append_round_messages(pRequest, pCurrent, ppToolCalls, pResults, iToolCount) != XRT_NET_OK ) {
            for ( i = 0; i < iToolCount; ++i ) {
                xllm__tool_exec_result_release(&pResults[i]);
            }
            xrtFree(ppToolCalls);
            xrtFree(pResults);
            xllm__llm_logf(
                pLlm,
                XLLM_LOG_WARN,
                "xllm.tool_loop",
                "tool loop stop: append failed for round=%u",
                (unsigned)(uRounds + 1u)
            );
            xllm__tool_loop_trace_stop(pLlm, uRounds, "append_failed", iStatus, pCurrent, (uint32)iToolCount);
            *ppResponse = pCurrent;
            xllm_error_reset(pWorkError);
            xllm_error_free(&tLocalError);
            return XRT_NET_OK;
        }

        for ( i = 0; i < iToolCount; ++i ) {
            xllm__tool_exec_result_release(&pResults[i]);
        }
        xrtFree(ppToolCalls);
        xrtFree(pResults);
        xllm__llm_logf(
            pLlm,
            XLLM_LOG_DEBUG,
            "xllm.tool_loop",
            "tool loop continue: completed round=%u with %u tool result(s)",
            (unsigned)(uRounds + 1u),
            (unsigned)iToolCount
        );
        xllm_response_free(pCurrent);
        pCurrent = NULL;
        ++uRounds;
    }
}

static void xllm__session_lock(xllm_session *pSession)
{
    if ( pSession && pSession->pMutex ) {
        xrtMutexLock(pSession->pMutex);
    }
}

static void xllm__session_unlock(xllm_session *pSession)
{
    if ( pSession && pSession->pMutex ) {
        xrtMutexUnlock(pSession->pMutex);
    }
}

static void xllm__session_history_reset_locked(xllm_session *pSession)
{
    if ( !pSession ) {
        return;
    }

    xllm__message_array_free(pSession->pHistory, pSession->iHistoryCount);
    pSession->pHistory = NULL;
    pSession->iHistoryCount = 0u;
    pSession->iHistoryCapacity = 0u;
    pSession->uCommittedTurns = 0u;
}

static void xllm__session_summary_reset_locked(xllm_session *pSession)
{
    if ( !pSession ) {
        return;
    }

    xllm__free_cstr(&pSession->sSessionSummary);
    pSession->uSummaryTurns = 0u;
}

static void xllm__session_reset_locked(xllm_session *pSession)
{
    if ( !pSession ) {
        return;
    }

    xllm__session_history_reset_locked(pSession);
    xllm__session_summary_reset_locked(pSession);
}

static uint32 xllm__session_history_count_turns(
    const xllm_message *pHistory,
    size_t iHistoryCount
)
{
    uint32 uTurns = 0u;
    size_t i;

    for ( i = 0; i < iHistoryCount; ++i ) {
        if ( pHistory[i].eRole == XLLM_ROLE_USER ) {
            ++uTurns;
        }
    }

    return uTurns;
}

static uint32 xllm__session_history_count_turns_prefix(
    const xllm_message *pHistory,
    size_t iHistoryCount,
    size_t iPrefixCount
)
{
    if ( iPrefixCount > iHistoryCount ) {
        iPrefixCount = iHistoryCount;
    }
    return xllm__session_history_count_turns(pHistory, iPrefixCount);
}

static size_t xllm__session_history_keep_start_index(
    const xllm_message *pHistory,
    size_t iHistoryCount,
    uint32 uKeepRecentTurns
)
{
    uint32 uSeenTurns = 0u;
    size_t i;

    if ( !pHistory || iHistoryCount == 0u ) {
        return 0u;
    }

    if ( uKeepRecentTurns == 0u ) {
        return iHistoryCount;
    }

    for ( i = iHistoryCount; i > 0u; --i ) {
        if ( pHistory[i - 1u].eRole == XLLM_ROLE_USER ) {
            ++uSeenTurns;
            if ( uSeenTurns == uKeepRecentTurns ) {
                return i - 1u;
            }
        }
    }

    return 0u;
}

static int xllm__session_append_history_clone_locked(xllm_session *pSession, const xllm_message *pMessage)
{
    xllm_message tCopy;

    if ( !pSession || !pMessage ) {
        return XRT_NET_ERROR;
    }

    memset(&tCopy, 0, sizeof(tCopy));
    if ( xllm__message_clone(&tCopy, pMessage) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }

    if ( xllm__append_buffer(
            (void **)&pSession->pHistory,
            sizeof(tCopy),
            &pSession->iHistoryCount,
            &pSession->iHistoryCapacity,
            &tCopy
         ) != XRT_NET_OK ) {
        xllm__message_free(&tCopy);
        return XRT_NET_ERROR;
    }

    return XRT_NET_OK;
}

static int xllm__session_append_history_array_locked(
    xllm_session *pSession,
    const xllm_message *pMessages,
    size_t iMessageCount
)
{
    size_t i;

    if ( !pSession || (!pMessages && iMessageCount != 0u) ) {
        return XRT_NET_ERROR;
    }

    for ( i = 0; i < iMessageCount; ++i ) {
        if ( xllm__session_append_history_clone_locked(pSession, &pMessages[i]) != XRT_NET_OK ) {
            return XRT_NET_ERROR;
        }
    }

    return XRT_NET_OK;
}

static bool xllm__session_should_persist_thinking_output(const xllm_output_item *pOutput)
{
    const char *sBlockType;

    if ( !pOutput || pOutput->eKind != XLLM_OUTPUT_THINKING ) {
        return false;
    }

    if ( !pOutput->as.tThinking.tVendorExtra ||
         xvoType(pOutput->as.tThinking.tVendorExtra) != XVO_DT_TABLE ) {
        return false;
    }

    sBlockType = (const char *)xvoTableGetText(
        pOutput->as.tThinking.tVendorExtra,
        (str)"anthropic_block_type",
        0u
    );
    return (sBlockType && strcmp(sBlockType, "thinking") == 0);
}

static int xllm__session_build_response_assistant_message(
    xllm_message *pOut,
    const xllm_response *pResponse
)
{
    size_t i;
    size_t iPartCount = 0u;
    size_t iToolCount = 0u;
    size_t iPartWrite = 0u;
    size_t iToolWrite = 0u;

    if ( !pOut || !pResponse ) {
        return XRT_NET_ERROR;
    }

    memset(pOut, 0, sizeof(*pOut));
    pOut->eRole = XLLM_ROLE_ASSISTANT;

    for ( i = 0; i < pResponse->iOutputCount; ++i ) {
        const xllm_output_item *pOutput = &pResponse->pOutputs[i];

        switch ( pOutput->eKind ) {
            case XLLM_OUTPUT_MESSAGE:
                iPartCount += pOutput->as.tMessage.iPartCount;
                break;
            case XLLM_OUTPUT_THINKING:
                if ( xllm__session_should_persist_thinking_output(pOutput) ) {
                    ++iPartCount;
                }
                break;
            case XLLM_OUTPUT_REFUSAL:
                if ( pOutput->as.tRefusal.sText && pOutput->as.tRefusal.sText[0] ) {
                    ++iPartCount;
                }
                break;
            case XLLM_OUTPUT_TOOL_CALL:
                ++iToolCount;
                break;
            default:
                break;
        }
    }

    if ( iPartCount == 0u && iToolCount == 0u ) {
        return XRT_NET_OK;
    }

    if ( iPartCount > 0u ) {
        pOut->pParts = (xllm_content_part *)xrtCalloc(iPartCount, sizeof(xllm_content_part));
        if ( !pOut->pParts ) {
            return XRT_NET_ERROR;
        }
        pOut->iPartCount = iPartCount;
    }
    if ( iToolCount > 0u ) {
        pOut->pToolCalls = (xllm_tool_call *)xrtCalloc(iToolCount, sizeof(xllm_tool_call));
        if ( !pOut->pToolCalls ) {
            xllm__message_free(pOut);
            return XRT_NET_ERROR;
        }
        pOut->iToolCallCount = iToolCount;
    }

    for ( i = 0; i < pResponse->iOutputCount; ++i ) {
        const xllm_output_item *pOutput = &pResponse->pOutputs[i];

        switch ( pOutput->eKind ) {
            case XLLM_OUTPUT_MESSAGE: {
                size_t j;
                for ( j = 0; j < pOutput->as.tMessage.iPartCount; ++j ) {
                    if ( xllm__content_part_clone(
                            &pOut->pParts[iPartWrite],
                            &pOutput->as.tMessage.pParts[j]
                         ) != XRT_NET_OK ) {
                        xllm__message_free(pOut);
                        return XRT_NET_ERROR;
                    }
                    ++iPartWrite;
                }
                break;
            }
            case XLLM_OUTPUT_THINKING:
                if ( xllm__session_should_persist_thinking_output(pOutput) ) {
                    xllm_content_part *pPart = &pOut->pParts[iPartWrite++];
                    pPart->eKind = XLLM_PART_TEXT;
                    pPart->as.tSource.eKind = XLLM_SOURCE_INLINE_TEXT;
                    pPart->as.tSource.sMimeType = xllm__dup_cstr("text/plain");
                    pPart->as.tSource.as.sText = xllm__dup_cstr(
                        pOutput->as.tThinking.sText ? pOutput->as.tThinking.sText : ""
                    );
                    pPart->tVendorExtra = pOutput->as.tThinking.tVendorExtra;
                    xllm__xvalue_addref(pPart->tVendorExtra);
                    if ( !pPart->as.tSource.sMimeType ||
                         ((pOutput->as.tThinking.sText != NULL) && !pPart->as.tSource.as.sText) ) {
                        xllm__message_free(pOut);
                        return XRT_NET_ERROR;
                    }
                }
                break;
            case XLLM_OUTPUT_REFUSAL:
                if ( pOutput->as.tRefusal.sText && pOutput->as.tRefusal.sText[0] ) {
                    xllm_content_part *pPart = &pOut->pParts[iPartWrite++];
                    pPart->eKind = XLLM_PART_TEXT;
                    pPart->as.tSource.eKind = XLLM_SOURCE_INLINE_TEXT;
                    pPart->as.tSource.sMimeType = xllm__dup_cstr("text/plain");
                    pPart->as.tSource.as.sText = xllm__dup_cstr(pOutput->as.tRefusal.sText);
                    if ( !pPart->as.tSource.sMimeType || !pPart->as.tSource.as.sText ) {
                        xllm__message_free(pOut);
                        return XRT_NET_ERROR;
                    }
                }
                break;
            case XLLM_OUTPUT_TOOL_CALL:
                if ( xllm__tool_call_clone_from_output(&pOut->pToolCalls[iToolWrite], &pOutput->as.tToolCall) != XRT_NET_OK ) {
                    xllm__message_free(pOut);
                    return XRT_NET_ERROR;
                }
                ++iToolWrite;
                break;
            default:
                break;
        }
    }

    return XRT_NET_OK;
}

static int xllm__session_append_response_assistant_locked(
    xllm_session *pSession,
    const xllm_response *pResponse
)
{
    xllm_message tMessage;

    if ( !pSession || !pResponse ) {
        return XRT_NET_ERROR;
    }

    memset(&tMessage, 0, sizeof(tMessage));
    if ( xllm__session_build_response_assistant_message(&tMessage, pResponse) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }

    if ( tMessage.iPartCount == 0u && tMessage.iToolCallCount == 0u ) {
        xllm__message_free(&tMessage);
        return XRT_NET_OK;
    }

    if ( xllm__append_buffer(
            (void **)&pSession->pHistory,
            sizeof(tMessage),
            &pSession->iHistoryCount,
            &pSession->iHistoryCapacity,
            &tMessage
         ) != XRT_NET_OK ) {
        xllm__message_free(&tMessage);
        return XRT_NET_ERROR;
    }

    return XRT_NET_OK;
}

static int xllm__session_commit_response_locked(xllm_session *pSession, const xllm_response *pResponse)
{
    if ( !pSession || !pResponse ) {
        return XRT_NET_ERROR;
    }

    return xllm__session_append_response_assistant_locked(pSession, pResponse);
}

static size_t xllm__request_leading_system_count(const xllm_request *pRequest)
{
    size_t i = 0u;

    if ( !pRequest ) {
        return 0u;
    }

    while ( i < pRequest->iMessageCount && pRequest->pMessages[i].eRole == XLLM_ROLE_SYSTEM ) {
        ++i;
    }

    return i;
}

static int xllm__request_insert_owned_messages(
    xllm_request *pRequest,
    size_t iInsertAt,
    xllm_message *pOwnedMessages,
    size_t iOwnedCount
)
{
    xllm_message *pNewMessages;
    size_t i;

    if ( !pRequest ) {
        return XRT_NET_ERROR;
    }

    if ( !pOwnedMessages || iOwnedCount == 0u ) {
        return XRT_NET_OK;
    }

    if ( iInsertAt > pRequest->iMessageCount ) {
        iInsertAt = pRequest->iMessageCount;
    }

    pNewMessages = (xllm_message *)xrtCalloc(pRequest->iMessageCount + iOwnedCount, sizeof(xllm_message));
    if ( !pNewMessages ) {
        xllm__message_array_free(pOwnedMessages, iOwnedCount);
        return XRT_NET_ERROR;
    }

    for ( i = 0; i < iInsertAt; ++i ) {
        pNewMessages[i] = pRequest->pMessages[i];
        memset(&pRequest->pMessages[i], 0, sizeof(xllm_message));
    }
    for ( i = 0; i < iOwnedCount; ++i ) {
        pNewMessages[iInsertAt + i] = pOwnedMessages[i];
        memset(&pOwnedMessages[i], 0, sizeof(xllm_message));
    }
    for ( i = iInsertAt; i < pRequest->iMessageCount; ++i ) {
        pNewMessages[iOwnedCount + i] = pRequest->pMessages[i];
        memset(&pRequest->pMessages[i], 0, sizeof(xllm_message));
    }

    xrtFree(pRequest->pMessages);
    xrtFree(pOwnedMessages);
    pRequest->pMessages = pNewMessages;
    pRequest->iMessageCount += iOwnedCount;
    return XRT_NET_OK;
}

static int xllm__summary_append_part_snippet(xllm__text_builder *pBuilder, const xllm_content_part *pPart)
{
    if ( !pBuilder || !pPart ) {
        return XRT_NET_ERROR;
    }

    switch ( pPart->eKind ) {
        case XLLM_PART_TEXT:
        case XLLM_PART_JSON:
            if ( pPart->as.tSource.eKind == XLLM_SOURCE_INLINE_TEXT && pPart->as.tSource.as.sText ) {
                return xllm__text_builder_append_snippet(pBuilder, pPart->as.tSource.as.sText, 120u);
            }
            return xllm__text_builder_append_cstr(pBuilder, pPart->eKind == XLLM_PART_JSON ? "[json]" : "[text]");
        case XLLM_PART_IMAGE:
            return xllm__text_builder_append_cstr(pBuilder, "[image]");
        case XLLM_PART_FILE:
            return xllm__text_builder_append_cstr(pBuilder, "[file]");
        case XLLM_PART_AUDIO:
            return xllm__text_builder_append_cstr(pBuilder, "[audio]");
        case XLLM_PART_VIDEO:
            return xllm__text_builder_append_cstr(pBuilder, "[video]");
        default:
            return xllm__text_builder_append_cstr(pBuilder, "[part]");
    }
}

static int xllm__summary_append_message_line(xllm__text_builder *pBuilder, const xllm_message *pMessage)
{
    size_t i;
    bool bHasContent = false;

    if ( !pBuilder || !pMessage ) {
        return XRT_NET_ERROR;
    }

    switch ( pMessage->eRole ) {
        case XLLM_ROLE_SYSTEM:
            if ( xllm__text_builder_append_cstr(pBuilder, "system: ") != XRT_NET_OK ) return XRT_NET_ERROR;
            break;
        case XLLM_ROLE_USER:
            if ( xllm__text_builder_append_cstr(pBuilder, "user: ") != XRT_NET_OK ) return XRT_NET_ERROR;
            break;
        case XLLM_ROLE_ASSISTANT:
            if ( xllm__text_builder_append_cstr(pBuilder, "assistant: ") != XRT_NET_OK ) return XRT_NET_ERROR;
            break;
        case XLLM_ROLE_TOOL:
            if ( xllm__text_builder_append_cstr(pBuilder, "tool: ") != XRT_NET_OK ) return XRT_NET_ERROR;
            break;
        default:
            if ( xllm__text_builder_append_cstr(pBuilder, "message: ") != XRT_NET_OK ) return XRT_NET_ERROR;
            break;
    }

    if ( pMessage->eRole == XLLM_ROLE_TOOL && pMessage->sToolCallId ) {
        if ( xllm__text_builder_append_cstr(pBuilder, "call_id=") != XRT_NET_OK ||
             xllm__text_builder_append_snippet(pBuilder, pMessage->sToolCallId, 48u) != XRT_NET_OK ||
             xllm__text_builder_append_cstr(pBuilder, " ") != XRT_NET_OK ) {
            return XRT_NET_ERROR;
        }
    }

    for ( i = 0; i < pMessage->iToolCallCount; ++i ) {
        const xllm_tool_call *pToolCall = &pMessage->pToolCalls[i];

        if ( bHasContent && xllm__text_builder_append_cstr(pBuilder, " | ") != XRT_NET_OK ) {
            return XRT_NET_ERROR;
        }
        if ( xllm__text_builder_append_cstr(pBuilder, "tool_call ") != XRT_NET_OK ) {
            return XRT_NET_ERROR;
        }
        if ( xllm__text_builder_append_snippet(
                pBuilder,
                pToolCall->sToolName ? pToolCall->sToolName : pToolCall->sToolId,
                48u
             ) != XRT_NET_OK ) {
            return XRT_NET_ERROR;
        }
        if ( pToolCall->sArgumentsJson ) {
            if ( xllm__text_builder_append_cstr(pBuilder, "(") != XRT_NET_OK ||
                 xllm__text_builder_append_snippet(pBuilder, pToolCall->sArgumentsJson, 96u) != XRT_NET_OK ||
                 xllm__text_builder_append_cstr(pBuilder, ")") != XRT_NET_OK ) {
                return XRT_NET_ERROR;
            }
        }
        bHasContent = true;
    }

    for ( i = 0; i < pMessage->iPartCount; ++i ) {
        if ( bHasContent && xllm__text_builder_append_cstr(pBuilder, " | ") != XRT_NET_OK ) {
            return XRT_NET_ERROR;
        }
        if ( xllm__summary_append_part_snippet(pBuilder, &pMessage->pParts[i]) != XRT_NET_OK ) {
            return XRT_NET_ERROR;
        }
        bHasContent = true;
    }

    if ( !bHasContent && xllm__text_builder_append_cstr(pBuilder, "(empty)") != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }

    return xllm__text_builder_append_cstr(pBuilder, "\n");
}

static char *xllm__session_build_rolling_summary(
    const char *sPreviousSummary,
    const xllm_message *pMessages,
    size_t iMessageCount
)
{
    xllm__text_builder tBuilder;
    size_t i;

    xllm__text_builder_init(&tBuilder, 4096u);

    if ( sPreviousSummary && sPreviousSummary[0] != '\0' ) {
        if ( xllm__text_builder_append_cstr(&tBuilder, "Previous summary:\n") != XRT_NET_OK ||
             xllm__text_builder_append_snippet(&tBuilder, sPreviousSummary, 1536u) != XRT_NET_OK ||
             xllm__text_builder_append_cstr(&tBuilder, "\n\n") != XRT_NET_OK ) {
            xllm__text_builder_reset(&tBuilder);
            return NULL;
        }
    }

    if ( pMessages && iMessageCount != 0u ) {
        if ( xllm__text_builder_append_cstr(&tBuilder, "Recently compacted turns:\n") != XRT_NET_OK ) {
            xllm__text_builder_reset(&tBuilder);
            return NULL;
        }
        for ( i = 0; i < iMessageCount; ++i ) {
            if ( xllm__summary_append_message_line(&tBuilder, &pMessages[i]) != XRT_NET_OK ) {
                xllm__text_builder_reset(&tBuilder);
                return NULL;
            }
        }
    }

    if ( !tBuilder.sText ) {
        return xllm__dup_cstr("");
    }

    return tBuilder.sText;
}

static char *xllm__session_build_summary_prompt_text(
    const char *sPreviousSummary,
    const xllm_message *pMessages,
    size_t iMessageCount
)
{
    xllm__text_builder tBuilder;
    size_t i;

    xllm__text_builder_init(&tBuilder, 12288u);
    if ( xllm__text_builder_append_cstr(&tBuilder, "Update the rolling session summary.\n\n") != XRT_NET_OK ||
         xllm__text_builder_append_cstr(&tBuilder, "Previous summary:\n") != XRT_NET_OK ) {
        xllm__text_builder_reset(&tBuilder);
        return NULL;
    }

    if ( sPreviousSummary && sPreviousSummary[0] != '\0' ) {
        if ( xllm__text_builder_append_cstr(&tBuilder, sPreviousSummary) != XRT_NET_OK ) {
            xllm__text_builder_reset(&tBuilder);
            return NULL;
        }
    } else if ( xllm__text_builder_append_cstr(&tBuilder, "(none)") != XRT_NET_OK ) {
        xllm__text_builder_reset(&tBuilder);
        return NULL;
    }

    if ( xllm__text_builder_append_cstr(&tBuilder, "\n\nNewly compacted conversation:\n") != XRT_NET_OK ) {
        xllm__text_builder_reset(&tBuilder);
        return NULL;
    }

    if ( pMessages && iMessageCount != 0u ) {
        for ( i = 0; i < iMessageCount; ++i ) {
            if ( xllm__summary_append_message_line(&tBuilder, &pMessages[i]) != XRT_NET_OK ) {
                xllm__text_builder_reset(&tBuilder);
                return NULL;
            }
        }
    } else if ( xllm__text_builder_append_cstr(&tBuilder, "(none)\n") != XRT_NET_OK ) {
        xllm__text_builder_reset(&tBuilder);
        return NULL;
    }

    if ( xllm__text_builder_append_cstr(&tBuilder, "\nWrite the updated summary now.\n") != XRT_NET_OK ) {
        xllm__text_builder_reset(&tBuilder);
        return NULL;
    }

    if ( !tBuilder.sText ) {
        return xllm__dup_cstr("");
    }

    return tBuilder.sText;
}

static char *xllm__session_try_model_summary_locked(
    xllm_session *pSession,
    const char *sPreviousSummary,
    const xllm_message *pMessages,
    size_t iMessageCount,
    xllm__session_compact_trace_state *pTraceState
)
{
    const char *sSummaryProfileId;
    xllm_turn tTurn;
    xllm_request tRequest;
    xllm_call_options tCallOptions;
    xllm_response *pResponse = NULL;
    char *sPromptText = NULL;
    char *sSummaryText = NULL;
    int32 iStatus;

    if ( !pSession || !pSession->pRuntime ) {
        return NULL;
    }

    sSummaryProfileId = pSession->tOptions.sSummarizerProfileId;
    if ( !sSummaryProfileId ||
         !xllm__runtime_find_profile(pSession->pRuntime, sSummaryProfileId) ) {
        sSummaryProfileId = pSession->sProfileId;
    }
    if ( !sSummaryProfileId ||
         !xllm__runtime_find_profile(pSession->pRuntime, sSummaryProfileId) ) {
        return NULL;
    }
    if ( pTraceState ) {
        pTraceState->bRemoteSummaryAttempted = true;
        pTraceState->sSummaryProfileId = sSummaryProfileId;
    }

    sPromptText = xllm__session_build_summary_prompt_text(
        sPreviousSummary,
        pMessages,
        iMessageCount
    );
    if ( !sPromptText ) {
        return NULL;
    }

    xllm_turn_init(&tTurn);
    xllm_request_init(&tRequest);
    xllm_call_options_init(&tCallOptions);
    tTurn.eSlot = XLLM_SLOT_TEXT;
    tTurn.eSystemMode = XLLM_SYSTEM_REPLACE;
    tTurn.tGeneration.tTemperature.bSet = true;
    tTurn.tGeneration.tTemperature.fValue = 0.1;
    tTurn.tGeneration.tMaxOutputTokens.bSet = true;
    tTurn.tGeneration.tMaxOutputTokens.iValue = 512u;
    tTurn.tReasoning.tEnabled.bSet = true;
    tTurn.tReasoning.tEnabled.bValue = false;
    tCallOptions.eStreamMode = XLLM_STREAM_OFF;

    iStatus = xllm_turn_set_system_prompt(&tTurn, xllm__kSessionSummarySystemPrompt);
    if ( iStatus == XRT_NET_OK ) {
        iStatus = xllm_turn_add_user_text(&tTurn, sPromptText);
    }
    if ( iStatus == XRT_NET_OK ) {
        iStatus = xllm__build_request_from_turn(&tRequest, sSummaryProfileId, NULL, &tTurn);
    }
    if ( iStatus == XRT_NET_OK ) {
        iStatus = xllm_chat(pSession->pRuntime, &tRequest, &tCallOptions, &pResponse);
    }
    if ( iStatus == XRT_NET_OK &&
         pResponse &&
         pResponse->eStatus != XLLM_STATUS_CANCELLED &&
         pResponse->eStatus != XLLM_STATUS_ERRORED &&
         pResponse->eStatus != XLLM_STATUS_TOOL_CALL_REQUIRED ) {
        sSummaryText = xllm__dup_trimmed_cstr(xllm_response_get_text(pResponse));
        if ( sSummaryText && sSummaryText[0] == '\0' ) {
            xllm__free_cstr(&sSummaryText);
        }
        if ( sSummaryText && pTraceState ) {
            pTraceState->bRemoteSummarySucceeded = true;
        }
    }

    xllm_response_free(pResponse);
    xllm_request_reset(&tRequest);
    xllm_turn_reset(&tTurn);
    xllm__free_cstr(&sPromptText);
    return sSummaryText;
}

static int xllm__session_build_summary_message_array(
    const char *sSummary,
    uint32 uSummaryTurns,
    xllm_message **ppMessages,
    size_t *piMessageCount
)
{
    xllm_message *pMessages;
    const char *sPrefix = "Session summary:\n";
    xllm__text_builder tBuilder;

    if ( !ppMessages || !piMessageCount ) {
        return XRT_NET_ERROR;
    }

    *ppMessages = NULL;
    *piMessageCount = 0u;
    if ( !sSummary || sSummary[0] == '\0' ) {
        return XRT_NET_OK;
    }

    pMessages = (xllm_message *)xrtCalloc(1u, sizeof(xllm_message));
    if ( !pMessages ) {
        return XRT_NET_ERROR;
    }

    xllm__text_builder_init(&tBuilder, 0u);
    (void)uSummaryTurns;
    if ( xllm__text_builder_append_cstr(&tBuilder, sPrefix) != XRT_NET_OK ||
         xllm__text_builder_append_cstr(&tBuilder, sSummary) != XRT_NET_OK ) {
        xllm__text_builder_reset(&tBuilder);
        xrtFree(pMessages);
        return XRT_NET_ERROR;
    }

    pMessages[0].eRole = XLLM_ROLE_SYSTEM;
    pMessages[0].pParts = (xllm_content_part *)xrtCalloc(1u, sizeof(xllm_content_part));
    if ( !pMessages[0].pParts ) {
        xllm__text_builder_reset(&tBuilder);
        xrtFree(pMessages);
        return XRT_NET_ERROR;
    }

    pMessages[0].iPartCount = 1u;
    pMessages[0].pParts[0].eKind = XLLM_PART_TEXT;
    pMessages[0].pParts[0].as.tSource.eKind = XLLM_SOURCE_INLINE_TEXT;
    pMessages[0].pParts[0].as.tSource.sMimeType = xllm__dup_cstr("text/plain");
    pMessages[0].pParts[0].as.tSource.as.sText = tBuilder.sText;
    if ( !pMessages[0].pParts[0].as.tSource.sMimeType || !pMessages[0].pParts[0].as.tSource.as.sText ) {
        tBuilder.sText = NULL;
        xllm__text_builder_reset(&tBuilder);
        xllm__message_array_free(pMessages, 1u);
        return XRT_NET_ERROR;
    }

    tBuilder.sText = NULL;
    xllm__text_builder_reset(&tBuilder);
    *ppMessages = pMessages;
    *piMessageCount = 1u;
    return XRT_NET_OK;
}

static int xllm__session_build_request_snapshot(
    xllm_session *pSession,
    const xllm_turn_request *pTurn,
    xllm_request *pOut
)
{
    xllm_message *pHistorySnapshot = NULL;
    xllm_message *pSummaryMessages = NULL;
    size_t iHistoryCount = 0u;
    size_t iSummaryMessageCount = 0u;
    char *sProfileId = NULL;
    char *sSystemPrompt = NULL;
    char *sSessionSummary = NULL;
    uint32 uSummaryTurns = 0u;
    int32 iStatus;

    if ( !pSession || !pTurn || !pOut ) {
        return XRT_NET_ERROR;
    }

    xllm_request_init(pOut);

    xllm__session_lock(pSession);
    sProfileId = xllm__dup_cstr(pSession->sProfileId);
    sSystemPrompt = xllm__dup_cstr(pSession->sSystemPrompt);
    sSessionSummary = xllm__dup_cstr(pSession->sSessionSummary);
    uSummaryTurns = pSession->uSummaryTurns;
    if ( sProfileId &&
         xllm__message_array_clone(&pHistorySnapshot, &iHistoryCount, pSession->pHistory, pSession->iHistoryCount) != XRT_NET_OK ) {
        xllm__session_unlock(pSession);
        xllm__free_cstr(&sProfileId);
        xllm__free_cstr(&sSystemPrompt);
        xllm__free_cstr(&sSessionSummary);
        return XRT_NET_ERROR;
    }
    xllm__session_unlock(pSession);

    if ( !sProfileId ) {
        xllm__message_array_free(pHistorySnapshot, iHistoryCount);
        xllm__free_cstr(&sSystemPrompt);
        xllm__free_cstr(&sSessionSummary);
        return XRT_NET_ERROR;
    }

    iStatus = xllm__build_request_from_turn(pOut, sProfileId, sSystemPrompt, pTurn);
    xllm__free_cstr(&sProfileId);
    xllm__free_cstr(&sSystemPrompt);
    if ( iStatus != XRT_NET_OK ) {
        xllm__message_array_free(pHistorySnapshot, iHistoryCount);
        xllm__free_cstr(&sSessionSummary);
        return iStatus;
    }

    if ( xllm__session_build_summary_message_array(
            sSessionSummary,
            uSummaryTurns,
            &pSummaryMessages,
            &iSummaryMessageCount
         ) != XRT_NET_OK ) {
        xllm__free_cstr(&sSessionSummary);
        xllm__message_array_free(pHistorySnapshot, iHistoryCount);
        xllm_request_reset(pOut);
        return XRT_NET_ERROR;
    }
    xllm__free_cstr(&sSessionSummary);

    if ( xllm__request_insert_owned_messages(
            pOut,
            xllm__request_leading_system_count(pOut),
            pHistorySnapshot,
            iHistoryCount
         ) != XRT_NET_OK ) {
        xllm__message_array_free(pSummaryMessages, iSummaryMessageCount);
        xllm_request_reset(pOut);
        return XRT_NET_ERROR;
    }

    if ( xllm__request_insert_owned_messages(
            pOut,
            xllm__request_leading_system_count(pOut),
            pSummaryMessages,
            iSummaryMessageCount
         ) != XRT_NET_OK ) {
        xllm_request_reset(pOut);
        return XRT_NET_ERROR;
    }

    return XRT_NET_OK;
}

static uint32 xllm__session_snapshot_input_tokens(xllm_session *pSession)
{
    xllm_turn tTurn;
    xllm_request tRequest;
    xllm_token_count_result tTokens;
    uint32 uInputTokens = 0u;

    if ( !pSession ) {
        return 0u;
    }

    xllm_turn_init(&tTurn);
    if ( xllm__session_build_request_snapshot(pSession, &tTurn, &tRequest) == XRT_NET_OK ) {
        memset(&tTokens, 0, sizeof(tTokens));
        if ( xllm_count_tokens(pSession->pRuntime, &tRequest, &tTokens, NULL) == XRT_NET_OK ) {
            uInputTokens = tTokens.uInputTokens;
        }
        xllm_request_reset(&tRequest);
    }
    xllm_turn_reset(&tTurn);
    return uInputTokens;
}

static uint32 xllm__session_snapshot_input_tokens_locked(
    xllm_session *pSession,
    const xllm_message *pHistory,
    size_t iHistoryCount,
    const char *sSessionSummary,
    uint32 uSummaryTurns
)
{
    xllm_turn tTurn;
    xllm_request tRequest;
    xllm_message *pHistorySnapshot = NULL;
    xllm_message *pSummaryMessages = NULL;
    size_t iSummaryMessageCount = 0u;
    xllm_token_count_result tTokens;
    uint32 uInputTokens = 0u;
    int32 iStatus;

    if ( !pSession || !pSession->pRuntime || !pSession->sProfileId ) {
        return 0u;
    }

    xllm_turn_init(&tTurn);
    iStatus = xllm__build_request_from_turn(&tRequest, pSession->sProfileId, pSession->sSystemPrompt, &tTurn);
    if ( iStatus != XRT_NET_OK ) {
        xllm_turn_reset(&tTurn);
        return 0u;
    }

    if ( pHistory && iHistoryCount > 0u &&
         xllm__message_array_clone(&pHistorySnapshot, &iHistoryCount, pHistory, iHistoryCount) != XRT_NET_OK ) {
        xllm_request_reset(&tRequest);
        xllm_turn_reset(&tTurn);
        return 0u;
    }

    if ( xllm__session_build_summary_message_array(
            sSessionSummary,
            uSummaryTurns,
            &pSummaryMessages,
            &iSummaryMessageCount
         ) != XRT_NET_OK ) {
        xllm__message_array_free(pHistorySnapshot, iHistoryCount);
        xllm_request_reset(&tRequest);
        xllm_turn_reset(&tTurn);
        return 0u;
    }

    if ( xllm__request_insert_owned_messages(
            &tRequest,
            xllm__request_leading_system_count(&tRequest),
            pHistorySnapshot,
            iHistoryCount
         ) != XRT_NET_OK ) {
        xllm__message_array_free(pSummaryMessages, iSummaryMessageCount);
        xllm_request_reset(&tRequest);
        xllm_turn_reset(&tTurn);
        return 0u;
    }

    if ( xllm__request_insert_owned_messages(
            &tRequest,
            xllm__request_leading_system_count(&tRequest),
            pSummaryMessages,
            iSummaryMessageCount
         ) != XRT_NET_OK ) {
        xllm_request_reset(&tRequest);
        xllm_turn_reset(&tTurn);
        return 0u;
    }

    memset(&tTokens, 0, sizeof(tTokens));
    if ( xllm_count_tokens(pSession->pRuntime, &tRequest, &tTokens, NULL) == XRT_NET_OK ) {
        uInputTokens = tTokens.uInputTokens;
    }

    xllm_request_reset(&tRequest);
    xllm_turn_reset(&tTurn);
    return uInputTokens;
}

static int xllm__session_snapshot_budget(
    xllm_session *pSession,
    uint32 *puInputTokens,
    uint32 *puTargetInputTokens
)
{
    xllm_turn tTurn;
    xllm_request tRequest;
    xllm_token_count_result tTokens;
    const xllm_profile *pProfile;
    const xllm_model_binding *pBinding = NULL;
    uint32 uReservedOutputTokens = 0u;
    int32 iStatus;

    if ( puInputTokens ) {
        *puInputTokens = 0u;
    }
    if ( puTargetInputTokens ) {
        *puTargetInputTokens = 0u;
    }
    if ( !pSession ) {
        return XRT_NET_ERROR;
    }

    xllm__session_lock(pSession);
    uReservedOutputTokens = pSession->tOptions.uReserveOutputTokens;
    xllm__session_unlock(pSession);

    xllm_turn_init(&tTurn);
    iStatus = xllm__session_build_request_snapshot(pSession, &tTurn, &tRequest);
    if ( iStatus != XRT_NET_OK ) {
        xllm_turn_reset(&tTurn);
        return iStatus;
    }

    memset(&tTokens, 0, sizeof(tTokens));
    iStatus = xllm_count_tokens(pSession->pRuntime, &tRequest, &tTokens, NULL);
    if ( iStatus != XRT_NET_OK ) {
        xllm_request_reset(&tRequest);
        xllm_turn_reset(&tTurn);
        return iStatus;
    }

    if ( puInputTokens ) {
        *puInputTokens = tTokens.uInputTokens;
    }

    pProfile = xllm__runtime_find_profile(pSession->pRuntime, tRequest.sProfileId);
    if ( pProfile ) {
        pBinding = xllm__request_requires_multimodal(&tRequest)
            ? &pProfile->tModels.tMultimodal
            : &pProfile->tModels.tText;
    }

    if ( uReservedOutputTokens == 0u ) {
        uReservedOutputTokens = tTokens.uEstimatedOutputReserve;
    }
    if ( uReservedOutputTokens == 0u && pBinding ) {
        uReservedOutputTokens = pBinding->tCaps.uRecommendedOutputReserve;
    }
    if ( uReservedOutputTokens == 0u && pBinding ) {
        uReservedOutputTokens = pBinding->tCaps.uMaxOutputTokens;
    }
    if ( uReservedOutputTokens == 0u ) {
        uReservedOutputTokens = 1024u;
    }

    if ( puTargetInputTokens && pBinding ) {
        if ( pBinding->tCaps.uMaxInputTokens != 0u ) {
            *puTargetInputTokens = pBinding->tCaps.uMaxInputTokens;
        } else if ( pBinding->tCaps.uMaxContextTokens > uReservedOutputTokens ) {
            *puTargetInputTokens = pBinding->tCaps.uMaxContextTokens - uReservedOutputTokens;
        } else if ( pBinding->tCaps.uMaxContextTokens != 0u && uReservedOutputTokens == 0u ) {
            *puTargetInputTokens = pBinding->tCaps.uMaxContextTokens;
        }
    }

    xllm_request_reset(&tRequest);
    xllm_turn_reset(&tTurn);
    return XRT_NET_OK;
}

static bool xllm__session_should_auto_compact(xllm_session *pSession)
{
    bool bEnableAutoCompact;
    bool bTriggered;
    double fCompactTriggerRatio;
    uint32 uCompactTriggerTurns;
    uint32 uKeepRecentTurns;
    uint32 uCommittedTurns;
    uint32 uInputTokens = 0u;
    uint32 uTargetInputTokens = 0u;

    if ( !pSession ) {
        return false;
    }

    xllm__session_lock(pSession);
    bEnableAutoCompact = pSession->tOptions.bEnableAutoCompact;
    fCompactTriggerRatio = pSession->tOptions.fCompactTriggerRatio;
    uCompactTriggerTurns = pSession->tOptions.uCompactTriggerTurns;
    uKeepRecentTurns = pSession->tOptions.uKeepRecentTurns;
    uCommittedTurns = pSession->uCommittedTurns;
    xllm__session_unlock(pSession);

    if ( !bEnableAutoCompact || uCommittedTurns == 0u ) {
        xllm__session_trace_auto_compact_check(
            pSession,
            false,
            uCommittedTurns,
            uKeepRecentTurns,
            0u,
            0u,
            uCompactTriggerTurns,
            fCompactTriggerRatio
        );
        return false;
    }
    if ( uCompactTriggerTurns != 0u && uCommittedTurns < uCompactTriggerTurns ) {
        xllm__session_trace_auto_compact_check(
            pSession,
            false,
            uCommittedTurns,
            uKeepRecentTurns,
            0u,
            0u,
            uCompactTriggerTurns,
            fCompactTriggerRatio
        );
        return false;
    }
    if ( uKeepRecentTurns != 0u && uCommittedTurns <= uKeepRecentTurns ) {
        xllm__session_trace_auto_compact_check(
            pSession,
            false,
            uCommittedTurns,
            uKeepRecentTurns,
            0u,
            0u,
            uCompactTriggerTurns,
            fCompactTriggerRatio
        );
        return false;
    }

    if ( xllm__session_snapshot_budget(pSession, &uInputTokens, &uTargetInputTokens) != XRT_NET_OK ) {
        xllm__session_trace_auto_compact_check(
            pSession,
            false,
            uCommittedTurns,
            uKeepRecentTurns,
            uInputTokens,
            uTargetInputTokens,
            uCompactTriggerTurns,
            fCompactTriggerRatio
        );
        return false;
    }
    if ( uTargetInputTokens == 0u ) {
        xllm__session_trace_auto_compact_check(
            pSession,
            false,
            uCommittedTurns,
            uKeepRecentTurns,
            uInputTokens,
            uTargetInputTokens,
            uCompactTriggerTurns,
            fCompactTriggerRatio
        );
        return false;
    }

    if ( fCompactTriggerRatio <= 0.0 || fCompactTriggerRatio > 1.0 ) {
        fCompactTriggerRatio = 0.85;
    }

    bTriggered = (double)uInputTokens >= ((double)uTargetInputTokens * fCompactTriggerRatio);
    if ( bTriggered ) {
        xllm__session_logf(
            pSession,
            XLLM_LOG_DEBUG,
            "xllm.session",
            "auto compact triggered: committed_turns=%u input_tokens=%u target_input_tokens=%u ratio=%.2f",
            (unsigned)uCommittedTurns,
            (unsigned)uInputTokens,
            (unsigned)uTargetInputTokens,
            fCompactTriggerRatio
        );
    }
    xllm__session_trace_auto_compact_check(
        pSession,
        bTriggered,
        uCommittedTurns,
        uKeepRecentTurns,
        uInputTokens,
        uTargetInputTokens,
        uCompactTriggerTurns,
        fCompactTriggerRatio
    );
    return bTriggered;
}

static int xllm__session_commit_turn(
    xllm_session *pSession,
    const xllm_turn_request *pTurn,
    const xllm_message *pExtraMessages,
    size_t iExtraMessageCount,
    const xllm_response *pResponse
)
{
    int32 iStatus = XRT_NET_OK;

    if ( !pSession || !pTurn || !pResponse ) {
        return XRT_NET_ERROR;
    }

    xllm__session_lock(pSession);
    if ( xllm__session_append_history_array_locked(pSession, pTurn->pMessages, pTurn->iMessageCount) != XRT_NET_OK ||
         (iExtraMessageCount > 0u &&
          xllm__session_append_history_array_locked(pSession, pExtraMessages, iExtraMessageCount) != XRT_NET_OK) ||
         xllm__session_commit_response_locked(pSession, pResponse) != XRT_NET_OK ) {
        iStatus = XRT_NET_ERROR;
    } else {
        ++pSession->uCommittedTurns;
    }
    xllm__session_unlock(pSession);
    return iStatus;
}

static xllm__send_async_task *xllm__send_async_task_create(
    xllm *pLlm,
    const xllm_turn *pTurn,
    const xllm_call_options *pOptions
)
{
    xllm__send_async_task *pTask;

    if ( !pLlm || !pTurn ) {
        return NULL;
    }

    pTask = (xllm__send_async_task *)xrtCalloc(1, sizeof(*pTask));
    if ( !pTask ) {
        return NULL;
    }

    pTask->pLlm = pLlm;
    if ( xllm__turn_clone(&pTask->tTurn, pTurn) != XRT_NET_OK ) {
        xrtFree(pTask);
        return NULL;
    }

    if ( pOptions ) {
        if ( xllm__call_options_clone(&pTask->tOptions, pOptions) != XRT_NET_OK ) {
            xllm_turn_reset(&pTask->tTurn);
            xrtFree(pTask);
            return NULL;
        }
        pTask->bHasOptions = true;
    }

    return pTask;
}

static void xllm__send_async_task_destroy(xllm__send_async_task *pTask)
{
    if ( !pTask ) {
        return;
    }

    xllm_turn_reset(&pTask->tTurn);
    if ( pTask->bHasOptions ) {
        xllm__call_options_reset(&pTask->tOptions);
    }
    xrtFree(pTask);
}

static int32 xllm__send_async_task_run(xllm__send_async_task *pTask, xfuture_result *pOut)
{
    xllm_response *pResponse = NULL;
    int32 iStatus;

    if ( !pTask ) {
        return xllm__async_future_result_error(pOut, XRT_NET_ERROR, "xllm send async task is null");
    }

    iStatus = xllm_send(
        pTask->pLlm,
        &pTask->tTurn,
        pTask->bHasOptions ? &pTask->tOptions : NULL,
        &pResponse
    );

    if ( iStatus == XRT_NET_OK ) {
        memset(pOut, 0, sizeof(*pOut));
        pOut->iStatus = XRT_NET_OK;
        pOut->pValue = pResponse;
    } else {
        if ( pResponse ) {
            xllm_response_free(pResponse);
        }
        (void)xllm__async_future_result_error(pOut, iStatus, "xllm async send failed");
    }

    xllm__send_async_task_destroy(pTask);
    return pOut ? pOut->iStatus : iStatus;
}

static int32 xllm__send_async_task_thread_fn(ptr pArg, xfuture_result *pOut)
{
    return xllm__send_async_task_run((xllm__send_async_task *)pArg, pOut);
}

static int32 xllm__send_async_task_engine_fn(xnetworker *pWorker, ptr pArg, xfuture_result *pOut)
{
    (void)pWorker;
    return xllm__send_async_task_run((xllm__send_async_task *)pArg, pOut);
}

static int32 xllm__send_async_task_co_fn(ptr pArg, xfuture_result *pOut)
{
    return xllm__send_async_task_run((xllm__send_async_task *)pArg, pOut);
}

static xllm__session_async_chat_task *xllm__session_async_chat_task_create(
    xllm_session *pSession,
    const xllm_turn_request *pTurn,
    const xllm_call_options *pOptions
)
{
    xllm__session_async_chat_task *pTask;

    if ( !pSession || !pTurn ) {
        return NULL;
    }

    pTask = (xllm__session_async_chat_task *)xrtCalloc(1, sizeof(*pTask));
    if ( !pTask ) {
        return NULL;
    }

    pTask->pSession = pSession;
    if ( xllm__turn_clone(&pTask->tTurn, pTurn) != XRT_NET_OK ) {
        xrtFree(pTask);
        return NULL;
    }

    if ( pOptions ) {
        if ( xllm__call_options_clone(&pTask->tOptions, pOptions) != XRT_NET_OK ) {
            xllm_turn_reset(&pTask->tTurn);
            xrtFree(pTask);
            return NULL;
        }
        pTask->bHasOptions = true;
    }

    return pTask;
}

static void xllm__session_async_chat_task_destroy(xllm__session_async_chat_task *pTask)
{
    if ( !pTask ) {
        return;
    }

    xllm_turn_reset(&pTask->tTurn);
    if ( pTask->bHasOptions ) {
        xllm__call_options_reset(&pTask->tOptions);
    }
    xrtFree(pTask);
}

static int32 xllm__session_async_chat_task_run(xllm__session_async_chat_task *pTask, xfuture_result *pOut)
{
    xllm_response *pResponse = NULL;
    int32 iStatus;

    if ( !pTask ) {
        return xllm__async_future_result_error(pOut, XRT_NET_ERROR, "xllm session async task is null");
    }

    iStatus = xllm_session_chat(
        pTask->pSession,
        &pTask->tTurn,
        pTask->bHasOptions ? &pTask->tOptions : NULL,
        &pResponse
    );

    if ( iStatus == XRT_NET_OK ) {
        memset(pOut, 0, sizeof(*pOut));
        pOut->iStatus = XRT_NET_OK;
        pOut->pValue = pResponse;
    } else {
        if ( pResponse ) {
            xllm_response_free(pResponse);
        }
        (void)xllm__async_future_result_error(pOut, iStatus, "xllm session async chat failed");
    }

    xllm__session_async_chat_task_destroy(pTask);
    return pOut ? pOut->iStatus : iStatus;
}

static int32 xllm__session_async_chat_task_thread_fn(ptr pArg, xfuture_result *pOut)
{
    return xllm__session_async_chat_task_run((xllm__session_async_chat_task *)pArg, pOut);
}

static int32 xllm__session_async_chat_task_engine_fn(xnetworker *pWorker, ptr pArg, xfuture_result *pOut)
{
    (void)pWorker;
    return xllm__session_async_chat_task_run((xllm__session_async_chat_task *)pArg, pOut);
}

static int32 xllm__session_async_chat_task_co_fn(ptr pArg, xfuture_result *pOut)
{
    return xllm__session_async_chat_task_run((xllm__session_async_chat_task *)pArg, pOut);
}

XLLM_API xllm *xllm_create(xllm_runtime *pRuntime, const xllm_create_options *pOptions)
{
    xllm *pLlm;

    if ( !pRuntime ) {
        return NULL;
    }

    pLlm = (xllm *)xrtCalloc(1, sizeof(*pLlm));
    if ( !pLlm ) {
        return NULL;
    }

    pLlm->pRuntime = pRuntime;
    xllm_call_options_init(&pLlm->tDefaultCallOptions);

    if ( pOptions ) {
        if ( pOptions->sInitialProfileId ) {
            if ( !xllm__runtime_find_profile(pRuntime, pOptions->sInitialProfileId) ) {
                xrtFree(pLlm);
                return NULL;
            }
            pLlm->sProfileId = xllm__dup_cstr(pOptions->sInitialProfileId);
        }
        pLlm->sSystemPrompt = xllm__dup_cstr(pOptions->sSystemPrompt);
        if ( xllm__call_options_clone(&pLlm->tDefaultCallOptions, &pOptions->tDefaultCallOptions) != XRT_NET_OK ) {
            xllm__free_cstr(&pLlm->sProfileId);
            xllm__free_cstr(&pLlm->sSystemPrompt);
            xrtFree(pLlm);
            return NULL;
        }
    }

    return pLlm;
}

XLLM_API void xllm_destroy(xllm *pLlm)
{
    if ( !pLlm ) {
        return;
    }

    xllm__free_cstr(&pLlm->sProfileId);
    xllm__free_cstr(&pLlm->sSystemPrompt);
    xllm__call_options_reset(&pLlm->tDefaultCallOptions);
    xrtFree(pLlm);
}

XLLM_API int xllm_bind_profile(xllm *pLlm, const char *sProfileId)
{
    char *sCopy;

    if ( !pLlm || !sProfileId || !xllm__runtime_find_profile(pLlm->pRuntime, sProfileId) ) {
        return XRT_NET_ERROR;
    }

    sCopy = xllm__dup_cstr(sProfileId);
    if ( !sCopy ) {
        return XRT_NET_ERROR;
    }

    xllm__free_cstr(&pLlm->sProfileId);
    pLlm->sProfileId = sCopy;
    return XRT_NET_OK;
}

XLLM_API int xllm_set_system_prompt(xllm *pLlm, const char *sText)
{
    char *sCopy = NULL;

    if ( !pLlm ) {
        return XRT_NET_ERROR;
    }

    if ( sText ) {
        sCopy = xllm__dup_cstr(sText);
        if ( !sCopy ) {
            return XRT_NET_ERROR;
        }
    }

    xllm__free_cstr(&pLlm->sSystemPrompt);
    pLlm->sSystemPrompt = sCopy;
    return XRT_NET_OK;
}

XLLM_API const char *xllm_get_system_prompt(const xllm *pLlm)
{
    return pLlm ? pLlm->sSystemPrompt : NULL;
}

XLLM_API int xllm_set_tool_executor(xllm *pLlm, const xllm_tool_executor *pExecutor)
{
    if ( !pLlm ) {
        return XRT_NET_ERROR;
    }

    if ( pExecutor ) {
        pLlm->tToolExecutor = *pExecutor;
        pLlm->bHasToolExecutor = true;
    } else {
        memset(&pLlm->tToolExecutor, 0, sizeof(pLlm->tToolExecutor));
        pLlm->bHasToolExecutor = false;
    }

    return XRT_NET_OK;
}

XLLM_API int xllm_set_tool_executor_async(xllm *pLlm, const xllm_tool_executor_async *pExecutor)
{
    if ( !pLlm ) {
        return XRT_NET_ERROR;
    }

    if ( pExecutor ) {
        pLlm->tToolExecutorAsync = *pExecutor;
        pLlm->bHasToolExecutorAsync = true;
    } else {
        memset(&pLlm->tToolExecutorAsync, 0, sizeof(pLlm->tToolExecutorAsync));
        pLlm->bHasToolExecutorAsync = false;
    }

    return XRT_NET_OK;
}

XLLM_API int xllm_send_ex(
    xllm *pLlm,
    const xllm_turn *pTurn,
    const xllm_call_options *pOptions,
    xllm_response **ppResponse,
    xllm_error *pError
)
{
    xllm_request tRequest;
    xllm_error tLocalError;
    xllm_error *pWorkError = pError ? pError : &tLocalError;
    int32 iStatus;

    xllm_error_init(&tLocalError);
    xllm_error_reset(pWorkError);

    if ( !pLlm || !pTurn || !ppResponse || !pLlm->sProfileId ) {
        xllm__error_set(pWorkError, XLLM_ERROR_INVALID_REQUEST, "send arguments are invalid");
        xllm_error_free(&tLocalError);
        return XRT_NET_ERROR;
    }

    xllm_request_init(&tRequest);
    iStatus = xllm__build_request_from_turn(&tRequest, pLlm->sProfileId, pLlm->sSystemPrompt, pTurn);
    if ( iStatus != XRT_NET_OK ) {
        xllm__error_set(pWorkError, XLLM_ERROR_INTERNAL, "failed to build request from turn");
        xllm_error_free(&tLocalError);
        return iStatus;
    }

    iStatus = xllm__send_with_tool_loop(
        pLlm,
        &tRequest,
        xllm__resolve_call_options(&pLlm->tDefaultCallOptions, pOptions),
        ppResponse,
        pWorkError
    );
    xllm_request_reset(&tRequest);
    xllm_error_free(&tLocalError);
    return iStatus;
}

XLLM_API int xllm_send(
    xllm *pLlm,
    const xllm_turn *pTurn,
    const xllm_call_options *pOptions,
    xllm_response **ppResponse
)
{
    xllm_error tError;
    int32 iStatus;

    xllm_error_init(&tError);
    iStatus = xllm_send_ex(pLlm, pTurn, pOptions, ppResponse, &tError);
    xllm_error_free(&tError);
    return iStatus;
}

XLLM_API xfuture *xllm_send_async_thread(
    xllm *pLlm,
    const xllm_turn *pTurn,
    const xllm_call_options *pOptions
)
{
    xllm__send_async_task *pTask;
    xfuture *pFuture;

    if ( !pLlm || !pTurn || !pLlm->sProfileId ) {
        return xllm__make_error_future(XRT_NET_ERROR, "xllm send arguments are invalid");
    }

    pTask = xllm__send_async_task_create(pLlm, pTurn, pOptions);
    if ( !pTask ) {
        return xllm__make_error_future(XRT_NET_ERROR, "xllm failed to build request");
    }

    pFuture = xTaskRunThread(xllm__send_async_task_thread_fn, pTask, 0u);
    if ( !pFuture ) {
        xllm__send_async_task_destroy(pTask);
        return xllm__make_error_future(XRT_NET_ERROR, "xllm async send task create failed");
    }
    return pFuture;
}

XLLM_API xfuture *xllm_send_async_engine(
    xllm *pLlm,
    xnetengine *pEngine,
    uint32 uAffinityKey,
    const xllm_turn *pTurn,
    const xllm_call_options *pOptions
)
{
    xllm__send_async_task *pTask;
    xfuture *pFuture;

    if ( !pLlm || !pTurn || !pLlm->sProfileId ) {
        return xllm__make_error_future(XRT_NET_ERROR, "xllm send arguments are invalid");
    }

    pTask = xllm__send_async_task_create(pLlm, pTurn, pOptions);
    if ( !pTask ) {
        return xllm__make_error_future(XRT_NET_ERROR, "xllm failed to build request");
    }

    pFuture = xTaskRunEngine(pEngine, uAffinityKey, xllm__send_async_task_engine_fn, pTask);
    if ( !pFuture ) {
        xllm__send_async_task_destroy(pTask);
        return xllm__make_error_future(XRT_NET_ERROR, "xllm async engine send task create failed");
    }
    return pFuture;
}

XLLM_API xfuture *xllm_send_async_co(
    xllm *pLlm,
    xcosched *pSched,
    const xllm_turn *pTurn,
    const xllm_call_options *pOptions,
    size_t iStackSize
)
{
    xllm__send_async_task *pTask;
    xfuture *pFuture;

    if ( !pLlm || !pTurn || !pLlm->sProfileId ) {
        return xllm__make_error_future(XRT_NET_ERROR, "xllm send arguments are invalid");
    }

    pTask = xllm__send_async_task_create(pLlm, pTurn, pOptions);
    if ( !pTask ) {
        return xllm__make_error_future(XRT_NET_ERROR, "xllm failed to build request");
    }

    pFuture = xTaskRunCo(pSched, xllm__send_async_task_co_fn, pTask, iStackSize);
    if ( !pFuture ) {
        xllm__send_async_task_destroy(pTask);
        return xllm__make_error_future(XRT_NET_ERROR, "xllm async coroutine send task create failed");
    }
    return pFuture;
}

XLLM_API int xllm_session_create(
    xllm_runtime *pRuntime,
    const xllm_session_options *pOptions,
    xllm_session **ppSession
)
{
    xllm_session *pSession;

    if ( !pRuntime || !ppSession ) {
        return XRT_NET_ERROR;
    }

    *ppSession = NULL;
    pSession = (xllm_session *)xrtCalloc(1, sizeof(*pSession));
    if ( !pSession ) {
        return XRT_NET_ERROR;
    }

    pSession->pRuntime = pRuntime;
    pSession->pMutex = xrtMutexCreate();
    if ( !pSession->pMutex ) {
        xrtFree(pSession);
        return XRT_NET_ERROR;
    }
    xllm_session_options_init(&pSession->tOptions);
    if ( pOptions ) {
        if ( xllm__session_options_clone(&pSession->tOptions, pOptions) != XRT_NET_OK ) {
            xrtMutexDestroy(pSession->pMutex);
            xrtFree(pSession);
            return XRT_NET_ERROR;
        }
    }

    if ( pSession->tOptions.sProfileId ) {
        pSession->sProfileId = xllm__dup_cstr(pSession->tOptions.sProfileId);
    }
    if ( pSession->tOptions.sSystemPrompt ) {
        pSession->sSystemPrompt = xllm__dup_cstr(pSession->tOptions.sSystemPrompt);
    }

    *ppSession = pSession;
    return XRT_NET_OK;
}

XLLM_API void xllm_session_destroy(xllm_session *pSession)
{
    if ( !pSession ) {
        return;
    }

    xllm__session_reset_locked(pSession);
    xllm__free_cstr(&pSession->sProfileId);
    xllm__free_cstr(&pSession->sSystemPrompt);
    xllm__session_options_reset(&pSession->tOptions);
    if ( pSession->pMutex ) {
        xrtMutexDestroy(pSession->pMutex);
    }
    xrtFree(pSession);
}

XLLM_API int xllm_session_set_system_prompt(xllm_session *pSession, const char *sText)
{
    char *sCopy = NULL;

    if ( !pSession ) {
        return XRT_NET_ERROR;
    }

    if ( sText ) {
        sCopy = xllm__dup_cstr(sText);
        if ( !sCopy ) {
            return XRT_NET_ERROR;
        }
    }

    xllm__free_cstr(&pSession->sSystemPrompt);
    pSession->sSystemPrompt = sCopy;
    return XRT_NET_OK;
}

XLLM_API int xllm_session_set_tool_executor(xllm_session *pSession, const xllm_tool_executor *pExecutor)
{
    if ( !pSession ) {
        return XRT_NET_ERROR;
    }

    if ( pExecutor ) {
        pSession->tToolExecutor = *pExecutor;
        pSession->bHasToolExecutor = true;
    } else {
        memset(&pSession->tToolExecutor, 0, sizeof(pSession->tToolExecutor));
        pSession->bHasToolExecutor = false;
    }

    return XRT_NET_OK;
}

XLLM_API int xllm_session_set_tool_executor_async(xllm_session *pSession, const xllm_tool_executor_async *pExecutor)
{
    if ( !pSession ) {
        return XRT_NET_ERROR;
    }

    if ( pExecutor ) {
        pSession->tToolExecutorAsync = *pExecutor;
        pSession->bHasToolExecutorAsync = true;
    } else {
        memset(&pSession->tToolExecutorAsync, 0, sizeof(pSession->tToolExecutorAsync));
        pSession->bHasToolExecutorAsync = false;
    }

    return XRT_NET_OK;
}

XLLM_API int xllm_session_clear_history(xllm_session *pSession)
{
    if ( !pSession ) {
        return XRT_NET_ERROR;
    }

    xllm__session_lock(pSession);
    xllm__session_reset_locked(pSession);
    xllm__session_unlock(pSession);
    return XRT_NET_OK;
}

XLLM_API int xllm_session_compact(
    xllm_session *pSession,
    const xllm_compact_options *pOptions,
    xllm_compact_result *pResult
)
{
    xllm_message *pNewHistory = NULL;
    size_t iNewHistoryCount = 0u;
    xllm_compact_strategy eStrategy;
    uint32 uKeepRecentTurns;
    uint32 uRemovedTurns = 0u;
    uint32 uNewSummaryTurns = 0u;
    uint32 uCandidateInputTokens = 0u;
    uint32 uTruncatedInputTokens = 0u;
    size_t iKeepStart;
    bool bApplyCandidateHistory = false;
    bool bApplyClearHistory = false;
    char *sOldSummary = NULL;
    char *sNewSummary = NULL;
    xllm__session_compact_trace_state tTraceState;

    if ( !pSession || !pResult ) {
        return XRT_NET_ERROR;
    }

    memset(pResult, 0, sizeof(*pResult));
    memset(&tTraceState, 0, sizeof(tTraceState));

    pResult->uInputTokensBefore = xllm__session_snapshot_input_tokens(pSession);
    tTraceState.uInputTokensBefore = pResult->uInputTokensBefore;

    xllm__session_lock(pSession);
    eStrategy = pOptions ? pOptions->eStrategy : pSession->tOptions.eCompactStrategy;
    if ( eStrategy == XLLM_COMPACT_CUSTOM ) {
        eStrategy = pSession->tOptions.eCompactStrategy;
    }
    if ( pOptions && pOptions->eMode == XLLM_COMPACT_TRUNCATE_ONLY ) {
        eStrategy = XLLM_COMPACT_TRUNCATE;
    }
    uKeepRecentTurns = pSession->tOptions.uKeepRecentTurns ? pSession->tOptions.uKeepRecentTurns : 4u;
    if ( pOptions && pOptions->eMode == XLLM_COMPACT_SUMMARIZE_OLDER_THAN_TURN ) {
        if ( pOptions->uOlderThanTurn >= pSession->uCommittedTurns ) {
            uKeepRecentTurns = 0u;
        } else {
            uKeepRecentTurns = pSession->uCommittedTurns - pOptions->uOlderThanTurn;
        }
    }
    tTraceState.eStrategy = eStrategy;
    tTraceState.uKeepRecentTurns = uKeepRecentTurns;
    tTraceState.uSummaryTurnsBefore = pSession->uSummaryTurns;

    iKeepStart = xllm__session_history_keep_start_index(
        pSession->pHistory,
        pSession->iHistoryCount,
        uKeepRecentTurns
    );
    uRemovedTurns = xllm__session_history_count_turns_prefix(pSession->pHistory, pSession->iHistoryCount, iKeepStart);
    tTraceState.uRemovedTurns = uRemovedTurns;
    if ( iKeepStart > 0u && iKeepStart < pSession->iHistoryCount ) {
        if ( eStrategy == XLLM_COMPACT_SUMMARIZE ) {
            sOldSummary = xllm__dup_cstr(pSession->sSessionSummary);
            if ( pSession->uSummaryTurns > 0u ) {
                uNewSummaryTurns = pSession->uSummaryTurns;
            }
            /* Hold the session lock so compaction sees a stable history snapshot while the
             * optional model-based summarizer runs. */
            sNewSummary = xllm__session_try_model_summary_locked(
                pSession,
                sOldSummary,
                pSession->pHistory,
                iKeepStart,
                &tTraceState
            );
            if ( !sNewSummary ) {
                sNewSummary = xllm__session_build_rolling_summary(
                    sOldSummary,
                    pSession->pHistory,
                    iKeepStart
                );
                if ( sNewSummary ) {
                    tTraceState.bLocalFallbackUsed = true;
                }
            }
            xllm__free_cstr(&sOldSummary);
        }
        if ( xllm__message_array_clone(
                &pNewHistory,
                &iNewHistoryCount,
                pSession->pHistory + iKeepStart,
                pSession->iHistoryCount - iKeepStart
             ) == XRT_NET_OK ) {
            bApplyCandidateHistory = true;
            if ( eStrategy == XLLM_COMPACT_SUMMARIZE && sNewSummary ) {
                uCandidateInputTokens = xllm__session_snapshot_input_tokens_locked(
                    pSession,
                    pNewHistory,
                    iNewHistoryCount,
                    sNewSummary,
                    uNewSummaryTurns + uRemovedTurns
                );
                if ( pResult->uInputTokensBefore > 0u &&
                     uCandidateInputTokens > 0u &&
                     uCandidateInputTokens >= pResult->uInputTokensBefore ) {
                    uTruncatedInputTokens = xllm__session_snapshot_input_tokens_locked(
                        pSession,
                        pNewHistory,
                        iNewHistoryCount,
                        pSession->sSessionSummary,
                        pSession->uSummaryTurns
                    );
                    if ( uTruncatedInputTokens > 0u &&
                         uTruncatedInputTokens < pResult->uInputTokensBefore ) {
                        xllm__free_cstr(&sNewSummary);
                    } else {
                        xllm__message_array_free(pNewHistory, iNewHistoryCount);
                        pNewHistory = NULL;
                        iNewHistoryCount = 0u;
                        bApplyCandidateHistory = false;
                    }
                }
            }
        }
        if ( bApplyCandidateHistory ) {
            xllm__session_history_reset_locked(pSession);
            pSession->pHistory = pNewHistory;
            pSession->iHistoryCount = iNewHistoryCount;
            pSession->iHistoryCapacity = iNewHistoryCount;
            pSession->uCommittedTurns = xllm__session_history_count_turns(pSession->pHistory, pSession->iHistoryCount);
            if ( eStrategy == XLLM_COMPACT_SUMMARIZE && sNewSummary ) {
                xllm__free_cstr(&pSession->sSessionSummary);
                pSession->sSessionSummary = sNewSummary;
                sNewSummary = NULL;
                pSession->uSummaryTurns = uNewSummaryTurns + uRemovedTurns;
                pResult->bSummarized = true;
            }
            pResult->bCompacted = true;
            tTraceState.uHistoryTurnsAfter = pSession->uCommittedTurns;
        }
    } else if ( iKeepStart >= pSession->iHistoryCount && pSession->iHistoryCount > 0u ) {
        if ( eStrategy == XLLM_COMPACT_SUMMARIZE ) {
            sOldSummary = xllm__dup_cstr(pSession->sSessionSummary);
            if ( pSession->uSummaryTurns > 0u ) {
                uNewSummaryTurns = pSession->uSummaryTurns;
            }
            sNewSummary = xllm__session_try_model_summary_locked(
                pSession,
                sOldSummary,
                pSession->pHistory,
                pSession->iHistoryCount,
                &tTraceState
            );
            if ( !sNewSummary ) {
                sNewSummary = xllm__session_build_rolling_summary(
                    sOldSummary,
                    pSession->pHistory,
                    pSession->iHistoryCount
                );
                if ( sNewSummary ) {
                    tTraceState.bLocalFallbackUsed = true;
                }
            }
            xllm__free_cstr(&sOldSummary);
        }
        if ( eStrategy == XLLM_COMPACT_SUMMARIZE && sNewSummary ) {
            uCandidateInputTokens = xllm__session_snapshot_input_tokens_locked(
                pSession,
                NULL,
                0u,
                sNewSummary,
                uNewSummaryTurns + uRemovedTurns
            );
            if ( pResult->uInputTokensBefore > 0u &&
                 uCandidateInputTokens > 0u &&
                 uCandidateInputTokens >= pResult->uInputTokensBefore ) {
                uTruncatedInputTokens = xllm__session_snapshot_input_tokens_locked(
                    pSession,
                    NULL,
                    0u,
                    pSession->sSessionSummary,
                    pSession->uSummaryTurns
                );
                if ( !(uTruncatedInputTokens > 0u &&
                       uTruncatedInputTokens < pResult->uInputTokensBefore) ) {
                    xllm__free_cstr(&sNewSummary);
                } else {
                    xllm__free_cstr(&sNewSummary);
                    bApplyClearHistory = true;
                }
            } else {
                bApplyClearHistory = true;
            }
        } else {
            bApplyClearHistory = true;
        }
        if ( bApplyClearHistory ) {
            xllm__session_history_reset_locked(pSession);
            if ( eStrategy == XLLM_COMPACT_SUMMARIZE && sNewSummary ) {
                xllm__free_cstr(&pSession->sSessionSummary);
                pSession->sSessionSummary = sNewSummary;
                sNewSummary = NULL;
                pSession->uSummaryTurns = uNewSummaryTurns + uRemovedTurns;
                pResult->bSummarized = true;
            }
            pResult->bCompacted = true;
            tTraceState.uHistoryTurnsAfter = pSession->uCommittedTurns;
        }
    }
    tTraceState.uSummaryTurnsAfter = pSession->uSummaryTurns;
    xllm__session_unlock(pSession);
    xllm__free_cstr(&sOldSummary);
    xllm__free_cstr(&sNewSummary);

    pResult->uInputTokensAfter = xllm__session_snapshot_input_tokens(pSession);
    tTraceState.uInputTokensAfter = pResult->uInputTokensAfter;
    if ( !pResult->bCompacted ) {
        pResult->uInputTokensAfter = pResult->uInputTokensBefore;
        tTraceState.uInputTokensAfter = pResult->uInputTokensAfter;
    }
    xllm__session_logf(
        pSession,
        pResult->bCompacted ? XLLM_LOG_INFO : XLLM_LOG_DEBUG,
        "xllm.session",
        "compact result: compacted=%s summarized=%s strategy=%s removed_turns=%u input_before=%u input_after=%u summary_source=%s",
        pResult->bCompacted ? "true" : "false",
        pResult->bSummarized ? "true" : "false",
        xllm__compact_strategy_name(tTraceState.eStrategy),
        (unsigned)tTraceState.uRemovedTurns,
        (unsigned)tTraceState.uInputTokensBefore,
        (unsigned)tTraceState.uInputTokensAfter,
        tTraceState.bRemoteSummarySucceeded ? "remote" : (tTraceState.bLocalFallbackUsed ? "local_fallback" : "none")
    );
    xllm__session_trace_compact_result(pSession, &tTraceState, pResult);
    return XRT_NET_OK;
}

XLLM_API int xllm_session_chat_ex(
    xllm_session *pSession,
    const xllm_turn_request *pTurn,
    const xllm_call_options *pOptions,
    xllm_response **ppResponse,
    xllm_error *pError
)
{
    xllm_request tRequest;
    xllm_compact_result tCompactResult;
    xllm_error tLocalError;
    xllm_error *pWorkError = pError ? pError : &tLocalError;
    bool bShouldAutoCompact = false;
    size_t iRequestMessageCountBeforeLoop = 0u;
    int32 iStatus;

    xllm_error_init(&tLocalError);
    xllm_error_reset(pWorkError);

    if ( !pSession || !pTurn || !ppResponse || !pSession->sProfileId ) {
        xllm__error_set(pWorkError, XLLM_ERROR_INVALID_REQUEST, "session chat arguments are invalid");
        xllm_error_free(&tLocalError);
        return XRT_NET_ERROR;
    }

    iStatus = xllm__session_build_request_snapshot(pSession, pTurn, &tRequest);
    if ( iStatus != XRT_NET_OK ) {
        xllm__error_set(pWorkError, XLLM_ERROR_INTERNAL, "failed to build session request snapshot");
        xllm_error_free(&tLocalError);
        return iStatus;
    }

    iRequestMessageCountBeforeLoop = tRequest.iMessageCount;
    if ( (pSession->bHasToolExecutor && pSession->tToolExecutor.pfnExecute) ||
         (pSession->bHasToolExecutorAsync && pSession->tToolExecutorAsync.pfnExecute) ) {
        xllm tToolLoopLlm;

        memset(&tToolLoopLlm, 0, sizeof(tToolLoopLlm));
        tToolLoopLlm.pRuntime = pSession->pRuntime;
        tToolLoopLlm.bHasToolExecutor = pSession->bHasToolExecutor;
        tToolLoopLlm.tToolExecutor = pSession->tToolExecutor;
        tToolLoopLlm.bHasToolExecutorAsync = pSession->bHasToolExecutorAsync;
        tToolLoopLlm.tToolExecutorAsync = pSession->tToolExecutorAsync;
        iStatus = xllm__send_with_tool_loop(&tToolLoopLlm, &tRequest, pOptions, ppResponse, pWorkError);
    } else {
        iStatus = xllm_chat_ex(pSession->pRuntime, &tRequest, pOptions, ppResponse, pWorkError);
    }
    if ( iStatus == XRT_NET_OK && *ppResponse ) {
        const xllm_message *pExtraMessages = NULL;
        size_t iExtraMessageCount = 0u;

        if ( tRequest.iMessageCount > iRequestMessageCountBeforeLoop ) {
            pExtraMessages = tRequest.pMessages + iRequestMessageCountBeforeLoop;
            iExtraMessageCount = tRequest.iMessageCount - iRequestMessageCountBeforeLoop;
        }
        if ( xllm__session_commit_turn(pSession, pTurn, pExtraMessages, iExtraMessageCount, *ppResponse) == XRT_NET_OK ) {
            bShouldAutoCompact = xllm__session_should_auto_compact(pSession);
        }
    }
    xllm_request_reset(&tRequest);

    if ( bShouldAutoCompact ) {
        memset(&tCompactResult, 0, sizeof(tCompactResult));
        (void)xllm_session_compact(pSession, NULL, &tCompactResult);
    }
    xllm_error_free(&tLocalError);
    return iStatus;
}

XLLM_API int xllm_session_chat(
    xllm_session *pSession,
    const xllm_turn_request *pTurn,
    const xllm_call_options *pOptions,
    xllm_response **ppResponse
)
{
    xllm_error tError;
    int32 iStatus;

    xllm_error_init(&tError);
    iStatus = xllm_session_chat_ex(pSession, pTurn, pOptions, ppResponse, &tError);
    xllm_error_free(&tError);
    return iStatus;
}

XLLM_API xfuture *xllm_session_chat_async_thread(
    xllm_session *pSession,
    const xllm_turn_request *pTurn,
    const xllm_call_options *pOptions
)
{
    if ( !pSession || !pTurn || !pSession->sProfileId ) {
        return xllm__make_error_future(XRT_NET_ERROR, "xllm session arguments are invalid");
    }

    {
        xllm__session_async_chat_task *pTask = xllm__session_async_chat_task_create(pSession, pTurn, pOptions);
        if ( !pTask ) {
            return xllm__make_error_future(XRT_NET_ERROR, "xllm failed to build session async task");
        }
        return xTaskRunThread(xllm__session_async_chat_task_thread_fn, pTask, 0);
    }
}

XLLM_API xfuture *xllm_session_chat_async_engine(
    xllm_session *pSession,
    xnetengine *pEngine,
    uint32 uAffinityKey,
    const xllm_turn_request *pTurn,
    const xllm_call_options *pOptions
)
{
    xllm__session_async_chat_task *pTask;

    if ( !pSession || !pTurn || !pSession->sProfileId ) {
        return xllm__make_error_future(XRT_NET_ERROR, "xllm session arguments are invalid");
    }
    if ( !pEngine ) {
        return xllm__make_error_future(XRT_NET_ERROR, "xllm engine is null");
    }

    pTask = xllm__session_async_chat_task_create(pSession, pTurn, pOptions);
    if ( !pTask ) {
        return xllm__make_error_future(XRT_NET_ERROR, "xllm failed to build session async task");
    }

    return xTaskRunEngine(pEngine, uAffinityKey, xllm__session_async_chat_task_engine_fn, pTask);
}

XLLM_API xfuture *xllm_session_chat_async_co(
    xllm_session *pSession,
    xcosched *pSched,
    const xllm_turn_request *pTurn,
    const xllm_call_options *pOptions,
    size_t iStackSize
)
{
    xllm__session_async_chat_task *pTask;

    if ( !pSession || !pTurn || !pSession->sProfileId ) {
        return xllm__make_error_future(XRT_NET_ERROR, "xllm session arguments are invalid");
    }
    if ( !pSched ) {
        return xllm__make_error_future(XRT_NET_ERROR, "xllm coroutine scheduler is null");
    }

    pTask = xllm__session_async_chat_task_create(pSession, pTurn, pOptions);
    if ( !pTask ) {
        return xllm__make_error_future(XRT_NET_ERROR, "xllm failed to build session async task");
    }

#if !defined(XRT_NO_COROUTINE)
    return xTaskRunCo(pSched, xllm__session_async_chat_task_co_fn, pTask, iStackSize);
#else
    xllm__session_async_chat_task_destroy(pTask);
    return xllm__make_error_future(XRT_NET_ERROR, "xrt coroutine support is disabled");
#endif
}

static bool xllm__session_state_table_set_text(xvalue tTable, const char *sKey, const char *sValue)
{
    if ( !tTable || !sKey ) {
        return false;
    }
    if ( !sValue ) {
        return true;
    }
    return xvoTableSetText(tTable, (str)sKey, 0u, (str)sValue, 0u, FALSE);
}

static bool xllm__session_state_table_set_value(xvalue tTable, const char *sKey, xvalue tValue)
{
    if ( !tTable || !sKey ) {
        return false;
    }
    if ( !tValue || xvoType(tValue) == XVO_DT_NULL ) {
        return true;
    }
    return xvoTableSetValue(tTable, (str)sKey, 0u, tValue, FALSE);
}

static const char *xllm__session_state_role_name(xllm_role eRole)
{
    switch ( eRole ) {
        case XLLM_ROLE_SYSTEM: return "system";
        case XLLM_ROLE_USER: return "user";
        case XLLM_ROLE_ASSISTANT: return "assistant";
        case XLLM_ROLE_TOOL: return "tool";
        default: return NULL;
    }
}

static int xllm__session_state_parse_role(const char *sRole, xllm_role *peRole)
{
    if ( !sRole || !peRole ) {
        return XRT_NET_ERROR;
    }
    if ( strcmp(sRole, "system") == 0 ) {
        *peRole = XLLM_ROLE_SYSTEM;
    } else if ( strcmp(sRole, "user") == 0 ) {
        *peRole = XLLM_ROLE_USER;
    } else if ( strcmp(sRole, "assistant") == 0 ) {
        *peRole = XLLM_ROLE_ASSISTANT;
    } else if ( strcmp(sRole, "tool") == 0 ) {
        *peRole = XLLM_ROLE_TOOL;
    } else {
        return XRT_NET_ERROR;
    }
    return XRT_NET_OK;
}

static const char *xllm__session_state_part_kind_name(xllm_part_kind eKind)
{
    switch ( eKind ) {
        case XLLM_PART_TEXT: return "text";
        case XLLM_PART_IMAGE: return "image";
        case XLLM_PART_FILE: return "file";
        case XLLM_PART_AUDIO: return "audio";
        case XLLM_PART_VIDEO: return "video";
        case XLLM_PART_JSON: return "json";
        default: return NULL;
    }
}

static int xllm__session_state_parse_part_kind(const char *sKind, xllm_part_kind *peKind)
{
    if ( !sKind || !peKind ) {
        return XRT_NET_ERROR;
    }
    if ( strcmp(sKind, "text") == 0 ) {
        *peKind = XLLM_PART_TEXT;
    } else if ( strcmp(sKind, "image") == 0 ) {
        *peKind = XLLM_PART_IMAGE;
    } else if ( strcmp(sKind, "file") == 0 ) {
        *peKind = XLLM_PART_FILE;
    } else if ( strcmp(sKind, "audio") == 0 ) {
        *peKind = XLLM_PART_AUDIO;
    } else if ( strcmp(sKind, "video") == 0 ) {
        *peKind = XLLM_PART_VIDEO;
    } else if ( strcmp(sKind, "json") == 0 ) {
        *peKind = XLLM_PART_JSON;
    } else {
        return XRT_NET_ERROR;
    }
    return XRT_NET_OK;
}

static const char *xllm__session_state_source_kind_name(xllm_source_kind eKind)
{
    switch ( eKind ) {
        case XLLM_SOURCE_INLINE_TEXT: return "inline_text";
        case XLLM_SOURCE_INLINE_BYTES: return "inline_bytes";
        case XLLM_SOURCE_URL: return "url";
        case XLLM_SOURCE_PROVIDER_FILE_ID: return "provider_file_id";
        default: return NULL;
    }
}

static int xllm__session_state_parse_source_kind(const char *sKind, xllm_source_kind *peKind)
{
    if ( !sKind || !peKind ) {
        return XRT_NET_ERROR;
    }
    if ( strcmp(sKind, "inline_text") == 0 ) {
        *peKind = XLLM_SOURCE_INLINE_TEXT;
    } else if ( strcmp(sKind, "inline_bytes") == 0 ) {
        *peKind = XLLM_SOURCE_INLINE_BYTES;
    } else if ( strcmp(sKind, "url") == 0 ) {
        *peKind = XLLM_SOURCE_URL;
    } else if ( strcmp(sKind, "provider_file_id") == 0 ) {
        *peKind = XLLM_SOURCE_PROVIDER_FILE_ID;
    } else {
        return XRT_NET_ERROR;
    }
    return XRT_NET_OK;
}

static int xllm__session_state_content_part_to_xvalue(const xllm_content_part *pPart, xvalue *ptValue)
{
    xvalue tPart;
    const char *sKind;

    if ( !pPart || !ptValue ) {
        return XRT_NET_ERROR;
    }

    *ptValue = NULL;
    sKind = xllm__session_state_part_kind_name(pPart->eKind);
    if ( !sKind ) {
        return XRT_NET_ERROR;
    }

    tPart = xvoCreateTable();
    if ( !tPart ) {
        return XRT_NET_ERROR;
    }

    if ( !xllm__session_state_table_set_text(tPart, "kind", sKind) ||
         !xllm__session_state_table_set_value(tPart, "vendor_extra", pPart->tVendorExtra) ) {
        xvoUnref(tPart);
        return XRT_NET_ERROR;
    }

    if ( pPart->eKind == XLLM_PART_JSON ) {
        if ( !pPart->as.tJsonValue || xvoType(pPart->as.tJsonValue) == XVO_DT_NULL ) {
            xvoUnref(tPart);
            return XRT_NET_ERROR;
        }
        if ( !xllm__session_state_table_set_value(tPart, "json_value", pPart->as.tJsonValue) ) {
            xvoUnref(tPart);
            return XRT_NET_ERROR;
        }
    } else {
        xvalue tSource = xvoCreateTable();
        const char *sSourceKind;

        if ( !tSource ) {
            xvoUnref(tPart);
            return XRT_NET_ERROR;
        }

        sSourceKind = xllm__session_state_source_kind_name(pPart->as.tSource.eKind);
        if ( !sSourceKind ||
             !xllm__session_state_table_set_text(tSource, "kind", sSourceKind) ||
             !xllm__session_state_table_set_text(tSource, "mime_type", pPart->as.tSource.sMimeType) ||
             !xllm__session_state_table_set_text(tSource, "name", pPart->as.tSource.sName) ) {
            xvoUnref(tSource);
            xvoUnref(tPart);
            return XRT_NET_ERROR;
        }

        switch ( pPart->as.tSource.eKind ) {
            case XLLM_SOURCE_INLINE_TEXT:
                if ( !xllm__session_state_table_set_text(tSource, "text", pPart->as.tSource.as.sText) ) {
                    xvoUnref(tSource);
                    xvoUnref(tPart);
                    return XRT_NET_ERROR;
                }
                break;
            case XLLM_SOURCE_URL:
                if ( !xllm__session_state_table_set_text(tSource, "url", pPart->as.tSource.as.sUrl) ) {
                    xvoUnref(tSource);
                    xvoUnref(tPart);
                    return XRT_NET_ERROR;
                }
                break;
            case XLLM_SOURCE_PROVIDER_FILE_ID:
                if ( !xllm__session_state_table_set_text(tSource, "file_id", pPart->as.tSource.as.sFileId) ) {
                    xvoUnref(tSource);
                    xvoUnref(tPart);
                    return XRT_NET_ERROR;
                }
                break;
            case XLLM_SOURCE_INLINE_BYTES:
            default:
                xvoUnref(tSource);
                xvoUnref(tPart);
                return XRT_NET_ERROR;
        }

        if ( !xvoTableSetValue(tPart, (str)"source", 0u, tSource, TRUE) ) {
            xvoUnref(tSource);
            xvoUnref(tPart);
            return XRT_NET_ERROR;
        }
    }

    *ptValue = tPart;
    return XRT_NET_OK;
}

static int xllm__session_state_content_part_from_xvalue(xvalue tValue, xllm_content_part *pPart)
{
    const char *sKind;
    xvalue tSource;
    xvalue tExtra;

    if ( !tValue || !pPart || xvoType(tValue) != XVO_DT_TABLE ) {
        return XRT_NET_ERROR;
    }

    memset(pPart, 0, sizeof(*pPart));
    sKind = (const char *)xvoTableGetText(tValue, (str)"kind", 0u);
    if ( xllm__session_state_parse_part_kind(sKind, &pPart->eKind) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }

    tExtra = xvoTableGetValue(tValue, (str)"vendor_extra", 0u);
    if ( tExtra && xvoType(tExtra) != XVO_DT_NULL ) {
        pPart->tVendorExtra = tExtra;
        xllm__xvalue_addref(pPart->tVendorExtra);
    }

    if ( pPart->eKind == XLLM_PART_JSON ) {
        xvalue tJson = xvoTableGetValue(tValue, (str)"json_value", 0u);
        if ( !tJson || xvoType(tJson) == XVO_DT_NULL ) {
            xllm__content_part_free(pPart);
            return XRT_NET_ERROR;
        }
        pPart->as.tJsonValue = tJson;
        xllm__xvalue_addref(pPart->as.tJsonValue);
        return XRT_NET_OK;
    }

    tSource = xvoTableGetValue(tValue, (str)"source", 0u);
    if ( !tSource || xvoType(tSource) != XVO_DT_TABLE ) {
        xllm__content_part_free(pPart);
        return XRT_NET_ERROR;
    }

    if ( xllm__session_state_parse_source_kind((const char *)xvoTableGetText(tSource, (str)"kind", 0u), &pPart->as.tSource.eKind) != XRT_NET_OK ) {
        xllm__content_part_free(pPart);
        return XRT_NET_ERROR;
    }

    pPart->as.tSource.sMimeType = xllm__dup_cstr((const char *)xvoTableGetText(tSource, (str)"mime_type", 0u));
    pPart->as.tSource.sName = xllm__dup_cstr((const char *)xvoTableGetText(tSource, (str)"name", 0u));

    switch ( pPart->as.tSource.eKind ) {
        case XLLM_SOURCE_INLINE_TEXT:
            pPart->as.tSource.as.sText = xllm__dup_cstr((const char *)xvoTableGetText(tSource, (str)"text", 0u));
            break;
        case XLLM_SOURCE_URL:
            pPart->as.tSource.as.sUrl = xllm__dup_cstr((const char *)xvoTableGetText(tSource, (str)"url", 0u));
            break;
        case XLLM_SOURCE_PROVIDER_FILE_ID:
            pPart->as.tSource.as.sFileId = xllm__dup_cstr((const char *)xvoTableGetText(tSource, (str)"file_id", 0u));
            break;
        case XLLM_SOURCE_INLINE_BYTES:
        default:
            xllm__content_part_free(pPart);
            return XRT_NET_ERROR;
    }

    return XRT_NET_OK;
}

static int xllm__session_state_tool_call_to_xvalue(const xllm_tool_call *pCall, xvalue *ptValue)
{
    xvalue tCall;

    if ( !pCall || !ptValue ) {
        return XRT_NET_ERROR;
    }

    *ptValue = NULL;
    tCall = xvoCreateTable();
    if ( !tCall ) {
        return XRT_NET_ERROR;
    }

    if ( !xllm__session_state_table_set_text(tCall, "call_id", pCall->sCallId) ||
         !xllm__session_state_table_set_text(tCall, "tool_id", pCall->sToolId) ||
         !xllm__session_state_table_set_text(tCall, "tool_name", pCall->sToolName) ||
         !xllm__session_state_table_set_text(tCall, "arguments_json", pCall->sArgumentsJson) ||
         !xllm__session_state_table_set_value(tCall, "continuation", pCall->tContinuation) ||
         !xllm__session_state_table_set_value(tCall, "vendor_extra", pCall->tVendorExtra) ) {
        xvoUnref(tCall);
        return XRT_NET_ERROR;
    }

    *ptValue = tCall;
    return XRT_NET_OK;
}

static int xllm__session_state_tool_call_from_xvalue(xvalue tValue, xllm_tool_call *pCall)
{
    xvalue tContinuation;
    xvalue tExtra;

    if ( !tValue || !pCall || xvoType(tValue) != XVO_DT_TABLE ) {
        return XRT_NET_ERROR;
    }

    memset(pCall, 0, sizeof(*pCall));
    pCall->sCallId = xllm__dup_cstr((const char *)xvoTableGetText(tValue, (str)"call_id", 0u));
    pCall->sToolId = xllm__dup_cstr((const char *)xvoTableGetText(tValue, (str)"tool_id", 0u));
    pCall->sToolName = xllm__dup_cstr((const char *)xvoTableGetText(tValue, (str)"tool_name", 0u));
    pCall->sArgumentsJson = xllm__dup_cstr((const char *)xvoTableGetText(tValue, (str)"arguments_json", 0u));

    tContinuation = xvoTableGetValue(tValue, (str)"continuation", 0u);
    if ( tContinuation && xvoType(tContinuation) != XVO_DT_NULL ) {
        pCall->tContinuation = tContinuation;
        xllm__xvalue_addref(pCall->tContinuation);
    }

    tExtra = xvoTableGetValue(tValue, (str)"vendor_extra", 0u);
    if ( tExtra && xvoType(tExtra) != XVO_DT_NULL ) {
        pCall->tVendorExtra = tExtra;
        xllm__xvalue_addref(pCall->tVendorExtra);
    }

    return XRT_NET_OK;
}

static int xllm__session_state_message_to_xvalue(const xllm_message *pMessage, xvalue *ptValue)
{
    xvalue tMessage;
    xvalue tParts = NULL;
    xvalue tToolCalls = NULL;
    const char *sRole;
    size_t i;

    if ( !pMessage || !ptValue ) {
        return XRT_NET_ERROR;
    }

    *ptValue = NULL;
    sRole = xllm__session_state_role_name(pMessage->eRole);
    if ( !sRole ) {
        return XRT_NET_ERROR;
    }

    tMessage = xvoCreateTable();
    if ( !tMessage ) {
        return XRT_NET_ERROR;
    }

    if ( !xllm__session_state_table_set_text(tMessage, "role", sRole) ||
         !xllm__session_state_table_set_text(tMessage, "tool_call_id", pMessage->sToolCallId) ||
         !xllm__session_state_table_set_text(tMessage, "tool_name", pMessage->sToolName) ||
         !xllm__session_state_table_set_value(tMessage, "vendor_extra", pMessage->tVendorExtra) ) {
        xvoUnref(tMessage);
        return XRT_NET_ERROR;
    }

    if ( pMessage->iPartCount > 0u ) {
        tParts = xvoCreateArray();
        if ( !tParts ) {
            xvoUnref(tMessage);
            return XRT_NET_ERROR;
        }
        for ( i = 0; i < pMessage->iPartCount; ++i ) {
            xvalue tPart = NULL;
            if ( xllm__session_state_content_part_to_xvalue(&pMessage->pParts[i], &tPart) != XRT_NET_OK ||
                 !tPart ||
                 !xvoArrayAppendValue(tParts, tPart, TRUE) ) {
                if ( tPart ) {
                    xvoUnref(tPart);
                }
                xvoUnref(tParts);
                xvoUnref(tMessage);
                return XRT_NET_ERROR;
            }
        }
        if ( !xvoTableSetValue(tMessage, (str)"parts", 0u, tParts, TRUE) ) {
            xvoUnref(tParts);
            xvoUnref(tMessage);
            return XRT_NET_ERROR;
        }
        tParts = NULL;
    }

    if ( pMessage->iToolCallCount > 0u ) {
        tToolCalls = xvoCreateArray();
        if ( !tToolCalls ) {
            xvoUnref(tMessage);
            return XRT_NET_ERROR;
        }
        for ( i = 0; i < pMessage->iToolCallCount; ++i ) {
            xvalue tCall = NULL;
            if ( xllm__session_state_tool_call_to_xvalue(&pMessage->pToolCalls[i], &tCall) != XRT_NET_OK ||
                 !tCall ||
                 !xvoArrayAppendValue(tToolCalls, tCall, TRUE) ) {
                if ( tCall ) {
                    xvoUnref(tCall);
                }
                xvoUnref(tToolCalls);
                xvoUnref(tMessage);
                return XRT_NET_ERROR;
            }
        }
        if ( !xvoTableSetValue(tMessage, (str)"tool_calls", 0u, tToolCalls, TRUE) ) {
            xvoUnref(tToolCalls);
            xvoUnref(tMessage);
            return XRT_NET_ERROR;
        }
        tToolCalls = NULL;
    }

    *ptValue = tMessage;
    return XRT_NET_OK;
}

static int xllm__session_state_message_from_xvalue(xvalue tValue, xllm_message *pMessage)
{
    const char *sRole;
    xvalue tParts;
    xvalue tToolCalls;
    xvalue tExtra;
    size_t i;

    if ( !tValue || !pMessage || xvoType(tValue) != XVO_DT_TABLE ) {
        return XRT_NET_ERROR;
    }

    memset(pMessage, 0, sizeof(*pMessage));
    sRole = (const char *)xvoTableGetText(tValue, (str)"role", 0u);
    if ( xllm__session_state_parse_role(sRole, &pMessage->eRole) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }

    pMessage->sToolCallId = xllm__dup_cstr((const char *)xvoTableGetText(tValue, (str)"tool_call_id", 0u));
    pMessage->sToolName = xllm__dup_cstr((const char *)xvoTableGetText(tValue, (str)"tool_name", 0u));
    tExtra = xvoTableGetValue(tValue, (str)"vendor_extra", 0u);
    if ( tExtra && xvoType(tExtra) != XVO_DT_NULL ) {
        pMessage->tVendorExtra = tExtra;
        xllm__xvalue_addref(pMessage->tVendorExtra);
    }

    tParts = xvoTableGetValue(tValue, (str)"parts", 0u);
    if ( tParts && xvoType(tParts) != XVO_DT_NULL ) {
        if ( xvoType(tParts) != XVO_DT_ARRAY ) {
            xllm__message_free(pMessage);
            return XRT_NET_ERROR;
        }
        pMessage->iPartCount = (size_t)xvoArrayItemCount(tParts);
        if ( pMessage->iPartCount > 0u ) {
            pMessage->pParts = (xllm_content_part *)xrtCalloc(pMessage->iPartCount, sizeof(xllm_content_part));
            if ( !pMessage->pParts ) {
                xllm__message_free(pMessage);
                return XRT_NET_ERROR;
            }
            for ( i = 0; i < pMessage->iPartCount; ++i ) {
                if ( xllm__session_state_content_part_from_xvalue(xvoArrayGetValue(tParts, (uint32)i), &pMessage->pParts[i]) != XRT_NET_OK ) {
                    xllm__message_free(pMessage);
                    return XRT_NET_ERROR;
                }
            }
        }
    }

    tToolCalls = xvoTableGetValue(tValue, (str)"tool_calls", 0u);
    if ( tToolCalls && xvoType(tToolCalls) != XVO_DT_NULL ) {
        if ( xvoType(tToolCalls) != XVO_DT_ARRAY ) {
            xllm__message_free(pMessage);
            return XRT_NET_ERROR;
        }
        pMessage->iToolCallCount = (size_t)xvoArrayItemCount(tToolCalls);
        if ( pMessage->iToolCallCount > 0u ) {
            pMessage->pToolCalls = (xllm_tool_call *)xrtCalloc(pMessage->iToolCallCount, sizeof(xllm_tool_call));
            if ( !pMessage->pToolCalls ) {
                xllm__message_free(pMessage);
                return XRT_NET_ERROR;
            }
            for ( i = 0; i < pMessage->iToolCallCount; ++i ) {
                if ( xllm__session_state_tool_call_from_xvalue(xvoArrayGetValue(tToolCalls, (uint32)i), &pMessage->pToolCalls[i]) != XRT_NET_OK ) {
                    xllm__message_free(pMessage);
                    return XRT_NET_ERROR;
                }
            }
        }
    }

    return XRT_NET_OK;
}

static int xllm__session_state_message_array_to_xvalue(const xllm_message *pMessages, size_t iMessageCount, xvalue *ptValue)
{
    xvalue tArray;
    size_t i;

    if ( !ptValue ) {
        return XRT_NET_ERROR;
    }

    *ptValue = NULL;
    tArray = xvoCreateArray();
    if ( !tArray ) {
        return XRT_NET_ERROR;
    }

    for ( i = 0; i < iMessageCount; ++i ) {
        xvalue tMessage = NULL;
        if ( xllm__session_state_message_to_xvalue(&pMessages[i], &tMessage) != XRT_NET_OK ||
             !tMessage ||
             !xvoArrayAppendValue(tArray, tMessage, TRUE) ) {
            if ( tMessage ) {
                xvoUnref(tMessage);
            }
            xvoUnref(tArray);
            return XRT_NET_ERROR;
        }
    }

    *ptValue = tArray;
    return XRT_NET_OK;
}

static int xllm__session_state_message_array_from_xvalue(xvalue tValue, xllm_message **ppMessages, size_t *piMessageCount)
{
    xllm_message *pMessages = NULL;
    size_t iCount;
    size_t i;

    if ( !ppMessages || !piMessageCount ) {
        return XRT_NET_ERROR;
    }

    *ppMessages = NULL;
    *piMessageCount = 0u;

    if ( !tValue || xvoType(tValue) == XVO_DT_NULL ) {
        return XRT_NET_OK;
    }
    if ( xvoType(tValue) != XVO_DT_ARRAY ) {
        return XRT_NET_ERROR;
    }

    iCount = (size_t)xvoArrayItemCount(tValue);
    if ( iCount == 0u ) {
        return XRT_NET_OK;
    }

    pMessages = (xllm_message *)xrtCalloc(iCount, sizeof(xllm_message));
    if ( !pMessages ) {
        return XRT_NET_ERROR;
    }

    for ( i = 0; i < iCount; ++i ) {
        if ( xllm__session_state_message_from_xvalue(xvoArrayGetValue(tValue, (uint32)i), &pMessages[i]) != XRT_NET_OK ) {
            xllm__message_array_free(pMessages, iCount);
            return XRT_NET_ERROR;
        }
    }

    *ppMessages = pMessages;
    *piMessageCount = iCount;
    return XRT_NET_OK;
}

XLLM_API int xllm_session_export_state(
    xllm_session *pSession,
    xllm_session_state **ppState
)
{
    xllm_session_state *pState;

    if ( !pSession || !ppState ) {
        return XRT_NET_ERROR;
    }

    *ppState = NULL;
    pState = (xllm_session_state *)xrtCalloc(1, sizeof(*pState));
    if ( !pState ) {
        return XRT_NET_ERROR;
    }
    xllm__session_state_init_defaults(pState);

    xllm__session_lock(pSession);
    pState->sProfileId = xllm__dup_cstr(pSession->sProfileId);
    pState->sSystemPrompt = xllm__dup_cstr(pSession->sSystemPrompt);
    pState->sSessionSummary = xllm__dup_cstr(pSession->sSessionSummary);
    pState->sSummarizerProfileId = xllm__dup_cstr(pSession->tOptions.sSummarizerProfileId);
    pState->uCommittedTurns = pSession->uCommittedTurns;
    pState->uSummaryTurns = pSession->uSummaryTurns;
    pState->bEnableAutoCompact = pSession->tOptions.bEnableAutoCompact;
    pState->fCompactTriggerRatio = pSession->tOptions.fCompactTriggerRatio;
    pState->uCompactTriggerTurns = pSession->tOptions.uCompactTriggerTurns;
    pState->uReserveOutputTokens = pSession->tOptions.uReserveOutputTokens;
    pState->uKeepRecentTurns = pSession->tOptions.uKeepRecentTurns;
    pState->bKeepActiveToolChain = pSession->tOptions.bKeepActiveToolChain;
    pState->eCompactStrategy = pSession->tOptions.eCompactStrategy;
    pState->tVendorExtra = pSession->tOptions.tVendorExtra;
    xllm__xvalue_addref(pState->tVendorExtra);
    if ( xllm__message_array_clone(&pState->pHistory, &pState->iHistoryCount, pSession->pHistory, pSession->iHistoryCount) != XRT_NET_OK ) {
        xllm__session_unlock(pSession);
        xllm_session_state_free(pState);
        return XRT_NET_ERROR;
    }
    xllm__session_unlock(pSession);

    *ppState = pState;
    return XRT_NET_OK;
}

XLLM_API int xllm_session_import_state(
    xllm_runtime *pRuntime,
    const xllm_session_state *pState,
    xllm_session **ppSession
)
{
    xllm_session_options tOptions;
    int32 iStatus;

    if ( !pRuntime || !pState || !ppSession ) {
        return XRT_NET_ERROR;
    }

    xllm_session_options_init(&tOptions);
    tOptions.sProfileId = pState->sProfileId;
    tOptions.sSystemPrompt = pState->sSystemPrompt;
    tOptions.bEnableAutoCompact = pState->bEnableAutoCompact;
    tOptions.fCompactTriggerRatio = pState->fCompactTriggerRatio;
    tOptions.uCompactTriggerTurns = pState->uCompactTriggerTurns;
    tOptions.uReserveOutputTokens = pState->uReserveOutputTokens;
    tOptions.uKeepRecentTurns = pState->uKeepRecentTurns;
    tOptions.bKeepActiveToolChain = pState->bKeepActiveToolChain;
    tOptions.eCompactStrategy = pState->eCompactStrategy;
    tOptions.sSummarizerProfileId = pState->sSummarizerProfileId;
    tOptions.tVendorExtra = pState->tVendorExtra;
    iStatus = xllm_session_create(pRuntime, &tOptions, ppSession);
    if ( iStatus == XRT_NET_OK && *ppSession ) {
        xllm__session_lock(*ppSession);
        (*ppSession)->sSessionSummary = xllm__dup_cstr(pState->sSessionSummary);
        if ( pState->sSessionSummary && !(*ppSession)->sSessionSummary ) {
            xllm__session_unlock(*ppSession);
            xllm_session_destroy(*ppSession);
            *ppSession = NULL;
            return XRT_NET_ERROR;
        }
        if ( xllm__message_array_clone(&(*ppSession)->pHistory, &(*ppSession)->iHistoryCount, pState->pHistory, pState->iHistoryCount) != XRT_NET_OK ) {
            xllm__session_unlock(*ppSession);
            xllm_session_destroy(*ppSession);
            *ppSession = NULL;
            return XRT_NET_ERROR;
        }
        (*ppSession)->iHistoryCapacity = (*ppSession)->iHistoryCount;
        (*ppSession)->uCommittedTurns = pState->uCommittedTurns;
        (*ppSession)->uSummaryTurns = pState->uSummaryTurns;
        xllm__session_unlock(*ppSession);
    }
    return iStatus;
}

XLLM_API int xllm_session_state_to_xvalue(
    const xllm_session_state *pState,
    xvalue *ptValue
)
{
    xvalue tTable;
    xvalue tHistory = NULL;

    if ( !pState || !ptValue ) {
        return XRT_NET_ERROR;
    }

    *ptValue = NULL;
    tTable = xvoCreateTable();
    if ( !tTable ) {
        return XRT_NET_ERROR;
    }

    if ( !xllm__session_state_table_set_text(tTable, "type", "xllm_session_state") ||
         !xvoTableSetInt(tTable, (str)"version", 0u, 2) ||
         !xllm__session_state_table_set_text(tTable, "profile_id", pState->sProfileId) ||
         !xllm__session_state_table_set_text(tTable, "system_prompt", pState->sSystemPrompt) ||
         !xllm__session_state_table_set_text(tTable, "session_summary", pState->sSessionSummary) ||
         !xllm__session_state_table_set_text(tTable, "summarizer_profile_id", pState->sSummarizerProfileId) ||
         !xvoTableSetInt(tTable, (str)"committed_turns", 0u, (int64)pState->uCommittedTurns) ||
         !xvoTableSetInt(tTable, (str)"summary_turns", 0u, (int64)pState->uSummaryTurns) ||
         !xvoTableSetBool(tTable, (str)"enable_auto_compact", 0u, pState->bEnableAutoCompact) ||
         !xvoTableSetFloat(tTable, (str)"compact_trigger_ratio", 0u, pState->fCompactTriggerRatio) ||
         !xvoTableSetInt(tTable, (str)"compact_trigger_turns", 0u, (int64)pState->uCompactTriggerTurns) ||
         !xvoTableSetInt(tTable, (str)"reserve_output_tokens", 0u, (int64)pState->uReserveOutputTokens) ||
         !xvoTableSetInt(tTable, (str)"keep_recent_turns", 0u, (int64)pState->uKeepRecentTurns) ||
         !xvoTableSetBool(tTable, (str)"keep_active_tool_chain", 0u, pState->bKeepActiveToolChain) ||
         !xvoTableSetInt(tTable, (str)"compact_strategy", 0u, (int64)pState->eCompactStrategy) ||
         !xllm__session_state_table_set_value(tTable, "vendor_extra", pState->tVendorExtra) ) {
        xvoUnref(tTable);
        return XRT_NET_ERROR;
    }

    if ( xllm__session_state_message_array_to_xvalue(pState->pHistory, pState->iHistoryCount, &tHistory) != XRT_NET_OK ) {
        xvoUnref(tTable);
        return XRT_NET_ERROR;
    }
    if ( tHistory && !xvoTableSetValue(tTable, (str)"history", 0u, tHistory, TRUE) ) {
        xvoUnref(tHistory);
        xvoUnref(tTable);
        return XRT_NET_ERROR;
    }

    *ptValue = tTable;
    return XRT_NET_OK;
}

XLLM_API int xllm_session_state_from_xvalue(
    xvalue tValue,
    xllm_session_state **ppState
)
{
    xllm_session_state *pState;
    int64 iVersion;
    xvalue tExtra;

    if ( !tValue || !ppState || xvoType(tValue) != XVO_DT_TABLE ) {
        return XRT_NET_ERROR;
    }

    *ppState = NULL;
    pState = (xllm_session_state *)xrtCalloc(1, sizeof(*pState));
    if ( !pState ) {
        return XRT_NET_ERROR;
    }
    xllm__session_state_init_defaults(pState);

    pState->sProfileId = xllm__dup_cstr((const char *)xvoTableGetText(tValue, (str)"profile_id", 0u));
    pState->sSystemPrompt = xllm__dup_cstr((const char *)xvoTableGetText(tValue, (str)"system_prompt", 0u));
    pState->sSessionSummary = xllm__dup_cstr((const char *)xvoTableGetText(tValue, (str)"session_summary", 0u));
    pState->uCommittedTurns = (uint32)xvoTableGetInt(tValue, (str)"committed_turns", 0u);
    pState->uSummaryTurns = (uint32)xvoTableGetInt(tValue, (str)"summary_turns", 0u);
    iVersion = xvoTableGetInt(tValue, (str)"version", 0u);
    if ( iVersion >= 2 ) {
        pState->sSummarizerProfileId = xllm__dup_cstr((const char *)xvoTableGetText(tValue, (str)"summarizer_profile_id", 0u));
        pState->bEnableAutoCompact = xvoTableGetBool(tValue, (str)"enable_auto_compact", 0u);
        pState->fCompactTriggerRatio = xvoTableGetFloat(tValue, (str)"compact_trigger_ratio", 0u);
        pState->uCompactTriggerTurns = (uint32)xvoTableGetInt(tValue, (str)"compact_trigger_turns", 0u);
        pState->uReserveOutputTokens = (uint32)xvoTableGetInt(tValue, (str)"reserve_output_tokens", 0u);
        pState->uKeepRecentTurns = (uint32)xvoTableGetInt(tValue, (str)"keep_recent_turns", 0u);
        pState->bKeepActiveToolChain = xvoTableGetBool(tValue, (str)"keep_active_tool_chain", 0u);
        pState->eCompactStrategy = (xllm_compact_strategy)xvoTableGetInt(tValue, (str)"compact_strategy", 0u);
    }

    tExtra = xvoTableGetValue(tValue, (str)"vendor_extra", 0u);
    if ( tExtra && xvoType(tExtra) != XVO_DT_NULL ) {
        pState->tVendorExtra = tExtra;
        xllm__xvalue_addref(pState->tVendorExtra);
    }

    if ( xllm__session_state_message_array_from_xvalue(
            xvoTableGetValue(tValue, (str)"history", 0u),
            &pState->pHistory,
            &pState->iHistoryCount
         ) != XRT_NET_OK ) {
        xllm_session_state_free(pState);
        return XRT_NET_ERROR;
    }

    *ppState = pState;
    return XRT_NET_OK;
}

XLLM_API void xllm_session_state_free(xllm_session_state *pState)
{
    if ( !pState ) {
        return;
    }

    xllm__free_cstr(&pState->sProfileId);
    xllm__free_cstr(&pState->sSystemPrompt);
    xllm__free_cstr(&pState->sSessionSummary);
    xllm__free_cstr(&pState->sSummarizerProfileId);
    xllm__message_array_free(pState->pHistory, pState->iHistoryCount);
    xllm__xvalue_release(&pState->tVendorExtra);
    xrtFree(pState);
}
