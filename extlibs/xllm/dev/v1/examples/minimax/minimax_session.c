#define MINIMAX_API_KEY getenv("MINIMAX_API_KEY")
#define MINIMAX_BASE_URL "https://api.minimaxi.com/v1"
#define MINIMAX_MODEL    "MiniMax-M2.7"

#include "xllm-session.h"

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

static bool demo_on_event(const xllm_event *pEvent, void *pUserData)
{
    (void)pUserData;

    if ( pEvent && pEvent->eType == XLLM_EVENT_TEXT_DELTA ) {
        printf("%s", pEvent->as.tTextDelta.sText ? pEvent->as.tTextDelta.sText : "");
        fflush(stdout);
    }
    return true;
}

static void demo_print_error(const char *sPrefix, int iStatus, const xllm_error *pError)
{
    fprintf(stderr, "%s (status=%d", sPrefix, iStatus);
    if ( pError ) {
        fprintf(
            stderr,
            ", code=%d, http=%d, msg=%s",
            (int)pError->eCode,
            (int)pError->iHttpStatus,
            pError->sMessage ? pError->sMessage : "(null)"
        );
    }
    fprintf(stderr, ")\n");
}

int main(void)
{
    xllm_runtime *pRuntime = NULL;
    xllm_session *pSession = NULL;
    xllm_response *pResponse = NULL;
    xllm_profile tProfile;
    xllm_session_options tSessionOptions;
    xllm_turn tTurn;
    xllm_call_options tCallOptions;
    xllm_error tError;
    int iStatus;

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    xrtInit();

    printf("=== MiniMax Native Session Example ===\n\n");

    if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || !pRuntime ) {
        fprintf(stderr, "failed to create runtime\n");
        return 1;
    }
    if ( xllm_register_minimax_native_adapter(pRuntime) != XRT_NET_OK ) {
        fprintf(stderr, "failed to register minimax native adapter\n");
        xllm_runtime_destroy(pRuntime);
        return 2;
    }

    xllm_profile_init(&tProfile);
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

    xllm_session_options_init(&tSessionOptions);
    tSessionOptions.sProfileId = "minimax-native";
    tSessionOptions.sSystemPrompt = "You are a concise assistant.";
    tSessionOptions.bEnableAutoCompact = true;
    tSessionOptions.uCompactTriggerTurns = 12u;

    if ( xllm_session_create(pRuntime, &tSessionOptions, &pSession) != XRT_NET_OK || !pSession ) {
        fprintf(stderr, "failed to create session\n");
        xllm_runtime_destroy(pRuntime);
        return 4;
    }

    xllm_call_options_init(&tCallOptions);
    tCallOptions.eStreamMode = XLLM_STREAM_PREFER;
    tCallOptions.pfnOnEvent = demo_on_event;
    tCallOptions.uTimeoutMs = 120000u;
    xllm_error_init(&tError);

    printf("--- turn 1 ---\n");
    xllm_turn_init(&tTurn);
    xllm_turn_add_user_text(&tTurn, "My project name is xllm. Please remember it.");
    printf("[user] My project name is xllm. Please remember it.\n");
    printf("[minimax] ");
    iStatus = xllm_session_chat_ex(pSession, &tTurn, &tCallOptions, &pResponse, &tError);
    printf("\n");
    if ( iStatus != XRT_NET_OK || !pResponse ) {
        demo_print_error("minimax session turn 1 failed", iStatus, &tError);
        xllm_turn_reset(&tTurn);
        xllm_error_free(&tError);
        xllm_session_destroy(pSession);
        xllm_runtime_destroy(pRuntime);
        return 5;
    }
    xllm_response_free(pResponse);
    pResponse = NULL;
    xllm_turn_reset(&tTurn);

    printf("\n--- turn 2 ---\n");
    xllm_turn_init(&tTurn);
    xllm_turn_add_user_text(&tTurn, "What is my project name?");
    printf("[user] What is my project name?\n");
    printf("[minimax] ");
    iStatus = xllm_session_chat_ex(pSession, &tTurn, &tCallOptions, &pResponse, &tError);
    printf("\n");
    if ( iStatus != XRT_NET_OK || !pResponse ) {
        demo_print_error("minimax session turn 2 failed", iStatus, &tError);
        xllm_turn_reset(&tTurn);
        xllm_error_free(&tError);
        xllm_session_destroy(pSession);
        xllm_runtime_destroy(pRuntime);
        return 6;
    }

    printf("\nvisible text: %s\n", xllm_response_get_text(pResponse));

    xllm_response_free(pResponse);
    xllm_turn_reset(&tTurn);
    xllm_error_free(&tError);
    xllm_session_destroy(pSession);
    xllm_runtime_destroy(pRuntime);
    return 0;
}
