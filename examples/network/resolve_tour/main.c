/*
 * 范例：network/resolve_tour —— Resolver 统计、缓存清理与 Op 生命周期
 * ----------------------------------------------------------------
 * 演示 API：
 *   【统计】      xrtNetResolverStats（并发一致快照）
 *   【缓存】      xrtNetResolverClear（清空成功/失败缓存）
 *   【Op 生命周期】xrtNetResolveOpState（PENDING→RESOLVED 状态机）
 *                 xrtNetResolveOpRef（共享引用，双份销毁）
 *   【配套】      xrtNetResolverConfigInit / Create / Resolve /
 *                 ResolveOpResult / ResolveOpDestroy / ResolverDestroy
 * 模块宏：XRT_MODULE_NET_RESOLVER
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/network/resolve_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   resolve: stats before submitted=0
 *   resolve: op pending->resolved with >=1 address ok
 *   resolve: op-ref double destroy ok
 *   resolve: stats after submitted>=1 resolved>=1 ok
 *   resolve: clear caches ok
 *
 * OpResult 返回"增加引用的地址列表"——调用方必须销毁；
 *   Op 在回调后仍可查询状态与结果，保留须先 OpRef。
 */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <xrt.h>

/* 回调与主线程共享的状态。 */
typedef struct examplestate {
	volatile bool bDone;
	xnetresolveopstate StateInCallback;
} examplestate;

/* 解析完成回调：记录回调时刻的操作状态。 */
static void exampleResolveDone(xnetresolveop* pOperation, ptr pData)
{
	examplestate* pState = (examplestate*)pData;

	pState->StateInCallback = xrtNetResolveOpState(pOperation);
	pState->bDone = true;
}

int main(void)
{
	xnetresolverconfig Config;
	xnetresolver* pResolver = NULL;
	xnetresolveop* pOperation = NULL;
	xnetresolveop* pRef = NULL;
	xnetaddrlist* pList = NULL;
	xnetresolverstats Stats;
	examplestate State;
	int iResult = 1;

	/* ---- 创建与初始统计：空转 Resolver 全零。 ---- */
	xrtNetResolverConfigInit(&Config);
	pResolver = xrtNetResolverCreate(&Config);
	if ( (pResolver == NULL) ||
		!xrtNetResolverStats(pResolver, &Stats) ||
		(Stats.Submitted != 0u) ) {
		goto Cleanup;
	}
	printf("resolve: stats before submitted=0\n");

	/* ---- 查询 localhost：状态机 PENDING → RESOLVED。 ---- */
	memset(&State, 0, sizeof(State));
	pOperation = xrtNetResolverResolve(pResolver, "localhost",
		XNET_FAMILY_IPV4, exampleResolveDone, (ptr)&State);
	if ( (pOperation == NULL) ||
		(xrtNetResolveOpState(pOperation) ==
			XNET_RESOLVE_RESOLVED) ) {
		goto Cleanup;
	}
	for ( int i = 0; (i < 3000) && !State.bDone; i++ ) {
		xrtSleep(1u);
	}
	if ( !State.bDone ||
		(State.StateInCallback != XNET_RESOLVE_RESOLVED) ||
		(xrtNetResolveOpState(pOperation) != XNET_RESOLVE_RESOLVED) ) {
		goto Cleanup;
	}
	pList = xrtNetResolveOpResult(pOperation);
	if ( (pList == NULL) ||
		(xrtNetAddrListCount(pList) < 1u) ) {
		goto Cleanup;
	}
	xrtNetAddrListDestroy(pList);
	pList = NULL;
	printf("resolve: op pending->resolved with >=1 address ok\n");

	/* ---- OpRef：共享引用后两份销毁各自配对。 ---- */
	pRef = xrtNetResolveOpRef(pOperation);
	if ( (pRef == NULL) || (pRef != pOperation) ) {
		goto Cleanup;
	}
	xrtNetResolveOpDestroy(pRef);
	xrtNetResolveOpDestroy(pOperation);
	pOperation = NULL;
	printf("resolve: op-ref double destroy ok\n");

	/* ---- 查询后统计：提交与解析计数推进。 ---- */
	if ( !xrtNetResolverStats(pResolver, &Stats) ||
		(Stats.Submitted < 1u) ||
		(Stats.Resolved < 1u) ) {
		goto Cleanup;
	}
	printf("resolve: stats after submitted>=1 resolved>=1 ok\n");

	/* ---- 清缓存：已完成的查询结果被丢弃。 ---- */
	if ( !xrtNetResolverClear(pResolver) ) {
		goto Cleanup;
	}
	if ( !xrtNetResolverStats(pResolver, &Stats) ||
		(Stats.CachedResults != 0u) ) {
		goto Cleanup;
	}
	printf("resolve: clear caches ok\n");
	iResult = 0;

Cleanup:
	if ( pOperation != NULL ) {
		xrtNetResolveOpDestroy(pOperation);
	}
	if ( (pResolver != NULL) &&
		!xrtNetResolverDestroy(pResolver) ) {
		iResult = 2;
	}
	return iResult;
}
