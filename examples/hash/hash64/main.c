/*
 * 范例：hash/hash64 —— 面向缓存分桶与内存索引的 64 位哈希
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtHash64  确定性 64 位哈希（xrt 内部哈希容器的默认键哈希）
 * 模块宏：XRT_MODULE_HASH
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/hash/hash64/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   0AD01480B2F6CC86
 *
 * 为什么 64 位：32 位哈希在千万键量级生日碰撞概率已不可忽视
 *   （约 1%@10 万键），64 位在十亿键内碰撞期望仍可忽略——
 *   分桶索引、去重指纹用它才稳。同样确定性、非密码学安全。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	/* URL 查询串作为缓存键的典型场景。 */
	static const char sKey[] = "/api/items?page=2";
	uint64 iHash = xrtHash64(sKey, sizeof(sKey) - 1u);

	printf("%016llX\n", (unsigned long long)iHash);
	return 0;
}
