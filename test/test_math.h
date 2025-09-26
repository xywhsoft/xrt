


// Math 库测试
void Test_Math(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n Math 库测试 :\n\n");
	for ( int i = 0; i < 10; i++ ) {
		printf("Rand 0 - 100 : %d\n", xrtRandRange(0, 100));
	}
	for ( int i = 0; i < 10; i++ ) {
		printf("Rand64 : %lld\n", xrtRand64());
	}
}


