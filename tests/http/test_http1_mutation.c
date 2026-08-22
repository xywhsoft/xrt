#include "../test.h"



/* 验证一个借用视图完全落在本轮输入范围内。 */
static bool testHttp1ViewInside(xstrview View, xbytesview Input)
{
	uintptr_t iInput = (uintptr_t)Input.Data;
	uintptr_t iView = (uintptr_t)View.Data;

	if ( View.Size == 0 ) {
		return (View.Data == NULL) ||
			((iView >= iInput) && (iView <= (iInput + Input.Size)));
	}
	return (View.Data != NULL) && (iView >= iInput) &&
		((iView - iInput) <= Input.Size) &&
		(View.Size <= (Input.Size - (iView - iInput)));
}



/* 所有返回状态都必须满足公开契约，不允许发布越界借用视图。 */
static void testHttp1CheckResult(
	xhttp1status Status,
	xhttpkind Kind,
	xbytesview Input,
	const xhttp1head* pHead,
	const xhttp1errorinfo* pError
)
{
	size_t i;

	if ( Status == XHTTP1_READY ) {
		testRequire((pHead->Kind == Kind) &&
			(pHead->Bytes <= Input.Size) &&
			(pHead->FieldCount <= pHead->FieldCapacity),
			"HTTP/1 mutation published invalid ready metadata");
		if ( Kind == XHTTP_REQUEST ) {
			testRequire(testHttp1ViewInside(pHead->Method, Input) &&
				testHttp1ViewInside(pHead->Target, Input),
				"HTTP/1 mutation published an invalid request view");
		} else {
			testRequire(testHttp1ViewInside(pHead->Reason, Input),
				"HTTP/1 mutation published an invalid reason view");
		}
		for ( i = 0; i < pHead->FieldCount; i++ ) {
			testRequire(testHttp1ViewInside(
				pHead->Fields[i].Name, Input
			) && testHttp1ViewInside(
				pHead->Fields[i].Value, Input
			), "HTTP/1 mutation published an invalid field view");
		}
	} else if ( Status == XHTTP1_FIELDS ) {
		testRequire(pHead->FieldCount > pHead->FieldCapacity,
			"HTTP/1 mutation returned FIELDS without a shortage");
	} else if ( Status == XHTTP1_MORE ) {
		testRequire((pHead->FieldCount == 0) &&
			(pHead->Kind == 0),
			"HTTP/1 mutation MORE state published partial output");
	} else {
		testRequire((Status == XHTTP1_ERROR) &&
			(pError->Code != 0),
			"HTTP/1 mutation returned an unknown state");
	}
}



/* 对一份输入同时执行请求和响应解析并检查状态不变量。 */
static void testHttp1Try(const uint8* pData, size_t iSize)
{
	xhttpfield Fields[8];
	xhttp1errorinfo Error;
	xhttp1limits Limits;
	xhttp1head Head;
	xbytesview Input;
	xhttp1status Status;

	Input.Data = pData;
	Input.Size = iSize;
	xrtHttp1LimitsInit(&Limits);
	Limits.MaxHead = 512;
	Limits.MaxStartLine = 128;
	Limits.MaxFieldLine = 128;
	Limits.MaxFields = 32;

	xrtHttp1HeadInit(&Head, Fields, 8);
	Status = xrtHttp1RequestParse(Input, &Head, &Limits, &Error);
	testHttp1CheckResult(Status, XHTTP_REQUEST, Input, &Head, &Error);
	xrtClearError();

	xrtHttp1HeadInit(&Head, Fields, 8);
	Status = xrtHttp1ResponseParse(Input, &Head, &Limits, &Error);
	testHttp1CheckResult(Status, XHTTP_RESPONSE, Input, &Head, &Error);
	xrtClearError();
}



/* 逐字节替换有效报文，覆盖结构字符和全部主要控制边界。 */
static void testHttp1StructuredMutation(void)
{
	static const uint8 Replacements[] = {
		0, '\r', '\n', ' ', ':', ',', ';', UINT8_C(0x1F),
		UINT8_C(0x7F), UINT8_C(0x80), UINT8_C(0xFF)
	};
	static const char Seed[] =
		"GET /path HTTP/1.1\r\nHost: example.test\r\n"
		"Content-Length: 3\r\n\r\nabc";
	uint8 Buffer[sizeof(Seed) - 1u];
	size_t i;
	size_t j;

	for ( i = 0; i < sizeof(Buffer); i++ ) {
		for ( j = 0; j < sizeof(Replacements); j++ ) {
			memcpy(Buffer, Seed, sizeof(Buffer));
			Buffer[i] = Replacements[j];
			testHttp1Try(Buffer, sizeof(Buffer));
		}
	}
}



/* 使用固定种子随机输入覆盖长度、截断和任意字节组合。 */
static void testHttp1RandomMutation(void)
{
	uint8 Buffer[1024];
	uint32 iState = UINT32_C(0x6A09E667);
	size_t iRound;

	testHttp1Try(NULL, 0);
	for ( iRound = 0; iRound < 4000; iRound++ ) {
		size_t iSize;
		size_t i;

		iState = (iState * UINT32_C(1664525)) +
			UINT32_C(1013904223);
		iSize = (size_t)(iState % (sizeof(Buffer) + 1u));
		for ( i = 0; i < iSize; i++ ) {
			iState = (iState * UINT32_C(1664525)) +
				UINT32_C(1013904223);
			Buffer[i] = (uint8)(iState >> 24);
		}
		testHttp1Try(Buffer, iSize);
	}
}



/* 执行从旧模糊入口提炼出的可重复协议变异回归。 */
int main(void)
{
	testHttp1StructuredMutation();
	testHttp1RandomMutation();
	printf("[PASS] http1_mutation\n");
	return 0;
}
