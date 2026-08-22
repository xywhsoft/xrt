#include <stdio.h>

#include <xrt.h>



/* 演示默认路径和明确 seed 的确定性 32 位哈希。 */
int main(void)
{
	static const char sKey[] = "user:42";

	printf("default: %08X\n", (unsigned int)xrtHash32(sKey, sizeof(sKey) - 1u));
	printf("seeded : %08X\n", (unsigned int)xrtHash32Seed(sKey,
		sizeof(sKey) - 1u, UINT32_C(0x12345678)));
	return 0;
}
