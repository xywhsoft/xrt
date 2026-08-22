#include <xrt.h>

#include <stdio.h>
#include <string.h>



/* 展示条件请求与单字节范围如何组合，而不依赖服务器对象。 */
int main(void)
{
	xhttpfield Fields[] = {
		{
			XRT_STR_INIT("If-None-Match"),
			XRT_STR_INIT("\"revision-7\"")
		},
		{
			XRT_STR_INIT("Range"),
			XRT_STR_INIT("bytes=100-199")
		}
	};
	xhttprepresentation Current = { 0 };
	xhttpprecondition Condition;
	xhttprangespec Spec;
	xhttpbyterange Range;
	xstrview Unit;
	xstrview Set;
	size_t iOffset = 0;

	Current.Exists = true;
	Current.HasETag = true;
	Current.ETag.Opaque = XRT_STR_LITERAL("revision-8");
	Condition = xrtHttpPreconditionsEvaluate(
		XRT_STR_LITERAL("GET"),
		Fields,
		2,
		&Current
	);
	if ( Condition != XHTTP_PRECONDITION_PROCEED ) {
		printf("status: %d\n", (int)Condition);
		return 0;
	}
	if ( !xrtHttpRangeParse(
		Fields[1].Value, &Unit, &Set
	) || !xrtHttpTokenEqual(
		Unit, XRT_STR_LITERAL("bytes")
	) || (xrtHttpByteRangeNext(
		Set, &iOffset, &Spec
	) != XHTTP_NEXT_ITEM) ) {
		return 1;
	}
	if ( xrtHttpByteRangeResolve(
		&Spec, UINT64_C(1000), &Range
	) != XHTTP_RANGE_SATISFIED ) {
		return 2;
	}
	printf(
		"status: 206, bytes %llu-%llu\n",
		(unsigned long long)Range.First,
		(unsigned long long)Range.Last
	);
	return 0;
}
