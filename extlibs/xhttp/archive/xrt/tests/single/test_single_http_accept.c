#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头发布必须保留重复字段内容协商。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Accept"),
			XRT_STR_INIT(
				"text/*;q=0.4, "
				"application/json;q=1;profile=full"
			)
		}
	};
	static const xstrview Available[] = {
		XRT_STR_INIT("text/html"),
		XRT_STR_INIT("application/json;profile=full")
	};
	xhttpmediarange Range;
	xhttpparam Param;
	size_t iIndex;
	size_t iOffset = 0;
	size_t iParamOffset = 0;

	return (xrtHttpMediaRangeNext(
		Fields[0].Value, &iOffset, &Range
	) == XHTTP_NEXT_ITEM) && (xrtHttpMediaRangeNext(
		Fields[0].Value, &iOffset, &Range
	) == XHTTP_NEXT_ITEM) && (xrtHttpMediaRangeParamNext(
		&Range, &iParamOffset, &Param
	) == XHTTP_NEXT_ITEM) && xrtHttpTokenEqual(
		Param.Name, XRT_STR_LITERAL("profile")
	) && (xrtHttpAcceptSelect(
		Fields, 1, Available, 2, &iIndex
	) == XHTTP_NEXT_ITEM) && (iIndex == 1u) ? 0 : 1;
}
