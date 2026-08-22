#include "../test.h"
#include "../fixtures/tls_identity_legacy.h"



/* 定位 PKCS#1 模数并修改一个不影响 DER 结构的内容字节。 */
static bool testTlsIdentityMutateModulus(uint8* pDer, size_t iSize)
{
	xdercursor Root;
	xdercursor Fields;
	xdervalue Sequence;
	xdervalue Version;
	xdervalue Modulus;
	xbytesview Bytes;

	if ( !xrtDerInit(&Root, pDer, iSize) ||
		(xrtDerRead(&Root, &Sequence) != XDER_VALUE) ||
		!xrtDerEnter(&Sequence, &Fields) ||
		(xrtDerRead(&Fields, &Version) != XDER_VALUE) ||
		(xrtDerRead(&Fields, &Modulus) != XDER_VALUE) ||
		!xrtDerUnsigned(&Modulus, &Bytes) || (Bytes.Size < 4u) ) {
		return false;
	}
	((bytes)Bytes.Data)[Bytes.Size - 2u] ^= 0x01;
	return true;
}



/* RSA 身份拒绝不匹配公钥、尾随数据、损坏 CRT 和非法参数。 */
int main(void)
{
	uint8 PrivateDer[2049];
	uint8 Original[2049];
	size_t iPrivateSize = 0;
	xbytesview Chain = {
		X509_LEGACY_RSA_CERT, sizeof(X509_LEGACY_RSA_CERT)
	};
	xtlsidentity* pIdentity;

	testRequire(testTlsIdentityLegacyKey(
		PrivateDer, sizeof(PrivateDer) - 1u, &iPrivateSize
	), "RSA identity negative fixture decode failed");
	memcpy(Original, PrivateDer, iPrivateSize);
	testRequire(testTlsIdentityMutateModulus(PrivateDer, iPrivateSize),
		"RSA identity modulus mutation failed");
	testRequire(xrtTlsIdentityRsa(
		&Chain, 1u, (xbytesview) { PrivateDer, iPrivateSize }
	) == NULL, "RSA identity accepted a mismatched private key");
	xrtClearError();
	memcpy(PrivateDer, Original, iPrivateSize);
	PrivateDer[iPrivateSize] = 0;
	testRequire(xrtTlsIdentityRsa(
		&Chain, 1u, (xbytesview) { PrivateDer, iPrivateSize + 1u }
	) == NULL, "RSA identity accepted trailing private-key bytes");
	xrtClearError();
	memcpy(PrivateDer, Original, iPrivateSize);
	PrivateDer[iPrivateSize - 1u] ^= 0x01;
	pIdentity = xrtTlsIdentityRsa(
		&Chain, 1u, (xbytesview) { PrivateDer, iPrivateSize }
	);
	testRequire(pIdentity == NULL,
		"RSA identity accepted inconsistent CRT parameters");
	testRequire(memcmp(Original, PrivateDer, iPrivateSize - 1u) == 0,
		"RSA identity failure modified caller private-key storage");
	testRequire(xrtTlsIdentityRsa(NULL, 0, (xbytesview) { NULL, 0 }) == NULL,
		"RSA identity accepted empty inputs");
	return 0;
}
