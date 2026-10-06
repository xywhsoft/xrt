/* 验证 xoauth2 模块清单的最小依赖闭包可独立构建。 */
#if defined(XOAUTH2_FEATURE_XOAUTH2) || defined(XOAUTH2_SINGLE_HEADER)
#include "../src/internal/xoauth2_internal.h"
#else
#define XRT_IMPLEMENTATION
#include "support/runtime.h"
#include "support/implementation.c"
#endif

int main(void)
{
	xoauth2client client = {0};
	char* url;
	xoauth2UseGithub(&client, "client-id", "client-secret", "https://app/callback");
	url = xoauth2BeginLogin(&client);
	if(url == NULL)
	{
		xoauth2ClientUnit(&client);
		return 1;
	}
	xrtFree(url);
	xoauth2ClientUnit(&client);
	{
		size_t pending = SIZE_MAX;
		if(!xoauth2HttpXrtCleanupPending(0, &pending) || pending != 0u) return 1;
	}
	return 0;
}
