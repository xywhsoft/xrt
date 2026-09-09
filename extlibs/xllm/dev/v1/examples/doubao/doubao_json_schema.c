#include "xllm-session.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

int main(void)
{
    static const char sSchemaText[] =
        "{\"type\":\"object\",\"properties\":{\"answer\":{\"type\":\"string\"},\"ok\":{\"type\":\"boolean\"}},"
        "\"required\":[\"answer\",\"ok\"],\"additionalProperties\":false}";
    const char *sApiKey = demo_env2("DOUBAO_API_KEY", "ARK_API_KEY", NULL);
    const char *sBaseUrl = demo_env2("DOUBAO_BASE_URL", "ARK_BASE_URL", "https://ark.cn-beijing.volces.com/api/v3");
    const char *sModel = demo_env2("DOUBAO_MODEL", "ARK_MODEL", NULL);
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

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    xrtInit();

    printf("=== Doubao Native JSON Schema Example ===\n");
    printf("base url : %s\n", sBaseUrl);
    printf("model    : %s\n", sModel ? sModel : "(set DOUBAO_MODEL or ARK_MODEL)");
    printf("mode     : native json schema\n\n");

    if ( !sApiKey || !sApiKey[0] ) {
        fprintf(stderr, "set DOUBAO_API_KEY or ARK_API_KEY first\n");
        return 2;
    }
    if ( !sModel || !sModel[0] ) {
        fprintf(stderr, "set DOUBAO_MODEL or ARK_MODEL first\n");
        return 3;
    }

    xllm_profile_init(&tProfile);
    xllm_turn_init(&tTurn);
    xllm_call_options_init(&tCallOpts);
    xllm_error_init(&tError);

    if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || !pRuntime ) {
        fprintf(stderr, "failed to create runtime\n");
        return 4;
    }
    if ( xllm_register_doubao_native_adapter(pRuntime) != XRT_NET_OK ) {
        fprintf(stderr, "failed to register doubao native adapter\n");
        xllm_runtime_destroy(pRuntime);
        return 5;
    }

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
        XLLM_CAP_JSON_OUT;

    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "failed to register profile\n");
        xllm_runtime_destroy(pRuntime);
        return 6;
    }

    pLlm = xllm_create(pRuntime, NULL);
    if ( !pLlm || xllm_bind_profile(pLlm, "doubao-native") != XRT_NET_OK ) {
        fprintf(stderr, "failed to create llm or bind profile\n");
        xllm_destroy(pLlm);
        xllm_runtime_destroy(pRuntime);
        return 7;
    }

    xllm_set_system_prompt(pLlm, "You are a concise assistant.");
    if ( xllm_turn_add_user_text(&tTurn, "Return JSON only. Include fields {\"answer\": string, \"ok\": boolean}.") != XRT_NET_OK ) {
        fprintf(stderr, "failed to build turn\n");
        xllm_destroy(pLlm);
        xllm_runtime_destroy(pRuntime);
        return 8;
    }

    tSchema = xrtParseJSON((str)sSchemaText, strlen(sSchemaText));
    if ( tSchema == NULL ) {
        fprintf(stderr, "failed to parse schema\n");
        xllm_turn_reset(&tTurn);
        xllm_destroy(pLlm);
        xllm_runtime_destroy(pRuntime);
        return 9;
    }

    tFormatExtra = xvoCreateTable();
    if ( tFormatExtra == NULL || !xvoTableSetBool(tFormatExtra, (str)"strict", 6u, true) ) {
        fprintf(stderr, "failed to create schema vendor extra\n");
        if ( tFormatExtra ) {
            xvoUnref(tFormatExtra);
        }
        xvoUnref(tSchema);
        xllm_turn_reset(&tTurn);
        xllm_destroy(pLlm);
        xllm_runtime_destroy(pRuntime);
        return 10;
    }

    if ( xllm_turn_set_json_schema_response(&tTurn, "doubao_demo_schema", tSchema, tFormatExtra) != XRT_NET_OK ) {
        fprintf(stderr, "failed to set json schema response\n");
        xvoUnref(tFormatExtra);
        xvoUnref(tSchema);
        xllm_turn_reset(&tTurn);
        xllm_destroy(pLlm);
        xllm_runtime_destroy(pRuntime);
        return 11;
    }
    xvoUnref(tFormatExtra);
    xvoUnref(tSchema);

    tCallOpts.uTimeoutMs = 120000u;
    tCallOpts.bBestEffortStructuredOutput = true;

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
        xllm_turn_reset(&tTurn);
        xllm_error_free(&tError);
        xllm_destroy(pLlm);
        xllm_runtime_destroy(pRuntime);
        return 12;
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
    xllm_turn_reset(&tTurn);
    xllm_error_free(&tError);
    xllm_destroy(pLlm);
    xllm_runtime_destroy(pRuntime);
    return 0;
}
