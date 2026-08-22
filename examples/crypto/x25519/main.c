#include <stdio.h>
#include <xrt.h>



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
