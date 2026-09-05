#include <stdio.h>
#include <string.h>
#include <xrt.h>



typedef struct exampleresolver {
	xatomic32 Done;
} exampleresolver;



/*
 * 范例：network/resolver —— 异步 DNS：引擎亲和的解析入口
 * ----------------------------------------------------------------
 * 演示 API：
 *   引擎版 DNS 解析   回调在 Worker 上执行
 * 模块宏：XRT_MODULE_NET
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/network/resolver/main.c -lws2_32 -liphlpapi
 * 预期输出（顺序可能互換）：
 *   ::1
 *   127.0.0.1
 *
 * 与 dns（同步版）对照：解析完成回调跑在引擎
 *   Worker 上——与后续连接操作同线程，天然免锁。
 */


/* 输出异步查询得到的全部地址。 */
static void exampleResolverDone(xnetresolveop* pOperation, ptr pData)
{
	exampleresolver* pState = (exampleresolver*)pData;
	xnetaddrlist* pAddresses = xrtNetResolveOpResult(pOperation);

	if ( pAddresses != NULL ) {
		for ( size_t i = 0; i < xrtNetAddrListCount(pAddresses); i++ ) {
			str sAddress = xrtNetAddrString(
				xrtNetAddrListGet(pAddresses, i)
			);

			if ( sAddress != NULL ) {
				printf("%s\n", sAddress);
				xrtFree(sAddress);
			}
		}
		xrtNetAddrListDestroy(pAddresses);
	} else {
		const xerror* pError = xrtNetResolveOpError(pOperation);

		fprintf(stderr, "%s\n", pError != NULL ?
			xrtErrorMessage(pError) : "resolve failed");
	}
	(void)xrtAtomic32FetchAdd(&pState->Done, 1, XMEMORY_RELEASE);
}



/* 使用独立 Resolver 异步查询并等待一次完成回调。 */
int main(void)
{
	exampleresolver State;
	xnetresolver* pResolver;
	xnetresolveop* pOperation;
	xdeadline iDeadline;

	memset(&State, 0, sizeof(State));
	pResolver = xrtNetResolverCreate(NULL);
	if ( pResolver == NULL ) {
		return 1;
	}
	pOperation = xrtNetResolverResolve(
		pResolver,
		"localhost",
		XNET_FAMILY_UNSPEC,
		exampleResolverDone,
		&State
	);
	if ( pOperation == NULL ) {
		(void)xrtNetResolverDestroy(pResolver);
		return 2;
	}
	iDeadline = xrtDeadlineAfter(5000000u);
	while ( xrtAtomic32Load(&State.Done, XMEMORY_ACQUIRE) == 0 ) {
		if ( xrtDeadlineExpired(iDeadline) ) {
			(void)xrtNetResolveOpCancel(pOperation);
			break;
		}
		xrtThreadYield();
	}
	xrtNetResolveOpDestroy(pOperation);
	return xrtNetResolverDestroy(pResolver) ? 0 : 3;
}
