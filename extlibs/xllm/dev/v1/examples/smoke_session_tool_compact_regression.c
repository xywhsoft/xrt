#include "xllm-session.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    int iToolRoundCount;
    int iToolResultCount;
    int iToolStopCount;
    int iCompactTriggeredCount;
    int iCompactResultCount;
    int iCompactSummarizedCount;
} demo_trace_stats;

static char *demo_dupstr(const char *sText)
{
    size_t iLen;
    char *sCopy;

    if ( !sText ) {
        return NULL;
    }

    iLen = strlen(sText);
    sCopy = (char *)xrtCalloc(iLen + 1u, sizeof(char));
    if ( !sCopy ) {
        return NULL;
    }
    memcpy(sCopy, sText, iLen);
    sCopy[iLen] = '\0';
    return sCopy;
}

static bool demo_request_contains_text(const xllm_request *pRequest, const char *sNeedle)
{
    size_t i;

    if ( !pRequest || !sNeedle ) {
        return false;
    }
    for ( i = 0; i < pRequest->iMessageCount; ++i ) {
        const xllm_message *pMessage = &pRequest->pMessages[i];
        size_t j;
        for ( j = 0; j < pMessage->iPartCount; ++j ) {
            const xllm_content_part *pPart = &pMessage->pParts[j];
            const char *sText;
            if ( pPart->eKind != XLLM_PART_TEXT ||
                 pPart->as.tSource.eKind != XLLM_SOURCE_INLINE_TEXT ) {
                continue;
            }
            sText = pPart->as.tSource.as.sText;
            if ( sText && strstr(sText, sNeedle) ) {
                return true;
            }
        }
    }
    return false;
}

static const char *demo_find_last_tool_text(const xllm_request *pRequest)
{
    const xllm_message *pMessage;
    size_t j;

    if ( !pRequest || pRequest->iMessageCount == 0u ) {
        return NULL;
    }
    pMessage = &pRequest->pMessages[pRequest->iMessageCount - 1u];
    if ( pMessage->eRole != XLLM_ROLE_TOOL ) {
        return NULL;
    }
    for ( j = 0; j < pMessage->iPartCount; ++j ) {
        const xllm_content_part *pPart = &pMessage->pParts[j];
        if ( pPart->eKind == XLLM_PART_TEXT &&
             pPart->as.tSource.eKind == XLLM_SOURCE_INLINE_TEXT &&
             pPart->as.tSource.as.sText ) {
            return pPart->as.tSource.as.sText;
        }
    }
    return NULL;
}

static const char *demo_find_last_user_text(const xllm_request *pRequest)
{
    size_t i;

    if ( !pRequest ) {
        return NULL;
    }
    for ( i = pRequest->iMessageCount; i > 0u; --i ) {
        const xllm_message *pMessage = &pRequest->pMessages[i - 1u];
        size_t j;
        if ( pMessage->eRole != XLLM_ROLE_USER ) {
            continue;
        }
        for ( j = 0; j < pMessage->iPartCount; ++j ) {
            const xllm_content_part *pPart = &pMessage->pParts[j];
            if ( pPart->eKind == XLLM_PART_TEXT &&
                 pPart->as.tSource.eKind == XLLM_SOURCE_INLINE_TEXT &&
                 pPart->as.tSource.as.sText ) {
                return pPart->as.tSource.as.sText;
            }
        }
    }
    return NULL;
}

