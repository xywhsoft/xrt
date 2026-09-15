#include "../test.h"

#include <xrt/acme_obtain.h>

#include <string.h>

/*
	一站式 Obtain 的无网络路径：配置初始化与参数错误语义。
	真实签发由 test_flow_live.c（LE staging）覆盖同等链路。
*/

int main(void)
{
	xacmeobtainconfig Config;
	xacmeaccountconfig Account;
	xacmeissuegrant Grant;
	xacmednsprovider Stub;
	bool bRenewed = false;

	/* 配置初始化。 */
	xrtAcmeObtainConfigInit(&Config);
	testRequire(
		(Config.pAccount == NULL) && (Config.sCaPem == NULL) &&
			(Config.pBorrowedEngine == NULL) &&
			(Config.uTimeoutUs == 0u) &&
			(Config.sPropagateResolvers == NULL) &&
			(Config.iPropagateResolverCount == 0u) &&
			(Config.uPropagateTimeoutMs == 0u) &&
			(Config.sStoreRoot == NULL) && (Config.iRenewalDays == 0),
		"acme obtain config init mismatch"
	);
	xrtClearError();
	xrtAcmeObtainConfigInit(NULL);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"acme obtain config null mismatch"
	);

	/* 参数错误：缺配置 / 缺账户 / 缺 store 根。 */
	xrtClearError();
	memset(&Grant, 0, sizeof(Grant));
	memset(&Stub, 0, sizeof(Stub));
	Stub.sId = "stub";
	Stub.Add = NULL;
	Stub.Remove = NULL;
	{
		xstrview Domains[1];
		Domains[0] = XRT_STR_LITERAL("test.xxrpa.com");
		testRequire(
			!xrtAcmeObtain(NULL, Domains, 1u, &Stub, &Grant, &bRenewed) &&
				(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
			"acme obtain null config mismatch"
		);
		xrtAcmeObtainConfigInit(&Config);
		xrtClearError();
		testRequire(
			!xrtAcmeObtain(
				&Config, Domains, 1u, &Stub, &Grant, &bRenewed) &&
				(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
			"acme obtain without account mismatch"
		);
		xrtAcmeAccountConfigInit(&Account);
		Account.sDirectoryUrl = XACME_DIRECTORY_LE_STAGING;
		Config.pAccount = &Account;
		xrtClearError();
		testRequire(
			!xrtAcmeObtain(
				&Config, Domains, 1u, &Stub, &Grant, &bRenewed) &&
				(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
			"acme obtain without store root mismatch"
		);
	}

	printf("[PASS] acme obtain public api contract\n");
	return 0;
}
