#include "azure_openai_config.h"
#include "xllm-session.h"

#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

static bool demo_on_event(const xllm_event *pEvent, void *pUserData)
{
    (void)pUserData;

    if ( pEvent == NULL ) {
        return true;
    }

    if ( pEvent->eType == XLLM_EVENT_TEXT_DELTA ) {
        printf("%s", pEvent->as.tTextDelta.sText ? pEvent->as.tTextDelta.sText : "");
        fflush(stdout);
    }

    return true;
}

static void demo_print_error(const char *sPrefix, int iStatus, const xllm_error *pError)
{
    fprintf(stderr, "%s (status=%d", sPrefix, iStatus);
    if ( pError != NULL ) {
        fprintf(stderr,
                ", code=%d, http=%d, msg=%s",
                (int)pError->eCode,
                (int)pError->iHttpStatus,
                pError->sMessage ? pError->sMessage : "(null)");
        if ( pError->sRequestId != NULL && pError->sRequestId[0] != '\0' ) {
            fprintf(stderr, ", request_id=%s", pError->sRequestId);
        }
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
    char aBaseUrl[1024];
    const char *sApiKey;
    int iStatus;

#if defined(_WIN32) || defined(_WIN64)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    xrtInit();

    if ( demo_build_azure_openai_chat_url(aBaseUrl, sizeof(aBaseUrl)) != 0 ) {
        fprintf(stderr, "failed to build Azure OpenAI chat URL\n");
        return 1;
    }

    printf("=== Azure OpenAI Session Example ===\n");
    printf("endpoint   : %s\n", AZURE_OPENAI_ENDPOINT);
    printf("model name : %s\n", AZURE_OPENAI_MODEL_NAME);
    printf("deployment : %s\n", AZURE_OPENAI_DEPLOYMENT);
    printf("api ver    : %s\n", AZURE_OPENAI_API_VERSION);
    printf("base url   : %s\n\n", aBaseUrl);

    sApiKey = demo_get_azure_openai_api_key();
    if ( sApiKey == NULL || sApiKey[0] == '\0' ) {
        fprintf(stderr, "missing environment variable: AZURE_OPENAI_API_KEY\n");
        return 2;
    }

    if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || pRuntime == NULL ) {
        fprintf(stderr, "failed to create runtime\n");
        return 3;
    }

    if ( xllm_register_openai_compat_adapter(pRuntime) != XRT_NET_OK ) {
        fprintf(stderr, "failed to register openai-compatible adapter\n");
        xllm_runtime_destroy(pRuntime);
        return 4;
    }

    xllm_profile_init(&tProfile);
    tProfile.sId = "azure-openai";
    tProfile.sProvider = "azure-openai";
    tProfile.sAdapter = XLLM_ADAPTER_OPENAI_COMPAT;
    tProfile.sBaseUrl = aBaseUrl;
    tProfile.tAuth.eKind = XLLM_AUTH_API_KEY_HEADER;
    tProfile.tAuth.sHeaderName = "api-key";
    tProfile.tAuth.sSecret = sApiKey;
    tProfile.tModels.tText.sModelId = AZURE_OPENAI_DEPLOYMENT;
    tProfile.tModels.tText.tCaps.uFlags =
        XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT | XLLM_CAP_STREAM;

    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "failed to register profile\n");
        xllm_runtime_destroy(pRuntime);
        return 5;
    }

    xllm_session_options_init(&tSessionOptions);
    tSessionOptions.sProfileId = "azure-openai";
    tSessionOptions.sSystemPrompt = AZURE_OPENAI_SYSTEM_PROMPT;
    tSessionOptions.bEnableAutoCompact = true;
    tSessionOptions.uCompactTriggerTurns = 12u;

    if ( xllm_session_create(pRuntime, &tSessionOptions, &pSession) != XRT_NET_OK || pSession == NULL ) {
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
    printf("[assistant] ");
    iStatus = xllm_session_chat_ex(pSession, &tTurn, &tCallOptions, &pResponse, &tError);
    printf("\n");
    if ( iStatus != XRT_NET_OK || pResponse == NULL ) {
        demo_print_error("Azure OpenAI session turn 1 failed", iStatus, &tError);
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
    printf("[assistant] ");
    iStatus = xllm_session_chat_ex(pSession, &tTurn, &tCallOptions, &pResponse, &tError);
    printf("\n");
    if ( iStatus != XRT_NET_OK || pResponse == NULL ) {
        demo_print_error("Azure OpenAI session turn 2 failed", iStatus, &tError);
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
