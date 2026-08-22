#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头发布必须保留 TE 重复字段和 Trailer 能力。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("TE"), XRT_STR_INIT("trailers") },
		{ XRT_STR_INIT("te"), XRT_STR_INIT("gzip;q=0.5") }
	};
	xhttpteinfo Info;

	if ( !xrtHttpTeParse(Fields, 2u, &Info) ) {
		return 1;
	}
	if ( (Info.CodingCount != 2u) ||
		(xrtHttpTeAcceptsTrailers(
			Fields, 2u
		) != XHTTP_NEXT_ITEM) ) {
		return 2;
	}
	return 0;
}
