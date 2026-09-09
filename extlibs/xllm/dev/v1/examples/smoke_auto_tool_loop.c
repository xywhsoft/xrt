#include "xllm-session.h"

#include <stdio.h>
#include <string.h>

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
    if ( !pResponse->pOutputs[0].as.tMessage.pParts[0].as.tSource.sMimeType ||
         !pResponse->pOutputs[0].as.tMessage.pParts[0].as.tSource.as.sText ) {
        xllm_response_free(pResponse);
        return XRT_NET_ERROR;
    }

    *ppResponse = pResponse;
    return XRT_NET_OK;
}

static int demo_make_tool_response(
    const xllm_profile *pProfile,
    xllm_response **ppResponse
)
{
    xllm_response *pResponse;

    if ( !pProfile || !ppResponse ) {
        return XRT_NET_ERROR;
    }

    pResponse = (xllm_response *)xrtCalloc(1u, sizeof(*pResponse));
    if ( !pResponse ) {
        return XRT_NET_ERROR;
    }

    pResponse->sId = demo_dupstr("mock-tool");
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
    if ( !pResponse->pOutputs[0].as.tToolCall.sCallId ||
         !pResponse->pOutputs[0].as.tToolCall.sToolId ||
         !pResponse->pOutputs[0].as.tToolCall.sToolName ||
         !pResponse->pOutputs[0].as.tToolCall.sArgumentsJson ) {
        xllm_response_free(pResponse);
        return XRT_NET_ERROR;
    }

    *ppResponse = pResponse;
    return XRT_NET_OK;
}

