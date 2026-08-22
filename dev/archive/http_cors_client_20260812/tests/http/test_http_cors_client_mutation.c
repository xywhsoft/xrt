#include "../test.h"



/* 生成固定可复现的客户端协议变异字节。 */
static uint32 nextRandom(uint32* pState)
{
	uint32 iValue = *pState;

	iValue ^= (iValue << 13);
	iValue ^= (iValue >> 17);
	iValue ^= (iValue << 5);
	*pState = iValue;
	return iValue;
}



/* 反复扰动预检响应，验证错误、拒绝和允许三条路径互不混淆。 */
int main(void)
{
	static const xhttpfield Request[] = {
		{ XRT_STR_INIT("Content-Type"), XRT_STR_INIT("application/json") }
	};
	char OriginValue[48] = "https://app.example";
	char MethodValue[48] = "GET, PATCH";
	char HeaderValue[48] = "content-type";
	xhttpfield Response[] = {
		{ XRT_STR_INIT("Access-Control-Allow-Origin"), { OriginValue, 19u } },
		{ XRT_STR_INIT("Access-Control-Allow-Methods"), { MethodValue, 10u } },
		{ XRT_STR_INIT("Access-Control-Allow-Headers"), { HeaderValue, 12u } }
	};
	xhttporigin Origin;
	xhttpcorsclientresult Result;
	uint32 iState = UINT32_C(0xC0C12026);
	size_t i;

	testRequire(xrtHttpOriginParse(
		XRT_STR_LITERAL("https://app.example"), &Origin
	), "CORS client mutation Origin parse failed");
	for ( i = 0; i < 6000u; i++ ) {
		size_t iOrigin = nextRandom(&iState) % sizeof(OriginValue);
		size_t iMethod = nextRandom(&iState) % sizeof(MethodValue);
		size_t iHeader = nextRandom(&iState) % sizeof(HeaderValue);
		char cOrigin = OriginValue[iOrigin];
		char cMethod = MethodValue[iMethod];
		char cHeader = HeaderValue[iHeader];
		bool bSuccess;

		OriginValue[iOrigin] = (char)(nextRandom(&iState) & 0x7Fu);
		MethodValue[iMethod] = (char)(nextRandom(&iState) & 0x7Fu);
		HeaderValue[iHeader] = (char)(nextRandom(&iState) & 0x7Fu);
		Response[0].Value.Size = nextRandom(&iState) % sizeof(OriginValue);
		Response[1].Value.Size = nextRandom(&iState) % sizeof(MethodValue);
		Response[2].Value.Size = nextRandom(&iState) % sizeof(HeaderValue);
		bSuccess = xrtHttpCorsPreflightCheck(
			(uint16)(100u + (nextRandom(&iState) % 500u)),
			&Origin,
			XRT_STR_LITERAL("PATCH"),
			Request,
			1u,
			false,
			Response,
			3u,
			&Result
		);
		if ( bSuccess ) {
			if ( Result.Reject == XHTTP_CORS_CLIENT_REJECT_NONE ) {
				testRequire(
					(Result.Flags & XHTTP_CORS_CLIENT_ALLOW) != 0,
					"CORS accepted mutation omitted allow flag"
				);
			} else {
				testRequire(
					(Result.Flags & XHTTP_CORS_CLIENT_ALLOW) == 0,
					"CORS rejected mutation retained allow flag"
				);
			}
		} else {
			testRequire(
				(Result.Flags == 0) &&
				(Result.Reject == XHTTP_CORS_CLIENT_REJECT_NONE) &&
				(Result.MaxAge == 0),
				"CORS failed mutation published a partial result"
			);
			xrtClearError();
		}
		OriginValue[iOrigin] = cOrigin;
		MethodValue[iMethod] = cMethod;
		HeaderValue[iHeader] = cHeader;
	}
	printf("[PASS] http_cors_client_mutation\n");
	return 0;
}
