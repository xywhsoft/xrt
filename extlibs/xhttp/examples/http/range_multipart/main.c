#include <stdio.h>

#include <xhttp.h>



/* 测量并写出一个可与文件区间零复制组合的 Part 头。 */
int main(void)
{
	xhttpbyterange Ranges[2] = {
		{ 0, 99 },
		{ 900, 999 }
	};
	char Head[160];
	uint64 iLength;
	size_t iHeadSize;

	if ( !xrtHttpRangeMultipartLength(
			Ranges,
			2,
			1000,
			XRT_STR_LITERAL("application/pdf"),
			XRT_STR_LITERAL("xrt-4f9a"),
			&iLength
		) || !xrtHttpRangeMultipartHeadWrite(
			&Ranges[0],
			1000,
			XRT_STR_LITERAL("application/pdf"),
			XRT_STR_LITERAL("xrt-4f9a"),
			Head,
			sizeof(Head),
			&iHeadSize
		) ) {
		return 1;
	}
	printf("content-length: %llu\n", (unsigned long long)iLength);
	(void)fwrite(Head, 1, iHeadSize, stdout);
	return 0;
}
