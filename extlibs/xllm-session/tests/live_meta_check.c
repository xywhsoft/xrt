/* live_meta_check — 真网验证：压缩元调用携带 {"store":false} 被服务端接受
 * 且摘要正常提交。用法：XLLM_LIVE_URL/XLLM_LIVE_KEY/XLLM_LIVE_MODEL 环境变量。 */
#define XRT_MODULE_JSONL_READ
#include "../../xllm/xllm.c"
#include "../xllm-session.c"

int main(void)
{
    const char* sUrl = getenv("XLLM_LIVE_URL");
    const char* sKey = getenv("XLLM_LIVE_KEY");
    const char* sModel = getenv("XLLM_LIVE_MODEL");
    xllm_client_config tClient;
    xllm_client* pClient = NULL;
    xllm_session_config tCfg;
    xllm_session* pSession = NULL;
    xllm_session_stats tStats;
    xllm_error tError;
    char sMsg[1600];
    bool bCompact = false;
    int i;
    if ( !sUrl || !sKey || !sModel ) {
        printf("live meta check: env not set, skipping\n");
        return 0;
    }
    memset(sMsg, 'x', sizeof(sMsg) - 1u);
    sMsg[sizeof(sMsg) - 1u] = '\0';
    xllmClientConfigInit(&tClient);
    tClient.sBaseUrl = sUrl;
    tClient.sApiKey = sKey;
    tClient.sModel = sModel;
    tClient.eProvider = XLLM_PROVIDER_OPENAI_COMPAT;
    tClient.uMaxOutputTokens = 512u;
    tClient.uTimeoutMs = 120u * 1000u;
    pClient = xllmClientCreate(&tClient, &tError);
    if ( !pClient ) { printf("client FAILED: %s\n", tError.sMessage); return 1; }
    xllmSessionConfigInit(&tCfg);
    tCfg.uContextWindowTokens = 12000u;
    tCfg.uMaxOutputTokens = 512u;
    tCfg.uSafetyReserveTokens = 1000u;
    tCfg.uKeepRecentTokens = 600u;
    tCfg.uUserMessageCapBytes = 4096u;
    tCfg.uSummaryMaxTokens = 2048u;
    tCfg.fPruneTrigger = 0.60;
    tCfg.fCompactTrigger = 0.70;
    pSession = xllmSessionCreateBound(&tCfg, pClient, &tError);
    if ( !pSession ) { printf("session FAILED: %s\n", tError.sMessage); return 1; }
    for ( i = 0; i < 40; ++i ) {
        xllm_request tRequest;
        xllm_response* pResponse = NULL;
        uint64_t uTurn = xllmSessionBeginTurn(pSession);
        char sHead[64];
        bool bOk;
        snprintf(sHead, sizeof(sHead), "fact %d: codename Orion-%d. Reply: ack. ", i, i);
        memcpy(sMsg, sHead, strlen(sHead));
        if ( !xllmSessionAddText(pSession, uTurn, XLLM_ROLE_USER, sMsg, 0u) ) {
            printf("add rejected at %d\n", i);
            break;
        }
        xllmRequestInit(&tRequest);
        if ( !xllmSessionBuildRequest(pSession, &tRequest, &tError) ) {
            printf("turn %d build failed: %s\n", i, tError.sMessage);
            xllmRequestUnit(&tRequest);
            break;
        }
        /* 关思考：让回复最小化，离线估算与精确占用同步爬升 */
        (void)xllmRequestSetExtraBody(&tRequest,
            "{\"chat_template_kwargs\":{\"enable_thinking\":false}}");
        bOk = xllmClientComplete(pClient, &tRequest, NULL, &pResponse, &tError) == XLLM_RESULT_OK;
        xllmRequestUnit(&tRequest);
        if ( !bOk || !pResponse ) {
            printf("turn %d call failed: %s\n", i, tError.sMessage);
            xllmResponseDestroy(pResponse);
            break;
        }
        (void)xllmSessionAddAssistantResponse(pSession, uTurn, pResponse);
        xllmResponseDestroy(pResponse);
        {
            bool bDid = false;
            if ( !xllmSessionMaybeCompact(pSession, &bDid, &tError) ) {
                printf("turn %d compact failed: %s\n", i, tError.sMessage);
                break;
            }
        }
        if ( !xllmSessionGetStats(pSession, &tStats) ) { break; }
        printf("turn %2d: fill=%llu pressure=%d compactions=%llu\n", i,
            (unsigned long long)tStats.uFillExact, (int)tStats.ePressure,
            (unsigned long long)tStats.uCompactionCount);
        if ( tStats.uCompactionCount > 0u ) { bCompact = true; break; }
    }
    printf("meta-call-with-store-false %s\n",
        bCompact ? "ACCEPTED (compaction committed)" : "not reached");
    xllmSessionDestroy(pSession);
    xllmClientDestroy(pClient);
    return bCompact ? 0 : 2;
}
