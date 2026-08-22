#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件基础分派层明确报告未启用后端。 */
int main(void)
{
	static const uint8 Byte = 1;
	xx509signature Scheme;
	xx509pubkey PublicKey;

	memset(&Scheme, 0, sizeof(Scheme));
	memset(&PublicKey, 0, sizeof(PublicKey));
	Scheme.Type = X509_SIGNATURE_ED448;
	PublicKey.Type = X509_KEY_ED448;
	return xrtX509SignatureVerify(
		&Scheme,
		(xbytesview) { NULL, 0 },
		(xbytesview) { &Byte, 1u },
		&PublicKey
	) || (xrtErrorKind(xrtGetError()) != XERR_UNSUPPORTED);
}
