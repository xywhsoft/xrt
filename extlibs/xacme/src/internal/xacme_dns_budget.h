#ifndef XACME_DNS_BUDGET_H
#define XACME_DNS_BUDGET_H

#include <xrt/acme_dns.h>

#if defined(XACME_FEATURE_ACME_DNS)
/* Internal synchronous Add dispatch. A native Add callback establishes its
 * typed-context contract independently of Remove decoration. Custom Add
 * callbacks retain their host contract. */
bool xacmeDnsProviderAddUntil(xacmednsprovider* pProvider,
	xstrview Fqdn, xstrview Txt, double Deadline);

#if defined(XACME_FEATURE_DNS_ALI)
/* A foreign Add leaves Matched=false without inspecting context. */
bool xacmeDnsAliAddUntil(xacmednsprovider* pProvider,
	xstrview Fqdn, xstrview Txt, double Deadline, bool* pMatched);
#endif

#if defined(XACME_FEATURE_DNS_CF)
/* A foreign Add leaves Matched=false without inspecting context. */
bool xacmeDnsCfAddUntil(xacmednsprovider* pProvider,
	xstrview Fqdn, xstrview Txt, double Deadline, bool* pMatched);
#endif

#if defined(XACME_FEATURE_DNS_TENCENT)
/* A foreign Add leaves Matched=false without inspecting context. */
bool xacmeDnsTencentAddUntil(xacmednsprovider* pProvider,
	xstrview Fqdn, xstrview Txt, double Deadline, bool* pMatched);
#endif

#if defined(XACME_FEATURE_DNS_AWS)
/* A foreign Add leaves Matched=false without inspecting context. */
bool xacmeDnsAwsAddUntil(xacmednsprovider* pProvider,
	xstrview Fqdn, xstrview Txt, double Deadline, bool* pMatched);
#endif

#if defined(XACME_FEATURE_DNS_HUAWEI)
/* A foreign Add leaves Matched=false without inspecting context. */
bool xacmeDnsHuaweiAddUntil(xacmednsprovider* pProvider,
	xstrview Fqdn, xstrview Txt, double Deadline, bool* pMatched);
#endif
#endif
#endif
