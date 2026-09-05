#include <stdio.h>

#include <xrt.h>



/*
 * 范例：crypto/sha512 —— SHA-384 与 SHA-512（对称 API 家族）
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtSha384Init / Update / Final   SHA-384 流式
 *   xrtSha512                        SHA-512 一次性
 * 模块宏：XRT_MODULE_CRYPTO（SHA512 特性，384 同源）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/crypto/sha512/main.c -lws2_32 -liphlpapi
 * 预期输出（均为 "hello world" 的标准摘要）：
 *   SHA-384: fdbd8e75a67f29f701a4e040385e2e23...
 *   SHA-512: 309ecc489c12d6eb4cc40f50c902f2b4...
 *
 * SHA-384/512 共用 64 位字核心，输出长度不同（48/64 字节）；
 *   每个 SHA 变体都同时提供三段式与一次性两种入口，
 *   命名规则完全一致（见 sha1/sha224/sha512_256 范例）。
 */


/* 用对称的流 API 分别计算 SHA-384 和 SHA-512。 */
int main(void)
{
	xsha384 State;
	uint8 arrDigest[XRT_SHA512_SIZE];

	xrtSha384Init(&State);
	if ( !xrtSha384Update(&State, "hello ", 6) ||
		 !xrtSha384Update(&State, "world", 5) ||
		 !xrtSha384Final(&State, arrDigest) ) {
		return 1;
	}
	printf("SHA-384: ");
	for ( size_t i = 0; i < XRT_SHA384_SIZE; i++ ) {
		printf("%02x", (unsigned int)arrDigest[i]);
	}
	printf("\n");
	if ( !xrtSha512("hello world", 11, arrDigest) ) {
		return 1;
	}
	printf("SHA-512: ");
	for ( size_t i = 0; i < XRT_SHA512_SIZE; i++ ) {
		printf("%02x", (unsigned int)arrDigest[i]);
	}
	printf("\n");
	return 0;
}
