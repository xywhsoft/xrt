#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件中的 Ed25519 分派与错误包装。 */
int main(void)
{
	uint8 Public[XRT_ED25519_PUBLIC_SIZE] = { 0 };
	uint8 Signature[XRT_ED25519_SIGNATURE_SIZE] = { 0 };
	xx509signature Scheme;
	xx509pubkey PublicKey;

	memset(&Scheme, 0, sizeof(Scheme));
	memset(&PublicKey, 0, sizeof(PublicKey));
	Scheme.Type = X509_SIGNATURE_ED25519;
	PublicKey.Type = X509_KEY_ED25519;
	PublicKey.Key = (xbytesview) { Public, sizeof(Public) };
	return xrtX509SignatureVerify(
		&Scheme,
		(xbytesview) { NULL, 0 },
		(xbytesview) { Signature, sizeof(Signature) },
		&PublicKey
	) || (xrtErrorCode(xrtGetError()) != X509_ERROR_SIGNATURE);
}
