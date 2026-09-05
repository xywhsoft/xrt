#include <stdio.h>

#include <xrt.h>



/*
 * 范例：crypto/sha256 —— SHA-256 流式三段式
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtSha256Init / Update / Final   状态机式分块计算
 * 模块宏：XRT_MODULE_CRYPTO（SHA256 特性）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/crypto/sha256/main.c -lws2_32 -liphlpapi
 * 预期输出（"hello world" 标准摘要）：
 *   b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9
 *
 * 三段式 vs 一次性：数据分块到达（文件流、网络包）时
 *   不必拼完整缓冲；Update 可调用任意次。
 *   一次性入口 xrtSha256(buf, len, out) 适合已知完整数据。
 */


/* 分块计算 SHA-256，并从不消耗状态的快照取得摘要。 */
int main(void)
{
	xsha256 State;
	uint8 arrDigest[XRT_SHA256_SIZE];

	xrtSha256Init(&State);
	if ( !xrtSha256Update(&State, "hello ", 6) ||
		 !xrtSha256Update(&State, "world", 5) ||
		 !xrtSha256Final(&State, arrDigest) ) {
		return 1;
	}
	for ( size_t i = 0; i < sizeof(arrDigest); i++ ) {
		printf("%02x", (unsigned int)arrDigest[i]);
	}
	printf("\n");
	return 0;
}
