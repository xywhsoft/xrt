#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



typedef struct testsingleresolver {
	xatomic32 Done;
} testsingleresolver;



/* 记录单头文件 Resolver 的异步终态。 */
static void testSingleResolverDone(xnetresolveop* pOperation, ptr pData)
{
	testsingleresolver* pState = (testsingleresolver*)pData;

	if ( xrtNetResolveOpState(pOperation) == XNET_RESOLVE_RESOLVED ) {
		(void)xrtAtomic32FetchAdd(&pState->Done, 1, XMEMORY_RELEASE);
	}
}



/* 验证单头文件包含完整 Resolver 线程、缓存和查询实现。 */
int main(void)
{
	testsingleresolver State;
	xnetresolver* pResolver;
	xnetresolveop* pOperation;
	xnetaddrlist* pAddresses;
	xdeadline iDeadline;
	int iResult = 1;

	memset(&State, 0, sizeof(State));
	pResolver = xrtNetResolverCreate(NULL);
	if ( pResolver == NULL ) {
		return 1;
	}
	pOperation = xrtNetResolverResolve(
		pResolver,
		"127.0.0.1",
		XNET_FAMILY_IPV4,
		testSingleResolverDone,
		&State
	);
	if ( pOperation == NULL ) {
		(void)xrtNetResolverDestroy(pResolver);
		return 2;
	}
	iDeadline = xrtDeadlineAfter(2000000u);
	while ( xrtAtomic32Load(&State.Done, XMEMORY_ACQUIRE) == 0 ) {
		if ( xrtDeadlineExpired(iDeadline) ) {
			xrtNetResolveOpDestroy(pOperation);
			(void)xrtNetResolverDestroy(pResolver);
			return 3;
		}
		xrtThreadYield();
	}
	pAddresses = xrtNetResolveOpResult(pOperation);
	if ( pAddresses != NULL ) {
		const xnetaddr* pAddress = xrtNetAddrListGet(pAddresses, 0);

		if ( (pAddress != NULL) && (pAddress->Port == 0) ) {
			iResult = 0;
		}
	}
	xrtNetAddrListDestroy(pAddresses);
	xrtNetResolveOpDestroy(pOperation);
	if ( !xrtNetResolverDestroy(pResolver) ) {
		return 4;
	}
	return iResult;
}
