/*
 * XRT Example - Regex Capture Config Line
 * XRT 范例 - Regex 提取配置行
 *
 * Description / 说明:
 *   EN: Demonstrates named captures, capture ranges, and builder flags.
 *   CN: 演示命名捕获、捕获范围和 Builder flags。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/regex_capture_config_line.exe -lws2_32 -lshell32 -liphlpapi
 *   gcc main.c -o ../../bin/regex_capture_config_line -lws2_32 -lshell32 -liphlpapi
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"
#include <stdio.h>
#include <string.h>


static void PrintSpan(const char* sText, xregexspan tSpan)
{
	printf("%.*s", (int)(tSpan.iEnd - tSpan.iBegin), sText + tSpan.iBegin);
}


int main(void)
{
	const char* sLine = "Timeout=3000";
	const char* sPattern = "(?P<key>[A-Za-z_]+)=(?P<value>[0-9]+)";
	xregexbuilder* pBuilder = NULL;
	xregex* pRegex = NULL;
	xregexspan arrCaps[3] = {0};
	int iErr = 0;
	int iRet = 0;

	xrtInit();

	iErr = xrtRegexBuilderCreate(&pBuilder, sPattern, strlen(sPattern), NULL);
	if ( (iErr == 0) && (pBuilder != NULL) ) {
		xrtRegexBuilderSetFlags(pBuilder, XRT_REGEX_FLAG_INSENSITIVE);
		iErr = xrtRegexCreateFromBuilder(&pRegex, pBuilder, NULL);
	}

	if ( (iErr < 0) && (pRegex != NULL) ) {
		printf("compile error: %s at %zu\n", xrtRegexGetErrorMsg(pRegex), xrtRegexGetErrorPos(pRegex));
		xrtRegexDestroy(pRegex);
		pRegex = NULL;
	}

	if ( pBuilder != NULL ) {
		xrtRegexBuilderDestroy(pBuilder);
		pBuilder = NULL;
	}

	if ( pRegex == NULL ) {
		xrtUnit();
		return 1;
	}

	iRet = xrtRegexCaptures(pRegex, sLine, strlen(sLine), arrCaps, 3u);
	printf("line = %s\n", sLine);
	printf("capture_count = %u\n", xrtRegexCaptureCount(pRegex));
	printf("captures_ret = %d\n", iRet);

	if ( iRet > 0 ) {
		size_t iNameSize = 0;
		const char* sName = NULL;

		printf("all   = ");
		PrintSpan(sLine, arrCaps[0]);
		printf("\n");

		sName = xrtRegexCaptureName(pRegex, 1u, &iNameSize);
		printf("%.*s = ", (int)iNameSize, sName);
		PrintSpan(sLine, arrCaps[1]);
		printf("\n");

		sName = xrtRegexCaptureName(pRegex, 2u, &iNameSize);
		printf("%.*s = ", (int)iNameSize, sName);
		PrintSpan(sLine, arrCaps[2]);
		printf("\n");
	}

	xrtRegexDestroy(pRegex);
	xrtUnit();
	return 0;
}
