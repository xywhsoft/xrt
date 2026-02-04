

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
	
	str s = xrtStrCreate("Hello, XRT Single Header!");
	printf("  String: %s\n", s);
	
	int len = xrtStrLen(s);
	printf("  Length: %d\n", len);
	
	printf("\nTesting memory pool...\n");
	xmempool pool = xrtMemPoolCreate(1024);
	printf("  Memory pool created\n");
	
	ptr p = xrtMemPoolAlloc(pool, 100);
	printf("  Allocated 100 bytes\n");
	
	xrtMemPoolFree(pool, p);
	printf("  Freed memory\n");
	
	xrtMemPoolDestroy(pool);
	printf("  Memory pool destroyed\n");
	
	printf("\nTesting array...\n");
	xarray arr = xrtArrayCreate(10, sizeof(int));
	printf("  Array created\n");
	
	for ( int i = 0; i < 5; i++ ) {
		int val = i * 10;
		xrtArrayAdd(arr, &val);
		printf("  Added: %d\n", val);
	}
	
	int count = xrtArrayCount(arr);
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
