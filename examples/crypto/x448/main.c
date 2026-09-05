#include <stdio.h>
#include <xrt.h>



/*
 * 范例：crypto/x448 —— X448 密钥交换（Goldilocks 曲线）
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtX448KeyPair / X448Shared   与 X25519 完全对称的 API
 * 模块宏：XRT_MODULE_CRYPTO（X448 特性）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/crypto/x448/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   shared secret matched: yes
 *
 * 与 X25519 的取舍：密钥更大（56 vs 32 字节）、
 *   运算更慢，换来更高安全余量（~224 位）——
 *   长期保密档案级通信选用；日常默认 X25519。
 */


/* 生成双方临时密钥，并验证它们得到同一个 X448 共享秘密。 */
int main(void)
{
	uint8 AlicePrivate[XRT_X448_PRIVATE_SIZE];
	uint8 AlicePublic[XRT_X448_PUBLIC_SIZE];
	uint8 BobPrivate[XRT_X448_PRIVATE_SIZE];
	uint8 BobPublic[XRT_X448_PUBLIC_SIZE];
	uint8 AliceShared[XRT_X448_SHARED_SIZE];
	uint8 BobShared[XRT_X448_SHARED_SIZE];
	bool bSame;

	if ( !xrtX448KeyPair(AlicePrivate, AlicePublic) ||
		 !xrtX448KeyPair(BobPrivate, BobPublic) ||
		 !xrtX448Shared(AlicePrivate, BobPublic, AliceShared) ||
		 !xrtX448Shared(BobPrivate, AlicePublic, BobShared) ) {
		fprintf(stderr, "X448 failed: %s\n", xrtErrorMessage(xrtGetError()));
		return 1;
	}
	bSame = xrtConstTimeEqual(AliceShared, BobShared, sizeof(AliceShared));
	printf("shared secret matched: %s\n", bSame ? "yes" : "no");
	xrtSecureZero(AlicePrivate, sizeof(AlicePrivate));
	xrtSecureZero(BobPrivate, sizeof(BobPrivate));
	xrtSecureZero(AliceShared, sizeof(AliceShared));
	xrtSecureZero(BobShared, sizeof(BobShared));
	return bSame ? 0 : 1;
}