static const char *demo_find_last_tool_text(const xllm_request *pRequest)
{
    size_t i;

    if ( !pRequest ) {
        return NULL;
    }

    for ( i = pRequest->iMessageCount; i > 0u; --i ) {
        const xllm_message *pMessage = &pRequest->pMessages[i - 1u];
        size_t j;

        if ( pMessage->eRole != XLLM_ROLE_TOOL ) {
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

static bool demo_emit_tool_event(const xllm_call_options *pOptions)
{
    xllm_event tEvent;

    if ( !pOptions || !pOptions->pfnOnEvent ) {
        return true;
    }

    memset(&tEvent, 0, sizeof(tEvent));
    tEvent.eType = XLLM_EVENT_START;
    tEvent.as.tStart.sResponseId = "mock-tool";
    tEvent.as.tStart.sModel = "mock-text";
    if ( !pOptions->pfnOnEvent(&tEvent, pOptions->pUserData) ) {
        return false;
    }

    memset(&tEvent, 0, sizeof(tEvent));
    tEvent.eType = XLLM_EVENT_OUTPUT_BEGIN;
    tEvent.as.tOutputBegin.eKind = XLLM_OUTPUT_TOOL_CALL;
    if ( !pOptions->pfnOnEvent(&tEvent, pOptions->pUserData) ) {
        return false;
    }

    memset(&tEvent, 0, sizeof(tEvent));
    tEvent.eType = XLLM_EVENT_TOOL_CALL_READY;
    tEvent.as.tToolCallReady.tToolCall.sCallId = "call-weather-1";
    tEvent.as.tToolCallReady.tToolCall.sToolId = "app.weather.get_current";
    tEvent.as.tToolCallReady.tToolCall.sToolName = "get_weather";
    tEvent.as.tToolCallReady.tToolCall.sArgumentsJson = "{\"city\":\"Shanghai\"}";
    if ( !pOptions->pfnOnEvent(&tEvent, pOptions->pUserData) ) {
        return false;
    }

    memset(&tEvent, 0, sizeof(tEvent));
    tEvent.eType = XLLM_EVENT_OUTPUT_END;
    if ( !pOptions->pfnOnEvent(&tEvent, pOptions->pUserData) ) {
        return false;
    }

    memset(&tEvent, 0, sizeof(tEvent));
    tEvent.eType = XLLM_EVENT_END;
    return pOptions->pfnOnEvent(&tEvent, pOptions->pUserData);
}

static bool demo_emit_text_event(const xllm_call_options *pOptions, const char *sText)
{
    xllm_event tEvent;

    if ( !pOptions || !pOptions->pfnOnEvent ) {
        return true;
    }

    memset(&tEvent, 0, sizeof(tEvent));
    tEvent.eType = XLLM_EVENT_START;
    tEvent.as.tStart.sResponseId = "mock-final";
    tEvent.as.tStart.sModel = "mock-text";
    if ( !pOptions->pfnOnEvent(&tEvent, pOptions->pUserData) ) {
        return false;
    }

    memset(&tEvent, 0, sizeof(tEvent));
    tEvent.eType = XLLM_EVENT_OUTPUT_BEGIN;
    tEvent.as.tOutputBegin.eKind = XLLM_OUTPUT_MESSAGE;
    if ( !pOptions->pfnOnEvent(&tEvent, pOptions->pUserData) ) {
        return false;
    }

    memset(&tEvent, 0, sizeof(tEvent));
    tEvent.eType = XLLM_EVENT_TEXT_DELTA;
    tEvent.as.tTextDelta.sText = sText;
    if ( !pOptions->pfnOnEvent(&tEvent, pOptions->pUserData) ) {
        return false;
    }

    memset(&tEvent, 0, sizeof(tEvent));
    tEvent.eType = XLLM_EVENT_OUTPUT_END;
    if ( !pOptions->pfnOnEvent(&tEvent, pOptions->pUserData) ) {
        return false;
    }

    memset(&tEvent, 0, sizeof(tEvent));
    tEvent.eType = XLLM_EVENT_END;
    return pOptions->pfnOnEvent(&tEvent, pOptions->pUserData);
}

static int32 demo_tool_loop_chat(
    void *pCtx,
    const xllm_profile *pProfile,
    const xllm_request *pRequest,
    const xllm_call_options *pOptions,
    xllm_response **ppResponse,
    xllm_error *pError
)
{
    const char *sToolText;
    char sText[128];

    (void)pCtx;
    (void)pError;

    if ( !pProfile || !pRequest || !ppResponse ) {
        return XRT_NET_ERROR;
    }

    sToolText = demo_find_last_tool_text(pRequest);
    if ( sToolText ) {
        if ( snprintf(sText, sizeof(sText), "weather result: %s", sToolText) <= 0 ) {
            return XRT_NET_ERROR;
        }
        if ( !demo_emit_text_event(pOptions, sText) ) {
            return XRT_NET_CANCELLED;
        }
        return demo_make_text_response(pProfile, "mock-final", sText, ppResponse);
    }

    if ( !demo_emit_tool_event(pOptions) ) {
        return XRT_NET_CANCELLED;
    }
    return demo_make_tool_response(pProfile, ppResponse);
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

typedef struct {
    int iToolReadyCount;
    int iTextDeltaCount;
} demo_event_stats;

typedef struct {
    int iRoundResponseCount;
    int iToolResultCount;
    int iStopFinalCount;
} demo_trace_stats;

typedef struct {
    int iToolLoopLogCount;
} demo_log_stats;

static bool demo_on_event(const xllm_event *pEvent, void *pUserData)
{
    demo_event_stats *pStats = (demo_event_stats *)pUserData;

    if ( !pEvent ) {
        return true;
    }

    if ( pEvent->eType == XLLM_EVENT_TOOL_CALL_READY ) {
        if ( pStats ) {
            ++pStats->iToolReadyCount;
        }
    } else if ( pEvent->eType == XLLM_EVENT_TEXT_DELTA ) {
        if ( pStats ) {
            ++pStats->iTextDeltaCount;
        }
        printf("stream: %s\n", pEvent->as.tTextDelta.sText ? pEvent->as.tTextDelta.sText : "");
    }

    return true;
}

static void demo_trace_callback(void *pCtx, xllm_trace_kind eKind, const xvalue *pPayload)
{
    demo_trace_stats *pStats = (demo_trace_stats *)pCtx;
    const char *sPhase;
    const char *sStatus;
    const char *sReason;

    if ( !pStats || eKind != XLLM_TRACE_TOOL_LOOP || !pPayload || !*pPayload ) {
        return;
    }

    sPhase = (const char *)xvoTableGetText(*pPayload, (str)"phase", 0u);
    if ( sPhase && strcmp(sPhase, "round_response") == 0 ) {
        sStatus = (const char *)xvoTableGetText(*pPayload, (str)"response_status", 0u);
        if ( sStatus && strcmp(sStatus, "tool_call_required") == 0 ) {
            ++pStats->iRoundResponseCount;
        }
    } else if ( sPhase && strcmp(sPhase, "tool_result") == 0 ) {
        if ( (int)xvoTableGetInt(*pPayload, (str)"exec_status", 0u) == XRT_NET_OK ) {
            ++pStats->iToolResultCount;
        }
    } else if ( sPhase && strcmp(sPhase, "stop") == 0 ) {
        sReason = (const char *)xvoTableGetText(*pPayload, (str)"reason", 0u);
        if ( sReason && strcmp(sReason, "response_final") == 0 ) {
            ++pStats->iStopFinalCount;
        }
    }
}

static void demo_log_callback(void *pCtx, xllm_log_level eLevel, const char *sComponent, const char *sMessage)
{
    demo_log_stats *pStats = (demo_log_stats *)pCtx;

    (void)eLevel;
    (void)sMessage;

    if ( !pStats || !sComponent ) {
        return;
    }

    if ( strcmp(sComponent, "xllm.tool_loop") == 0 ) {
        ++pStats->iToolLoopLogCount;
    }
}

int main(void)
{
    xllm_runtime *pRuntime = NULL;
    xllm *pPlain = NULL;
    xllm *pAuto = NULL;
    xllm_response *pResponse = NULL;
    xfuture *pFuture = NULL;
    xllm_adapter tAdapter;
    xllm_profile tProfile;
    xllm_create_options tCreate;
    xllm_turn tTurn;
    xllm_tool_def tTool;
    xllm_tool_executor tExecutor;
    xllm_call_options tCallOptions;
    demo_event_stats tStats;
    demo_trace_stats tTraceStats;
    demo_log_stats tLogStats;
    const char *sText;
    int iStatus;

    memset(&tAdapter, 0, sizeof(tAdapter));
    memset(&tProfile, 0, sizeof(tProfile));
    memset(&tCreate, 0, sizeof(tCreate));
    memset(&tTool, 0, sizeof(tTool));
    memset(&tExecutor, 0, sizeof(tExecutor));
    memset(&tStats, 0, sizeof(tStats));
    memset(&tTraceStats, 0, sizeof(tTraceStats));
    memset(&tLogStats, 0, sizeof(tLogStats));

    if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || !pRuntime ) {
        fprintf(stderr, "create runtime failed\n");
        return 1;
    }
    if ( xllm_runtime_set_trace_callback(pRuntime, demo_trace_callback, &tTraceStats) != XRT_NET_OK ) {
        fprintf(stderr, "set trace callback failed\n");
        return 2;
    }
    if ( xllm_runtime_set_log_callback(pRuntime, demo_log_callback, &tLogStats) != XRT_NET_OK ) {
        fprintf(stderr, "set log callback failed\n");
        return 3;
    }

    tAdapter.sName = "mock_tool_loop";
    tAdapter.pfnChat = demo_tool_loop_chat;
    if ( xllm_register_adapter(pRuntime, &tAdapter) != XRT_NET_OK ) {
        fprintf(stderr, "register adapter failed\n");
        return 4;
    }

    xllm_profile_init(&tProfile);
    tProfile.sId = "mock-tool";
    tProfile.sProvider = "mock";
    tProfile.sAdapter = "mock_tool_loop";
    tProfile.tModels.tText.sModelId = "mock-text";
    tProfile.tModels.tText.tCaps.uFlags =
        XLLM_CAP_TEXT_IN |
        XLLM_CAP_TEXT_OUT |
        XLLM_CAP_TOOL_CALL_OUT |
        XLLM_CAP_TOOL_RESULT_IN;
    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "register profile failed\n");
        return 5;
    }

    tCreate.sInitialProfileId = "mock-tool";
    pPlain = xllm_create(pRuntime, &tCreate);
    pAuto = xllm_create(pRuntime, &tCreate);
    if ( !pPlain || !pAuto ) {
        fprintf(stderr, "create llm failed\n");
        return 6;
    }

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "what is the weather?") != XRT_NET_OK ) {
        fprintf(stderr, "add user text failed\n");
        return 7;
    }

    tTool.sToolId = "app.weather.get_current";
    tTool.sWireName = "get_weather";
    tTool.sDescription = "Get current weather";
    if ( xllm_turn_add_tool(&tTurn, &tTool) != XRT_NET_OK ) {
        fprintf(stderr, "add tool failed\n");
        return 8;
    }

    iStatus = xllm_send(pPlain, &tTurn, NULL, &pResponse);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "plain send failed: %d\n", iStatus);
        return 9;
    }
    if ( pResponse->eStatus != XLLM_STATUS_TOOL_CALL_REQUIRED ||
         xllm_response_get_tool_call_count(pResponse) != 1u ) {
        fprintf(stderr, "plain send did not return tool_call_required\n");
        return 10;
    }
    xllm_response_free(pResponse);
    pResponse = NULL;

    tExecutor.pfnExecute = demo_tool_execute;
    if ( xllm_set_tool_executor(pAuto, &tExecutor) != XRT_NET_OK ) {
        fprintf(stderr, "set tool executor failed\n");
        return 11;
    }

    xllm_call_options_init(&tCallOptions);
    tCallOptions.pfnOnEvent = demo_on_event;
    tCallOptions.pUserData = &tStats;

    iStatus = xllm_send(pAuto, &tTurn, &tCallOptions, &pResponse);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "auto send failed: %d\n", iStatus);
        return 12;
    }

    sText = xllm_response_get_text(pResponse);
    if ( pResponse->eStatus != XLLM_STATUS_COMPLETED ||
         !sText ||
         strcmp(sText, "weather result: sunny") != 0 ) {
        fprintf(stderr, "unexpected auto response text: %s\n", sText ? sText : "(null)");
        return 13;
    }

    if ( tStats.iToolReadyCount != 1 || tStats.iTextDeltaCount != 1 ) {
        fprintf(stderr, "unexpected event stats: tool=%d text=%d\n", tStats.iToolReadyCount, tStats.iTextDeltaCount);
        return 14;
    }
    if ( tTraceStats.iRoundResponseCount < 1 ||
         tTraceStats.iToolResultCount < 1 ||
         tTraceStats.iStopFinalCount < 1 ) {
        fprintf(
            stderr,
            "unexpected trace stats: round=%d tool=%d stop=%d\n",
            tTraceStats.iRoundResponseCount,
            tTraceStats.iToolResultCount,
            tTraceStats.iStopFinalCount
        );
        return 15;
    }
    if ( tLogStats.iToolLoopLogCount < 3 ) {
        fprintf(stderr, "unexpected tool loop log count: %d\n", tLogStats.iToolLoopLogCount);
        return 16;
    }

    xllm_response_free(pResponse);
    pResponse = NULL;

    pFuture = xllm_send_async_thread(pAuto, &tTurn, NULL);
    if ( !pFuture ) {
        fprintf(stderr, "async send future create failed\n");
        return 17;
    }
    pResponse = (xllm_response *)xFutureWaitValue(pFuture);
    if ( xFutureStatus(pFuture) != XRT_NET_OK || !pResponse ) {
        fprintf(stderr, "async send failed: %d\n", (int)xFutureStatus(pFuture));
        return 18;
    }

    sText = xllm_response_get_text(pResponse);
    if ( !sText || strcmp(sText, "weather result: sunny") != 0 ) {
        fprintf(stderr, "unexpected async response text: %s\n", sText ? sText : "(null)");
        return 19;
    }
    if ( tTraceStats.iRoundResponseCount < 2 ||
         tTraceStats.iToolResultCount < 2 ||
         tTraceStats.iStopFinalCount < 2 ) {
        fprintf(
            stderr,
            "unexpected async trace stats: round=%d tool=%d stop=%d\n",
            tTraceStats.iRoundResponseCount,
            tTraceStats.iToolResultCount,
            tTraceStats.iStopFinalCount
        );
        return 20;
    }
    if ( tLogStats.iToolLoopLogCount < 6 ) {
        fprintf(stderr, "unexpected async tool loop log count: %d\n", tLogStats.iToolLoopLogCount);
        return 21;
    }

    printf("ok: %s\n", sText);

    xFutureRelease(pFuture);
    xllm_response_free(pResponse);
    xllm_turn_reset(&tTurn);
    xllm_destroy(pPlain);
    xllm_destroy(pAuto);
    xllm_runtime_destroy(pRuntime);
    return 0;
}
