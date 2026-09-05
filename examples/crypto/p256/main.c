#include <xrt/crypto.h>



/*
 * 范例：crypto/p256 —— P-256 ECDH（NIST 曲线密钥交换）
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtP256KeyPair / P256Shared   与 X25519 对称的 API
 * 模块宏：XRT_MODULE_CRYPTO（ECDSA 特性）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/crypto/p256/main.c -lws2_32 -liphlpapi
 * 预期输出：（静默成功，退出码 0）
 *
 * 与 X25519 的取舍：NIST 曲线合规面广（FIPS 体系）、
 *   与 ECDSA 同曲线可复用密钥材料；性能与实现简洁性
 *   X25519 更优。所有失败路径也执行 SecureZero——
 *   goto cleanup 统一收尾的密钥卫生范式。
 */


/* 生成双方临时密钥，验证共享秘密并在所有返回路径清理敏感栈数据。 */
int main(void)
{
	uint8 PrivateA[XRT_P256_PRIVATE_SIZE] = { 0 };
	uint8 PublicA[XRT_P256_PUBLIC_SIZE];
	uint8 PrivateB[XRT_P256_PRIVATE_SIZE] = { 0 };
	uint8 PublicB[XRT_P256_PUBLIC_SIZE];
	uint8 SharedA[XRT_P256_SHARED_SIZE] = { 0 };
	uint8 SharedB[XRT_P256_SHARED_SIZE] = { 0 };
	bool bSame = false;

	if ( !xrtP256KeyPair(PrivateA, PublicA) ||
		!xrtP256KeyPair(PrivateB, PublicB) ||
		!xrtP256Shared(PrivateA, PublicB, SharedA) ||
		!xrtP256Shared(PrivateB, PublicA, SharedB) ) {
		goto cleanup;
	}
	bSame = xrtConstTimeEqual(SharedA, SharedB, sizeof(SharedA));

cleanup:
	xrtSecureZero(PrivateA, sizeof(PrivateA));
	xrtSecureZero(PrivateB, sizeof(PrivateB));
	xrtSecureZero(SharedA, sizeof(SharedA));
	xrtSecureZero(SharedB, sizeof(SharedB));
	return bSame ? 0 : 1;
}
