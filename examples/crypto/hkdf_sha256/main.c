#include <stdio.h>

#include <xrt.h>



/*
 * 范例：crypto/hkdf_sha256 —— HKDF-SHA256：密钥派生一行流
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtHkdfSha256   (salt, IKM, info) → 任意长输出密钥
 * 模块宏：XRT_MODULE_CRYPTO（HKDF 特性）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/crypto/hkdf_sha256/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   184de99cd5c9f1af2dee024de950759b818bce38644013e38c890f4745a9ff8e
 *
 * 三参数各司其职：salt 公开随机值（换 salt 全新密钥）、
 *   IKM 是原始熵（DH 共享秘密）、info 是用途绑定
 *   （"session"/"cookie" 派生出的密钥互不相同——
 *   同源不同用途密钥隔离的标准做法）。
 *   TLS 1.3 全部密钥调度就是 HKDF 驱动的。
 */


/* 一行派生 32 字节会话密钥。 */
int main(void)
{
	uint8 arrKey[32];

	if ( !xrtHkdfSha256(
			"salt", 4, "input key material", 18,
			"session", 7, arrKey, sizeof(arrKey)
		) ) {
		return 1;
	}
	for ( size_t i = 0; i < sizeof(arrKey); i++ ) {
		printf("%02x", (unsigned int)arrKey[i]);
	}
	printf("\n");
	return 0;
}
