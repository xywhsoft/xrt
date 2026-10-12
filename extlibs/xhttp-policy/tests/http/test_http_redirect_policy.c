#include "../test.h"
#include <xrt/http_redirect_policy.h>

/* 验证无网络环境下的重定向转换、输入所有权和默认安全边界。 */
int main(void)
{
	xhttprequest* Request = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"), XRT_STR_LITERAL("https://example.com/a#part")
	);
	xhttpbody* Body = xrtHttpBodyCopy((xbytesview){ (cbytes)"body", 4 });
	xhttpredirecterror Error;
	xhttprequest* Next;
	xstrview Url;

	testRequire(Request && Body && xrtHttpRequestSetBody(Request, Body) &&
		xrtHttpRequestSetHeader(Request, XRT_STR_LITERAL("Authorization"), XRT_STR_LITERAL("Bearer secret")) &&
		xrtHttpRequestSetHeader(Request, XRT_STR_LITERAL("Content-Type"), XRT_STR_LITERAL("text/plain")),
		"redirect fixture failed");
	xrtHttpBodyDestroy(Body);
	Next = xrtHttpRedirectRequest(Request, 302, XRT_STR_LITERAL("https://other.example/b"),
		XHTTP_REDIRECT_POST_TO_GET, &Error);
	testRequire(Next && Error == XHTTP_REDIRECT_OK &&
		xrtHttpMethodEqual(xrtHttpRequestMethod(Next), XRT_STR_LITERAL("GET")) &&
		!xrtHttpRequestBody(Next) &&
		!xrtHttpRequestHeader(Next, XRT_STR_LITERAL("Authorization")) &&
		!xrtHttpRequestHeader(Next, XRT_STR_LITERAL("Content-Type")), "redirect rewrite failed");
	Url = xrtHttpRequestUrlText(Next);
	testRequire(Url.Size == sizeof("https://other.example/b#part") - 1 &&
		memcmp(Url.Data, "https://other.example/b#part", Url.Size) == 0, "fragment inheritance failed");
	xrtHttpRequestDestroy(Next);
	testRequire(xrtHttpRequestBody(Request) &&
		xrtHttpMethodEqual(xrtHttpRequestMethod(Request), XRT_STR_LITERAL("POST")) &&
		xrtHttpRequestHeader(Request, XRT_STR_LITERAL("Authorization")), "policy modified its input");
	Next = xrtHttpRedirectRequest(Request, 307, XRT_STR_LITERAL("/keep#"), 0, &Error);
	testRequire(Next && xrtHttpRequestBody(Next) &&
		xrtHttpRequestHeader(Next, XRT_STR_LITERAL("Authorization")), "same-origin preservation failed");
	Url = xrtHttpRequestUrlText(Next);
	testRequire(Url.Size && Url.Data[Url.Size - 1] == '#', "explicit empty fragment failed");
	xrtHttpRequestDestroy(Next);
	testRequire(!xrtHttpRedirectRequest(Request, 307, XRT_STR_LITERAL("http://example.com/b"), 0, &Error) &&
		Error == XHTTP_REDIRECT_DOWNGRADE, "unauthorized downgrade accepted");
	testRequire(!xrtHttpRedirectRequest(Request, 200, XRT_STR_LITERAL("/b"), 0, &Error) &&
		Error == XHTTP_REDIRECT_INVALID, "invalid redirect status accepted");
	xrtClearError();
	xrtHttpRequestDestroy(Request);
	puts("[PASS] HTTP redirect policy without IO");
	return 0;
}
