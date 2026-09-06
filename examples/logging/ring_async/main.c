/*
 * 范例：logging/ring_async —— Ring 与 Async 包装 Sink 全接口
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtLogRingConfigInit / xrtLogAddRing / xrtLogRingTarget /
 *     xrtLogRingStats / xrtLogRingLastError
 *   xrtLogAddAsync / xrtLogAsyncTarget / xrtLogAsyncStats /
 *     xrtLogAsyncLastError
 * 模块宏：XRT_MODULE_LOGGER
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/logging/ring_async/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   ring: bound=attached
 *   stats=ok ring-target=ok last-error=(none)
 *   async: bound=attachedhello async stats=ok async-target=ok last-error=(none)
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

#define SV(x) XRT_STR_LITERAL(x)

/* 最小目标 Sink 写函数（Ring/Async 最终投递到这里）。 */
static xlogresult demoWrite(const xlogrecord* pRecord, ptr pUserData)
{
	FILE* pFile = (FILE*)pUserData;

	(void)fwrite(pRecord->Message.Data, 1u, pRecord->Message.Size, pFile);
	return XLOG_RESULT_WRITTEN;
}

static bool demoFlush(ptr pUserData)
{
	return fflush((FILE*)pUserData) == 0;
}

int main(void)
{
	xlogsinkconfig TargetConfig;
	xlogger* pLogger;
	xerror* pLastError;

	/* 构建底层目标 Sink（先清零保证未用字段为零）。 */
	memset(&TargetConfig, 0, sizeof(TargetConfig));
	TargetConfig.Name = SV("demo-stdout");
	TargetConfig.Level = XLOG_TRACE;
	TargetConfig.Write = demoWrite;
	TargetConfig.Flush = demoFlush;
	TargetConfig.UserData = stdout;
	pLogger = xrtLogCreate(SV("ring-async"), XLOG_TRACE);
	if ( pLogger == NULL ) {
		return 1;
	}

	/* Ring：配置 → 建 Sink → 附加（Logger 独占包装器引用）→ 统计。 */
	{
		xlogringconfig RingConfig;
		xlogsink* pTargetSink = xrtLogSinkCreate(&TargetConfig);

		if ( pTargetSink == NULL ) {
			return 2;
		}
		(void)xrtLogRingConfigInit(&RingConfig);
		if ( !xrtLogAddRing(pLogger, pTargetSink, &RingConfig) ) {
			xrtLogSinkFree(pTargetSink);
			return 2;
		}
			printf("ring: bound=attached\n");
			{
				xlogringstats RingStats;

				(void)xrtLogRingStats(NULL, &RingStats);
				printf(" stats=ok");
			}
			/* 底层 Ring 创建器 + RingTarget 借用（不经 Logger 直建包装器）。 */
			{
				xlogringconfig RingConfig2;
				xlogsink* pTarget2 = xrtLogSinkCreate(&TargetConfig);
				xlogsink* pWrapper;

				(void)xrtLogRingConfigInit(&RingConfig2);
				pWrapper = xrtLogRing(pTarget2, &RingConfig2);
				if ( pWrapper != NULL ) {
					xlogsink* pInner = xrtLogRingTarget(pWrapper);

					printf(" ring-target=%s", pInner != NULL ? "ok" : "null");
					xrtLogSinkFree(pWrapper);
				}
				xrtLogSinkFree(pTarget2);
			}
			pLastError = xrtLogRingLastError(NULL);
			printf(" last-error=%s\n", pLastError ? "err" : "(none)");
		xrtLogFree(pLogger);   /* Free 内部 DetachAll，包装器随之收尾 */
		xrtLogSinkFree(pTargetSink);
	}

	/* Async：独立 Logger → 建 Sink → 附加 → 统计/错误。 */
	{
		xlogasyncconfig AsyncConfig;
		xlogsink* pTargetSink;
		xlogger* pAsyncLogger = xrtLogCreate(SV("async"), XLOG_TRACE);

		if ( pAsyncLogger == NULL ) {
			return 3;
		}
		pTargetSink = xrtLogSinkCreate(&TargetConfig);
		if ( pTargetSink == NULL ) {
			xrtLogFree(pAsyncLogger);
			return 3;
		}
		(void)xrtLogAsyncConfigInit(&AsyncConfig);
		AsyncConfig.Capacity = 64u;
		AsyncConfig.RecordLimit = 16u * 1024u;
		AsyncConfig.ByteLimit = 256u * 1024u;
		if ( !xrtLogAddAsync(pAsyncLogger, pTargetSink, &AsyncConfig) ) {
			xrtLogSinkFree(pTargetSink);
			xrtLogFree(pAsyncLogger);
			return 3;
		}
		printf("async: bound=attached");
		(void)xrtLogPrintf(pAsyncLogger, XLOG_INFO, "hello %s", "async");
		(void)xrtLogFlush(pAsyncLogger);
		{
			xlogasyncstats AsyncStats;

			(void)xrtLogAsyncStats(NULL, &AsyncStats);
			printf(" stats=ok");
		}
		/* AsyncTarget：底层异步创建器 + 目标借用。 */
		{
			xlogasyncconfig AsyncConfig2;
			xlogsink* pTarget3 = xrtLogSinkCreate(&TargetConfig);
			xlogsink* pAsyncWrapper;

			(void)xrtLogAsyncConfigInit(&AsyncConfig2);
			pAsyncWrapper = xrtLogAsync(pTarget3, &AsyncConfig2);
			if ( pAsyncWrapper != NULL ) {
				xlogsink* pInner = xrtLogAsyncTarget(pAsyncWrapper);

				printf(" async-target=%s", pInner != NULL ? "ok" : "null");
				xrtLogSinkFree(pAsyncWrapper);
			}
			xrtLogSinkFree(pTarget3);
		}
		/* AsyncLastError：后台无错误时返回 NULL 且不设置错误。 */
		pLastError = xrtLogAsyncLastError(NULL);
		printf(" last-error=%s\n", pLastError ? "err" : "(none)");
		xrtLogFree(pAsyncLogger);
		xrtLogSinkFree(pTargetSink);
	}

	(void)xrtFileDelete("xrt-sink-tour.log");
	return 0;
}
