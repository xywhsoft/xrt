#include "azure_openai_demo_common.h"

static xllm_stream_mode demo_multimodal_stream_mode(void)
{
    const char *sValue = getenv("AZURE_OPENAI_MULTIMODAL_STREAM");

    if ( sValue == NULL || sValue[0] == '\0' ) {
        return XLLM_STREAM_OFF;
    }

    if ( strcmp(sValue, "1") == 0 ||
         strcmp(sValue, "true") == 0 ||
         strcmp(sValue, "TRUE") == 0 ||
         strcmp(sValue, "on") == 0 ||
         strcmp(sValue, "ON") == 0 ) {
        return XLLM_STREAM_PREFER;
    }

    return XLLM_STREAM_OFF;
}

static int demo_add_optional_multimodal_inputs(xllm_turn *pTurn)
{
    const char *sImageUrl = getenv("AZURE_OPENAI_IMAGE_URL");
    const char *sImagePath = getenv("AZURE_OPENAI_IMAGE_PATH");
    const char *sImageFileId = getenv("AZURE_OPENAI_IMAGE_FILE_ID");
    const char *sImageMime = demo_env_optional("AZURE_OPENAI_IMAGE_MIME", "image/png");
    const char *sFileUrl = getenv("AZURE_OPENAI_FILE_URL");
    const char *sFilePath = getenv("AZURE_OPENAI_FILE_PATH");
    const char *sFileFileId = getenv("AZURE_OPENAI_FILE_FILE_ID");
    const char *sFileMime = demo_env_optional("AZURE_OPENAI_FILE_MIME", "application/pdf");
    int iStatus;

    if ( sImageUrl && sImageUrl[0] != '\0' ) {
        iStatus = xllm_turn_add_image_url(pTurn, sImageUrl, sImageMime);
        if ( iStatus != XRT_NET_OK ) {
            return iStatus;
        }
    }
    if ( sImagePath && sImagePath[0] != '\0' ) {
        iStatus = xllm_turn_add_image_file(pTurn, sImagePath, sImageMime);
        if ( iStatus != XRT_NET_OK ) {
            return iStatus;
        }
    }
    if ( sImageFileId && sImageFileId[0] != '\0' ) {
        iStatus = xllm_turn_add_image_file_id(pTurn, sImageFileId, sImageMime);
        if ( iStatus != XRT_NET_OK ) {
            return iStatus;
        }
    }
    if ( sFileUrl && sFileUrl[0] != '\0' ) {
        iStatus = xllm_turn_add_file_url(pTurn, sFileUrl, sFileMime);
        if ( iStatus != XRT_NET_OK ) {
            return iStatus;
        }
    }
    if ( sFilePath && sFilePath[0] != '\0' ) {
        iStatus = xllm_turn_add_file(pTurn, sFilePath, sFileMime);
        if ( iStatus != XRT_NET_OK ) {
            return iStatus;
        }
    }
    if ( sFileFileId && sFileFileId[0] != '\0' ) {
        iStatus = xllm_turn_add_file_file_id(pTurn, sFileFileId, sFileMime);
        if ( iStatus != XRT_NET_OK ) {
            return iStatus;
        }
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

    xllm_turn_init(&tTurn);
    if ( xllm_turn_add_user_text(&tTurn, "Please analyze the attached image and/or file. Describe what you can infer and clearly state if some input was not provided.") != XRT_NET_OK ) {
        fprintf(stderr, "failed to add user text\n");
        xllm_destroy(pLlm);
        xllm_runtime_destroy(pRuntime);
        return 2;
    }
    if ( demo_add_optional_multimodal_inputs(&tTurn) != XRT_NET_OK ) {
        fprintf(stderr, "failed to add multimodal inputs\n");
        xllm_turn_reset(&tTurn);
        xllm_destroy(pLlm);
        xllm_runtime_destroy(pRuntime);
        return 3;
    }

    xllm_call_options_init(&tCallOptions);
    tCallOptions.eStreamMode = demo_multimodal_stream_mode();
    tCallOptions.pfnOnEvent = demo_on_event_print;
    tCallOptions.uTimeoutMs = demo_env_u32("AZURE_OPENAI_MULTIMODAL_TIMEOUT_MS", 300000u);

    printf("=== Azure OpenAI Multimodal Example ===\n");
    printf("Optional envs:\n");
    printf("  AZURE_OPENAI_IMAGE_URL / AZURE_OPENAI_IMAGE_PATH / AZURE_OPENAI_IMAGE_FILE_ID\n");
    printf("  AZURE_OPENAI_FILE_URL / AZURE_OPENAI_FILE_PATH / AZURE_OPENAI_FILE_FILE_ID\n");
    printf("  AZURE_OPENAI_MULTIMODAL_STREAM=1   (optional, default is non-streaming)\n\n");
    printf("timeout ms : %u\n\n", (unsigned)tCallOptions.uTimeoutMs);

    printf("[assistant] ");
    iStatus = xllm_send_ex(pLlm, &tTurn, &tCallOptions, &pResponse, &tError);
    printf("\n");
    if ( iStatus != XRT_NET_OK || pResponse == NULL ) {
        demo_print_error("Azure OpenAI multimodal request failed", iStatus, &tError);
        xllm_turn_reset(&tTurn);
        xllm_error_free(&tError);
        xllm_destroy(pLlm);
        xllm_runtime_destroy(pRuntime);
        return 4;
    }

    printf("visible text: %s\n", xllm_response_get_text(pResponse));
    xllm_response_free(pResponse);
    xllm_turn_reset(&tTurn);
    xllm_error_free(&tError);
    xllm_destroy(pLlm);
    xllm_runtime_destroy(pRuntime);
    return 0;
}
