#include "../test.h"
#include "../fixtures/x509_vectors.h"



#if !defined(XRT_FEATURE_X509_VERIFY_ED25519)

/* 验证无可用验签后端时保留完整建链原因。 */
static void testX509PathBuildCause(void)
{
	xx509cert Certificate;
	xx509anchor Anchor;
	xx509pathsource Source;
	xx509pathconfig Config;
	xx509pathresult Result;
	xx509pathresult Before;
	const xx509cert* Path[1];

	testRequire(xrtX509Parse(
		X509_VALID_ED25519, sizeof(X509_VALID_ED25519), &Certificate
	) && xrtX509Anchor(&Certificate, &Anchor),
		"X.509 path builder fixture initialization failed");
	Anchor.Name = Certificate.Issuer;
	Anchor.Certificate = (xbytesview) { NULL, 0 };
	memset(&Source, 0, sizeof(Source));
	Source.Anchors = &Anchor;
	Source.AnchorCount = 1u;
	memset(&Config, 0, sizeof(Config));
	Config.Time = Certificate.NotBefore;
	memset(&Result, 0xA5, sizeof(Result));
	Before = Result;
	testRequire(!xrtX509PathBuild(
		&Certificate, &Source, &Config, Path, 1u, &Result
	) && (memcmp(&Result, &Before, sizeof(Result)) == 0) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_PATH_BUILD) &&
		(xrtErrorIs(xrtGetError(), XERR_UNSUPPORTED) != NULL),
		"X.509 path builder lost the validation failure cause");
}

#endif



/* 验证未找到发行者、容量不足和循环候选边界。 */
static void testX509PathBuildBoundaries(void)
{
	static const uint8 EmptyName[] = { 0x30, 0x00 };
	static const uint8 Marker = 1;
	xx509cert Certificate;
	xx509cert Issuer;
	xx509anchor Anchor;
	xx509pathsource Source;
	xx509pathconfig Config;
	xx509pathresult Result;
	const xx509cert* Issuers[1];
	const xx509cert* Path[2];

	testRequire(xrtX509Parse(
		X509_VALID_ED25519, sizeof(X509_VALID_ED25519), &Certificate
	) && xrtX509Anchor(&Certificate, &Anchor),
		"X.509 path builder boundary fixture initialization failed");
	Anchor.Name = (xbytesview) { EmptyName, sizeof(EmptyName) };
	Anchor.Certificate = (xbytesview) { NULL, 0 };
	memset(&Source, 0, sizeof(Source));
	Source.Anchors = &Anchor;
	Source.AnchorCount = 1u;
	memset(&Config, 0, sizeof(Config));
	Config.Time = Certificate.NotBefore;
	testRequire(!xrtX509PathBuild(
		&Certificate, &Source, &Config, Path, 2u, &Result
	) && (xrtErrorKind(xrtGetError()) == XERR_NOT_FOUND) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_PATH_BUILD),
		"missing X.509 issuer chain returned the wrong error");

	Issuer = Certificate;
	Issuer.Raw = (xbytesview) { &Marker, 1u };
	Certificate.Issuer = Issuer.Subject;
	Certificate.Extensions = (xbytesview) { NULL, 0 };
	Issuers[0] = &Issuer;
	Source.Issuers = Issuers;
	Source.IssuerCount = 1u;
	testRequire(!xrtX509PathBuild(
		&Certificate, &Source, &Config, Path, 1u, &Result
	) && (xrtErrorKind(xrtGetError()) == XERR_RANGE) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_PATH_BUILD),
		"undersized X.509 path output was not reported");
}



/* 验证参数错误不发布结果。 */
static void testX509PathBuildArguments(void)
{
	static const uint8 EmptyName[] = { 0x30, 0x00 };
	xx509cert Certificate;
	xx509anchor Anchor;
	xx509pathsource Source;
	xx509pathconfig Config;
	xx509pathresult Result;
	xx509pathresult Before;
	const xx509cert* Shared[1];

	memset(&Result, 0xA5, sizeof(Result));
	Before = Result;
	testRequire(!xrtX509PathBuild(
		NULL, NULL, NULL, NULL, 0, &Result
	) && (memcmp(&Result, &Before, sizeof(Result)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"invalid X.509 path builder input changed its result");

	testRequire(xrtX509Parse(
		X509_VALID_ED25519, sizeof(X509_VALID_ED25519), &Certificate
	) && xrtX509Anchor(&Certificate, &Anchor),
		"X.509 path builder alias fixture initialization failed");
	Anchor.Name = (xbytesview) { EmptyName, sizeof(EmptyName) };
	Anchor.Certificate = (xbytesview) { NULL, 0 };
	Shared[0] = &Certificate;
	Source.Issuers = Shared;
	Source.IssuerCount = 1u;
	Source.Anchors = &Anchor;
	Source.AnchorCount = 1u;
	memset(&Config, 0, sizeof(Config));
	Config.Time = Certificate.NotBefore;
	testRequire(!xrtX509PathBuild(
		&Certificate, &Source, &Config, Shared, 1u, &Result
	) && (Shared[0] == &Certificate) &&
		(memcmp(&Result, &Before, sizeof(Result)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"aliased X.509 candidate and path arrays were accepted");
}



/* 执行 X.509 自动建链底座测试。 */
int main(void)
{

#if !defined(XRT_FEATURE_X509_VERIFY_ED25519)
	testX509PathBuildCause();
#endif

	testX509PathBuildBoundaries();
	testX509PathBuildArguments();
	printf("[PASS] x509_path_build\n");
	return 0;
}
