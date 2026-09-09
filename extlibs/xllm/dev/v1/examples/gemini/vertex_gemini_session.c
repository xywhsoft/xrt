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

static char *build_vertex_base_url(const char *sProjectId, const char *sLocation)
{
    const char *sFormat =
        "https://%s-aiplatform.googleapis.com/v1/projects/%s/locations/%s/publishers/google/models";
    int iLen;
    char *sUrl;

    if ( !sProjectId || !sProjectId[0] || !sLocation || !sLocation[0] ) {
        return NULL;
    }

    iLen = snprintf(NULL, 0, sFormat, sLocation, sProjectId, sLocation);
    if ( iLen <= 0 ) {
        return NULL;
    }

    sUrl = (char *)xrtCalloc((size_t)iLen + 1u, sizeof(char));
    if ( !sUrl ) {
        return NULL;
    }
    snprintf(sUrl, (size_t)iLen + 1u, sFormat, sLocation, sProjectId, sLocation);
    return sUrl;
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
    const char *sProjectId = getenv("VERTEX_GEMINI_PROJECT_ID");
    const char *sLocation = demo_env("VERTEX_GEMINI_LOCATION", "us-central1");
    const char *sModel = demo_env("VERTEX_GEMINI_MODEL", "gemini-2.5-flash");
    const char *sApiKey = getenv("VERTEX_GEMINI_API_KEY");
    const char *sCredentialsPath = getenv("VERTEX_GEMINI_CREDENTIALS_PATH");
    const char *sCredentialsJson = getenv("VERTEX_GEMINI_CREDENTIALS_JSON");
    const char *sUseAdc = getenv("VERTEX_GEMINI_USE_ADC");
    const char *sAuthLabel =
        (sApiKey && sApiKey[0]) ? "api_key" :
        ((sCredentialsJson && sCredentialsJson[0]) ? "credentials_json" :
         ((sCredentialsPath && sCredentialsPath[0]) ? "credentials_path" :
          ((sUseAdc && sUseAdc[0] && strcmp(sUseAdc, "0") != 0) ? "adc_env" : "missing")));
    const char *sBaseUrlEnv = getenv("VERTEX_GEMINI_BASE_URL");
    char *sOwnedBaseUrl = NULL;
    const char *sBaseUrl = sBaseUrlEnv;
    xvalue tVendorExtra = NULL;
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

    if ( !sBaseUrl || !sBaseUrl[0] ) {
        sOwnedBaseUrl = build_vertex_base_url(sProjectId, sLocation);
        sBaseUrl = sOwnedBaseUrl;
    }

    printf("=== Vertex Gemini Session Example ===\n");
    printf("base url : %s\n", sBaseUrl ? sBaseUrl : "(null)");
    printf("model    : %s\n", sModel);
    printf("auth     : %s\n\n", sAuthLabel);

    if ( !sBaseUrl || !sBaseUrl[0] ) {
        fprintf(stderr, "set VERTEX_GEMINI_BASE_URL or VERTEX_GEMINI_PROJECT_ID\n");
        xrtFree(sOwnedBaseUrl);
        return 2;
    }
    if ( (!sApiKey || !sApiKey[0]) &&
         (!(sCredentialsPath && sCredentialsPath[0])) &&
         (!(sCredentialsJson && sCredentialsJson[0])) &&
         (!(sUseAdc && sUseAdc[0] && strcmp(sUseAdc, "0") != 0)) ) {
        fprintf(stderr, "set VERTEX_GEMINI_API_KEY or VERTEX_GEMINI_CREDENTIALS_PATH or VERTEX_GEMINI_CREDENTIALS_JSON (or VERTEX_GEMINI_USE_ADC=1)\n");
        xrtFree(sOwnedBaseUrl);
        return 3;
    }

    if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || !pRuntime ) {
        fprintf(stderr, "failed to create runtime\n");
        xrtFree(sOwnedBaseUrl);
        return 4;
    }
    if ( xllm_register_vertex_gemini_native_adapter(pRuntime) != XRT_NET_OK ) {
        fprintf(stderr, "failed to register vertex gemini native adapter\n");
        xllm_runtime_destroy(pRuntime);
        xrtFree(sOwnedBaseUrl);
        return 5;
    }

    xllm_profile_init(&tProfile);
    tProfile.sId = "vertex-gemini-native";
    tProfile.sProvider = "google_vertex";
    tProfile.sAdapter = XLLM_ADAPTER_VERTEX_GEMINI_NATIVE;
    tProfile.sBaseUrl = sBaseUrl;
    if ( sApiKey && sApiKey[0] ) {
        tProfile.tAuth.eKind = XLLM_AUTH_API_KEY_HEADER;
        tProfile.tAuth.sHeaderName = "x-goog-api-key";
        tProfile.tAuth.sSecret = sApiKey;
    } else {
        tVendorExtra = xvoCreateTable();
        if ( !tVendorExtra ||
             ((sCredentialsPath && sCredentialsPath[0]) &&
              !xvoTableSetText(tVendorExtra, "vertex_credentials_path", 0u, (ptr)sCredentialsPath, 0u, TRUE)) ||
             ((sCredentialsJson && sCredentialsJson[0]) &&
              !xvoTableSetText(tVendorExtra, "vertex_credentials_json", 0u, (ptr)sCredentialsJson, 0u, TRUE)) ) {
            fprintf(stderr, "failed to create vertex credentials config\n");
            xllm_runtime_destroy(pRuntime);
            if ( tVendorExtra ) {
                xvoUnref(tVendorExtra);
            }
            xrtFree(sOwnedBaseUrl);
            return 6;
        }
        tProfile.tAuth.eKind = XLLM_AUTH_NONE;
        tProfile.tProviderOptions.tVendorExtra = tVendorExtra;
        tVendorExtra = NULL;
    }
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
        xrtFree(sOwnedBaseUrl);
        return 7;
    }

    xllm_session_options_init(&tSessionOptions);
    tSessionOptions.sProfileId = "vertex-gemini-native";
    tSessionOptions.sSystemPrompt = "You are a concise assistant.";
    tSessionOptions.bEnableAutoCompact = true;
    tSessionOptions.uCompactTriggerTurns = 12u;

    if ( xllm_session_create(pRuntime, &tSessionOptions, &pSession) != XRT_NET_OK || !pSession ) {
        fprintf(stderr, "failed to create session\n");
        xllm_runtime_destroy(pRuntime);
        xrtFree(sOwnedBaseUrl);
        return 8;
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
    printf("[vertex-gemini] ");
    iStatus = xllm_session_chat_ex(pSession, &tTurn, &tCallOptions, &pResponse, &tError);
    printf("\n");
    if ( iStatus != XRT_NET_OK || !pResponse ) {
        demo_print_error("vertex gemini session turn 1 failed", iStatus, &tError);
        xllm_turn_reset(&tTurn);
        xllm_error_free(&tError);
        xllm_session_destroy(pSession);
        xllm_runtime_destroy(pRuntime);
        xrtFree(sOwnedBaseUrl);
        return 9;
    }
    xllm_response_free(pResponse);
    pResponse = NULL;
    xllm_turn_reset(&tTurn);

    printf("\n--- turn 2 ---\n");
    xllm_turn_init(&tTurn);
    xllm_turn_add_user_text(&tTurn, "What is my project name?");
    printf("[user] What is my project name?\n");
    printf("[vertex-gemini] ");
    iStatus = xllm_session_chat_ex(pSession, &tTurn, &tCallOptions, &pResponse, &tError);
    printf("\n");
    if ( iStatus != XRT_NET_OK || !pResponse ) {
        demo_print_error("vertex gemini session turn 2 failed", iStatus, &tError);
        xllm_turn_reset(&tTurn);
        xllm_error_free(&tError);
        xllm_session_destroy(pSession);
        xllm_runtime_destroy(pRuntime);
        xrtFree(sOwnedBaseUrl);
        return 10;
    }

    printf("\nvisible text: %s\n", xllm_response_get_text(pResponse));

    xllm_response_free(pResponse);
    xllm_turn_reset(&tTurn);
    xllm_error_free(&tError);
    xllm_session_destroy(pSession);
    xllm_runtime_destroy(pRuntime);
    xrtFree(sOwnedBaseUrl);
    return 0;
}
