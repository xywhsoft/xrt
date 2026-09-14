#include <xrt/acme_dns.h>

#if defined(XACME_FEATURE_ACME_DNS)

bool xrtAcmeDnsProviderValidate(const xacmednsprovider* pProvider)
{
	if(pProvider == NULL || pProvider->sId == NULL ||
		pProvider->Add == NULL || pProvider->Remove == NULL)
	{
		xrtSetErrorInfo(
			XERR_ARGUMENT,
			"xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT,
			"acme dns provider requires id, add and remove"
		);
		return false;
	}
	return true;
}

#endif
