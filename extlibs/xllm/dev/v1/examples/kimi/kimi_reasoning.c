#define KIMI_API_KEY getenv("KIMI_API_KEY")
#define KIMI_BASE_URL "https://api.moonshot.cn/v1"
#define KIMI_MODEL    "kimi-k2-thinking"

#include "xllm-session.h"

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

int main(void)
{
    xllm_runtime *pRuntime = NULL;
    xllm *pLlm = NULL;
    xllm_response *pResponse = NULL;
    xllm_profile tProfile;
    xllm_turn tTurn;
    xllm_call_options tCallOpts;
    xllm_error tError;
    int iStatus;
    size_t i;

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    xrtInit();

    printf("=== Kimi Native Reasoning Example ===\n");
    printf("base url : %s\n", KIMI_BASE_URL);
    printf("model    : %s\n\n", KIMI_MODEL);

    xllm_error_init(&tError);
    xllm_profile_init(&tProfile);
    xllm_turn_init(&tTurn);
    xllm_call_options_init(&tCallOpts);

    if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || !pRuntime ) {
        fprintf(stderr, "failed to create runtime\n");
        return 1;
    }
    if ( xllm_register_kimi_native_adapter(pRuntime) != XRT_NET_OK ) {
        fprintf(stderr, "failed to register kimi native adapter\n");
        xllm_runtime_destroy(pRuntime);
        return 2;
    }

    tProfile.sId = "kimi-native";
    tProfile.sProvider = "moonshot";
    tProfile.sAdapter = XLLM_ADAPTER_KIMI_NATIVE;
    tProfile.sBaseUrl = KIMI_BASE_URL;
    tProfile.tAuth.eKind = XLLM_AUTH_BEARER;
    tProfile.tAuth.sSecret = KIMI_API_KEY;
    tProfile.tModels.tText.sModelId = KIMI_MODEL;
    tProfile.tModels.tText.tCaps.uFlags =
        XLLM_CAP_TEXT_IN |
        XLLM_CAP_TEXT_OUT |
        XLLM_CAP_THINKING_FULL_OUT |
        XLLM_CAP_REASONING_CONTROL;

    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "failed to register profile\n");
        xllm_runtime_destroy(pRuntime);
        return 3;
    }

    pLlm = xllm_create(pRuntime, NULL);
    if ( !pLlm || xllm_bind_profile(pLlm, "kimi-native") != XRT_NET_OK ) {
        fprintf(stderr, "failed to create llm or bind profile\n");
        xllm_destroy(pLlm);
        xllm_runtime_destroy(pRuntime);
        return 4;
    }

    xllm_set_system_prompt(pLlm, "You are a concise assistant.");
    xllm_turn_add_user_text(&tTurn, "Think briefly and then answer: what is xllm?");

    tTurn.tReasoning.tEnabled.bSet = true;
    tTurn.tReasoning.tEnabled.bValue = true;
    tTurn.tReasoning.eLevel = XLLM_REASONING_HIGH;
    tTurn.tReasoning.tExposeThinking.bSet = true;
    tTurn.tReasoning.tExposeThinking.bValue = true;

    tCallOpts.uTimeoutMs = 120000u;

    iStatus = xllm_send_ex(pLlm, &tTurn, &tCallOpts, &pResponse, &tError);
    if ( iStatus != XRT_NET_OK || !pResponse ) {
        fprintf(
            stderr,
            "request failed (status=%d, code=%d, http=%d, msg=%s)\n",
            iStatus,
            (int)tError.eCode,
            (int)tError.iHttpStatus,
            tError.sMessage ? tError.sMessage : "(null)"
        );
        xllm_error_reset(&tError);
        xllm_turn_reset(&tTurn);
        xllm_destroy(pLlm);
        xllm_runtime_destroy(pRuntime);
        return 5;
    }

    for ( i = 0u; i < pResponse->iOutputCount; ++i ) {
        const xllm_output_item *pOutput = &pResponse->pOutputs[i];
        if ( pOutput->eKind == XLLM_OUTPUT_THINKING ) {
            printf("[thinking] %s\n", pOutput->as.tThinking.sText ? pOutput->as.tThinking.sText : "(empty)");
        } else if ( pOutput->eKind == XLLM_OUTPUT_MESSAGE ) {
            printf("[assistant] %s\n", xllm_response_get_text(pResponse));
        }
    }

    if ( !pResponse->iOutputCount ) {
        printf("[assistant] %s\n", xllm_response_get_text(pResponse));
    }

    printf("response model: %s\n", pResponse->sModel ? pResponse->sModel : "(unknown)");

    xllm_response_free(pResponse);
    xllm_error_reset(&tError);
    xllm_turn_reset(&tTurn);
    xllm_destroy(pLlm);
    xllm_runtime_destroy(pRuntime);
    return 0;
}
