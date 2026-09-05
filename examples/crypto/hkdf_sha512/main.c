#include <stdio.h>

#include <xrt.h>



/*
 * 范例：crypto/hkdf_sha512 —— HKDF 两步式：Extract 与 Expand 分离
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtHkdfSha384Extract   原始熵 → 固定长度伪随机密钥(PRK)
 *   xrtHkdfSha384Expand    PRK + info → 多段输出
 * 模块宏：XRT_MODULE_CRYPTO（HKDF 特性）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/crypto/hkdf_sha512/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   4c21e729972ab613108552e9a6ca410dd655d09c03b39385ab3a56428929ce58
 *
 * 两步式的价值：Extract 一次，Expand 多次——
 *   同一 PRK 按不同 info 派生收发两个方向的密钥、
 *   重握手/密钥更新（TLS KeyUpdate）不重做 Extract。
 *   PRK 用完 SecureZero（范例示范）。
 */


/* 分开执行 SHA-384 HKDF 的 Extract 与 Expand。 */
int main(void)
{
	uint8 arrPrk[XRT_SHA384_SIZE];
	uint8 arrKey[32];

	if ( !xrtHkdfSha384Extract("salt", 4, "ikm", 3, arrPrk) ||
		 !xrtHkdfSha384Expand(
			arrPrk, sizeof(arrPrk), "context", 7, arrKey, sizeof(arrKey)
		 ) ) {
		return 1;
	}
	for ( size_t i = 0; i < sizeof(arrKey); i++ ) {
		printf("%02x", (unsigned int)arrKey[i]);
	}
	printf("\n");
	xrtSecureZero(arrPrk, sizeof(arrPrk));
	return 0;
}
