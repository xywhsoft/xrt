#include <xrt/acme_obtain.h>

#if defined(XACME_FEATURE_ACME_OBTAIN)

#include "../internal/xacme_flow.h"

#include <xrt/acme_client.h>
#include <xrt/acme_store.h>

#include <string.h>

/*
	一站式组合：账户持久化复用 + IssueStored。
	账户钥从 store 读出为 xrtMalloc 文本，账户配置借用它完成
	客户端构建后由本函数释放。
*/

static void xacmeObtainError(xerrkind Kind, cstr sMessage)
{
	xrtSetErrorInfo(Kind, "xrt.acme.obtain", 1, sMessage);
}

void xrtAcmeObtainConfigInit(xacmeobtainconfig* pConfig)
{
	if(pConfig == NULL)
	{
		xacmeObtainError(
			XERR_ARGUMENT, "acme obtain config init requires config");
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
}

bool xrtAcmeObtain(
	const xacmeobtainconfig* pConfig, const xstrview* pDomains,
	size_t iDomainCount, const xacmednsprovider* pDns,
	xacmeissuegrant* pOut, bool* pbRenewed)
{
	xacmeaccountconfig Account;
	xacmeclientconfig ClientConfig;
	struct xacmeclient* pClient = NULL;
	str sStoredAccountPem = NULL;
	str sFreshAccountPem = NULL;
	bool bResult = false;
	xerror* pCause = NULL;

	if((pConfig == NULL) || (pConfig->pAccount == NULL) ||
		(pDomains == NULL) || (iDomainCount == 0u) || (pDns == NULL) ||
		(pOut == NULL) || (pbRenewed == NULL) ||
		(pConfig->sStoreRoot == NULL) || (pConfig->sStoreRoot[0] == '\0'))
	{
		xacmeObtainError(
			XERR_ARGUMENT,
			"acme obtain requires config with account, store root, "
				"domains, dns provider and outputs");
		return false;
	}
	memset(pOut, 0, sizeof(*pOut));
	*pbRenewed = false;

	xrtClearError();
	/* 账户层：只有明确 NOT_FOUND 才可开户，存储故障必须停止。 */
	Account = *pConfig->pAccount;
	sStoredAccountPem = xrtAcmeStoreLoadAccount(
		pConfig->sStoreRoot, Account.sDirectoryUrl);
	if(sStoredAccountPem != NULL)
	{
		Account.sAccountKeyPem = sStoredAccountPem;
		Account.Eab.sKid = NULL; /* 复用账户无需再绑定。 */
		Account.Eab.sHmac = NULL;
	}
	else
	{
		if(xrtErrorKind(xrtGetError()) != XERR_NOT_FOUND) goto Done;
		xrtClearError();
	}

	xrtAcmeClientConfigInit(&ClientConfig);
	ClientConfig.pAccount = &Account;
	ClientConfig.sCaPem = pConfig->sCaPem;
	ClientConfig.pBorrowedEngine = pConfig->pBorrowedEngine;
	ClientConfig.uTimeoutMs = pConfig->uTimeoutMs;
	ClientConfig.sPropagateResolvers = pConfig->sPropagateResolvers;
	ClientConfig.iPropagateResolverCount =
		pConfig->iPropagateResolverCount;
	ClientConfig.uPropagateTimeoutMs = pConfig->uPropagateTimeoutMs;
	ClientConfig.uIssueTimeoutMs = pConfig->uIssueTimeoutMs;
	ClientConfig.sCertKeyPem = pConfig->sCertKeyPem;

	pClient = xrtAcmeClientCreate(&ClientConfig);
	if(pClient == NULL)
	{
		goto Done;
	}
	if(sStoredAccountPem == NULL)
	{
		sFreshAccountPem = xrtAcmeClientAccountPem(pClient);
		if(sFreshAccountPem == NULL) goto Done;
		if(!xrtAcmeStoreSaveAccount(
				pConfig->sStoreRoot, Account.sDirectoryUrl,
				sFreshAccountPem))
		{
			/* 账户密钥导出和落盘成功后才允许签发。 */
			goto Done;
		}
	}

	if(!xrtAcmeClientIssueStored(
			pClient, pDomains, iDomainCount, pDns, pConfig->sStoreRoot,
			(pConfig->iRenewalDays != 0) ? pConfig->iRenewalDays : 30,
			pOut, pbRenewed))
	{
		goto Done;
	}
	bResult = true;

Done:
	if(!bResult && xrtGetError() == NULL)
		xacmeObtainError(XERR_INTERNAL, "acme obtain backend failed without diagnostic");
	pCause = bResult ? NULL : xrtErrorRef(xrtGetError());
	if(sFreshAccountPem != NULL) xrtSecureZero(sFreshAccountPem, strlen(sFreshAccountPem));
	xrtFree(sFreshAccountPem);
	if(sStoredAccountPem != NULL) xrtSecureZero(sStoredAccountPem, strlen(sStoredAccountPem));
	xrtFree(sStoredAccountPem);
	if(pClient != NULL)
	{
		/* 此临时客户端不会交付调用者；回滚单独留出退休预算。 */
		if(pClient->Http.uTimeoutMs < INT64_C(30000))
			pClient->Http.uTimeoutMs = INT64_C(30000);
		xacmeClientDiscard(pClient);
	}
	if(!bResult)
	{
		xrtAcmeGrantUnit(pOut);
		*pbRenewed = false;
	}
	if(pCause != NULL) xrtSetErrorTake(pCause);
	return bResult;
}

#endif
