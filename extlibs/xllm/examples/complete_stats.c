/*
 * complete_stats.c — xllm 单次调用示例：发送一条提示词，打印回复与链路参数。
 *
 * 演示的最小契约：
 *   1. 配置字符串全部借用（栈/环境变量），库创建客户端时自行复制；
 *   2. Complete 返回后请求即可释放（请求已被序列化接管）；
 *   3. 响应所有权归调用方，xllmResponseDestroy 恰好释放一次；
 *   4. 失败路径打印结构化错误（类别名 + 诊断 + 服务端原文）。
 *
 * 编译（仓库根目录，Windows/GCC）：
 *   gcc -std=c11 -Wall -Wextra -Werror -O2 \
 *       -I extlibs/xllm -I single \
 *       extlibs/xllm/examples/complete_stats.c \
 *       extlibs/xllm/release/xllm.o extlibs/xllm/release/xllm-xrt.o \
 *       -lWs2_32 -lIPHLPAPI -lBcrypt -lCrypt32 -lSecur32 -lAdvapi32 \
 *       -o build/complete_stats
 *
 * 运行（环境变量均有默认值，密钥无默认）：
 *   XLLM_API_KEY=... ./build/complete_stats "你的提示词"
 *   可选：XLLM_BASE_URL / XLLM_MODEL / XLLM_CONTEXT_WINDOW
 */

#include <xllm.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 客户端未挂模型画像时的窗口显示兜底；画像存在则优先取画像真值。 */
#define FALLBACK_CONTEXT_WINDOW 131072u

static const char* env_or(const char* sName, const char* sFallback)
{
    const char* sValue = getenv(sName);
    return (sValue && sValue[0]) ? sValue : sFallback;
}

static uint64_t context_window_of(const xllm_client* pClient)
{
    xllm_model_profile tProfile;
    if ( xllmClientGetModelProfile(pClient, &tProfile) &&
         tProfile.uContextWindowTokens > 0u ) {
        return tProfile.uContextWindowTokens;
    }
    {
        uint64_t uWindow =
            strtoull(env_or("XLLM_CONTEXT_WINDOW", "0"), NULL, 10);
        return uWindow ? uWindow : (uint64_t)FALLBACK_CONTEXT_WINDOW;
    }
}

static void print_stats(const xllm_response* pResponse, uint64_t uWindow)
{
    const xllm_usage* pUsage = &pResponse->tUsage;
    const xllm_stats* pStats = &pResponse->tStats;
    uint64_t uCtxUsed = pUsage->uInputTokens + pUsage->uOutputTokens;

    printf("\n---- 参数 ----\n");
    printf("finish:   %s (%s)\n",
        xllmFinishReasonName(pResponse->eFinish),
        pResponse->sFinishReason ? pResponse->sFinishReason : "-");
    printf("tokens:   in=%llu out=%llu total=%llu",
        (unsigned long long)pUsage->uInputTokens,
        (unsigned long long)pUsage->uOutputTokens,
        (unsigned long long)pUsage->uTotalTokens);
    if ( pUsage->uCachedInputTokens ) {
        printf(" (cached=%llu)",
            (unsigned long long)pUsage->uCachedInputTokens);
    }
    if ( pUsage->uReasoningTokens ) {
        printf(" (reasoning=%llu)",
            (unsigned long long)pUsage->uReasoningTokens);
    }
    printf("\n");
    printf("耗时:     connect=%llums 首token=%llums 总=%llums\n",
        (unsigned long long)pStats->uConnectMs,
        (unsigned long long)pStats->uFirstTokenMs,
        (unsigned long long)pStats->uTotalMs);
    printf("速度:     %.2f t/s（墙钟口径，含网络）\n",
        pStats->fOutputTokensPerSec);
    printf("上下文:   %llu/%llu（%.1f%%）\n",
        (unsigned long long)uCtxUsed, (unsigned long long)uWindow,
        uWindow ? 100.0 * (double)uCtxUsed / (double)uWindow : 0.0);
    printf("链路:     尝试=%u 复用=%s 请求=%llub 响应=%llub\n",
        (unsigned)pStats->uAttempts,
        pStats->bReusedConnection ? "是" : "否",
        (unsigned long long)pStats->uRequestBytes,
        (unsigned long long)pStats->uResponseBytes);
}

static void print_failure(xllm_result eResult, const xllm_error* pError)
{
    fprintf(stderr, "调用失败: result=%d code=%s http=%d\n  %s\n",
        (int)eResult, xllmErrorCodeName(pError->eCode),
        pError->iHttpStatus, pError->sMessage);
    if ( pError->sProviderMessage[0] ) {
        fprintf(stderr, "  provider: %s\n", pError->sProviderMessage);
    }
    if ( pError->sRequestId[0] ) {
        fprintf(stderr, "  request-id: %s\n", pError->sRequestId);
    }
}

int main(int argc, char** argv)
{
    const char* sPrompt =
        (argc > 1) ? argv[1] : "用一句话介绍你自己";
    xllm_client_config tClientConfig;
    xllm_request tRequest;
    xllm_response* pResponse = NULL;
    xllm_error tError;
    xllm_client* pClient;
    xllm_result eResult;
    int iExit = 0;

    xllmClientConfigInit(&tClientConfig);
    tClientConfig.sBaseUrl = env_or("XLLM_BASE_URL",
        "https://open.bigmodel.cn/api/paas/v4");
    tClientConfig.sApiKey = env_or("XLLM_API_KEY", "");
    tClientConfig.sModel = env_or("XLLM_MODEL", "glm-5.3-flash");
    tClientConfig.eProvider = XLLM_PROVIDER_OPENAI_COMPAT;
    tClientConfig.uMaxOutputTokens = 1024u;
    tClientConfig.uTimeoutMs = 60u * 1000u;
    tClientConfig.uMaxAttempts = 2u;

    pClient = xllmClientCreate(&tClientConfig, &tError);
    if ( !pClient ) {
        print_failure(XLLM_RESULT_ERROR, &tError);
        return 1;
    }

    xllmRequestInit(&tRequest);
    if ( !xllmRequestSetModel(&tRequest, tClientConfig.sModel) ||
         !xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, sPrompt) ) {
        fprintf(stderr, "请求构造失败\n");
        xllmRequestUnit(&tRequest);
        xllmClientDestroy(pClient);
        return 1;
    }
    /* 保持默认流式（回调为 NULL，库仍自动组装完整响应）：流式才能观测
     * 首 token 时刻与真实 t/s；非流式整包返回时速度只能算整体吞吐。 */

    eResult = xllmClientComplete(pClient, &tRequest, NULL, &pResponse, &tError);
    xllmRequestUnit(&tRequest); /* 请求已序列化，立即释放 */

    if ( eResult == XLLM_RESULT_OK && pResponse ) {
        if ( pResponse->sReasoningContent && pResponse->sReasoningContent[0] ) {
            printf("[思考] %.60s...（共 %zu 字符）\n\n",
                pResponse->sReasoningContent,
                strlen(pResponse->sReasoningContent));
        }
        printf("%s\n",
            pResponse->sContent ? pResponse->sContent : "(空响应)");
        print_stats(pResponse, context_window_of(pClient));
    } else {
        print_failure(eResult, &tError);
        iExit = 1;
    }

    xllmResponseDestroy(pResponse);
    xllmClientDestroy(pClient);
    return iExit;
}
