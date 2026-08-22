#ifndef XRT_TEST_HTTP_CONNECT_PROXY_H
#define XRT_TEST_HTTP_CONNECT_PROXY_H

#include "../test.h"



/* 查找完整 HTTP/1 Header 的线缆长度，输入不足返回零。 */
static inline size_t testHttpHeaderSize(
	const char* pData,
	size_t iSize
)
{
	for ( size_t i = 3; i < iSize; i++ ) {
		if ( (pData[i - 3] == '\r') &&
			(pData[i - 2] == '\n') &&
			(pData[i - 1] == '\r') &&
			(pData[i] == '\n') ) {
			return i + 1u;
		}
	}
	return 0;
}



/*
	增量验证并消费一条 HTTP CONNECT 请求。
	输入不足返回 false；协议不匹配由测试断言立即终止。
*/
static inline bool testHttpConnectProxyRequest(
	xnetbuf* pBuffer,
	cstr sHost,
	uint16 iPort,
	bool bAuthorization
)
{
	static const char Authorization[] =
		"\r\nProxy-Authorization: Basic "
		"dXNlcjpwYXNzd29yZA==\r\n";
	char Request[2048];
	char Expected[512];
	size_t iSize = xrtNetBufSize(pBuffer);
	size_t iHeader;
	int iLength;

	if ( iSize == 0 ) {
		return false;
	}
	testRequire(
		iSize < sizeof(Request),
		"HTTP CONNECT fixture input overflowed"
	);
	testRequire(
		xrtNetBufPeek(
			pBuffer,
			0,
			Request,
			iSize
		) == iSize,
		"HTTP CONNECT fixture input peek failed"
	);
	Request[iSize] = 0;
	iHeader = testHttpHeaderSize(Request, iSize);
	if ( iHeader == 0 ) {
		return false;
	}
	iLength = snprintf(
		Expected,
		sizeof(Expected),
		"CONNECT %s:%u HTTP/1.1\r\n"
		"Host: %s:%u\r\n",
		sHost,
		(unsigned)iPort,
		sHost,
		(unsigned)iPort
	);
	testRequire(
		(iLength > 0) &&
		((size_t)iLength < sizeof(Expected)) &&
		(iHeader >= (size_t)iLength) &&
		(memcmp(
			Request,
			Expected,
			(size_t)iLength
		) == 0),
		"HTTP CONNECT authority or Host mismatch"
	);
	testRequire(
		(strstr(Request, Authorization) != NULL) ==
			bAuthorization,
		"HTTP CONNECT authorization mismatch"
	);
	(void)xrtNetBufConsume(pBuffer, iHeader);
	return true;
}



/*
	增量验证一条 HTTP CONNECT 请求并发送成功响应。
	输入不足返回 false；协议不匹配由测试断言立即终止。
*/
static inline bool testHttpConnectProxyStep(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	cstr sHost,
	uint16 iPort,
	bool bAuthorization
)
{
	static const char Response[] =
		"HTTP/1.1 200 Connection Established\r\n"
		"Proxy-Agent: xrt-test\r\n"
		"\r\n";

	if ( !testHttpConnectProxyRequest(
		pBuffer,
		sHost,
		iPort,
		bAuthorization
	) ) {
		return false;
	}
	testRequire(
		xrtNetStreamSend(
			pStream,
			Response,
			sizeof(Response) - 1u
		) == XNET_RESULT_OK,
		"HTTP CONNECT response failed"
	);
	return true;
}



#endif