static int demo_make_text_response(
    const xllm_profile *pProfile,
    const char *sId,
    const char *sText,
    xllm_response **ppResponse
)
{
    xllm_response *pResponse;

    if ( !pProfile || !sId || !sText || !ppResponse ) {
        return XRT_NET_ERROR;
    }

    pResponse = (xllm_response *)xrtCalloc(1u, sizeof(*pResponse));
    if ( !pResponse ) {
        return XRT_NET_ERROR;
    }
    pResponse->sId = demo_dupstr(sId);
    pResponse->sProvider = demo_dupstr(pProfile->sProvider ? pProfile->sProvider : "mock");
    pResponse->sProfileId = demo_dupstr(pProfile->sId);
    pResponse->sModel = demo_dupstr(pProfile->tModels.tText.sModelId ? pProfile->tModels.tText.sModelId : "mock-text");
    pResponse->eStatus = XLLM_STATUS_COMPLETED;
    pResponse->sFinishReason = demo_dupstr("stop");
    pResponse->sVisibleText = demo_dupstr(sText);
    pResponse->iOutputCount = 1u;
    pResponse->pOutputs = (xllm_output_item *)xrtCalloc(1u, sizeof(xllm_output_item));
    if ( !pResponse->pOutputs ) {
        xllm_response_free(pResponse);
        return XRT_NET_ERROR;
    }
    pResponse->pOutputs[0].eKind = XLLM_OUTPUT_MESSAGE;
    pResponse->pOutputs[0].as.tMessage.iPartCount = 1u;
    pResponse->pOutputs[0].as.tMessage.pParts = (xllm_content_part *)xrtCalloc(1u, sizeof(xllm_content_part));
    if ( !pResponse->pOutputs[0].as.tMessage.pParts ) {
        xllm_response_free(pResponse);
        return XRT_NET_ERROR;
    }
    pResponse->pOutputs[0].as.tMessage.pParts[0].eKind = XLLM_PART_TEXT;
    pResponse->pOutputs[0].as.tMessage.pParts[0].as.tSource.eKind = XLLM_SOURCE_INLINE_TEXT;
    pResponse->pOutputs[0].as.tMessage.pParts[0].as.tSource.sMimeType = demo_dupstr("text/plain");
    pResponse->pOutputs[0].as.tMessage.pParts[0].as.tSource.as.sText = demo_dupstr(sText);
    if ( !pResponse->sId || !pResponse->sProvider || !pResponse->sProfileId ||
         !pResponse->sModel || !pResponse->sFinishReason || !pResponse->sVisibleText ||
         !pResponse->pOutputs[0].as.tMessage.pParts[0].as.tSource.sMimeType ||
         !pResponse->pOutputs[0].as.tMessage.pParts[0].as.tSource.as.sText ) {
        xllm_response_free(pResponse);
        return XRT_NET_ERROR;
    }

    *ppResponse = pResponse;
    return XRT_NET_OK;
}

static int demo_make_tool_response(const xllm_profile *pProfile, xllm_response **ppResponse)
{
    xllm_response *pResponse;

    if ( !pProfile || !ppResponse ) {
        return XRT_NET_ERROR;
    }
    pResponse = (xllm_response *)xrtCalloc(1u, sizeof(*pResponse));
    if ( !pResponse ) {
        return XRT_NET_ERROR;
    }
    pResponse->sId = demo_dupstr("mock-session-tool");
    pResponse->sProvider = demo_dupstr(pProfile->sProvider ? pProfile->sProvider : "mock");
    pResponse->sProfileId = demo_dupstr(pProfile->sId);
    pResponse->sModel = demo_dupstr(pProfile->tModels.tText.sModelId ? pProfile->tModels.tText.sModelId : "mock-text");
    pResponse->eStatus = XLLM_STATUS_TOOL_CALL_REQUIRED;
    pResponse->sFinishReason = demo_dupstr("tool_calls");
    pResponse->iOutputCount = 1u;
    pResponse->pOutputs = (xllm_output_item *)xrtCalloc(1u, sizeof(xllm_output_item));
    if ( !pResponse->pOutputs ) {
        xllm_response_free(pResponse);
        return XRT_NET_ERROR;
    }
    pResponse->pOutputs[0].eKind = XLLM_OUTPUT_TOOL_CALL;
    pResponse->pOutputs[0].as.tToolCall.sCallId = demo_dupstr("call-weather-1");
    pResponse->pOutputs[0].as.tToolCall.sToolId = demo_dupstr("app.weather.get_current");
    pResponse->pOutputs[0].as.tToolCall.sToolName = demo_dupstr("get_weather");
    pResponse->pOutputs[0].as.tToolCall.sArgumentsJson = demo_dupstr("{\"city\":\"Shanghai\"}");
    if ( !pResponse->sId || !pResponse->sProvider || !pResponse->sProfileId ||
         !pResponse->sModel || !pResponse->sFinishReason ||
         !pResponse->pOutputs[0].as.tToolCall.sCallId ||
         !pResponse->pOutputs[0].as.tToolCall.sToolId ||
         !pResponse->pOutputs[0].as.tToolCall.sToolName ||
         !pResponse->pOutputs[0].as.tToolCall.sArgumentsJson ) {
        xllm_response_free(pResponse);
        return XRT_NET_ERROR;
    }

    *ppResponse = pResponse;
    return XRT_NET_OK;
}

