#include "xllm-session.h"

#include <errno.h>
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

static bool demo_env_enabled(const char *sName)
{
    const char *sValue = getenv(sName);

    if ( !sValue || !sValue[0] ) {
        return false;
    }
    return strcmp(sValue, "1") == 0 ||
           strcmp(sValue, "true") == 0 ||
           strcmp(sValue, "TRUE") == 0 ||
           strcmp(sValue, "on") == 0 ||
           strcmp(sValue, "ON") == 0;
}

static uint32 demo_env_u32(const char *sName, uint32 uDefaultValue)
{
    const char *sValue = getenv(sName);
    char *pEnd = NULL;
    unsigned long uParsed;

    if ( !sValue || !sValue[0] ) {
        return uDefaultValue;
    }

    errno = 0;
    uParsed = strtoul(sValue, &pEnd, 10);
    if ( errno != 0 || pEnd == sValue || (pEnd && *pEnd != '\0') ) {
        return uDefaultValue;
    }
    if ( uParsed == 0ul ) {
        return uDefaultValue;
    }
    if ( uParsed > 0xfffffffful ) {
        return 0xffffffffu;
    }
    return (uint32)uParsed;
}

static const char *demo_log_level_name(xllm_log_level eLevel)
{
    switch ( eLevel ) {
        case XLLM_LOG_ERROR: return "error";
        case XLLM_LOG_WARN: return "warn";
        case XLLM_LOG_INFO: return "info";
        case XLLM_LOG_DEBUG: return "debug";
        case XLLM_LOG_TRACE: return "trace";
        default: return "unknown";
    }
}

static void demo_log_callback(void *pCtx, xllm_log_level eLevel, const char *sComponent, const char *sMessage)
{
    FILE *pOut = (FILE *)pCtx;

    if ( !pOut ) {
        pOut = stderr;
    }
    fprintf(
        pOut,
        "[xllm:%s:%s] %s\n",
        demo_log_level_name(eLevel),
        sComponent ? sComponent : "(null)",
        sMessage ? sMessage : "(null)"
    );
    fflush(pOut);
}

