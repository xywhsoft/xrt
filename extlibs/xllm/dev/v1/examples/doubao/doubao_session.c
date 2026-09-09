#include "xllm-session.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#endif

static const char *demo_env2(const char *sNameA, const char *sNameB, const char *sDefault)
{
    const char *sValue = getenv(sNameA);
    if ( sValue && sValue[0] ) {
        return sValue;
    }
    sValue = getenv(sNameB);
    return (sValue && sValue[0]) ? sValue : sDefault;
}

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
    const char *sApiKey = demo_env2("DOUBAO_API_KEY", "ARK_API_KEY", NULL);
    const char *sBaseUrl = demo_env2("DOUBAO_BASE_URL", "ARK_BASE_URL", "https://ark.cn-beijing.volces.com/api/v3");
    const char *sModel = demo_env2("DOUBAO_MODEL", "ARK_MODEL", NULL);
    xllm_runtime *pRuntime = NULL;
    xllm_session *pSession = NULL;
    xllm_response *pResponse = NULL;
    xllm_profile tProfile;
    xllm_session_options tSessionOptions;
    xllm_turn tTurn;
    xllm_call_options tCallOptions;
    xllm_error tError;
    int iStatus;

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    xrtInit();

    printf("=== Doubao Native Session Example ===\n");
    printf("base url : %s\n", sBaseUrl);
    printf("model    : %s\n\n", sModel ? sModel : "(set DOUBAO_MODEL or ARK_MODEL)");

    if ( !sApiKey || !sApiKey[0] ) {
        fprintf(stderr, "set DOUBAO_API_KEY or ARK_API_KEY first\n");
        return 2;
    }
    if ( !sModel || !sModel[0] ) {
        fprintf(stderr, "set DOUBAO_MODEL or ARK_MODEL first\n");
        return 3;
    }

    if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || !pRuntime ) {
        fprintf(stderr, "failed to create runtime\n");
        return 4;
    }
    if ( xllm_register_doubao_native_adapter(pRuntime) != XRT_NET_OK ) {
        fprintf(stderr, "failed to register doubao native adapter\n");
        xllm_runtime_destroy(pRuntime);
        return 5;
    }

    xllm_profile_init(&tProfile);
    tProfile.sId = "doubao-native";
    tProfile.sProvider = "volcengine";
    tProfile.sAdapter = XLLM_ADAPTER_DOUBAO_NATIVE;
    tProfile.sBaseUrl = sBaseUrl;
    tProfile.tAuth.eKind = XLLM_AUTH_BEARER;
    tProfile.tAuth.sSecret = sApiKey;
    tProfile.tModels.tText.sModelId = sModel;
    tProfile.tModels.tText.tCaps.uFlags =
        XLLM_CAP_TEXT_IN |
        XLLM_CAP_TEXT_OUT |
        XLLM_CAP_JSON_OUT |
        XLLM_CAP_TOOL_CALL_OUT |
        XLLM_CAP_THINKING_FULL_OUT |
        XLLM_CAP_REASONING_CONTROL;

    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "failed to register profile\n");
        xllm_runtime_destroy(pRuntime);
        return 6;
    }

    xllm_session_options_init(&tSessionOptions);
    tSessionOptions.sProfileId = "doubao-native";
    tSessionOptions.sSystemPrompt = "You are a concise assistant.";
    tSessionOptions.bEnableAutoCompact = true;
    tSessionOptions.uCompactTriggerTurns = 12u;

    if ( xllm_session_create(pRuntime, &tSessionOptions, &pSession) != XRT_NET_OK || !pSession ) {
        fprintf(stderr, "failed to create session\n");
        xllm_runtime_destroy(pRuntime);
        return 7;
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
    printf("[doubao] ");
    iStatus = xllm_session_chat_ex(pSession, &tTurn, &tCallOptions, &pResponse, &tError);
    printf("\n");
    if ( iStatus != XRT_NET_OK || !pResponse ) {
        demo_print_error("doubao session turn 1 failed", iStatus, &tError);
        xllm_turn_reset(&tTurn);
        xllm_error_free(&tError);
        xllm_session_destroy(pSession);
        xllm_runtime_destroy(pRuntime);
        return 8;
    }
    xllm_response_free(pResponse);
    pResponse = NULL;
    xllm_turn_reset(&tTurn);

    printf("\n--- turn 2 ---\n");
    xllm_turn_init(&tTurn);
    xllm_turn_add_user_text(&tTurn, "What is my project name?");
    printf("[user] What is my project name?\n");
    printf("[doubao] ");
    iStatus = xllm_session_chat_ex(pSession, &tTurn, &tCallOptions, &pResponse, &tError);
    printf("\n");
    if ( iStatus != XRT_NET_OK || !pResponse ) {
        demo_print_error("doubao session turn 2 failed", iStatus, &tError);
        xllm_turn_reset(&tTurn);
        xllm_error_free(&tError);
        xllm_session_destroy(pSession);
        xllm_runtime_destroy(pRuntime);
        return 9;
    }

    printf("\nvisible text: %s\n", xllm_response_get_text(pResponse));

    xllm_response_free(pResponse);
    xllm_turn_reset(&tTurn);
    xllm_error_free(&tError);
    xllm_session_destroy(pSession);
    xllm_runtime_destroy(pRuntime);
    return 0;
}
