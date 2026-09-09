#include "azure_openai_demo_common.h"

int main(void)
{
    xllm_runtime *pRuntime = NULL;
    xllm *pLlm = NULL;
    xllm_response *pResponse = NULL;
    xllm_turn tTurn;
    xllm_call_options tCallOptions;
    xllm_error tError;
    xvalue tSchema = NULL;
    xvalue tFormatExtra = NULL;
    const xvalue *pJson = NULL;
    char *sJson = NULL;
    int iStatus;

    demo_setup_console();
    xrtInit();
    xllm_error_init(&tError);

    iStatus = demo_create_runtime_and_llm(
        XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT | XLLM_CAP_JSON_OUT | XLLM_CAP_STREAM,
        XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT | XLLM_CAP_JSON_OUT | XLLM_CAP_STREAM | XLLM_CAP_IMAGE_IN | XLLM_CAP_FILE_IN,
        &pRuntime,
        &pLlm
    );
    if ( iStatus != XRT_NET_OK ) {
        return 1;
    }

    if ( demo_prepare_text_turn(&tTurn, "Return JSON only. Include fields {\"answer\": string, \"ok\": boolean}.") != XRT_NET_OK ) {
        fprintf(stderr, "failed to build turn\n");
        xllm_destroy(pLlm);
        xllm_runtime_destroy(pRuntime);
        return 2;
    }

    tSchema = xrtParseJSON(
        (str)"{\"type\":\"object\",\"properties\":{\"answer\":{\"type\":\"string\"},\"ok\":{\"type\":\"boolean\"}},\"required\":[\"answer\",\"ok\"],\"additionalProperties\":false}",
        strlen("{\"type\":\"object\",\"properties\":{\"answer\":{\"type\":\"string\"},\"ok\":{\"type\":\"boolean\"}},\"required\":[\"answer\",\"ok\"],\"additionalProperties\":false}")
    );
    if ( tSchema == NULL ) {
        fprintf(stderr, "failed to parse schema\n");
        xllm_turn_reset(&tTurn);
        xllm_destroy(pLlm);
        xllm_runtime_destroy(pRuntime);
        return 3;
    }

    tFormatExtra = xvoCreateTable();
    if ( tFormatExtra == NULL || !xvoTableSetBool(tFormatExtra, (str)"strict", 6u, true) ) {
        fprintf(stderr, "failed to create schema vendor extra\n");
        if ( tFormatExtra ) {
            xvoUnref(tFormatExtra);
        }
        xvoUnref(tSchema);
        xllm_turn_reset(&tTurn);
        xllm_destroy(pLlm);
        xllm_runtime_destroy(pRuntime);
        return 4;
    }

    if ( xllm_turn_set_json_schema_response(&tTurn, "azure_openai_demo_schema", tSchema, tFormatExtra) != XRT_NET_OK ) {
        fprintf(stderr, "failed to set json schema response\n");
        xvoUnref(tFormatExtra);
        xvoUnref(tSchema);
        xllm_turn_reset(&tTurn);
        xllm_destroy(pLlm);
        xllm_runtime_destroy(pRuntime);
        return 5;
    }
    xvoUnref(tFormatExtra);
    xvoUnref(tSchema);

    xllm_call_options_init(&tCallOptions);
    tCallOptions.eStreamMode = XLLM_STREAM_PREFER;
    tCallOptions.pfnOnEvent = demo_on_event_print;
    tCallOptions.uTimeoutMs = 120000u;

    printf("=== Azure OpenAI JSON Schema Example ===\n");
    printf("[assistant] ");
    iStatus = xllm_send_ex(pLlm, &tTurn, &tCallOptions, &pResponse, &tError);
    printf("\n");
    if ( iStatus != XRT_NET_OK || pResponse == NULL ) {
        demo_print_error("Azure OpenAI JSON schema request failed", iStatus, &tError);
        xllm_turn_reset(&tTurn);
        xllm_error_free(&tError);
        xllm_destroy(pLlm);
        xllm_runtime_destroy(pRuntime);
        return 6;
    }

    pJson = xllm_response_get_first_json(pResponse, NULL, NULL);
    if ( pJson != NULL ) {
        sJson = (char *)xrtStringifyJSON(*pJson, 0, NULL);
    }
    printf("visible text: %s\n", xllm_response_get_text(pResponse));
    printf("json output : %s\n", sJson ? sJson : "(null)");

    if ( sJson ) {
        xrtFree(sJson);
    }
    xllm_response_free(pResponse);
    xllm_turn_reset(&tTurn);
    xllm_error_free(&tError);
    xllm_destroy(pLlm);
    xllm_runtime_destroy(pRuntime);
    return 0;
}
