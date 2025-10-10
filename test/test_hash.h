


// Hash 库测试
void Test_Hash(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n Hash 库测试 :\n\n");
	
	uint32 iHash32 = xrtHash32_WithSeed("hashtablekey", 12, HASH32_SEED);
	printf("hash32 value : hashtablekey = %x\n", iHash32);
	iHash32 = xrtHash32_WithSeed("xywhsoft", 8, HASH32_SEED);
	printf("hash32 value : xywhsoft = %x\n", iHash32);
	iHash32 = xrtHash32_WithSeed("abcdefghijklmnopqrstuvwxyz", 8, HASH32_SEED);
	printf("hash32 value : abcdefghijklmnopqrstuvwxyz = %x\n", iHash32);
	
	uint64 iHash64 = xrtHash64_WithSeed("hashtablekey", 12, HASH64_SEED);
	printf("hash64 value : hashtablekey = %llx\n", iHash64);
	iHash64 = xrtHash64_WithSeed("xywhsoft", 8, HASH64_SEED);
	printf("hash64 value : xywhsoft = %llx\n", iHash64);
	iHash64 = xrtHash64_WithSeed("abcdefghijklmnopqrstuvwxyz", 8, HASH64_SEED);
	printf("hash64 value : abcdefghijklmnopqrstuvwxyz = %llx\n", iHash64);
	
}


