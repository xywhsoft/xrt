/*
 * XRT Example - URL Resolve
 * XRT 范例 - URL 解析相对引用
 *
 * Description / 说明:
 *   EN: Demonstrates resolving relative, query-only, protocol-relative, absolute-path, and fragment references against a base URL.
 *   CN: 演示把相对路径、仅 query、协议相对、绝对路径与 fragment 引用解析到基准 URL 上。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/xurl_url_resolve.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/xurl_url_resolve -lm -lpthread
 */

#include "../../../xrt.c"
#include <stdio.h>


static bool procResolveAndPrint(const xrturlview* pBase, const char* sRef)
{
	char sOut[256];
	size_t iOutLen = 0u;

	memset(sOut, 0, sizeof(sOut));
	if ( !xrtUrlResolve(pBase, sRef, sOut, sizeof(sOut), &iOutLen) ) {
		printf("resolve[%s] = failed\n", sRef);
		return false;
	}

	printf("resolve[%s] = %s\n", sRef, sOut);
	return true;
}


int main(void)
{
	const char* sBaseURL = "wss://[::1]:9443/chat/rooms/current?room=1#live";
	xrturlview tBase;
	int iRet = 0;

	xrtInit();
	memset(&tBase, 0, sizeof(tBase));

	if ( !xrtUrlParseView(sBaseURL, &tBase) ) {
		printf("parse_base = failed\n");
		iRet = 1;
		goto cleanup;
	}

	printf("base = %s\n", sBaseURL);
	if ( !procResolveAndPrint(&tBase, "../list?page=2") ) iRet = 1;
	if ( !procResolveAndPrint(&tBase, "?room=2") ) iRet = 1;
	if ( !procResolveAndPrint(&tBase, "//example.org/ws") ) iRet = 1;
	if ( !procResolveAndPrint(&tBase, "/v2/socket") ) iRet = 1;
	if ( !procResolveAndPrint(&tBase, "#tail") ) iRet = 1;

cleanup:
	xrtUnit();
	return iRet;
}
