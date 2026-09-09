#include "azure_openai_demo_common.h"

static int32 demo_tool_execute(
    void *pCtx,
    const xllm_tool_exec_request *pRequest,
    xllm_tool_exec_result *pResult,
    xllm_error *pError
)
{
    const char *sResultText = demo_env_optional("AZURE_OPENAI_TOOL_RESULT_TEXT", "tool-result-from-local-executor");

    (void)pCtx;

    if ( pRequest == NULL || pResult == NULL || pRequest->sToolId == NULL ) {
        if ( pError ) {
            pError->eCode = XLLM_ERROR_INVALID_REQUEST;
            pError->sMessage = "invalid tool request";
        }
        return XRT_NET_ERROR;
    }

    pResult->pParts = (xllm_content_part *)xrtCalloc(1u, sizeof(xllm_content_part));
    if ( pResult->pParts == NULL ) {
        return XRT_NET_ERROR;
    }
    pResult->iPartCount = 1u;
    pResult->pParts[0].eKind = XLLM_PART_TEXT;
    pResult->pParts[0].as.tSource.eKind = XLLM_SOURCE_INLINE_TEXT;
    pResult->pParts[0].as.tSource.sMimeType = demo_dup_text("text/plain");
    pResult->pParts[0].as.tSource.as.sText = demo_dup_text(sResultText);
    if ( pResult->pParts[0].as.tSource.sMimeType == NULL ||
         pResult->pParts[0].as.tSource.as.sText == NULL ) {
        xllm_tool_exec_result_free(pResult);
        return XRT_NET_ERROR;
    }

    return XRT_NET_OK;
}

int main(void)
{
    xllm_runtime *pRuntime = NULL;
    xllm *pLlm = NULL;
    xllm_response *pResponse = NULL;
    xllm_turn tTurn;
    xllm_call_options tCallOptions;
    xllm_tool_def tTool;
    xllm_tool_executor tExecutor;
    xllm_error tError;
    xvalue tSchema = NULL;
    int iStatus;

    demo_setup_console();
    xrtInit();
    xllm_error_init(&tError);

    iStatus = demo_create_runtime_and_llm(
        XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT | XLLM_CAP_STREAM | XLLM_CAP_TOOL_CALL_OUT | XLLM_CAP_TOOL_RESULT_IN,
        XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT | XLLM_CAP_STREAM | XLLM_CAP_TOOL_CALL_OUT | XLLM_CAP_TOOL_RESULT_IN | XLLM_CAP_IMAGE_IN | XLLM_CAP_FILE_IN,
        &pRuntime,
        &pLlm
    );
    if ( iStatus != XRT_NET_OK ) {
        return 1;
    }

    memset(&tExecutor, 0, sizeof(tExecutor));
    tExecutor.pfnExecute = demo_tool_execute;
    if ( xllm_set_tool_executor(pLlm, &tExecutor) != XRT_NET_OK ) {
        fprintf(stderr, "failed to set tool executor\n");
        xllm_destroy(pLlm);
        xllm_runtime_destroy(pRuntime);
        return 2;
    }

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "Call the tool to get the local probe value, then explain the result briefly.") != XRT_NET_OK ) {
        fprintf(stderr, "failed to add user text\n");
        xllm_destroy(pLlm);
        xllm_runtime_destroy(pRuntime);
        return 3;
    }

    memset(&tTool, 0, sizeof(tTool));
    tTool.sToolId = "app.azure.probe.get_value";
    tTool.sWireName = "get_probe_value";
    tTool.sDescription = "Return a local probe value for Azure OpenAI tool-loop testing.";
    tSchema = xrtParseJSON(
        (str)"{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}",
        strlen("{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}")
    );
    if ( tSchema == NULL ) {
        fprintf(stderr, "failed to parse tool schema\n");
        xllm_turn_reset(&tTurn);
        xllm_destroy(pLlm);
        xllm_runtime_destroy(pRuntime);
        return 4;
    }
    tTool.tInputSchema = tSchema;
    if ( xllm_turn_add_tool(&tTurn, &tTool) != XRT_NET_OK ) {
        fprintf(stderr, "failed to add tool\n");
        xvoUnref(tSchema);
        xllm_turn_reset(&tTurn);
        xllm_destroy(pLlm);
        xllm_runtime_destroy(pRuntime);
        return 5;
    }
    xvoUnref(tSchema);
    if ( xllm_turn_set_tool_choice(&tTurn, XLLM_TOOL_CHOICE_AUTO, NULL, false) != XRT_NET_OK ) {
        fprintf(stderr, "failed to set tool choice\n");
        xllm_turn_reset(&tTurn);
        xllm_destroy(pLlm);
        xllm_runtime_destroy(pRuntime);
        return 6;
    }

    xllm_call_options_init(&tCallOptions);
    tCallOptions.eStreamMode = XLLM_STREAM_PREFER;
    tCallOptions.pfnOnEvent = demo_on_event_print;
    tCallOptions.uTimeoutMs = 120000u;

    printf("=== Azure OpenAI Tool Loop Example ===\n");
    printf("[assistant] ");
    iStatus = xllm_send_ex(pLlm, &tTurn, &tCallOptions, &pResponse, &tError);
    printf("\n");
    if ( iStatus != XRT_NET_OK || pResponse == NULL ) {
        demo_print_error("Azure OpenAI tool-loop request failed", iStatus, &tError);
        xllm_turn_reset(&tTurn);
        xllm_error_free(&tError);
        xllm_destroy(pLlm);
        xllm_runtime_destroy(pRuntime);
        return 7;
    }

    printf("visible text: %s\n", xllm_response_get_text(pResponse));
    xllm_response_free(pResponse);
    xllm_turn_reset(&tTurn);
    xllm_error_free(&tError);
    xllm_destroy(pLlm);
    xllm_runtime_destroy(pRuntime);
    return 0;
}
