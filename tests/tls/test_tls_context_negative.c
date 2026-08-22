#include "../test.h"



/* 验证限制失败统一落在 TLS limit 错误阶段。 */
static void testTlsLimitError(const xtlslimits* pLimits, cstr sMessage)
{
	xrtClearError();
	testRequire(!xrtTlsLimitsValid(pLimits), sMessage);
	testRequire((xrtGetError() != NULL) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "xrt.tls") == 0) &&
		(xrtErrorCode(xrtGetError()) == (int32)XTLS_ERROR_LIMIT),
		"TLS limit error metadata mismatch");
}



/* 每个队列下限和驱动预算都必须在创建会话前拒绝。 */
static void testTlsContextLimits(void)
{
	xtlslimits Limits;

	xrtClearError();
	xrtTlsLimitsInit(NULL);
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorCode(xrtGetError()) == (int32)XTLS_ERROR_ARGUMENT),
		"null TLS limit initialization did not fail");

	xrtClearError();
	testRequire(!xrtTlsLimitsValid(NULL) &&
		(xrtErrorCode(xrtGetError()) == (int32)XTLS_ERROR_ARGUMENT),
		"null TLS limits were accepted");

	xrtTlsLimitsInit(&Limits);
	Limits.FeedLimit = XTLS_RECORD_HEADER_SIZE +
		XTLS12_RECORD_CIPHERTEXT_MAX - 1u;
	testTlsLimitError(&Limits, "short TLS feed limit was accepted");
	xrtTlsLimitsInit(&Limits);
	Limits.SendLimit = XTLS_RECORD_HEADER_SIZE +
		XTLS12_RECORD_CIPHERTEXT_MAX - 1u;
	testTlsLimitError(&Limits, "short TLS send limit was accepted");
	xrtTlsLimitsInit(&Limits);
	Limits.PlainLimit = XTLS_RECORD_PLAINTEXT_MAX - 1u;
	testTlsLimitError(&Limits, "short TLS plain limit was accepted");
	xrtTlsLimitsInit(&Limits);
	Limits.HandshakeLimit = XTLS_HANDSHAKE_HEADER_SIZE - 1u;
	testTlsLimitError(&Limits, "short TLS handshake limit was accepted");
	xrtTlsLimitsInit(&Limits);
	Limits.HandshakeLimit = XTLS_HANDSHAKE_HEADER_SIZE +
		XTLS_HANDSHAKE_BODY_MAX + 1u;
	testTlsLimitError(&Limits, "oversized TLS handshake limit was accepted");
	xrtTlsLimitsInit(&Limits);
	Limits.RecordBudget = 0;
	testTlsLimitError(&Limits, "zero TLS record budget was accepted");
	xrtTlsLimitsInit(&Limits);
	Limits.HandshakeBudget = 0;
	testTlsLimitError(&Limits, "zero TLS handshake budget was accepted");
}



/* 上下文入口必须拒绝无效配置和空对象查询。 */
static void testTlsContextErrors(void)
{
	xtlscontextconfig Config;
	xtlspolicy Policy;

	xrtClearError();
	xrtTlsContextConfigInit(NULL);
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorCode(xrtGetError()) == (int32)XTLS_ERROR_ARGUMENT),
		"null TLS context config initialization did not fail");

	xrtTlsContextConfigInit(&Config);
	Config.Limits.RecordBudget = 0;
	testRequire(xrtTlsContextCreate(&Config) == NULL,
		"TLS context accepted invalid limits");

	xrtTlsContextConfigInit(&Config);
	xrtTlsPolicyInit(&Policy);
	Policy.KeySharePolicy = (xtlskeysharepolicy)9;
	Config.Policy = &Policy;
	testRequire((xrtTlsContextCreate(&Config) == NULL) &&
		(xrtErrorCode(xrtGetError()) == (int32)XTLS_ERROR_NEGOTIATION),
		"TLS context accepted an invalid policy");

	xrtClearError();
	testRequire((xrtTlsContextRetain(NULL) == NULL) &&
		(xrtErrorCode(xrtGetError()) == (int32)XTLS_ERROR_ARGUMENT),
		"null TLS context retain did not fail");
	xrtClearError();
	testRequire((xrtTlsContextPolicy(NULL) == NULL) &&
		(xrtErrorCode(xrtGetError()) == (int32)XTLS_ERROR_ARGUMENT),
		"null TLS context policy query did not fail");
	xrtClearError();
	testRequire((xrtTlsContextLimits(NULL) == NULL) &&
		(xrtErrorCode(xrtGetError()) == (int32)XTLS_ERROR_ARGUMENT),
		"null TLS context limit query did not fail");
}



/* 执行 TLS 上下文负向边界回归。 */
int main(void)
{
	testTlsContextLimits();
	testTlsContextErrors();
	return 0;
}
