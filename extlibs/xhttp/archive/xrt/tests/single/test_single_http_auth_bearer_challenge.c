#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <string.h>



/* 单头发布必须保留 Bearer challenge 的标准参数解析。 */
int main(void)
{
	xhttpbearerchallenge Challenge;
	char Output[64];
	size_t iSize;

	if ( !xrtHttpBearerChallengeRead(
		XRT_STR_LITERAL(
			"Bearer error=\"insufficient_scope\", scope=\"read\""
		),
		Output, sizeof(Output), &iSize, &Challenge
	) || (Challenge.Error.Size != 18u) ||
		(memcmp(
			Challenge.Error.Data, "insufficient_scope", 18u
		) != 0) ) {
		return 1;
	}
	return 0;
}
