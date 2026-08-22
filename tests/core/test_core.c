#include "../test.h"



/* 验证最小核心不需要显式初始化即可使用。 */
int main(void)
{
	xrtresourcelimits Limits;
	static const xstrview Strings[] = {
		XRT_STR_INIT("h2"),
		XRT_STR_INIT("http/1.1")
	};
	static const xbytesview Bytes[] = {
		XRT_BYTES_INIT("abc"),
		XRT_BYTES_INIT("xyz")
	};

	testRequire(strcmp(xrtVersion(), XRT_VERSION_TEXT) == 0, "version mismatch");
	testRequire((Strings[0].Size == 2u) &&
		(memcmp(Strings[0].Data, "h2", 2u) == 0) &&
		(Strings[1].Size == 8u) &&
		(memcmp(Strings[1].Data, "http/1.1", 8u) == 0),
		"static string view initializer mismatch");
	testRequire((Bytes[0].Size == 3u) &&
		(memcmp(Bytes[0].Data, "abc", 3u) == 0) &&
		(Bytes[1].Size == 3u) &&
		(memcmp(Bytes[1].Data, "xyz", 3u) == 0),
		"static byte view initializer mismatch");
	xrtResourceLimitsInit(&Limits);
	testRequire((Limits.iSize == sizeof(Limits)) &&
		(Limits.iVersion == XRT_RESOURCE_LIMITS_VERSION) &&
		(Limits.iMaxInputBytes == (256u * 1024u * 1024u)) &&
		(Limits.iMaxOutputBytes == (512u * 1024u * 1024u)) &&
		(Limits.iMaxItemBytes == (256u * 1024u * 1024u)) &&
		(Limits.iMaxEntries == 100000u) &&
		(Limits.iMaxNodes == 1000000u) &&
		(Limits.iMaxDepth == 128u) &&
		(Limits.iMaxCompressionRatio == 1000u) &&
		(Limits.iFlags == 0u) &&
		(Limits.iReserved == 0u),
		"resource limits defaults mismatch");
	printf("[PASS] core\n");
	return 0;
}
