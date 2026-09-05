#include <stdio.h>
#include <string.h>
#include <xrt.h>



/*
 * 范例：crypto/session —— 原语组合：一次完整临时会话的建立
 * ----------------------------------------------------------------
 * 演示 API（前序范例的原语组合成真实协议形态）：
 *   xrtX25519KeyPair/Shared      双方交换 → 共享秘密
 *   双方公钥拼接 Transcript       派生上下文绑定会话身份
 *   xrtHkdfSha256                秘密 + 盐 + 上下文 → 会话密钥
 *   xrtChaCha20Poly1305Seal/Open 用会话密钥认证加密消息
 * 模块宏：XRT_MODULE_CRYPTO
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/crypto/session/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   session round trip: valid
 *
 * 三个安全设计点（示例代码里有对应注释）：
 *   原始共享秘密不直接加密——必经 HKDF 派生；
 *   公钥入 Transcript——派生绑定双方身份（防未知密钥共享）；
 *   AAD 绑定记录序号——防重排/重放。
 *   这正是 TLS 1.3 的骨架缩微版。
 */


/* 组合 X25519、HKDF-SHA256 和 ChaCha20-Poly1305 建立一次临时会话。 */
int main(void)
{
	static const char Message[] = "authenticated session message";
	static const char Salt[] = "xrt-session-v1";
	static const char Aad[] = "record:0";
	uint8 AlicePrivate[XRT_X25519_PRIVATE_SIZE];
	uint8 AlicePublic[XRT_X25519_PUBLIC_SIZE];
	uint8 BobPrivate[XRT_X25519_PRIVATE_SIZE];
	uint8 BobPublic[XRT_X25519_PUBLIC_SIZE];
	uint8 AliceShared[XRT_X25519_SHARED_SIZE];
	uint8 BobShared[XRT_X25519_SHARED_SIZE];
	uint8 Transcript[XRT_X25519_PUBLIC_SIZE * 2u];
	uint8 SessionKey[XRT_CHACHA20_POLY1305_KEY_SIZE];
	uint8 Nonce[XRT_CHACHA20_POLY1305_NONCE_SIZE] = { 0 };
	uint8 Buffer[96];
	size_t iMessageSize = sizeof(Message) - 1u;
	size_t iSealedSize = iMessageSize + XRT_CHACHA20_POLY1305_OVERHEAD;
	bool bValid = false;

	/* 双方共享秘密必须相同，且对端公钥应进入派生上下文。 */
	if ( !xrtX25519KeyPair(AlicePrivate, AlicePublic) ||
		 !xrtX25519KeyPair(BobPrivate, BobPublic) ||
		 !xrtX25519Shared(AlicePrivate, BobPublic, AliceShared) ||
		 !xrtX25519Shared(BobPrivate, AlicePublic, BobShared) ||
		 !xrtConstTimeEqual(
			AliceShared, BobShared, sizeof(AliceShared)
		) ) {
		goto cleanup;
	}
	memcpy(Transcript, AlicePublic, sizeof(AlicePublic));
	memcpy(
		Transcript + sizeof(AlicePublic), BobPublic, sizeof(BobPublic)
	);

	/* 派生后的会话密钥才进入 AEAD，原始共享秘密不直接使用。 */
	if ( !xrtHkdfSha256(
			Salt,
			sizeof(Salt) - 1u,
			AliceShared,
			sizeof(AliceShared),
			Transcript,
			sizeof(Transcript),
			SessionKey,
			sizeof(SessionKey)
		) ) {
		goto cleanup;
	}

	/* 新临时密钥下的首条记录使用序号零，后续记录必须递增 nonce。 */
	memcpy(Buffer, Message, iMessageSize);
	if ( !xrtChaCha20Poly1305Seal(
			SessionKey,
			Nonce,
			Aad,
			sizeof(Aad) - 1u,
			Buffer,
			iMessageSize,
			Buffer,
			sizeof(Buffer)
		) || !xrtChaCha20Poly1305Open(
			SessionKey,
			Nonce,
			Aad,
			sizeof(Aad) - 1u,
			Buffer,
			iSealedSize,
			Buffer,
			sizeof(Buffer)
		) ) {
		goto cleanup;
	}
	bValid = memcmp(Buffer, Message, iMessageSize) == 0;

cleanup:
	xrtSecureZero(AlicePrivate, sizeof(AlicePrivate));
	xrtSecureZero(BobPrivate, sizeof(BobPrivate));
	xrtSecureZero(AliceShared, sizeof(AliceShared));
	xrtSecureZero(BobShared, sizeof(BobShared));
	xrtSecureZero(SessionKey, sizeof(SessionKey));
	xrtSecureZero(Buffer, sizeof(Buffer));
	printf("session round trip: %s\n", bValid ? "valid" : "invalid");
	return bValid ? 0 : 1;
}
