#include <stdio.h>
#include <xrt.h>



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
