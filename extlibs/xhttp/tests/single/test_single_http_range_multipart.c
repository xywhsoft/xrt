#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 单头发布必须包含字节多范围线缆构建能力。 */
int main(void)
{
	xhttpbyterange Ranges[2] = {
		{ 0, 0 },
		{ 2, 2 }
	};
	char Output[128];
	uint64 iLength;
	size_t iSize;

	if ( !xrtHttpRangeMultipartLength(
			Ranges,
			2,
			3,
			XRT_STR_LITERAL("text/plain"),
			XRT_STR_LITERAL("single"),
			&iLength
		) || !xrtHttpRangeMultipartHeadWrite(
			&Ranges[0],
			3,
			XRT_STR_LITERAL("text/plain"),
			XRT_STR_LITERAL("single"),
			Output,
			sizeof(Output),
			&iSize
		) || (iLength == 0) || (iSize == 0) ) {
		return 1;
	}
	return 0;
}
