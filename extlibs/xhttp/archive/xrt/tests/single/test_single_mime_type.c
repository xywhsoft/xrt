#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头发布必须保留媒体类型解析与比较能力。 */
int main(void)
{
	xmediatype Type;

	return xrtHttpMediaTypeParse(
		XRT_STR_LITERAL("application/problem+json"), &Type
	) && xrtHttpMediaTypeEqual(
		&Type, XRT_STR_LITERAL("application"),
		XRT_STR_LITERAL("problem+json")
	) ? 0 : 1;
}
