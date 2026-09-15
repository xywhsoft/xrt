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

	/* 账户层：store 有则复用，无则开户后持久化。 */
	Account = *pConfig->pAccount;
	sStoredAccountPem = xrtAcmeStoreLoadAccount(
		pConfig->sStoreRoot, Account.sDirectoryUrl);
	if(sStoredAccountPem != NULL)
	{
		Account.sAccountKeyPem = sStoredAccountPem;
		Account.Eab.sKid = NULL; /* 复用账户无需再绑定。 */
		Account.Eab.sHmac = NULL;
	}

	xrtAcmeClientConfigInit(&ClientConfig);
	ClientConfig.pAccount = &Account;
	ClientConfig.sCaPem = pConfig->sCaPem;
	ClientConfig.pBorrowedEngine = pConfig->pBorrowedEngine;
	ClientConfig.uTimeoutUs = pConfig->uTimeoutUs;
	ClientConfig.sPropagateResolvers = pConfig->sPropagateResolvers;
	ClientConfig.iPropagateResolverCount =
		pConfig->iPropagateResolverCount;
	ClientConfig.uPropagateTimeoutMs = pConfig->uPropagateTimeoutMs;
	ClientConfig.uIssueTimeoutUs = pConfig->uIssueTimeoutUs;
	ClientConfig.sCertKeyPem = pConfig->sCertKeyPem;

	pClient = xrtAcmeClientCreate(&ClientConfig);
	if(pClient == NULL)
	{
		goto Done;
	}
	if(sStoredAccountPem == NULL)
	{
		sFreshAccountPem = xrtAcmeClientAccountPem(pClient);
		if((sFreshAccountPem != NULL) &&
			!xrtAcmeStoreSaveAccount(
				pConfig->sStoreRoot, Account.sDirectoryUrl,
				sFreshAccountPem))
		{
			/* 账户落盘失败不作废本次签发；续期时会再开新账户。 */
			xacmeObtainError(
				XERR_IO, "acme obtain save account failed");
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
	xrtFree(sFreshAccountPem);
	xrtFree(sStoredAccountPem);
	if(pClient != NULL)
	{
		xrtAcmeClientDestroy(pClient);
	}
	if(!bResult)
	{
		xrtFree(pOut->sFullchainPem);
		xrtFree(pOut->sKeyPem);
		memset(pOut, 0, sizeof(*pOut));
	}
	return bResult;
}

#endif
