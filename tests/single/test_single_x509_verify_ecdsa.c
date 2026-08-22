#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件中的 ECDSA 分派与严格 DER 错误路径。 */
int main(void)
{
	static const uint8 Der[] = {
		0x30, 0x06, 0x02, 0x01, 0, 0x02, 0x01, 0
	};
	uint8 Private[XRT_P256_PRIVATE_SIZE] = { 0 };
	uint8 Public[XRT_P256_PUBLIC_SIZE];
	xx509signature Scheme;
	xx509pubkey PublicKey;

	memset(&Scheme, 0, sizeof(Scheme));
	memset(&PublicKey, 0, sizeof(PublicKey));
	Private[sizeof(Private) - 1u] = 1;
	Scheme.Type = X509_SIGNATURE_ECDSA;
	Scheme.Hash = X509_HASH_SHA256;
	PublicKey.Type = X509_KEY_EC;
	PublicKey.Curve = X509_CURVE_P256;
	PublicKey.Key = (xbytesview) { Public, sizeof(Public) };
	return (!xrtP256Public(Private, Public) ||
		xrtX509SignatureVerify(
			&Scheme,
			(xbytesview) { NULL, 0 },
			(xbytesview) { Der, sizeof(Der) },
			&PublicKey
		) || (xrtErrorCode(xrtGetError()) != X509_ERROR_SIGNATURE)) ? 1 : 0;
}
