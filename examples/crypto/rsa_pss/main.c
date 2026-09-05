#include "../rsa_fixture.h"

#include <stdio.h>



/*
 * 范例：crypto/rsa_pss —— RSA 全家：原始运算 / 显式盐 / 随机盐 PSS
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtRsaPublic / RsaPrivate     原始模幂（仅演示层次）
 *   xrtRsaPssSignSalt / PssVerify 显式盐签名（协议层可复现）
 *   xrtRsaPssSign                 随机盐便利入口（常用）
 * 模块宏：XRT_MODULE_CRYPTO（RSA 特性）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/crypto/rsa_pss/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   RSA-PSS: valid
 *
 * 三层演示刻意分层：原始运算只证明数学底座
 *   （明文不得这样加密——无填充即无安全）；
 *   PSS 是现代标准（TLS 1.3 唯一 RSA 签名模式）。
 *   显式盐适合测试向量/确定性协议；随机盐为默认。
 */


/* 展示 RSA 原始运算、显式盐 PSS 和随机盐便利入口。 */
int main(void)
{
	__xrt_example_rsa_fixture Fixture;
	uint8 Raw[128] = { 0 };
	uint8 Cipher[128];
	uint8 Plain[128];
	uint8 Signature[128];
	bool bValid = false;

	/* 原始运算只用于说明底层层次，普通消息不得直接这样加密。 */
	Raw[sizeof(Raw) - 1u] = 2u;
	if ( !__xrtExampleRsaInit(&Fixture) ||
		 !xrtRsaPublic(
			&Fixture.Key.Public, Raw, sizeof(Raw), Cipher
		 ) || !xrtRsaPrivate(
			&Fixture.Key, Cipher, sizeof(Cipher), Plain
		 ) || !xrtConstTimeEqual(Raw, Plain, sizeof(Raw)) ) {
		goto cleanup;
	}

	/* 显式盐入口适合协议层，随机盐入口覆盖常见签名路径。 */
	if ( !xrtRsaPssSignSalt(
			&Fixture.Key,
			XCRYPTO_HASH_SHA256,
			XCRYPTO_HASH_SHA256,
			Fixture.Salt,
			sizeof(Fixture.Salt),
			Fixture.Hash,
			Signature
		) || !xrtRsaPssVerify(
			&Fixture.Key.Public,
			XCRYPTO_HASH_SHA256,
			XCRYPTO_HASH_SHA256,
			sizeof(Fixture.Salt),
			Fixture.Hash,
			Signature,
			sizeof(Signature)
		) || !xrtRsaPssSign(
			&Fixture.Key,
			XCRYPTO_HASH_SHA256,
			XCRYPTO_HASH_SHA256,
			Fixture.Hash,
			Signature
		) || !xrtRsaPssVerify(
			&Fixture.Key.Public,
			XCRYPTO_HASH_SHA256,
			XCRYPTO_HASH_SHA256,
			XRT_RSA_PSS_SALT_ANY,
			Fixture.Hash,
			Signature,
			sizeof(Signature)
		) ) {
		goto cleanup;
	}
	bValid = true;

cleanup:
	xrtSecureZero(&Fixture, sizeof(Fixture));
	xrtSecureZero(Plain, sizeof(Plain));
	xrtSecureZero(Signature, sizeof(Signature));
	printf("RSA-PSS: %s\n", bValid ? "valid" : "invalid");
	return bValid ? 0 : 1;
}
