#include "xllm-session.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    char *sToolId;
} demo_async_tool_task;

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

    pResponse->sId = demo_dupstr("mock-tool-async");
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
    pResponse->pOutputs[0].as.tToolCall.sCallId = demo_dupstr("call-weather-async-1");
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
    (void)pOptions;
    (void)pError;

    if ( !pProfile || !pRequest || !ppResponse ) {
        return XRT_NET_ERROR;
    }

    sToolText = demo_find_last_tool_text(pRequest);
    if ( sToolText ) {
        if ( snprintf(sText, sizeof(sText), "weather result: %s", sToolText) <= 0 ) {
            return XRT_NET_ERROR;
        }
        return demo_make_text_response(pProfile, "mock-final-async", sText, ppResponse);
    }

    return demo_make_tool_response(pProfile, ppResponse);
}

static void demo_async_tool_task_destroy(demo_async_tool_task *pTask)
{
    if ( !pTask ) {
        return;
    }

    xllm__free_cstr(&pTask->sToolId);
    xrtFree(pTask);
}

static int32 demo_tool_execute_async_task(ptr pArg, xfuture_result *pOut)
{
    demo_async_tool_task *pTask = (demo_async_tool_task *)pArg;
    xllm_tool_exec_result *pResult = NULL;

    if ( !pTask || !pOut || !pTask->sToolId ||
         strcmp(pTask->sToolId, "app.weather.get_current") != 0 ) {
        demo_async_tool_task_destroy(pTask);
        return XRT_NET_ERROR;
    }

    pResult = (xllm_tool_exec_result *)xrtCalloc(1u, sizeof(*pResult));
    if ( !pResult ) {
        demo_async_tool_task_destroy(pTask);
        return XRT_NET_ERROR;
    }

    pResult->pParts = (xllm_content_part *)xrtCalloc(1u, sizeof(xllm_content_part));
    if ( !pResult->pParts ) {
        xrtFree(pResult);
        demo_async_tool_task_destroy(pTask);
        return XRT_NET_ERROR;
    }

    pResult->iPartCount = 1u;
    pResult->pParts[0].eKind = XLLM_PART_TEXT;
    pResult->pParts[0].as.tSource.eKind = XLLM_SOURCE_INLINE_TEXT;
    pResult->pParts[0].as.tSource.sMimeType = demo_dupstr("text/plain");
    pResult->pParts[0].as.tSource.as.sText = demo_dupstr("sunny-async");
    if ( !pResult->pParts[0].as.tSource.sMimeType ||
         !pResult->pParts[0].as.tSource.as.sText ) {
        xllm_tool_exec_result_free(pResult);
        xrtFree(pResult);
        demo_async_tool_task_destroy(pTask);
        return XRT_NET_ERROR;
    }

    memset(pOut, 0, sizeof(*pOut));
    pOut->iStatus = XRT_NET_OK;
    pOut->pValue = pResult;
    pOut->iFlags = XFUTURE_RESULT_F_OWN_VALUE;
    demo_async_tool_task_destroy(pTask);
    return XRT_NET_OK;
}

static xfuture *demo_tool_execute_async(void *pCtx, const xllm_tool_exec_request *pRequest)
{
    demo_async_tool_task *pTask;

    (void)pCtx;

    if ( !pRequest || !pRequest->sToolId ) {
        return NULL;
    }

    pTask = (demo_async_tool_task *)xrtCalloc(1u, sizeof(*pTask));
    if ( !pTask ) {
        return NULL;
    }

    pTask->sToolId = demo_dupstr(pRequest->sToolId);
    if ( !pTask->sToolId ) {
        demo_async_tool_task_destroy(pTask);
        return NULL;
    }

    return xTaskRunThread(demo_tool_execute_async_task, pTask, 0u);
}

