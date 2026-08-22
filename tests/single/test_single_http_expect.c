#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头发布必须保留 Expect 重复字段和扩展分类。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Expect"), XRT_STR_INIT("100-continue") },
		{ XRT_STR_INIT("expect"), XRT_STR_INIT("100-CONTINUE") }
	};
	xhttpexpectation Expectation;

	if ( !xrtHttpExpectationParse(
		XRT_STR_LITERAL("feature=\"a,b\"; mode=fast"),
		&Expectation
	) ) {
		return 1;
	}
	if ( xrtHttpExpectFields(
		Fields, 2u
	) != XHTTP_EXPECT_CONTINUE ) {
		return 2;
	}
	return 0;
}
