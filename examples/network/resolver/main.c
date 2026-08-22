#include <stdio.h>
#include <string.h>
#include <xrt.h>



typedef struct exampleresolver {
	xatomic32 Done;
} exampleresolver;



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
