#include <stdio.h>

#include <xrt.h>



/*
 * 范例：crypto/hmac_sha256 —— HMAC-SHA256（带密钥的认证）
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtHmacSha256   一次性 (密钥, 消息) → MAC
 * 模块宏：XRT_MODULE_CRYPTO（HMAC 特性）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/crypto/hmac_sha256/main.c -lws2_32 -liphlpapi
 * 预期输出（密钥 "secret" + 消息 "message" 的标准 MAC）：
 *   8b5f48702995c1598c573db1e21866a9b825d4a794d169d7060a03605796360b
 *
 * HMAC vs 裸哈希：哈希无密钥——攻击者可自行计算；
 *   HMAC 只有持密钥方算得出，API 签名、Webhook 校验、
 *   Cookie 完整性的标准件。验证侧务必用 ConstTimeEqual
 *   比较（见 core 范例）。流式版 Init/Update/Final 同族。
 */


/* 用一次性入口计算请求正文的 HMAC-SHA256。 */
int main(void)
{
	uint8 arrMac[XRT_SHA256_SIZE];

	if ( !xrtHmacSha256("secret", 6, "message", 7, arrMac) ) {
		return 1;
	}
	for ( size_t i = 0; i < sizeof(arrMac); i++ ) {
		printf("%02x", (unsigned int)arrMac[i]);
	}
	printf("\n");
	return 0;
}
