#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <stdio.h>



/* 验证单头文件可创建一份拥有式系统信任库快照。 */
int main(void)
{
	xx509store* pStore = xrtX509StoreSystem();

	if ( (pStore == NULL) || (xrtX509StoreCount(pStore) == 0) ) {
		xrtX509StoreFree(pStore);
		return 1;
	}
	xrtX509StoreFree(pStore);
	printf("[PASS] single-x509-store-system\n");
	return 0;
}
