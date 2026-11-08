#include <xrt/acme_dns.h>
#include <xrt/detail/wait.h>
#include "../internal/xacme_dns_budget.h"

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

bool xacmeDnsProviderAddUntil(xacmednsprovider* pProvider,
	xstrview Fqdn, xstrview Txt, double Deadline)
{
	bool Matched = false, Result = false;
	if(pProvider == NULL || pProvider->Add == NULL) {
		xrtSetErrorInfo(XERR_ARGUMENT, "xrt.acme.dns", XACME_DNS_ERROR_ARGUMENT,
			"acme DNS Add requires a provider callback");
		return false;
	}
	if(!__xrtWaitValid(Deadline)) return false;
	if(Deadline != INFINITY) {
		xrtClearError();
		if(__xrtWaitExpired(Deadline)) {
			xrtSetErrorInfo(XERR_TIMEOUT, "xrt.acme.dns", XACME_DNS_ERROR_NETWORK,
				"acme DNS operation budget exhausted before Add");
			return false;
		}
	}
#if defined(XACME_FEATURE_DNS_ALI)
	Result = xacmeDnsAliAddUntil(pProvider, Fqdn, Txt, Deadline, &Matched);
	if(Matched) return Result;
#endif
#if defined(XACME_FEATURE_DNS_CF)
	Result = xacmeDnsCfAddUntil(pProvider, Fqdn, Txt, Deadline, &Matched);
	if(Matched) return Result;
#endif
#if defined(XACME_FEATURE_DNS_TENCENT)
	Result = xacmeDnsTencentAddUntil(pProvider, Fqdn, Txt, Deadline, &Matched);
	if(Matched) return Result;
#endif
#if defined(XACME_FEATURE_DNS_AWS)
	Result = xacmeDnsAwsAddUntil(pProvider, Fqdn, Txt, Deadline, &Matched);
	if(Matched) return Result;
#endif
#if defined(XACME_FEATURE_DNS_HUAWEI)
	Result = xacmeDnsHuaweiAddUntil(pProvider, Fqdn, Txt, Deadline, &Matched);
	if(Matched) return Result;
#endif
	(void)Matched; (void)Result;
	return pProvider->Add(pProvider, Fqdn, Txt);
}

#endif
