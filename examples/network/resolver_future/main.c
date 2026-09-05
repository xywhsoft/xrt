#include <stdio.h>
#include <xrt.h>



/*
 * 范例：network/resolver_future —— Future 化 DNS 解析
 * ----------------------------------------------------------------
 * 演示 API：
 *   DNS 解析的 Future 入口   等待并取地址列表
 * 模块宏：XRT_MODULE_NET（依赖 FUTURE）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/network/resolver_future/main.c -lws2_32 -liphlpapi
 * 预期输出（顺序可能互換）：
 *   ::1
 *   127.0.0.1
 *
 * Future 值即完整地址列表（拥有式）——
 *   拿到后可直接喂给拨号循环（多地址依次尝试）。
 */


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
