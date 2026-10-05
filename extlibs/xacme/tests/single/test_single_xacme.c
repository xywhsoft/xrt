#define XACME_IMPLEMENTATION
#include "../../include/xacme/features.h"
#ifndef XRT_IMPLEMENTATION
#define XRT_IMPLEMENTATION
#endif
#include "../../../../single/xrt.h"
#include "../../../../single/extlibs/xacme.h"

#include <string.h>

/*
	产品伞形单头测试：默认全量选择下五家 DNS provider、签发流程、
	存储与一站式入口同时可用，公开 API 可无网络调用。
*/
int main(void)
{
	xacmeaccountconfig Account;
	xacmeclientconfig Client;
	xacmeobtainconfig Obtain;
	xacmednsprovider Stub;
	xacmeissuegrant Grant;

	#if !defined(XACME_FEATURE_ACME_FLOW) || \
		!defined(XACME_FEATURE_ACME_STORE) || \
		!defined(XACME_FEATURE_ACME_OBTAIN) || \
		!defined(XACME_FEATURE_DNS_ALI) || \
		!defined(XACME_FEATURE_DNS_CF) || \
		!defined(XACME_FEATURE_DNS_TENCENT) || \
		!defined(XACME_FEATURE_DNS_AWS) || \
		!defined(XACME_FEATURE_DNS_HUAWEI)
		#error "xacme default selection closure is incomplete"
	#endif

	xrtAcmeAccountConfigInit(&Account);
	xrtAcmeClientConfigInit(&Client);
	xrtAcmeObtainConfigInit(&Obtain);
	memset(&Stub, 0, sizeof(Stub));
	memset(&Grant, 0, sizeof(Grant));

	if((Account.sDirectoryUrl != NULL) || (Obtain.sStoreRoot != NULL) ||
		(Client.pAccount != NULL))
	{
		return 1;
	}
	if(xrtAcmeDnsProviderCount() < 5u)
	{
		return 2;
	}
	xrtAcmeGrantUnit(&Grant);
	if(!xrtAcmeClientCleanup(NULL)) return 3;
	{
		size_t pending = SIZE_MAX;
		if(!xrtAcmeCleanupPending(0u, &pending) || pending != 0u) return 4;
	}
	xrtAcmeClientDestroy(NULL);
	return 0;
}
