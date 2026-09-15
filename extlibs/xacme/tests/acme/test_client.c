#include "../test.h"

#include <xrt/acme_client.h>

#include <string.h>

/*
	公开客户端 API 的无网络路径：配置初始化与参数错误语义。
	真实 Create/Issue 由 test_flow.c（Pebble）与 test_flow_live.c
	（LE staging）覆盖。
*/

int main(void)
{
	xacmeclientconfig Config;

	/* 配置初始化。 */
	xrtAcmeClientConfigInit(&Config);
	testRequire(
		(Config.pAccount == NULL) && (Config.sCaPem == NULL) &&
			(Config.pBorrowedEngine == NULL) &&
			(Config.uTimeoutUs == 0u) &&
			(Config.sPropagateResolvers == NULL) &&
			(Config.iPropagateResolverCount == 0u) &&
			(Config.uPropagateTimeoutMs == 0u),
		"acme client config init mismatch"
	);
	xrtClearError();
	xrtAcmeClientConfigInit(NULL);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"acme client config null mismatch"
	);

	/* Create 参数错误：缺配置 / 缺账户。 */
	xrtClearError();
	testRequire(
		(xrtAcmeClientCreate(NULL) == NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"acme client create null mismatch"
	);
	xrtAcmeClientConfigInit(&Config);
	xrtClearError();
	testRequire(
		(xrtAcmeClientCreate(&Config) == NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"acme client create without account mismatch"
	);

	/* Destroy 空指针为无操作。 */
	xrtAcmeClientDestroy(NULL);

	/* 签发入口参数错误语义。 */
	{
		xacmeissuegrant Grant;
		xrtClearError();
		testRequire(
			!xrtAcmeClientIssue(NULL, NULL, 0u, NULL, &Grant) &&
				(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
			"acme client issue null mismatch"
		);
	}

	/* GrantUnit：释放与清零（含空指针）。 */
	{
		xacmeissuegrant Grant;
		Grant.sFullchainPem = (str)xrtMalloc(4u);
		Grant.sKeyPem = (str)xrtMalloc(4u);
		testRequire(
			(Grant.sFullchainPem != NULL) && (Grant.sKeyPem != NULL),
			"acme grant unit alloc failed"
		);
		xrtAcmeGrantUnit(&Grant);
		testRequire(
			(Grant.sFullchainPem == NULL) && (Grant.sKeyPem == NULL),
			"acme grant unit clear mismatch"
		);
		xrtAcmeGrantUnit(NULL);
	}

	printf("[PASS] acme client public api contract\n");
	return 0;
}
