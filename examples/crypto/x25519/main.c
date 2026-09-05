#include <stdio.h>
#include <xrt.h>



/*
 * 范例：crypto/x25519 —— X25519 密钥交换（ECDH）
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtX25519KeyPair   临时密钥对
 *   xrtX25519Shared    己方私钥 + 对端公钥 → 共享秘密
 * 模块宏：XRT_MODULE_CRYPTO（X25519 特性）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/crypto/x25519/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   shared secret matched: yes
 *
 * 双向 Shared 得到同一秘密即 ECDH 成立。共享秘密
 *   不是最终密钥——必须再过 HKDF 派生（见 session 范例
 *   的完整链路）。私钥与共享秘密全部 SecureZero。
 *   X25519 是 TLS 1.3 首选组（见 tls/key_exchange）。
 */


/* 生成双方临时密钥，并验证它们得到同一个共享秘密。 */
int main(void)
{
	uint8 AlicePrivate[XRT_X25519_PRIVATE_SIZE];
	uint8 AlicePublic[XRT_X25519_PUBLIC_SIZE];
	uint8 BobPrivate[XRT_X25519_PRIVATE_SIZE];
	uint8 BobPublic[XRT_X25519_PUBLIC_SIZE];
	uint8 AliceShared[XRT_X25519_SHARED_SIZE];
	uint8 BobShared[XRT_X25519_SHARED_SIZE];
	bool bSame;

	if ( !xrtX25519KeyPair(AlicePrivate, AlicePublic) ||
		 !xrtX25519KeyPair(BobPrivate, BobPublic) ||
		 !xrtX25519Shared(AlicePrivate, BobPublic, AliceShared) ||
		 !xrtX25519Shared(BobPrivate, AlicePublic, BobShared) ) {
		fprintf(stderr, "X25519 failed: %s\n", xrtErrorMessage(xrtGetError()));
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
