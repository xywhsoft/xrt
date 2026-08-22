#include <stdio.h>
#include <xrt.h>



/* 使用 Future 等待异步 DNS，并读取 Future 持有的完整地址列表。 */
int main(void)
{
	xnetresolver* pResolver = xrtNetResolverCreate(NULL);
	xfuture* pFuture;
	xnetaddrlist* pAddresses;

	if ( pResolver == NULL ) {
		return 1;
	}
	pFuture = xrtNetResolveAsync(
		pResolver,
		"localhost",
		XNET_FAMILY_UNSPEC
	);
	if ( (pFuture == NULL) ||
		 (xrtFutureWaitFor(pFuture, 5000000u) != XWAIT_OK) ) {
		xrtFutureDestroy(pFuture);
		(void)xrtNetResolverDestroy(pResolver);
		return 2;
	}
	pAddresses = (xnetaddrlist*)xrtFutureValue(pFuture);
	if ( pAddresses == NULL ) {
		const xerror* pError = xrtFutureError(pFuture);

		fprintf(stderr, "%s\n", pError != NULL ?
			xrtErrorMessage(pError) : "resolve failed");
	} else {
		for ( size_t i = 0; i < xrtNetAddrListCount(pAddresses); i++ ) {
			str sAddress = xrtNetAddrString(
				xrtNetAddrListGet(pAddresses, i)
			);

			if ( sAddress != NULL ) {
				printf("%s\n", sAddress);
				xrtFree(sAddress);
			}
		}
	}
	xrtFutureDestroy(pFuture);
	return xrtNetResolverDestroy(pResolver) ? 0 : 3;
}
