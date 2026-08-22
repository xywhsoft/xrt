#include "../test.h"
#include "../fixtures/x509_crl_vectors.h"



typedef struct test_tls_verify_policy {
	xtime Time;
	const void* CrlData;
	size_t CrlSize;
	size_t Calls;
} test_tls_verify_policy;



/* 旧版 TLS 回归资产中的吊销叶证书，保留 Base64 以避免重复维护 DER。 */
static const char TEST_TLS_VERIFY_POLICY_LEAF[] =
	"MIIDBTCCAe2gAwIBAgICIAIwDQYJKoZIhvcNAQELBQAwHDEaMBgGA1UEAwwRWFJUIENSTCBUZXN0IFJvb3QwHhcNMjYwNDA3MDAwMDAwWhcNMzYwNDA1MD"
	"AwMDAwWjAbMRkwFwYDVQQDDBByZXZva2VkLnRlc3QueHJ0MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA03btCP3359H348Ae7ZWXD6qWX8K8"
	"qk3hXxVdXreyMeLslGKCHSJYL1uxVgcjdOt39e4vcGk0MaFbumESqloj3uKCCD5LqFxm16IyIhGxibqDvYm/6vOF5N/oIFWzmpozxmxPWUYWCgIQP+0hWx"
	"r2YZUTVhJKRpqxySkcpEU88Q34BRZjgToGYeyH/78vVqsPBEap/Wqg8RzJoM/oFlBgeUoQZYciXADrErF7KolILMWxY6M6XxMjJciX2oNgDoljb7abL+wT"
	"Z+1GgI2mJRbkHam8dNdSwrPGuxGTRIht1oGgFfb1Ttbb1xy4fSnBNdQjAbhE9ogp5frcUos7evTbpwIDAQABo1IwUDAMBgNVHRMBAf8EAjAAMA4GA1UdDw"
	"EB/wQEAwIFoDATBgNVHSUEDDAKBggrBgEFBQcDATAbBgNVHREEFDASghByZXZva2VkLnRlc3QueHJ0MA0GCSqGSIb3DQEBCwUAA4IBAQCbzHe9iZDxdCp9"
	"qDBlBf35VNkWje1qYhxzGKacK4bQoBQyBrSJj5kxm1GN9X7MhrGF8tVkwTGHtLXcphd3SHUE/AaNcQqx5jALVb33vxvs/rNNHwLwv3wF7PMD3+p335ITZQ"
	"n/YAO/1q/o3u8mFnle3fa5ef6+sKHYigUsnz2tRO2DjI/gvc2UgtmXflHWf1v2izuSwNkFuB/EYbQJIiQWqwN4hZQwOJ1gM9NXtUNSj2QvXy4M9T35UGRl"
	"Cg84uSCgCiDvdQCUqsuuepSpytP1/xNvgMGrfCOz1sv1GeHKakL1f37fx1TV5tDs8Sz27PiezdlkXTj6RYFhCc3K0lWy";



/* 返回证书和 CRL 发布窗口内的确定时间。 */
static xtime testTlsVerifyPolicyTime(ptr pContext)
{
	return ((test_tls_verify_policy*)pContext)->Time;
}



/* 在已经验证的路径上组合调用方提供的 CRL 策略。 */
static bool testTlsVerifyPolicyCrl(
	const xtlsverifiedpeer* pVerified,
	ptr pContext
)
{
	test_tls_verify_policy* pState = (test_tls_verify_policy*)pContext;
	xx509cert Issuer;
	xx509crl Crl;
	xx509crlconfig Config;
	xx509crlvalid Valid;
	xx509revocation Revocation;
	xx509result Result;

	testRequire((pVerified != NULL) && (pVerified->Peer != NULL) &&
		(pVerified->Path != NULL) && (pVerified->PathCount == 1u) &&
		(pVerified->Path[0] == &pVerified->Peer->Certificates[0]) &&
		(pVerified->Anchor != NULL) &&
		(pVerified->Anchor->Certificate.Data != NULL),
		"TLS verified path policy view mismatch");
	pState->Calls++;
	if ( pState->CrlData == NULL ) {
		return true;
	}
	if ( !xrtX509Parse(
		pVerified->Anchor->Certificate.Data,
		pVerified->Anchor->Certificate.Size,
		&Issuer
	) || !xrtX509CrlParse(
		pState->CrlData, pState->CrlSize, &Crl
	) ) {
		return false;
	}
	memset(&Config, 0, sizeof(Config));
	Config.Time = pVerified->Peer->Time;
	if ( !xrtX509CrlValidate(&Crl, &Issuer, &Config, &Valid) ) {
		return false;
	}
	Result = xrtX509CrlCheck(
		&Valid, pVerified->Path[0], &Revocation
	);
	return (Result == X509_VALUE) &&
		(Revocation.State == X509_REVOCATION_GOOD);
}



