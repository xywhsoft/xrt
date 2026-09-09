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

static char *demo_dup(const char *sText)
{
    size_t iLen = sText ? strlen(sText) : 0u;
    char *sRet = (char *)xrtCalloc(iLen + 1u, sizeof(char));

    if ( !sRet ) {
        return NULL;
    }
    if ( iLen > 0u ) {
        memcpy(sRet, sText, iLen);
    }
    sRet[iLen] = '\0';
    return sRet;
}

static int32 demo_tool_execute(
    void *pCtx,
    const xllm_tool_exec_request *pRequest,
    xllm_tool_exec_result *pResult,
    xllm_error *pError
)
{
    const char *sResultText = (const char *)pCtx;
    xllm_content_part *pPart = NULL;

    (void)pError;

    if ( !pRequest || !pResult ) {
        return XRT_NET_ERROR;
    }

    pPart = (xllm_content_part *)xrtCalloc(1u, sizeof(*pPart));
    if ( !pPart ) {
        return XRT_NET_ERROR;
    }

    pPart[0].eKind = XLLM_PART_TEXT;
    pPart[0].as.tSource.eKind = XLLM_SOURCE_INLINE_TEXT;
    pPart[0].as.tSource.sMimeType = demo_dup("text/plain");
    pPart[0].as.tSource.as.sText = demo_dup(sResultText ? sResultText : "tool-result-from-local-executor");

    pResult->pParts = pPart;
    pResult->iPartCount = 1u;
    return XRT_NET_OK;
}

int main(void)
{
    const char *sApiKey = getenv("GEMINI_API_KEY");
    const char *sBaseUrl = demo_env("GEMINI_BASE_URL", "https://generativelanguage.googleapis.com/v1beta/models");
    const char *sModel = demo_env("GEMINI_MODEL", "gemini-2.5-flash");
    const char *sToolResult = demo_env("GEMINI_TOOL_RESULT_TEXT", "tool-result-from-local-executor");
    xllm_runtime *pRuntime = NULL;
    xllm *pLlm = NULL;
    xllm_response *pResponse = NULL;
    xllm_profile tProfile;
    xllm_turn tTurn;
    xllm_call_options tCallOpts;
    xllm_tool_def tTool;
    xllm_tool_executor tExecutor;
    xllm_error tError;
    int iStatus;

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    xrtInit();

    printf("=== Gemini Native Tool Loop Example ===\n");
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
    memset(&tTool, 0, sizeof(tTool));
    memset(&tExecutor, 0, sizeof(tExecutor));

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
        XLLM_CAP_TEXT_OUT |
        XLLM_CAP_JSON_OUT |
        XLLM_CAP_TOOL_CALL_OUT |
        XLLM_CAP_TOOL_RESULT_IN;

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
    xllm_turn_add_user_text(&tTurn, "Call the weather tool once, then summarize the returned value in one short paragraph.");

    tTool.sToolId = "app.weather.get_current";
    tTool.sWireName = "get_weather";
    tTool.sDescription = "Get current weather";
    if ( xllm_turn_add_tool(&tTurn, &tTool) != XRT_NET_OK ) {
        fprintf(stderr, "failed to add tool\n");
        xllm_destroy(pLlm);
        xllm_runtime_destroy(pRuntime);
        return 7;
    }

    tExecutor.pCtx = (void *)sToolResult;
    tExecutor.pfnExecute = demo_tool_execute;
    if ( xllm_set_tool_executor(pLlm, &tExecutor) != XRT_NET_OK ) {
        fprintf(stderr, "failed to set tool executor\n");
        xllm_destroy(pLlm);
        xllm_runtime_destroy(pRuntime);
        return 8;
    }

    tCallOpts.uTimeoutMs = 120000u;

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
        return 9;
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
