#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 单头发布必须保留字节范围解析、解析到长度和 Content-Range。 */
int main(void)
{
	xhttprangespec Spec;
	xhttpbyterange Range;
	xhttpcontentrange Content;
	size_t iOffset = 0;

	if ( xrtHttpByteRangeNext(
		XRT_STR_LITERAL("-20"), &iOffset, &Spec
	) != XHTTP_NEXT_ITEM ) {
		return 1;
	}
	if ( xrtHttpByteRangeResolve(
		&Spec, 100, &Range
	) != XHTTP_RANGE_SATISFIED ||
		(Range.First != 80) || (Range.Last != 99) ) {
		return 2;
	}
	if ( !xrtHttpContentRangeParse(
		XRT_STR_LITERAL("bytes 80-99/100"), &Content
	) || !Content.Satisfied || (Content.Length != 100) ) {
		return 3;
	}
	return 0;
}
