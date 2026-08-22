#include <xrt.h>

#include <stdio.h>



/* 汇总重复 TE 字段并读取客户端的传输能力。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("TE"), XRT_STR_INIT("trailers") },
		{ XRT_STR_INIT("TE"), XRT_STR_INIT("gzip;q=0.5") }
	};
	xhttpteinfo Info;

	if ( !xrtHttpTeParse(Fields, 2u, &Info) ) {
		return 1;
	}
	printf(
		"codings=%zu trailers=%s gzip=%u\n",
		Info.TransferCodingCount,
		(Info.Flags & XHTTP_TE_ACCEPTS_TRAILERS) != 0 ?
			"yes" : "no",
		(unsigned int)xrtHttpTeQuality(
			Fields, 2u, XRT_STR_LITERAL("gzip")
		)
	);
	return 0;
}
