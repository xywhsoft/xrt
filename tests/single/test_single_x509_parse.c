#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include "../fixtures/x509_vectors.h"

#include <stdio.h>



/* 验证单头文件中的证书、Name、Extension 和公钥视图。 */
int main(void)
{
	static const uint8 SanOid[] = { 0x55, 0x1D, 0x11 };
	xx509cert Cert;
	xx509ext Extension;
	xx509pubkey PublicKey;

	if ( !xrtX509Parse(
		X509_VALID_ED25519, sizeof(X509_VALID_ED25519), &Cert
	) || (Cert.Version != X509_VERSION_3) ||
		!xrtX509ExtensionFind(&Cert, SanOid, sizeof(SanOid), &Extension) ||
		!Extension.Critical || !xrtX509PublicKey(&Cert, &PublicKey) ||
		(PublicKey.Type != X509_KEY_ED25519) ||
		(PublicKey.Key.Size != 32u) ) {
		return 1;
	}
	printf("[PASS] single-x509-parse\n");
	return 0;
}
