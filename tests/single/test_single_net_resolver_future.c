#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件 Future 便捷层与 Resolver 使用同一份实现。 */
int main(void)
{
	xnetresolver* pResolver = xrtNetResolverCreate(NULL);
	xfuture* pFuture;
	xnetaddrlist* pAddresses;
	int iResult = 1;

	if ( pResolver == NULL ) {
		return 1;
	}
	pFuture = xrtNetResolveAsync(
		pResolver,
		"127.0.0.1",
		XNET_FAMILY_IPV4
	);
	if ( (pFuture == NULL) ||
		 (xrtFutureWaitFor(pFuture, 2000000u) != XWAIT_OK) ||
		 (xrtFutureState(pFuture) != XFUTURE_RESOLVED) ) {
		xrtFutureDestroy(pFuture);
		(void)xrtNetResolverDestroy(pResolver);
		return 2;
	}
	pAddresses = (xnetaddrlist*)xrtFutureValue(pFuture);
	if ( (pAddresses != NULL) &&
		 (xrtNetAddrListCount(pAddresses) == 1) ) {
		iResult = 0;
	}
	xrtFutureDestroy(pFuture);
	if ( !xrtNetResolverDestroy(pResolver) ) {
		return 3;
	}
	return iResult;
}
