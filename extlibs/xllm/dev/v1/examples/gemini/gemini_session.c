#include "xllm-session.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

static const char *demo_env(const char *sName, const char *sDefault)
{
    const char *sValue = getenv(sName);
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
    const char *sApiKey = getenv("GEMINI_API_KEY");
    const char *sBaseUrl = demo_env("GEMINI_BASE_URL", "https://generativelanguage.googleapis.com/v1beta/models");
    const char *sModel = demo_env("GEMINI_MODEL", "gemini-2.5-flash");
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

    printf("=== Gemini Native Session Example ===\n");
    printf("base url : %s\n", sBaseUrl);
    printf("model    : %s\n\n", sModel);

    if ( !sApiKey || !sApiKey[0] ) {
        fprintf(stderr, "set GEMINI_API_KEY first\n");
        return 2;
    }

    if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || !pRuntime ) {
        fprintf(stderr, "failed to create runtime\n");
        return 3;
    }
    if ( xllm_register_gemini_native_adapter(pRuntime) != XRT_NET_OK ) {
        fprintf(stderr, "failed to register gemini native adapter\n");
        xllm_runtime_destroy(pRuntime);
        return 4;
    }

    xllm_profile_init(&tProfile);
    tProfile.sId = "gemini-native";
    tProfile.sProvider = "google";
    tProfile.sAdapter = XLLM_ADAPTER_GEMINI_NATIVE;
    tProfile.sBaseUrl = sBaseUrl;
    tProfile.tAuth.eKind = XLLM_AUTH_API_KEY_HEADER;
    tProfile.tAuth.sHeaderName = "x-goog-api-key";
    tProfile.tAuth.sSecret = sApiKey;
    tProfile.tModels.tText.sModelId = sModel;
    tProfile.tModels.tText.tCaps.uFlags =
        XLLM_CAP_TEXT_IN |
        XLLM_CAP_IMAGE_IN |
        XLLM_CAP_FILE_IN |
        XLLM_CAP_TEXT_OUT |
        XLLM_CAP_JSON_OUT |
        XLLM_CAP_TOOL_CALL_OUT;

    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "failed to register profile\n");
        xllm_runtime_destroy(pRuntime);
        return 5;
    }

    xllm_session_options_init(&tSessionOptions);
    tSessionOptions.sProfileId = "gemini-native";
    tSessionOptions.sSystemPrompt = "You are a concise assistant.";
    tSessionOptions.bEnableAutoCompact = true;
    tSessionOptions.uCompactTriggerTurns = 12u;

    if ( xllm_session_create(pRuntime, &tSessionOptions, &pSession) != XRT_NET_OK || !pSession ) {
        fprintf(stderr, "failed to create session\n");
        xllm_runtime_destroy(pRuntime);
        return 6;
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
    printf("[gemini] ");
    iStatus = xllm_session_chat_ex(pSession, &tTurn, &tCallOptions, &pResponse, &tError);
    printf("\n");
    if ( iStatus != XRT_NET_OK || !pResponse ) {
        demo_print_error("gemini session turn 1 failed", iStatus, &tError);
        xllm_turn_reset(&tTurn);
        xllm_error_free(&tError);
        xllm_session_destroy(pSession);
        xllm_runtime_destroy(pRuntime);
        return 7;
    }
    xllm_response_free(pResponse);
    pResponse = NULL;
    xllm_turn_reset(&tTurn);

    printf("\n--- turn 2 ---\n");
    xllm_turn_init(&tTurn);
    xllm_turn_add_user_text(&tTurn, "What is my project name?");
    printf("[user] What is my project name?\n");
    printf("[gemini] ");
    iStatus = xllm_session_chat_ex(pSession, &tTurn, &tCallOptions, &pResponse, &tError);
    printf("\n");
    if ( iStatus != XRT_NET_OK || !pResponse ) {
        demo_print_error("gemini session turn 2 failed", iStatus, &tError);
        xllm_turn_reset(&tTurn);
        xllm_error_free(&tError);
        xllm_session_destroy(pSession);
        xllm_runtime_destroy(pRuntime);
        return 8;
    }

    printf("\nvisible text: %s\n", xllm_response_get_text(pResponse));

    xllm_response_free(pResponse);
    xllm_turn_reset(&tTurn);
    xllm_error_free(&tError);
    xllm_session_destroy(pSession);
    xllm_runtime_destroy(pRuntime);
    return 0;
}
