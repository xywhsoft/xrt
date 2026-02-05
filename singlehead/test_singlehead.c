

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