static int32 demo_chat(
    void *pCtx,
    const xllm_profile *pProfile,
    const xllm_request *pRequest,
    const xllm_call_options *pOptions,
    xllm_response **ppResponse,
    xllm_error *pError
)
{
    const char *sToolText;
    const char *sLastUserText;
    char sText[160];

    (void)pCtx;
    (void)pOptions;
    (void)pError;

    if ( !pProfile || !pRequest || !ppResponse ) {
        return XRT_NET_ERROR;
    }
    if ( demo_request_contains_text(pRequest, "Update the rolling session summary.") ) {
        return demo_make_text_response(
            pProfile,
            "mock-session-summary",
            "Facts:\n- weather tool returned sunny\nOpen items:\n- compact after continuation\nTool state:\n- chain completed\nRecent context:\n- follow-up committed",
            ppResponse
        );
    }

    sToolText = demo_find_last_tool_text(pRequest);
    if ( sToolText ) {
        if ( snprintf(sText, sizeof(sText), "weather result: %s", sToolText) <= 0 ) {
            return XRT_NET_ERROR;
        }
        return demo_make_text_response(pProfile, "mock-session-final", sText, ppResponse);
    }
    sLastUserText = demo_find_last_user_text(pRequest);
    if ( sLastUserText && strstr(sLastUserText, "weather") ) {
        return demo_make_tool_response(pProfile, ppResponse);
    }
    return demo_make_text_response(pProfile, "mock-session-followup", "follow-up committed", ppResponse);
}

static int32 demo_tool_execute(
    void *pCtx,
    const xllm_tool_exec_request *pRequest,
    xllm_tool_exec_result *pResult,
    xllm_error *pError
)
{
    (void)pCtx;
    (void)pError;

    if ( !pRequest || !pResult || !pRequest->sToolId ||
         strcmp(pRequest->sToolId, "app.weather.get_current") != 0 ) {
        return XRT_NET_ERROR;
    }
    pResult->pParts = (xllm_content_part *)xrtCalloc(1u, sizeof(xllm_content_part));
    if ( !pResult->pParts ) {
        return XRT_NET_ERROR;
    }
    pResult->iPartCount = 1u;
    pResult->pParts[0].eKind = XLLM_PART_TEXT;
    pResult->pParts[0].as.tSource.eKind = XLLM_SOURCE_INLINE_TEXT;
    pResult->pParts[0].as.tSource.sMimeType = demo_dupstr("text/plain");
    pResult->pParts[0].as.tSource.as.sText = demo_dupstr("sunny");
    if ( !pResult->pParts[0].as.tSource.sMimeType ||
         !pResult->pParts[0].as.tSource.as.sText ) {
        xllm_tool_exec_result_free(pResult);
        return XRT_NET_ERROR;
    }
    return XRT_NET_OK;
}

static void demo_trace_callback(void *pCtx, xllm_trace_kind eKind, const xvalue *pPayload)
{
    demo_trace_stats *pStats = (demo_trace_stats *)pCtx;
    const char *sPhase;
    const char *sReason;

    if ( !pStats || !pPayload || !*pPayload ) {
        return;
    }
    sPhase = (const char *)xvoTableGetText(*pPayload, (str)"phase", 0u);
    if ( eKind == XLLM_TRACE_TOOL_LOOP ) {
        if ( sPhase && strcmp(sPhase, "round_response") == 0 ) {
            ++pStats->iToolRoundCount;
        } else if ( sPhase && strcmp(sPhase, "tool_result") == 0 ) {
            ++pStats->iToolResultCount;
        } else if ( sPhase && strcmp(sPhase, "stop") == 0 ) {
            sReason = (const char *)xvoTableGetText(*pPayload, (str)"reason", 0u);
            if ( sReason && strcmp(sReason, "response_final") == 0 ) {
                ++pStats->iToolStopCount;
            }
        }
    } else if ( eKind == XLLM_TRACE_COMPACT ) {
        if ( sPhase && strcmp(sPhase, "auto_check") == 0 &&
             xvoTableGetBool(*pPayload, (str)"triggered", 0u) ) {
            ++pStats->iCompactTriggeredCount;
        } else if ( sPhase && strcmp(sPhase, "compact_result") == 0 ) {
            if ( xvoTableGetBool(*pPayload, (str)"compacted", 0u) ) {
                ++pStats->iCompactResultCount;
            }
            if ( xvoTableGetBool(*pPayload, (str)"summarized", 0u) ) {
                ++pStats->iCompactSummarizedCount;
            }
        }
    }
}

static int demo_add_weather_turn(xllm_turn *pTurn)
{
    xllm_tool_def tTool;

    memset(&tTool, 0, sizeof(tTool));
    xllm_turn_init(pTurn);
    if ( xllm_turn_add_user_text(pTurn, "what is the weather?") != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }
    tTool.sToolId = "app.weather.get_current";
    tTool.sWireName = "get_weather";
    tTool.sDescription = "Get current weather";
    return xllm_turn_add_tool(pTurn, &tTool);
}

