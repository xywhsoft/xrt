#include <xrt/acme.h>
#include <xrt/memory.h>

#include <string.h>

#if defined(XACME_FEATURE_ACME_CORE)

void xrtAcmeAccountConfigInit(xacmeaccountconfig* pConfig)
{
	if(pConfig == NULL)
	{
		xrtSetErrorInfo(
			XERR_ARGUMENT,
			"xrt.acme",
			XACME_ERROR_ARGUMENT,
			"acme account config init requires config"
		);
		return;
	}
	pConfig->sDirectoryUrl = NULL;
	pConfig->sAccountKeyPem = NULL;
	pConfig->Eab.sKid = NULL;
	pConfig->Eab.sHmac = NULL;
	pConfig->sContactEmail = NULL;
}

void xrtAcmeGrantUnit(xacmeissuegrant* pGrant)
{
	if(pGrant == NULL)
	{
		return;
	}
	xrtFree(pGrant->sFullchainPem);
	xrtFree(pGrant->sKeyPem);
	memset(pGrant, 0, sizeof(*pGrant));
}

#endif
