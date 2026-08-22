#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <stdio.h>



/* 验证单头文件中的共享 TLS 上下文快照。 */
int main(void)
{
	xtlscontext* pContext = xrtTlsContextCreate(NULL);
	const xtlspolicy* pPolicy;

	if ( pContext == NULL ) {
		return 1;
	}
	pPolicy = xrtTlsContextPolicy(pContext);
	if ( (pPolicy == NULL) || (pPolicy->VersionCount != 2u) ) {
		xrtTlsContextRelease(pContext);
		return 1;
	}
	xrtTlsContextRelease(pContext);
	printf("[PASS] single-tls-context\n");
	return 0;
}
