#include "../../../dev/bench/bench_common.h"

#define XACME_MODULE_ACME_JOSE
#include <xacme.h>

#include "../src/internal/xacme_jose.h"

/* 测量 ES256 JWS 组装热路径（确定性 RFC 6979 密钥，无网络依赖）。 */
int main(int argc, char** argv)
{
	uint32 iIterations = xbenchArgU32(argc, argv, 1, 5000u);
	xacmees256key Key;
	xacmejwsheader Header;
	uint64 iChecksum = 0;
	xbenchtimer Timer;
	uint64 iElapsed;

	if(iIterations == 0)
	{
		return 1;
	}
	if(!xacmeEs256Generate(&Key))
	{
		return 2;
	}
	Header.Nonce = XRT_STR_LITERAL("bench-nonce-0123456789abcdef");
	Header.Url = XRT_STR_LITERAL("https://example.com/acme/new-order");
	Header.Kid = XRT_STR_LITERAL(
		"https://example.com/acme/account/1234567890");

	xbenchTimerStart(&Timer);
	for(uint32 i = 0; i < iIterations; i++)
	{
		str sToken = xacmeJwsEs256(
			&Key, &Header,
			XRT_STR_LITERAL(
				"{\"identifiers\":[{\"type\":\"dns\","
				"\"value\":\"bench.example.com\"}]}"));
		if(sToken == NULL)
		{
			return 3;
		}
		iChecksum += (uint64)strlen(sToken);
		xrtFree(sToken);
	}
	xbenchTimerStop(&Timer);
	iElapsed = xbenchTimerElapsedNs(&Timer);

	printf("xacme jose ES256 JWS benchmark\n");
	xbenchPrintMetricDouble(
		"acme_jws_per_sec", xbenchSafeRate(iIterations, iElapsed));
	xbenchPrintMetricU64("checksum", iChecksum);
	return 0;
}
