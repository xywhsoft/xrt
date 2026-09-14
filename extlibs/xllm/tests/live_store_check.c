/* live_store_check — GLM 云验证：请求体携带 {"store":false} 是否被接受。
 * 用法：XLLM_LIVE_URL / XLLM_LIVE_KEY / XLLM_LIVE_MODEL 环境变量。 */
#include "../xllm.c"

int main(void)
{
    const char* sUrl = getenv("XLLM_LIVE_URL");
    const char* sKey = getenv("XLLM_LIVE_KEY");
    const char* sModel = getenv("XLLM_LIVE_MODEL");
    xllm_client_config tConfig;
    xllm_client* pClient = NULL;
    xllm_request tRequest;
    xllm_response* pResponse = NULL;
    xllm_error tError;
    if ( !sUrl || !sKey || !sModel ) {
        printf("live store check: env not set, skipping\n");
        return 0;
    }
    xllmClientConfigInit(&tConfig);
    tConfig.sBaseUrl = sUrl;
    tConfig.sApiKey = sKey;
    tConfig.sModel = sModel;
    tConfig.eProvider = XLLM_PROVIDER_OPENAI_COMPAT;
    tConfig.uMaxOutputTokens = 64u;
    tConfig.uTimeoutMs = 60u * 1000u;
    pClient = xllmClientCreate(&tConfig, &tError);
    if ( !pClient ) { printf("client FAILED: %s\n", tError.sMessage); return 1; }
    xllmRequestInit(&tRequest);
    tRequest.bStream = false;
    (void)xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "Reply with exactly: ok");
    (void)xllmRequestSetExtraBody(&tRequest, "{\"store\":false}");
    if ( xllmClientComplete(pClient, &tRequest, NULL, &pResponse, &tError) == XLLM_RESULT_OK && pResponse ) {
        printf("store:false ACCEPTED by server; content='%.40s' usage_in=%llu\n",
            pResponse->sContent ? pResponse->sContent : "(empty)",
            (unsigned long long)pResponse->tUsage.uInputTokens);
        xllmResponseDestroy(pResponse);
        xllmRequestUnit(&tRequest);
        xllmClientDestroy(pClient);
        return 0;
    }
    printf("store:false REJECTED: http=%d msg=%.120s provider=%.80s\n",
        tError.iHttpStatus, tError.sMessage, tError.sProviderMessage);
    xllmRequestUnit(&tRequest);
    xllmClientDestroy(pClient);
    return 1;
}
