


// Hash 库测试
void Test_Hash(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n Hash 库测试 :\n\n");
	
	// 基本测试
	printf("--- 基本哈希测试 ---\n");
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
	
	// 默认种子测试
	printf("\n--- 默认种子测试 ---\n");
	uint32 h32_default = xrtHash32("test", 4);
	uint32 h32_with_seed = xrtHash32_WithSeed("test", 4, HASH32_SEED);
	printf("hash32 (default) : test = %x\n", h32_default);
	printf("hash32 (seed) : test = %x\n", h32_with_seed);
	
	uint64 h64_default = xrtHash64("test", 4);
	uint64 h64_with_seed = xrtHash64_WithSeed("test", 4, HASH64_SEED);
	printf("hash64 (default) : test = %llx\n", h64_default);
	printf("hash64 (seed) : test = %llx\n", h64_with_seed);
	
	// 不同种子测试
	printf("\n--- 不同种子测试 ---\n");
	uint32 h32_seed1 = xrtHash32_WithSeed("samekey", 7, 0x12345678);
	uint32 h32_seed2 = xrtHash32_WithSeed("samekey", 7, 0x87654321);
	printf("hash32 (seed1) : samekey = %x\n", h32_seed1);
	printf("hash32 (seed2) : samekey = %x\n", h32_seed2);
	printf("不同种子应该产生不同哈希值: %s\n", h32_seed1 != h32_seed2 ? "✓" : "✗");
	
	// 边界情况测试
	printf("\n--- 边界情况测试 ---\n");
	uint32 h32_empty = xrtHash32("", 0);
	uint64 h64_empty = xrtHash64("", 0);
	printf("hash32 (empty) : '' = %x\n", h32_empty);
	printf("hash64 (empty) : '' = %llx\n", h64_empty);
	
	uint32 h32_one = xrtHash32("a", 1);
	uint64 h64_one = xrtHash64("a", 1);
	printf("hash32 (one char) : 'a' = %x\n", h32_one);
	printf("hash64 (one char) : 'a' = %llx\n", h64_one);
	
	uint32 h32_long = xrtHash32("This is a very long string that should test the hash function's ability to handle larger inputs efficiently without collisions", 108);
	uint64 h64_long = xrtHash64("This is a very long string that should test the hash function's ability to handle larger inputs efficiently without collisions", 108);
	printf("hash32 (long) : long string = %x\n", h32_long);
	printf("hash64 (long) : long string = %llx\n", h64_long);
	
	// 特殊字符测试
	printf("\n--- 特殊字符测试 ---\n");
	uint32 h32_special = xrtHash32("特殊字符!@#$%^&*()", 22);
	uint64 h64_special = xrtHash64("特殊字符!@#$%^&*()", 22);
	printf("hash32 (special) : 特殊字符 = %x\n", h32_special);
	printf("hash64 (special) : 特殊字符 = %llx\n", h64_special);
	
	// 数字测试
	printf("\n--- 数字测试 ---\n");
	uint32 h32_num = xrtHash32("1234567890", 10);
	uint64 h64_num = xrtHash64("1234567890", 10);
	printf("hash32 (number) : 1234567890 = %x\n", h32_num);
	printf("hash64 (number) : 1234567890 = %llx\n", h64_num);
	
	// 散列性测试
	printf("\n--- 散列性测试 ---\n");
	uint32 h32_a = xrtHash32("a", 1);
	uint32 h32_b = xrtHash32("b", 1);
	uint32 h32_c = xrtHash32("c", 1);
	printf("hash32 : 'a' = %x, 'b' = %x, 'c' = %x\n", h32_a, h32_b, h32_c);
	if ( h32_a != h32_b && h32_b != h32_c && h32_a != h32_c ) {
		printf("✓ 不同输入产生不同哈希值（基本散列性）\n");
	} else {
		printf("✗ 存在哈希冲突\n");
	}
	
	// Micro 和 Nano 版本测试
	printf("\n--- Micro/Nano 版本测试 ---\n");
	uint64 h64_normal = xrtHash64("teststring", 10);
	uint64 h64_micro = xrtHash64_Micro("teststring", 10);
	uint64 h64_nano = xrtHash64_Nano("teststring", 10);
	printf("hash64 (normal) : teststring = %llx\n", h64_normal);
	printf("hash64 (micro) : teststring = %llx\n", h64_micro);
	printf("hash64 (nano) : teststring = %llx\n", h64_nano);
	
	// Micro/Nano 带种子测试
	uint64 h64_micro_seed = xrtHash64_Micro_WithSeed("test", 4, 0x1234567890ABCDEF);
	uint64 h64_nano_seed = xrtHash64_Nano_WithSeed("test", 4, 0x1234567890ABCDEF);
	printf("hash64 (micro+seed) : test = %llx\n", h64_micro_seed);
	printf("hash64 (nano+seed) : test = %llx\n", h64_nano_seed);
	
	// 大小写敏感测试
	printf("\n--- 大小写敏感测试 ---\n");
	uint32 h32_upper = xrtHash32("HELLO", 5);
	uint32 h32_lower = xrtHash32("hello", 5);
	printf("hash32 : 'HELLO' = %x\n", h32_upper);
	printf("hash32 : 'hello' = %x\n", h32_lower);
	printf("大小写敏感: %s\n", h32_upper != h32_lower ? "✓" : "✗");
	
	// UTF-8 编码测试
	printf("\n--- UTF-8 编码测试 ---\n");
	uint32 h32_utf8 = xrtHash32("你好世界", 12);
	uint64 h64_utf8 = xrtHash64("你好世界", 12);
	printf("hash32 (UTF-8) : 你好世界 = %x\n", h32_utf8);
	printf("hash64 (UTF-8) : 你好世界 = %llx\n", h64_utf8);
	
	// 哈希稳定性测试（多次调用应该返回相同结果）
	printf("\n--- 哈希稳定性测试 ---\n");
	uint32 h32_first = xrtHash32("stability", 9);
	uint32 h32_second = xrtHash32("stability", 9);
	uint32 h32_third = xrtHash32("stability", 9);
	uint64 h64_first = xrtHash64("stability", 9);
	uint64 h64_second = xrtHash64("stability", 9);
	uint64 h64_third = xrtHash64("stability", 9);
	if ( h32_first == h32_second && h32_second == h32_third ) {
		printf("✓ Hash32 稳定性良好（3次调用结果一致）\n");
	} else {
		printf("✗ Hash32 稳定性差\n");
	}
	if ( h64_first == h64_second && h64_second == h64_third ) {
		printf("✓ Hash64 稳定性良好（3次调用结果一致）\n");
	} else {
		printf("✗ Hash64 稳定性差\n");
	}
}


