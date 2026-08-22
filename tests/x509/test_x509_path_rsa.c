#include "../test.h"
#include "../fixtures/x509_legacy_cert.h"
#include "../fixtures/x509_path_legacy.h"



/* RSA 路径测试回调接受一个应用自定义 critical 扩展。 */
static xx509result testRsaCritical(
	const xx509cert* pCertificate,
	const xx509ext* pExtension,
	size_t iDepth,
	ptr pUserData
)
{
	(void)pCertificate;
	(void)pExtension;
	(void)pUserData;
	return iDepth == 0 ? X509_VALUE : X509_ERROR;
}



/* 验证真实旧版自签名证书作为目标和独立信任锚。 */
static void testX509PathRsaDirect(
	xx509cert* pCertificate,
	xx509anchor* pAnchor,
	xx509pathconfig* pConfig
)
{
	const xx509cert* Path[1];

	testRequire(xrtX509Parse(
		X509_LEGACY_RSA_CERT, sizeof(X509_LEGACY_RSA_CERT), pCertificate
	) && xrtX509Anchor(pCertificate, pAnchor),
		"X.509 RSA path fixture initialization failed");
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->Time = pCertificate->NotBefore;
	pAnchor->Certificate = (xbytesview) { NULL, 0 };
	Path[0] = pCertificate;
	testRequire(xrtX509PathValidate(
		Path, 1u, pAnchor, pConfig
	), "real legacy RSA certificate path validation failed");

	pConfig->Time = pCertificate->NotAfter + 1;
	testRequire(!xrtX509PathValidate(Path, 1u, pAnchor, pConfig) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_PATH),
		"expired RSA certificate path was accepted");
	pConfig->Time = pCertificate->NotBefore;
}



/* 验证从旧版 TLS 测试继承的真实叶证书、中间 CA 和独立根锚。 */
static void testX509PathRsaChain(void)
{
	static const uint8 ServerAuthOid[] = {
		0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x01
	};
	xx509cert Leaf;
	xx509cert Intermediate;
	xx509cert Root;
	xx509anchor Anchor;
	xx509pathconfig Config;
	xx509basicconstraints Constraints;
	const xx509cert* Path[3];

	testRequire(xrtX509Parse(
		X509_PATH_LEAF, sizeof(X509_PATH_LEAF), &Leaf
	) && xrtX509Parse(
		X509_PATH_INTERMEDIATE, sizeof(X509_PATH_INTERMEDIATE), &Intermediate
	) && xrtX509Parse(
		X509_PATH_ROOT, sizeof(X509_PATH_ROOT), &Root
	) && xrtX509Anchor(&Root, &Anchor),
		"legacy X.509 RSA chain initialization failed");
	testRequire(
		(xrtX509IssuerMatch(&Leaf, &Intermediate) == X509_VALUE) &&
		(xrtX509IssuerMatch(&Intermediate, &Root) == X509_VALUE),
		"legacy X.509 RSA chain issuer selection failed"
	);
	testRequire(
		(xrtX509BasicConstraints(&Intermediate, &Constraints) == X509_VALUE) &&
		Constraints.CA && Constraints.HasPathLimit &&
		(Constraints.PathLimit == 0),
		"legacy X.509 intermediate pathLen constraint changed"
	);

	memset(&Config, 0, sizeof(Config));
	Config.Time = Leaf.NotBefore;
	Config.Flags = X509_PATH_REQUIRE_KEY_USAGE |
		X509_PATH_REQUIRE_PURPOSE;
	Config.KeyUsage = X509_USAGE_DIGITAL_SIGNATURE;
	Config.Purpose = (xbytesview) {
		ServerAuthOid, sizeof(ServerAuthOid)
	};
	Path[0] = &Leaf;
	Path[1] = &Intermediate;
	testRequire(xrtX509PathValidate(Path, 2u, &Anchor, &Config),
		"legacy X.509 RSA chain validation failed");

	Path[2] = &Root;
	testRequire(!xrtX509PathValidate(Path, 3u, &Anchor, &Config) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_PATH),
		"trust anchor certificate was accepted inside the certification path");
}



/* 验证中间 CA、发行者名称、用途和自定义 critical 扩展约束。 */
static void testX509PathRsaConstraints(
	const xx509cert* pCertificate,
	const xx509anchor* pAnchor,
	xx509pathconfig* pConfig
)
{
	static const uint8 Marker = 1;
	static const uint8 EmptyName[] = { 0x30, 0x00 };
	static const uint8 ServerAuthOid[] = {
		0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x01
	};
	static const uint8 UnknownCritical[] = {
		0x30, 0x0D,
		0x30, 0x0B,
		0x06, 0x02, 0x2A, 0x03,
		0x01, 0x01, 0xFF,
		0x04, 0x02, 0x05, 0x00
	};
	xx509cert Child = *pCertificate;
	const xx509cert* Path[2];

	Child.Raw = (xbytesview) { &Marker, 1u };
	Path[0] = &Child;
	Path[1] = pCertificate;
	testRequire(!xrtX509PathValidate(Path, 2u, pAnchor, pConfig) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_PATH_CONSTRAINT),
		"non-CA intermediate certificate was accepted");

	Child = *pCertificate;
	Child.Issuer = (xbytesview) { EmptyName, sizeof(EmptyName) };
	Path[0] = &Child;
	testRequire(!xrtX509PathValidate(Path, 1u, pAnchor, pConfig) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_ISSUER),
		"mismatched RSA certificate issuer was accepted");

	Path[0] = pCertificate;
	pConfig->Purpose = (xbytesview) {
		ServerAuthOid, sizeof(ServerAuthOid)
	};
	pConfig->Flags = X509_PATH_REQUIRE_PURPOSE;
	testRequire(!xrtX509PathValidate(Path, 1u, pAnchor, pConfig) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_PURPOSE),
		"required absent ExtendedKeyUsage was accepted");
	pConfig->Flags = 0;
	pConfig->Purpose = (xbytesview) { NULL, 0 };

	Child = *pCertificate;
	Child.Extensions = (xbytesview) {
		UnknownCritical, sizeof(UnknownCritical)
	};
	Path[0] = &Child;
	pConfig->Critical = testRsaCritical;
	testRequire(xrtX509PathValidate(Path, 1u, pAnchor, pConfig),
		"custom critical extension handler did not preserve valid RSA path");
	pConfig->Critical = NULL;
}



/* 执行 RSA 路径的真实签名、约束和零分配验证。 */
int main(void)
{
	xx509cert Certificate;
	xx509anchor Anchor;
	xx509pathconfig Config;

	testX509PathRsaDirect(&Certificate, &Anchor, &Config);
	testX509PathRsaChain();
	testX509PathRsaConstraints(&Certificate, &Anchor, &Config);
	printf("[PASS] x509_path_rsa\n");
	return 0;
}