/* 验证默认 TLS 路径可以自然组合无 CRL、空 CRL、吊销和过期策略。 */
int main(void)
{
	test_tls_verify_policy State;
	xtlsverifierconfig Config;
	xtlsverifier* pVerifier;
	xtlspeer Peer;
	xx509store* pStore;
	xx509cert Certificate;
	bytes pLeaf;
	size_t iLeafSize;

	memset(&State, 0, sizeof(State));
	testRequire(xrtDateTime(
		2026, 4, 9, 0, 0, 0, 0, &State.Time
	), "TLS CRL policy time creation failed");
	pLeaf = xrtBase64DecodeNew(
		TEST_TLS_VERIFY_POLICY_LEAF,
		sizeof(TEST_TLS_VERIFY_POLICY_LEAF) - 1u,
		&iLeafSize,
		NULL
	);
	testRequire((pLeaf != NULL) && xrtX509Parse(
		pLeaf, iLeafSize, &Certificate
	), "TLS CRL policy leaf fixture failed to decode");
	pStore = xrtX509StoreCreate();
	testRequire((pStore != NULL) && (xrtX509StoreAdd(
		pStore, X509_CRL_LEGACY_ROOT, sizeof(X509_CRL_LEGACY_ROOT)
	) == X509_VALUE), "TLS CRL policy trust store setup failed");
	Peer.Role = XTLS_SERVER;
	Peer.Name = XRT_STR_LITERAL("revoked.test.xrt");
	Peer.Time = State.Time;
	Peer.Certificates = &Certificate;
	Peer.CertificateCount = 1u;
	testRequire(xrtTlsPeerVerify(
		&Peer, pStore, testTlsVerifyPolicyCrl, &State
	) && (State.Calls == 1u),
		"direct TLS verified-path policy failed");
	Peer.Name = XRT_STR_LITERAL("example.com");
	testRequire(!xrtTlsPeerVerify(
		&Peer, pStore, testTlsVerifyPolicyCrl, &State
	) && (State.Calls == 1u),
		"TLS policy ran before identity verification");
	xrtClearError();
	xrtTlsVerifierConfigInit(&Config);
	Config.Store = pStore;
	Config.Policy = testTlsVerifyPolicyCrl;
	Config.Time = testTlsVerifyPolicyTime;
	Config.Context = &State;
	pVerifier = xrtTlsVerifierCreate(&Config);
	xrtX509StoreFree(pStore);
	testRequire(pVerifier != NULL, "TLS CRL policy verifier creation failed");

	testRequire(xrtTlsVerifierVerify(
		pVerifier, XTLS_SERVER, XRT_STR_LITERAL("revoked.test.xrt"),
		&Certificate, 1u
	) && (State.Calls == 2u),
		"TLS policy without CRL rejected a valid path");
	State.CrlData = X509_CRL_LEGACY_EMPTY;
	State.CrlSize = sizeof(X509_CRL_LEGACY_EMPTY);
	testRequire(xrtTlsVerifierVerify(
		pVerifier, XTLS_SERVER, XRT_STR_LITERAL("revoked.test.xrt"),
		&Certificate, 1u
	) && (State.Calls == 3u),
		"TLS policy rejected an empty valid CRL");
	State.CrlData = X509_CRL_LEGACY_REVOKED;
	State.CrlSize = sizeof(X509_CRL_LEGACY_REVOKED);
	testRequire(!xrtTlsVerifierVerify(
		pVerifier, XTLS_SERVER, XRT_STR_LITERAL("revoked.test.xrt"),
		&Certificate, 1u
	) && (State.Calls == 4u) &&
		(xrtErrorKind(xrtGetError()) == XERR_PERMISSION) &&
		(xrtErrorCode(xrtGetError()) == XTLS_ERROR_VERIFY),
		"TLS policy accepted a revoked leaf certificate");
	xrtClearError();
	State.CrlData = X509_CRL_LEGACY_EXPIRED;
	State.CrlSize = sizeof(X509_CRL_LEGACY_EXPIRED);
	testRequire(!xrtTlsVerifierVerify(
		pVerifier, XTLS_SERVER, XRT_STR_LITERAL("revoked.test.xrt"),
		&Certificate, 1u
	) && (State.Calls == 5u) &&
		(xrtErrorCode(xrtGetError()) == XTLS_ERROR_VERIFY) &&
		(xrtErrorCause(xrtGetError()) != NULL),
		"TLS policy accepted an expired CRL or lost its cause");

	xrtClearError();
	xrtTlsVerifierRelease(pVerifier);
	xrtFree(pLeaf);
	printf("[PASS] tls_verify_policy_rsa\n");
	return 0;
}
