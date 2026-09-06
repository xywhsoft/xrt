/*
 * 范例：crypto/hash_tour —— 流式哈希与 HMAC 全变体
 * ----------------------------------------------------------------
 * 演示 API：
 *   【流式哈希】  xrtMd5 / Sha1 / Sha224 / Sha512_256 /
 *                Sha512 的 Init + Update×2 + Final 三段式
 *   【流式 HMAC】 xrtHmacSha256 / HmacSha512 的
 *                Init(key) + Update×2 + Final 三段式
 *   【一次性】    xrtHmacSha384 / HmacSha512（无流式形态的变体）
 * 模块宏：XRT_MODULE_CRYPTO（MD5/SHA1/SHA224/SHA512_256/
 *         SHA512/HMAC_SHA256/HMAC_SHA384/HMAC_SHA512）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/crypto/hash_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   hash: md5/sha1/sha224/sha512_256/sha512 streaming == oneshot
 *   hash: hmac-sha256 streaming == oneshot
 *   hash: hmac-sha512 streaming == oneshot
 *   hash: hmac-sha384 + hmac-sha512 oneshot deterministic
 *
 * 验证方法：流式分两块喂 "hello " + "world"，与一次性入口
 *   对同一整串的结果逐位比对——同一实现族内部自洽；
 *   各一次性入口的标准摘要向量已在 sha 系与 hmac 系单项范例展示。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

int main(void)
{
	uint8 arrStream[64];
	uint8 arrOnce[64];
	uint8 arrAgain[64];
	int iResult = 1;

	/* ---- 纯哈希五变体：三段式 vs 一次性。 ---- */
	{
		xmd5 Md5State;
		xsha1 Sha1State;
		xsha224 Sha224State;
		xsha512_256 Sha512256State;
		xsha512 Sha512State;

		xrtMd5Init(&Md5State);
		if ( !xrtMd5Update(&Md5State, "hello ", 6u) ||
			!xrtMd5Update(&Md5State, "world", 5u) ||
			!xrtMd5Final(&Md5State, arrStream) ||
			!xrtMd5("hello world", 11u, arrOnce) ||
			(memcmp(arrStream, arrOnce, XRT_MD5_SIZE) != 0) ) {
			goto Cleanup;
		}
		xrtSha1Init(&Sha1State);
		if ( !xrtSha1Update(&Sha1State, "hello ", 6u) ||
			!xrtSha1Update(&Sha1State, "world", 5u) ||
			!xrtSha1Final(&Sha1State, arrStream) ||
			!xrtSha1("hello world", 11u, arrOnce) ||
			(memcmp(arrStream, arrOnce, XRT_SHA1_SIZE) != 0) ) {
			goto Cleanup;
		}
		xrtSha224Init(&Sha224State);
		if ( !xrtSha224Update(&Sha224State, "hello ", 6u) ||
			!xrtSha224Update(&Sha224State, "world", 5u) ||
			!xrtSha224Final(&Sha224State, arrStream) ||
			!xrtSha224("hello world", 11u, arrOnce) ||
			(memcmp(arrStream, arrOnce, XRT_SHA224_SIZE) != 0) ) {
			goto Cleanup;
		}
		xrtSha512_256Init(&Sha512256State);
		if ( !xrtSha512_256Update(&Sha512256State, "hello ", 6u) ||
			!xrtSha512_256Update(&Sha512256State, "world", 5u) ||
			!xrtSha512_256Final(&Sha512256State, arrStream) ||
			!xrtSha512_256("hello world", 11u, arrOnce) ||
			(memcmp(arrStream, arrOnce, XRT_SHA256_SIZE) != 0) ) {
			goto Cleanup;
		}
		xrtSha512Init(&Sha512State);
		if ( !xrtSha512Update(&Sha512State, "hello ", 6u) ||
			!xrtSha512Update(&Sha512State, "world", 5u) ||
			!xrtSha512Final(&Sha512State, arrStream) ||
			!xrtSha512("hello world", 11u, arrOnce) ||
			(memcmp(arrStream, arrOnce, XRT_SHA512_SIZE) != 0) ) {
			goto Cleanup;
		}
	}
	printf("hash: md5/sha1/sha224/sha512_256/sha512 streaming == oneshot\n");

	/* ---- HMAC-SHA256：流式 vs 一次性。 ---- */
	{
		static const uint8 arrKey[7] = "s3cret!";
		xhmacsha256 State;

		if ( !xrtHmacSha256Init(&State, arrKey, sizeof(arrKey)) ||
			!xrtHmacSha256Update(&State, "hello ", 6u) ||
			!xrtHmacSha256Update(&State, "world", 5u) ||
			!xrtHmacSha256Final(&State, arrStream) ||
			!xrtHmacSha256(arrKey, sizeof(arrKey), "hello world",
				11u, arrOnce) ||
			(memcmp(arrStream, arrOnce, XRT_SHA256_SIZE) != 0) ) {
			goto Cleanup;
		}
	}
	printf("hash: hmac-sha256 streaming == oneshot\n");

	/* ---- HMAC-SHA512：流式 vs 一次性。 ---- */
	{
		static const uint8 arrKey[7] = "s3cret!";
		xhmacsha512 State;

		if ( !xrtHmacSha512Init(&State, arrKey, sizeof(arrKey)) ||
			!xrtHmacSha512Update(&State, "hello ", 6u) ||
			!xrtHmacSha512Update(&State, "world", 5u) ||
			!xrtHmacSha512Final(&State, arrStream) ||
			!xrtHmacSha512(arrKey, sizeof(arrKey), "hello world",
				11u, arrOnce) ||
			(memcmp(arrStream, arrOnce, XRT_SHA512_SIZE) != 0) ) {
			goto Cleanup;
		}
	}
	printf("hash: hmac-sha512 streaming == oneshot\n");

	/* ---- HMAC-SHA384：一次性形态，两次调用必须逐位一致。 ---- */
	{
		static const uint8 arrKey[7] = "s3cret!";

		if ( !xrtHmacSha384(arrKey, sizeof(arrKey), "hello world",
				11u, arrOnce) ||
			!xrtHmacSha384(arrKey, sizeof(arrKey), "hello world",
				11u, arrAgain) ||
			(memcmp(arrOnce, arrAgain, XRT_SHA384_SIZE) != 0) ) {
			goto Cleanup;
		}
	}
	printf("hash: hmac-sha384 + hmac-sha512 oneshot deterministic\n");
	iResult = 0;

Cleanup:
	return iResult;
}