int main(void)
{
    xllm_runtime *pRuntime = NULL;
    xllm *pAuto = NULL;
    xllm_session *pSession = NULL;
    xllm_response *pResponse = NULL;
    xfuture *pFuture = NULL;
    xllm_adapter tAdapter;
    xllm_profile tProfile;
    xllm_create_options tCreate;
    xllm_session_options tSessionOptions;
    xllm_turn tTurn;
    xllm_tool_def tTool;
    xllm_tool_executor_async tExecutorAsync;
    const char *sText;
    int iStatus;

    memset(&tAdapter, 0, sizeof(tAdapter));
    memset(&tProfile, 0, sizeof(tProfile));
    memset(&tCreate, 0, sizeof(tCreate));
    memset(&tSessionOptions, 0, sizeof(tSessionOptions));
    memset(&tTool, 0, sizeof(tTool));
    memset(&tExecutorAsync, 0, sizeof(tExecutorAsync));

    if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || !pRuntime ) {
        fprintf(stderr, "create runtime failed\n");
        return 1;
    }

    tAdapter.sName = "mock_tool_loop_async";
    tAdapter.pfnChat = demo_tool_loop_chat;
    if ( xllm_register_adapter(pRuntime, &tAdapter) != XRT_NET_OK ) {
        fprintf(stderr, "register adapter failed\n");
        return 2;
    }

    xllm_profile_init(&tProfile);
    tProfile.sId = "mock-tool-async";
    tProfile.sProvider = "mock";
    tProfile.sAdapter = "mock_tool_loop_async";
    tProfile.tModels.tText.sModelId = "mock-text";
    tProfile.tModels.tText.tCaps.uFlags =
        XLLM_CAP_TEXT_IN |
        XLLM_CAP_TEXT_OUT |
        XLLM_CAP_TOOL_CALL_OUT |
        XLLM_CAP_TOOL_RESULT_IN;
    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "register profile failed\n");
        return 3;
    }

    tCreate.sInitialProfileId = "mock-tool-async";
    pAuto = xllm_create(pRuntime, &tCreate);
    if ( !pAuto ) {
        fprintf(stderr, "create llm failed\n");
        return 4;
    }

    tExecutorAsync.pfnExecute = demo_tool_execute_async;
    if ( xllm_set_tool_executor_async(pAuto, &tExecutorAsync) != XRT_NET_OK ) {
        fprintf(stderr, "set llm async tool executor failed\n");
        return 5;
    }

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "what is the weather?") != XRT_NET_OK ) {
        fprintf(stderr, "add user text failed\n");
        return 6;
    }

    tTool.sToolId = "app.weather.get_current";
    tTool.sWireName = "get_weather";
    tTool.sDescription = "Get current weather";
    if ( xllm_turn_add_tool(&tTurn, &tTool) != XRT_NET_OK ) {
        fprintf(stderr, "add tool failed\n");
        return 7;
    }

    iStatus = xllm_send(pAuto, &tTurn, NULL, &pResponse);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "sync send with async executor failed: %d\n", iStatus);
        return 8;
    }
    sText = xllm_response_get_text(pResponse);
    if ( !sText || strcmp(sText, "weather result: sunny-async") != 0 ) {
        fprintf(stderr, "unexpected sync response text: %s\n", sText ? sText : "(null)");
        return 9;
    }
    xllm_response_free(pResponse);
    pResponse = NULL;

    xllm_session_options_init(&tSessionOptions);
    tSessionOptions.sProfileId = "mock-tool-async";
    if ( xllm_session_create(pRuntime, &tSessionOptions, &pSession) != XRT_NET_OK || !pSession ) {
        fprintf(stderr, "create session failed\n");
        return 10;
    }
    if ( xllm_session_set_tool_executor_async(pSession, &tExecutorAsync) != XRT_NET_OK ) {
        fprintf(stderr, "set session async tool executor failed\n");
        return 11;
    }

    pFuture = xllm_session_chat_async_thread(pSession, &tTurn, NULL);
    if ( !pFuture ) {
        fprintf(stderr, "session async future create failed\n");
        return 12;
    }

    pResponse = (xllm_response *)xFutureWaitValue(pFuture);
    if ( xFutureStatus(pFuture) != XRT_NET_OK || !pResponse ) {
        fprintf(stderr, "session async chat failed: %d\n", (int)xFutureStatus(pFuture));
        return 13;
    }

    sText = xllm_response_get_text(pResponse);
    if ( !sText || strcmp(sText, "weather result: sunny-async") != 0 ) {
        fprintf(stderr, "unexpected session async response text: %s\n", sText ? sText : "(null)");
        return 14;
    }

    printf("ok: %s\n", sText);

    xFutureRelease(pFuture);
    xllm_response_free(pResponse);
    xllm_turn_reset(&tTurn);
    xllm_session_destroy(pSession);
    xllm_destroy(pAuto);
    xllm_runtime_destroy(pRuntime);
    return 0;
}
