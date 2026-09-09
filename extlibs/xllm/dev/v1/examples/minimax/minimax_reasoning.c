#include "xllm-session.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#endif

static const char *demo_env(const char *sName, const char *sDefault)
{
    const char *sValue = getenv(sName);
    return (sValue && sValue[0]) ? sValue : sDefault;
}

static void print_thinking_outputs(const xllm_response *pResponse)
{
    size_t i;
    bool bPrinted = false;

    if ( !pResponse ) {
        return;
    }
    for ( i = 0u; i < pResponse->iOutputCount; ++i ) {
        const xllm_output_item *pOutput = &pResponse->pOutputs[i];
        if ( pOutput->eKind == XLLM_OUTPUT_THINKING ) {
            printf("[thinking] %s\n", pOutput->as.tThinking.sText ? pOutput->as.tThinking.sText : "");
            bPrinted = true;
        }
    }
    if ( !bPrinted ) {
        printf("[thinking] (none)\n");
    }
}

int main(void)
{
    const char *sApiKey = getenv("MINIMAX_API_KEY");
    const char *sBaseUrl = demo_env("MINIMAX_BASE_URL", "https://api.minimaxi.com/v1");
    const char *sModel = demo_env("MINIMAX_MODEL", "MiniMax-M2.7");
    xllm_runtime *pRuntime = NULL;
    xllm *pLlm = NULL;
    xllm_response *pResponse = NULL;
    xllm_profile tProfile;
    xllm_turn tTurn;
    xllm_call_options tCallOpts;
    xllm_error tError;
    int iStatus;

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    xrtInit();

    printf("=== MiniMax Native Reasoning Example ===\n");
    printf("base url : %s\n", sBaseUrl);
    printf("model    : %s\n\n", sModel);

    if ( !sApiKey || !sApiKey[0] ) {
        fprintf(stderr, "set MINIMAX_API_KEY first\n");
        return 2;
    }

    xllm_error_init(&tError);
    xllm_profile_init(&tProfile);
    xllm_turn_init(&tTurn);
    xllm_call_options_init(&tCallOpts);

    if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || !pRuntime ) {
        fprintf(stderr, "failed to create runtime\n");
        return 3;
    }
    if ( xllm_register_minimax_native_adapter(pRuntime) != XRT_NET_OK ) {
        fprintf(stderr, "failed to register minimax native adapter\n");
        xllm_runtime_destroy(pRuntime);
        return 4;
    }

    tProfile.sId = "minimax-native";
    tProfile.sProvider = "minimax";
    tProfile.sAdapter = XLLM_ADAPTER_MINIMAX_NATIVE;
    tProfile.sBaseUrl = sBaseUrl;
    tProfile.tAuth.eKind = XLLM_AUTH_BEARER;
    tProfile.tAuth.sSecret = sApiKey;
    tProfile.tModels.tText.sModelId = sModel;
    tProfile.tModels.tText.tCaps.uFlags =
        XLLM_CAP_TEXT_IN |
        XLLM_CAP_TEXT_OUT |
        XLLM_CAP_THINKING_FULL_OUT |
        XLLM_CAP_REASONING_CONTROL;

    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "failed to register profile\n");
        xllm_runtime_destroy(pRuntime);
        return 5;
    }

    pLlm = xllm_create(pRuntime, NULL);
    if ( !pLlm || xllm_bind_profile(pLlm, "minimax-native") != XRT_NET_OK ) {
        fprintf(stderr, "failed to create llm or bind profile\n");
        xllm_turn_reset(&tTurn);
        xllm_runtime_destroy(pRuntime);
        return 6;
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
            "request failed (status=%d, code=%d, msg=%s, http=%d)\n",
            iStatus,
            (int)tError.eCode,
            tError.sMessage ? tError.sMessage : "(null)",
            (int)tError.iHttpStatus
        );
        xllm_destroy(pLlm);
        xllm_error_free(&tError);
        xllm_turn_reset(&tTurn);
        xllm_runtime_destroy(pRuntime);
        return 7;
    }

    print_thinking_outputs(pResponse);
    printf("[assistant] %s\n", xllm_response_get_text(pResponse));
    if ( pResponse->sModel ) {
        printf("response model: %s\n", pResponse->sModel);
    }

    xllm_response_free(pResponse);
    xllm_destroy(pLlm);
    xllm_error_free(&tError);
    xllm_turn_reset(&tTurn);
    xllm_runtime_destroy(pRuntime);
    return 0;
}
