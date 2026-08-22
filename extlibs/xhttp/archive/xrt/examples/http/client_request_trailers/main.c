#include <xrt.h>



/* 为流式请求声明由 HTTP/1.1 准备层自动发送的 Trailer 字段。 */
int main(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("https://example.test/upload")
	);
	bool bPass = (pRequest != NULL) &&
		xrtHttpRequestAddTrailer(
			pRequest,
			XRT_STR_LITERAL("Digest"),
			XRT_STR_LITERAL("sha-256=:example:")
		);

	xrtHttpRequestDestroy(pRequest);
	return bPass ? 0 : 1;
}
