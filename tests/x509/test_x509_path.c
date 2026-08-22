#include "../test.h"
#include "../fixtures/x509_name_constraints_vectors.h"
#include "../fixtures/x509_profile_vectors.h"
#include "../fixtures/x509_vectors.h"



/* critical 扩展测试回调只记录调用并明确接受该扩展。 */
static xx509result testAcceptCritical(
	const xx509cert* pCertificate,
	const xx509ext* pExtension,
	size_t iDepth,
	ptr pUserData
)
{
	size_t* pCalls = (size_t*)pUserData;

	(void)pCertificate;
	(void)pExtension;
	testRequire(iDepth == 0, "critical extension callback depth mismatch");
	(*pCalls)++;
	return X509_VALUE;
}



/* 验证信任锚提取和无密码后端时的路径分派错误。 */
static void testX509PathAnchor(void)
{
	xx509cert Certificate;
	xx509anchor Anchor;
	xx509anchor Before;

#if !defined(XRT_FEATURE_X509_VERIFY_ED25519)
	xx509pathconfig Config;
	const xx509cert* Path[1];
#endif

	testRequire(xrtX509Parse(
		X509_VALID_ED25519, sizeof(X509_VALID_ED25519), &Certificate
	), "X.509 path certificate parse failed");
	memset(&Anchor, 0xA5, sizeof(Anchor));
	testRequire(xrtX509Anchor(&Certificate, &Anchor) &&
		(Anchor.PublicKey.Type == X509_KEY_ED25519) &&
		(Anchor.Name.Size != 0),
		"X.509 trust anchor extraction failed");

#if !defined(XRT_FEATURE_X509_VERIFY_ED25519)
	Anchor.Name = Certificate.Issuer;
	Anchor.Certificate = (xbytesview) { NULL, 0 };
	Path[0] = &Certificate;
	memset(&Config, 0, sizeof(Config));
	Config.Time = Certificate.NotBefore;
	testRequire(!xrtX509PathValidate(Path, 1u, &Anchor, &Config) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_SIGNATURE) &&
		(xrtErrorCause(xrtGetError()) != NULL) &&
		(xrtErrorKind(xrtErrorCause(xrtGetError())) == XERR_UNSUPPORTED),
		"X.509 path did not preserve the missing crypto backend cause");
#endif

	memset(&Anchor, 0xA5, sizeof(Anchor));
	Before = Anchor;
	testRequire(!xrtX509Anchor(NULL, &Anchor) &&
		(memcmp(&Anchor, &Before, sizeof(Anchor)) == 0),
		"invalid X.509 trust anchor input changed output");
}



/* 验证重复证书、有效期和未知 critical 扩展处理。 */
static void testX509PathBoundaries(void)
{
	static const uint8 UnknownCritical[] = {
		0x30, 0x0D,
		0x30, 0x0B,
		0x06, 0x02, 0x2A, 0x03,
		0x01, 0x01, 0xFF,
		0x04, 0x02, 0x05, 0x00
	};
	xx509cert Certificate;
	xx509cert Modified;
	xx509anchor Anchor;
	xx509pathconfig Config;
	const xx509cert* Path[2];
	size_t iCalls = 0;

	testRequire(xrtX509Parse(
		X509_VALID_ED25519, sizeof(X509_VALID_ED25519), &Certificate
	) && xrtX509Anchor(&Certificate, &Anchor),
		"X.509 path boundary fixture initialization failed");
	Anchor.Name = Certificate.Issuer;
	Anchor.Certificate = (xbytesview) { NULL, 0 };
	memset(&Config, 0, sizeof(Config));
	Config.Time = Certificate.NotBefore;
	Path[0] = &Certificate;
	Path[1] = &Certificate;
	testRequire(!xrtX509PathValidate(Path, 2u, &Anchor, &Config) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_PATH),
		"duplicate certificate was accepted in X.509 path");

	Config.Time = Certificate.NotBefore - 1;
	testRequire(!xrtX509PathValidate(Path, 1u, &Anchor, &Config) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_PATH),
		"certificate outside validity interval was accepted");

	Modified = Certificate;
	Modified.Extensions = (xbytesview) {
		UnknownCritical, sizeof(UnknownCritical)
	};
	Path[0] = &Modified;
	Config.Time = Certificate.NotBefore;
	testRequire(!xrtX509PathValidate(Path, 1u, &Anchor, &Config) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_CRITICAL_EXTENSION),
		"unsupported critical extension was accepted");

	Config.Critical = testAcceptCritical;
	Config.UserData = &iCalls;
	testRequire(!xrtX509PathValidate(Path, 1u, &Anchor, &Config) &&
		(iCalls == 1u) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_SIGNATURE),
		"accepted custom critical extension did not continue path processing");
}



