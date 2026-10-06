/* 单头发布包消费探针：全量选择 + 实现宏。 */
#define XACME_MODULE_ALL
#define XACME_IMPLEMENTATION
#include "xacme.h"

int main(void)
{
	size_t pending = SIZE_MAX;
	return (xrtAcmeDnsProviderCount() > 0 && xrtAcmeClientCleanup(NULL) &&
		xrtAcmeCleanupPending(0, &pending) && pending == 0u) ? 0 : 1;
}
