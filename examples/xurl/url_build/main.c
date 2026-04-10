/*
 * XRT Example - URL Build
 * XRT 范例 - URL 构建
 *
 * Description / 说明:
 *   EN: Demonstrates building target, authority, full URL, and Host header text from parsed URL views.
 *   CN: 演示从已解析的 URL 视图构建 target、authority、完整 URL，以及 Host 头文本。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/xurl_url_build.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/xurl_url_build -lm -lpthread
 */

#include "../../../xrt.c"
#include <stdio.h>


int main(void)
{
	xrturlview tURL;
	xrturlview tTarget;
	xrturlview tAuthority;
	char sTarget[256];
	char sAuthority[256];
	char sURL[256];
	char sHostHeader[128];
	size_t iBuiltLen = 0u;
	int iRet = 0;

	xrtInit();
	memset(&tURL, 0, sizeof(tURL));
	memset(&tTarget, 0, sizeof(tTarget));
	memset(&tAuthority, 0, sizeof(tAuthority));
	memset(sTarget, 0, sizeof(sTarget));
	memset(sAuthority, 0, sizeof(sAuthority));
	memset(sURL, 0, sizeof(sURL));
	memset(sHostHeader, 0, sizeof(sHostHeader));

	if ( !xrtUrlParseView("wss://api.example.com:9443/chat?room=1", &tURL) ) {
		printf("parse_url = failed\n");
		iRet = 1;
		goto cleanup;
	}
	if ( !xrtUrlParseTarget("/v1/chat/completions?model=gpt-5&stream=1", &tTarget) ) {
		printf("parse_target = failed\n");
		iRet = 1;
		goto cleanup;
	}
	if ( !xrtUrlParseAuthority("[2001:db8::1]:8080", &tAuthority) ) {
		printf("parse_authority = failed\n");
		iRet = 1;
		goto cleanup;
	}

	iBuiltLen = 0u;
	if ( !xrtUrlBuildTarget(&tTarget, sTarget, sizeof(sTarget), &iBuiltLen) ) {
		printf("build_target = failed\n");
		iRet = 1;
		goto cleanup;
	}
	iBuiltLen = 0u;
	if ( !xrtUrlBuildAuthority(&tAuthority, sAuthority, sizeof(sAuthority), &iBuiltLen) ) {
		printf("build_authority = failed\n");
		iRet = 1;
		goto cleanup;
	}
	iBuiltLen = 0u;
	if ( !xrtUrlBuild(&tURL, sURL, sizeof(sURL), &iBuiltLen) ) {
		printf("build_url = failed\n");
		iRet = 1;
		goto cleanup;
	}
	if ( !xrtUrlMakeHostHeader(&tURL, sHostHeader, sizeof(sHostHeader)) ) {
		printf("build_host_header = failed\n");
		iRet = 1;
		goto cleanup;
	}

	printf("target = %s\n", sTarget);
	printf("authority = %s\n", sAuthority);
	printf("url = %s\n", sURL);
	printf("host_header = %s\n", sHostHeader);
	printf("default_port = %s\n", xrtUrlIsDefaultPort(&tURL) ? "yes" : "no");

cleanup:
	xrtUnit();
	return iRet;
}
