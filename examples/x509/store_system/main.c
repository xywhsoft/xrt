#include <xrt.h>

#include <stdio.h>



/* 创建当前平台的独立系统信任库快照。 */
int main(void)
{
	xx509store* pStore = xrtX509StoreSystem();

	if ( pStore == NULL ) {
		return 1;
	}
	printf("system anchors=%llu\n",
		(unsigned long long)xrtX509StoreCount(pStore));
	xrtX509StoreFree(pStore);
	return 0;
}
