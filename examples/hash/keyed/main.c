/*
 * 范例：hash/keyed —— SipHash 带密钥哈希：对抗哈希洪泛攻击
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtSipKey         由两个 64 位半段组装 128 位密钥
 *   xrtSipHashInit    以密钥初始化流式哈希状态
 *   xrtSipHashUpdate  分块喂入数据（可多次）
 *   xrtSipHashFinal   收尾取 64 位结果
 * 模块宏：XRT_MODULE_HASH
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/hash/keyed/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   040320100DED48E9
 *
 * 为什么需要带密钥：普通哈希（hash32/64）输出可被预计算——
 *   攻击者构造一串碰撞键发起请求，能把服务端哈希表打成 O(n) 链
 *  （Hash-Flooding）。密钥在进程启动时随机生成后，
 *   相同输入在不同进程/重启后哈希不同，预计算失效。
 * 流式三段式（Init/Update/Final）允许数据分块到达，
 *   不必先拼出完整缓冲。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	/* 128 位密钥：真实代码应来自 xrtSecureRandom，不要写死。 */
	xsipkey Key = xrtSipKey(UINT64_C(0x0123456789ABCDEF),
		UINT64_C(0xFEDCBA9876543210));
	xsiphash State;
	uint64 iHash;

	xrtSipHashInit(&State, Key);

	/*
	 * 分块喂入："request:" 是受信任前缀，"user-input" 是不可信部分。
	 * 分两次 Update 与一次喂拼接结果完全等价。
	 */
	if ( !xrtSipHashUpdate(&State, "request:", 8) ||
		 !xrtSipHashUpdate(&State, "user-input", 10) ) {
		return 1;
	}
	iHash = xrtSipHashFinal(&State);
	printf("%016llX\n", (unsigned long long)iHash);
	return 0;
}
