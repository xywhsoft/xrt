#include "azure_openai_demo_common.h"

int main(void)
{
    xllm_runtime *pRuntime = NULL;
    xllm *pLlm = NULL;
    xllm_response *pResponse = NULL;
    xllm_turn tTurn;
    xllm_call_options tCallOptions;
    xllm_error tError;
    int iStatus;

    demo_setup_console();
    xrtInit();
    xllm_error_init(&tError);

    iStatus = demo_create_runtime_and_llm(
        XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT | XLLM_CAP_STREAM,
        XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT | XLLM_CAP_STREAM | XLLM_CAP_IMAGE_IN | XLLM_CAP_FILE_IN,
        &pRuntime,
        &pLlm
    );
    if ( iStatus != XRT_NET_OK ) {
        return 1;
    }

    if ( demo_prepare_text_turn(&tTurn, "Please stream a short 3-line reply to confirm Azure OpenAI streaming works.") != XRT_NET_OK ) {
        fprintf(stderr, "failed to build turn\n");
        xllm_destroy(pLlm);
        xllm_runtime_destroy(pRuntime);
        return 2;
    }

    xllm_call_options_init(&tCallOptions);
    tCallOptions.eStreamMode = XLLM_STREAM_REQUIRE;
    tCallOptions.pfnOnEvent = demo_on_event_print;
    tCallOptions.uTimeoutMs = 120000u;

    printf("=== Azure OpenAI Stream Example ===\n");
    printf("[assistant] ");
    iStatus = xllm_send_ex(pLlm, &tTurn, &tCallOptions, &pResponse, &tError);
    printf("\n");
    if ( iStatus != XRT_NET_OK || pResponse == NULL ) {
        demo_print_error("Azure OpenAI stream request failed", iStatus, &tError);
        xllm_turn_reset(&tTurn);
        xllm_error_free(&tError);
        xllm_destroy(pLlm);
        xllm_runtime_destroy(pRuntime);
        return 3;
    }

    printf("\nvisible text: %s\n", xllm_response_get_text(pResponse));
    xllm_response_free(pResponse);
    xllm_turn_reset(&tTurn);
    xllm_error_free(&tError);
    xllm_destroy(pLlm);
    xllm_runtime_destroy(pRuntime);
    return 0;
}