/* 验证目标用途策略以及可用于路径构建的 AKI/SKI 发行者筛选。 */
static void testX509PathPurposeAndIssuer(void)
{
	static const uint8 ServerAuthOid[] = {
		0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x01
	};
	static const uint8 UnknownPurpose[] = { 0x2A, 0x03 };
	static const uint8 MismatchedAuthority[] = {
		0x30, 0x10,
		0x30, 0x0E,
		0x06, 0x03, 0x55, 0x1D, 0x23,
		0x04, 0x07, 0x30, 0x05, 0x80, 0x03, 0xAA, 0xBB, 0xCC
	};
	xx509cert Certificate;
	xx509cert Issuer;
	xx509cert Child;
	xx509anchor Anchor;
	xx509pathconfig Config;
	const xx509cert* Path[1];

	testRequire(xrtX509Parse(
		X509_PROFILE_VALID, sizeof(X509_PROFILE_VALID), &Certificate
	) && xrtX509Anchor(&Certificate, &Anchor),
		"X.509 path purpose fixture initialization failed");
	Anchor.Name = Certificate.Issuer;
	Anchor.Certificate = (xbytesview) { NULL, 0 };
	memset(&Config, 0, sizeof(Config));
	Config.Time = Certificate.NotBefore;
	Config.KeyUsage = X509_USAGE_DIGITAL_SIGNATURE;
	Config.Purpose = (xbytesview) {
		ServerAuthOid, sizeof(ServerAuthOid)
	};
	Path[0] = &Certificate;
	testRequire(!xrtX509PathValidate(Path, 1u, &Anchor, &Config) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_SIGNATURE),
		"allowed target purpose did not reach signature verification");

	Config.Purpose = (xbytesview) {
		UnknownPurpose, sizeof(UnknownPurpose)
	};
	testRequire(!xrtX509PathValidate(Path, 1u, &Anchor, &Config) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_PURPOSE),
		"disallowed ExtendedKeyUsage purpose was accepted");

	Issuer = Certificate;
	Child = Certificate;
	Child.Issuer = Issuer.Subject;
	testRequire(xrtX509IssuerMatch(&Child, &Issuer) == X509_VALUE,
		"issuer name match without AKI failed");
	Child.Extensions = (xbytesview) {
		MismatchedAuthority, sizeof(MismatchedAuthority)
	};
	testRequire(xrtX509IssuerMatch(&Child, &Issuer) == X509_DONE,
		"mismatched AKI/SKI issuer candidate was accepted");
	Child.Issuer = (xbytesview) { (const uint8*)"\x30\x00", 2u };
	testRequire(xrtX509IssuerMatch(&Child, &Issuer) == X509_DONE,
		"mismatched issuer distinguished name was accepted");
}



