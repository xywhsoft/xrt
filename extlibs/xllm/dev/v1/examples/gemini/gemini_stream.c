#include "xllm-session.h"

#include <stdbool.h>
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
    const char *sApiKey = getenv("GEMINI_API_KEY");
    const char *sBaseUrl = demo_env("GEMINI_BASE_URL", "https://generativelanguage.googleapis.com/v1beta/models");
    const char *sModel = demo_env("GEMINI_MODEL", "gemini-2.5-flash");
    xllm_runtime *pRuntime = NULL;
    xllm *pLlm = NULL;
    xllm_response *pResponse = NULL;
    xllm_profile tProfile;
    xllm_turn tTurn;
    xllm_call_options tCallOpts;
    xllm_error tError;
    bool bStarted = false;
    int iStatus;

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    xrtInit();

    printf("=== Gemini Native Stream Example ===\n");
    printf("base url : %s\n", sBaseUrl);
    printf("model    : %s\n\n", sModel);

    if ( !sApiKey || !sApiKey[0] ) {
        fprintf(stderr, "set GEMINI_API_KEY first\n");
        return 2;
    }

    xllm_profile_init(&tProfile);
    xllm_turn_init(&tTurn);
    xllm_call_options_init(&tCallOpts);
    xllm_error_init(&tError);

    if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || !pRuntime ) {
        fprintf(stderr, "failed to create runtime\n");
        return 3;
    }
    if ( xllm_register_gemini_native_adapter(pRuntime) != XRT_NET_OK ) {
        fprintf(stderr, "failed to register gemini native adapter\n");
        xllm_runtime_destroy(pRuntime);
        return 4;
    }

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

    pLlm = xllm_create(pRuntime, NULL);
    if ( !pLlm || xllm_bind_profile(pLlm, "gemini-native") != XRT_NET_OK ) {
        fprintf(stderr, "failed to create llm or bind profile\n");
        xllm_destroy(pLlm);
        xllm_runtime_destroy(pRuntime);
        return 6;
    }

    xllm_set_system_prompt(pLlm, "You are a concise assistant.");
    xllm_turn_add_user_text(&tTurn, "Return exactly three short lines confirming Gemini streaming works.");

    tCallOpts.eStreamMode = XLLM_STREAM_REQUIRE;
    tCallOpts.uTimeoutMs = 120000u;
    tCallOpts.pfnOnEvent = demo_on_event;
    tCallOpts.pUserData = &bStarted;

    iStatus = xllm_send_ex(pLlm, &tTurn, &tCallOpts, &pResponse, &tError);
    if ( iStatus != XRT_NET_OK || !pResponse ) {
        fprintf(
            stderr,
            "\nrequest failed (status=%d, code=%d, http=%d, msg=%s)\n",
            iStatus,
            (int)tError.eCode,
            (int)tError.iHttpStatus,
            tError.sMessage ? tError.sMessage : "(null)"
        );
        xllm_destroy(pLlm);
        xllm_error_free(&tError);
        xllm_turn_reset(&tTurn);
        xllm_runtime_destroy(pRuntime);
        return 7;
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
