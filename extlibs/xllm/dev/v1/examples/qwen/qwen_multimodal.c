#include "xllm-session.h"

#include <stdbool.h>
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

static int demo_add_optional_multimodal_inputs(xllm_turn *pTurn)
{
    const char *sImageUrl = getenv("QWEN_IMAGE_URL");
    const char *sImagePath = getenv("QWEN_IMAGE_PATH");
    const char *sImageMime = demo_env("QWEN_IMAGE_MIME", "image/png");
    int iStatus;

    if ( sImageUrl && sImageUrl[0] ) {
        iStatus = xllm_turn_add_image_url(pTurn, sImageUrl, sImageMime);
        if ( iStatus != XRT_NET_OK ) return iStatus;
    }
    if ( sImagePath && sImagePath[0] ) {
        iStatus = xllm_turn_add_image_file(pTurn, sImagePath, sImageMime);
        if ( iStatus != XRT_NET_OK ) return iStatus;
    }

    return XRT_NET_OK;
}

int main(void)
{
    const char *sApiKey = getenv("DASHSCOPE_API_KEY");
    const char *sBaseUrl = demo_env("QWEN_BASE_URL", "https://dashscope.aliyuncs.com/api/v1/services/aigc/text-generation/generation");
    const char *sModel = demo_env("QWEN_MODEL", "qwen-vl-plus-latest");
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

    printf("=== Qwen Native Multimodal Example ===\n");
    printf("base url : %s\n", sBaseUrl);
    printf("model    : %s\n\n", sModel);
    printf("Optional envs:\n");
    printf("  DASHSCOPE_API_KEY\n");
    printf("  QWEN_IMAGE_URL / QWEN_IMAGE_PATH\n");
    printf("  QWEN_TIMEOUT_MS\n");
    printf("  QWEN_DEBUG=1\n");
    printf("  QWEN_MULTIMODAL_STREAM=1\n\n");

    if ( !sApiKey || !sApiKey[0] ) {
        fprintf(stderr, "set DASHSCOPE_API_KEY first\n");
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
    if ( demo_env_enabled("QWEN_DEBUG") ) {
        (void)xllm_runtime_set_log_callback(pRuntime, demo_log_callback, stderr);
        (void)xllm_runtime_set_trace_callback(pRuntime, demo_trace_callback, stderr);
        (void)xllm_runtime_set_debug_mode(pRuntime, XLLM_DEBUG_BODY, XLLM_REDACT_DEFAULT);
    }
    if ( xllm_register_qwen_native_adapter(pRuntime) != XRT_NET_OK ) {
        fprintf(stderr, "failed to register qwen native adapter\n");
        xllm_runtime_destroy(pRuntime);
        return 4;
    }

    tProfile.sId = "qwen-native";
    tProfile.sProvider = "alibaba";
    tProfile.sAdapter = XLLM_ADAPTER_QWEN_NATIVE;
    tProfile.sBaseUrl = sBaseUrl;
    tProfile.tAuth.eKind = XLLM_AUTH_BEARER;
    tProfile.tAuth.sSecret = sApiKey;
    tProfile.tModels.tText.sModelId = sModel;
    tProfile.tModels.tText.tCaps.uFlags = XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT;
    tProfile.tModels.tMultimodal.sModelId = sModel;
    tProfile.tModels.tMultimodal.tCaps.uFlags =
        XLLM_CAP_TEXT_IN |
        XLLM_CAP_IMAGE_IN |
        XLLM_CAP_TEXT_OUT;

    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "failed to register profile\n");
        xllm_runtime_destroy(pRuntime);
        return 5;
    }

    pLlm = xllm_create(pRuntime, NULL);
    if ( !pLlm || xllm_bind_profile(pLlm, "qwen-native") != XRT_NET_OK ) {
        fprintf(stderr, "failed to create llm or bind profile\n");
        xllm_destroy(pLlm);
        xllm_runtime_destroy(pRuntime);
        return 6;
    }

    xllm_set_system_prompt(pLlm, "You are a concise multimodal assistant.");
    xllm_turn_add_user_text(&tTurn, "Please analyze any provided image or file and briefly say what you can infer.");

    if ( getenv("QWEN_IMAGE_URL") && getenv("QWEN_IMAGE_URL")[0] ) bHasInput = true;
    if ( getenv("QWEN_IMAGE_PATH") && getenv("QWEN_IMAGE_PATH")[0] ) bHasInput = true;

    if ( demo_add_optional_multimodal_inputs(&tTurn) != XRT_NET_OK ) {
        fprintf(stderr, "failed to add multimodal inputs\n");
        xllm_destroy(pLlm);
        xllm_error_free(&tError);
        xllm_turn_reset(&tTurn);
        xllm_runtime_destroy(pRuntime);
        return 7;
    }

    if ( !bHasInput ) {
        printf("[assistant] No image/file input configured; set one of the optional QWEN_* multimodal env vars.\n");
        xllm_destroy(pLlm);
        xllm_error_free(&tError);
        xllm_turn_reset(&tTurn);
        xllm_runtime_destroy(pRuntime);
        return 0;
    }

    tCallOpts.uTimeoutMs = demo_env_u32("QWEN_TIMEOUT_MS", 180000u);
    if ( demo_env_enabled("QWEN_MULTIMODAL_STREAM") ) {
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
        xllm_error_free(&tError);
        xllm_turn_reset(&tTurn);
        xllm_runtime_destroy(pRuntime);
        return 8;
    }

    printf("[assistant] %s\n\n", xllm_response_get_text(pResponse));
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
