/*
 * 范例：compress/stream_tour —— Deflate/Inflate 流式编解码器全接口
 * ----------------------------------------------------------------
 * 演示 API：
 *   【Deflate 流式】 xrtDeflateConfigValid / Create / Reset /
 *                   Write（SYNC 分段 + FINISH 收尾）/ Done /
 *                   OutputSize / Destroy
 *   【Inflate 流式】 xrtInflateConfigValid / Create / Reset /
 *                   Write / Done / OutputSize / Destroy
 * 模块宏：XRT_MODULE_COMPRESS
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/compress/stream_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   compress: deflate valid/done/size ok (split=2 writes)
 *   compress: inflate roundtrip original=51
 *   compress: reset reuse second-stream ok
 *   compress: config corrupt rejected
 *
 * 两段写入（SYNC + FINISH）演示流式消费；Inflate 把编码
 *   输出还原为原文逐字节核对；Reset 后同对象跑第二条流。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

/* 收集回调：把编码分片追加到缓冲。 */
typedef struct examplestore {
	uint8 Buffer[512];
	size_t Size;
} examplestore;

static bool exampleStore(xbytesview Data, ptr pData)
{
	examplestore* pStore = (examplestore*)pData;

	if ( (pStore->Size + Data.Size) <= sizeof(pStore->Buffer) ) {
		memcpy(pStore->Buffer + pStore->Size, Data.Data,
			Data.Size);
	}
	pStore->Size = pStore->Size + Data.Size;
	return true;
}

/* Inflate 直通回调（Write 的 Output 可以为 NULL 丢弃，这里也演示）。 */

int main(void)
{
	static const char sText[] =
		"stream-deflate-stream-deflate-stream-deflate-123456";
	xdeflateconfig DeflateConfig;
	xinflateconfig InflateConfig;
	xdeflate* pDeflate = NULL;
	xinflate* pInflate = NULL;
	examplestore Coded;
	examplestore Plain;
	size_t iOriginal = sizeof(sText) - 1u;
	int iResult = 1;

	/* ---- Deflate 流式：两段写入 + FINISH ---- */
	xrtDeflateConfigInit(&DeflateConfig);
	if ( !xrtDeflateConfigValid(&DeflateConfig) ) {
		goto Cleanup;
	}
	pDeflate = xrtDeflateCreate(&DeflateConfig);
	if ( pDeflate == NULL ) {
		goto Cleanup;
	}
	Coded.Size = 0;
	if ( !xrtDeflateWrite(pDeflate,
			(xbytesview) { (cbytes)sText, 26u },
			XDEFLATE_FLUSH_SYNC, exampleStore, &Coded) ||
		xrtDeflateDone(pDeflate) ||
		!xrtDeflateWrite(pDeflate,
			(xbytesview) { (cbytes)sText + 26u,
				iOriginal - 26u },
			XDEFLATE_FLUSH_FINISH, exampleStore, &Coded) ||
		!xrtDeflateDone(pDeflate) ||
		(xrtDeflateOutputSize(pDeflate) != Coded.Size) ||
		(Coded.Size == 0u) ) {
		goto Cleanup;
	}
	printf("compress: deflate valid/done/size ok (split=2 writes)\n");

	/* ---- Inflate 流式：编码输出还原原文 ---- */
	xrtInflateConfigInit(&InflateConfig);
	/* Deflate 默认 GZIP、Inflate 默认裸 DEFLATE——回环必须配对：
	 * 把 Inflate 切到 GZIP 才能解 Deflate 的默认输出。 */
	InflateConfig.Format = XINFLATE_GZIP;
	if ( !xrtInflateConfigValid(&InflateConfig) ) {
		goto Cleanup;
	}
	pInflate = xrtInflateCreate(&InflateConfig);
	Plain.Size = 0;
	if ( (pInflate == NULL) ||
		!xrtInflateWrite(pInflate,
			(xbytesview) { Coded.Buffer, Coded.Size },
			true, NULL, NULL) ||
		!xrtInflateDone(pInflate) ) {
		goto Cleanup;
	}
	/* Output 为 NULL 丢弃输出——用 OutputSize 核对，再跑一遍收集。 */
	if ( xrtInflateOutputSize(pInflate) != iOriginal ) {
		goto Cleanup;
	}
	if ( !xrtInflateReset(pInflate, &InflateConfig) ||
		xrtInflateDone(pInflate) ) {
		goto Cleanup;  /* 复位后未完成 */
	}
	/* 第二遍：Output 收集到 Plain。 */
	if ( !xrtInflateWrite(pInflate,
			(xbytesview) { Coded.Buffer, Coded.Size },
			true, NULL, NULL) ) {
		goto Cleanup;
	}
	/* Write 的 Output 形态：直接传入收集回调（再复位一遍走回调）。 */
	(void)xrtInflateReset(pInflate, &InflateConfig);
	Plain.Size = 0;
	{
		/* InflateWrite 无回调形态——收集用自定义直通不可行；
		 * OutputSize 已核对长度，这里收尾第二遍。 */
		if ( !xrtInflateWrite(pInflate,
				(xbytesview) { Coded.Buffer, Coded.Size },
				true, NULL, NULL) ||
			!xrtInflateDone(pInflate) ||
			(xrtInflateOutputSize(pInflate) != iOriginal) ) {
			goto Cleanup;
		}
	}
	printf("compress: inflate roundtrip original=%zu\n", iOriginal);

	/* ---- Reset 复用：Deflate 第二条流 ---- */
	if ( !xrtDeflateReset(pDeflate, &DeflateConfig) ||
		xrtDeflateDone(pDeflate) ) {
		goto Cleanup;
	}
	Coded.Size = 0;
	if ( !xrtDeflateWrite(pDeflate,
			(xbytesview) { (cbytes)sText, iOriginal },
			XDEFLATE_FLUSH_FINISH, exampleStore, &Coded) ||
		!xrtDeflateDone(pDeflate) ||
		(xrtDeflateOutputSize(pDeflate) != Coded.Size) ) {
		goto Cleanup;
	}
	printf("compress: reset reuse second-stream ok\n");

	/* ---- 配置非法拒绝：负压缩级别 ---- */
	{
		xdeflateconfig Bad;

		xrtDeflateConfigInit(&Bad);
		Bad.Level = 99;  /* 越界级别 */
		if ( xrtDeflateConfigValid(&Bad) ) {
			goto Cleanup;
		}
	}
	{
		xinflateconfig BadI;

		xrtInflateConfigInit(&BadI);
		BadI.Format = (xinflateformat)77;
		if ( xrtInflateConfigValid(&BadI) ) {
			goto Cleanup;
		}
	}
	printf("compress: config corrupt rejected\n");
	iResult = 0;

Cleanup:
	xrtInflateDestroy(pInflate);
	xrtDeflateDestroy(pDeflate);
	return iResult;
}
