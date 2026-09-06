/*
 * 范例：network/buf_tour —— 缓冲链四类追加 + 拼合/查找/消费
 * ----------------------------------------------------------------
 * 演示 API：
 *   【池】        xrtNetBufPoolConfigInit / PoolCreate / PoolGet /
 *                 PoolTrim / PoolDestroy
 *   【追加四类】  xrtNetBufAppend（复制，已在基础范例覆盖）
 *                 xrtNetBufAppendBorrow（借用）
 *                 xrtNetBufAppendTake（接管 xrtMalloc 块）
 *                 xrtNetBufAppendRef（自定义释放回调）
 *   【形状】      xrtNetBufEmpty / Size / SpanCount / Spans
 *   【编辑】      xrtNetBufPrepend（链首插入复制段）
 *                 xrtNetBufPullup（前缀连续化）
 *                 xrtNetBufPeek（偏移复制）
 *                 xrtNetBufFind（字节查找，未命中 XRT_NPOS）
 *                 xrtNetBufConsume（消费前缀）
 *   【预留】      xrtNetBufReserve + Commit（基础）+ Cancel（放弃预留）
 *   【转移】      xrtNetBufMove（源块全部移入目标，源恢复空）
 * 模块宏：XRT_MODULE_NET_BUFFER
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/network/buf_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   buf: pool create + info ok
 *   buf: append x4 borrow/take/ref -> 11 bytes in >=4 spans
 *   buf: prepend+pullup+peek+find+consume ok
 *   buf: reserve-cancel keeps 11 bytes ok
 *   buf: move source->target ok
 *   buf: trim released >=1 block, release-cb fired once
 *
 * 四类追加拼出 "hello world"：复制 5 + 借用 2 + 接管 2 + 引用 2；
 *   AppendRef 的释放回调在 Clear 后必须恰好执行一次。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

/* AppendRef 的释放回调：只记录次数，不做真实释放（栈上数据）。 */
static void exampleRelease(ptr pContext, cbytes pData, size_t iSize)
{
	int* pCount = (int*)pContext;

	(void)pData;
	(void)iSize;
	++(*pCount);
}

