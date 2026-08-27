#include <xrt.h>

#include <stdio.h>



/* 读取预检请求，但不把协议层绑定到具体服务器对象。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Origin"), XRT_STR_INIT("https://app.example") },
		{ XRT_STR_INIT("Access-Control-Request-Method"), XRT_STR_INIT("PATCH") },
		{ XRT_STR_INIT("Access-Control-Request-Headers"), XRT_STR_INIT("Content-Type, X-Trace") }
	};
	xhttpcorsrequest Request;
	xhttpcorscursor Cursor;
	xstrview Name;
#if defined(XRT_FEATURE_HTTP_CORS_POLICY)
	xhttpcorspolicy Policy = { 0 };
	xhttpcorsdecision Decision;
#endif
#if defined(XRT_FEATURE_HTTP_CORS_WRITE)
	char Output[256];
	size_t iSize;
#endif
#if defined(XRT_FEATURE_HTTP_CORS_CLIENT)
	static const xhttpfield ClientRequest[] = {
		{ XRT_STR_INIT("Content-Type"), XRT_STR_INIT("application/json") }
	};
	static const xhttpfield ClientResponse[] = {
		{ XRT_STR_INIT("Access-Control-Allow-Origin"), XRT_STR_INIT("*") },
		{ XRT_STR_INIT("Access-Control-Allow-Methods"), XRT_STR_INIT("PATCH") },
		{ XRT_STR_INIT("Access-Control-Allow-Headers"), XRT_STR_INIT("content-type") }
	};
	xhttporigin ClientOrigin;
	xhttpcorspreflightplan Plan;
	xhttpcorsclientresult Result;
#endif
#if defined(XRT_FEATURE_HTTP_CORS_CLIENT_WRITE)
	char ClientOutput[256];
	size_t iClientSize;
#endif

	if ( !xrtHttpCorsRequestRead(
		XRT_STR_LITERAL("OPTIONS"), Fields, 3u, &Request
	) ) {
		return 1;
	}
	printf("method: %.*s\n", (int)Request.RequestMethod.Size,
		Request.RequestMethod.Data);
	xrtHttpCorsCursorInit(&Cursor);
	while ( xrtHttpCorsRequestHeaderNext(
		Fields, 3u, &Cursor, &Name
	) == XHTTP_NEXT_ITEM ) {
		printf("request header: %.*s\n", (int)Name.Size, Name.Data);
	}
#if defined(XRT_FEATURE_HTTP_CORS_POLICY)
	Policy.Flags = XHTTP_CORS_POLICY_ANY_ORIGIN |
		XHTTP_CORS_POLICY_ANY_METHOD |
		XHTTP_CORS_POLICY_ANY_HEADER;
	if ( !xrtHttpCorsPolicyCheck(
		&Policy,
		XRT_STR_LITERAL("OPTIONS"),
		Fields,
		3u,
		&Decision
	) || (Decision.Reject != XHTTP_CORS_REJECT_NONE) ) {
		return 2;
	}
#endif
#if defined(XRT_FEATURE_HTTP_CORS_WRITE)
	if ( !xrtHttpCorsDecisionWrite(
		&Decision,
		Fields,
		3u,
		Output,
		sizeof(Output),
		&iSize
	) ) {
		return 3;
	}
	fwrite(Output, 1u, iSize, stdout);
#endif
#if defined(XRT_FEATURE_HTTP_CORS_CLIENT)
	if ( !xrtHttpOriginParse(
		XRT_STR_LITERAL("https://app.example"),
		&ClientOrigin
	) || !xrtHttpCorsPreflightPlan(
		XRT_STR_LITERAL("PATCH"),
		ClientRequest,
		1u,
		false,
		&Plan
	) || !xrtHttpCorsPreflightCheck(
		204u,
		&ClientOrigin,
		XRT_STR_LITERAL("PATCH"),
		ClientRequest,
		1u,
		false,
		ClientResponse,
		3u,
		&Result
	) || ((Result.Flags & XHTTP_CORS_CLIENT_ALLOW) == 0) ) {
		return 4;
	}
	printf("preflight headers: %zu\n", Plan.HeaderCount);
#endif
#if defined(XRT_FEATURE_HTTP_CORS_CLIENT_WRITE)
	if ( !xrtHttpCorsPreflightFieldsWrite(
		&ClientOrigin,
		XRT_STR_LITERAL("PATCH"),
		ClientRequest,
		1u,
		ClientOutput,
		sizeof(ClientOutput),
		&iClientSize
	) ) {
		return 5;
	}
	fwrite(ClientOutput, 1u, iClientSize, stdout);
#endif
	return 0;
}
