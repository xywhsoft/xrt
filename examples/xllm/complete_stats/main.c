/*
 * 范例：xllm/complete_stats —— 单次模型调用：提示词 → 回复 → 链路参数
 * ----------------------------------------------------------------
 * 演示 API（xllm 扩展库，vendored 于 extlibs/xllm）：
 *   xllmClientCreate / xllmClientComplete   客户端与一次性同步调用
 *   xllmRequestAddTextMessage               组装用户消息
 *   response->tUsage / tStats               token 计数与速度/耗时/链路统计
 *   xllmClientGetModelProfile               上下文窗口取画像真值
 * 模块宏：XRT_MODULE_*（由 extlibs/xllm/xllm-xrt.h 自选，无需手工定义）
 * 编译（本仓库清单驱动，Windows）：
 *   python tools/build.py --suite xllm_examples
 * 手工编译（单 TU，Windows/GCC）：
 *   gcc -std=c11 -Wall -Wextra -Werror -O2 -I include -I single \
 *       examples/xllm/complete_stats/main.c \
 *       single/xrt.h 所需实现对象……（推荐直接用上面的清单命令）
 * 预期输出（设置 XLLM_API_KEY 后）：
 *   [思考] ……（共 N 字符）
 *   <回复正文>
 *   ---- 参数 ----
 *   finish/tokens/耗时/速度/上下文/链路 各一行
 * 未设置 XLLM_API_KEY 时打印提示并成功退出（门禁友好）。
 *
 * 生命周期契约（本例刻意示范的四条）：
 *   1. 配置字符串借用（环境变量指针即可，客户端创建时自行复制）；
 *   2. Complete 返回后请求立即释放（请求已被序列化接管）；
 *   3. 响应所有权归调用方，xllmResponseDestroy 恰好释放一次；
 *   4. 失败路径打印结构化错误（类别名 + HTTP 状态 + 服务端原文）。
 *
 * 实现并入方式：xllm 是 extlib（不在核心清单闭包内），本 TU 直接
 * include 其 unity 源（声明+实现一体）；XRT 符号由套件闭包对象提供。
 * 保持默认流式（回调为 NULL，库自动组装完整响应）：只有流式才能观测
 * 首 token 时刻与真实 t/s，非流式整包返回时速度只能算整体吞吐。
 */

#include "../../../extlibs/xllm/xllm.c"

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
	const char* sApiKey = env_or("XLLM_API_KEY", "");
	xllm_client_config tClientConfig;
	xllm_request tRequest;
	xllm_response* pResponse = NULL;
	xllm_error tError;
	xllm_client* pClient;
	xllm_result eResult;
	int iExit = 0;

	if ( !sApiKey[0] ) {
		printf("未设置 XLLM_API_KEY：跳过真实调用。\n"
			"  XLLM_API_KEY=... %s \"提示词\"\n",
			argc > 1 ? argv[0] : "本程序");
		return 0;
	}

	xllmClientConfigInit(&tClientConfig);
	tClientConfig.sBaseUrl = env_or("XLLM_BASE_URL",
		"https://open.bigmodel.cn/api/paas/v4");
	tClientConfig.sApiKey = sApiKey;
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

	eResult = xllmClientComplete(pClient, &tRequest, NULL, &pResponse, &tError);
	xllmRequestUnit(&tRequest);

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
