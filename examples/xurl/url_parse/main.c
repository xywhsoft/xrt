/*
 * XRT Example - URL Parse
 * XRT 范例 - URL 解析
 *
 * Description / 说明:
 *   EN: Demonstrates parsing an absolute URL view and reading its scheme, host, port, path, query, fragment, and Host header.
 *   CN: 演示解析绝对 URL 视图，并读取 scheme、host、port、path、query、fragment 以及 Host 头文本。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/xurl_url_parse.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/xurl_url_parse -lm -lpthread
 */

#include "../../../xrt.c"
#include <stdio.h>


static void procPrintView(const char* sLabel, xrtstrview tView)
{
	printf("%s = %.*s\n", sLabel, (int)tView.iLen, tView.sPtr ? tView.sPtr : "");
}


int main(void)
{
	xrturlview tURL;
	char sHostHeader[128];
	int iRet = 0;

	xrtInit();
	memset(&tURL, 0, sizeof(tURL));
	memset(sHostHeader, 0, sizeof(sHostHeader));

	if ( !xrtUrlParseView("https://user@example.com:8443/api/v1/users?id=1#frag", &tURL) ) {
		printf("parse = failed\n");
		iRet = 1;
		goto cleanup;
	}
	if ( !xrtUrlMakeHostHeader(&tURL, sHostHeader, sizeof(sHostHeader)) ) {
		printf("host_header = failed\n");
		iRet = 1;
		goto cleanup;
	}

	printf("absolute = %s\n", (tURL.iFlags & XRT_URL_F_ABSOLUTE) != 0u ? "yes" : "no");
	printf("has_port = %s\n", (tURL.iFlags & XRT_URL_F_HAS_PORT) != 0u ? "yes" : "no");
	printf("is_https = %s\n", xrtUrlViewIsScheme(&tURL, "https") ? "yes" : "no");
	procPrintView("scheme", tURL.tScheme);
	procPrintView("authority", tURL.tAuthority);
	procPrintView("host", tURL.tHost);
	printf("port = %u\n", (unsigned)tURL.iPort);
	procPrintView("path", tURL.tPath);
	procPrintView("query", tURL.tQuery);
	procPrintView("fragment", tURL.tFragment);
	procPrintView("target", tURL.tTarget);
	printf("host_header = %s\n", sHostHeader);

cleanup:
	xrtUnit();
	return iRet;
}
