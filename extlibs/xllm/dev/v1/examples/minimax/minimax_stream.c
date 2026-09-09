#define MINIMAX_API_KEY getenv("MINIMAX_API_KEY")
#define MINIMAX_BASE_URL "https://api.minimaxi.com/v1"
#define MINIMAX_MODEL    "MiniMax-M2.7"

#include "xllm-session.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

static bool demo_on_event(const xllm_event *pEvent, void *pUserData)
{
    bool *pbStarted = (bool *)pUserData;

    if ( !pEvent ) {
        return true;
    }

    if ( pEvent->eType == XLLM_EVENT_TEXT_DELTA && pEvent->as.tTextDelta.sText ) {
        if ( pbStarted && !*pbStarted ) {
            printf("[assistant] ");
            *pbStarted = true;
        }
        printf("%s", pEvent->as.tTextDelta.sText);
        fflush(stdout);
    }

    return true;
}

int main(void)
{
    xllm_runtime *pRuntime = NULL;
    xllm *pLlm = NULL;
    xllm_response *pResponse = NULL;
    xllm_profile tProfile;
    xllm_turn tTurn;
    xllm_call_options tCallOpts;
    xllm_error tError;
    bool bStarted = false;
    int iStatus;

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    xrtInit();

    printf("=== MiniMax Native Stream Example ===\n\n");

    xllm_error_init(&tError);
    xllm_profile_init(&tProfile);
    xllm_turn_init(&tTurn);
    xllm_call_options_init(&tCallOpts);

    if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || !pRuntime ) {
        fprintf(stderr, "failed to create runtime\n");
        return 1;
    }
    if ( xllm_register_minimax_native_adapter(pRuntime) != XRT_NET_OK ) {
        fprintf(stderr, "failed to register minimax native adapter\n");
        xllm_runtime_destroy(pRuntime);
        return 2;
    }

    tProfile.sId = "minimax-native";
    tProfile.sProvider = "minimax";
    tProfile.sAdapter = XLLM_ADAPTER_MINIMAX_NATIVE;
    tProfile.sBaseUrl = MINIMAX_BASE_URL;
    tProfile.tAuth.eKind = XLLM_AUTH_BEARER;
    tProfile.tAuth.sSecret = MINIMAX_API_KEY;
    tProfile.tModels.tText.sModelId = MINIMAX_MODEL;
    tProfile.tModels.tText.tCaps.uFlags = XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT | XLLM_CAP_STREAM;

    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "failed to register profile\n");
        xllm_runtime_destroy(pRuntime);
        return 3;
    }

    pLlm = xllm_create(pRuntime, NULL);
    if ( !pLlm || xllm_bind_profile(pLlm, "minimax-native") != XRT_NET_OK ) {
        fprintf(stderr, "failed to create llm or bind profile\n");
        xllm_turn_reset(&tTurn);
        xllm_runtime_destroy(pRuntime);
        return 4;
    }

    xllm_set_system_prompt(pLlm, "You are a concise assistant.");
    xllm_turn_add_user_text(&tTurn, "Return exactly three short lines confirming MiniMax streaming works.");

    tCallOpts.eStreamMode = XLLM_STREAM_REQUIRE;
    tCallOpts.pfnOnEvent = demo_on_event;
    tCallOpts.pUserData = &bStarted;
    tCallOpts.uTimeoutMs = 120000u;

    iStatus = xllm_send_ex(pLlm, &tTurn, &tCallOpts, &pResponse, &tError);
    if ( iStatus != XRT_NET_OK || !pResponse ) {
        fprintf(stderr, "\nrequest failed (status=%d, code=%d, msg=%s, http=%d)\n",
                iStatus,
                (int)tError.eCode,
                tError.sMessage ? tError.sMessage : "(null)",
                (int)tError.iHttpStatus);
        xllm_destroy(pLlm);
        xllm_error_free(&tError);
        xllm_turn_reset(&tTurn);
        xllm_runtime_destroy(pRuntime);
        return 5;
    }

    if ( bStarted ) {
        printf("\n\n");
    }
    printf("visible text: %s\n", xllm_response_get_text(pResponse));

    xllm_response_free(pResponse);
    xllm_destroy(pLlm);
    xllm_error_free(&tError);
    xllm_turn_reset(&tTurn);
    xllm_runtime_destroy(pRuntime);
    return 0;
}