int main(void)
{
	xnetbufpoolconfig Config;
	xnetbufpoolinfo Info;
	xnetbufpool* pPool = NULL;
	xnetbuf BufA;
	xnetbuf BufB;
	xnetspan Spans[8];
	xnetspan Span;
	xnetwspan Reserve;
	char arrText[32];
	size_t iSpans;
	size_t iGot;
	size_t iSize;
	int iReleased = 0;
	bool bInited = false;
	int iResult = 1;
	static char arrBorrow[2] = { ' ', 'w' };
	static char arrRef[2] = { 'l', 'd' };
	ptr pTaken;

	/* ---- 池生命周期：默认配置 + 统计快照。 ---- */
	xrtNetBufPoolConfigInit(&Config);
	pPool = xrtNetBufPoolCreate(&Config);
	if ( (pPool == NULL) ||
		!xrtNetBufInit(&BufA, pPool) ||
		!xrtNetBufInit(&BufB, pPool) ) {
		goto Cleanup;
	}
	bInited = true;
	if ( !xrtNetBufEmpty(&BufA) ) {
		goto Cleanup;
	}
	xrtNetBufPoolGet(pPool, &Info);
	if ( (Info.LiveBlocks != 0u) ||
		(Info.AllocCount != 0u) ) {
		goto Cleanup;
	}
	printf("buf: pool create + info ok\n");

	/* ---- 四类追加：拼出 11 字节 "hello world"。 ---- */
	if ( !xrtNetBufAppend(&BufA, "hello", 5u) ||
		!xrtNetBufAppendBorrow(&BufA, arrBorrow, 2u) ) {
		goto Cleanup;
	}
	pTaken = xrtMalloc(2u);
	if ( (pTaken == NULL) ) {
		goto Cleanup;
	}
	memcpy(pTaken, "or", 2u);
	if ( !xrtNetBufAppendTake(&BufA, pTaken, 2u) ) {
		xrtFree(pTaken);
		goto Cleanup;
	}
	if ( !xrtNetBufAppendRef(&BufA, arrRef, 2u,
			exampleRelease, (ptr)&iReleased) ) {
		goto Cleanup;
	}
	iSize = xrtNetBufSize(&BufA);
	iSpans = xrtNetBufSpanCount(&BufA);
	if ( (iSize != 11u) ||
		(iSpans < 4u) ||
		(xrtNetBufEmpty(&BufA)) ||
		(xrtNetBufSpans(&BufA, Spans, 8u) != iSpans) ||
		(Spans[0].Size != 5u) ||
		(memcmp(Spans[0].Data, "hello", 5u) != 0) ) {
		goto Cleanup;
	}
	printf("buf: append x4 borrow/take/ref -> 11 bytes in >=4 spans\n");

	/* ---- 编辑族：前缀插入 → 拼合 → 偏移读 → 查找 → 消费。 ---- */
	if ( !xrtNetBufPrepend(&BufA, ">> ", 3u) ||
		(xrtNetBufSize(&BufA) != 14u) ||
		!xrtNetBufPullup(&BufA, 5u, &Span) ||
		(Span.Size < 5u) ||
		(memcmp(Span.Data, ">> he", 5u) != 0) ||
		(xrtNetBufPeek(&BufA, 8u, arrText, 3u) != 3u) ||
		(memcmp(arrText, " wo", 3u) != 0) ||
		(xrtNetBufFind(&BufA, 'w', 0u) != 9u) ||
		(xrtNetBufFind(&BufA, 'z', 0u) != XRT_NPOS) ||
		(xrtNetBufConsume(&BufA, 3u) != 3u) ||
		(xrtNetBufSize(&BufA) != 11u) ||
		(xrtNetBufPeek(&BufA, 0u, arrText, 11u) != 11u) ||
		(memcmp(arrText, "hello world", 11u) != 0) ) {
		goto Cleanup;
	}
	printf("buf: prepend+pullup+peek+find+consume ok\n");

	/* ---- 预留与放弃：Cancel 后总长不变。 ---- */
	if ( !xrtNetBufReserve(&BufA, 8u, &Reserve) ||
		!xrtNetBufCancel(&BufA) ||
		(xrtNetBufSize(&BufA) != 11u) ) {
		goto Cleanup;
	}
	printf("buf: reserve-cancel keeps 11 bytes ok\n");

	/* ---- Move：源的块全部转移到目标尾部。 ---- */
	if ( !xrtNetBufAppend(&BufB, "!", 1u) ||
		!xrtNetBufMove(&BufA, &BufB) ||
		!xrtNetBufEmpty(&BufB) ||
		(xrtNetBufSize(&BufA) != 12u) ||
		(xrtNetBufPeek(&BufA, 11u, arrText, 1u) != 1u) ||
		(arrText[0] != '!') ) {
		goto Cleanup;
	}
	printf("buf: move source->target ok\n");

	/* ---- 收尾：Trim 释放缓存块；Clear 触发 AppendRef 回调。 ---- */
	xrtNetBufClear(&BufA);
	xrtNetBufClear(&BufB);
	if ( iReleased != 1 ) {
		goto Cleanup;
	}
	iGot = xrtNetBufPoolTrim(pPool, 0u);
	if ( iGot < 1u ) {
		goto Cleanup;
	}
	printf("buf: trim released >=1 block, release-cb fired once\n");
	iResult = 0;

Cleanup:
	/* 失败路径同样收尾：Clear 幂等，池在块归还后再销毁。 */
	if ( bInited ) {
		xrtNetBufClear(&BufA);
		xrtNetBufClear(&BufB);
	}
	if ( pPool != NULL ) {
		xrtNetBufPoolDestroy(pPool);
	}
	return iResult;
}
