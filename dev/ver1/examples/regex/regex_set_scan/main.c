/*
 * XRT Example - Regex Set Scan
 * XRT 范例 - Regex 集合扫描
 *
 * Description / 说明:
 *   EN: Demonstrates multi-pattern classification with xregexset.
 *   CN: 演示用 xregexset 做多模式分类。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/regex_set_scan.exe -lws2_32 -lshell32 -liphlpapi
 *   gcc main.c -o ../../bin/regex_set_scan -lws2_32 -lshell32 -liphlpapi
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"
#include <stdio.h>
#include <string.h>


int main(void)
{
	const char* arrPatterns[] = {
		"error",
		"warn",
		"timeout",
	};
	const char* sText = "warn: upstream timeout";
	xregexset* pSet = NULL;
	uint32 arrIndexes[8] = {0};
	uint32 iCount = 0;
	int iRet = 0;
	uint32 i;

	xrtInit();

	pSet = xrtRegexSetCreate(arrPatterns, 3u);
	if ( pSet == NULL ) {
		xrtUnit();
		return 1;
	}

	iRet = xrtRegexSetMatches(pSet, sText, strlen(sText), arrIndexes, 8u, &iCount);
	printf("text = %s\n", sText);
	printf("matches_ret = %d\n", iRet);
	printf("match_count = %u\n", iCount);

	if ( iRet > 0 ) {
		for ( i = 0; i < iCount; i++ ) {
			printf("pattern[%u] = %s\n", arrIndexes[i], arrPatterns[arrIndexes[i]]);
		}
	}

	xrtRegexSetDestroy(pSet);
	xrtUnit();
	return 0;
}
