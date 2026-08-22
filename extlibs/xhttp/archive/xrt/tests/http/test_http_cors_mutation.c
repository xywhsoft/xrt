#include "../test.h"



/* 生成固定可复现的协议变异字节。 */
static uint32 nextRandom(uint32* pState)
{
	uint32 iValue = *pState;

	iValue ^= (iValue << 13);
	iValue ^= (iValue >> 17);
	iValue ^= (iValue << 5);
	*pState = iValue;
	return iValue;
}



/* 反复扰动 CORS 字段，验证所有接受路径仍可完整迭代。 */
int main(void)
{
	char Origin[48] = "https://app.example";
	char Methods[48] = "GET, POST, PATCH";
	xhttpfield Fields[] = {
		{ XRT_STR_INIT("Access-Control-Allow-Origin"), { Origin, 19u } },
		{ XRT_STR_INIT("Access-Control-Allow-Methods"), { Methods, 16u } }
	};
	xhttpcorsresponse Response;
	xhttpcorscursor Cursor;
	xstrview Method;
	uint32 iState = UINT32_C(0xC0752026);
	size_t i;

	for ( i = 0; i < 6000u; i++ ) {
		size_t iOrigin = nextRandom(&iState) % sizeof(Origin);
		size_t iMethod = nextRandom(&iState) % sizeof(Methods);
		char cOrigin = Origin[iOrigin];
		char cMethod = Methods[iMethod];

		Origin[iOrigin] = (char)(nextRandom(&iState) & 0x7Fu);
		Methods[iMethod] = (char)(nextRandom(&iState) & 0x7Fu);
		Fields[0].Value.Size = nextRandom(&iState) % sizeof(Origin);
		Fields[1].Value.Size = nextRandom(&iState) % sizeof(Methods);
		if ( xrtHttpCorsResponseRead(Fields, 2u, &Response) ) {
			size_t iCount = 0;
			xhttpnext Next;

			xrtHttpCorsCursorInit(&Cursor);
			while ( (Next = xrtHttpCorsAllowMethodNext(
				Fields, 2u, &Cursor, &Method
			)) == XHTTP_NEXT_ITEM ) {
				iCount++;
			}
			testRequire(
				(Next == XHTTP_NEXT_END) &&
				(iCount == Response.MethodCount),
				"CORS accepted response did not round-trip"
			);
		} else {
			xrtClearError();
		}
		Origin[iOrigin] = cOrigin;
		Methods[iMethod] = cMethod;
	}
	printf("[PASS] http_cors_mutation\n");
	return 0;
}
