#include <stdio.h>

#include <xrt.h>



/* 演示适合缓存分桶和内存索引的确定性 64 位哈希。 */
int main(void)
{
	static const char sKey[] = "/api/items?page=2";
	uint64 iHash = xrtHash64(sKey, sizeof(sKey) - 1u);

	printf("%016llX\n", (unsigned long long)iHash);
	return 0;
}