int main(void)
{
    xllm_runtime *pRuntime = NULL;
    xllm_session *pSession = NULL;
    xllm_response *pResponse = NULL;
    xllm_adapter tAdapter;
    xllm_profile tProfile;
    xllm_session_options tSessionOptions;
    xllm_tool_executor tExecutor;
    xllm_turn tTurn;
    demo_trace_stats tTraceStats;
    const char *sText;

    memset(&tAdapter, 0, sizeof(tAdapter));
    memset(&tProfile, 0, sizeof(tProfile));
    memset(&tExecutor, 0, sizeof(tExecutor));
    memset(&tTraceStats, 0, sizeof(tTraceStats));

    if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || !pRuntime ) {
        return 1;
    }
    if ( xllm_runtime_set_trace_callback(pRuntime, demo_trace_callback, &tTraceStats) != XRT_NET_OK ) {
        return 2;
    }

    tAdapter.sName = "mock_session_tool_compact";
    tAdapter.pfnChat = demo_chat;
    if ( xllm_register_adapter(pRuntime, &tAdapter) != XRT_NET_OK ) {
        return 3;
    }

    xllm_profile_init(&tProfile);
    tProfile.sId = "mock-session-tool-compact";
    tProfile.sProvider = "mock";
    tProfile.sAdapter = "mock_session_tool_compact";
    tProfile.tModels.tText.sModelId = "mock-text";
    tProfile.tModels.tText.tCaps.uMaxInputTokens = 1u;
    tProfile.tModels.tText.tCaps.uFlags =
        XLLM_CAP_TEXT_IN |
        XLLM_CAP_TEXT_OUT |
        XLLM_CAP_TOOL_CALL_OUT |
        XLLM_CAP_TOOL_RESULT_IN;
    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        return 4;
    }

    xllm_session_options_init(&tSessionOptions);
    tSessionOptions.sProfileId = "mock-session-tool-compact";
    tSessionOptions.sSystemPrompt = "session tool compact regression";
    tSessionOptions.bEnableAutoCompact = true;
    tSessionOptions.fCompactTriggerRatio = 0.1;
    tSessionOptions.uCompactTriggerTurns = 2u;
    tSessionOptions.uKeepRecentTurns = 1u;
    tSessionOptions.bKeepActiveToolChain = true;
    tSessionOptions.eCompactStrategy = XLLM_COMPACT_SUMMARIZE;
    if ( xllm_session_create(pRuntime, &tSessionOptions, &pSession) != XRT_NET_OK || !pSession ) {
        return 5;
    }

    tExecutor.pfnExecute = demo_tool_execute;
    if ( xllm_session_set_tool_executor(pSession, &tExecutor) != XRT_NET_OK ) {
        return 6;
    }

    if ( demo_add_weather_turn(&tTurn) != XRT_NET_OK ) {
        return 7;
    }
    if ( xllm_session_chat(pSession, &tTurn, NULL, &pResponse) != XRT_NET_OK || !pResponse ) {
        return 8;
    }
    sText = xllm_response_get_text(pResponse);
    if ( !sText || strcmp(sText, "weather result: sunny") != 0 ) {
        fprintf(stderr, "unexpected weather response: %s\n", sText ? sText : "(null)");
        return 9;
    }
    xllm_response_free(pResponse);
    pResponse = NULL;
    xllm_turn_reset(&tTurn);

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "commit follow-up after tool continuation") != XRT_NET_OK ) {
        return 10;
    }
    if ( xllm_session_chat(pSession, &tTurn, NULL, &pResponse) != XRT_NET_OK || !pResponse ) {
        return 11;
    }
    sText = xllm_response_get_text(pResponse);
    if ( !sText || strcmp(sText, "follow-up committed") != 0 ) {
        fprintf(stderr, "unexpected follow-up response: %s\n", sText ? sText : "(null)");
        return 12;
    }

    if ( tTraceStats.iToolRoundCount < 1 ||
         tTraceStats.iToolResultCount < 1 ||
         tTraceStats.iToolStopCount < 1 ||
         tTraceStats.iCompactTriggeredCount < 1 ||
         tTraceStats.iCompactResultCount < 1 ||
         tTraceStats.iCompactSummarizedCount < 1 ) {
        fprintf(
            stderr,
            "unexpected trace stats: round=%d tool=%d stop=%d compact_trigger=%d compact=%d summarized=%d\n",
            tTraceStats.iToolRoundCount,
            tTraceStats.iToolResultCount,
            tTraceStats.iToolStopCount,
            tTraceStats.iCompactTriggeredCount,
            tTraceStats.iCompactResultCount,
            tTraceStats.iCompactSummarizedCount
        );
        return 13;
    }

    xllm_response_free(pResponse);
    xllm_turn_reset(&tTurn);
    xllm_session_destroy(pSession);
    xllm_runtime_destroy(pRuntime);
    puts("smoke_session_tool_compact_regression ok");
    return 0;
}
