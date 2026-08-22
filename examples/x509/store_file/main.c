#include <xrt.h>

#include <stdio.h>



/* 从命令行指定的 DER 或 PEM 文件创建可直接用于建链的信任库。 */
int main(int iArgc, char** ppArgs)
{
	xx509store* pStore;
	size_t iAdded;

	if ( iArgc != 2 ) {
		fprintf(stderr, "usage: x509-store-file <ca-file>\n");
		return 0;
	}
	pStore = xrtX509StoreCreate();
	if ( (pStore == NULL) || !xrtX509StoreAddFile(
		pStore, ppArgs[1], &iAdded
	) ) {
		xrtX509StoreFree(pStore);
		return 2;
	}
	printf("anchors=%llu added=%llu\n",
		(unsigned long long)xrtX509StoreCount(pStore),
		(unsigned long long)iAdded);
	xrtX509StoreFree(pStore);
	return 0;
}
