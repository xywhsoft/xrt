/*
	组合根：内建 DNS provider 注册表实现在这里而不是 acme_dns 模块，
	使 provider 可以独立编译链接而不强制引入全部内建实现。
	注册表内容随 features.h 三态选择结果变化。
*/
#include <xacme.h>

#if defined(XACME_FEATURE_ACME_DNS)

typedef struct xacmednsentry {
	const char* sId;
} xacmednsentry;

static const xacmednsentry __xacmeDnsBuiltins[] = {
#if defined(XACME_FEATURE_DNS_ALI)
	{ "ali" },
#endif
#if defined(XACME_FEATURE_DNS_CF)
	{ "cf" },
#endif
#if defined(XACME_FEATURE_DNS_TENCENT)
	{ "tencent" },
#endif
#if defined(XACME_FEATURE_DNS_AWS)
	{ "aws" },
#endif
#if defined(XACME_FEATURE_DNS_HUAWEI)
	{ "huawei" },
#endif
};

size_t xrtAcmeDnsProviderCount(void)
{
	return sizeof(__xacmeDnsBuiltins) / sizeof(__xacmeDnsBuiltins[0]);
}

const char* xrtAcmeDnsProviderId(size_t iIndex)
{
	if(iIndex >= xrtAcmeDnsProviderCount())
	{
		xrtSetErrorInfo(
			XERR_RANGE,
			"xrt.acme.dns",
			XACME_DNS_ERROR_ARGUMENT,
			"acme dns provider index out of range"
		);
		return NULL;
	}
	return __xacmeDnsBuiltins[iIndex].sId;
}

#endif