static void demo_trace_callback(void *pCtx, xllm_trace_kind eKind, const xvalue *pPayload)
{
    FILE *pOut = (FILE *)pCtx;
    char *sJson = NULL;

    if ( !pOut ) {
        pOut = stderr;
    }
    if ( pPayload && *pPayload ) {
        sJson = (char *)xrtStringifyJSON(*pPayload, 0, NULL);
    }
    fprintf(pOut, "[xllm:trace:%d] %s\n", (int)eKind, sJson ? sJson : "(null)");
    fflush(pOut);
    if ( sJson ) {
        xrtFree(sJson);
    }
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
    const char *sImageUrl = getenv("VERTEX_GEMINI_IMAGE_URL");
    const char *sImagePath = getenv("VERTEX_GEMINI_IMAGE_PATH");
    const char *sImageFileId = getenv("VERTEX_GEMINI_IMAGE_FILE_ID");
    const char *sImageMime = demo_env("VERTEX_GEMINI_IMAGE_MIME", "image/png");
    const char *sFileUrl = getenv("VERTEX_GEMINI_FILE_URL");
    const char *sFilePath = getenv("VERTEX_GEMINI_FILE_PATH");
    const char *sFileFileId = getenv("VERTEX_GEMINI_FILE_FILE_ID");
    const char *sFileMime = demo_env("VERTEX_GEMINI_FILE_MIME", "application/pdf");
    char *sOwnedBaseUrl = NULL;
    const char *sBaseUrl = sBaseUrlEnv;
    xvalue tVendorExtra = NULL;
    xllm_runtime *pRuntime = NULL;
    xllm *pLlm = NULL;
    xllm_response *pResponse = NULL;
    xllm_profile tProfile;
    xllm_turn tTurn;
    xllm_call_options tCallOpts;
    xllm_error tError;
    bool bHasInput = false;
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

    printf("=== Vertex Gemini Multimodal Example ===\n");
    printf("base url : %s\n", sBaseUrl ? sBaseUrl : "(null)");
    printf("model    : %s\n", sModel);
    printf("auth     : %s\n\n", sAuthLabel);
    printf("Optional envs:\n");
    printf("  VERTEX_GEMINI_IMAGE_URL / VERTEX_GEMINI_IMAGE_PATH / VERTEX_GEMINI_IMAGE_FILE_ID\n");
    printf("  VERTEX_GEMINI_FILE_URL / VERTEX_GEMINI_FILE_PATH / VERTEX_GEMINI_FILE_FILE_ID\n");
    printf("  VERTEX_GEMINI_TIMEOUT_MS\n");
    printf("  VERTEX_GEMINI_DEBUG=1\n");
    printf("  VERTEX_GEMINI_MULTIMODAL_STREAM=1\n\n");

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

    xllm_profile_init(&tProfile);
    xllm_turn_init(&tTurn);
    xllm_call_options_init(&tCallOpts);
    xllm_error_init(&tError);

    if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || !pRuntime ) {
        fprintf(stderr, "failed to create runtime\n");
        xrtFree(sOwnedBaseUrl);
        return 4;
    }
    if ( demo_env_enabled("VERTEX_GEMINI_DEBUG") ) {
        (void)xllm_runtime_set_log_callback(pRuntime, demo_log_callback, stderr);
        (void)xllm_runtime_set_trace_callback(pRuntime, demo_trace_callback, stderr);
        (void)xllm_runtime_set_debug_mode(pRuntime, XLLM_DEBUG_BODY, XLLM_REDACT_DEFAULT);
    }
    if ( xllm_register_vertex_gemini_native_adapter(pRuntime) != XRT_NET_OK ) {
        fprintf(stderr, "failed to register vertex gemini native adapter\n");
        xllm_runtime_destroy(pRuntime);
        xrtFree(sOwnedBaseUrl);
        return 5;
    }

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
            xllm_error_free(&tError);
            xllm_turn_reset(&tTurn);
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
        XLLM_CAP_JSON_OUT;

    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "failed to register profile\n");
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        xllm_turn_reset(&tTurn);
        xrtFree(sOwnedBaseUrl);
        return 7;
    }

    pLlm = xllm_create(pRuntime, NULL);
    if ( !pLlm || xllm_bind_profile(pLlm, "vertex-gemini-native") != XRT_NET_OK ) {
        fprintf(stderr, "failed to create llm or bind profile\n");
        xllm_destroy(pLlm);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        xllm_turn_reset(&tTurn);
        xrtFree(sOwnedBaseUrl);
        return 8;
    }

    xllm_set_system_prompt(pLlm, "You are a concise multimodal assistant.");
    xllm_turn_add_user_text(&tTurn, "Please describe every provided image or file briefly and mention missing context if needed.");

    if ( sImageUrl && sImageUrl[0] ) {
        if ( xllm_turn_add_image_url(&tTurn, sImageUrl, sImageMime) != XRT_NET_OK ) {
            fprintf(stderr, "failed to add image url\n");
            return 9;
        }
        bHasInput = true;
    }
    if ( sImagePath && sImagePath[0] ) {
        if ( xllm_turn_add_image_file(&tTurn, sImagePath, sImageMime) != XRT_NET_OK ) {
            fprintf(stderr, "failed to add image path\n");
            return 10;
        }
        bHasInput = true;
    }
    if ( sImageFileId && sImageFileId[0] ) {
        if ( xllm_turn_add_image_file_id(&tTurn, sImageFileId, sImageMime) != XRT_NET_OK ) {
            fprintf(stderr, "failed to add image file id\n");
            return 11;
        }
        bHasInput = true;
    }
    if ( sFileUrl && sFileUrl[0] ) {
        if ( xllm_turn_add_file_url(&tTurn, sFileUrl, sFileMime) != XRT_NET_OK ) {
            fprintf(stderr, "failed to add file url\n");
            return 12;
        }
        bHasInput = true;
    }
    if ( sFilePath && sFilePath[0] ) {
        if ( xllm_turn_add_file(&tTurn, sFilePath, sFileMime) != XRT_NET_OK ) {
            fprintf(stderr, "failed to add file path\n");
            return 13;
        }
        bHasInput = true;
    }
    if ( sFileFileId && sFileFileId[0] ) {
        if ( xllm_turn_add_file_file_id(&tTurn, sFileFileId, sFileMime) != XRT_NET_OK ) {
            fprintf(stderr, "failed to add file file id\n");
            return 14;
        }
        bHasInput = true;
    }

    if ( !bHasInput ) {
        printf("[assistant] No image/file input configured; set one of the optional VERTEX_GEMINI_* input env vars.\n");
        xllm_destroy(pLlm);
        xllm_error_free(&tError);
        xllm_turn_reset(&tTurn);
        xllm_runtime_destroy(pRuntime);
        xrtFree(sOwnedBaseUrl);
        return 0;
    }

    tCallOpts.uTimeoutMs = demo_env_u32("VERTEX_GEMINI_TIMEOUT_MS", 120000u);
    if ( demo_env_enabled("VERTEX_GEMINI_MULTIMODAL_STREAM") ) {
        tCallOpts.eStreamMode = XLLM_STREAM_PREFER;
    }

    printf("timeout ms: %u\n\n", (unsigned)tCallOpts.uTimeoutMs);

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
        xllm_destroy(pLlm);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        xllm_turn_reset(&tTurn);
        xrtFree(sOwnedBaseUrl);
        return 15;
    }

    printf("[assistant] %s\n\n", xllm_response_get_text(pResponse));
    if ( pResponse->sModel ) {
        printf("response model: %s\n", pResponse->sModel);
    }

    xllm_response_free(pResponse);
    xllm_destroy(pLlm);
    xllm_runtime_destroy(pRuntime);
    xllm_error_free(&tError);
    xllm_turn_reset(&tTurn);
    xrtFree(sOwnedBaseUrl);
    return 0;
}