/* 验证中间 CA 的 BasicConstraints、Subject、pathLen 和 EKU 契约。 */
static void testX509PathCaProfile(void)
{
	static const uint8 NonCriticalBasic[] = {
		0x30, 0x0E,
		0x30, 0x0C,
		0x06, 0x03, 0x55, 0x1D, 0x13,
		0x04, 0x05, 0x30, 0x03, 0x01, 0x01, 0xFF
	};
	static const uint8 CriticalBasic[] = {
		0x30, 0x11,
		0x30, 0x0F,
		0x06, 0x03, 0x55, 0x1D, 0x13,
		0x01, 0x01, 0xFF,
		0x04, 0x05, 0x30, 0x03, 0x01, 0x01, 0xFF
	};
	static const uint8 PathLimitWithoutUsage[] = {
		0x30, 0x14,
		0x30, 0x12,
		0x06, 0x03, 0x55, 0x1D, 0x13,
		0x01, 0x01, 0xFF,
		0x04, 0x08, 0x30, 0x06, 0x01, 0x01, 0xFF, 0x02, 0x01, 0x00
	};
	static const uint8 ClientAuthCa[] = {
		0x30, 0x26,
		0x30, 0x0F,
		0x06, 0x03, 0x55, 0x1D, 0x13,
		0x01, 0x01, 0xFF,
		0x04, 0x05, 0x30, 0x03, 0x01, 0x01, 0xFF,
		0x30, 0x13,
		0x06, 0x03, 0x55, 0x1D, 0x25,
		0x04, 0x0C, 0x30, 0x0A, 0x06, 0x08,
		0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x02
	};
	static const uint8 EmptyName[] = { 0x30, 0x00 };
	static const uint8 ServerAuthOid[] = {
		0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x01
	};
	xx509cert Certificate;
	xx509cert Ca;
	xx509anchor Anchor;
	xx509pathconfig Config;
	const xx509cert* Path[2];

	testRequire(xrtX509Parse(
		X509_PROFILE_VALID, sizeof(X509_PROFILE_VALID), &Certificate
	) && xrtX509Anchor(&Certificate, &Anchor),
		"X.509 CA profile fixture initialization failed");
	Anchor.Name = Certificate.Issuer;
	Anchor.Certificate = (xbytesview) { NULL, 0 };
	memset(&Config, 0, sizeof(Config));
	Config.Time = Certificate.NotBefore;
	Path[0] = &Certificate;
	Path[1] = &Ca;

	Ca = Certificate;
	Ca.Raw = (xbytesview) {
		NonCriticalBasic, sizeof(NonCriticalBasic)
	};
	Ca.Extensions = Ca.Raw;
	testRequire(!xrtX509PathValidate(Path, 2u, &Anchor, &Config) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_PATH_CONSTRAINT),
		"non-critical intermediate BasicConstraints was accepted");

	Ca.Raw = (xbytesview) { CriticalBasic, sizeof(CriticalBasic) };
	Ca.Extensions = Ca.Raw;
	Ca.Subject = (xbytesview) { EmptyName, sizeof(EmptyName) };
	testRequire(!xrtX509PathValidate(Path, 2u, &Anchor, &Config) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_PATH_CONSTRAINT),
		"empty intermediate CA subject was accepted");

	Ca = Certificate;
	Ca.Raw = (xbytesview) {
		PathLimitWithoutUsage, sizeof(PathLimitWithoutUsage)
	};
	Ca.Extensions = Ca.Raw;
	testRequire(!xrtX509PathValidate(Path, 2u, &Anchor, &Config) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_PATH_CONSTRAINT),
		"CA pathLenConstraint without KeyUsage was accepted");

	Ca.Raw = (xbytesview) { ClientAuthCa, sizeof(ClientAuthCa) };
	Ca.Extensions = Ca.Raw;
	Config.Purpose = (xbytesview) {
		ServerAuthOid, sizeof(ServerAuthOid)
	};
	testRequire(!xrtX509PathValidate(Path, 2u, &Anchor, &Config) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_PURPOSE),
		"intermediate CA EKU did not constrain the target purpose");
}



