/*
 * 范例：hash/hash32 —— 确定性 32 位哈希：默认种子与显式种子
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtHash32      固定默认种子的 32 位哈希（最常用路径）
 *   xrtHash32Seed  显式种子的 32 位哈希（跨进程防碰撞/防预计算）
 * 模块宏：XRT_MODULE_HASH
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/hash/hash32/main.c -lws2_32 -liphlpapi
 * 预期输出（值由算法决定，跨平台/版本稳定）：
 *   default: 9590B597
 *   seeded : 064A210B
 *
 * 确定性含义：同输入永远同输出（无随机种子），
 *   适合校验和、分桶、缓存键；不要用于密码存储
 *  （那是 xrtSipHash 带密钥或 crypto 模块的领域）。
 * 显式种子的用途：多进程一致分片、或避免对手预计算碰撞。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	/* 长度用 sizeof-1 显式排除结尾零——键是二进制安全的。 */
	static const char sKey[] = "user:42";

	printf("default: %08X\n", (unsigned int)xrtHash32(sKey, sizeof(sKey) - 1u));
	printf("seeded : %08X\n", (unsigned int)xrtHash32Seed(sKey,
		sizeof(sKey) - 1u, UINT32_C(0x12345678)));
	return 0;
}
