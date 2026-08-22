/*
 * XRT Example - Query And Header
 * XRT 范例 - Query 与 Header 解析
 *
 * Description / 说明:
 *   EN: Demonstrates iterating query pairs, decoding values, splitting header lines, scanning a header block, and building canonical headers.
 *   CN: 演示遍历 query 键值、解码参数值、拆分单行 header、扫描 header block，以及构建规范化 header。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/http_util_query_and_header.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/http_util_query_and_header -lm -lpthread
 */

#include "../../../xrt.c"
#include <stdio.h>


static void procPrintView(const char* sLabel, xrtstrview tView)
{
	printf("%s = %.*s\n", sLabel, (int)tView.iLen, tView.sPtr ? tView.sPtr : "");
}


int main(void)
{
	const char* sQuery = "name=alice+bob&city=shanghai&stream=1";
	const char* sHeaderBlock =
		"Host: example.com\r\n"
		"Connection: keep-alive, Upgrade\r\n"
		"X-Trace: abc-123\r\n"
		"\r\n";
	xrtquerypair tPair;
	xrtheaderpair tHeader;
	size_t iOffset = 0u;
	size_t iDecodedLen = 0u;
	size_t iBuiltLen = 0u;
	char sDecoded[128];
	char sBuilt[128];
	int iIndex = 0;
	int iRet = 0;

	xrtInit();
	memset(&tPair, 0, sizeof(tPair));
	memset(&tHeader, 0, sizeof(tHeader));
	memset(sDecoded, 0, sizeof(sDecoded));
	memset(sBuilt, 0, sizeof(sBuilt));

	printf("query_count = %u\n", (unsigned)xrtQueryCount(sQuery));
	iOffset = 0u;
	iIndex = 0;
	while ( xrtQueryNext(sQuery, &iOffset, &tPair) ) {
		++iIndex;
		printf("query[%d].key = %.*s\n", iIndex, (int)tPair.tKey.iLen, tPair.tKey.sPtr);
		if ( (tPair.iFlags & XRT_QUERY_F_HAS_VALUE) != 0u ) {
			iDecodedLen = 0u;
			memset(sDecoded, 0, sizeof(sDecoded));
			if ( !xrtPercentDecodeTo(tPair.tValue.sPtr, tPair.tValue.iLen, sDecoded, sizeof(sDecoded), &iDecodedLen, true) ) {
				printf("query[%d].value = decode_failed\n", iIndex);
				iRet = 1;
				goto cleanup;
			}
			printf("query[%d].value = %s\n", iIndex, sDecoded);
		} else {
			printf("query[%d].value = <none>\n", iIndex);
		}
	}

	if ( !xrtHttpHeaderSplitLine("Content-Type: application/json", &tHeader) ) {
		printf("split_header = failed\n");
		iRet = 1;
		goto cleanup;
	}
	procPrintView("split.name", tHeader.tName);
	procPrintView("split.value", tHeader.tValue);

	iBuiltLen = 0u;
	if ( !xrtHttpHeaderBuildCanonicalLineTo("content-type", "application/json", sBuilt, sizeof(sBuilt), &iBuiltLen) ) {
		printf("build_header = failed\n");
		iRet = 1;
		goto cleanup;
	}
	printf("canonical_line = %s", sBuilt);
	printf("contains_upgrade = %s\n", xrtHttpHeaderContainsToken("keep-alive, Upgrade", "upgrade") ? "yes" : "no");

	iOffset = 0u;
	iIndex = 0;
	while ( xrtHttpHeaderNextLine(sHeaderBlock, &iOffset, &tHeader) ) {
		++iIndex;
		printf("header[%d].name = %.*s\n", iIndex, (int)tHeader.tName.iLen, tHeader.tName.sPtr);
		printf("header[%d].value = %.*s\n", iIndex, (int)tHeader.tValue.iLen, tHeader.tValue.sPtr);
	}

cleanup:
	xrtUnit();
	return iRet;
}
