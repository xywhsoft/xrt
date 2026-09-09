#define GLM_API_KEY getenv("GLM_API_KEY")
#define GLM_BASE_URL "https://open.bigmodel.cn/api/paas/v4"
#define GLM_MODEL    "glm-5-turbo"

#include "xllm-session.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

int main(void)
{
    static const char sSchemaText[] =
        "{\"type\":\"object\",\"properties\":{\"answer\":{\"type\":\"string\"},\"ok\":{\"type\":\"boolean\"}},"
        "\"required\":[\"answer\",\"ok\"],\"additionalProperties\":false}";
    xllm_runtime *pRuntime = NULL;
    xllm *pLlm = NULL;
    xllm_response *pResponse = NULL;
    xllm_profile tProfile;
    xllm_turn tTurn;
    xllm_call_options tCallOpts;
    xllm_error tError;
    xvalue tSchema = NULL;
    xvalue tFormatExtra = NULL;
    const xvalue *pJson = NULL;
    char *sJson = NULL;
    int iStatus;

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    xrtInit();

    printf("=== GLM Native JSON Schema Example ===\n");
    printf("base url : %s\n", GLM_BASE_URL);
    printf("model    : %s\n", GLM_MODEL);
    printf("mode     : native json schema\n\n");

    xllm_error_init(&tError);
    xllm_profile_init(&tProfile);
    xllm_turn_init(&tTurn);
    xllm_call_options_init(&tCallOpts);

    if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || !pRuntime ) {
        fprintf(stderr, "failed to create runtime\n");
        return 1;
    }
    if ( xllm_register_glm_native_adapter(pRuntime) != XRT_NET_OK ) {
        fprintf(stderr, "failed to register glm native adapter\n");
        xllm_runtime_destroy(pRuntime);
        return 2;
    }

    tProfile.sId = "glm-native";
    tProfile.sProvider = "zhipu";
    tProfile.sAdapter = XLLM_ADAPTER_GLM_NATIVE;
    tProfile.sBaseUrl = GLM_BASE_URL;
    tProfile.tAuth.eKind = XLLM_AUTH_BEARER;
    tProfile.tAuth.sSecret = GLM_API_KEY;
    tProfile.tModels.tText.sModelId = GLM_MODEL;
    tProfile.tModels.tText.tCaps.uFlags =
        XLLM_CAP_TEXT_IN |
        XLLM_CAP_TEXT_OUT |
        XLLM_CAP_JSON_OUT;

    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "failed to register profile\n");
        xllm_runtime_destroy(pRuntime);
        return 3;
    }

    pLlm = xllm_create(pRuntime, NULL);
    if ( !pLlm || xllm_bind_profile(pLlm, "glm-native") != XRT_NET_OK ) {
        fprintf(stderr, "failed to create llm or bind profile\n");
        xllm_destroy(pLlm);
        xllm_runtime_destroy(pRuntime);
        return 4;
    }

    xllm_set_system_prompt(pLlm, "You are a concise assistant.");
    xllm_turn_add_user_text(&tTurn, "Return JSON only. Include fields {\"answer\": string, \"ok\": boolean}.");

    tSchema = xrtParseJSON((str)sSchemaText, strlen(sSchemaText));
    if ( tSchema == NULL ) {
        fprintf(stderr, "failed to parse schema\n");
        xllm_destroy(pLlm);
        xllm_turn_reset(&tTurn);
        xllm_runtime_destroy(pRuntime);
        return 5;
    }

    tFormatExtra = xvoCreateTable();
    if ( tFormatExtra == NULL || !xvoTableSetBool(tFormatExtra, (str)"strict", 6u, true) ) {
        fprintf(stderr, "failed to create schema vendor extra\n");
        if ( tFormatExtra ) {
            xvoUnref(tFormatExtra);
        }
        xvoUnref(tSchema);
        xllm_destroy(pLlm);
        xllm_turn_reset(&tTurn);
        xllm_runtime_destroy(pRuntime);
        return 6;
    }

    if ( xllm_turn_set_json_schema_response(&tTurn, "glm_demo_schema", tSchema, tFormatExtra) != XRT_NET_OK ) {
        fprintf(stderr, "failed to set json schema response\n");
        xvoUnref(tFormatExtra);
        xvoUnref(tSchema);
        xllm_destroy(pLlm);
        xllm_turn_reset(&tTurn);
        xllm_runtime_destroy(pRuntime);
        return 7;
    }
    xvoUnref(tFormatExtra);
    xvoUnref(tSchema);

    tCallOpts.uTimeoutMs = 120000u;

    iStatus = xllm_send_ex(pLlm, &tTurn, &tCallOpts, &pResponse, &tError);
    if ( iStatus != XRT_NET_OK || !pResponse ) {
        fprintf(
            stderr,
            "request failed (status=%d, code=%d, msg=%s, http=%d)\n",
            iStatus,
            (int)tError.eCode,
            tError.sMessage ? tError.sMessage : "(null)",
            (int)tError.iHttpStatus
        );
        xllm_destroy(pLlm);
        xllm_error_free(&tError);
        xllm_turn_reset(&tTurn);
        xllm_runtime_destroy(pRuntime);
        return 8;
    }

    pJson = xllm_response_get_first_json(pResponse, NULL, NULL);
    if ( pJson != NULL ) {
        sJson = (char *)xrtStringifyJSON(*pJson, 0, NULL);
    }

    printf("[assistant] %s\n\n", xllm_response_get_text(pResponse));
    printf("visible text: %s\n", xllm_response_get_text(pResponse));
    printf("json output : %s\n", sJson ? sJson : "(null)");

    if ( sJson ) {
        xrtFree(sJson);
    }
    xllm_response_free(pResponse);
    xllm_destroy(pLlm);
    xllm_error_free(&tError);
    xllm_turn_reset(&tTurn);
    xllm_runtime_destroy(pRuntime);
    return 0;
}
