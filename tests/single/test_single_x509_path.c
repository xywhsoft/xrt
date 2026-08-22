#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include "../fixtures/x509_vectors.h"

#include <stdio.h>



/* 验证单头文件中的 X.509 信任锚和路径分派。 */
int main(void)
{
	xx509cert Certificate;
	xx509anchor Anchor;
	xx509pathconfig Config;
	const xx509cert* Path[1];

	if ( !xrtX509Parse(
		X509_VALID_ED25519, sizeof(X509_VALID_ED25519), &Certificate
	) || !xrtX509Anchor(&Certificate, &Anchor) ) {
		return 1;
	}
	Anchor.Name = Certificate.Issuer;
	Anchor.Certificate = (xbytesview) { NULL, 0 };
	memset(&Config, 0, sizeof(Config));
	Config.Time = Certificate.NotBefore;
	Path[0] = &Certificate;
	if ( xrtX509PathValidate(Path, 1u, &Anchor, &Config) ||
		(xrtErrorCode(xrtGetError()) != X509_ERROR_SIGNATURE) ) {
		return 1;
	}
	printf("[PASS] single-x509-path\n");
	return 0;
}