/* 验证目标、CA、信任锚和 self-issued 中间证书的名称约束传播。 */
static void testX509PathNameConstraints(void)
{
	xx509cert Certificate;
	xx509cert Target;
	xx509cert Intermediate;
	xx509cert Ca;
	xx509anchor Anchor;
	xx509pathconfig Config;
	const xx509cert* Path[3];

	testRequire(xrtX509Parse(
		X509_PROFILE_VALID, sizeof(X509_PROFILE_VALID), &Certificate
	) && xrtX509Anchor(&Certificate, &Anchor),
		"X.509 NameConstraints path fixture initialization failed");
	memset(&Config, 0, sizeof(Config));
	Config.Time = Certificate.NotBefore;
	Anchor.Name = Certificate.Issuer;
	Anchor.Certificate = (xbytesview) { NULL, 0 };

	Target = Certificate;
	Target.Raw = (xbytesview) {
		X509_NAME_CONSTRAINTS_SAN_OUTSIDE,
		sizeof(X509_NAME_CONSTRAINTS_SAN_OUTSIDE)
	};
	Target.Extensions = Target.Raw;
	Ca = Certificate;
	Ca.Raw = (xbytesview) {
		X509_NAME_CONSTRAINTS_CA_EXTENSIONS,
		sizeof(X509_NAME_CONSTRAINTS_CA_EXTENSIONS)
	};
	Ca.Extensions = Ca.Raw;
	Target.Issuer = Ca.Subject;
	Path[0] = &Target;
	Path[1] = &Ca;
	testRequire(!xrtX509PathValidate(Path, 2u, &Anchor, &Config) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_NAME_CONSTRAINTS) &&
		(xrtErrorCause(xrtGetError()) != NULL),
		"CA NameConstraints did not reject a lower DNS name");

	Target.Raw = (xbytesview) {
		X509_NAME_CONSTRAINTS_SAN_ALLOWED,
		sizeof(X509_NAME_CONSTRAINTS_SAN_ALLOWED)
	};
	Target.Extensions = Target.Raw;
	testRequire(!xrtX509PathValidate(Path, 2u, &Anchor, &Config) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_SIGNATURE),
		"permitted CA NameConstraints did not reach signature verification");

	Target = Certificate;
	Target.Raw = (xbytesview) {
		X509_NAME_CONSTRAINTS_EXTENSION,
		sizeof(X509_NAME_CONSTRAINTS_EXTENSION)
	};
	Target.Extensions = Target.Raw;
	Path[0] = &Target;
	testRequire(!xrtX509PathValidate(Path, 1u, &Anchor, &Config) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_NAME_CONSTRAINTS),
		"target certificate NameConstraints extension was accepted");

	Target = Certificate;
	Target.Raw = (xbytesview) {
		X509_NAME_CONSTRAINTS_SAN_OUTSIDE,
		sizeof(X509_NAME_CONSTRAINTS_SAN_OUTSIDE)
	};
	Target.Extensions = Target.Raw;
	Path[0] = &Target;
	testRequire(xrtX509NameConstraintsParse(
		(xbytesview) {
			X509_NAME_CONSTRAINTS_DNS,
			sizeof(X509_NAME_CONSTRAINTS_DNS)
		}, &Anchor.NameConstraints
	), "trust anchor NameConstraints parse failed");
	Anchor.HasNameConstraints = true;
	testRequire(!xrtX509PathValidate(Path, 1u, &Anchor, &Config) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_NAME_CONSTRAINTS),
		"trust anchor NameConstraints did not constrain the target");

	Certificate.Extensions = (xbytesview) {
		X509_NAME_CONSTRAINTS_EXTENSION,
		sizeof(X509_NAME_CONSTRAINTS_EXTENSION)
	};
	testRequire(xrtX509Anchor(&Certificate, &Anchor) &&
		Anchor.HasNameConstraints,
		"trust anchor extraction discarded NameConstraints");
	Anchor.Name = Certificate.Issuer;
	Anchor.Certificate = (xbytesview) { NULL, 0 };

	Target = Certificate;
	Target.Raw = (xbytesview) {
		X509_NAME_CONSTRAINTS_SAN_ALLOWED,
		sizeof(X509_NAME_CONSTRAINTS_SAN_ALLOWED)
	};
	Target.Extensions = Target.Raw;
	Intermediate = Certificate;
	Intermediate.Raw = (xbytesview) {
		X509_NAME_CONSTRAINTS_CA_OUTSIDE,
		sizeof(X509_NAME_CONSTRAINTS_CA_OUTSIDE)
	};
	Intermediate.Extensions = Intermediate.Raw;
	Intermediate.Issuer = Intermediate.Subject;
	Ca = Certificate;
	Ca.Raw = (xbytesview) {
		X509_NAME_CONSTRAINTS_CA_EXTENSIONS,
		sizeof(X509_NAME_CONSTRAINTS_CA_EXTENSIONS)
	};
	Ca.Extensions = Ca.Raw;
	Ca.Subject = Intermediate.Subject;
	Target.Issuer = Intermediate.Subject;
	Path[0] = &Target;
	Path[1] = &Intermediate;
	Path[2] = &Ca;
	testRequire(!xrtX509PathValidate(Path, 3u, &Anchor, &Config) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_SIGNATURE),
		"self-issued intermediate was incorrectly constrained");

	Ca.Subject = Certificate.Issuer;
	Intermediate.Issuer = Ca.Subject;
	testRequire(!xrtX509PathValidate(Path, 3u, &Anchor, &Config) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_NAME_CONSTRAINTS),
		"non-self-issued intermediate escaped NameConstraints");
}



/* 执行 X.509 有序路径底座测试。 */
int main(void)
{
	testX509PathAnchor();
	testX509PathBoundaries();
	testX509PathPurposeAndIssuer();
	testX509PathCaProfile();
	testX509PathNameConstraints();
	printf("[PASS] x509_path\n");
	return 0;
}
