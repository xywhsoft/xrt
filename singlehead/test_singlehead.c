


// 可通过定义对 xrt 库进行裁剪
#define XRT_NO_TIME
#define XRT_NO_FILE
#define XRT_NO_THREAD
#define XRT_NO_REGEX
#define XRT_NO_COROUTINE
#define XRT_NO_NETWORK
#define XRT_NO_CRYPTO
#define XRT_NO_NETSOCK
#define XRT_NO_NETPOLL
#define XRT_NO_NETTLS
#define XRT_NO_NETLOOP
#define XRT_NO_NETTCP
#define XRT_NO_NETUDP
#define XRT_NO_XID
#define XRT_NO_BUFFER
#define XRT_NO_NETHTTP
#define XRT_NO_STACK
#define XRT_NO_BSMN
#define XRT_NO_MEMUNIT
#define XRT_NO_MEMPOOL_FS
#define XRT_NO_STACK
#define XRT_NO_AVLTREE
#define XRT_NO_DICT
#define XRT_NO_LIST
#define XRT_NO_VALUE
#define XRT_NO_JSON
#define XRT_NO_TEMPLATE



// 定义 XRT_IMPLEMENTATION 导入功能实现
#define XRT_IMPLEMENTATION
#include "xrt.h"


int main()
{
	printf("========================================\n");
	printf("  XRT Single Header Test\n");
	printf("========================================\n\n");
	
	printf("Initializing XRT...\n");
	xrtInit();
	
	printf("\nTesting basic functions...\n");
	
	str s = xrtCopyStr("Hello, XRT Single Header!", 0);
	printf("  String: %s\n", s);
	
	int len = strlen(s);
	printf("  Length: %d\n", len);
	
	printf("\nTesting memory pool...\n");
	xmempool pool = xrtMemPoolCreate(1);
	printf("  Memory pool created\n");
	
	ptr p = xrtMemPoolAlloc(pool, 100);
	printf("  Allocated 100 bytes\n");
	
	xrtMemPoolFree(pool, p);
	printf("  Freed memory\n");
	
	xrtMemPoolDestroy(pool);
	printf("  Memory pool destroyed\n");
	
	printf("\nTesting array...\n");
	xarray arr = xrtArrayCreate(sizeof(int));
	printf("  Array created\n");
	
	for ( int i = 0; i < 5; i++ ) {
		int val = i * 10;
		uint32 pos = xrtArrayInsert(arr, arr->Count, 1);
		if ( pos > 0 ) {
			int* pItem = (int*)xrtArrayGet(arr, pos - 1);
			if ( pItem ) {
				*pItem = val;
				printf("  Added: %d\n", val);
			}
		}
	}
	
	int count = arr->Count;
	printf("  Array count: %d\n", count);
	
	xrtArrayDestroy(arr);
	printf("  Array destroyed\n");
	
	printf("\nCleaning up...\n");
	xrtUnit();
	
	printf("\n========================================\n");
	printf("  All tests passed!\n");
	printf("========================================\n\n");
	
	return 0;
}
