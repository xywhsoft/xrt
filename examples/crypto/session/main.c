#include <stdio.h>
#include <string.h>
#include <xrt.h>



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
