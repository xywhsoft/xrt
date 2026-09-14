#include <xrt/acme.h>

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

#endif
