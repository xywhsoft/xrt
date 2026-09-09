#define KIMI_API_KEY getenv("KIMI_API_KEY")
#define KIMI_BASE_URL "https://api.moonshot.cn/v1"
#define KIMI_MODEL    "kimi-k2.5"

#include "xllm-session.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

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

static xllm_tool_choice_mode demo_tool_choice_mode(const char *sMode)
{
    if ( !sMode || !sMode[0] ) {
        return XLLM_TOOL_CHOICE_AUTO;
    }
    if ( strcmp(sMode, "none") == 0 ) {
        return XLLM_TOOL_CHOICE_NONE;
    }
    if ( strcmp(sMode, "required") == 0 ) {
        return XLLM_TOOL_CHOICE_REQUIRED;
    }
    if ( strcmp(sMode, "named") == 0 ) {
        return XLLM_TOOL_CHOICE_NAMED;
    }
    return XLLM_TOOL_CHOICE_AUTO;
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

    (void)pRequest;
    (void)pError;

    if ( !pResult ) {
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
    const char *sToolResult = "tool-result-from-local-executor";
    const char *sToolChoice = getenv("KIMI_TOOL_CHOICE");
    const char *sToolChoiceName = getenv("KIMI_TOOL_CHOICE_NAME");
    const char *sModel = getenv("KIMI_MODEL");
    const char *sReasoningOff = getenv("KIMI_REASONING_OFF");
    xllm_tool_choice_mode eToolChoice = demo_tool_choice_mode(sToolChoice);
    bool bReasoningOff = (sReasoningOff && sReasoningOff[0] && strcmp(sReasoningOff, "0") != 0);
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

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    xrtInit();

    printf("=== Kimi Native Tool Loop Example ===\n\n");
    printf("model       : %s\n", (sModel && sModel[0]) ? sModel : KIMI_MODEL);
    printf("tool choice : %s\n", sToolChoice && sToolChoice[0] ? sToolChoice : "auto");
    printf("reasoning   : %s\n", bReasoningOff ? "off" : "default");
    if ( eToolChoice == XLLM_TOOL_CHOICE_NAMED ) {
        printf("tool name   : %s\n", (sToolChoiceName && sToolChoiceName[0]) ? sToolChoiceName : "get_weather");
    }
    printf("\n");

    xllm_error_init(&tError);
    xllm_profile_init(&tProfile);
    xllm_turn_init(&tTurn);
    xllm_call_options_init(&tCallOpts);
    memset(&tTool, 0, sizeof(tTool));
    memset(&tExecutor, 0, sizeof(tExecutor));

    if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || !pRuntime ) {
        fprintf(stderr, "failed to create runtime\n");
        return 1;
    }
    if ( xllm_register_kimi_native_adapter(pRuntime) != XRT_NET_OK ) {
        fprintf(stderr, "failed to register kimi native adapter\n");
        xllm_runtime_destroy(pRuntime);
        return 2;
    }

    tProfile.sId = "kimi-native";
    tProfile.sProvider = "moonshot";
    tProfile.sAdapter = XLLM_ADAPTER_KIMI_NATIVE;
    tProfile.sBaseUrl = KIMI_BASE_URL;
    tProfile.tAuth.eKind = XLLM_AUTH_BEARER;
    tProfile.tAuth.sSecret = KIMI_API_KEY;
    tProfile.tModels.tText.sModelId = (sModel && sModel[0]) ? sModel : KIMI_MODEL;
    tProfile.tModels.tText.tCaps.uFlags =
        XLLM_CAP_TEXT_IN |
        XLLM_CAP_TEXT_OUT |
        XLLM_CAP_TOOL_CALL_OUT |
        XLLM_CAP_TOOL_RESULT_IN;

    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "failed to register profile\n");
        xllm_runtime_destroy(pRuntime);
        return 3;
    }

    pLlm = xllm_create(pRuntime, NULL);
    if ( !pLlm || xllm_bind_profile(pLlm, "kimi-native") != XRT_NET_OK ) {
        fprintf(stderr, "failed to create llm or bind profile\n");
        xllm_destroy(pLlm);
        xllm_runtime_destroy(pRuntime);
        return 4;
    }

    xllm_set_system_prompt(pLlm, "You are a concise assistant.");
    xllm_turn_add_user_text(&tTurn, "Call the weather tool once, then summarize the returned value in one short paragraph.");
    if ( bReasoningOff ) {
        tTurn.tReasoning.tEnabled.bSet = true;
        tTurn.tReasoning.tEnabled.bValue = false;
        tTurn.tReasoning.eLevel = XLLM_REASONING_OFF;
    }

    tTool.sToolId = "app.weather.get_current";
    tTool.sWireName = "get_weather";
    tTool.sDescription = "Get current weather";
    if ( xllm_turn_add_tool(&tTurn, &tTool) != XRT_NET_OK ) {
        fprintf(stderr, "failed to add tool\n");
        xllm_destroy(pLlm);
        xllm_runtime_destroy(pRuntime);
        return 5;
    }
    if ( xllm_turn_set_tool_choice(
            &tTurn,
            eToolChoice,
            (eToolChoice == XLLM_TOOL_CHOICE_NAMED) ?
                ((sToolChoiceName && sToolChoiceName[0]) ? sToolChoiceName : "get_weather") :
                NULL,
            false
        ) != XRT_NET_OK ) {
        fprintf(stderr, "failed to set tool choice\n");
        xllm_destroy(pLlm);
        xllm_runtime_destroy(pRuntime);
        return 6;
    }

    tExecutor.pCtx = (void *)sToolResult;
    tExecutor.pfnExecute = demo_tool_execute;
    if ( xllm_set_tool_executor(pLlm, &tExecutor) != XRT_NET_OK ) {
        fprintf(stderr, "failed to set tool executor\n");
        xllm_destroy(pLlm);
        xllm_runtime_destroy(pRuntime);
        return 7;
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
