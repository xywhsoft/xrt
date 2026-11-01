#ifndef XACME_DNS_BUDGET_H
#define XACME_DNS_BUDGET_H

#include <xrt/acme_dns.h>

#if defined(XACME_FEATURE_ACME_DNS)
/* Internal synchronous Add dispatch. Only exact built-in callback pairs may
 * expose their private context; custom callbacks keep their host contract. */
bool xacmeDnsProviderAddUntil(xacmednsprovider* pProvider,
	xstrview Fqdn, xstrview Txt, double Deadline);

#if defined(XACME_FEATURE_DNS_ALI)
/* A foreign callback pair leaves Matched=false without inspecting context. */
bool xacmeDnsAliAddUntil(xacmednsprovider* pProvider,
	xstrview Fqdn, xstrview Txt, double Deadline, bool* pMatched);
#endif

#if defined(XACME_FEATURE_DNS_CF)
/* A foreign callback pair leaves Matched=false without inspecting context. */
bool xacmeDnsCfAddUntil(xacmednsprovider* pProvider,
	xstrview Fqdn, xstrview Txt, double Deadline, bool* pMatched);
#endif

#if defined(XACME_FEATURE_DNS_TENCENT)
/* A foreign callback pair leaves Matched=false without inspecting context. */
bool xacmeDnsTencentAddUntil(xacmednsprovider* pProvider,
	xstrview Fqdn, xstrview Txt, double Deadline, bool* pMatched);
#endif

#if defined(XACME_FEATURE_DNS_AWS)
/* A foreign callback pair leaves Matched=false without inspecting context. */
bool xacmeDnsAwsAddUntil(xacmednsprovider* pProvider,
	xstrview Fqdn, xstrview Txt, double Deadline, bool* pMatched);
#endif

#if defined(XACME_FEATURE_DNS_HUAWEI)
/* A foreign callback pair leaves Matched=false without inspecting context. */
bool xacmeDnsHuaweiAddUntil(xacmednsprovider* pProvider,
	xstrview Fqdn, xstrview Txt, double Deadline, bool* pMatched);
#endif
#endif
#endif
