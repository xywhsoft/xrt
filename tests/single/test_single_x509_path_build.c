#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include "../fixtures/x509_vectors.h"

#include <stdio.h>



/* 验证单头文件中的 X.509 自动建链和原因链。 */
int main(void)
{
	xx509cert Certificate;
	xx509anchor Anchor;

#if !defined(XRT_FEATURE_X509_VERIFY_ED25519)
	xx509pathsource Source;
	xx509pathconfig Config;
	xx509pathresult Result;
	const xx509cert* Path[1];
#endif

	if ( !xrtX509Parse(
		X509_VALID_ED25519, sizeof(X509_VALID_ED25519), &Certificate
	) || !xrtX509Anchor(&Certificate, &Anchor) ) {
		return 1;
	}

#if !defined(XRT_FEATURE_X509_VERIFY_ED25519)
	Anchor.Name = Certificate.Issuer;
	Anchor.Certificate = (xbytesview) { NULL, 0 };
	memset(&Source, 0, sizeof(Source));
	Source.Anchors = &Anchor;
	Source.AnchorCount = 1u;
	memset(&Config, 0, sizeof(Config));
	Config.Time = Certificate.NotBefore;
	if ( xrtX509PathBuild(
		&Certificate, &Source, &Config, Path, 1u, &Result
	) || (xrtErrorCode(xrtGetError()) != X509_ERROR_PATH_BUILD) ||
		(xrtErrorIs(xrtGetError(), XERR_UNSUPPORTED) == NULL) ) {
		return 1;
	}
#endif

	printf("[PASS] single-x509-path-build\n");
	return 0;
}
