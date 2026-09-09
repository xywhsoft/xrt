#include "xllm-session.h"

#include <stdio.h>
#include <string.h>

static int demo_expect_error_contains(
    const xllm_error *pError,
    const char *sNeedle,
    int iExitCode
)
{
    if ( !pError ) {
        fprintf(stderr, "error object is null\n");
        return iExitCode;
    }
    if ( pError->eCode != XLLM_ERROR_UNSUPPORTED_INPUT_TYPE ) {
        fprintf(stderr, "unexpected error code: %d\n", (int)pError->eCode);
        return iExitCode;
    }
    if ( !pError->sMessage || strstr(pError->sMessage, sNeedle) == NULL ) {
        fprintf(stderr, "unexpected error message: %s\n", pError->sMessage ? pError->sMessage : "(null)");
        return iExitCode;
    }
    return 0;
}

int main(void)
{
    xllm_runtime *pRuntime = NULL;
    xllm *pLlm = NULL;
    xllm_response *pResponse = NULL;
    xllm_profile tProfile;
    xllm_turn tTurn;
    xllm_error tError;
    int iStatus;

    xllm_profile_init(&tProfile);
    xllm_turn_init(&tTurn);
    xllm_error_init(&tError);

    if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || !pRuntime ) {
        fprintf(stderr, "create runtime failed\n");
        return 1;
    }

    if ( xllm_register_ollama_native_adapter(pRuntime) != XRT_NET_OK ) {
        fprintf(stderr, "register ollama-native adapter failed\n");
        xllm_runtime_destroy(pRuntime);
        return 2;
    }

    tProfile.sId = "ollama-unsupported-image";
    tProfile.sProvider = "ollama";
    tProfile.sAdapter = XLLM_ADAPTER_OLLAMA_NATIVE;
    tProfile.sBaseUrl = "http://127.0.0.1:1";
    tProfile.tModels.tText.sModelId = "llama3.2";
    tProfile.tModels.tText.tCaps.uFlags = XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT;
    tProfile.tModels.tMultimodal.sModelId = "llava";
    tProfile.tModels.tMultimodal.tCaps.uFlags = XLLM_CAP_TEXT_IN | XLLM_CAP_IMAGE_IN | XLLM_CAP_TEXT_OUT;
    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "register profile failed\n");
        xllm_runtime_destroy(pRuntime);
        return 3;
    }

    pLlm = xllm_create(pRuntime, NULL);
    if ( !pLlm ) {
        fprintf(stderr, "create llm failed\n");
        xllm_runtime_destroy(pRuntime);
        return 4;
    }
    if ( xllm_bind_profile(pLlm, "ollama-unsupported-image") != XRT_NET_OK ) {
        fprintf(stderr, "bind profile failed\n");
        xllm_destroy(pLlm);
        xllm_runtime_destroy(pRuntime);
        return 5;
    }

    if ( xllm_turn_add_user_text(&tTurn, "describe this file-id image") != XRT_NET_OK ||
         xllm_turn_add_image_file_id(&tTurn, "img-file-1", "image/png") != XRT_NET_OK ) {
        fprintf(stderr, "build file-id turn failed\n");
        return 6;
    }
    iStatus = xllm_send_ex(pLlm, &tTurn, NULL, &pResponse, &tError);
    if ( iStatus == XRT_NET_OK || pResponse ) {
        fprintf(stderr, "ollama image file-id unexpectedly succeeded\n");
        return 7;
    }
    iStatus = demo_expect_error_contains(&tError, "provider file_id", 8);
    if ( iStatus != 0 ) {
        return iStatus;
    }

    xllm_error_free(&tError);
    xllm_turn_reset(&tTurn);
    xllm_destroy(pLlm);
    xllm_runtime_destroy(pRuntime);
    return 0;
}
