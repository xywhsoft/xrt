


// Math 库测试
void Test_Math(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n Math 库测试 :\n\n");
	
	// 测试基础随机数函数
	printf("=== 基础随机数函数（线程不安全）===\n");
	printf("xrtRandRange(0, 100) x10:\n");
	for ( int i = 0; i < 10; i++ ) {
		printf("  %d", xrtRandRange(0, 100));
	}
	printf("\n\n");
	
	printf("xrtRand32() x5:\n");
	for ( int i = 0; i < 5; i++ ) {
		printf("  %u\n", xrtRand32());
	}
	printf("\n");
	
	printf("xrtRand64() x5:\n");
	for ( int i = 0; i < 5; i++ ) {
		printf("  %llu\n", xrtRand64());
	}
	printf("\n");
	
	// 测试 Ex 版本 API（线程安全）
	printf("=== Ex 版本 API（调用者管理状态，线程安全）===\n");
	
	// 创建独立的随机数生成器
	xrand myRng = XRAND_INITIALIZER;
	xrtRandSeed(&myRng, 12345, 1);
	
	printf("固定种子 xrtRand32Ex() x5:\n");
	for ( int i = 0; i < 5; i++ ) {
		printf("  %u\n", xrtRand32Ex(&myRng));
	}
	printf("\n");
	
	// 重置种子，验证可重复性
	xrtRandSeed(&myRng, 12345, 1);
	printf("重置种子后 xrtRand32Ex() x5（应与上面相同）:\n");
	for ( int i = 0; i < 5; i++ ) {
		printf("  %u\n", xrtRand32Ex(&myRng));
	}
	printf("\n");
	
	// 测试范围随机数
	xrtRandSeed(&myRng, 67890, 3);
	printf("xrtRandRangeEx(1, 6) x10（模拟掷骰子）:\n");
	for ( int i = 0; i < 10; i++ ) {
		printf("  %d", xrtRandRangeEx(&myRng, 1, 6));
	}
	printf("\n\n");
	
	// 测试64位随机数
	xrand rngLow = XRAND_INITIALIZER;
	xrand rngHigh = XRAND_INITIALIZER;
	xrtRandSeed(&rngLow, 11111, 1);
	xrtRandSeed(&rngHigh, 22222, 3);
	
	printf("xrtRand64Ex() x3:\n");
	for ( int i = 0; i < 3; i++ ) {
		printf("  0x%016llX\n", xrtRand64Ex(&rngLow, &rngHigh));
	}
	printf("\n");
	
	printf("Math 库测试完成\n");
}


