#include "../test.h"



/* 验证从旧版保留的尺寸类和跨层重分配契约。 */
int main(void)
{
	unsigned char* pSmall;
	unsigned char* pSameClass;
	unsigned char* pNextClass;
	unsigned char* pBacking;
	unsigned char* pBackingGrow;
	unsigned char* pPooledAgain;
	unsigned char* pZero;

	pSmall = (unsigned char*)xrtMalloc(24);
	testRequire(pSmall != NULL, "small allocation failed");
	testRequire(((uintptr_t)pSmall & 15u) == 0, "small allocation is not 16-byte aligned");
	memset(pSmall, 0x5A, 24);
	pSameClass = (unsigned char*)xrtRealloc(pSmall, 32);
	testRequire(pSameClass == pSmall, "same size class realloc must stay in place");
	pNextClass = (unsigned char*)xrtRealloc(pSameClass, 48);
	testRequire(pNextClass != NULL, "cross size class realloc failed");
	testRequire(((uintptr_t)pNextClass & 15u) == 0, "pooled realloc lost 16-byte alignment");
	testRequire((pNextClass[0] == 0x5A) && (pNextClass[23] == 0x5A), "cross class realloc lost data");

	pBacking = (unsigned char*)xrtMalloc(2048);
	testRequire(pBacking != NULL, "backing allocation failed");
	testRequire(((uintptr_t)pBacking & 15u) == 0, "backing allocation is not 16-byte aligned");
	memset(pBacking, 0x3C, 2048);
	pBackingGrow = (unsigned char*)xrtRealloc(pBacking, 4096);
	testRequire(pBackingGrow != NULL, "backing realloc failed");
	testRequire(((uintptr_t)pBackingGrow & 15u) == 0, "backing realloc lost 16-byte alignment");
	testRequire((pBackingGrow[0] == 0x3C) && (pBackingGrow[2047] == 0x3C), "backing realloc lost data");
	pPooledAgain = (unsigned char*)xrtRealloc(pBackingGrow, 128);
	testRequire(pPooledAgain != NULL, "backing to pooled realloc failed");
	testRequire((pPooledAgain[0] == 0x3C) && (pPooledAgain[127] == 0x3C), "backing shrink lost data");

	pZero = (unsigned char*)xrtCalloc(4, 16);
	testRequire(pZero != NULL, "pooled calloc failed");
	for ( size_t i = 0; i < 64; i++ ) {
		testRequire(pZero[i] == 0, "pooled calloc did not clear memory");
	}

	xrtFree(pNextClass);
	xrtFree(pPooledAgain);
	xrtFree(pZero);
	printf("[PASS] heap\n");
	return 0;
}
