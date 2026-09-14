#include "../test.h"

#include <string.h>

/* 组合根注册表：默认全量构建下必须包含阿里 provider。 */
int main(void)
{
	size_t iCount = xrtAcmeDnsProviderCount();
	bool bHasAli = false;
	size_t i;

	testRequire(iCount > 0, "acme dns provider registry empty");

	for(i = 0; i < iCount; i++)
	{
		const char* sId = xrtAcmeDnsProviderId(i);
		testRequire(sId != NULL, "acme dns provider id null");
		if((sId != NULL) && (strcmp(sId, "ali") == 0))
		{
			bHasAli = true;
		}
	}
	testRequire(bHasAli, "acme dns provider registry missing ali");

	xrtClearError();
	testRequire(
		(xrtAcmeDnsProviderId(iCount) == NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"acme dns provider id range mismatch"
	);

	printf("[PASS] acme dns provider registry\n");
	return 0;
}
