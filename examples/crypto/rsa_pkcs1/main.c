#include "../rsa_fixture.h"

#include <stdio.h>



/*
 * 范例：crypto/rsa_pkcs1 —— RSA PKCS#1 v1.5 签名（遗留互操作）
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtRsaPkcs1Sign / Verify   签名与验签（严格填充校验）
 *   rsa_fixture.h               1024 位测试密钥夹具
 * 模块宏：XRT_MODULE_CRYPTO（RSA 特性）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/crypto/rsa_pkcs1/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   RSA PKCS#1 v1.5: valid
 *
 * 定位：v1.5 填充 1990 年代设计，Bleichenbacher 系攻击
 *   历史悠久——新协议一律 PSS（见 rsa_pss）；
 *   本入口服务既有 TLS 证书链/旧 JWT 的互操作。
 *   夹具密钥仅供测试，密钥与签名用完即清。
 */


/* 展示严格的 RSA PKCS#1 v1.5 签名与验签。 */
int main(void)
{
	__xrt_example_rsa_fixture Fixture;
	uint8 Signature[128];
	bool bValid = false;

	/* 新协议通常应优先选择 PSS，本入口用于既有协议互操作。 */
	if ( __xrtExampleRsaInit(&Fixture) &&
		 xrtRsaPkcs1Sign(
			&Fixture.Key,
			XCRYPTO_HASH_SHA256,
			Fixture.Hash,
			Signature
		 ) && xrtRsaPkcs1Verify(
			&Fixture.Key.Public,
			XCRYPTO_HASH_SHA256,
			Fixture.Hash,
			Signature,
			sizeof(Signature)
		 ) ) {
		bValid = true;
	}
	xrtSecureZero(&Fixture, sizeof(Fixture));
	xrtSecureZero(Signature, sizeof(Signature));

	printf("RSA PKCS#1 v1.5: %s\n", bValid ? "valid" : "invalid");
	return bValid ? 0 : 1;
}
