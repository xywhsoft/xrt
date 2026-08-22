#define XRUNTIME_MODULE_RUNTIME_TYPE
#include <xruntime.h>



/* 验证发布包可由只安装公开头的最小 C 消费者直接链接。 */
int main(void)
{
	xrttyperegistry* pRegistry = xrtTypeRegistryCreate();
	const xrttype* pType = xrtTypeInt64();
	bool bReady = (pRegistry != NULL) &&
		xrtTypeValidate(pType) &&
		xrtTypeRegistryAdd(pRegistry, pType) &&
		(xrtTypeRegistryFindId(pRegistry, pType->Id) == pType);

	xrtTypeRegistryDestroy(pRegistry);
	return bReady ? 0 : 1;
}
