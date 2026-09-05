#include <xrt/crypto.h>



/*
 * 范例：crypto/p384 —— P-384 ECDH（高强度 NIST 曲线）
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtP384KeyPair / P384Shared   P-256 的高强度兄弟
 * 模块宏：XRT_MODULE_CRYPTO（ECDSA 特性）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/crypto/p384/main.c -lws2_32 -liphlpapi
 * 预期输出：（静默成功，退出码 0）
 *
 * 与 ECDSA-P384 同曲线：一套密钥材料同时服务
 *   密钥交换与签名（合规系统常见配对）；
 *   全部返回路径统一 SecureZero（密钥卫生范式）。
 */


/* 生成双方临时密钥，验证共享秘密并在所有返回路径清理敏感栈数据。 */
int main(void)
{
	uint8 PrivateA[XRT_P384_PRIVATE_SIZE] = { 0 };
	uint8 PublicA[XRT_P384_PUBLIC_SIZE];
	uint8 PrivateB[XRT_P384_PRIVATE_SIZE] = { 0 };
	uint8 PublicB[XRT_P384_PUBLIC_SIZE];
	uint8 SharedA[XRT_P384_SHARED_SIZE] = { 0 };
	uint8 SharedB[XRT_P384_SHARED_SIZE] = { 0 };
	bool bSame = false;

	if ( !xrtP384KeyPair(PrivateA, PublicA) ||
		!xrtP384KeyPair(PrivateB, PublicB) ||
		!xrtP384Shared(PrivateA, PublicB, SharedA) ||
		!xrtP384Shared(PrivateB, PublicA, SharedB) ) {
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
