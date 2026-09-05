#include <stdio.h>

#include <xrt.h>



/*
 * 范例：crypto/sha1 —— SHA-1（仅限遗留协议互操作）
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtSha1   一次性摘要
 * 模块宏：XRT_MODULE_CRYPTO（SHA1 特性）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/crypto/sha1/main.c -lws2_32 -liphlpapi
 * 预期输出（"hello" 标准摘要）：
 *   aaf4c61ddcc5e8a2dabede0f3b482cd9aea9434d
 *
 * 存在意义：SHA-1 已可碰撞，新设计一律 SHA-256 起；
 *   但 WebSocket 握手（RFC 6455 Accept 计算）、旧证书、
 *   Git 对象 ID 等协议仍要求它——按特性裁剪引入。
 */


/* 计算 WebSocket 握手等兼容协议仍需要的 SHA-1 摘要。 */
int main(void)
{
	uint8 arrDigest[XRT_SHA1_SIZE];

	if ( !xrtSha1("hello", 5, arrDigest) ) {
		return 1;
	}
	for ( size_t i = 0; i < sizeof(arrDigest); i++ ) {
		printf("%02x", (unsigned int)arrDigest[i]);
	}
	printf("\n");
	return 0;
}
