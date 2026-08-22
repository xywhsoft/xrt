#include "../test.h"
#include "../fixtures/x509_legacy_cert.h"



typedef struct test_tls_verify_context {
	xtlsverifydecision Decision;
	xtime Time;
	size_t Calls;
	size_t Releases;
	bool CallbackError;
} test_tls_verify_context;



/* 返回测试指定的信任决策并记录调用次数。 */
static xtlsverifydecision testTlsVerifyCustom(
	const xtlspeer* pPeer,
	ptr pContext
)
{
	test_tls_verify_context* pState =
		(test_tls_verify_context*)pContext;

	testRequire((pPeer != NULL) &&
		(pPeer->CertificateCount == 1u),
		"TLS custom verifier peer view mismatch");
	pState->Calls++;
	if ( pState->CallbackError ) {
		(void)xrtTlsPeerVerify(NULL, NULL, NULL, NULL);
	}
	return pState->Decision;
}



/* 返回固定验证时间。 */
static xtime testTlsVerifyTime(ptr pContext)
{
	return ((test_tls_verify_context*)pContext)->Time;
}



/* 记录验证器最后一个引用释放了调用方上下文。 */
static void testTlsVerifyRelease(ptr pContext)
{
	((test_tls_verify_context*)pContext)->Releases++;
}



/* 自定义验证器必须支持共享引用、固定时钟和明确决策。 */
static void testTlsVerifyCustomContract(void)
{
	test_tls_verify_context State;
	xtlsverifierconfig Config;
	xtlsverifier* pVerifier;
	xtlsverifier* pRetained;
	xx509cert Certificate;

	memset(&State, 0, sizeof(State));
	State.Decision = XTLS_VERIFY_ACCEPT;
	testRequire(xrtX509Parse(
		X509_LEGACY_RSA_CERT, sizeof(X509_LEGACY_RSA_CERT), &Certificate
	), "TLS verifier certificate fixture parsing failed");
	State.Time = Certificate.NotBefore;
	xrtTlsVerifierConfigInit(&Config);
	Config.Verify = testTlsVerifyCustom;
	Config.Time = testTlsVerifyTime;
	Config.Release = testTlsVerifyRelease;
	Config.Context = &State;
	pVerifier = xrtTlsVerifierCreate(&Config);
	testRequire(pVerifier != NULL, "custom TLS verifier creation failed");
	pRetained = xrtTlsVerifierRetain(pVerifier);
	testRequire((pRetained == pVerifier) && xrtTlsVerifierVerify(
		pVerifier, XTLS_SERVER, XRT_STR_LITERAL("localhost"),
		&Certificate, 1u
	) && (State.Calls == 1u),
		"custom TLS verifier accept decision failed");

	State.Decision = XTLS_VERIFY_REJECT;
	testRequire(!xrtTlsVerifierVerify(
		pVerifier, XTLS_SERVER, XRT_STR_LITERAL("localhost"),
		&Certificate, 1u
	) && (xrtErrorKind(xrtGetError()) == XERR_PERMISSION),
		"custom TLS verifier reject decision failed");
	xrtClearError();

	State.Decision = XTLS_VERIFY_ERROR;
	State.CallbackError = false;
	(void)xrtTlsPeerVerify(NULL, NULL, NULL, NULL);
	testRequire(!xrtTlsVerifierVerify(
		pVerifier, XTLS_SERVER, XRT_STR_LITERAL("localhost"),
		&Certificate, 1u
	) && (xrtErrorKind(xrtGetError()) == XERR_INTERNAL) &&
		(xrtErrorCause(xrtGetError()) == NULL),
		"TLS verifier reused an error from before its callback");
	xrtClearError();

	State.CallbackError = true;
	testRequire(!xrtTlsVerifierVerify(
		pVerifier, XTLS_SERVER, XRT_STR_LITERAL("localhost"),
		&Certificate, 1u
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCause(xrtGetError()) != NULL),
		"TLS verifier did not preserve its callback error cause");
	xrtClearError();

	xrtTlsVerifierRelease(pVerifier);
	testRequire(State.Releases == 0,
		"TLS verifier released context before its last reference");
	xrtTlsVerifierRelease(pRetained);
	testRequire(State.Releases == 1,
		"TLS verifier did not release its custom context exactly once");
}



/* 参数错误和无信任来源必须稳定失败。 */
static void testTlsVerifyArguments(void)
{
	xtlsverifierconfig Config;
	xx509store* pStore;
	xx509cert Certificate;

	testRequire(xrtX509Parse(
		X509_LEGACY_RSA_CERT, sizeof(X509_LEGACY_RSA_CERT), &Certificate
	), "TLS verifier argument fixture parsing failed");
	xrtTlsVerifierConfigInit(&Config);
	testRequire((xrtTlsVerifierCreate(&Config) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"TLS verifier accepted an empty configuration");
	xrtClearError();
	pStore = xrtX509StoreCreate();
	testRequire(pStore != NULL,
		"TLS verifier empty-store fixture creation failed");
	Config.Store = pStore;
	testRequire((xrtTlsVerifierCreate(&Config) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"TLS verifier accepted an empty trust store without a callback");
	xrtX509StoreFree(pStore);
	xrtClearError();
	testRequire(!xrtTlsPeerVerify(NULL, NULL, NULL, NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"TLS peer verifier accepted null input");
	xrtClearError();
	testRequire(!xrtTls13CertificateVerifySignature(
		XTLS_SERVER, XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256,
		(xbytesview) { NULL, 0 }, (xbytesview) { NULL, 0 }, NULL
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"TLS CertificateVerify accepted null input");
}



/* 执行 TLS 验证器基础契约回归。 */
int main(void)
{
	testTlsVerifyCustomContract();
	testTlsVerifyArguments();
	return 0;
}
