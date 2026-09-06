/*
 * 范例：crypto/ecdh_tour —— X25519/X448 密钥交换与 NIST 曲线算术
 * ----------------------------------------------------------------
 * 演示 API：
 *   【X25519】     xrtX25519Public（私钥→公钥）
 *                  xrtX25519（标量乘；双方共享密钥一致）
 *   【X448】       xrtX448Public / xrtX448（同构）
 *   【P-256】      xrtP256Public / Valid / Add / Multiply
 *   【P-384】      xrtP384Public / Valid / Add / Multiply
 * 模块宏：XRT_MODULE_CRYPTO（X25519 + X448 + P256 + P384）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/crypto/ecdh_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   ecdh: x25519 public + shared-secret symmetric ok
 *   ecdh: x448 public + shared-secret symmetric ok
 *   ecdh: p256 public/valid + add == multiply-by-2 ok
 *   ecdh: p384 public/valid + add == multiply-by-2 ok
 *
 * ECDH 对称性：A(sA·G_pubB) == B(sB·G_pubA)；
 *   曲线算术自检：Add(P, P) == Multiply(2, P)，
 *   无需预置基点常量即可验证实现自洽。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

int main(void)
{
	/* 两方私钥（演示用固定值；生产应来自安全随机源）。 */
	static const uint8 arrSecretA[56] = {
		1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u,
		9u, 10u, 11u, 12u, 13u, 14u, 15u, 16u,
		17u, 18u, 19u, 20u, 21u, 22u, 23u, 24u,
		25u, 26u, 27u, 28u, 29u, 30u, 31u, 32u,
		33u, 34u, 35u, 36u, 37u, 38u, 39u, 40u,
		41u, 42u, 43u, 44u, 45u, 46u, 47u, 48u,
		49u, 50u, 51u, 52u, 53u, 54u, 55u, 56u
	};
	static const uint8 arrSecretB[56] = {
		56u, 55u, 54u, 53u, 52u, 51u, 50u, 49u,
		48u, 47u, 46u, 45u, 44u, 43u, 42u, 41u,
		40u, 39u, 38u, 37u, 36u, 35u, 34u, 33u,
		32u, 31u, 30u, 29u, 28u, 27u, 26u, 25u,
		24u, 23u, 22u, 21u, 20u, 19u, 18u, 17u,
		16u, 15u, 14u, 13u, 12u, 11u, 10u, 9u,
		8u, 7u, 6u, 5u, 4u, 3u, 2u, 1u
	};
	uint8 arrPubA[64];
	uint8 arrPubB[64];
	uint8 arrSharedA[64];
	uint8 arrSharedB[64];
	uint8 arrScalarTwo[48];
	uint8 arrPoint[97];
	uint8 arrSum[97];
	uint8 arrDouble[97];
	uint8 arrBad[97];
	int iResult = 1;

	/* ---- X25519：公钥导出 + 双方共享密钥对称。 ---- */
	if ( !xrtX25519Public(arrSecretA, arrPubA) ||
		!xrtX25519Public(arrSecretB, arrPubB) ||
		!xrtX25519(arrSecretA, arrPubB, arrSharedA) ||
		!xrtX25519(arrSecretB, arrPubA, arrSharedB) ||
		(memcmp(arrSharedA, arrSharedB, 32u) != 0) ) {
		goto Cleanup;
	}
	printf("ecdh: x25519 public + shared-secret symmetric ok\n");

	/* ---- X448：56 字节私钥同构闭环。 ---- */
	if ( !xrtX448Public(arrSecretA, arrPubA) ||
		!xrtX448Public(arrSecretB, arrPubB) ||
		!xrtX448(arrSecretA, arrPubB, arrSharedA) ||
		!xrtX448(arrSecretB, arrPubA, arrSharedB) ||
		(memcmp(arrSharedA, arrSharedB, 56u) != 0) ) {
		goto Cleanup;
	}
	printf("ecdh: x448 public + shared-secret symmetric ok\n");

	/* ---- P-256：公钥有效 + 倍点一致 + 非法点被拒。 ---- */
	memset(arrScalarTwo, 0, sizeof(arrScalarTwo));
	arrScalarTwo[31] = 2u; /* 大端 32 字节标量 2。 */
	if ( !xrtP256Public(arrSecretA, arrPoint) ||
		!xrtP256Valid(arrPoint) ||
		!xrtP256Add(arrPoint, arrPoint, arrSum) ||
		!xrtP256Multiply(arrScalarTwo, arrPoint, arrDouble) ||
		(memcmp(arrSum, arrDouble, 65u) != 0) ) {
		goto Cleanup;
	}
	/* 未知点前缀 0xFF → Valid 必须拒绝。 */
	memset(arrBad, 0xFFu, sizeof(arrBad));
	if ( xrtP256Valid(arrBad) ) {
		goto Cleanup;
	}
	printf("ecdh: p256 public/valid + add == multiply-by-2 ok\n");

	/* ---- P-384：48 字节标量 / 97 字节点同构闭环。 ---- */
	/* 清掉 P-256 段写入的字节 31，48 字节标量才恰好为 2。 */
	arrScalarTwo[31] = 0u;
	arrScalarTwo[47] = 2u;
	if ( !xrtP384Public(arrSecretA, arrPoint) ||
		!xrtP384Valid(arrPoint) ||
		!xrtP384Add(arrPoint, arrPoint, arrSum) ||
		!xrtP384Multiply(arrScalarTwo, arrPoint, arrDouble) ||
		(memcmp(arrSum, arrDouble, 97u) != 0) ) {
		goto Cleanup;
	}
	memset(arrBad, 0xFFu, sizeof(arrBad));
	if ( xrtP384Valid(arrBad) ) {
		goto Cleanup;
	}
	printf("ecdh: p384 public/valid + add == multiply-by-2 ok\n");
	iResult = 0;

Cleanup:
	return iResult;
}
